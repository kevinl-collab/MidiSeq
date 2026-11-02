#include "GeneratorPanel.h"

GeneratorPanel::GeneratorPanel (EditorState& st, int t) : state (st), track (t), s (st.genSettings[(size_t) t])
{
    presetBox.addItemList ({ "Custom", "Bassline", "Arpeggio", "Rolling groove" }, 1);
    arpBox.addItemList ({ "Up", "Down", "Up / down", "Random" }, 1);

    styleBar (low, -36, 0, 1);       low.setTextValueSuffix (" st");
    styleBar (high, 0, 48, 1);       high.setTextValueSuffix (" st");
    styleBar (density, 0, 100, 1);   density.setTextValueSuffix ("%");
    styleBar (leap, 0, 100, 1);      leap.setTextValueSuffix ("%");
    styleBar (root, 0, 100, 1);      root.setTextValueSuffix ("%");
    styleBar (gate, 5, 100, 1);      gate.setTextValueSuffix ("%");
    styleBar (seed, 1, 9999, 1);

    styleSection (lPreset, "PRESET");
    styleSection (lArp, "ARP");
    styleSection (lLow, "LOW");
    styleSection (lHigh, "HIGH");
    styleSection (lDensity, "DENSITY");
    styleSection (lLeap, "LEAPS");
    styleSection (lRoot, "ROOT");
    styleSection (lGate, "LENGTH");
    styleSection (lSeed, "SEED");
    styleSection (hint, "Notes snap to the global scale. Range is relative to the track's note.");

    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &arpBox, &low, &high, &density, &leap, &root, &gate, &seed, &dice,
                                                             &lLow, &lHigh, &lDensity, &lLeap, &lRoot, &lGate, &lSeed, &lPreset, &lArp, &hint })
        addAndMakeVisible (c);

    presetBox.onChange = [this]
    {
        if (updating) return;
        const auto p = (ms::GenPreset) (presetBox.getSelectedId() - 1);
        if (p != ms::GenPreset::Custom) s = ms::presetSettings (p, s.seed);
        else s.preset = p;
        pull();
        apply (true);
    };
    arpBox.onChange = [this] { if (! updating) { push(); apply (false); } };
    for (auto* sl : { &low, &high, &density, &leap, &root, &gate, &seed })
    {
        sl->onDragStart = [this] { state.beginGesture(); };
        sl->onValueChange = [this] { if (! updating) { push(); apply (false); } };
    }
    dice.setTooltip ("New random seed");
    dice.onClick = [this]
    {
        s.seed = (uint32_t) juce::Random::getSystemRandom().nextInt (juce::Range<int> (1, 10000));
        pull();
        apply (true);
    };

    if (state.lnf != nullptr) setLookAndFeel (state.lnf);
    pull();
    setSize (270, 262);
}

GeneratorPanel::~GeneratorPanel()
{
    state.genSettings[(size_t) track] = s;
    setLookAndFeel (nullptr);
}

void GeneratorPanel::pull()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    const auto n = juce::dontSendNotification;
    presetBox.setSelectedId ((int) s.preset + 1, n);
    arpBox.setSelectedId ((int) s.arp + 1, n);
    low.setValue (s.lowSemis, n);
    high.setValue (s.highSemis, n);
    density.setValue (s.density * 100.0f, n);
    leap.setValue (s.leap * 100.0f, n);
    root.setValue (s.rootEmphasis * 100.0f, n);
    gate.setValue (s.gate * 100.0f, n);
    seed.setValue ((double) s.seed, n);
    arpBox.setEnabled (s.preset == ms::GenPreset::Arpeggio);
}

void GeneratorPanel::push()
{
    s.lowSemis = (int) low.getValue();
    s.highSemis = (int) high.getValue();
    s.density = (float) density.getValue() / 100.0f;
    s.leap = (float) leap.getValue() / 100.0f;
    s.rootEmphasis = (float) root.getValue() / 100.0f;
    s.gate = (float) gate.getValue() / 100.0f;
    s.seed = (uint32_t) seed.getValue();
    s.arp = (ms::ArpPattern) juce::jmax (0, arpBox.getSelectedId() - 1);
}

void GeneratorPanel::apply (bool ownUndoStep)
{
    const int t = track;
    const auto cfg = s;
    state.edit ("Generate", ownUndoStep ? 0 : 31000 + t, [t, cfg] (ms::Pattern& p)
    {
        if (t < p.scene.numTracks) ms::generateTrack (p.scene.tracks[(size_t) t], p.global, cfg);
    });
}

void GeneratorPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto row = [&r] (juce::Label& label, juce::Component& c, int h = 22)
    {
        auto line = r.removeFromTop (h);
        label.setBounds (line.removeFromLeft (64));
        c.setBounds (line);
        r.removeFromTop (4);
    };
    row (lPreset, presetBox);
    row (lArp, arpBox);
    row (lLow, low);
    row (lHigh, high);
    row (lDensity, density);
    row (lLeap, leap);
    row (lRoot, root);
    row (lGate, gate);

    auto line = r.removeFromTop (22);
    lSeed.setBounds (line.removeFromLeft (64));
    dice.setBounds (line.removeFromRight (48));
    line.removeFromRight (4);
    seed.setBounds (line);
    r.removeFromTop (4);
    hint.setBounds (r);
}
