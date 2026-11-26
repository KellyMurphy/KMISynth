#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "Parameters.h"
#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/LookAndFeel.h"
#include "ui/PresetBrowser.h"
#include "ui/RandomPanel.h"

// KMISynth editor: a 1280x800 design surface scaled to the window (75% - 200%, R-UI-1).
class KMISynthEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KMISynthEditor(KMISynthProcessor&);
    ~KMISynthEditor() override;

    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

    static constexpr int kDesignW = 1280, kDesignH = 800;

private:
    struct Surface final : juce::Component
    {
        void paint(juce::Graphics& g) override;
    };

    void timerCallback() override;
    void showPage(int index);
    void layoutSurface();
    void loadPreset();
    void savePreset();
    void toggleBrowser();
    void showRandomPanel();
    void randomise(const kmi::RandomiseOptions&);
    void stepFactoryPreset(int delta);
    void afterPresetChange();
    void verifyFactoryPresets(juce::File dir);
    void verifyMidiLearn(juce::File dir);
    void verifyPresetSearch(juce::File dir);
    void verifyUserWaves(juce::File dir);
    void verifyRandomise(juce::File dir);
    void verifySequencer(juce::File dir);
    void verifyTuning(juce::File dir);
    void verifyGraphHandles(juce::File dir);
    void verifyCommunityPatches(juce::File dir);
    void snapshotPages(juce::File dir, int index);

    KMISynthProcessor& proc_;
    kmiui::KmiLookAndFeel lnf_;

    // A copy of every parameter value for the views (the UI never touches the engine's patch).
    kmi::Patch uiPatch_;
    kmiplug::ParameterBinding uiBinding_;
    kmiui::UiContext ctx_;

    Surface surface_;
    juce::Label title_, status_;
    juce::TextButton presetButton_, prevButton_{ "<" }, nextButton_{ ">" };
    juce::TextButton randomButton_{ "Random" };
    juce::TextButton initButton_{ "Init" }, loadButton_{ "Load..." }, saveButton_{ "Save..." };
    juce::TextButton undoButton_{ "Undo" }, redoButton_{ "Redo" };
    std::unique_ptr<kmiui::Selector> tabs_;
    std::unique_ptr<juce::Component> page_;
    int pageIndex_ = 0;
    juce::MidiKeyboardComponent keyboard_;
    juce::TooltipWindow tooltips_{ this, 700 };
    std::unique_ptr<juce::FileChooser> chooser_;
    std::unique_ptr<kmiui::PresetBrowser> browser_;
    int transactionTicks_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KMISynthEditor)
};
