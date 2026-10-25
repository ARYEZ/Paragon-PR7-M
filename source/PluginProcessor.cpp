#include "PluginProcessor.h"

#include "PluginEditor.h"

ParagonPR7MAudioProcessor::ParagonPR7MAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", Parameters::createLayout()),
      bypass (parameters.getRawParameterValue (ParamIDs::bypass)),
      volume (parameters.getRawParameterValue (ParamIDs::volume)),
      push (parameters.getRawParameterValue (ParamIDs::push)),
      air (parameters.getRawParameterValue (ParamIDs::air)),
      focus (parameters.getRawParameterValue (ParamIDs::focus)),
      body (parameters.getRawParameterValue (ParamIDs::body)),
      bassPunch (parameters.getRawParameterValue (ParamIDs::bassPunch)),
      transients (parameters.getRawParameterValue (ParamIDs::transients)),
      analog (parameters.getRawParameterValue (ParamIDs::analog)),
      width (parameters.getRawParameterValue (ParamIDs::width))
{
    // Until prepareToPlay gives the real sample rate, report the delay at
    // the usual 48 kHz, for hosts that ask before then.
    chain.prepare (48000.0, 512, knobs());
    setLatencySamples (chain.getLatency());
    presets.stateRestored();   // "Default", unmodified
}

bool ParagonPR7MAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo in and out, or mono in and out: the same on both sides.
    const auto output = layouts.getMainOutputChannelSet();
    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == output;
}

void ParagonPR7MAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    chain.prepare (sampleRate, samplesPerBlock, knobs());
    // Listening at another sample rate would mix two recordings up.
    if (assistant.getState() == Assistant::State::listening && ! juce::approximatelyEqual (assistant.getSampleRate(), sampleRate))
        assistant.cancel();
    const auto latency = chain.getLatency();
    setLatencySamples (latency);

    dry.setSize (metering::maxChannels, juce::jmax (1, samplesPerBlock));
    dryDelay.prepare (latency);
    matchedBypass.prepare (sampleRate);
    matchedBypass.setImmediately (bypass->load() >= 0.5f);
    loudness.prepare (sampleRate);
    truePeak.prepare (sampleRate);
    for (auto& needle : vu)
        needle.prepare (sampleRate);
    vuLevel = 0.0f;
    shortTermLufs = metering::ShortTermLoudness::silent;
    truePeakHold.reset();
}

void ParagonPR7MAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();
    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    // A fresh true-peak reading for each play-through.
    if (auto* host = getPlayHead())
        if (const auto position = host->getPosition())
        {
            if (position->getIsPlaying() && ! wasPlaying)
                truePeakHold.reset();
            wasPlaying = position->getIsPlaying();
        }

    pushMeters (inputMeters, buffer, numSamples);
    assistant.capture (buffer.getArrayOfReadPointers(), juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels()), numSamples);

    // In pieces no longer than prepareToPlay said, in case a host sends more.
    const auto block = juce::dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (0, (size_t) juce::jmin (buffer.getNumChannels(), metering::maxChannels));
    for (int start = 0; start < numSamples; start += dry.getNumSamples())
        processChunk (block.getSubBlock ((size_t) start, (size_t) juce::jmin (dry.getNumSamples(), numSamples - start)));

    pushMeters (outputMeters, buffer, numSamples);
    meteredBlocks.fetch_add (1, std::memory_order_relaxed);
}

void ParagonPR7MAudioProcessor::processChunk (juce::dsp::AudioBlock<float> block) noexcept
{
    const auto numChannels = (int) block.getNumChannels(), numSamples = (int) block.getNumSamples();
    auto dryBlock = juce::dsp::AudioBlock<float> (dry).getSubsetChannelBlock (0, (size_t) numChannels).getSubBlock (0, (size_t) numSamples);
    dryBlock.copyFrom (block);
    dryDelay.process (dryBlock);

    // The modules, in their order (see CLAUDE.md).
    chain.set (knobs());
    const auto reduction = chain.process (block);
    pushReduction.push (reduction.push);
    volumeReduction.push (reduction.volume);

    matchedBypass.process (dryBlock, block, bypass->load() >= 0.5f);

    // The metering, of what's heard.
    std::array<const float*, metering::maxChannels> channels {};
    auto needle = 0.0f, peak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        channels[(size_t) channel] = block.getChannelPointer ((size_t) channel);
        needle = juce::jmax (needle, vu[(size_t) channel].process (channels[(size_t) channel], numSamples));
        peak = juce::jmax (peak, truePeak.process (channel, channels[(size_t) channel], numSamples));
    }
    if (loudness.process (channels.data(), numChannels, numSamples))
        shortTermLufs.store (loudness.getLufs(), std::memory_order_relaxed);
    vuLevel.store (needle, std::memory_order_relaxed);
    truePeakHold.push (peak);
}

effects::MasteringChain::Settings ParagonPR7MAudioProcessor::knobs() const noexcept
{
    return { air->load(), focus->load(), body->load(), bassPunch->load(), transients->load(),
             push->load(), analog->load(), width->load(), volume->load() };
}

std::optional<Assistant::Result> ParagonPR7MAudioProcessor::applyAssistant()
{
    assistant.update();
    auto decided = assistant.takeResult();
    if (! decided)
        return decided;
    const auto& s = decided->settings;
    presets.loadValues (juce::String ("Assistant - ") + Assistant::targets[decided->target].name,
                        { { ParamIDs::air, s.air }, { ParamIDs::focus, s.focus }, { ParamIDs::body, s.body },
                          { ParamIDs::bassPunch, s.bassPunch }, { ParamIDs::transients, s.transients }, { ParamIDs::push, s.push },
                          { ParamIDs::analog, s.analog }, { ParamIDs::width, s.width }, { ParamIDs::volume, s.volume } });
    return decided;
}

int ParagonPR7MAudioProcessor::getVuReference() const
{
    const auto value = (int) parameters.state.getProperty ("vuReference", defaultVuReference);
    for (const auto choice : vuReferenceChoices)
        if (value == choice)
            return value;
    return defaultVuReference;
}

void ParagonPR7MAudioProcessor::setVuReference (int dBFS)
{
    parameters.state.setProperty ("vuReference", dBFS, nullptr);
}

void ParagonPR7MAudioProcessor::resetKnobs()
{
    presets.loadFactory (0);   // Default: every knob at its starting point, BYPASS left alone
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
    {
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
        presets.stateRestored();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParagonPR7MAudioProcessor();
}
