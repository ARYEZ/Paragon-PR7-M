#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"
#include "dsp/Analog.h"
#include "dsp/Clipper.h"
#include "dsp/Limiter.h"
#include "dsp/Loudness.h"
#include "dsp/TruePeak.h"

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    // A dense, mix-like signal at 120 bpm: a kick, a bass line, a snare on 2
    // and 4, hi-hats and a noise pad, peaking at -1 dBFS (about -15 LUFS).
    juce::AudioBuffer<float> denseMix (double seconds)
    {
        const auto n = (int) (sampleRate * seconds);
        juce::AudioBuffer<float> mix (2, n);
        juce::Random random (7);
        double pad[2] {};
        const double bassNotes[] = { 55.0, 55.0, 73.4, 65.4 };
        for (int i = 0; i < n; ++i)
        {
            const auto t = i / sampleRate;
            const auto beat = std::fmod (t, 0.5);
            const auto kickPitch = 50.0 + 120.0 * std::exp (-beat * 40.0);
            const auto kick = 0.9 * std::sin (juce::MathConstants<double>::twoPi * kickPitch * beat) * std::exp (-beat * 9.0);
            const auto bass = 0.35 * std::sin (juce::MathConstants<double>::twoPi * bassNotes[(int) (t / 2.0) % 4] * t);
            const auto snareTime = std::fmod (t + 0.5, 1.0);
            const auto snare = (random.nextDouble() * 2.0 - 1.0) * 0.5 * std::exp (-snareTime * 25.0) * (snareTime < 0.3 ? 1.0 : 0.0);
            const auto hat = (random.nextDouble() * 2.0 - 1.0) * 0.12 * std::exp (-std::fmod (t, 0.25) * 80.0);
            for (int c = 0; c < 2; ++c)
            {
                pad[c] = 0.97 * pad[c] + (random.nextDouble() * 2.0 - 1.0) * 0.06;
                mix.setSample (c, i, (float) (kick + bass + snare + hat + pad[c] * (c == 0 ? 1.0 : 0.9)));
            }
        }
        mix.applyGain (juce::Decibels::decibelsToGain (-1.0f) / mix.getMagnitude (0, n));
        return mix;
    }

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

    void process (ParagonPR7MAudioProcessor& processor, juce::AudioBuffer<float>& buffer)
    {
        juce::MidiBuffer midi;
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        {
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, juce::jmin (blockSize, buffer.getNumSamples() - start));
            processor.processBlock (block, midi);
        }
    }

    void clip (effects::VolumeClipper& clipper, juce::AudioBuffer<float>& buffer)
    {
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
            clipper.process (juce::dsp::AudioBlock<float> (buffer).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, buffer.getNumSamples() - start)));
    }

    // How strong one frequency is in a signal (dB, 0 = a full-scale sine), by Goertzel.
    float levelAtDb (const float* samples, int numSamples, double frequency)
    {
        const auto w = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        const auto coefficient = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            // A Hann window, so neighbouring frequencies don't leak in.
            const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (numSamples - 1));
            const auto s = samples[i] * window + coefficient * s1 - s2;
            s2 = s1;
            s1 = s;
        }
        const auto power = s1 * s1 + s2 * s2 - coefficient * s1 * s2;
        return (float) (10.0 * std::log10 (power + 1.0e-30) - 20.0 * std::log10 (numSamples * 0.25));
    }
}

TEST_CASE ("VOLUME keeps making a dense mix louder all the way up, under the ceiling", "[volume]")
{
    // It used to stop getting louder from about 9 (a clean limiter only turns
    // a dense mix down harder): 9 to 15 added 0.4 LU, 15 was 6.4 LU over 0.
    // With the clipper every 3 dB step adds at least 0.8 LU (measured: 2.7,
    // 2.4, 2.2, 1.7, 0.9), about 10 LU in all.
    const auto mix = denseMix (6.0);
    auto last = lufsOf (mix);
    const auto start = last;
    for (const auto volume : { 3.0f, 6.0f, 9.0f, 12.0f, 15.0f })
    {
        INFO ("VOLUME " << volume);
        ParagonPR7MAudioProcessor processor;
        setParameter (processor, ParamIDs::volume, volume);
        processor.prepareToPlay (sampleRate, blockSize);
        auto audio = mix;
        process (processor, audio);
        const auto loudness = lufsOf (audio);
        CHECK (loudness > last + 0.8f);
        CHECK (juce::Decibels::gainToDecibels (processor.getTruePeak()) <= effects::TruePeakLimiter::ceilingDb);
        last = loudness;
    }
    CHECK (last - start > 9.0f);
}

TEST_CASE ("VOLUME's clipper leaves what's under its knee exactly as it was, only later", "[volume]")
{
    // At 0 the knee starts at the ceiling: a -3 dBFS tone passes bit for bit.
    // At 6 it starts at -7 dBFS: a -20 dBFS tone is only turned up.
    for (const auto& [volume, peakDb] : { std::pair (0.0f, -3.0f), { 6.0f, -20.0f } })
    {
        INFO ("VOLUME " << volume);
        effects::VolumeClipper clipper;
        clipper.prepare (sampleRate, blockSize);
        clipper.setVolumeImmediately (volume);
        const auto input = sine (440.0, peakDb, 0.5);
        auto output = input;
        clip (clipper, output);
        const auto latency = clipper.getLatency();
        const auto gain = juce::Decibels::decibelsToGain (volume);
        for (int i = latency; i < input.getNumSamples(); ++i)
            if (! juce::exactlyEqual (output.getSample (0, i), input.getSample (0, i - latency) * gain))
                FAIL ("sample " << i << " changed");
    }
}

