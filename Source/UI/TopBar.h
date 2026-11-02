#pragma once
#include "EditorState.h"
#include "Widgets.h"

/** Drag this onto a DAW track (or desktop) to drop a .mid of the viewed scene. */
class DragHandle : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<juce::File()> makeFile;

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::panelHi);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f);
        g.setColour (isEnabled() ? theme::text : theme::dim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (".mid", getLocalBounds(), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override { started = false; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() < 6 || ! makeFile) return;
        started = true;
        const auto file = makeFile();
        if (! file.existsAsFile()) return;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            c->performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this, nullptr);
    }
    juce::MouseCursor getMouseCursor() override { return juce::MouseCursor::DraggingHandCursor; }

private:
    bool started = false;
};

class TopBar : public juce::Component, private juce::ChangeListener
{
public:
    explicit TopBar (EditorState&);
    ~TopBar() override { state.removeChangeListener (this); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void updateTransport();   // called from the editor's timer

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void showKeyMenu();
    void showExportMenu();
    void startExport (bool allScenes);
    juce::File makeDragFile();

    EditorState& state;
    juce::Label title;
    juce::TextButton keyBtn, undoBtn { "Undo" }, redoBtn { "Redo" }, exportBtn { "Export MIDI" },
                     recBtn { "Rec" }, metBtn { "Metro" }, playBtn { "Play" }, clockBtn { "Int Clk" };
    DragHandle dragHandle;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ComboBox stepLenBox;
    juce::Label stepLenLabel, stepsLabel;
    juce::Slider stepsSlider, bpmSlider;
    juce::AudioProcessorValueTreeState::ButtonAttachment playAtt, clockAtt, recAtt, metAtt;
    juce::AudioProcessorValueTreeState::SliderAttachment bpmAtt;
    juce::Rectangle<int> syncDot;
    juce::String syncText { "STOP" };
    juce::Colour syncColour { theme::dim };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
};
