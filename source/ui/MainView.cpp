#include "ui/MainView.h"

#include "ui/Finish.h"
#include "ui/Logo.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Rectangle<float> body { 13.0f, 71.0f, 1422.0f, 556.0f };

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

    // The faceplate in the finish's pearl (PR7-C's plates; Pearl Black is
    // PR7-G's black unit's face gradient).
    void drawPlate (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        const auto black = Finish::isBlack();
        juce::Path plate;
        plate.addRoundedRectangle (bounds, 10.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 12, { 0, 4 }).drawForPath (g, plate);
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
        g.drawRoundedRectangle (bounds.reduced (1.0f), 9.0f, 1.5f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds, 10.0f, 1.0f);
    }

    // A black domed screw in a shallow socket.
    void drawScrew (juce::Graphics& g, juce::Point<float> c)
    {
        const auto circle = [&] (float radius) { return juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c); };
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.5f), c.x, c.y - 9.0f, juce::Colours::white.withAlpha (0.5f), c.x, c.y + 9.0f, false));
        g.fillEllipse (circle (9.0f));
        juce::ColourGradient dome (juce::Colour (0xff57555a), c.translated (-2.0f, -2.5f), juce::Colour (0xff050506), c.translated (5.0f, 4.0f), true);
        g.setGradientFill (dome);
        g.fillEllipse (circle (7.0f));
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.fillEllipse (juce::Rectangle<float> (3.2f, 2.2f).withCentre (c.translated (-2.0f, -3.0f)));
    }

    std::vector<juce::String> numbers (float from, float to, float step, bool signedLabels)
    {
        std::vector<juce::String> labels;
        for (auto value = from; value <= to + 0.001f; value += step)
        {
            const auto rounded = std::round (value * 10.0f) / 10.0f;
            auto text = juce::approximatelyEqual (rounded, std::round (rounded)) ? juce::String ((int) std::round (rounded)) : juce::String (rounded, 1);
            labels.push_back (signedLabels && rounded > 0.0f ? "+" + text : text);
        }
        return labels;
    }
}

