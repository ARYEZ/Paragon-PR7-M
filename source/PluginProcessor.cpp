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
      transients (parameters.getRawParameterValue (ParamIDs::transients))
{
    // Until prepareToPlay gives the real sample rate, report the delay at
    // the usual 48 kHz, for hosts that ask before then.
    clipper.prepare (48000.0, 512);
    setLatencySamples (clipper.getLatency() + effects::TruePeakLimiter::latencyFor (48000.0));
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
    tone.prepare (sampleRate);
    tone.setImmediately (air->load(), focus->load(), body->load());
    punch.prepare (sampleRate);
    punch.setPunchImmediately (bassPunch->load());
    transientShaper.prepare (sampleRate);
    transientShaper.setAmountImmediately (transients->load());
    compressor.prepare (sampleRate);
    compressor.setPushImmediately (push->load());
    clipper.prepare (sampleRate, samplesPerBlock);
    clipper.setVolumeImmediately (volume->load());
    limiter.prepare (sampleRate);   // its own gain stays at 0 dB: VOLUME's gain is the clipper's
    const auto latency = clipper.getLatency() + limiter.getLatency();
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

    // The modules, in their order (see CLAUDE.md). Still to come: ANALOG
    // and WIDTH (between PUSH and VOLUME).
    tone.setAir (air->load());
    tone.setFocus (focus->load());
    tone.setBody (body->load());
    tone.process (block);

    punch.setPunch (bassPunch->load());
    punch.process (block);

    transientShaper.setAmount (transients->load());
    transientShaper.process (block);

    compressor.setPush (push->load());
    pushReduction.push (compressor.process (block));

    // VOLUME: the gain and the clipper, then the limiter. Its LEDs show
    // both: what the clipper shaves off and what the limiter turns down.
    clipper.setVolume (volume->load());
    const auto shaved = clipper.process (block);
    volumeReduction.push (shaved + limiter.process (block));

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
    for (auto* parameter : AudioProcessor::getParameters())   // the plugin's, not the tree's
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter); ranged != nullptr && ranged->getParameterID() != ParamIDs::bypass)
        {
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
            ranged->endChangeGesture();
        }
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
