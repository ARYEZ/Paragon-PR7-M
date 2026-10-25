#pragma once

#include <array>
#include <memory>

#include "dsp/EffectUtils.h"

namespace effects
{
    // The console colour of FILTERS / TONE, in stereo (our design): what a
    // console's own electronics add to everything through it.
    //
    //   THD     the line amplifiers' gentle harmonics, a 2nd and a smaller
    //           3rd. They grow with the level, as a real amplifier's do: at
    //           100 about 3% 2nd harmonic at digital full scale, 0.3% at the
    //           console's nominal level (-20 dBFS), less below. The level
    //           stays where it is.
    //   NOISE   (V GAIN) the console's noise floor: hiss, different on each
    //           side, and a little 50 Hz mains hum. At 100, the hiss is
    //           about -64 dBFS and the hum about -72 dBFS.
    //
    // Each side can be a different console channel, with its own slightly
    // different amounts (`ConsoleChannel`).
    //
    // THD's curve is a smooth polynomial (x, x², x³) run at twice the sample
    // rate, which is enough that its harmonics never fold back down as
    // aliasing. While THD is on, that shifts the sound by a few samples;
    // while it's off, the audio is untouched (the filters keep running on a
    // copy, so switching on doesn't ring).
    class ConsoleColour
    {
    public:
        struct Settings
        {
            float thd = 0.0f;     // 0 (off) to 1
            float noise = 0.0f;   // 0 (off) to 1
            // Each side's channel tolerances: THD's 2nd and 3rd (factors),
            // hiss and hum (dB offsets).
            std::array<float, 2> second { 1.0f, 1.0f }, third { 1.0f, 1.0f }, hissDb {}, humDb {};
        };

        void prepare (double sampleRate, int maxBlockSize);
        void reset();
        void process (juce::AudioBuffer<float>&, int numSamples, const Settings&) noexcept;

        // THD's curve at full amount, for one sample.
        static constexpr float secondAtFull = 0.06f, thirdAtFull = 0.04f;
        static constexpr float hissDbAtFull = -64.0f, humDbAtFull = -72.0f;

    private:
        void addNoise (juce::AudioBuffer<float>&, int numSamples, const Settings&) noexcept;

        double sampleRate = 48000.0;
        bool fresh = true;
        BypassFade bypass;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        std::array<juce::SmoothedValue<float>, 2> second, third, hiss, hum;
        std::array<float, 2> dcIn {}, dcOut {};   // x² leaves a DC offset: removed below 5 Hz
        float dcCoefficient = 0.9995f;
        juce::AudioBuffer<float> warmUp;
        std::array<std::uint32_t, 2> random { 0x12345678u, 0x9abcdef1u };   // each side's hiss
        double humPhase = 0.0;
    };
}
