#pragma once

#include <array>
#include <cmath>

#include <juce_dsp/juce_dsp.h>

#include "dsp/Biquad.h"

namespace effects
{
    // BODY, FOCUS and AIR: the tone, at the start of the chain. Our own
    // design: three gentle filters (no latency), each bypassed exactly at 0.
    //
    //   AIR    a high shelf, half its gain at 12 kHz: the air above the
    //          cymbals. -15 to +15 on the knob is -6 to +6 dB.
    //   FOCUS  a broad bell at 2.5 kHz (about two octaves wide), where voices
    //          and leads are heard clearly. -15 to +15 is -6 to +6 dB.
    //   BODY   a low shelf, half its gain at 200 Hz, -5 to +5 dB, with the
    //          whole mix turned the other way by what the shelf adds to its
    //          loudness: more or less body, not louder or quieter. In effect
    //          a tilt round the low mids. (Fitted on pink noise through the
    //          loudness meter's K-weighting: the shelf adds 0.138 g +
    //          0.0102 g^2 LU for g dB; a boost adds more than a cut takes.)
    //
    // Each knob's change is smoothed over 50 ms, with the filter worked out
    // afresh every sample while it moves.
    class ToneShaper
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr float airHz = 12000.0f, focusHz = 2500.0f, focusQ = 0.6f, bodyHz = 200.0f;
        static constexpr float dbPerStep = 0.4f;   // AIR and FOCUS: 15 steps is 6 dB

        // The loudness BODY's shelf adds (LU) for its gain (dB), which it
        // takes back off the whole mix.
        static float bodyLoudnessDb (float gainDb) noexcept { return 0.138f * gainDb + 0.0102f * gainDb * gainDb; }

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            for (auto* band : { &air, &focus, &body })
                band->gain.reset (sampleRate, 0.05);
            reset();
        }

        void reset()
        {
            for (auto* band : { &air, &focus, &body })
            {
                for (auto& filter : band->filters)
                    filter.reset();
                band->gain.setCurrentAndTargetValue (band->gain.getTargetValue());
                band->update (*this);
            }
        }

        // The knobs: AIR and FOCUS -15 to +15, BODY -5 to +5.
        void setAir (float steps) { air.gain.setTargetValue (juce::jlimit (-15.0f, 15.0f, steps) * dbPerStep); }
        void setFocus (float steps) { focus.gain.setTargetValue (juce::jlimit (-15.0f, 15.0f, steps) * dbPerStep); }
        void setBody (float steps) { body.gain.setTargetValue (juce::jlimit (-5.0f, 5.0f, steps)); }
        void setImmediately (float airSteps, float focusSteps, float bodySteps)
        {
            setAir (airSteps);
            setFocus (focusSteps);
            setBody (bodySteps);
            reset();
        }

        void process (const juce::dsp::AudioBlock<float>& block) noexcept
        {
            body.process (*this, block, true);
            focus.process (*this, block, false);
            air.process (*this, block, false);
        }

    private:
        enum class Kind { highShelf, bell, lowShelf };

        struct Band
        {
            explicit Band (Kind k) : kind (k) {}

            Kind kind;
            juce::SmoothedValue<float> gain { 0.0f };   // dB
            std::array<Biquad, maxChannels> filters;
            float compensation = 1.0f;

            void update (const ToneShaper& owner) noexcept
            {
                const auto db = (double) gain.getCurrentValue();
                Biquad::Coefficients c;
                switch (kind)
                {
                    case Kind::highShelf: c = Biquad::highShelf (owner.sampleRate, airHz, db); break;
                    case Kind::bell:      c = Biquad::bell (owner.sampleRate, focusHz, focusQ, db); break;
                    case Kind::lowShelf:  c = Biquad::lowShelf (owner.sampleRate, bodyHz, db); break;
                }
                for (auto& filter : filters)
                    filter.c = c;
                compensation = juce::Decibels::decibelsToGain (-bodyLoudnessDb ((float) db));
            }

            void process (const ToneShaper& owner, const juce::dsp::AudioBlock<float>& block, bool compensated) noexcept
            {
                const auto numChannels = (int) juce::jmin (block.getNumChannels(), (size_t) maxChannels);
                if (! gain.isSmoothing() && juce::exactlyEqual (gain.getTargetValue(), 0.0f))
                {
                    // At 0: untouched, and fresh for when it's turned.
                    if (! juce::exactlyEqual (gain.getCurrentValue(), 0.0f) || ! juce::exactlyEqual (filters[0].z1, 0.0) || ! juce::exactlyEqual (filters[0].z2, 0.0))
                    {
                        gain.setCurrentAndTargetValue (0.0f);
                        for (auto& filter : filters)
                            filter.reset();
                    }
                    return;
                }

                for (size_t i = 0; i < block.getNumSamples(); ++i)
                {
                    if (gain.isSmoothing())
                    {
                        gain.getNextValue();
                        update (owner);
                    }
                    for (int channel = 0; channel < numChannels; ++channel)
                    {
                        auto& sample = block.getChannelPointer ((size_t) channel)[i];
                        sample = filters[(size_t) channel].process (sample);
                        if (compensated)
                            sample *= compensation;
                    }
                }
            }
        };

        double sampleRate = 48000.0;
        Band air { Kind::highShelf }, focus { Kind::bell }, body { Kind::lowShelf };
    };
}
