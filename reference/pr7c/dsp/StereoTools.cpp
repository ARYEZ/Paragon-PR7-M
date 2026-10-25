#include "dsp/StereoTools.h"

namespace effects
{
    namespace
    {
        constexpr int updateInterval = 8;   // samples between filter updates while the frequency glides
    }

    void StereoTools::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        monoFade.prepare (sampleRate, maxBlockSize);
        widthFade.prepare (sampleRate, maxBlockSize);
        monoFrequency.reset (sampleRate, 0.05);
        width.reset (sampleRate, 0.03);
        reset();
    }

    void StereoTools::reset()
    {
        for (auto* filters : { &sideHighPass, &midHighPass, &midLowPass })
            for (auto& biquad : *filters)
                biquad.reset();
        fresh = true;
    }

    void StereoTools::updateFilters (float frequency) noexcept
    {
        const auto highPass = Biquad::highPass (sampleRate, frequency), lowPass = Biquad::lowPass (sampleRate, frequency);
        for (size_t i = 0; i < 2; ++i)
        {
            sideHighPass[i].c = highPass;
            midHighPass[i].c = highPass;
            midLowPass[i].c = lowPass;
        }
    }

    void StereoTools::process (juce::AudioBuffer<float>& audio, int numSamples, const Settings& settings) noexcept
    {
        const auto widthTarget = juce::jlimit (0.0f, 2.0f, settings.width);
        if (fresh)
        {
            fresh = false;
            monoFade.setImmediately (settings.monoOn);
            widthFade.setImmediately (settings.widthOn);
            monoFrequency.setCurrentAndTargetValue (settings.monoFrequency);
            width.setCurrentAndTargetValue (widthTarget);
        }
        monoFrequency.setTargetValue (settings.monoFrequency);
        width.setTargetValue (widthTarget);
        if (audio.getNumChannels() < 2)
            return;

        auto block = juce::dsp::AudioBlock<float> (audio).getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, 2);

        // MONO MAKER.
        if (! settings.monoOn && monoFade.isSilent())
        {
            monoFrequency.setCurrentAndTargetValue (settings.monoFrequency);
            for (auto* filters : { &sideHighPass, &midHighPass, &midLowPass })
                for (auto& biquad : *filters)
                    biquad.reset();
        }
        else
        {
            monoFade.process (block, settings.monoOn, [&] (juce::dsp::AudioBlock<float> wet)
            {
                auto* left = wet.getChannelPointer (0);
                auto* right = wet.getChannelPointer (1);
                for (int start = 0; start < numSamples; start += updateInterval)
                {
                    const auto length = juce::jmin (updateInterval, numSamples - start);
                    updateFilters (monoFrequency.skip (length));
                    for (int i = start; i < start + length; ++i)
                    {
                        const auto mid = 0.5f * (left[i] + right[i]);
                        auto side = 0.5f * (left[i] - right[i]);
                        side = sideHighPass[1].process (sideHighPass[0].process (side));
                        // A Linkwitz-Riley crossover's two halves added: the same level, the same timing as the side.
                        const auto low = midLowPass[1].process (midLowPass[0].process (mid));
                        const auto high = midHighPass[1].process (midHighPass[0].process (mid));
                        const auto newMid = low + high;
                        left[i] = newMid + side;
                        right[i] = newMid - side;
                    }
                }
            });
        }

        // STEREO WIDTH.
        if (! settings.widthOn && widthFade.isSilent())
        {
            width.setCurrentAndTargetValue (widthTarget);
            return;
        }
        widthFade.process (block, settings.widthOn, [&] (juce::dsp::AudioBlock<float> wet)
        {
            auto* left = wet.getChannelPointer (0);
            auto* right = wet.getChannelPointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                const auto w = width.getNextValue();
                const auto mid = 0.5f * (left[i] + right[i]);
                const auto side = 0.5f * (left[i] - right[i]) * w;
                left[i] = mid + side;
                right[i] = mid - side;
            }
        });
    }
}
