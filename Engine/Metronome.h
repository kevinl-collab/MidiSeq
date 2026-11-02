#pragma once
// Click generator: a short decaying sine on every quarter note (higher on the downbeat).
// Adds into the supplied buffers; no allocation.
#include "Scheduler.h"
#include <cmath>

namespace ms
{
class Metronome
{
public:
    void reset() { remaining = 0; }

    void process (const BlockInfo& info, int beatsPerBar, float* left, float* right, int numSamples)
    {
        if (! info.playing || info.bpm <= 0.0 || info.sampleRate <= 0.0)
        {
            remaining = 0;
            return;
        }

        const double pps = info.bpm / 60.0 / info.sampleRate;
        const double end = info.ppqStart + double (numSamples) * pps;
        const int bpb = std::max (1, beatsPerBar);
        const int clickLen = std::max (1, int (info.sampleRate * 0.025));

        int64_t beat = int64_t (std::ceil (info.ppqStart - 1e-9));
        auto nextOffset = [&] () -> int
        {
            if (double (beat) >= end - 1e-9) return numSamples + 1;
            return int (std::clamp<long long> (std::llround ((double (beat) - info.ppqStart) / pps), 0, numSamples - 1));
        };
        int next = nextOffset();

        for (int i = 0; i < numSamples; ++i)
        {
            while (i == next)
            {
                remaining = clickLen;
                total = clickLen;
                phase = 0.0;
                phaseInc = 2.0 * 3.14159265358979323846 * (posMod (beat, bpb) == 0 ? 1500.0 : 1000.0) / info.sampleRate;
                ++beat;
                next = nextOffset();
            }
            if (remaining > 0)
            {
                const float env = float (remaining) / float (total);
                const float s = 0.3f * env * env * float (std::sin (phase));
                phase += phaseInc;
                --remaining;
                left[i] += s;
                if (right != nullptr) right[i] += s;
            }
        }
    }

private:
    int remaining = 0, total = 1;
    double phase = 0.0, phaseInc = 0.0;
};
} // namespace ms
