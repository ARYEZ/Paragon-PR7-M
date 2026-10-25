#pragma once

#include <cmath>

#include <juce_core/juce_core.h>

namespace metering
{
    // A VU meter's needle (IEC 60268-17): it follows the average of the
    // rectified signal through the needle's own mass and spring, so it
    // takes 300 ms to reach 99% of a steady tone, overshoots by about 1.5%
    // and comes back down at the same speed. Modelled as that spring
    // (stiffness and damping worked out to give exactly 300 ms and 1.25%
    // overshoot), run at the sample rate so it moves the same at any block
    // size.
    //
    // The reading is linear and scaled so a steady sine reads its peak: a
    // sine peaking at -14 dBFS reads 0.2 (-14 dB), the way "0 VU = -14 dBFS"
    // is meant.
    class VuBallistics
    {
    public:
        static constexpr double damping = 0.8127169864286775;   // 1.25% overshoot
        static constexpr double omega = 13.511912562582637;     // rad/s: 99% at 300 ms

        void prepare (double sampleRate)
        {
            dt = 1.0 / sampleRate;
            reset();
        }

        void reset() noexcept { position = velocity = 0.0; }

        // Moves the needle through these samples; returns where it ends up.
        float process (const float* samples, int numSamples) noexcept
        {
            // The average of a rectified sine is 2/pi of its peak.
            constexpr auto toPeak = juce::MathConstants<double>::halfPi;
            for (int i = 0; i < numSamples; ++i)
            {
                const auto drive = std::abs ((double) samples[i]) * toPeak;
                velocity += (omega * omega * (drive - position) - 2.0 * damping * omega * velocity) * dt;
                position += velocity * dt;
            }
            if (std::abs (position) < 1.0e-9 && std::abs (velocity) < 1.0e-9)   // at rest: no denormals
                position = velocity = 0.0;
            return (float) juce::jmax (0.0, position);
        }

        float getPosition() const noexcept { return (float) juce::jmax (0.0, position); }

    private:
        double dt = 1.0 / 48000.0, position = 0.0, velocity = 0.0;
    };
}
