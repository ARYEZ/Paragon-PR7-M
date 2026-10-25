#pragma once

#include <array>
#include <cstdint>

#include <juce_core/juce_core.h>

namespace effects
{
    // One of the console's 72 channels, and its component tolerances (our
    // design, after the idea of modelling a real console's channel-to-channel
    // differences): every channel was built to the same design, but no two
    // came out quite the same. Each channel's differences are small, fixed
    // and the same every time: they come from the channel number, not from
    // chance. Channel 1 is the exact design.
    //
    // The EQ and gain differences scale what's dialled in, so a flat EQ is
    // flat on every channel.
    struct ConsoleChannel
    {
        static constexpr int count = 72;

        // Factors (1 = the design) and offsets (0 dB = the design).
        std::array<float, 5> eqFrequency { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };   // the EQ bands, top to bottom: about ±4%
        std::array<float, 5> eqGain { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };        // about ±4% of the gain dialled in
        float highPass = 1.0f, lowPass = 1.0f;                                 // the HPF / LPF corners: about ±3%
        float second = 1.0f, third = 1.0f;                                     // THD's 2nd and 3rd harmonics: about ±25%
        float hissDb = 0.0f, humDb = 0.0f;                                     // the noise floor: about ±1.5 dB, the hum ±3 dB

        static ConsoleChannel number (int channel) noexcept
        {
            ConsoleChannel c;
            channel = juce::jlimit (1, count, channel);
            if (channel == 1)
                return c;

            // A fixed number between -1 and 1 for each of this channel's parts.
            int part = 0;
            const auto spread = [channel, &part]
            {
                // splitmix64, on the channel and part numbers.
                constexpr std::uint64_t golden = 0x9e3779b97f4a7c15u, mixA = 0xbf58476d1ce4e5b9u, mixB = 0x94d049bb133111ebu;
                std::uint64_t x = static_cast<std::uint64_t> (channel) * golden + static_cast<std::uint64_t> (++part) * mixA;
                x = (x ^ (x >> 30)) * mixA;
                x = (x ^ (x >> 27)) * mixB;
                x ^= x >> 31;
                return (float) ((double) (x >> 11) / 9007199254740992.0) * 2.0f - 1.0f;   // 2^53
            };
            for (auto& f : c.eqFrequency)
                f = 1.0f + 0.04f * spread();
            for (auto& g : c.eqGain)
                g = 1.0f + 0.04f * spread();
            c.highPass = 1.0f + 0.03f * spread();
            c.lowPass = 1.0f + 0.03f * spread();
            c.second = 1.0f + 0.25f * spread();
            c.third = 1.0f + 0.25f * spread();
            c.hissDb = 1.5f * spread();
            c.humDb = 3.0f * spread();
            return c;
        }

        // The channel next to this one, as a stereo pair uses (72 wraps to 1).
        static int nextTo (int channel) noexcept { return juce::jlimit (1, count, channel) % count + 1; }
    };
}
