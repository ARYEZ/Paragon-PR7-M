#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "dsp/Loudness.h"
#include "dsp/MatchedBypass.h"
#include "dsp/TruePeak.h"
#include "dsp/VuBallistics.h"

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr int blockSize = 256;

    // A sine of `peakDb` dBFS on both channels.
    juce::AudioBuffer<float> sine (double sampleRate, double frequency, float peakDb, double seconds, double phase = 0.0)
    {
        const auto numSamples = (int) (sampleRate * seconds);
        juce::AudioBuffer<float> buffer (2, numSamples);
        const auto amplitude = juce::Decibels::decibelsToGain (peakDb);
        for (int i = 0; i < numSamples; ++i)
            for (int channel = 0; channel < 2; ++channel)
                buffer.setSample (channel, i, amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * frequency * i / sampleRate + phase));
        return buffer;
    }

    float shortTermOf (const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        metering::ShortTermLoudness meter;
        meter.prepare (sampleRate);
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        {
            const auto n = juce::jmin (blockSize, buffer.getNumSamples() - start);
            const float* channels[] = { buffer.getReadPointer (0, start), buffer.getReadPointer (1, start) };
            meter.process (channels, buffer.getNumChannels(), n);
        }
        return meter.getLufs();
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

    void setBypass (ParagonPR7MAudioProcessor& processor, bool on)
    {
        auto* parameter = processor.getParameters().getParameter (ParamIDs::bypass);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (on ? 1.0f : 0.0f);
    }
}

//==============================================================================
TEST_CASE ("K-weighting matches the coefficients BS.1770 prints for 48 kHz", "[metering]")
{
    const auto shelf = metering::KWeighting::shelf (48000.0);
    CHECK_THAT (shelf.b0, WithinAbs (1.53512485958697, 1.0e-9));
    CHECK_THAT (shelf.b1, WithinAbs (-2.69169618940638, 1.0e-9));
    CHECK_THAT (shelf.b2, WithinAbs (1.19839281085285, 1.0e-9));
    CHECK_THAT (shelf.a1, WithinAbs (-1.69065929318241, 1.0e-9));
    CHECK_THAT (shelf.a2, WithinAbs (0.73248077421585, 1.0e-9));

    const auto highPass = metering::KWeighting::highPass (48000.0);
    CHECK_THAT (highPass.a1, WithinAbs (-1.99004745483398, 1.0e-9));
    CHECK_THAT (highPass.a2, WithinAbs (0.99007225036621, 1.0e-9));
}

TEST_CASE ("A stereo 1 kHz tone at -23 dBFS reads -23 LUFS short-term, at any sample rate", "[metering]")
{
    // EBU Tech 3341's first test: a sine on both channels reads its level.
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        INFO (sampleRate);
        CHECK_THAT (shortTermOf (sine (sampleRate, 997.0, -23.0f, 4.0), sampleRate), WithinAbs (-23.0, 0.1));
    }
}

TEST_CASE ("Short-term loudness covers the last 3 seconds and reads silent with nothing playing", "[metering]")
{
    constexpr double sampleRate = 48000.0;
    // 1.5 s of tone then 1.5 s of silence: half the power over the window, -3 LU.
    auto buffer = sine (sampleRate, 997.0, -20.0f, 3.0);
    buffer.clear (0, (int) (sampleRate * 1.5), (int) (sampleRate * 1.5));
    buffer.clear (1, (int) (sampleRate * 1.5), (int) (sampleRate * 1.5));
    CHECK_THAT (shortTermOf (buffer, sampleRate), WithinAbs (-23.0, 0.15));

    juce::AudioBuffer<float> silence (2, 48000 * 4);
    silence.clear();
    CHECK (juce::exactlyEqual (shortTermOf (silence, sampleRate), metering::ShortTermLoudness::silent));
}

TEST_CASE ("Deep bass counts for less, as K-weighting says", "[metering]")
{
    // The high-pass at about 38 Hz: a 20 Hz tone reads well below a 1 kHz one
    // at the same level, and 100 Hz a little below.
    constexpr double sampleRate = 48000.0;
    // The standard's 48 kHz filters weigh 1 kHz +0.70 dB, 100 Hz -1.13 dB
    // and 20 Hz -13.28 dB.
    const auto at1k = shortTermOf (sine (sampleRate, 1000.0, -20.0f, 4.0), sampleRate);
    CHECK_THAT (shortTermOf (sine (sampleRate, 100.0, -20.0f, 4.0), sampleRate), WithinAbs (at1k - 1.83, 0.05));
    CHECK_THAT (shortTermOf (sine (sampleRate, 20.0, -20.0f, 4.0), sampleRate), WithinAbs (at1k - 13.97, 0.1));
}

