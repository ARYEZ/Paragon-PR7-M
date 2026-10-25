#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The orange LED screen (from PR7-G): a chrome frame, black bezel and dark
// glass with seven-segment digits (PR7-C draws its channel numbers with it)
// displays with it.
struct LedDisplay
{
    // Draws one seven-segment digit ('0'-'9' or '-') in `area`.
    static void drawDigit (juce::Graphics&, juce::juce_wchar digit, juce::Rectangle<float> area, juce::Colour lit);

    // The screen itself: the chrome frame, black bezel and dark glass,
    // returning the glass's area to draw the readout in. `size` scales the
    // frame and bezel.
    static juce::Rectangle<float> paintScreen (juce::Graphics&, juce::Rectangle<float> bounds, float size = 1.0f);
    // What sits in front of the readout: pixel rows, the bezel's shadow and reflections.
    static void paintGlassOverlay (juce::Graphics&, juce::Rectangle<float> glass, float size = 1.0f);

    static inline const juce::Colour ledOrange { 0xffff7a1a };
};
