#pragma once

#include <array>
#include <cmath>

#include <juce_dsp/juce_dsp.h>

#include "dsp/Biquad.h"

namespace effects
{
    // WIDTH: the stereo image, -5 (mono) to +5 (twice as wide). Our design,
    // after PR7-C's stereo tools: it works on the side signal (the difference
    // between left and right) and never touches the mid (what's the same in
    // both), so the mix sums to mono exactly as it was, at any width.
    //
    // Above about 120 Hz the side is scaled: x0 at -5, x1 at 0, x2 at +5.
    // Below it the bass is kept in the middle: the side's lows are never
    // widened, and are taken out as WIDTH moves off 0 (all of them from +-1
    // on), so widening never spreads the low end. The split is a steep
    // high-pass on the side (24 dB an octave), with the lows as what's left
    // over (side minus its highs), so nothing else shifts.
    //
    // At 0 it doesn't run: bit for bit. On a mono track there's no image, so
    // it does nothing. No latency.
    class StereoWidth
    {
    public:
        static constexpr float bassHz = 120.0f;

        // The side's gain above the bass, and how much of the side's bass is
        // taken out, for a WIDTH.
        static float sideGainFor (float width) noexcept { return 1.0f + juce::jlimit (-5.0f, 5.0f, width) / 5.0f; }
        static float bassMonoFor (float width) noexcept { return juce::jmin (1.0f, std::abs (width)); }

        void prepare (double sampleRate)
        {
            for (auto& filter : highPass)
                filter.c = Biquad::highPass (sampleRate, bassHz);
            width.reset (sampleRate, 0.05);
            reset();
        }

        void reset()
        {
            for (auto& filter : highPass)
                filter.reset();
            width.setCurrentAndTargetValue (width.getTargetValue());
        }

        // WIDTH (-5 to +5), smoothed over 50 ms.
        void setWidth (float value) { width.setTargetValue (juce::jlimit (-5.0f, 5.0f, value)); }
        void setWidthImmediately (float value) { width.setCurrentAndTargetValue (juce::jlimit (-5.0f, 5.0f, value)); }

        void process (const juce::dsp::AudioBlock<float>& block) noexcept
        {
            if (block.getNumChannels() < 2)
                return;
            if (! width.isSmoothing() && juce::exactlyEqual (width.getTargetValue(), 0.0f))
            {
                // At 0: untouched, and fresh for when it's turned.
                if (! juce::exactlyEqual (highPass[0].z1, 0.0) || ! juce::exactlyEqual (highPass[0].z2, 0.0))
                    reset();
                return;
            }

            auto* left = block.getChannelPointer (0);
            auto* right = block.getChannelPointer (1);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                const auto w = width.getNextValue();
                const auto mid = 0.5f * (left[i] + right[i]);
                const auto side = 0.5f * (left[i] - right[i]);
                const auto highs = highPass[1].process (highPass[0].process (side));
                const auto lows = side - highs;
                const auto newSide = highs * sideGainFor (w) + lows * (1.0f - bassMonoFor (w));
                left[i] = mid + newSide;
                right[i] = mid - newSide;
            }
        }

    private:
        std::array<Biquad, 2> highPass;   // two Butterworth sections: 24 dB an octave
        juce::SmoothedValue<float> width { 0.0f };
    };
}
