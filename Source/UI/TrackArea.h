#pragma once
#include "EditorState.h"
#include "Widgets.h"

struct GridMetrics
{
    int cols = 16;       // widest track, in steps
    float cellW = 20.0f; // width of one step cell
};

/** The step cells for one track: click/drag to paint, vertical drag on an active step for velocity,
    shift-click to select a step. Draws its own playhead cell. */
class StepCells : public juce::Component
{
public:
    StepCells (EditorState&, int track, const GridMetrics&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void updatePlayhead();

private:
    int cellAt (int x) const;
    juce::Rectangle<int> cellRect (int c) const;
    void setActive (int cell, bool on);

    enum class Mode { None, Pending, Paint, Velocity };

    EditorState& state;
    int track;
    const GridMetrics& gm;
    int shownPlayhead = -1;
    Mode mode = Mode::None;
    int startCell = -1, startVel = 100, startY = 0;
    bool paintValue = true;
};

/** Per-step bars for one parameter (velocity, length, ...). Click or sweep to draw values. */
class LaneView : public juce::Component
{
public:
    LaneView (EditorState&, int track, const GridMetrics&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { state.beginGesture(); apply (e); }
    void mouseDrag (const juce::MouseEvent& e) override { apply (e); }

private:
    void apply (const juce::MouseEvent&);
    juce::Rectangle<int> cellRect (int c) const;

    EditorState& state;
    int track;
    const GridMetrics& gm;
};

class TrackRow : public juce::Component
{
public:
    TrackRow (EditorState&, int track, const GridMetrics&);

    void refresh();
    void updatePlayhead() { cells.updatePlayhead(); }
    int desiredHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int headerW = 250, rowH = 28, laneH = 44, euclidH = 26;

private:
    void editTrack (const juce::String& name, int field, std::function<void (ms::Track&)> fn);
    void applyEuclidIfLive();

    EditorState& state;
    int track;
    const GridMetrics& gm;

    juce::TextButton expandBtn { ">" }, muteBtn { "M" }, soloBtn { "S" };
    juce::Slider outNote, inNote, offsetSlider, velKnob;
    StepCells cells;

    // expanded section
    LaneView laneView;
    juce::ComboBox laneBox;
    juce::TextButton resetBtn { "Reset" }, randBtn { "Rand" }, genBtn { "Generate" };
    juce::Slider stepsSlider, pulsesSlider, rotSlider;
    juce::TextButton applyBtn { "Apply" }, liveBtn { "Live" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackRow)
};

class TrackArea : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    explicit TrackArea (EditorState&);
    ~TrackArea() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuild();
    bool updateMetrics();
    void layoutRows();

    EditorState& state;
    GridMetrics gm;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<TrackRow> rows;
    juce::TextButton addTrackBtn { "+ Add track" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackArea)
};
