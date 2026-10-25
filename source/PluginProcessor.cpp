#include "PluginProcessor.h"

#include "PluginEditor.h"

ParagonPR7MAudioProcessor::ParagonPR7MAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", Parameters::createLayout()),
      inputGainDb (parameters.getRawParameterValue (ParamIDs::inputGain)),
      outputGainDb (parameters.getRawParameterValue (ParamIDs::outputGain))
{
}

bool ParagonPR7MAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo in and out, or mono in and out: the same on both sides.
    const auto output = layouts.getMainOutputChannelSet();
    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == output;
}

void ParagonPR7MAudioProcessor::prepareToPlay (double sampleRate, int)
{
    for (auto* gain : { &inputGain, &outputGain })
        gain->reset (sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputGainDb->load()));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputGainDb->load()));
}

namespace
{
    // Applies a gain that may be gliding, to every channel. At exactly 1 it
    // does nothing, so the defaults pass audio through untouched.
    void applyGain (juce::AudioBuffer<float>& buffer, juce::SmoothedValue<float>& gain, int numSamples) noexcept
    {
        if (! gain.isSmoothing())
        {
            if (! juce::approximatelyEqual (gain.getTargetValue(), 1.0f))
                buffer.applyGain (0, numSamples, gain.getTargetValue());
            return;
        }
        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = gain.getNextValue();
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.getWritePointer (channel)[i] *= value;
        }
    }
}

void ParagonPR7MAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();
    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    inputGain.setTargetValue (juce::Decibels::decibelsToGain (inputGainDb->load()));
    applyGain (buffer, inputGain, numSamples);
    pushMeters (inputMeters, buffer, numSamples);

    // The mastering modules will go here.

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputGainDb->load()));
    applyGain (buffer, outputGain, numSamples);
    pushMeters (outputMeters, buffer, numSamples);
}

void ParagonPR7MAudioProcessor::pushMeters (std::array<LevelMeterSource, 2>& meters, const juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    for (int channel = 0; channel < 2; ++channel)
        meters[(size_t) channel].pushSamples (buffer.getReadPointer (juce::jmin (channel, buffer.getNumChannels() - 1)), numSamples);
}

juce::AudioProcessorEditor* ParagonPR7MAudioProcessor::createEditor()
{
    return new ParagonPR7MAudioProcessorEditor (*this);
}

void ParagonPR7MAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destination);
}

void ParagonPR7MAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParagonPR7MAudioProcessor();
}