//==============================================================================
TEST_CASE ("True peak finds the peaks between the samples", "[metering]")
{
    // A sine at a quarter of the sample rate, sampled 45 degrees off its
    // peaks: every sample is at -3 dB, but the wave itself reaches 0 dB.
    for (const auto sampleRate : { 44100.0, 48000.0 })
    {
        INFO (sampleRate);
        const auto buffer = sine (sampleRate, sampleRate / 4.0, 0.0f, 0.5, juce::MathConstants<double>::pi / 4.0);
        metering::TruePeak meter;
        meter.prepare (sampleRate);
        const auto peak = meter.process (0, buffer.getReadPointer (0), buffer.getNumSamples());
        CHECK_THAT (buffer.getMagnitude (0, 0, buffer.getNumSamples()), WithinAbs (0.7071, 0.001));
        CHECK_THAT (juce::Decibels::gainToDecibels (peak), WithinAbs (0.0, 0.1));
    }
}

TEST_CASE ("True peak is never below the sample peak, and reads a low tone as its sample peak", "[metering]")
{
    constexpr double sampleRate = 48000.0;
    const auto buffer = sine (sampleRate, 100.0, -6.0f, 0.5);
    metering::TruePeak meter;
    meter.prepare (sampleRate);
    const auto peak = meter.process (0, buffer.getReadPointer (0), buffer.getNumSamples());
    CHECK (peak >= buffer.getMagnitude (0, 0, buffer.getNumSamples()));
    CHECK_THAT (juce::Decibels::gainToDecibels (peak), WithinAbs (-6.0, 0.02));
}

TEST_CASE ("True peak oversamples 4x below 96 kHz, 2x up to 192 kHz", "[metering]")
{
    metering::TruePeak meter;
    meter.prepare (44100.0);
    CHECK (meter.getFactor() == 4);
    meter.prepare (96000.0);
    CHECK (meter.getFactor() == 2);
    meter.prepare (192000.0);
    CHECK (meter.getFactor() == 1);
}

//==============================================================================
TEST_CASE ("The VU needle takes 300 ms to reach 99% and overshoots by 1 to 1.5%", "[metering]")
{
    // IEC 60268-17's VU: 99% of a steady tone's reading in 300 ms (+-10%),
    // overshoot 1% to 1.5%. Driven with a steady level (a rectified sine
    // averages 2/pi of its peak, so this reads 1).
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        INFO (sampleRate);
        metering::VuBallistics needle;
        needle.prepare (sampleRate);
        const auto level = 2.0f / juce::MathConstants<float>::pi;
        double reached99 = -1.0;
        float highest = 0.0f;
        for (int i = 0; i < (int) sampleRate; ++i)
        {
            const auto position = needle.process (&level, 1);
            if (reached99 < 0.0 && position >= 0.99f)
                reached99 = i / sampleRate;
            highest = juce::jmax (highest, position);
        }
        CHECK_THAT (reached99, WithinAbs (0.3, 0.03));
        CHECK (highest >= 1.01f);
        CHECK (highest <= 1.015f);
        CHECK_THAT (needle.getPosition(), WithinAbs (1.0, 0.001));
    }
}

TEST_CASE ("The VU needle reads a steady sine's peak level and falls back to rest", "[metering]")
{
    constexpr double sampleRate = 48000.0;
    metering::VuBallistics needle;
    needle.prepare (sampleRate);
    const auto buffer = sine (sampleRate, 1000.0, -14.0f, 1.5);
    needle.process (buffer.getReadPointer (0), buffer.getNumSamples());
    CHECK_THAT (juce::Decibels::gainToDecibels (needle.getPosition()), WithinAbs (-14.0, 0.1));

    std::vector<float> silence ((size_t) sampleRate, 0.0f);
    needle.process (silence.data(), (int) silence.size());
    CHECK (needle.getPosition() < 0.001f);
}

