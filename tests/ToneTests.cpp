#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"
#include "dsp/Loudness.h"
#include "dsp/ToneShaper.h"

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    juce::AudioBuffer<float> sine (double frequency, float peakDb, double seconds)
    {
        const auto numSamples = (int) (sampleRate * seconds);
        juce::AudioBuffer<float> buffer (2, numSamples);
        const auto amplitude = juce::Decibels::decibelsToGain (peakDb);
        for (int i = 0; i < numSamples; ++i)
            for (int channel = 0; channel < 2; ++channel)
                buffer.setSample (channel, i, amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * frequency * i / sampleRate));
        return buffer;
    }

    // Pink noise (the same power in every octave, roughly like a mix), by
    // Paul Kellet's filter on white noise.
    juce::AudioBuffer<float> pinkNoise (double seconds)
    {
        juce::AudioBuffer<float> buffer (2, (int) (sampleRate * seconds));
        juce::Random random (42);
        for (int channel = 0; channel < 2; ++channel)
        {
            double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto white = random.nextDouble() * 2.0 - 1.0;
                b0 = 0.99886 * b0 + white * 0.0555179;
                b1 = 0.99332 * b1 + white * 0.0750759;
                b2 = 0.96900 * b2 + white * 0.1538520;
                b3 = 0.86650 * b3 + white * 0.3104856;
                b4 = 0.55000 * b4 + white * 0.5329522;
                b5 = -0.7616 * b5 - white * 0.0168980;
                buffer.setSample (channel, i, (float) ((b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362) * 0.05));
                b6 = white * 0.115926;
            }
        }
        return buffer;
    }

    void run (effects::ToneShaper& tone, juce::AudioBuffer<float>& buffer)
    {
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
            tone.process (juce::dsp::AudioBlock<float> (buffer).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, buffer.getNumSamples() - start)));
    }

    // A steady sine's level (dB of its peak) over the last quarter second,
    // from its RMS: the highest sample can miss a high tone's real peak by
    // up to 3 dB (only 4 samples a cycle at 12 kHz), its average can't.
    float sineLevelDb (const juce::AudioBuffer<float>& audio)
    {
        return juce::Decibels::gainToDecibels (audio.getRMSLevel (0, audio.getNumSamples() - 12000, 12000) * juce::MathConstants<float>::sqrt2);
    }

    // How much the tone turns a sine at `frequency` up or down (dB), once settled.
    float responseDb (float air, float focus, float body, double frequency)
    {
        effects::ToneShaper tone;
        tone.prepare (sampleRate);
        tone.setImmediately (air, focus, body);
        auto audio = sine (frequency, -20.0f, 0.5);
        run (tone, audio);
        return sineLevelDb (audio) + 20.0f;
    }

    float lufsOf (const juce::AudioBuffer<float>& buffer)
    {
        metering::ShortTermLoudness meter;
        meter.prepare (sampleRate);
        const float* channels[] = { buffer.getReadPointer (0), buffer.getReadPointer (1) };
        meter.process (channels, 2, buffer.getNumSamples());
        return meter.getLufs();
    }

    void setParameter (ParagonPR7MAudioProcessor& processor, const char* id, float value)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }
}

TEST_CASE ("AIR lifts or lowers the very top, up to 6 dB, and leaves the mids", "[tone]")
{
    CHECK_THAT (responseDb (15.0f, 0.0f, 0.0f, 12000.0), WithinAbs (3.0, 0.05));    // half its gain at 12 kHz
    CHECK (responseDb (15.0f, 0.0f, 0.0f, 20000.0) > 5.0f);
    CHECK (responseDb (15.0f, 0.0f, 0.0f, 1000.0) < 0.1f);
    CHECK_THAT (responseDb (-15.0f, 0.0f, 0.0f, 12000.0), WithinAbs (-3.0, 0.05));
    CHECK_THAT (responseDb (6.0f, 0.0f, 0.0f, 12000.0), WithinAbs (6.0 * 0.4 / 2.0, 0.05));   // +6 on the knob: 2.4 dB
}

TEST_CASE ("FOCUS lifts or lowers the mids round 2.5 kHz, up to 6 dB, and leaves the ends", "[tone]")
{
    CHECK_THAT (responseDb (0.0f, 15.0f, 0.0f, 2500.0), WithinAbs (6.0, 0.05));
    CHECK_THAT (responseDb (0.0f, -15.0f, 0.0f, 2500.0), WithinAbs (-6.0, 0.05));
    CHECK (responseDb (0.0f, 15.0f, 0.0f, 1000.0) > 2.0f);   // broad: still well up an octave and more away
    CHECK (responseDb (0.0f, 15.0f, 0.0f, 60.0) < 0.3f);
    CHECK (responseDb (0.0f, 15.0f, 0.0f, 18000.0) < 1.0f);
}

TEST_CASE ("BODY tilts the low end against the rest", "[tone]")
{
    // +5: the lows up by 5 dB less the loudness it adds back off; the rest
    // down by that much.
    const auto taken = effects::ToneShaper::bodyLoudnessDb (5.0f);
    CHECK_THAT (taken, WithinAbs (0.945, 0.001));
    CHECK_THAT (responseDb (0.0f, 0.0f, 5.0f, 40.0), WithinAbs (5.0 - taken, 0.1));
    CHECK_THAT (responseDb (0.0f, 0.0f, 5.0f, 5000.0), WithinAbs (-taken, 0.05));
    CHECK_THAT (responseDb (0.0f, 0.0f, -5.0f, 40.0), WithinAbs (-5.0 - effects::ToneShaper::bodyLoudnessDb (-5.0f), 0.1));
}

