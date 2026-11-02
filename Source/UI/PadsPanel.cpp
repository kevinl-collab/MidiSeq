#include "PadsPanel.h"

class PadsPanel::Pad : public juce::Component
{
public:
    Pad (EditorState& s, int idx) : state (s), index (idx) {}

    void paint (juce::Graphics& g) override
    {
        const bool exists = index < state.numTracks();
        const bool sel = exists && state.selectedTrack == index;
        auto r = getLocalBounds().toFloat().reduced (2.0f);

        g.setColour (! exists ? theme::panelHi.withAlpha (0.35f)
                              : down ? theme::accent : (sel ? theme::panelHi.brighter (0.15f) : theme::panelHi));
        g.fillRoundedRectangle (r, 5.0f);
        if (sel)
        {
            g.setColour (theme::accent);
            g.drawRoundedRectangle (r, 5.0f, 1.5f);
        }
        if (exists)
        {
            g.setColour (down ? theme::bg : theme::text);
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText (theme::noteName (state.pattern().scene.tracks[(size_t) index].outNote), getLocalBounds(),
                        juce::Justification::centred);
            g.setColour (down ? theme::bg : theme::dim);
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText (juce::String (index + 1), getLocalBounds().reduced (6, 4), juce::Justification::topLeft);
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (index >= state.numTracks()) return;
        state.select (index, -1);
        const auto& t = state.pattern().scene.tracks[(size_t) index];
        sounding = { t.outChannel, juce::jlimit (0, 127, t.outNote + t.transposeOffset), 100, false, index };
        state.proc.pushAudition ({ sounding.channel, sounding.note, 100, true, index });
        down = true;
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { release(); }
    ~Pad() override { release(); }

private:
    void release()
    {
        if (! down) return;
        down = false;
        state.proc.pushAudition ({ sounding.channel, sounding.note, 0, false, index });
        repaint();
    }

    EditorState& state;
    int index;
    bool down = false;
    MelodicSequencerProcessor::AuditionEvent sounding;
};

PadsPanel::PadsPanel (EditorState& s) : state (s)
{
    styleSection (title, "PADS");
    addAndMakeVisible (title);

    presetBtn.setTooltip ("Presets: factory + your own (saved in Application Support). * = edited since loading.");
    presetBtn.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetBtn);
    refreshPresetButton();

    recordModeBox.addItemList ({ "Overdub", "Replace" }, 1);
    recordModeBox.setTooltip ("Overdub adds to the pattern; Replace wipes a track on its first recorded note each pass");
    styleBar (quantSlider, 0, 100, 1);
    quantSlider.setTextValueSuffix ("% quant");
    quantSlider.setTooltip ("Record quantize strength: 100% snaps to the grid, 0% keeps your timing as step shift");
    addAndMakeVisible (recordModeBox);
    addAndMakeVisible (quantSlider);
    modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s.proc.apvts, "recordMode", recordModeBox);
    quantAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s.proc.apvts, "quantStrength", quantSlider);

    for (int i = 0; i < 12; ++i)
        addAndMakeVisible (pads.add (new Pad (state, i)));

    state.addChangeListener (this);
}

PadsPanel::~PadsPanel() { state.removeChangeListener (this); }

void PadsPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto* p : pads) p->repaint();
    refreshPresetButton();
}

void PadsPanel::refreshPresetButton()
{
    presetBtn.setButtonText (state.proc.model.presetName + (state.proc.model.dirty ? "*" : ""));
}

void PadsPanel::showPresetMenu()
{
    const auto presets = state.proc.listPresets();
    const auto current = state.proc.model.presetName;

    juce::PopupMenu menu, factory, user, del;
    bool haveUser = false, currentIsUser = false;
    for (size_t i = 0; i < presets.size(); ++i)
    {
        const bool isCurrent = presets[i].name == current;
        if (presets[i].factory)
            factory.addItem (100 + (int) i, presets[i].name, true, isCurrent);
        else
        {
            haveUser = true;
            currentIsUser = currentIsUser || isCurrent;
            user.addItem (100 + (int) i, presets[i].name, true, isCurrent);
            del.addItem (1000 + (int) i, presets[i].name);
        }
    }
    menu.addSubMenu ("Factory presets", factory);
    if (haveUser) menu.addSubMenu ("User presets", user);
    else          menu.addItem (-1, "User presets (none yet)", false);
    menu.addSeparator();
    menu.addItem (1, "Save as...");
    menu.addItem (2, "Save", currentIsUser);
    if (haveUser) menu.addSubMenu ("Delete preset", del);
    menu.addSeparator();
    menu.addItem (3, "Show presets folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [this, presets, current] (int id)
    {
        if (id >= 1000 && (size_t) (id - 1000) < presets.size())
        {
            const auto info = presets[(size_t) (id - 1000)];
            juce::Component::SafePointer<PadsPanel> safe (this);
            auto* w = new juce::AlertWindow ("Delete preset", "Delete \"" + info.name + "\"? This cannot be undone.",
                                             juce::MessageBoxIconType::WarningIcon);
            w->addButton ("Delete", 1);
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            if (state.lnf != nullptr) w->setLookAndFeel (state.lnf);
            w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, info, w] (int result)
            {
                if (result == 1 && safe != nullptr)
                {
                    safe->state.proc.deleteUserPreset (info);
                    safe->refreshPresetButton();
                }
                w->setLookAndFeel (nullptr);
            }), true);
        }
        else if (id >= 100 && (size_t) (id - 100) < presets.size())
        {
            state.proc.loadPreset (presets[(size_t) (id - 100)]);
            refreshPresetButton();
        }
        else if (id == 1)
            promptSaveAs();
        else if (id == 2)
        {
            state.proc.saveUserPreset (current);
            refreshPresetButton();
        }
        else if (id == 3)
            PresetManager::presetsDirectory().revealToUser();
    });
}

