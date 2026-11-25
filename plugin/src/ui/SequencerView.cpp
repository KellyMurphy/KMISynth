#include "SequencerView.h"

#include "../PluginProcessor.h"

namespace kmiui {

namespace {
const char* trackPrefix(int track)
{
    static const char* p[kmi::kSeqTracks] = { "seq_t1_", "seq_t2_", "seq_mac_" };
    return p[juce::jlimit(0, kmi::kSeqTracks - 1, track)];
}
} // namespace

SeqEditor::SeqEditor(UiContext& ctx, int track) : ctx_(ctx), track_(track), data_(ctx.proc.getSeqData())
{
    for (int i = 0; i < 4; ++i)
    {
        pageButtons_[i].setButtonText(juce::String(i * kStepsPerPage + 1) + "-" + juce::String((i + 1) * kStepsPerPage));
        pageButtons_[i].setClickingTogglesState(false);
        pageButtons_[i].onClick = [this, i] {
            page_ = i;
            repaint();
        };
        addAndMakeVisible(pageButtons_[i]);
    }
    if (!isMacroTrack())
    {
        octDown_.onClick = [this] { baseNote_ = juce::jmax(0, baseNote_ - 12); repaint(); };
        octUp_.onClick = [this] { baseNote_ = juce::jmin(127 - kRows + 1, baseNote_ + 12); repaint(); };
        addAndMakeVisible(octDown_);
        addAndMakeVisible(octUp_);
        record_.setTooltip("Real-time record: notes you play go into the step that is playing (start the sequence first)");
        stepRecord_.setTooltip("Step record: notes you play go into the selected step, then the cursor moves on (play a chord for a chord)");
        auto arm = [this](juce::ToggleButton& other) {
            if (other.getToggleState()) other.setToggleState(false, juce::dontSendNotification);
            ctx_.proc.setSequencerRecording(record_.getToggleState() || stepRecord_.getToggleState());
        };
        record_.onClick = [this, arm] { arm(stepRecord_); };
        stepRecord_.onClick = [this, arm] { arm(record_); };
        addAndMakeVisible(record_);
        addAndMakeVisible(stepRecord_);
    }
    else
    {
        for (int l = 0; l < kmi::kSeqLanes; ++l)
        {
            laneButtons_[l].setButtonText("Macro " + juce::String(l + 1));
            laneButtons_[l].onClick = [this, l] {
                lane_ = l;
                laneOn_.setToggleState(data_.laneOn[static_cast<size_t>(l)], juce::dontSendNotification);
                repaint();
            };
            addAndMakeVisible(laneButtons_[l]);
        }
        laneOn_.setToggleState(data_.laneOn[0], juce::dontSendNotification);
        laneOn_.setTooltip("While the sequence runs, this lane sets its macro's value step by step");
        laneOn_.onClick = [this] {
            data_.laneOn[static_cast<size_t>(lane_)] = laneOn_.getToggleState();
            commit();
        };
        addAndMakeVisible(laneOn_);
    }
    clear_.setTooltip(isMacroTrack() ? "Reset this lane to zero" : "Remove every note and step setting from this track");
    clear_.onClick = [this] { clearTrack(); };
    addAndMakeVisible(clear_);

    if (!isMacroTrack())
    {
        auto setupSlider = [this](juce::Slider& s, juce::Label& l, double lo, double hi, double step, const juce::String& suffix) {
            s.setRange(lo, hi, step);
            s.setTextValueSuffix(suffix);
            s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 18);
            l.setColour(juce::Label::textColourId, theme::textDim);
            addAndMakeVisible(s);
            addAndMakeVisible(l);
        };
        setupSlider(prob_, probLabel_, 0, 100, 1, " %");
        setupSlider(ratchet_, ratchetLabel_, 1, 4, 1, " x");
        setupSlider(drift_, driftLabel_, 0, 100, 1, " %");
        prob_.setTooltip("Chance that this step plays");
        ratchet_.setTooltip("Hits inside the step (1-4)");
        drift_.setTooltip("Random lateness (up to 30% of a step) and velocity spread");
        auto edit = [this] {
            auto& st = data_.steps[static_cast<size_t>(track_)][static_cast<size_t>(selected_)];
            st.probability = static_cast<uint8_t>(prob_.getValue());
            st.ratchet = static_cast<uint8_t>(ratchet_.getValue());
            st.drift = static_cast<uint8_t>(drift_.getValue());
            commit();
        };
        prob_.onValueChange = edit;
        ratchet_.onValueChange = edit;
        drift_.onValueChange = edit;
        stepLabel_.setColour(juce::Label::textColourId, theme::text);
        addAndMakeVisible(stepLabel_);
        updateInspector();
    }
    startTimerHz(20);
}

