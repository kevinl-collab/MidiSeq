#pragma once
#include "Types.h"
#include <cmath>

namespace ms
{
/** Fraction of an (even, odd) step pair taken by the first step. 0.5 = straight, 0.667 = triplet swing. */
inline double swingRatio (ShuffleType t, float amountPct)
{
    const double a = std::clamp (double (amountPct), 0.0, 100.0) / 100.0;
    switch (t)
    {
        case ShuffleType::Accurate:     return 0.5 + a * (1.0 / 6.0);                       // linear, up to 66.7%
        case ShuffleType::MPC:          return 0.5 + a * 0.25;                              // 50-75%
        case ShuffleType::EightOhEight: return 0.5 + std::round (a * 6.0) / 6.0 * (1.0 / 6.0); // 7 discrete levels
        case ShuffleType::Humanize:     return 0.5;                                         // random, see below
    }
    return 0.5;
}

/** Delay (in PPQ) applied to step `stepIndex`: odd steps are pushed late, even steps stay on the grid. */
inline double swingOffsetPpq (ShuffleType t, float amountPct, int64_t stepIndex, double stepLenPpq)
{
    if ((stepIndex & 1) == 0) return 0.0;
    return (2.0 * swingRatio (t, amountPct) - 1.0) * stepLenPpq;
}

inline double humanizeTimingOffset (Rng& rng, float amountPct, double stepLenPpq)
{
    return double (rng.next01() * 2.0f - 1.0f) * (double (amountPct) / 100.0) * 0.15 * stepLenPpq;
}

inline float humanizeVelocityScale (Rng& rng, float amountPct)
{
    return 1.0f + (rng.next01() * 2.0f - 1.0f) * (amountPct / 100.0f) * 0.2f;
}
} // namespace ms
