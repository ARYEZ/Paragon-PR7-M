#pragma once

#include <array>
#include <cmath>

#include <juce_dsp/juce_dsp.h>

#include "dsp/Biquad.h"

namespace effects
{
    // PUSH: a glue compressor for the whole mix on one knob, gentle to hard.
    // Our own design (after PR7-C's compressor: its soft-knee curve, RMS
    // detector and stretching release). Along PUSH (p = PUSH / 15, 0 to 1)
    // everything moves together:
    //
    //   threshold  -6 dB at the start of the travel, down to -24 dB
    //   ratio      1:1 (nothing) up to 3:1
    //   attack     30 ms down to 10 ms (slow at first, so drum hits get
    //              through: the "glue" rather than flattening)
    //   release    300 ms down to 150 ms, stretching up to twice that the
    //              longer and harder it has been working, so it doesn't pump
    //   make-up    half what it turns a -6 dBFS peak down by (+6 dB at 15),
    //              so pushing harder makes the mix denser, not just quieter
    //
    // It listens to an RMS reading (which follows loudness, not every spike),
    // with the bass below about 80 Hz left out so kick and bass don't pump
    // the whole mix (BASS PUNCH looks after the low end). Both channels are
    // turned down together, from the louder one. Soft knee, 6 dB wide.
    //
    // At PUSH 0 the ratio is exactly 1:1 and it doesn't run at all: the
    // audio passes bit for bit. It adds no latency.
    class GlueCompressor
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr float kneeDb = 6.0f;
        static constexpr float makeUpReferenceDb = -6.0f;
        static constexpr float sidechainHighPassHz = 80.0f;

        // The settings at a point on PUSH's travel (0 to 15).
        struct Curve
        {
            float thresholdDb, ratio, attackMs, releaseMs, makeUpDb;
        };

        static Curve curveFor (float push) noexcept
        {
            const auto p = juce::jlimit (0.0f, 1.0f, push / 15.0f);
            Curve curve { -6.0f - 18.0f * p, 1.0f + 2.0f * p, 30.0f - 20.0f * p, 300.0f - 150.0f * p, 0.0f };
            curve.makeUpDb = -0.5f * gainComputerDb (makeUpReferenceDb, curve.thresholdDb, curve.ratio);
            return curve;
        }

        // The static curve: the gain change (dB, 0 or less) for a steady
        // level (dB, a sine's peak).
        static float gainComputerDb (float levelDb, float thresholdDb, float ratio) noexcept
        {
            const auto over = levelDb - thresholdDb;
            const auto slope = 1.0f - 1.0f / juce::jmax (1.0f, ratio);
            if (2.0f * over < -kneeDb)
                return 0.0f;
            if (2.0f * over <= kneeDb)
                return -slope * (over + kneeDb * 0.5f) * (over + kneeDb * 0.5f) / (2.0f * kneeDb);
            return -slope * over;
        }

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            push.reset (sampleRate, 0.1);
            rmsCoefficient = coefficientFor (0.005f);
            memoryCoefficient = coefficientFor (1.5f);
            for (auto& filter : sidechainFilters)
                filter.c = Biquad::highPass (sampleRate, sidechainHighPassHz);
            reset();
        }

        void reset()
        {
            for (auto& filter : sidechainFilters)
                filter.reset();
            meanSquare = 0.0f;
            reduction = 0.0f;
            memory = 0.0f;
            push.setCurrentAndTargetValue (push.getTargetValue());
        }

        // PUSH (0 to 15), smoothed over 100 ms.
        void setPush (float value) { push.setTargetValue (juce::jlimit (0.0f, 15.0f, value)); }
        void setPushImmediately (float value) { push.setCurrentAndTargetValue (juce::jlimit (0.0f, 15.0f, value)); }

        // Compresses the block in place; returns the most it turned down
        // (dB, positive, not counting the make-up gain).
        float process (const juce::dsp::AudioBlock<float>& block) noexcept
        {
            const auto numChannels = (int) juce::jmin (block.getNumChannels(), (size_t) maxChannels);
            const auto numSamples = (int) block.getNumSamples();

            // Off (and done letting go, so stopping can't jump): untouched,
            // and fresh for when it's turned up.
            if (! push.isSmoothing() && push.getTargetValue() <= 0.0f && reduction < 0.001f)
            {
                if (reduction > 0.0f || meanSquare > 0.0f)
                    reset();
                return 0.0f;
            }

            auto curve = curveFor (push.getCurrentValue());
            auto attack = coefficientFor (curve.attackMs * 0.001f);
            auto most = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                if (push.isSmoothing())
                {
                    curve = curveFor (push.getNextValue());
                    attack = coefficientFor (curve.attackMs * 0.001f);
                }

                // What the detector hears: the louder channel, without the deep bass.
                auto power = 0.0f;
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    const auto x = sidechainFilters[(size_t) channel].process (block.getChannelPointer ((size_t) channel)[(size_t) i]);
                    power = juce::jmax (power, x * x);
                }
                meanSquare += (power - meanSquare) * rmsCoefficient;
                // An RMS reading is 3 dB under a sine's peak: added back, so the
                // threshold reads like a peak level.
                const auto levelDb = 10.0f * std::log10 (meanSquare + 1.0e-12f) + 3.0103f;
                const auto target = -gainComputerDb (levelDb, curve.thresholdDb, curve.ratio);

                // The release stretches with how long and hard it has been
                // working: 1x after a short peak, up to 2x.
                memory += (juce::jmin (1.0f, reduction / 6.0f) - memory) * memoryCoefficient;
                const auto release = coefficientFor (curve.releaseMs * 0.001f * (1.0f + memory));
                reduction += (target - reduction) * (target > reduction ? attack : release);
                most = juce::jmax (most, reduction);

                const auto gain = juce::Decibels::decibelsToGain (curve.makeUpDb - reduction);
                for (int channel = 0; channel < numChannels; ++channel)
                    block.getChannelPointer ((size_t) channel)[(size_t) i] *= gain;
            }
            return most;
        }

    private:
        float coefficientFor (float seconds) const noexcept
        {
            return 1.0f - std::exp (-1.0f / (juce::jmax (1.0e-5f, seconds) * (float) sampleRate));
        }

        double sampleRate = 48000.0;
        juce::SmoothedValue<float> push { 0.0f };
        std::array<Biquad, maxChannels> sidechainFilters;
        float meanSquare = 0.0f, reduction = 0.0f, memory = 0.0f;
        float rmsCoefficient = 0.1f, memoryCoefficient = 0.001f;
    };
}
