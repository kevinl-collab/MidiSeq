#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/Factory.h"

MelodicSequencerProcessor::MelodicSequencerProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      model ([this] (const ms::Project& p)
             {
                 projectExchange.writeBuffer() = p;
                 projectExchange.publish();
             }),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pTranspose      = apvts.getRawParameterValue ("transpose");
    pOffset         = apvts.getRawParameterValue ("offset");
    pShuffleAmount  = apvts.getRawParameterValue ("shuffleAmount");
    pShuffleType    = apvts.getRawParameterValue ("shuffleType");
    pScene          = apvts.getRawParameterValue ("scene");
    pInternalClock  = apvts.getRawParameterValue ("internalClock");
    pInternalRun    = apvts.getRawParameterValue ("internalRun");
    pInternalBpm    = apvts.getRawParameterValue ("internalBpm");
    pRecordArm      = apvts.getRawParameterValue ("recordArm");
    pRecordMode     = apvts.getRawParameterValue ("recordMode");
    pQuantStrength  = apvts.getRawParameterValue ("quantStrength");
    pMetronome      = apvts.getRawParameterValue ("metronome");
    pTransposeMode  = apvts.getRawParameterValue ("transposeMode");

    model.resetToDemo();
    userScales = PresetManager::loadUserScales();

    // The recording / scene-follow timer must run on the message thread even if a host builds us elsewhere.
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        juce::MessageManager::callAsync ([this, flag = alive]
        {
            if (flag->load()) startTimerHz (30);
        });
}

MelodicSequencerProcessor::~MelodicSequencerProcessor()
{
    alive->store (false);
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout MelodicSequencerProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "transpose", 1 }, "Transpose", -24, 24, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "offset", 1 }, "Offset", -16, 16, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "shuffleAmount", 1 }, "Shuffle Amount",
                                                       NormalisableRange<float> (0.f, 100.f), 0.f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "shuffleType", 1 }, "Shuffle Type",
                                                        StringArray { "Accurate", "MPC", "808", "Humanize" }, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "scene", 1 }, "Scene", 0, ms::kMaxScenes - 1, 0));

    // Internal clock: always used by the Standalone build; in a plugin host it only runs while
    // the host transport is stopped and "Internal Clock" is on.
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "internalClock", 1 }, "Internal Clock", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "internalRun", 1 }, "Internal Play", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "internalBpm", 1 }, "Internal BPM",
                                                       NormalisableRange<float> (20.f, 300.f, 0.1f), 120.f));

    // Recording, metronome, MIDI transpose
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "recordArm", 1 }, "Record Arm", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "recordMode", 1 }, "Record Mode",
                                                        StringArray { "Overdub", "Replace" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "quantStrength", 1 }, "Quantize Strength",
                                                       NormalisableRange<float> (0.f, 100.f), 100.f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "metronome", 1 }, "Metronome", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "transposeMode", 1 }, "Transpose Mode",
                                                        StringArray { "Latch", "Hold" }, 0));
    return layout;
}

void MelodicSequencerProcessor::prepareToPlay (double, int)
{
    sequencer.reset();
    metronome.reset();
    midiTranspose.reset();
    internalPpq = 0.0;
    lastSceneParam = -1;
}

bool MelodicSequencerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MelodicSequencerProcessor::emitEvents (juce::MidiBuffer& midi)
{
    for (int i = 0; i < events.size(); ++i)
    {
        const auto& e = events[i];
        midi.addEvent (e.isOn ? juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity)
                              : juce::MidiMessage::noteOff (e.channel, e.note),
                       e.sampleOffset);
    }
}

void MelodicSequencerProcessor::pushRecord (const RecordEvent& e)
{
    int s1, n1, s2, n2;
    recordFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0)      recordBuf[(size_t) s1] = e;
    else if (n2 > 0) recordBuf[(size_t) s2] = e;
    recordFifo.finishedWrite (n1 + n2);   // full FIFO: the note is dropped, never blocks
}

void MelodicSequencerProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();                // silent stereo bus (plus the metronome click, below)
    midi.ensureSize (4096);       // grows at most once; keeps addEvent from allocating later

    projectExchange.acquire();
    const ms::Project& project = projectExchange.read();

    const int numSamples = audio.getNumSamples();
    const double sr = getSampleRate();
    const bool standalone = wrapperType == wrapperType_Standalone;

    // --- globals: automatable parameters override the snapshot's values
    ms::Global g = project.global;
    g.offset = (int) pOffset->load();
    g.shuffleAmount = pShuffleAmount->load();
    g.shuffleType = (ms::ShuffleType) juce::jlimit (0, 3, (int) pShuffleType->load());

    // --- timing source: host transport if it is rolling, otherwise the internal clock
    ms::BlockInfo info;
    info.sampleRate = sr;
    info.numSamples = numSamples;
    int tsNum = 4, tsDen = 4;

    bool hostRolling = false;
    if (! standalone)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                const double bpm = pos->getBpm().orFallback (120.0);
                transportInfo.bpm.store (bpm, std::memory_order_relaxed);

                if (auto ts = pos->getTimeSignature())
                {
                    tsNum = juce::jmax (1, ts->numerator);
                    tsDen = juce::jmax (1, ts->denominator);
                    transportInfo.timeSigNum.store (tsNum, std::memory_order_relaxed);
                    transportInfo.timeSigDen.store (tsDen, std::memory_order_relaxed);
                }
                transportInfo.looping.store (pos->getIsLooping(), std::memory_order_relaxed);

                if (pos->getIsPlaying())
                {
                    double ppq = 0.0;
                    if (auto p = pos->getPpqPosition())
                        ppq = *p;
                    else if (auto t = pos->getTimeInSamples())
                        ppq = double (*t) / sr * bpm / 60.0;   // host gave no PPQ: derive it from samples

                    info.playing = true;
                    info.bpm = bpm;
                    info.ppqStart = ppq;
                    hostRolling = true;
                }
            }

    const bool usingInternal = ! hostRolling && (standalone || pInternalClock->load() > 0.5f)
                               && pInternalRun->load() > 0.5f;
    if (usingInternal)
    {
        info.playing = true;
        info.bpm = (double) pInternalBpm->load();
        info.ppqStart = internalPpq;
        internalPpq += double (numSamples) * info.bpm / 60.0 / sr;
        transportInfo.bpm.store (info.bpm, std::memory_order_relaxed);
    }
    else
        internalPpq = 0.0;   // internal transport restarts from the top

    const double pps = info.bpm / 60.0 / sr;
    const double barLenPpq = double (tsNum) * 4.0 / double (tsDen);

    // --- scene launch requests: UI, automation of the "Scene" parameter
    if (const int r = pendingScene.exchange (-1); r >= 0)
        sequencer.requestScene (r);
    if (const int sp = (int) pScene->load(); sp != lastSceneParam)
    {
        lastSceneParam = sp;
        sequencer.requestScene (sp);
    }

    // --- incoming MIDI: scene triggers, transpose, recording. Nothing is passed through.
    const bool armed = pRecordArm->load() > 0.5f && info.playing;
    const auto transposeMode = pTransposeMode->load() > 0.5f ? ms::TransposeMode::Hold : ms::TransposeMode::Latch;
    if (! g.midiTransposeOn) midiTranspose.reset();
    const auto& activeScene = project.scenes[(size_t) juce::jlimit (0, ms::kMaxScenes - 1, sequencer.activeScene())];

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! (m.isNoteOn() || m.isNoteOff())) continue;

        const int note = m.getNoteNumber();
        const bool onChannel = m.getChannel() == g.inChannel;

        if (onChannel && note >= ms::kSceneTriggerBase && note < ms::kSceneTriggerBase + ms::kMaxScenes)
        {
            if (m.isNoteOn()) sequencer.requestScene (note - ms::kSceneTriggerBase);
            continue;   // trigger notes are consumed
        }
        if (onChannel && g.midiTransposeOn)
        {
            if (m.isNoteOn()) midiTranspose.noteOn (note, g.root, g.scaleEnabled, g.scaleMask);
            else              midiTranspose.noteOff (note, transposeMode);
            continue;
        }
        if (armed && m.isNoteOn())
        {
            RecordEvent ev;
            ev.ppq = info.ppqStart + double (meta.samplePosition) * pps;
            ev.note = note;
            ev.velocity = m.getVelocity();
            ev.pass = sequencer.getScheduler().resyncCount();
            for (int t = 0; t < juce::jlimit (0, ms::kMaxTracks, activeScene.numTracks); ++t)
                if (activeScene.tracks[(size_t) t].inNote == note) { ev.track = t; break; }
            pushRecord (ev);
        }
    }
    midi.clear();

    g.transpose = (int) pTranspose->load() + (g.midiTransposeOn ? midiTranspose.semitones() : 0);

    // --- run the engine, translate to MIDI at exact sample offsets
    events.clear();
    sequencer.process (info, project, g, barLenPpq, events);
    emitEvents (midi);

    // --- pad audition from the UI (plays even when the transport is stopped; records when armed)
    for (int n = auditionFifo.getNumReady(); n > 0; --n)
    {
        int s1, n1, s2, n2;
        auditionFifo.prepareToRead (1, s1, n1, s2, n2);
        const AuditionEvent a = auditionBuf[(size_t) (n1 > 0 ? s1 : s2)];
        auditionFifo.finishedRead (n1 + n2);

        midi.addEvent (a.on ? juce::MidiMessage::noteOn (a.channel, a.note, (juce::uint8) a.velocity)
                            : juce::MidiMessage::noteOff (a.channel, a.note), 0);
        if (armed && a.on && a.track >= 0)
        {
            RecordEvent ev;
            ev.ppq = info.ppqStart;
            ev.track = a.track;
            ev.note = a.note;
            ev.velocity = a.velocity;
            ev.pass = sequencer.getScheduler().resyncCount();
            pushRecord (ev);
        }
    }

    // --- metronome click into the (otherwise silent) output
    if (pMetronome->load() > 0.5f && audio.getNumChannels() > 0)
        metronome.process (info, tsNum, audio.getWritePointer (0),
                           audio.getNumChannels() > 1 ? audio.getWritePointer (1) : nullptr, numSamples);
    else
        metronome.reset();

    // --- publish state for the UI
    transportInfo.playing.store (info.playing, std::memory_order_relaxed);
    transportInfo.usingHostClock.store (hostRolling, std::memory_order_relaxed);
    transportInfo.recording.store (armed, std::memory_order_relaxed);
    transportInfo.ppq.store (info.ppqStart, std::memory_order_relaxed);
    transportInfo.activeScene.store (sequencer.activeScene(), std::memory_order_relaxed);
    transportInfo.queuedScene.store (sequencer.queuedScene(), std::memory_order_relaxed);
    transportInfo.midiTranspose.store (midiTranspose.semitones(), std::memory_order_relaxed);
    for (int t = 0; t < ms::kMaxTracks; ++t)
        transportInfo.trackStep[(size_t) t].store (sequencer.getScheduler().lastStepIndex (t), std::memory_order_relaxed);
}

