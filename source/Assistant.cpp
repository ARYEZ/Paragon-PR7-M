#include "Assistant.h"

#include "dsp/Loudness.h"
#include "dsp/TruePeak.h"

namespace
{
    // A typical modern master's average spectrum, as the power in each
    // octave band (dB, 0 at 1 kHz). Our approximation of the long-term
    // average spectra of commercial pop and rock masters: full, even lows,
    // falling about 4 to 5 dB an octave above 1 kHz and faster at the very
    // top. A starting point to tune by ear, not a measurement of any record.
    constexpr std::pair<float, float> octaves[] = { { 31.5f, 4.0f }, { 63.0f, 7.0f }, { 125.0f, 6.0f }, { 250.0f, 3.0f }, { 500.0f, 1.0f },
                                                    { 1000.0f, 0.0f }, { 2000.0f, -3.0f }, { 4000.0f, -7.0f }, { 8000.0f, -12.0f },
                                                    { 16000.0f, -21.0f } };

    // The bands the knobs work on, and the mids they're measured from.
    struct Band
    {
        float from, to;
    };
    constexpr Band lowsBand { 30.0f, 120.0f }, bodyBand { 120.0f, 400.0f }, midsBand { 400.0f, 1500.0f },
                   presenceBand { 1500.0f, 5000.0f }, airBand { 8000.0f, 16000.0f };

    constexpr int fftOrder = 12;
    constexpr int fftSize = 1 << fftOrder;

    float roundTo (float value, float step) { return std::round (value / step) * step; }

    // The average power spectrum (per bin) of the mid (left plus right) and
    // the side (left minus right), Hann-windowed, half-overlapped.
    struct Spectra
    {
        std::vector<double> mid, side;
    };

    Spectra spectraOf (const juce::AudioBuffer<float>& audio)
    {
        juce::dsp::FFT fft (fftOrder);
        juce::dsp::WindowingFunction<float> window (fftSize, juce::dsp::WindowingFunction<float>::hann, false);
        Spectra spectra { std::vector<double> (fftSize / 2 + 1), std::vector<double> (fftSize / 2 + 1) };
        std::vector<float> midFrame (fftSize * 2), sideFrame (fftSize * 2);
        const auto stereo = audio.getNumChannels() > 1;
        const auto* left = audio.getReadPointer (0);
        const auto* right = audio.getReadPointer (stereo ? 1 : 0);
        for (int start = 0; start + fftSize <= audio.getNumSamples(); start += fftSize / 2)
        {
            for (int i = 0; i < fftSize; ++i)
            {
                midFrame[(size_t) i] = 0.5f * (left[start + i] + right[start + i]);
                sideFrame[(size_t) i] = 0.5f * (left[start + i] - right[start + i]);
            }
            window.multiplyWithWindowingTable (midFrame.data(), fftSize);
            window.multiplyWithWindowingTable (sideFrame.data(), fftSize);
            fft.performFrequencyOnlyForwardTransform (midFrame.data(), true);
            fft.performFrequencyOnlyForwardTransform (sideFrame.data(), true);
            for (int bin = 0; bin <= fftSize / 2; ++bin)
            {
                spectra.mid[(size_t) bin] += (double) midFrame[(size_t) bin] * midFrame[(size_t) bin];
                spectra.side[(size_t) bin] += (double) sideFrame[(size_t) bin] * sideFrame[(size_t) bin];
            }
        }
        return spectra;
    }

    template <typename PowerOfBin>
    double bandPower (Band band, double sampleRate, PowerOfBin&& power)
    {
        auto sum = 1.0e-30;
        const auto hzPerBin = sampleRate / fftSize;
        for (int bin = juce::jmax (1, (int) std::ceil (band.from / hzPerBin)); bin <= juce::jmin (fftSize / 2, (int) std::floor (band.to / hzPerBin)); ++bin)
            sum += power (bin, (float) (bin * hzPerBin));
        return sum;
    }
}

float Assistant::referenceDb (float hz) noexcept
{
    // The octave bands' power, read between them along the octaves, then
    // spread over each band's width (an octave is twice as wide each time up).
    const auto octave = std::log2 (juce::jmax (1.0f, hz));
    auto bandDb = octaves[0].second;
    if (hz >= octaves[std::size (octaves) - 1].first)
        bandDb = octaves[std::size (octaves) - 1].second;
    else
        for (size_t i = 0; i + 1 < std::size (octaves); ++i)
            if (hz >= octaves[i].first && hz < octaves[i + 1].first)
            {
                const auto a = std::log2 (octaves[i].first), b = std::log2 (octaves[i + 1].first);
                bandDb = juce::jmap (octave, a, b, octaves[i].second, octaves[i + 1].second);
            }
    // Past 16 kHz it keeps falling as it did from 8.
    if (hz > octaves[std::size (octaves) - 1].first)
        bandDb += (octaves[std::size (octaves) - 1].second - octaves[std::size (octaves) - 2].second) * (octave - std::log2 (16000.0f));
    return bandDb - 10.0f * std::log10 (hz / 1000.0f);
}

