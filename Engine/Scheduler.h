#pragma once
// Sample-accurate step scheduler. Real-time safe: fixed-size pools, no allocation, no locks.
//
// Per block it works in PPQ: every track resolves "its next step" (pattern index via the
// direction walker, plus swing/shift/humanize) to a PPQ time. Steps whose time falls inside
// the block are expanded into note-on/off pairs (repeats included) that go into a scheduled-event
// pool; the pool is then drained in time order for everything that falls inside the block. Because
// events live in the pool until their PPQ time, repeats and gates may cross block boundaries and
// survive tempo changes. Transport jumps / loop wraps / stop flush all sounding notes.
#include "Directions.h"
#include "Repeats.h"
#include "Scale.h"
#include "Swing.h"
#include "Types.h"
#include <cmath>

namespace ms
{
struct NoteEvent
{
    int sampleOffset = 0;
    int channel = 1;      // 1..16
    int note = 60;
    int velocity = 0;
    bool isOn = false;
};

class EventList
{
public:
    static constexpr int kCapacity = 2048;
    void clear() { count = 0; }
    bool push (const NoteEvent& e)
    {
        if (count >= kCapacity) return false;
        events[size_t (count++)] = e;
        return true;
    }
    int size() const { return count; }
    const NoteEvent& operator[] (int i) const { return events[size_t (i)]; }

private:
    std::array<NoteEvent, kCapacity> events {};
    int count = 0;
};

struct BlockInfo
{
    double ppqStart = 0.0;   // PPQ position of the first sample of the block
    double bpm = 120.0;
    double sampleRate = 44100.0;
    int numSamples = 0;
    bool playing = false;
};

class Scheduler
{
public:
    Scheduler() { reset(); }

    void reset (uint32_t seed = 1u)
    {
        rng.seed (seed);
        for (auto& t : trackState) t = TrackState {};
        for (auto& p : pool) p.used = false;
        activeCount = 0;
        wasPlaying = false;
        forceResync = false;
        expectedPpq = 0.0;
    }

    /** Call (audio thread) after a pattern/scene change: flushes notes and re-aligns all playheads. */
    void requestResync() { forceResync = true; }

    /** Increments whenever playback (re)starts or jumps - lets recording tell one pass over the loop from the next. */
    uint32_t resyncCount() const { return resyncs; }

    /** Pattern index of the step this track most recently triggered (-1 if none) - for the UI playhead. */
    int lastStepIndex (int track) const { return trackState[size_t (std::clamp (track, 0, kMaxTracks - 1))].lastFired; }

    /** Emits note-offs for everything sounding and drops all scheduled events. */
    void allNotesOff (EventList& out, int sampleOffset = 0)
    {
        for (int i = 0; i < activeCount; ++i)
            out.push ({ sampleOffset, active[size_t (i)].channel, active[size_t (i)].note, 0, false });
        activeCount = 0;
        for (auto& p : pool) p.used = false;
    }

    void process (const BlockInfo& info, const Scene& scene, const Global& g, EventList& out)
    {
        if (! info.playing || info.numSamples <= 0 || info.bpm <= 0.0 || info.sampleRate <= 0.0)
        {
            if (wasPlaying) allNotesOff (out, 0);
            wasPlaying = false;
            return;
        }

        const double pps = info.bpm / 60.0 / info.sampleRate;   // PPQ per sample
        const double blockStart = info.ppqStart;
        const double blockEnd = blockStart + double (info.numSamples) * pps;

        const bool contiguous = wasPlaying && ! forceResync
                                && std::abs (blockStart - expectedPpq) <= 2.0 * pps + 1e-6;
        if (! contiguous)
        {
            ++resyncs;
            allNotesOff (out, 0);
            for (auto& t : trackState) t.primed = false;
        }
        forceResync = false;
        wasPlaying = true;
        expectedPpq = blockEnd;

        const int numTracks = std::clamp (scene.numTracks, 0, kMaxTracks);
        bool anySolo = false;
        for (int t = 0; t < numTracks; ++t) anySolo = anySolo || scene.tracks[size_t (t)].solo;

        for (int t = 0; t < numTracks; ++t)
            scheduleTrack (t, scene.tracks[size_t (t)], g, blockStart, blockEnd, pps, anySolo);

        drain (blockStart, blockEnd, pps, info.numSamples, out);
    }

private:
    static constexpr double kEps = 1e-9;
    static constexpr int kPoolSize = 1024;
    static constexpr int kMaxActive = 256;

