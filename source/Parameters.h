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
}

namespace Parameters
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