void MelodicSequencerProcessor::processBlockBypassed (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    audio.clear();
    midi.ensureSize (4096);
    midi.clear();
    events.clear();
    sequencer.getScheduler().allNotesOff (events, 0);   // release anything still sounding
    sequencer.getScheduler().requestResync();           // re-align when bypass is lifted
    metronome.reset();
    emitEvents (midi);
    transportInfo.playing.store (false, std::memory_order_relaxed);
    transportInfo.recording.store (false, std::memory_order_relaxed);
}

// ---------------------------------------------------------------- message thread
void MelodicSequencerProcessor::timerCallback()
{
    // follow the scene the audio thread is actually playing (launches are quantized)
    const int active = transportInfo.activeScene.load (std::memory_order_relaxed);
    if (active != model.viewScene && active < model.project.numScenes)
        model.setViewScene (active);

    if (pRecordArm->load() < 0.5f) lastPassForTrack.fill (0);

    for (;;)
    {
        int s1, n1, s2, n2;
        recordFifo.prepareToRead (1, s1, n1, s2, n2);
        if (n1 + n2 == 0) break;
        const RecordEvent ev = recordBuf[(size_t) (n1 > 0 ? s1 : s2)];
        recordFifo.finishedRead (n1 + n2);
        handleRecordEvent (ev);
    }
}

void MelodicSequencerProcessor::handleRecordEvent (const RecordEvent& ev)
{
    // one undo step per take: start a new transaction unless the previous edit was also a recorded note
    if (model.lastEditKey != kRecordKey)
        model.undo.beginNewTransaction ("Record");

    int track = ev.track;
    const bool melodic = track < 0;   // unmapped note -> selected track, stored as a pitch offset
    if (melodic) track = recordTargetTrack.load();
    if (track < 0 || track >= model.current.scene.numTracks) return;

    const float strength = pQuantStrength->load() / 100.0f;
    const bool replace = pRecordMode->load() > 0.5f;

    model.edit ("Record", kRecordKey, [&] (ms::Pattern& p)
    {
        auto& tr = p.scene.tracks[(size_t) track];
        const double len = ms::stepLengthPpq (tr.linkStepLength ? p.global.stepLength : tr.stepLength);
        const auto q = ms::quantizeToStep (ev.ppq, len, strength);
        const int idx = ms::patternIndexForStep (q.step, p.global.offset, tr.stepCount);

        if (replace && lastPassForTrack[(size_t) track] != ev.pass)   // first note of this pass: wipe the track
        {
            ms::clearTrackSteps (tr);
            lastPassForTrack[(size_t) track] = ev.pass;
        }
        ms::recordHit (tr, idx, ev.velocity, q.shift, melodic ? ev.note : -1);
    });
}