void PadsPanel::promptSaveAs()
{
    auto* w = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", state.proc.model.presetName);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    if (state.lnf != nullptr) w->setLookAndFeel (state.lnf);

    juce::Component::SafePointer<PadsPanel> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            safe->state.proc.saveUserPreset (w->getTextEditorContents ("name"));
            safe->refreshPresetButton();
        }
        w->setLookAndFeel (nullptr);
    }), true);
}

void PadsPanel::paint (juce::Graphics& g) { paintPanel (g, getLocalBounds()); }

void PadsPanel::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto head = r.removeFromTop (20);
    title.setBounds (head.removeFromLeft (40));
    presetBtn.setBounds (head);
    r.removeFromTop (3);
    auto opts = r.removeFromTop (20);
    recordModeBox.setBounds (opts.removeFromLeft (70)); opts.removeFromLeft (3);
    quantSlider.setBounds (opts);
    r.removeFromTop (3);

    const int cw = r.getWidth() / 4, ch = r.getHeight() / 3;
    for (int i = 0; i < pads.size(); ++i)
    {
        const int col = i % 4, row = 2 - i / 4;   // pad 1 at bottom-left, like hardware
        pads[i]->setBounds (r.getX() + col * cw, r.getY() + row * ch, cw, ch);
    }
}

SceneTabs::SceneTabs (EditorState& s) : state (s)
{
    for (auto* b : { &plus, &copyBtn, &pasteBtn, &dupBtn, &clearBtn })
        addAndMakeVisible (b);

    plus.setTooltip ("Add an empty scene (same track layout) and launch it");
    plus.onClick = [this]
    {
        if (state.proc.model.addScene())
            state.proc.requestSceneLaunch (state.project().numScenes - 1);
    };
    copyBtn.setTooltip ("Copy the viewed scene");
    copyBtn.onClick = [this] { state.proc.model.copyScene(); };
    pasteBtn.setTooltip ("Paste over the viewed scene");
    pasteBtn.onClick = [this] { state.proc.model.pasteScene(); };
    dupBtn.setTooltip ("Duplicate the viewed scene into a new one and launch it");
    dupBtn.onClick = [this]
    {
        if (state.proc.model.duplicateScene())
            state.proc.requestSceneLaunch (state.project().numScenes - 1);
    };
    clearBtn.setTooltip ("Clear every step of the viewed scene");
    clearBtn.onClick = [this] { state.proc.model.clearScene(); };

    quantBox.addItemList ({ "Next bar", "End of pattern", "Immediate" }, 1);
    quantBox.setTooltip ("When a launched scene takes over");
    quantBox.onChange = [this]
    {
        const auto q = (ms::LaunchQuantize) (quantBox.getSelectedId() - 1);
        state.edit ("Launch quantize", 0, [q] (ms::Pattern& p) { p.global.launchQuantize = q; });
    };
    addAndMakeVisible (quantBox);

    state.addChangeListener (this);
    refresh();
}

void SceneTabs::refresh()
{
    const int n = state.project().numScenes;
    if (tabs.size() != n)
    {
        tabs.clear();
        for (int i = 0; i < n; ++i)
        {
            auto* b = tabs.add (new juce::TextButton (juce::String::charToString ((juce::juce_wchar) ('A' + i))));
            b->setRadioGroupId (21);
            b->onClick = [this, i] { state.proc.requestSceneLaunch (i); };
            addAndMakeVisible (b);
        }
        resized();
    }
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setToggleState (i == state.viewScene(), juce::dontSendNotification);

    plus.setEnabled (n < ms::kMaxScenes);
    dupBtn.setEnabled (n < ms::kMaxScenes);
    pasteBtn.setEnabled (state.proc.model.hasClipboard());
    quantBox.setSelectedId ((int) state.pattern().global.launchQuantize + 1, juce::dontSendNotification);
}

void SceneTabs::updateQueued()
{
    const int queued = state.proc.transportInfo.queuedScene.load (std::memory_order_relaxed);
    const bool blink = (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
    for (int i = 0; i < tabs.size(); ++i)
    {
        const auto c = (i == queued && blink) ? theme::warn : theme::panelHi;
        if (tabs[i]->findColour (juce::TextButton::buttonColourId) != c)
            tabs[i]->setColour (juce::TextButton::buttonColourId, c);
    }
}

void SceneTabs::resized()
{
    auto r = getLocalBounds().reduced (0, 2);
    auto right = r.removeFromRight (330);
    for (auto* t : tabs) { t->setBounds (r.removeFromLeft (34)); r.removeFromLeft (3); }
    plus.setBounds (r.removeFromLeft (34));

    copyBtn.setBounds (right.removeFromLeft (44));  right.removeFromLeft (3);
    pasteBtn.setBounds (right.removeFromLeft (46)); right.removeFromLeft (3);
    dupBtn.setBounds (right.removeFromLeft (38));   right.removeFromLeft (3);
    clearBtn.setBounds (right.removeFromLeft (44)); right.removeFromLeft (6);
    quantBox.setBounds (right);
}