SeqEditor::~SeqEditor()
{
    if (record_.getToggleState() || stepRecord_.getToggleState()) ctx_.proc.setSequencerRecording(false);
}

void SeqEditor::resized()
{
    auto r = getLocalBounds();
    auto bar = r.removeFromBottom(isMacroTrack() ? 28 : 54).reduced(0, 3);
    auto top = bar.removeFromTop(24);
    for (auto& b : pageButtons_)
    {
        b.setBounds(top.removeFromLeft(70));
        top.removeFromLeft(4);
    }
    top.removeFromLeft(10);
    if (!isMacroTrack())
    {
        octDown_.setBounds(top.removeFromLeft(56));
        top.removeFromLeft(4);
        octUp_.setBounds(top.removeFromLeft(56));
        top.removeFromLeft(16);
        record_.setBounds(top.removeFromLeft(90));
        stepRecord_.setBounds(top.removeFromLeft(120));
        top.removeFromLeft(10);
        clear_.setBounds(top.removeFromLeft(100));
        top.removeFromLeft(16);
        stepLabel_.setBounds(top);
        auto ins = bar.reduced(0, 2);
        const int w = ins.getWidth() / 3;
        for (auto [label, slider] : { std::pair{ &probLabel_, &prob_ }, std::pair{ &ratchetLabel_, &ratchet_ }, std::pair{ &driftLabel_, &drift_ } })
        {
            auto cell = ins.removeFromLeft(w).reduced(4, 0);
            label->setBounds(cell.removeFromLeft(90));
            slider->setBounds(cell);
        }
    }
    else
    {
        for (auto& b : laneButtons_)
        {
            b.setBounds(top.removeFromLeft(72));
            top.removeFromLeft(3);
        }
        top.removeFromLeft(10);
        laneOn_.setBounds(top.removeFromLeft(180));
        clear_.setBounds(top.removeFromLeft(100));
    }
}

juce::Rectangle<int> SeqEditor::gridArea() const
{
    auto r = getLocalBounds();
    r.removeFromBottom(isMacroTrack() ? 28 : 54);
    r.removeFromLeft(kGutter);
    r.removeFromTop(kHeader);
    return r;
}

int SeqEditor::stepAt(int x) const
{
    const auto g = gridArea();
    if (x < g.getX() || x >= g.getRight()) return -1;
    const int col = (x - g.getX()) * kStepsPerPage / g.getWidth();
    return page_ * kStepsPerPage + juce::jlimit(0, kStepsPerPage - 1, col);
}

int SeqEditor::noteAt(int y) const
{
    const auto g = gridArea();
    const int row = juce::jlimit(0, kRows - 1, (y - g.getY()) * kRows / juce::jmax(1, g.getHeight()));
    return baseNote_ + (kRows - 1 - row);
}

int SeqEditor::findNote(const kmi::SeqStep& st, int pitch)
{
    for (int n = 0; n < st.count; ++n)
        if (st.notes[static_cast<size_t>(n)].note == pitch) return n;
    return -1;
}

void SeqEditor::commit()
{
    ctx_.proc.setSeqData(data_);
    repaint();
}

void SeqEditor::updateInspector()
{
    if (isMacroTrack()) return;
    const auto& st = data_.steps[static_cast<size_t>(track_)][static_cast<size_t>(selected_)];
    prob_.setValue(st.probability, juce::dontSendNotification);
    ratchet_.setValue(st.ratchet, juce::dontSendNotification);
    drift_.setValue(st.drift, juce::dontSendNotification);
    juce::String notes;
    for (int n = 0; n < st.count; ++n)
        notes << (n ? " " : "") << juce::MidiMessage::getMidiNoteName(st.notes[static_cast<size_t>(n)].note, true, true, 3);
    stepLabel_.setText("Step " + juce::String(selected_ + 1) + (notes.isEmpty() ? juce::String(": empty") : ": " + notes),
                       juce::dontSendNotification);
}

