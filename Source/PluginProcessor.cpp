/**
 *
 * Copyright (c) 2013-2025 Pascal Gauthier.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 */

#include <stdarg.h>
#include <bitset>

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "Dexed.h"
#include "msfa/synth.h"
#include "msfa/freqlut.h"
#include "msfa/sin.h"
#include "msfa/exp2.h"
#include "msfa/env.h"
#include "msfa/pitchenv.h"
#include "msfa/porta.h"
#include "msfa/aligned_buf.h"
#include "msfa/fm_op_kernel.h"

#if JUCE_MSVC
    #pragma comment (lib, "kernel32.lib")
    #pragma comment (lib, "user32.lib")
    #pragma comment (lib, "wininet.lib")
    #pragma comment (lib, "advapi32.lib")
    #pragma comment (lib, "ws2_32.lib")
    #pragma comment (lib, "version.lib")
    #pragma comment (lib, "shlwapi.lib")
    #pragma comment (lib, "winmm.lib")
	#pragma comment (lib, "DbgHelp.lib")
	#pragma comment (lib, "Imm32.lib")

	#ifdef _NATIVE_WCHAR_T_DEFINED
		#ifdef _DEBUG
			#pragma comment (lib, "comsuppwd.lib")
		#else
			#pragma comment (lib, "comsuppw.lib")
		#endif
	#else
		#ifdef _DEBUG
			#pragma comment (lib, "comsuppd.lib")
		#else
			#pragma comment (lib, "comsupp.lib")
		#endif
	#endif
#endif

//==============================================================================
DexedAudioProcessor::DexedAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("output", AudioChannelSet::stereo(), true)) {
#ifdef DEBUG
    if ( !JUCEApplication::isStandaloneApp() ) {
        Logger *tmp = Logger::getCurrentLogger();
        if ( tmp == NULL ) {
            Logger::setCurrentLogger(FileLogger::createDateStampedLogger("Dexed", "DebugSession-", "log", "DexedAudioProcessor Created"));
        }
    }
    TRACE("Hi");
#endif

    Exp2::init();
    Tanh::init();
    Sin::init();

    synthTuningState = createStandardTuning();
    synthTuningStateLast = createStandardTuning();
    
    lastStateSave = 0;
    currentNote = -1;
    engineType = -1;
    
    vuSignal = 0;
    monoMode = 0;

    resolvAppDir();
    initCtrl();
    sendSysexChange = true;
    normalizeDxVelocity = false;
    sysexComm.listener = this;
    showKeyboard = true;
    
    memset(&voiceStatus, 0, sizeof(VoiceStatus));
    setEngineType(DEXED_ENGINE_MARKI);
    
    controllers.values_[kControllerPitchRangeUp] = 3;
    controllers.values_[kControllerPitchRangeDn] = 3;
    controllers.values_[kControllerPitchStep] = 0;
    controllers.masterTune = 0;
    
    loadPreference();

    for (int note = 0; note < MAX_ACTIVE_NOTES; ++note)
        voices[note].dx7_note = NULL;

    setCurrentProgram(0);
    nextMidi = NULL;
    midiMsg  = NULL;
    
    mtsClient = NULL;
    mtsClient = MTS_RegisterClient();
}

DexedAudioProcessor::~DexedAudioProcessor() {
    Logger *tmp = Logger::getCurrentLogger();
    if ( tmp != NULL ) {
        Logger::setCurrentLogger(NULL);
        delete tmp;
    }
    TRACE("Bye");
    if (mtsClient) MTS_DeregisterClient(mtsClient);
}

