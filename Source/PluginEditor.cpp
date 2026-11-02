#include "PluginEditor.h"

MelodicSequencerEditor::MelodicSequencerEditor (MelodicSequencerProcessor& p)
    : AudioProcessorEditor (&p),
      state (p),
      topBar (state), sceneTabs (state), trackArea (state), pads (state), editPanel (state), globalPanel (state)
{
    setLookAndFeel (&laf);
    state.lnf = &laf;

    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<juce::Component*> { &topBar, &sceneTabs, &trackArea, &pads, &editPanel, &globalPanel })
        content.addAndMakeVisible (c);

    setResizable (true, true);
    setResizeLimits (theme::designWidth * 3 / 4, theme::designHeight * 3 / 4, theme::designWidth * 2, theme::designHeight * 2);   // 75% - 200%
    getConstrainer()->setFixedAspectRatio ((double) theme::designWidth / theme::designHeight);
    setSize (theme::designWidth, theme::designHeight);
    startTimerHz (20);
}

MelodicSequencerEditor::~MelodicSequencerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MelodicSequencerEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void MelodicSequencerEditor::resized()
{
    content.setTransform ({});
    content.setBounds (0, 0, theme::designWidth, theme::designHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) theme::designWidth));

    topBar.setBounds (4, 4, 952, 32);
    sceneTabs.setBounds (4, 38, 400, 24);
    trackArea.setBounds (4, 64, 952, 286);
    pads.setBounds (4, 354, 190, 182);
    editPanel.setBounds (198, 354, 440, 182);
    globalPanel.setBounds (642, 354, 314, 182);
}
