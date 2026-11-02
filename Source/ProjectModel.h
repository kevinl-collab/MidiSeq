#pragma once
#include "Engine/Pattern.h"
#include <functional>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>

/** Message-thread owner of the project being edited. Lives in the processor (not the editor) so that
    recording and scene changes keep working, and stay undoable, while the plugin window is closed.
    Every change publishes a fresh snapshot to the audio thread through `publish`. */
class ProjectModel : public juce::ChangeBroadcaster
{
public:
    using PublishFn = std::function<void (const ms::Project&)>;
    explicit ProjectModel (PublishFn publishFn) : publish (std::move (publishFn)) {}

    ms::Project project;          // master copy
    ms::Pattern current;          // working view: project.scenes[viewScene] + project.global
    int viewScene = 0;
    int lastEditKey = 0;
    juce::String presetName { "Default" };
    bool dirty = false;           // edited since the last load / save (shown as "*")
    juce::UndoManager undo { 30000, 100 };

    /** Replace the whole project (load / reset). Clears undo history. */
    void setProject (const ms::Project&, const juce::String& name = "Default");
    void markClean() { dirty = false; sendChangeMessage(); }
    void resetToDemo();   // in place - avoids a half-megabyte temporary on the caller's stack
    /** Fills the project in place via `fill` (so a 0.5 MB Project never lands on a small thread stack). */
    void loadInPlace (const std::function<void (ms::Project&)>& fill, const juce::String& name);

    void beginGesture() { undo.beginNewTransaction(); }

    /** key == 0: own undo step. key != 0: consecutive edits with the same key merge. */
    void edit (const juce::String& name, int coalesceKey, std::function<void (ms::Pattern&)> fn);

    void setViewScene (int scene);

    // scene management
    bool addScene();            // empty scene with the current track layout
    bool duplicateScene();
    void clearScene();          // undoable
    void copyScene();
    bool pasteScene();          // undoable
    bool hasClipboard() const { return hasClip; }

    void applySnapshot (const ms::Pattern&, int sceneIndex);   // used by undo actions

private:
    void syncCurrent();
    void republish();

    PublishFn publish;
    ms::Scene clipboard;
    bool hasClip = false;
};
