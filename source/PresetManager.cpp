#include "PresetManager.h"

#include "Parameters.h"

namespace
{
    // The factory presets: starting points for a whole mix (our own
    // settings, not copies of anything), each only listing what differs
    // from the defaults, in each knob's own units. How loud VOLUME gets
    // depends on how loud the mix comes in: these assume a mix around
    // -18 to -14 LUFS, peaking a few dB under 0.
    struct FactoryPreset
    {
        const char* name;
        std::vector<std::pair<juce::String, float>> values;
    };

    const std::vector<FactoryPreset>& factoryPresets()
    {
        using namespace ParamIDs;
        static const std::vector<FactoryPreset> presets {
            { PresetManager::defaultName, {} },
            { "Gentle - Just a Touch", { { push, 1.5f }, { volume, 2 }, { air, 1 } } },
            { "Streaming - Balanced", { { air, 2 }, { push, 4.5f }, { volume, 2 } } },
            { "Pop - Polished", { { air, 4 }, { focus, 2 }, { transients, 1 }, { push, 6 }, { width, 1 }, { volume, 6 } } },
            { "Hip-Hop - Heavy Low End", { { body, 1 }, { bassPunch, 6 }, { transients, 3 }, { push, 6 }, { volume, 7 } } },
            { "EDM - Loud and Wide", { { air, 3 }, { bassPunch, 4 }, { push, 7.5f }, { analog, 2 }, { width, 1.5f }, { volume, 9 } } },
            { "Rock - Driven", { { focus, 3 }, { bassPunch, 2 }, { transients, 2 }, { push, 7.5f }, { analog, 4 }, { volume, 7 } } },
            { "Acoustic - Natural", { { air, 2 }, { transients, 2 }, { push, 1.5f }, { volume, 3 } } },
            { "Vocal Forward", { { focus, 6 }, { body, -1 }, { push, 4.5f }, { volume, 4 } } },
            { "Warm Analog", { { air, -3 }, { body, 1 }, { push, 4.5f }, { analog, 5 }, { volume, 4 } } },
            { "Bright and Open", { { air, 6 }, { focus, 2 }, { push, 3 }, { width, 1 }, { volume, 4 } } },
            { "Soft and Smooth", { { air, -3 }, { transients, -4 }, { push, 4.5f }, { analog, 2 }, { volume, 3 } } },
            { "Lo-Fi - Dusty", { { air, -10 }, { body, 1.5f }, { transients, -4 }, { push, 6 }, { analog, 7 }, { width, -2 }, { volume, 4 } } },
            { "Maximum Loudness", { { bassPunch, 3 }, { transients, 2 }, { push, 9 }, { volume, 12 } } },
        };
        return presets;
    }

    constexpr auto nameProperty = "presetName";
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvts, juce::File folder)
    : parameters (apvts), userFolder (std::move (folder))
{
}

juce::File PresetManager::defaultUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-M").getChildFile ("Presets");
}

bool PresetManager::isKept (const juce::String& id)
{
    return id == ParamIDs::bypass;
}

int PresetManager::getNumFactoryPresets() { return (int) factoryPresets().size(); }

juce::String PresetManager::getFactoryName (int index)
{
    return juce::isPositiveAndBelow (index, getNumFactoryPresets()) ? juce::String (factoryPresets()[(size_t) index].name) : juce::String();
}

std::vector<std::pair<juce::String, float>> PresetManager::getFactoryValues (int index)
{
    return juce::isPositiveAndBelow (index, getNumFactoryPresets()) ? factoryPresets()[(size_t) index].values
                                                                    : std::vector<std::pair<juce::String, float>>();
}

std::vector<PresetManager::Preset> PresetManager::getPresets() const
{
    std::vector<Preset> presets;
    for (int i = 0; i < getNumFactoryPresets(); ++i)
        presets.push_back ({ getFactoryName (i), true, i, {} });

    auto files = userFolder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0;
    });
    for (const auto& file : files)
        presets.push_back ({ file.getFileNameWithoutExtension(), false, -1, file });
    return presets;
}

