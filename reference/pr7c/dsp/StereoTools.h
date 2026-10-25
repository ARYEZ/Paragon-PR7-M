#pragma once

#include <array>

#include "dsp/Biquad.h"
#include "dsp/EffectUtils.h"

namespace effects
{
    // MONO MAKER and STEREO WIDTH, on the stereo image (our design). Both
    // work on mid (what's the same in both sides) and side (the difference).
    //
    //   MONO MAKER  folds everything below its frequency to mono: the side
    //               signal goes through a steep high-pass (24 dB an octave,
    //               a Linkwitz-Riley crossover's), so the lows are left in
    //               the middle. The mid goes through the same crossover's
    //               two halves added back together, which keeps its level
    //               exactly but shifts its timing the same way as the side's,
    //               so above the frequency the stereo image is just as it
    //               was.
    //   WIDTH       scales the side signal: 0% is mono, 100% as it was, 200%
    //               twice as wide. The mid is never touched, so the mono
    //               sum stays the same at any width (mono-compatible).
    //
    // The mono maker comes first, so widening never spreads the lows again.
    // Each fades in and out over 20 ms, and its knob glides. Off, the audio
    // is untouched. On a mono track there's no image, so it does nothing.
    class StereoTools
    {
    public:
        struct Settings
        {
            bool monoOn = false;
            float monoFrequency = 100.0f;
            bool widthOn = false;
            float width = 1.0f;   // 0 to 2
        };

        void prepare (double sampleRate, int maxBlockSize);
        void reset();
        void process (juce::AudioBuffer<float>&, int numSamples, const Settings&) noexcept;

    private:
        void updateFilters (float frequency) noexcept;

        double sampleRate = 48000.0;
        bool fresh = true;
        BypassFade monoFade, widthFade;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> monoFrequency { 100.0f };
        juce::SmoothedValue<float> width { 1.0f };
        std::array<Biquad, 2> sideHighPass, midHighPass, midLowPass;   // each a pair of Butterworth sections
    };
}
