#pragma once

#include <array>
#include <cmath>
#include <vector>

#include <juce_dsp/juce_dsp.h>

#include "dsp/TruePeak.h"

namespace effects
{
    // VOLUME: gain (0 to +15 dB) into a look-ahead true-peak limiter, so the
    // mix gets louder without its peaks passing the ceiling (-1 dBTP, where
    // streaming services ask a master to stay). Our own design.
    //
    // How it keeps every peak under the ceiling, including those between the
    // samples:
    //  - It watches the waveform itself (TruePeak, interpolated 8x), and
    //    works out for each stretch how far it must be turned down, aiming
    //    0.06 dB under the ceiling for what it can miss between its points.
    //  - It looks 3 ms ahead: the audio is delayed, and the turning down is
    //    eased in over those 3 ms so it's complete when the peak arrives,
    //    instead of clipping it (a click) or grabbing it all at once (a
    //    crackle).
    //  - Then it lets go: in 15 ms after a short peak, up to 120 ms after
    //    long, hard limiting.
    // Both channels are turned down together, so the stereo image holds still.
    //
    // The delay is fixed (getLatency(), reported to the host), whatever
    // VOLUME is set to. With nothing to turn down, the gain is exactly 1, so
    // the audio comes out bit for bit, only later.
    class TruePeakLimiter
    {
    public:
        static constexpr float ceilingDb = -1.0f;
        // What it aims for: a hair under the ceiling, for what even 8x
        // detection can miss between its points (up to about 0.06 dB, with
        // music reaching 20 kHz).
        static constexpr float targetDb = ceilingDb - 0.06f;
        static constexpr float lookAheadSeconds = 0.003f;
        // The release follows the music: 15 ms after a short peak, so the
        // level comes back between hits and the mix stays loud; stretching
        // up to 8x (120 ms) the longer and harder it has been limiting (over
        // about a second, 6 dB counting as hard), so steady heavy limiting
        // doesn't wobble the bass. Measured on a dense mix, it gets about as
        // loud as limiting can before distorting (-8.8 LUFS from -15.5, the
        // waveform untouched to -42 dB).
        static constexpr float releaseSeconds = 0.015f;
        static constexpr float releaseStretch = 7.0f;
        static constexpr int maxChannels = 2;

        // The delay it adds at a sample rate, in samples.
        static int latencyFor (double sampleRate) noexcept { return lookAheadFor (sampleRate) - 1 + metering::TruePeak::delay; }

        void prepare (double sampleRate)
        {
            lookAhead = lookAheadFor (sampleRate);
            latency = latencyFor (sampleRate);
            detector.prepare (sampleRate, 8);
            for (auto& line : delayLines)
                line.assign ((size_t) latency + 1, 0.0f);
            holdValues.assign ((size_t) lookAhead + 1, 1.0f);
            holdIndices.assign ((size_t) lookAhead + 1, 0);
            averaged.assign ((size_t) lookAhead, 1.0f);
            rate = sampleRate;
            memoryCoefficient = 1.0 - std::exp (-1.0 / sampleRate);
            volume.reset (sampleRate, 0.05);
            reset();
        }

        void reset()
        {
            detector.reset();
            for (auto& line : delayLines)
                std::fill (line.begin(), line.end(), 0.0f);
            writePosition = 0;
            holdStart = holdCount = 0;
            sampleIndex = 0;
            envelope = 1.0;
            memory = 0.0;
            std::fill (averaged.begin(), averaged.end(), 1.0f);
            averagePosition = 0;
            averageSum = (double) lookAhead;
            reducedInWindow = 0;
            volume.setCurrentAndTargetValue (volume.getTargetValue());
        }

        // VOLUME, in dB (0 to 15). Smoothed over 50 ms.
        void setVolume (float db) { volume.setTargetValue (juce::Decibels::decibelsToGain (db)); }
        void setVolumeImmediately (float db) { volume.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (db)); }

        // The delay it adds, in samples.
        int getLatency() const noexcept { return latency; }

