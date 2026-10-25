#pragma once

#include <array>
#include <memory>

#include "dsp/EffectUtils.h"

namespace effects
{
    // The channel saturation, in stereo (our design, after the general idea
    // of a harmonics generator: a drive, a balance of even and odd
    // harmonics, and a dry / wet mix).
    //
    //   DRIVE      0 to +24 dB into the saturation. The level is compensated,
    //              so DRIVE changes the colour, not the loudness. At 0 dB the
    //              saturation point is 6 dB above digital full scale, so
    //              normal levels are only lightly coloured; turning DRIVE up
    //              pushes them in (at +24 dB the console's nominal level is
    //              well into it).
    //   HARMONICS  0: 2nd-harmonic warmth from a lopsided, valve-like curve;
    //              1: 3rd-harmonic grit from a symmetrical, transistor-like
    //              curve; in between, a blend.
    //   VCA CLIP   a much harder clip on the peaks before the saturation, for
    //              crunch, like overdriving a console's VCA.
    //   MIX        dry / wet, for parallel saturation.
    //
    // The curves run at 4x the sample rate, so the harmonics they make above
    // the audio range are filtered away instead of folding back down as
    // aliasing. The dry signal goes through the same filters, so MIX blends
    // two signals that line up. While on, that shifts the sound by a few
    // samples (about 0.1 ms at 48 kHz). While off, the audio is untouched,
    // but those filters keep running on a copy of it, so switching on
    // doesn't start them cold (which rings: a small click).
    class Saturation
    {
    public:
        struct Settings
        {
            bool on = false;
            float driveDb = 6.0f;
            float harmonics = 0.3f;   // 0 (2nd) to 1 (3rd)
            bool vcaClip = false;
            float mix = 1.0f;         // 0 to 1
        };

        void prepare (double sampleRate, int maxBlockSize);
        void reset();
        void process (juce::AudioBuffer<float>&, int numSamples, const Settings&) noexcept;

        // The curves, for one sample in the driven domain (1 = the saturation
        // point). Both rise with a slope of 1 at silence.
        static float evenCurve (float u) noexcept;   // valve-like: lopsided, mostly 2nd harmonic
        static float oddCurve (float u) noexcept;    // transistor-like: symmetrical, mostly 3rd
        static float vcaClip (float u) noexcept;     // a hard-kneed clip at 0.8

        static constexpr int oversamplingStages = 2;   // 4x
        static constexpr float headroom = 2.0f;        // the saturation point at DRIVE 0: +6 dBFS

    private:
        double sampleRate = 48000.0;
        bool fresh = true;
        BypassFade bypass;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        juce::SmoothedValue<float> drive { 1.0f }, harmonics { 0.3f }, clip { 0.0f }, mix { 1.0f };
        std::array<float, 2> dcIn {}, dcOut {};   // a gentle DC blocker: the valve curve's lopsidedness leaves a DC offset
        float dcCoefficient = 0.9995f;
        juce::AudioBuffer<float> warmUp;   // the copy the filters keep running on while off
    };
}
