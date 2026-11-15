#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/Finish.h"

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
    for (const auto* id : { "preset.previous", "preset.next", "assistant.listen", "preset.save", "preset.menu", "settings" })
    {
        INFO (id);
        auto* control = view.findControl (id);
        REQUIRE (control != nullptr);
        CHECK (control->getBottom() <= (int) MainView::faceplate().getY());
    }
}

TEST_CASE ("OBSIDIAN and AURORA are the finishes, and the knobs are black in neon rings", "[ui]")
{
    // Saved by number: 0 the retired Pearl White, 1 Pearl Black (now
    // Obsidian), 2 Pearl White + Neon (now Aurora).
    CHECK ((int) Finish::Type::retiredPearlWhite == 0);
    CHECK ((int) Finish::Type::obsidian == 1);
    CHECK ((int) Finish::Type::aurora == 2);
    REQUIRE (std::size (Finish::choices) == 2);
    CHECK (Finish::nameOf (Finish::choices[0]) == "OBSIDIAN");
    CHECK (Finish::nameOf (Finish::choices[1]) == "AURORA");

    Finish::set (Finish::Type::retiredPearlWhite, false);
    CHECK (Finish::get() == Finish::Type::obsidian);   // an old Pearl White opens as Obsidian
    CHECK (Finish::isBlack());

    for (const auto type : Finish::choices)
    {
        Finish::set (type, false);
        CHECK (Finish::get() == type);
        CHECK (Finish::isBlack() == (type == Finish::Type::obsidian));

        // The knob's cap is black on both finishes; its ring is the neon
        // (pink on its left, orange on its right).
        juce::Image image (juce::Image::ARGB, 200, 200, true, juce::SoftwareImageType());
        {
            juce::Graphics g (image);
            g.fillAll (juce::Colours::grey);
            Knob::drawKnob (g, { 100.0f, 100.0f }, 70.0f, 0.0f, Knob::startAngle, Knob::endAngle);
        }
        const auto cap = image.getPixelAt (100, 130);
        CHECK (juce::jmax (cap.getRed(), cap.getGreen(), cap.getBlue()) < 80);
        const auto ring = juce::roundToInt (70.0f * 0.86f + 2.0f);   // just outside the ring's white-hot core
        const auto left = image.getPixelAt (100 - ring, 100), right = image.getPixelAt (100 + ring, 100);
        INFO ("left " << left.toDisplayString (false) << ", right " << right.toDisplayString (false));
        CHECK (left.getRed() > 150);
        CHECK (right.getRed() > 150);
        CHECK (right.getGreen() > left.getGreen() + 20);   // orange on the right, pink on the left
        CHECK (left.getBlue() > right.getBlue());
    }
    Finish::set (Finish::Type::obsidian, false);
}

TEST_CASE ("AURORA's pearl is 10% less bright; OBSIDIAN is unchanged", "[ui]")
{
    // The faceplate's pearl and grain drawn with and without the dimming: on
    // Aurora every pixel of the pearl comes out at 90% (to within rounding),
    // so pure white is about 230; on Obsidian the two are identical.
    const auto draw = [] (bool dim)
    {
        juce::Image image (juce::Image::ARGB, 400, 300, true, juce::SoftwareImageType());
        juce::Graphics g (image);
        g.fillAll (juce::Colours::black);
        const auto grain = Finish::makeSatinGrain (400, 300, 99);
        MainView::paintFaceplate (g, { 40.0f, 40.0f, 320.0f, 220.0f }, grain, dim);
        return image;
    };
    const auto brightness = [] (juce::Colour c) { return juce::jmax (c.getRed(), c.getGreen(), c.getBlue()); };

    Finish::set (Finish::Type::aurora, false);
    const auto bright = draw (false), dimmed = draw (true);
    int worst = 0, brightest = 0;
    juce::Path inside;   // the plate, 2 px in from its edge (outside it is its shadow, which isn't pearl)
    inside.addRoundedRectangle (juce::Rectangle<float> (40.0f, 40.0f, 320.0f, 220.0f).reduced (2.0f), 8.0f);
    for (int y = 40; y < 260; ++y)
        for (int x = 40; x < 360; ++x)
        {
            if (! inside.contains ((float) x + 0.5f, (float) y + 0.5f))
                continue;
            const auto before = bright.getPixelAt (x, y), after = dimmed.getPixelAt (x, y);
            for (const auto [b, a] : { std::pair { before.getRed(), after.getRed() }, { before.getGreen(), after.getGreen() }, { before.getBlue(), after.getBlue() } })
                worst = juce::jmax (worst, std::abs ((int) a - juce::roundToInt ((1.0f - Finish::auroraDimming) * (float) b)));
            brightest = juce::jmax (brightest, (int) brightness (after));
        }
    CHECK (worst <= 1);
    CHECK (Finish::auroraDimming == 0.10f);
    CHECK (brightest <= 231);
    CHECK (brightest >= 215);   // still a white plate, not grey: the brightest pearl near 230

    Finish::set (Finish::Type::obsidian, false);
    const auto blackPlain = draw (false), blackDimmed = draw (true);
    int blackDifference = 0;
    for (int y = 0; y < 300; ++y)
        for (int x = 0; x < 400; ++x)
            if (blackPlain.getPixelAt (x, y) != blackDimmed.getPixelAt (x, y))
                ++blackDifference;
    CHECK (blackDifference == 0);
}
