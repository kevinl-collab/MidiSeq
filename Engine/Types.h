#pragma once
// Plain-C++ data model. No JUCE dependency; everything is fixed-size so a Scene can be
// copied into a snapshot on the message thread and read on the audio thread with no allocation.
#include <algorithm>
#include <array>
#include <cstdint>

namespace ms
{
constexpr int kMaxSteps   = 64;
constexpr int kMaxTracks  = 16;
constexpr int kMaxScenes  = 8;      // A-H; also the MIDI scene-trigger range (C-1..G-1)
constexpr int kSceneTriggerBase = 12;   // MIDI note of C-1 in the C-2 = 0 naming used by the UI
constexpr int kMaxRepeats = 8;

struct Rng
{
    uint32_t state = 0x2545F491u;
    void seed (uint32_t s) { state = s != 0 ? s : 0x2545F491u; }
    uint32_t nextU32() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
    float next01() { return float (nextU32() >> 8) * (1.0f / 16777216.0f); }
    int nextInt (int n) { return n <= 1 ? 0 : int (nextU32() % uint32_t (n)); }
};

inline int64_t posMod (int64_t a, int64_t m) { const auto r = a % m; return r < 0 ? r + m : r; }

enum class Direction : uint8_t { Forward, Reverse, PingPong, Pendulum, Random, RandomWalk };
enum class StepDivision : uint8_t { Quarter, Eighth, Sixteenth, ThirtySecond };
enum class StepModifier : uint8_t { Straight, Triplet, Dotted };
enum class ShuffleType : uint8_t { Accurate, MPC, EightOhEight, Humanize };
enum class RepeatShape : uint8_t { Flat, RampUp, RampDown, UpDown, Random };
enum class Condition : uint8_t { Always, EveryN, First, Fill, NotFill };
enum class LaunchQuantize : uint8_t { NextBar, EndOfPattern, Immediate };

struct StepLength
{
    StepDivision division = StepDivision::Sixteenth;
    StepModifier modifier = StepModifier::Straight;
};

/** Length of one step in quarter notes (PPQ). */
inline double stepLengthPpq (StepLength l)
{
    double base = 0.25;
    switch (l.division)
    {
        case StepDivision::Quarter:      base = 1.0;   break;
        case StepDivision::Eighth:       base = 0.5;   break;
        case StepDivision::Sixteenth:    base = 0.25;  break;
        case StepDivision::ThirtySecond: base = 0.125; break;
    }
    if (l.modifier == StepModifier::Triplet) base *= 2.0 / 3.0;
    if (l.modifier == StepModifier::Dotted)  base *= 1.5;
    return base;
}

struct Step
{
    bool active = false;
    int velocity = 100;            // 1..127
    float length = 0.5f;           // gate, as a fraction of the step length
    float shift = 0.0f;            // micro-timing, fraction of a step (-0.5..0.5)
    float probability = 1.0f;      // 0..1
    int reps = 0;                  // 0..8 extra sub-hits
    float interval = 0.25f;        // average PPQ between repeats
    RepeatShape shape = RepeatShape::Flat;
    float curve = 0.0f;            // -100..100 (+ accelerate, - decelerate)
    float gain = 1.0f;
    Condition condition = Condition::Always;
    int conditionN = 2;            // for EveryN: plays on loops 0, N, 2N, ...
    int pitch = 0;                 // semitone offset from the track note
};

inline bool conditionPasses (const Step& s, int64_t loop, bool fill)
{
    switch (s.condition)
    {
        case Condition::Always: return true;
        case Condition::EveryN: return loop % std::max (2, s.conditionN) == 0;
        case Condition::First:  return loop == 0;
        case Condition::Fill:   return fill;
        case Condition::NotFill: return ! fill;
    }
    return true;
}

struct EuclidSettings
{
    int pulses = 4, steps = 16, rotation = 0;
    bool live = false;
};

struct Track
{
    int outNote = 60, inNote = 60, outChannel = 1;
    int transposeOffset = 0;       // semitones, per-track
    float velScale = 1.0f, gain = 1.0f;
    bool mute = false, solo = false, followTranspose = true;
    Direction direction = Direction::Forward;
    int stepCount = 16;
    StepLength stepLength;
    bool linkStepLength = true;
    ShuffleType shuffleType = ShuffleType::Accurate;
    float shuffleAmount = 0.0f;
    bool linkShuffle = true;
    int chokeMode = 0;             // 0 off, 1 cut itself, 2..5 = choke group 1..4
    EuclidSettings euclid;
    std::array<Step, kMaxSteps> steps {};
};

struct Scene
{
    int numTracks = 8;
    std::array<Track, kMaxTracks> tracks {};
};

struct Global
{
    StepLength stepLength;
    ShuffleType shuffleType = ShuffleType::Accurate;
    float shuffleAmount = 0.0f;    // 0..100
    int transpose = 0;             // semitones, applied to tracks with followTranspose
    int offset = 0;                // pattern start offset in steps
    bool scaleEnabled = false;
    int root = 0;                  // 0..11
    uint16_t scaleMask = 0xFFF;    // bit i = semitone i above root
    int rangeLow = 0, rangeHigh = 127;
    bool fill = false;
    LaunchQuantize launchQuantize = LaunchQuantize::NextBar;
    bool midiTransposeOn = false;
    int inChannel = 1;             // channel for MIDI-transpose and scene-trigger notes
};
} // namespace ms
