#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// ── Custom LookAndFeel ─────────────────────────────────────
class VibeVerbLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VibeVerbLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float startAngle, float endAngle,
        juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
        const juce::Colour& bg, bool isMouseOver, bool isDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
        bool isMouseOver, bool isDown) override;

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    // Colours
    juce::Colour bgColour   = juce::Colour(0xff141418);
    juce::Colour knobColour = juce::Colour(0xffe0e0e0);
    juce::Colour accentColour = juce::Colour(0xff6c8cff);
    juce::Colour textColour   = juce::Colour(0xffaaaaaa);
};

// ── Level Meter (stereo L/R) ──────────────────────────────
class LevelMeter : public juce::Component, public juce::Timer
{
public:
    LevelMeter(const juce::String& label);
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    void setLevels(float l, float r);

private:
    float levelL = -60.0f, levelR = -60.0f;
    juce::String labelText;
};

// ── Editor ─────────────────────────────────────────────────
class VibeVerbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VibeVerbAudioProcessorEditor(VibeVerbAudioProcessor&);
    ~VibeVerbAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    VibeVerbAudioProcessor& processor;

    VibeVerbLookAndFeel laf;

    // Mode buttons
    juce::TextButton btnHall, btnRoom, btnPlate;

    // Knobs
    juce::Slider knobSize, knobDecay, knobPreDelay, knobTone, knobMix;
    juce::Label  labelSize, labelDecay, labelPreDelay, labelTone, labelMix;

    // Level meter
    LevelMeter meterIn, meterOut;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sizeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach;

    void setupKnob(juce::Slider& s, juce::Label& l, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VibeVerbAudioProcessorEditor)
};
