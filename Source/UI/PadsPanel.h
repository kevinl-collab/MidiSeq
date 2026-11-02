#pragma once
#include "EditorState.h"
#include "Widgets.h"

/** 4x3 pad grid mirroring the first 12 tracks. Press to audition (MIDI out via the audio thread). */
class PadsPanel : public juce::Component, private juce::ChangeListener
{
public:
    explicit PadsPanel (EditorState&);
    ~PadsPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Pad;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showPresetMenu();
    void promptSaveAs();
    void refreshPresetButton();

    EditorState& state;
    juce::Label title;
    juce::TextButton presetBtn;
    juce::ComboBox recordModeBox;
    juce::Slider quantSlider;
    juce::OwnedArray<Pad> pads;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> quantAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadsPanel)
};

/** Scene tabs (click = launch, quantized), blinking while queued; copy / paste / duplicate / clear; launch quantize. */
class SceneTabs : public juce::Component, private juce::ChangeListener
{
public:
    explicit SceneTabs (EditorState&);
    ~SceneTabs() override { state.removeChangeListener (this); }

    void resized() override;
    void updateQueued();   // from the editor timer: blink the queued scene

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();

    EditorState& state;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::TextButton plus { "+" }, copyBtn { "Copy" }, pasteBtn { "Paste" }, dupBtn { "Dup" }, clearBtn { "Clear" };
    juce::ComboBox quantBox;
};
