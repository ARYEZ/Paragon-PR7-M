#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The LED readout under the VU meter: the output's short-term loudness
// (the last 3 seconds, LUFS) on the left and its highest true peak (dBTP)
// on the right, in the family's seven-segment digits. The true peak turns
// red above -1 dBTP, where streaming services ask a master to stay below.
// A click clears the true peak.
class LoudnessDisplay final : public juce::Component,
                              public juce::SettableTooltipClient
{
public:
    LoudnessDisplay();

    // LUFS (below -70 shows dashes) and dBTP (below -70 shows dashes).
    void setReadings (float lufs, float truePeakDb);
    float getLufs() const noexcept { return lufs; }
    float getTruePeakDb() const noexcept { return truePeakDb; }

    std::function<void()> onClear;

    static constexpr float lowest = -70.0f;
    static constexpr float truePeakWarning = -1.0f;

    // The digits a reading shows: "-14.2", "0.3", or "--.-" when too low.
    static juce::String format (float value);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    float lufs = -100.0f, truePeakDb = -100.0f;
};
