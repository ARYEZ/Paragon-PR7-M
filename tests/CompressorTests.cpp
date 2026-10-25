#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"
#include "dsp/GlueCompressor.h"

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

    // Runs the compressor over `buffer` in blocks; returns the most it turned down (dB).
    float compress (effects::GlueCompressor& compressor, juce::AudioBuffer<float>& buffer)
    {
        auto most = 0.0f;
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        {
            const auto n = juce::jmin (blockSize, buffer.getNumSamples() - start);
            most = juce::jmax (most, compressor.process (juce::dsp::AudioBlock<float> (buffer).getSubBlock ((size_t) start, (size_t) n)));
        }
        return most;
    }

    // The level of the last half second (dB, a sine's peak).
    float settledDb (const juce::AudioBuffer<float>& buffer, int channel = 0)
    {
        return juce::Decibels::gainToDecibels (buffer.getMagnitude (channel, buffer.getNumSamples() - 24000, 24000));
    }

    void setParameter (ParagonPR7MAudioProcessor& processor, const char* id, float value)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }
}

TEST_CASE ("PUSH's curve goes from nothing to hard", "[push]")
{
    const auto off = effects::GlueCompressor::curveFor (0.0f);
    CHECK (juce::exactlyEqual (off.ratio, 1.0f));
    CHECK (juce::exactlyEqual (off.makeUpDb, 0.0f));
    CHECK_THAT (off.thresholdDb, WithinAbs (-6.0, 1.0e-6));

    const auto hard = effects::GlueCompressor::curveFor (15.0f);
    CHECK_THAT (hard.thresholdDb, WithinAbs (-24.0, 1.0e-4));
    CHECK_THAT (hard.ratio, WithinAbs (3.0, 1.0e-6));
    CHECK_THAT (hard.attackMs, WithinAbs (10.0, 1.0e-4));
    CHECK_THAT (hard.releaseMs, WithinAbs (150.0, 1.0e-3));
    CHECK_THAT (hard.makeUpDb, WithinAbs (6.0, 1.0e-4));   // half of (24 - 6) x (1 - 1/3)

    // Every step harder: a lower threshold, a higher ratio, faster.
    for (float push = 0.5f; push <= 15.0f; push += 0.5f)
    {
        INFO (push);
        const auto before = effects::GlueCompressor::curveFor (push - 0.5f), now = effects::GlueCompressor::curveFor (push);
        CHECK (now.thresholdDb < before.thresholdDb);
        CHECK (now.ratio > before.ratio);
        CHECK (now.attackMs < before.attackMs);
        CHECK (now.releaseMs < before.releaseMs);
        CHECK (now.makeUpDb > before.makeUpDb);
    }
}

TEST_CASE ("At PUSH 0 the audio passes bit for bit", "[push]")
{
    effects::GlueCompressor compressor;
    compressor.prepare (sampleRate);
    const auto input = sine (1000.0, 0.0f, 0.5);
    auto output = input;
    CHECK (compress (compressor, output) == 0.0f);
    for (int i = 0; i < input.getNumSamples(); ++i)
        if (! juce::exactlyEqual (input.getSample (0, i), output.getSample (0, i)))
            FAIL ("sample " << i << " changed");
}

TEST_CASE ("PUSH turns a loud steady tone down as its curve says", "[push]")
{
    // A 1 kHz tone peaking at -6 dBFS, at PUSH 15: 12 dB over the threshold
    // (-24) at 3:1 is 12 dB down, then +6 dB make-up: -12 dBFS.
    effects::GlueCompressor compressor;
    compressor.prepare (sampleRate);
    compressor.setPushImmediately (15.0f);
    auto audio = sine (1000.0, -6.0f, 2.0);
    compress (compressor, audio);
    CHECK_THAT (settledDb (audio), WithinAbs (-12.0, 0.25));

    // Halfway: threshold -15, 2:1, 9 dB over is 4.5 down, +2.25 make-up.
    effects::GlueCompressor half;
    half.prepare (sampleRate);
    half.setPushImmediately (7.5f);
    auto audio2 = sine (1000.0, -6.0f, 2.0);
    compress (half, audio2);
    CHECK_THAT (settledDb (audio2), WithinAbs (-6.0 - 4.5 + 2.25, 0.25));
}

TEST_CASE ("PUSH leaves quiet passages alone, but for its make-up gain", "[push]")
{
    effects::GlueCompressor compressor;
    compressor.prepare (sampleRate);
    compressor.setPushImmediately (15.0f);
    auto audio = sine (1000.0, -40.0f, 1.0);   // far under the threshold
    CHECK (compress (compressor, audio) < 0.001f);
    CHECK_THAT (settledDb (audio), WithinAbs (-40.0 + 6.0, 0.01));
}

TEST_CASE ("More PUSH turns the same mix down more", "[push]")
{
    auto last = -1.0f;
    for (const auto push : { 1.5f, 4.5f, 7.5f, 10.5f, 15.0f })
    {
        INFO (push);
        effects::GlueCompressor compressor;
        compressor.prepare (sampleRate);
        compressor.setPushImmediately (push);
        auto audio = sine (500.0, -8.0f, 1.0);
        const auto reduction = compress (compressor, audio);
        CHECK (reduction > last);
        last = reduction;
    }
}

