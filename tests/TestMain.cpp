#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

int main (int argc, char* argv[])
{
    // JUCE classes such as AudioProcessorValueTreeState expect the message
    // manager to exist, as it does inside a real host.
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    return Catch::Session().run (argc, argv);
}
