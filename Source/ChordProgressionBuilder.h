#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <array>
#include <vector>

class DexedAudioProcessor;

class ChordProgressionBuilder : public juce::Component,
                                private juce::Timer
{
public:
    explicit ChordProgressionBuilder(DexedAudioProcessor& processorToUse);
    ~ChordProgressionBuilder() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    DexedAudioProcessor& processor;

    juce::ComboBox rootSelector;
    juce::ComboBox modeSelector;
    juce::ComboBox rateSelector;
    juce::TextButton playButton { "Play" };

    std::array<juce::ComboBox, 4> progressionSelectors;
    std::array<juce::Rectangle<int>, 4> progressionStepBounds;
    std::vector<int> activeNotes;

    bool isPlaying = false;
    int activeStep = -1;

    void timerCallback() override;
    void refreshProgressionChoices(bool resetToDefaults);
    void updateUIState();
    void togglePlayback();
    void advancePlayback();
    void stopPlayback();
    void stopActiveNotes();

    juce::String getChordNameForStep(int stepIndex) const;
    std::vector<int> getChordNotesForStep(int stepIndex) const;
    int getStepDurationMs() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordProgressionBuilder)
};
