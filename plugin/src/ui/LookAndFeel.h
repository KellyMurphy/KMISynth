#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace kmiui {

class KmiLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    KmiLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float startAngle,
                          float endAngle, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minPos,
                          float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& background, bool highlighted,
                              bool down) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(12.5f)); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::Font(juce::FontOptions(12.5f)); }
    juce::Font getPopupMenuFont() override { return juce::Font(juce::FontOptions(13.0f)); }
};

} // namespace kmiui
