#include "LookAndFeel.h"

#include "Theme.h"

namespace kmiui {

KmiLookAndFeel::KmiLookAndFeel()
{
    setColourScheme({ theme::background, theme::panel, theme::control, theme::panelEdge, theme::text,
                      theme::accent, juce::Colours::black, theme::accent.darker(0.6f), theme::text });

    setColour(juce::ResizableWindow::backgroundColourId, theme::background);
    setColour(juce::Slider::rotarySliderFillColourId, theme::accent);
    setColour(juce::Slider::rotarySliderOutlineColourId, theme::track);
    setColour(juce::Slider::thumbColourId, theme::accent);
    setColour(juce::Slider::trackColourId, theme::accent.withAlpha(0.7f));
    setColour(juce::Slider::backgroundColourId, theme::track);
    setColour(juce::Slider::textBoxTextColourId, theme::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, theme::text);
    setColour(juce::ComboBox::backgroundColourId, theme::control);
    setColour(juce::ComboBox::outlineColourId, theme::panelEdge);
    setColour(juce::ComboBox::textColourId, theme::text);
    setColour(juce::ComboBox::arrowColourId, theme::textDim);
    setColour(juce::PopupMenu::backgroundColourId, theme::panel);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, theme::accent.darker(0.5f));
    setColour(juce::PopupMenu::textColourId, theme::text);
    setColour(juce::TextButton::buttonColourId, theme::control);
    setColour(juce::TextButton::buttonOnColourId, theme::accent.darker(0.4f));
    setColour(juce::TextButton::textColourOffId, theme::text);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::ToggleButton::textColourId, theme::text);
    setColour(juce::ToggleButton::tickColourId, theme::accent);
    setColour(juce::ToggleButton::tickDisabledColourId, theme::textDim);
    setColour(juce::TooltipWindow::backgroundColourId, theme::panel.brighter(0.1f));
    setColour(juce::TooltipWindow::textColourId, theme::text);
    setColour(juce::TooltipWindow::outlineColourId, theme::panelEdge);
    setColour(juce::ScrollBar::thumbColourId, theme::track);
}

void KmiLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float pos,
                                      float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float lineW = juce::jmax(2.5f, radius * 0.14f);
    const float arcR = radius - lineW * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto fill = slider.findColour(juce::Slider::rotarySliderFillColourId);

    juce::Path trackArc;
    trackArc.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
    g.strokePath(trackArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar parameters fill from the centre (zero), unipolar ones from the start.
    float fromAngle = startAngle;
    if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        fromAngle = startAngle + static_cast<float>(slider.valueToProportionOfLength(0.0)) * (endAngle - startAngle);
    if (std::abs(angle - fromAngle) > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin(fromAngle, angle),
                               juce::jmax(fromAngle, angle), true);
        g.setColour(slider.isEnabled() ? fill : fill.withAlpha(0.3f));
        g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Modulation ring (set by ParamKnob): the reachable range as a thin outer arc, plus a dot at the
    // live modulated position of the newest voice.
    const auto& props = slider.getProperties();
    if (props.contains("kmiModLo"))
    {
        const float lo = static_cast<float>(props["kmiModLo"]), hi = static_cast<float>(props["kmiModHi"]);
        const float ringR = arcR + lineW * 0.5f + 1.5f;
        const float a0 = startAngle + lo * (endAngle - startAngle), a1 = startAngle + hi * (endAngle - startAngle);
        juce::Path ring;
        ring.addCentredArc(centre.x, centre.y, ringR, ringR, 0.0f, juce::jmin(a0, a1), juce::jmax(a0, a1) + 0.001f, true);
        g.setColour(theme::accentWarm.withAlpha(0.85f));
        g.strokePath(ring, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float live = static_cast<float>(props.getWithDefault("kmiModLive", -1.0f));
        if (live >= 0.0f)
        {
            const float al = startAngle + live * (endAngle - startAngle);
            const juce::Point<float> p(centre.x + ringR * std::sin(al), centre.y - ringR * std::cos(al));
            g.setColour(theme::text);
            g.fillEllipse(p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);
        }
    }

    const float knobR = arcR - lineW * 1.2f;
    g.setColour(theme::control.brighter(slider.isMouseOverOrDragging() ? 0.15f : 0.05f));
    g.fillEllipse(centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    juce::Path pointer;
    pointer.addRoundedRectangle(-1.25f, -knobR, 2.5f, knobR * 0.6f, 1.0f);
    g.setColour(slider.isEnabled() ? theme::text : theme::textDim.withAlpha(0.5f));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void KmiLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                      float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
        return;
    }
    const auto r = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float alpha = slider.isEnabled() ? 1.0f : 0.35f;
    g.setColour(theme::control);
    g.fillRoundedRectangle(r, 3.0f);
    float zero = r.getX();
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    if (bipolar) zero = r.getX() + r.getWidth() * static_cast<float>(slider.valueToProportionOfLength(0.0));
    const float a = juce::jmin(zero, sliderPos), b = juce::jmax(zero, sliderPos);
    g.setColour(slider.findColour(juce::Slider::trackColourId).withMultipliedAlpha(alpha));
    g.fillRect(juce::Rectangle<float>(a, r.getY() + 1.0f, b - a, r.getHeight() - 2.0f));
    if (bipolar)
    {
        g.setColour(theme::textDim.withAlpha(0.6f));
        g.drawVerticalLine(juce::roundToInt(zero), r.getY() + 2.0f, r.getBottom() - 2.0f);
    }
    g.setColour(theme::panelEdge);
    g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
}

void KmiLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& background,
                                          bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    // Segments of a Selector: only the selected one (and the one under the mouse) is drawn, on the selector's track.
    if (b.getProperties().contains("kmiSegment"))
    {
        if (b.getToggleState())
        {
            const auto on = b.findColour(juce::TextButton::buttonOnColourId);
            g.setColour(down ? on.brighter(0.15f) : on);
            g.fillRoundedRectangle(r.reduced(1.0f), 6.0f);
        }
        else if (highlighted || down)
        {
            g.setColour(theme::control.brighter(down ? 0.12f : 0.04f));
            g.fillRoundedRectangle(r.reduced(1.0f), 6.0f);
        }
        return;
    }
    auto c = b.getToggleState() ? b.findColour(juce::TextButton::buttonOnColourId) : background;
    if (down) c = c.brighter(0.2f);
    else if (highlighted) c = c.brighter(0.08f);
    g.setColour(c);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(theme::panelEdge);
    g.drawRoundedRectangle(r, 4.0f, 1.0f);
}

} // namespace kmiui
