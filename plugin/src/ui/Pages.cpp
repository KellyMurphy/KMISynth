#include "Pages.h"

#include "../PluginProcessor.h"
#include "Views.h"
#include "kmi/Modulation.h"

namespace kmiui {

namespace {
juce::String n1(int i) { return juce::String(i + 1); }

juce::StringArray oscLabels()
{
    juce::StringArray a;
    for (int i = 0; i < kmi::kNumOscs; ++i) a.add("Osc " + n1(i));
    return a;
}

// ---- Home -------------------------------------------------------------------------------------------------
class HomePage final : public Page
{
public:
    using Page::Page;

protected:
    void build() override
    {
        auto& macros = addSection(std::make_unique<Section>("Macros", 8, theme::accentWarm));
        for (int m = 0; m < kmi::kNumMacros; ++m)
            macros.add(std::make_unique<ParamKnob>(ctx, "macro" + n1(m) + "_value", "Macro " + n1(m), theme::osc(m)));

        auto& algo = addSection(std::make_unique<Section>("Algorithm", 6));
        algo.add(std::make_unique<AlgorithmView>(ctx, false), 4, 3);
        algo.add(std::make_unique<ParamChoice>(ctx, "algo_mode", "Mode"), 2);
        algo.add(std::make_unique<ParamChoice>(ctx, "algo_index", "Algorithm"), 2);
        algo.addParam(ctx, "algo_morph", "Morph");

        auto& out = addSection(std::make_unique<Section>("Output", 6));
        out.add(std::make_unique<Scope>(ctx), 6, 2);
        out.addParam(ctx, "vca_patchlevel", "Patch Level");
        out.add(std::make_unique<ParamChoice>(ctx, "voice_polymode", "Polyphony"), 2);
        out.addParam(ctx, "voice_glide", "Glide");
        out.addParam(ctx, "voice_glidetime", "Glide Time");
        out.addParam(ctx, "voice_oversampling", "Quality");
        out.addParam(ctx, "arp_on", "Arp");

        auto& filt = addSection(std::make_unique<Section>("Filters", 6));
        filt.add(std::make_unique<ParamChoice>(ctx, "dfilt_type", "Digital Type"), 2);
        filt.addParam(ctx, "dfilt_cutoff", "D.Cutoff");
        filt.addParam(ctx, "dfilt_res", "D.Reso");
        filt.addParam(ctx, "afilt_cutoff", "A.Cutoff");
        filt.addParam(ctx, "afilt_res", "A.Reso");
        filt.addParam(ctx, "dfilt_envamt", "Env 1 Amt");
        filt.addParam(ctx, "afilt_envamt", "Env 2 Amt");
        filt.addParam(ctx, "afilt_predrive", "Pre-Drive");
        filt.addParam(ctx, "dfilt_drive", "D.Drive");

        auto& fx = addSection(std::make_unique<Section>("Effects", 4));
        fx.addParam(ctx, "delay_on", "Delay");
        fx.addParam(ctx, "delay_mix", "Delay Mix");
        fx.addParam(ctx, "reverb_on", "Reverb");
        fx.addParam(ctx, "reverb_mix", "Reverb Mix");
    }
};

// ---- Algorithm -------------------------------------------------------------------------------------------
class AlgorithmPage final : public Page
{
public:
    using Page::Page;

protected:
    void build() override
    {
        auto& view = addSection(std::make_unique<Section>("Structure", 10));
        view.add(std::make_unique<AlgorithmView>(ctx, true), 10, 4);

        auto& sel = addSection(std::make_unique<Section>("Algorithm", 6));
        sel.add(std::make_unique<ParamChoice>(ctx, "algo_mode", "Mode"), 2);
        sel.add(std::make_unique<ParamChoice>(ctx, "algo_index", "Preset Algorithm"), 4);
        sel.addParam(ctx, "algo_morph", "Morph");
        for (int s = 0; s < kmi::kNumMorphSlots; ++s)
            sel.add(std::make_unique<ParamChoice>(ctx, "algo_slot" + n1(s), "Morph Slot " + n1(s)), 2);

        auto& oscs = addSection(std::make_unique<Section>("Oscillator Modes and Levels", 8));
        for (int i = 0; i < kmi::kNumOscs; ++i)
            oscs.add(std::make_unique<ParamChoice>(ctx, "osc" + n1(i) + "_mode", "Osc " + n1(i)));
        for (int i = 0; i < kmi::kNumOscs; ++i)
            oscs.add(std::make_unique<ParamKnob>(ctx, "osc" + n1(i) + "_level", "Level " + n1(i), theme::osc(i)));
        for (int i = 0; i < kmi::kNumOscs; ++i)
            oscs.add(std::make_unique<ParamKnob>(ctx, "osc" + n1(i) + "_feedback", "Feedback " + n1(i), theme::osc(i)));
    }
};

// ---- Oscillators (single + group edit) --------------------------------------------------------------------
struct GroupParam
{
    const char* label;
    const char* suffix;
};
const GroupParam kGroupParams[] = {
    { "Initial Level", "level" }, { "Env Level", "envlevel" }, { "Mode", "mode" }, { "Wave", "wave" },
    { "Pitch Mode", "pitchmode" }, { "Semitone", "semi" }, { "Cents", "cents" }, { "Ratio", "ratio" },
    { "Ratio Fine", "ratiofine" }, { "Frequency", "freq" }, { "Feedback", "feedback" }, { "Keytrack", "keytrack" },
    { "Phase", "phase" }, { "Velocity", "velocity" }, { "Poly AT", "polyat" }, { "Mute", "mute" },
    { "Pan (Osc Stereo)", "pan" }, { "Keyscale Break", "ksbreak" }, { "Keyscale Low Depth", "kslodepth" },
    { "Keyscale High Depth", "kshidepth" },
    { "Env Delay", "env_delay" }, { "Env Attack", "env_attack" }, { "Env Hold", "env_hold" },
    { "Env Decay", "env_decay" }, { "Env Sustain", "env_sustain" }, { "Env Release", "env_release" },
    { "Attack Curve", "env_atkcurve" }, { "Decay Curve", "env_deccurve" }, { "Release Curve", "env_relcurve" },
};

void addEnvParams(UiContext& ctx, Section& s, const juce::String& p, bool withLevel)
{
    s.addParam(ctx, p + "delay", "Delay");
    s.addParam(ctx, p + "attack", "Attack");
    s.addParam(ctx, p + "hold", "Hold");
    s.addParam(ctx, p + "decay", "Decay");
    s.addParam(ctx, p + "sustain", "Sustain");
    s.addParam(ctx, p + "release", "Release");
    if (withLevel) s.addParam(ctx, p + "level", "Level");
    s.addParam(ctx, p + "atkcurve", "Atk Curve");
    s.addParam(ctx, p + "deccurve", "Dec Curve");
    s.addParam(ctx, p + "relcurve", "Rel Curve");
    s.addParam(ctx, p + "velamt", "Vel > Env");
    s.addParam(ctx, p + "velcurve", "Vel Curve");
    s.addParam(ctx, p + "slow", "Slow");
    s.addParam(ctx, p + "bpm", "BPM");
    s.addParam(ctx, p + "legato", "Legato");
    s.addParam(ctx, p + "reset", "Reset");
    s.addParam(ctx, p + "freerun", "Freerun");
    s.addParam(ctx, p + "loop", "Loop");
    s.add(std::make_unique<ParamChoice>(ctx, p + "stageloop", "Loop Stages"), 2);
    for (int k = 0; k < kmi::kNumEnvTrigs; ++k)
        s.add(std::make_unique<ParamChoice>(ctx, p + "trig" + n1(k), "Trigger " + n1(k)), 2);
}

class OscPage final : public Page
{
public:
    using Page::Page;

protected:
    juce::String layoutKey() override
    {
        const auto pm = static_cast<int>(ctx.patch.osc[static_cast<size_t>(sel_)].pitchMode);
        return juce::String(sel_) + "/" + juce::String(group_ ? 1 : 0) + "/" + juce::String(groupParam_) + "/" + juce::String(pm) +
               "/" + juce::String(ctx.patch.voice.oscStereo ? 1 : 0);
    }

