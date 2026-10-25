#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <thread>

#include "Assistant.h"
#include "Parameters.h"
#include "PluginProcessor.h"

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double rate = 48000.0;

    // Noise with the reference's spectrum (a typical master's), tilted by
    // `shape` (dB at a frequency), with its side `sideDb` under its mid,
    // pulsing like a song with a beat (so it has peaks over its loudness;
    // `between`: how loud it is between the hits), peaking at -1 dBFS.
    // About 11 seconds.
    template <typename Shape>
    juce::AudioBuffer<float> mixLike (Shape&& shape, float sideDb = -12.0f, float between = 0.4f, unsigned seed = 1)
    {
        constexpr int order = 19, size = 1 << order;
        juce::dsp::FFT fft (order);
        juce::Random random ((juce::int64) seed);
        const auto shaped = [&]
        {
            std::vector<std::complex<float>> spectrum ((size_t) size), out ((size_t) size);
            for (int bin = 1; bin < size / 2; ++bin)
            {
                const auto hz = (float) (bin * rate / size);
                const auto magnitude = hz < 25.0f ? 0.0f : std::pow (10.0f, (Assistant::referenceDb (hz) + shape (hz)) / 20.0f);
                const auto phase = random.nextFloat() * juce::MathConstants<float>::twoPi;
                spectrum[(size_t) bin] = std::polar (magnitude, phase);
                spectrum[(size_t) (size - bin)] = std::conj (spectrum[(size_t) bin]);
            }
            fft.perform (spectrum.data(), out.data(), true);
            std::vector<float> samples ((size_t) size);
            for (int i = 0; i < size; ++i)
                samples[(size_t) i] = out[(size_t) i].real();
            return samples;
        };
        const auto mid = shaped();
        const auto side = shaped();
        const auto sideGain = juce::Decibels::decibelsToGain (sideDb, -200.0f);
        juce::AudioBuffer<float> audio (2, size);
        for (int i = 0; i < size; ++i)
        {
            const auto beat = between + (1.0f - between) * std::exp (-(float) (i % 24000) / 3000.0f);   // a hit every half second
            audio.setSample (0, i, beat * (mid[(size_t) i] + sideGain * side[(size_t) i]));
            audio.setSample (1, i, beat * (mid[(size_t) i] - sideGain * side[(size_t) i]));
        }
        audio.applyGain (juce::Decibels::decibelsToGain (-1.0f) / audio.getMagnitude (0, size));
        return audio;
    }

    auto flat() { return [] (float) { return 0.0f; }; }

    float valueOf (ParagonPR7MAudioProcessor& processor, const char* id)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        REQUIRE (parameter != nullptr);
        return parameter->convertFrom0to1 (parameter->getValue());
    }

    void processAll (ParagonPR7MAudioProcessor& processor, juce::AudioBuffer<float>& audio, int blockSize = 512)
    {
        juce::MidiBuffer midi;
        for (int start = 0; start < audio.getNumSamples(); start += blockSize)
        {
            juce::AudioBuffer<float> block (audio.getArrayOfWritePointers(), audio.getNumChannels(), start, juce::jmin (blockSize, audio.getNumSamples() - start));
            processor.processBlock (block, midi);
        }
    }
}

TEST_CASE ("Assistant: the reference spectrum falls above 1 kHz", "[assistant]")
{
    CHECK_THAT (Assistant::referenceDb (1000.0f), WithinAbs (0.0, 1e-4));
    // Per Hz, an octave band twice as wide holds the same power at 3 dB less.
    CHECK (Assistant::referenceDb (2000.0f) < -5.0f);
    CHECK (Assistant::referenceDb (16000.0f) < Assistant::referenceDb (8000.0f) - 10.0f);
    CHECK (Assistant::referenceDb (60.0f) > Assistant::referenceDb (500.0f));
}

TEST_CASE ("Assistant: a mix already like the reference keeps a neutral tone", "[assistant]")
{
    const auto analysis = Assistant::analyse (mixLike (flat()), rate);
    CHECK (std::abs (analysis.air) < 1.0f);
    CHECK (std::abs (analysis.presence) < 1.0f);
    CHECK (std::abs (analysis.body) < 1.0f);
    CHECK (std::abs (analysis.lows) < 1.0f);
    CHECK_THAT (analysis.sideDb, WithinAbs (-12.0, 1.0));
    const auto s = Assistant::suggest (analysis, -14.0f);
    CHECK (std::abs (s.air) <= 1.0f);
    CHECK (std::abs (s.focus) <= 1.0f);
    CHECK (std::abs (s.body) <= 0.5f);
    CHECK (std::abs (s.width) <= 0.5f);
    CHECK (s.bassPunch >= 1.0f);
    CHECK (s.bassPunch <= 3.0f);
}

