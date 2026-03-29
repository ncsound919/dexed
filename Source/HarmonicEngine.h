#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <vector>
#include <array>

//==============================================================================
// NCS: HarmonicEngine
// Generates chord progression MIDI events into processBlock's midiBuffer.
// Sits upstream of the FM engine — does not touch engine internals.
//==============================================================================

struct ChordVoicing
{
    std::vector<int> midiNotes;  // absolute MIDI note numbers
    juce::String     name;       // e.g. "Cmaj7"
};

struct ProgressionTemplate
{
    juce::String              name;         // e.g. "Pop I-V-vi-IV"
    std::vector<int>          degrees;      // scale degrees 0-6 (0=I, 4=V, etc.)
    std::vector<juce::String> qualities;   // "maj","min","dom7","maj7","min7","dim"
};

class HarmonicEngine
{
public:
    HarmonicEngine();

    //--- Configuration ---
    void setKey   (int rootMidi);          // e.g. 60 = C4
    void setScale (int scaleIndex);        // 0=Major,1=NatMinor,2=Dorian,3=Mixolydian
    void setProgression (int progIndex);   // index into builtInProgressions
    void setChordsPerBar (int cpb);        // 1,2,4
    void setOctave (int oct);              // chord voicing base octave (3-5)
    void setHumanizeAmt (float amount);    // 0-1 timing/velocity humanize
    void setEnabled (bool e) { enabled = e; }

    //--- Called from processBlock ---
    // timeInfo: host AudioPlayHead::CurrentPositionInfo
    // midiOut : MidiBuffer to inject notes into
    // numSamples / sampleRate for timing
    void process (const juce::AudioPlayHead::CurrentPositionInfo& pos,
                  juce::MidiBuffer& midiOut,
                  int numSamples,
                  double sampleRate);

    void reset();

    //--- Preset data access ---
    int  getNumProgressions() const { return (int)builtInProgressions.size(); }
    juce::String getProgressionName (int i) const;

    static const std::array<juce::String, 12> noteNames;
    static constexpr int NUM_SCALES = 4;
    static const juce::String scaleNames[NUM_SCALES];

private:
    bool  enabled        = false;
    int   rootNote       = 60;   // C4
    int   scaleIndex     = 0;    // Major
    int   progIndex      = 0;
    int   chordsPerBar   = 1;
    int   baseOctave     = 4;
    float humanizeAmt    = 0.0f;

    // Playback state
    int    lastBar       = -1;
    int    lastChordSlot = -1;
    double lastPPQ       = -1.0;
    std::vector<int> activeNotes;

    // Scale intervals (semitones from root for 7 diatonic degrees)
    static const int scaleIntervals[NUM_SCALES][7];

    // Chord quality offsets relative to scale degree root
    std::vector<int> getQualityIntervals (const juce::String& quality) const;

    // Build an absolute voicing from scale degree + quality + octave
    ChordVoicing buildVoicing (int degree, const juce::String& quality) const;

    // Inject NoteOn/NoteOff into buffer
    void sendNoteOff (juce::MidiBuffer& buf, int note, int sampleOffset);
    void sendNoteOn  (juce::MidiBuffer& buf, int note, int vel, int sampleOffset);
    void killActiveNotes (juce::MidiBuffer& buf, int sampleOffset);

    // Built-in progression templates
    std::vector<ProgressionTemplate> builtInProgressions;
    void buildPresets();

    juce::Random rng;
};
