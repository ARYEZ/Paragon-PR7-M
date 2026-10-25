#pragma once

#include <array>
#include <cmath>

#include <juce_core/juce_core.h>

namespace metering
{
    // True peak (ITU-R BS.1770-4, Annex 2): the highest point of the
    // waveform itself, including peaks that fall between the samples. Those
    // are what clip a converter or an MP3/AAC encoder even when no sample
    // reaches 0 dBFS. The audio is oversampled 4x (2x from 96 kHz, none from
    // 192 kHz) with a windowed-sinc interpolator, and the highest point of
    // the result is taken. 32 taps per phase (the standard's example uses 12)
    // keeps the reading within about 0.1 dB up to 20 kHz.
    class TruePeak
    {
    public:
        static constexpr int tapsPerPhase = 32;
        static constexpr int maxFactor = 8;

        // `oversampling` is for 44.1/48 kHz: 4 as the standard says (the
        // meter), or 8 for a closer look (the limiter: 4x can miss a peak
        // falling between its points by up to about 0.2 dB at 14 kHz). Half
        // that from 96 kHz, a quarter from 192 kHz.
        void prepare (double sampleRate, int oversampling = 4)
        {
            const auto steps = sampleRate < 96000.0 ? 1 : sampleRate < 192000.0 ? 2 : 4;
            factor = juce::jlimit (1, maxFactor, oversampling / steps);
            design();
            reset();
        }

        void reset() noexcept
        {
            for (auto& channel : history)
                channel.fill (0.0f);
            positions.fill (0);
        }

        // How late next()'s readings are, in samples: the interpolator's delay.
        static constexpr int delay = tapsPerPhase / 2;

        // Takes the next sample of one channel (0 or 1) and returns the
        // highest point of the waveform (linear) between the two samples
        // `delay` and `delay - 1` back, both included: what a limiter needs
        // to keep every point of that stretch under its ceiling.
        float next (int channel, float sample) noexcept
        {
            auto& past = history[(size_t) juce::jlimit (0, 1, channel)];
            auto& position = positions[(size_t) juce::jlimit (0, 1, channel)];

            // Kept twice over, so the last `tapsPerPhase` samples are always
            // in one run, oldest first: past[position + 1 ...].
            past[(size_t) position] = past[(size_t) (position + tapsPerPhase)] = sample;
            const auto* window = past.data() + position + 1;
            position = (position + 1) % tapsPerPhase;

            // Phase 0 is the sample `delay` back itself; the others fall
            // between it and the one after.
            auto peak = std::fmax (std::abs (window[tapsPerPhase - 1 - delay]), std::abs (window[tapsPerPhase - delay]));
            for (int phase = 1; phase < factor; ++phase)
            {
                const auto* taps = phases[(size_t) phase].data();
                auto value = 0.0f;
                for (int t = 0; t < tapsPerPhase; ++t)
                    value += taps[t] * window[t];
                peak = std::fmax (peak, std::abs (value));
            }
            return peak;
        }

        // The highest true peak (linear, 1 = 0 dBTP) of these samples of one
        // channel (0 or 1), for the meter.
        float process (int channel, const float* samples, int numSamples) noexcept
        {
            auto peak = 0.0f;
            for (int i = 0; i < numSamples; ++i)
                peak = std::fmax (peak, std::fmax (std::abs (samples[i]), next (channel, samples[i])));
            return peak;
        }

        int getFactor() const noexcept { return factor; }

    private:
        // A low-pass at the original Nyquist frequency, Kaiser-windowed (70 dB
        // down), split into one short filter per in-between position.
        void design()
        {
            const auto length = factor * tapsPerPhase;
            const auto centre = (double) (factor * tapsPerPhase / 2);
            constexpr double beta = 6.76;
            const auto bessel = [] (double x)   // the modified Bessel function I0, by its series
            {
                double sum = 1.0, term = 1.0;
                for (int k = 1; k < 30; ++k)
                {
                    term *= (x / (2.0 * k)) * (x / (2.0 * k));
                    sum += term;
                }
                return sum;
            };

            for (int phase = 0; phase < factor; ++phase)
            {
                std::array<double, tapsPerPhase> taps {};
                auto total = 0.0;
                for (int k = 0; k < tapsPerPhase; ++k)
                {
                    const auto index = phase + k * factor;
                    const auto x = ((double) index - centre) / (double) factor;
                    const auto sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
                    const auto ratio = ((double) index - centre) / ((double) length * 0.5);
                    const auto window = bessel (beta * std::sqrt (juce::jmax (0.0, 1.0 - ratio * ratio))) / bessel (beta);
                    taps[(size_t) k] = sinc * window;
                    total += taps[(size_t) k];
                }
                // Oldest sample first, matching the history's order, and
                // each phase passing a steady level unchanged.
                for (int k = 0; k < tapsPerPhase; ++k)
                    phases[(size_t) phase][(size_t) (tapsPerPhase - 1 - k)] = (float) (taps[(size_t) k] / total);
            }
        }

        int factor = 4;
        std::array<std::array<float, tapsPerPhase>, maxFactor> phases {};
        std::array<std::array<float, tapsPerPhase * 2>, 2> history {};
        std::array<int, 2> positions {};
    };
}
