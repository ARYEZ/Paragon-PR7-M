#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/LevelMeter.h"

// Everything in the plugin window, on a fixed canvas that the editor scales
// to the window.
//
// For now a placeholder in the family look: the header (the PR7-M mark and
// the finish menu), the body frame, a plate where the mastering modules will
// go, and a working IN / OUT section (INPUT, OUTPUT and the meters). When the
// concept design arrives (docs/concept.png), the canvas takes its size, so
// positions can be read straight off it, and the panel is built to match.
class MainView final : public juce::Component,
                       private juce::ChangeListener
{
public:
    explicit MainView (ParagonPR7MAudioProcessor&);
    ~MainView() override;

    static constexpr int width = 1448;
    static constexpr int height = 1086;

    void paint (juce::Graphics&) override;

    // Every control, by ID: "<module>.<control>" ("output.input").
    juce::Component* findControl (const juce::String& id) const;
    const std::map<juce::String, juce::Component*>& getControls() const noexcept { return controls; }

    // The four level meters (IN L, IN R, OUT L, OUT R).
    std::array<LevelMeter*, 4> getLevelMeters() noexcept { return { &inputLeft, &inputRight, &outputLeft, &outputRight }; }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;   // the finish changed
    void showSettingsMenu();
    Knob& addKnob (const juce::String& id, juce::Point<float> centre, float ringRadius, const juce::String& caption,
                   const juce::String& parameterId, const juce::String& tooltip);

    ParagonPR7MAudioProcessor& processor;
    juce::OwnedArray<juce::Component> ownedControls;
    std::map<juce::String, juce::Component*> controls;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;

    LevelMeter inputLeft, inputRight, outputLeft, outputRight;
    juce::Image grain;   // the satin grain over the plates, made for the current finish

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
