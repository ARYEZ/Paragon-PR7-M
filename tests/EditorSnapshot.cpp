#include <catch2/catch_test_macros.hpp>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/Finish.h"

// Hidden test (the "[.]" tag): renders the editor to a PNG for reviewing the
// UI without a DAW. Run with:
//   PARAGON_SNAPSHOT=editor.png ./ParagonPR7MTests "[snapshot]"
// PARAGON_SNAPSHOT_WIDTH sets the window width (default: the canvas's;
// twice that for a 2x render). PARAGON_SNAPSHOT_FINISH=black or =neon picks
// the finish. PARAGON_SNAPSHOT_DEMO=1 sets the VU needle, the loudness
// readout and the PUSH and VOLUME LEDs, to show them.
TEST_CASE ("Render the editor to a PNG", "[.][snapshot]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "editor.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);

    ParagonPR7MAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    const auto width = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_WIDTH", juce::String (MainView::width)).getIntValue();
    editor->setSize (width, width * MainView::height / MainView::width);

    // Play some signal and let the meters' timers pick it up, so they show.
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    for (int round = 0; round < 8; ++round)
    {
        for (int i = 0; i < 256; ++i)
        {
            buffer.setSample (0, i, 0.5f * std::sin ((float) i * 0.1f));
            buffer.setSample (1, i, 0.3f * std::sin ((float) i * 0.13f));
        }
        processor.processBlock (buffer, midi);
        juce::Timer::callPendingTimersSynchronously();
        juce::Thread::sleep (40);
    }

    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_DEMO", {}) == "1")
        if (auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0)))
        {
            view->getVuMeter().setNeedle (-3.0f);
            view->getLoudnessDisplay().setReadings (-9.8f, -0.3f);
            view->setGainReduction (3.0f, 5.0f);
        }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
    juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (path)).deleteFile();
    juce::FileOutputStream stream (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    REQUIRE (stream.openedOk());
    juce::PNGImageFormat png;
    REQUIRE (png.writeImageToStream (image, stream));
}
