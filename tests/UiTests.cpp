#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginEditor.h"
#include "PluginProcessor.h"

using Catch::Matchers::WithinAbs;

namespace
{
    MainView& viewOf (juce::AudioProcessorEditor& editor)
    {
        auto* view = dynamic_cast<MainView*> (editor.getChildComponent (0));
        REQUIRE (view != nullptr);
        return *view;
    }
}

TEST_CASE ("The window keeps the canvas's shape at every size", "[ui]")
{
    ParagonPR7MAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    for (const auto width : { MainView::width / 2, MainView::width, MainView::width * 3 / 2 })
    {
        editor->setSize (width, width * MainView::height / MainView::width);
        CHECK_THAT ((double) editor->getHeight() / editor->getWidth(), WithinAbs ((double) MainView::height / MainView::width, 0.01));
    }
}

TEST_CASE ("Every control has a tooltip", "[ui]")
{
    ParagonPR7MAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    for (const auto& [id, control] : viewOf (*editor).getControls())
    {
        INFO (id.toStdString());
        juce::String tooltip;
        if (auto* knob = dynamic_cast<Knob*> (control))
            tooltip = knob->slider.getTooltip();
        else if (auto* client = dynamic_cast<juce::TooltipClient*> (control))
            tooltip = client->getTooltip();
        CHECK (tooltip.isNotEmpty());
    }
}

TEST_CASE ("The panel has every control of the agreed layout, on the faceplate", "[ui]")
{
    ParagonPR7MAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto& view = viewOf (*editor);
    for (const auto* id : { "tone.air", "tone.focus", "tone.body", "dyn.push", "dyn.volume", "panel.reset", "panel.bypass",
                            "stereo.width", "dyn.transients", "colour.analog", "dyn.bassPunch" })
    {
        INFO (id);
        auto* control = view.findControl (id);
        REQUIRE (control != nullptr);
        CHECK (MainView::faceplate().contains (control->getBounds().toFloat()));
    }
    CHECK (MainView::faceplate().contains (view.getVuMeter().getBounds().toFloat()));

    // The header: the presets and the settings, above the faceplate.
    for (const auto* id : { "preset.previous", "preset.next", "preset.save", "preset.menu", "settings" })
    {
        INFO (id);
        auto* control = view.findControl (id);
        REQUIRE (control != nullptr);
        CHECK (control->getBottom() <= (int) MainView::faceplate().getY());
    }
}
