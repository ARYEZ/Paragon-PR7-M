#pragma once

#include <array>
#include <cmath>
#include <memory>

#include <juce_dsp/juce_dsp.h>

#include "dsp/EffectUtils.h"

namespace effects
{
    // ANALOG: harmonics for dimension and colour, 0 to 10. Our design, after
    // PR7-C's saturation (its curves), made gentler for a whole mix.
    //
    // Along a = ANALOG / 10, the mix is driven harder into a curve (0 to
    // +12 dB, the curve's bend at +6 dBFS before the drive) and more of the
    // result is heard (0 to 100%), so it fades in from nothing: no jump when
    // it's first turned up. The curve is mostly a lopsided, valve-like one
    // (2nd-harmonic warmth) with a quarter of a symmetrical one (3rd-harmonic
    // edge). Quiet sounds come out at the same level (the drive is taken
    // back off after the curve), so ANALOG changes the colour, not the level.
    //
    // It works at 4x and only on what it adds, as VOLUME's clipper does: the
    // audio is oversampled, the change the curve makes worked out there,
    // brought back down (without the DC the lopsided curve leaves) and added
    // to the audio delayed to match. So the harmonics above the audio range
    // can't fold back as aliasing, and at 0 the audio comes out exactly as it
    // went in, only later (getLatency(), the same at any setting).
    class AnalogColour
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr float maxDriveDb = 12.0f;
        static constexpr float headroom = 2.0f;      // the curve bends from +6 dBFS (before the drive)
        static constexpr float oddShare = 0.25f;     // a quarter 3rd-harmonic curve, three quarters 2nd
        static constexpr float valveBias = 0.5f;     // how lopsided the valve curve is (PR7-C's)

        // PR7-C's curves, for one sample in the driven domain (1 = the bend).
        // Both rise with a slope of 1 at silence.
        static float evenCurve (float u) noexcept
        {
            static const auto offset = std::tanh (valveBias);
            static const auto slope = 1.0f - offset * offset;
            return (std::tanh (u + valveBias) - offset) / slope;
        }
        static float oddCurve (float u) noexcept { return std::tanh (u); }

        void prepare (double sampleRate, int maxBlockSize)
        {
            oversampling = std::make_unique<juce::dsp::Oversampling<float>> (maxChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                                                                              true, true);
            oversampling->initProcessing ((size_t) juce::jmax (1, maxBlockSize));
            latency = (int) std::round (oversampling->getLatencyInSamples());
            delay.prepare (latency);
            added.setSize (maxChannels, juce::jmax (1, maxBlockSize));
            amount.reset (sampleRate * 4.0, 0.05);
            dcCoefficient = 1.0f - juce::MathConstants<float>::twoPi * 5.0f / (float) sampleRate;   // 5 Hz
            idleSamples = (int) sampleRate / 5;   // 200 ms
            reset();
        }

        void reset()
        {
            if (oversampling != nullptr)
                oversampling->reset();
            delay.reset();
            dcIn.fill (0.0f);
            dcOut.fill (0.0f);
            quietFor = idleSamples;
            amount.setCurrentAndTargetValue (amount.getTargetValue());
        }

        // ANALOG (0 to 10), smoothed over 50 ms.
        void setAmount (float value) { amount.setTargetValue (juce::jlimit (0.0f, 10.0f, value) / 10.0f); }
        void setAmountImmediately (float value) { amount.setCurrentAndTargetValue (juce::jlimit (0.0f, 10.0f, value) / 10.0f); }

        int getLatency() const noexcept { return latency; }

        void process (const juce::dsp::AudioBlock<float>& block) noexcept
        {
            const auto numChannels = juce::jmin (block.getNumChannels(), (size_t) maxChannels);
            const auto numSamples = block.getNumSamples();
            const auto off = ! amount.isSmoothing() && amount.getTargetValue() <= 0.0f;

            // Off for a while (the filters' and DC blocker's tails gone): only
            // the delay runs, so the audio is exactly as it came in, later.
            quietFor = off ? quietFor + (int) numSamples : 0;
            if (quietFor >= idleSamples)
            {
                if (quietFor - (int) numSamples < idleSamples)   // just gone quiet: start fresh next time
                {
                    oversampling->reset();
                    dcIn.fill (0.0f);
                    dcOut.fill (0.0f);
                }
                delay.process (block);
                return;
            }

            // What the curve adds, at 4x.
            auto change = juce::dsp::AudioBlock<float> (added).getSubsetChannelBlock (0, numChannels).getSubBlock (0, numSamples);
            change.copyFrom (block);
            auto up = oversampling->processSamplesUp (change);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const auto a = amount.getNextValue();
                const auto drive = juce::Decibels::decibelsToGain (maxDriveDb * a) / headroom;
                for (size_t channel = 0; channel < up.getNumChannels(); ++channel)
                {
                    auto& sample = up.getChannelPointer (channel)[i];
                    const auto u = sample * drive;
                    const auto shaped = (evenCurve (u) * (1.0f - oddShare) + oddCurve (u) * oddShare) / drive;
                    sample = a * (shaped - sample);
                }
            }
            oversampling->processSamplesDown (change);

            // Without its DC, onto the audio delayed to match.
            delay.process (block);
            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                auto* out = block.getChannelPointer (channel);
                const auto* add = change.getChannelPointer (channel);
                for (size_t i = 0; i < numSamples; ++i)
                {
                    dcOut[channel] = add[i] - dcIn[channel] + dcCoefficient * dcOut[channel];
                    dcIn[channel] = add[i];
                    out[i] += dcOut[channel];
                }
            }
        }

    private:
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        FixedDelay delay;
        juce::AudioBuffer<float> added;
        juce::SmoothedValue<float> amount { 0.0f };   // 0 to 1, at the 4x rate
        std::array<float, maxChannels> dcIn {}, dcOut {};
        float dcCoefficient = 0.9993f;
        int latency = 0, idleSamples = 9600, quietFor = 0;
    };
}
