#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"
#include "dsp/BassPunch.h"
#include "dsp/Biquad.h"
#include "dsp/TransientShaper.h"

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

    // Silence, then a tone from `start` seconds on: a hit.
    juce::AudioBuffer<float> hit (double frequency, float peakDb, double start, double seconds)
    {
        auto buffer = sine (frequency, peakDb, seconds);
        buffer.clear (0, 0, (int) (sampleRate * start));
        buffer.clear (1, 0, (int) (sampleRate * start));
        return buffer;
    }

    template <typename Module>
    void run (Module& module, juce::AudioBuffer<float>& buffer)
    {
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
            module.process (juce::dsp::AudioBlock<float> (buffer).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, buffer.getNumSamples() - start)));
    }

    // A steady sine's level (dB of its peak) from its RMS, over `length` samples from `start`.
    float levelDb (const juce::AudioBuffer<float>& buffer, int start, int length, int channel = 0)
    {
        return juce::Decibels::gainToDecibels (buffer.getRMSLevel (channel, start, length) * juce::MathConstants<float>::sqrt2);
    }

    // What a fixed low shelf of `db` at BASS PUNCH's frequency does to a sine (dB).
    float shelfResponseDb (double frequency, float db)
    {
        effects::Biquad filter;
        filter.c = effects::Biquad::lowShelf (sampleRate, effects::BassPunch::shelfHz, db);
        auto audio = sine (frequency, 0.0f, 1.0);
        filter.process (audio.getWritePointer (0), audio.getNumSamples());
        return levelDb (audio, 24000, 24000);
    }

    float punchedLevelDb (float punch, double frequency, float peakDb)
    {
        effects::BassPunch module;
        module.prepare (sampleRate);
        module.setPunchImmediately (punch);
        auto audio = sine (frequency, peakDb, 2.0);
        run (module, audio);
        return levelDb (audio, 72000, 24000);
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
TEST_CASE ("BASS PUNCH at 0 passes bit for bit", "[punch]")
{
    effects::BassPunch module;
    module.prepare (sampleRate);
    const auto input = sine (50.0, -3.0f, 0.5);
    auto output = input;
    run (module, output);
    for (int i = 0; i < input.getNumSamples(); ++i)
        if (! juce::exactlyEqual (input.getSample (0, i), output.getSample (0, i)))
            FAIL ("sample " << i << " changed");
}

TEST_CASE ("BASS PUNCH lifts quiet bass by up to 6 dB and leaves the mids", "[punch]")
{
    // Far under its threshold: the full lift, as a fixed +6 dB shelf would give.
    CHECK_THAT (punchedLevelDb (10.0f, 40.0, -40.0f) + 40.0f, WithinAbs (shelfResponseDb (40.0, 6.0f), 0.05));
    CHECK (shelfResponseDb (40.0, 6.0f) > 5.0f);
    CHECK_THAT (punchedLevelDb (5.0f, 40.0, -40.0f) + 40.0f, WithinAbs (shelfResponseDb (40.0, 3.0f), 0.05));
    CHECK (std::abs (punchedLevelDb (10.0f, 1000.0, -40.0f) + 40.0f) < 0.1f);

    // More BASS PUNCH, more lift.
    auto last = 0.0f;
    for (const auto punch : { 2.0f, 4.0f, 6.0f, 8.0f, 10.0f })
    {
        const auto lift = punchedLevelDb (punch, 40.0, -40.0f) + 40.0f;
        CHECK (lift > last);
        last = lift;
    }
}

TEST_CASE ("BASS PUNCH takes its lift back on loud bass: tighter", "[punch]")
{
    // At 10: a quiet bass note comes up about 6 dB, a loud one is held at or
    // under its own level, so 34 dB between them going in is much less
    // coming out.
    const auto quiet = punchedLevelDb (10.0f, 40.0, -40.0f);
    const auto loud = punchedLevelDb (10.0f, 40.0, -6.0f);
    CHECK (loud <= -6.0f + 0.1f);
    CHECK (loud - quiet < 34.0f - 5.0f);

    effects::BassPunch module;
    module.prepare (sampleRate);
    module.setPunchImmediately (10.0f);
    auto audio = sine (40.0, -6.0f, 2.0);
    run (module, audio);
    CHECK (module.getShelfDb() < 0.0f);
    CHECK (module.getShelfDb() >= effects::BassPunch::lowestGainDb);
}

TEST_CASE ("BASS PUNCH lets a kick's first thump through before it tightens", "[punch]")
{
    // A loud 50 Hz hit after silence: lifted at first (it reacts in about
    // 25 ms), held back once it has been going a while.
    effects::BassPunch module;
    module.prepare (sampleRate);
    module.setPunchImmediately (10.0f);
    auto audio = hit (50.0, -6.0f, 0.5, 1.0);
    std::vector<float> shelf;
    for (int start = 0; start < audio.getNumSamples(); start += 48)   // every millisecond
    {
        module.process (juce::dsp::AudioBlock<float> (audio).getSubBlock ((size_t) start, 48));
        shelf.push_back (module.getShelfDb());
    }
    CHECK (shelf[500 + 5] > 4.0f);    // 5 ms in
    CHECK (shelf[500 + 300] < 0.0f);  // 300 ms in
}

TEST_CASE ("BASS PUNCH turns both channels together and doesn't click when turned", "[punch]")
{
    {
        effects::BassPunch module;
        module.prepare (sampleRate);
        module.setPunchImmediately (10.0f);
        auto audio = sine (40.0, -6.0f, 2.0);
        audio.applyGain (1, 0, audio.getNumSamples(), 0.1f);
        run (module, audio);
        CHECK_THAT (levelDb (audio, 72000, 24000, 0) - levelDb (audio, 72000, 24000, 1), WithinAbs (20.0, 0.01));
    }

    effects::BassPunch module;
    module.prepare (sampleRate);
    auto audio = sine (50.0, -12.0f, 3.0);
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        module.setPunch (start >= 48000 && start < 96000 ? 10.0f : 0.0f);
        module.process (juce::dsp::AudioBlock<float> (audio).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, audio.getNumSamples() - start)));
    }
    // A 50 Hz sine at -12 dBFS lifted at most 6 dB: under 0.0033 a sample.
    CHECK (biggestStep (audio) < 0.004f);
}

