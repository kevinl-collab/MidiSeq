#include "EditPanel.h"
#include "Engine/Repeats.h"

// ====================================================================== RepeatDisplay
void RepeatDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::panelHi);
    g.fillRoundedRectangle (r, 4.0f);

    auto plot = r.reduced (8.0f, 6.0f);
    const float axisY = plot.getBottom() - 10.0f;
    g.setColour (theme::line);
    g.drawHorizontalLine ((int) axisY, plot.getX(), plot.getRight());

    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    for (int i = 0; i <= 5; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / 5.0f;
        g.setColour (theme::line);
        g.drawVerticalLine ((int) x, axisY - 2.0f, axisY + 3.0f);
        g.setColour (theme::dim);
        g.drawText (juce::String (i), juce::Rectangle<float> (x - 8.0f, axisY + 2.0f, 16.0f, 9.0f), juce::Justification::centred);
    }

    ms::Rng rng;
    rng.seed (1);   // fixed, so "random" shapes don't flicker while painting
    ms::RepeatHit hits[ms::kMaxRepeats + 1];
    const int n = ms::computeRepeats (step, rng, hits);
    const float h = axisY - plot.getY() - 2.0f;
    for (int i = 0; i < n; ++i)
    {
        const float pos = (float) (hits[i].offsetPpq / lenPpq);
        const bool beyond = pos > 5.0f;
        const float x = plot.getX() + plot.getWidth() * juce::jmin (pos, 5.0f) / 5.0f;
        const float bh = juce::jmax (3.0f, h * hits[i].velScale * (float) step.velocity / 127.0f);
        g.setColour (beyond ? theme::warn : theme::accent);
        g.fillRoundedRectangle (x - 2.0f, axisY - bh, 4.0f, bh, 1.5f);
    }
}

