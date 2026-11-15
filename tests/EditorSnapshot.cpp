#include <catch2/catch_test_macros.hpp>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/Finish.h"

// Hidden test (the "[.]" tag): renders the editor to a PNG for reviewing the
// UI without a DAW. Run with:
//   PARAGON_SNAPSHOT=editor.png ./ParagonPR7MTests "[snapshot]"
// PARAGON_SNAPSHOT_WIDTH sets the window width (default: the canvas's;
// twice that for a 2x render). PARAGON_SNAPSHOT_FINISH=aurora (or neon or
// white) draws AURORA; otherwise OBSIDIAN, the default. PARAGON_SNAPSHOT_DEMO=1 loads the "Pop - Polished" preset and
// sets the VU needle, the loudness readout and PUSH and VOLUME's LEDs, to
// show them. PARAGON_SNAPSHOT_LISTENING=1 shows the assistant listening.
TEST_CASE ("Render the editor to a PNG", "[.][snapshot]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "editor.png");
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "aurora" || finish == "neon" || finish == "white" ? Finish::Type::aurora : Finish::Type::obsidian, false);

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
            const auto presets = processor.getPresets().getPresets();
            for (const auto& preset : presets)
                if (preset.name == "Pop - Polished")
                    processor.getPresets().load (preset);
        }

    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_LISTENING", {}) == "1")
        if (auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0)))
        {
            processor.startListening (1);
            juce::AudioBuffer<float> song (2, 48000 * 4);
            for (int i = 0; i < song.getNumSamples(); ++i)
                for (int channel = 0; channel < 2; ++channel)
                    song.setSample (channel, i, 0.3f * std::sin ((float) i * 0.05f));
            for (int start = 0; start < song.getNumSamples(); start += 256)
            {
                juce::AudioBuffer<float> block (song.getArrayOfWritePointers(), 2, start, 256);
                processor.processBlock (block, midi);
            }
            view->updateAssistant();
        }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
    juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (path)).deleteFile();
    juce::FileOutputStream stream (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    REQUIRE (stream.openedOk());
    juce::PNGImageFormat png;
    REQUIRE (png.writeImageToStream (image, stream));
}

// Hidden test: the gear menu (the finish chooser and the 0 VU setting) as
// the plugin's look-and-feel draws it. A menu opens in its own window, so
// the editor snapshot can't show it. PARAGON_SNAPSHOT=menu.png.
TEST_CASE ("Render the settings menu to a PNG", "[.][snapshot-menu]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", "menu.png");
    ParagonPR7MAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());   // sets the plugin's look-and-feel
    auto& laf = juce::LookAndFeel::getDefaultLookAndFeel();
    Finish::set (Finish::Type::obsidian, false);

    struct Row { juce::String text; bool header = false, ticked = false; };
    std::vector<Row> rows { { "Finish", true } };
    for (const auto type : Finish::choices)
        rows.push_back ({ Finish::nameOf (type), false, Finish::get() == type });
    rows.push_back ({ "VU meter: 0 VU is", true });
    for (const auto reference : ParagonPR7MAudioProcessor::vuReferenceChoices)
        rows.push_back ({ juce::String (reference) + " dBFS" + (reference == ParagonPR7MAudioProcessor::defaultVuReference ? " (streaming loudness)" : ""),
                          false, reference == processor.getVuReference() });

    constexpr int width = 260, rowHeight = 26, scale = 2;
    const auto height = (int) rows.size() * rowHeight + 8;
    juce::Image image (juce::Image::ARGB, width * scale, height * scale, true);
    {
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale ((float) scale));
        laf.drawPopupMenuBackground (g, width, height);
        auto y = 4;
        for (const auto& row : rows)
        {
            const juce::Rectangle<int> area (0, y, width, rowHeight);
            if (row.header)
                laf.drawPopupMenuSectionHeader (g, area, row.text);
            else
                laf.drawPopupMenuItem (g, area, false, true, false, row.ticked, false, row.text, {}, nullptr, nullptr);
            y += rowHeight;
        }
    }
    juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (path)).deleteFile();
    juce::FileOutputStream stream (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    REQUIRE (stream.openedOk());
    juce::PNGImageFormat png;
    REQUIRE (png.writeImageToStream (image, stream));
}
