#include "HybridOscillator.h"
#include <cmath>

HybridOscillator::HybridOscillator()
{
    updateFilter();
}

void HybridOscillator::prepare (double sr)
{
    sampleRate = sr;
    updateFrequency();
    updateFilter();
}

void HybridOscillator::reset()
{
    phase    = 0.0;
    filterZ1 = 0.0f;
    active   = false;
}

void HybridOscillator::noteOn (int note, float vel)
{
    midiNote = (float)note;
    velocity = vel;
    active   = true;
    updateFrequency();
}

void HybridOscillator::noteOff()
{
    active = false;
}

void HybridOscillator::updateFrequency()
{
    if (sampleRate <= 0.0) return;
    float noteWithPitch = midiNote + pitchOffsetCents / 100.0f;
    float freq = 440.0f * std::pow (2.0f, (noteWithPitch - 69.0f) / 12.0f);
    phaseInc = freq / sampleRate;
}

void HybridOscillator::updateFilter()
{
    if (sampleRate <= 0.0) return;
    // Simple one-pole LP: H(z) = a0 / (1 - b1*z^-1)
    float fc = cutoff / (float)sampleRate;
    fc = juce::jlimit (0.001f, 0.499f, fc);
    filterB1 = std::exp (-juce::MathConstants<float>::twoPi * fc);
    filterA0 = 1.0f - filterB1;
}

float HybridOscillator::polyBlep (double t, double dt) const
{
    if (t < dt)
    {
        t /= dt;
        return (float)(2.0 * t - t * t - 1.0);
    }
    else if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return (float)(t * t + 2.0 * t + 1.0);
    }
    return 0.0f;
}

float HybridOscillator::generateSample()
{
    float out = 0.0f;
    float t = (float)phase;

    switch (waveform)
    {
        case 1: // Saw (PolyBLEP anti-aliased)
            out = (float)(2.0 * phase - 1.0);
            out -= polyBlep (phase, phaseInc);
            break;

        case 2: // Square (PolyBLEP)
            out = phase < 0.5 ? 1.0f : -1.0f;
            out += polyBlep (phase, phaseInc);
            out -= polyBlep (std::fmod (phase + 0.5, 1.0), phaseInc);
            break;

        case 3: // Triangle
            out = (float)(phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
            break;

        case 4: // Sine
            out = std::sin ((float)(juce::MathConstants<double>::twoPi * phase));
            break;

        default:
            out = 0.0f;
            break;
    }

    // Advance phase
    phase += phaseInc;
    if (phase >= 1.0) phase -= 1.0;

    // Apply one-pole LP filter
    filterZ1 = filterA0 * out + filterB1 * filterZ1;
    return filterZ1 * velocity;
}

void HybridOscillator::processAdd (float* outBuffer, int numSamples, float mixLevel)
{
    if (!active || waveform == 0 || mixLevel <= 0.0f) return;

    for (int i = 0; i < numSamples; ++i)
    {
        float s = generateSample() * mixLevel;
        outBuffer[i * 2]     += s;  // L
        outBuffer[i * 2 + 1] += s;  // R
    }
}
