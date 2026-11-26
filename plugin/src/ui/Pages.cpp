#include "Pages.h"

#include "../PluginProcessor.h"
#include "SequencerView.h"
#include "Views.h"
#include "kmi/Modulation.h"
#include "kmi/Tuning.h"

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

        auto& out = addSection(std::make_unique<Section>("Voice", 7));
        out.addParam(ctx, "vca_patchlevel", "Patch Level");
        out.add(std::make_unique<ParamChoice>(ctx, "voice_polymode", "Polyphony"), 2);
        out.addParam(ctx, "voice_glide", "Glide");
        enableWhen(out.addParam(ctx, "voice_glidetime", "Glide Time"), [this] { return glideOn(); });
        out.addParam(ctx, "voice_oversampling", "Quality");
        out.addParam(ctx, "arp_on", "Arp");

        newRow();
        auto& algo = addSection(std::make_unique<Section>("Algorithm", 8));
        algo.add(std::make_unique<AlgorithmView>(ctx, false), 8, 2);
        algo.add(std::make_unique<ParamChoice>(ctx, "algo_mode", "Mode"), 3);
        enableWhen(algo.add(std::make_unique<ParamChoice>(ctx, "algo_index", "Algorithm"), 4), [this] { return algoIs(kmi::AlgoMode::Single); });
        enableWhen(algo.addParam(ctx, "algo_morph", "Morph"), [this] { return algoIs(kmi::AlgoMode::Morph); });

        auto& scope = addSection(std::make_unique<Section>("Output", 7));
        scope.add(std::make_unique<Scope>(ctx), 7, 3);

        newRow();
        auto& filt = addSection(std::make_unique<Section>("Filters", 11));
        filt.add(std::make_unique<ParamChoice>(ctx, "dfilt_type", "Digital Type"), 2);
        filt.addParam(ctx, "dfilt_cutoff", "D.Cutoff");
        filt.addParam(ctx, "dfilt_res", "D.Reso");
        filt.addParam(ctx, "dfilt_envamt", "Env 1 Amt");
        filt.addParam(ctx, "dfilt_drive", "D.Drive");
        filt.addParam(ctx, "afilt_predrive", "Pre-Drive");
        filt.addParam(ctx, "afilt_cutoff", "A.Cutoff");
        filt.addParam(ctx, "afilt_res", "A.Reso");
        filt.addParam(ctx, "afilt_envamt", "Env 2 Amt");

        auto& fx = addSection(std::make_unique<Section>("Effects", 4));
        fx.addParam(ctx, "delay_on", "Delay");
        enableWhen(fx.addParam(ctx, "delay_mix", "Delay Mix"), [this] { return ctx.patch.fx.delay.enabled; });
        fx.addParam(ctx, "reverb_on", "Reverb");
        enableWhen(fx.addParam(ctx, "reverb_mix", "Reverb Mix"), [this] { return ctx.patch.fx.reverb.enabled; });
    }

private:
    bool glideOn() const { return ctx.patch.voice.glideMode != kmi::GlideMode::Off; }
    bool algoIs(kmi::AlgoMode m) const { return ctx.patch.algo.mode == m; }
};

// True if oscillator i modulates at least one oscillator in the current algorithm (its Mode then matters).
bool modulatesAnything(UiContext& ctx, int i)
{
    kmi::Algorithm alg;
    kmi::resolveAlgorithm(ctx.patch, 0.0f, alg);
    for (int c = 0; c < kmi::kNumOscs; ++c)
        if (alg.conn[i][c] > 0.0f) return true;
    return false;
}

