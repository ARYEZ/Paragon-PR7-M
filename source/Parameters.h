#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Every parameter's ID, in one place. IDs are saved in projects and presets:
// never rename one once it has shipped. Choice parameters are saved by
// index, so only ever add choices to the end.
namespace ParamIDs
{
    inline constexpr auto inputGain = "inputGain";     // INPUT, dB
    inline constexpr auto outputGain = "outputGain";   // OUTPUT, dB
}

namespace ParamRanges
{
    inline constexpr float gainMin = -24.0f, gainMax = 12.0f;
}

namespace Parameters
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
