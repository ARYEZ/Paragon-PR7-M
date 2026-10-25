#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The VU meter in the middle of the panel: a cream face with a scale from
// -20 to +3 VU (the red zone from 0 up), a needle, and its glass. It shows
// the output level, with the level 0 VU stands for printed small on the
// face. It only draws: the needle's movement (VU ballistics) is worked out
// in the processor (metering::VuBallistics).
class VuMeter final : public juce::Component
{
public:
    VuMeter() { setInterceptsMouseClicks (false, false); }

    // Where the needle points, in VU (-20 to +3).
    void setNeedle (float vu);
    float getNeedle() const noexcept { return needle; }

    // The level 0 VU stands for (dBFS), printed on the face.
    void setReference (int dBFS);
    int getReference() const noexcept { return reference; }

    void paint (juce::Graphics&) override;

    static constexpr float minVu = -20.0f, maxVu = 3.0f;
    static constexpr float scaleMarks[] = { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };

private:
    float needle = minVu;
    int reference = -14;
};
