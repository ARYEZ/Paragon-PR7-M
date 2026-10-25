#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Factory presets (built in) and user presets (files, made with SAVE).
//
// A preset holds every knob. BYPASS isn't part of one (it's for comparing,
// not a setting), nor is the VU meter's 0 VU (it isn't a host parameter).
// Loading one sets every knob; anything a preset doesn't mention goes back
// to its default.
//
// The current preset's name is kept in the plugin's state, so it's saved
// with the song. Changing a setting after loading marks it modified.
//
// User presets are XML files ("<name>.pr7mpreset") in a folder of their own
// (Documents/Paragon PR7-M/Presets by default), so they can be copied
// between computers.
//
// Carried over from PR7-C, so the family's presets work alike. Not exposed
// to the host as programs: hosts calling setCurrentProgram (0) when a song
// loads would reset the settings.
class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState&, juce::File userFolder = defaultUserFolder());

    static juce::File defaultUserFolder();
    static constexpr auto fileExtension = ".pr7mpreset";
    static constexpr auto defaultName = "Default";

    struct Preset
    {
        juce::String name;
        bool factory = true;
        int factoryIndex = -1;   // factory presets
        juce::File file;         // user presets
    };

    // Factory presets first (in their order), then the user's (A to Z).
    std::vector<Preset> getPresets() const;
    static int getNumFactoryPresets();
    static juce::String getFactoryName (int index);
    // What a factory preset sets (ID, value in the parameter's units), for the tests.
    static std::vector<std::pair<juce::String, float>> getFactoryValues (int index);

    void load (const Preset&);
    void loadFactory (int index);
    // ◀ ▶: the previous or next preset in the list, wrapping round.
    void step (int direction);

    // Sets the knobs to `values` (ID -> value in the knob's own units; any
    // knob not listed goes to its default), named `name`, as if a preset
    // of that name had been loaded (the assistant's settings).
    void loadValues (const juce::String& name, const std::map<juce::String, float>& values);

    // SAVE: the current settings as a user preset (replacing one of the same name).
    bool save (const juce::String& name);
    bool remove (const Preset&);

    juce::String getCurrentName() const;
    const Preset* findCurrent (const std::vector<Preset>&) const;
    bool isModified() const;
    // After the host restores a song: work out what the named preset holds,
    // so "modified" is right again.
    void stateRestored();

    // Settings presets leave alone.
    static bool isKept (const juce::String& parameterId);

    juce::File getUserFolder() const { return userFolder; }

private:
    using Values = std::map<juce::String, float>;   // parameter ID -> value (in the parameter's own units)
    Values valuesOf (const Preset&) const;
    Values currentValues() const;
    void apply (const Values&);
    void setCurrent (const juce::String& name, const Values&);

    juce::AudioProcessorValueTreeState& parameters;
    juce::File userFolder;
    mutable juce::CriticalSection lock;   // `expected` (never touched on the audio thread)
    std::vector<std::pair<juce::RangedAudioParameter*, float>> expected;   // what the loaded preset set, 0 to 1
};
