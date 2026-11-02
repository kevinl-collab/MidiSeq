#pragma once
#include "Types.h"
#include <cmath>

namespace ms
{
struct RepeatHit
{
    double offsetPpq = 0.0;   // from the start of the step
    float velScale = 1.0f;
};

/** Expands a step into 1 + reps hits. `out` must hold kMaxRepeats + 1 entries. Returns the hit count. */
inline int computeRepeats (const Step& s, Rng& rng, RepeatHit* out)
{
    const int r = std::clamp (s.reps, 0, kMaxRepeats);
    if (r == 0)
    {
        out[0] = { 0.0, 1.0f };
        return 1;
    }

    const double span = double (r) * double (s.interval);
    // curve > 0 -> gaps shrink (bouncing ball); curve < 0 -> gaps grow. The overall span is unchanged.
    const double p = std::pow (2.0, -double (std::clamp (s.curve, -100.0f, 100.0f)) / 50.0);

    for (int k = 0; k <= r; ++k)
    {
        const double x = double (k) / double (r);
        float v = 1.0f;
        switch (s.shape)
        {
            case RepeatShape::Flat:     v = 1.0f; break;
            case RepeatShape::RampUp:   v = 0.4f + 0.6f * float (x); break;
            case RepeatShape::RampDown: v = 1.0f - 0.6f * float (x); break;
            case RepeatShape::UpDown:   v = 0.4f + 0.6f * float (1.0 - std::abs (2.0 * x - 1.0)); break;
            case RepeatShape::Random:   v = 0.4f + 0.6f * rng.next01(); break;
        }
        out[k] = { span * std::pow (x, p), v };
    }
    return r + 1;
}
} // namespace ms