    void build() override
    {
        auto labels = oscLabels();
        labels.add("Group Edit");
        addHeader(std::make_unique<Selector>(labels, group_ ? kmi::kNumOscs : sel_,
                                             [this](int i) {
                                                 group_ = i == kmi::kNumOscs;
                                                 if (!group_) sel_ = i;
                                                 requestRebuild();
                                             },
                                             [](int i) { return i < kmi::kNumOscs ? theme::osc(i) : theme::accentWarm; }),
                  28);
        if (group_) buildGroup();
        else buildSingle();
    }

private:
    void buildSingle()
    {
        const int i = sel_;
        const juce::String p = "osc" + n1(i) + "_";
        const auto col = theme::osc(i);
        auto& s = addSection(std::make_unique<Section>("Oscillator " + n1(i), 8, col));
        s.add(std::make_unique<ParamChoice>(ctx, p + "mode", "Mode (as modulator)"), 2);
        s.add(std::make_unique<ParamChoice>(ctx, p + "wave", "Waveform"), 2);
        s.addParam(ctx, p + "invert", "Invert");
        s.addParam(ctx, p + "pitchmode", "Pitch Mode");
        switch (ctx.patch.osc[static_cast<size_t>(i)].pitchMode)
        {
            case kmi::PitchMode::Ratio:
                s.addParam(ctx, p + "ratio", "Ratio");
                s.addParam(ctx, p + "ratiofine", "Fine %");
                break;
            case kmi::PitchMode::Frequency:
                s.addParam(ctx, p + "freq", "Frequency");
                s.addParam(ctx, p + "freqfine", "Fine Hz");
                break;
            default:
                s.addParam(ctx, p + "semi", "Semitone");
                s.addParam(ctx, p + "cents", "Cents");
                break;
        }
        s.addParam(ctx, p + "level", "Initial Level");
        s.addParam(ctx, p + "envlevel", "Env Level");
        s.addParam(ctx, p + "feedback", "Feedback");
        s.addParam(ctx, p + "keytrack", "Keytrack");
        s.addParam(ctx, p + "phase", "Phase");
        s.addParam(ctx, p + "velocity", "Velocity");
        s.addParam(ctx, p + "polyat", "Poly AT");
        s.addParam(ctx, p + "envsource", "Env Source");
        if (i > 0) s.addParam(ctx, p + "directout", "Direct Out");
        s.addParam(ctx, p + "mute", "Mute");
        s.addParam(ctx, p + "ksbreak", "KS Break");
        s.addParam(ctx, p + "kslodepth", "KS Lo Depth");
        s.addParam(ctx, p + "kslocurve", "KS Lo Curve");
        s.addParam(ctx, p + "kshidepth", "KS Hi Depth");
        s.addParam(ctx, p + "kshicurve", "KS Hi Curve");
        if (ctx.patch.voice.oscStereo) s.addParam(ctx, p + "pan", "Pan");

        auto& e = addSection(std::make_unique<Section>("Oscillator " + n1(i) + " Envelope", 8, col));
        e.add(std::make_unique<EnvelopeView>([this, i]() -> const kmi::EnvParams& { return ctx.patch.osc[static_cast<size_t>(i)].env; }, col), 8, 2);
        addEnvParams(ctx, e, p + "env_", false);
    }

