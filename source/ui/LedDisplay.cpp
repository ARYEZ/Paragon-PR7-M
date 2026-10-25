#include "LedDisplay.h"

#include "ParagonLookAndFeel.h"

namespace
{
    // Which of the seven segments (a-g) light for each digit.
    //    a
    //  f   b
    //    g
    //  e   c
    //    d
    std::array<bool, 7> segmentsFor (juce::juce_wchar digit)
    {
        switch (digit)
        {
            case '0': return { 1, 1, 1, 1, 1, 1, 0 };
            case '1': return { 0, 1, 1, 0, 0, 0, 0 };
            case '2': return { 1, 1, 0, 1, 1, 0, 1 };
            case '3': return { 1, 1, 1, 1, 0, 0, 1 };
            case '4': return { 0, 1, 1, 0, 0, 1, 1 };
            case '5': return { 1, 0, 1, 1, 0, 1, 1 };
            case '6': return { 1, 0, 1, 1, 1, 1, 1 };
            case '7': return { 1, 1, 1, 0, 0, 0, 0 };
            case '8': return { 1, 1, 1, 1, 1, 1, 1 };
            case '9': return { 1, 1, 1, 1, 0, 1, 1 };
            case '-': return { 0, 0, 0, 0, 0, 0, 1 };
            default:  return { 0, 0, 0, 0, 0, 0, 0 };
        }
    }
}

void LedDisplay::drawDigit (juce::Graphics& g, juce::juce_wchar digit, juce::Rectangle<float> area, juce::Colour lit)
{
    // A classic seven-segment digit: each segment a long hexagon with
    // pointed ends and a small gap at the joints, leaning forwards like
    // the mockup's italic "001". Unlit segments are all but invisible, as
    // on a real LED display behind tinted glass.
    const auto w = area.getWidth(), h = area.getHeight();
    const auto t = w * 0.19f;            // segment thickness
    const auto gap = t * 0.18f;          // space at each joint
    const auto lean = h * 0.2f;          // how far the top leans right of the bottom
    const auto lit7 = segmentsFor (digit);

    // Upright cell (x 0..cellW, y 0..h); leaned with a shear afterwards.
    const auto cellW = w - lean;
    const auto half = t * 0.5f;

    const auto horizontal = [&] (float y)
    {
        juce::Path p;
        const auto x1 = half + gap, x2 = cellW - half - gap;
        p.startNewSubPath (x1, y);
        p.lineTo (x1 + half, y - half);
        p.lineTo (x2 - half, y - half);
        p.lineTo (x2, y);
        p.lineTo (x2 - half, y + half);
        p.lineTo (x1 + half, y + half);
        p.closeSubPath();
        return p;
    };
    const auto vertical = [&] (float x, float y1, float y2)
    {
        juce::Path p;
        y1 += gap;
        y2 -= gap;
        p.startNewSubPath (x, y1);
        p.lineTo (x + half, y1 + half);
        p.lineTo (x + half, y2 - half);
        p.lineTo (x, y2);
        p.lineTo (x - half, y2 - half);
        p.lineTo (x - half, y1 + half);
        p.closeSubPath();
        return p;
    };

    const auto top = half, middle = h * 0.5f, bottom = h - half;
    const auto left = half, right = cellW - half;
    const std::array<juce::Path, 7> segments {
        horizontal (top),                     // a
        vertical (right, top, middle),        // b
        vertical (right, middle, bottom),     // c
        horizontal (bottom),                  // d
        vertical (left, middle, bottom),      // e
        vertical (left, top, middle),         // f
        horizontal (middle),                  // g
    };

    const auto shear = juce::AffineTransform::shear (-lean / h, 0.0f).translated (area.getX() + lean, area.getY());

    juce::Path litPath, unlitPath;
    for (size_t i = 0; i < segments.size(); ++i)
        (lit7[i] ? litPath : unlitPath).addPath (segments[i], shear);

    g.setColour (lit.withAlpha (0.05f));
    g.fillPath (unlitPath);

    // Glow: the light bleeding into the glass round each lit segment.
    g.setColour (lit.withAlpha (0.12f));
    g.strokePath (litPath, juce::PathStrokeType (t * 1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (lit.withAlpha (0.22f));
    g.strokePath (litPath, juce::PathStrokeType (t * 0.45f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (lit);
    g.fillPath (litPath);
    g.setColour (juce::Colour (0xffffc28a).withAlpha (0.5f));   // hotter centre line, like a lit LED segment
    g.strokePath (litPath, juce::PathStrokeType (t * 0.12f), juce::AffineTransform());
}

juce::Rectangle<float> LedDisplay::paintScreen (juce::Graphics& g, juce::Rectangle<float> bounds, float size)
{
    // A polished metal frame set into the face: chrome lit from above, with
    // a dark seam where it meets the black bezel inside it.
    juce::Path frame;
    frame.addRoundedRectangle (bounds.reduced (1.0f), 11.0f * size);
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), juce::roundToInt (9.0f * size), { 0, juce::roundToInt (3.0f * size) }).drawForPath (g, frame);
    {
        juce::ColourGradient chrome (juce::Colour (0xfffaf3f6), 0.0f, bounds.getY(), juce::Colour (0xff5e4f57), 0.0f, bounds.getBottom(), false);
        chrome.addColour (0.12, juce::Colour (0xffcbbac3));
        chrome.addColour (0.5, juce::Colour (0xff8f7d87));
        chrome.addColour (0.85, juce::Colour (0xffb7a4ae));
        g.setGradientFill (chrome);
        g.fillPath (frame);
    }
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.strokePath (frame, juce::PathStrokeType (1.0f));

    // Inside it, the bezel: thick glossy black plastic, with a fine highlight
    // along its top edge fading down the sides.
    const auto bezelArea = bounds.reduced (4.0f * size);
    juce::Path bezel;
    bezel.addRoundedRectangle (bezelArea, 8.0f * size);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b292c), bezelArea.getTopLeft(),
                                             juce::Colour (0xff070607), bezelArea.getBottomLeft(), false));
    g.fillPath (bezel);
    {
        juce::Path edge;
        edge.addRoundedRectangle (bezelArea.reduced (0.5f), 8.0f * size);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.3f), 0.0f, bezelArea.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), 0.0f, bezelArea.getY() + bezelArea.getHeight() * 0.4f, false));
        g.strokePath (edge, juce::PathStrokeType (1.0f));
    }

    // Tinted glass: very dark, warmed by the orange light behind it, with a
    // faint reflection across the top.
    const auto glass = bounds.reduced (11.0f * size);
    const auto radius = 5.0f * size;
    juce::Path glassPath;
    glassPath.addRoundedRectangle (glass, radius);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c0b06), glass.getX() + glass.getWidth() * 0.25f, glass.getCentreY(),
                                             juce::Colour (0xff070302), glass.getRight(), glass.getY(), true));
    g.fillPath (glassPath);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (glass, radius, 1.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), glass.getX(), glass.getY(),
                                             juce::Colours::white.withAlpha (0.0f), glass.getX(), glass.getCentreY(), false));
    g.fillRoundedRectangle (glass.withHeight (glass.getHeight() * 0.5f), radius);
    return glass;
}