MainView::MainView (ParagonPR7MAudioProcessor& processorToShow) : processor (processorToShow)
{
    setOpaque (true);
    using P = juce::Point<float>;

    // Down the left: the tone.
    attach (addKnob ("tone.air", P (287, 170), 36, { "-15", "-10", "-6", "-3", "0", "+3", "+6", "+10", "+15" }, "AIR",
                     "AIR: a gentle boost or cut of the very top (above about 12 kHz), up to 6 dB either way"), ParamIDs::air);
    attach (addKnob ("tone.focus", P (136, 326), 38, { "-15", "-10", "-6", "-3", "0", "+3", "+6", "+10", "+15" }, "FOCUS",
                     "FOCUS: the clarity of the mids, around 2.5 kHz (how well the words and the lead come through), up to 6 dB either way"),
            ParamIDs::focus);
    attach (addKnob ("tone.body", P (287, 480), 36, numbers (-5, 5, 1, true), "BODY",
                     "BODY: more or less low end (around 200 Hz and down), with the mix's loudness held the same"), ParamIDs::body);

    // The middle: PUSH, the VU meter, VOLUME; RESET and BYPASS above; WIDTH below.
    attach (addKnob ("dyn.push", P (470, 346), 74, numbers (0, 15, 1.5f, false), "PUSH",
                     "PUSH: the compressor, gentle to hard on one knob. It holds the mix together and makes it denser. "
                     "The green LEDs show how much it's turning down (1 dB each)."),
            ParamIDs::push);
    attach (addKnob ("dyn.volume", P (978, 346), 74, numbers (0, 15, 1.5f, false), "VOLUME",
                     "VOLUME: louder, all the way up. Turns the mix up (0 to +15 dB) into a soft clipper that rounds "
                     "the peaks off, then a limiter that keeps every peak under -1 dBTP. The green LEDs show how much "
                     "it's taking off the peaks (1 dB each)."),
            ParamIDs::volume);
    auto& reset = addLever ("panel.reset", P (480, 158), "RESET", "RESET: flick it up to set every knob back to its starting point");
    reset.onChange = [this, &reset]
    {
        if (reset.getPosition() != 0)
            return;
        processor.resetKnobs();
        // Momentary: it springs back down.
        juce::Timer::callAfterDelay (250, [lever = juce::Component::SafePointer<LeverSwitch> (&reset)]
        {
            if (lever != nullptr)
                lever->setPosition (1);
        });
    };
    auto& bypass = addLever ("panel.bypass", P (968, 158), "BYPASS",
                             "BYPASS (up): hear the mix without the processing, turned to the same loudness, to compare fairly", true);
    if (auto* parameter = processor.getParameters().getParameter (ParamIDs::bypass))
    {
        bypassAttachment = std::make_unique<juce::ParameterAttachment> (*parameter, [&bypass] (float value) { bypass.setPosition (value >= 0.5f ? 0 : 1); });
        bypass.onChange = [this, &bypass] { bypassAttachment->setValueAsCompleteGesture (bypass.getPosition() == 0 ? 1.0f : 0.0f); };
        bypassAttachment->sendInitialUpdate();
    }
    vu.setBounds (juce::Rectangle<int> (264, 200).withCentre ({ 724, 236 }));
    vu.setReference (processor.getVuReference());
    addAndMakeVisible (vu);
    loudness.setBounds (juce::Rectangle<int> (264, 46).withCentre ({ 724, 366 }));
    loudness.onClear = [this] { processor.resetTruePeak(); };
    addAndMakeVisible (loudness);
    controls["meter.loudness"] = &loudness;
    attach (addKnob ("stereo.width", P (724, 480), 36, numbers (-5, 5, 1, true), "WIDTH",
                     "WIDTH: narrower (left, mono at -5) or wider (right, twice as wide at +5) stereo. "
                     "The bass always stays in the middle, and the mix sounds the same in mono."),
            ParamIDs::width);

    // Down the right: the character.
    attach (addKnob ("dyn.transients", P (1160, 170), 36, numbers (-10, 10, 2, true), "TRANSIENTS",
                     "TRANSIENTS: sharper attacks (right) or softer ones (left), up to 6 dB on the start of each hit. "
                     "What sustains is left alone."),
            ParamIDs::transients);
    attach (addKnob ("colour.analog", P (1305, 326), 38, numbers (0, 10, 1, false), "ANALOG",
                     "ANALOG: harmonics for dimension and colour, mostly warm (2nd harmonic) with a little edge (3rd). "
                     "It changes the colour, not the loudness."),
            ParamIDs::analog);
    attach (addKnob ("dyn.bassPunch", P (1160, 480), 36, numbers (0, 10, 1, false), "BASS PUNCH",
                     "BASS PUNCH: more low end, and tighter, at the same time. Lifts the lows up to 6 dB, "
                     "and takes the lift back on loud bass notes, so the low end comes out fuller and more even."),
            ParamIDs::bassPunch);

    // The header: the presets and the settings.
    addHeaderButton ("preset.previous", {}, { 522, 24, 32, 32 }, "The previous preset").getProperties().set (ParagonLookAndFeel::iconProperty, "left");
    addHeaderButton ("preset.next", {}, { 894, 24, 32, 32 }, "The next preset").getProperties().set (ParagonLookAndFeel::iconProperty, "right");
    for (const auto* id : { "preset.previous", "preset.next" })
        findControl (id)->getProperties().set (ParagonLookAndFeel::bareProperty, true);
    dynamic_cast<juce::Button*> (findControl ("preset.previous"))->onClick = [this] { processor.getPresets().step (-1); };
    dynamic_cast<juce::Button*> (findControl ("preset.next"))->onClick = [this] { processor.getPresets().step (1); };
    addHeaderButton ("preset.save", "SAVE", { 1203, 22, 96, 36 }, "Save the knobs' settings as your own preset").onClick = [this] { askToSavePreset(); };
    auto& presetMenu = addHeaderButton ("preset.menu", {}, { 1314, 22, 56, 36 },
                                        "More preset options: back to the defaults, delete one of your presets, find the presets folder");
    presetMenu.getProperties().set (ParagonLookAndFeel::iconProperty, "dots");
    presetMenu.onClick = [this] { showPresetMenu(); };
    auto& settings = addHeaderButton ("settings", {}, { 1382, 20, 40, 40 }, "Settings: the panel's finish, and the level the VU meter's 0 VU stands for");
    settings.getProperties().set (ParagonLookAndFeel::iconProperty, "gear");
    settings.getProperties().set (ParagonLookAndFeel::bareProperty, true);
    settings.onClick = [this] { showSettingsMenu(); };
    shownPreset = presetDisplayText();

    grain = Finish::makeSatinGrain (width, height, 7);
    Finish::changes().addChangeListener (this);
    startTimerHz (60);
}

