#pragma once
#include "Types.h"
#include <initializer_list>

namespace ms
{
constexpr uint16_t maskOf (std::initializer_list<int> degrees)
{
    uint16_t m = 0;
    for (int d : degrees) m = uint16_t (m | (1u << d));
    return m;
}

struct ScaleDef { const char* name; uint16_t mask; };

inline constexpr ScaleDef kBuiltInScales[] = {
    { "Major",          maskOf ({ 0, 2, 4, 5, 7, 9, 11 }) },
    { "Minor",          maskOf ({ 0, 2, 3, 5, 7, 8, 10 }) },
    { "Dorian",         maskOf ({ 0, 2, 3, 5, 7, 9, 10 }) },
    { "Phrygian",       maskOf ({ 0, 1, 3, 5, 7, 8, 10 }) },
    { "Lydian",         maskOf ({ 0, 2, 4, 6, 7, 9, 11 }) },
    { "Mixolydian",     maskOf ({ 0, 2, 4, 5, 7, 9, 10 }) },
    { "Locrian",        maskOf ({ 0, 1, 3, 5, 6, 8, 10 }) },
    { "Harmonic Minor", maskOf ({ 0, 2, 3, 5, 7, 8, 11 }) },
    { "Melodic Minor",  maskOf ({ 0, 2, 3, 5, 7, 9, 11 }) },
    { "Major Pent",     maskOf ({ 0, 2, 4, 7, 9 }) },
    { "Minor Pent",     maskOf ({ 0, 3, 5, 7, 10 }) },
    { "Blues",          maskOf ({ 0, 3, 5, 6, 7, 10 }) },
    { "Whole Tone",     maskOf ({ 0, 2, 4, 6, 8, 10 }) },
    { "Chromatic",      0xFFF },
};

inline bool isInScale (int note, int root, uint16_t mask)
{
    return (mask >> posMod (note - root, 12)) & 1u;
}

/** Nearest in-scale note; ties resolve downwards. An empty mask leaves the note unchanged. */
inline int quantizeToScale (int note, int root, uint16_t mask)
{
    if ((mask & 0xFFF) == 0 || isInScale (note, root, mask)) return note;
    for (int d = 1; d <= 6; ++d)
    {
        if (isInScale (note - d, root, mask)) return note - d;
        if (isInScale (note + d, root, mask)) return note + d;
    }
    return note;
}

/** Folds a note by octaves into [low, high] (or clamps if the range is under an octave). */
inline int fitToRange (int note, int low, int high)
{
    const int lo = std::clamp (low, 0, 127);
    const int hi = std::clamp (high, lo, 127);
    if (hi - lo >= 11)
    {
        while (note > hi) note -= 12;
        while (note < lo) note += 12;
    }
    return std::clamp (note, lo, hi);
}
} // namespace ms
