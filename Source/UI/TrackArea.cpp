#include "TrackArea.h"
#include "Engine/Euclid.h"
#include "GeneratorPanel.h"

// ====================================================================== StepCells
StepCells::StepCells (EditorState& s, int t, const GridMetrics& g) : state (s), track (t), gm (g) {}

juce::Rectangle<int> StepCells::cellRect (int c) const
{
    const int x0 = (int) std::lround ((double) c * gm.cellW);
    const int x1 = (int) std::lround ((double) (c + 1) * gm.cellW);
    return { x0, 0, x1 - x0, getHeight() };
}

int StepCells::cellAt (int x) const
{
    const int c = (int) std::floor ((float) x / gm.cellW);
    return (c >= 0 && c < gm.cols) ? c : -1;
}

void StepCells::paint (juce::Graphics& g)
{
    if (track >= state.numTracks()) return;
    const auto& tr = state.pattern().scene.tracks[(size_t) track];
    const auto clip = g.getClipBounds();
    const bool isSel = state.selectedTrack == track;

    for (int c = 0; c < gm.cols; ++c)
    {
        const auto r = cellRect (c);
        if (! r.intersects (clip)) continue;

        const bool inRange = c < tr.stepCount;
        const auto& st = tr.steps[(size_t) c];
        const auto cell = r.toFloat().reduced (1.0f);

        juce::Colour col = ! inRange ? theme::bg.brighter (0.03f)
                                     : ((c / 4) % 2 == 0 ? theme::panelHi : theme::panel.brighter (0.05f));
        if (inRange && st.active)
            col = theme::accent.withMultipliedBrightness (0.55f + 0.45f * (float) st.velocity / 127.0f);
        if (c == shownPlayhead)
            col = col.brighter (0.35f);

        g.setColour (col);
        g.fillRoundedRectangle (cell, 2.5f);

        if (inRange && st.active && st.probability < 1.0f)   // little marker for probabilistic steps
        {
            g.setColour (theme::bg.withAlpha (0.5f));
            g.fillRect (cell.withHeight (3.0f));
        }
        if (inRange && st.active && st.reps > 0)
        {
            g.setColour (theme::bg.withAlpha (0.55f));
            g.fillRect (cell.withTop (cell.getBottom() - 3.0f));
        }
        if (isSel && state.selectedStep == c)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r.toFloat().reduced (1.0f), 2.5f, 1.5f);
        }
    }
}

void StepCells::updatePlayhead()
{
    const auto& t = state.proc.transportInfo;
    const int ph = t.playing.load(std::memory_order_relaxed) ? t.trackStep[(size_t) track].load (std::memory_order_relaxed) : -1;
    if (ph == shownPlayhead) return;
    const int old = shownPlayhead;
    shownPlayhead = ph;
    if (old >= 0) repaint (cellRect (old));
    if (ph >= 0)  repaint (cellRect (ph));
}

void StepCells::setActive (int cell, bool on)
{
    state.edit ("Toggle step", 5000 + track, [t = track, cell, on] (ms::Pattern& p)
    {
        if (t < p.scene.numTracks) p.scene.tracks[(size_t) t].steps[(size_t) cell].active = on;
    });
}

void StepCells::mouseDown (const juce::MouseEvent& e)
{
    mode = Mode::None;
    if (track >= state.numTracks()) return;
    const auto& tr = state.pattern().scene.tracks[(size_t) track];
    const int c = cellAt (e.x);
    if (c < 0 || c >= tr.stepCount) return;

    if (e.mods.isShiftDown())   // shift-click: select a step for editing (again to deselect)
    {
        state.select (track, (state.selectedTrack == track && state.selectedStep == c) ? -1 : c);
        return;
    }

    if (state.selectedTrack != track) state.select (track, -1);
    state.beginGesture();
    startCell = c;
    startY = e.y;
    startVel = tr.steps[(size_t) c].velocity;

    if (! tr.steps[(size_t) c].active)
    {
        mode = Mode::Paint;
        paintValue = true;
        setActive (c, true);
    }
    else
        mode = Mode::Pending;   // decided on drag (velocity / erase-sweep) or release (toggle off)
}

