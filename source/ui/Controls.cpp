#include "Controls.h"

#include "Finish.h"
#include "ParagonLookAndFeel.h"

namespace
{
    // The concept's orange-red ring light, and its white-hot core.
    const juce::Colour ringNeon { 0xffff4a1e };
    const juce::Colour ringCore { 0xffffc79a };

    // The meter window's dark glass, which the meters fill behind themselves.
    const juce::Colour meterGlass { 0xff050405 };

    juce::Point<float> onCircle (juce::Point<float> centre, float radius, float angle)
    {
        return { centre.x + std::sin (angle) * radius, centre.y - std::cos (angle) * radius };
    }
}

//==============================================================================
Knob::Knob (std::vector<juce::String> scale, juce::String unitText, juce::String captionText)
    : unit (std::move (unitText)), caption (std::move (captionText))
{
    for (size_t i = 0; i < scale.size(); ++i)
        marks.emplace_back (scale.size() > 1 ? (float) i / (float) (scale.size() - 1) : 0.5f, scale[i]);

    slider.setRotaryParameters (startAngle, endAngle, true);
    slider.setRange (0.0, 1.0);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.getProperties().set (ParagonLookAndFeel::invisibleSliderProperty, true);   // the knob draws itself
    slider.onValueChange = [this] { repaint(); };
    slider.setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);   // Shift-drag: fine control
    addAndMakeVisible (slider);
    setInterceptsMouseClicks (false, true);   // only the knob itself takes the mouse, not its printed scale
}

void Knob::setTravel (float start, float end)
{
    travelStart = start;
    travelEnd = end;
    slider.setRotaryParameters (start, end, true);
    repaint();
}

void Knob::addLabel (float proportion, juce::String text)
{
    labels.emplace_back (proportion, std::move (text));
    repaint();
}

void Knob::setScale (std::vector<std::pair<float, juce::String>> newMarks)
{
    marks = std::move (newMarks);
    repaint();
}

void Knob::place (juce::Point<float> centre, float ringRadius)
{
    radius = ringRadius;
    // Room for the scale round it and the unit and caption under it.
    const auto side = ringRadius + 40.0f;
    const auto below = ringRadius + (unit.isNotEmpty() && caption.isNotEmpty() ? 38.0f : 24.0f);
    const auto area = juce::Rectangle<float>::leftTopRightBottom (centre.x - side, centre.y - ringRadius - 24.0f,
                                                                  centre.x + side, centre.y + below);
    knobCentre = centre - area.getSmallestIntegerContainer().getPosition().toFloat();
    setBounds (area.getSmallestIntegerContainer());
    resized();
}