    struct TrackState
    {
        bool primed = false, resolved = false;
        int64_t nextStep = 0;
        double lenPpq = 0.0, time = 0.0;
        int patIdx = 0, lastFired = -1;
        DirectionWalker walker;
    };

    struct Scheduled
    {
        bool used = false, on = false;
        double ppq = 0.0;
        int channel = 1, note = 60, velocity = 0, track = 0, chokeMode = 0;
    };

    struct ActiveNote { int channel, note, track, chokeMode; };

    void scheduleTrack (int t, const Track& track, const Global& g,
                        double blockStart, double blockEnd, double pps, bool anySolo)
    {
        TrackState& ts = trackState[size_t (t)];
        const double len = stepLengthPpq (track.linkStepLength ? g.stepLength : track.stepLength);
        const int count = std::clamp (track.stepCount, 1, kMaxSteps);
        const bool audible = ! track.mute && (! anySolo || track.solo);
        const ShuffleType sType = track.linkShuffle ? g.shuffleType : track.shuffleType;
        const float sAmt = track.linkShuffle ? g.shuffleAmount : track.shuffleAmount;

        if (! ts.primed || ts.lenPpq != len)
        {
            ts.nextStep = int64_t (std::ceil (blockStart / len - kEps));
            ts.resolved = false;
            ts.primed = true;
            ts.lenPpq = len;
            ts.walker.reset();
        }

        for (int guard = 0; guard < 4096; ++guard)
        {
            if (! ts.resolved)
            {
                const int64_t n = ts.nextStep;
                ts.patIdx = ts.walker.index (track.direction, n + g.offset, count, rng);
                const Step& st = track.steps[size_t (ts.patIdx)];
                double time = double (n) * len + swingOffsetPpq (sType, sAmt, n, len) + double (st.shift) * len;
                if (sType == ShuffleType::Humanize) time += humanizeTimingOffset (rng, sAmt, len);
                ts.time = time;
                ts.resolved = true;
            }

            if (ts.time >= blockEnd - kEps) break;

            fireStep (t, track, g, ts, len, pps, audible, sType, sAmt);
            ts.lastFired = ts.patIdx;
            ++ts.nextStep;
            ts.resolved = false;
        }
    }

    void fireStep (int t, const Track& track, const Global& g, const TrackState& ts, double len, double pps,
                   bool audible, ShuffleType sType, float sAmt)
    {
        const Step& st = track.steps[size_t (ts.patIdx)];
        if (! st.active || ! audible) return;

        const int count = std::clamp (track.stepCount, 1, kMaxSteps);
        if (! conditionPasses (st, ts.nextStep / count, g.fill)) return;
        if (st.probability < 1.0f && rng.next01() >= st.probability) return;

        RepeatHit hits[kMaxRepeats + 1];
        const int nh = computeRepeats (st, rng, hits);

        int freeSlots = 0;
        for (const auto& p : pool) freeSlots += p.used ? 0 : 1;
        if (freeSlots < 2 * nh) return;   // pool exhausted: drop the whole step rather than a lone note-on

        int note = track.outNote + track.transposeOffset + st.pitch;
        if (track.followTranspose) note += g.transpose;
        if (g.scaleEnabled) note = quantizeToScale (note, g.root, g.scaleMask);
        note = fitToRange (note, g.rangeLow, g.rangeHigh);

        float vel = float (st.velocity) * track.velScale * track.gain * st.gain;
        if (sType == ShuffleType::Humanize) vel *= humanizeVelocityScale (rng, sAmt);

        double gate = double (st.length) * len;
        if (gate <= 0.0) gate = len * 0.01;

        for (int k = 0; k < nh; ++k)
        {
            double hitGate = gate;
            if (k < nh - 1)
                hitGate = std::min (gate, (hits[k + 1].offsetPpq - hits[k].offsetPpq) * 0.95);
            hitGate = std::max (hitGate, 2.0 * pps);

            const int v = std::clamp (int (std::lround (vel * hits[k].velScale)), 1, 127);
            const double onPpq = ts.time + hits[k].offsetPpq;
            schedule (onPpq, true, track.outChannel, note, v, t, track.chokeMode);
            schedule (onPpq + hitGate, false, track.outChannel, note, 0, t, track.chokeMode);
        }
    }