juce::AudioProcessorEditor* MelodicSequencerProcessor::createEditor()
{
    return new MelodicSequencerEditor (*this);
}

// ---------------------------------------------------------------- state, presets
namespace
{
// transport-style parameters are session-local; presets and "load preset" must not flip them
bool isRuntimeParam (const juce::String& id)
{
    return id == "internalRun" || id == "internalClock" || id == "recordArm" || id == "metronome";
}
}

void MelodicSequencerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("MelodicSequencer");
    root.setProperty ("stateVersion", ser::kStateVersion, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (ser::projectToTree (model.project, model.presetName), nullptr);
    root.appendChild (ser::scalesToTree (userScales), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void MelodicSequencerProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);

    if (tree.hasType (apvts.state.getType()))   // version 1 session: parameters only
    {
        apvts.replaceState (tree);
        return;
    }
    if (! tree.hasType ("MelodicSequencer")) return;
    ser::migrate (tree);

    // The project is shared with the UI and undo history, which live on the message thread.
    auto* mm = juce::MessageManager::getInstanceWithoutCreating();
    if (mm == nullptr || mm->isThisTheMessageThread())
        applyLoadedState (tree);
    else
        juce::MessageManager::callAsync ([this, tree, flag = alive]
        {
            if (flag->load()) applyLoadedState (tree);
        });
}

void MelodicSequencerProcessor::applyParams (const juce::ValueTree& paramsTree, bool skipRuntime)
{
    for (const auto& p : paramsTree)
    {
        const auto id = p.getProperty ("id").toString();
        if (id.isEmpty() || (skipRuntime && isRuntimeParam (id))) continue;
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) (double) p.getProperty ("value", param->convertFrom0to1 (param->getDefaultValue()))));
    }
}

void MelodicSequencerProcessor::mergeUserScales (const std::vector<UserScale>& incoming)
{
    bool changed = false;
    for (const auto& sc : incoming)
    {
        const bool known = std::any_of (userScales.begin(), userScales.end(), [&] (const UserScale& u) { return u.name == sc.name; });
        if (! known) { userScales.push_back (sc); changed = true; }
    }
    if (changed) saveUserScales();
}

void MelodicSequencerProcessor::applyLoadedState (const juce::ValueTree& root)
{
    const auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid()) applyParams (params, false);

    if (auto proj = root.getChildWithName ("PROJECT"); proj.isValid())
    {
        juce::String name = "Default";
        model.loadInPlace ([&] (ms::Project& dst) { if (! ser::treeToProject (proj, dst, &name)) ms::fillDemoProject (dst); }, name);
        model.presetName = name;
        model.markClean();
    }
    mergeUserScales (ser::treeToScales (root.getChildWithName ("USERSCALES")));
}

void MelodicSequencerProcessor::loadPreset (const PresetManager::Info& info)
{
    if (info.factory)
    {
        const int idx = info.factoryIndex;
        model.loadInPlace ([idx] (ms::Project& dst) { ms::fillFactoryProject (idx, dst); }, info.name);
    }
    else
    {
        const auto tree = PresetManager::readPreset (info.file);
        const auto proj = tree.getChildWithName ("PROJECT");
        if (! proj.isValid()) return;
        const auto params = tree.getChildWithName (apvts.state.getType());
        if (params.isValid()) applyParams (params, true);
        juce::String name = info.name;
        model.loadInPlace ([&] (ms::Project& dst) { if (! ser::treeToProject (proj, dst, nullptr)) ms::fillDemoProject (dst); }, name);
    }
    requestSceneLaunch (0);
}

bool MelodicSequencerProcessor::saveUserPreset (const juce::String& name)
{
    if (name.trim().isEmpty()) return false;
    juce::ValueTree root ("MSPRESET");
    root.setProperty ("stateVersion", ser::kStateVersion, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (ser::projectToTree (model.project, name.trim()), nullptr);
    if (! PresetManager::writePreset (name, root)) return false;
    model.presetName = name.trim();
    model.markClean();
    return true;
}

bool MelodicSequencerProcessor::deleteUserPreset (const PresetManager::Info& info)
{
    return PresetManager::deletePreset (info);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MelodicSequencerProcessor();
}
