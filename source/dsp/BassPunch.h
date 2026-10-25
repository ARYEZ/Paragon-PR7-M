#pragma once

#include <array>
#include <cmath>

#include <juce_dsp/juce_dsp.h>

#include "dsp/Biquad.h"

namespace effects
{
    // BASS PUNCH: more low end and tighter at once, on one knob (0 to 10).
    // Our own design: a low shelf (half its gain at 100 Hz) whose gain moves
    // with the bass itself, a "dynamic EQ":
    //
    //   - it lifts the lows by up to +6 dB at 10 (0.6 dB a step), so quiet
    //     bass comes up: more low end;
    //   - a compressor listening only to the lows (under 120 Hz) takes that
    //     lift back on loud notes, and past it into a small cut on the
    //     loudest, so the low end comes out fuller and more even: tighter.
    //     Its threshold falls (-12 to -24 dB) and its ratio rises (1:1 to
    //     3:1) with the knob. It reacts in about 25 ms and lets go over
    //     150 ms, so a kick's first thump gets through before it acts.
    //
    // One filter, not a split into bands added back together: a split
    // shifts the phase of everything round the crossover even when it does
    // little, this changes only the lows. Both channels move together. At 0
    // it doesn't run: bit for bit. No latency.
    class BassPunch
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr float shelfHz = 100.0f, detectorHz = 120.0f;
        static constexpr float boostPerStep = 0.6f;   // 10 steps is +6 dB
        static constexpr float lowestGainDb = -6.0f;  // the most it cuts

        struct Curve
        {
            float boostDb, thresholdDb, ratio;
        };

        static Curve curveFor (float punch) noexcept
        {
            const auto p = juce::jlimit (0.0f, 1.0f, punch / 10.0f);
            return { 6.0f * p, -12.0f - 12.0f * p, 1.0f + 2.0f * p };
        }

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            punch.reset (sampleRate, 0.1);
            rmsCoefficient = coefficientFor (0.01f);
            attack = coefficientFor (0.025f);
            release = coefficientFor (0.15f);
            reset();
        }

        void reset()
        {
            for (auto& channel : detectors)
                for (auto& filter : channel)
                {
                    filter.c = Biquad::lowPass (sampleRate, detectorHz);
                    filter.reset();
                }
            for (auto& filter : shelves)
                filter.reset();
            meanSquare = 0.0f;
            reduction = 0.0f;
            punch.setCurrentAndTargetValue (punch.getTargetValue());
            setShelf (0.0f);
        }

        // BASS PUNCH (0 to 10), smoothed over 100 ms.
        void setPunch (float value) { punch.setTargetValue (juce::jlimit (0.0f, 10.0f, value)); }
        void setPunchImmediately (float value) { punch.setCurrentAndTargetValue (juce::jlimit (0.0f, 10.0f, value)); }

        void process (const juce::dsp::AudioBlock<float>& block) noexcept
        {
            const auto numChannels = (int) juce::jmin (block.getNumChannels(), (size_t) maxChannels);
            if (! punch.isSmoothing() && punch.getTargetValue() <= 0.0f && std::abs (shelfDb) < 0.001f)
            {
                // Off (and back to flat, so stopping can't jump): untouched,
                // and fresh for when it's turned up.
                if (meanSquare > 0.0f || reduction > 0.0f)
                    reset();
                return;
            }

            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                const auto curve = curveFor (punch.getNextValue());

                // How loud the lows are: the louder channel, under 120 Hz
                // (a steep low-pass: two in a row), as RMS read like a peak.
                auto power = 0.0f;
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto& [first, second] = detectors[(size_t) channel];
                    const auto low = second.process (first.process (block.getChannelPointer ((size_t) channel)[i]));
                    power = juce::jmax (power, low * low);
                }
                meanSquare += (power - meanSquare) * rmsCoefficient;
                const auto levelDb = 10.0f * std::log10 (meanSquare + 1.0e-12f) + 3.0103f + curve.boostDb;   // as lifted

                // How much to take back off the lift.
                const auto over = levelDb - curve.thresholdDb;
                const auto target = over > 0.0f ? over * (1.0f - 1.0f / curve.ratio) : 0.0f;
                reduction += (target - reduction) * (target > reduction ? attack : release);

                setShelf (juce::jmax (lowestGainDb, curve.boostDb - reduction));
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto& sample = block.getChannelPointer ((size_t) channel)[i];
                    sample = shelves[(size_t) channel].process (sample);
                }
            }
        }

        // The shelf's gain just now (dB): the lift, less what the bass took back.
        float getShelfDb() const noexcept { return shelfDb; }

    private:
        float coefficientFor (float seconds) const noexcept
        {
            return 1.0f - std::exp (-1.0f / (seconds * (float) sampleRate));
        }

        void setShelf (float db) noexcept
        {
            if (juce::exactlyEqual (db, shelfDb))
                return;
            shelfDb = db;
            const auto c = Biquad::lowShelf (sampleRate, shelfHz, (double) db);
            for (auto& filter : shelves)
                filter.c = c;
        }

        double sampleRate = 48000.0;
        juce::SmoothedValue<float> punch { 0.0f };
        std::array<std::array<Biquad, 2>, maxChannels> detectors;
        std::array<Biquad, maxChannels> shelves;
        float meanSquare = 0.0f, reduction = 0.0f, shelfDb = 0.0f;
        float rmsCoefficient = 0.01f, attack = 0.001f, release = 0.001f;
    };
}
