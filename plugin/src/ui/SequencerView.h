// Step-sequencer editor: a piano-roll grid for the two note tracks (32 steps per page, two octaves visible),
// a bar editor for the 8 macro lanes, a per-step inspector (probability, multi-trigger, drift), a playhead,
// and recording (real-time into the playing step, or step-by-step at the cursor).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Controls.h"
#include "kmi/Patch.h"

namespace kmiui {

class SeqEditor final : public juce::Component, private juce::Timer
{
public:
    SeqEditor(UiContext&, int track);
    ~SeqEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr int kStepsPerPage = 32, kRows = 24, kGutter = 44, kHeader = 18;

    void timerCallback() override;
    bool isMacroTrack() const noexcept { return track_ >= kmi::kSeqNoteTracks; }
    juce::Rectangle<int> gridArea() const;
    int stepAt(int x) const;          // absolute step under x (or -1)
    int noteAt(int y) const;          // pitch under y (note tracks)
    // The note on `step` covering `pitch` (index into its notes), or -1.
    static int findNote(const kmi::SeqStep& st, int pitch);
    void commit();                    // publish data_ to the processor
    void updateInspector();
    void handleRecording();
    void clearTrack();

    UiContext& ctx_;
    int track_;
    int page_ = 0;
    int baseNote_ = 48;               // lowest visible pitch (C2)
    int lane_ = 0;                    // macro track: lane being edited
    int selected_ = 0;                // selected step (inspector, step-record cursor)
    kmi::SeqData data_;               // working copy

    // Mouse gesture state
    int dragStep_ = -1, dragNote_ = -1, dragStart_ = -1;

    juce::TextButton pageButtons_[4], octDown_{ "Oct -" }, octUp_{ "Oct +" };
    juce::TextButton laneButtons_[kmi::kSeqLanes];
    juce::ToggleButton laneOn_{ "Lane drives its macro" };
    juce::ToggleButton record_{ "Record" }, stepRecord_{ "Step Record" };
    juce::TextButton clear_{ "Clear Track" };
    juce::Label probLabel_{ {}, "Probability" }, ratchetLabel_{ {}, "Multi-Trigger" }, driftLabel_{ {}, "Drift" };
    juce::Slider prob_{ juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider ratchet_{ juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider drift_{ juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label stepLabel_;
    int lastPlayhead_ = -2;
};

} // namespace kmiui