MainView::~MainView()
{
    Finish::changes().removeChangeListener (this);
}

Knob& MainView::addKnob (const juce::String& id, juce::Point<float> centre, float ringRadius, std::vector<juce::String> scale,
                         const juce::String& caption, const juce::String& tooltip)
{
    auto* knob = new Knob (std::move (scale), {}, caption);
    ownedControls.add (knob);
    knob->place (centre, ringRadius);
    knob->slider.setValue (0.5, juce::dontSendNotification);
    knob->slider.setTooltip (tooltip);
    addAndMakeVisible (knob);
    controls[id] = knob;
    return *knob;
}

void MainView::attach (Knob& knob, const char* parameterId)
{
    // The knob follows its parameter; a double-click sets it back to its
    // starting point.
    if (auto* parameter = processor.getParameters().getParameter (parameterId))
    {
        attachments.push_back (std::make_unique<juce::SliderParameterAttachment> (*parameter, knob.slider));
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            knob.slider.setDoubleClickReturnValue (true, ranged->convertFrom0to1 (ranged->getDefaultValue()));
    }
}

juce::TextButton& MainView::addHeaderButton (const juce::String& id, const juce::String& text, juce::Rectangle<int> bounds, const juce::String& tooltip)
{
    auto* button = new juce::TextButton (text);
    ownedControls.add (button);
    button->getProperties().set (ParagonLookAndFeel::consoleButtonProperty, true);
    button->setTooltip (tooltip);
    button->setBounds (bounds);
    addAndMakeVisible (button);
    controls[id] = button;
    return *button;
}

LeverSwitch& MainView::addLever (const juce::String& id, juce::Point<float> centre, const juce::String& caption, const juce::String& tooltip, bool glowWhenUp)
{
    auto* lever = new LeverSwitch (2, 1, glowWhenUp);
    ownedControls.add (lever);
    lever->setTooltip (tooltip);
    lever->setBounds (juce::Rectangle<int> (24, 54).withCentre (centre.toInt()));
    addAndMakeVisible (lever);
    controls[id] = lever;
    leverCaptions.emplace_back (caption, centre.translated (0.0f, 42.0f));
    return *lever;
}

juce::Component* MainView::findControl (const juce::String& id) const
{
    const auto found = controls.find (id);
    return found != controls.end() ? found->second : nullptr;
}

void MainView::setGainReduction (float pushDb, float volumeDb)
{
    if (! juce::approximatelyEqual (pushDb, pushReduction) || ! juce::approximatelyEqual (volumeDb, volumeReduction))
    {
        pushReduction = pushDb;
        volumeReduction = volumeDb;
        repaint();
    }
}