    void buildGroup()
    {
        const auto& gp = kGroupParams[groupParam_];
        auto& s = addSection(std::make_unique<Section>("Group Edit: one parameter, all oscillators", 10, theme::accentWarm));
        auto combo = std::make_unique<juce::ComboBox>();
        for (int k = 0; k < static_cast<int>(std::size(kGroupParams)); ++k) combo->addItem(kGroupParams[k].label, k + 1);
        combo->setSelectedId(groupParam_ + 1, juce::dontSendNotification);
        auto* cb = combo.get();
        cb->onChange = [this, cb] {
            groupParam_ = cb->getSelectedId() - 1;
            requestRebuild();
        };
        s.add(std::move(combo), 2);
        for (int i = 0; i < kmi::kNumOscs; ++i)
            s.add(makeControl(ctx, "osc" + n1(i) + "_" + gp.suffix, "Osc " + n1(i), theme::osc(i)));

        auto& env = addSection(std::make_unique<Section>("All Oscillator Envelopes (oscillators with Env Level)", 10, theme::accentWarm));
        env.add(std::make_unique<EnvelopeOverlay>(ctx), 10, 3);
    }

    int sel_ = 0;
    bool group_ = false;
    int groupParam_ = 0;
};

// ---- Filters and VCA ----------------------------------------------------------------------------------------
class FilterPage final : public Page
{
public:
    using Page::Page;

protected:
    juce::String layoutKey() override { return juce::String(static_cast<int>(ctx.patch.dfilt.type)); }

