#include "ReverbEngine.h"
#include <cmath>

void ReverbEngine::initPresets()
{
    // Hall: long sparse delays, 2 all-pass, bright-ish
    presets[Hall] = {
        { 37.0f, 43.0f, 53.0f, 59.0f, 67.0f, 73.0f, 79.0f, 83.0f },
        { 8.3f, 17.1f, 0.0f },
        2,
        8000.0f
    };

    // Room: short dense delays, 1 all-pass, bright
    presets[Room] = {
        { 13.0f, 17.0f, 19.0f, 23.0f, 27.0f, 31.0f, 35.0f, 37.0f },
        { 5.2f, 0.0f, 0.0f },
        1,
        12000.0f
    };

    // Plate: medium delays, 3 all-pass (high density), darker
    presets[Plate] = {
        { 19.0f, 23.0f, 29.0f, 31.0f, 37.0f, 41.0f, 43.0f, 47.0f },
        { 3.1f, 7.3f, 13.7f },
        3,
        6000.0f
    };
}

void ReverbEngine::prepare(double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    initPresets();

    // FDN buffer: worst case Hall 83ms × Size 1.5 ≈ 125ms, use 150ms margin
    int maxFdnLen = juce::roundToInt(0.15f * (float)sr) + 1;
    for (int i = 0; i < numLines; ++i)
    {
        fdnBuffer[i].assign(maxFdnLen, 0.0f);
        fdnWritePos[i] = 0;
    }

    // Pre-delay buffer: stereo interleaved (L/R pairs), max 200ms → 2×
    int maxPreLen = juce::roundToInt(0.2f * (float)sr) + 1;
    preDelayBuffer.assign(maxPreLen * 2, 0.0f);
    preDelayWritePos = 0;

    // All-pass filters — init both L and R chains
    for (auto& a : apL) a.prepare(sampleRate);
    for (auto& a : apR) a.prepare(sampleRate);

    // Dry buffers
    dryL.resize(maxBlockSize);
    dryR.resize(maxBlockSize);

    applyModePreset();
    updateFDNGains();
    updateToneCoeff();
    updatePreDelayBuffer();

    reset();
}

void ReverbEngine::reset()
{
    for (auto& buf : fdnBuffer)
        std::fill(buf.begin(), buf.end(), 0.0f);
    for (auto& wp : fdnWritePos)
        wp = 0;

    for (auto& a : apL) a.reset();
    for (auto& a : apR) a.reset();

    std::fill(preDelayBuffer.begin(), preDelayBuffer.end(), 0.0f);
    preDelayWritePos = 0;

    toneZ_L = 0.0f;
    toneZ_R = 0.0f;

    inputSmoothL = 0.0f;
    inputSmoothR = 0.0f;
    outputSmoothL = 0.0f;
    outputSmoothR = 0.0f;
    inputLevelL.store(0.0f);
    inputLevelR.store(0.0f);
    outputLevelL.store(0.0f);
    outputLevelR.store(0.0f);
}

// ── Parameter setters ──────────────────────────────────────

void ReverbEngine::setMode(Mode m)
{
    if (mode != m)
    {
        mode = m;
        applyModePreset();
        updateFDNGains();
        updateToneCoeff();  // toneBase changes per mode
    }
}

void ReverbEngine::setSize(float v)
{
    size = juce::jlimit(0.0f, 1.0f, v);
    applyModePreset();  // rescale delays
    updateFDNGains();
}

void ReverbEngine::setDecay(float seconds)
{
    decay = juce::jlimit(0.1f, 10.0f, seconds);
    updateFDNGains();
}

void ReverbEngine::setPreDelay(float ms)
{
    preDelayMs = juce::jlimit(0.0f, 200.0f, ms);
    updatePreDelayBuffer();
}

void ReverbEngine::setTone(float v)
{
    tone = juce::jlimit(0.0f, 1.0f, v);
    updateToneCoeff();
}

void ReverbEngine::setMix(float v)
{
    mix = juce::jlimit(0.0f, 1.0f, v);
}

// ── Internal helpers ───────────────────────────────────────