// ---- Algorithm -------------------------------------------------------------------------------------------
class AlgorithmPage final : public Page
{
public:
    using Page::Page;

protected:
    void build() override
    {
        const auto morph = [this] { return ctx.patch.algo.mode == kmi::AlgoMode::Morph; };
        auto& view = addSection(std::make_unique<Section>("Structure", 10));
        view.add(std::make_unique<AlgorithmView>(ctx, true), 10, 4);

        auto& sel = addSection(std::make_unique<Section>("Algorithm", 6));
        sel.add(std::make_unique<ParamChoice>(ctx, "algo_mode", "Mode"), 2);
        enableWhen(sel.add(std::make_unique<ParamChoice>(ctx, "algo_index", "Preset Algorithm"), 4),
                   [this] { return ctx.patch.algo.mode == kmi::AlgoMode::Single; });
        enableWhen(sel.add(std::make_unique<ParamKnob>(ctx, "algo_morph", "Morph"), 2), morph);
        for (int s = 0; s < kmi::kNumMorphSlots; ++s)
            enableWhen(sel.add(std::make_unique<ParamChoice>(ctx, "algo_slot" + n1(s), "Morph Slot " + n1(s)), 2), morph);

        newRow();
        auto& oscs = addSection(std::make_unique<Section>("Oscillator Modes and Levels", 8));
        // Mode is how an oscillator acts on the ones it modulates: it does nothing for a carrier-only oscillator.
        for (int i = 0; i < kmi::kNumOscs; ++i)
            enableWhen(oscs.add(std::make_unique<ParamChoice>(ctx, "osc" + n1(i) + "_mode", "Osc " + n1(i))),
                       [this, i] { return modulatesAnything(ctx, i); });
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

// Times, curves and velocity of an envelope.
void addEnvKnobs(UiContext& ctx, Section& s, const juce::String& p, bool withLevel)
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
}

void addEnvSwitches(UiContext& ctx, Section& s, const juce::String& p)
{
    s.addParam(ctx, p + "slow", "Slow");
    s.addParam(ctx, p + "bpm", "BPM");
    s.addParam(ctx, p + "legato", "Legato");
    s.addParam(ctx, p + "reset", "Reset");
    s.addParam(ctx, p + "freerun", "Freerun");
}

// Returns Loop Stages, which only matters while Loop is on.
juce::Component& addEnvLoopAndTriggers(UiContext& ctx, Section& s, const juce::String& p)
{
    s.addParam(ctx, p + "loop", "Loop");
    auto& stages = s.add(std::make_unique<ParamChoice>(ctx, p + "stageloop", "Loop Stages"), 2);
    for (int k = 0; k < kmi::kNumEnvTrigs; ++k)
        s.add(std::make_unique<ParamChoice>(ctx, p + "trig" + n1(k), "Trigger " + n1(k)), 2);
    return stages;
}

// Does any envelope (the five shared ones or an oscillator's own) use this trigger source?
bool anyEnvTrigger(const kmi::Patch& patch, kmi::EnvTrig t)
{
    auto uses = [t](const kmi::EnvParams& e) { return std::find(e.trig.begin(), e.trig.end(), t) != e.trig.end(); };
    for (const auto& e : patch.env)
        if (uses(e)) return true;
    for (const auto& o : patch.osc)
        if (uses(o.env)) return true;
    return false;
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
               "/" + juce::String(ctx.patch.voice.oscStereo ? 1 : 0) + "/" + modTargets(sel_) + "/" +
               juce::String(static_cast<int>(ctx.patch.osc[static_cast<size_t>(sel_)].envSource));
    }

    // The oscillators that osc i modulates in the current algorithm ("1, 3"), or empty for none. Mode only
    // matters for a modulator: it is how this oscillator acts on its targets.
    juce::String modTargets(int i) const
    {
        kmi::Algorithm alg;
        kmi::resolveAlgorithm(ctx.patch, 0.0f, alg);
        juce::StringArray t;
        for (int c = 0; c < kmi::kNumOscs; ++c)
            if (alg.conn[i][c] > 0.0f) t.add(n1(c));
        return t.joinIntoString(", ");
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
                  32);
        if (group_) buildGroup();
        else buildSingle();
    }