TEST_CASE ("BODY holds the loudness of a mix steady", "[tone]")
{
    // On pink noise, as the meter measures it: within 0.15 LU of untouched
    // across the whole travel.
    const auto noise = pinkNoise (4.0);
    const auto untouched = lufsOf (noise);
    for (const auto body : { -5.0f, -2.5f, 2.5f, 5.0f })
    {
        INFO (body);
        effects::ToneShaper tone;
        tone.prepare (sampleRate);
        tone.setImmediately (0.0f, 0.0f, body);
        auto audio = noise;
        run (tone, audio);
        CHECK_THAT (lufsOf (audio), WithinAbs (untouched, 0.15));
    }
}

TEST_CASE ("AIR and FOCUS's parameters follow the marks printed round their knobs", "[tone]")
{
    ParagonPR7MAudioProcessor processor;
    for (const auto* id : { ParamIDs::air, ParamIDs::focus })
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.getParameters().getParameter (id));
        REQUIRE (parameter != nullptr);
        const auto numMarks = (int) std::size (Parameters::toneMarks);
        for (int mark = 0; mark < numMarks; ++mark)
        {
            INFO (id << " mark " << mark);
            const auto proportion = (float) mark / (float) (numMarks - 1);
            CHECK (juce::exactlyEqual (parameter->convertFrom0to1 (proportion), Parameters::toneMarks[mark]));
            CHECK_THAT (parameter->convertTo0to1 (Parameters::toneMarks[mark]), WithinAbs (proportion, 1.0e-6));
        }
        // Between marks it's straight, in 0.1 steps: a quarter of the way
        // from +6 to +10 is +7.
        CHECK (juce::exactlyEqual (parameter->convertFrom0to1 ((6.0f + 0.25f) / 8.0f), 7.0f));
        CHECK (juce::exactlyEqual (parameter->convertFrom0to1 (0.5f), 0.0f));
    }
}

TEST_CASE ("The tone at 0 passes bit for bit, also after being turned and back", "[tone]")
{
    effects::ToneShaper tone;
    tone.prepare (sampleRate);
    auto audio = pinkNoise (2.0);
    const auto input = audio;
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        // Turned up for the first half second, then back to 0.
        const auto on = start < 24000;
        tone.setAir (on ? 15.0f : 0.0f);
        tone.setFocus (on ? -15.0f : 0.0f);
        tone.setBody (on ? 5.0f : 0.0f);
        tone.process (juce::dsp::AudioBlock<float> (audio).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, audio.getNumSamples() - start)));
    }
    for (int i = 48000; i < audio.getNumSamples(); ++i)   // from a second in
        if (! juce::exactlyEqual (audio.getSample (0, i), input.getSample (0, i)))
            FAIL ("sample " << i << " changed");
}

TEST_CASE ("Turning the tone knobs doesn't click", "[tone]")
{
    effects::ToneShaper tone;
    tone.prepare (sampleRate);
    auto audio = sine (100.0, -6.0f, 2.0);
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        // Every knob from one end to the other at once, and back.
        const auto up = start >= 24000 && start < 72000;
        tone.setAir (up ? 15.0f : -15.0f);
        tone.setFocus (up ? 15.0f : -15.0f);
        tone.setBody (up ? 5.0f : -5.0f);
        tone.process (juce::dsp::AudioBlock<float> (audio).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, audio.getNumSamples() - start)));
    }
    float biggestStep = 0.0f;
    for (int i = 1; i < audio.getNumSamples(); ++i)
        biggestStep = juce::jmax (biggestStep, std::abs (audio.getSample (0, i) - audio.getSample (0, i - 1)));
    // A 100 Hz sine at -6 dBFS, BODY lifting it at most 5 dB: under 0.012 a sample.
    CHECK (biggestStep < 0.015f);
}

TEST_CASE ("The processor runs the tone first, and RESET sets it back", "[processor][tone]")
{
    ParagonPR7MAudioProcessor processor;
    setParameter (processor, ParamIDs::air, 15.0f);
    setParameter (processor, ParamIDs::focus, -15.0f);
    setParameter (processor, ParamIDs::body, 3.0f);
    processor.prepareToPlay (sampleRate, blockSize);

    auto audio = sine (12000.0, -20.0f, 1.0);
    juce::MidiBuffer midi;
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        juce::AudioBuffer<float> block (audio.getArrayOfWritePointers(), 2, start, juce::jmin (blockSize, audio.getNumSamples() - start));
        processor.processBlock (block, midi);
    }
    // At 12 kHz: AIR's +3, FOCUS's cut well past its bell, and BODY's tilt.
    const auto expected = responseDb (15.0f, -15.0f, 3.0f, 12000.0) - 20.0f;
    CHECK_THAT (sineLevelDb (audio), WithinAbs (expected, 0.05));

    processor.resetKnobs();
    for (const auto* id : { ParamIDs::air, ParamIDs::focus, ParamIDs::body })
        CHECK (juce::exactlyEqual (processor.getParameters().getRawParameterValue (id)->load(), 0.0f));
}