//==============================================================================
TEST_CASE ("Matched bypass plays the unprocessed mix at the processed loudness", "[bypass]")
{
    // The "processing" here turns the mix up 6 dB: bypassed, the mix as it
    // came in is turned up 6 dB to match.
    constexpr double sampleRate = 48000.0;
    effects::MatchedBypass bypass;
    bypass.prepare (sampleRate);
    const auto dry = sine (sampleRate, 500.0, -20.0f, 6.0);
    auto out = dry;
    out.applyGain (2.0f);
    for (int start = 0; start < dry.getNumSamples(); start += blockSize)
    {
        const auto n = juce::jmin (blockSize, dry.getNumSamples() - start);
        bypass.process (juce::dsp::AudioBlock<const float> (dry).getSubBlock ((size_t) start, (size_t) n),
                        juce::dsp::AudioBlock<float> (out).getSubBlock ((size_t) start, (size_t) n), true);
    }
    CHECK_THAT (bypass.getMatchDb(), WithinAbs (6.02, 0.1));
    const auto last = (int) (sampleRate * 1.0);
    const auto tail = out.getNumSamples() - last;
    CHECK_THAT (juce::Decibels::gainToDecibels (out.getMagnitude (0, tail, last)), WithinAbs (-20.0 + 6.02, 0.1));
}

TEST_CASE ("Matched bypass never turns the unprocessed mix up into clipping", "[bypass]")
{
    // The processed mix is 12 dB louder, but the mix as it came in peaks at
    // -3 dBFS: it can only come up to -0.3 dBFS.
    constexpr double sampleRate = 48000.0;
    effects::MatchedBypass bypass;
    bypass.prepare (sampleRate);
    const auto dry = sine (sampleRate, 500.0, -3.0f, 6.0);
    auto out = dry;
    out.applyGain (4.0f);
    for (int start = 0; start < dry.getNumSamples(); start += blockSize)
    {
        const auto n = juce::jmin (blockSize, dry.getNumSamples() - start);
        bypass.process (juce::dsp::AudioBlock<const float> (dry).getSubBlock ((size_t) start, (size_t) n),
                        juce::dsp::AudioBlock<float> (out).getSubBlock ((size_t) start, (size_t) n), true);
    }
    CHECK_THAT (bypass.getMatchDb(), WithinAbs (2.7, 0.05));
    // After the 20 ms fade in from the processed mix.
    const auto start = (int) (sampleRate * 0.05);
    CHECK (out.getMagnitude (0, start, out.getNumSamples() - start) < juce::Decibels::decibelsToGain (-0.29f));
}

TEST_CASE ("Matched bypass switches with a crossfade, not a click", "[bypass]")
{
    // Processed is a different tone from unprocessed, so a hard switch would
    // jump. Over the 20 ms fade no sample moves further than the two tones can.
    constexpr double sampleRate = 48000.0;
    effects::MatchedBypass bypass;
    bypass.prepare (sampleRate);
    const auto dry = sine (sampleRate, 200.0, -12.0f, 1.0);
    const auto wet = sine (sampleRate, 200.0, -12.0f, 1.0, juce::MathConstants<double>::pi);   // the opposite polarity
    auto out = wet;
    const auto half = dry.getNumSamples() / 2;
    for (int start = 0; start < dry.getNumSamples(); start += blockSize)
    {
        const auto n = juce::jmin (blockSize, dry.getNumSamples() - start);
        bypass.process (juce::dsp::AudioBlock<const float> (dry).getSubBlock ((size_t) start, (size_t) n),
                        juce::dsp::AudioBlock<float> (out).getSubBlock ((size_t) start, (size_t) n), start >= half);
    }
    float biggestStep = 0.0f;
    for (int i = 1; i < out.getNumSamples(); ++i)
        biggestStep = juce::jmax (biggestStep, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
    // A 200 Hz sine at 0.25 moves at most 2 pi f A / fs = 0.0065 a sample; the fade adds a little.
    CHECK (biggestStep < 0.012f);
}

//==============================================================================
TEST_CASE ("With nothing processing, BYPASS on gives the input back bit for bit", "[processor][bypass]")
{
    constexpr double sampleRate = 48000.0;
    ParagonPR7MAudioProcessor processor;
    setBypass (processor, true);
    processor.prepareToPlay (sampleRate, blockSize);
    const auto input = sine (sampleRate, 440.0, -6.0f, 4.0);
    auto output = input;
    process (processor, output);
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < input.getNumSamples(); ++i)
            if (! juce::exactlyEqual (input.getSample (channel, i), output.getSample (channel, i)))
                FAIL ("sample " << i << " of channel " << channel << " changed");
}

