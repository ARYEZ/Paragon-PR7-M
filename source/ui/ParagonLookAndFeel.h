#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Colours taken from the mockup: near-black panels with an
// orange accent.
namespace ParagonColours
{
    inline const juce::Colour background { 0xff060407 };   // measured off the mockup: near black, faintly purple
    inline const juce::Colour panel      { 0xff0c0a0d };
    inline const juce::Colour panelEdge  { 0xff27222a };
    inline const juce::Colour accent     { 0xffff7a1a };
    inline const juce::Colour accentDim  { 0xff5a2a0a };
    inline const juce::Colour meterRed   { 0xffff3b2f };
    inline const juce::Colour text       { 0xffe8e8ea };

    // The meters' LEDs, `t` from 0 (the bottom of the scale) to 1 (the top):
    // orange rising through red to hot pink, the neon keys' colours at LED
    // brightness.
    inline juce::Colour meterColour (float t)
    {
        return juce::Colour (0xffff7a1a).interpolatedWith (juce::Colour (0xffff1f5a), juce::jlimit (0.0f, 1.0f, t));
    }
    inline const juce::Colour textDim    { 0xff8a8a92 };

    // The silver pedal unit, and dark "ink" for text printed on it.
    inline const juce::Colour metalLight { 0xfff1edef };
    inline const juce::Colour metalMid   { 0xffd5cfd2 };
    inline const juce::Colour metalDark  { 0xffa59ea2 };
    inline const juce::Colour ink        { 0xff26252a };
    inline const juce::Colour inkDim     { 0xff6b6569 };
    inline const juce::Colour glow       { 0xffff4a3a };   // the red-orange edge light

    // Each block's colour in the signal chain, as in the mockup.
    inline const juce::Colour comp       { 0xff35d6a8 };
    inline const juce::Colour drive      { 0xffffb22e };
    inline const juce::Colour amp        { 0xffff4b3e };
    inline const juce::Colour cab        { 0xffff5d6c };
    inline const juce::Colour modulation { 0xffb86dff };
    inline const juce::Colour delay      { 0xff45b4ff };
    inline const juce::Colour reverb     { 0xff4fe3dd };
    inline const juce::Colour eq         { 0xffff5a2c };
    inline const juce::Colour filterPitch { 0xffb8f040 };   // lime
    inline const juce::Colour glitch     { 0xffff4fd8 };   // hot magenta
    inline const juce::Colour pitch      { 0xffbfe6ff };   // ice blue
    inline const juce::Colour loFi       { 0xffe8d29a };   // old-radio cream   // the plugin's neon orange-red
}

class ParagonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ParagonLookAndFeel();

    // Set this property on a slider to draw it as one of the big black
    // knobs on the pedal face (RIG LEVEL, PRESENCE) instead of the
    // small ringed knob.
    static constexpr const char* pedalKnobProperty = "pedalKnob";

    // Set this on a TextButton to draw it as one of the pedal's small black
    // buttons with an LED stripe (SAVE, BYPASS...). The stripe lights when
    // the button's toggle state is on.
    static constexpr const char* pedalButtonProperty = "pedalButton";

    // Set this on a TextButton to draw it as the mockup's light outlined
    // buttons printed on the metal (VOLUME, WAH / FILTER, MORPH / FX). Its
    // outline and text turn red when its toggle state is on.
    static constexpr const char* outlinedButtonProperty = "outlinedButton";

    // Set this (to a font size) on a TextButton to draw just its text, no
    // box: the header's rig name and tempo, which sit in framed groups.
    static constexpr const char* plainTextProperty = "plainText";

    // Set this on a vertical slider to draw it as a graphic-EQ fader: a thin
    // groove lit from its centre (0 dB) to the cap, and a small black cap.
    static constexpr const char* eqFaderProperty = "eqFader";
    static constexpr int eqFaderThumbRadius = 7;

    // Set this on a slider that something else draws (a Knob draws its own
    // knob and only uses the slider for dragging), so it paints nothing.
    static constexpr const char* invisibleSliderProperty = "invisibleSlider";

    // Set this on a TextButton to draw it as one of the channel strip's dark
    // keys (VCA / OPTO / FET, EXP, SAVE...). When its toggle state is on, its
    // lettering lights orange with a neon line under it (red with
    // alertProperty, for MUTE).
    static constexpr const char* consoleButtonProperty = "consoleButton";
    static constexpr const char* alertProperty = "alert";
    // On a console button: draw a small icon instead of its text. One of
    // "phase" (Ø), "shape" (a shelf curve when on, a bell when off), "left",
    // "right", "dots" or "gear".
    static constexpr const char* iconProperty = "icon";
    // On a console button: an LED that lights with it, left of its text
    // (LINK), or in its middle when it has none (LISTEN, HIGH Q).
    static constexpr const char* ledProperty = "led";
    // On a console button: no key, just its icon (the header's arrows and gear).
    static constexpr const char* bareProperty = "bare";

    // Set this on a vertical slider to draw it as the channel's output
    // fader: a black slot and a chrome cap with a red line across it.
    static constexpr const char* channelFaderProperty = "channelFader";
    static constexpr int channelFaderThumbRadius = 34;   // half the cap's height

    // Every font in the plugin is Inter, built in: semibold for bold text,
    // and the heavy display italic for bold italic (the PARAGON badge).
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    // Toggle buttons are drawn as the mockup's red/orange status LEDs.
    // EQ faders: travel inset by half the cap's height, so the cap's centre
    // reaches exactly the top and bottom of the scale.
    int getSliderThumbRadius (juce::Slider&) override;
    // Drop-down lists: a long choice ("Multi-Tap", "1/8 triplet") squeezes to
    // fit a narrow box rather than being cut off with "...".
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    // The two-thumb MIN / MAX slider: a thin track with round glowing thumbs.
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

private:
    juce::Typeface::Ptr regular, semiBold, blackItalic;

    static void drawPedalKnob (juce::Graphics&, juce::Rectangle<float> bounds, float angle);

public:
    // The mockup's piano-black keys (footswitches, slot blocks, small
    // buttons): a glossy black top set in chamfered edges, the top bevel
    // catching the light, the side bevels reflecting the red glow, and a
    // soft shadow on the metal. `bevel` is the chamfer's width. `neon`
    // draws it pink to red to orange (the pearl black unit's keys).
    static void drawPianoKey (juce::Graphics&, juce::Rectangle<float> area, float radius, float bevel,
                              bool pressed, bool highlighted, bool neon = false);

    // A console button's icon (see iconProperty), in `colour`.
    static void drawIcon (juce::Graphics&, const juce::String& icon, juce::Rectangle<float> area, juce::Colour colour, bool on);

    // The mockup's LED bars: when lit, a neon tube (white-hot core inside a
    // red-orange outline, with a soft glow); when off, a dimmer solid red.
    static void drawNeonBar (juce::Graphics&, juce::Rectangle<float> bar, bool lit);
    // A raised chrome bezel on the metal with a dark well inside it, the way a
    // real unit's footswitches and big keys are mounted. Returns the well,
    // for the key to be drawn in.
    static juce::Rectangle<float> drawChromeBezel (juce::Graphics&, juce::Rectangle<float> bezel, float radius);
    // Text stamped into the metal, with a fine edge underneath (a light
    // catch on pearl white, a shadow on pearl black: Finish::stampEdge).
    static void drawStampedText (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                                 juce::Justification, juce::Colour colour);
};

// JUCE finds fonts through the *default* look-and-feel, so while any plugin
// window is open ours is made the default. Shared by every open window, and
// the default is restored when the last one closes. (The default is per
// plugin binary, so other plugins in the DAW aren't affected.)
struct SharedParagonLookAndFeel
{
    SharedParagonLookAndFeel();
    ~SharedParagonLookAndFeel();

    ParagonLookAndFeel lookAndFeel;
};