//==============================================================================
void DexedAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    Freqlut::init(sampleRate);
    Lfo::init(sampleRate);
    PitchEnv::init(sampleRate);
    Env::init_sr(sampleRate);
    Porta::init_sr(sampleRate);
    fx.init(sampleRate);

    vuDecayFactor = VuMeterOutput::getDecayFactor(sampleRate);
    
    for (int note = 0; note < MAX_ACTIVE_NOTES; ++note) {
        voices[note].dx7_note = new Dx7Note(synthTuningState, mtsClient);
        voices[note].midi_note = -1;
        voices[note].keydown   = false;
        voices[note].sustained = false;
        voices[note].live      = false;
        voices[note].keydown_seq = -1;
    }

    currentNote  = 0;
    nextKeydownSeq = 0;
    controllers.values_[kControllerPitch] = 0x2000;
    controllers.modwheel_cc  = 0;
    controllers.foot_cc      = 0;
    controllers.breath_cc    = 0;
    controllers.aftertouch_cc = 0;
    controllers.portamento_enable_cc = false;
    controllers.portamento_cc = 0;
    controllers.refresh();

    sustain = false;
    extra_buf_size = 0;

    keyboardState.reset();
    lfo.reset(data + 137);
    
    nextMidi = new MidiMessage(0xF0);
    midiMsg  = new MidiMessage(0xF0);

    // NCS: prepare new engines
    driftEngine.prepare(sampleRate);
    harmonicEngine.reset();
    bassEngine.reset();
    for (auto& osc : hybridOscs)
        osc.prepare(sampleRate);
}

void DexedAudioProcessor::releaseResources() {
    currentNote = -1;

    for (int note = 0; note < MAX_ACTIVE_NOTES; ++note) {
        if ( voices[note].dx7_note != NULL ) {
            delete voices[note].dx7_note;
            voices[note].dx7_note = NULL;
        }
        voices[note].keydown   = false;
        voices[note].sustained = false;
        voices[note].live      = false;
    }

    keyboardState.reset();
    if ( nextMidi != NULL ) { delete nextMidi; nextMidi = NULL; }
    if ( midiMsg  != NULL ) { delete midiMsg;  midiMsg  = NULL; }
}