void ReverbEngine::applyModePreset()
{
    auto& p = presets[mode];
    float scale = 0.5f + size;  // Size 0→1 maps to scale 0.5→1.5

    for (int i = 0; i < numLines; ++i)
    {
        float delayMs = p.delayMs[i] * scale;
        fdnDelayLen[i] = juce::jmax(1, juce::roundToInt(delayMs * 0.001f * (float)sr));
    }

    // All-pass configuration — L/R chains with offset delays for stereo width
    // Clear AP buffers on mode change to prevent stale-data transients
    bool modeChanged = (activeAP != p.numAP);
    activeAP = p.numAP;
    for (int i = 0; i < 3; ++i)
    {
        float delayL = p.apDelays[i] * scale;
        apL[i].setDelayMs(delayL);
        apL[i].setGain(0.618f);
        if (modeChanged) apL[i].reset();

        float delayR = delayL * 0.85f;
        if (delayR < 1.0f) delayR = 1.0f;
        apR[i].setDelayMs(delayR);
        apR[i].setGain(0.618f);
        if (modeChanged) apR[i].reset();
    }
}

void ReverbEngine::updateFDNGains()
{
    // Average delay time in seconds
    float avgDelayS = 0.0f;
    for (int i = 0; i < numLines; ++i)
        avgDelayS += (float)fdnDelayLen[i] / (float)sr;
    avgDelayS /= (float)numLines;

    // feedback = 0.001 ^ (delay / T60)
    float t60 = juce::jmax(0.1f, decay);
    float fb = std::pow(0.001f, avgDelayS / t60);

    for (int i = 0; i < numLines; ++i)
        fdnGain[i] = fb;
}

void ReverbEngine::updateToneCoeff()
{
    // tone 0→1 maps to cutoff 500Hz→toneBase (logarithmic, per-mode preset)
    float fcMax = presets[mode].toneBase;
    float fc = 500.0f * std::pow(fcMax / 500.0f, tone);
    float w = 2.0f * juce::MathConstants<float>::pi * fc / (float)sr;
    toneB0 = 1.0f - std::exp(-w);
    if (toneB0 > 1.0f) toneB0 = 1.0f;
}

void ReverbEngine::updatePreDelayBuffer()
{
    preDelayLen = juce::jmax(0, juce::roundToInt(preDelayMs * 0.001f * (float)sr));
    if (preDelayWritePos >= (int)preDelayBuffer.size())
        preDelayWritePos = 0;
}

// ── Main processing ────────────────────────────────────────

