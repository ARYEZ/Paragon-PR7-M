#pragma once

#include <atomic>
#include <cmath>

// Hands the peak level of the audio from the audio thread to the UI thread.
// The audio thread calls pushSamples() every block; the UI timer calls
// takePeak(), which returns the highest peak since the last call. Lock-free,
// so it is safe to use on the audio thread.
class LevelMeterSource
{
public:
    void pushSamples (const float* samples, int numSamples) noexcept
    {
        float blockPeak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            blockPeak = std::fmax (blockPeak, std::abs (samples[i]));

        auto current = peak.load (std::memory_order_relaxed);
        while (blockPeak > current
               && ! peak.compare_exchange_weak (current, blockPeak, std::memory_order_relaxed))
        {
        }
    }

    float takePeak() noexcept { return peak.exchange (0.0f, std::memory_order_relaxed); }

private:
    std::atomic<float> peak { 0.0f };
};

// Hands a dynamics module's gain reduction (in dB, positive) from the audio
// thread to its GR meter, the same way: the audio thread calls push() every
// block, and the UI's takeReduction() returns the most since the last call.
class GainReductionSource
{
public:
    void push (float reductionDb) noexcept
    {
        auto current = most.load (std::memory_order_relaxed);
        while (reductionDb > current
               && ! most.compare_exchange_weak (current, reductionDb, std::memory_order_relaxed))
        {
        }
    }

    float takeReduction() noexcept { return most.exchange (0.0f, std::memory_order_relaxed); }

private:
    std::atomic<float> most { 0.0f };
};

// Hands a held maximum (the true-peak readout's) from the audio thread to
// the UI: the audio thread push()es every block, the UI reads it with get()
// without clearing it, and reset() starts it again (a click on the readout).
class HeldPeakSource
{
public:
    void push (float value) noexcept
    {
        auto current = most.load (std::memory_order_relaxed);
        while (value > current
               && ! most.compare_exchange_weak (current, value, std::memory_order_relaxed))
        {
        }
    }

    float get() const noexcept { return most.load (std::memory_order_relaxed); }
    void reset() noexcept { most.store (0.0f, std::memory_order_relaxed); }

private:
    std::atomic<float> most { 0.0f };
};
