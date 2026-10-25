#pragma once

#include <array>
#include <atomic>
#include <memory>

#include "dsp/EffectUtils.h"

namespace effects
{
    // The channel compressor, in stereo. It listens to a separate sidechain
    // signal (the audio itself, or the audio through the HPF / LPF when the
    // filters are set to SC), and turns the audio down.
    //
    // Three characters (our design, not a copy of any unit):
    //   VCA   an RMS detector (it follows loudness), a near-hard knee: clean
    //         and precise.
    //   OPTO  a slower RMS detector, a wide soft knee and a gentler attack,
    //         and a release that stretches the longer it has been working,
    //         like an optical cell: smooth.
    //   FET   a peak detector (it catches every transient), a firm knee,
    //         and a touch of third-harmonic grit that grows with the gain
    //         reduction: fast and aggressive. The grit is worked out at
    //         twice the sample rate, so its harmonics never fold back down
    //         as aliasing, and added to the compressed sound. Only the grit
    //         goes through those filters (a few samples late, which can't
    //         be heard at 1%); the rest of the sound doesn't, so VCA and OPTO
    //         are untouched and switching character doesn't shift anything.
    //
    // RATIO's SOFT position (ratio 1) is a curve whose ratio rises with
    // level: about 3:1 at 8 dB over the threshold, 6:1 at 20 dB, 10:1 at
    // 36 dB. Ratios from 30 up are infinite (a limiter's flat top).
    //
    // AUTO gives VCA and FET the same stretching release: quick after a
    // short peak, slower after long compression, so it doesn't pump.
    // LINK gives both channels the same reduction, from the louder one.
    class Compressor
    {
    public:
        enum class Character { vca, opto, fet };   // saved by index: only append

        struct Settings
        {
            bool on = false;
            float thresholdDb = -18.0f;   // dBFS
            float ratio = 4.0f;           // 1 = SOFT; 30 or more = infinite
            float attackMs = 10.0f, releaseMs = 300.0f;
            bool autoRelease = false;
            float makeUpDb = 0.0f;
            Character character = Character::vca;
            bool link = true;
        };

        static constexpr float softRatio = 1.0f, infiniteRatio = 30.0f;

        void prepare (double sampleRate, int maxBlockSize);
        void reset();

        // `sidechain` is what the detector hears, the same length as `audio`.
        void process (juce::AudioBuffer<float>& audio, const juce::AudioBuffer<float>& sidechain, int numSamples,
                      const Settings&) noexcept;

        // The most gain reduction on a channel during the last block (dB, 0 or
        // more), for the meters. 0 while off.
        float getReductionDb (int channel) const noexcept { return reductionDb[(size_t) juce::jlimit (0, 1, channel)].load(); }

        // The static curve: the gain change (dB, 0 or less) for a steady level.
        static float gainComputerDb (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept;
        static float kneeDbFor (Character) noexcept;

    private:
        double sampleRate = 48000.0;
        bool fresh = true;   // the first block after prepare: on or off at once, no fade
        BypassFade bypass;
        juce::SmoothedValue<float> makeUp { 1.0f }, fetGrit { 0.0f };   // the grit fades in and out with FET
        std::array<float, 2> meanSquare {}, peak {}, reduction {}, memory {};
        std::array<std::atomic<float>, 2> reductionDb {};

        // FET grit at twice the rate: per sample, its amount and the level
        // the waveform is measured against, and the sound it works on.
        void addGrit (juce::dsp::AudioBlock<float>, int numSamples) noexcept;
        std::unique_ptr<juce::dsp::Oversampling<float>> gritOversampling;
        juce::AudioBuffer<float> gritAmount, gritLevel, gritSignal;
        bool gritRunning = false;
    };
}
