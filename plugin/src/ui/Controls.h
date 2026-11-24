// Parameter-bound controls, a titled grid section, and the page base class.
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "kmi/Patch.h"

class KMISynthProcessor;

namespace kmiui {

struct UiContext
{
    KMISynthProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    const kmi::Patch& patch; // snapshot of all parameter values, refreshed by the editor at 30 Hz
};

// ---- Parameter helpers -----------------------------------------------------------------------------
juce::RangedAudioParameter* param(UiContext&, const juce::String& id);
float getParam(UiContext&, const juce::String& id);              // plain (denormalised) value
void setParam(UiContext&, const juce::String& id, float plain);  // with host gesture
int destForParam(const juce::String& id);                        // mod destination, or -1
void addModRoute(UiContext&, int source, int dest);              // first free mod-matrix slot

// ---- Controls ----------------------------------------------------------------------------------------
class ParamKnob final : public juce::Component
{
public:
    ParamKnob(UiContext&, const juce::String& id, const juce::String& label = {}, juce::Colour colour = theme::accent);
    ~ParamKnob() override;
    void resized() override;

private:
    class KnobSlider;
    UiContext& ctx_;
    juce::String id_;
    juce::Label label_;
    std::unique_ptr<KnobSlider> slider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
};

class ParamChoice final : public juce::Component
{
public:
    ParamChoice(UiContext&, const juce::String& id, const juce::String& label = {});
    void resized() override;

private:
    juce::Label label_;
    juce::ComboBox combo_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment_;
};

class ParamToggle final : public juce::Component
{
public:
    ParamToggle(UiContext&, const juce::String& id, const juce::String& label = {});
    void resized() override;

private:
    juce::ToggleButton button_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment_;
};

// Picks knob / choice / toggle from the parameter type.
std::unique_ptr<juce::Component> makeControl(UiContext&, const juce::String& id, const juce::String& label = {},
                                             juce::Colour colour = theme::accent);

// Horizontal slider bound to a parameter (mod matrix / macro rows).
class ParamBar final : public juce::Component
{
public:
    ParamBar(UiContext&, const juce::String& id);
    void resized() override { slider_.setBounds(getLocalBounds()); }

private:
    juce::Slider slider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
};

// Combo bound to a parameter, without a label.
class ParamCombo final : public juce::Component
{
public:
    ParamCombo(UiContext&, const juce::String& id);
    void resized() override { combo_.setBounds(getLocalBounds()); }

private:
    juce::ComboBox combo_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment_;
};

// ---- Layout -------------------------------------------------------------------------------------------
// A titled panel laying out controls on a grid of fixed-size cells; items may span cells.
class Section final : public juce::Component
{
public:
    static constexpr int kCellW = 76, kCellH = 80, kTitleH = 22, kPad = 6;

    Section(juce::String title, int columns, juce::Colour accent = theme::accent);
    juce::Component& add(std::unique_ptr<juce::Component> c, int colSpan = 1, int rowSpan = 1);
    void addParams(UiContext&, std::initializer_list<juce::String> ids);
    void addParam(UiContext&, const juce::String& id, const juce::String& label);
    void skip(int cells = 1);

    int preferredWidth() const { return columns_ * kCellW + 2 * kPad; }
    int preferredHeight() const { return kTitleH + rows_ * kCellH + kPad; }

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Item
    {
        std::unique_ptr<juce::Component> comp;
        int col, row, colSpan, rowSpan;
    };
    bool fits(int col, int row, int cs, int rs) const;
    void occupy(int col, int row, int cs, int rs);

    juce::String title_;
    int columns_;
    juce::Colour accent_;
    std::vector<Item> items_;
    std::vector<std::vector<bool>> used_;
    int rows_ = 0;
};

// A row of mutually-exclusive buttons (oscillator / envelope / LFO pickers, page tabs).
class Selector final : public juce::Component
{
public:
    Selector(const juce::StringArray& labels, int selected, std::function<void(int)> onSelect,
             std::function<juce::Colour(int)> colourFor = {});
    void setSelected(int index);
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons_;
};

// Base page: flows its sections left-to-right, wrapping rows. Rebuilds (asynchronously) whenever
// layoutKey() changes, e.g. a different oscillator is selected or a filter type is changed.
class Page : public juce::Component, private juce::Timer, private juce::AsyncUpdater
{
public:
    explicit Page(UiContext& c) : ctx(c) {}
    ~Page() override;
    void start();
    void resized() override;

protected:
    virtual void build() = 0;
    virtual juce::String layoutKey() { return {}; }
    void requestRebuild() { triggerAsyncUpdate(); }
    Section& addSection(std::unique_ptr<Section> s);
    void addHeader(std::unique_ptr<juce::Component> c, int height);

    UiContext& ctx;

private:
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void rebuild();

    std::vector<std::pair<std::unique_ptr<juce::Component>, int>> headers_;
    std::vector<std::unique_ptr<Section>> sections_;
    juce::String key_;
};

} // namespace kmiui