TEST_CASE ("PUSH's attack lets a drum hit's start through, then holds it down", "[push]")
{
    // From quiet to loud at once: at PUSH 15 (10 ms attack) the first
    // couple of milliseconds come through nearly whole; 100 ms in it's down.
    effects::GlueCompressor compressor;
    compressor.prepare (sampleRate);
    compressor.setPushImmediately (15.0f);
    auto audio = sine (1000.0, -40.0f, 1.0);
    auto loud = sine (1000.0, -6.0f, 1.0);
    for (int channel = 0; channel < 2; ++channel)
        audio.copyFrom (channel, 24000, loud, channel, 24000, 24000);
    compress (compressor, audio);
    const auto makeUp = 6.0f;
    const auto start = juce::Decibels::gainToDecibels (audio.getMagnitude (0, 24000, 96)) - makeUp;   // the first 2 ms
    const auto later = juce::Decibels::gainToDecibels (audio.getMagnitude (0, 24000 + 4800, 480)) - makeUp;   // 100 ms in
    CHECK (start > -6.0f - 2.0f);
    CHECK (later < -6.0f - 9.0f);
}

TEST_CASE ("PUSH lets go after a loud passage, more slowly after a long one", "[push]")
{
    const auto recoveryAfter = [] (double loudSeconds)
    {
        effects::GlueCompressor compressor;
        compressor.prepare (sampleRate);
        compressor.setPushImmediately (15.0f);
        const auto loudSamples = (int) (sampleRate * loudSeconds);
        auto audio = sine (1000.0, -40.0f, loudSeconds + 1.0);
        auto loud = sine (1000.0, -6.0f, loudSeconds);
        for (int channel = 0; channel < 2; ++channel)
            audio.copyFrom (channel, 0, loud, channel, 0, loudSamples);
        compress (compressor, audio);
        // How far the quiet tone still sits under its level, 150 ms after.
        const auto after = loudSamples + 7200;
        return (-40.0f + 6.0f) - juce::Decibels::gainToDecibels (audio.getMagnitude (0, after, 480));
    };
    const auto shortHit = recoveryAfter (0.05), longPassage = recoveryAfter (4.0);
    CHECK (shortHit < 6.0f);
    CHECK (longPassage > shortHit);
    CHECK (longPassage < 12.0f);
}

TEST_CASE ("PUSH doesn't pump on the deep bass", "[push]")
{
    // A 40 Hz tone and a 1 kHz one at the same level: the bass is turned down
    // much less, as the detector leaves out what's under about 80 Hz.
    const auto reductionFor = [] (double frequency)
    {
        effects::GlueCompressor compressor;
        compressor.prepare (sampleRate);
        compressor.setPushImmediately (15.0f);
        auto audio = sine (frequency, -6.0f, 2.0);
        compress (compressor, audio);
        return -6.0f + 6.0f - settledDb (audio);
    };
    CHECK (reductionFor (40.0) < reductionFor (1000.0) - 6.0f);
}

TEST_CASE ("PUSH turns both channels down together", "[push]")
{
    effects::GlueCompressor compressor;
    compressor.prepare (sampleRate);
    compressor.setPushImmediately (15.0f);
    auto audio = sine (1000.0, -6.0f, 2.0);
    audio.applyGain (1, 0, audio.getNumSamples(), 0.1f);   // the right 20 dB down
    compress (compressor, audio);
    CHECK_THAT (settledDb (audio, 0) - settledDb (audio, 1), WithinAbs (20.0, 0.01));
}

TEST_CASE ("Turning PUSH up or down doesn't click", "[push]")
{
    effects::GlueCompressor compressor;
    compressor.prepare (sampleRate);
    auto audio = sine (100.0, -6.0f, 3.0);
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        // 0, then 15 at once, then back to 0 at once.
        compressor.setPush (start >= 48000 && start < 96000 ? 15.0f : 0.0f);
        compressor.process (juce::dsp::AudioBlock<float> (audio).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, audio.getNumSamples() - start)));
    }
    float biggestStep = 0.0f;
    for (int i = 1; i < audio.getNumSamples(); ++i)
        biggestStep = juce::jmax (biggestStep, std::abs (audio.getSample (0, i) - audio.getSample (0, i - 1)));
    // A 100 Hz sine at its loudest here (-6 dBFS) moves at most 0.0066 a sample.
    CHECK (biggestStep < 0.008f);
}

TEST_CASE ("The processor runs PUSH before VOLUME and lights PUSH's LEDs", "[processor][push]")
{
    ParagonPR7MAudioProcessor processor;
    setParameter (processor, ParamIDs::push, 15.0f);
    processor.prepareToPlay (sampleRate, blockSize);
    const auto latencyWithPush = processor.getLatencySamples();

    auto audio = sine (1000.0, -6.0f, 2.0);
    juce::MidiBuffer midi;
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        // The last half second only: at the very start the tone gets
        // through before PUSH's attack, with the make-up gain, and VOLUME's
        // limiter rightly catches that.
        if (start >= audio.getNumSamples() - 24000 && start < audio.getNumSamples() - 24000 + blockSize)
        {
            processor.getPushReduction().takeReduction();
            processor.getVolumeReduction().takeReduction();
        }
        juce::AudioBuffer<float> block (audio.getArrayOfWritePointers(), 2, start, juce::jmin (blockSize, audio.getNumSamples() - start));
        processor.processBlock (block, midi);
    }
    CHECK_THAT (processor.getPushReduction().takeReduction(), WithinAbs (12.0, 0.5));
    CHECK (processor.getVolumeReduction().takeReduction() == 0.0f);   // settled at -12 dBFS, under the limiter
    CHECK_THAT (settledDb (audio), WithinAbs (-12.0, 0.25));

    // PUSH adds no delay.
    ParagonPR7MAudioProcessor plain;
    plain.prepareToPlay (sampleRate, blockSize);
    CHECK (plain.getLatencySamples() == latencyWithPush);

    // RESET sets it back to 0.
    processor.resetKnobs();
    CHECK (juce::exactlyEqual (processor.getParameters().getRawParameterValue (ParamIDs::push)->load(), 0.0f));
}
