#include "Parameters.h"

#include <memory>

#include "kmi/Dsp.h"
#include "kmi/Filters.h"
#include "kmi/Modulation.h"
#include "kmi/Wavetables.h"

namespace kmiplug {

namespace {
using Kind = ParameterBinding::Kind;
using Range = juce::NormalisableRange<float>;

// ---- Visitor: the single source of truth walks every parameter once per use ------------------
struct Visitor
{
    virtual ~Visitor() = default;
    virtual void beginGroup(const juce::String& id, const juce::String& name) = 0;
    virtual void endGroup() = 0;
    // value_in_patch = host_value * scale + offset
    virtual void number(const juce::String& id, const juce::String& name, Range range, float def, void* target,
                        Kind kind, float scale, float offset, const juce::String& unit, int decimals) = 0;
    virtual void choice(const juce::String& id, const juce::String& name, const juce::StringArray& items, int def,
                        void* target, Kind kind, float offset) = 0;
    virtual void toggle(const juce::String& id, const juce::String& name, bool def, bool* target) = 0;
};

juce::String formatTime(float ms)
{
    if (ms < 1000.0f) return juce::String(ms, ms < 10.0f ? 2 : 1) + " ms";
    return juce::String(ms / 1000.0f, 2) + " s";
}

Range timeRange(float maxMs)
{
    Range r(0.0f, maxMs, 0.0f);
    r.setSkewForCentre(std::min(1000.0f, maxMs * 0.1f));
    return r;
}

Range skewed(float lo, float hi, float centre)
{
    Range r(lo, hi, 0.0f);
    r.setSkewForCentre(centre);
    return r;
}

juce::StringArray names(std::initializer_list<const char*> list)
{
    juce::StringArray a;
    for (auto* s : list) a.add(s);
    return a;
}

juce::StringArray fromVector(const std::vector<std::string>& v)
{
    juce::StringArray a;
    for (const auto& s : v) a.add(juce::String(s));
    return a;
}

juce::StringArray waveNames()
{
    juce::StringArray a;
    const auto& bank = kmi::WavetableBank::instance();
    for (int i = 0; i < bank.numWaves(); ++i) a.add(bank.waveName(i));
    return a;
}

juce::StringArray syncNames()
{
    juce::StringArray a;
    for (const auto& d : kmi::kSyncDivisions) a.add(d.name);
    return a;
}

juce::StringArray algorithmNames()
{
    juce::StringArray a;
    for (int i = 0; i < kmi::numPresetAlgorithms(); ++i)
        a.add(juce::String(i + 1).paddedLeft('0', 2) + " " + kmi::presetAlgorithmName(i));
    return a;
}

juce::StringArray morphSlotNames()
{
    juce::StringArray a = names({ "Off", "Silence" });
    a.addArray(algorithmNames());
    return a;
}

juce::StringArray envLoopNames()
{
    juce::StringArray a("Off");
    for (int i = 2; i <= 50; ++i) a.add(juce::String(i));
    a.add("Infinite");
    return a;
}

int envLoopToIndex(int loop) { return loop <= 0 ? 0 : (loop >= kmi::kEnvLoopInfinite ? 50 : loop - 1); }

template <typename E>
void* enumPtr(E& e) { return static_cast<void*>(&e); }

// ---- The parameter description -----------------------------------------------------------------
void describeEnv(Visitor& v, const juce::String& p, kmi::EnvParams& t, const kmi::EnvParams& d, bool withLevel)
{
    v.number(p + "delay", "Delay", timeRange(32000.0f), d.delayMs, &t.delayMs, Kind::Float, 1, 0, "ms", -1);
    v.number(p + "attack", "Attack", timeRange(36000.0f), d.attackMs, &t.attackMs, Kind::Float, 1, 0, "ms", -1);
    v.number(p + "hold", "Hold", timeRange(36000.0f), d.holdMs, &t.holdMs, Kind::Float, 1, 0, "ms", -1);
    v.number(p + "decay", "Decay", timeRange(60000.0f), d.decayMs, &t.decayMs, Kind::Float, 1, 0, "ms", -1);
    v.number(p + "sustain", "Sustain", Range(0, 128, 0.1f), d.sustain * 128.0f, &t.sustain, Kind::Float, 1.0f / 128.0f, 0, "", 1);
    v.number(p + "release", "Release", timeRange(60000.0f), d.releaseMs, &t.releaseMs, Kind::Float, 1, 0, "ms", -1);
    v.number(p + "atkcurve", "Attack Curve", Range(-64, 64, 0.1f), d.attackCurve * 64.0f, &t.attackCurve, Kind::Float, 1.0f / 64.0f, 0, "", 1);
    v.number(p + "deccurve", "Decay Curve", Range(-64, 64, 0.1f), d.decayCurve * 64.0f, &t.decayCurve, Kind::Float, 1.0f / 64.0f, 0, "", 1);
    v.number(p + "relcurve", "Release Curve", Range(-64, 64, 0.1f), d.releaseCurve * 64.0f, &t.releaseCurve, Kind::Float, 1.0f / 64.0f, 0, "", 1);
    v.toggle(p + "slow", "Slow Mode", d.slow, &t.slow);
    v.toggle(p + "bpm", "BPM Sync", d.bpmSync, &t.bpmSync);
    v.toggle(p + "legato", "Legato", d.legato, &t.legato);
    v.toggle(p + "reset", "Reset", d.reset, &t.reset);
    v.toggle(p + "freerun", "Freerun", d.freerun, &t.freerun);
    v.choice(p + "loop", "Loop", envLoopNames(), envLoopToIndex(d.loop), &t.loop, Kind::EnvLoop, 0);
    v.choice(p + "stageloop", "Stage Loop", names({ "Delay>Attack", "Delay>Hold", "Delay>Decay" }), d.stageLoop, &t.stageLoop, Kind::Int, 0);
    v.number(p + "velcurve", "Velocity Curve", Range(-64, 64, 0.1f), d.velCurve * 64.0f, &t.velCurve, Kind::Float, 1.0f / 64.0f, 0, "", 1);
    v.number(p + "velamt", "Velocity > Env", Range(-64, 64, 0.1f), d.velAmount * 64.0f, &t.velAmount, Kind::Float, 1.0f / 64.0f, 0, "", 1);
    if (withLevel)
        v.number(p + "level", "Level", Range(0, 128, 0.1f), d.level * 128.0f, &t.level, Kind::Float, 1.0f / 128.0f, 0, "", 1);
}

void describe(Visitor& v, kmi::Patch& t, const kmi::Patch& d)
{
    const auto bipolar64 = Range(-64, 64, 0.1f);
    const auto level128 = Range(0, 128, 0.1f);
    const auto bipolar128 = Range(-128, 128, 0.1f);
    const auto keytrack = Range(-200, 200, 1.0f);

    // Oscillators
    for (int i = 0; i < kmi::kNumOscs; ++i)
    {
        const juce::String n(i + 1), p = "osc" + n + "_";
        auto& o = t.osc[static_cast<size_t>(i)];
        const auto& od = d.osc[static_cast<size_t>(i)];
        v.beginGroup("osc" + n, "Osc " + n);
        v.choice(p + "mode", "Mode", names({ "Phase Mod", "Freq Mod", "PW Mod", "Hard Sync", "PD Square", "PD Saw", "PD Saw-Pulse" }),
                 static_cast<int>(od.mode), enumPtr(o.mode), Kind::Enum8, 0);
        v.choice(p + "wave", "Wave", waveNames(), od.wave, &o.wave, Kind::Int, 0);
        v.toggle(p + "invert", "Invert", od.invert, &o.invert);
        v.choice(p + "pitchmode", "Pitch Mode", names({ "Semitone", "Ratio", "Frequency" }), static_cast<int>(od.pitchMode),
                 enumPtr(o.pitchMode), Kind::Enum8, 0);
        v.number(p + "semi", "Semitone", Range(-36, 36, 1), od.semitone, &o.semitone, Kind::Float, 1, 0, "st", 0);
        v.number(p + "cents", "Cents", Range(-50, 50, 0.1f), od.cents, &o.cents, Kind::Float, 1, 0, "ct", 1);
        v.number(p + "ratio", "Ratio", skewed(0.25f, 64.0f, 4.0f), od.ratio, &o.ratio, Kind::Float, 1, 0, "", 2);
        v.number(p + "ratiofine", "Ratio Fine", Range(-50, 100, 0.01f), od.ratioFine, &o.ratioFine, Kind::Float, 1, 0, "%", 2);
        v.number(p + "freq", "Frequency", skewed(0.0f, 10000.0f, 500.0f), od.freqHz, &o.freqHz, Kind::Float, 1, 0, "Hz", 2);
        v.number(p + "freqfine", "Frequency Fine", Range(0, 0.99f, 0.01f), od.freqFine, &o.freqFine, Kind::Float, 1, 0, "Hz", 2);
        v.number(p + "level", "Initial Level", level128, od.initLevel * 128.0f, &o.initLevel, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.number(p + "envlevel", "Env Level", bipolar128, od.envLevel * 128.0f, &o.envLevel, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.number(p + "feedback", "Feedback", Range(0, 100, 0.1f), od.feedback * 100.0f, &o.feedback, Kind::Float, 0.01f, 0, "", 1);
        v.number(p + "keytrack", "Keytrack", keytrack, od.keytrack * 100.0f, &o.keytrack, Kind::Float, 0.01f, 0, "%", 0);
        v.number(p + "phase", "Phase", Range(0, 360, 1), od.phaseDeg, &o.phaseDeg, Kind::Float, 1, 0, "deg", 0);
        v.number(p + "velocity", "Velocity", bipolar64, od.velocity * 64.0f, &o.velocity, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number(p + "polyat", "Poly Aftertouch", bipolar64, od.polyAT * 64.0f, &o.polyAT, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.choice(p + "envsource", "Env Source", names({ "Self", "Env 1", "Env 2", "Env 3" }), static_cast<int>(od.envSource),
                 enumPtr(o.envSource), Kind::Enum8, 0);
        if (i > 0) v.toggle(p + "directout", "Direct Out", od.directOut, &o.directOut);
        v.toggle(p + "mute", "Mute", od.mute, &o.mute);
        describeEnv(v, p + "env_", o.env, od.env, false);
        v.endGroup();
    }

    // Algorithm
    v.beginGroup("algo", "Algorithm");
    v.choice("algo_mode", "Algorithm Mode", names({ "Single", "Morph", "Custom" }), static_cast<int>(d.algo.mode), enumPtr(t.algo.mode), Kind::Enum8, 0);
    v.choice("algo_index", "Algorithm", algorithmNames(), d.algo.index, &t.algo.index, Kind::Int, 0);
    for (int s = 0; s < kmi::kNumMorphSlots; ++s)
    {
        const juce::String n(s + 1);
        v.choice("algo_slot" + n, "Morph Slot " + n, morphSlotNames(), d.algo.morphSlots[static_cast<size_t>(s)],
                 &t.algo.morphSlots[static_cast<size_t>(s)], Kind::Int, 0);
    }
    v.number("algo_morph", "Algorithm Morph", Range(1, 8, 0.01f), d.algo.morphPos + 1.0f, &t.algo.morphPos, Kind::Float, 1, -1, "", 2);
    v.endGroup();

    // Digital filter
    {
        auto& f = t.dfilt;
        const auto& fd = d.dfilt;
        v.beginGroup("dfilt", "Digital Filter");
        v.choice("dfilt_type", "Type",
                 names({ "SVF LP-BP-HP", "SVF LP-Notch-HP", "HP Diode", "HP Modular", "HP 12", "BP Modular", "BP 12",
                         "LP Ladder 12", "LP Ladder 24", "LP Fat 12", "LP Fat 24", "LP Gate", "LP Diode", "LP Modular",
                         "LP 12", "LP 1-Pole", "LP 8-Pole", "Vowel" }),
                 static_cast<int>(fd.type), enumPtr(f.type), Kind::Enum8, 0);
        v.number("dfilt_cutoff", "Cutoff", level128, fd.cutoff * 128.0f, &f.cutoff, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.number("dfilt_res", "Resonance", level128, fd.resonance * 128.0f, &f.resonance, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.number("dfilt_morph", "Morph", level128, fd.morph * 128.0f, &f.morph, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.number("dfilt_drive", "Drive", level128, fd.drive * 128.0f, &f.drive, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.toggle("dfilt_drivepost", "Drive Post", fd.drivePost, &f.drivePost);
        v.number("dfilt_envamt", "Env 1 Amount", bipolar64, fd.envAmt * 64.0f, &f.envAmt, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number("dfilt_velenv", "Velocity > Env", bipolar64, fd.velEnv * 64.0f, &f.velEnv, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number("dfilt_polyat", "Poly Aftertouch", bipolar64, fd.polyAT * 64.0f, &f.polyAT, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number("dfilt_keytrack", "Keytrack", keytrack, fd.keytrack * 100.0f, &f.keytrack, Kind::Float, 0.01f, 0, "%", 0);
        v.number("dfilt_lfoamt", "LFO 1 Amount", bipolar64, fd.lfoAmt * 64.0f, &f.lfoAmt, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number("dfilt_level", "Output Level", level128, fd.level * 64.0f, &f.level, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        juce::StringArray vowels;
        for (int i = 0; i < kmi::kNumVowelOrders; ++i) vowels.add(kmi::vowelOrderName(i));
        v.choice("dfilt_vowelorder", "Vowel Order", vowels, fd.vowelOrder, &f.vowelOrder, Kind::Int, 0);
        v.endGroup();
    }

    // Analog filter
    {
        auto& f = t.afilt;
        const auto& fd = d.afilt;
        v.beginGroup("afilt", "Analog Filter");
        v.number("afilt_predrive", "Pre-Drive", level128, fd.preDrive * 128.0f, &f.preDrive, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.number("afilt_cutoff", "Cutoff", level128, fd.cutoff * 128.0f, &f.cutoff, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.number("afilt_res", "Resonance", level128, fd.resonance * 128.0f, &f.resonance, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.number("afilt_envamt", "Env 2 Amount", bipolar64, fd.envAmt * 64.0f, &f.envAmt, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number("afilt_velenv", "Velocity > Env", bipolar64, fd.velEnv * 64.0f, &f.velEnv, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number("afilt_polyat", "Poly Aftertouch", bipolar64, fd.polyAT * 64.0f, &f.polyAT, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number("afilt_keytrack", "Keytrack", keytrack, fd.keytrack * 100.0f, &f.keytrack, Kind::Float, 0.01f, 0, "%", 0);
        v.number("afilt_lfoamt", "LFO 2 Amount", bipolar64, fd.lfoAmt * 64.0f, &f.lfoAmt, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.endGroup();
    }

    // VCA
    {
        auto& a = t.vca;
        const auto& ad = d.vca;
        v.beginGroup("vca", "VCA");
        v.number("vca_oscslevel", "OSCs Level", level128, ad.oscsLevel * 128.0f, &a.oscsLevel, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.number("vca_level", "VCA Level", level128, ad.vcaLevel * 128.0f, &a.vcaLevel, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.number("vca_patchlevel", "Patch Level", level128, ad.patchLevel * 64.0f, &a.patchLevel, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number("vca_lfoamt", "LFO 3 Amount", bipolar64, ad.lfoAmt * 64.0f, &a.lfoAmt, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number("vca_velenv", "Velocity > Env", bipolar64, ad.velEnv * 64.0f, &a.velEnv, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number("vca_polyat", "Poly Aftertouch", bipolar64, ad.polyAT * 64.0f, &a.polyAT, Kind::Float, 1.0f / 64.0f, 0, "", 1);
        v.number("vca_initial", "Initial Level", level128, ad.initialLevel * 128.0f, &a.initialLevel, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.toggle("vca_snap", "Snap", ad.snap, &a.snap);
        v.endGroup();
    }

    // Envelopes 1-5
    for (int k = 0; k < kmi::kNumEnvs; ++k)
    {
        const juce::String n(k + 1);
        v.beginGroup("env" + n, "Env " + n);
        describeEnv(v, "env" + n + "_", t.env[static_cast<size_t>(k)], d.env[static_cast<size_t>(k)], true);
        v.endGroup();
    }

    // LFOs 1-5
    for (int k = 0; k < kmi::kNumLfos; ++k)
    {
        const juce::String n(k + 1), p = "lfo" + n + "_";
        auto& l = t.lfo[static_cast<size_t>(k)];
        const auto& ld = d.lfo[static_cast<size_t>(k)];
        v.beginGroup("lfo" + n, "LFO " + n);
        v.choice(p + "wave", "Wave",
                 names({ "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Pulse 27%", "Pulse 13%", "S&H", "Noise", "Random", "Step" }),
                 static_cast<int>(ld.wave), enumPtr(l.wave), Kind::Enum8, 0);
        v.number(p + "rate", "Rate", skewed(0.0f, 150.0f, 3.0f), ld.rateHz, &l.rateHz, Kind::Float, 1, 0, "Hz", 2);
        v.toggle(p + "bpm", "BPM Sync", ld.bpmSync, &l.bpmSync);
        v.choice(p + "div", "Sync Division", syncNames(), ld.syncDiv, &l.syncDiv, Kind::Int, 0);
        v.choice(p + "trig", "Trig Sync", names({ "Poly", "Single", "Free" }), static_cast<int>(ld.trig), enumPtr(l.trig), Kind::Enum8, 0);
        v.number(p + "delay", "Delay", timeRange(60000.0f), ld.delayMs, &l.delayMs, Kind::Float, 1, 0, "ms", -1);
        v.number(p + "fade", "Fade In", timeRange(36000.0f), ld.fadeMs, &l.fadeMs, Kind::Float, 1, 0, "ms", -1);
        v.number(p + "phase", "Phase", Range(0, 360, 1), ld.phaseDeg, &l.phaseDeg, Kind::Float, 1, 0, "deg", 0);
        v.number(p + "level", "Level", level128, ld.level * 128.0f, &l.level, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        v.number(p + "smooth", "Smooth", Range(0, 127, 1), ld.smooth * 127.0f, &l.smooth, Kind::Float, 1.0f / 127.0f, 0, "", 0);
        v.choice(p + "oneshot", "One-Shot", names({ "Off", "On", "Step" }), static_cast<int>(ld.oneShot), enumPtr(l.oneShot), Kind::Enum8, 0);
        v.number(p + "steps", "Steps", Range(2, 64, 1), static_cast<float>(ld.steps), &l.steps, Kind::Int, 1, 0, "", 0);
        v.number(p + "stagger", "Phase Stagger", Range(0, 360, 1), ld.phaseStagger, &l.phaseStagger, Kind::Float, 1, 0, "deg", 0);
        v.endGroup();
    }

    // Voice
    {
        auto& vp = t.voice;
        const auto& vd = d.voice;
        v.beginGroup("voice", "Voice");
        v.choice("voice_polymode", "Polyphony",
                 names({ "Poly Rotate", "Poly Reassign", "Mono", "Mono Lo", "Mono Hi", "Unison", "Unison Lo", "Unison Hi", "Unison Poly" }),
                 static_cast<int>(vd.polyMode), enumPtr(vp.polyMode), Kind::Enum8, 0);
        v.number("voice_limit", "Voice Limit", Range(1, 16, 1), static_cast<float>(vd.voiceLimit), &vp.voiceLimit, Kind::Int, 1, 0, "", 0);
        v.number("voice_density", "Density", Range(1, 16, 1), static_cast<float>(vd.density), &vp.density, Kind::Int, 1, 0, "", 0);
        v.number("voice_detune", "Unison Detune", Range(0, 127, 1), vd.unisonDetune * 127.0f, &vp.unisonDetune, Kind::Float, 1.0f / 127.0f, 0, "", 0);
        v.number("voice_analogfeel", "Analog Feel", Range(0, 127, 1), vd.analogFeel * 127.0f, &vp.analogFeel, Kind::Float, 1.0f / 127.0f, 0, "", 0);
        v.toggle("voice_randomphase", "Random Phase", vd.randomPhase, &vp.randomPhase);
        v.number("voice_bendrange", "Pitch Bend Range", Range(0, 24, 1), static_cast<float>(vd.pitchBendRange), &vp.pitchBendRange, Kind::Int, 1, 0, "st", 0);
        v.number("voice_vibamt", "Vibrato Amount", Range(0, 12, 0.01f), vd.vibratoAmount, &vp.vibratoAmount, Kind::Float, 1, 0, "st", 2);
        v.toggle("voice_vibbpm", "Vibrato BPM Sync", vd.vibratoBpmSync, &vp.vibratoBpmSync);
        v.number("voice_vibrate", "Vibrato Rate", Range(0.3f, 10.0f, 0.01f), vd.vibratoRateHz, &vp.vibratoRateHz, Kind::Float, 1, 0, "Hz", 2);
        v.choice("voice_vibdiv", "Vibrato Division", syncNames(), vd.vibratoSyncDiv, &vp.vibratoSyncDiv, Kind::Int, 0);
        v.choice("voice_vibwave", "Vibrato Wave", names({ "Sine", "Triangle", "Square", "S&H", "Noise" }),
                 static_cast<int>(vd.vibratoWave), enumPtr(vp.vibratoWave), Kind::Enum8, 0);
        v.choice("voice_glide", "Glide", names({ "Off", "Glide", "Glissando" }), static_cast<int>(vd.glideMode),
                 enumPtr(vp.glideMode), Kind::Enum8, 0);
        v.number("voice_glidetime", "Glide Time", Range(0, 127, 1), vd.glideTime * 127.0f, &vp.glideTime, Kind::Float, 1.0f / 127.0f, 0, "", 0);
        v.toggle("voice_glidelegato", "Glide Legato", vd.glideLegato, &vp.glideLegato);
        v.choice("voice_oversampling", "Oversampling", names({ "1x", "2x" }), d.oversampling - 1, &t.oversampling, Kind::Int, 1);
        v.endGroup();
    }

    // Mod matrix
    {
        const auto srcNames = fromVector(kmi::modSourceNames());
        const auto dstNames = fromVector(kmi::modDestNames());
        v.beginGroup("mod", "Mod Matrix");
        for (int k = 0; k < kmi::kNumModSlots; ++k)
        {
            const juce::String n(k + 1), p = "mod" + n + "_";
            auto& s = t.mod[static_cast<size_t>(k)];
            const auto& sd = d.mod[static_cast<size_t>(k)];
            v.choice(p + "src", "Mod " + n + " Source", srcNames, sd.source, &s.source, Kind::Int, 0);
            v.choice(p + "dst", "Mod " + n + " Destination", dstNames, sd.dest, &s.dest, Kind::Int, 0);
            v.number(p + "depth", "Mod " + n + " Depth", bipolar128, sd.depth * 128.0f, &s.depth, Kind::Float, 1.0f / 128.0f, 0, "", 1);
        }
        v.endGroup();

        // Macros
        v.beginGroup("macro", "Macros");
        for (int m = 0; m < kmi::kNumMacros; ++m)
        {
            const juce::String n(m + 1), p = "macro" + n + "_";
            auto& mc = t.macro[static_cast<size_t>(m)];
            const auto& md = d.macro[static_cast<size_t>(m)];
            v.number(p + "value", "Macro " + n, level128, md.value * 128.0f, &mc.value, Kind::Float, 1.0f / 128.0f, 0, "", 1);
            for (int r = 0; r < kmi::kNumMacroRoutes; ++r)
            {
                const juce::String rn(r + 1), rp = p + "r" + rn + "_";
                auto& route = mc.routes[static_cast<size_t>(r)];
                const auto& rd = md.routes[static_cast<size_t>(r)];
                v.choice(rp + "dst", "Macro " + n + " Dest " + rn, dstNames, rd.dest, &route.dest, Kind::Int, 0);
                v.number(rp + "depth", "Macro " + n + " Depth " + rn, bipolar128, rd.depth * 128.0f, &route.depth, Kind::Float,
                         1.0f / 128.0f, 0, "", 1);
            }
        }
        v.endGroup();
    }
}

// ---- Layout builder ------------------------------------------------------------------------------
struct LayoutBuilder final : Visitor
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    std::unique_ptr<juce::AudioProcessorParameterGroup> group;

    void beginGroup(const juce::String& id, const juce::String& name) override
    {
        group = std::make_unique<juce::AudioProcessorParameterGroup>(id, name, " | ");
    }
    void endGroup() override { layout.add(std::move(group)); }

    template <typename P>
    void add(std::unique_ptr<P> p)
    {
        if (group) group->addChild(std::move(p));
        else layout.add(std::move(p));
    }

    void number(const juce::String& id, const juce::String& name, Range range, float def, void*, Kind kind, float,
                float, const juce::String& unit, int decimals) override
    {
        if (kind == Kind::Int)
        {
            add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ id, 1 }, name, static_cast<int>(range.start),
                                                          static_cast<int>(range.end), static_cast<int>(def)));
            return;
        }
        auto attrs = juce::AudioParameterFloatAttributes().withLabel(unit);
        if (unit == "ms" && decimals < 0)
            attrs = attrs.withStringFromValueFunction([](float v, int) { return formatTime(v); });
        else
            attrs = attrs.withStringFromValueFunction([decimals](float v, int) { return juce::String(v, std::max(0, decimals)); });
        add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id, 1 }, name, range, def, attrs));
    }

    void choice(const juce::String& id, const juce::String& name, const juce::StringArray& items, int def, void*, Kind,
                float) override
    {
        add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id, 1 }, name, items,
                                                         juce::jlimit(0, items.size() - 1, def)));
    }

    void toggle(const juce::String& id, const juce::String& name, bool def, bool*) override
    {
        add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id, 1 }, name, def));
    }
};

// ---- Binder ----------------------------------------------------------------------------------------
struct Binder final : Visitor
{
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<ParameterBinding::Entry>& out;

    Binder(juce::AudioProcessorValueTreeState& s, std::vector<ParameterBinding::Entry>& e) : apvts(s), out(e) {}

    void link(const juce::String& id, void* target, Kind kind, float scale, float offset)
    {
        auto* raw = apvts.getRawParameterValue(id);
        jassert(raw != nullptr);
        if (raw) out.push_back({ raw, target, kind, scale, offset });
    }

    void beginGroup(const juce::String&, const juce::String&) override {}
    void endGroup() override {}
    void number(const juce::String& id, const juce::String&, Range, float, void* target, Kind kind, float scale,
                float offset, const juce::String&, int) override
    {
        link(id, target, kind, scale, offset);
    }
    void choice(const juce::String& id, const juce::String&, const juce::StringArray&, int, void* target, Kind kind,
                float offset) override
    {
        link(id, target, kind, 1.0f, offset);
    }
    void toggle(const juce::String& id, const juce::String&, bool, bool* target) override
    {
        link(id, target, Kind::Bool, 1.0f, 0.0f);
    }
};
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout ParameterBinding::createLayout()
{
    LayoutBuilder builder;
    kmi::Patch scratch = kmi::makeInitPatch();
    const kmi::Patch defaults = kmi::makeInitPatch();
    describe(builder, scratch, defaults);
    return std::move(builder.layout);
}

void ParameterBinding::bind(juce::AudioProcessorValueTreeState& apvts, kmi::Patch& patch)
{
    entries_.clear();
    Binder binder(apvts, entries_);
    const kmi::Patch defaults = kmi::makeInitPatch();
    describe(binder, patch, defaults);
}

void ParameterBinding::apply() const noexcept
{
    for (const Entry& e : entries_)
    {
        const float v = e.source->load(std::memory_order_relaxed);
        switch (e.kind)
        {
            case Kind::Float: *static_cast<float*>(e.target) = v * e.scale + e.offset; break;
            case Kind::Int: *static_cast<int*>(e.target) = static_cast<int>(std::lround(v * e.scale + e.offset)); break;
            case Kind::Bool: *static_cast<bool*>(e.target) = v >= 0.5f; break;
            case Kind::Enum8: *static_cast<uint8_t*>(e.target) = static_cast<uint8_t>(std::lround(v)); break;
            case Kind::EnvLoop:
            {
                const int idx = static_cast<int>(std::lround(v));
                *static_cast<int*>(e.target) = idx <= 0 ? 0 : (idx >= 50 ? kmi::kEnvLoopInfinite : idx + 1);
                break;
            }
        }
    }
}

} // namespace kmiplug
