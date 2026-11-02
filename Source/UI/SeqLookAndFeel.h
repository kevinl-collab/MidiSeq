#pragma once
#include "Theme.h"

/** Flat, dark, lime-accent look: rounded rectangles, arc knobs. */
class SeqLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SeqLookAndFeel()
    {
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, theme::bg);
        setColour (Label::textColourId, theme::text);
        setColour (TextButton::buttonColourId, theme::panelHi);
        setColour (TextButton::buttonOnColourId, theme::accent);
        setColour (TextButton::textColourOffId, theme::text);
        setColour (TextButton::textColourOnId, theme::bg);
        setColour (ComboBox::backgroundColourId, theme::panelHi);
        setColour (ComboBox::outlineColourId, Colours::transparentBlack);
        setColour (ComboBox::textColourId, theme::text);
        setColour (ComboBox::arrowColourId, theme::dim);
        setColour (PopupMenu::backgroundColourId, theme::panel);
        setColour (PopupMenu::textColourId, theme::text);
        setColour (PopupMenu::highlightedBackgroundColourId, theme::accentDim);
        setColour (PopupMenu::highlightedTextColourId, theme::text);
        setColour (Slider::trackColourId, theme::accentDim);
        setColour (Slider::backgroundColourId, theme::panelHi);
        setColour (Slider::textBoxTextColourId, theme::text);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (AlertWindow::backgroundColourId, theme::panel);
        setColour (AlertWindow::textColourId, theme::text);
        setColour (TextEditor::backgroundColourId, theme::panelHi);
        setColour (TextEditor::textColourId, theme::text);
        setColour (TextEditor::outlineColourId, theme::line);
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
            return juce::Font (juce::FontOptions (11.0f));
        return LookAndFeel_V4::getLabelFont (l);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.0f)); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                               bool highlighted, bool down) override
    {
        auto c = bg;
        if (! b.isEnabled())   c = c.withAlpha (0.4f);
        else if (down)         c = c.brighter (0.25f);
        else if (highlighted)  c = c.brighter (0.12f);
        g.setColour (c);
        g.fillRoundedRectangle (b.getLocalBounds().toFloat().reduced (0.5f), 4.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId).withAlpha (box.isEnabled() ? 1.0f : 0.5f));
        g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 4.0f);
        juce::Path arrow;
        const float cx = (float) w - 10.0f, cy = (float) h * 0.5f;
        arrow.addTriangle (cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 2.5f);
        g.setColour (theme::dim);
        g.fillPath (arrow);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float lw = juce::jmax (2.0f, r * 0.2f), ar = r - lw * 0.5f;
        const bool en = s.isEnabled();

        juce::Path track;
        track.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, a1, true);
        g.setColour (theme::line);
        g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float ang = a0 + pos * (a1 - a0);
        juce::Path val;
        val.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, ang, true);
        g.setColour (en ? theme::accent : theme::dim.withAlpha (0.4f));
        g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float pr = ar - lw * 1.4f;
        g.setColour (en ? theme::text : theme::dim.withAlpha (0.5f));
        g.drawLine (c.x, c.y, c.x + pr * std::sin (ang), c.y - pr * std::cos (ang), 1.6f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (s.isBar())
        {
            auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
            g.setColour (theme::panelHi);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (s.isEnabled() ? theme::accentDim : theme::line);
            g.fillRoundedRectangle (r.withWidth (juce::jmax (0.0f, pos - r.getX())), 3.0f);
            return;
        }
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
    }
};
