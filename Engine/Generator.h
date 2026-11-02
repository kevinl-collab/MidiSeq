#pragma once
// Scale-aware pattern generation for melodic tracks. Deterministic for a given seed.
#include "Scale.h"
#include "Types.h"
#include <cstdlib>
#include <vector>

namespace ms
{
enum class GenPreset : uint8_t { Custom, Bassline, Arpeggio, Rolling };
enum class ArpPattern : uint8_t { Up, Down, UpDown, Random };

struct GeneratorSettings
{
    GenPreset preset = GenPreset::Custom;
    int lowSemis = -12, highSemis = 12;     // note range, in semitones around the track's output note
    float density = 0.6f;                   // chance a step is active
    float leap = 0.3f;                      // 0 = step-wise movement, 1 = jumps anywhere in range
    float rootEmphasis = 0.3f;              // chance a note is forced to the root
    float gate = 0.5f;                      // note length as a fraction of a step
    uint32_t seed = 1;
    ArpPattern arp = ArpPattern::Up;        // Arpeggio preset
    uint16_t chordDegrees = 0x15;           // Arpeggio: scale degrees (bit 0 = root, 2 = third, 4 = fifth, 6 = seventh)
};

inline GeneratorSettings presetSettings (GenPreset p, uint32_t seed = 1)
{
    GeneratorSettings s;
    s.preset = p;
    s.seed = seed;
    switch (p)
    {
        case GenPreset::Custom: break;
        case GenPreset::Bassline: s.lowSemis = -12; s.highSemis = 7;  s.density = 0.55f; s.leap = 0.2f;  s.rootEmphasis = 0.6f;  s.gate = 0.9f; break;
        case GenPreset::Arpeggio: s.lowSemis = 0;   s.highSemis = 24; s.density = 1.0f;  s.leap = 0.0f;  s.rootEmphasis = 0.0f;  s.gate = 0.7f; s.arp = ArpPattern::UpDown; s.chordDegrees = 0x55; break;
        case GenPreset::Rolling:  s.lowSemis = -12; s.highSemis = 12; s.density = 0.9f;  s.leap = 0.15f; s.rootEmphasis = 0.45f; s.gate = 0.4f; break;
    }
    return s;
}

namespace detail
{
/** In-scale offsets (semitones from the track's note) within the settings' range, ascending. */
inline std::vector<int> candidateOffsets (const Track& tr, const Global& g, const GeneratorSettings& s)
{
    const int base = tr.outNote + tr.transposeOffset;
    std::vector<int> out;
    for (int o = std::min (s.lowSemis, s.highSemis); o <= std::max (s.lowSemis, s.highSemis); ++o)
    {
        const int note = base + o;
        if (note < 0 || note > 127) continue;
        if (! g.scaleEnabled || isInScale (note, g.root, g.scaleMask)) out.push_back (o);
    }
    if (out.empty()) out.push_back (0);
    return out;
}
} // namespace detail

/** Fills the track's steps (up to its step count) with generated notes. Overwrites active, pitch, velocity, length. */
inline void generateTrack (Track& tr, const Global& g, const GeneratorSettings& s)
{
    Rng rng;
    rng.seed (s.seed * 2654435761u + 12345u);

    const auto cand = detail::candidateOffsets (tr, g, s);
    const int n = int (cand.size());
    const int base = tr.outNote + tr.transposeOffset;
    const int count = std::clamp (tr.stepCount, 1, kMaxSteps);

    std::vector<int> rootIdx;
    for (int i = 0; i < n; ++i)
        if (posMod (base + cand[size_t (i)] - g.root, 12) == 0) rootIdx.push_back (i);

    // start from the candidate closest to the track's own note
    int idx = 0;
    for (int i = 0; i < n; ++i)
        if (std::abs (cand[size_t (i)]) < std::abs (cand[size_t (idx)])) idx = i;

    auto accent = [] (int i) { return i % 4 == 0 ? 12 : 0; };

    if (s.preset == GenPreset::Arpeggio)
    {
        // chord = scale steps above the first root at/above the track note
        int start = 0;
        for (int i = 0; i < n; ++i)
            if (cand[size_t (i)] >= 0 && (rootIdx.empty() || std::find (rootIdx.begin(), rootIdx.end(), i) != rootIdx.end())) { start = i; break; }

        std::vector<int> chord;
        for (int d = 0; d < 7; ++d)
            if ((s.chordDegrees >> d) & 1u)
                for (int oct = 0; oct < 4; ++oct)   // degrees repeat in higher octaves while the range allows
                {
                    const int ci = start + d + oct * 7;
                    if (ci < n) chord.push_back (cand[size_t (ci)]);
                }
        std::sort (chord.begin(), chord.end());
        chord.erase (std::unique (chord.begin(), chord.end()), chord.end());
        if (chord.empty()) chord.push_back (0);

        std::vector<int> order;
        const int m = int (chord.size());
        switch (s.arp)
        {
            case ArpPattern::Up:     for (int i = 0; i < m; ++i) order.push_back (i); break;
            case ArpPattern::Down:   for (int i = m - 1; i >= 0; --i) order.push_back (i); break;
            case ArpPattern::UpDown: for (int i = 0; i < m; ++i) order.push_back (i);
                                     for (int i = m - 2; i >= 1; --i) order.push_back (i); break;
            case ArpPattern::Random: for (int i = 0; i < count; ++i) order.push_back (rng.nextInt (m)); break;
        }
        for (int i = 0; i < count; ++i)
        {
            auto& st = tr.steps[size_t (i)];
            st = Step {};
            st.active = true;
            st.pitch = chord[size_t (order[size_t (i) % order.size()])];
            st.velocity = 80 + rng.nextInt (20) + accent (i);
            st.length = s.gate;
        }
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        auto& st = tr.steps[size_t (i)];
        st = Step {};
        const float d = s.density <= 0.0f ? 0.0f : std::clamp (s.density + (i % 4 == 0 ? 0.25f : 0.0f), 0.0f, 1.0f);
        if (rng.next01() >= d) continue;

        if (rng.next01() < s.rootEmphasis && ! rootIdx.empty())
            idx = rootIdx[size_t (rng.nextInt (int (rootIdx.size())))];
        else if (rng.next01() < s.leap)
            idx = rng.nextInt (n);
        else
            idx = std::clamp (idx + (rng.nextInt (2) == 0 ? -1 : 1), 0, n - 1);

        st.active = true;
        st.pitch = cand[size_t (idx)];
        st.velocity = std::min (127, 78 + rng.nextInt (30) + accent (i));
        st.length = s.gate;
    }
}
} // namespace ms