private:
    void buildSingle()
    {
        const int i = sel_;
        const juce::String p = "osc" + n1(i) + "_";
        const auto col = theme::osc(i);
        const juce::String targets = modTargets(i);
        const juce::String role = targets.isEmpty()
                                      ? "carrier only: Mode has no effect (it modulates nothing in this algorithm)"
                                      : "modulates Osc " + targets + " using Mode";
        auto& s = addSection(std::make_unique<Section>("Oscillator " + n1(i) + "   -   " + role, 8, col));
        const auto& o = ctx.patch.osc[static_cast<size_t>(i)];
        enableWhen(s.add(std::make_unique<ParamChoice>(ctx, p + "mode", "Mode (as modulator)"), 2), [this, i] { return modulatesAnything(ctx, i); });
        s.add(std::make_unique<ParamChoice>(ctx, p + "wave", "Waveform"), 2);
        enableWhen(s.addParam(ctx, p + "scan", "Wave Scan"), [&o] { return o.scanRange > 0; }); // Scan Range 0 = off
        s.addParam(ctx, p + "scanrange", "Scan Range");
        s.add(std::make_unique<WavePanel>(ctx, i, col), 2, 2);
        s.keepHeight();
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
        // Keyscale: the breakpoint and curves only act once a depth is above zero.
        enableWhen(s.addParam(ctx, p + "ksbreak", "KS Break"), [&o] { return o.keyscaleLoDepth > 0.0f || o.keyscaleHiDepth > 0.0f; });
        s.addParam(ctx, p + "kslodepth", "KS Lo Depth");
        enableWhen(s.addParam(ctx, p + "kslocurve", "KS Lo Curve"), [&o] { return o.keyscaleLoDepth > 0.0f; });
        s.addParam(ctx, p + "kshidepth", "KS Hi Depth");
        enableWhen(s.addParam(ctx, p + "kshicurve", "KS Hi Curve"), [&o] { return o.keyscaleHiDepth > 0.0f; });
        if (ctx.patch.voice.oscStereo) s.addParam(ctx, p + "pan", "Pan");

        // With Env Source set to Env 1-3 the oscillator follows that envelope and its own is unused.
        const bool ownEnv = o.envSource == kmi::EnvSource::Self;
        const juce::String unused = ownEnv ? juce::String() : "   -   not used: Env Source is Env " + juce::String(static_cast<int>(o.envSource));
        auto& e = addSection(std::make_unique<Section>("Oscillator " + n1(i) + " Envelope" + unused, 8, col));
        enableWhen(e, [&o] { return o.envSource == kmi::EnvSource::Self; });
        e.add(std::make_unique<EnvelopeView>([this, i]() -> const kmi::EnvParams& { return ctx.patch.osc[static_cast<size_t>(i)].env; }, col,
                                             &ctx, p + "env_"),
              8, 2);
        addEnvKnobs(ctx, e, p + "env_", false);
        addEnvSwitches(ctx, e, p + "env_");

        newRow();
        auto& t = addSection(std::make_unique<Section>("Oscillator " + n1(i) + " Envelope Loop and Triggers" + unused, Section::kAuto, col));
        enableWhen(t, [&o] { return o.envSource == kmi::EnvSource::Self; });
        enableWhen(addEnvLoopAndTriggers(ctx, t, p + "env_"), [&o] { return o.env.loop != 0; });
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

        newRow();
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
            enableWhen(d.addParam(ctx, "dfilt_drivepost", "Drive Post"), [this] { return ctx.patch.dfilt.drive > 0.0f; });
        }
        d.addParams(ctx, { "dfilt_envamt", "dfilt_velenv", "dfilt_lfoamt", "dfilt_keytrack", "dfilt_polyat", "dfilt_level" });

        auto& a = addSection(std::make_unique<Section>("Analog Filter (Env 2, LFO 2)", 4));
        a.addParams(ctx, { "afilt_predrive", "afilt_cutoff", "afilt_res", "afilt_envamt", "afilt_velenv", "afilt_lfoamt",
                           "afilt_keytrack", "afilt_polyat" });

        auto& v = addSection(std::make_unique<Section>("VCA (Env 3, LFO 3)", 4));
        v.addParams(ctx, { "vca_oscslevel", "vca_level", "vca_patchlevel", "vca_initial", "vca_velenv", "vca_lfoamt",
                           "vca_polyat", "vca_snap" });

        // The three envelopes these blocks use, editable here (curves, loops and triggers on the Envelopes page).
        newRow();
        for (int k = 0; k < 3; ++k)
        {
            const auto col = theme::osc(k + 3);
            const juce::String p = "env" + n1(k) + "_";
            auto& e = addSection(std::make_unique<Section>("Env " + n1(k) + (k == 0 ? "  >  Digital Filter" : k == 1 ? "  >  Analog Filter" : "  >  VCA"), 6, col));
            e.add(std::make_unique<EnvelopeView>([this, k]() -> const kmi::EnvParams& { return ctx.patch.env[static_cast<size_t>(k)]; }, col,
                                                 &ctx, p),
                  6, 2);
            e.addParam(ctx, p + "delay", "Delay");
            e.addParam(ctx, p + "attack", "Attack");
            e.addParam(ctx, p + "hold", "Hold");
            e.addParam(ctx, p + "decay", "Decay");
            e.addParam(ctx, p + "sustain", "Sustain");
            e.addParam(ctx, p + "release", "Release");
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
                  32);
        const juce::String p = "env" + n1(sel_) + "_";
        const auto col = theme::osc(sel_ + 3);
        auto& s = addSection(std::make_unique<Section>("Envelope " + n1(sel_), 12, col));
        const int k = sel_;
        s.add(std::make_unique<EnvelopeView>([this, k]() -> const kmi::EnvParams& { return ctx.patch.env[static_cast<size_t>(k)]; }, col,
                                             &ctx, p),
              12, 2);
        addEnvKnobs(ctx, s, p, true);

        newRow();
        auto& o = addSection(std::make_unique<Section>("Envelope " + n1(sel_) + " Options, Loop and Triggers", 8, col));
        addEnvSwitches(ctx, o, p);
        enableWhen(addEnvLoopAndTriggers(ctx, o, p), [this, k] { return ctx.patch.env[static_cast<size_t>(k)].loop != 0; });

        auto& t = addSection(std::make_unique<Section>("Trigger Sources   -   all envelopes", 2, theme::accentWarm));
        auto tap = std::make_unique<juce::TextButton>("Tap");
        tap->setTooltip("Fires every envelope that lists Tap as a trigger source (while a note is held)");
        tap->onClick = [this] { ctx.proc.tapTrigger(); };
        // Each source only matters while some envelope lists it as a trigger.
        enableWhen(t.add(std::move(tap)), [this] { return anyEnvTrigger(ctx.patch, kmi::EnvTrig::Tap); });
        const auto cc = [this] { return anyEnvTrigger(ctx.patch, kmi::EnvTrig::CcThreshold); };
        enableWhen(t.addParam(ctx, "voice_trigcc", "Trigger CC"), cc);
        enableWhen(t.addParam(ctx, "voice_trigthresh", "Threshold"), cc);
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
                  32);
        const auto& l = ctx.patch.lfo[static_cast<size_t>(sel_)];
        const juce::String p = "lfo" + n1(sel_) + "_";
        const auto col = theme::osc(sel_);
        auto& s = addSection(std::make_unique<Section>("LFO " + n1(sel_), Section::kAuto, col));
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

        newRow();
        if (l.wave == kmi::LfoWave::Step)
        {
            auto& st = addSection(std::make_unique<Section>("Step Sequence   -   draw with the mouse", 10, col));
            st.add(std::make_unique<StepEditor>(ctx, sel_), 10, 3);
        }
        auto& shape = addSection(std::make_unique<Section>("Shape", l.wave == kmi::LfoWave::Step ? 6 : 16, col));
        shape.add(std::make_unique<LfoView>(ctx, sel_, col), shape.columns(), 3);
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

    // Column positions shared by the header and every row.
    struct Cols
    {
        juce::Rectangle<int> num, src, dst, depth, clear;
    };
    static Cols columnsFor(juce::Rectangle<int> r)
    {
        Cols c;
        c.num = r.removeFromLeft(28);
        r.removeFromLeft(10);
        c.clear = r.removeFromRight(64);
        r.removeFromRight(10);
        const int w = r.getWidth() - 20;
        c.src = r.removeFromLeft(w * 30 / 100);
        r.removeFromLeft(10);
        c.dst = r.removeFromLeft(w * 38 / 100);
        r.removeFromLeft(10);
        c.depth = r;
        return c;
    }

    void paint(juce::Graphics& g) override
    {
        auto panel = getLocalBounds().reduced(8).toFloat().reduced(0.5f);
        g.setColour(theme::panel);
        g.fillRoundedRectangle(panel, 8.0f);
        g.setColour(theme::panelEdge.withAlpha(0.7f));
        g.drawRoundedRectangle(panel, 8.0f, 1.0f);

        auto h = headerArea();
        const auto c = columnsFor(h.withWidth(h.getWidth() - viewport_.getScrollBarThickness()).reduced(2, 0));
        g.setColour(theme::textDim);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText("Source", c.src, juce::Justification::centredLeft);
        g.drawText("Destination", c.dst, juce::Justification::centredLeft);
        g.drawText("Depth (+/-128)", c.depth, juce::Justification::centredLeft);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText("Tip: right-click any knob > Modulate with...", c.depth, juce::Justification::centredRight);
    }

    juce::Rectangle<int> headerArea() const { return getLocalBounds().reduced(16, 14).removeFromTop(20); }

    void resized() override
    {
        auto r = getLocalBounds().reduced(16, 14);
        r.removeFromTop(26);
        viewport_.setBounds(r);
        const int rowH = 30;
        content_.setSize(r.getWidth() - viewport_.getScrollBarThickness(), rowH * static_cast<int>(rows_.size()));
        for (size_t i = 0; i < rows_.size(); ++i)
            rows_[i]->setBounds(0, static_cast<int>(i) * rowH, content_.getWidth(), rowH);
    }

