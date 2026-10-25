#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"

// The plugin window. All the content lives in MainView, laid out on a fixed
// canvas the size of the concept (docs/concept.png); the editor scales it to
// whatever size the window is, keeping its shape.
class ParagonPR7MAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ParagonPR7MAudioProcessorEditor (ParagonPR7MAudioProcessor&);
    ~ParagonPR7MAudioProcessorEditor() override;

    void resized() override;

    // The size to open at, given the screen's area above the taskbar
    // (logical pixels): as big as fits with room for the DAW's own window
    // frame and toolbar, up to the concept's full size.
    static juce::Point<int> openingSize (juce::Rectangle<int> screen);

private:
    juce::SharedResourcePointer<SharedParagonLookAndFeel> look;   // declared first: outlives the view
    MainView view;
    juce::TooltipWindow tooltips { this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParagonPR7MAudioProcessorEditor)
};
