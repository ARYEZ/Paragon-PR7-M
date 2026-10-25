#pragma once

#include <cmath>

#include <juce_core/juce_core.h>

namespace effects
{
    // One second-order filter section (a "biquad"), with the standard
    // audio-EQ-cookbook designs. Coefficients can change between blocks while
    // it runs; the transposed direct form II structure takes that smoothly.
    // Real-time safe: no allocation.
    struct Biquad
    {
        struct Coefficients
        {
            double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;   // normalised: a0 = 1
        };

        Coefficients c;
        double z1 = 0.0, z2 = 0.0;

        void reset() noexcept { z1 = z2 = 0.0; }

        float process (float input) noexcept
        {
            const auto x = (double) input;
            const auto y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return (float) y;
        }

        void process (float* samples, int numSamples) noexcept
        {
            for (int i = 0; i < numSamples; ++i)
                samples[i] = process (samples[i]);
        }

        // The designs. `frequency` is kept safely below Nyquist.
        static Coefficients bell (double sampleRate, double frequency, double q, double gainDb) noexcept
        {
            const auto [cosW, alpha] = prewarp (sampleRate, frequency, q);
            const auto a = std::pow (10.0, gainDb / 40.0);
            return normalise (1.0 + alpha * a, -2.0 * cosW, 1.0 - alpha * a, 1.0 + alpha / a, -2.0 * cosW, 1.0 - alpha / a);
        }

        // Shelves with the cookbook's slope of 1 (no overshoot); `frequency`
        // is where the shelf has reached half its gain (in dB).
        static Coefficients lowShelf (double sampleRate, double frequency, double gainDb) noexcept
        {
            const auto [cosW, alpha] = prewarp (sampleRate, frequency, juce::MathConstants<double>::sqrt2 * 0.5);
            const auto a = std::pow (10.0, gainDb / 40.0);
            const auto twoRootAAlpha = 2.0 * std::sqrt (a) * alpha;
            return normalise (a * ((a + 1.0) - (a - 1.0) * cosW + twoRootAAlpha),
                              2.0 * a * ((a - 1.0) - (a + 1.0) * cosW),
                              a * ((a + 1.0) - (a - 1.0) * cosW - twoRootAAlpha),
                              (a + 1.0) + (a - 1.0) * cosW + twoRootAAlpha,
                              -2.0 * ((a - 1.0) + (a + 1.0) * cosW),
                              (a + 1.0) + (a - 1.0) * cosW - twoRootAAlpha);
        }

        static Coefficients highShelf (double sampleRate, double frequency, double gainDb) noexcept
        {
            const auto [cosW, alpha] = prewarp (sampleRate, frequency, juce::MathConstants<double>::sqrt2 * 0.5);
            const auto a = std::pow (10.0, gainDb / 40.0);
            const auto twoRootAAlpha = 2.0 * std::sqrt (a) * alpha;
            return normalise (a * ((a + 1.0) + (a - 1.0) * cosW + twoRootAAlpha),
                              -2.0 * a * ((a - 1.0) + (a + 1.0) * cosW),
                              a * ((a + 1.0) + (a - 1.0) * cosW - twoRootAAlpha),
                              (a + 1.0) - (a - 1.0) * cosW + twoRootAAlpha,
                              2.0 * ((a - 1.0) - (a + 1.0) * cosW),
                              (a + 1.0) - (a - 1.0) * cosW - twoRootAAlpha);
        }

        // 12 dB/octave; at the default Q (0.707, Butterworth) -3 dB at
        // `frequency`. A higher Q gives a resonant bump at the cutoff.
        static Coefficients highPass (double sampleRate, double frequency, double q = juce::MathConstants<double>::sqrt2 * 0.5) noexcept
        {
            const auto [cosW, alpha] = prewarp (sampleRate, frequency, q);
            return normalise ((1.0 + cosW) * 0.5, -(1.0 + cosW), (1.0 + cosW) * 0.5, 1.0 + alpha, -2.0 * cosW, 1.0 - alpha);
        }

        static Coefficients lowPass (double sampleRate, double frequency, double q = juce::MathConstants<double>::sqrt2 * 0.5) noexcept
        {
            const auto [cosW, alpha] = prewarp (sampleRate, frequency, q);
            return normalise ((1.0 - cosW) * 0.5, 1.0 - cosW, (1.0 - cosW) * 0.5, 1.0 + alpha, -2.0 * cosW, 1.0 - alpha);
        }

    private:
        struct Warped { double cosW, alpha; };

        static Warped prewarp (double sampleRate, double frequency, double q) noexcept
        {
            const auto w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sampleRate * 0.45, frequency) / sampleRate;
            return { std::cos (w), std::sin (w) / (2.0 * q) };
        }

        static Coefficients normalise (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
        {
            return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
        }
    };
}
