#include "GlobalPanel.h"

// ====================================================================== PianoKeyboard
namespace
{
constexpr int kWhitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
bool isBlack (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
int whiteIndexLeftOf (int blackPc) { return blackPc == 1 ? 0 : blackPc == 3 ? 1 : blackPc == 6 ? 3 : blackPc == 8 ? 4 : 5; }
}

juce::Rectangle<float> PianoKeyboard::keyRect (int pc) const
{
    const float w = (float) getWidth() / 7.0f, h = (float) getHeight();
    if (isBlack (pc))
        return { (float) (whiteIndexLeftOf (pc) + 1) * w - w * 0.3f, 0.0f, w * 0.6f, h * 0.6f };
    for (int i = 0; i < 7; ++i)
        if (kWhitePc[i] == pc)
            return { (float) i * w, 0.0f, w, h };
    return {};
}

int PianoKeyboard::keyAt (juce::Point<int> p) const
{
    const auto pt = p.toFloat();
    for (int pc = 0; pc < 12; ++pc)       // black keys sit on top, test them first
        if (isBlack (pc) && keyRect (pc).contains (pt)) return pc;
    for (int pc = 0; pc < 12; ++pc)
        if (! isBlack (pc) && keyRect (pc).contains (pt)) return pc;
    return -1;
}

void PianoKeyboard::paint (juce::Graphics& g)
{
    for (int pass = 0; pass < 2; ++pass)   // whites first, then blacks
        for (int pc = 0; pc < 12; ++pc)
        {
            if (isBlack (pc) != (pass == 1)) continue;
            const auto r = keyRect (pc).reduced (0.5f);
            const bool on = inScale (pc);
            const bool isRoot = pc == root;
            if (pass == 0)
                g.setColour (on ? (isRoot ? theme::accent : theme::accent.withMultipliedBrightness (0.7f)) : theme::text.withAlpha (0.85f));
            else
                g.setColour (on ? (isRoot ? theme::accent : theme::accentDim) : theme::bg);
            g.fillRoundedRectangle (r, 2.0f);
        }
}

void PianoKeyboard::mouseDown (const juce::MouseEvent& e)
{
    const int pc = keyAt (e.getPosition());
    if (pc >= 0 && onKeyClicked) onKeyClicked (pc);
}

// ====================================================================== GlobalPanel
GlobalPanel::GlobalPanel (EditorState& s) : state (s)
{
    styleSection (title, "GLOBAL");
    addAndMakeVisible (title);

    // shuffle: automatable, bound to the processor's parameters
    shuffleBox.addItemList ({ "Accurate", "MPC", "808", "Humanize" }, 1);
    styleBar (shuffleAmount, 0, 100, 1);
    shuffleAmount.setTextValueSuffix ("%");
    addAndMakeVisible (shuffleBox);
    addAndMakeVisible (shuffleAmount);
    shuffleTypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s.proc.apvts, "shuffleType", shuffleBox);
    shuffleAmountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s.proc.apvts, "shuffleAmount", shuffleAmount);

    // scale + root
    rebuildScaleItems();
    scaleBox.onChange = [this]
    {
        const int id = scaleBox.getSelectedId();
        if (id == 1)
            state.edit ("Scale off", 0, [] (ms::Pattern& p) { p.global.scaleEnabled = false; });
        else if (id >= 10 && id < 99)
        {
            const auto mask = ms::kBuiltInScales[id - 10].mask;
            state.edit ("Scale", 0, [mask] (ms::Pattern& p) { p.global.scaleMask = mask; p.global.scaleEnabled = true; });
        }
        else if (id >= 100)
        {
            const auto idx = (size_t) (id - 100);
            if (idx < state.proc.userScales.size())
            {
                const auto mask = state.proc.userScales[idx].mask;
                state.edit ("Scale", 0, [mask] (ms::Pattern& p) { p.global.scaleMask = mask; p.global.scaleEnabled = true; });
            }
        }
        else
            refresh();   // "Custom" is a status, not an action
    };
    addAndMakeVisible (scaleBox);

    for (int i = 0; i < 12; ++i) rootBox.addItem (theme::rootName (i), i + 1);
    rootBox.onChange = [this]
    {
        const int root = rootBox.getSelectedId() - 1;
        state.edit ("Root", 0, [root] (ms::Pattern& p) { p.global.root = root; });
    };
    addAndMakeVisible (rootBox);

    saveScaleBtn.setTooltip ("Save the keyboard selection as a custom scale");
    saveScaleBtn.onClick = [this] { promptSaveScale(); };
    addAndMakeVisible (saveScaleBtn);

    keyboard.onKeyClicked = [this] (int pc)
    {
        const int deg = (pc - state.pattern().global.root + 12) % 12;
        if (deg == 0) return;   // the root is always in the scale
        state.edit ("Scale degree", 0, [deg] (ms::Pattern& p)
        {
            p.global.scaleMask = (uint16_t) (p.global.scaleMask ^ (1u << deg));
            p.global.scaleEnabled = true;
        });
    };
    addAndMakeVisible (keyboard);

    // note range
    styleSection (rangeLabel, "RANGE");
    addAndMakeVisible (rangeLabel);
    styleNoteBar (rangeLow);
    styleNoteBar (rangeHigh);
    rangeLow.setTooltip ("Lowest output note");
    rangeHigh.setTooltip ("Highest output note");
    rangeLow.onDragStart = rangeHigh.onDragStart = [this] { state.beginGesture(); };
    rangeLow.onValueChange = [this]
    {
        const int v = (int) rangeLow.getValue();
        state.edit ("Range low", 20, [v] (ms::Pattern& p) { p.global.rangeLow = v; p.global.rangeHigh = juce::jmax (v, p.global.rangeHigh); });
    };
    rangeHigh.onValueChange = [this]
    {
        const int v = (int) rangeHigh.getValue();
        state.edit ("Range high", 21, [v] (ms::Pattern& p) { p.global.rangeHigh = v; p.global.rangeLow = juce::jmin (v, p.global.rangeLow); });
    };
    addAndMakeVisible (rangeLow);
    addAndMakeVisible (rangeHigh);

    // MIDI transpose (functional in stage 5)
    midiTransposeBtn.setClickingTogglesState (true);
    midiTransposeBtn.setTooltip ("Notes on the input channel set the transpose, relative to the root at C3, snapped to the scale. C-1..G-1 on that channel launch scenes.");
    midiTransposeBtn.onClick = [this]
    {
        const bool v = midiTransposeBtn.getToggleState();
        state.edit ("MIDI transpose", 0, [v] (ms::Pattern& p) { p.global.midiTransposeOn = v; });
    };
    for (int ch = 1; ch <= 16; ++ch) channelBox.addItem ("Ch " + juce::String (ch), ch);
    channelBox.onChange = [this]
    {
        const int ch = channelBox.getSelectedId();
        state.edit ("Input channel", 0, [ch] (ms::Pattern& p) { p.global.inChannel = ch; });
    };
    transposeModeBox.addItemList ({ "Latch", "Hold" }, 1);
    transposeModeBox.setTooltip ("Latch: the last key sets the transpose until the next one. Hold: back to 0 when released.");
    transposeModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s.proc.apvts, "transposeMode", transposeModeBox);
    addAndMakeVisible (midiTransposeBtn);
    addAndMakeVisible (channelBox);
    addAndMakeVisible (transposeModeBox);

    addAndMakeVisible (offsetKnob);
    addAndMakeVisible (transposeKnob);
    offsetAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s.proc.apvts, "offset", offsetKnob.slider);
    transposeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s.proc.apvts, "transpose", transposeKnob.slider);

    state.addChangeListener (this);
    refresh();
}

