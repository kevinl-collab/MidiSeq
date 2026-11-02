#pragma once
#include "EditorState.h"
#include "Widgets.h"

/** One octave. Keys are pitch classes; lit keys are in the scale (relative to the root). Click to toggle. */
class PianoKeyboard : public juce::Component
{
public:
    std::function<void (int pitchClass)> onKeyClicked;

    void setScale (uint16_t newMask, int newRoot)
    {
        if (newMask == mask && newRoot == root) return;
        mask = newMask;
        root = newRoot;
        repaint();
    }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int keyAt (juce::Point<int>) const;
    juce::Rectangle<float> keyRect (int pitchClass) const;
    bool inScale (int pc) const { return (mask >> (((pc - root) % 12 + 12) % 12)) & 1u; }

    uint16_t mask = 0xFFF;
    int root = 0;
};

class GlobalPanel : public juce::Component, private juce::ChangeListener
{
public:
    explicit GlobalPanel (EditorState&);
    ~GlobalPanel() override { state.removeChangeListener (this); }

    void paint (juce::Graphics& g) override { paintPanel (g, getLocalBounds()); }
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void rebuildScaleItems();
    void promptSaveScale();

    EditorState& state;
    juce::Label title;
    juce::ComboBox shuffleBox, scaleBox, rootBox, channelBox, transposeModeBox;
    juce::Slider shuffleAmount, rangeLow, rangeHigh;
    juce::TextButton saveScaleBtn { "+" }, midiTransposeBtn { "MIDI Tr" };
    PianoKeyboard keyboard;
    LabeledKnob offsetKnob { "Offset" }, transposeKnob { "Transpose" };
    juce::Label rangeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shuffleTypeAtt, transposeModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shuffleAmountAtt, offsetAtt, transposeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalPanel)
};
