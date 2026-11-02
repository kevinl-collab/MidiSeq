#include "TopBar.h"
#include "../MidiExport.h"

namespace
{
juce::String scaleNameFor (const ms::Global& g)
{
    if (! g.scaleEnabled) return "No scale";
    for (const auto& s : ms::kBuiltInScales)
        if (s.mask == g.scaleMask) return juce::String (theme::rootName (g.root)) + " " + s.name;
    return juce::String (theme::rootName (g.root)) + " Custom";
}
} // namespace

TopBar::TopBar (EditorState& s)
    : state (s),
      playAtt (s.proc.apvts, "internalRun", playBtn),
      clockAtt (s.proc.apvts, "internalClock", clockBtn),
      recAtt (s.proc.apvts, "recordArm", recBtn),
      metAtt (s.proc.apvts, "metronome", metBtn),
      bpmAtt (s.proc.apvts, "internalBpm", bpmSlider)
{
    title.setText ("MELODIC SEQ", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, theme::accent);
    addAndMakeVisible (title);

    keyBtn.onClick = [this] { showKeyMenu(); };
    addAndMakeVisible (keyBtn);

    undoBtn.onClick = [this] { state.undo.undo(); };
    redoBtn.onClick = [this] { state.undo.redo(); };
    addAndMakeVisible (undoBtn);
    addAndMakeVisible (redoBtn);

    exportBtn.setTooltip ("Export the viewed scene, or every scene, as a .mid file");
    exportBtn.onClick = [this] { showExportMenu(); };
    addAndMakeVisible (exportBtn);
    dragHandle.setTooltip ("Drag onto a DAW track to drop a .mid of the viewed scene");
    dragHandle.makeFile = [this] { return makeDragFile(); };
    addAndMakeVisible (dragHandle);

    for (auto* b : { &recBtn, &metBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    recBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe05050));
    recBtn.setTooltip ("Record arm: notes from MIDI input or the pads are quantized into the grid while the transport runs");
    metBtn.setTooltip ("Metronome click on the audio output");

    fillStepLengthBox (stepLenBox);
    stepLenBox.onChange = [this]
    {
        const auto len = stepLengthFromIndex (stepLenBox.getSelectedId() - 1);
        state.edit ("Step length", 0, [len] (ms::Pattern& p) { p.global.stepLength = len; });
    };
    addAndMakeVisible (stepLenBox);
    styleSection (stepLenLabel, "Step");
    styleSection (stepsLabel, "Steps");
    addAndMakeVisible (stepLenLabel);
    addAndMakeVisible (stepsLabel);

    styleBar (stepsSlider, 1, ms::kMaxSteps, 1);
    stepsSlider.onDragStart = [this] { state.beginGesture(); };
    stepsSlider.onValueChange = [this]
    {
        const int v = (int) stepsSlider.getValue();
        state.edit ("Steps", 7, [v] (ms::Pattern& p)
        {
            for (int t = 0; t < p.scene.numTracks; ++t)
            {
                p.scene.tracks[(size_t) t].stepCount = v;
                p.scene.tracks[(size_t) t].euclid.steps = v;
            }
        });
    };
    addAndMakeVisible (stepsSlider);

    playBtn.setClickingTogglesState (true);
    clockBtn.setClickingTogglesState (true);
    clockBtn.setTooltip ("Run the internal clock while the host transport is stopped (always on in Standalone)");
    addAndMakeVisible (playBtn);
    addAndMakeVisible (clockBtn);

    bpmSlider.setSliderStyle (juce::Slider::LinearBar);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, true, 60, 18);
    bpmSlider.setTextValueSuffix (" bpm");
    addAndMakeVisible (bpmSlider);

    state.addChangeListener (this);
    refresh();
}

