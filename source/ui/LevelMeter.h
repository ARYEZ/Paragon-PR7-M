#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "LevelMeterSource.h"

// One channel's peak meter in the meter window: a column of fine LED
// segments, +24 at the top and -60 at the bottom, read like a console's
// meter: 0 is the console's nominal level, 20 dB below digital full scale
// (0 = -20 dBFS), so +20 is full scale. Lit segments are warm white up to 0,
// amber above it (hot, where the console starts to colour), and red past
// +20 (clipping). The highest peak stays lit as a single segment, and an
// overload is remembered, until resetPeakHold().
class LevelMeter final : public juce::Component,
                         private juce::Timer
{
public:
    explicit LevelMeter (LevelMeterSource& source);

    void paint (juce::Graphics&) override;
    void resetPeakHold();

    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 24.0f;
    static constexpr float dbfsAtZero = -20.0f;   // the console calibration: 0 on the scale is -20 dBFS

    // The printed scale, top to bottom. The marks are evenly spaced, as on
    // the concept, so the 12 dB steps at the bottom are squeezed into the
    // same space as the 6 dB steps above them.
    static constexpr float scaleMarksDb[] = { 24.0f, 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f, -48.0f, -60.0f };
    // How far up the meter a level sits (0 at the bottom, 1 at the top), and back.
    static float proportionForDb (float db) noexcept;
    static float dbForProportion (float proportion) noexcept;

    float getDisplayedDb() const noexcept { return displayedDb; }
    float getPeakHoldDb() const noexcept { return heldDb; }
    // True once a peak has gone over digital full scale (0 dBFS), until reset.
    bool hasOverloaded() const noexcept { return overloaded; }

private:
    void timerCallback() override;

    LevelMeterSource& source;
    float displayedDb = minDb, heldDb = minDb;
    bool overloaded = false;
};