PresetManager::Values PresetManager::valuesOf (const Preset& preset) const
{
    // Every setting a preset sets: its default, unless the preset says otherwise.
    Values values;
    for (auto* parameter : parameters.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter); ranged != nullptr && ! isKept (ranged->getParameterID()))
            values[ranged->getParameterID()] = ranged->convertFrom0to1 (ranged->getDefaultValue());

    if (preset.factory)
    {
        if (juce::isPositiveAndBelow (preset.factoryIndex, getNumFactoryPresets()))
            for (const auto& [id, value] : factoryPresets()[(size_t) preset.factoryIndex].values)
                if (values.count (id) != 0)
                    values[id] = value;
    }
    else if (const auto xml = juce::XmlDocument::parse (preset.file))
    {
        for (const auto* child : xml->getChildWithTagNameIterator ("PARAM"))
        {
            const auto id = child->getStringAttribute ("id");
            if (values.count (id) != 0)   // unknown or kept settings are ignored
                values[id] = (float) child->getDoubleAttribute ("value");
        }
    }
    return values;
}

PresetManager::Values PresetManager::currentValues() const
{
    Values values;
    for (auto* parameter : parameters.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter); ranged != nullptr && ! isKept (ranged->getParameterID()))
            values[ranged->getParameterID()] = ranged->convertFrom0to1 (ranged->getValue());
    return values;
}

void PresetManager::apply (const Values& values)
{
    for (const auto& [id, value] : values)
    {
        if (auto* parameter = parameters.getParameter (id))
        {
            const auto normalised = parameter->convertTo0to1 (value);
            if (! juce::approximatelyEqual (parameter->getValue(), normalised))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (normalised);
                parameter->endChangeGesture();
            }
        }
    }
}

void PresetManager::setCurrent (const juce::String& name, const Values& values)
{
    {
        const juce::ScopedLock scoped (lock);
        expected.clear();
        for (const auto& [id, value] : values)
            if (auto* parameter = parameters.getParameter (id))
                expected.emplace_back (parameter, parameter->convertTo0to1 (value));
    }
    parameters.state.setProperty (nameProperty, name, nullptr);
}

void PresetManager::load (const Preset& preset)
{
    const auto values = valuesOf (preset);
    apply (values);
    setCurrent (preset.name, values);
}

void PresetManager::loadFactory (int index)
{
    if (juce::isPositiveAndBelow (index, getNumFactoryPresets()))
        load ({ getFactoryName (index), true, index, {} });
}

const PresetManager::Preset* PresetManager::findCurrent (const std::vector<Preset>& presets) const
{
    const auto name = getCurrentName();
    for (const auto& preset : presets)
        if (preset.name == name)
            return &preset;
    return nullptr;
}

void PresetManager::step (int direction)
{
    const auto presets = getPresets();
    if (presets.empty())
        return;
    const auto* current = findCurrent (presets);
    const auto index = current != nullptr ? (int) (current - presets.data()) : (direction > 0 ? -1 : 0);
    const auto count = (int) presets.size();
    load (presets[(size_t) (((index + direction) % count + count) % count)]);
}

bool PresetManager::save (const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty() || ! userFolder.createDirectory())
        return false;

    const auto values = currentValues();
    juce::XmlElement xml ("PR7MPreset");
    xml.setAttribute ("name", trimmed);
    for (const auto& [id, value] : values)
    {
        auto* child = xml.createNewChildElement ("PARAM");
        child->setAttribute ("id", id);
        child->setAttribute ("value", value);
    }
    const auto file = userFolder.getChildFile (juce::File::createLegalFileName (trimmed) + fileExtension);
    if (! xml.writeTo (file))
        return false;
    setCurrent (file.getFileNameWithoutExtension(), values);
    return true;
}

bool PresetManager::remove (const Preset& preset)
{
    if (preset.factory || ! preset.file.deleteFile())
        return false;
    if (preset.name == getCurrentName())
    {
        // The settings stay as they are, now unnamed.
        const juce::ScopedLock scoped (lock);
        expected.clear();
        parameters.state.setProperty (nameProperty, juce::String(), nullptr);
    }
    return true;
}

juce::String PresetManager::getCurrentName() const
{
    return parameters.state.getProperty (nameProperty, defaultName).toString();
}

bool PresetManager::isModified() const
{
    const juce::ScopedLock scoped (lock);
    if (expected.empty())
        return getCurrentName().isNotEmpty();   // a name we can't find (a deleted preset): call it modified
    for (const auto& [parameter, value] : expected)
        if (std::abs (parameter->getValue() - value) > 1.0e-4f)
            return true;
    return false;
}

void PresetManager::stateRestored()
{
    const auto presets = getPresets();
    if (const auto* current = findCurrent (presets))
    {
        setCurrent (current->name, valuesOf (*current));
        return;
    }
    const juce::ScopedLock scoped (lock);
    expected.clear();
}
