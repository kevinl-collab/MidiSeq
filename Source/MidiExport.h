#pragma once
#include "Engine/Pattern.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace midiexport
{
constexpr int kTicksPerQuarter = 960;

/** Renders scene(s) with the real scheduler (so shuffle, repeats, conditions, directions and polymetric lengths
    come out exactly as they play) and returns a type-1 MIDI file: track 0 = tempo/meter, track 1 = notes.
    sceneIndex < 0 exports every scene, chained back to back with a marker at each scene start.
    Each scene is rendered for its full polymetric cycle, rounded up to whole bars (at most 16). */
juce::MidiFile build (const ms::Project& project, const ms::Global& effectiveGlobal, int sceneIndex, double bpm = 120.0);

bool writeToFile (const juce::MidiFile&, const juce::File&);

/** Length in ticks of one scene's export (whole bars, 4/4). */
int sceneLengthTicks (const ms::Scene&, const ms::Global&);
} // namespace midiexport