TEST_CASE ("Assistant: dull gets more AIR, bright less", "[assistant]")
{
    const auto dull = Assistant::suggest (Assistant::analyse (mixLike ([] (float hz) { return hz > 6000.0f ? -8.0f : 0.0f; }), rate), -14.0f);
    CHECK (dull.air >= 4.0f);
    const auto bright = Assistant::suggest (Assistant::analyse (mixLike ([] (float hz) { return hz > 6000.0f ? 6.0f : 0.0f; }), rate), -14.0f);
    CHECK (bright.air <= -4.0f);
}

TEST_CASE ("Assistant: boxy low mids get less BODY, harsh presence less FOCUS", "[assistant]")
{
    const auto boxy = Assistant::suggest (Assistant::analyse (mixLike ([] (float hz) { return hz > 120.0f && hz < 400.0f ? 5.0f : 0.0f; }), rate), -14.0f);
    CHECK (boxy.body <= -2.0f);
    const auto harsh = Assistant::suggest (Assistant::analyse (mixLike ([] (float hz) { return hz > 1500.0f && hz < 5000.0f ? 5.0f : 0.0f; }), rate), -14.0f);
    CHECK (harsh.focus <= -5.0f);
}

TEST_CASE ("Assistant: thin lows get more BASS PUNCH", "[assistant]")
{
    const auto thin = Assistant::suggest (Assistant::analyse (mixLike ([] (float hz) { return hz < 120.0f ? -6.0f : 0.0f; }), rate), -14.0f);
    CHECK (thin.bassPunch >= 5.0f);
    const auto boomy = Assistant::suggest (Assistant::analyse (mixLike ([] (float hz) { return hz < 120.0f ? 6.0f : 0.0f; }), rate), -14.0f);
    CHECK (boomy.bassPunch <= 0.5f);
}

TEST_CASE ("Assistant: narrow gets wider, very wide a little narrower, mono stays", "[assistant]")
{
    CHECK (Assistant::suggest (Assistant::analyse (mixLike (flat(), -24.0f), rate), -14.0f).width >= 1.5f);
    CHECK (Assistant::suggest (Assistant::analyse (mixLike (flat(), -4.0f), rate), -14.0f).width <= -1.0f);
    CHECK (juce::exactlyEqual (Assistant::suggest (Assistant::analyse (mixLike (flat(), -200.0f), rate), -14.0f).width, 0.0f));
}

TEST_CASE ("Assistant: a spiky mix gets more PUSH than a squashed one", "[assistant]")
{
    Assistant::Analysis spiky, squashed;
    spiky.peakToLoudness = 18.0f;
    squashed.peakToLoudness = 9.0f;
    const auto a = Assistant::suggest (spiky, -14.0f), b = Assistant::suggest (squashed, -14.0f);
    CHECK (a.push > b.push + 3.0f);
    CHECK (b.transients > a.transients);
}

TEST_CASE ("Assistant: VOLUME goes no further than the clipper allows", "[assistant]")
{
    // A mix peaking at -1 dBFS: clean to 9 (the limiter alone), Maximum to 12.
    CHECK_THAT (Assistant::mostVolumeFor (-1.0f, Assistant::targets[0].mostOvershootDb), WithinAbs (9.0, 0.15));
    CHECK_THAT (Assistant::mostVolumeFor (-1.0f, Assistant::targets[2].mostOvershootDb), WithinAbs (12.0, 0.15));
    // Quieter peaks leave more room; all the way for a very quiet one.
    CHECK (Assistant::mostVolumeFor (-7.0f, 3.0f) > Assistant::mostVolumeFor (-1.0f, 3.0f) + 1.5f);
    CHECK_THAT (Assistant::mostVolumeFor (-40.0f, 3.0f), WithinAbs (15.0, 1e-4));
}