void ReverbEngine::processBlock(float* left, float* right, int numSamples)
{
    // Save dry signal
    for (int n = 0; n < numSamples; ++n)
    {
        dryL[n] = left[n];
        dryR[n] = right[n];
    }

    // Input level metering — true stereo peak (L and R independent)
    float peakInL = 0.0f, peakInR = 0.0f;
    for (int n = 0; n < numSamples; ++n)
    {
        peakInL = std::max(peakInL, std::abs(left[n]));
        peakInR = std::max(peakInR, std::abs(right[n]));
    }

    // Process each sample
    float wetL = 0.0f, wetR = 0.0f;
    float peakOutL = 0.0f, peakOutR = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        // ── Pre-delay (stereo: store L+R as two consecutive samples) ──
        float dryInL = left[n];
        float dryInR = right[n];

        float delayedL = 0.0f, delayedR = 0.0f;
        if (preDelayLen > 0)
        {
            // Read L from even index, R from odd index
            int readPosBase = (preDelayWritePos - preDelayLen * 2);
            if (readPosBase < 0) readPosBase += (int)preDelayBuffer.size();

            int readPosL = readPosBase;
            int readPosR = (readPosL + 1) % (int)preDelayBuffer.size();

            delayedL = preDelayBuffer[readPosL];
            delayedR = preDelayBuffer[readPosR];

            // Write interleaved L/R
            preDelayBuffer[preDelayWritePos] = dryInL;
            preDelayWritePos = (preDelayWritePos + 1) % (int)preDelayBuffer.size();
            preDelayBuffer[preDelayWritePos] = dryInR;
            preDelayWritePos = (preDelayWritePos + 1) % (int)preDelayBuffer.size();
        }
        else
        {
            delayedL = dryInL;
            delayedR = dryInR;
        }

        // ── FDN: read delay lines (with NaN guard) ──
        std::array<float, numLines> lineOut;
        for (int i = 0; i < numLines; ++i)
        {
            float val = fdnBuffer[i][fdnWritePos[i]];
            lineOut[i] = std::isfinite(val) ? val : 0.0f;
        }

        // ── FDN: stereo sum — even lines → L, odd lines → R ──
        // Per-channel norm = 1/sqrt(4) = 0.5, total = 1/sqrt(8) ≈ 0.3535
        constexpr float normFactor = 0.35355339f;
        float fdnSumL = 0.0f;
        float fdnSumR = 0.0f;
        for (int i = 0; i < numLines; ++i)
        {
            if ((i & 1) == 0)  // even → L
                fdnSumL += lineOut[i];
            else                // odd  → R
                fdnSumR += lineOut[i];
        }
        fdnSumL *= normFactor;
        fdnSumR *= normFactor;
        fdnSumL = std::isfinite(fdnSumL) ? juce::jlimit(-8.0f, 8.0f, fdnSumL) : 0.0f;
        fdnSumR = std::isfinite(fdnSumR) ? juce::jlimit(-8.0f, 8.0f, fdnSumR) : 0.0f;

        // ── FDN: compute feedback via Hadamard matrix ──
        std::array<float, numLines> feedback;
        for (int i = 0; i < numLines; ++i)
        {
            float fb = 0.0f;
            for (int j = 0; j < numLines; ++j)
                fb += hadamard8[i][j] * lineOut[j];
            feedback[i] = fb;
        }

        // ── FDN: write back — L→even lines, R→odd lines ──
        float inputScale = 0.25f;
        for (int i = 0; i < numLines; ++i)
        {
            float inputGain = ((i & 1) == 0) ? delayedL : delayedR;
            float val = feedback[i] * fdnGain[i] + inputGain * inputScale;
            fdnBuffer[i][fdnWritePos[i]] = juce::jlimit(-8.0f, 8.0f, val);
            fdnWritePos[i] = (fdnWritePos[i] + 1) % fdnDelayLen[i];
        }

        // ── All-pass diffusion — true stereo: fdnSumL→apL, fdnSumR→apR ──
        float apOutL = fdnSumL;
        float apOutR = fdnSumR;
        for (int i = 0; i < activeAP; ++i)
        {
            apOutL = apL[i].process(apOutL);
            apOutR = apR[i].process(apOutR);
        }

        // ── Tone filter (1-pole lowpass, stereo) ──
        toneZ_L = toneZ_L + toneB0 * (apOutL - toneZ_L);
        toneZ_R = toneZ_R + toneB0 * (apOutR - toneZ_R);

        wetL = toneZ_L;
        wetR = toneZ_R;

        // ── Mix dry/wet ──
        left[n]  = dryL[n] * (1.0f - mix) + wetL * mix;
        right[n] = dryR[n] * (1.0f - mix) + wetR * mix;

        // NaN/Inf guard: if FDN goes unstable, mute output
        if (!std::isfinite(left[n]))  left[n]  = 0.0f;
        if (!std::isfinite(right[n])) right[n] = 0.0f;

        peakOutL = std::max(peakOutL, std::abs(left[n]));
        peakOutR = std::max(peakOutR, std::abs(right[n]));
    }

    // ── Level metering — stereo L/R independent, PPM-style ──
    float attack  = 0.6f;
    float release = 0.002f;

    auto smooth = [attack, release](float& state, float peak) {
        float coeff = (peak > state) ? attack : release;
        state = coeff * peak + (1.0f - coeff) * state;
    };

    smooth(inputSmoothL, peakInL);
    smooth(inputSmoothR, peakInR);
    smooth(outputSmoothL, peakOutL);
    smooth(outputSmoothR, peakOutR);

    auto toDB = [](float v) { return juce::Decibels::gainToDecibels(v + 1e-6f, -60.0f); };
    inputLevelL.store(toDB(inputSmoothL));
    inputLevelR.store(toDB(inputSmoothR));
    outputLevelL.store(toDB(outputSmoothL));
    outputLevelR.store(toDB(outputSmoothR));
}