void DexedAudioProcessor::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) {

    juce::ScopedNoDenormals noDenormals;

    int numSamples = buffer.getNumSamples();
    int i;
    
    if ( refreshVoice ) {
        for (i = 0; i < MAX_ACTIVE_NOTES; i++)
            if ( voices[i].live )
                voices[i].dx7_note->update(data, voices[i].midi_note, voices[i].velocity, voices[i].channel);
        lfo.reset(data + 137);
        refreshVoice = false;
    }

    // NCS: Step 1 — update drift states for this block
    driftEngine.processBlock(numSamples);

    // NCS: Step 2 — inject chord + bass MIDI before FM engine processes them
    {
        juce::AudioPlayHead::CurrentPositionInfo pos;
        pos.bpm         = 120.0;
        pos.isPlaying   = false;
        pos.ppqPosition = 0.0;
        if (auto* ph = getPlayHead())
            ph->getCurrentPosition(pos);

        harmonicEngine.process(pos, midiMessages, numSamples, getSampleRate());
        bassEngine.process(pos, midiMessages, numSamples, getSampleRate(),
                           ncsLastChordRoot, ncsLastChordNotes);
    }

    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);
    
    MidiBuffer::Iterator it(midiMessages);
    hasMidiMessage = it.getNextEvent(*nextMidi, midiEventPos);

    float *channelData = buffer.getWritePointer(0);
  
    for (i = 0; i < numSamples && i < extra_buf_size; i++)
        channelData[i] = extra_buf[i];
    
    if (extra_buf_size > numSamples) {
        for (int j = 0; j < extra_buf_size - numSamples; j++)
            extra_buf[j] = extra_buf[j + numSamples];
        extra_buf_size -= numSamples;
        while (getNextEvent(&it, numSamples))
            processMidiMessage(midiMsg);
    } else {
        for (; i < numSamples; i += N) {
            AlignedBuf<int32_t, N> audiobuf;
            float sumbuf[N];
            
            while (getNextEvent(&it, i))
                processMidiMessage(midiMsg);
            
            for (int j = 0; j < N; ++j) {
                audiobuf.get()[j] = 0;
                sumbuf[j] = 0;
            }

            int32_t lfovalue = lfo.getsample();
            int32_t lfodelay = lfo.getdelay();
            
            bool checkMTSESPRetuning = synthTuningState->is_standard_tuning() &&
                                        MTS_HasMaster(mtsClient);
            
            for (int note = 0; note < MAX_ACTIVE_NOTES; ++note) {
                if (voices[note].live) {
                    if (checkMTSESPRetuning)
                        voices[note].dx7_note->updateBasePitches();
                    
                    // NCS: apply pitch drift to fine tune before render
                    if (driftEngine.isEnabled()) {
                        float pitchCents = driftEngine.getVoicePitchDrift(note);
                        // controllers.masterTune is in units; 1 unit ~ 1 cent at centre
                        // We temporarily offset per-note fine tune via masterTune delta
                        // A cleaner approach is to call dx7_note->update() with modified data,
                        // but a lightweight approach is to stash and restore masterTune:
                        int savedTune = controllers.masterTune;
                        controllers.masterTune = savedTune + (int)(pitchCents * 100.0f);
                        controllers.refresh();
                        voices[note].dx7_note->compute(audiobuf.get(), lfovalue, lfodelay, &controllers);
                        controllers.masterTune = savedTune;
                        controllers.refresh();
                    } else {
                        voices[note].dx7_note->compute(audiobuf.get(), lfovalue, lfodelay, &controllers);
                    }
                    
                    float levelScale = driftEngine.isEnabled()
                                       ? driftEngine.getVoiceLevelDrift(note)
                                       : 1.0f;

                    for (int j = 0; j < N; ++j) {
                        int32_t val = audiobuf.get()[j];
                        val = val >> 4;
                        int clip_val = val < -(1 << 24) ? 0x8000 : val >= (1 << 24) ? 0x7fff : val >> 9;
                        float f = ((float) clip_val) / (float) 0x8000;
                        if (f >  1) f =  1;
                        if (f < -1) f = -1;
                        sumbuf[j] += f * levelScale;
                        audiobuf.get()[j] = 0;
                    }

                    // NCS: mix hybrid oscillator on top of FM for this voice
                    if (hybridMixLevel > 0.0f && hybridOscs[note].isActive()) {
                        int jmax = numSamples - i;
                        // Build a temp stereo block and add into sumbuf (mono L only for now)
                        std::vector<float> hybBuf(N * 2, 0.0f);
                        hybridOscs[note].processAdd(hybBuf.data(), N, hybridMixLevel);
                        for (int j = 0; j < N && j < jmax; ++j)
                            sumbuf[j] += hybBuf[j * 2];  // L channel
                    }
                }
            }
            
            int jmax = numSamples - i;
            for (int j = 0; j < N; ++j) {
                if (j < jmax)
                    channelData[i + j] = sumbuf[j];
                else
                    extra_buf[j - jmax] = sumbuf[j];
            }
        }
        extra_buf_size = i - numSamples;
    }
    
    while (getNextEvent(&it, numSamples))
        processMidiMessage(midiMsg);

    fx.process(channelData, numSamples);

    for (i = 0; i < numSamples; i++) {
        float s = std::abs(channelData[i]);
        if (s > vuSignal)
            vuSignal = s;
        else if (vuSignal > 1.26E-4F)
            vuSignal *= vuDecayFactor;
        else
            vuSignal = 0;
    }
    
    if ( buffer.getNumChannels() > 1 )
        buffer.copyFrom(1, 0, channelData, numSamples, 1);
}

//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new DexedAudioProcessor();
}

bool DexedAudioProcessor::getNextEvent(MidiBuffer::Iterator* iter, const int samplePos) {
    if (hasMidiMessage && midiEventPos <= samplePos) {
        *midiMsg = *nextMidi;
        hasMidiMessage = iter->getNextEvent(*nextMidi, midiEventPos);
        return true;
    }
    return false;
}

