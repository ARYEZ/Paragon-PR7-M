#pragma once

#include <cmath>

#include <juce_dsp/juce_dsp.h>

namespace effects
{
    // TRANSIENTS: sharper attacks (right) or softer ones (left), -10 to +10.
    // Our own design, a transient shaper: a fast follower of the level
    // (0.5 ms up, 100 ms down), and a slow one following the fast one
    // (25 ms up, 100 ms down). At the start of a hit the fast one leaps
    // ahead of the slow one; that gap (in dB, up to 12) marks the attack,
    // and the sound is turned up or down by half of it times TRANSIENTS / 10:
    // up to +6 dB on attacks at +10, down to -6 dB at -10.
    //
    // On a held note the two stay together (the slow one follows the fast
    // one, and both let go at the same pace), so what sustains is
    // untouched; the gain is smoothed over 2 ms, so a low note's ripple
    // doesn't wobble it (a test holds a steady 50 Hz tone within 0.05 dB).
    // Hits closer than about 100 ms apart are lifted a little less.
    //
    // It follows the louder channel and turns both together, so the stereo
    // image holds still. At 0 it doesn't run: bit for bit. No latency.
    class TransientShaper
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr float maxGapDb = 12.0f;

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            amount.reset (sampleRate, 0.05);
            fastUp = coefficientFor (0.0005f);
            slowUp = coefficientFor (0.025f);
            down = coefficientFor (0.1f);
            gainSmoothing = coefficientFor (0.002f);
            reset();
        }

        void reset()
        {
            fast = slow = gainDb = 0.0f;
            amount.setCurrentAndTargetValue (amount.getTargetValue());
        }

        // TRANSIENTS (-10 to +10), smoothed over 50 ms.
        void setAmount (float value) { amount.setTargetValue (juce::jlimit (-10.0f, 10.0f, value) / 10.0f); }
        void setAmountImmediately (float value) { amount.setCurrentAndTargetValue (juce::jlimit (-10.0f, 10.0f, value) / 10.0f); }

        void process (const juce::dsp::AudioBlock<float>& block) noexcept
        {
            const auto numChannels = (int) juce::jmin (block.getNumChannels(), (size_t) maxChannels);
            if (! amount.isSmoothing() && juce::exactlyEqual (amount.getTargetValue(), 0.0f))
            {
                // Off: untouched, and fresh for when it's turned.
                if (fast > 0.0f || slow > 0.0f || std::abs (gainDb) > 0.0f)
                    reset();
                return;
            }

            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                auto level = 0.0f;
                for (int channel = 0; channel < numChannels; ++channel)
                    level = juce::jmax (level, std::abs (block.getChannelPointer ((size_t) channel)[i]));
                fast += (level - fast) * (level > fast ? fastUp : down);
                slow += (fast - slow) * (fast > slow ? slowUp : down);

                // The attack: how far the fast follower is ahead, in dB.
                const auto gapDb = fast > slow && slow > 1.0e-6f ? juce::jmin (maxGapDb, 20.0f * std::log10 (fast / slow))
                                                                 : (fast > 1.0e-6f && slow <= 1.0e-6f ? maxGapDb : 0.0f);
                gainDb += (amount.getNextValue() * 0.5f * gapDb - gainDb) * gainSmoothing;
                const auto gain = juce::Decibels::decibelsToGain (gainDb);
                for (int channel = 0; channel < numChannels; ++channel)
                    block.getChannelPointer ((size_t) channel)[i] *= gain;
            }
        }

    private:
        float coefficientFor (float seconds) const noexcept
        {
            return 1.0f - std::exp (-1.0f / (seconds * (float) sampleRate));
        }

        double sampleRate = 48000.0;
        juce::SmoothedValue<float> amount { 0.0f };
        float fast = 0.0f, slow = 0.0f, gainDb = 0.0f;
        float fastUp = 0.1f, slowUp = 0.001f, down = 0.001f, gainSmoothing = 0.01f;
    };
}
