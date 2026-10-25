#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"
#include "dsp/Biquad.h"
#include "dsp/Limiter.h"
#include "dsp/TruePeak.h"

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr int blockSize = 256;

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

    juce::AudioBuffer<float> noise (double sampleRate, float peakDb, double seconds)
    {
        juce::Random random (1234);
        juce::AudioBuffer<float> buffer (2, (int) (sampleRate * seconds));
        const auto amplitude = juce::Decibels::decibelsToGain (peakDb);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (channel, i, amplitude * (random.nextFloat() * 2.0f - 1.0f));
        return buffer;
    }

    // Rolls the top off above 20 kHz (steeply), as a real mix's spectrum
    // does: what the limiter is for.
    juce::AudioBuffer<float> bandLimited (juce::AudioBuffer<float> buffer, double sampleRate)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int stage = 0; stage < 4; ++stage)
            {
                effects::Biquad filter;
                filter.c = effects::Biquad::lowPass (sampleRate, 20000.0);
                filter.process (buffer.getWritePointer (channel), buffer.getNumSamples());
            }
        return buffer;
    }

    // A full-scale 1 kHz square wave, built from its harmonics up to 20 kHz:
    // the ringing between its samples is a classic true-peak trap.
    juce::AudioBuffer<float> square (double sampleRate, double seconds)
    {
        juce::AudioBuffer<float> buffer (2, (int) (sampleRate * seconds));
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            double value = 0.0;
            for (int harmonic = 1; harmonic * 1000 < 20000; harmonic += 2)
                value += std::sin (juce::MathConstants<double>::twoPi * 1000.0 * harmonic * i / sampleRate) / harmonic;
            for (int channel = 0; channel < 2; ++channel)
                buffer.setSample (channel, i, (float) (value * 4.0 / juce::MathConstants<double>::pi));
        }
        return buffer;
    }

    // Quiet, then sudden full-scale hits: the look-ahead has to catch them.
    juce::AudioBuffer<float> bursts (double sampleRate, double seconds)
    {
        auto buffer = sine (sampleRate, 220.0, -30.0f, seconds);
        juce::Random random (99);
        for (int start = (int) (sampleRate * 0.25); start + 400 < buffer.getNumSamples(); start += (int) (sampleRate * 0.3))
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 400; ++i)
                    buffer.setSample (channel, start + i, random.nextFloat() * 2.0f - 1.0f);
        return bandLimited (std::move (buffer), sampleRate);
    }

    // Runs the limiter over `buffer` in blocks; returns the most it turned down (dB).
    float limit (effects::TruePeakLimiter& limiter, juce::AudioBuffer<float>& buffer)
    {
        auto most = 0.0f;
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        {
            const auto n = juce::jmin (blockSize, buffer.getNumSamples() - start);
            most = juce::jmax (most, limiter.process (juce::dsp::AudioBlock<float> (buffer).getSubBlock ((size_t) start, (size_t) n)));
        }
        return most;
    }

    float truePeakDb (const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        metering::TruePeak meter;
        meter.prepare (sampleRate);
        auto peak = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            peak = juce::jmax (peak, meter.process (channel, buffer.getReadPointer (channel), buffer.getNumSamples()));
        return juce::Decibels::gainToDecibels (peak, -100.0f);
    }

    // A much finer reference than the limiter's own 8x reading: 16x, with a
    // long filter, so the test doesn't just agree with itself. Only where a
    // sample is within 6 dB of the ceiling (a peak between samples can't
    // climb further than that above them here), to keep it quick.
    float referenceTruePeakDb (const juce::AudioBuffer<float>& buffer)
    {
        constexpr int factor = 16, halfTaps = 64;
        const auto near = juce::Decibels::decibelsToGain (effects::TruePeakLimiter::ceilingDb - 6.0f);
        auto peak = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* x = buffer.getReadPointer (channel);
            for (int n = halfTaps; n < buffer.getNumSamples() - halfTaps; ++n)
            {
                peak = juce::jmax (peak, std::abs (x[n]));
                if (std::abs (x[n]) < near && std::abs (x[n + 1]) < near)
                    continue;
                for (int p = 1; p < factor; ++p)
                {
                    const auto t = (double) p / factor;
                    double value = 0.0;
                    for (int k = -halfTaps + 1; k <= halfTaps; ++k)
                    {
                        const auto d = t - k;
                        const auto sinc = std::abs (d) < 1.0e-9 ? 1.0 : std::sin (juce::MathConstants<double>::pi * d) / (juce::MathConstants<double>::pi * d);
                        const auto w = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * d / (halfTaps + 1));
                        value += x[n + k] * sinc * w;
                    }
                    peak = juce::jmax (peak, (float) std::abs (value));
                }
            }
        }
        return juce::Decibels::gainToDecibels (peak, -100.0f);
    }

    void setParameter (ParagonPR7MAudioProcessor& processor, const char* id, float value)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }
}