    void build() override
    {
        using T = kmi::DigitalFilterType;
        const auto type = ctx.patch.dfilt.type;
        auto& d = addSection(std::make_unique<Section>("Digital Filter (Env 1, LFO 1)", 6));
        d.add(std::make_unique<ParamChoice>(ctx, "dfilt_type", "Type"), 2);
        d.addParam(ctx, "dfilt_cutoff", type == T::Vowel ? "Vowel" : "Cutoff");
        d.addParam(ctx, "dfilt_res", "Resonance");
        const bool morphType = type == T::SvfLpBpHp || type == T::SvfLpNotchHp || type == T::Vowel;
        if (morphType)
        {
            d.addParam(ctx, "dfilt_morph", type == T::Vowel ? "Formant" : "Morph");
            if (type == T::Vowel) d.add(std::make_unique<ParamChoice>(ctx, "dfilt_vowelorder", "Vowel Order"), 2);
        }
        else
        {
            d.addParam(ctx, "dfilt_drive", "Drive");
            d.addParam(ctx, "dfilt_drivepost", "Drive Post");
        }
        d.addParams(ctx, { "dfilt_envamt", "dfilt_velenv", "dfilt_lfoamt", "dfilt_keytrack", "dfilt_polyat", "dfilt_level" });

        auto& a = addSection(std::make_unique<Section>("Analog Filter (Env 2, LFO 2)", 4));
        a.addParams(ctx, { "afilt_predrive", "afilt_cutoff", "afilt_res", "afilt_envamt", "afilt_velenv", "afilt_lfoamt",
                           "afilt_keytrack", "afilt_polyat" });

        auto& v = addSection(std::make_unique<Section>("VCA (Env 3, LFO 3)", 4));
        v.addParams(ctx, { "vca_oscslevel", "vca_level", "vca_patchlevel", "vca_initial", "vca_velenv", "vca_lfoamt",
                           "vca_polyat", "vca_snap" });

        for (int k = 0; k < 3; ++k)
        {
            auto& e = addSection(std::make_unique<Section>("Env " + n1(k) + (k == 0 ? " > Digital" : k == 1 ? " > Analog" : " > VCA"), 3, theme::osc(k + 3)));
            e.add(std::make_unique<EnvelopeView>([this, k]() -> const kmi::EnvParams& { return ctx.patch.env[static_cast<size_t>(k)]; },
                                                 theme::osc(k + 3)),
                  3, 1);
        }
    }
};

// ---- Envelopes 1-5 ------------------------------------------------------------------------------------------
class EnvPage final : public Page
{
public:
    using Page::Page;

protected:
    juce::String layoutKey() override { return juce::String(sel_); }

    void build() override
    {
        addHeader(std::make_unique<Selector>(juce::StringArray{ "Env 1  (D.Filter)", "Env 2  (A.Filter)", "Env 3  (VCA)", "Env 4", "Env 5" },
                                             sel_, [this](int i) { sel_ = i; requestRebuild(); },
                                             [](int i) { return theme::osc(i + 3); }),
                  28);
        const juce::String p = "env" + n1(sel_) + "_";
        const auto col = theme::osc(sel_ + 3);
        auto& s = addSection(std::make_unique<Section>("Envelope " + n1(sel_), 10, col));
        const int k = sel_;
        s.add(std::make_unique<EnvelopeView>([this, k]() -> const kmi::EnvParams& { return ctx.patch.env[static_cast<size_t>(k)]; }, col), 10, 2);
        addEnvParams(ctx, s, p, true);

        auto& t = addSection(std::make_unique<Section>("Trigger Sources (all envelopes)", 3, theme::accentWarm));
        auto tap = std::make_unique<juce::TextButton>("Tap");
        tap->setTooltip("Fires every envelope that lists Tap as a trigger source (while a note is held)");
        tap->onClick = [this] { ctx.proc.tapTrigger(); };
        t.add(std::move(tap));
        t.addParam(ctx, "voice_trigcc", "Trigger CC");
        t.addParam(ctx, "voice_trigthresh", "Threshold");
    }

private:
    int sel_ = 0;
};

// ---- LFOs ------------------------------------------------------------------------------------------------------
class LfoPage final : public Page
{
public:
    using Page::Page;

protected:
    juce::String layoutKey() override
    {
        const auto& l = ctx.patch.lfo[static_cast<size_t>(sel_)];
        return juce::String(sel_) + "/" + juce::String(l.bpmSync ? 1 : 0) + "/" + juce::String(static_cast<int>(l.wave)) + "/" +
               juce::String(static_cast<int>(l.trig)) + "/" + juce::String(static_cast<int>(l.oneShot));
    }