void SeqEditor::clearTrack()
{
    if (isMacroTrack()) data_.lanes[static_cast<size_t>(lane_)].fill(0.0f);
    else data_.steps[static_cast<size_t>(track_)].fill(kmi::SeqStep{});
    updateInspector();
    commit();
}

void SeqEditor::mouseDown(const juce::MouseEvent& e)
{
    const auto g = gridArea();
    const int step = stepAt(e.x);
    if (step < 0) return;
    if (e.y < g.getY()) // header: select the step
    {
        selected_ = step;
        updateInspector();
        repaint();
        return;
    }
    if (isMacroTrack())
    {
        mouseDrag(e);
        return;
    }
    const int pitch = noteAt(e.y);
    selected_ = step;
    auto& steps = data_.steps[static_cast<size_t>(track_)];
    // A click on an existing note (where it starts, or under its tail) removes it.
    for (int s = step; s >= 0 && s > step - 16; --s)
    {
        auto& st = steps[static_cast<size_t>(s)];
        const int n = findNote(st, pitch);
        if (n >= 0 && (s == step || st.notes[static_cast<size_t>(n)].length > (step - s) * 4))
        {
            for (int k = n; k + 1 < st.count; ++k) st.notes[static_cast<size_t>(k)] = st.notes[static_cast<size_t>(k + 1)];
            --st.count;
            dragStep_ = -1;
            updateInspector();
            commit();
            return;
        }
    }
    auto& st = steps[static_cast<size_t>(step)];
    if (e.mods.isPopupMenu() || st.count >= kmi::kSeqNotesPerStep) return;
    st.notes[static_cast<size_t>(st.count++)] = { static_cast<uint8_t>(pitch), 100, 4 };
    dragStep_ = step;
    dragNote_ = pitch;
    dragStart_ = e.x;
    updateInspector();
    commit();
}

void SeqEditor::mouseDrag(const juce::MouseEvent& e)
{
    const auto g = gridArea();
    if (isMacroTrack())
    {
        const int step = stepAt(e.x);
        if (step < 0 || e.y < g.getY() - 4) return;
        const float v = juce::jlimit(0.0f, 1.0f, 1.0f - static_cast<float>(e.y - g.getY()) / static_cast<float>(g.getHeight()));
        data_.lanes[static_cast<size_t>(lane_)][static_cast<size_t>(step)] = e.mods.isPopupMenu() ? 0.0f : v;
        commit();
        return;
    }
    if (dragStep_ < 0) return;
    // Dragging right after placing a note sets its length (in quarter steps).
    auto& st = data_.steps[static_cast<size_t>(track_)][static_cast<size_t>(dragStep_)];
    const int n = findNote(st, dragNote_);
    if (n < 0) return;
    const float cell = static_cast<float>(g.getWidth()) / kStepsPerPage;
    const int quarters = juce::jlimit(1, 64, 4 + juce::roundToInt(static_cast<float>(e.x - dragStart_) / cell * 4.0f));
    if (st.notes[static_cast<size_t>(n)].length != quarters)
    {
        st.notes[static_cast<size_t>(n)].length = static_cast<uint8_t>(quarters);
        commit();
    }
}

void SeqEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (isMacroTrack()) return;
    const int step = stepAt(e.x);
    if (step < 0) return;
    auto& st = data_.steps[static_cast<size_t>(track_)][static_cast<size_t>(step)];
    const int n = findNote(st, noteAt(e.y));
    if (n < 0) return;
    auto& v = st.notes[static_cast<size_t>(n)].velocity;
    v = static_cast<uint8_t>(juce::jlimit(1, 127, v + (w.deltaY > 0 ? 8 : -8)));
    commit();
}

