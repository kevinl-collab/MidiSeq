#pragma once
#include "../PluginProcessor.h"
#include "Engine/Generator.h"
#include "StepParams.h"
#include <functional>
#include <memory>

/** UI-only state (selection, expanded lanes) plus thin forwarding to the processor's ProjectModel, which owns
    the project, the undo history and the edit/publish machinery. Re-broadcasts model changes to all panels. */
class EditorState : public juce::ChangeBroadcaster, private juce::ChangeListener
{
public:
    explicit EditorState (MelodicSequencerProcessor& p) : proc (p), undo (p.model.undo)
    {
        lane.fill (StepParam::Velocity);
        proc.model.addChangeListener (this);
        selectedTrack = proc.recordTargetTrack.load();
    }
    ~EditorState() override { proc.model.removeChangeListener (this); }

    MelodicSequencerProcessor& proc;
    juce::UndoManager& undo;
    juce::LookAndFeel* lnf = nullptr;   // so pop-ups (call-out boxes) can use the plugin's look

    ms::Pattern& pattern() { return proc.model.current; }      // working view: the viewed scene + globals
    ms::Project& project() { return proc.model.project; }
    int numTracks() { return pattern().scene.numTracks; }
    int viewScene() const { return proc.model.viewScene; }

    int selectedTrack = 0;
    int selectedStep = -1;                                   // -1 = whole track
    std::array<bool, ms::kMaxTracks> expanded {};
    std::array<StepParam, ms::kMaxTracks> lane {};
    std::array<ms::GeneratorSettings, ms::kMaxTracks> genSettings {};   // remembered per track for the Generate panel

    void select (int track, int step)
    {
        track = juce::jlimit (0, juce::jmax (0, numTracks() - 1), track);
        if (track == selectedTrack && step == selectedStep) return;
        selectedTrack = track;
        selectedStep = step;
        proc.recordTargetTrack.store (track);
        sendChangeMessage();
    }

    void beginGesture() { proc.model.beginGesture(); }

    /** key == 0: its own undo step. key != 0: consecutive edits with the same key merge (drags, sweeps). */
    void edit (const juce::String& name, int coalesceKey, std::function<void (ms::Pattern&)> fn)
    {
        proc.model.edit (name, coalesceKey, std::move (fn));
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        selectedTrack = juce::jlimit (0, juce::jmax (0, numTracks() - 1), selectedTrack);
        if (selectedStep >= pattern().scene.tracks[(size_t) selectedTrack].stepCount)
            selectedStep = -1;
        sendChangeMessage();
    }
};
