#pragma once
// Scheduler + live scene launching. A scene request is queued and takes effect on the chosen
// boundary (next bar / end of pattern / immediately). If the boundary falls inside a block, the
// block is split at that exact sample: the old scene renders up to it, hanging notes are released
// there, and the new scene renders from it.
#include "Pattern.h"
#include "Scheduler.h"
#include <cmath>

namespace ms
{
/** Length in PPQ of the longest track cycle in a scene (what "end of pattern" means). */
inline double patternLengthPpq (const Scene& scene, const Global& g)
{
    double longest = 0.0;
    for (int t = 0; t < std::clamp (scene.numTracks, 0, kMaxTracks); ++t)
    {
        const auto& tr = scene.tracks[size_t (t)];
        const double len = stepLengthPpq (tr.linkStepLength ? g.stepLength : tr.stepLength);
        longest = std::max (longest, len * double (std::clamp (tr.stepCount, 1, kMaxSteps)));
    }
    return longest > 0.0 ? longest : 4.0;
}

class Sequencer
{
public:
    Sequencer() = default;

    void reset()
    {
        scheduler.reset();
        active = 0;
        queued = -1;
        requested = -1;
    }

    /** Audio thread. Request a scene launch; applied at the next process() call. */
    void requestScene (int scene) { requested = scene; }

    int activeScene() const { return active; }
    int queuedScene() const { return queued; }
    const Scheduler& getScheduler() const { return scheduler; }
    Scheduler& getScheduler() { return scheduler; }

    /** barLenPpq: length of one bar in quarter notes (e.g. 4 for 4/4). */
    void process (const BlockInfo& info, const Project& project, const Global& g, double barLenPpq, EventList& out)
    {
        const int numScenes = std::clamp (project.numScenes, 1, kMaxScenes);
        if (active >= numScenes)
        {
            active = 0;
            scheduler.requestResync();
        }
        if (requested >= 0)
        {
            queued = (requested < numScenes && requested != active) ? requested : -1;
            requested = -1;
        }

        const double pps = info.bpm / 60.0 / std::max (1.0, info.sampleRate);
        int splitAt = 0;   // sample offset where the queued scene takes over (0 = right at block start)

        if (queued >= 0)
        {
            if (! info.playing || g.launchQuantize == LaunchQuantize::Immediate || info.numSamples <= 0 || pps <= 0.0)
                switchNow();
            else
            {
                const double grid = g.launchQuantize == LaunchQuantize::NextBar
                                        ? std::max (0.25, barLenPpq)
                                        : patternLengthPpq (project.scenes[size_t (active)], g);
                const double boundary = std::ceil (info.ppqStart / grid - 1e-9) * grid;
                const double blockEnd = info.ppqStart + double (info.numSamples) * pps;
                if (boundary < blockEnd - 1e-9)
                {
                    splitAt = int (std::clamp<long long> (std::llround ((boundary - info.ppqStart) / pps), 0, info.numSamples - 1));
                    if (splitAt == 0) switchNow();
                }
            }
        }

        if (splitAt > 0)
        {
            BlockInfo first = info;
            first.numSamples = splitAt;
            run (first, project.scenes[size_t (active)], g, 0, out);

            switchNow();
            BlockInfo second = info;
            second.ppqStart = info.ppqStart + double (splitAt) * pps;
            second.numSamples = info.numSamples - splitAt;
            run (second, project.scenes[size_t (active)], g, splitAt, out);
        }
        else
            run (info, project.scenes[size_t (active)], g, 0, out);
    }

private:
    void switchNow()
    {
        if (queued < 0) return;
        active = queued;
        queued = -1;
        scheduler.requestResync();   // releases hanging notes and re-aligns playheads on the next process()
    }

    void run (const BlockInfo& info, const Scene& scene, const Global& g, int shift, EventList& out)
    {
        tmp.clear();
        scheduler.process (info, scene, g, tmp);
        for (int i = 0; i < tmp.size(); ++i)
        {
            NoteEvent e = tmp[i];
            e.sampleOffset += shift;
            out.push (e);
        }
    }

    Scheduler scheduler;
    EventList tmp;
    int active = 0, queued = -1, requested = -1;
};
} // namespace ms