private:
    struct Row final : juce::Component, private juce::Timer
    {
        Row(UiContext& ctx, int k)
            : ctx_(ctx), index_(k), prefix_("mod" + n1(k) + "_"), src_(ctx, prefix_ + "src"), dst_(ctx, prefix_ + "dst"),
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
            timerCallback();
            startTimerHz(8);
        }
        // Depth does nothing until the route has both a source and a destination; Clear needs something to clear.
        void timerCallback() override
        {
            const auto& m = ctx_.patch.mod[static_cast<size_t>(index_)];
            const bool complete = m.source != 0 && m.dest != 0, any = m.source != 0 || m.dest != 0 || m.depth != 0.0f;
            if (depth_.isEnabled() != complete) depth_.setEnabled(complete);
            if (clear_.isEnabled() != any) clear_.setEnabled(any);
        }
        void resized() override
        {
            const auto c = columnsFor(getLocalBounds().reduced(2, 3));
            label_.setBounds(c.num);
            src_.setBounds(c.src);
            dst_.setBounds(c.dst);
            depth_.setBounds(c.depth);
            clear_.setBounds(c.clear);
        }
        UiContext& ctx_;
        int index_;
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
        const int w = (r.getWidth() + 8) / 4, h = (r.getHeight() + 8) / 2;
        for (int m = 0; m < static_cast<int>(cards_.size()); ++m)
            cards_[static_cast<size_t>(m)]->setBounds(r.getX() + (m % 4) * w, r.getY() + (m / 4) * h, w - 8, h - 8);
    }