void DexedAudioProcessor::processMidiMessage(const MidiMessage *msg) {
    if ( msg->isSysEx() ) {
        handleIncomingMidiMessage(NULL, *msg);
        return;
    }

    const uint8 *buf  = msg->getRawData();
    uint8_t cmd = buf[0];
    uint8_t cf0 = cmd & 0xf0;
    auto channel = msg->getChannel();

    if( controllers.mpeEnabled && channel != 1 &&
        (
            (cf0 == 0xb0 && buf[1] == 74 ) ||
            (cf0 == 0xd0 ) ||
            (cf0 == 0xe0 )
        )
    ) {
        int voiceIndex = -1;
        for( int i = 0; i < MAX_ACTIVE_NOTES; ++i ) {
            if( voices[i].keydown && voices[i].channel == channel ) {
                voiceIndex = i;
                break;
            }
        }
        if( voiceIndex >= 0 ) {
            int i = voiceIndex;
            switch(cf0) {
            case 0xe0:
                voices[i].mpePitchBend = (int)( buf[1] | (buf[2] << 7) );
                voices[i].dx7_note->mpePitchBend = (int)( buf[1] | ( buf[2] << 7 ) );
                break;
            }
        }
    }
    else {
        switch(cmd & 0xf0) {
        case 0x80:
            keyup(channel, buf[1], buf[2]);
            return;

        case 0x90:
            if (!synthTuningState->is_standard_tuning() || !buf[2] ||
                !MTS_HasMaster(mtsClient) || !MTS_ShouldFilterNote(mtsClient, buf[1], channel - 1))
                keydown(channel, buf[1], buf[2]);
            return;
            
        case 0xb0: {
            int ctrl  = buf[1];
            int value = buf[2];
            switch(ctrl) {
            case 1:
                controllers.modwheel_cc = value;
                controllers.refresh();
                break;
            case 2:
                controllers.breath_cc = value;
                controllers.refresh();
                break;
            case 4:
                controllers.foot_cc = value;
                controllers.refresh();
                break;
            case 5:
                controllers.portamento_cc = value;
                break;
            case 64:
                sustain = value > 63;
                if (!sustain) {
                    for (int note = 0; note < MAX_ACTIVE_NOTES; note++) {
                        if (voices[note].sustained && !voices[note].keydown) {
                            voices[note].dx7_note->keyup();
                            voices[note].sustained = false;
                        }
                    }
                }
                break;
            case 65:
                controllers.portamento_enable_cc = value >= 64;
                break;
            case 120:
                panic();
                break;
            case 123:
                for (int note = 0; note < MAX_ACTIVE_NOTES; note++)
                    if (voices[note].keydown)
                        keyup(channel, voices[note].midi_note, 0);
                break;
            default:
                TRACE("handle channel %d CC %d = %d", channel, ctrl, value);
                int channel_cc = (channel << 8) | ctrl;
                if ( mappedMidiCC.contains(channel_cc) ) {
                    Ctrl *linkedCtrl = mappedMidiCC[channel_cc];
                    linkedCtrl->publishValueAsync((float) value / 127);
                }
                lastCCUsed.setValue(channel_cc);
            }
            return;
        }
            
        case 0xc0:
            setCurrentProgram(buf[1]);
            return;
            
        case 0xd0:
            controllers.aftertouch_cc = buf[1];
            controllers.refresh();
            return;
        
        case 0xe0:
            controllers.values_[kControllerPitch] = buf[1] | (buf[2] << 7);
            return;
        }
    }
}

#define ACT(v) (v.keydown ? v.midi_note : -1)

int DexedAudioProcessor::chooseNote(uint8_t pitch) {
    int bestNote  = currentNote;
    int bestScore = -1;
    int note      = currentNote;
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        int score = 0;
        if ( !voices[note].dx7_note->isPlaying() ) score += 4;
        if ( !voices[note].keydown )               score += 2;
        if (  voices[note].midi_note == pitch )    score += 1;
        if ( (score > bestScore) || (score == bestScore && voices[note].keydown_seq < voices[bestNote].keydown_seq) ) {
            bestNote  = note;
            bestScore = score;
        }
        note = (note + 1) % MAX_ACTIVE_NOTES;
    }
    return bestNote;
}

