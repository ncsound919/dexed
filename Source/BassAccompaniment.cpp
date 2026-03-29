#include "BassAccompaniment.h"

BassAccompaniment::BassAccompaniment() {}

void BassAccompaniment::reset()
{
    lastBeatFrac = -1.0;
    arpIndex = 0;
    activeNotes.clear();
}

void BassAccompaniment::sendNoteOn (juce::MidiBuffer& buf, int note, int vel, int offset)
{
    buf.addEvent (juce::MidiMessage::noteOn  (2, note, (juce::uint8)vel), offset);
    activeNotes.push_back (note);
}

void BassAccompaniment::sendNoteOff (juce::MidiBuffer& buf, int note, int offset)
{
    buf.addEvent (juce::MidiMessage::noteOff (2, note), offset);
}

void BassAccompaniment::killActiveNotes (juce::MidiBuffer& buf, int offset)
{
    for (int n : activeNotes)
        sendNoteOff (buf, n, offset);
    activeNotes.clear();
}

std::vector<BassAccompaniment::BassHit>
BassAccompaniment::buildPattern (const std::vector<int>& chordNotes) const
{
    std::vector<BassHit> pattern;

    switch (mode)
    {
        case BassMode::RootOnly:
            pattern = { {0.0, 0} };
            break;

        case BassMode::RootAndFifth:
            pattern = { {0.0, 0}, {0.5, 7} };
            break;

        case BassMode::Arpeggiated:
        {
            // Spread chord notes evenly across the beat
            int n = (int)chordNotes.size();
            if (n == 0) break;
            for (int i = 0; i < n; ++i)
                pattern.push_back ({ (double)i / (double)n,
                                     chordNotes[i] - chordNotes[0] });
            break;
        }

        case BassMode::Syncopated:
            // Off-beat pattern (1 and-2 and)
            pattern = { {0.0, 0}, {0.375, 0}, {0.75, 7} };
            break;

        default:
            break;
    }
    return pattern;
}

void BassAccompaniment::process (const juce::AudioPlayHead::CurrentPositionInfo& pos,
                                  juce::MidiBuffer& midiOut,
                                  int numSamples,
                                  double sampleRate,
                                  int chordRootMidi,
                                  const std::vector<int>& chordNotes)
{
    if (!enabled || mode == BassMode::Off || !pos.isPlaying) return;
    if (chordRootMidi < 0) return;

    int bassRoot = (chordRootMidi % 12) + (bassOctave * 12);
    auto pattern = buildPattern (chordNotes);
    if (pattern.empty()) return;

    double ppq = pos.ppqPosition;
    double beatFrac = ppq - std::floor (ppq);
    double samplesPerBeat = sampleRate * 60.0 / pos.bpm;

    for (auto& hit : pattern)
    {
        // Check if this hit falls inside the current buffer
        double hitPPQ = std::floor (ppq) + hit.beatFraction;
        if (hitPPQ < ppq) hitPPQ += 1.0;  // push to next beat if already past
        if (hitPPQ >= ppq + (double)numSamples / samplesPerBeat) continue;

        int sampleOffset = juce::jlimit (0, numSamples - 1,
            (int)((hitPPQ - ppq) * samplesPerBeat));

        // Humanize velocity
        int vel = baseVelocity;
        if (humanize > 0.0f)
            vel += rng.nextInt (juce::Range<int> (-(int)(humanize * 15), (int)(humanize * 15)));
        vel = juce::jlimit (30, 127, vel);

        killActiveNotes (midiOut, sampleOffset);
        int note = juce::jlimit (0, 127, bassRoot + hit.noteOffset);
        sendNoteOn (midiOut, note, vel, sampleOffset);
    }
}
