#include "ChordProgressionBuilder.h"

#include "DXLookNFeel.h"
#include "PluginProcessor.h"

namespace
{
const char* rootNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const char* majorDegrees[] = { "I", "ii", "iii", "IV", "V", "vi", "vii°" };
const char* minorDegrees[] = { "i", "ii°", "III", "iv", "v", "VI", "VII" };
const int majorScaleOffsets[] = { 0, 2, 4, 5, 7, 9, 11 };
const int minorScaleOffsets[] = { 0, 2, 3, 5, 7, 8, 10 };
}

ChordProgressionBuilder::ChordProgressionBuilder(DexedAudioProcessor& processorToUse)
    : processor(processorToUse)
{
    setInterceptsMouseClicks(true, true);

    rootSelector.setTitle("Chord progression root note");
    for (int i = 0; i < 12; ++i)
        rootSelector.addItem(rootNames[i], i + 1);
    rootSelector.setSelectedId(1, juce::dontSendNotification);
    rootSelector.onChange = [this] { repaint(); };
    addAndMakeVisible(rootSelector);

    modeSelector.setTitle("Chord progression mode");
    modeSelector.addItem("Major", 1);
    modeSelector.addItem("Minor", 2);
    modeSelector.setSelectedId(1, juce::dontSendNotification);
    modeSelector.onChange = [this] { refreshProgressionChoices(false); };
    addAndMakeVisible(modeSelector);

    rateSelector.setTitle("Chord playback speed");
    rateSelector.addItem("Slow", 1);
    rateSelector.addItem("Medium", 2);
    rateSelector.addItem("Fast", 3);
    rateSelector.setSelectedId(2, juce::dontSendNotification);
    rateSelector.onChange = [this]
    {
        if (isPlaying)
            startTimer(getStepDurationMs());
    };
    addAndMakeVisible(rateSelector);

    playButton.setTitle("Play or stop the chord progression");
    playButton.onClick = [this] { togglePlayback(); };
    addAndMakeVisible(playButton);

    for (int i = 0; i < (int) progressionSelectors.size(); ++i)
    {
        progressionSelectors[(size_t) i].setTitle("Chord progression step " + juce::String(i + 1));
        progressionSelectors[(size_t) i].onChange = [this] { repaint(); };
        addAndMakeVisible(progressionSelectors[(size_t) i]);
    }

    refreshProgressionChoices(true);
}

ChordProgressionBuilder::~ChordProgressionBuilder()
{
    stopPlayback();
}

void ChordProgressionBuilder::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    juce::ColourGradient panelGradient(juce::Colour(0xff121722), bounds.getTopLeft(),
                                       juce::Colour(0xff26202c), bounds.getBottomRight(), false);
    panelGradient.addColour(0.55, juce::Colour(0xff172438));
    g.setGradientFill(panelGradient);
    g.fillRoundedRectangle(bounds, 18.0f);

    g.setColour(juce::Colour(0x6647d8c7));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 18.0f, 1.5f);

    g.setColour(juce::Colour(0x20ffb86b));
    g.fillRoundedRectangle(bounds.removeFromTop(28.0f), 18.0f);

    g.setColour(juce::Colour(0xffffd28c));
    g.setFont(juce::Font(17.0f, juce::Font::bold));
    g.drawText("Chord Progression", 16, 10, 180, 20, juce::Justification::centredLeft, false);

    g.setColour(juce::Colour(0xff8ea2bb));
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText("Juno warmth meets Serum polish", 16, 30, 200, 16, juce::Justification::centredLeft, false);

    g.setColour(juce::Colour(0x55ffffff));
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawText("Root", rootSelector.getX(), 10, rootSelector.getWidth(), 16, juce::Justification::centred, false);
    g.drawText("Mode", modeSelector.getX(), 10, modeSelector.getWidth(), 16, juce::Justification::centred, false);
    g.drawText("Rate", rateSelector.getX(), 10, rateSelector.getWidth(), 16, juce::Justification::centred, false);

    for (int i = 0; i < (int) progressionStepBounds.size(); ++i)
    {
        auto stepBounds = progressionStepBounds[(size_t) i];
        auto stepArea = stepBounds.toFloat();
        auto activeFill = i == activeStep ? juce::Colour(0x3347d8c7) : juce::Colour(0x16ffffff);

        g.setColour(activeFill);
        g.fillRoundedRectangle(stepArea, 14.0f);

        g.setColour(i == activeStep ? juce::Colour(0xff47d8c7) : juce::Colour(0x33ffffff));
        g.drawRoundedRectangle(stepArea, 14.0f, i == activeStep ? 2.0f : 1.0f);

        g.setColour(juce::Colour(0xff91a4bc));
        g.setFont(juce::Font(10.0f, juce::Font::plain));
        g.drawText("STEP " + juce::String(i + 1),
                   stepBounds.getX() + 10,
                   stepBounds.getY() + 6,
                   stepBounds.getWidth() - 20,
                   12,
                   juce::Justification::centredLeft,
                   false);

        g.setColour(juce::Colour(0xffffd28c));
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(getChordNameForStep(i),
                   stepBounds.getX() + 10,
                   stepBounds.getBottom() - 26,
                   stepBounds.getWidth() - 20,
                   16,
                   juce::Justification::centredLeft,
                   false);
    }
}

