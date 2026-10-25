#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "LevelMeterSource.h"

// The channel strip's controls, drawn as on the concept (docs/concept.png).
// Each is placed on MainView's canvas by its centre or its rectangle, read
// straight off the concept.

// A black knob seen from the front, standing in a glowing orange ring that's
// open at the bottom, with a white pointer. Its scale is printed round it,
// with a unit ("Hz") and a caption ("THRESHOLD") under it. A slider covers
// the whole component and does the work (dragging, double-click to reset,
// parameter attachments); the knob draws itself from the slider's position.
class Knob final : public juce::Component
{
public:
    // `scale` is printed evenly round the knob's travel, first at the
    // bottom left, last at the bottom right (see setScale for marks at
    // other positions).
    Knob (std::vector<juce::String> scale, juce::String unit, juce::String caption);

    // Places the knob: `ringRadius` is the outer radius of its ring, as
    // measured off the concept. Leaves room round it for the scale.
    void place (juce::Point<float> centre, float ringRadius);

    // Puts each mark at its own place on the travel (0 to 1), for a knob
    // whose scale isn't evenly spaced (INPUT's 0 dB sits at the top).
    void setScale (std::vector<std::pair<float, juce::String>> marks);
    // Prints `text` beside the knob at that place on the travel, without a
    // tick ("Q" beside the Q knobs).
    void addLabel (float proportion, juce::String text);

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };

    void paint (juce::Graphics&) override;
    void resized() override;

    // The usual travel: from the bottom left (7:30) round to the bottom right (4:30).
    static constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float endAngle = juce::MathConstants<float>::pi * 2.75f;

    // A shorter travel, in radians clockwise from 12 o'clock (the small Q
    // knobs turn from 9 o'clock to 3, with their scale round the top). The
    // ring of light follows it.
    void setTravel (float start, float end);

    // The knob itself, for anything else that wants one.
    static void drawKnob (juce::Graphics&, juce::Point<float> centre, float ringRadius, float angle,
                          float travelStart = startAngle, float travelEnd = endAngle);

private:
    void printBeside (juce::Graphics&, float proportion, const juce::String& text) const;

    std::vector<std::pair<float, juce::String>> marks, labels;
    juce::String unit, caption;
    juce::Point<float> knobCentre;   // in this component
    float radius = 30.0f;
    float travelStart = startAngle, travelEnd = endAngle;
};

// A module's status lamp (or COMP's AUTO): a glowing red-orange lens when on.
class Led final : public juce::ToggleButton
{
public:
    Led() { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    static void drawLed (juce::Graphics&, juce::Rectangle<float> area, bool on);
};

// A bat-handle switch in a slot, with two or three positions (top to
// bottom). Clicking moves it to the next position, wrapping round.
class LeverSwitch final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    // `glowWhenUp` gives the handle an orange glowing tip in the top
    // position (LINEAR), as on the concept.
    LeverSwitch (int numPositions, int initialPosition, bool glowWhenUp = false);

    int getPosition() const noexcept { return position; }
    void setPosition (int newPosition);
    std::function<void()> onChange;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int numPositions, position;
    bool glowWhenUp;
};

// A round chrome push button (the meter window's peak-hold reset).
class RoundButton final : public juce::Button
{
public:
    RoundButton() : juce::Button ({}) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// A dynamics module's gain-reduction meter: LED segments lit by how much
// the level is turned down. Vertical ones (the meter window's GR / COMP /
// LIMIT) light from the bottom up; the horizontal "gain change" one in COMP
// lights left from its centre for reduction (and right for make-up gain).
class GainReductionMeter final : public juce::Component,
                                 private juce::Timer
{
public:
    enum class Style { vertical, gainChange };
    GainReductionMeter (GainReductionSource&, Style);

    void paint (juce::Graphics&) override;

    static constexpr float rangeDb = 20.0f;
    float getDisplayedReduction() const noexcept { return displayedDb; }

    // A gain-change meter's make-up gain (dB), lit rightwards from the centre.
    void setMakeUpSource (const std::atomic<float>* makeUpDb) noexcept { makeUp = makeUpDb; }

private:
    void timerCallback() override;

    GainReductionSource& source;
    Style style;
    float displayedDb = 0.0f, displayedMakeUp = 0.0f;
    const std::atomic<float>* makeUp = nullptr;
};
