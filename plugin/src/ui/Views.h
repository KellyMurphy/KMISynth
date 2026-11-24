// Custom visual components: algorithm diagram/matrix, envelope drawings, step-LFO editor, scope.
#pragma once

#include <functional>

#include "Controls.h"
#include "kmi/Algorithm.h"

namespace kmiui {

// Shows the algorithm as a carrier/modulator diagram and (when interactive) an editable 8x8 matrix.
// Editing a preset or morph algorithm converts the patch to a Custom algorithm.
class AlgorithmView final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    AlgorithmView(UiContext&, bool interactive);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    kmi::Algorithm effective() const;           // what the engine plays (incl. Direct Out)
    kmi::Algorithm editableCopy();              // custom algorithm, converting the patch if needed
    void paintDiagram(juce::Graphics&, juce::Rectangle<float>, const kmi::Algorithm&);
    void paintMatrix(juce::Graphics&, juce::Rectangle<float>, const kmi::Algorithm&);
    juce::Rectangle<float> matrixArea() const;
    juce::Rectangle<float> cellRect(int row, int col) const; // row 8 = output row

    UiContext& ctx_;
    bool interactive_;
};

// Draws one DAHDSR envelope from parameters (getter returns the current values).
class EnvelopeView final : public juce::Component, private juce::Timer
{
public:
    EnvelopeView(std::function<const kmi::EnvParams&()> get, juce::Colour colour);
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    std::function<const kmi::EnvParams&()> get_;
    juce::Colour colour_;
};

// All eight oscillator envelopes overlaid in their oscillator colours (group edit).
class EnvelopeOverlay final : public juce::Component, private juce::Timer
{
public:
    explicit EnvelopeOverlay(UiContext& ctx);
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    UiContext& ctx_;
};

// Draw / edit the 64 values of a Step LFO.
class StepEditor final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    StepEditor(UiContext&, int lfoIndex);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& e) override { edit(e.position, e.mods.isRightButtonDown()); }
    void mouseDrag(const juce::MouseEvent& e) override { edit(e.position, e.mods.isRightButtonDown()); }

private:
    void timerCallback() override { repaint(); }
    void edit(juce::Point<float> p, bool clearStep); // right button sets the step to zero
    int numSteps() const;

    UiContext& ctx_;
    int lfo_;
};

// Oscilloscope of the plugin output (zero-crossing triggered).
class Scope final : public juce::Component, private juce::Timer
{
public:
    explicit Scope(UiContext&);
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    UiContext& ctx_;
};

void drawEnvelope(juce::Graphics&, juce::Rectangle<float>, const kmi::EnvParams&, juce::Colour, bool fill);


// The oscillator's waveform as it plays: the selected wave, or with Wave Scan the live crossfade (start and
// end of the scan range drawn faintly). Load WAV fills a User slot (the one selected, else the first empty
// one) and selects it; Clear empties the selected User slot.
class WavePanel final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    WavePanel(UiContext&, int osc, juce::Colour colour);
    void paint(juce::Graphics&) override;
    void resized() override;

    // Peak-normalised single cycle of a wave (built-in or User slot; empty for an empty User slot).
    static std::vector<float> waveCycle(UiContext&, int wave);

private:
    void timerCallback() override;
    void loadWav();
    float scanPosition() const; // menu position incl. live modulation, or -1 if not scanning

    UiContext& ctx_;
    int osc_;
    juce::Colour colour_;
    juce::TextButton load_{ "Load WAV..." }, clear_{ "Clear" };
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String lastKey_;
};

} // namespace kmiui
