#include <catch2/catch_test_macros.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    // A different sine on each channel, so a swapped or mixed channel shows.
    juce::AudioBuffer<float> stereoTone (int numSamples, int numChannels = 2)
    {
        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        for (int i = 0; i < numSamples; ++i)
            for (int channel = 0; channel < numChannels; ++channel)
                buffer.setSample (channel, i, (channel == 0 ? 0.5f : 0.25f)
                                                  * std::sin (juce::MathConstants<float>::twoPi * (channel == 0 ? 440.0f : 1000.0f) * (float) i / (float) sampleRate));
        return buffer;
    }

    void process (ParagonPR7MAudioProcessor& processor, juce::AudioBuffer<float>& buffer)
    {
        juce::MidiBuffer midi;
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        {
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start,
                                            juce::jmin (blockSize, buffer.getNumSamples() - start));
            processor.processBlock (block, midi);
        }
    }

    // `output` is `input` exactly, only `latency` samples later (silence
    // before it): VOLUME's limiter delays the audio.
    bool identical (const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>& output, int latency)
    {
        for (int channel = 0; channel < input.getNumChannels(); ++channel)
            for (int i = 0; i < input.getNumSamples(); ++i)
                if (! juce::exactlyEqual (output.getSample (channel, i), i < latency ? 0.0f : input.getSample (channel, i - latency)))
                    return false;
        return true;
    }
}

TEST_CASE ("At its defaults it passes audio through untouched, bit for bit", "[processor]")
{
    // Below the limiter's ceiling (the tone peaks at -6 dBFS).
    ParagonPR7MAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    REQUIRE (processor.getLatencySamples() > 0);
    const auto input = stereoTone (4800);
    auto output = input;
    process (processor, output);
    CHECK (identical (input, output, processor.getLatencySamples()));
}

TEST_CASE ("Defaults that should be 0 read back as exactly 0", "[processor]")
{
    // A stepped knob's 0 must be exactly 0, not a rounding hair off it (as a
    // fused multiply-add on Apple silicon once made it in PR7-C).
    ParagonPR7MAudioProcessor processor;
    for (auto* parameter : processor.getParameters().processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        REQUIRE (ranged != nullptr);
        INFO (ranged->getParameterID().toStdString());
        const auto raw = processor.getParameters().getRawParameterValue (ranged->getParameterID())->load();
        if (std::abs (raw) < 0.01f)
            CHECK (juce::exactlyEqual (raw, 0.0f));
    }
}

TEST_CASE ("Mono and stereo tracks are supported, mismatched layouts aren't", "[processor]")
{
    ParagonPR7MAudioProcessor processor;
    const auto layout = [] (juce::AudioChannelSet in, juce::AudioChannelSet out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.outputBuses.add (out);
        return l;
    };
    CHECK (processor.checkBusesLayoutSupported (layout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())));
    CHECK (processor.checkBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())));
    CHECK (! processor.checkBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo())));

    REQUIRE (processor.setBusesLayout (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())));
    processor.prepareToPlay (sampleRate, blockSize);
    const auto input = stereoTone (4800, 1);
    auto output = input;
    process (processor, output);
    CHECK (identical (input, output, processor.getLatencySamples()));
}
