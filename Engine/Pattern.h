#pragma once
#include "Euclid.h"
#include "Scale.h"
#include "Types.h"

namespace ms
{
/** Working view used by the UI: one scene plus the project-wide globals. Edits are made on a Pattern. */
struct Pattern
{
    Scene scene;
    Global global;
};

/** Everything the audio thread needs: all scenes + globals. Published as one snapshot. */
struct Project
{
    std::array<Scene, kMaxScenes> scenes;
    int numScenes = 4;
    Global global;
};

/** A new empty scene with the same track layout (notes, channels, lengths) as `src`, all steps cleared. */
inline Scene clearedCopy (const Scene& src)
{
    Scene s = src;
    for (auto& t : s.tracks)
        for (auto& st : t.steps)
            st = Step {};
    return s;
}

/** Small demo groove (C minor pentatonic) so the plugin makes music before the user has programmed anything. */
inline Pattern makeDemoPattern()
{
    Pattern p;
    p.scene.numTracks = 4;
    p.global.scaleEnabled = true;
    p.global.root = 0;
    p.global.scaleMask = maskOf ({ 0, 3, 5, 7, 10 });

    auto& kick = p.scene.tracks[0];
    kick.outNote = 36; kick.inNote = 36; kick.euclid = { 5, 16, 0, false };
    applyEuclid (kick);

    auto& pulse = p.scene.tracks[1];
    pulse.outNote = 48; pulse.inNote = 48; pulse.euclid = { 3, 8, 0, false };
    applyEuclid (pulse);

    auto& lead = p.scene.tracks[2];
    lead.outNote = 60; lead.inNote = 60; lead.euclid = { 7, 16, 2, false };
    applyEuclid (lead);
    const int pitches[] = { 0, 3, 7, 10, 7, 5, 3, 0 };
    int k = 0;
    for (auto& s : lead.steps)
        if (s.active) s.pitch = pitches[k++ % 8];

    auto& hat = p.scene.tracks[3];
    hat.outNote = 42; hat.inNote = 42; hat.followTranspose = false;
    for (int i = 0; i < 16; i += 4) hat.steps[size_t (i)].active = true;
    hat.steps[12].reps = 2; hat.steps[12].interval = 0.0625f; hat.steps[12].shape = RepeatShape::RampUp;
    return p;
}

/** Demo project: scene A is the groove above; B-D are variations so scene launching is audible. */
/** Fills `pr` in place (a Project is ~0.5 MB - keep it off small thread stacks). */
inline void fillDemoProject (Project& pr)
{
    const Pattern a = makeDemoPattern();
    pr.global = a.global;
    pr.numScenes = 4;
    for (auto& s : pr.scenes) s = a.scene;

    applyEuclidTo (pr.scenes[1].tracks[0], 7, 16, 0);   // B: busier kick, sparser lead
    applyEuclidTo (pr.scenes[1].tracks[2], 4, 16, 1);
    applyEuclidTo (pr.scenes[2].tracks[1], 5, 8, 0);    // C: busy pulse, half-time kick
    applyEuclidTo (pr.scenes[2].tracks[0], 3, 16, 0);
    pr.scenes[3] = clearedCopy (a.scene);               // D: empty - record into it
}

inline Project makeDemoProject()
{
    Project pr;
    fillDemoProject (pr);
    return pr;
}
} // namespace ms