// ====================================================================== EditPanel
EditPanel::EditPanel (EditorState& s) : state (s)
{
    header.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    header.setColour (juce::Label::textColourId, theme::accent);
    addAndMakeVisible (header);

    fillStepLengthBox (lenBox);
    lenBox.onChange = [this]
    {
        const auto len = stepLengthFromIndex (lenBox.getSelectedId() - 1);
        editTrack ("Track step length", -1, [len] (ms::Track& t) { t.stepLength = len; });
    };
    linkLen.setClickingTogglesState (true);
    linkLen.setTooltip ("Follow the global step length");
    linkLen.onClick = [this]
    {
        const bool v = linkLen.getToggleState();
        editTrack ("Link step length", -1, [v] (ms::Track& t) { t.linkStepLength = v; });
    };

    shuffleBox.addItemList ({ "Accurate", "MPC", "808", "Humanize" }, 1);
    shuffleBox.onChange = [this]
    {
        const auto type = (ms::ShuffleType) (shuffleBox.getSelectedId() - 1);
        editTrack ("Track shuffle type", -1, [type] (ms::Track& t) { t.shuffleType = type; });
    };
    styleBar (shuffleAmount, 0, 100, 1);
    shuffleAmount.setTextValueSuffix ("%");
    shuffleAmount.onDragStart = [this] { state.beginGesture(); };
    shuffleAmount.onValueChange = [this]
    {
        const float v = (float) shuffleAmount.getValue();
        editTrack ("Track shuffle amount", 1, [v] (ms::Track& t) { t.shuffleAmount = v; });
    };
    linkShuffle.setClickingTogglesState (true);
    linkShuffle.setTooltip ("Follow the global shuffle");
    linkShuffle.onClick = [this]
    {
        const bool v = linkShuffle.getToggleState();
        editTrack ("Link shuffle", -1, [v] (ms::Track& t) { t.linkShuffle = v; });
    };

    for (auto* c : std::initializer_list<juce::Component*> { &lenBox, &linkLen, &shuffleBox, &shuffleAmount, &linkShuffle, &repeatDisplay })
        addAndMakeVisible (c);

    // enum order: Forward, Reverse, PingPong, Pendulum, Random, RandomWalk
    const char* dirNames[] = { "Fwd", "Rev", "Ping", "Pend", "Rand", "Walk" };
    const char* dirTips[] = { "Forward", "Reverse", "Ping-pong", "Pendulum", "Random", "Random walk" };
    for (int i = 0; i < 6; ++i)
    {
        auto& b = dirButtons[i];
        b.setButtonText (dirNames[i]);
        b.setTooltip (dirTips[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (31);
        b.onClick = [this, i]
        {
            if (! dirButtons[i].getToggleState()) return;
            editTrack ("Direction", -1, [i] (ms::Track& t) { t.direction = (ms::Direction) i; });
        };
        addAndMakeVisible (b);
    }
    styleSection (dirLabel, "DIRECTION");
    styleSection (chokeLabel, "CHOKE");
    addAndMakeVisible (dirLabel);
    addAndMakeVisible (chokeLabel);

    chokeBox.addItemList ({ "Off", "Cut itself", "Group 1", "Group 2", "Group 3", "Group 4" }, 1);
    chokeBox.onChange = [this]
    {
        const int mode = chokeBox.getSelectedId() - 1;
        editTrack ("Choke", -1, [mode] (ms::Track& t) { t.chokeMode = mode; });
    };
    addAndMakeVisible (chokeBox);

    followBtn.setClickingTogglesState (true);
    followBtn.setTooltip ("On: this track follows the global / MIDI transpose. Off: it stays fixed (drum tracks).");
    followBtn.onClick = [this]
    {
        const bool v = followBtn.getToggleState();
        editTrack ("Follow transpose", -1, [v] (ms::Track& t) { t.followTranspose = v; });
    };
    addAndMakeVisible (followBtn);

    for (int i = 0; i < kNumStepParams; ++i)
    {
        const auto prm = (StepParam) i;
        const auto& info = paramInfo (prm);
        auto k = std::make_unique<LabeledKnob> (info.name);
        k->slider.setRange (info.min, info.max, info.isInt ? 1.0 : 0.005);
        k->slider.setDoubleClickReturnValue (true, info.def);
        switch (prm)
        {
            case StepParam::Probability:
                k->slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
                break;
            case StepParam::Shape:
                k->slider.textFromValueFunction = [] (double v) { return juce::String (shapeName ((int) v)); };
                break;
            case StepParam::Curve:
                k->slider.textFromValueFunction = [] (double v) { return juce::String ((int) v); };
                break;
            case StepParam::Interval:
                k->slider.setSkewFactorFromMidPoint (0.2);
                k->slider.textFromValueFunction = [] (double v) { return juce::String (v, 3); };
                break;
            case StepParam::Shift:
                k->slider.textFromValueFunction = [] (double v) { return (v >= 0 ? "+" : "") + juce::String (v, 2); };
                break;
            default:
                k->slider.textFromValueFunction = [info] (double v) { return info.isInt ? juce::String ((int) v) : juce::String (v, 2); };
                break;
        }
        k->slider.onDragStart = [this] { state.beginGesture(); };
        k->slider.onValueChange = [this, prm, ptr = k.get()] { editParam (prm, (float) ptr->slider.getValue()); };
        addAndMakeVisible (*k);
        knobs[(size_t) i] = std::move (k);
    }

    state.addChangeListener (this);
    refresh();
}

void EditPanel::resized()
{
    auto r = getLocalBounds().reduced (6);

    auto row1 = r.removeFromTop (22);
    header.setBounds (row1.removeFromLeft (112));
    lenBox.setBounds (row1.removeFromLeft (52));   row1.removeFromLeft (2);
    linkLen.setBounds (row1.removeFromLeft (34));  row1.removeFromLeft (8);
    shuffleBox.setBounds (row1.removeFromLeft (72)); row1.removeFromLeft (2);
    shuffleAmount.setBounds (row1.removeFromLeft (56)); row1.removeFromLeft (2);
    linkShuffle.setBounds (row1.removeFromLeft (34));

    r.removeFromTop (4);
    auto row2 = r.removeFromTop (52);
    repeatDisplay.setBounds (row2.removeFromLeft (150)); row2.removeFromLeft (8);

    auto dir = row2.removeFromLeft (156);
    dirLabel.setBounds (dir.removeFromTop (11));
    auto d1 = dir.removeFromTop (18);
    auto d2 = dir.removeFromTop (18).translated (0, 2);
    for (int i = 0; i < 3; ++i) dirButtons[i].setBounds (d1.removeFromLeft (50).reduced (1, 0));
    for (int i = 3; i < 6; ++i) dirButtons[i].setBounds (d2.removeFromLeft (50).reduced (1, 0));

    row2.removeFromLeft (8);
    chokeLabel.setBounds (row2.removeFromTop (11));
    chokeBox.setBounds (row2.removeFromTop (21));
    row2.removeFromTop (1);
    followBtn.setBounds (row2.removeFromTop (17));

    r.removeFromTop (4);
    const int kw = r.getWidth() / kNumStepParams;
    for (auto& k : knobs)
        k->setBounds (r.removeFromLeft (kw));
}

void EditPanel::editTrack (const juce::String& name, int field, std::function<void (ms::Track&)> fn)
{
    const int t = state.selectedTrack;
    const int key = field < 0 ? 0 : 9000 + t * 100 + field;
    state.edit (name, key, [t, fn = std::move (fn)] (ms::Pattern& p)
    {
        if (t < p.scene.numTracks) fn (p.scene.tracks[(size_t) t]);
    });
}

void EditPanel::editParam (StepParam prm, float v)
{
    const int t = state.selectedTrack, sel = state.selectedStep;
    state.edit (paramInfo (prm).name, 8000 + t * 16 + (int) prm, [t, sel, prm, v] (ms::Pattern& p)
    {
        if (t >= p.scene.numTracks) return;
        auto& tr = p.scene.tracks[(size_t) t];
        if (sel >= 0)
            setParam (tr.steps[(size_t) sel], prm, v);
        else
            for (int i = 0; i < tr.stepCount; ++i)   // no step selected: apply to the track's active steps
                if (tr.steps[(size_t) i].active) setParam (tr.steps[(size_t) i], prm, v);
    });
}

void EditPanel::refresh()
{
    const auto n = juce::dontSendNotification;
    const auto& pat = state.pattern();
    if (state.selectedTrack >= pat.scene.numTracks) return;
    const auto& tr = pat.scene.tracks[(size_t) state.selectedTrack];

    juce::String h = theme::noteName (tr.outNote) + juce::String::fromUTF8 (" \xc2\xb7 ") + theme::noteName (tr.inNote);
    if (state.selectedStep >= 0) h << "  #" << (state.selectedStep + 1);
    header.setText (h, n);

    linkLen.setToggleState (tr.linkStepLength, n);
    lenBox.setEnabled (! tr.linkStepLength);
    lenBox.setSelectedId (stepLengthIndex (tr.linkStepLength ? pat.global.stepLength : tr.stepLength) + 1, n);

    linkShuffle.setToggleState (tr.linkShuffle, n);
    shuffleBox.setEnabled (! tr.linkShuffle);
    shuffleAmount.setEnabled (! tr.linkShuffle);
    shuffleBox.setSelectedId ((int) (tr.linkShuffle ? pat.global.shuffleType : tr.shuffleType) + 1, n);
    shuffleAmount.setValue (tr.linkShuffle ? pat.global.shuffleAmount : tr.shuffleAmount, n);

    dirButtons[(int) tr.direction].setToggleState (true, n);
    chokeBox.setSelectedId (tr.chokeMode + 1, n);
    followBtn.setToggleState (tr.followTranspose, n);

    // which step do the knobs show? the selected one, else the first active one, else defaults
    ms::Step shown;
    if (state.selectedStep >= 0)
        shown = tr.steps[(size_t) state.selectedStep];
    else
        for (int i = 0; i < tr.stepCount; ++i)
            if (tr.steps[(size_t) i].active) { shown = tr.steps[(size_t) i]; break; }

    for (int i = 0; i < kNumStepParams; ++i)
        knobs[(size_t) i]->slider.setValue (getParam (shown, (StepParam) i), n);

    const bool reps = shown.reps > 0;   // Interval / Shape / Curve only mean something with repeats
    knobs[(size_t) StepParam::Interval]->setEnabledLook (reps);
    knobs[(size_t) StepParam::Shape]->setEnabledLook (reps);
    knobs[(size_t) StepParam::Curve]->setEnabledLook (reps);

    repeatDisplay.set (shown, ms::stepLengthPpq (tr.linkStepLength ? pat.global.stepLength : tr.stepLength));
}