private:
    struct Card final : juce::Component, private juce::Timer
    {
        Card(UiContext& ctx, int m) : ctx_(ctx), index_(m), knob_(ctx, "macro" + n1(m) + "_value", "Value", theme::osc(m))
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
            timerCallback();
            startTimerHz(8);
        }
        // A route's depth does nothing until it has a destination.
        void timerCallback() override
        {
            const auto& routes = ctx_.patch.macro[static_cast<size_t>(index_)].routes;
            for (size_t r = 0; r < depths_.size(); ++r)
            {
                const bool on = routes[r].dest != 0;
                if (depths_[r]->isEnabled() != on) depths_[r]->setEnabled(on);
            }
        }
        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour(theme::panel);
            g.fillRoundedRectangle(r, 8.0f);
            g.setColour(theme::panelEdge.withAlpha(0.7f));
            g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
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
        UiContext& ctx_;
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
        addInsert("1   Pre-FX Insert", "prefx_", fx.pre.type, theme::osc(1));

        auto& d = addSection(std::make_unique<Section>("2   Delay", 4, theme::osc(4)));
        // Everything but On is inactive while the delay (or reverb) is switched off.
        const auto delayOn = [this] { return ctx.patch.fx.delay.enabled; };
        d.addParam(ctx, "delay_on", "On");
        enableWhen({ &d.add(std::make_unique<ParamChoice>(ctx, "delay_type", "Type"), 2), &d.addParam(ctx, "delay_bpm", "BPM Sync"),
                     fx.delay.bpmSync ? &d.addParam(ctx, "delay_div", "Division") : &d.addParam(ctx, "delay_time", "Time"),
                     &d.addParam(ctx, "delay_feedback", "Feedback"), &d.addParam(ctx, "delay_fbtone", "Fb Tone"),
                     &d.addParam(ctx, "delay_wettone", "Wet Tone"), &d.addParam(ctx, "delay_mix", "Dry/Wet") },
                   delayOn);

        auto& r = addSection(std::make_unique<Section>("3   Reverb", 4, theme::osc(6)));
        const auto reverbOn = [this] { return ctx.patch.fx.reverb.enabled; };
        r.addParam(ctx, "reverb_on", "On");
        enableWhen({ &r.add(std::make_unique<ParamChoice>(ctx, "reverb_type", "Type"), 2), &r.addParam(ctx, "reverb_freeze", "Freeze"),
                     &r.addParam(ctx, "reverb_predelay", "Pre-Delay"), &r.addParam(ctx, "reverb_time", "Time"),
                     &r.addParam(ctx, "reverb_tone", "Tone"), &r.addParam(ctx, "reverb_hidamp", "Hi Damp"),
                     &r.addParam(ctx, "reverb_lodamp", "Lo Damp"), &r.addParam(ctx, "reverb_mix", "Dry/Wet") },
                   reverbOn);

        addInsert("4   Post-FX Insert", "postfx_", fx.post.type, theme::osc(2));

        newRow();
        auto& out = addSection(std::make_unique<Section>("Output   -   the signal runs left to right: Pre-FX, Delay, Reverb, Post-FX", 16));
        out.add(std::make_unique<Scope>(ctx), 16, 2);
    }

