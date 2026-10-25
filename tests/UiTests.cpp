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

TEST_CASE ("The INPUT and OUTPUT knobs work their parameters", "[ui]")
{
    ParagonPR7MAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto& view = viewOf (*editor);
    for (const auto& [controlId, parameterId] : { std::pair ("output.input", ParamIDs::inputGain), { "output.output", ParamIDs::outputGain } })
    {
        INFO (controlId);
        auto* knob = dynamic_cast<Knob*> (view.findControl (controlId));
        REQUIRE (knob != nullptr);
        knob->slider.setValue (-9.0, juce::sendNotificationSync);
        CHECK_THAT (processor.getParameters().getRawParameterValue (parameterId)->load(), WithinAbs (-9.0, 1e-4));
    }
}