void SeqEditor::handleRecording()
{
    KMISynthProcessor::RecordedNote notes[32];
    const int count = ctx_.proc.popRecordedNotes(notes, 32);
    if (count == 0 || isMacroTrack()) return;
    int target = selected_;
    if (record_.getToggleState())
    {
        target = ctx_.proc.sequencerPosition(track_);
        if (target < 0) return; // not running: nothing to record into
    }
    else if (!stepRecord_.getToggleState())
        return;
    auto& st = data_.steps[static_cast<size_t>(track_)][static_cast<size_t>(target)];
    for (int i = 0; i < count; ++i)
    {
        if (findNote(st, notes[i].note) >= 0 || st.count >= kmi::kSeqNotesPerStep) continue;
        st.notes[static_cast<size_t>(st.count++)] = { static_cast<uint8_t>(notes[i].note),
                                                      static_cast<uint8_t>(juce::jlimit(1, 127, juce::roundToInt(notes[i].velocity * 127.0f))), 4 };
    }
    if (stepRecord_.getToggleState())
    {
        const int len = juce::jlimit(1, kmi::kSeqSteps, static_cast<int>(getParam(ctx_, juce::String(trackPrefix(track_)) + "length")));
        selected_ = (selected_ + 1) % len;
        page_ = selected_ / kStepsPerPage;
    }
    updateInspector();
    commit();
}

void SeqEditor::timerCallback()
{
    handleRecording();
    // Follow outside changes (preset loads, undo) when not mid-gesture.
    if (!isMouseButtonDown() && !(ctx_.proc.getSeqData().steps == data_.steps && ctx_.proc.getSeqData().lanes == data_.lanes &&
                                  ctx_.proc.getSeqData().laneOn == data_.laneOn))
    {
        data_ = ctx_.proc.getSeqData();
        if (isMacroTrack()) laneOn_.setToggleState(data_.laneOn[static_cast<size_t>(lane_)], juce::dontSendNotification);
        updateInspector();
        repaint();
    }
    const int ph = ctx_.proc.sequencerPosition(track_);
    if (ph != lastPlayhead_)
    {
        lastPlayhead_ = ph;
        if (ph >= 0 && record_.getToggleState()) page_ = ph / kStepsPerPage;
        repaint();
    }
    for (int i = 0; i < 4; ++i) pageButtons_[i].setToggleState(i == page_, juce::dontSendNotification);
    for (int l = 0; l < kmi::kSeqLanes; ++l) laneButtons_[l].setToggleState(l == lane_, juce::dontSendNotification);
}