private:
    void addInsert(const juce::String& title, const juce::String& slot, kmi::FxType type, juce::Colour col)
    {
        auto& s = addSection(std::make_unique<Section>(title, 4, col));
        s.add(std::make_unique<ParamChoice>(ctx, slot + "type", "Effect"), 4);
        const juce::String p = slot + fxPrefix(type);
        for (const auto& f : fieldsFor(type)) s.addParam(ctx, p + f.suffix, f.label);
        if (type == kmi::FxType::Bypass)
        {
            auto note = std::make_unique<juce::Label>();
            note->setText("Empty slot. Choose an effect above: chorus, flanger, rotary, phaser, lo-fi, tremolo, EQ, "
                          "compressor or distortion.",
                          juce::dontSendNotification);
            note->setJustificationType(juce::Justification::centredTop);
            note->setColour(juce::Label::textColourId, theme::textDim);
            note->setFont(juce::Font(juce::FontOptions(12.0f)));
            s.add(std::move(note), 4, 2);
            s.keepHeight();
        }
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

        auto& pitch = addSection(std::make_unique<Section>("Pitch and Vibrato   -   vibrato on the mod wheel", 5));
        pitch.addParam(ctx, "voice_bendrange", "Bend Range");
        pitch.addParam(ctx, "voice_vibamt", "Vibrato");
        // With no vibrato depth the mod wheel adds none, so its rate and shape do nothing.
        enableWhen({ &pitch.addParam(ctx, "voice_vibbpm", "BPM Sync"),
                     v.vibratoBpmSync ? &pitch.addParam(ctx, "voice_vibdiv", "Division") : &pitch.addParam(ctx, "voice_vibrate", "Rate"),
                     &pitch.addParam(ctx, "voice_vibwave", "Wave") },
                   [this] { return ctx.patch.voice.vibratoAmount > 0.0f; });

        auto& ch = addSection(std::make_unique<Section>("Character", 5));
        ch.addParams(ctx, { "voice_analogfeel", "voice_randomphase", "voice_oversampling", "voice_bitdepth", "voice_vintage" });

        newRow();
        auto& glide = addSection(std::make_unique<Section>("Glide", 3));
        glide.addParam(ctx, "voice_glide", "Glide");
        enableWhen({ &glide.addParam(ctx, "voice_glidetime", "Glide Time"), &glide.addParam(ctx, "voice_glidelegato", "Glide Legato") },
                   [this] { return ctx.patch.voice.glideMode != kmi::GlideMode::Off; });

        auto& st = addSection(std::make_unique<Section>("Osc Stereo   -   pairs of voices", 3));
        st.addParam(ctx, "voice_oscstereo", "Osc Stereo");
        if (v.oscStereo)
        {
            st.addParam(ctx, "voice_oscpantarget", "Pan Applies");
            st.addParam(ctx, "voice_carrierdetune", "Carrier Detune");
        }

        auto& pan = addSection(std::make_unique<Section>("Panner   -   per note", 4));
        pan.add(std::make_unique<ParamChoice>(ctx, "voice_panner", "Sequence"), 2);
        if (v.panner > 0)
        {
            pan.addParam(ctx, "voice_pannerwidth", "Width");
            pan.addParam(ctx, "voice_pannermode", "Mode");
        }

        auto& mpe = addSection(std::make_unique<Section>("MPE   -   ch 1 global, 2-16 per note", 3));
        mpe.addParam(ctx, "voice_mpe", "MPE");
        if (v.mpe) mpe.addParam(ctx, "voice_mpebend", "Note Bend");

        newRow();
        auto& vm = addSection(std::make_unique<Section>("Voice Mod   -   per-voice offsets for the Voice Mod sources", 16));
        for (int k = 0; k < kmi::kNumVoiceMods; ++k) vm.addParam(ctx, "voice_mod" + n1(k), "Voice " + n1(k));

        newRow();
        auto& arp = addSection(std::make_unique<Section>("Arpeggiator   -   follows host tempo and transport", 11, theme::accentWarm));
        // Everything but On is inactive while the arpeggiator is off; Phrase only applies in Phrase mode.
        const auto arpOn = [this] { return ctx.patch.arp.enabled; };
        arp.addParam(ctx, "arp_on", "On");
        enableWhen({ &arp.add(std::make_unique<ParamChoice>(ctx, "arp_mode", "Mode"), 2),
                     &arp.add(std::make_unique<ParamChoice>(ctx, "arp_rate", "Rate"), 2), &arp.addParam(ctx, "arp_octaves", "Octaves"),
                     &arp.addParam(ctx, "arp_gate", "Gate"), &arp.addParam(ctx, "arp_swing", "Swing"), &arp.addParam(ctx, "arp_latch", "Latch"),
                     &arp.add(std::make_unique<ParamChoice>(ctx, "arp_velocity", "Velocity"), 2),
                     &arp.add(std::make_unique<ParamChoice>(ctx, "arp_octmode", "Octave Mode"), 2),
                     &arp.add(std::make_unique<ParamChoice>(ctx, "arp_length", "Length"), 2), &arp.addParam(ctx, "arp_offset", "Step Offset"),
                     &arp.addParam(ctx, "arp_ratchet", "Ratchet"), &arp.addParam(ctx, "arp_chance", "Chance") },
                   arpOn);
        enableWhen(arp.add(std::make_unique<ParamChoice>(ctx, "arp_phrase", "Phrase (Phrase mode)"), 3),
                   [this] { return ctx.patch.arp.enabled && ctx.patch.arp.mode == kmi::ArpMode::Phrase; });
        enableWhen(arp.addParam(ctx, "arp_midiout", "MIDI Out"), arpOn);
    }
};
} // namespace

