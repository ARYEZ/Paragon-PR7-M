#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Assistant.h"
#include "LevelMeterSource.h"
#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/Chain.h"
#include "dsp/EffectUtils.h"
#include "dsp/Loudness.h"
#include "dsp/MatchedBypass.h"
#include "dsp/TruePeak.h"
#include "dsp/VuBallistics.h"

// The mastering processor. True stereo (stereo in, stereo out), or mono in,
// mono out. The modules, in their order (see CLAUDE.md): BODY / FOCUS /
// AIR (the tone), BASS PUNCH, TRANSIENTS, PUSH (the glue compressor),
// ANALOG, WIDTH, and VOLUME (gain into a soft clipper and a true-peak
// limiter). ANALOG and VOLUME delay the audio by a few milliseconds, the
// same at any setting, reported to the host. BYPASS is at matched
// loudness, and the output is metered: the VU needle, short-term loudness
// and true peak.
class ParagonPR7MAudioProcessor final : public juce::AudioProcessor
{
public:
    ParagonPR7MAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }

    // Peak levels for the meters, per channel (0 left, 1 right; on a mono
    // track both show the one channel).
    LevelMeterSource& getInputMeter (int channel) noexcept { return inputMeters[(size_t) juce::jlimit (0, 1, channel)]; }
    LevelMeterSource& getOutputMeter (int channel) noexcept { return outputMeters[(size_t) juce::jlimit (0, 1, channel)]; }

    // The output's metering, for the UI. The VU needle's position (linear,
    // the louder side; a sine reads its peak), the short-term loudness
    // (LUFS, ShortTermLoudness::silent with nothing playing), and the
    // highest true peak since it was last cleared (linear). It clears itself
    // when the host starts playing.
    float getVuLevel() const noexcept { return vuLevel.load (std::memory_order_relaxed); }
    float getShortTermLufs() const noexcept { return shortTermLufs.load (std::memory_order_relaxed); }
    float getTruePeak() const noexcept { return truePeakHold.get(); }
    void resetTruePeak() noexcept { truePeakHold.reset(); }

    // How much PUSH's compressor and VOLUME's limiter are turning down (dB),
    // for their LED rings.
    GainReductionSource& getPushReduction() noexcept { return pushReduction; }
    GainReductionSource& getVolumeReduction() noexcept { return volumeReduction; }
    // Counts the blocks metered, so the UI can tell when the host has
    // stopped sending audio (and let the needle fall).
    juce::uint32 getMeteredBlocks() const noexcept { return meteredBlocks.load (std::memory_order_relaxed); }

    // Where 0 VU sits, in dBFS (a sine peaking there reads 0 VU): -18, -14
    // or -10, -14 by default. Saved with the project. Message thread only.
    static constexpr int vuReferenceChoices[] = { -18, -14, -10 };
    static constexpr int defaultVuReference = -14;
    int getVuReference() const;
    void setVuReference (int dBFS);

    // Factory and user presets (the header). Message thread only.
    PresetManager& getPresets() noexcept { return presets; }

    // The assistant (LISTEN). Message thread, but for what processBlock
    // hands it.
    Assistant& getAssistant() noexcept { return assistant; }
    // LISTEN, for a target (an index into Assistant::targets), at the
    // current sample rate.
    void startListening (int target) { assistant.start (target, getSampleRate()); }
    // Once the assistant has decided: sets the knobs, named for it
    // ("Assistant - Streaming"). Returns what it decided, if it had.
    std::optional<Assistant::Result> applyAssistant();

    // RESET: every knob back to its starting point (the Default preset), as
    // host gestures (so a host that records them can undo it). BYPASS isn't
    // a knob and stays as it is. Message thread only.
    void resetKnobs();

private:
    void processChunk (juce::dsp::AudioBlock<float>) noexcept;
    effects::MasteringChain::Settings knobs() const noexcept;
    void pushMeters (std::array<LevelMeterSource, 2>&, const juce::AudioBuffer<float>&, int numSamples) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    PresetManager presets { parameters };
    Assistant assistant;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* volume = nullptr;
    std::atomic<float>* push = nullptr;
    std::atomic<float>* air = nullptr;
    std::atomic<float>* focus = nullptr;
    std::atomic<float>* body = nullptr;
    std::atomic<float>* bassPunch = nullptr;
    std::atomic<float>* transients = nullptr;
    std::atomic<float>* analog = nullptr;
    std::atomic<float>* width = nullptr;
    std::array<LevelMeterSource, 2> inputMeters, outputMeters;

    effects::MasteringChain chain;
    GainReductionSource pushReduction, volumeReduction;

    // The mix as it came in, for BYPASS, delayed as much as the modules
    // delay the processed mix, so the two stay aligned.
    juce::AudioBuffer<float> dry;
    effects::FixedDelay dryDelay;
    effects::MatchedBypass matchedBypass;

    metering::ShortTermLoudness loudness;
    metering::TruePeak truePeak;
    std::array<metering::VuBallistics, 2> vu;
    std::atomic<float> vuLevel { 0.0f }, shortTermLufs { metering::ShortTermLoudness::silent };
    HeldPeakSource truePeakHold;
    std::atomic<juce::uint32> meteredBlocks { 0 };
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParagonPR7MAudioProcessor)
};
