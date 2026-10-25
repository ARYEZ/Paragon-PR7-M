#pragma once

#include <cmath>
#include <memory>

#include <juce_dsp/juce_dsp.h>

#include "dsp/EffectUtils.h"

namespace effects
{
    // VOLUME's first half: the gain (0 to +15 dB) and a soft clipper, before
    // the true-peak limiter. Our own design.
    //
    // Clean first: through most of VOLUME's travel the limiter does all the
    // work (its quick, music-following release gets a dense mix about as loud
    // as limiting can without changing the waveform). Measured on a dense
    // mix through the plugin: -15.5 LUFS at 0, -8.7 at 9 with the waveform
    // untouched to -41 dB. Beyond that no design we tried (faster or slower
    // releases, PR7-C's AUTO, two bands with the lows limited apart) got
    // louder: the rest can only come from cutting into the waveform. So from
    // about 10 the clipper starts to, evenly: -8.2 LUFS at 10 (distortion
    // -31 dB), -7.5 at 12 (-26 dB), -5.9 at 15 (-21 dB, pushed hard on
    // purpose). Every step adds a little loudness and a little edge.
    //
    // (0.6.0 and 0.7.0 had the clipper do the work from the start, into the
    // limiter's ceiling: louder sooner but distorting badly, -30 dB by
    // VOLUME 6 and -18 dB at the top. The owner: "the volume is just causing
    // overload now".)
    //
    // The law: a peak at -1 dBFS, turned up by VOLUME, goes 6 dB over the
    // clipper's ceiling at 10 and 12 dB over at 15 (1.2 dB more a step);
    // below 10 the ceiling rises 2 dB a step, out of reach (it first touches
    // such a peak, gently, at about 7, where shaving that little does
    // nothing you'd hear). A 3 dB soft knee under the ceiling. The limiter
    // after it takes what's left down under -1 dBTP.
    //
    // It works at 4x the sample rate, and only on what it shaves off: the
    // audio is oversampled, the shaving (clipped minus unclipped) worked out
    // there and brought back down, and added to the audio delayed to match.
    // So the harmonics the shaving makes can't fold back down as harsh
    // aliasing, and where nothing is shaved the audio comes out exactly as
    // it went in, only later (getLatency(), reported with the limiter's).
    class VolumeClipper
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr float limiterCeilingDb = -1.0f;
        static constexpr float kneeDb = 3.0f;

        // The clipper's ceiling and where its rounding starts, for a VOLUME
        // (dB), as the law above: +3 dBFS at 10, +2 at 15, +23 at 0.
        static float ceilingDbFor (float volumeDb) noexcept
        {
            const auto v = juce::jlimit (0.0f, 15.0f, volumeDb);
            // How far a peak at -1 dBFS, turned up by VOLUME, goes over the
            // clipper's ceiling: 6 dB at 10, 12 dB at 15.
            const auto peakDb = v + limiterCeilingDb;
            return v >= 10.0f ? peakDb - (6.0f + 1.2f * (v - 10.0f)) : 3.0f + 2.0f * (10.0f - v);
        }
        static float kneeStartDb (float volumeDb) noexcept { return ceilingDbFor (volumeDb) - kneeDb; }

        // The curve, for a level (positive, linear) and a knee start (linear).
        static float shape (float level, float kneeStart, float ceiling) noexcept
        {
            if (level <= kneeStart)
                return level;
            if (kneeStart >= ceiling)
                return ceiling;   // no knee: a hard ceiling
            const auto width = ceiling - kneeStart;
            return kneeStart + width * std::tanh ((level - kneeStart) / width);
        }

        void prepare (double sampleRate, int maxBlockSize)
        {
            oversampling = std::make_unique<juce::dsp::Oversampling<float>> (maxChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                                                                              true, true);
            oversampling->initProcessing ((size_t) juce::jmax (1, maxBlockSize));
            latency = (int) std::round (oversampling->getLatencyInSamples());
            delay.prepare (latency);
            shaved.setSize (maxChannels, juce::jmax (1, maxBlockSize));
            volume.reset (sampleRate, 0.05);
            reset();
        }

        void reset()
        {
            if (oversampling != nullptr)
                oversampling->reset();
            delay.reset();
            volume.setCurrentAndTargetValue (volume.getTargetValue());
        }

        // VOLUME (dB, 0 to 15), smoothed over 50 ms.
        void setVolume (float db) { volume.setTargetValue (juce::jlimit (0.0f, 15.0f, db)); }
        void setVolumeImmediately (float db) { volume.setCurrentAndTargetValue (juce::jlimit (0.0f, 15.0f, db)); }

        int getLatency() const noexcept { return latency; }

        // Turns the block up and shaves its peaks, in place; returns the most
        // it shaved off (dB, positive).
        float process (const juce::dsp::AudioBlock<float>& block) noexcept
        {
            const auto numChannels = juce::jmin (block.getNumChannels(), (size_t) maxChannels);
            const auto numSamples = block.getNumSamples();

            // The gain.
            if (volume.isSmoothing() || ! juce::exactlyEqual (volume.getTargetValue(), 0.0f))
                for (size_t i = 0; i < numSamples; ++i)
                {
                    const auto gain = juce::Decibels::decibelsToGain (volume.getNextValue());
                    for (size_t channel = 0; channel < numChannels; ++channel)
                        block.getChannelPointer (channel)[i] *= gain;
                }

            // The shaving, at 4x: clipped minus unclipped.
            auto shaving = juce::dsp::AudioBlock<float> (shaved).getSubsetChannelBlock (0, numChannels).getSubBlock (0, numSamples);
            shaving.copyFrom (block);
            auto up = oversampling->processSamplesUp (shaving);
            const auto ceiling = juce::Decibels::decibelsToGain (ceilingDbFor (volume.getCurrentValue()));
            const auto kneeStart = juce::Decibels::decibelsToGain (kneeStartDb (volume.getCurrentValue()));
            auto mostRatio = 1.0f;
            for (size_t channel = 0; channel < up.getNumChannels(); ++channel)
            {
                auto* samples = up.getChannelPointer (channel);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    const auto level = std::abs (samples[i]);
                    if (level <= kneeStart)
                    {
                        samples[i] = 0.0f;
                        continue;
                    }
                    const auto shaped = shape (level, kneeStart, ceiling);
                    mostRatio = juce::jmax (mostRatio, level / shaped);
                    samples[i] = -std::copysign (level - shaped, samples[i]);   // takes the excess off, towards 0
                }
            }
            oversampling->processSamplesDown (shaving);

            // The audio, delayed to line up, plus the shaving.
            delay.process (block);
            for (size_t channel = 0; channel < numChannels; ++channel)
                juce::FloatVectorOperations::add (block.getChannelPointer (channel), shaving.getChannelPointer (channel), (int) numSamples);
            return juce::Decibels::gainToDecibels (mostRatio);
        }

    private:
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        FixedDelay delay;
        juce::AudioBuffer<float> shaved;
        juce::SmoothedValue<float> volume { 0.0f };   // dB
        int latency = 0;
    };
}
