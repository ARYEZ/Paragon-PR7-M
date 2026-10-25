#pragma once

#include <array>
#include <atomic>

#include "dsp/EffectUtils.h"

namespace effects
{
    // The channel limiter, in stereo: a peak (not RMS) limiter with no
    // look-ahead, so it adds no delay. It reacts to every peak at once, so on
    // hard limiting the very start of each peak is squared off a little: the
    // price of zero latency, as on a hardware limiter.
    //
    // Three modes (our design):
    //   BRICK  a hard ceiling: nothing gets past THRESHOLD; it lets go over
    //          100 ms.
    //   SOFT   a 6 dB soft knee that starts easing in 3 dB below the
    //          threshold, and a gentler 150 ms release. The knee flattens
    //          out exactly at the threshold, so it holds the ceiling too.
    //          (An attack time would let every hit through: without
    //          look-ahead, a limiter has to react at once.)
    //   AUTO   BRICK's ceiling with a release that follows the music: quick
    //          after a short peak (so it stays loud), slow under steady
    //          limiting (so it doesn't distort).
    //
    // MAKE UP comes after; MIX blends the limited sound with the dry (below
    // 100%, some of the original peaks come back). LINK limits both channels
    // together, from the louder one.
    class Limiter
    {
    public:
        enum class Mode { brick, soft, autoRelease };   // saved by index: only append

        struct Settings
        {
            bool on = false;
            float thresholdDb = -1.0f;   // dBFS
            float makeUpDb = 0.0f;
            Mode mode = Mode::brick;
            float mix = 1.0f;            // 0 to 1
            bool link = true;
        };

        void prepare (double sampleRate, int maxBlockSize);
        void reset();
        void process (juce::AudioBuffer<float>&, int numSamples, const Settings&) noexcept;

        // The most gain reduction on a channel in the last block (dB, 0 or more).
        float getReductionDb (int channel) const noexcept { return reductionDb[(size_t) juce::jlimit (0, 1, channel)].load(); }

        static constexpr float softKneeDb = 6.0f;

    private:
        double sampleRate = 48000.0;
        bool fresh = true;
        BypassFade bypass;
        juce::SmoothedValue<float> makeUp { 1.0f }, mix { 1.0f }, threshold { 1.0f };
        std::array<float, 2> envelope {}, memory {};
        std::array<std::atomic<float>, 2> reductionDb {};
    };
}
