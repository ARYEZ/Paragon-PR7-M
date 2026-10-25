#pragma once

#include <juce_dsp/juce_dsp.h>

#include "dsp/Analog.h"
#include "dsp/BassPunch.h"
#include "dsp/Clipper.h"
#include "dsp/GlueCompressor.h"
#include "dsp/Limiter.h"
#include "dsp/ToneShaper.h"
#include "dsp/TransientShaper.h"
#include "dsp/Width.h"

namespace effects
{
    // Every knob's module, in their order (see CLAUDE.md): BODY / FOCUS /
    // AIR, BASS PUNCH, TRANSIENTS, PUSH, ANALOG, WIDTH, then VOLUME (the
    // clipper and the limiter). The processor runs one; the assistant runs
    // its own on what it heard, so what it predicts is exactly what plays.
    class MasteringChain
    {
    public:
        // The knobs, in their own units.
        struct Settings
        {
            float air = 0.0f, focus = 0.0f, body = 0.0f, bassPunch = 0.0f, transients = 0.0f,
                  push = 0.0f, analog = 0.0f, width = 0.0f, volume = 0.0f;
        };

        // How much PUSH and VOLUME turned down in a block (dB), for their LEDs.
        struct Reduction
        {
            float push = 0.0f, volume = 0.0f;
        };

        void prepare (double sampleRate, int maxBlockSize, const Settings& settings)
        {
            tone.prepare (sampleRate);
            punch.prepare (sampleRate);
            transients.prepare (sampleRate);
            compressor.prepare (sampleRate);
            analog.prepare (sampleRate, maxBlockSize);
            width.prepare (sampleRate);
            clipper.prepare (sampleRate, maxBlockSize);
            limiter.prepare (sampleRate);   // its own gain stays at 0 dB: VOLUME's gain is the clipper's
            setImmediately (settings);
        }

        // The delay, the same at any setting.
        int getLatency() const noexcept { return analog.getLatency() + clipper.getLatency() + limiter.getLatency(); }

        void setImmediately (const Settings& s)
        {
            tone.setImmediately (s.air, s.focus, s.body);
            punch.setPunchImmediately (s.bassPunch);
            transients.setAmountImmediately (s.transients);
            compressor.setPushImmediately (s.push);
            analog.setAmountImmediately (s.analog);
            width.setWidthImmediately (s.width);
            clipper.setVolumeImmediately (s.volume);
        }

        // Smoothed: each module glides to its new setting.
        void set (const Settings& s)
        {
            tone.setAir (s.air);
            tone.setFocus (s.focus);
            tone.setBody (s.body);
            punch.setPunch (s.bassPunch);
            transients.setAmount (s.transients);
            compressor.setPush (s.push);
            analog.setAmount (s.analog);
            width.setWidth (s.width);
            clipper.setVolume (s.volume);
        }

        // Everything before VOLUME, in place; returns PUSH's reduction.
        float processBeforeVolume (const juce::dsp::AudioBlock<float>& block) noexcept
        {
            tone.process (block);
            punch.process (block);
            transients.process (block);
            const auto pushed = compressor.process (block);
            analog.process (block);
            width.process (block);
            return pushed;
        }

        // VOLUME, in place: its LEDs show both what the clipper shaves off
        // and what the limiter turns down.
        float processVolume (const juce::dsp::AudioBlock<float>& block) noexcept
        {
            const auto shaved = clipper.process (block);
            return shaved + limiter.process (block);
        }

        Reduction process (const juce::dsp::AudioBlock<float>& block) noexcept
        {
            const auto pushed = processBeforeVolume (block);
            return { pushed, processVolume (block) };
        }

    private:
        ToneShaper tone;
        BassPunch punch;
        TransientShaper transients;
        GlueCompressor compressor;
        AnalogColour analog;
        StereoWidth width;
        VolumeClipper clipper;
        TruePeakLimiter limiter;
    };
}