TEST_CASE ("The limiter keeps every true peak under -1 dBTP", "[limiter]")
{
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        struct Case { const char* name; juce::AudioBuffer<float> audio; float volumeDb; };
        std::vector<Case> cases;
        cases.push_back ({ "quarter-rate sine between its samples", sine (sampleRate, sampleRate / 4.0, 0.0f, 0.25, juce::MathConstants<double>::pi / 4.0), 0.0f });
        cases.push_back ({ "noise pushed 15 dB", bandLimited (noise (sampleRate, -6.0f, 0.5), sampleRate), 15.0f });
        cases.push_back ({ "full-scale square wave", square (sampleRate, 0.25), 0.0f });
        cases.push_back ({ "sudden full-scale bursts", bursts (sampleRate, 1.0), 6.0f });
        cases.push_back ({ "bass pushed 12 dB", sine (sampleRate, 50.0, -6.0f, 0.5), 12.0f });

        for (auto& [name, audio, volumeDb] : cases)
        {
            INFO (sampleRate << " Hz, " << name);
            effects::TruePeakLimiter limiter;
            limiter.prepare (sampleRate);
            limiter.setVolumeImmediately (volumeDb);
            CHECK (limit (limiter, audio) > 0.0f);
            // By a meter to the standard (4x), and by a much finer 16x reference.
            CHECK (truePeakDb (audio, sampleRate) <= effects::TruePeakLimiter::ceilingDb);
            CHECK (referenceTruePeakDb (audio) <= effects::TruePeakLimiter::ceilingDb);
        }
    }
}

TEST_CASE ("Worst case: white noise right up to the top of the range slips past by about 1 dB", "[limiter]")
{
    // A known limit, not a goal: nothing like a real mix. Full-strength
    // content between 21 kHz and half the sample rate, which any practical
    // true-peak reading under-reads (the same noise rolled off at 21 kHz
    // stays under). A meter to the standard still reads it under the
    // ceiling. Measured: about +0.04 dBTP, at 44.1 and 48 kHz.
    for (const auto sampleRate : { 44100.0, 48000.0 })
    {
        INFO (sampleRate);
        effects::TruePeakLimiter limiter;
        limiter.prepare (sampleRate);
        limiter.setVolumeImmediately (15.0f);
        auto audio = noise (sampleRate, -6.0f, 0.5);
        limit (limiter, audio);
        CHECK (truePeakDb (audio, sampleRate) <= effects::TruePeakLimiter::ceilingDb);
        const auto reference = referenceTruePeakDb (audio);
        UNSCOPED_INFO ("white noise reference true peak " << reference << " dBTP");
        CHECK (reference <= effects::TruePeakLimiter::ceilingDb + 1.2f);
    }
}

TEST_CASE ("Below the ceiling the limiter only delays the audio, bit for bit", "[limiter]")
{
    constexpr double sampleRate = 48000.0;
    effects::TruePeakLimiter limiter;
    limiter.prepare (sampleRate);
    const auto input = noise (sampleRate, -12.0f, 1.0);   // white noise's true peaks reach several dB over its samples
    auto output = input;
    CHECK (limit (limiter, output) == 0.0f);
    const auto latency = limiter.getLatency();
    for (int channel = 0; channel < 2; ++channel)
        for (int i = latency; i < input.getNumSamples(); ++i)
            if (! juce::exactlyEqual (output.getSample (channel, i), input.getSample (channel, i - latency)))
                FAIL ("sample " << i << " changed");
}

