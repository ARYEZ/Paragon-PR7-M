#include "Parameters.h"

namespace Parameters
{
    namespace
    {
        // Each new parameter gets the next version hint, so hosts can tell
        // which plugin version added it. The first is 1.

        // A straight range in steps (0.1 dB, 1%...), snapped to whole steps
        // counted from 0. JUCE's own interval counts them from the minimum
        // (minimum + step x n), which a fused multiply-add (as on Apple
        // silicon) rounds a hair off, so 0 dB wasn't exactly 0 (found in
        // PR7-C, where it kept EQ bands running at their defaults).
        [[maybe_unused]] juce::NormalisableRange<float> steppedRange (float minimum, float maximum, float step)
        {
            const auto perUnit = std::round (1.0f / step);   // 10 for 0.1, 1 for 1
            return { minimum, maximum,
                     [] (float start, float end, float proportion) { return start + (end - start) * proportion; },
                     [] (float start, float end, float value) { return (value - start) / (end - start); },
                     [perUnit] (float start, float end, float value) { return juce::jlimit (start, end, std::round (value * perUnit) / perUnit); } };
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {};
    }
}
