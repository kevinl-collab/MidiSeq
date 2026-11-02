#pragma once
#include "Engine/Types.h"
#include <juce_gui_basics/juce_gui_basics.h>

enum class StepParam { Velocity, Length, Shift, Probability, Reps, Interval, Shape, Curve, Gain };
constexpr int kNumStepParams = 9;

struct StepParamInfo { const char* name; float min, max, def; bool isInt, bipolar; };

inline const StepParamInfo& paramInfo (StepParam p)
{
    static const StepParamInfo table[kNumStepParams] = {
        { "Velocity", 1.f, 127.f, 100.f, true, false },
        { "Length", 0.05f, 2.f, 0.5f, false, false },
        { "Shift", -0.5f, 0.5f, 0.f, false, true },
        { "Prob", 0.f, 1.f, 1.f, false, false },
        { "Reps", 0.f, 8.f, 0.f, true, false },
        { "Interval", 0.03125f, 1.f, 0.25f, false, false },
        { "Shape", 0.f, 4.f, 0.f, true, false },
        { "Curve", -100.f, 100.f, 0.f, false, true },
        { "Gain", 0.f, 2.f, 1.f, false, false },
    };
    return table[(int) p];
}

inline float getParam (const ms::Step& s, StepParam p)
{
    switch (p)
    {
        case StepParam::Velocity:    return (float) s.velocity;
        case StepParam::Length:      return s.length;
        case StepParam::Shift:       return s.shift;
        case StepParam::Probability: return s.probability;
        case StepParam::Reps:        return (float) s.reps;
        case StepParam::Interval:    return s.interval;
        case StepParam::Shape:       return (float) (int) s.shape;
        case StepParam::Curve:       return s.curve;
        case StepParam::Gain:        return s.gain;
    }
    return 0.f;
}

inline void setParam (ms::Step& s, StepParam p, float v)
{
    const auto& i = paramInfo (p);
    v = juce::jlimit (i.min, i.max, v);
    switch (p)
    {
        case StepParam::Velocity:    s.velocity = (int) std::lround (v); break;
        case StepParam::Length:      s.length = v; break;
        case StepParam::Shift:       s.shift = v; break;
        case StepParam::Probability: s.probability = v; break;
        case StepParam::Reps:        s.reps = (int) std::lround (v); break;
        case StepParam::Interval:    s.interval = v; break;
        case StepParam::Shape:       s.shape = (ms::RepeatShape) (int) std::lround (v); break;
        case StepParam::Curve:       s.curve = v; break;
        case StepParam::Gain:        s.gain = v; break;
    }
}

inline const char* shapeName (int i)
{
    static const char* n[] = { "Flat", "Up", "Down", "Up/Dn", "Rand" };
    return n[juce::jlimit (0, 4, i)];
}

// ---- step length list: index = division * 3 + modifier (0 straight, 1 triplet, 2 dotted)
inline int stepLengthIndex (ms::StepLength l) { return (int) l.division * 3 + (int) l.modifier; }
inline ms::StepLength stepLengthFromIndex (int i)
{
    i = juce::jlimit (0, 11, i);
    return { (ms::StepDivision) (i / 3), (ms::StepModifier) (i % 3) };
}
inline void fillStepLengthBox (juce::ComboBox& box)
{
    static const char* base[] = { "1/4", "1/8", "1/16", "1/32" };
    static const char* mod[] = { "", "T", "D" };
    for (int i = 0; i < 12; ++i)
        box.addItem (juce::String (base[i / 3]) + mod[i % 3], i + 1);
}