TEST_CASE ("The limiter's delay is 3 ms of look-ahead plus the true-peak reading's", "[limiter]")
{
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        INFO (sampleRate);
        effects::TruePeakLimiter limiter;
        limiter.prepare (sampleRate);
        CHECK (limiter.getLatency() == (int) std::round (sampleRate * 0.003) - 1 + metering::TruePeak::delay);

        // A click comes out exactly that much later.
        juce::AudioBuffer<float> click (2, 2048);
        click.clear();
        click.setSample (0, 100, 0.5f);
        click.setSample (1, 100, 0.5f);
        limit (limiter, click);
        CHECK (juce::exactlyEqual (click.getSample (0, 100 + limiter.getLatency()), 0.5f));
    }
}

TEST_CASE ("VOLUME turns the mix up by its dB until the ceiling", "[limiter]")
{
    constexpr double sampleRate = 48000.0;
    effects::TruePeakLimiter limiter;
    limiter.prepare (sampleRate);
    limiter.setVolumeImmediately (6.0f);
    auto audio = sine (sampleRate, 1000.0, -20.0f, 1.0);
    CHECK (limit (limiter, audio) == 0.0f);
    CHECK_THAT (juce::Decibels::gainToDecibels (audio.getMagnitude (0, 24000, 24000)), WithinAbs (-14.0, 0.01));

    // A steady tone pushed past the ceiling settles right at its target, a
    // hair under it.
    limiter.reset();
    limiter.setVolumeImmediately (12.0f);
    auto loud = sine (sampleRate, 1000.0, -6.0f, 1.0);
    CHECK_THAT (limit (limiter, loud), WithinAbs (6.0 - effects::TruePeakLimiter::targetDb, 0.02));
    CHECK_THAT (juce::Decibels::gainToDecibels (loud.getMagnitude (0, 24000, 24000)), WithinAbs (effects::TruePeakLimiter::targetDb, 0.02));
}

TEST_CASE ("The limiter lets go within about half a second of a loud hit", "[limiter]")
{
    constexpr double sampleRate = 48000.0;
    effects::TruePeakLimiter limiter;
    limiter.prepare (sampleRate);
    auto audio = sine (sampleRate, 1000.0, -20.0f, 1.5);
    for (int channel = 0; channel < 2; ++channel)   // a 10 ms hit at +6 dBFS
        for (int i = 4800; i < 5280; ++i)
            audio.setSample (channel, i, 2.0f * (i % 2 == 0 ? 1.0f : -1.0f));
    limit (limiter, audio);
    // Half a second after the hit, the quiet tone is back to -20 dBFS.
    const auto after = 4800 + limiter.getLatency() + 24000;
    CHECK_THAT (juce::Decibels::gainToDecibels (audio.getMagnitude (0, after, 4800)), WithinAbs (-20.0, 0.1));
}

TEST_CASE ("After a loud hit the limiter lets go completely, back to bit for bit", "[limiter]")
{
    // It once got stuck a hair under 1 (-0.001 dB) for good: in float, the
    // last steps of its release rounded to nothing.
    constexpr double sampleRate = 48000.0;
    effects::TruePeakLimiter limiter;
    limiter.prepare (sampleRate);
    auto audio = sine (sampleRate, 1000.0, -20.0f, 3.0);
    for (int channel = 0; channel < 2; ++channel)   // a 10 ms hit at +6 dBFS
        for (int i = 4800; i < 5280; ++i)
            audio.setSample (channel, i, 2.0f);
    const auto input = audio;
    limit (limiter, audio);
    const auto latency = limiter.getLatency();
    for (int i = (int) (sampleRate * 2.5); i < audio.getNumSamples(); ++i)
        if (! juce::exactlyEqual (audio.getSample (0, i), input.getSample (0, i - latency)))
            FAIL ("sample " << i << " still turned down");
}

