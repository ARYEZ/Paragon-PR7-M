#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Every parameter's ID, in one place. IDs are saved in projects and presets:
// never rename one once it has shipped. Choice parameters are saved by
// index, so only ever add choices to the end.
//
// Each build step adds its own (see CLAUDE.md).
namespace ParamIDs
{
    // BYPASS: hear the mix as it came in, at the processed loudness.
    inline constexpr const char* bypass = "panel.bypass";
    // VOLUME: gain into the true-peak limiter, 0 to +15 dB.
    inline constexpr const char* volume = "dyn.volume";
    // PUSH: the glue compressor, gentle (0) to hard (15).
    inline constexpr const char* push = "dyn.push";
    // The tone: AIR and FOCUS -15 to +15, BODY -5 to +5.
    inline constexpr const char* air = "tone.air";
    inline constexpr const char* focus = "tone.focus";
    inline constexpr const char* body = "tone.body";
    // BASS PUNCH 0 to 10; TRANSIENTS -10 (softer attacks) to +10 (sharper).
    inline constexpr const char* bassPunch = "dyn.bassPunch";
    inline constexpr const char* transients = "dyn.transients";
    // ANALOG 0 to 10; WIDTH -5 (mono) to +5 (twice as wide).
    inline constexpr const char* analog = "colour.analog";
    inline constexpr const char* width = "stereo.width";
}

namespace Parameters
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // AIR and FOCUS's scale, as printed evenly round their knobs: finer near
    // 0. Their parameters follow it, so the pointer reads true.
    inline constexpr float toneMarks[] = { -15.0f, -10.0f, -6.0f, -3.0f, 0.0f, 3.0f, 6.0f, 10.0f, 15.0f };
}