//==============================================================================
TEST_CASE ("TRANSIENTS at 0 passes bit for bit", "[transients]")
{
    effects::TransientShaper module;
    module.prepare (sampleRate);
    const auto input = hit (1000.0, -6.0f, 0.1, 0.5);
    auto output = input;
    run (module, output);
    for (int i = 0; i < input.getNumSamples(); ++i)
        if (! juce::exactlyEqual (input.getSample (0, i), output.getSample (0, i)))
            FAIL ("sample " << i << " changed");
}

TEST_CASE ("TRANSIENTS lifts or softens the start of a hit and leaves what sustains", "[transients]")
{
    const auto shaped = [] (float amount)
    {
        effects::TransientShaper module;
        module.prepare (sampleRate);
        module.setAmountImmediately (amount);
        auto audio = hit (1000.0, -12.0f, 0.5, 2.0);
        run (module, audio);
        return audio;
    };
    const auto onset = 24000;
    const auto sharper = shaped (10.0f), softer = shaped (-10.0f);
    // 1 to 5 ms into the hit, against the hit as it came in (-12 dBFS).
    CHECK (levelDb (sharper, onset + 48, 192) > -12.0f + 4.0f);
    CHECK (levelDb (softer, onset + 48, 192) < -12.0f - 4.0f);
    // Half a second in, the held tone is as it came in.
    CHECK_THAT (levelDb (sharper, onset + 48000, 12000), WithinAbs (-12.0, 0.05));
    CHECK_THAT (levelDb (softer, onset + 48000, 12000), WithinAbs (-12.0, 0.05));
}

TEST_CASE ("TRANSIENTS doesn't wobble a held low note", "[transients]")
{
    // A steady 50 Hz tone: the followers ripple with it, but the gain
    // mustn't (that would be distortion). Sample by sample, out against in.
    for (const auto amount : { 10.0f, -10.0f })
    {
        INFO (amount);
        effects::TransientShaper module;
        module.prepare (sampleRate);
        module.setAmountImmediately (amount);
        const auto input = sine (50.0, -6.0f, 2.0);
        auto audio = input;
        run (module, audio);
        auto worst = 0.0f;
        for (int i = 48000; i < audio.getNumSamples(); ++i)
            if (std::abs (input.getSample (0, i)) > 0.05f)
                worst = juce::jmax (worst, std::abs (juce::Decibels::gainToDecibels (audio.getSample (0, i) / input.getSample (0, i))));
        CHECK (worst < 0.05f);
    }
}

TEST_CASE ("TRANSIENTS turns both channels together and doesn't click when turned", "[transients]")
{
    {
        effects::TransientShaper module;
        module.prepare (sampleRate);
        module.setAmountImmediately (10.0f);
        auto audio = hit (1000.0, -6.0f, 0.2, 1.0);
        audio.applyGain (1, 0, audio.getNumSamples(), 0.1f);
        run (module, audio);
        for (const auto start : { 9600 + 48, 9600 + 24000 })
            CHECK_THAT (levelDb (audio, start, 192, 0) - levelDb (audio, start, 192, 1), WithinAbs (20.0, 0.01));
    }

    effects::TransientShaper module;
    module.prepare (sampleRate);
    auto audio = sine (100.0, -6.0f, 3.0);
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        module.setAmount (start >= 48000 && start < 96000 ? 10.0f : -10.0f);
        module.process (juce::dsp::AudioBlock<float> (audio).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, audio.getNumSamples() - start)));
    }
    // A 100 Hz sine at -6 dBFS moves at most 0.0066 a sample.
    CHECK (biggestStep (audio) < 0.008f);
}

//==============================================================================
TEST_CASE ("The processor runs BASS PUNCH and TRANSIENTS, adds no delay, and RESET sets them back", "[processor][punch][transients]")
{
    ParagonPR7MAudioProcessor plain;
    plain.prepareToPlay (sampleRate, blockSize);

    ParagonPR7MAudioProcessor processor;
    setParameter (processor, ParamIDs::bassPunch, 10.0f);
    setParameter (processor, ParamIDs::transients, 5.0f);
    processor.prepareToPlay (sampleRate, blockSize);
    CHECK (processor.getLatencySamples() == plain.getLatencySamples());

    auto audio = sine (40.0, -40.0f, 2.0);
    juce::MidiBuffer midi;
    for (int start = 0; start < audio.getNumSamples(); start += blockSize)
    {
        juce::AudioBuffer<float> block (audio.getArrayOfWritePointers(), 2, start, juce::jmin (blockSize, audio.getNumSamples() - start));
        processor.processBlock (block, midi);
    }
    // Quiet steady bass: BASS PUNCH's full lift; TRANSIENTS leaves a held note.
    CHECK_THAT (levelDb (audio, 72000, 24000), WithinAbs (-40.0 + shelfResponseDb (40.0, 6.0f), 0.05));

    processor.resetKnobs();
    for (const auto* id : { ParamIDs::bassPunch, ParamIDs::transients })
        CHECK (juce::exactlyEqual (processor.getParameters().getRawParameterValue (id)->load(), 0.0f));
}