void Knob::resized()
{
    slider.setBounds (juce::Rectangle<float> ((radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f).withCentre (knobCentre).getSmallestIntegerContainer());
}

void Knob::paint (juce::Graphics& g)
{
    // The scale: a short tick by the ring and the number just outside it.
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    for (const auto& [proportion, text] : marks)
    {
        const auto angle = travelStart + proportion * (travelEnd - travelStart);
        g.setColour (Finish::inkDim());
        g.drawLine ({ onCircle (knobCentre, radius + 2.0f, angle), onCircle (knobCentre, radius + 6.0f, angle) }, 1.2f);
    }
    for (const auto& [proportion, text] : marks)
        printBeside (g, proportion, text);
    for (const auto& [proportion, text] : labels)
        printBeside (g, proportion, text);

    const auto proportion = (float) slider.valueToProportionOfLength (slider.getValue());
    drawKnob (g, knobCentre, radius, travelStart + proportion * (travelEnd - travelStart), travelStart, travelEnd);

    auto below = getLocalBounds().toFloat().withTop (knobCentre.y + radius + 4.0f);
    if (unit.isNotEmpty())
    {
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        ParagonLookAndFeel::drawStampedText (g, unit, below.removeFromTop (16.0f), juce::Justification::centredTop, Finish::ink());
    }
    if (caption.isNotEmpty())
    {
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        ParagonLookAndFeel::drawStampedText (g, caption, below.withTrimmedTop (unit.isEmpty() ? 4.0f : 0.0f).removeFromTop (18.0f),
                                             juce::Justification::centredTop, Finish::ink());
    }
}

void Knob::printBeside (juce::Graphics& g, float proportion, const juce::String& text) const
{
    // Set the label's box against the ring, whichever side it's on.
    const auto angle = travelStart + proportion * (travelEnd - travelStart);
    const auto width = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 2.0f;
    const auto height = 13.0f;
    const auto anchor = onCircle (knobCentre, radius + 8.0f, angle);
    const auto centre = anchor.translated (std::sin (angle) * width * 0.5f, -std::cos (angle) * height * 0.5f);
    ParagonLookAndFeel::drawStampedText (g, text, juce::Rectangle<float> (width, height).withCentre (centre),
                                         juce::Justification::centred, Finish::ink());
}

void Knob::drawKnob (juce::Graphics& g, juce::Point<float> c, float ringRadius, float angle, float travelStart, float travelEnd)
{
    const auto r = ringRadius;
    const auto capRadius = r * 0.72f;
    const auto grooveRadius = r * 0.86f;
    const auto circle = [&] (float radius) { return juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c); };

    // The knob's shadow on the panel, and the ring's light spilling onto it
    // where the ring ends, either side of the opening.
    {
        juce::Path disc;
        disc.addEllipse (circle (r));
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), juce::roundToInt (r * 0.4f), { 0, juce::roundToInt (r * 0.14f) }).drawForPath (g, disc);
        for (auto end : { travelStart, travelEnd })
        {
            const auto spot = onCircle (c, r * 0.9f, end);
            g.setGradientFill (juce::ColourGradient (ringNeon.withAlpha (0.4f), spot, ringNeon.withAlpha (0.0f), spot.translated (r * 0.5f, 0.0f), true));
            g.fillEllipse (juce::Rectangle<float> (r, r).withCentre (spot));
        }
    }

    // The bezel: a black collar, lit along its top edge.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2a2e), c.x, c.y - r, juce::Colour (0xff030303), c.x, c.y + r, false));
    g.fillEllipse (circle (r));
    g.setColour (juce::Colours::black);
    g.drawEllipse (circle (r), 1.0f);

    // The ring of light in its groove, open at the bottom.
    {
        g.setColour (juce::Colour (0xff0b0605));
        g.drawEllipse (circle (grooveRadius), r * 0.14f);
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, grooveRadius, grooveRadius, 0.0f, travelStart, travelEnd, true);
        const auto stroke = [&] (juce::Colour colour, float width)
        {
            g.setColour (colour);
            g.strokePath (arc, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        stroke (ringNeon.withAlpha (0.22f), r * 0.22f);
        stroke (ringNeon.withAlpha (0.55f), r * 0.11f);
        stroke (ringNeon, r * 0.06f);
        stroke (ringCore, r * 0.022f);

        // Hot spots where the ring ends, as on the concept.
        for (auto end : { travelStart, travelEnd })
        {
            const auto spot = onCircle (c, grooveRadius, end);
            g.setColour (ringCore.withAlpha (0.8f));
            g.fillEllipse (juce::Rectangle<float> (r * 0.09f, r * 0.09f).withCentre (spot));
        }
    }

    // The cap: glossy black (or the neon finish's colours), slightly domed,
    // with fine machining rings and a soft reflection at the top left.
    const auto cap = circle (capRadius);
    {
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), juce::roundToInt (r * 0.12f), { 0, juce::roundToInt (r * 0.05f) })
            .drawForRectangle (g, cap.reduced (capRadius * 0.15f).toNearestInt());

        if (Finish::neonKeys())
        {
            g.setGradientFill (Finish::neonGradient (cap.getX(), cap.getRight()));
            g.fillEllipse (cap);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.2f), c.x, cap.getY(),
                                                     juce::Colours::black.withAlpha (0.45f), c.x, cap.getBottom(), false));
            g.fillEllipse (cap);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35343a), cap.getX(), cap.getY(),
                                                     juce::Colour (0xff050506), cap.getRight(), cap.getBottom(), false));
            g.fillEllipse (cap);
        }

        g.setColour (juce::Colours::white.withAlpha (0.018f));
        for (int ring = 2; ring <= 9; ++ring)
            g.drawEllipse (circle (capRadius * (float) ring / 10.0f), 0.8f);

        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), c.x - capRadius * 0.35f, c.y - capRadius * 0.45f,
                                                 juce::Colours::white.withAlpha (0.0f), c.x, c.y + capRadius * 0.2f, true));
        g.fillEllipse (cap);

        juce::Path rim;
        rim.addCentredArc (c.x, c.y, capRadius - 0.6f, capRadius - 0.6f, 0.0f, -1.9f, 1.1f, true);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.strokePath (rim, juce::PathStrokeType (1.0f));
        g.setColour (juce::Colours::black);
        g.drawEllipse (cap, 1.0f);
    }

    // The pointer: a white line from near the centre to the edge.
    const auto from = onCircle (c, capRadius * 0.12f, angle), to = onCircle (c, capRadius * 0.9f, angle);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine ({ from.translated (0.0f, 1.0f), to.translated (0.0f, 1.0f) }, juce::jmax (2.0f, r * 0.08f));
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.drawLine ({ from, to }, juce::jmax (1.6f, r * 0.055f));
}