void LedDisplay::paintGlassOverlay (juce::Graphics& g, juce::Rectangle<float> glass, float size)
{
    // What sits in front of the readout: the glass itself.
    juce::Path glassPath;
    glassPath.addRoundedRectangle (glass, 5.0f * size);
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (glassPath);

    // The display's fine pixel rows, faintly visible.
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    for (auto y = glass.getY() + 1.0f; y < glass.getBottom(); y += 2.0f)
        g.fillRect (glass.getX(), y, glass.getWidth(), 0.6f);

    // The bezel's shadow falling on the glass along the top.
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0.0f, glass.getY(),
                                             juce::Colours::black.withAlpha (0.0f), 0.0f, glass.getY() + 9.0f * size, false));
    g.fillRect (glass.withHeight (9.0f * size));

    // A reflection in the glass: a soft diagonal band of light across the
    // upper left, and a fainter one beside it.
    const auto band = [&] (float start, float width, float alpha)
    {
        juce::Path streak;
        const auto x = glass.getX() + glass.getWidth() * start, w = glass.getWidth() * width, slant = glass.getHeight() * 0.6f;
        streak.addQuadrilateral (x + slant, glass.getY(), x + slant + w, glass.getY(),
                                 x + w, glass.getBottom(), x, glass.getBottom());
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (alpha), 0.0f, glass.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), 0.0f, glass.getBottom(), false));
        g.fillPath (streak);
    };
    band (0.04f, 0.13f, 0.09f);
    band (0.2f, 0.035f, 0.06f);
}