TEST_CASE ("Assistant: the knobs it picks land the song on the loudness asked for", "[assistant]")
{
    // A dense mix with some headroom, peaking at -7 dBFS.
    auto song = mixLike (flat(), -12.0f, 0.7f);
    song.applyGain (0.5f);
    for (int target = 0; target < Assistant::numTargets; ++target)
    {
        DYNAMIC_SECTION (Assistant::targets[target].name)
        {
            const auto decided = Assistant::decide (song, rate, target);
            INFO ("VOLUME " << decided.settings.volume << ", PUSH " << decided.settings.push << ", reached " << decided.reachedLufs);
            CHECK_THAT (decided.reachedLufs, WithinAbs (Assistant::targets[target].lufs, 0.3));

            // And the plugin really plays it that loud.
            ParagonPR7MAudioProcessor processor;
            const auto& s = decided.settings;
            for (const auto& [id, value] : { std::pair (ParamIDs::air, s.air), { ParamIDs::focus, s.focus }, { ParamIDs::body, s.body },
                                             { ParamIDs::bassPunch, s.bassPunch }, { ParamIDs::transients, s.transients },
                                             { ParamIDs::push, s.push }, { ParamIDs::analog, s.analog }, { ParamIDs::width, s.width },
                                             { ParamIDs::volume, s.volume } })
            {
                auto* parameter = processor.getParameters().getParameter (id);
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            }
            processor.prepareToPlay (rate, 512);
            auto played = song;
            processAll (processor, played);
            CHECK_THAT (Assistant::loudnessOf (played, rate, (int) rate / 2), WithinAbs (decided.reachedLufs, 0.3));
        }
    }
}

TEST_CASE ("Assistant: a mix too spiky to reach the target cleanly stops short, never over", "[assistant]")
{
    const auto song = mixLike (flat());   // peaks 16 dB over its loudness
    const auto streaming = Assistant::decide (song, rate, 0);
    const auto loud = Assistant::decide (song, rate, 1);
    INFO ("Streaming " << streaming.reachedLufs << ", Loud " << loud.reachedLufs << " at VOLUME " << loud.settings.volume);
    CHECK_THAT (streaming.reachedLufs, WithinAbs (-14.0, 0.3));
    CHECK (loud.reachedLufs <= -10.0f + 0.3f);
    CHECK (loud.reachedLufs > streaming.reachedLufs + 1.5f);
    CHECK (loud.settings.volume <= Assistant::mostVolumeFor (-1.0f, Assistant::targets[1].mostOvershootDb) + 0.1f);
}

TEST_CASE ("Assistant: LISTEN records after the silence, thinks, and sets the knobs", "[assistant]")
{
    ParagonPR7MAudioProcessor processor;
    processor.prepareToPlay (rate, 512);
    auto* bypass = processor.getParameters().getParameter (ParamIDs::bypass);
    bypass->setValueNotifyingHost (1.0f);
    processor.startListening (1);
    CHECK (processor.getAssistant().getState() == Assistant::State::listening);

    juce::AudioBuffer<float> silence (2, (int) rate * 2);
    silence.clear();
    processAll (processor, silence);
    CHECK (processor.getAssistant().getSecondsHeard() <= 0.0);   // silence doesn't count

    const auto song = mixLike (flat());
    for (int round = 0; round < 2 && processor.getAssistant().getState() == Assistant::State::listening; ++round)
    {
        auto audio = song;
        processAll (processor, audio);
    }
    REQUIRE (processor.getAssistant().getState() != Assistant::State::listening);

    std::optional<Assistant::Result> decided;
    for (int wait = 0; wait < 600 && ! decided; ++wait)
    {
        decided = processor.applyAssistant();
        if (! decided)
            std::this_thread::sleep_for (std::chrono::milliseconds (50));
    }
    REQUIRE (decided.has_value());
    CHECK (processor.getAssistant().getState() == Assistant::State::idle);
    CHECK (processor.getPresets().getCurrentName() == "Assistant - Loud");
    CHECK (! processor.getPresets().isModified());
    CHECK_THAT (valueOf (processor, ParamIDs::volume), WithinAbs (decided->settings.volume, 1e-4));
    CHECK_THAT (valueOf (processor, ParamIDs::push), WithinAbs (decided->settings.push, 1e-4));
    CHECK (valueOf (processor, ParamIDs::bypass) >= 0.5f);   // left alone
}

TEST_CASE ("Assistant: stopping LISTEN records nothing more", "[assistant]")
{
    ParagonPR7MAudioProcessor processor;
    processor.prepareToPlay (rate, 512);
    processor.startListening (0);
    auto audio = mixLike (flat());
    juce::AudioBuffer<float> some (audio.getArrayOfWritePointers(), 2, 0, (int) rate);
    processAll (processor, some);
    CHECK (processor.getAssistant().getSecondsHeard() > 0.9);
    processor.getAssistant().cancel();
    CHECK (processor.getAssistant().getState() == Assistant::State::idle);
    processAll (processor, audio);
    CHECK (processor.getAssistant().getState() == Assistant::State::idle);
    CHECK (! processor.applyAssistant().has_value());
}