float Assistant::loudnessOf (const juce::AudioBuffer<float>& audio, double sampleRate, int fromSample)
{
    std::array<std::array<effects::Biquad, 2>, 2> filters;
    for (auto& channel : filters)
    {
        channel[0].c = metering::KWeighting::shelf (sampleRate);
        channel[1].c = metering::KWeighting::highPass (sampleRate);
    }
    const auto numChannels = juce::jmin (2, audio.getNumChannels());
    auto sum = 0.0;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto* samples = audio.getReadPointer (channel);
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const auto weighted = (double) filters[(size_t) channel][1].process (filters[(size_t) channel][0].process (samples[i]));
            if (i >= fromSample)
                sum += weighted * weighted;
        }
    }
    const auto power = sum / juce::jmax (1, audio.getNumSamples() - fromSample);
    return power > 1.0e-10 ? (float) (-0.691 + 10.0 * std::log10 (power)) : -100.0f;
}

Assistant::Analysis Assistant::analyse (const juce::AudioBuffer<float>& audio, double sampleRate)
{
    Analysis analysis;
    const auto spectra = spectraOf (audio);
    const auto measured = [&] (Band band) { return bandPower (band, sampleRate, [&] (int bin, float) { return spectra.mid[(size_t) bin]; }); };
    const auto reference = [&] (Band band)
    {
        return bandPower (band, sampleRate, [] (int, float hz) { return std::pow (10.0, referenceDb (hz) / 10.0); });
    };
    // How each band stands against the mids, compared with how it stands in the reference.
    const auto against = [&] (Band band)
    {
        return (float) (10.0 * std::log10 (measured (band) / measured (midsBand)) - 10.0 * std::log10 (reference (band) / reference (midsBand)));
    };
    analysis.lows = against (lowsBand);
    analysis.body = against (bodyBand);
    analysis.presence = against (presenceBand);
    analysis.air = against (airBand);

    const Band image { 120.0f, 16000.0f };
    const auto side = bandPower (image, sampleRate, [&] (int bin, float) { return spectra.side[(size_t) bin]; });
    const auto mid = bandPower (image, sampleRate, [&] (int bin, float) { return spectra.mid[(size_t) bin]; });
    analysis.sideDb = (float) (10.0 * std::log10 (side / mid));

    analysis.loudnessLufs = loudnessOf (audio, sampleRate);
    metering::TruePeak truePeak;
    truePeak.prepare (sampleRate);
    auto peak = 0.0f;
    for (int channel = 0; channel < juce::jmin (2, audio.getNumChannels()); ++channel)
        peak = juce::jmax (peak, truePeak.process (channel, audio.getReadPointer (channel), audio.getNumSamples()));
    analysis.peakToLoudness = juce::Decibels::gainToDecibels (peak, -100.0f) - analysis.loudnessLufs;
    return analysis;
}

effects::MasteringChain::Settings Assistant::suggest (const Analysis& a, float targetLufs)
{
    effects::MasteringChain::Settings s;
    const auto louder = juce::jmax (0.0f, targetLufs + 14.0f);   // dB past streaming loudness

    // The tone: part of the way back towards the reference, gently. AIR and
    // FOCUS are 0.4 dB a step; BODY is in dB.
    s.air = roundTo (juce::jlimit (-10.0f, 10.0f, -0.6f * a.air / 0.4f), 0.5f);
    s.focus = roundTo (juce::jlimit (-7.5f, 7.5f, -0.6f * a.presence / 0.4f), 0.5f);
    s.body = roundTo (juce::jlimit (-3.0f, 3.0f, -0.5f * a.body - 0.25f * juce::jmax (0.0f, a.lows)), 0.5f);
    // BASS PUNCH: a little for tightness anyway, more when the lows are thin.
    s.bassPunch = roundTo (juce::jlimit (0.0f, 6.0f, 2.0f - 0.8f * a.lows), 0.5f);

    // The dynamics. A typical unmastered mix peaks 12 to 18 dB over its
    // loudness; a loud master 8 to 10.
    s.push = roundTo (juce::jlimit (1.5f, 7.5f, 0.6f * (a.peakToLoudness - 9.0f) + 0.3f * louder), 0.5f);
    s.transients = roundTo (juce::jlimit (0.0f, 4.0f, 0.6f * (13.0f - a.peakToLoudness) + (louder >= 4.0f ? 1.0f : 0.0f)), 0.5f);

    // The image: a typical master's side is about 12 dB under its mid.
    // Mono (or nearly) has nothing to widen.
    s.width = a.sideDb < -35.0f ? 0.0f : roundTo (juce::jlimit (-1.0f, 2.0f, 0.2f * (-12.0f - a.sideDb)), 0.5f);

    s.analog = 1.5f + 0.15f * louder;
    s.analog = roundTo (s.analog, 0.5f);
    return s;
}

float Assistant::mostVolumeFor (float peakDb, float mostOvershootDb) noexcept
{
    // The overshoot only grows as VOLUME turns up (the ceiling comes down).
    auto volume = 0.0f;
    while (volume < 15.0f && peakDb + (volume + 0.1f) - effects::VolumeClipper::ceilingDbFor (volume + 0.1f) <= mostOvershootDb)
        volume += 0.1f;
    return roundTo (volume, 0.1f);
}

