#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The faceplate's finish: OBSIDIAN (pearl black, the default) or AURORA
// (pearl white, drawn 10% less bright than full white). On both the knobs'
// caps and the keys are black, and the knobs' rings of light and the keys'
// surrounds carry the neon, pink to red to orange across. Only the plate
// changes (its pearl and the lettering printed on it); the VU meter, the
// readouts, the knobs and the dark frame round it stay as they are.
//
// It's a preference of the person, not part of a project or a rig: kept in a
// small settings file in the user's application data folder, so every
// PR7-M opened afterwards, in any project, uses the same finish. Every open
// window repaints when it changes.
namespace Finish
{
    // Saved by number: only ever add to the end. 0 was Pearl White (retired
    // in 0.9.0: a saved one opens as Obsidian), 1 Pearl Black (now OBSIDIAN),
    // 2 Pearl White + Neon (now AURORA).
    enum class Type { retiredPearlWhite, obsidian, aurora };
    // The finishes offered, in the order they're shown.
    inline constexpr Type choices[] { Type::obsidian, Type::aurora };
    juce::String nameOf (Type);

    Type get();
    // `remember` saves it to the settings file (off for tests and snapshots).
    // The retired Pearl White is set as Obsidian.
    void set (Type, bool remember = true);
    inline bool isBlack() { return get() != Type::aurora; }              // the plate's colour

    // Told whenever the finish changes, to repaint.
    juce::ChangeBroadcaster& changes();

    // Colours that follow the finish.
    juce::Colour ink();          // lettering printed on the unit: near black on white, pearl white on black
    juce::Colour inkDim();
    juce::Colour stampEdge();    // the fine edge under stamped lettering: a light catch on white, a shadow on black
    juce::Colour printedRed();   // the red lettering and lines (a touch brighter on black, so it reads as clearly)

    // The satin grain laid over pearl surfaces: a fine speckle with no
    // direction to it (so no brushed lines) and the odd bright mica fleck.
    // On black the flecks show more, as they do in a real black pearl paint.
    juce::Image makeSatinGrain (int width, int height, int seed);

    // The neon of the knobs' rings and the keys' surrounds: hot pink at the
    // left through red to orange at the right (the reference swatch's
    // colours, 25% darker), across `left` to `right`. Lighting is drawn over it.
    juce::ColourGradient neonGradient (float left, float right);

    // AURORA's pearl is darkened by this much (the owner, for the whole PR7
    // family: full white was "too bright, almost disorienting"): every pixel
    // of the faceplate's pearl, its sheen and highlights included, is drawn at
    // 90% of its old value, so pure white becomes 230/255. The lettering, the
    // neon, the knobs, the VU meter and the readouts keep their own colours.
    inline constexpr float auroraDimming = 0.10f;

    // Darkens a pearl surface just drawn, inside `area`, by auroraDimming on
    // Aurora (black laid over it: each colour times 1 - auroraDimming); nothing on Obsidian.
    void dimPearl (juce::Graphics&, const juce::Path& area);
    // A colour of Aurora's pearl as drawn (times 1 - auroraDimming); unchanged on Obsidian.
    juce::Colour pearl (juce::Colour);

    // Picks the colour for the current finish.
    inline juce::Colour pick (juce::uint32 onWhite, juce::uint32 onBlack) { return juce::Colour (isBlack() ? onBlack : onWhite); }
}
