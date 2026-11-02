#include "ProjectSerializer.h"

namespace
{
using juce::Identifier;
using juce::ValueTree;

template <class T>
void put (ValueTree& t, const char* name, T value, T def)
{
    if (! (value == def)) t.setProperty (Identifier (name), value, nullptr);
}
void putEnum (ValueTree& t, const char* name, int value, int def)
{
    if (value != def) t.setProperty (Identifier (name), value, nullptr);
}

int getInt (const ValueTree& t, const char* name, int def, int lo, int hi)
{
    return juce::jlimit (lo, hi, (int) t.getProperty (Identifier (name), def));
}
float getFloat (const ValueTree& t, const char* name, float def, float lo, float hi)
{
    return juce::jlimit (lo, hi, (float) (double) t.getProperty (Identifier (name), (double) def));
}
bool getBool (const ValueTree& t, const char* name, bool def)
{
    return (bool) t.getProperty (Identifier (name), def);
}

ValueTree stepToTree (int index, const ms::Step& s)
{
    const ms::Step d {};
    ValueTree t ("STEP");
    t.setProperty ("idx", index, nullptr);
    put (t, "on", s.active, d.active);
    put (t, "vel", s.velocity, d.velocity);
    put (t, "len", (double) s.length, (double) d.length);
    put (t, "shift", (double) s.shift, (double) d.shift);
    put (t, "prob", (double) s.probability, (double) d.probability);
    put (t, "reps", s.reps, d.reps);
    put (t, "int", (double) s.interval, (double) d.interval);
    putEnum (t, "shape", (int) s.shape, (int) d.shape);
    put (t, "curve", (double) s.curve, (double) d.curve);
    put (t, "gain", (double) s.gain, (double) d.gain);
    putEnum (t, "cond", (int) s.condition, (int) d.condition);
    put (t, "condN", s.conditionN, d.conditionN);
    put (t, "pitch", s.pitch, d.pitch);
    return t;
}

ms::Step treeToStep (const ValueTree& t)
{
    const ms::Step d {};
    ms::Step s;
    s.active = getBool (t, "on", d.active);
    s.velocity = getInt (t, "vel", d.velocity, 1, 127);
    s.length = getFloat (t, "len", d.length, 0.01f, 8.0f);
    s.shift = getFloat (t, "shift", d.shift, -0.5f, 0.5f);
    s.probability = getFloat (t, "prob", d.probability, 0.0f, 1.0f);
    s.reps = getInt (t, "reps", d.reps, 0, ms::kMaxRepeats);
    s.interval = getFloat (t, "int", d.interval, 0.001f, 4.0f);
    s.shape = (ms::RepeatShape) getInt (t, "shape", (int) d.shape, 0, 4);
    s.curve = getFloat (t, "curve", d.curve, -100.0f, 100.0f);
    s.gain = getFloat (t, "gain", d.gain, 0.0f, 4.0f);
    s.condition = (ms::Condition) getInt (t, "cond", (int) d.condition, 0, 4);
    s.conditionN = getInt (t, "condN", d.conditionN, 2, 16);
    s.pitch = getInt (t, "pitch", d.pitch, -48, 48);
    return s;
}

ValueTree trackToTree (const ms::Track& tr)
{
    const ms::Track d {};
    ValueTree t ("TRACK");
    put (t, "out", tr.outNote, d.outNote);
    put (t, "in", tr.inNote, d.inNote);
    put (t, "ch", tr.outChannel, d.outChannel);
    put (t, "tr", tr.transposeOffset, d.transposeOffset);
    put (t, "vscale", (double) tr.velScale, (double) d.velScale);
    put (t, "gain", (double) tr.gain, (double) d.gain);
    put (t, "mute", tr.mute, d.mute);
    put (t, "solo", tr.solo, d.solo);
    put (t, "follow", tr.followTranspose, d.followTranspose);
    putEnum (t, "dir", (int) tr.direction, (int) d.direction);
    put (t, "count", tr.stepCount, d.stepCount);
    putEnum (t, "div", (int) tr.stepLength.division, (int) d.stepLength.division);
    putEnum (t, "mod", (int) tr.stepLength.modifier, (int) d.stepLength.modifier);
    put (t, "linkLen", tr.linkStepLength, d.linkStepLength);
    putEnum (t, "shufType", (int) tr.shuffleType, (int) d.shuffleType);
    put (t, "shufAmt", (double) tr.shuffleAmount, (double) d.shuffleAmount);
    put (t, "linkShuf", tr.linkShuffle, d.linkShuffle);
    put (t, "choke", tr.chokeMode, d.chokeMode);
    put (t, "ePulses", tr.euclid.pulses, d.euclid.pulses);
    put (t, "eSteps", tr.euclid.steps, d.euclid.steps);
    put (t, "eRot", tr.euclid.rotation, d.euclid.rotation);
    put (t, "eLive", tr.euclid.live, d.euclid.live);

    const ms::Step ds {};
    for (int i = 0; i < ms::kMaxSteps; ++i)
    {
        const auto& s = tr.steps[(size_t) i];
        const bool isDefault = s.active == ds.active && s.velocity == ds.velocity && s.length == ds.length
                               && s.shift == ds.shift && s.probability == ds.probability && s.reps == ds.reps
                               && s.interval == ds.interval && s.shape == ds.shape && s.curve == ds.curve
                               && s.gain == ds.gain && s.condition == ds.condition && s.conditionN == ds.conditionN
                               && s.pitch == ds.pitch;
        if (! isDefault) t.appendChild (stepToTree (i, s), nullptr);
    }
    return t;
}

void treeToTrack (const ValueTree& t, ms::Track& tr)
{
    const ms::Track d {};
    tr = ms::Track {};
    tr.outNote = getInt (t, "out", d.outNote, 0, 127);
    tr.inNote = getInt (t, "in", d.inNote, 0, 127);
    tr.outChannel = getInt (t, "ch", d.outChannel, 1, 16);
    tr.transposeOffset = getInt (t, "tr", d.transposeOffset, -48, 48);
    tr.velScale = getFloat (t, "vscale", d.velScale, 0.0f, 4.0f);
    tr.gain = getFloat (t, "gain", d.gain, 0.0f, 4.0f);
    tr.mute = getBool (t, "mute", d.mute);
    tr.solo = getBool (t, "solo", d.solo);
    tr.followTranspose = getBool (t, "follow", d.followTranspose);
    tr.direction = (ms::Direction) getInt (t, "dir", (int) d.direction, 0, 5);
    tr.stepCount = getInt (t, "count", d.stepCount, 1, ms::kMaxSteps);
    tr.stepLength.division = (ms::StepDivision) getInt (t, "div", (int) d.stepLength.division, 0, 3);
    tr.stepLength.modifier = (ms::StepModifier) getInt (t, "mod", (int) d.stepLength.modifier, 0, 2);
    tr.linkStepLength = getBool (t, "linkLen", d.linkStepLength);
    tr.shuffleType = (ms::ShuffleType) getInt (t, "shufType", (int) d.shuffleType, 0, 3);
    tr.shuffleAmount = getFloat (t, "shufAmt", d.shuffleAmount, 0.0f, 100.0f);
    tr.linkShuffle = getBool (t, "linkShuf", d.linkShuffle);
    tr.chokeMode = getInt (t, "choke", d.chokeMode, 0, 5);
    tr.euclid.pulses = getInt (t, "ePulses", d.euclid.pulses, 0, ms::kMaxSteps);
    tr.euclid.steps = getInt (t, "eSteps", d.euclid.steps, 1, ms::kMaxSteps);
    tr.euclid.rotation = getInt (t, "eRot", d.euclid.rotation, -ms::kMaxSteps, ms::kMaxSteps);
    tr.euclid.live = getBool (t, "eLive", d.euclid.live);

    for (const auto& c : t)
        if (c.hasType ("STEP"))
        {
            const int idx = (int) c.getProperty ("idx", -1);
            if (idx >= 0 && idx < ms::kMaxSteps) tr.steps[(size_t) idx] = treeToStep (c);
        }
}

ValueTree globalToTree (const ms::Global& g)
{
    const ms::Global d {};
    ValueTree t ("GLOBAL");
    putEnum (t, "div", (int) g.stepLength.division, (int) d.stepLength.division);
    putEnum (t, "mod", (int) g.stepLength.modifier, (int) d.stepLength.modifier);
    putEnum (t, "shufType", (int) g.shuffleType, (int) d.shuffleType);
    put (t, "shufAmt", (double) g.shuffleAmount, (double) d.shuffleAmount);
    put (t, "transpose", g.transpose, d.transpose);
    put (t, "offset", g.offset, d.offset);
    put (t, "scaleOn", g.scaleEnabled, d.scaleEnabled);
    put (t, "root", g.root, d.root);
    put (t, "mask", (int) g.scaleMask, (int) d.scaleMask);
    put (t, "lo", g.rangeLow, d.rangeLow);
    put (t, "hi", g.rangeHigh, d.rangeHigh);
    put (t, "midiTr", g.midiTransposeOn, d.midiTransposeOn);
    put (t, "inCh", g.inChannel, d.inChannel);
    putEnum (t, "launchQ", (int) g.launchQuantize, (int) d.launchQuantize);
    return t;
}

void treeToGlobal (const ValueTree& t, ms::Global& g)
{
    const ms::Global d {};
    g = ms::Global {};
    g.stepLength.division = (ms::StepDivision) getInt (t, "div", (int) d.stepLength.division, 0, 3);
    g.stepLength.modifier = (ms::StepModifier) getInt (t, "mod", (int) d.stepLength.modifier, 0, 2);
    g.shuffleType = (ms::ShuffleType) getInt (t, "shufType", (int) d.shuffleType, 0, 3);
    g.shuffleAmount = getFloat (t, "shufAmt", d.shuffleAmount, 0.0f, 100.0f);
    g.transpose = getInt (t, "transpose", d.transpose, -24, 24);
    g.offset = getInt (t, "offset", d.offset, -16, 16);
    g.scaleEnabled = getBool (t, "scaleOn", d.scaleEnabled);
    g.root = getInt (t, "root", d.root, 0, 11);
    g.scaleMask = (uint16_t) getInt (t, "mask", (int) d.scaleMask, 0, 0xFFF);
    g.rangeLow = getInt (t, "lo", d.rangeLow, 0, 127);
    g.rangeHigh = getInt (t, "hi", d.rangeHigh, 0, 127);
    g.midiTransposeOn = getBool (t, "midiTr", d.midiTransposeOn);
    g.inChannel = getInt (t, "inCh", d.inChannel, 1, 16);
    g.launchQuantize = (ms::LaunchQuantize) getInt (t, "launchQ", (int) d.launchQuantize, 0, 2);
}
} // namespace

