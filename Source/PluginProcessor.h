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

#ifndef PLUGINPROCESSOR_H_INCLUDED
#define PLUGINPROCESSOR_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

#include "clap-juce-extensions/clap-juce-extensions.h"

#include "msfa/controllers.h"
#include "msfa/dx7note.h"
#include "msfa/lfo.h"
#include "msfa/synth.h"
#include "msfa/fm_core.h"
#include "msfa/tuning.h"
#include "PluginParam.h"
#include "PluginData.h"
#include "PluginFx.h"
#include "SysexComm.h"
#include "EngineMkI.h"
#include "EngineOpl.h"

// NCS: Harmonic, bass, drift, hybrid oscillator modules
#include "HarmonicEngine.h"
#include "BassAccompaniment.h"
#include "DriftEngine.h"
#include "HybridOscillator.h"

struct ProcessorVoice {
    int channel;
    int midi_note;
    int velocity;
    bool keydown;
    bool sustained;
    bool live;
    int32_t keydown_seq;

    int mpePitchBend;
    Dx7Note *dx7_note;
};

enum DexedEngineResolution {
    DEXED_ENGINE_MODERN,
    DEXED_ENGINE_MARKI,
    DEXED_ENGINE_OPL
};

/// Maximum allowed size for SCL and KBM files.
const int MAX_SCL_KBM_FILE_SIZE = 16384;

//==============================================================================
class DexedAudioProcessor  : public AudioProcessor, public AsyncUpdater, public MidiInputCallback, public clap_juce_extensions::clap_properties
{
    static const int MAX_ACTIVE_NOTES = 16;
    ProcessorVoice voices[MAX_ACTIVE_NOTES];
    int currentNote;

    Lfo lfo;

    bool sustain;
    bool monoMode;
    
    float extra_buf[N];
    int extra_buf_size;

    int currentProgram;
    long lastStateSave;
    PluginFx fx;

    bool refreshVoice;
    bool normalizeDxVelocity;
    bool sendSysexChange;
    
    void processMidiMessage(const MidiMessage *msg);
    void keydown(uint8_t chan, uint8_t pitch, uint8_t velo);
    void keyup(uint8_t, uint8_t pitch, uint8_t velo);
    void handleAsyncUpdate() override;
    void initCtrl();

    MidiMessage* nextMidi, *midiMsg;
    bool hasMidiMessage;
    int midiEventPos;
    bool getNextEvent(MidiBuffer::Iterator* iter, const int samplePos);
    
    void handleIncomingMidiMessage(MidiInput* source, const MidiMessage& message) override;
    uint32_t engineType;
    
    FmCore engineMsfa;
    EngineMkI engineMkI;
    EngineOpl engineOpl;

    void resolvAppDir();
    void unpackOpSwitch(char packOpValue);
    void packOpSwitch();

    float zoomFactor = 1;

    // NCS: last chord root played by harmonicEngine (for bassEngine handoff)
    int ncsLastChordRoot = -1;
    std::vector<int> ncsLastChordNotes;

public:
    // in MIDI units (0x4000 is neutral)
    Controllers controllers;
    StringArray programNames;
    Cartridge currentCart;
    uint8_t data[161];

    SysexComm sysexComm;
    VoiceStatus voiceStatus;
    File activeFileCartridge;
    
    bool forceRefreshUI;
    float vuSignal;
    double vuDecayFactor = 0.999361;
    bool showKeyboard;
    int getEngineType();
    void setEngineType(int rs);
    
    HashMap<int, Ctrl*> mappedMidiCC;
    Array<Ctrl*> ctrl;

    OperatorCtrl opCtrl[6];
    std::unique_ptr<CtrlDX> pitchEgRate[4];
    std::unique_ptr<CtrlDX> pitchEgLevel[4];
    std::unique_ptr<CtrlDX> pitchModSens;
    std::unique_ptr<CtrlDX> algo;
    std::unique_ptr<CtrlDX> oscSync;
    std::unique_ptr<CtrlDX> feedback;
    std::unique_ptr<CtrlDX> lfoRate;
    std::unique_ptr<CtrlDX> lfoDelay;
    std::unique_ptr<CtrlDX> lfoAmpDepth;
    std::unique_ptr<CtrlDX> lfoPitchDepth;
    std::unique_ptr<CtrlDX> lfoWaveform;
    std::unique_ptr<CtrlDX> lfoSync;
    std::unique_ptr<CtrlDX> transpose;

    std::unique_ptr<CtrlFloat> fxCutoff;
    std::unique_ptr<CtrlFloat> fxReso;
    std::unique_ptr<CtrlFloat> output;
    std::unique_ptr<Ctrl> tune;
    std::unique_ptr<Ctrl> monoModeCtrl;

    // NCS: public engine instances so UI panels can configure them
    HarmonicEngine  harmonicEngine;
    BassAccompaniment bassEngine;
    DriftEngine     driftEngine;
    std::array<HybridOscillator, MAX_ACTIVE_NOTES> hybridOscs;
    float hybridMixLevel = 0.3f;   // 0 = FM only, 1 = full hybrid mix

    void loadCartridge(Cartridge &cart);
    void setDxValue(int offset, int v);

    //==============================================================================
    DexedAudioProcessor();
    ~DexedAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override;
    void panic();
    bool isMonoMode() { return monoMode; }
    void setMonoMode(bool mode);
    
    void copyToClipboard(int srcOp);
    void pasteOpFromClipboard(int destOp);
    void pasteEnvFromClipboard(int destOp);
    void sendCurrentSysexProgram();
    void sendCurrentSysexCartridge();
    void sendSysexCartridge(File cart);
    
    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    void updateUI();
    bool peekVoiceStatus();
    int updateProgramFromSysex(const uint8 *rawdata);
    void setupStartupCart();
    
    //==============================================================================
    const String getName() const override;
    int getNumParameters() override;
    float getParameter (int index) override;
    void setParameter (int index, float newValue) override;
    const String getParameterName (int index) override;
    const String getParameterText (int index) override;
    String getParameterID (int index) override;

    const String getInputChannelName (int channelIndex) const override;
    const String getOutputChannelName (int channelIndex) const override;
    bool isInputChannelStereoPair (int index) const override;
    bool isOutputChannelStereoPair (int index) const override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool silenceInProducesSilenceOut() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const String getProgramName (int index) override;
    void changeProgramName(int index, const String& newName) override;
    void resetToInitVoice();
    
    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    MidiKeyboardState keyboardState;
    void unbindUI();

    void loadPreference();
    void savePreference();
    
    static File dexedAppDir;
    static File dexedCartDir;

    Value lastCCUsed;
    int lastActiveVoice = 0;

    MTSClient *mtsClient;
    std::shared_ptr<TuningState> synthTuningState;
    std::shared_ptr<TuningState> synthTuningStateLast;

    void applySCLTuning();
    void applyKBMMapping();
    void applySCLTuning(File sclf);
    void applyKBMMapping(File kbmf);
    void applySCLTuning(std::string scld);
    void applyKBMMapping(std::string kbmd);
    
    void retuneToStandard();
    void resetTuning(std::shared_ptr<TuningState> t);
    int tuningTranspositionShift();
    
    std::string currentSCLData = "";
    std::string currentKBMData = "";
    void setZoomFactor(float factor);
    float getZoomFactor() { return zoomFactor; }

private:
    int chooseNote(uint8_t pitch);
    int32_t nextKeydownSeq;;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DexedAudioProcessor)
};

#endif  // PLUGINPROCESSOR_H_INCLUDED