void StepCells::mouseDrag (const juce::MouseEvent& e)
{
    if (mode == Mode::None || track >= state.numTracks()) return;
    const int c = cellAt (e.x);

    if (mode == Mode::Pending)
    {
        if (std::abs (e.y - startY) > 3)
            mode = Mode::Velocity;
        else if (c >= 0 && c != startCell)
        {
            mode = Mode::Paint;
            paintValue = false;
            setActive (startCell, false);
        }
    }

    if (mode == Mode::Velocity)
    {
        const int v = juce::jlimit (1, 127, startVel + (startY - e.y));
        state.edit ("Step velocity", 6000 + track, [t = track, s = startCell, v] (ms::Pattern& p)
        {
            if (t < p.scene.numTracks) p.scene.tracks[(size_t) t].steps[(size_t) s].velocity = v;
        });
    }
    else if (mode == Mode::Paint && c >= 0 && c < state.pattern().scene.tracks[(size_t) track].stepCount)
    {
        if (state.pattern().scene.tracks[(size_t) track].steps[(size_t) c].active != paintValue)
            setActive (c, paintValue);
    }
}

void StepCells::mouseUp (const juce::MouseEvent&)
{
    if (mode == Mode::Pending && startCell >= 0)
        setActive (startCell, false);
    mode = Mode::None;
}

// ====================================================================== LaneView
LaneView::LaneView (EditorState& s, int t, const GridMetrics& g) : state (s), track (t), gm (g) {}

juce::Rectangle<int> LaneView::cellRect (int c) const
{
    const int x0 = (int) std::lround ((double) c * gm.cellW);
    const int x1 = (int) std::lround ((double) (c + 1) * gm.cellW);
    return { x0, 0, x1 - x0, getHeight() };
}

void LaneView::paint (juce::Graphics& g)
{
    if (track >= state.numTracks()) return;
    const auto& tr = state.pattern().scene.tracks[(size_t) track];
    const auto prm = state.lane[(size_t) track];
    const auto& info = paramInfo (prm);

    g.setColour (theme::panel);
    g.fillRect (getLocalBounds());

    for (int c = 0; c < tr.stepCount && c < gm.cols; ++c)
    {
        const auto r = cellRect (c).toFloat().reduced (1.0f, 2.0f);
        const auto& st = tr.steps[(size_t) c];
        const float n = (getParam (st, prm) - info.min) / (info.max - info.min);

        g.setColour (theme::panelHi.withAlpha (0.5f));
        g.fillRoundedRectangle (r, 2.0f);

        g.setColour (st.active ? theme::accent : theme::dim.withAlpha (0.6f));
        if (info.bipolar)
        {
            const float mid = r.getCentreY(), y = r.getBottom() - n * r.getHeight();
            g.fillRect (juce::Rectangle<float> (r.getX(), juce::jmin (mid, y), r.getWidth(), std::abs (mid - y) + 1.0f));
        }
        else
            g.fillRect (r.withTop (r.getBottom() - juce::jmax (1.0f, n * r.getHeight())));
    }
}

void LaneView::apply (const juce::MouseEvent& e)
{
    if (track >= state.numTracks()) return;
    const int c = (int) std::floor ((float) e.x / gm.cellW);
    if (c < 0 || c >= state.pattern().scene.tracks[(size_t) track].stepCount) return;

    const auto prm = state.lane[(size_t) track];
    const auto& info = paramInfo (prm);
    const float n = 1.0f - juce::jlimit (0.0f, 1.0f, ((float) e.y - 2.0f) / juce::jmax (1.0f, (float) getHeight() - 4.0f));
    const float v = info.min + n * (info.max - info.min);

    state.edit ("Lane edit", 7000 + track * 16 + (int) prm, [t = track, c, prm, v] (ms::Pattern& p)
    {
        if (t < p.scene.numTracks) setParam (p.scene.tracks[(size_t) t].steps[(size_t) c], prm, v);
    });
}

