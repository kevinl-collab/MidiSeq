#pragma once
#include "PluginProcessor.h"
#include "UI/EditPanel.h"
#include "UI/EditorState.h"
#include "UI/GlobalPanel.h"
#include "UI/PadsPanel.h"
#include "UI/SeqLookAndFeel.h"
#include "UI/TopBar.h"
#include "UI/TrackArea.h"

/** The whole UI is laid out at 960x540 inside `content`, which is scaled to the window size (75-200%). */
class MelodicSequencerEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit MelodicSequencerEditor (MelodicSequencerProcessor&);
    ~MelodicSequencerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { topBar.updateTransport(); sceneTabs.updateQueued(); }

    SeqLookAndFeel laf;
    EditorState state;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Component content;
    TopBar topBar;
    SceneTabs sceneTabs;
    TrackArea trackArea;
    PadsPanel pads;
    EditPanel editPanel;
    GlobalPanel globalPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodicSequencerEditor)
};