void ChordProgressionBuilder::resized()
{
    auto area = getLocalBounds().reduced(14, 12);
    auto controls = area.removeFromTop(42);
    auto progression = area.removeFromBottom(46);

    auto titleSpace = controls.removeFromLeft(220);
    juce::ignoreUnused(titleSpace);

    auto controlWidth = 86;
    rootSelector.setBounds(controls.removeFromLeft(controlWidth).reduced(4, 8));
    modeSelector.setBounds(controls.removeFromLeft(controlWidth + 10).reduced(4, 8));
    rateSelector.setBounds(controls.removeFromLeft(controlWidth).reduced(4, 8));
    playButton.setBounds(controls.removeFromRight(98).reduced(4, 7));

    const int gap = 10;
    const int stepWidth = (progression.getWidth() - (gap * 3)) / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto stepBounds = progression.removeFromLeft(stepWidth);
        if (i < 3)
            progression.removeFromLeft(gap);

        progressionStepBounds[(size_t) i] = stepBounds;
        progressionSelectors[(size_t) i].setBounds(stepBounds.reduced(10, 18).withHeight(24));
    }
}

void ChordProgressionBuilder::timerCallback()
{
    advancePlayback();
}

void ChordProgressionBuilder::refreshProgressionChoices(bool resetToDefaults)
{
    const bool isMinorMode = modeSelector.getSelectedId() == 2;
    auto labels = isMinorMode ? minorDegrees : majorDegrees;
    const int defaults[4] = { 1, isMinorMode ? 6 : 6, isMinorMode ? 3 : 4, isMinorMode ? 7 : 5 };

    for (int i = 0; i < (int) progressionSelectors.size(); ++i)
    {
        auto& selector = progressionSelectors[(size_t) i];
        const auto current = selector.getSelectedId();

        selector.clear(juce::dontSendNotification);
        for (int degree = 0; degree < 7; ++degree)
            selector.addItem(labels[degree], degree + 1);

        selector.setSelectedId(resetToDefaults ? defaults[i] : juce::jlimit(1, 7, current), juce::dontSendNotification);
    }

    repaint();
}

void ChordProgressionBuilder::updateUIState()
{
    playButton.setButtonText(isPlaying ? "Stop" : "Play");
    repaint();
}

void ChordProgressionBuilder::togglePlayback()
{
    if (isPlaying)
    {
        stopPlayback();
        return;
    }

    isPlaying = true;
    activeStep = -1;
    updateUIState();
    advancePlayback();
    startTimer(getStepDurationMs());
}

void ChordProgressionBuilder::advancePlayback()
{
    stopActiveNotes();

    activeStep = (activeStep + 1) % (int) progressionSelectors.size();
    activeNotes = getChordNotesForStep(activeStep);

    for (auto note : activeNotes)
        processor.keyboardState.noteOn(1, note, 0.85f);

    repaint();
}

void ChordProgressionBuilder::stopPlayback()
{
    stopTimer();
    stopActiveNotes();
    isPlaying = false;
    activeStep = -1;
    updateUIState();
}

void ChordProgressionBuilder::stopActiveNotes()
{
    for (auto note : activeNotes)
        processor.keyboardState.noteOff(1, note, 0.0f);

    activeNotes.clear();
}

juce::String ChordProgressionBuilder::getChordNameForStep(int stepIndex) const
{
    const bool isMinorMode = modeSelector.getSelectedId() == 2;
    const int selectedDegree = progressionSelectors[(size_t) stepIndex].getSelectedId() - 1;
    const int safeDegree = juce::jlimit(0, 6, selectedDegree);
    const auto& scaleOffsets = isMinorMode ? minorScaleOffsets : majorScaleOffsets;
    const int rootIndex = (rootSelector.getSelectedId() - 1 + scaleOffsets[safeDegree]) % 12;

    const bool isDiminished = (isMinorMode && safeDegree == 1) || (!isMinorMode && safeDegree == 6);
    const bool isMinor = isMinorMode ? (safeDegree == 0 || safeDegree == 3 || safeDegree == 4)
                                     : (safeDegree == 1 || safeDegree == 2 || safeDegree == 5);

    juce::String suffix = isDiminished ? "dim" : (isMinor ? "min" : "maj");
    return juce::String(rootNames[rootIndex]) + suffix;
}

std::vector<int> ChordProgressionBuilder::getChordNotesForStep(int stepIndex) const
{
    const bool isMinorMode = modeSelector.getSelectedId() == 2;
    const int selectedDegree = progressionSelectors[(size_t) stepIndex].getSelectedId() - 1;
    const int safeDegree = juce::jlimit(0, 6, selectedDegree);
    const auto& scaleOffsets = isMinorMode ? minorScaleOffsets : majorScaleOffsets;

    const bool isDiminished = (isMinorMode && safeDegree == 1) || (!isMinorMode && safeDegree == 6);
    const bool isMinor = isMinorMode ? (safeDegree == 0 || safeDegree == 3 || safeDegree == 4)
                                     : (safeDegree == 1 || safeDegree == 2 || safeDegree == 5);

    const int baseRoot = 48 + (rootSelector.getSelectedId() - 1) + scaleOffsets[safeDegree];
    const int third = baseRoot + (isDiminished || isMinor ? 3 : 4);
    const int fifth = baseRoot + (isDiminished ? 6 : 7);

    return { baseRoot, third, fifth, baseRoot + 12 };
}

int ChordProgressionBuilder::getStepDurationMs() const
{
    switch (rateSelector.getSelectedId())
    {
        case 1: return 800;
        case 3: return 360;
        default: return 560;
    }
}