// ====================================================================== TrackRow
TrackRow::TrackRow (EditorState& s, int t, const GridMetrics& g)
    : state (s), track (t), gm (g), cells (s, t, g), laneView (s, t, g)
{
    expandBtn.onClick = [this] { state.expanded[(size_t) track] = ! state.expanded[(size_t) track]; state.sendChangeMessage(); };
    addAndMakeVisible (expandBtn);

    styleNoteBar (outNote);
    styleNoteBar (inNote);
    outNote.setTooltip ("Output note");
    inNote.setTooltip ("Input note (record / trigger)");
    outNote.onDragStart = inNote.onDragStart = [this] { state.beginGesture(); };
    outNote.onValueChange = [this] { const int v = (int) outNote.getValue(); editTrack ("Output note", 0, [v] (ms::Track& tr) { tr.outNote = v; }); };
    inNote.onValueChange  = [this] { const int v = (int) inNote.getValue();  editTrack ("Input note", 1, [v] (ms::Track& tr) { tr.inNote = v; }); };
    addAndMakeVisible (outNote);
    addAndMakeVisible (inNote);

    styleBar (offsetSlider, -24, 24, 1);
    offsetSlider.setTooltip ("Track transpose (semitones)");
    offsetSlider.textFromValueFunction = [] (double v) { return (v >= 0 ? "+" : "") + juce::String ((int) v); };
    offsetSlider.onDragStart = [this] { state.beginGesture(); };
    offsetSlider.onValueChange = [this] { const int v = (int) offsetSlider.getValue(); editTrack ("Track transpose", 2, [v] (ms::Track& tr) { tr.transposeOffset = v; }); };
    addAndMakeVisible (offsetSlider);

    velKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    velKnob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    velKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    velKnob.setRange (0.0, 2.0, 0.01);
    velKnob.setDoubleClickReturnValue (true, 1.0);
    velKnob.setTooltip ("Velocity scale");
    velKnob.onDragStart = [this] { state.beginGesture(); };
    velKnob.onValueChange = [this] { const float v = (float) velKnob.getValue(); editTrack ("Velocity scale", 3, [v] (ms::Track& tr) { tr.velScale = v; }); };
    addAndMakeVisible (velKnob);

    muteBtn.setClickingTogglesState (true);
    soloBtn.setClickingTogglesState (true);
    muteBtn.setColour (juce::TextButton::buttonOnColourId, theme::warn);
    muteBtn.onClick = [this] { const bool v = muteBtn.getToggleState(); editTrack ("Mute", -1, [v] (ms::Track& tr) { tr.mute = v; }); };
    soloBtn.onClick = [this] { const bool v = soloBtn.getToggleState(); editTrack ("Solo", -1, [v] (ms::Track& tr) { tr.solo = v; }); };
    addAndMakeVisible (muteBtn);
    addAndMakeVisible (soloBtn);

    addAndMakeVisible (cells);

    // ---- expanded: parameter lane
    for (int i = 0; i < kNumStepParams; ++i)
        laneBox.addItem (paramInfo ((StepParam) i).name, i + 1);
    laneBox.onChange = [this] { state.lane[(size_t) track] = (StepParam) (laneBox.getSelectedId() - 1); laneView.repaint(); };
    addChildComponent (laneBox);
    addChildComponent (laneView);

    resetBtn.onClick = [this]
    {
        const auto prm = state.lane[(size_t) track];
        const float def = paramInfo (prm).def;
        editTrack ("Reset lane", -1, [prm, def] (ms::Track& tr) { for (auto& st : tr.steps) setParam (st, prm, def); });
    };
    randBtn.onClick = [this]
    {
        const auto prm = state.lane[(size_t) track];
        editTrack ("Randomize lane", -1, [prm] (ms::Track& tr)
        {
            juce::Random rng;
            const auto& info = paramInfo (prm);
            for (int i = 0; i < tr.stepCount; ++i)
                setParam (tr.steps[(size_t) i], prm, info.min + rng.nextFloat() * (info.max - info.min));
        });
    };
    genBtn.setTooltip ("Fill this track with scale-aware notes (bassline, arpeggio, rolling groove)");
    genBtn.onClick = [this]
    {
        juce::CallOutBox::launchAsynchronously (std::make_unique<GeneratorPanel> (state, track), genBtn.getScreenBounds(), nullptr);
    };
    addChildComponent (resetBtn);
    addChildComponent (randBtn);
    addChildComponent (genBtn);

    // ---- expanded: Euclid
    styleBar (stepsSlider, 1, ms::kMaxSteps, 1);
    styleBar (pulsesSlider, 0, ms::kMaxSteps, 1);
    styleBar (rotSlider, -63, 63, 1);
    stepsSlider.textFromValueFunction  = [] (double v) { return "St " + juce::String ((int) v); };
    pulsesSlider.textFromValueFunction = [] (double v) { return "Pu " + juce::String ((int) v); };
    rotSlider.textFromValueFunction    = [] (double v) { return "Ro " + juce::String ((int) v); };
    stepsSlider.setTooltip ("Track step count (Euclid steps)");
    pulsesSlider.setTooltip ("Euclid pulses");
    rotSlider.setTooltip ("Euclid rotation");
    for (auto* sl : { &stepsSlider, &pulsesSlider, &rotSlider })
        sl->onDragStart = [this] { state.beginGesture(); };
    stepsSlider.onValueChange = [this]
    {
        const int v = (int) stepsSlider.getValue();
        editTrack ("Steps", 4, [v] (ms::Track& tr)
        {
            tr.euclid.steps = v;
            tr.stepCount = v;
            if (tr.euclid.live) ms::applyEuclid (tr);
        });
    };
    pulsesSlider.onValueChange = [this]
    {
        const int v = (int) pulsesSlider.getValue();
        editTrack ("Euclid pulses", 5, [v] (ms::Track& tr) { tr.euclid.pulses = v; if (tr.euclid.live) ms::applyEuclid (tr); });
    };
    rotSlider.onValueChange = [this]
    {
        const int v = (int) rotSlider.getValue();
        editTrack ("Euclid rotation", 6, [v] (ms::Track& tr) { tr.euclid.rotation = v; if (tr.euclid.live) ms::applyEuclid (tr); });
    };
    applyBtn.onClick = [this] { editTrack ("Apply Euclid", -1, [] (ms::Track& tr) { ms::applyEuclid (tr); }); };
    liveBtn.setClickingTogglesState (true);
    liveBtn.setTooltip ("Keep regenerating the rhythm as you turn the knobs");
    liveBtn.onClick = [this]
    {
        const bool v = liveBtn.getToggleState();
        editTrack ("Euclid live", -1, [v] (ms::Track& tr) { tr.euclid.live = v; if (v) ms::applyEuclid (tr); });
    };
    for (auto* c : std::initializer_list<juce::Component*> { &stepsSlider, &pulsesSlider, &rotSlider, &applyBtn, &liveBtn })
        addChildComponent (c);

    refresh();
}

