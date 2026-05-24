#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AllPassFilter.h"
#include <array>

/**
  FDN (Feedback Delay Network) reverb engine.
  Single engine, three preset modes (Hall/Room/Plate).
  Signal chain: Pre-Delay → FDN 8-way → All-Pass diffusers → Tone → Mix
*/
class ReverbEngine
{
public:
    enum Mode { Room = 0, Plate = 1, Hall = 2 };

    ReverbEngine() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Parameter setters (called from audio thread)
    void setMode(Mode m);
    void setSize(float v);       // 0..1
    void setDecay(float seconds); // 0.1..10.0
    void setPreDelay(float ms);   // 0..200
    void setTone(float v);        // 0..1 (dark→bright)
    void setMix(float v);         // 0..1

    // Process stereo block in-place
    void processBlock(float* left, float* right, int numSamples);

    // Level metering — stereo L/R (read by GUI)
    float getInputLevelL() const  { return inputLevelL.load(); }
    float getInputLevelR() const  { return inputLevelR.load(); }
    float getOutputLevelL() const { return outputLevelL.load(); }
    float getOutputLevelR() const { return outputLevelR.load(); }

private:
    struct ModePreset
    {
        std::array<float, 8> delayMs;     // FDN delay line lengths
        float apDelays[3] = {};           // all-pass delay times (ms)
        int   numAP = 0;                  // how many all-pass stages
        float toneBase = 8000.0f;         // base cutoff frequency (Hz)
    };

    void applyModePreset();
    void updateFDNGains();
    void updateToneCoeff();
    void updatePreDelayBuffer();

    // Settings
    Mode   mode = Hall;
    float  size = 0.5f;
    float  decay = 2.0f;
    float  preDelayMs = 20.0f;
    float  tone = 0.5f;
    float  mix = 0.3f;

    // Runtime
    double sr = 44100.0;

    // FDN: 8 delay lines
    static constexpr int numLines = 8;
    std::array<std::vector<float>, numLines> fdnBuffer;
    std::array<int, numLines>   fdnDelayLen;
    std::array<int, numLines>   fdnWritePos;
    std::array<float, numLines> fdnGain;

    // Hadamard 8x8 mixing matrix (normalized)
    static constexpr float hadamard8[8][8] = {
        { 0.35355339f,  0.35355339f,  0.35355339f,  0.35355339f,  0.35355339f,  0.35355339f,  0.35355339f,  0.35355339f },
        { 0.35355339f, -0.35355339f,  0.35355339f, -0.35355339f,  0.35355339f, -0.35355339f,  0.35355339f, -0.35355339f },
        { 0.35355339f,  0.35355339f, -0.35355339f, -0.35355339f,  0.35355339f,  0.35355339f, -0.35355339f, -0.35355339f },
        { 0.35355339f, -0.35355339f, -0.35355339f,  0.35355339f,  0.35355339f, -0.35355339f, -0.35355339f,  0.35355339f },
        { 0.35355339f,  0.35355339f,  0.35355339f,  0.35355339f, -0.35355339f, -0.35355339f, -0.35355339f, -0.35355339f },
        { 0.35355339f, -0.35355339f,  0.35355339f, -0.35355339f, -0.35355339f,  0.35355339f, -0.35355339f,  0.35355339f },
        { 0.35355339f,  0.35355339f, -0.35355339f, -0.35355339f, -0.35355339f, -0.35355339f,  0.35355339f,  0.35355339f },
        { 0.35355339f, -0.35355339f, -0.35355339f,  0.35355339f, -0.35355339f,  0.35355339f,  0.35355339f, -0.35355339f }
    };

    // All-pass diffusers — separate L/R chains for stereo width
    AllPassFilter apL[3];
    AllPassFilter apR[3];
    int activeAP = 0;

    // Tone filter (1-pole lowpass per channel)
    float toneB0 = 1.0f;
    float toneZ_L = 0.0f;
    float toneZ_R = 0.0f;

    // Pre-delay
    std::vector<float> preDelayBuffer;
    int preDelayLen = 0;
    int preDelayWritePos = 0;

    // Level metering (smoothed peak, stereo)
    mutable std::atomic<float> inputLevelL{0.0f};
    mutable std::atomic<float> inputLevelR{0.0f};
    mutable std::atomic<float> outputLevelL{0.0f};
    mutable std::atomic<float> outputLevelR{0.0f};
    float inputSmoothL = 0.0f;
    float inputSmoothR = 0.0f;
    float outputSmoothL = 0.0f;
    float outputSmoothR = 0.0f;

    // Mode presets
    ModePreset presets[3];
    void initPresets();

    // Dry buffer (for Mix)
    std::vector<float> dryL, dryR;
};
