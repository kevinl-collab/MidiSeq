#pragma once
// Pure helpers for step recording and MIDI-transpose input. The audio thread timestamps incoming notes
// in PPQ; the message thread turns them into step edits with these functions.
#include "Scale.h"
#include "Types.h"
#include <cmath>

namespace ms
{
struct QuantizedHit
{
    int64_t step = 0;   // absolute step counter (0 = transport start)
    float shift = 0.0f; // micro-timing left over after quantizing, as a fraction of a step (-0.5..0.5)
};

/** strength01 = 1: snap fully to the grid. 0: nearest step, but keep the played timing as step shift. */
inline QuantizedHit quantizeToStep (double ppq, double stepLenPpq, float strength01)
{
    const double pos = ppq / stepLenPpq;
    const auto n = int64_t (std::floor (pos + 0.5));
    const double err = pos - double (n);   // in steps, within [-0.5, 0.5]
    const float s = std::clamp (strength01, 0.0f, 1.0f);
    return { n, float ((1.0 - double (s)) * err) };
}

/** Pattern slot an absolute step lands on (Forward direction; other directions are approximated). */
inline int patternIndexForStep (int64_t step, int offset, int stepCount)
{
    return int (posMod (step + offset, std::clamp (stepCount, 1, kMaxSteps)));
}

/** Writes a recorded note into a step. For melodic input pass the played note; drum input passes -1. */
inline void recordHit (Track& tr, int idx, int velocity, float shift, int playedNote = -1)
{
    auto& s = tr.steps[size_t (std::clamp (idx, 0, kMaxSteps - 1))];
    s.active = true;
    s.velocity = std::clamp (velocity, 1, 127);
    s.shift = std::clamp (shift, -0.5f, 0.5f);
    if (playedNote >= 0)
        s.pitch = std::clamp (playedNote - tr.outNote - tr.transposeOffset, -48, 48);
}

inline void clearTrackSteps (Track& tr)
{
    for (auto& s : tr.steps) s.active = false;
}

// ---------------------------------------------------------------------------------------------
enum class TransposeMode { Latch, Hold };

/** Incoming notes set a transpose offset relative to a reference root (root pitch class in octave C3), snapped to the scale. */
class MidiTranspose
{
public:
    void reset() { held.fill (false); last = -1; offset = 0; }
    int semitones() const { return offset; }

    void noteOn (int note, int root, bool scaleEnabled, uint16_t mask)
    {
        if (note < 0 || note > 127) return;
        held[size_t (note)] = true;
        last = note;
        offset = compute (note, root, scaleEnabled, mask);
        lastRoot = root; lastEnabled = scaleEnabled; lastMask = mask;
    }

    void noteOff (int note, TransposeMode mode)
    {
        if (note < 0 || note > 127) return;
        held[size_t (note)] = false;
        if (mode == TransposeMode::Latch || last != note) return;

        last = -1;   // Hold: fall back to another held key (highest), else back to zero
        for (int n = 127; n >= 0; --n)
            if (held[size_t (n)]) { last = n; break; }
        offset = last >= 0 ? compute (last, lastRoot, lastEnabled, lastMask) : 0;
    }

    static int compute (int note, int root, bool scaleEnabled, uint16_t mask)
    {
        const int snapped = scaleEnabled ? quantizeToScale (note, root, mask) : note;
        return std::clamp (snapped - (60 + root), -24, 24);
    }

private:
    std::array<bool, 128> held {};
    int last = -1, offset = 0, lastRoot = 0;
    bool lastEnabled = false;
    uint16_t lastMask = 0xFFF;
};
} // namespace ms