void TrackRow::editTrack (const juce::String& name, int field, std::function<void (ms::Track&)> fn)
{
    const int t = track;
    const int key = field < 0 ? 0 : 1 + t * 100 + field;
    state.edit (name, key, [t, fn = std::move (fn)] (ms::Pattern& p)
    {
        if (t < p.scene.numTracks) fn (p.scene.tracks[(size_t) t]);
    });
}

int TrackRow::desiredHeight() const
{
    return rowH + (state.expanded[(size_t) track] ? laneH + euclidH : 0);
}

void TrackRow::refresh()
{
    if (track >= state.numTracks()) return;
    const auto& tr = state.pattern().scene.tracks[(size_t) track];
    const bool ex = state.expanded[(size_t) track];
    const auto n = juce::dontSendNotification;

    expandBtn.setButtonText (ex ? "v" : ">");
    muteBtn.setToggleState (tr.mute, n);
    soloBtn.setToggleState (tr.solo, n);
    outNote.setValue (tr.outNote, n);
    inNote.setValue (tr.inNote, n);
    offsetSlider.setValue (tr.transposeOffset, n);
    velKnob.setValue (tr.velScale, n);

    laneBox.setSelectedId ((int) state.lane[(size_t) track] + 1, n);
    stepsSlider.setValue (tr.euclid.steps, n);
    pulsesSlider.setValue (tr.euclid.pulses, n);
    rotSlider.setValue (tr.euclid.rotation, n);
    liveBtn.setToggleState (tr.euclid.live, n);

    for (auto* c : std::initializer_list<juce::Component*> { &laneView, &laneBox, &resetBtn, &randBtn, &genBtn,
                                                              &stepsSlider, &pulsesSlider, &rotSlider, &applyBtn, &liveBtn })
        c->setVisible (ex);

    cells.repaint();
    laneView.repaint();
    repaint();
}

