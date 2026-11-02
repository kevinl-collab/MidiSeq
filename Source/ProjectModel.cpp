#include "ProjectModel.h"

namespace
{
class SnapshotAction : public juce::UndoableAction
{
public:
    SnapshotAction (ProjectModel& m, std::shared_ptr<const ms::Pattern> b, std::shared_ptr<const ms::Pattern> a,
                    int sceneIdx, int k)
        : model (m), before (std::move (b)), after (std::move (a)), scene (sceneIdx), key (k) {}

    bool perform() override { model.applySnapshot (*after, scene); return true; }
    bool undo() override    { model.applySnapshot (*before, scene); return true; }
    int getSizeInUnits() override { return 50; }

    juce::UndoableAction* createCoalescedAction (juce::UndoableAction* next) override
    {
        if (key != 0)
            if (auto* n = dynamic_cast<SnapshotAction*> (next))
                if (n->key == key && n->scene == scene)
                    return new SnapshotAction (model, before, n->after, scene, key);
        return nullptr;
    }

private:
    ProjectModel& model;
    std::shared_ptr<const ms::Pattern> before, after;
    int scene, key;
};
} // namespace

void ProjectModel::syncCurrent()
{
    viewScene = juce::jlimit (0, project.numScenes - 1, viewScene);
    current.scene = project.scenes[(size_t) viewScene];
    current.global = project.global;
}

void ProjectModel::republish()
{
    dirty = true;
    if (publish) publish (project);
    sendChangeMessage();
}

void ProjectModel::setProject (const ms::Project& p, const juce::String& name)
{
    loadInPlace ([&p] (ms::Project& dst) { if (&dst != &p) dst = p; }, name);
}

void ProjectModel::resetToDemo()
{
    loadInPlace ([] (ms::Project& dst) { ms::fillDemoProject (dst); }, "Default");
}

void ProjectModel::loadInPlace (const std::function<void (ms::Project&)>& fill, const juce::String& name)
{
    fill (project);
    presetName = name;
    undo.clearUndoHistory();
    lastEditKey = 0;
    syncCurrent();
    republish();
    dirty = false;
}

void ProjectModel::setViewScene (int scene)
{
    scene = juce::jlimit (0, project.numScenes - 1, scene);
    if (scene == viewScene) return;
    viewScene = scene;
    syncCurrent();
    sendChangeMessage();
}

void ProjectModel::edit (const juce::String& name, int coalesceKey, std::function<void (ms::Pattern&)> fn)
{
    auto before = std::make_shared<const ms::Pattern> (current);
    auto after = std::make_shared<ms::Pattern> (current);
    fn (*after);
    if (coalesceKey == 0) undo.beginNewTransaction (name);
    lastEditKey = coalesceKey;
    undo.perform (new SnapshotAction (*this, std::move (before), std::move (after), viewScene, coalesceKey), name);
}

void ProjectModel::applySnapshot (const ms::Pattern& snap, int sceneIndex)
{
    sceneIndex = juce::jlimit (0, ms::kMaxScenes - 1, sceneIndex);
    project.scenes[(size_t) sceneIndex] = snap.scene;
    project.global = snap.global;
    if (sceneIndex == viewScene) current.scene = snap.scene;
    current.global = snap.global;
    republish();
}

bool ProjectModel::addScene()
{
    if (project.numScenes >= ms::kMaxScenes) return false;
    project.scenes[(size_t) project.numScenes] = ms::clearedCopy (project.scenes[(size_t) viewScene]);
    ++project.numScenes;
    republish();
    return true;
}

bool ProjectModel::duplicateScene()
{
    if (project.numScenes >= ms::kMaxScenes) return false;
    project.scenes[(size_t) project.numScenes] = project.scenes[(size_t) viewScene];
    ++project.numScenes;
    republish();
    return true;
}

void ProjectModel::clearScene()
{
    edit ("Clear scene", 0, [] (ms::Pattern& p) { p.scene = ms::clearedCopy (p.scene); });
}

void ProjectModel::copyScene()
{
    clipboard = current.scene;
    hasClip = true;
    sendChangeMessage();
}

bool ProjectModel::pasteScene()
{
    if (! hasClip) return false;
    const auto src = clipboard;
    edit ("Paste scene", 0, [src] (ms::Pattern& p) { p.scene = src; });
    return true;
}
