#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <array>

//==============================================================================
// NCS: DriftEngine
// Per-voice analog drift: pitch, operator level, and envelope time variations.
// Apply before each voice render call. Zero cost when disabled.
//
// Usage:
//   driftEngine.prepare(sampleRate);
//   // In processBlock, per voice:
//   float pitchOffsetCents  = driftEngine.getVoicePitchDrift(voiceId);
//   float levelOffsetLinear = driftEngine.getVoiceLevelDrift(voiceId);
//==============================================================================

struct VoiceDriftState
{
    // Random walk accumulators
    float pitchDrift   = 0.0f;  // cents, applied to voice fine tune
    float levelDrift   = 0.0f;  // linear 0-1 scalar on output level
    float envTimeDrift = 0.0f;  // fraction added to attack/decay times

    // Slow LFO phases (radians)
    float pitchLFOPhase = 0.0f;
    float levelLFOPhase = 0.0f;

    // Per-voice seed offset so voices drift independently
    float phaseOffset = 0.0f;
};

class DriftEngine
{
public:
    static constexpr int MAX_VOICES = 16;

    DriftEngine();

    void prepare (double sampleRate);
    void reset();

    // Update all voice drift states for this block
    void processBlock (int numSamples);

    // Accessors for use in voice rendering
    float getVoicePitchDrift  (int voiceId) const; // in cents (-maxPitch..+maxPitch)
    float getVoiceLevelDrift  (int voiceId) const; // multiplier (1.0 = no drift)
    float getVoiceEnvTimeDrift(int voiceId) const; // fractional time offset (0..maxEnv)

    // Notify when a note starts on a voice (re-randomizes phase seed)
    void noteOn (int voiceId);

    //--- Controls (0-1 range for all amounts) ---
    void setTuneDriftAmt  (float v) { tuneDriftAmt  = juce::jlimit(0.0f,1.0f,v); }
    void setLevelDriftAmt (float v) { levelDriftAmt = juce::jlimit(0.0f,1.0f,v); }
    void setDriftSpeed    (float v) { driftSpeed    = juce::jlimit(0.0f,1.0f,v); }
    void setVintageAmt    (float v) { vintageAmt    = juce::jlimit(0.0f,1.0f,v); }
    void setEnabled       (bool e)  { enabled = e; }
    bool isEnabled()       const    { return enabled; }

    // Max ranges (editable if desired)
    float maxPitchCents = 12.0f;   // max drift +/- cents
    float maxLevelRange = 0.08f;   // max level drift fraction
    float maxEnvRange   = 0.05f;   // max envelope time drift fraction

private:
    bool  enabled        = false;
    float tuneDriftAmt   = 0.0f;
    float levelDriftAmt  = 0.0f;
    float driftSpeed     = 0.3f;  // 0=very slow, 1=fast
    float vintageAmt     = 0.0f;  // adds extra randomness

    double sampleRate    = 44100.0;
    double phaseAccum    = 0.0;   // master phase accumulator

    std::array<VoiceDriftState, MAX_VOICES> voices;
    juce::Random rng;

    // Slew-limited random walk helper
    float randomWalk (float current, float stepSize, float clamp, juce::Random& r) const;

    // Drift LFO frequency derived from driftSpeed
    float getLFOFreq() const;
};
