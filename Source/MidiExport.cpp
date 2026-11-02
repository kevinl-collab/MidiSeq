#include "MidiExport.h"
#include "Engine/Scheduler.h"
#include <numeric>

namespace midiexport
{
int sceneLengthTicks (const ms::Scene& scene, const ms::Global& g)
{
    constexpr int64_t bar = 4 * kTicksPerQuarter;
    constexpr int64_t cap = 16 * bar;

    int64_t cycle = 1;
    for (int t = 0; t < juce::jlimit (0, ms::kMaxTracks, scene.numTracks); ++t)
    {
        const auto& tr = scene.tracks[(size_t) t];
        const double len = ms::stepLengthPpq (tr.linkStepLength ? g.stepLength : tr.stepLength);
        const int64_t ticks = std::max<int64_t> (1, std::llround (len * kTicksPerQuarter)) * juce::jlimit (1, ms::kMaxSteps, tr.stepCount);
        cycle = std::lcm (cycle, ticks);
        if (cycle > cap) { cycle = cap; break; }
    }
    const int64_t bars = std::clamp<int64_t> ((cycle + bar - 1) / bar, 1, 16);
    return (int) (bars * bar);
}

namespace
{
/** Appends one scene's notes at `baseTick`; returns the scene length in ticks. */
int renderScene (const ms::Scene& scene, const ms::Global& g, double bpm, int baseTick, juce::MidiMessageSequence& out)
{
    const int lengthTicks = sceneLengthTicks (scene, g);
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    const double pps = bpm / 60.0 / sr;                           // PPQ per sample
    const double totalPpq = (double) lengthTicks / kTicksPerQuarter;
    const auto totalSamples = (int64_t) std::ceil (totalPpq / pps);

    auto sched = std::make_unique<ms::Scheduler>();
    sched->reset (1);
    ms::EventList events;

    auto emit = [&] (int64_t samplePos, const ms::NoteEvent& e)
    {
        const double ppq = (double) samplePos * pps;
        const int tick = juce::jlimit (0, lengthTicks, (int) std::llround (ppq * kTicksPerQuarter));
        out.addEvent (e.isOn ? juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity)
                             : juce::MidiMessage::noteOff (e.channel, e.note),
                      (double) (baseTick + tick));
    };

    for (int64_t pos = 0; pos < totalSamples; pos += block)
    {
        const int n = (int) std::min<int64_t> (block, totalSamples - pos);
        ms::BlockInfo info;
        info.ppqStart = (double) pos * pps;
        info.bpm = bpm;
        info.sampleRate = sr;
        info.numSamples = n;
        info.playing = true;
        events.clear();
        sched->process (info, scene, g, events);
        for (int i = 0; i < events.size(); ++i)
            emit (pos + events[i].sampleOffset, events[i]);
    }

    events.clear();                       // end of the scene: release anything still ringing
    sched->allNotesOff (events, 0);
    for (int i = 0; i < events.size(); ++i)
        emit (totalSamples, events[i]);

    return lengthTicks;
}
} // namespace

juce::MidiFile build (const ms::Project& project, const ms::Global& g, int sceneIndex, double bpm)
{
    juce::MidiMessageSequence conductor, notes;

    conductor.addEvent (juce::MidiMessage::textMetaEvent (3, "Melodic Sequencer"), 0.0);
    conductor.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::llround (60000000.0 / bpm)), 0.0);
    conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

    const int numScenes = juce::jlimit (1, ms::kMaxScenes, project.numScenes);
    const int first = sceneIndex < 0 ? 0 : juce::jlimit (0, numScenes - 1, sceneIndex);
    const int last = sceneIndex < 0 ? numScenes - 1 : first;

    int tick = 0;
    for (int s = first; s <= last; ++s)
    {
        notes.addEvent (juce::MidiMessage::textMetaEvent (6, "Scene " + juce::String::charToString ((juce::juce_wchar) ('A' + s))), (double) tick);
        tick += renderScene (project.scenes[(size_t) s], g, bpm, tick, notes);
    }

    notes.sort();
    notes.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (kTicksPerQuarter);
    file.addTrack (conductor);
    file.addTrack (notes);
    return file;
}

bool writeToFile (const juce::MidiFile& file, const juce::File& dest)
{
    dest.deleteFile();
    juce::FileOutputStream out (dest);
    return out.openedOk() && file.writeTo (out, 1);
}
} // namespace midiexport
