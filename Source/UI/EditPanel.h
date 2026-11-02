#pragma once
#include "EditorState.h"
#include "Widgets.h"

/** Sub-hits of a step on a 0-5 timeline (in step lengths). */
class RepeatDisplay : public juce::Component
{
public:
    void set (const ms::Step& s, double stepLenPpq) { step = s; lenPpq = stepLenPpq; repaint(); }
    void paint (juce::Graphics&) override;

private:
    ms::Step step;
    double lenPpq = 0.25;
};

class EditPanel : public juce::Component, private juce::ChangeListener
{
public:
    explicit EditPanel (EditorState&);
    ~EditPanel() override { state.removeChangeListener (this); }

    void paint (juce::Graphics& g) override { paintPanel (g, getLocalBounds()); }
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void editParam (StepParam, float);
    void editTrack (const juce::String& name, int field, std::function<void (ms::Track&)>);

    EditorState& state;
    juce::Label header;
    juce::ComboBox lenBox, shuffleBox, chokeBox;
    juce::TextButton linkLen { "Link" }, linkShuffle { "Link" }, followBtn { "Follow transpose" };
    juce::Slider shuffleAmount;
    juce::TextButton dirButtons[6];
    RepeatDisplay repeatDisplay;
    juce::Label dirLabel, chokeLabel;
    std::array<std::unique_ptr<LabeledKnob>, kNumStepParams> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditPanel)
};