void TrackRow::paint (juce::Graphics& g)
{
    const bool sel = state.selectedTrack == track;
    g.setColour (sel ? theme::panelHi : theme::panel);
    g.fillRoundedRectangle (juce::Rectangle<int> (0, 0, headerW - 2, rowH - 2).toFloat(), 4.0f);
    if (sel)
    {
        g.setColour (theme::accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 3.0f, 3.0f, (float) rowH - 8.0f), 1.5f);
    }
    if (state.expanded[(size_t) track])
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (juce::Rectangle<int> (0, rowH, headerW - 2, laneH + euclidH - 2).toFloat(), 4.0f);
    }
}

void TrackRow::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (rowH);
    auto hdr = top.removeFromLeft (headerW).reduced (4, 3);
    hdr.setWidth (hdr.getWidth() - 4);

    expandBtn.setBounds (hdr.removeFromLeft (18)); hdr.removeFromLeft (2);
    outNote.setBounds (hdr.removeFromLeft (44));   hdr.removeFromLeft (2);
    inNote.setBounds (hdr.removeFromLeft (44));    hdr.removeFromLeft (2);
    offsetSlider.setBounds (hdr.removeFromLeft (44)); hdr.removeFromLeft (4);
    velKnob.setBounds (hdr.removeFromLeft (24).withSizeKeepingCentre (24, 24)); hdr.removeFromLeft (4);
    muteBtn.setBounds (hdr.removeFromLeft (22));   hdr.removeFromLeft (2);
    soloBtn.setBounds (hdr.removeFromLeft (22));
    cells.setBounds (top);

    if (state.expanded[(size_t) track])
    {
        auto laneStrip = r.removeFromTop (laneH);
        auto lh = laneStrip.removeFromLeft (headerW).reduced (4, 2);
        auto l1 = lh.removeFromTop (19);
        laneBox.setBounds (l1.removeFromLeft (120));
        lh.removeFromTop (2);
        resetBtn.setBounds (lh.removeFromLeft (46)); lh.removeFromLeft (3);
        randBtn.setBounds (lh.removeFromLeft (46));  lh.removeFromLeft (3);
        genBtn.setBounds (lh.removeFromLeft (64));
        laneView.setBounds (laneStrip);

        auto eu = r.removeFromTop (euclidH).removeFromLeft (headerW).reduced (4, 3);
        stepsSlider.setBounds (eu.removeFromLeft (52));  eu.removeFromLeft (2);
        pulsesSlider.setBounds (eu.removeFromLeft (52)); eu.removeFromLeft (2);
        rotSlider.setBounds (eu.removeFromLeft (52));    eu.removeFromLeft (2);
        applyBtn.setBounds (eu.removeFromLeft (44));     eu.removeFromLeft (2);
        liveBtn.setBounds (eu.removeFromLeft (36));
    }
}

