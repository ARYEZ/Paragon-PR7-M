#include "ui/LoudnessDisplay.h"

#include "ui/LedDisplay.h"

namespace
{
    const juce::Colour ledRed { 0xffff3a2a };

    // Seven-segment digits right-aligned in `area`, the point a narrow
    // cell of its own; returns where the number starts.
    float drawNumber (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, juce::Colour lit)
    {
        const auto height = area.getHeight();
        const auto digitWidth = height * 0.58f, spacing = height * 0.12f, pointWidth = height * 0.3f;
        auto x = area.getRight();
        for (auto i = text.length() - 1; i >= 0; --i)
        {
            const auto c = text[i];
            if (c == '.')
            {
                x -= pointWidth;
                const auto dot = juce::Rectangle<float> (height * 0.13f, height * 0.13f).withCentre ({ x + pointWidth * 0.3f, area.getBottom() - height * 0.08f });
                g.setColour (lit.withAlpha (0.25f));
                g.fillEllipse (dot.expanded (1.5f));
                g.setColour (lit);
                g.fillEllipse (dot);
                continue;
            }
            x -= digitWidth;
            LedDisplay::drawDigit (g, c, { x, area.getY(), digitWidth, height }, lit);
            x -= spacing;
        }
        return x;
    }
}

LoudnessDisplay::LoudnessDisplay()
{
    setTooltip ("The output's loudness.\n"
                "Left: short-term loudness, the last 3 seconds, in LUFS (as streaming services measure it).\n"
                "Right: the highest true peak since playback started, in dBTP, including peaks between the samples. "
                "Red above -1 dBTP.\n"
                "Click to clear the true peak.");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::String LoudnessDisplay::format (float value)
{
    if (value < lowest)
        return "--.-";
    const auto rounded = std::round (value * 10.0f) / 10.0f;
    // No plus sign on seven segments: above 0 is shown in red anyway.
    return juce::String (std::abs (rounded) < 0.05f ? 0.0f : rounded, 1);
}

void LoudnessDisplay::setReadings (float newLufs, float newTruePeakDb)
{
    // Repaint only when what's shown changes.
    if (format (newLufs) != format (lufs) || format (newTruePeakDb) != format (truePeakDb)
        || (newTruePeakDb > truePeakWarning) != (truePeakDb > truePeakWarning))
    {
        lufs = newLufs;
        truePeakDb = newTruePeakDb;
        repaint();
    }
}

void LoudnessDisplay::mouseUp (const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && onClear)
        onClear();
}

void LoudnessDisplay::paint (juce::Graphics& g)
{
    constexpr float size = 0.7f;
    const auto glass = LedDisplay::paintScreen (g, getLocalBounds().toFloat(), size);
    const auto orange = LedDisplay::ledOrange;

    const auto inner = glass.reduced (glass.getHeight() * 0.22f, glass.getHeight() * 0.2f);
    const auto half = inner.getWidth() * 0.5f;
    const auto unitWidth = inner.getHeight() * 1.7f;
    const auto drawHalf = [&] (juce::Rectangle<float> area, float value, const juce::String& unit, const juce::String& what, juce::Colour lit)
    {
        const auto units = area.removeFromRight (unitWidth);
        drawNumber (g, format (value), area.withTrimmedRight (inner.getHeight() * 0.2f), lit);
        g.setColour (lit.withAlpha (0.9f));
        g.setFont (juce::FontOptions (inner.getHeight() * 0.48f, juce::Font::bold));
        g.drawText (unit, units.withTrimmedBottom (units.getHeight() * 0.42f), juce::Justification::bottomLeft, false);
        g.setColour (lit.withAlpha (0.55f));
        g.setFont (juce::FontOptions (inner.getHeight() * 0.34f, juce::Font::bold));
        g.drawText (what, units.withTrimmedTop (units.getHeight() * 0.6f), juce::Justification::topLeft, false);
    };

    auto area = inner;
    drawHalf (area.removeFromLeft (half).withTrimmedRight (inner.getHeight() * 0.3f), lufs, "LUFS", "SHORT", orange);
    drawHalf (area.withTrimmedLeft (inner.getHeight() * 0.3f), truePeakDb, "dBTP", "MAX", truePeakDb > truePeakWarning ? ledRed : orange);

    // A faint divider between the two.
    g.setColour (orange.withAlpha (0.18f));
    g.fillRect (juce::Rectangle<float> (1.0f, inner.getHeight() * 0.9f).withCentre (inner.getCentre()));

    LedDisplay::paintGlassOverlay (g, glass, size);
}
