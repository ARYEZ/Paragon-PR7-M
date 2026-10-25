#include "dsp/Limiter.h"

namespace effects
{
    namespace
    {
        float coefficientFor (double sampleRate, float seconds) noexcept
        {
            return 1.0f - std::exp (-1.0f / (juce::jmax (1.0e-5f, seconds) * (float) sampleRate));
        }

        // SOFT's curve: the reduction (dB, 0 or more) for a peak level, with
        // an infinite ratio over a 6 dB knee centred on the threshold.
        float softReductionDb (float levelDb, float thresholdDb) noexcept
        {
            constexpr auto knee = Limiter::softKneeDb;
            const auto over = levelDb - thresholdDb;
            if (2.0f * over < -knee)
                return 0.0f;
            if (2.0f * over <= knee)
                return juce::square (over + knee * 0.5f) / (2.0f * knee);
            return over;
        }
    }

    void Limiter::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        bypass.prepare (sampleRate, maxBlockSize);
        for (auto* smoothed : { &makeUp, &mix, &threshold })
            smoothed->reset (sampleRate, 0.02);
        reset();
    }

    void Limiter::reset()
    {
        envelope = {};
                memory = {};
        for (auto& r : reductionDb)
            r.store (0.0f);
        fresh = true;
    }

    void Limiter::process (juce::AudioBuffer<float>& audio, int numSamples, const Settings& settings) noexcept
    {
        const auto numChannels = juce::jmin (2, audio.getNumChannels());
        const auto makeUpTarget = juce::Decibels::decibelsToGain (settings.makeUpDb);
        const auto thresholdTarget = juce::Decibels::decibelsToGain (settings.thresholdDb);
        const auto mixTarget = juce::jlimit (0.0f, 1.0f, settings.mix);
        if (fresh)
        {
            // Playback is starting: be at the settings from the first sample.
            fresh = false;
            bypass.setImmediately (settings.on);
            makeUp.setCurrentAndTargetValue (makeUpTarget);
            threshold.setCurrentAndTargetValue (thresholdTarget);
            mix.setCurrentAndTargetValue (mixTarget);
        }
        makeUp.setTargetValue (makeUpTarget);
        threshold.setTargetValue (thresholdTarget);
        mix.setTargetValue (mixTarget);

        if (! settings.on && bypass.isSilent())
        {
            envelope = {};
                        memory = {};
            for (auto& r : reductionDb)
                r.store (0.0f);
            return;
        }

        const auto mode = settings.mode;
        const auto fixedRelease = coefficientFor (sampleRate, mode == Mode::soft ? 0.15f : 0.1f);
        const auto memoryCoefficient = coefficientFor (sampleRate, 1.0f);

        std::array<float, 2> most {};
        auto block = juce::dsp::AudioBlock<float> (audio).getSubBlock (0, (size_t) numSamples);
        bypass.process (block, settings.on, [&] (juce::dsp::AudioBlock<float> wet)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const auto ceiling = threshold.getNextValue();
                const auto makeUpGain = makeUp.getNextValue();
                const auto mixNow = mix.getNextValue();

                // The peak envelope: up at once, down at the release rate.
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    const auto c = (size_t) channel;
                    const auto level = std::abs (wet.getSample (channel, i));
                    // AUTO: 30 ms after a short peak, stretching to about 400 ms
                    // under steady limiting.
                    const auto release = mode == Mode::autoRelease ? coefficientFor (sampleRate, 0.03f * (1.0f + 12.0f * memory[c])) : fixedRelease;
                    envelope[c] = level > envelope[c] ? level : envelope[c] + (level - envelope[c]) * release;
                }
                auto linkedEnvelope = envelope;
                if (settings.link && numChannels == 2)
                    linkedEnvelope[0] = linkedEnvelope[1] = juce::jmax (envelope[0], envelope[1]);

                for (int channel = 0; channel < numChannels; ++channel)
                {
                    const auto c = (size_t) channel;
                    float gain = 1.0f;
                    if (mode == Mode::soft)
                    {
                        const auto target = softReductionDb (juce::Decibels::gainToDecibels (linkedEnvelope[c], -120.0f),
                                                             juce::Decibels::gainToDecibels (ceiling));
                        gain = juce::Decibels::decibelsToGain (-target);
                    }
                    else if (linkedEnvelope[c] > ceiling)
                    {
                        gain = ceiling / linkedEnvelope[c];   // exactly at the ceiling: a brick wall
                    }

                    const auto reduction = -juce::Decibels::gainToDecibels (gain, -120.0f);
                    most[c] = juce::jmax (most[c], reduction);
                    memory[c] += (juce::jmin (1.0f, reduction / 6.0f) - memory[c]) * memoryCoefficient;

                    const auto dry = wet.getSample (channel, i);
                    const auto limited = dry * gain * makeUpGain;
                    wet.setSample (channel, i, dry + (limited - dry) * mixNow);
                }
            }
        });

        for (int channel = 0; channel < 2; ++channel)
            reductionDb[(size_t) channel].store (settings.on ? most[(size_t) juce::jmin (channel, numChannels - 1)] : 0.0f);
    }
}
