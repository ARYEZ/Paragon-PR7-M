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
    // Why a clipper: a clean limiter can only turn the whole mix down round
    // each peak, so on a dense mix pushing harder mostly just makes it turn
    // down harder (measured: VOLUME 9 to 15 added 0.4 LU). A clipper shaves
    // the peaks off instead and leaves the rest, so the mix keeps getting
    // louder all the way up the knob (on the same mix, VOLUME 15 is about
    // 11 LU louder than 0). It's how the loudness of PR7-C's OUTPUT TRIM
    // pushed past 0 dBFS comes about, here kept under the ceiling; the
    // limiter after it catches what's left between the samples.
    //
    // The curve: straight up to the start of a soft knee, then rounding off
    // into the ceiling (-1 dBFS). The knee widens with VOLUME, 1 dB per dB up
    // to 6 dB (from -7 dBFS), so at VOLUME 0 nothing under the ceiling is
    // touched, and the more it's pushed the rounder (less harsh) it shaves.
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
        static constexpr float ceilingDb = -1.0f;
        static constexpr float maxKneeDb = 6.0f;

        // Where the rounding starts, for a VOLUME (dB).
        static float kneeStartDb (float volumeDb) noexcept { return ceilingDb - juce::jlimit (0.0f, maxKneeDb, volumeDb); }

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
            const auto ceiling = juce::Decibels::decibelsToGain (ceilingDb);
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
