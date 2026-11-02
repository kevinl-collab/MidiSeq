#pragma once
#include "Types.h"
#include <vector>

namespace ms
{
/** Bjorklund's algorithm. Result has `steps` valid entries; rotation shifts the pattern right. */
inline std::array<bool, kMaxSteps> euclidPattern (int pulses, int steps, int rotation)
{
    std::array<bool, kMaxSteps> out {};
    steps = std::clamp (steps, 0, kMaxSteps);
    if (steps == 0) return out;
    pulses = std::clamp (pulses, 0, steps);

    std::vector<bool> seq;
    if (pulses == 0)           seq.assign (size_t (steps), false);
    else if (pulses == steps)  seq.assign (size_t (steps), true);
    else
    {
        using Group = std::vector<bool>;
        std::vector<Group> a (size_t (pulses), Group { true });
        std::vector<Group> b (size_t (steps - pulses), Group { false });

        while (b.size() > 1)
        {
            const size_t n = std::min (a.size(), b.size());
            for (size_t i = 0; i < n; ++i)
                a[i].insert (a[i].end(), b[i].begin(), b[i].end());

            if (a.size() > b.size())
            {
                std::vector<Group> rest (a.begin() + long (n), a.end());
                a.resize (n);
                b = std::move (rest);
            }
            else
                b.erase (b.begin(), b.begin() + long (n));
        }
        for (auto& g : a) seq.insert (seq.end(), g.begin(), g.end());
        for (auto& g : b) seq.insert (seq.end(), g.begin(), g.end());
    }

    const auto r = int (posMod (rotation, steps));
    for (int i = 0; i < steps; ++i)
        out[size_t ((i + r) % steps)] = seq[size_t (i)];
    return out;
}

/** "Apply" action: writes the Euclidean rhythm into the track's active flags and step count. */
inline void applyEuclid (Track& t)
{
    const auto pat = euclidPattern (t.euclid.pulses, t.euclid.steps, t.euclid.rotation);
    const int n = std::clamp (t.euclid.steps, 1, kMaxSteps);
    t.stepCount = n;
    for (int i = 0; i < kMaxSteps; ++i)
        t.steps[size_t (i)].active = i < n && pat[size_t (i)];
}

inline void applyEuclidTo (Track& t, int pulses, int steps, int rotation)
{
    t.euclid.pulses = pulses;
    t.euclid.steps = steps;
    t.euclid.rotation = rotation;
    applyEuclid (t);
}
} // namespace ms
