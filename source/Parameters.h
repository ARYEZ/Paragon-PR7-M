#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Every parameter's ID, in one place. IDs are saved in projects and presets:
// never rename one once it has shipped. Choice parameters are saved by
// index, so only ever add choices to the end.
//
// None yet: each build step adds its own (see CLAUDE.md).
namespace ParamIDs
{
}

namespace Parameters
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
