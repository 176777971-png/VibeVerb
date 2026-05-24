#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
  First-order all-pass filter with M-sample delay.
  Transfer function: H(z) = (-g + z^{-M}) / (1 - g*z^{-M})
  Implementation: Dattorro/Schroeder all-pass diffuser.

  IMPORTANT: Buffer is pre-allocated once in prepare().
  setDelayMs() only changes the effective delay length;
  no heap allocation happens on the audio thread.
*/
class AllPassFilter
{
public:
    AllPassFilter() = default;

    void prepare(double sampleRate)
    {
        jassert(sampleRate > 0.0);
        sr = sampleRate;
        // Pre-allocate for worst case: 30ms at 192kHz ≈ 5760 samples
        int maxDelay = juce::roundToInt(0.03 * sampleRate) + 1;
        buffer.assign(maxDelay, 0.0f);
        delaySamples = juce::jmin(1, maxDelay);
        writePos = 0;
    }

    void setDelayMs(float ms)
    {
        int newDelay = juce::jmax(1, juce::roundToInt(ms * 0.001f * (float)sr));
        // Clamp to pre-allocated buffer size — NO reallocation!
        delaySamples = juce::jmin(newDelay, (int)buffer.size());
        // Wrap write position if it exceeds the new effective delay
        if (writePos >= delaySamples)
            writePos = 0;
    }

    void setGain(float g)
    {
        gain = juce::jlimit(0.0f, 0.99f, g);
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    float process(float input)
    {
        // Dattorro all-pass: v[n] stored in buffer, y[n] = v[n-M] - g*x[n]
        float vDelayed = buffer[writePos];
        float y = vDelayed - gain * input;

        // Soft clamp on BOTH output and buffer state
        y = juce::jlimit(-8.0f, 8.0f, y);
        float v = input + gain * y;
        v = juce::jlimit(-12.0f, 12.0f, v);   // buffer state can be ~1.618×signal

        buffer[writePos] = v;
        writePos = (writePos + 1) % delaySamples;

        return y;
    }

private:
    double sr = 44100.0;
    float gain = 0.618f;
    int delaySamples = 1;
    int writePos = 0;
    std::vector<float> buffer;
};
