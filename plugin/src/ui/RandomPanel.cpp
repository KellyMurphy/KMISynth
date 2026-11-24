#include "RandomPanel.h"

#include "Theme.h"

namespace kmiui {

kmi::RandomiseOptions& RandomPanel::options()
{
    static kmi::RandomiseOptions o;
    return o;
}

RandomPanel::RandomPanel(std::function<void(const kmi::RandomiseOptions&)> apply) : apply_(std::move(apply))
{
    const auto& o = options();
    for (auto* l : { &amountLabel_, &groupsLabel_ })
    {
        l->setColour(juce::Label::textColourId, theme::textDim);
        addAndMakeVisible(l);
    }
    amount_.setRange(0.0, 100.0, 1.0);
    amount_.setValue(o.amount * 100.0, juce::dontSendNotification);
    amount_.setTextValueSuffix(" %");
    amount_.setTooltip("How far to move from the current sound: low = variations, 100 % = a new patch");
    amount_.onValueChange = [this] { store(); };
    addAndMakeVisible(amount_);

    const std::pair<juce::ToggleButton*, bool> toggles[] = { { &algorithm_, o.algorithm }, { &oscillators_, o.oscillators },
                                                             { &envelopes_, o.envelopes }, { &filters_, o.filters },
                                                             { &modulation_, o.modulation }, { &effects_, o.effects } };
    for (const auto& [t, on] : toggles)
    {
        t->setToggleState(on, juce::dontSendNotification);
        t->onClick = [this] { store(); };
        addAndMakeVisible(t);
    }
    algorithm_.setTooltip("Pick a new algorithm");
    oscillators_.setTooltip("Waves, modes, ratios, levels, feedback, modulator envelopes and wave scan");
    envelopes_.setTooltip("Env 1-3: filter and VCA envelopes");
    filters_.setTooltip("Cutoff, resonance, envelope amount and drive of both filters");
    modulation_.setTooltip("LFO rates and shapes");
    effects_.setTooltip("Insert effects, delay and reverb");

    go_.setTooltip("Apply (each press is one Undo step)");
    go_.onClick = [this] {
        if (apply_) apply_(options());
    };
    addAndMakeVisible(go_);
    setSize(280, 214);
}

void RandomPanel::store()
{
    auto& o = options();
    o.amount = static_cast<float>(amount_.getValue() / 100.0);
    o.algorithm = algorithm_.getToggleState();
    o.oscillators = oscillators_.getToggleState();
    o.envelopes = envelopes_.getToggleState();
    o.filters = filters_.getToggleState();
    o.modulation = modulation_.getToggleState();
    o.effects = effects_.getToggleState();
}

void RandomPanel::paint(juce::Graphics& g) { g.fillAll(theme::panel); }

void RandomPanel::resized()
{
    auto r = getLocalBounds().reduced(10);
    amountLabel_.setBounds(r.removeFromTop(18));
    amount_.setBounds(r.removeFromTop(26));
    r.removeFromTop(6);
    groupsLabel_.setBounds(r.removeFromTop(18));
    juce::ToggleButton* grid[] = { &algorithm_, &oscillators_, &envelopes_, &filters_, &modulation_, &effects_ };
    for (int row = 0; row < 3; ++row)
    {
        auto line = r.removeFromTop(24);
        grid[row * 2]->setBounds(line.removeFromLeft(line.getWidth() / 2));
        grid[row * 2 + 1]->setBounds(line);
    }
    r.removeFromTop(8);
    go_.setBounds(r.removeFromTop(30));
}

} // namespace kmiui
