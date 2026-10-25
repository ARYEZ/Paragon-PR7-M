#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"
#include "dsp/Analog.h"
#include "dsp/Width.h"

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

    // A stereo signal with a different tone on each side: `midHz` the same in
    // both (mid), `sideHz` opposite (side), each at `peakDb`.
    juce::AudioBuffer<float> stereo (double midHz, double sideHz, float peakDb, double seconds)
    {
        const auto numSamples = (int) (sampleRate * seconds);
        juce::AudioBuffer<float> buffer (2, numSamples);
        const auto amplitude = juce::Decibels::decibelsToGain (peakDb);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto mid = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * midHz * i / sampleRate);
            const auto side = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * sideHz * i / sampleRate);
            buffer.setSample (0, i, mid + side);
            buffer.setSample (1, i, mid - side);
        }
        return buffer;
    }

    template <typename Module>
    void run (Module& module, juce::AudioBuffer<float>& buffer)
    {
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
            module.process (juce::dsp::AudioBlock<float> (buffer).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, buffer.getNumSamples() - start)));
    }

    // How strong one frequency is (dB, 0 = a full-scale sine), by Goertzel with a Hann window.
    float levelAtDb (const float* samples, int numSamples, double frequency)
    {
        const auto coefficient = 2.0 * std::cos (juce::MathConstants<double>::twoPi * frequency / sampleRate);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (numSamples - 1));
            const auto s = samples[i] * window + coefficient * s1 - s2;
            s2 = s1;
            s1 = s;
        }
        const auto power = s1 * s1 + s2 * s2 - coefficient * s1 * s2;
        return (float) (10.0 * std::log10 (power + 1.0e-30) - 20.0 * std::log10 (numSamples * 0.25));
    }

    // The mid and side of a stereo buffer, from `start` on.
    juce::AudioBuffer<float> midSide (const juce::AudioBuffer<float>& buffer, int start)
    {
        const auto n = buffer.getNumSamples() - start;
        juce::AudioBuffer<float> out (2, n);
        for (int i = 0; i < n; ++i)
        {
            out.setSample (0, i, 0.5f * (buffer.getSample (0, start + i) + buffer.getSample (1, start + i)));
            out.setSample (1, i, 0.5f * (buffer.getSample (0, start + i) - buffer.getSample (1, start + i)));
        }
        return out;
    }

    juce::AudioBuffer<float> coloured (float amount, double frequency, float peakDb)
    {
        effects::AnalogColour analog;
        analog.prepare (sampleRate, blockSize);
        analog.setAmountImmediately (amount);
        auto audio = sine (frequency, peakDb, 1.0);
        run (analog, audio);
        return audio;
    }

    void setParameter (ParagonPR7MAudioProcessor& processor, const char* id, float value)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    float biggestStep (const juce::AudioBuffer<float>& audio)
    {
        float most = 0.0f;
        for (int i = 1; i < audio.getNumSamples(); ++i)
            most = juce::jmax (most, std::abs (audio.getSample (0, i) - audio.getSample (0, i - 1)));
        return most;
    }
}

//==============================================================================
TEST_CASE ("ANALOG at 0 passes bit for bit, only later", "[analog]")
{
    effects::AnalogColour analog;
    analog.prepare (sampleRate, blockSize);
    REQUIRE (analog.getLatency() > 0);
    const auto input = sine (1000.0, -1.0f, 0.5);
    auto output = input;
    run (analog, output);
    const auto latency = analog.getLatency();
    for (int i = 0; i < input.getNumSamples(); ++i)
        if (! juce::exactlyEqual (output.getSample (0, i), i < latency ? 0.0f : input.getSample (0, i - latency)))
            FAIL ("sample " << i << " changed");
}

TEST_CASE ("ANALOG adds harmonics, mostly the 2nd, more as it's turned up", "[analog]")
{
    // A 1 kHz tone at -6 dBFS: its 2nd (2 kHz) and 3rd (3 kHz) harmonics,
    // against the tone.
    const auto harmonics = [] (float amount)
    {
        const auto audio = coloured (amount, 1000.0, -6.0f);
        const auto* samples = audio.getReadPointer (0, 9600);
        const auto n = 32768;
        const auto tone = levelAtDb (samples, n, 1000.0);
        return std::pair (levelAtDb (samples, n, 2000.0) - tone, levelAtDb (samples, n, 3000.0) - tone);
    };
    const auto [secondAt5, thirdAt5] = harmonics (5.0f);
    const auto [secondAt10, thirdAt10] = harmonics (10.0f);
    INFO ("2nd and 3rd harmonics: at 5 " << secondAt5 << " / " << thirdAt5 << " dB, at 10 " << secondAt10 << " / " << thirdAt10 << " dB");
    CHECK (secondAt10 > -40.0f);        // clearly there at full
    CHECK (secondAt10 < -10.0f);        // but a colour, not a fuzz
    CHECK (secondAt10 > secondAt5 + 6.0f);
    CHECK (secondAt10 > thirdAt10);     // mostly warmth
    CHECK (thirdAt10 > thirdAt5);
}