    void build() override
    {
        addHeader(std::make_unique<Selector>(juce::StringArray{ "LFO 1  (D.Filter)", "LFO 2  (A.Filter)", "LFO 3  (VCA)", "LFO 4", "LFO 5" },
                                             sel_, [this](int i) { sel_ = i; requestRebuild(); },
                                             [](int i) { return theme::osc(i); }),
                  28);
        const auto& l = ctx.patch.lfo[static_cast<size_t>(sel_)];
        const juce::String p = "lfo" + n1(sel_) + "_";
        const auto col = theme::osc(sel_);
        auto& s = addSection(std::make_unique<Section>("LFO " + n1(sel_), 8, col));
        s.add(std::make_unique<ParamChoice>(ctx, p + "wave", "Wave"), 2);
        if (l.bpmSync) s.add(std::make_unique<ParamChoice>(ctx, p + "div", "Division"), 2);
        else s.addParam(ctx, p + "rate", "Rate");
        s.addParam(ctx, p + "bpm", "BPM Sync");
        s.addParam(ctx, p + "trig", "Trig Sync");
        s.addParam(ctx, p + "level", "Level");
        s.addParam(ctx, p + "delay", "Delay");
        s.addParam(ctx, p + "fade", "Fade In");
        s.addParam(ctx, p + "phase", "Phase");
        s.addParam(ctx, p + "smooth", "Smooth");
        s.addParam(ctx, p + "oneshot", "One-Shot");
        const bool stepMode = l.wave == kmi::LfoWave::Step || l.oneShot == kmi::LfoOneShot::Step;
        if (stepMode) s.addParam(ctx, p + "steps", "Steps");
        if (l.trig == kmi::LfoTrig::Free) s.addParam(ctx, p + "stagger", "Stagger");

        if (l.wave == kmi::LfoWave::Step)
        {
            auto& st = addSection(std::make_unique<Section>("Step Sequence (draw with the mouse)", 16, col));
            st.add(std::make_unique<StepEditor>(ctx, sel_), 16, 3);
        }
    }

private:
    int sel_ = 0;
};

// ---- Mod matrix ---------------------------------------------------------------------------------------------------
class ModMatrixPage final : public juce::Component
{
public:
    explicit ModMatrixPage(UiContext& ctx) : ctx_(ctx)
    {
        for (int k = 0; k < kmi::kNumModSlots; ++k)
        {
            auto row = std::make_unique<Row>(ctx, k);
            content_.addAndMakeVisible(*row);
            rows_.push_back(std::move(row));
        }
        viewport_.setViewedComponent(&content_, false);
        viewport_.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport_);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(theme::textDim);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        auto h = getLocalBounds().reduced(14, 8).removeFromTop(18);
        h.removeFromLeft(36);
        g.drawText("Source", h.removeFromLeft(230), juce::Justification::centredLeft);
        g.drawText("Destination", h.removeFromLeft(330), juce::Justification::centredLeft);
        g.drawText("Depth (+/-128)", h.removeFromLeft(380), juce::Justification::centredLeft);
        g.drawText("Tip: right-click a knob > Modulate with...", h, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(8);
        r.removeFromTop(24);
        viewport_.setBounds(r);
        const int rowH = 30;
        content_.setSize(r.getWidth() - viewport_.getScrollBarThickness(), rowH * static_cast<int>(rows_.size()));
        for (size_t i = 0; i < rows_.size(); ++i)
            rows_[i]->setBounds(0, static_cast<int>(i) * rowH, content_.getWidth(), rowH);
    }

private:
    struct Row final : juce::Component
    {
        Row(UiContext& ctx, int k)
            : ctx_(ctx), prefix_("mod" + n1(k) + "_"), src_(ctx, prefix_ + "src"), dst_(ctx, prefix_ + "dst"),
              depth_(ctx, prefix_ + "depth")
        {
            label_.setText(n1(k), juce::dontSendNotification);
            label_.setColour(juce::Label::textColourId, theme::textDim);
            label_.setJustificationType(juce::Justification::centredRight);
            clear_.setButtonText("Clear");
            clear_.setTooltip("Remove this route");
            clear_.onClick = [this] {
                setParam(ctx_, prefix_ + "src", 0.0f);
                setParam(ctx_, prefix_ + "dst", 0.0f);
                setParam(ctx_, prefix_ + "depth", 0.0f);
            };
            for (juce::Component* c : { static_cast<juce::Component*>(&label_), static_cast<juce::Component*>(&src_),
                                        static_cast<juce::Component*>(&dst_), static_cast<juce::Component*>(&depth_),
                                        static_cast<juce::Component*>(&clear_) })
                addAndMakeVisible(c);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced(2, 3);
            label_.setBounds(r.removeFromLeft(28));
            r.removeFromLeft(8);
            src_.setBounds(r.removeFromLeft(222));
            r.removeFromLeft(8);
            dst_.setBounds(r.removeFromLeft(322));
            r.removeFromLeft(8);
            depth_.setBounds(r.removeFromLeft(372));
            r.removeFromLeft(8);
            clear_.setBounds(r.removeFromLeft(60));
        }
        UiContext& ctx_;
        juce::String prefix_;
        juce::Label label_;
        ParamCombo src_, dst_;
        ParamBar depth_;
        juce::TextButton clear_;
    };