void MainView::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Finish");
    // In the order they're best read, not the order they're saved in.
    for (const auto type : { Finish::Type::pearlWhite, Finish::Type::pearlWhiteNeon, Finish::Type::pearlBlack })
        menu.addItem (Finish::names[(int) type], true, Finish::get() == type, [type] { Finish::set (type); });
    // Saved with the project, unlike the finish.
    menu.addSectionHeader ("VU meter: 0 VU is");
    for (const auto reference : ParagonPR7MAudioProcessor::vuReferenceChoices)
        menu.addItem (juce::String (reference) + " dBFS" + (reference == ParagonPR7MAudioProcessor::defaultVuReference ? " (streaming loudness)" : ""),
                      true, processor.getVuReference() == reference,
                      [this, reference]
                      {
                          processor.setVuReference (reference);
                          vu.setReference (reference);
                      });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (findControl ("settings")));
}

void MainView::timerCallback()
{
    updateMeters();
    // The preset's name follows loads, saves and changes made since.
    if (const auto text = presetDisplayText(); text != shownPreset)
    {
        shownPreset = text;
        repaint (presetDisplay.toNearestInt());
    }
}

juce::String MainView::presetDisplayText() const
{
    auto& presets = processor.getPresets();
    const auto name = presets.getCurrentName();
    if (name.isEmpty())
        return "Untitled";
    return presets.isModified() ? name + " *" : name;   // * : changed since it was loaded
}

void MainView::mouseDown (const juce::MouseEvent& event)
{
    // Clicking the preset's name lists them all.
    if (presetDisplay.contains (event.position))
        showPresetList();
}

void MainView::showPresetList()
{
    auto& presets = processor.getPresets();
    const auto list = presets.getPresets();
    const auto* current = presets.findCurrent (list);
    juce::PopupMenu menu;
    bool userHeader = false;
    menu.addSectionHeader ("Factory");
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (! list[i].factory && ! userHeader)
        {
            menu.addSectionHeader ("Yours");
            userHeader = true;
        }
        menu.addItem ((int) i + 1, list[i].name, true, &list[i] == current);
    }
    if (! userHeader)
        menu.addItem (-1, "(Your presets appear here once you SAVE one)", false);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (findControl ("preset.next")).withTargetScreenArea (
                            localAreaToGlobal (presetDisplay.toNearestInt())),
                        [this, list] (int chosen)
                        {
                            if (juce::isPositiveAndNotGreaterThan (chosen, (int) list.size()))
                                processor.getPresets().load (list[(size_t) chosen - 1]);
                        });
}

void MainView::showPresetMenu()
{
    auto& presets = processor.getPresets();
    const auto list = presets.getPresets();
    const auto* current = presets.findCurrent (list);
    const auto deletable = current != nullptr && ! current->factory;
    juce::PopupMenu menu;
    menu.addItem (1, "Back to the default settings");
    menu.addItem (2, deletable ? "Delete \"" + current->name + "\"" : juce::String ("Delete (only your own presets)"), deletable);
    menu.addItem (3, "Show the presets folder");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (findControl ("preset.menu")),
                        [this, toDelete = deletable ? *current : PresetManager::Preset()] (int chosen)
                        {
                            auto& p = processor.getPresets();
                            if (chosen == 1)
                                p.loadFactory (0);
                            else if (chosen == 2)
                                juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                                                  .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                                                  .withTitle ("Delete preset")
                                                                  .withMessage ("Delete \"" + toDelete.name + "\"? This can't be undone.")
                                                                  .withButton ("Delete").withButton ("Cancel").withAssociatedComponent (this),
                                                              [this, toDelete] (int result)
                                                              {
                                                                  if (result == 1)
                                                                      processor.getPresets().remove (toDelete);
                                                              });
                            else if (chosen == 3)
                            {
                                p.getUserFolder().createDirectory();
                                p.getUserFolder().startAsProcess();
                            }
                        });
}

