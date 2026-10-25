#include "Logo.h"

namespace Logo
{
    namespace
    {
        // The letters are designed 1 unit tall, upright, then slanted. Stems
        // are 0.24 wide and bars 0.2 tall: chunky, for a bold silhouette.
        constexpr float slant = 0.3f;     // how far the top leans right, per unit of height
        constexpr float spacing = 0.1f;
        constexpr float letterWidths[] = { 0.8f, 0.84f, 0.8f, 0.4f, 0.98f };   // P, R, 7, dash, M
        constexpr float totalWidth = 0.8f + 0.84f + 0.8f + 0.4f + 0.98f + 4.0f * spacing + slant;

        // The two speed cuts, as fractions of the height from the top.
        constexpr std::pair<float, float> cuts[] = { { 0.63f, 0.68f }, { 0.79f, 0.83f } };

        void addPolygon (juce::Path& path, std::initializer_list<juce::Point<float>> points, float x)
        {
            auto first = true;
            for (auto point : points)
            {
                point.x += x;
                if (first)
                    path.startNewSubPath (point);
                else
                    path.lineTo (point);
                first = false;
            }
            path.closeSubPath();
        }

        // P's body (R shares it): bevelled corners on the bowl, and its counter.
        void addBowl (juce::Path& path, float x)
        {
            addPolygon (path, { { 0.0f, 0.0f }, { 0.64f, 0.0f }, { 0.8f, 0.15f }, { 0.8f, 0.45f }, { 0.64f, 0.6f },
                                { 0.24f, 0.6f }, { 0.24f, 1.0f }, { 0.0f, 1.0f } }, x);
            addPolygon (path, { { 0.24f, 0.2f }, { 0.56f, 0.2f }, { 0.56f, 0.4f }, { 0.24f, 0.4f } }, x);
        }

        // The letters, 1 unit tall, upright: [0] is P, R and "-M", [1] is the 7.
        std::pair<juce::Path, juce::Path> letterPaths()
        {
            juce::Path pr, seven;
            pr.setUsingNonZeroWinding (false);   // so the bowls' counters are holes

            auto x = 0.0f;
            addBowl (pr, x);
            x += letterWidths[0] + spacing;

            addBowl (pr, x);
            addPolygon (pr, { { 0.4f, 0.6f }, { 0.64f, 0.6f }, { 0.84f, 1.0f }, { 0.6f, 1.0f } }, x);   // R's leg
            x += letterWidths[1] + spacing;

            addPolygon (seven, { { 0.0f, 0.0f }, { 0.8f, 0.0f }, { 0.8f, 0.2f }, { 0.44f, 1.0f }, { 0.18f, 1.0f },
                                 { 0.5f, 0.2f }, { 0.0f, 0.2f } }, x);
            x += letterWidths[2] + spacing;

            // "-M" (mastering): a dash at mid height, clear of the speed
            // cuts, and an M with the same 0.24 stems as the P and R.
            addPolygon (pr, { { 0.0f, 0.42f }, { 0.4f, 0.42f }, { 0.4f, 0.6f }, { 0.0f, 0.6f } }, x);
            x += letterWidths[3] + spacing;

            addPolygon (pr, { { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.26f, 0.0f }, { 0.49f, 0.36f }, { 0.72f, 0.0f },
                              { 0.98f, 0.0f }, { 0.98f, 1.0f }, { 0.74f, 1.0f }, { 0.74f, 0.42f }, { 0.56f, 0.7f },
                              { 0.42f, 0.7f }, { 0.24f, 0.42f }, { 0.24f, 1.0f } }, x);

            // Lean them forward: the top moves right, the baseline stays put.
            const auto lean = juce::AffineTransform (1.0f, -slant, slant, 0.0f, 1.0f, 0.0f);
            pr.applyTransform (lean);
            seven.applyTransform (lean);
            return { pr, seven };
        }
    }

    float widthFor (float height) noexcept
    {
        return height * totalWidth;
    }

    juce::Rectangle<float> drawMark (juce::Graphics& g, juce::Rectangle<float> area, juce::Justification justification, const Style& style)
    {
        const auto height = juce::jmin (area.getHeight(), area.getWidth() / totalWidth);
        const auto bounds = justification.appliedToRectangle (juce::Rectangle<float> (widthFor (height), height), area);

        static const auto letters = letterPaths();
        const auto place = juce::AffineTransform::scale (height).translated (bounds.getX(), bounds.getY());
        auto pr = letters.first, seven = letters.second;
        pr.applyTransform (place);
        seven.applyTransform (place);

        if (style.glow)
        {
            // A soft neon halo behind the 7, wider and fainter outwards.
            for (int ring = 3; ring >= 1; --ring)
            {
                g.setColour (style.seven.withAlpha (0.1f * (float) (4 - ring)));
                g.strokePath (seven, juce::PathStrokeType (height * 0.07f * (float) ring, juce::PathStrokeType::curved));
            }
        }

        juce::Graphics::ScopedSaveState saved (g);
        for (const auto& [top, bottom] : cuts)
            g.excludeClipRegion (juce::Rectangle<float> (bounds.getX() - height, bounds.getY() + height * top,
                                                         bounds.getWidth() + 2.0f * height, height * (bottom - top))
                                     .getSmallestIntegerContainer());

        if (! style.highlight.isTransparent())
        {
            const auto down = juce::AffineTransform::translation (0.0f, juce::jmax (1.0f, height * 0.04f));
            g.setColour (style.highlight);
            g.fillPath (pr, down);
            g.fillPath (seven, down);
        }

        g.setColour (style.letters);
        g.fillPath (pr);
        g.setColour (style.seven);
        g.fillPath (seven);

        if (! style.outline.isTransparent())
        {
            g.setColour (style.outline);
            const juce::PathStrokeType stroke (style.outlineWidth, juce::PathStrokeType::mitered);
            g.strokePath (pr, stroke);
            g.strokePath (seven, stroke);
        }
        return bounds;
    }
}
