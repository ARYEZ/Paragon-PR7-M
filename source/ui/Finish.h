#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The unit's finish: the original pearl white with black keys, or pearl
// white or pearl black with neon keys and knobs (pink to red to orange). Only the unit itself changes (its chassis, the expression
// pedal's plate, the lettering printed on it, its keys and knobs); the
// screens, the neon and the dark panels round it stay as they are.
//
// It's a preference of the person, not part of a project or a rig: kept in a
// small settings file in the user's application data folder, so every
// PR7-M opened afterwards, in any project, uses the same finish. Every open
// window repaints when it changes.
namespace Finish
{
    // Saved by number: only ever add to the end.
    enum class Type { pearlWhite, pearlBlack, pearlWhiteNeon };
    inline const juce::StringArray names { "Pearl White", "Pearl Black", "Pearl White + Neon" };

    Type get();
    // `remember` saves it to the settings file (off for tests and snapshots).
    void set (Type, bool remember = true);
    inline bool isBlack() { return get() == Type::pearlBlack; }          // the chassis colour
    inline bool neonKeys() { return get() != Type::pearlWhite; }         // the keys, knobs and pedal grips

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

    // The pearl black unit's keys, knobs and pedal grips: hot pink at the
    // left through red to orange at the right (the reference swatch's
    // colours, 25% darker), across `left` to `right`. Lighting is drawn over it.
    juce::ColourGradient neonGradient (float left, float right);

    // Picks the colour for the current finish.
    inline juce::Colour pick (juce::uint32 onWhite, juce::uint32 onBlack) { return juce::Colour (isBlack() ? onBlack : onWhite); }
}
