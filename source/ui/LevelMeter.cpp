#include "LevelMeter.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include "ParagonLookAndFeel.h"

namespace
{
    constexpr int refreshHz = 30;
    constexpr float fallDbPerSecond = 24.0f;   // how fast the meter drops after a peak
    constexpr auto numMarks = (int) std::size (LevelMeter::scaleMarksDb);
}

LevelMeter::LevelMeter (LevelMeterSource& sourceToUse) : source (sourceToUse)
{
    setOpaque (true);   // fills the meter glass itself, so updating it doesn't redraw the panel behind
    startTimerHz (refreshHz);
}

float LevelMeter::proportionForDb (float db) noexcept
{
    db = juce::jlimit (minDb, maxDb, db);
    for (int i = numMarks - 1; i > 0; --i)
    {
        const auto low = scaleMarksDb[i], high = scaleMarksDb[i - 1];
        if (db <= high)
            return ((float) (numMarks - 1 - i) + (db - low) / (high - low)) / (float) (numMarks - 1);
    }
    return 1.0f;
}

float LevelMeter::dbForProportion (float proportion) noexcept
{
    const auto position = juce::jlimit (0.0f, 1.0f, proportion) * (float) (numMarks - 1);
    const auto below = juce::jmin ((int) position, numMarks - 2);
    const auto low = scaleMarksDb[numMarks - 1 - below], high = scaleMarksDb[numMarks - 2 - below];
    return low + (high - low) * (position - (float) below);
}

void LevelMeter::resetPeakHold()
{
    heldDb = minDb;
    overloaded = false;
    repaint();
}

void LevelMeter::timerCallback()
{
    const auto peak = source.takePeak();
    overloaded = overloaded || peak > 1.0f;
    const auto peakDb = juce::jmax (minDb, juce::Decibels::gainToDecibels (peak, -200.0f) - dbfsAtZero);
    const auto fallenDb = displayedDb - fallDbPerSecond / (float) refreshHz;
    const auto newDb = juce::jmax (peakDb, fallenDb, minDb);
    const auto newHeld = juce::jmax (heldDb, peakDb);

    if (! juce::approximatelyEqual (newDb, displayedDb) || ! juce::approximatelyEqual (newHeld, heldDb))
    {
        displayedDb = newDb;
        heldDb = newHeld;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff050405));   // the meter window's glass

    constexpr auto pitch = 3.0f;   // one segment every 3 px, as on the concept
    const auto bounds = getLocalBounds().toFloat();
    const auto count = (int) (bounds.getHeight() / pitch);
    const auto litProportion = proportionForDb (displayedDb);
    const auto heldSegment = heldDb > minDb ? juce::jlimit (0, count - 1, (int) (proportionForDb (heldDb) * (float) count)) : -1;

    for (int i = 0; i < count; ++i)
    {
        const auto proportion = ((float) i + 0.5f) / (float) count;
        const auto colour = ParagonColours::meterColour (proportion);
        const auto lit = proportion <= litProportion || i == heldSegment;
        const auto segment = juce::Rectangle<float> (bounds.getX(), bounds.getBottom() - (float) (i + 1) * pitch + 0.6f, bounds.getWidth(), pitch - 1.2f);

        if (lit)
        {
            g.setColour (colour.withAlpha (0.25f));
            g.fillRect (segment.expanded (1.0f, 0.6f));
        }
        g.setColour (lit ? colour : colour.withAlpha (0.1f));
        g.fillRect (segment);
    }
}
