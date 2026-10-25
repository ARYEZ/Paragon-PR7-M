#include "dsp/ChannelEq.h"

#include <algorithm>

namespace effects
{
    namespace
    {
        constexpr int updateInterval = 32;   // samples between coefficient updates
    }

    //==============================================================================
    void EqBand::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        frequency.reset (sampleRate, 0.03);
        q.reset (sampleRate, 0.03);
        gainDb.reset (sampleRate, 0.03);
        shapeFade.reset (sampleRate, 0.015);
        reset();
    }

    void EqBand::reset()
    {
        for (auto& filter : filters)
            filter.reset();
        fresh = true;
    }

    float EqBand::proportionalQ (float knobQ, float gain) noexcept
    {
        return knobQ * std::pow (2.0f, (std::abs (gain) - 6.0f) / 18.0f);
    }

    Biquad::Coefficients EqBand::design (float f, float gain, float bandQ) const noexcept
    {
        if (isShelf && shelfType == Shelf::low)
            return Biquad::lowShelf (sampleRate, f, gain);
        if (isShelf && shelfType == Shelf::high)
            return Biquad::highShelf (sampleRate, f, gain);
        return Biquad::bell (sampleRate, f, proportionalQ (bandQ, gain), gain);
    }

    void EqBand::process (juce::AudioBuffer<float>& buffer, int numSamples, const Settings& settings) noexcept
    {
        const auto bandQ = shelfType == Shelf::none ? settings.q : fixedBellQ;   // shelves ignore Q
        if (fresh)
        {
            // Playback is starting: be at the settings from the first sample.
            fresh = false;
            frequency.setCurrentAndTargetValue (settings.frequency);
            q.setCurrentAndTargetValue (bandQ);
            gainDb.setCurrentAndTargetValue (settings.on ? settings.gainDb : 0.0f);
            shapeFade.setCurrentAndTargetValue (1.0f);
            isShelf = shelfType != Shelf::none && settings.shelf;
        }
        if (! gainDb.isSmoothing() && juce::exactlyEqual (gainDb.getCurrentValue(), 0.0f))
        {
            // Silent: no need to glide to the new frequency and Q.
            frequency.setCurrentAndTargetValue (settings.frequency);
            q.setCurrentAndTargetValue (bandQ);
        }
        frequency.setTargetValue (settings.frequency);
        q.setTargetValue (bandQ);
        gainDb.setTargetValue (settings.on ? settings.gainDb : 0.0f);

        // A change of curve: fade the band out, swap at 0 dB, fade it back
        // in. If it's at 0 dB already, the swap is silent, so just swap.
        const auto wantShelf = shelfType != Shelf::none && settings.shelf;
        if (wantShelf != isShelf && (juce::exactlyEqual (gainDb.getCurrentValue(), 0.0f) || shapeFade.getCurrentValue() <= 0.0f))
            isShelf = wantShelf;
        shapeFade.setTargetValue (wantShelf != isShelf ? 0.0f : 1.0f);

        // Doing nothing: 0 dB and settled. Once whatever was still ringing in
        // the filters has died away, the band stops running and its memory is
        // cleared (which is what a 0 dB filter's memory holds anyway), so
        // starting again later is seamless.
        const auto settled = ! frequency.isSmoothing() && ! q.isSmoothing() && ! gainDb.isSmoothing() && ! shapeFade.isSmoothing();
        if (settled && juce::exactlyEqual (gainDb.getCurrentValue(), 0.0f)
            && std::all_of (filters.begin(), filters.end(), [] (const Biquad& f) { return std::abs (f.z1) + std::abs (f.z2) < 1.0e-7; }))
        {
            reset();
            return;
        }

        const auto numChannels = juce::jmin (buffer.getNumChannels(), (int) filters.size());
        for (int start = 0; start < numSamples; start += updateInterval)
        {
            const auto length = juce::jmin (updateInterval, numSamples - start);
            const auto f = frequency.skip (length);
            const auto currentQ = q.skip (length);
            const auto gain = gainDb.skip (length) * shapeFade.skip (length);
            const auto coefficients = design (f, gain, currentQ);
            for (int channel = 0; channel < numChannels; ++channel)
            {
                filters[(size_t) channel].c = coefficients;
                filters[(size_t) channel].process (buffer.getWritePointer (channel, start), length);
            }

            // Swap the curve the moment the fade reaches silence, even mid-block.
            if (wantShelf != isShelf && shapeFade.getCurrentValue() <= 0.0f)
            {
                isShelf = wantShelf;
                shapeFade.setTargetValue (1.0f);
            }
        }
    }

    //==============================================================================
    void CutFilters::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        for (auto* filter : { &highPass, &lowPass })
        {
            filter->frequency.reset (sampleRate, 0.05);
            filter->q.reset (sampleRate, 0.05);
            filter->wet.reset (sampleRate, 0.02);
        }
        reset();
    }

    void CutFilters::reset()
    {
        for (auto* filter : { &highPass, &lowPass })
            for (auto& biquad : filter->biquads)
                biquad.reset();
        fresh = true;
    }

    void CutFilters::process (juce::AudioBuffer<float>& buffer, int numSamples, const Settings& settings) noexcept
    {
        if (fresh)
        {
            // Playback is starting: be at the settings from the first sample.
            fresh = false;
            highPass.frequency.setCurrentAndTargetValue (settings.highPassFrequency);
            highPass.q.setCurrentAndTargetValue (settings.q);
            lowPass.q.setCurrentAndTargetValue (settings.q);
            highPass.wet.setCurrentAndTargetValue (settings.highPass ? 1.0f : 0.0f);
            lowPass.frequency.setCurrentAndTargetValue (settings.lowPassFrequency);
            lowPass.wet.setCurrentAndTargetValue (settings.lowPass ? 1.0f : 0.0f);
        }
        process (highPass, true, buffer, numSamples, settings.highPass, settings.highPassFrequency, settings.q);
        process (lowPass, false, buffer, numSamples, settings.lowPass, settings.lowPassFrequency, settings.q);
    }

    void CutFilters::process (Filter& filter, bool isHighPass, juce::AudioBuffer<float>& buffer, int numSamples, bool on, float frequency, float q) noexcept
    {
        filter.q.setTargetValue (q);
        // Snap the frequency while it's off, so it doesn't sweep in from an
        // old setting when switched on.
        if (filter.wet.getCurrentValue() <= 0.0f && ! filter.wet.isSmoothing())
            filter.frequency.setCurrentAndTargetValue (frequency);
        filter.frequency.setTargetValue (frequency);
        filter.wet.setTargetValue (on ? 1.0f : 0.0f);

        if (! filter.wet.isSmoothing() && filter.wet.getCurrentValue() <= 0.0f)
        {
            for (auto& biquad : filter.biquads)
                biquad.reset();
            return;
        }

        const auto numChannels = juce::jmin (buffer.getNumChannels(), (int) filter.biquads.size());
        for (int start = 0; start < numSamples; start += updateInterval)
        {
            const auto length = juce::jmin (updateInterval, numSamples - start);
            const auto f = filter.frequency.skip (length);
            const auto filterQ = filter.q.skip (length);
            const auto coefficients = isHighPass ? Biquad::highPass (sampleRate, f, filterQ) : Biquad::lowPass (sampleRate, f, filterQ);

            if (! filter.wet.isSmoothing())
            {
                // Finished fading out part-way through the block: the rest is dry.
                if (filter.wet.getCurrentValue() <= 0.0f)
                    break;
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    filter.biquads[(size_t) channel].c = coefficients;
                    filter.biquads[(size_t) channel].process (buffer.getWritePointer (channel, start), length);
                }
                continue;
            }

            // Fading in or out: blend the filtered signal with the dry one.
            const auto wetStart = filter.wet.getCurrentValue();
            const auto wetEnd = filter.wet.skip (length);
            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto& biquad = filter.biquads[(size_t) channel];
                biquad.c = coefficients;
                auto* samples = buffer.getWritePointer (channel, start);
                for (int i = 0; i < length; ++i)
                {
                    const auto wet = wetStart + (wetEnd - wetStart) * (float) (i + 1) / (float) length;
                    samples[i] += (biquad.process (samples[i]) - samples[i]) * wet;
                }
            }
        }
    }
}