TEST_CASE ("ANALOG leaves the level of quiet sounds alone, and colours them much less", "[analog]")
{
    // At ANALOG 10 a -30 dBFS tone comes out at the same level. The valve
    // curve's warmth shrinks with the level but never quite goes (its 2nd
    // harmonic falls 1 dB for every dB quieter): measured, about -39 dB here
    // against -18 dB at -6 dBFS.
    const auto secondHarmonic = [] (float peakDb, float& toneDb)
    {
        const auto audio = coloured (10.0f, 1000.0, peakDb);
        const auto* samples = audio.getReadPointer (0, 9600);
        const auto n = 32768;
        toneDb = levelAtDb (samples, n, 1000.0);
        return levelAtDb (samples, n, 2000.0) - toneDb;
    };
    float quietTone = 0.0f, loudTone = 0.0f;
    const auto quiet = secondHarmonic (-30.0f, quietTone);
    const auto loud = secondHarmonic (-6.0f, loudTone);
    CHECK_THAT (quietTone, WithinAbs (-30.0, 0.1));
    CHECK (quiet < -35.0f);
    CHECK (quiet < loud - 20.0f);
}

TEST_CASE ("ANALOG's harmonics don't fold back as aliasing, and leave no DC", "[analog]")
{
    // A 7 kHz tone near full scale at 10: harmonics above 24 kHz would fold
    // to 20, 13 and 6 kHz (not harmonics of 7 kHz) without the oversampling.
    const auto audio = coloured (10.0f, 7000.0, -1.0f);
    const auto* samples = audio.getReadPointer (0, 9600);
    const auto n = 32768;
    const auto tone = levelAtDb (samples, n, 7000.0);
    auto worst = -200.0f;
    for (const auto alias : { 20000.0, 13000.0, 6000.0 })
        worst = juce::jmax (worst, levelAtDb (samples, n, alias) - tone);
    INFO ("aliasing against the tone: " << worst << " dB");
    CHECK (worst < -70.0f);

    // The lopsided curve's DC is taken off: the average sits at 0.
    const auto lowTone = coloured (10.0f, 100.0, -1.0f);
    double sum = 0.0;
    for (int i = 24000; i < 48000; ++i)
        sum += lowTone.getSample (0, i);
    CHECK (std::abs (sum / 24000.0) < 0.001);
}

TEST_CASE ("Turning ANALOG doesn't click, and it goes back to bit for bit at 0", "[analog]")
{
    effects::AnalogColour analog;
    analog.prepare (sampleRate, blockSize);
    const auto input = sine (100.0, -6.0f, 2.0);
    auto audio = input;
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        analog.setAmount (start >= 24000 && start < 48000 ? 10.0f : 0.0f);
        analog.process (juce::dsp::AudioBlock<float> (audio).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, audio.getNumSamples() - start)));
    }
    // A 100 Hz sine at -6 dBFS moves at most 0.0066 a sample; ANALOG
    // squares its top a little.
    CHECK (biggestStep (audio) < 0.009f);
    const auto latency = analog.getLatency();
    for (int i = 72000; i < audio.getNumSamples(); ++i)   // half a second after it's back at 0
        if (! juce::exactlyEqual (audio.getSample (0, i), input.getSample (0, i - latency)))
            FAIL ("sample " << i << " still changed");
}

//==============================================================================
TEST_CASE ("WIDTH at 0 passes bit for bit", "[width]")
{
    effects::StereoWidth width;
    width.prepare (sampleRate);
    const auto input = stereo (440.0, 1000.0, -6.0f, 0.5);
    auto output = input;
    run (width, output);
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < input.getNumSamples(); ++i)
            if (! juce::exactlyEqual (output.getSample (channel, i), input.getSample (channel, i)))
                FAIL ("sample " << i << " changed");
}

