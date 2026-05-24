#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "ReverbEngine.h"

class VibeVerbAudioProcessor : public juce::AudioProcessor
{
public:
    VibeVerbAudioProcessor();
    ~VibeVerbAudioProcessor() override = default;

    // AudioProcessor
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VibeVerb"; }

    bool acceptsMidi() const override    { return false; }
    bool producesMidi() const override    { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram(int) override                   {}
    const juce::String getProgramName(int) override        { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // Parameter access
    juce::AudioProcessorValueTreeState apvts;
    ReverbEngine& getReverbEngine() { return reverb; }

private:
    ReverbEngine reverb;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VibeVerbAudioProcessor)
};
