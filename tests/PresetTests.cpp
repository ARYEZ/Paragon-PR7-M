#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"
#include "PresetManager.h"

using Catch::Matchers::WithinAbs;

namespace
{
    float valueOf (ParagonPR7MAudioProcessor& processor, const juce::String& id)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        REQUIRE (parameter != nullptr);
        return parameter->convertFrom0to1 (parameter->getValue());
    }

    void set (ParagonPR7MAudioProcessor& processor, const juce::String& id, float value)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    // A preset manager whose user presets go in a fresh temporary folder.
    struct Scratch
    {
        juce::TemporaryFile folder;
        Scratch() { folder.getFile().createDirectory(); }
        ~Scratch() { folder.getFile().deleteRecursively(); }
        juce::File get() const { return folder.getFile(); }
    };

    int indexOf (const std::vector<PresetManager::Preset>& list, const juce::String& name)
    {
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].name == name)
                return (int) i;
        return -1;
    }
}

TEST_CASE ("Presets: every factory preset names real knobs, with values in range", "[presets]")
{
    ParagonPR7MAudioProcessor processor;
    REQUIRE (PresetManager::getNumFactoryPresets() > 10);
    REQUIRE (PresetManager::getFactoryName (0) == PresetManager::defaultName);
    juce::StringArray names;
    for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
    {
        const auto name = PresetManager::getFactoryName (i);
        INFO (name.toStdString());
        CHECK (! names.contains (name, true));
        names.add (name);
        for (const auto& [id, value] : PresetManager::getFactoryValues (i))
        {
            INFO (id.toStdString());
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.getParameters().getParameter (id));
            REQUIRE (parameter != nullptr);
            REQUIRE (! PresetManager::isKept (id));
            const auto range = parameter->getNormalisableRange();
            CHECK (value >= range.start);
            CHECK (value <= range.end);
            // The knob lands exactly on the value (every knob is in 0.1 steps).
            CHECK_THAT (parameter->convertFrom0to1 (parameter->convertTo0to1 (value)), WithinAbs (value, 1e-4));
        }
    }
}

TEST_CASE ("Presets: a new PR7-M is on Default, unmodified", "[presets]")
{
    ParagonPR7MAudioProcessor processor;
    CHECK (processor.getPresets().getCurrentName() == "Default");
    CHECK (! processor.getPresets().isModified());
}

TEST_CASE ("Presets: loading sets every knob and leaves BYPASS alone", "[presets]")
{
    ParagonPR7MAudioProcessor processor;
    Scratch scratch;
    PresetManager presets (processor.getParameters(), scratch.get());
    set (processor, ParamIDs::bypass, 1.0f);
    set (processor, ParamIDs::width, 3.0f);   // not in the preset: back to its default (0)

    const auto list = presets.getPresets();
    const auto hipHop = indexOf (list, "Hip-Hop - Heavy Low End");
    REQUIRE (hipHop >= 0);
    presets.load (list[(size_t) hipHop]);
    CHECK (presets.getCurrentName() == "Hip-Hop - Heavy Low End");
    CHECK (! presets.isModified());
    CHECK_THAT (valueOf (processor, ParamIDs::bassPunch), WithinAbs (6.0, 1e-4));
    CHECK_THAT (valueOf (processor, ParamIDs::volume), WithinAbs (7.0, 1e-4));
    CHECK_THAT (valueOf (processor, ParamIDs::width), WithinAbs (0.0, 1e-6));
    CHECK_THAT (valueOf (processor, ParamIDs::bypass), WithinAbs (1.0, 1e-6));

    // Changing a knob marks it modified; BYPASS doesn't (it isn't part of a preset).
    set (processor, ParamIDs::bypass, 0.0f);
    CHECK (! presets.isModified());
    set (processor, ParamIDs::push, 9.0f);
    CHECK (presets.isModified());
    presets.load (list[(size_t) hipHop]);
    CHECK (! presets.isModified());
}

TEST_CASE ("Presets: the arrows step through the list and wrap round", "[presets]")
{
    ParagonPR7MAudioProcessor processor;
    Scratch scratch;
    PresetManager presets (processor.getParameters(), scratch.get());
    const auto list = presets.getPresets();
    presets.step (1);
    CHECK (presets.getCurrentName() == list[1].name);
    presets.step (-1);
    presets.step (-1);
    CHECK (presets.getCurrentName() == list.back().name);
    presets.step (1);
    CHECK (presets.getCurrentName() == list.front().name);
}