void MainView::askToSavePreset()
{
    auto& presets = processor.getPresets();
    const auto list = presets.getPresets();
    const auto* current = presets.findCurrent (list);
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "A name for your preset:", juce::MessageBoxIconType::NoIcon, this);
    saveDialog->addTextEditor ("name", current != nullptr && ! current->factory ? current->name : juce::String());
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        const auto name = saveDialog->getTextEditorContents ("name").trim();
        saveDialog.reset();
        if (result == 1 && name.isNotEmpty())
            savePreset (name);
    }), false);
}

void MainView::savePreset (const juce::String& name)
{
    auto& presets = processor.getPresets();
    const auto list = presets.getPresets();
    const auto clash = std::find_if (list.begin(), list.end(), [&] (const auto& p) { return p.name.equalsIgnoreCase (name); });
    const auto tell = [this] (const juce::String& message)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon).withTitle ("Save preset")
                                          .withMessage (message).withButton ("OK").withAssociatedComponent (this), nullptr);
    };
    if (clash != list.end() && clash->factory)
        return tell ("\"" + clash->name + "\" is a factory preset. Please pick another name.");
    if (clash != list.end() && clash->name != presets.getCurrentName())
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon).withTitle ("Save preset")
                                          .withMessage ("You already have a preset called \"" + clash->name + "\". Replace it?")
                                          .withButton ("Replace").withButton ("Cancel").withAssociatedComponent (this),
                                      [this, name, tell] (int result)
                                      {
                                          if (result == 1 && ! processor.getPresets().save (name))
                                              tell ("The preset couldn't be saved.");
                                      });
        return;
    }
    if (! presets.save (name))
        tell ("The preset couldn't be saved.");
}

void MainView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    grain = Finish::makeSatinGrain (width, height, 7);
    repaint();
}

void MainView::updateMeters()
{
    // The needle follows the processor's own VU ballistics. If the host
    // stops sending audio, let it fall back as a real one would.
    const auto blocks = processor.getMeteredBlocks();
    ticksWithoutAudio = blocks != lastMeteredBlocks ? 0 : ticksWithoutAudio + 1;
    lastMeteredBlocks = blocks;
    shownVuLevel = ticksWithoutAudio < 10 ? processor.getVuLevel() : shownVuLevel * 0.88f;

    const auto reference = processor.getVuReference();
    if (reference != vu.getReference())   // the project was reloaded
        vu.setReference (reference);
    vu.setNeedle (shownVuLevel > 0.0f ? juce::Decibels::gainToDecibels (shownVuLevel) - (float) reference : VuMeter::minVu);

    loudness.setReadings (processor.getShortTermLufs(), juce::Decibels::gainToDecibels (processor.getTruePeak(), -100.0f));

    // PUSH and VOLUME's LEDs: the most each turned down since the last
    // look, falling back gently so short bursts stay visible.
    shownPushReduction = juce::jmax (processor.getPushReduction().takeReduction(), shownPushReduction - 0.5f);
    shownVolumeReduction = juce::jmax (processor.getVolumeReduction().takeReduction(), shownVolumeReduction - 0.5f);
    setGainReduction (shownPushReduction, shownVolumeReduction);
}