void TrackRow::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Remove track", state.numTracks() > 1);
        m.addItem (2, "Duplicate track", state.numTracks() < ms::kMaxTracks);
        m.showMenuAsync (juce::PopupMenu::Options(), [this, t = track] (int id)
        {
            if (id == 1)
                state.edit ("Remove track", 0, [t] (ms::Pattern& p)
                {
                    const int n = p.scene.numTracks;
                    for (int i = t; i < n - 1; ++i) p.scene.tracks[(size_t) i] = p.scene.tracks[(size_t) i + 1];
                    p.scene.tracks[(size_t) n - 1] = ms::Track {};
                    --p.scene.numTracks;
                });
            else if (id == 2)
                state.edit ("Duplicate track", 0, [t] (ms::Pattern& p)
                {
                    const int n = p.scene.numTracks;
                    for (int i = n; i > t + 1; --i) p.scene.tracks[(size_t) i] = p.scene.tracks[(size_t) i - 1];
                    p.scene.tracks[(size_t) t + 1] = p.scene.tracks[(size_t) t];
                    ++p.scene.numTracks;
                });
        });
        return;
    }
    state.select (track, -1);
}

// ====================================================================== TrackArea
namespace { constexpr int kRulerH = 16, kAddH = 26; }

TrackArea::TrackArea (EditorState& s) : state (s)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    addTrackBtn.onClick = [this]
    {
        state.edit ("Add track", 0, [] (ms::Pattern& p)
        {
            if (p.scene.numTracks >= ms::kMaxTracks) return;
            auto& t = p.scene.tracks[(size_t) p.scene.numTracks];
            t = ms::Track {};
            t.outNote = 60;
            t.inNote = 60;
            t.stepCount = p.scene.tracks[0].stepCount;
            t.euclid.steps = t.stepCount;
            ++p.scene.numTracks;
        });
        state.select (state.numTracks() - 1, -1);
    };
    addAndMakeVisible (addTrackBtn);

    state.addChangeListener (this);
    rebuild();
    startTimerHz (30);
}

TrackArea::~TrackArea()
{
    stopTimer();
    state.removeChangeListener (this);
}

bool TrackArea::updateMetrics()
{
    int cols = 1;
    for (int t = 0; t < state.numTracks(); ++t)
        cols = juce::jmax (cols, state.pattern().scene.tracks[(size_t) t].stepCount);
    const float w = (float) juce::jmax (1, getWidth() - 10 - TrackRow::headerW);
    const float cw = w / (float) cols;
    const bool changed = cols != gm.cols || std::abs (cw - gm.cellW) > 0.01f;
    gm.cols = cols;
    gm.cellW = cw;
    return changed;
}

void TrackArea::layoutRows()
{
    const int w = juce::jmax (1, getWidth() - 10);
    int y = 0;
    for (auto* r : rows)
    {
        const int h = r->desiredHeight();
        r->setBounds (0, y, w, h);
        y += h + 2;
    }
    content.setSize (w, juce::jmax (y, viewport.getHeight()));
}

void TrackArea::rebuild()
{
    rows.clear();
    for (int t = 0; t < state.numTracks(); ++t)
        content.addAndMakeVisible (rows.add (new TrackRow (state, t, gm)));
    updateMetrics();
    layoutRows();
    for (auto* r : rows) r->refresh();
    addTrackBtn.setEnabled (state.numTracks() < ms::kMaxTracks);
    repaint();
}

void TrackArea::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (state.numTracks() != rows.size())
    {
        rebuild();
        return;
    }
    updateMetrics();
    layoutRows();
    for (auto* r : rows) r->refresh();
    repaint (0, 0, getWidth(), kRulerH);
}

void TrackArea::timerCallback()
{
    for (auto* r : rows) r->updatePlayhead();
}

void TrackArea::paint (juce::Graphics& g)
{
    g.setColour (theme::dim);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("TRACKS", 6, 0, 80, kRulerH, juce::Justification::centredLeft);
    for (int c = 0; c < gm.cols; c += 4)   // bar numbers: 1, 5, 9, ...
    {
        const int x = TrackRow::headerW + (int) std::lround ((double) c * gm.cellW);
        g.drawText (juce::String (c + 1), x + 2, 0, 28, kRulerH, juce::Justification::centredLeft);
    }
}

void TrackArea::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (kRulerH);
    auto bottom = r.removeFromBottom (kAddH);
    viewport.setBounds (r);
    addTrackBtn.setBounds (bottom.removeFromLeft (TrackRow::headerW - 2).reduced (0, 2));
    updateMetrics();
    layoutRows();
    for (auto* row : rows) row->repaint();
}