namespace ser
{
ValueTree projectToTree (const ms::Project& p, const juce::String& presetName)
{
    ValueTree t ("PROJECT");
    t.setProperty ("numScenes", p.numScenes, nullptr);
    t.setProperty ("presetName", presetName, nullptr);
    t.appendChild (globalToTree (p.global), nullptr);

    for (int s = 0; s < juce::jlimit (1, ms::kMaxScenes, p.numScenes); ++s)
    {
        const auto& scene = p.scenes[(size_t) s];
        ValueTree st ("SCENE");
        st.setProperty ("numTracks", scene.numTracks, nullptr);
        for (int i = 0; i < juce::jlimit (0, ms::kMaxTracks, scene.numTracks); ++i)
            st.appendChild (trackToTree (scene.tracks[(size_t) i]), nullptr);
        t.appendChild (st, nullptr);
    }
    return t;
}

bool treeToProject (const ValueTree& t, ms::Project& out, juce::String* presetName)
{
    if (! t.hasType ("PROJECT")) return false;

    if (presetName != nullptr) *presetName = t.getProperty ("presetName", "Default").toString();
    out.numScenes = getInt (t, "numScenes", 4, 1, ms::kMaxScenes);
    treeToGlobal (t.getChildWithName ("GLOBAL"), out.global);

    int s = 0;
    for (const auto& st : t)
    {
        if (! st.hasType ("SCENE")) continue;
        if (s >= ms::kMaxScenes) break;
        auto& scene = out.scenes[(size_t) s];
        scene = ms::Scene {};
        scene.numTracks = getInt (st, "numTracks", 0, 0, ms::kMaxTracks);
        int i = 0;
        for (const auto& tt : st)
            if (tt.hasType ("TRACK") && i < ms::kMaxTracks)
                treeToTrack (tt, scene.tracks[(size_t) i++]);
        scene.numTracks = juce::jmin (scene.numTracks, i);
        ++s;
    }
    if (s == 0) return false;
    out.numScenes = juce::jmin (out.numScenes, s);
    return true;
}

ValueTree scalesToTree (const std::vector<NamedScale>& scales)
{
    ValueTree t ("USERSCALES");
    for (const auto& sc : scales)
    {
        ValueTree n ("SCALE");
        n.setProperty ("name", sc.name, nullptr);
        n.setProperty ("mask", (int) sc.mask, nullptr);
        t.appendChild (n, nullptr);
    }
    return t;
}

std::vector<NamedScale> treeToScales (const ValueTree& t)
{
    std::vector<NamedScale> out;
    for (const auto& n : t)
        if (n.hasType ("SCALE"))
            out.push_back ({ n.getProperty ("name").toString(), (uint16_t) juce::jlimit (1, 0xFFF, (int) n.getProperty ("mask", 0xFFF)) });
    return out;
}

void migrate (ValueTree& root)
{
    int version = (int) root.getProperty ("stateVersion", 1);
    // v1 -> v2 only added children (PROJECT, USERSCALES); nothing to rewrite. Future format changes go here,
    // one `if (version < N) { ...; version = N; }` block per step so any old session upgrades in order.
    if (version < 2) version = 2;
    root.setProperty ("stateVersion", version, nullptr);
}
} // namespace ser
