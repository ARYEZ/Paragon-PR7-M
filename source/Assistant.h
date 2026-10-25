#pragma once

#include <atomic>
#include <optional>
#include <thread>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Chain.h"

// The assistant: press LISTEN, pick a loudness, play the loudest part of
// the song, and it sets every knob. Our own design (not a copy of anyone's).
//
// 1. Listening (the audio thread): it records what comes in, before any
//    processing, skipping silence, until it has 15 seconds.
// 2. Thinking (a background thread): it measures the recording
//    (analyse()) and picks the knobs from rules (suggest()):
//    - The tone balance against a typical modern master's average spectrum
//      (referenceDb(): our approximation, to be tuned by ear): too dull
//      turns AIR up, too boxy turns BODY down, thin lows turn BASS PUNCH
//      up, and so on, correcting only part of the way, and gently.
//    - How dynamic the song is (its peak-to-loudness ratio): a spiky mix
//      gets more PUSH, an already squashed one less, and a little
//      TRANSIENTS to keep the attacks through the limiting.
//    - The stereo image: a narrow mix gets a little WIDTH, a very wide one
//      a little less (and the bass stays in the middle either way).
//    - ANALOG at a gentle amount.
//    Then it runs the real chain on the recording, again and again, to find
//    the VOLUME that lands the loudest part on the target (findVolume()),
//    so what it promises is what plays. It won't turn VOLUME past where the
//    song's peaks go too far into the clipper (mostVolumeFor()): Streaming
//    and Loud stay clean, Maximum may add edge. A quiet mix can go further
//    up the knob than a loud one. (More PUSH doesn't help a spiky mix get
//    there: its slow attack lets the hits' peaks through. Tried, measured,
//    taken out.)
// 3. Done: the processor's owner (the editor) takes the result and sets
//    the knobs, named as a preset ("Assistant - Streaming").
class Assistant
{
public:
    struct Target
    {
        const char* name;
        float lufs;
        // How far the song's highest peak may go over VOLUME's clipper's
        // ceiling (dB): 3 keeps it clean (a mix peaking at -1 dBFS reaches
        // that at VOLUME 9), 8.4 is VOLUME 12's edge on such a mix.
        float mostOvershootDb;
    };
    static constexpr Target targets[] = { { "Streaming", -14.0f, 3.0f }, { "Loud", -10.0f, 3.0f }, { "Maximum", -8.0f, 8.4f } };
    static constexpr int numTargets = 3;
    static constexpr double listenSeconds = 15.0;
    static constexpr float silenceDb = -50.0f;   // quieter blocks aren't recorded

    enum class State { idle, listening, thinking, done };

    struct Analysis
    {
        // Each band against the reference, in dB (positive: more than a typical
        // master has), measured from the mids (400 Hz to 1.5 kHz).
        float lows = 0.0f, body = 0.0f, presence = 0.0f, air = 0.0f;
        float sideDb = -100.0f;        // the side's level against the mid's, above 120 Hz
        float loudnessLufs = -100.0f;  // the whole recording's loudness
        float peakToLoudness = 0.0f;   // its highest peak over its loudness, in dB
    };

    struct Result
    {
        int target = 0;
        effects::MasteringChain::Settings settings;
        float reachedLufs = -100.0f;   // what the chain gives the recording at those settings
    };

    Assistant() = default;
    ~Assistant();

    // Message thread. Starts listening for `target` (an index into targets).
    void start (int target, double sampleRate);
    void cancel();
    State getState() const noexcept { return (State) state.load(); }
    int getTarget() const noexcept { return target; }
    double getSampleRate() const noexcept { return sampleRate; }
    double getSecondsHeard() const noexcept { return (double) written.load() / sampleRate; }

    // Message thread, regularly: once it has heard enough, thinks on a
    // background thread; then hands the result over (once).
    void update();
    std::optional<Result> takeResult();

    // Audio thread: the incoming audio (1 or 2 channels). No allocation, no locks.
    void capture (const float* const* channels, int numChannels, int numSamples) noexcept;

    // The thinking, on its own, for the tests.
    static Analysis analyse (const juce::AudioBuffer<float>&, double sampleRate);
    static effects::MasteringChain::Settings suggest (const Analysis&, float targetLufs);
    static float findVolume (const juce::AudioBuffer<float>&, double sampleRate, effects::MasteringChain::Settings, float targetLufs,
                             float mostOvershootDb, float* reachedLufs = nullptr);
    // The most VOLUME can be for a song peaking at `peakDb` (before VOLUME)
    // with its peak no more than `mostOvershootDb` over the clipper's ceiling.
    static float mostVolumeFor (float peakDb, float mostOvershootDb) noexcept;
    static Result decide (const juce::AudioBuffer<float>&, double sampleRate, int target);

    // A typical modern master's average spectrum: its power per Hz at
    // `hz`, in dB (relative; 0 at 1 kHz).
    static float referenceDb (float hz) noexcept;
    // The K-weighted loudness of a whole recording (LUFS, no gating).
    static float loudnessOf (const juce::AudioBuffer<float>&, double sampleRate, int fromSample = 0);

private:
    void joinWorker();

    std::atomic<int> state { (int) State::idle };
    std::atomic<int> written { 0 };
    juce::AudioBuffer<float> recording;
    double sampleRate = 48000.0;
    int target = 0;
    std::thread worker;
    std::optional<Result> result;   // written by the worker before state becomes done
};
