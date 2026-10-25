#include "ui/MainView.h"

#include "ui/Finish.h"
#include "ui/Logo.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    // The placeholder's layout.
    const juce::Rectangle<float> body { 13.0f, 71.0f, 1422.0f, 934.0f };
    const juce::Rectangle<float> modulesPlate { 30.0f, 88.0f, 1040.0f, 900.0f };
    const juce::Rectangle<float> inOutPlate { 1086.0f, 88.0f, 332.0f, 900.0f };
    const juce::Rectangle<float> meterWindow { 1104.0f, 430.0f, 296.0f, 520.0f };

    // "PARAGON PR7-M" with "MASTERING" spaced out under it.
    void drawName (juce::Graphics& g, juce::Point<float> origin)
    {
        const juce::Rectangle<float> name { origin.x, origin.y, 250.0f, 26.0f };
        const auto font = juce::FontOptions (26.0f, juce::Font::bold | juce::Font::italic);
        g.setFont (font);
        g.setColour (ParagonColours::text);
        g.drawText ("PARAGON", name, juce::Justification::centredLeft, false);
        const auto nameWidth = (float) juce::GlyphArrangement::getStringWidthInt (font, "PARAGON ");
        Logo::drawMark (g, name.withTrimmedLeft (nameWidth).withSizeKeepingCentre (name.getWidth() - nameWidth, 20.0f),
                        juce::Justification::centredLeft, { ParagonColours::glow, ParagonColours::glow, {}, {}, 1.0f, true });

        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.setColour (ParagonColours::text.withAlpha (0.8f));
        g.drawText ("M A S T E R I N G", juce::Rectangle<float> (origin.x + 2.0f, origin.y + 28.0f, 240.0f, 14.0f),
                    juce::Justification::centredLeft, false);
    }

    // A plate in the finish's pearl (PR7-C's plates; Pearl Black is PR7-G's
    // black unit's face gradient).
    void drawPlate (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        const auto black = Finish::isBlack();
        juce::Path plate;
        plate.addRoundedRectangle (bounds, 8.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 10, { 0, 3 }).drawForPath (g, plate);
        juce::ColourGradient pearl (Finish::pick (0xfff6f3f4, 0xff1a1819), 0.0f, bounds.getY(),
                                    Finish::pick (0xffd6d0d3, 0xff070707), 0.0f, bounds.getBottom(), false);
        if (black)
        {
            pearl.addColour (0.18, juce::Colour (0xff121112));
            pearl.addColour (0.42, juce::Colour (0xff0a0a0a));
            pearl.addColour (0.7, juce::Colour (0xff0e0d0e));
        }
        g.setGradientFill (pearl);
        g.fillPath (plate);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (black ? 0.25f : 0.95f), 0.0f, bounds.getY(),
                                                 juce::Colours::black.withAlpha (black ? 0.6f : 0.35f), 0.0f, bounds.getBottom(), false));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 7.0f, 1.5f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
    }

    juce::String signedDb (float db) { return (db > 0.0f ? "+" : "") + juce::String (juce::roundToInt (db)); }
}

MainView::MainView (ParagonPR7MAudioProcessor& processorToShow)
    : processor (processorToShow),
      inputLeft (processorToShow.getInputMeter (0)), inputRight (processorToShow.getInputMeter (1)),
      outputLeft (processorToShow.getOutputMeter (0)), outputRight (processorToShow.getOutputMeter (1))
{
    setOpaque (true);

    addKnob ("output.input", { 1252.0f, 200.0f }, 43.0f, "INPUT", ParamIDs::inputGain, "INPUT: the level into the processor, -24 to +12 dB");
    addKnob ("output.output", { 1252.0f, 340.0f }, 43.0f, "OUTPUT", ParamIDs::outputGain, "OUTPUT: the level out of the processor, -24 to +12 dB");

    // IN L, IN R | OUT L, OUT R, in the meter window.
    const auto top = juce::roundToInt (meterWindow.getY()) + 40, meterHeight = juce::roundToInt (meterWindow.getHeight()) - 90;
    int x = juce::roundToInt (meterWindow.getX()) + 70;
    for (auto* meter : getLevelMeters())
    {
        meter->setBounds (x, top, 16, meterHeight);
        addAndMakeVisible (meter);
        x += meter == &inputRight ? 88 : 24;
    }

    auto* settings = new juce::TextButton();
    ownedControls.add (settings);
    settings->getProperties().set (ParagonLookAndFeel::consoleButtonProperty, true);
    settings->getProperties().set (ParagonLookAndFeel::iconProperty, "gear");
    settings->getProperties().set (ParagonLookAndFeel::bareProperty, true);
    settings->setTooltip ("Settings: the panel's finish");
    settings->setBounds (1382, 20, 40, 40);
    settings->onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible (settings);
    controls["settings"] = settings;

    grain = Finish::makeSatinGrain (width, height, 7);
    Finish::changes().addChangeListener (this);
}

MainView::~MainView()
{
    Finish::changes().removeChangeListener (this);
}