TEST_CASE ("Presets: SAVE makes a user preset that loads back exactly", "[presets]")
{
    ParagonPR7MAudioProcessor processor;
    Scratch scratch;
    PresetManager presets (processor.getParameters(), scratch.get());
    set (processor, ParamIDs::push, 7.3f);
    set (processor, ParamIDs::air, 4.5f);
    set (processor, ParamIDs::body, -2.1f);
    set (processor, ParamIDs::width, 1.2f);
    REQUIRE (presets.save ("My Master"));
    CHECK (presets.getCurrentName() == "My Master");
    CHECK (! presets.isModified());
    CHECK (scratch.get().getChildFile ("My Master.pr7mpreset").existsAsFile());

    // Somewhere else, then back.
    presets.loadFactory (0);
    CHECK_THAT (valueOf (processor, ParamIDs::push), WithinAbs (0.0, 1e-6));
    auto list = presets.getPresets();
    const auto mine = indexOf (list, "My Master");
    REQUIRE (mine == PresetManager::getNumFactoryPresets());   // after the factory ones
    CHECK (! list[(size_t) mine].factory);
    presets.load (list[(size_t) mine]);
    CHECK_THAT (valueOf (processor, ParamIDs::push), WithinAbs (7.3, 1e-4));
    CHECK_THAT (valueOf (processor, ParamIDs::air), WithinAbs (4.5, 1e-4));
    CHECK_THAT (valueOf (processor, ParamIDs::body), WithinAbs (-2.1, 1e-4));
    CHECK_THAT (valueOf (processor, ParamIDs::width), WithinAbs (1.2, 1e-4));
    CHECK (! presets.isModified());

    // User presets are listed A to Z, and can be deleted.
    REQUIRE (presets.save ("A Loud One"));
    list = presets.getPresets();
    CHECK (indexOf (list, "A Loud One") < indexOf (list, "My Master"));
    CHECK (presets.remove (list[(size_t) indexOf (list, "My Master")]));
    CHECK (indexOf (presets.getPresets(), "My Master") == -1);
    CHECK (! presets.remove (presets.getPresets()[0]));   // factory presets can't be deleted
}

TEST_CASE ("Presets: a song remembers its preset's name and whether it was changed", "[presets]")
{
    juce::MemoryBlock saved;
    {
        ParagonPR7MAudioProcessor processor;
        processor.getPresets().loadFactory (4);
        set (processor, ParamIDs::analog, 7.0f);
        processor.getStateInformation (saved);
    }
    ParagonPR7MAudioProcessor restored;
    restored.setStateInformation (saved.getData(), (int) saved.getSize());
    CHECK (restored.getPresets().getCurrentName() == PresetManager::getFactoryName (4));
    CHECK (restored.getPresets().isModified());
    CHECK_THAT (valueOf (restored, ParamIDs::analog), WithinAbs (7.0, 1e-4));
}

TEST_CASE ("Presets: RESET goes back to Default", "[presets]")
{
    ParagonPR7MAudioProcessor processor;
    processor.getPresets().loadFactory (5);
    set (processor, ParamIDs::bypass, 1.0f);
    processor.resetKnobs();
    CHECK (processor.getPresets().getCurrentName() == "Default");
    CHECK (! processor.getPresets().isModified());
    CHECK_THAT (valueOf (processor, ParamIDs::bypass), WithinAbs (1.0, 1e-6));
}

TEST_CASE ("Presets: every factory preset plays, under the ceiling", "[presets]")
{
    for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
    {
        DYNAMIC_SECTION (PresetManager::getFactoryName (i).toStdString())
        {
            ParagonPR7MAudioProcessor processor;
            processor.prepareToPlay (48000.0, 256);
            processor.getPresets().loadFactory (i);
            juce::AudioBuffer<float> buffer (2, 2 * 48000);
            juce::Random random (7);
            for (int s = 0; s < buffer.getNumSamples(); ++s)
            {
                const auto t = (float) s / 48000.0f;
                const auto kick = std::exp (-(float) (s % 24000) / 2400.0f) * std::sin (juce::MathConstants<float>::twoPi * 55.0f * t);
                const auto noise = 0.05f * (random.nextFloat() * 2.0f - 1.0f);
                buffer.setSample (0, s, 0.4f * kick + 0.2f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * t) + noise);
                buffer.setSample (1, s, 0.4f * kick + 0.2f * std::sin (juce::MathConstants<float>::twoPi * 330.0f * t) + noise);
            }
            juce::MidiBuffer midi;
            for (int start = 0; start < buffer.getNumSamples(); start += 256)
            {
                juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), 2, start, juce::jmin (256, buffer.getNumSamples() - start));
                processor.processBlock (block, midi);
            }
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto peak = buffer.getMagnitude (channel, 0, buffer.getNumSamples());
                CHECK (std::isfinite (peak));
                CHECK (peak < juce::Decibels::decibelsToGain (-0.9f));   // the limiter's -1 dBTP ceiling
                CHECK (buffer.getRMSLevel (channel, 48000, 48000) > 0.01f);   // and something comes out
            }
        }
    }
}