void SeqEditor::paint(juce::Graphics& g)
{
    const auto area = gridArea();
    const float cw = static_cast<float>(area.getWidth()) / kStepsPerPage;
    const int len = juce::jlimit(1, kmi::kSeqSteps, static_cast<int>(getParam(ctx_, juce::String(trackPrefix(track_)) + "length")));
    const int first = page_ * kStepsPerPage;
    const auto col = isMacroTrack() ? theme::osc(lane_) : theme::osc(track_ * 3);

    g.setColour(theme::background.brighter(0.03f));
    g.fillRect(area);

    // Header: step numbers, beat marks, the selected step and the playhead.
    const int ph = ctx_.proc.sequencerPosition(track_);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    for (int c = 0; c < kStepsPerPage; ++c)
    {
        const int step = first + c;
        const auto x = static_cast<float>(area.getX()) + c * cw;
        const juce::Rectangle<float> head(x, static_cast<float>(area.getY() - kHeader), cw, static_cast<float>(kHeader - 2));
        if (step == selected_ && !isMacroTrack())
        {
            g.setColour(theme::accent.withAlpha(0.35f));
            g.fillRect(head);
        }
        g.setColour(step >= len ? theme::textDim.withAlpha(0.35f) : (step % 4 == 0 ? theme::text : theme::textDim));
        if (step % 4 == 0 || cw > 30.0f) g.drawText(juce::String(step + 1), head, juce::Justification::centred);
        // Column backgrounds: beats, steps past the track length, the playhead.
        const juce::Rectangle<float> column(x, static_cast<float>(area.getY()), cw, static_cast<float>(area.getHeight()));
        if (step >= len)
        {
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillRect(column);
        }
        else if ((step / 4) % 2 == 1)
        {
            g.setColour(juce::Colours::white.withAlpha(0.025f));
            g.fillRect(column);
        }
        if (step == ph)
        {
            g.setColour(theme::accentWarm.withAlpha(0.25f));
            g.fillRect(column);
        }
        g.setColour(theme::panelEdge.withAlpha(step % 4 == 0 ? 0.9f : 0.4f));
        g.drawVerticalLine(juce::roundToInt(x), static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
    }

    if (isMacroTrack())
    {
        const auto& lane = data_.lanes[static_cast<size_t>(lane_)];
        const bool on = data_.laneOn[static_cast<size_t>(lane_)];
        for (int c = 0; c < kStepsPerPage; ++c)
        {
            const float v = lane[static_cast<size_t>(first + c)];
            const float h = v * static_cast<float>(area.getHeight());
            g.setColour(col.withAlpha(on ? 0.8f : 0.3f));
            g.fillRect(juce::Rectangle<float>(area.getX() + c * cw + 1.0f, area.getBottom() - h, cw - 2.0f, h));
        }
        g.setColour(theme::textDim);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText(on ? "Drag to draw Macro " + juce::String(lane_ + 1) + " values (right-drag: zero)"
                      : "Lane off: tick \"Lane drives its macro\" to use it",
                   getLocalBounds().removeFromTop(kHeader).withTrimmedLeft(kGutter + 4), juce::Justification::centredRight);
        return;
    }

    // Piano gutter and rows.
    const float rh = static_cast<float>(area.getHeight()) / kRows;
    for (int row = 0; row < kRows; ++row)
    {
        const int pitch = baseNote_ + (kRows - 1 - row);
        const bool black = juce::MidiMessage::isMidiNoteBlack(pitch);
        const juce::Rectangle<float> key(0.0f, area.getY() + row * rh, static_cast<float>(kGutter - 2), rh);
        g.setColour(black ? theme::panel.darker(0.4f) : theme::panel.brighter(0.1f));
        g.fillRect(key.reduced(0.0f, 0.5f));
        if (pitch % 12 == 0)
        {
            g.setColour(theme::textDim);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText(juce::MidiMessage::getMidiNoteName(pitch, true, true, 3), key.reduced(3.0f, 0.0f), juce::Justification::centredLeft);
        }
        if (black)
        {
            g.setColour(juce::Colours::black.withAlpha(0.15f));
            g.fillRect(juce::Rectangle<float>(static_cast<float>(area.getX()), key.getY(), static_cast<float>(area.getWidth()), rh));
        }
        g.setColour(theme::panelEdge.withAlpha(pitch % 12 == 0 ? 0.8f : 0.25f));
        g.drawHorizontalLine(juce::roundToInt(key.getBottom()), static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
    }

    // Notes (including those that start on earlier pages and continue onto this one).
    const auto& steps = data_.steps[static_cast<size_t>(track_)];
    g.saveState();
    g.reduceClipRegion(area);
    for (int s = juce::jmax(0, first - 16); s < first + kStepsPerPage; ++s)
    {
        const auto& st = steps[static_cast<size_t>(s)];
        for (int n = 0; n < st.count; ++n)
        {
            const auto& sn = st.notes[static_cast<size_t>(n)];
            const int row = kRows - 1 - (sn.note - baseNote_);
            if (row < 0 || row >= kRows) continue;
            const float x = area.getX() + (s - first) * cw;
            const juce::Rectangle<float> r(x + 1.0f, area.getY() + row * rh + 1.0f, cw * sn.length / 4.0f - 2.0f, rh - 2.0f);
            const float a = 0.35f + 0.65f * sn.velocity / 127.0f;
            g.setColour(col.withAlpha(a * (st.probability < 100 ? 0.7f : 1.0f)));
            g.fillRoundedRectangle(r, 2.0f);
            if (st.ratchet > 1 || st.drift > 0)
            {
                g.setColour(theme::accentWarm);
                g.fillEllipse(r.getX() + 2.0f, r.getCentreY() - 2.0f, 4.0f, 4.0f);
            }
        }
    }
    g.restoreState();

    // Notes outside the visible range: a mark at the top or bottom edge.
    for (int c = 0; c < kStepsPerPage; ++c)
    {
        const auto& st = steps[static_cast<size_t>(first + c)];
        bool above = false, below = false;
        for (int n = 0; n < st.count; ++n)
        {
            above = above || st.notes[static_cast<size_t>(n)].note > baseNote_ + kRows - 1;
            below = below || st.notes[static_cast<size_t>(n)].note < baseNote_;
        }
        g.setColour(theme::accentWarm);
        if (above) g.fillRect(juce::Rectangle<float>(area.getX() + c * cw + 2.0f, static_cast<float>(area.getY()), cw - 4.0f, 2.0f));
        if (below) g.fillRect(juce::Rectangle<float>(area.getX() + c * cw + 2.0f, area.getBottom() - 2.0f, cw - 4.0f, 2.0f));
    }
}

} // namespace kmiui
