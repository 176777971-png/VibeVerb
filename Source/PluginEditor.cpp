#include "PluginProcessor.h"
#include "PluginEditor.h"

// ── LookAndFeel ─────────────────────────────────────────────

VibeVerbLookAndFeel::VibeVerbLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, bgColour);
    setColour(juce::Slider::rotarySliderFillColourId, accentColour);
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff333340));
    setColour(juce::Slider::thumbColourId, knobColour);
    setColour(juce::Slider::textBoxTextColourId, textColour);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void VibeVerbLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    jassert(startAngle < endAngle); (void) slider;

    auto radius = juce::jmin(w, h) * 0.38f;
    auto cx = x + w * 0.5f;
    auto cy = y + h * 0.5f;
    auto rw = radius * 0.15f;

    // Arc = (+sin θ, -cos θ). Matches pointer (upward needle × rotation(θ)).
    auto pointOnArc = [&](float angle, float r) -> juce::Point<float> {
        return { cx + r * std::sin(angle), cy - r * std::cos(angle) };
    };

    // Draw an arc segment using sampled lineTo (avoids addCentredArc coordinate confusion)
    auto drawArcSegment = [&](float fromAngle, float toAngle, juce::Colour colour, float arcRadius) {
        juce::Path p;
        p.startNewSubPath(pointOnArc(fromAngle, arcRadius));
        constexpr int steps = 64;
        float range = toAngle - fromAngle;
        for (int i = 1; i <= steps; ++i)
            p.lineTo(pointOnArc(fromAngle + range * i / steps, arcRadius));
        g.setColour(colour);
        g.strokePath(p, juce::PathStrokeType(rw));
    };

    // Outer track ring (dark)
    drawArcSegment(startAngle, endAngle, juce::Colour(0xff2a2a35), radius);

    // Filled arc — skip when value ≈ 0
    auto toAngle = startAngle + sliderPos * (endAngle - startAngle);
    if (sliderPos > 0.001f)
        drawArcSegment(startAngle, toAngle, accentColour, radius);

    // Pointer — needle points UP (12 o'clock) + rotation(toAngle):
    //   toAngle=0     → 12 o'clock
    //   toAngle=π/2   → 3 o'clock (CW)
    //   (+sin θ, -cos θ) — same as arc
    juce::Path needle;
    needle.addRectangle(-rw * 0.35f, -radius * 0.78f, rw * 0.7f, radius * 0.6f);
    needle.applyTransform(juce::AffineTransform::rotation(toAngle).translated(cx, cy));
    g.setColour(knobColour);
    g.fillPath(needle);
}

void VibeVerbLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
    const juce::Colour&, bool isMouseOver, bool isDown)
{
    auto& tb = dynamic_cast<juce::TextButton&>(button);
    auto area = button.getLocalBounds().toFloat().reduced(2.0f);

    if (tb.getToggleState())
    {
        g.setColour(accentColour);
        g.fillRoundedRectangle(area, 4.0f);
    }
    else if (isDown)
    {
        g.setColour(accentColour.withAlpha(0.6f));
        g.fillRoundedRectangle(area, 4.0f);
    }
    else if (isMouseOver)
    {
        g.setColour(juce::Colour(0xff3a3a48));
        g.fillRoundedRectangle(area, 4.0f);
    }
    else
    {
        g.setColour(juce::Colour(0xff22222c));
        g.fillRoundedRectangle(area, 4.0f);
    }

    g.setColour(juce::Colour(0xff333340));
    g.drawRoundedRectangle(area, 4.0f, 1.0f);
}

void VibeVerbLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
    bool, bool)
{
    g.setColour(button.getToggleState() ? juce::Colours::black : textColour);
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
}

juce::Font VibeVerbLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::FontOptions(juce::jmin(28.0f, buttonHeight * 0.5f))).withExtraKerningFactor(0.08f);
}

// ── Level Meter (stereo) ───────────────────────────────────

LevelMeter::LevelMeter(const juce::String& label) : labelText(label)
{
    startTimerHz(24);
}

void LevelMeter::setLevels(float l, float r)
{
    levelL = juce::jlimit(-60.0f, 6.0f, l);
    levelR = juce::jlimit(-60.0f, 6.0f, r);
}

void LevelMeter::timerCallback()
{
    repaint();
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    int w = bounds.getWidth();
    int barAreaH = bounds.getHeight() - 20;  // reserve 20px for label below
    int barH = barAreaH;
    int halfW = w / 2 - 2;
    int barW = halfW - 1;

    auto norm = [](float db) { return juce::jmap(db, -60.0f, 6.0f, 0.0f, 1.0f); };

    // Backgrounds (L + R)
    g.setColour(juce::Colour(0xff1a1a22));
    g.fillRect(0, 0, barW, barH);
    g.fillRect(halfW + 2, 0, barW, barH);

    // L bar (green for IN, blue for OUT)
    int barHL = juce::roundToInt(norm(levelL) * barH);
    g.setColour(labelText == "In" ? juce::Colour(0xff4a8c5c) : juce::Colour(0xff6c8cff));
    g.fillRect(0, barH - barHL, barW, barHL);

    // R bar
    int barHR = juce::roundToInt(norm(levelR) * barH);
    g.fillRect(halfW + 2, barH - barHR, barW, barHR);

    // Label below bars
    g.setColour(juce::Colour(0xff666670));
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(labelText, 0, barAreaH, w, 20, juce::Justification::centred, false);
}

// ── Editor ─────────────────────────────────────────────────

