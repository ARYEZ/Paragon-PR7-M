#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/LoudnessDisplay.h"
#include "ui/VuMeter.h"

// Everything in the plugin window, on a fixed canvas that the editor scales
// to the window.
//
// The layout follows the owner's reference (an all-in-one mastering
// processor's front panel), redrawn in the Paragon look: AIR, FOCUS and BODY
// down the left; the big PUSH and VOLUME knobs either side of the VU meter,
// with RESET and BYPASS levers above them; WIDTH under the meter; TRANSIENTS,
// ANALOG and BASS PUNCH down the right. PUSH and VOLUME have a ring of green
// LEDs that light with their gain reduction.
//
// Controls are registered by ID ("tone.air", "dyn.push"...). Until their
// step is built they're inert, and their tooltips say which step brings them.
//
// The VU meter shows the output with its 0 VU at the level chosen in the
// settings menu, and the LED readout under it ("meter.loudness") the
// short-term loudness and true peak. BYPASS (up = bypassed) is the bypass
// parameter; RESET is momentary: flick it up and it springs back, setting
// every knob to its starting point.
class MainView final : public juce::Component,
                       private juce::ChangeListener,
                       private juce::Timer
{
public:
    explicit MainView (ParagonPR7MAudioProcessor&);
    ~MainView() override;

    static constexpr int width = 1448;
    static constexpr int height = 640;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;

    juce::Component* findControl (const juce::String& id) const;
    const std::map<juce::String, juce::Component*>& getControls() const noexcept { return controls; }
    VuMeter& getVuMeter() noexcept { return vu; }
    LoudnessDisplay& getLoudnessDisplay() noexcept { return loudness; }

    // The faceplate, on the canvas.
    static juce::Rectangle<float> faceplate() noexcept { return { 28.0f, 86.0f, 1392.0f, 526.0f }; }

    // Reads the processor's metering into the VU needle and the readout
    // (60 times a second, from the timer).
    void updateMeters();

    // How many dB PUSH and VOLUME are turning down, for their LED rings.
    void setGainReduction (float pushDb, float volumeDb);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;   // the finish changed
    void timerCallback() override { updateMeters(); }
    void showSettingsMenu();
    Knob& addKnob (const juce::String& id, juce::Point<float> centre, float ringRadius, std::vector<juce::String> scale,
                   const juce::String& caption, const juce::String& tooltip);
    LeverSwitch& addLever (const juce::String& id, juce::Point<float> centre, const juce::String& caption, const juce::String& tooltip,
                           bool glowWhenUp = false);
    void paintReductionRing (juce::Graphics&, const Knob&, float reductionDb) const;

    ParagonPR7MAudioProcessor& processor;
    juce::OwnedArray<juce::Component> ownedControls;
    std::map<juce::String, juce::Component*> controls;
    std::vector<std::pair<juce::String, juce::Point<float>>> leverCaptions;
    VuMeter vu;
    LoudnessDisplay loudness;
    std::unique_ptr<juce::ParameterAttachment> bypassAttachment;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    float shownVolumeReduction = 0.0f;
    float shownVuLevel = 0.0f;
    juce::uint32 lastMeteredBlocks = 0;
    int ticksWithoutAudio = 0;
    float pushReduction = 0.0f, volumeReduction = 0.0f;
    juce::Image grain;   // the satin grain over the faceplate, made for the current finish

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