void GlobalPanel::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto row1 = r.removeFromTop (20);
    title.setBounds (row1.removeFromLeft (52));
    shuffleBox.setBounds (row1.removeFromLeft (92)); row1.removeFromLeft (4);
    shuffleAmount.setBounds (row1);

    r.removeFromTop (4);
    auto row2 = r.removeFromTop (22);
    saveScaleBtn.setBounds (row2.removeFromRight (24)); row2.removeFromRight (4);
    rootBox.setBounds (row2.removeFromRight (52));      row2.removeFromRight (4);
    scaleBox.setBounds (row2);

    r.removeFromTop (4);
    keyboard.setBounds (r.removeFromTop (40));

    r.removeFromTop (4);
    auto knobs = r.removeFromRight (132);
    offsetKnob.setBounds (knobs.removeFromLeft (66));
    transposeKnob.setBounds (knobs);

    r.removeFromRight (6);
    auto a = r.removeFromTop (24).reduced (0, 2);
    rangeLabel.setBounds (a.removeFromLeft (38));
    rangeLow.setBounds (a.removeFromLeft (56)); a.removeFromLeft (4);
    rangeHigh.setBounds (a.removeFromLeft (56));
    auto b = r.removeFromTop (24).reduced (0, 2);
    midiTransposeBtn.setBounds (b.removeFromLeft (62)); b.removeFromLeft (3);
    channelBox.setBounds (b.removeFromLeft (50));       b.removeFromLeft (3);
    transposeModeBox.setBounds (b);
}

void GlobalPanel::rebuildScaleItems()
{
    scaleBox.clear (juce::dontSendNotification);
    scaleBox.addItem ("No scale", 1);
    scaleBox.addSeparator();
    int i = 0;
    for (const auto& sc : ms::kBuiltInScales) scaleBox.addItem (sc.name, 10 + i++);
    if (! state.proc.userScales.empty()) scaleBox.addSeparator();
    for (size_t u = 0; u < state.proc.userScales.size(); ++u)
        scaleBox.addItem (state.proc.userScales[u].name, 100 + (int) u);
    scaleBox.addItem ("Custom", 99);
}

void GlobalPanel::refresh()
{
    const auto n = juce::dontSendNotification;
    const auto& g = state.pattern().global;

    int id = 99;   // custom
    if (! g.scaleEnabled)
        id = 1;
    else
    {
        for (size_t i = 0; i < std::size (ms::kBuiltInScales); ++i)
            if (ms::kBuiltInScales[i].mask == g.scaleMask) { id = 10 + (int) i; break; }
        if (id == 99)
            for (size_t u = 0; u < state.proc.userScales.size(); ++u)
                if (state.proc.userScales[u].mask == g.scaleMask) { id = 100 + (int) u; break; }
    }
    scaleBox.setSelectedId (id, n);
    rootBox.setSelectedId (g.root + 1, n);
    keyboard.setScale (g.scaleEnabled ? g.scaleMask : (uint16_t) 0xFFF, g.root);
    saveScaleBtn.setEnabled (g.scaleEnabled && id == 99);

    rangeLow.setValue (g.rangeLow, n);
    rangeHigh.setValue (g.rangeHigh, n);
    midiTransposeBtn.setToggleState (g.midiTransposeOn, n);
    channelBox.setSelectedId (g.inChannel, n);
}

void GlobalPanel::promptSaveScale()
{
    auto* w = new juce::AlertWindow ("Save scale", "Name for this scale:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "My Scale");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<GlobalPanel> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;
        const auto mask = safe->state.pattern().global.scaleMask;
        safe->state.proc.userScales.push_back ({ name, mask });
        safe->state.proc.saveUserScales();
        safe->rebuildScaleItems();
        safe->refresh();
    }), true);
}
