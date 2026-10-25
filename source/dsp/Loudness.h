#pragma once

#include <array>
#include <cmath>

#include "dsp/Biquad.h"

namespace metering
{
    constexpr int maxChannels = 2;

    // Loudness as ITU-R BS.1770-4 / EBU R128 measure it (the LUFS that
    // streaming services go by). The audio is "K-weighted" first: a gentle
    // high shelf (+4 dB above about 1.7 kHz, for how the head hears) and a
    // high-pass at about 38 Hz (deep bass counts for less), then its power
    // is averaged.
    //
    // The two filters, designed for any sample rate. These are the standard's
    // own filters (it prints them for 48 kHz only); the formulas are the
    // usual ones that reproduce them exactly at 48 kHz (as in libebur128).
    struct KWeighting
    {
        static effects::Biquad::Coefficients shelf (double sampleRate) noexcept
        {
            constexpr double f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;
            const auto k = std::tan (juce::MathConstants<double>::pi * f0 / sampleRate);
            const auto vh = std::pow (10.0, gainDb / 20.0);
            const auto vb = std::pow (vh, 0.4996667741545416);
            const auto a0 = 1.0 + k / q + k * k;
            return { (vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0,
                     2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
        }

        static effects::Biquad::Coefficients highPass (double sampleRate) noexcept
        {
            constexpr double f0 = 38.13547087602444, q = 0.5003270373238773;
            const auto k = std::tan (juce::MathConstants<double>::pi * f0 / sampleRate);
            const auto a0 = 1.0 + k / q + k * k;
            return { 1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
        }
    };

    // Short-term loudness: the loudness of the last 3 seconds, in LUFS,
    // updated every 100 ms (EBU R128's "S"). Left and right count equally;
    // a mono track counts as one channel, as the standard says. Real-time
    // safe: no allocation after prepare().
    class ShortTermLoudness
    {
    public:
        static constexpr float silent = -100.0f;      // what it reads with nothing playing
        static constexpr int blocksPerWindow = 30;    // 3 s of 100 ms blocks

        void prepare (double sampleRate)
        {
            for (auto& channel : filters)
            {
                channel[0].c = KWeighting::shelf (sampleRate);
                channel[1].c = KWeighting::highPass (sampleRate);
            }
            samplesPerBlock = juce::jmax (1, (int) std::round (sampleRate * 0.1));
            reset();
        }

        void reset() noexcept
        {
            for (auto& channel : filters)
                for (auto& filter : channel)
                    filter.reset();
            blocks.fill (0.0);
            sum = 0.0;
            samplesInBlock = 0;
            next = 0;
            lufs = silent;
        }

        // Adds `numSamples` of each of `numChannels` channels (1 or 2).
        // Returns true if a 100 ms block finished, so the reading moved on.
        bool process (const float* const* channels, int numChannels, int numSamples) noexcept
        {
            numChannels = juce::jmin (numChannels, maxChannels);
            auto finished = false;
            for (int i = 0; i < numSamples; ++i)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto& [shelfFilter, highPassFilter] = filters[(size_t) channel];
                    const auto weighted = (double) highPassFilter.process (shelfFilter.process (channels[channel][i]));
                    sum += weighted * weighted;
                }

                if (++samplesInBlock == samplesPerBlock)
                {
                    blocks[(size_t) next] = sum / (double) samplesPerBlock;
                    next = (next + 1) % blocksPerWindow;
                    sum = 0.0;
                    samplesInBlock = 0;
                    update();
                    finished = true;
                }
            }
            return finished;
        }

        float getLufs() const noexcept { return lufs; }

    private:
        void update() noexcept
        {
            auto power = 0.0;
            for (const auto block : blocks)
                power += block;
            power /= (double) blocksPerWindow;
            lufs = power > 1.0e-10 ? juce::jmax (silent, (float) (-0.691 + 10.0 * std::log10 (power))) : silent;
        }

        std::array<std::array<effects::Biquad, 2>, maxChannels> filters;
        std::array<double, blocksPerWindow> blocks {};
        double sum = 0.0;
        int samplesPerBlock = 4800, samplesInBlock = 0, next = 0;
        float lufs = silent;
    };
}