// ---- Scales ----------------------------------------------------------------------------------------------------------
class ScalesPage final : public Page
{
public:
    using Page::Page;

protected:
    juce::String layoutKey() override { return juce::String(ctx.patch.scale.scale == kmi::customScaleIndex() ? 1 : 0); }

    void build() override
    {
        auto& s = addSection(std::make_unique<Section>("Scale and Tuning   -   Key Lock: Nearest snaps keys to the scale, White Keys plays its degrees from the white keys (middle C = root). Temperaments follow the Root", 16));
        s.add(std::make_unique<ParamChoice>(ctx, "scale_type", "Scale"), 4);
        // Chromatic with equal temperament has no root to speak of, and every key is already in the scale.
        enableWhen(s.add(std::make_unique<ParamChoice>(ctx, "scale_root", "Root"), 2),
                   [this] { return ctx.patch.scale.scale != 0 || ctx.patch.scale.tuning != 0; });
        enableWhen(s.add(std::make_unique<ParamChoice>(ctx, "scale_keylock", "Key Lock"), 2), [this] { return ctx.patch.scale.scale != 0; });
        s.add(std::make_unique<ParamChoice>(ctx, "tuning_type", "Tuning"), 3);
        s.add(std::make_unique<ScalaPanel>(ctx), 5);
        s.keepHeight();
        if (ctx.patch.scale.scale == kmi::customScaleIndex())
        {
            newRow();
            auto& c = addSection(std::make_unique<Section>("Custom Scale   -   notes above the root", 12, theme::accentWarm));
            static const char* intervals[12] = { "Root", "m2", "M2", "m3", "M3", "P4", "Tritone", "P5", "m6", "M6", "m7", "M7" };
            for (int k = 0; k < 12; ++k) c.addParam(ctx, "scale_custom" + juce::String(k + 1), intervals[k]);
        }
        newRow();
        auto& v = addSection(std::make_unique<Section>("One Octave from the Root   -   scale notes lit; cents from equal temperament (Scala: pitch per key)", 16));
        v.add(std::make_unique<ScaleView>(ctx), 16, 2);
    }
};

