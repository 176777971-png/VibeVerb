#include "PluginProcessor.h"
#include "PluginEditor.h"

VibeVerbAudioProcessor::VibeVerbAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "VibeVerbParams", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout VibeVerbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "mode", "Mode", juce::StringArray{"Room", "Plate", "Hall"}, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "size", "Size", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "decay", "Decay", juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.4f), 2.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "predelay", "Pre-Delay", juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f), 20.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(v, 1) + " ms"; },
        [](const juce::String& s) { return s.getFloatValue(); }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "tone", "Tone", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

    return layout;
}

bool VibeVerbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void VibeVerbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverb.prepare(sampleRate, samplesPerBlock);
}

void VibeVerbAudioProcessor::releaseResources()
{
    reverb.reset();
}

void VibeVerbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Read parameters
    auto* modeParam  = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("mode"));
    auto* sizeParam  = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("size"));
    auto* decayParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("decay"));
    auto* preParam   = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("predelay"));
    auto* toneParam  = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("tone"));
    auto* mixParam   = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("mix"));

    reverb.setMode(static_cast<ReverbEngine::Mode>(modeParam->getIndex()));
    reverb.setSize(sizeParam->get());
    reverb.setDecay(decayParam->get());
    reverb.setPreDelay(preParam->get());
    reverb.setTone(toneParam->get());
    reverb.setMix(mixParam->get());

    // Process
    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    reverb.processBlock(left, right, buffer.getNumSamples());
}

juce::AudioProcessorEditor* VibeVerbAudioProcessor::createEditor()
{
    return new VibeVerbAudioProcessorEditor(*this);
}

void VibeVerbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VibeVerbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName("VibeVerbParams"))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// ── Plugin entry point ────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VibeVerbAudioProcessor();
}
