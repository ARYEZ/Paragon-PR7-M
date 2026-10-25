#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/Controls.h"
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
class MainView final : public juce::Component,
                       private juce::ChangeListener
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

    // The faceplate, on the canvas.
    static juce::Rectangle<float> faceplate() noexcept { return { 28.0f, 86.0f, 1392.0f, 526.0f }; }

    // How many dB PUSH and VOLUME are turning down, for their LED rings.
    void setGainReduction (float pushDb, float volumeDb);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;   // the finish changed
    void showSettingsMenu();
    Knob& addKnob (const juce::String& id, juce::Point<float> centre, float ringRadius, std::vector<juce::String> scale,
                   const juce::String& caption, const juce::String& tooltip);
    LeverSwitch& addLever (const juce::String& id, juce::Point<float> centre, const juce::String& caption, const juce::String& tooltip);
    void paintReductionRing (juce::Graphics&, const Knob&, float reductionDb) const;

    ParagonPR7MAudioProcessor& processor;
    juce::OwnedArray<juce::Component> ownedControls;
    std::map<juce::String, juce::Component*> controls;
    std::vector<std::pair<juce::String, juce::Point<float>>> leverCaptions;
    VuMeter vu;
    float pushReduction = 0.0f, volumeReduction = 0.0f;
    juce::Image grain;   // the satin grain over the faceplate, made for the current finish

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
