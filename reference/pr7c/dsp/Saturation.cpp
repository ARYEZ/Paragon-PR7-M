#include "dsp/Saturation.h"

namespace effects
{
    namespace
    {
        constexpr float valveBias = 0.5f;   // how lopsided the valve curve is
    }

    float Saturation::evenCurve (float u) noexcept
    {
        // tanh, shifted off-centre so it bends sooner one way than the other,
        // scaled back to a slope of 1 at silence.
        static const auto offset = std::tanh (valveBias);
        static const auto slope = 1.0f - offset * offset;
        return (std::tanh (u + valveBias) - offset) / slope;
    }

    float Saturation::oddCurve (float u) noexcept
    {
        return std::tanh (u);
    }

    float Saturation::vcaClip (float u) noexcept
    {
        // Nearly flat-topped above 0.8, with a short rounded knee.
        constexpr auto ceiling = 0.8f, hardness = 8.0f;
        return u / std::pow (1.0f + std::pow (std::abs (u) / ceiling, hardness), 1.0f / hardness);
    }

    void Saturation::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        bypass.prepare (sampleRate, maxBlockSize);
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (2, oversamplingStages,
                                                                          juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampling->initProcessing ((size_t) maxBlockSize);
        warmUp.setSize (2, maxBlockSize);
        const auto oversampledRate = sampleRate * (double) (1 << oversamplingStages);
        for (auto* smoothed : { &drive, &harmonics, &clip, &mix })
            smoothed->reset (oversampledRate, 0.03);
        dcCoefficient = 1.0f - juce::MathConstants<float>::twoPi * 5.0f / (float) sampleRate;   // 5 Hz
        reset();
    }

    void Saturation::reset()
    {
        if (oversampling != nullptr)
            oversampling->reset();
        dcIn = {};
        dcOut = {};
        fresh = true;
    }

    void Saturation::process (juce::AudioBuffer<float>& audio, int numSamples, const Settings& settings) noexcept
    {
        const auto driveTarget = juce::Decibels::decibelsToGain (settings.driveDb);
        const auto harmonicsTarget = juce::jlimit (0.0f, 1.0f, settings.harmonics);
        const auto clipTarget = settings.vcaClip ? 1.0f : 0.0f;
        const auto mixTarget = juce::jlimit (0.0f, 1.0f, settings.mix);
        if (fresh)
        {
            fresh = false;
            bypass.setImmediately (settings.on);
            drive.setCurrentAndTargetValue (driveTarget);
            harmonics.setCurrentAndTargetValue (harmonicsTarget);
            clip.setCurrentAndTargetValue (clipTarget);
            mix.setCurrentAndTargetValue (mixTarget);
        }
        drive.setTargetValue (driveTarget);
        harmonics.setTargetValue (harmonicsTarget);
        clip.setTargetValue (clipTarget);
        mix.setTargetValue (mixTarget);

        const auto numChannels = juce::jmin (2, audio.getNumChannels());
        if (! settings.on && bypass.isSilent())
        {
            // Off: keep the filters running on a copy, so they're warm when
            // it's switched on.
            for (int channel = 0; channel < numChannels; ++channel)
                warmUp.copyFrom (channel, 0, audio, channel, 0, numSamples);
            auto copy = juce::dsp::AudioBlock<float> (warmUp).getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) numChannels);
            oversampling->processSamplesUp (copy);
            oversampling->processSamplesDown (copy);
            return;
        }

        auto block = juce::dsp::AudioBlock<float> (audio).getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) numChannels);
        bypass.process (block, settings.on, [&] (juce::dsp::AudioBlock<float> wet)
        {
            auto up = oversampling->processSamplesUp (wet);
            const auto upSamples = (int) up.getNumSamples();
            for (int i = 0; i < upSamples; ++i)
            {
                const auto g = drive.getNextValue();
                const auto h = harmonics.getNextValue();
                const auto c = clip.getNextValue();
                const auto m = mix.getNextValue();
                for (size_t channel = 0; channel < up.getNumChannels(); ++channel)
                {
                    const auto dry = up.getSample ((int) channel, i);
                    auto u = dry * g / headroom;
                    u += (vcaClip (u) - u) * c;
                    const auto shaped = evenCurve (u) * (1.0f - h) + oddCurve (u) * h;
                    const auto saturated = shaped * headroom / g;   // the same level as the dry for quiet sounds
                    up.setSample ((int) channel, i, dry + (saturated - dry) * m);
                }
            }
            oversampling->processSamplesDown (wet);

            // Remove the DC the valve curve leaves behind.
            for (size_t channel = 0; channel < wet.getNumChannels(); ++channel)
            {
                auto* samples = wet.getChannelPointer (channel);
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto x = samples[i];
                    dcOut[channel] = x - dcIn[channel] + dcCoefficient * dcOut[channel];
                    dcIn[channel] = x;
                    samples[i] = dcOut[channel];
                }
            }
        });
    }
}
