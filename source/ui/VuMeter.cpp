#include "ui/VuMeter.h"

namespace
{
    // How far round the arc a reading sits (0 at the left end, 1 at the
    // right). A real VU scale is set out by voltage, so the dB marks crowd
    // to the left; this follows that, so it reads like the real thing.
    float positionFor (float vu)
    {
        const auto toVoltage = [] (float db) { return std::pow (10.0f, db / 20.0f); };
        const auto low = toVoltage (VuMeter::minVu), high = toVoltage (VuMeter::maxVu);
        return (toVoltage (juce::jlimit (VuMeter::minVu, VuMeter::maxVu, vu)) - low) / (high - low);
    }

    constexpr float sweep = 0.74f;   // the arc's half-angle, radians either side of straight up
}

void VuMeter::setNeedle (float vu)
{
    vu = juce::jlimit (minVu, maxVu, vu);
    if (! juce::approximatelyEqual (vu, needle))
    {
        needle = vu;
        repaint();
    }
}

void VuMeter::setReference (int dBFS)
{
    if (dBFS != reference)
    {
        reference = dBFS;
        repaint();
    }
}

void VuMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // The bezel: dark chrome, rounded.
    juce::Path bezel;
    bezel.addRoundedRectangle (bounds.reduced (2.0f), 22.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a575c), 0.0f, bounds.getY(), juce::Colour (0xff0d0c0e), 0.0f, bounds.getBottom(), false));
    g.fillPath (bezel);
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawRoundedRectangle (bounds.reduced (3.0f), 21.0f, 1.0f);

    // The face: warm cream, lit from the top.
    const auto face = bounds.reduced (14.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6ecc8), 0.0f, face.getY(), juce::Colour (0xffd9c792), 0.0f, face.getBottom(), false));
    g.fillRoundedRectangle (face, 12.0f);

    // The arc and its marks, round a pivot below the face.
    const juce::Point<float> pivot { face.getCentreX(), face.getBottom() + face.getHeight() * 0.18f };
    const auto radius = face.getHeight() * 0.86f;
    const auto pointAt = [&] (float vu, float r)
    {
        const auto angle = -sweep + 2.0f * sweep * positionFor (vu);
        return pivot + juce::Point<float> (std::sin (angle) * r, -std::cos (angle) * r);
    };
    const auto ink = juce::Colour (0xff201c18), red = juce::Colour (0xffd8321f);

    juce::Path arc, redArc;
    for (int i = 0; i <= 60; ++i)
    {
        const auto p = pointAt (minVu + (maxVu - minVu) * (float) i / 60.0f, radius);
        i == 0 ? arc.startNewSubPath (p) : arc.lineTo (p);
    }
    for (int i = 0; i <= 20; ++i)
    {
        const auto p = pointAt (maxVu * (float) i / 20.0f, radius + 4.0f);
        i == 0 ? redArc.startNewSubPath (p) : redArc.lineTo (p);
    }
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.6f));
    g.setColour (red);
    g.strokePath (redArc, juce::PathStrokeType (6.0f));

    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    for (const auto mark : scaleMarks)
    {
        g.setColour (mark > 0.0f ? red : ink);
        g.drawLine (juce::Line<float> (pointAt (mark, radius), pointAt (mark, radius + 9.0f)), 1.6f);
        const auto label = mark > 0.0f ? "+" + juce::String ((int) mark) : juce::String ((int) mark);
        g.drawText (label, juce::Rectangle<float> (30.0f, 14.0f).withCentre (pointAt (mark, radius + 21.0f)), juce::Justification::centred, false);
    }
    g.setColour (ink);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    // In the lower corners, clear of the needle's sweep.
    g.drawText ("VU", juce::Rectangle<float> (60.0f, 20.0f).withCentre ({ face.getX() + face.getWidth() * 0.16f, face.getBottom() - face.getHeight() * 0.2f }),
                juce::Justification::centred, false);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("O U T P U T", juce::Rectangle<float> (100.0f, 14.0f).withCentre ({ face.getRight() - face.getWidth() * 0.18f, face.getBottom() - face.getHeight() * 0.2f }),
                juce::Justification::centred, false);
    g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
    g.setColour (ink.withAlpha (0.75f));
    g.drawText ("0 VU = " + juce::String (reference) + " dBFS",
                juce::Rectangle<float> (100.0f, 12.0f).withCentre ({ face.getRight() - face.getWidth() * 0.18f, face.getBottom() - face.getHeight() * 0.2f + 13.0f }),
                juce::Justification::centred, false);

    // The needle, and the dark hood over its pivot.
    {
        juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (face.toNearestInt());
        const auto tip = pointAt (needle, radius + 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawLine (juce::Line<float> (pivot.translated (3.0f, 3.0f), tip.translated (3.0f, 3.0f)), 2.0f);
        g.setColour (juce::Colour (0xff141210));
        g.drawLine (juce::Line<float> (pivot, tip), 1.8f);

        const auto hood = juce::Rectangle<float> (face.getWidth() * 0.5f, face.getHeight() * 0.42f)
                              .withCentre ({ pivot.x, face.getBottom() + face.getHeight() * 0.03f });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2622), 0.0f, hood.getY(), juce::Colour (0xff0a0908), 0.0f, hood.getBottom(), false));
        g.fillEllipse (hood);
    }

    // The glass: a soft reflection across the top.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.28f), 0.0f, face.getY(),
                                             juce::Colours::white.withAlpha (0.0f), 0.0f, face.getY() + face.getHeight() * 0.45f, false));
    g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.45f), 12.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (face, 12.0f, 1.2f);
}