TEST_CASE ("The limiter turns both channels down together", "[limiter]")
{
    // Loud on the left, quiet on the right: the right comes down as much.
    constexpr double sampleRate = 48000.0;
    effects::TruePeakLimiter limiter;
    limiter.prepare (sampleRate);
    limiter.setVolumeImmediately (6.0f);
    auto audio = sine (sampleRate, 1000.0, -3.0f, 1.0);
    audio.applyGain (1, 0, audio.getNumSamples(), 0.1f);   // right 20 dB down
    const auto reduction = limit (limiter, audio);
    REQUIRE (reduction > 1.0f);
    const auto left = juce::Decibels::gainToDecibels (audio.getMagnitude (0, 24000, 24000));
    const auto right = juce::Decibels::gainToDecibels (audio.getMagnitude (1, 24000, 24000));
    CHECK_THAT (left - right, WithinAbs (20.0, 0.05));
}

TEST_CASE ("Turning VOLUME doesn't click", "[limiter]")
{
    constexpr double sampleRate = 48000.0;
    effects::TruePeakLimiter limiter;
    limiter.prepare (sampleRate);
    auto audio = sine (sampleRate, 100.0, -20.0f, 1.0);
    const auto half = audio.getNumSamples() / 2;
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        limiter.setVolume (start >= half ? 15.0f : 0.0f);   // a jump of 15 dB
        limiter.process (juce::dsp::AudioBlock<float> (audio).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, audio.getNumSamples() - start)));
    }
    float biggestStep = 0.0f;
    for (int i = 1; i < audio.getNumSamples(); ++i)
        biggestStep = juce::jmax (biggestStep, std::abs (audio.getSample (0, i) - audio.getSample (0, i - 1)));
    // A 100 Hz sine at its loudest here (-5 dBFS) moves at most 0.0074 a sample.
    CHECK (biggestStep < 0.01f);
}

TEST_CASE ("The processor reports the limiter's delay and drives VOLUME's LEDs", "[processor][limiter]")
{
    constexpr double sampleRate = 48000.0;
    ParagonPR7MAudioProcessor processor;
    CHECK (processor.getLatencySamples() == effects::TruePeakLimiter::latencyFor (48000.0));   // before prepareToPlay
    setParameter (processor, ParamIDs::volume, 10.0f);
    processor.prepareToPlay (sampleRate, blockSize);
    CHECK (processor.getLatencySamples() == (int) std::round (sampleRate * 0.003) - 1 + metering::TruePeak::delay);

    auto audio = sine (sampleRate, 1000.0, -6.0f, 4.0);   // past the 3 s loudness window
    juce::MidiBuffer midi;
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        juce::AudioBuffer<float> block (audio.getArrayOfWritePointers(), 2, start, juce::jmin (blockSize, audio.getNumSamples() - start));
        processor.processBlock (block, midi);
    }
    CHECK_THAT (processor.getVolumeReduction().takeReduction(), WithinAbs (4.0 - effects::TruePeakLimiter::targetDb, 0.02));
    CHECK (juce::Decibels::gainToDecibels (processor.getTruePeak()) <= effects::TruePeakLimiter::ceilingDb);
    CHECK_THAT (processor.getShortTermLufs(), WithinAbs (effects::TruePeakLimiter::targetDb, 0.05));
}


TEST_CASE ("Benchmark: the limiter's CPU", "[.benchmark]")
{
    effects::TruePeakLimiter limiter;
    limiter.prepare (48000.0);
    limiter.setVolumeImmediately (12.0f);
    auto audio = noise (48000.0, -6.0f, 60.0);   // a minute
    const auto start = juce::Time::getMillisecondCounterHiRes();
    limit (limiter, audio);
    const auto seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
    std::cout << "A minute of stereo 48 kHz in " << seconds << " s: " << 100.0 * seconds / 60.0 << "% of one core" << std::endl;
}