    UiContext& ctx_;
    juce::Viewport viewport_;
    juce::Component content_;
    std::vector<std::unique_ptr<Row>> rows_;
};

// ---- Macros --------------------------------------------------------------------------------------------------------
class MacroPage final : public juce::Component
{
public:
    explicit MacroPage(UiContext& ctx)
    {
        for (int m = 0; m < kmi::kNumMacros; ++m)
        {
            auto card = std::make_unique<Card>(ctx, m);
            addAndMakeVisible(*card);
            cards_.push_back(std::move(card));
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(8);
        const int w = r.getWidth() / 4, h = r.getHeight() / 2;
        for (int m = 0; m < static_cast<int>(cards_.size()); ++m)
            cards_[static_cast<size_t>(m)]->setBounds(r.getX() + (m % 4) * w, r.getY() + (m / 4) * h, w - 6, h - 6);
    }

private:
    struct Card final : juce::Component
    {
        Card(UiContext& ctx, int m) : index_(m), knob_(ctx, "macro" + n1(m) + "_value", "Value", theme::osc(m))
        {
            addAndMakeVisible(knob_);
            for (int r = 0; r < kmi::kNumMacroRoutes; ++r)
            {
                const juce::String p = "macro" + n1(m) + "_r" + n1(r) + "_";
                auto d = std::make_unique<ParamCombo>(ctx, p + "dst");
                auto b = std::make_unique<ParamBar>(ctx, p + "depth");
                addAndMakeVisible(*d);
                addAndMakeVisible(*b);
                dests_.push_back(std::move(d));
                depths_.push_back(std::move(b));
            }
        }
        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour(theme::panel);
            g.fillRoundedRectangle(r, 6.0f);
            g.setColour(theme::panelEdge);
            g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
            g.setColour(theme::osc(index_));
            g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
            g.drawText("Macro " + n1(index_), r.reduced(10.0f, 6.0f).removeFromTop(20.0f), juce::Justification::centredLeft);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced(8);
            r.removeFromTop(22);
            knob_.setBounds(r.removeFromTop(78).withSizeKeepingCentre(80, 78));
            const int rowH = juce::jmin(26, r.getHeight() / kmi::kNumMacroRoutes);
            for (size_t i = 0; i < dests_.size(); ++i)
            {
                auto row = r.removeFromTop(rowH).reduced(0, 2);
                const int split = row.getWidth() * 3 / 5;
                dests_[i]->setBounds(row.removeFromLeft(split));
                row.removeFromLeft(4);
                depths_[i]->setBounds(row);
            }
        }
        int index_;
        ParamKnob knob_;
        std::vector<std::unique_ptr<ParamCombo>> dests_;
        std::vector<std::unique_ptr<ParamBar>> depths_;
    };
    std::vector<std::unique_ptr<Card>> cards_;
};

// ---- Effects -----------------------------------------------------------------------------------------------------
struct FxField
{
    const char* suffix;
    const char* label;
};
const std::vector<FxField>& fieldsFor(kmi::FxType t)
{
    static const std::vector<FxField> none{};
    static const std::vector<FxField> mod{ { "rate", "Rate" }, { "depth", "Depth" }, { "offset", "Offset" },
                                           { "feedback", "Feedback" }, { "stereo", "Stereo" }, { "mix", "Dry/Wet" } };
    static const std::vector<FxField> rotary{ { "woofer", "Woofer" }, { "horn", "Horn" }, { "lodepth", "Low Depth" },
                                              { "hidepth", "High Depth" }, { "balance", "Low/High" }, { "mix", "Dry/Wet" } };
    static const std::vector<FxField> phaser{ { "rate", "Rate" }, { "feedback", "Feedback" }, { "depth", "Depth" },
                                              { "lowfreq", "Low Freq" }, { "offset", "Offset" }, { "mix", "Dry/Wet" } };
    static const std::vector<FxField> lofi{ { "filter", "Filter" }, { "cutoff", "Cutoff" }, { "res", "Resonance" },
                                            { "rate", "Sample Rate" }, { "output", "Output" }, { "mix", "Dry/Wet" } };
    static const std::vector<FxField> trem{ { "rate", "Rate" }, { "depth", "Depth" }, { "square", "Square" },
                                            { "phase", "L/R Phase" }, { "pitchmod", "Pitch Mod" }, { "mix", "Dry/Wet" } };
    static const std::vector<FxField> eq{ { "low", "Low" }, { "mid", "Mid" }, { "high", "High" },
                                          { "xlo", "Low X-over" }, { "xhi", "High X-over" }, { "mix", "Dry/Wet" } };
    static const std::vector<FxField> comp{ { "sidechain", "Sidechain" }, { "ratio", "Ratio" }, { "threshold", "Threshold" },
                                            { "attack", "Attack" }, { "release", "Release" }, { "makeup", "Makeup" },
                                            { "mix", "Dry/Wet" } };
    static const std::vector<FxField> dist{ { "drive", "Drive" }, { "tone", "Tone" }, { "asym", "Asymmetry" },
                                            { "curve", "Curve" }, { "output", "Output" }, { "mix", "Dry/Wet" } };
    switch (t)
    {
        case kmi::FxType::Chorus:
        case kmi::FxType::Flanger: return mod;
        case kmi::FxType::Rotary: return rotary;
        case kmi::FxType::Phaser: return phaser;
        case kmi::FxType::LoFi: return lofi;
        case kmi::FxType::Tremolo: return trem;
        case kmi::FxType::Eq: return eq;
        case kmi::FxType::Compressor: return comp;
        case kmi::FxType::Distort: return dist;
        default: return none;
    }
}

const char* fxPrefix(kmi::FxType t)
{
    static const char* p[] = { "", "chorus_", "flanger_", "rotary_", "phaser_", "lofi_", "tremolo_", "eq_", "comp_", "distort_" };
    const int i = static_cast<int>(t);
    return i >= 0 && i < 10 ? p[i] : "";
}

class FxPage final : public Page
{
public:
    using Page::Page;

protected:
    juce::String layoutKey() override
    {
        const auto& fx = ctx.patch.fx;
        return juce::String(static_cast<int>(fx.pre.type)) + "/" + juce::String(static_cast<int>(fx.post.type)) + "/" +
               juce::String(fx.delay.bpmSync ? 1 : 0);
    }