TEST_CASE ("WIDTH scales the side from mono to twice as wide, and never the mid", "[width]")
{
    // The mid at 440 Hz, the side at 1 kHz, each at -12 dBFS.
    for (const auto& [setting, sideGainDb] : { std::pair (-5.0f, -200.0f), { -2.5f, -6.02f }, { 2.5f, 3.52f }, { 5.0f, 6.02f } })
    {
        INFO ("WIDTH " << setting);
        effects::StereoWidth width;
        width.prepare (sampleRate);
        width.setWidthImmediately (setting);
        auto audio = stereo (440.0, 1000.0, -12.0f, 1.0);
        const auto input = midSide (audio, 9600);
        run (width, audio);
        const auto output = midSide (audio, 9600);
        const auto n = output.getNumSamples();
        // The mid (and so the mono sum) as it was, to rounding.
        auto worst = 0.0f;
        for (int i = 0; i < n; ++i)
            worst = juce::jmax (worst, std::abs (output.getSample (0, i) - input.getSample (0, i)));
        CHECK (worst < 1.0e-6f);
        const auto side = juce::Decibels::gainToDecibels (output.getRMSLevel (1, 0, n) / input.getRMSLevel (1, 0, n), -200.0f);
        if (setting <= -5.0f)
            CHECK (side < -100.0f);   // mono
        else
            CHECK_THAT (side, WithinAbs (sideGainDb, 0.1));
    }
}

TEST_CASE ("WIDTH keeps the bass in the middle", "[width]")
{
    // Bass in the side (50 Hz): widening doesn't spread it; it's taken out.
    for (const auto setting : { 5.0f, 2.0f, -2.0f })
    {
        INFO ("WIDTH " << setting);
        effects::StereoWidth width;
        width.prepare (sampleRate);
        width.setWidthImmediately (setting);
        auto audio = stereo (440.0, 50.0, -12.0f, 1.0);
        const auto input = midSide (audio, 24000);
        run (width, audio);
        const auto output = midSide (audio, 24000);
        const auto n = output.getNumSamples();
        CHECK (juce::Decibels::gainToDecibels (output.getRMSLevel (1, 0, n) / input.getRMSLevel (1, 0, n)) < -15.0f);
    }
}

TEST_CASE ("WIDTH leaves a mono track alone and doesn't click when turned", "[width]")
{
    {
        effects::StereoWidth width;
        width.prepare (sampleRate);
        width.setWidthImmediately (5.0f);
        juce::AudioBuffer<float> mono (1, 4800);
        for (int i = 0; i < mono.getNumSamples(); ++i)
            mono.setSample (0, i, 0.5f * std::sin ((float) i * 0.05f));
        const auto input = mono;
        run (width, mono);
        for (int i = 0; i < mono.getNumSamples(); ++i)
            if (! juce::exactlyEqual (mono.getSample (0, i), input.getSample (0, i)))
                FAIL ("sample " << i << " changed");
    }

    effects::StereoWidth width;
    width.prepare (sampleRate);
    auto audio = stereo (100.0, 150.0, -12.0f, 2.0);
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        width.setWidth (start >= 24000 && start < 72000 ? 5.0f : -5.0f);
        width.process (juce::dsp::AudioBlock<float> (audio).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, audio.getNumSamples() - start)));
    }
    // Two tones at -12 dBFS, the side at most doubled: under 0.01 a sample.
    CHECK (biggestStep (audio) < 0.012f);
}

//==============================================================================
TEST_CASE ("The processor runs ANALOG and WIDTH, and RESET sets them back", "[processor][analog][width]")
{
    ParagonPR7MAudioProcessor processor;
    setParameter (processor, ParamIDs::width, -5.0f);
    setParameter (processor, ParamIDs::analog, 3.0f);
    processor.prepareToPlay (sampleRate, blockSize);
    auto audio = stereo (440.0, 1000.0, -20.0f, 1.0);
    juce::MidiBuffer midi;
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        juce::AudioBuffer<float> block (audio.getArrayOfWritePointers(), 2, start, juce::jmin (blockSize, audio.getNumSamples() - start));
        processor.processBlock (block, midi);
    }
    // WIDTH -5: mono.
    const auto output = midSide (audio, 24000);
    CHECK (output.getRMSLevel (1, 0, output.getNumSamples()) < 1.0e-4f);

    processor.resetKnobs();
    for (const auto* id : { ParamIDs::analog, ParamIDs::width })
        CHECK (juce::exactlyEqual (processor.getParameters().getRawParameterValue (id)->load(), 0.0f));
}