//==============================================================================
void Led::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto area = getLocalBounds().toFloat();
    drawLed (g, area.withSizeKeepingCentre (area.getHeight() * 0.65f, area.getHeight() * 0.65f), getToggleState());
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawEllipse (area.withSizeKeepingCentre (area.getHeight() * 0.7f, area.getHeight() * 0.7f), 1.0f);
    }
}

void Led::drawLed (juce::Graphics& g, juce::Rectangle<float> area, bool on)
{
    const auto c = area.getCentre();
    const auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto circle = [&] (float radius) { return juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c); };

    if (on)
    {
        // The light round a lit lamp.
        juce::ColourGradient halo (juce::Colour (0xffff4a1e).withAlpha (0.65f), c, juce::Colour (0xffff4a1e).withAlpha (0.0f), c.translated (r * 1.5f, 0.0f), true);
        halo.addColour (0.55, juce::Colour (0xffff4a1e).withAlpha (0.25f));
        g.setGradientFill (halo);
        g.fillEllipse (circle (r * 1.5f));
    }

    // A dark bezel, then the lens.
    g.setColour (on ? juce::Colour (0xff3a0c06) : juce::Colour (0xff1c0a08));
    g.fillEllipse (circle (r));
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (circle (r), 0.8f);

    juce::ColourGradient lens (on ? juce::Colour (0xfffff3b8) : juce::Colour (0xff6a2016), c.translated (-r * 0.15f, -r * 0.2f),
                               on ? juce::Colour (0xffe8341a) : juce::Colour (0xff2a0806), c.translated (r * 0.75f, 0.0f), true);
    lens.addColour (0.45, on ? juce::Colour (0xffffa83a) : juce::Colour (0xff4a120c));
    g.setGradientFill (lens);
    g.fillEllipse (circle (r * 0.74f));

    g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.25f));
    g.fillEllipse (juce::Rectangle<float> (r * 0.36f, r * 0.24f).withCentre (c.translated (-r * 0.22f, -r * 0.38f)));
}

//==============================================================================
LeverSwitch::LeverSwitch (int positions, int initialPosition, bool glow)
    : numPositions (juce::jmax (2, positions)), position (juce::jlimit (0, numPositions - 1, initialPosition)), glowWhenUp (glow)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void LeverSwitch::setPosition (int newPosition)
{
    newPosition = juce::jlimit (0, numPositions - 1, newPosition);
    if (newPosition == position)
        return;
    position = newPosition;
    repaint();
    if (onChange)
        onChange();
}

void LeverSwitch::mouseUp (const juce::MouseEvent& event)
{
    if (event.mouseWasClicked())
        setPosition ((position + 1) % numPositions);
}

void LeverSwitch::paint (juce::Graphics& g)
{
    const auto slot = getLocalBounds().toFloat().reduced (1.0f);
    const auto width = slot.getWidth();

    // The slot: a dark well with a lit lower lip.
    juce::Path well;
    well.addRoundedRectangle (slot, width * 0.5f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.4f), 6, { 0, 2 }).drawForPath (g, well);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050505), 0.0f, slot.getY(), juce::Colour (0xff2a282c), 0.0f, slot.getBottom(), false));
    g.fillPath (well);
    g.setColour (juce::Colours::black);
    g.strokePath (well, juce::PathStrokeType (1.0f));

    // The handle: a stem from the pivot and a ball on its end.
    const auto ballRadius = width * 0.4f;
    const auto travel = slot.getHeight() - ballRadius * 2.0f - 4.0f;
    const auto ballY = slot.getY() + 2.0f + ballRadius + travel * (float) position / (float) (numPositions - 1);
    const auto pivot = slot.getCentre();
    const auto ball = juce::Point<float> (pivot.x, ballY);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a383c), pivot.x - width * 0.2f, 0.0f, juce::Colour (0xff0c0b0d), pivot.x + width * 0.2f, 0.0f, false));
    g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (pivot.x - width * 0.18f, juce::jmin (pivot.y, ballY),
                                                                        pivot.x + width * 0.18f, juce::jmax (pivot.y, ballY)), width * 0.18f);

    const auto glowing = glowWhenUp && position == 0;
    const auto ballArea = juce::Rectangle<float> (ballRadius * 2.0f, ballRadius * 2.0f).withCentre (ball);
    if (glowing)
    {
        g.setGradientFill (juce::ColourGradient (ringNeon.withAlpha (0.5f), ball, ringNeon.withAlpha (0.0f), ball.translated (ballRadius * 2.0f, 0.0f), true));
        g.fillEllipse (ballArea.expanded (ballRadius));
    }
    juce::ColourGradient chrome (glowing ? juce::Colour (0xfffff0d8) : juce::Colours::white, ball.translated (-ballRadius * 0.3f, -ballRadius * 0.4f),
                                 glowing ? juce::Colour (0xffd8401a) : juce::Colour (0xff3a383c), ball.translated (ballRadius, ballRadius * 0.6f), true);
    chrome.addColour (0.5, glowing ? juce::Colour (0xffff9a52) : juce::Colour (0xffa8a4aa));
    g.setGradientFill (chrome);
    g.fillEllipse (ballArea);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (ballArea, 0.8f);
}

