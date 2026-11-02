#pragma once
#include "Theme.h"

/** Small rotary knob with a caption above and a value read-out below. */
class LabeledKnob : public juce::Component
{
public:
    explicit LabeledKnob (const juce::String& caption)
    {
        name.setText (caption, juce::dontSendNotification);
        name.setJustificationType (juce::Justification::centred);
        name.setFont (juce::Font (juce::FontOptions (11.0f)));
        name.setColour (juce::Label::textColourId, theme::dim);
        name.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (name);

        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 13);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (slider);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        name.setBounds (r.removeFromTop (12));
        slider.setBounds (r);
    }

    void setEnabledLook (bool e)
    {
        slider.setEnabled (e);
        name.setAlpha (e ? 1.0f : 0.4f);
        slider.setAlpha (e ? 1.0f : 0.5f);
    }

    juce::Label name;
    juce::Slider slider;
};

inline void styleBar (juce::Slider& s, double lo, double hi, double step = 1.0)
{
    s.setSliderStyle (juce::Slider::LinearBar);
    s.setTextBoxStyle (juce::Slider::TextBoxLeft, true, 60, 18);
    s.setRange (lo, hi, step);
    s.setMouseDragSensitivity (150);
}

inline void styleNoteBar (juce::Slider& s)
{
    styleBar (s, 0, 127, 1);
    s.textFromValueFunction = [] (double v) { return theme::noteName ((int) v); };
    s.valueFromTextFunction = [] (const juce::String& t) { const int n = theme::parseNoteName (t); return n < 0 ? 60.0 : (double) n; };
}

inline void styleSection (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::Font (juce::FontOptions (11.0f)));
    l.setColour (juce::Label::textColourId, theme::dim);
}

inline void paintPanel (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r.toFloat(), 6.0f);
}