        // Limits the block in place; returns the most it turned down (dB, positive).
        float process (const juce::dsp::AudioBlock<float>& block) noexcept
        {
            const auto numChannels = (int) juce::jmin (block.getNumChannels(), (size_t) maxChannels);
            const auto numSamples = (int) block.getNumSamples();
            const auto ceiling = juce::Decibels::decibelsToGain (targetDb);
            const auto delayLength = latency + 1;
            auto lowestGain = 1.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const auto gainIn = volume.getNextValue();

                // How far this stretch of the waveform must come down.
                auto peak = 0.0f;
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto& sample = block.getChannelPointer ((size_t) channel)[(size_t) i];
                    sample *= gainIn;
                    peak = std::fmax (peak, detector.next (channel, sample));
                }
                const auto needed = peak > ceiling ? ceiling / peak : 1.0f;

                // Hold the lowest over the look-ahead (and one more sample,
                // since a reading covers the stretch to the next sample)...
                const auto held = hold (needed);
                // ...let go, quickly after a short peak, more slowly after
                // long limiting (in double precision: in float, the last
                // steps back to 1 round to nothing and it stays stuck a hair
                // under)...
                memory += (juce::jmin (1.0, -20.0 * std::log10 (juce::jmax (1.0e-6, envelope)) / 6.0) - memory) * memoryCoefficient;
                const auto release = 1.0 - std::exp (-1.0 / ((double) releaseSeconds * (1.0 + (double) releaseStretch * memory) * rate));
                envelope = held < envelope ? (double) held : envelope + ((double) held - envelope) * release;
                if ((double) held - envelope < 1.0e-6)
                    envelope = held;
                // ...and ease it in over the look-ahead.
                const auto gain = average ((float) envelope);
                lowestGain = juce::jmin (lowestGain, gain);

                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto& line = delayLines[(size_t) channel];
                    auto& sample = block.getChannelPointer ((size_t) channel)[(size_t) i];
                    line[(size_t) writePosition] = sample;
                    const auto delayed = line[(size_t) ((writePosition + 1) % delayLength)];
                    sample = gain >= 1.0f ? delayed : delayed * gain;
                }
                writePosition = (writePosition + 1) % delayLength;
                ++sampleIndex;
            }
            return -juce::Decibels::gainToDecibels (lowestGain);
        }

    private:
        static int lookAheadFor (double sampleRate) noexcept { return juce::jmax (1, (int) std::round (sampleRate * lookAheadSeconds)); }

        // The lowest of the last lookAhead + 1 values (a monotonic queue: no
        // allocation, constant time on average).
        float hold (float value) noexcept
        {
            const auto capacity = (int) holdValues.size();
            while (holdCount > 0 && holdValues[(size_t) ((holdStart + holdCount - 1) % capacity)] >= value)
                --holdCount;
            const auto slot = (holdStart + holdCount) % capacity;
            holdValues[(size_t) slot] = value;
            holdIndices[(size_t) slot] = sampleIndex;
            ++holdCount;
            if (holdIndices[(size_t) holdStart] <= sampleIndex - capacity)
            {
                holdStart = (holdStart + 1) % capacity;
                --holdCount;
            }
            return holdValues[(size_t) holdStart];
        }

        // The average of the last lookAhead values. Exactly 1 when none of
        // them is turning down, so an untouched signal stays bit for bit.
        float average (float value) noexcept
        {
            auto& slot = averaged[(size_t) averagePosition];
            reducedInWindow += (value < 1.0f ? 1 : 0) - (slot < 1.0f ? 1 : 0);
            averageSum += (double) value - (double) slot;
            slot = value;
            averagePosition = (averagePosition + 1) % lookAhead;
            if (reducedInWindow == 0)
            {
                averageSum = (double) lookAhead;   // clear any rounding drift
                return 1.0f;
            }
            return juce::jmin (1.0f, (float) (averageSum / (double) lookAhead));
        }

        metering::TruePeak detector;
        std::array<std::vector<float>, maxChannels> delayLines;
        std::vector<float> holdValues, averaged;
        std::vector<juce::int64> holdIndices;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> volume { 1.0f };
        juce::int64 sampleIndex = 0;
        double averageSum = 0.0;
        double envelope = 1.0, memory = 0.0, memoryCoefficient = 0.0, rate = 48000.0;
        int lookAhead = 144, latency = 159, writePosition = 0, holdStart = 0, holdCount = 0, averagePosition = 0, reducedInWindow = 0;
    };
}
