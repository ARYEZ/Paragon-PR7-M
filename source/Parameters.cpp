#include "Parameters.h"

namespace Parameters
{
    namespace
    {
        // Each new parameter gets the next version hint, so hosts can tell
        // which plugin version added it.
        constexpr int firstVersion = 1;

        // A straight range in steps (0.1 dB, 1%...), snapped to whole steps
        // counted from 0. JUCE's own interval counts them from the minimum
        // (minimum + step x n), which a fused multiply-add (as on Apple
        // silicon) rounds a hair off, so 0 dB wasn't exactly 0 (found in
        // PR7-C, where it kept EQ bands running at their defaults).
        [[maybe_unused]] juce::NormalisableRange<float> steppedRange (float minimum, float maximum, float step)   // for the modules to come
        {
            const auto perUnit = std::round (1.0f / step);   // 10 for 0.1, 1 for 1
            return { minimum, maximum,
                     [] (float start, float end, float proportion) { return start + (end - start) * proportion; },
                     [] (float start, float end, float value) { return (value - start) / (end - start); },
                     [perUnit] (float start, float end, float value) { return juce::jlimit (start, end, std::round (value * perUnit) / perUnit); } };
        }

        // INPUT and OUTPUT: 0 dB at the top of the knob (as PR7-C's INPUT),
        // -24 dB in the left half and +12 dB in the right, in 0.1 dB steps.
        juce::NormalisableRange<float> gainRange()
        {
            using namespace ParamRanges;
            return { gainMin, gainMax,
                     [] (float, float, float proportion)
                     {
                         return proportion < 0.5f ? gainMin * (1.0f - proportion * 2.0f) : gainMax * (proportion * 2.0f - 1.0f);
                     },
                     [] (float, float, float db) { return db < 0.0f ? 0.5f * (1.0f - db / gainMin) : 0.5f + 0.5f * db / gainMax; },
                     [] (float, float, float db) { return juce::jlimit (gainMin, gainMax, std::round (db * 10.0f) / 10.0f); } };
        }

        juce::String asDb (float value, int)
        {
            return (value > 0.0f ? "+" : "") + juce::String (value, 1) + " dB";
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace ParamRanges;
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (const auto& [id, name] : { std::pair (ParamIDs::inputGain, "Input"), { ParamIDs::outputGain, "Output" } })
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { id, firstVersion }, name, gainRange(), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (asDb)));
        return layout;
    }
}