Knob& MainView::addKnob (const juce::String& id, juce::Point<float> centre, float ringRadius, const juce::String& caption,
                         const juce::String& parameterId, const juce::String& tooltip)
{
    auto* knob = new Knob ({}, {}, caption);
    ownedControls.add (knob);
    knob->place (centre, ringRadius);
    auto* parameter = processor.getParameters().getParameter (parameterId);
    std::vector<std::pair<float, juce::String>> marks;
    for (const auto db : { -24.0f, -12.0f, 0.0f, 12.0f })
        marks.emplace_back (parameter->convertTo0to1 (db), signedDb (db));
    knob->setScale (std::move (marks));
    attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.getParameters(), parameterId, knob->slider));
    knob->slider.setDoubleClickReturnValue (true, 0.0);
    knob->slider.setPopupDisplayEnabled (true, true, this);
    knob->slider.setTooltip (tooltip);
    addAndMakeVisible (knob);
    controls[id] = knob;
    return *knob;
}

juce::Component* MainView::findControl (const juce::String& id) const
{
    const auto found = controls.find (id);
    return found != controls.end() ? found->second : nullptr;
}

void MainView::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Finish");
    // In the order they're best read, not the order they're saved in.
    for (const auto type : { Finish::Type::pearlWhite, Finish::Type::pearlWhiteNeon, Finish::Type::pearlBlack })
        menu.addItem (Finish::names[(int) type], true, Finish::get() == type, [type] { Finish::set (type); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (findControl ("settings")));
}

void MainView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    grain = Finish::makeSatinGrain (width, height, 7);
    repaint();
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (ParagonColours::background);

    // Header: the emblem and the name.
    {
        const juce::Rectangle<float> emblem { 34.0f, 14.0f, 42.0f, 40.0f };
        juce::Path triangle;
        triangle.addTriangle (emblem.getTopLeft(), emblem.getTopRight(), { emblem.getCentreX(), emblem.getBottom() });
        g.setColour (ParagonColours::glow.withAlpha (0.3f));
        g.strokePath (triangle, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (ParagonColours::glow);
        g.strokePath (triangle, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        triangle.applyTransform (juce::AffineTransform::scale (0.5f, 0.5f, emblem.getCentreX(), emblem.getCentreY() - 3.0f));
        g.strokePath (triangle, juce::PathStrokeType (1.5f));
        drawName (g, { 98.0f, 18.0f });
    }

    // The body: a dark frame, edged in red-orange light.
    g.setColour (juce::Colour (0xff070507));
    g.fillRoundedRectangle (body, 16.0f);
    for (const auto& [alpha, thickness] : { std::pair (0.07f, 16.0f), { 0.16f, 8.0f }, { 0.4f, 4.0f }, { 0.9f, 1.6f } })
    {
        g.setColour (ParagonColours::glow.withAlpha (alpha));
        g.drawRoundedRectangle (body, 16.0f, thickness);
    }

    // The plates, with the satin grain over them.
    juce::Path plates;
    for (const auto& plate : { modulesPlate, inOutPlate })
    {
        drawPlate (g, plate);
        plates.addRoundedRectangle (plate, 8.0f);
    }
    {
        juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (plates);
        g.drawImageAt (grain, 0, 0);
    }

    // Where the modules will go.
    g.setColour (Finish::inkDim());
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("THE MASTERING MODULES GO HERE", modulesPlate.withSizeKeepingCentre (modulesPlate.getWidth(), 30.0f).translated (0.0f, -16.0f),
                juce::Justification::centred, false);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("built from the concept design, one module at a time",
                modulesPlate.withSizeKeepingCentre (modulesPlate.getWidth(), 24.0f).translated (0.0f, 18.0f), juce::Justification::centred, false);

    // IN / OUT: the title, and the meter window with its scale.
    g.setColour (Finish::ink());
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText ("IN / OUT", inOutPlate.withHeight (40.0f).translated (0.0f, 10.0f), juce::Justification::centred, false);

    g.setColour (juce::Colour (0xff050405));
    g.fillRoundedRectangle (meterWindow, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawRoundedRectangle (meterWindow.reduced (1.5f), 7.0f, 0.8f);

    const auto meterTop = (float) inputLeft.getY(), meterBottom = (float) inputLeft.getBottom();
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    for (const auto db : LevelMeter::scaleMarksDb)
    {
        const auto y = meterBottom - (meterBottom - meterTop) * LevelMeter::proportionForDb (db);
        for (const auto x : { meterWindow.getX() + 30.0f, meterWindow.getRight() - 30.0f })
            g.drawText (signedDb (db), juce::Rectangle<float> (40.0f, 14.0f).withCentre ({ x, y }), juce::Justification::centred, false);
    }
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    for (const auto& [text, left, right] : { std::tuple ("IN", &inputLeft, &inputRight), { "OUT", &outputLeft, &outputRight } })
        g.drawText (text, juce::Rectangle<float> ((float) left->getX(), meterBottom + 12.0f, (float) (right->getRight() - left->getX()), 18.0f),
                    juce::Justification::centred, false);
}
