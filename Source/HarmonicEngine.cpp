#include "HarmonicEngine.h"

//--- Static data ---
const std::array<juce::String, 12> HarmonicEngine::noteNames = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

const juce::String HarmonicEngine::scaleNames[HarmonicEngine::NUM_SCALES] = {
    "Major", "Natural Minor", "Dorian", "Mixolydian"
};

// Diatonic intervals for each scale (7 degrees, semitones from root)
const int HarmonicEngine::scaleIntervals[HarmonicEngine::NUM_SCALES][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },   // Major
    { 0, 2, 3, 5, 7, 8, 10 },   // Natural Minor
    { 0, 2, 3, 5, 7, 9, 10 },   // Dorian
    { 0, 2, 4, 5, 7, 9, 10 }    // Mixolydian
};

HarmonicEngine::HarmonicEngine()
{
    buildPresets();
}

void HarmonicEngine::buildPresets()
{
    builtInProgressions = {
        { "Pop I-V-vi-IV",        { 0,4,5,3 }, { "maj","maj","min","maj" } },
        { "Pop I-IV-V",           { 0,3,4 },   { "maj","maj","maj" }       },
        { "Minor i-VI-III-VII",   { 0,5,2,6 }, { "min","maj","maj","maj" } },
        { "Jazz ii-V-I",          { 1,4,0 },   { "min7","dom7","maj7" }    },
        { "Jazz I-vi-ii-V",       { 0,5,1,4 }, { "maj7","min7","min7","dom7" } },
        { "Trap Minor i-VII-VI",  { 0,6,5 },   { "min","maj","maj" }       },
        { "EDM I-IV-VI-V",        { 0,3,5,4 }, { "maj","maj","maj","maj" } },
        { "Lofi i-III-VII-VI",    { 0,2,6,5 }, { "min7","maj7","maj7","min7" } },
        { "RnB I-iii-IV-V",       { 0,2,3,4 }, { "maj7","min7","maj7","dom7" } },
        { "Blues I-IV-I-V-IV-I",  { 0,3,0,4,3,0 }, { "dom7","dom7","dom7","dom7","dom7","dom7" } }
    };
}

//--- Quality intervals ---
std::vector<int> HarmonicEngine::getQualityIntervals (const juce::String& q) const
{
    if (q == "maj")   return { 0, 4, 7 };
    if (q == "min")   return { 0, 3, 7 };
    if (q == "dom7")  return { 0, 4, 7, 10 };
    if (q == "maj7")  return { 0, 4, 7, 11 };
    if (q == "min7")  return { 0, 3, 7, 10 };
    if (q == "dim")   return { 0, 3, 6 };
    if (q == "aug")   return { 0, 4, 8 };
    return { 0, 4, 7 }; // fallback major
}

ChordVoicing HarmonicEngine::buildVoicing (int degree, const juce::String& quality) const
{
    int degreeIndex = degree % 7;
    int semitone = scaleIntervals[scaleIndex][degreeIndex];
    int baseNote = (rootNote % 12) + (baseOctave * 12) + semitone;

    auto intervals = getQualityIntervals (quality);
    ChordVoicing v;
    for (int i : intervals)
        v.midiNotes.push_back (juce::jlimit (0, 127, baseNote + i));

    // Build name string
    int noteClass = (baseNote) % 12;
    v.name = noteNames[noteClass] + " " + quality;
    return v;
}

void HarmonicEngine::setKey        (int r) { rootNote = r; }
void HarmonicEngine::setScale      (int s) { scaleIndex = juce::jlimit(0, NUM_SCALES-1, s); }
void HarmonicEngine::setProgression(int p) { progIndex = juce::jlimit(0, (int)builtInProgressions.size()-1, p); }
void HarmonicEngine::setChordsPerBar(int c){ chordsPerBar = juce::jlimit(1, 4, c); }
void HarmonicEngine::setOctave     (int o) { baseOctave = juce::jlimit(2, 6, o); }
void HarmonicEngine::setHumanizeAmt(float a){ humanizeAmt = juce::jlimit(0.0f, 1.0f, a); }

juce::String HarmonicEngine::getProgressionName (int i) const
{
    if (i >= 0 && i < (int)builtInProgressions.size())
        return builtInProgressions[i].name;
    return {};
}

void HarmonicEngine::reset()
{
    lastBar = -1;
    lastChordSlot = -1;
    lastPPQ = -1.0;
    activeNotes.clear();
}

void HarmonicEngine::sendNoteOn (juce::MidiBuffer& buf, int note, int vel, int offset)
{
    buf.addEvent (juce::MidiMessage::noteOn  (1, note, (juce::uint8)vel), offset);
    activeNotes.push_back (note);
}

void HarmonicEngine::sendNoteOff (juce::MidiBuffer& buf, int note, int offset)
{
    buf.addEvent (juce::MidiMessage::noteOff (1, note), offset);
}

void HarmonicEngine::killActiveNotes (juce::MidiBuffer& buf, int sampleOffset)
{
    for (int n : activeNotes)
        sendNoteOff (buf, n, sampleOffset);
    activeNotes.clear();
}

void HarmonicEngine::process (const juce::AudioPlayHead::CurrentPositionInfo& pos,
                               juce::MidiBuffer& midiOut,
                               int numSamples,
                               double sampleRate)
{
    if (!enabled || !pos.isPlaying) return;

    const auto& prog = builtInProgressions[progIndex];
    int numChords = (int)prog.degrees.size();
    if (numChords == 0) return;

    double ppq = pos.ppqPosition;
    double beatsPerChord = 4.0 / (double)chordsPerBar;
    double totalBeats = beatsPerChord * numChords;
    double loopedPPQ = std::fmod (ppq, totalBeats);
    int chordSlot = (int)(loopedPPQ / beatsPerChord) % numChords;

    if (chordSlot != lastChordSlot)
    {
        // Offset within this block where chord change occurs (approx)
        double samplesPerBeat = sampleRate * 60.0 / pos.bpm;
        double beatsIntoBlock = ppq - std::floor(ppq / beatsPerChord) * beatsPerChord;
        int sampleOffset = juce::jlimit (0, numSamples - 1,
            (int)((beatsPerChord - beatsIntoBlock) * samplesPerBeat));
        sampleOffset = 0; // snap to block start for safety

        // Velocity humanize
        int vel = 90;
        if (humanizeAmt > 0.0f)
            vel += rng.nextInt (juce::Range<int> (-(int)(humanizeAmt * 20), (int)(humanizeAmt * 20)));
        vel = juce::jlimit (40, 127, vel);

        killActiveNotes (midiOut, sampleOffset);

        int deg     = prog.degrees [chordSlot % (int)prog.degrees.size()];
        auto qual   = prog.qualities[chordSlot % (int)prog.qualities.size()];
        auto voicing = buildVoicing (deg, qual);

        for (int note : voicing.midiNotes)
            sendNoteOn (midiOut, note, vel, sampleOffset);

        lastChordSlot = chordSlot;
    }

    lastPPQ = ppq;
}
