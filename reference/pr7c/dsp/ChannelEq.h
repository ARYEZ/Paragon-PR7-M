#pragma once

#include <array>

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Biquad.h"

namespace effects
{
    // One band of the channel EQ, in stereo (the same settings on both
    // channels, each with its own filter memory).
    //
    // Every change glides: frequency, gain and Q over 30 ms, and the
    // coefficients are recalculated every 32 samples, so sweeping a knob
    // doesn't zipper. Switching the band off glides its gain to 0 dB.
    // Switching between shelf and bell fades the band out, swaps the curve
    // and fades it back in, so it never clicks. While the band does nothing
    // (gain 0 dB) it isn't run at all, so the audio passes through untouched.
    class EqBand
    {
    public:
        // Which shelf a band becomes when it isn't a bell.
        enum class Shelf { none, low, high };

        explicit EqBand (Shelf shelf) : shelfType (shelf) {}

        void prepare (double sampleRate);
        void reset();

        struct Settings
        {
            bool on = true;
            float frequency = 1000.0f, gainDb = 0.0f, q = 1.0f;
            bool shelf = false;   // ignored by a band with Shelf::none
        };

        void process (juce::AudioBuffer<float>&, int numSamples, const Settings&) noexcept;

        // The bell's Q at a given gain. Console EQs narrow as they boost or
        // cut harder ("proportional Q"): broad and gentle for small moves,
        // more focused for big ones. The Q knob reads true at ±6 dB.
        static float proportionalQ (float q, float gainDb) noexcept;

        // The Q a shelf-or-bell band (HF, LMF, LF) uses as a bell.
        static constexpr float fixedBellQ = 0.9f;

    private:
        Biquad::Coefficients design (float frequency, float gainDb, float q) const noexcept;

        Shelf shelfType;
        double sampleRate = 48000.0;
        bool fresh = true;      // the first block after prepare / reset: jump to the settings, no glide
        bool isShelf = false;   // the curve it's running now
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> frequency { 1000.0f }, q { 1.0f };
        juce::SmoothedValue<float> gainDb { 0.0f }, shapeFade { 1.0f };
        std::array<Biquad, 2> filters;
    };

    // The HPF and LPF in one place in the chain (12 dB/octave each). Each
    // fades in and out over 20 ms when it's switched on or off, or when the
    // IN / POST / SC switch moves it to or from here. While off here, it
    // isn't run.
    class CutFilters
    {
    public:
        void prepare (double sampleRate);
        void reset();

        struct Settings
        {
            bool highPass = false, lowPass = false;   // on, at this place in the chain
            float highPassFrequency = 80.0f, lowPassFrequency = 18000.0f;
            float q = butterworthQ;   // higher: resonant (the SC FILTER's HIGH Q)
        };

        static constexpr float butterworthQ = 0.7071f;

        void process (juce::AudioBuffer<float>&, int numSamples, const Settings&) noexcept;

    private:
        struct Filter
        {
            std::array<Biquad, 2> biquads;
            juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> frequency { 1000.0f }, q { butterworthQ };
            juce::SmoothedValue<float> wet { 0.0f };
        };

        void process (Filter&, bool highPass, juce::AudioBuffer<float>&, int numSamples, bool on, float frequency, float q) noexcept;

        double sampleRate = 48000.0;
        bool fresh = true;      // the first block after prepare / reset: jump to the settings, no fade
        Filter highPass, lowPass;
    };
}