void MainView::paintReductionRing (juce::Graphics& g, const Knob& knob, float reductionDb) const
{
    // Green LEDs on the dark rim round the knob, along the right-hand end of
    // its travel (inside the scale), lighting from the top of the travel
    // downwards with the gain reduction: one LED per dB, up to 12.
    const auto centre = knob.getBounds().toFloat().getTopLeft() + knob.slider.getBounds().toFloat().getCentre();
    const auto radius = (float) knob.slider.getWidth() * 0.5f - 1.5f;
    constexpr int count = 12;
    for (int i = 0; i < count; ++i)
    {
        const auto proportion = 1.0f - 0.32f * (float) i / (float) (count - 1);   // from the end of the travel back
        const auto angle = Knob::startAngle + (Knob::endAngle - Knob::startAngle) * proportion;
        const auto at = centre + juce::Point<float> (std::sin (angle) * radius, -std::cos (angle) * radius);
        const auto lit = (float) i < reductionDb;
        const auto led = juce::Rectangle<float> (5.5f, 5.5f).withCentre (at);
        if (lit)
        {
            g.setColour (juce::Colour (0xff39ff6a).withAlpha (0.35f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (lit ? juce::Colour (0xff4dff7a) : juce::Colour (0xff163a1f));
        g.fillEllipse (led);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (led, 0.8f);
    }
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (ParagonColours::background);

    // Header: the emblem, the name, and the preset display.
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

        g.setColour (juce::Colour (0xff0a090b));
        g.fillRoundedRectangle (presetDisplay, 6.0f);
        g.setColour (ParagonColours::panelEdge);
        g.drawRoundedRectangle (presetDisplay, 6.0f, 1.0f);
        g.setColour (juce::Colour (0xffffb04a));
        g.setFont (juce::FontOptions (19.0f));
        g.drawFittedText (presetDisplayText(), presetDisplay.reduced (48.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }

    // The body: a dark frame, edged in red-orange light.
    g.setColour (juce::Colour (0xff070507));
    g.fillRoundedRectangle (body, 16.0f);
    for (const auto& [alpha, thickness] : { std::pair (0.07f, 16.0f), { 0.16f, 8.0f }, { 0.4f, 4.0f }, { 0.9f, 1.6f } })
    {
        g.setColour (ParagonColours::glow.withAlpha (alpha));
        g.drawRoundedRectangle (body, 16.0f, thickness);
    }

    // The faceplate, its satin grain, and a screw in each corner.
    const auto plate = faceplate();
    drawPlate (g, plate);
    {
        juce::Graphics::ScopedSaveState saved (g);
        juce::Path clip;
        clip.addRoundedRectangle (plate, 10.0f);
        g.reduceClipRegion (clip);
        g.drawImageAt (grain, 0, 0);
    }
    for (const auto corner : { plate.getTopLeft().translated (24.0f, 22.0f), plate.getTopRight().translated (-24.0f, 22.0f),
                               plate.getBottomLeft().translated (24.0f, -22.0f), plate.getBottomRight().translated (-24.0f, -22.0f) })
        drawScrew (g, corner);

    // The levers' captions, the name stamped low on the plate, and the mark.
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    for (const auto& [caption, at] : leverCaptions)
        ParagonLookAndFeel::drawStampedText (g, caption, juce::Rectangle<float> (120.0f, 18.0f).withCentre (at), juce::Justification::centred, Finish::ink());
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold | juce::Font::italic));
    ParagonLookAndFeel::drawStampedText (g, "MASTERING", juce::Rectangle<float> (220.0f, 20.0f).withCentre ({ 520.0f, 548.0f }),
                                         juce::Justification::centred, Finish::inkDim());
    ParagonLookAndFeel::drawStampedText (g, "PROCESSOR", juce::Rectangle<float> (220.0f, 20.0f).withCentre ({ 520.0f, 570.0f }),
                                         juce::Justification::centred, Finish::inkDim());
    Logo::drawMark (g, juce::Rectangle<float> (180.0f, 34.0f).withCentre ({ 958.0f, 556.0f }), juce::Justification::centred,
                    { Finish::ink(), Finish::printedRed(), Finish::stampEdge(), {}, 1.0f, false });
}

void MainView::paintOverChildren (juce::Graphics& g)
{
    // PUSH and VOLUME's gain-reduction LEDs, over the knobs' glow.
    if (auto* push = dynamic_cast<Knob*> (findControl ("dyn.push")))
        paintReductionRing (g, *push, pushReduction);
    if (auto* volume = dynamic_cast<Knob*> (findControl ("dyn.volume")))
        paintReductionRing (g, *volume, volumeReduction);
}