VibeVerbAudioProcessorEditor::VibeVerbAudioProcessorEditor(VibeVerbAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), meterIn("In"), meterOut("Out")
{
    setLookAndFeel(&laf);
    setSize(880, 520);

    // ── Mode buttons (left→right: Room, Plate, Hall) ──
    // Pure manual control — no ClickingTogglesState, no radio group, no ButtonAttachment.
    // Single click fires onClick which does everything: set button states + update parameter.
    auto setupModeBtn = [&](juce::TextButton& btn, const juce::String& text, float paramValue)
    {
        btn.setButtonText(text);
        btn.onClick = [this, paramValue] {
            btnRoom.setToggleState(paramValue == 0.0f, juce::dontSendNotification);
            btnPlate.setToggleState(paramValue == 0.5f, juce::dontSendNotification);
            btnHall.setToggleState(paramValue == 1.0f, juce::dontSendNotification);
            processor.apvts.getParameter("mode")->setValueNotifyingHost(paramValue);
        };
        addAndMakeVisible(btn);
    };

    // Room=0, Plate=1, Hall=2  (matches StringArray{"Room","Plate","Hall"})
    setupModeBtn(btnRoom,  "Room",  0.0f);
    setupModeBtn(btnPlate, "Plate", 0.5f);
    setupModeBtn(btnHall,  "Hall",  1.0f);

    btnRoom.setToggleState(true, juce::dontSendNotification);

    // ── Knobs ──
    setupKnob(knobSize,      labelSize,      "Size");
    setupKnob(knobDecay,     labelDecay,     "Decay");
    setupKnob(knobPreDelay,  labelPreDelay,  "Pre-Dly");
    setupKnob(knobTone,      labelTone,      "Tone");
    setupKnob(knobMix,       labelMix,       "Mix");

    // Attach parameters
    sizeAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "size", knobSize);
    decayAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "decay", knobDecay);
    preAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "predelay", knobPreDelay);
    toneAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "tone", knobTone);
    mixAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "mix", knobMix);

    // ── Level meters (In left, Out right) ──
    addAndMakeVisible(meterIn);
    addAndMakeVisible(meterOut);
}

void VibeVerbAudioProcessorEditor::setupKnob(juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 100, 32);
    s.setColour(juce::Slider::textBoxTextColourId, laf.textColour);
    s.setTextValueSuffix("");

    // Explicit 270° rotary range (gap at bottom-left)
    juce::Slider::RotaryParameters rp;
    rp.startAngleRadians = juce::MathConstants<float>::pi * 1.25f;   // 225° (7:30)
    rp.endAngleRadians   = juce::MathConstants<float>::pi * 2.75f;   // 495°→135° (4:30 clock)
    rp.stopAtEnd = true;
    s.setRotaryParameters(rp);

    addAndMakeVisible(s);

    l.setText(name, juce::dontSendNotification);
    l.setFont(juce::Font(juce::FontOptions(20.0f)));
    l.setColour(juce::Label::textColourId, laf.textColour);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

void VibeVerbAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(laf.bgColour);

    // Title
    g.setColour(juce::Colour(0xff888890));
    g.setFont(juce::Font(juce::FontOptions(20.0f)).withExtraKerningFactor(0.15f));
    g.drawText("VIBEVERB", getLocalBounds().removeFromTop(40).withTrimmedLeft(16),
               juce::Justification::topLeft, false);

    // Version
    g.setColour(juce::Colour(0xff555560));
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText("v1.0.0", getLocalBounds().removeFromTop(40).withTrimmedRight(20),
               juce::Justification::topRight, false);

    // Update meter levels
    auto& reverb = processor.getReverbEngine();
    meterIn.setLevels(reverb.getInputLevelL(), reverb.getInputLevelR());
    meterOut.setLevels(reverb.getOutputLevelL(), reverb.getOutputLevelR());
}

void VibeVerbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(24);

    // Title row
    area.removeFromTop(40);

    auto modeRow = area.removeFromTop(56);
    int btnW = 130;
    int btnH = 48;
    int totalBtnW = btnW * 3 + 16;
    int startX = area.getX() + (area.getWidth() - totalBtnW) / 2;
    btnRoom.setBounds(startX, 4, btnW, btnH);
    btnPlate.setBounds(startX + btnW + 8, 4, btnW, btnH);
    btnHall.setBounds(startX + btnW * 2 + 16, 4, btnW, btnH);

    area.removeFromTop(24);

    // Knobs + Meters
    int meterW = 48;
    auto meterInArea  = area.removeFromLeft(meterW);
    auto meterOutArea = area.removeFromRight(meterW);
    meterIn.setBounds(meterInArea);
    meterOut.setBounds(meterOutArea);

    // Knobs centered between meters
    int numKnobs = 5;
    int knobW = 140;
    int knobsTotalW = numKnobs * knobW;
    int knobsStartX = area.getX() + (area.getWidth() - knobsTotalW) / 2;

    auto placeKnob = [&](int i, juce::Slider& s, juce::Label& l)
    {
        auto r = juce::Rectangle<int>(knobsStartX + i * knobW, area.getY(), knobW, area.getHeight());
        s.setBounds(r.removeFromTop(area.getHeight() - 32));
        l.setBounds(r.withSizeKeepingCentre(knobW, 28));
    };

    placeKnob(0, knobSize,     labelSize);
    placeKnob(1, knobDecay,    labelDecay);
    placeKnob(2, knobPreDelay, labelPreDelay);
    placeKnob(3, knobTone,     labelTone);
    placeKnob(4, knobMix,      labelMix);
}