void DexedAudioProcessor::keydown(uint8_t channel, uint8_t pitch, uint8_t velo) {
    if ( velo == 0 ) {
        keyup(channel, pitch, velo);
        return;
    }

    pitch += tuningTranspositionShift();
    
    if ( normalizeDxVelocity )
        velo = ((float)velo) * 0.7874015;

    if( controllers.mpeEnabled ) {
        int note = currentNote;
        for( int i = 0; i < MAX_ACTIVE_NOTES; ++i ) {
            if( voices[note].keydown && voices[note].channel == channel )
                controllers.mpeEnabled = false;
            note = (note + 1) % MAX_ACTIVE_NOTES;
        }
    }

    bool triggerLfo = true;
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if ( voices[i].keydown ) { triggerLfo = false; break; }
    }
    if ( triggerLfo ) lfo.keydown();

    int note = chooseNote(pitch);
    currentNote = (note + 1) % MAX_ACTIVE_NOTES;
    voices[note].channel     = channel;
    voices[note].midi_note   = pitch;
    voices[note].velocity    = velo;
    voices[note].sustained   = sustain;
    voices[note].keydown     = true;
    voices[note].keydown_seq = nextKeydownSeq++;
    bool voice_steal = voices[note].dx7_note->isPlaying();
    voices[note].dx7_note->init(data, pitch, velo, channel, &controllers);
    if ( data[136] && !voice_steal )
        voices[note].dx7_note->oscSync();
    if ( (voices[lastActiveVoice].midi_note != -1 && controllers.portamento_enable_cc)
       && controllers.portamento_cc > 0 )
        voices[note].dx7_note->initPortamento(*voices[lastActiveVoice].dx7_note);

    if ( monoMode ) {
        for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
            if ( voices[i].live ) {
                if ( !voices[i].keydown ) {
                    voices[i].live = false;
                    voices[note].dx7_note->transferSignal(*voices[i].dx7_note);
                    break;
                }
                if ( voices[i].midi_note < pitch ) {
                    voices[i].live = false;
                    voices[note].dx7_note->transferState(*voices[i].dx7_note);
                    break;
                }
                return;
            }
        }
    } else if ( !data[136] ) {
        for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
            if ( i != note && voices[i].dx7_note->isPlaying() && voices[i].midi_note == pitch ) {
                voices[note].dx7_note->transferPhase(*voices[i].dx7_note);
                break;
            }
        }
    }

    voices[note].live = true;
    lastActiveVoice   = note;

    // NCS: notify drift engine of new note-on and activate hybrid oscillator
    driftEngine.noteOn(note);
    hybridOscs[note].noteOn(pitch, (float)velo / 127.0f);
}

void DexedAudioProcessor::keyup(uint8_t chan, uint8_t pitch, uint8_t velo) {
    pitch += tuningTranspositionShift();

    int note;
    for (note = 0; note < MAX_ACTIVE_NOTES; ++note) {
        if ( ( ( controllers.mpeEnabled && voices[note].channel == chan ) ||
               (!controllers.mpeEnabled && voices[note].midi_note == pitch) ) &&
             voices[note].keydown )
        {
            voices[note].keydown = false;
            break;
        }
    }
    
    if ( note >= MAX_ACTIVE_NOTES ) {
        TRACE("note found ??? %d", pitch);
        return;
    }
    
    if ( monoMode ) {
        int highNote = -1;
        int target   = 0;
        for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
            if ( voices[i].keydown && voices[i].midi_note > highNote ) {
                target   = i;
                highNote = voices[i].midi_note;
            }
        }
        if ( highNote != -1 && voices[note].live ) {
            voices[note].live = false;
            voices[target].live = true;
            voices[target].dx7_note->transferState(*voices[note].dx7_note);
        }
    }
    
    if ( sustain ) {
        voices[note].sustained = true;
    } else {
        voices[note].dx7_note->keyup();
        // NCS: deactivate hybrid oscillator on key up
        hybridOscs[note].noteOff();
    }
}

int DexedAudioProcessor::tuningTranspositionShift() {
    if( synthTuningState->is_standard_tuning() || !controllers.transpose12AsScale )
        return data[144] - 24;
    else {
        int d144 = data[144];
        if( d144 % 12 == 0 ) {
            int oct = (d144 - 24) / 12;
            return oct * synthTuningState->scale_length();
        } else
            return data[144] - 24;
    }
}

void DexedAudioProcessor::panic() {
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        voices[i].midi_note = -1;
        voices[i].keydown   = false;
        voices[i].live      = false;
        if ( voices[i].dx7_note != NULL )
            voices[i].dx7_note->oscSync();
        // NCS: reset hybrid oscillators on panic
        hybridOscs[i].reset();
    }
    keyboardState.reset();
}

