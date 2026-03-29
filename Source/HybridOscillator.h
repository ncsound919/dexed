#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

//==============================================================================
// NCS: HybridOscillator
// A lightweight per-voice analog oscillator that layers on top of the FM engine.
// Output: a mono float buffer that is mixed (with mix level) into the FM output.
//
// Supported waveforms:
//   0 = Off
//   1 = Saw
//   2 = Square
//   3 = Triangle
//   4 = Sine
//
// Simple one-pole lowpass filter included for basic tone shaping.
//==============================================================================

class HybridOscillator
{
public:
    HybridOscillator();

    void prepare  (double sampleRate);
    void noteOn   (int midiNote, float velocity);
    void noteOff  ();
    void reset    ();

    // Render numSamples into outBuffer (interleaved stereo, 2*numSamples floats)
    // Mixes (adds) into existing buffer content.
    void processAdd (float* outBuffer, int numSamples, float mixLevel);

    bool isActive() const { return active; }

    //--- Controls ---
    void setWaveform   (int w)     { waveform = juce::jlimit (0, 4, w); }
    void setCutoff     (float hz)  { cutoff = juce::jlimit (20.0f, 20000.0f, hz); updateFilter(); }
    void setResonance  (float q)   { resonance = juce::jlimit (0.5f, 10.0f, q); updateFilter(); }
    void setPitchOffsetCents (float c) { pitchOffsetCents = c; updateFrequency(); }
    int  getWaveform() const { return waveform; }

private:
    double sampleRate      = 44100.0;
    int    waveform        = 0;       // Off by default
    float  midiNote        = 60.0f;
    float  velocity        = 1.0f;
    bool   active          = false;

    // Oscillator state
    double phase           = 0.0;
    double phaseInc        = 0.0;
    float  pitchOffsetCents= 0.0f;

    // Simple one-pole LP filter
    float cutoff           = 8000.0f;
    float resonance        = 1.0f;
    float filterA0         = 1.0f;    // feedforward
    float filterB1         = 0.0f;    // feedback
    float filterZ1         = 0.0f;    // state

    void updateFrequency();
    void updateFilter();

    float generateSample();

    // Anti-aliased saw via PolyBLEP
    float polyBlep (double t, double dt) const;
};