    void build() override
    {
        const auto& fx = ctx.patch.fx;
        addInsert("1  Pre-FX insert  (before delay)", "prefx_", fx.pre.type, theme::osc(1));

        auto& d = addSection(std::make_unique<Section>("2  Delay", 5, theme::osc(4)));
        d.addParam(ctx, "delay_on", "On");
        d.add(std::make_unique<ParamChoice>(ctx, "delay_type", "Type"), 2);
        d.addParam(ctx, "delay_bpm", "BPM Sync");
        if (fx.delay.bpmSync) d.addParam(ctx, "delay_div", "Division");
        else d.addParam(ctx, "delay_time", "Time");
        d.addParam(ctx, "delay_feedback", "Feedback");
        d.addParam(ctx, "delay_fbtone", "Fb Tone");
        d.addParam(ctx, "delay_wettone", "Wet Tone");
        d.addParam(ctx, "delay_mix", "Dry/Wet");

        auto& r = addSection(std::make_unique<Section>("3  Reverb", 5, theme::osc(6)));
        r.addParam(ctx, "reverb_on", "On");
        r.add(std::make_unique<ParamChoice>(ctx, "reverb_type", "Type"), 2);
        r.addParam(ctx, "reverb_freeze", "Freeze");
        r.addParam(ctx, "reverb_predelay", "Pre-Delay");
        r.addParam(ctx, "reverb_time", "Time");
        r.addParam(ctx, "reverb_tone", "Tone");
        r.addParam(ctx, "reverb_hidamp", "Hi Damp");
        r.addParam(ctx, "reverb_lodamp", "Lo Damp");
        r.addParam(ctx, "reverb_mix", "Dry/Wet");

        addInsert("4  Post-FX insert  (after reverb)", "postfx_", fx.post.type, theme::osc(2));
    }

private:
    void addInsert(const juce::String& title, const juce::String& slot, kmi::FxType type, juce::Colour col)
    {
        auto& s = addSection(std::make_unique<Section>(title, 4, col));
        s.add(std::make_unique<ParamChoice>(ctx, slot + "type", "Effect"), 2);
        const juce::String p = slot + fxPrefix(type);
        for (const auto& f : fieldsFor(type)) s.addParam(ctx, p + f.suffix, f.label);
    }
};

// ---- Voice -----------------------------------------------------------------------------------------------------------
class VoicePage final : public Page
{
public:
    using Page::Page;

protected:
    juce::String layoutKey() override
    {
        const auto& v = ctx.patch.voice;
        return juce::String(static_cast<int>(v.polyMode)) + "/" + juce::String(v.vibratoBpmSync ? 1 : 0) + "/" +
               juce::String(v.mpe ? 1 : 0) + "/" + juce::String(v.oscStereo ? 1 : 0) + "/" + juce::String(v.panner > 0 ? 1 : 0);
    }