void DexedAudioProcessor::handleIncomingMidiMessage(MidiInput* source, const MidiMessage& message) {
    if ( message.isActiveSense() ) return;

#ifdef IMPLEMENT_MidiMonitor
    sysexComm.inActivity = true;
#endif

    const uint8 *buf = message.getRawData();
    int sz = message.getRawDataSize();

    if ( !message.isSysEx() ) return;
    if ( buf[1] != 0x43 ) {
        TRACE("not a yamaha sysex %d", buf[1]);
        return;
    }

    int substatus = buf[2] >> 4;
    switch(substatus) {
        case 0: {
            if ( buf[3] == 0 ) {
                if ( sz < 156 ) { TRACE("wrong single voice datasize %d", sz); return; }
                if ( updateProgramFromSysex(buf+6) ) TRACE("bad checksum");
            }
            if ( buf[3] == 9 ) {
                if ( sz < 4104 ) { TRACE("wrong 32 voice dump data size %d", sz); return; }
                Cartridge received;
                if ( received.load(buf, sz) == 0 ) {
                    loadCartridge(received);
                    setCurrentProgram(0);
                }
            }
        } break;
        case 1: {
            if ( sz < 7 ) { TRACE("wrong single voice datasize %d", sz); return; }
            uint8 offset = (buf[3] << 7) + buf[4];
            uint8 value  = buf[5];
            TRACE("parameter change message offset:%d value:%d", offset, value);
            if ( offset > 155 ) { TRACE("wrong offset size"); return; }
            if ( offset == 155 ) unpackOpSwitch(value);
            else data[offset] = value;
        } break;
        case 2: {
            if ( buf[3] == 0 ) sendCurrentSysexProgram();
            else if ( buf[3] == 9 ) sendCurrentSysexCartridge();
            else TRACE("Unknown voice request: %d", buf[3]);
        } return;
        default:
            TRACE("unknown sysex substatus: %d", substatus);
            return;
    }

    forceRefreshUI = true;
    triggerAsyncUpdate();
}

int DexedAudioProcessor::getEngineType() {
    return engineType;
}

void DexedAudioProcessor::setEngineType(int tp) {
    TRACE("settings engine %d", tp);
    switch (tp) {
        case DEXED_ENGINE_MARKI:  controllers.core = &engineMkI; break;
        case DEXED_ENGINE_OPL:    controllers.core = &engineOpl; break;
        default:                  controllers.core = &engineMsfa; break;
    }
    engineType = tp;
}

void DexedAudioProcessor::setMonoMode(bool mode) {
    panic();
    monoMode = mode;
}

bool DexedAudioProcessor::peekVoiceStatus() {
    if ( currentNote == -1 ) return false;
    int note = currentNote;
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (voices[note].keydown) {
            voices[note].dx7_note->peekVoiceStatus(voiceStatus);
            return true;
        }
        if ( --note < 0 ) note = MAX_ACTIVE_NOTES - 1;
    }
    note = currentNote;
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (voices[note].live) {
            voices[note].dx7_note->peekVoiceStatus(voiceStatus);
            return true;
        }
        if ( --note < 0 ) note = MAX_ACTIVE_NOTES - 1;
    }
    return true;
}

const String DexedAudioProcessor::getInputChannelName  (int i) const { return String(i + 1); }
const String DexedAudioProcessor::getOutputChannelName (int i) const { return String(i + 1); }
bool DexedAudioProcessor::isInputChannelStereoPair  (int) const { return true; }
bool DexedAudioProcessor::isOutputChannelStereoPair (int) const { return true; }

bool DexedAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
    return layouts.getMainOutputChannelSet() == AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

bool DexedAudioProcessor::acceptsMidi() const { return true; }
bool DexedAudioProcessor::producesMidi() const { return true; }
bool DexedAudioProcessor::silenceInProducesSilenceOut() const { return false; }
double DexedAudioProcessor::getTailLengthSeconds() const { return 0.0; }
const String DexedAudioProcessor::getName() const { return JucePlugin_Name; }

bool DexedAudioProcessor::hasEditor() const { return true; }

void DexedAudioProcessor::updateUI() {
    updateHostDisplay();
    AudioProcessorEditor *editor = getActiveEditor();
    if ( editor == NULL ) return;
    DexedAudioProcessorEditor *dexedEditor = (DexedAudioProcessorEditor *) editor;
    dexedEditor->updateUI();
}

AudioProcessorEditor* DexedAudioProcessor::createEditor() {
    return new DexedAudioProcessorEditor(this);
}

void DexedAudioProcessor::setZoomFactor(float factor) { zoomFactor = factor; }
void DexedAudioProcessor::handleAsyncUpdate() { updateUI(); }