// ---- Sequencer -------------------------------------------------------------------------------------------------------
class SequencerPage final : public Page
{
public:
    using Page::Page;

protected:
    juce::String layoutKey() override { return juce::String(track_); }

    void build() override
    {
        addHeader(std::make_unique<Selector>(juce::StringArray{ "Track 1", "Track 2", "Macro Lanes" }, track_,
                                             [this](int i) {
                                                 track_ = i;
                                                 requestRebuild();
                                             }),
                  32);
        static const char* ids[kmi::kSeqTracks] = { "seq_t1_", "seq_t2_", "seq_mac_" };
        const juce::String p = ids[track_];
        const auto col = track_ < kmi::kSeqNoteTracks ? theme::osc(track_ * 3) : theme::accentWarm;
        auto& s = addSection(std::make_unique<Section>(
            "Sequencer   -   Host Transport: runs while the host plays. Held Key: runs while a key is held, transposed from C3", Section::kAuto, col));
        // Settings are inactive while the sequencer is off, track settings also while the track is off. The pattern
        // itself stays editable either way.
        const int tr = track_;
        const auto seqOn = [this] { return ctx.patch.seq.enabled; };
        const auto trackOn = [this, tr] { return ctx.patch.seq.enabled && ctx.patch.seq.track[static_cast<size_t>(tr)].enabled; };
        s.addParam(ctx, "seq_on", "On");
        enableWhen({ &s.add(std::make_unique<ParamChoice>(ctx, "seq_trigger", "Trigger"), 2), &s.addParam(ctx, "seq_midiout", "MIDI Out"),
                     &s.addParam(ctx, p + "on", track_ < kmi::kSeqNoteTracks ? "Track On" : "Lanes On") },
                   seqOn);
        enableWhen({ &s.addParam(ctx, p + "length", "Length"), &s.add(std::make_unique<ParamChoice>(ctx, p + "rate", "Rate"), 2),
                     &s.add(std::make_unique<ParamChoice>(ctx, p + "mode", "Play Mode"), 2),
                     track_ < kmi::kSeqNoteTracks ? &s.addParam(ctx, p + "transpose", "Transpose") : nullptr },
                   trackOn);

        const juce::String help = track_ < kmi::kSeqNoteTracks
            ? "Click to add a note, drag right for length, click a note to remove it, wheel over a note for velocity. Click a step number to select it."
            : "Each lane drives one macro while the sequence runs. Pick a lane, tick it on, then draw.";
        newRow();
        auto& g = addSection(std::make_unique<Section>("Pattern   -   " + help, 16, col));
        g.add(std::make_unique<SeqEditor>(ctx, track_), 16, 5);
    }

private:
    int track_ = 0;
};

juce::StringArray pageNames()
{
    return { "Home", "Algorithm", "Oscillators", "Filters / VCA", "Envelopes", "LFOs", "Mod Matrix", "Macros", "Effects", "Voice", "Scales", "Sequencer" };
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
        case 9: page = std::make_unique<VoicePage>(ctx); break;
        case 10: page = std::make_unique<ScalesPage>(ctx); break;
        default: page = std::make_unique<SequencerPage>(ctx); break;
    }
    page->start();
    return page;
}

} // namespace kmiui
