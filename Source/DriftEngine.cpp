#include "DriftEngine.h"
#include <cmath>

DriftEngine::DriftEngine()
{
    rng.setSeedRandomly();
    for (int i = 0; i < MAX_VOICES; ++i)
    {
        voices[i].phaseOffset = (float)i * (juce::MathConstants<float>::twoPi / MAX_VOICES);
        voices[i].pitchLFOPhase  = voices[i].phaseOffset;
        voices[i].levelLFOPhase  = voices[i].phaseOffset * 0.7f;
    }
}

void DriftEngine::prepare (double sr)
{
    sampleRate = sr;
    phaseAccum = 0.0;
}

void DriftEngine::reset()
{
    for (auto& v : voices)
    {
        v.pitchDrift = 0.0f;
        v.levelDrift = 0.0f;
        v.envTimeDrift = 0.0f;
    }
}

void DriftEngine::noteOn (int voiceId)
{
    if (voiceId < 0 || voiceId >= MAX_VOICES) return;
    // Slightly re-seed phase for organic feel
    voices[voiceId].pitchLFOPhase += rng.nextFloat() * 0.5f;
    voices[voiceId].levelLFOPhase += rng.nextFloat() * 0.5f;
}

float DriftEngine::getLFOFreq() const
{
    // driftSpeed 0 -> 0.01 Hz, 1 -> 0.3 Hz
    return 0.01f + driftSpeed * 0.29f;
}

float DriftEngine::randomWalk (float current, float stepSize, float clamp, juce::Random& r) const
{
    float delta = (r.nextFloat() * 2.0f - 1.0f) * stepSize;
    return juce::jlimit (-clamp, clamp, current + delta);
}

void DriftEngine::processBlock (int numSamples)
{
    if (!enabled) return;

    float lfoFreq = getLFOFreq();
    double lfoInc = juce::MathConstants<double>::twoPi * lfoFreq / sampleRate;
    double blockPhaseInc = lfoInc * numSamples;

    float vintageStep = vintageAmt * 0.002f;
    float tuneStep    = tuneDriftAmt  * 0.005f;
    float levelStep   = levelDriftAmt * 0.003f;

    for (int i = 0; i < MAX_VOICES; ++i)
    {
        auto& v = voices[i];

        // Advance LFO phases
        v.pitchLFOPhase  = (float)std::fmod (v.pitchLFOPhase  + blockPhaseInc, juce::MathConstants<double>::twoPi);
        v.levelLFOPhase  = (float)std::fmod (v.levelLFOPhase  + blockPhaseInc * 0.73, juce::MathConstants<double>::twoPi);

        // LFO contributions
        float pitchLFO = std::sin (v.pitchLFOPhase) * tuneDriftAmt  * maxPitchCents * 0.5f;
        float levelLFO = std::sin (v.levelLFOPhase) * levelDriftAmt * maxLevelRange * 0.5f;

        // Random walk contributions
        v.pitchDrift    = randomWalk (v.pitchDrift,    tuneStep,    maxPitchCents,  rng) + pitchLFO;
        v.levelDrift    = randomWalk (v.levelDrift,    levelStep,   maxLevelRange,  rng) + levelLFO;
        v.envTimeDrift  = randomWalk (v.envTimeDrift,  vintageStep, maxEnvRange,    rng);

        // Clamp finals
        v.pitchDrift   = juce::jlimit (-maxPitchCents,  maxPitchCents,  v.pitchDrift);
        v.levelDrift   = juce::jlimit (-maxLevelRange,  maxLevelRange,  v.levelDrift);
        v.envTimeDrift = juce::jlimit (0.0f,             maxEnvRange,   v.envTimeDrift);
    }
}

float DriftEngine::getVoicePitchDrift  (int i) const
{
    if (!enabled || i < 0 || i >= MAX_VOICES) return 0.0f;
    return voices[i].pitchDrift;
}

float DriftEngine::getVoiceLevelDrift  (int i) const
{
    if (!enabled || i < 0 || i >= MAX_VOICES) return 1.0f;
    return 1.0f + voices[i].levelDrift;
}

float DriftEngine::getVoiceEnvTimeDrift(int i) const
{
    if (!enabled || i < 0 || i >= MAX_VOICES) return 0.0f;
    return voices[i].envTimeDrift;
}