float Assistant::findVolume (const juce::AudioBuffer<float>& audio, double sampleRate, effects::MasteringChain::Settings settings,
                             float targetLufs, float mostOvershootDb, float* reachedLufs)
{
    constexpr int blockSize = 512;
    const auto runBlocks = [&] (juce::AudioBuffer<float>& buffer, auto&& process)
    {
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        {
            auto block = juce::dsp::AudioBlock<float> (buffer).getSubBlock ((size_t) start, (size_t) juce::jmin (blockSize, buffer.getNumSamples() - start));
            process (block);
        }
    };

    // Everything before VOLUME, once: VOLUME doesn't change it.
    auto before = audio;
    {
        effects::MasteringChain chain;
        settings.volume = 0.0f;
        chain.prepare (sampleRate, blockSize, settings);
        runBlocks (before, [&] (auto& block) { chain.processBeforeVolume (block); });
    }

    // Then VOLUME, until the loudness lands (it only rises as VOLUME does).
    const auto settle = (int) (sampleRate * 0.5);   // leave out the first moments, while the modules settle
    const auto loudnessAt = [&] (float volume)
    {
        auto out = before;
        effects::MasteringChain chain;
        settings.volume = volume;
        chain.prepare (sampleRate, blockSize, settings);
        runBlocks (out, [&] (auto& block) { chain.processVolume (block); });
        return loudnessOf (out, sampleRate, juce::jmin (settle, out.getNumSamples() / 2));
    };

    auto peak = 0.0f;
    for (int channel = 0; channel < before.getNumChannels(); ++channel)
        peak = juce::jmax (peak, before.getMagnitude (channel, 0, before.getNumSamples()));
    auto low = 0.0f, high = mostVolumeFor (juce::Decibels::gainToDecibels (peak, -100.0f), mostOvershootDb);
    auto reached = loudnessAt (high);
    if (reached <= targetLufs)
    {
        if (reachedLufs != nullptr)
            *reachedLufs = reached;
        return high;
    }
    reached = loudnessAt (low);
    if (reached >= targetLufs)
    {
        if (reachedLufs != nullptr)
            *reachedLufs = reached;
        return low;
    }
    while (high - low > 0.15f)
    {
        const auto middle = 0.5f * (low + high);
        (loudnessAt (middle) < targetLufs ? low : high) = middle;
    }
    const auto volume = roundTo (0.5f * (low + high), 0.1f);
    if (reachedLufs != nullptr)
        *reachedLufs = loudnessAt (volume);
    return volume;
}

Assistant::Result Assistant::decide (const juce::AudioBuffer<float>& audio, double sampleRate, int targetIndex)
{
    const auto& aim = targets[juce::jlimit (0, numTargets - 1, targetIndex)];
    Result decided;
    decided.target = targetIndex;
    decided.settings = suggest (analyse (audio, sampleRate), aim.lufs);
    decided.settings.volume = findVolume (audio, sampleRate, decided.settings, aim.lufs, aim.mostOvershootDb, &decided.reachedLufs);
    return decided;
}

Assistant::~Assistant()
{
    joinWorker();
}

void Assistant::joinWorker()
{
    if (worker.joinable())
        worker.join();
}

void Assistant::start (int newTarget, double newSampleRate)
{
    cancel();
    target = juce::jlimit (0, numTargets - 1, newTarget);
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    recording.setSize (2, (int) (listenSeconds * sampleRate));   // the audio thread doesn't touch it while idle
    written.store (0);
    state.store ((int) State::listening);
}

void Assistant::cancel()
{
    // Thinking can't be stopped part way; it's a few seconds at most.
    joinWorker();
    state.store ((int) State::idle);
    result.reset();
}

void Assistant::capture (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (state.load (std::memory_order_acquire) != (int) State::listening || numChannels < 1)
        return;

    // Silence doesn't count: press LISTEN, then start the song.
    auto peak = 0.0f;
    for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax (peak, std::abs (channels[channel][i]));
    if (peak < juce::Decibels::decibelsToGain (silenceDb))
        return;

    const auto at = written.load (std::memory_order_relaxed);
    const auto count = juce::jmin (numSamples, recording.getNumSamples() - at);
    for (int channel = 0; channel < 2; ++channel)
        recording.copyFrom (channel, at, channels[juce::jmin (channel, numChannels - 1)], count);   // mono: both the same
    written.store (at + count, std::memory_order_release);
    if (at + count >= recording.getNumSamples())
        state.store ((int) State::thinking, std::memory_order_release);
}

void Assistant::update()
{
    if (getState() != State::thinking || worker.joinable())
        return;
    worker = std::thread ([this]
    {
        result = decide (recording, sampleRate, target);
        state.store ((int) State::done, std::memory_order_release);
    });
}

std::optional<Assistant::Result> Assistant::takeResult()
{
    if (getState() != State::done)
        return std::nullopt;
    joinWorker();
    auto taken = result;
    result.reset();
    state.store ((int) State::idle);
    return taken;
}
