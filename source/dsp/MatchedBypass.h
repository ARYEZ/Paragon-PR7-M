#pragma once

#include <array>
#include <cmath>

#include <juce_dsp/juce_dsp.h>

#include "dsp/Loudness.h"

namespace effects
{
    // BYPASS at matched loudness. Louder almost always sounds better, so a
    // plain bypass makes the processing seem better than it is. This
    // measures the short-term loudness (the last 3 s, LUFS) of the mix as
    // it came in and of the processed mix, and when bypassed plays the mix
    // as it came in turned up or down to the processed loudness. Switching
    // crossfades over 20 ms, and the match follows the music smoothly.
    //
    // The match only moves while both are playing (above -60 LUFS), so the
    // gaps between songs don't throw it. It's at most 24 dB either way, and
    // it never turns the unprocessed mix up so far that its peaks (over the
    // same 3 s) pass -0.3 dBFS: then the comparison would clip. Against a
    // much louder master, the bypassed mix can stay a little quieter.
    //
    // The processing keeps running while bypassed, so the match stays up to
    // date and switching back is seamless.
    class MatchedBypass
    {
    public:
        static constexpr float maxMatchDb = 24.0f;
        static constexpr float gateLufs = -60.0f;
        static constexpr float headroomDb = 0.3f;

        void prepare (double sampleRate)
        {
            dryLoudness.prepare (sampleRate);
            wetLoudness.prepare (sampleRate);
            fade.reset (sampleRate, 0.02);
            gain.reset (sampleRate, 0.5);
            peakBlockLength = juce::jmax (1, (int) std::round (sampleRate * 0.1));
            reset();
        }

        void reset()
        {
            dryLoudness.reset();
            wetLoudness.reset();
            fade.setCurrentAndTargetValue (fade.getTargetValue());
            gain.setCurrentAndTargetValue (1.0f);
            peaks.fill (0.0f);
            blockPeak = 0.0f;
            samplesInPeakBlock = 0;
            nextPeak = 0;
        }

        // Snap on or off without the crossfade: when the host restarts playback.
        void setImmediately (bool bypassed) { fade.setCurrentAndTargetValue (bypassed ? 1.0f : 0.0f); }

        // `dry` is the mix as it came in, `wet` the processed mix (aligned
        // with it), which this replaces with what's heard.
        void process (const juce::dsp::AudioBlock<const float>& dry, const juce::dsp::AudioBlock<float>& wet, bool bypassed) noexcept
        {
            const auto numChannels = (int) juce::jmin (dry.getNumChannels(), wet.getNumChannels(), (size_t) metering::maxChannels);
            const auto numSamples = (int) wet.getNumSamples();
            std::array<const float*, metering::maxChannels> dryChannels {}, wetChannels {};
            for (int channel = 0; channel < numChannels; ++channel)
            {
                dryChannels[(size_t) channel] = dry.getChannelPointer ((size_t) channel);
                wetChannels[(size_t) channel] = wet.getChannelPointer ((size_t) channel);
            }

            trackPeaks (dryChannels.data(), numChannels, numSamples);
            dryLoudness.process (dryChannels.data(), numChannels, numSamples);
            if (wetLoudness.process (wetChannels.data(), numChannels, numSamples))
                updateMatch();

            fade.setTargetValue (bypassed ? 1.0f : 0.0f);
            if (! fade.isSmoothing() && ! bypassed)
            {
                gain.skip (numSamples);
                return;   // the processed mix, untouched
            }

            for (int i = 0; i < numSamples; ++i)
            {
                const auto matched = gain.getNextValue();
                const auto amount = fade.getNextValue();
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto& out = wet.getChannelPointer ((size_t) channel)[(size_t) i];
                    const auto in = dryChannels[(size_t) channel][i] * matched;
                    out = amount >= 1.0f ? in : out + (in - out) * amount;
                }
            }
        }

        // How far the unprocessed mix is turned when bypassed (dB).
        float getMatchDb() const noexcept { return juce::Decibels::gainToDecibels (gain.getTargetValue(), -100.0f); }

    private:
        void trackPeaks (const float* const* channels, int numChannels, int numSamples) noexcept
        {
            for (int i = 0; i < numSamples; ++i)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                    blockPeak = std::fmax (blockPeak, std::abs (channels[channel][i]));
                if (++samplesInPeakBlock == peakBlockLength)
                {
                    peaks[(size_t) nextPeak] = blockPeak;
                    nextPeak = (nextPeak + 1) % (int) peaks.size();
                    blockPeak = 0.0f;
                    samplesInPeakBlock = 0;
                }
            }
        }

        void updateMatch() noexcept
        {
            const auto dryLufs = dryLoudness.getLufs(), wetLufs = wetLoudness.getLufs();
            if (dryLufs < gateLufs || wetLufs < gateLufs)
                return;   // hold the match through silence

            auto matchDb = juce::jlimit (-maxMatchDb, maxMatchDb, wetLufs - dryLufs);
            auto peak = 0.0f;
            for (const auto p : peaks)
                peak = std::fmax (peak, p);
            if (matchDb > 0.0f && peak > 0.0f)   // only ever limits a boost
                matchDb = juce::jmin (matchDb, juce::jmax (0.0f, -headroomDb - juce::Decibels::gainToDecibels (peak)));

            // A match of 0 dB is exactly 1, so with nothing processed the
            // bypassed mix is the input, bit for bit.
            gain.setTargetValue (std::abs (matchDb) < 0.001f ? 1.0f : juce::Decibels::decibelsToGain (matchDb));
        }

        metering::ShortTermLoudness dryLoudness, wetLoudness;
        juce::SmoothedValue<float> fade;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gain;
        std::array<float, metering::ShortTermLoudness::blocksPerWindow> peaks {};
        float blockPeak = 0.0f;
        int peakBlockLength = 4800, samplesInPeakBlock = 0, nextPeak = 0;
    };
}
