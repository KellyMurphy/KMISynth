// The patch randomiser's pop-up: how far to move (amount), which groups may change, and a button that
// applies it (repeatable; each press is one undo step).
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "kmi/Randomise.h"

namespace kmiui {

class RandomPanel final : public juce::Component
{
public:
    explicit RandomPanel(std::function<void(const kmi::RandomiseOptions&)> apply);
    void paint(juce::Graphics&) override;
    void resized() override;

    // The settings are remembered while the plugin is loaded.
    static kmi::RandomiseOptions& options();

private:
    void store();

    std::function<void(const kmi::RandomiseOptions&)> apply_;
    juce::Label amountLabel_{ {}, "Amount" }, groupsLabel_{ {}, "Randomise" };
    juce::Slider amount_{ juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton algorithm_{ "Algorithm" }, oscillators_{ "Oscillators" }, envelopes_{ "Envelopes" },
        filters_{ "Filters" }, modulation_{ "LFOs" }, effects_{ "Effects" };
    juce::TextButton go_{ "Randomise" };
};

} // namespace kmiui