TEST_CASE ("The processor meters its output: VU, short-term loudness and true peak", "[processor][metering]")
{
    constexpr double sampleRate = 48000.0;
    ParagonPR7MAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    auto buffer = sine (sampleRate, 997.0, -14.0f, 4.0);
    process (processor, buffer);
    CHECK_THAT (juce::Decibels::gainToDecibels (processor.getVuLevel()), WithinAbs (-14.0, 0.1));
    CHECK_THAT (processor.getShortTermLufs(), WithinAbs (-14.0, 0.1));
    CHECK_THAT (juce::Decibels::gainToDecibels (processor.getTruePeak()), WithinAbs (-14.0, 0.1));

    processor.resetTruePeak();
    CHECK (juce::exactlyEqual (processor.getTruePeak(), 0.0f));
}

TEST_CASE ("Hosts sending bigger blocks than they said are metered the same", "[processor][metering]")
{
    constexpr double sampleRate = 48000.0;
    ParagonPR7MAudioProcessor processor;
    processor.prepareToPlay (sampleRate, 64);
    auto buffer = sine (sampleRate, 997.0, -20.0f, 4.0);
    juce::MidiBuffer midi;
    for (int start = 0; start + 1000 <= buffer.getNumSamples(); start += 1000)
    {
        juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), 2, start, 1000);
        processor.processBlock (block, midi);
    }
    CHECK_THAT (processor.getShortTermLufs(), WithinAbs (-20.0, 0.1));
}

TEST_CASE ("RESET leaves BYPASS alone and sets every other parameter to its default", "[processor]")
{
    ParagonPR7MAudioProcessor processor;
    setBypass (processor, true);
    processor.resetKnobs();
    CHECK (processor.getParameters().getRawParameterValue (ParamIDs::bypass)->load() >= 0.5f);
    for (auto* parameter : processor.getParameters().processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter); ranged->getParameterID() != ParamIDs::bypass)
        {
            INFO (ranged->getParameterID().toStdString());
            CHECK (juce::exactlyEqual (ranged->getValue(), ranged->getDefaultValue()));
        }
}

TEST_CASE ("The 0 VU level is -14 dBFS by default and is saved with the project", "[processor]")
{
    ParagonPR7MAudioProcessor processor;
    CHECK (processor.getVuReference() == -14);
    processor.setVuReference (-18);
    juce::MemoryBlock state;
    processor.getStateInformation (state);

    ParagonPR7MAudioProcessor reloaded;
    reloaded.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (reloaded.getVuReference() == -18);

    reloaded.setVuReference (-7);   // not a choice: back to the default
    CHECK (reloaded.getVuReference() == -14);
}

//==============================================================================
TEST_CASE ("The panel's VU needle shows the output at the chosen 0 VU", "[ui][metering]")
{
    constexpr double sampleRate = 48000.0;
    ParagonPR7MAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);

    // A tone peaking at -14 dBFS: 0 VU at -14, -4 VU at -10, and pinned at
    // +3 at -18 (it would be +4).
    for (const auto& [reference, expected] : { std::pair (-14, 0.0f), { -10, -4.0f }, { -18, VuMeter::maxVu } })
    {
        INFO (reference);
        processor.setVuReference (reference);
        auto buffer = sine (sampleRate, 1000.0, -14.0f, 1.5);
        process (processor, buffer);
        view->updateMeters();
        CHECK_THAT (view->getVuMeter().getNeedle(), WithinAbs (expected, 0.15));
        CHECK (view->getVuMeter().getReference() == reference);
    }
    CHECK_THAT (view->getLoudnessDisplay().getLufs(), WithinAbs (-14.0, 0.2));
}

TEST_CASE ("The BYPASS lever and the bypass parameter follow each other", "[ui][bypass]")
{
    ParagonPR7MAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    auto* lever = dynamic_cast<LeverSwitch*> (view->findControl ("panel.bypass"));
    REQUIRE (lever != nullptr);
    const auto bypassed = [&] { return processor.getParameters().getRawParameterValue (ParamIDs::bypass)->load() >= 0.5f; };

    CHECK (lever->getPosition() == 1);   // down: not bypassed
    lever->setPosition (0);
    CHECK (bypassed());
    lever->setPosition (1);
    CHECK (! bypassed());

    setBypass (processor, true);   // from the host (on the message thread, so at once)
    CHECK (lever->getPosition() == 0);
}

TEST_CASE ("The readout formats LUFS and dBTP for seven segments", "[ui][metering]")
{
    CHECK (LoudnessDisplay::format (-14.24f) == "-14.2");
    CHECK (LoudnessDisplay::format (-0.04f) == "0.0");
    CHECK (LoudnessDisplay::format (0.36f) == "0.4");
    CHECK (LoudnessDisplay::format (-80.0f) == "--.-");
}
