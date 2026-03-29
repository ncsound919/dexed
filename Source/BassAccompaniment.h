#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <vector>

//==============================================================================
// NCS: BassAccompaniment
// Generates bass MIDI lines locked to the HarmonicEngine's current chord root.
// Outputs on MIDI channel 2 (channel 1 = chords, channel 2 = bass).
//==============================================================================

enum class BassMode
{
    Off = 0,
    RootOnly,
    RootAndFifth,
    Arpeggiated,
    Syncopated
};

class BassAccompaniment
{
public:
    BassAccompaniment();

    void setMode     (BassMode m)   { mode = m; }
    void setEnabled  (bool e)       { enabled = e; }
    void setOctave   (int oct)      { bassOctave = juce::jlimit (1, 4, oct); }
    void setVelocity (int vel)      { baseVelocity = juce::jlimit (40, 127, vel); }
    void setHumanize (float h)      { humanize = juce::jlimit (0.0f, 1.0f, h); }

    // Call after HarmonicEngine::process to inject bass notes.
    // chordRootMidi: the root note the HarmonicEngine just played
    // chordNotes: all notes in the current chord
    void process (const juce::AudioPlayHead::CurrentPositionInfo& pos,
                  juce::MidiBuffer& midiOut,
                  int numSamples,
                  double sampleRate,
                  int chordRootMidi,
                  const std::vector<int>& chordNotes);

    void reset();

private:
    bool      enabled       = false;
    BassMode  mode          = BassMode::RootOnly;
    int       bassOctave    = 2;
    int       baseVelocity  = 80;
    float     humanize      = 0.0f;

    // Playback state
    double    lastBeatFrac  = -1.0;
    int       arpIndex      = 0;
    std::vector<int> activeNotes;
    juce::Random rng;

    // Pattern definitions: list of (beat fraction, note offset from root)
    struct BassHit { double beatFraction; int noteOffset; };
    std::vector<BassHit> buildPattern (const std::vector<int>& chordNotes) const;

    void sendNoteOn  (juce::MidiBuffer& buf, int note, int vel, int offset);
    void sendNoteOff (juce::MidiBuffer& buf, int note, int offset);
    void killActiveNotes (juce::MidiBuffer& buf, int offset);
};
