#pragma once
#include "EditorState.h"
#include "Widgets.h"

/** Pop-over for the "Generate" action: fills a melodic track with in-scale notes. Every change regenerates live
    (one undo step per tuning session); the dice button rolls a new seed as its own undo step. */
class GeneratorPanel : public juce::Component
{
public:
    GeneratorPanel (EditorState&, int track);
    ~GeneratorPanel() override;

    void paint (juce::Graphics& g) override { g.fillAll (theme::panel); }
    void resized() override;

private:
    void apply (bool ownUndoStep);
    void pull();   // settings -> controls
    void push();   // controls -> settings

    EditorState& state;
    int track;
    ms::GeneratorSettings s;
    bool updating = false;

    juce::ComboBox presetBox, arpBox;
    juce::Slider low, high, density, leap, root, gate, seed;
    juce::Label lLow, lHigh, lDensity, lLeap, lRoot, lGate, lSeed, lPreset, lArp, hint;
    juce::TextButton dice { "Dice" };
};
