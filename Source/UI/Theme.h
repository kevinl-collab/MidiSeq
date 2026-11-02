#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
inline const juce::Colour bg       { 0xff161618 };
inline const juce::Colour panel    { 0xff222225 };
inline const juce::Colour panelHi  { 0xff2f2f34 };
inline const juce::Colour line     { 0xff3b3b42 };
inline const juce::Colour text     { 0xffd9d9dc };
inline const juce::Colour dim      { 0xff85858c };
inline const juce::Colour accent   { 0xffa6e22e };
inline const juce::Colour accentDim{ 0xff5d7f1d };
inline const juce::Colour warn     { 0xffe2a62e };

constexpr int designWidth = 960, designHeight = 540;

/** MIDI note name with C-2 = 0 (so 60 = C3 and the range ends at G8 = 127). */
inline juce::String noteName (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    n = juce::jlimit (0, 127, n);
    return juce::String (names[n % 12]) + juce::String (n / 12 - 2);
}

/** Parses "C#3", "g-1", "60". Returns -1 if it can't. */
inline int parseNoteName (juce::String s)
{
    s = s.trim();
    if (s.isEmpty()) return -1;
    if (s.containsOnly ("0123456789")) return juce::jlimit (0, 127, s.getIntValue());

    static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
    const auto c = juce::CharacterFunctions::toUpperCase (s[0]);
    if (c < 'A' || c > 'G') return -1;
    int pc = base[c - 'A'];
    int i = 1;
    if (s[i] == '#') { ++pc; ++i; }
    else if (s[i] == 'b') { --pc; ++i; }
    const auto octave = s.substring (i);
    if (octave.isEmpty() || ! octave.containsOnly ("-0123456789")) return -1;
    return juce::jlimit (0, 127, (octave.getIntValue() + 2) * 12 + pc);
}

inline const char* rootName (int root)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((root % 12) + 12) % 12];
}
} // namespace theme