void dexed_trace(const char *source, const char *fmt, ...) {
    char output[4096];
    va_list argptr;
    va_start(argptr, fmt);
    vsnprintf(output, 4095, fmt, argptr);
    va_end(argptr);
    String dest;
    dest << source << " " << output;
    Logger::writeToLog(dest);
}

void DexedAudioProcessor::resetTuning(std::shared_ptr<TuningState> t) {
    synthTuningState = t;
    synthTuningStateLast = t;
    for( int i = 0; i < MAX_ACTIVE_NOTES; ++i )
        if( voices[i].dx7_note != nullptr )
            voices[i].dx7_note->tuning_state_ = synthTuningState;
}

void DexedAudioProcessor::retuneToStandard() {
    currentSCLData = "";
    currentKBMData = "";
    resetTuning(createStandardTuning());
}

void DexedAudioProcessor::applySCLTuning() {
    FileChooser fc( "Please select a scale (.scl) file.", File(), "*.scl" );
    File s;
    for (;;) {
        if (!fc.browseForFileToOpen()) return;
        s = fc.getResult();
        if (s.getFileExtension() != ".scl") {
            AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Invalid file type!", "Only files with the \".scl\" extension (in lowercase!) are allowed.");
            continue;
        }
        if (s.getSize() > MAX_SCL_KBM_FILE_SIZE) {
            AlertWindow::showMessageBox(AlertWindow::WarningIcon, "File size error!",
                "File size exceeded the maximum limit of " + std::to_string(MAX_SCL_KBM_FILE_SIZE) + " bytes.");
            continue;
        }
        if (s.getSize() == 0) {
            AlertWindow::showMessageBox(AlertWindow::WarningIcon, "File size error!", "File is empty.");
            continue;
        }
        applySCLTuning(s);
        break;
    }
}

void DexedAudioProcessor::applySCLTuning(File s) {
    applySCLTuning(s.loadFileAsString().toStdString());
}

void DexedAudioProcessor::applySCLTuning(std::string sclcontents) {
    if( currentKBMData.size() < 1 ) {
        auto t = createTuningFromSCLData(sclcontents);
        if (t) { resetTuning(t); currentSCLData = sclcontents; synthTuningStateLast = t; }
        else     resetTuning(synthTuningStateLast);
    } else {
        auto t = createTuningFromSCLAndKBMData(sclcontents, currentKBMData);
        if (t) { resetTuning(t); currentSCLData = sclcontents; synthTuningStateLast = t; }
        else     resetTuning(synthTuningStateLast);
    }
}

void DexedAudioProcessor::applyKBMMapping() {
    FileChooser fc( "Please select a keyboard map (.kbm) file.", File(), "*.kbm" );
    File s;
    for (;;) {
        if (!fc.browseForFileToOpen()) return;
        s = fc.getResult();
        if (s.getFileExtension() != ".kbm") {
            AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Invalid file type!", "Only files with the \".kbm\" extension (in lowercase!) are allowed.");
            continue;
        }
        if (s.getSize() > MAX_SCL_KBM_FILE_SIZE) {
            AlertWindow::showMessageBox(AlertWindow::WarningIcon, "File size error!",
                "File size exceeded the maximum limit of " + std::to_string(MAX_SCL_KBM_FILE_SIZE) + " bytes.");
            continue;
        }
        if (s.getSize() == 0) {
            AlertWindow::showMessageBox(AlertWindow::WarningIcon, "File size error!", "File is empty.");
            continue;
        }
        applyKBMMapping(s);
        break;
    }
}

void DexedAudioProcessor::applyKBMMapping(File s) {
    applyKBMMapping(s.loadFileAsString().toStdString());
}

void DexedAudioProcessor::applyKBMMapping(std::string kbmcontents) {
    if( currentSCLData.size() < 1 ) {
        auto t = createTuningFromKBMData(kbmcontents);
        if (t) { resetTuning(t); currentKBMData = kbmcontents; synthTuningStateLast = t; }
        else     resetTuning(synthTuningStateLast);
    } else {
        auto t = createTuningFromSCLAndKBMData(currentSCLData, kbmcontents);
        if (t) { resetTuning(t); currentKBMData = kbmcontents; synthTuningStateLast = t; }
        else     resetTuning(synthTuningStateLast);
    }
}