    void build() override
    {
        const auto& v = ctx.patch.voice;
        auto& poly = addSection(std::make_unique<Section>("Polyphony", 4));
        poly.add(std::make_unique<ParamChoice>(ctx, "voice_polymode", "Mode"), 2);
        const bool unison = v.polyMode == kmi::PolyMode::Unison || v.polyMode == kmi::PolyMode::UnisonLo ||
                            v.polyMode == kmi::PolyMode::UnisonHi || v.polyMode == kmi::PolyMode::UnisonPoly;
        const bool polyMode = v.polyMode == kmi::PolyMode::PolyRotate || v.polyMode == kmi::PolyMode::PolyReassign;
        if (polyMode) poly.addParam(ctx, "voice_limit", "Voice Limit");
        if (unison)
        {
            poly.addParam(ctx, "voice_density", "Density");
            poly.addParam(ctx, "voice_detune", "Detune");
        }

        auto& glide = addSection(std::make_unique<Section>("Glide", 3));
        glide.addParams(ctx, { "voice_glide", "voice_glidetime", "voice_glidelegato" });

        auto& pitch = addSection(std::make_unique<Section>("Pitch and Vibrato (mod wheel)", 5));
        pitch.addParam(ctx, "voice_bendrange", "Bend Range");
        pitch.addParam(ctx, "voice_vibamt", "Vibrato");
        pitch.addParam(ctx, "voice_vibbpm", "BPM Sync");
        if (v.vibratoBpmSync) pitch.addParam(ctx, "voice_vibdiv", "Division");
        else pitch.addParam(ctx, "voice_vibrate", "Rate");
        pitch.addParam(ctx, "voice_vibwave", "Wave");

        auto& st = addSection(std::make_unique<Section>("Osc Stereo (pairs of voices)", 4));
        st.addParam(ctx, "voice_oscstereo", "Osc Stereo");
        if (v.oscStereo)
        {
            st.addParam(ctx, "voice_oscpantarget", "Pan Applies");
            st.addParam(ctx, "voice_carrierdetune", "Carrier Detune");
        }

        auto& pan = addSection(std::make_unique<Section>("Panner (per note)", 4));
        pan.add(std::make_unique<ParamChoice>(ctx, "voice_panner", "Sequence"), 2);
        if (v.panner > 0)
        {
            pan.addParam(ctx, "voice_pannerwidth", "Width");
            pan.addParam(ctx, "voice_pannermode", "Mode");
        }

        auto& mpe = addSection(std::make_unique<Section>("MPE (ch 1 global, 2-16 per note)", 3));
        mpe.addParam(ctx, "voice_mpe", "MPE");
        if (v.mpe) mpe.addParam(ctx, "voice_mpebend", "Note Bend");

        auto& ch = addSection(std::make_unique<Section>("Character", 5));
        ch.addParams(ctx, { "voice_analogfeel", "voice_randomphase", "voice_oversampling", "voice_bitdepth", "voice_vintage" });

        auto& vm = addSection(std::make_unique<Section>("Voice Mod (per-voice offsets for the Voice Mod sources)", 8));
        for (int k = 0; k < kmi::kNumVoiceMods; ++k) vm.addParam(ctx, "voice_mod" + n1(k), "Voice " + n1(k));

        auto& arp = addSection(std::make_unique<Section>("Arpeggiator (follows host tempo and transport)", 11, theme::accentWarm));
        arp.addParam(ctx, "arp_on", "On");
        arp.add(std::make_unique<ParamChoice>(ctx, "arp_mode", "Mode"), 2);
        arp.add(std::make_unique<ParamChoice>(ctx, "arp_rate", "Rate"), 2);
        arp.addParam(ctx, "arp_octaves", "Octaves");
        arp.addParam(ctx, "arp_gate", "Gate");
        arp.addParam(ctx, "arp_swing", "Swing");
        arp.addParam(ctx, "arp_latch", "Latch");
        arp.add(std::make_unique<ParamChoice>(ctx, "arp_velocity", "Velocity"), 2);
    }
};
} // namespace

juce::StringArray pageNames()
{
    return { "Home", "Algorithm", "Oscillators", "Filters / VCA", "Envelopes", "LFOs", "Mod Matrix", "Macros", "Effects", "Voice" };
}

std::unique_ptr<juce::Component> createPage(int index, UiContext& ctx)
{
    std::unique_ptr<Page> page;
    switch (index)
    {
        case 0: page = std::make_unique<HomePage>(ctx); break;
        case 1: page = std::make_unique<AlgorithmPage>(ctx); break;
        case 2: page = std::make_unique<OscPage>(ctx); break;
        case 3: page = std::make_unique<FilterPage>(ctx); break;
        case 4: page = std::make_unique<EnvPage>(ctx); break;
        case 5: page = std::make_unique<LfoPage>(ctx); break;
        case 6: return std::make_unique<ModMatrixPage>(ctx);
        case 7: return std::make_unique<MacroPage>(ctx);
        case 8: page = std::make_unique<FxPage>(ctx); break;
        default: page = std::make_unique<VoicePage>(ctx); break;
    }
    page->start();
    return page;
}

} // namespace kmiui