//==============================================================================
void RoundButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    const auto c = area.getCentre();
    const auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto circle = [&] (float radius) { return juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c); };

    // A dark collar, then a chrome dome.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050505), c.x, c.y - r, juce::Colour (0xff3a383c), c.x, c.y + r, false));
    g.fillEllipse (circle (r));
    const auto dome = circle (r * 0.72f).translated (0.0f, down ? 0.8f : 0.0f);
    juce::ColourGradient chrome (juce::Colours::white, dome.getCentre().translated (-r * 0.2f, -r * 0.35f),
                                 juce::Colour (0xff2e2c30), dome.getCentre().translated (r * 0.7f, r * 0.5f), true);
    chrome.addColour (0.55, juce::Colour (highlighted ? 0xffc0bcc2 : 0xff9a969c));
    g.setGradientFill (chrome);
    g.fillEllipse (dome);
    g.setColour (juce::Colours::black);
    g.drawEllipse (dome, 0.8f);
}

//==============================================================================
GainReductionMeter::GainReductionMeter (GainReductionSource& sourceToUse, Style styleToUse)
    : source (sourceToUse), style (styleToUse)
{
    setOpaque (true);   // fills the meter glass itself, so updating it doesn't redraw the panel behind
    startTimerHz (30);
}

void GainReductionMeter::timerCallback()
{
    constexpr auto fallDbPerTick = 30.0f / 30.0f;   // returns at 30 dB a second
    const auto reduction = juce::jlimit (0.0f, rangeDb, source.takeReduction());
    const auto newDb = juce::jmax (reduction, displayedDb - fallDbPerTick);
    const auto newMakeUp = makeUp != nullptr ? juce::jlimit (0.0f, rangeDb, makeUp->load()) : 0.0f;
    if (! juce::approximatelyEqual (newDb, displayedDb) || ! juce::approximatelyEqual (newMakeUp, displayedMakeUp))
    {
        displayedDb = newDb;
        displayedMakeUp = newMakeUp;
        repaint();
    }
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    g.fillAll (meterGlass);
    const auto bounds = getLocalBounds().toFloat();

    const auto drawSegment = [&] (juce::Rectangle<float> segment, juce::Colour colour, bool lit)
    {
        if (lit)
        {
            g.setColour (colour.withAlpha (0.3f));
            g.fillRect (segment.expanded (1.0f));
        }
        g.setColour (lit ? colour : colour.withAlpha (0.13f));
        g.fillRect (segment);
    };

    if (style == Style::vertical)
    {
        // From the bottom up: orange to pink, as the level meters.
        constexpr auto pitch = 4.0f;
        const auto count = (int) (bounds.getHeight() / pitch);
        for (int i = 0; i < count; ++i)
        {
            const auto db = rangeDb * ((float) i + 0.5f) / (float) count;
            const auto segment = juce::Rectangle<float> (bounds.getX() + 1.0f, bounds.getBottom() - (float) (i + 1) * pitch + 1.0f, bounds.getWidth() - 2.0f, pitch - 1.6f);
            drawSegment (segment, ParagonColours::meterColour (db / rangeDb), db <= displayedDb);
        }
        return;
    }

    // Gain change: reduction lights leftwards from the centre (0 dB),
    // make-up gain rightwards.
    constexpr auto pitch = 5.0f;
    const auto half = (int) (bounds.getWidth() * 0.5f / pitch);
    for (int i = -half; i < half; ++i)
    {
        const auto x = bounds.getCentreX() + (float) i * pitch;
        const auto db = rangeDb * ((float) (i < 0 ? -i : i + 1) - 0.5f) / (float) half;
        const auto segment = juce::Rectangle<float> (x + 0.8f, bounds.getY() + 1.0f, pitch - 1.8f, bounds.getHeight() - 2.0f);
        drawSegment (segment, ParagonColours::meterColour (db / rangeDb), i < 0 ? db <= displayedDb : db <= displayedMakeUp);
    }
}
