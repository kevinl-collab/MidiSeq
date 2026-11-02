#pragma once
#include "Engine/Metronome.h"
#include "Engine/Pattern.h"
#include "Engine/Recording.h"
#include "Engine/Sequencer.h"
#include "Engine/TripleBuffer.h"
#include "PresetManager.h"
#include "ProjectModel.h"
#include "ProjectSerializer.h"
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

class MelodicSequencerProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    MelodicSequencerProcessor();
    ~MelodicSequencerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- message thread: the project being edited (undo, scenes, recording all live here)
    ProjectModel model;

    /** Ask the audio thread to launch a scene (quantized per Global::launchQuantize). */
    void requestSceneLaunch (int scene) { pendingScene.store (scene); }

    /** The track that receives incoming MIDI notes that don't match any track's input note. */
    std::atomic<int> recordTargetTrack { 0 };

    // ---- UI -> audio: pad audition (message thread pushes, audio thread pops)
    struct AuditionEvent { int channel = 1, note = 60, velocity = 100; bool on = false; int track = -1; };
    void pushAudition (const AuditionEvent& e)
    {
        int s1, n1, s2, n2;
        auditionFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0)      auditionBuf[(size_t) s1] = e;
        else if (n2 > 0) auditionBuf[(size_t) s2] = e;
        auditionFifo.finishedWrite (n1 + n2);
    }

    // ---- message thread only: user-made scales. Saved in the session and in a global file shared by all instances.
    using UserScale = ser::NamedScale;
    std::vector<UserScale> userScales;
    void saveUserScales() { PresetManager::saveUserScales (userScales); }

    /** Project globals with the automatable parameters applied - what the audio thread actually plays with. */
    ms::Global effectiveGlobal() const
    {
        ms::Global g = model.project.global;
        g.offset = (int) pOffset->load();
        g.transpose = (int) pTranspose->load();
        g.shuffleAmount = pShuffleAmount->load();
        g.shuffleType = (ms::ShuffleType) juce::jlimit (0, 3, (int) pShuffleType->load());
        return g;
    }

    // ---- presets (message thread)
    std::vector<PresetManager::Info> listPresets() const { return PresetManager::list(); }
    void loadPreset (const PresetManager::Info&);
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (const PresetManager::Info&);

    // ---- audio -> UI (relaxed atomics, polled from timers)
    struct TransportInfo
    {
        std::atomic<double> ppq { 0.0 }, bpm { 120.0 };
        std::atomic<bool> playing { false }, usingHostClock { false }, looping { false }, recording { false };
        std::atomic<int> timeSigNum { 4 }, timeSigDen { 4 };
        std::atomic<int> activeScene { 0 }, queuedScene { -1 }, midiTranspose { 0 };
        std::array<std::atomic<int>, ms::kMaxTracks> trackStep;
        TransportInfo() { for (auto& s : trackStep) s.store (-1); }
    } transportInfo;

    juce::AudioProcessorValueTreeState apvts;

private:
    struct RecordEvent { double ppq = 0.0; int track = -1, note = 60, velocity = 100; uint32_t pass = 0; };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void emitEvents (juce::MidiBuffer&);
    void timerCallback() override;
    void handleRecordEvent (const RecordEvent&);
    void applyLoadedState (const juce::ValueTree& root);
    void applyParams (const juce::ValueTree& paramsTree, bool skipRuntime);
    void mergeUserScales (const std::vector<UserScale>& incoming);
    void pushRecord (const RecordEvent&);

    static constexpr int stateVersion = 1;
    static constexpr int kRecordKey = 424242;

    ms::TripleBuffer<ms::Project> projectExchange;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    juce::AbstractFifo auditionFifo { 64 }, recordFifo { 256 };
    std::array<AuditionEvent, 64> auditionBuf {};
    std::array<RecordEvent, 256> recordBuf {};
    std::atomic<int> pendingScene { -1 };

    // audio-thread state
    ms::Sequencer sequencer;
    ms::Metronome metronome;
    ms::MidiTranspose midiTranspose;
    ms::EventList events;
    double internalPpq = 0.0;
    int lastSceneParam = -1;

    // message-thread recording state
    std::array<uint32_t, ms::kMaxTracks> lastPassForTrack {};

    std::atomic<float> *pTranspose = nullptr, *pOffset = nullptr, *pShuffleAmount = nullptr, *pShuffleType = nullptr,
                       *pInternalClock = nullptr, *pInternalRun = nullptr, *pInternalBpm = nullptr, *pScene = nullptr,
                       *pRecordArm = nullptr, *pRecordMode = nullptr, *pQuantStrength = nullptr,
                       *pMetronome = nullptr, *pTransposeMode = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodicSequencerProcessor)
};
