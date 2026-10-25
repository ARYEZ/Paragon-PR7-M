#include "dsp/ConsoleColour.h"

namespace effects
{
    namespace
    {
        // The hum's harmonics: 50 Hz mains, and the 100 and 150 Hz a power
        // supply adds. Scaled so the whole hum has an RMS of 1.
        constexpr float humParts[] = { 1.0f, 0.5f, 0.3f };
        const float humScale = std::sqrt (2.0f / (1.0f + 0.25f + 0.09f));
    }

    void ConsoleColour::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        bypass.prepare (sampleRate, maxBlockSize);
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampling->initProcessing ((size_t) maxBlockSize);
        warmUp.setSize (2, maxBlockSize);
        for (size_t side = 0; side < 2; ++side)
        {
            second[side].reset (sampleRate * 2.0, 0.03);
            third[side].reset (sampleRate * 2.0, 0.03);
            hiss[side].reset (sampleRate, 0.03);
            hum[side].reset (sampleRate, 0.03);
        }
        dcCoefficient = 1.0f - juce::MathConstants<float>::twoPi * 5.0f / (float) sampleRate;
        reset();
    }

    void ConsoleColour::reset()
    {
        if (oversampling != nullptr)
            oversampling->reset();
        dcIn = {};
        dcOut = {};
        fresh = true;
    }

    void ConsoleColour::process (juce::AudioBuffer<float>& audio, int numSamples, const Settings& settings) noexcept
    {
        const auto thd = juce::jlimit (0.0f, 1.0f, settings.thd);
        const auto on = thd > 0.0f;
        const auto noise = juce::jlimit (0.0f, 1.0f, settings.noise);
        for (size_t side = 0; side < 2; ++side)
        {
            const auto secondTarget = secondAtFull * thd * settings.second[side];
            const auto thirdTarget = thirdAtFull * thd * settings.third[side];
            // The noise rises with the square of V GAIN: from nothing, smoothly,
            // to -76 dBFS at 50 and -64 at 100.
            const auto hissTarget = noise > 0.0f ? juce::Decibels::decibelsToGain (hissDbAtFull + settings.hissDb[side]) * noise * noise : 0.0f;
            const auto humTarget = noise > 0.0f ? juce::Decibels::decibelsToGain (humDbAtFull + settings.humDb[side]) * noise * noise : 0.0f;
            if (fresh)
            {
                second[side].setCurrentAndTargetValue (secondTarget);
                third[side].setCurrentAndTargetValue (thirdTarget);
                hiss[side].setCurrentAndTargetValue (hissTarget);
                hum[side].setCurrentAndTargetValue (humTarget);
            }
            second[side].setTargetValue (secondTarget);
            third[side].setTargetValue (thirdTarget);
            hiss[side].setTargetValue (hissTarget);
            hum[side].setTargetValue (humTarget);
        }
        if (fresh)
        {
            fresh = false;
            bypass.setImmediately (on);
        }

        const auto numChannels = juce::jmin (2, audio.getNumChannels());
        if (! on && bypass.isSilent())
        {
            // Off: keep the filters warm on a copy, so switching on doesn't ring.
            for (int channel = 0; channel < numChannels; ++channel)
                warmUp.copyFrom (channel, 0, audio, channel, 0, numSamples);
            auto copy = juce::dsp::AudioBlock<float> (warmUp).getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) numChannels);
            oversampling->processSamplesUp (copy);
            oversampling->processSamplesDown (copy);
            for (size_t side = 0; side < 2; ++side)
            {
                second[side].setCurrentAndTargetValue (second[side].getTargetValue());
                third[side].setCurrentAndTargetValue (third[side].getTargetValue());
            }
        }
        else
        {
            auto block = juce::dsp::AudioBlock<float> (audio).getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) numChannels);
            bypass.process (block, on, [&] (juce::dsp::AudioBlock<float> wet)
            {
                auto up = oversampling->processSamplesUp (wet);
                const auto upSamples = (int) up.getNumSamples();
                for (size_t channel = 0; channel < up.getNumChannels(); ++channel)
                {
                    auto* samples = up.getChannelPointer (channel);
                    for (int i = 0; i < upSamples; ++i)
                    {
                        const auto x = samples[i];
                        samples[i] = x + second[channel].getNextValue() * x * x + third[channel].getNextValue() * x * x * x;
                    }
                }
                // A mono track: keep the right side's glides in step.
                for (auto channel = up.getNumChannels(); channel < 2; ++channel)
                {
                    second[channel].skip (upSamples);
                    third[channel].skip (upSamples);
                }
                oversampling->processSamplesDown (wet);

                // Remove the DC offset x² leaves.
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

        addNoise (audio, numSamples, settings);
    }

    void ConsoleColour::addNoise (juce::AudioBuffer<float>& audio, int numSamples, const Settings&) noexcept
    {
        const auto silent = [this]
        {
            for (size_t side = 0; side < 2; ++side)
                if (hiss[side].isSmoothing() || hum[side].isSmoothing() || hiss[side].getCurrentValue() > 0.0f || hum[side].getCurrentValue() > 0.0f)
                    return false;
            return true;
        };
        if (silent())
        {
            humPhase = 0.0;
            return;
        }

        const auto numChannels = juce::jmin (2, audio.getNumChannels());
        const auto humStep = juce::MathConstants<double>::twoPi * 50.0 / sampleRate;
        const auto uniformScale = std::sqrt (3.0f);   // a uniform random number's RMS is 1 / sqrt 3
        for (int i = 0; i < numSamples; ++i)
        {
            const auto p = humPhase;
            const auto mains = humScale * (humParts[0] * (float) std::sin (p) + humParts[1] * (float) std::sin (2.0 * p) + humParts[2] * (float) std::sin (3.0 * p));
            humPhase += humStep;
            if (humPhase > juce::MathConstants<double>::twoPi)
                humPhase -= juce::MathConstants<double>::twoPi;
            for (size_t side = 0; side < 2; ++side)
            {
                // xorshift32: cheap, and fine for hiss.
                auto& r = random[side];
                r ^= r << 13;
                r ^= r >> 17;
                r ^= r << 5;
                const auto white = ((float) r / 2147483648.0f - 1.0f) * uniformScale;
                const auto added = white * hiss[side].getNextValue() + mains * hum[side].getNextValue();
                if ((int) side < numChannels)
                    audio.getWritePointer ((int) side)[i] += added;
            }
        }
    }
}