TEST_CASE ("VOLUME's knee widens as it's pushed, from none at 0 to 6 dB", "[volume]")
{
    CHECK_THAT (effects::VolumeClipper::kneeStartDb (0.0f), WithinAbs (-1.0, 1.0e-6));
    CHECK_THAT (effects::VolumeClipper::kneeStartDb (3.0f), WithinAbs (-4.0, 1.0e-6));
    CHECK_THAT (effects::VolumeClipper::kneeStartDb (6.0f), WithinAbs (-7.0, 1.0e-6));
    CHECK_THAT (effects::VolumeClipper::kneeStartDb (15.0f), WithinAbs (-7.0, 1.0e-6));

    // The curve: untouched to the knee, smooth into the ceiling, never past it.
    const auto ceiling = juce::Decibels::decibelsToGain (-1.0f), knee = juce::Decibels::decibelsToGain (-7.0f);
    CHECK (juce::exactlyEqual (effects::VolumeClipper::shape (knee, knee, ceiling), knee));
    for (const auto level : { 0.6f, 0.8f, 1.0f, 2.0f, 8.0f })
        CHECK (effects::VolumeClipper::shape (level, knee, ceiling) <= ceiling);
    CHECK (effects::VolumeClipper::shape (0.8f, knee, ceiling) < effects::VolumeClipper::shape (1.0f, knee, ceiling));
}

TEST_CASE ("VOLUME's clipping doesn't fold back down as harsh aliasing", "[volume]")
{
    // A 5 kHz tone pushed 12 dB into the clipper. Its harmonics above 24 kHz
    // would fold back to 3, 7, 13 and 17 kHz (not harmonics of 5 kHz) if
    // clipped at the sample rate; at 4x they're filtered off first.
    const auto measure = [] (bool oversampled)
    {
        auto audio = sine (5000.0, -1.0f, 1.0);
        if (oversampled)
        {
            effects::VolumeClipper clipper;
            clipper.prepare (sampleRate, blockSize);
            clipper.setVolumeImmediately (12.0f);
            clip (clipper, audio);
        }
        else
        {
            const auto ceiling = juce::Decibels::decibelsToGain (-1.0f), knee = juce::Decibels::decibelsToGain (-7.0f);
            const auto gain = juce::Decibels::decibelsToGain (12.0f);
            for (int i = 0; i < audio.getNumSamples(); ++i)
            {
                const auto x = audio.getSample (0, i) * gain;
                audio.setSample (0, i, std::copysign (effects::VolumeClipper::shape (std::abs (x), knee, ceiling), x));
            }
        }
        const auto* samples = audio.getReadPointer (0, 4800);
        const auto n = 32768;
        auto worst = -200.0f;
        for (const auto alias : { 3000.0, 7000.0, 13000.0, 17000.0 })
            worst = juce::jmax (worst, levelAtDb (samples, n, alias));
        return worst - levelAtDb (samples, n, 5000.0);
    };
    const auto naive = measure (false), ours = measure (true);
    INFO ("aliasing, against the tone: at the sample rate " << naive << " dB, at 4x " << ours << " dB");
    CHECK (ours < naive - 20.0f);
    CHECK (ours < -60.0f);
}

TEST_CASE ("Turning VOLUME doesn't click", "[volume]")
{
    effects::VolumeClipper clipper;
    clipper.prepare (sampleRate, blockSize);
    auto audio = sine (100.0, -20.0f, 1.0);
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        clipper.setVolume (start >= 24000 ? 15.0f : 0.0f);
        clipper.process (juce::dsp::AudioBlock<float> (audio).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, audio.getNumSamples() - start)));
    }
    float biggestStep = 0.0f;
    for (int i = 1; i < audio.getNumSamples(); ++i)
        biggestStep = juce::jmax (biggestStep, std::abs (audio.getSample (0, i) - audio.getSample (0, i - 1)));
    // A 100 Hz sine at -5 dBFS (its loudest here) moves at most 0.0074 a sample.
    CHECK (biggestStep < 0.01f);
}

TEST_CASE ("The processor reports ANALOG's, the clipper's and the limiter's delay, from the start", "[processor][volume]")
{
    effects::AnalogColour analog;
    analog.prepare (sampleRate, blockSize);
    effects::VolumeClipper clipper;
    clipper.prepare (sampleRate, blockSize);
    REQUIRE (clipper.getLatency() > 0);
    REQUIRE (analog.getLatency() > 0);
    const auto expected = analog.getLatency() + clipper.getLatency() + effects::TruePeakLimiter::latencyFor (sampleRate);

    ParagonPR7MAudioProcessor processor;
    CHECK (processor.getLatencySamples() == expected);   // before prepareToPlay: the 48 kHz guess
    processor.prepareToPlay (sampleRate, blockSize);
    CHECK (processor.getLatencySamples() == expected);
}

TEST_CASE ("Benchmark: VOLUME's clipper CPU", "[.benchmark]")
{
    effects::VolumeClipper clipper;
    clipper.prepare (sampleRate, blockSize);
    clipper.setVolumeImmediately (12.0f);
    auto audio = denseMix (60.0);   // a minute
    const auto start = juce::Time::getMillisecondCounterHiRes();
    clip (clipper, audio);
    const auto seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
    std::cout << "A minute of stereo 48 kHz in " << seconds << " s: " << 100.0 * seconds / 60.0 << "% of one core" << std::endl;
}