    void schedule (double ppq, bool on, int ch, int note, int vel, int track, int choke)
    {
        for (auto& p : pool)
            if (! p.used)
            {
                p = { true, on, ppq, std::clamp (ch, 1, 16), std::clamp (note, 0, 127), vel, track, choke };
                return;
            }
    }

    void drain (double blockStart, double blockEnd, double pps, int numSamples, EventList& out)
    {
        for (;;)
        {
            int best = -1;
            for (int i = 0; i < kPoolSize; ++i)
            {
                const Scheduled& e = pool[size_t (i)];
                if (! e.used || e.ppq >= blockEnd - kEps) continue;
                if (best < 0 || e.ppq < pool[size_t (best)].ppq
                    || (e.ppq == pool[size_t (best)].ppq && ! e.on && pool[size_t (best)].on))
                    best = i;
            }
            if (best < 0) break;

            const Scheduled e = pool[size_t (best)];
            pool[size_t (best)].used = false;
            const int offset = int (std::clamp<long long> (std::llround ((e.ppq - blockStart) / pps), 0, numSamples - 1));
            if (e.on) emitOn (e, offset, out); else emitOff (e, offset, out);
        }
    }

    void emitOn (const Scheduled& e, int offset, EventList& out)
    {
        // Retrigger of the same note, "cut itself", and choke groups all cut what is sounding first.
        for (int i = 0; i < activeCount;)
        {
            const ActiveNote a = active[size_t (i)];
            const bool cut = (a.channel == e.channel && a.note == e.note)
                          || (e.chokeMode == 1 && a.track == e.track)
                          || (e.chokeMode >= 2 && a.chokeMode == e.chokeMode);
            if (cut)
            {
                out.push ({ offset, a.channel, a.note, 0, false });
                cancelPendingOff (a);
                active[size_t (i)] = active[size_t (--activeCount)];
            }
            else
                ++i;
        }

        if (activeCount >= kMaxActive) return;
        out.push ({ offset, e.channel, e.note, e.velocity, true });
        active[size_t (activeCount++)] = { e.channel, e.note, e.track, e.chokeMode };
    }

    void emitOff (const Scheduled& e, int offset, EventList& out)
    {
        for (int i = 0; i < activeCount; ++i)
            if (active[size_t (i)].channel == e.channel && active[size_t (i)].note == e.note)
            {
                out.push ({ offset, e.channel, e.note, 0, false });
                active[size_t (i)] = active[size_t (--activeCount)];
                return;
            }
        // not active: its note was already cut (choke/retrigger) - nothing to do
    }

    void cancelPendingOff (const ActiveNote& a)
    {
        int best = -1;
        for (int i = 0; i < kPoolSize; ++i)
        {
            const Scheduled& p = pool[size_t (i)];
            if (p.used && ! p.on && p.channel == a.channel && p.note == a.note && p.track == a.track
                && (best < 0 || p.ppq < pool[size_t (best)].ppq))
                best = i;
        }
        if (best >= 0) pool[size_t (best)].used = false;
    }

    std::array<TrackState, kMaxTracks> trackState {};
    std::array<Scheduled, kPoolSize> pool {};
    std::array<ActiveNote, kMaxActive> active {};
    int activeCount = 0;
    uint32_t resyncs = 0;
    bool wasPlaying = false, forceResync = false;
    double expectedPpq = 0.0;
    Rng rng;
};
} // namespace ms