void TopBar::paint (juce::Graphics& g)
{
    paintPanel (g, getLocalBounds());
    g.setColour (syncColour);
    g.fillEllipse (syncDot.toFloat().withSizeKeepingCentre (8.0f, 8.0f));
    g.setColour (theme::dim);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (syncText, syncDot.withTrimmedLeft (14).withWidth (46), juce::Justification::centredLeft);
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (6, 5);
    const int gap = 4;
    title.setBounds (r.removeFromLeft (112));
    keyBtn.setBounds (r.removeFromLeft (104)); r.removeFromLeft (gap);
    undoBtn.setBounds (r.removeFromLeft (38)); r.removeFromLeft (2);
    redoBtn.setBounds (r.removeFromLeft (38)); r.removeFromLeft (gap * 2);
    exportBtn.setBounds (r.removeFromLeft (76)); r.removeFromLeft (2);
    dragHandle.setBounds (r.removeFromLeft (38)); r.removeFromLeft (gap * 2);
    stepLenLabel.setBounds (r.removeFromLeft (28));
    stepLenBox.setBounds (r.removeFromLeft (54)); r.removeFromLeft (gap);
    stepsLabel.setBounds (r.removeFromLeft (32));
    stepsSlider.setBounds (r.removeFromLeft (46)); r.removeFromLeft (gap * 2);
    recBtn.setBounds (r.removeFromLeft (32)); r.removeFromLeft (2);
    metBtn.setBounds (r.removeFromLeft (44)); r.removeFromLeft (gap * 2);
    playBtn.setBounds (r.removeFromLeft (40)); r.removeFromLeft (2);
    clockBtn.setBounds (r.removeFromLeft (48)); r.removeFromLeft (2);
    bpmSlider.setBounds (r.removeFromLeft (84)); r.removeFromLeft (gap * 2);
    syncDot = r.removeFromLeft (64);
}

void TopBar::refresh()
{
    const auto& p = state.pattern();
    keyBtn.setButtonText (scaleNameFor (p.global));
    stepLenBox.setSelectedId (stepLengthIndex (p.global.stepLength) + 1, juce::dontSendNotification);
    stepsSlider.setValue (p.scene.tracks[0].stepCount, juce::dontSendNotification);
    undoBtn.setEnabled (state.undo.canUndo());
    redoBtn.setEnabled (state.undo.canRedo());
}

void TopBar::updateTransport()
{
    const auto& t = state.proc.transportInfo;
    juce::String text = "STOP";
    juce::Colour c = theme::dim;
    if (t.playing.load())
    {
        const bool host = t.usingHostClock.load();
        text = host ? "HOST" : "INT";
        c = host ? theme::accent : theme::warn;
    }
    if (text != syncText || c != syncColour)
    {
        syncText = text;
        syncColour = c;
        repaint (syncDot);
    }
}

void TopBar::showKeyMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "No scale (chromatic, unsnapped)");
    menu.addSeparator();
    const auto& g = state.pattern().global;
    for (int root = 0; root < 12; ++root)
    {
        juce::PopupMenu sub;
        int i = 0;
        for (const auto& s : ms::kBuiltInScales)
            sub.addItem (1000 + root * 100 + i++, s.name, true, g.scaleEnabled && g.root == root && g.scaleMask == s.mask);
        menu.addSubMenu (theme::rootName (root), sub);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keyBtn), [this] (int id)
    {
        if (id == 1)
            state.edit ("Scale off", 0, [] (ms::Pattern& p) { p.global.scaleEnabled = false; });
        else if (id >= 1000)
        {
            const int root = (id - 1000) / 100, idx = (id - 1000) % 100;
            const auto mask = ms::kBuiltInScales[idx].mask;
            state.edit ("Scale", 0, [root, mask] (ms::Pattern& p)
            {
                p.global.root = root;
                p.global.scaleMask = mask;
                p.global.scaleEnabled = true;
            });
        }
    });
}

juce::File TopBar::makeDragFile()
{
    const int scene = state.viewScene();
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Melodic Sequencer");
    dir.createDirectory();
    const auto file = dir.getChildFile ("Scene " + juce::String::charToString ((juce::juce_wchar) ('A' + scene)) + ".mid");
    midiexport::writeToFile (midiexport::build (state.project(), state.proc.effectiveGlobal(), scene), file);
    return file;
}

void TopBar::showExportMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Export viewed scene...");
    menu.addItem (2, "Export all scenes (chained)...");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportBtn), [this] (int id)
    {
        if (id == 1 || id == 2) startExport (id == 2);
    });
}

void TopBar::startExport (bool allScenes)
{
    const auto name = allScenes ? juce::String ("All scenes")
                                : "Scene " + juce::String::charToString ((juce::juce_wchar) ('A' + state.viewScene()));
    chooser = std::make_unique<juce::FileChooser> ("Export MIDI",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (name + ".mid"),
                                                   "*.mid");
    const int flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                      | juce::FileBrowserComponent::warnAboutOverwriting;
    chooser->launchAsync (flags, [this, allScenes] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File()) return;
        file = file.withFileExtension ("mid");
        const auto mf = midiexport::build (state.project(), state.proc.effectiveGlobal(), allScenes ? -1 : state.viewScene());
        midiexport::writeToFile (mf, file);
    });
}