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

void describeModFx(Visitor& v, const juce::String& p, const juce::String& n, kmi::ModFxParams& t,
                   const kmi::ModFxParams& d)
{
    v.number(p + "rate", n + " Rate", skewed(0.02f, 10.0f, 1.0f), d.rateHz, &t.rateHz, Kind::Float, 1, 0, "Hz", 2);
    v.number(p + "depth", n + " Depth", Range(0, 128, 0.1f), d.depth * 128.0f, &t.depth, Kind::Float, 1.0f / 128.0f, 0, "", 1);
    v.number(p + "offset", n + " Offset", Range(-180, 180, 1), d.offsetDeg, &t.offsetDeg, Kind::Float, 1, 0, "deg", 0);
    v.number(p + "feedback", n + " Feedback", Range(-63, 63, 0.1f), d.feedback * 63.0f, &t.feedback, Kind::Float, 1.0f / 63.0f, 0, "", 1);
    v.toggle(p + "stereo", n + " Stereo", d.stereo, &t.stereo);
    v.number(p + "mix", n + " Dry/Wet", Range(0, 100, 0.1f), d.mix * 100.0f, &t.mix, Kind::Float, 0.01f, 0, "%", 1);
}

void describeInsert(Visitor& v, const juce::String& p, const juce::String& label, kmi::InsertFxParams& t,
                    const kmi::InsertFxParams& d)
{
    const auto mix = Range(0, 100, 0.1f);
    const auto gainDb = Range(-36, 24, 0.1f);
    v.beginGroup(p.dropLastCharacters(1), label);
    v.choice(p + "type", "Type",
             names({ "Bypass", "Chorus", "Flanger", "Rotary", "Phaser", "Lo-Fi", "Tremolo", "EQ", "Compressor", "Distort" }),
             static_cast<int>(d.type), enumPtr(t.type), Kind::Enum8, 0);
    describeModFx(v, p + "chorus_", "Chorus", t.chorus, d.chorus);
    describeModFx(v, p + "flanger_", "Flanger", t.flanger, d.flanger);

    const juce::String ro = p + "rotary_";
    v.number(ro + "woofer", "Rotary Woofer Rate", skewed(0.02f, 10.0f, 1.0f), d.rotary.wooferHz, &t.rotary.wooferHz, Kind::Float, 1, 0, "Hz", 2);
    v.number(ro + "horn", "Rotary Horn Rate", skewed(0.02f, 10.0f, 1.0f), d.rotary.hornHz, &t.rotary.hornHz, Kind::Float, 1, 0, "Hz", 2);
    v.number(ro + "lodepth", "Rotary Low Depth", Range(0, 127, 1), d.rotary.loDepth * 127.0f, &t.rotary.loDepth, Kind::Float, 1.0f / 127.0f, 0, "", 0);
    v.number(ro + "hidepth", "Rotary High Depth", Range(0, 127, 1), d.rotary.hiDepth * 127.0f, &t.rotary.hiDepth, Kind::Float, 1.0f / 127.0f, 0, "", 0);
    v.number(ro + "balance", "Rotary Low/High", Range(-63, 63, 1), d.rotary.balance * 63.0f, &t.rotary.balance, Kind::Float, 1.0f / 63.0f, 0, "", 0);
    v.number(ro + "mix", "Rotary Dry/Wet", mix, d.rotary.mix * 100.0f, &t.rotary.mix, Kind::Float, 0.01f, 0, "%", 1);

    const juce::String ph = p + "phaser_";
    v.number(ph + "rate", "Phaser Rate", skewed(0.02f, 10.0f, 1.0f), d.phaser.rateHz, &t.phaser.rateHz, Kind::Float, 1, 0, "Hz", 2);
    v.number(ph + "feedback", "Phaser Feedback", Range(-63, 63, 0.1f), d.phaser.feedback * 63.0f, &t.phaser.feedback, Kind::Float, 1.0f / 63.0f, 0, "", 1);
    v.number(ph + "depth", "Phaser Depth", Range(0, 127, 1), d.phaser.depth * 127.0f, &t.phaser.depth, Kind::Float, 1.0f / 127.0f, 0, "", 0);
    v.number(ph + "lowfreq", "Phaser Low Freq", Range(0, 127, 1), d.phaser.lowFreq * 127.0f, &t.phaser.lowFreq, Kind::Float, 1.0f / 127.0f, 0, "", 0);
    v.number(ph + "offset", "Phaser Offset", Range(-180, 180, 1), d.phaser.offsetDeg, &t.phaser.offsetDeg, Kind::Float, 1, 0, "deg", 0);
    v.number(ph + "mix", "Phaser Dry/Wet", mix, d.phaser.mix * 100.0f, &t.phaser.mix, Kind::Float, 0.01f, 0, "%", 1);

    const juce::String lf = p + "lofi_";
    v.number(lf + "cutoff", "Lo-Fi Cutoff", skewed(160.0f, 20000.0f, 2000.0f), d.lofi.cutoffHz, &t.lofi.cutoffHz, Kind::Float, 1, 0, "Hz", 0);
    v.number(lf + "res", "Lo-Fi Resonance", Range(1, 12, 0.1f), d.lofi.resonance, &t.lofi.resonance, Kind::Float, 1, 0, "", 1);
    v.choice(lf + "filter", "Lo-Fi Filter", names({ "Thru", "Low Pass", "Radio", "Telephone", "High Pass" }),
             static_cast<int>(d.lofi.filter), enumPtr(t.lofi.filter), Kind::Enum8, 0);
    v.number(lf + "output", "Lo-Fi Output", Range(-6, 36, 0.1f), d.lofi.outputDb, &t.lofi.outputDb, Kind::Float, 1, 0, "dB", 1);
    v.number(lf + "rate", "Lo-Fi Sample Rate", skewed(2756.0f, 44100.0f, 11025.0f), d.lofi.sampleRate, &t.lofi.sampleRate, Kind::Float, 1, 0, "Hz", 0);
    v.number(lf + "mix", "Lo-Fi Dry/Wet", mix, d.lofi.mix * 100.0f, &t.lofi.mix, Kind::Float, 0.01f, 0, "%", 1);

    const juce::String tr = p + "tremolo_";
    v.number(tr + "rate", "Tremolo Rate", skewed(0.02f, 10.0f, 1.0f), d.tremolo.rateHz, &t.tremolo.rateHz, Kind::Float, 1, 0, "Hz", 2);
    v.number(tr + "depth", "Tremolo Depth", Range(0, 128, 0.1f), d.tremolo.depth * 128.0f, &t.tremolo.depth, Kind::Float, 1.0f / 128.0f, 0, "", 1);
    v.toggle(tr + "square", "Tremolo Square", d.tremolo.square, &t.tremolo.square);
    v.number(tr + "phase", "Tremolo L/R Phase", Range(-180, 180, 1), d.tremolo.phaseDeg, &t.tremolo.phaseDeg, Kind::Float, 1, 0, "deg", 0);
    v.number(tr + "pitchmod", "Tremolo Pitch Mod", Range(0, 127, 1), d.tremolo.pitchMod * 127.0f, &t.tremolo.pitchMod, Kind::Float, 1.0f / 127.0f, 0, "", 0);
    v.number(tr + "mix", "Tremolo Dry/Wet", mix, d.tremolo.mix * 100.0f, &t.tremolo.mix, Kind::Float, 0.01f, 0, "%", 1);

    const juce::String eq = p + "eq_";
    v.number(eq + "low", "EQ Low Gain", gainDb, d.eq.lowDb, &t.eq.lowDb, Kind::Float, 1, 0, "dB", 1);
    v.number(eq + "mid", "EQ Mid Gain", gainDb, d.eq.midDb, &t.eq.midDb, Kind::Float, 1, 0, "dB", 1);
    v.number(eq + "high", "EQ High Gain", gainDb, d.eq.highDb, &t.eq.highDb, Kind::Float, 1, 0, "dB", 1);
    v.number(eq + "xlo", "EQ Low Crossover", skewed(32.0f, 2000.0f, 250.0f), d.eq.xoverLoHz, &t.eq.xoverLoHz, Kind::Float, 1, 0, "Hz", 0);
    v.number(eq + "xhi", "EQ High Crossover", skewed(512.0f, 16000.0f, 3000.0f), d.eq.xoverHiHz, &t.eq.xoverHiHz, Kind::Float, 1, 0, "Hz", 0);
    v.number(eq + "mix", "EQ Dry/Wet", mix, d.eq.mix * 100.0f, &t.eq.mix, Kind::Float, 0.01f, 0, "%", 1);

    const juce::String co = p + "comp_";
    v.choice(co + "sidechain", "Comp Sidechain", names({ "Off", "Tempo Duck", "Note Duck" }), static_cast<int>(d.comp.sidechain),
             enumPtr(t.comp.sidechain), Kind::Enum8, 0);
    v.number(co + "ratio", "Comp Ratio", skewed(1.0f, 20.0f, 4.0f), d.comp.ratio, &t.comp.ratio, Kind::Float, 1, 0, ":1", 1);
    v.number(co + "threshold", "Comp Threshold", Range(-64, 0, 0.1f), d.comp.thresholdDb, &t.comp.thresholdDb, Kind::Float, 1, 0, "dB", 1);
    v.number(co + "attack", "Comp Attack", skewed(1.0f, 400.0f, 30.0f), d.comp.attackMs, &t.comp.attackMs, Kind::Float, 1, 0, "ms", 1);
    v.number(co + "release", "Comp Release", skewed(5.0f, 560.0f, 120.0f), d.comp.releaseMs, &t.comp.releaseMs, Kind::Float, 1, 0, "ms", 1);
    v.number(co + "makeup", "Comp Makeup", Range(-12, 24, 0.1f), d.comp.makeupDb, &t.comp.makeupDb, Kind::Float, 1, 0, "dB", 1);
    v.number(co + "mix", "Comp Dry/Wet", mix, d.comp.mix * 100.0f, &t.comp.mix, Kind::Float, 0.01f, 0, "%", 1);

    const juce::String di = p + "distort_";
    v.number(di + "drive", "Distort Drive", Range(0, 128, 0.1f), d.distort.drive * 128.0f, &t.distort.drive, Kind::Float, 1.0f / 128.0f, 0, "", 1);
    v.number(di + "tone", "Distort Tone", Range(-63, 63, 0.1f), d.distort.tone * 63.0f, &t.distort.tone, Kind::Float, 1.0f / 63.0f, 0, "", 1);
    v.number(di + "asym", "Distort Asymmetry", Range(0, 128, 1), d.distort.asym * 128.0f, &t.distort.asym, Kind::Float, 1.0f / 128.0f, 0, "", 0);
    v.number(di + "curve", "Distort Curve", Range(0, 128, 1), d.distort.curve * 128.0f, &t.distort.curve, Kind::Float, 1.0f / 128.0f, 0, "", 0);
    v.number(di + "output", "Distort Output", gainDb, d.distort.outputDb, &t.distort.outputDb, Kind::Float, 1, 0, "dB", 1);
    v.number(di + "mix", "Distort Dry/Wet", mix, d.distort.mix * 100.0f, &t.distort.mix, Kind::Float, 0.01f, 0, "%", 1);
    v.endGroup();
}

void describeFx(Visitor& v, kmi::FxParams& t, const kmi::FxParams& d)
{
    describeInsert(v, "prefx_", "Pre-FX", t.pre, d.pre);

    v.beginGroup("delay", "Delay");
    v.toggle("delay_on", "Delay On", d.delay.enabled, &t.delay.enabled);
    v.choice("delay_type", "Delay Type", names({ "Mono", "Stereo", "Ping-Pong", "L-R-C", "Reverse" }),
             static_cast<int>(d.delay.type), enumPtr(t.delay.type), Kind::Enum8, 0);
    v.number("delay_time", "Delay Time", skewed(1.0f, 3000.0f, 400.0f), d.delay.timeMs, &t.delay.timeMs, Kind::Float, 1, 0, "ms", -1);
    v.toggle("delay_bpm", "Delay BPM Sync", d.delay.bpmSync, &t.delay.bpmSync);
    v.choice("delay_div", "Delay Division", syncNames(), d.delay.syncDiv, &t.delay.syncDiv, Kind::Int, 0);
    v.number("delay_feedback", "Delay Feedback", Range(0, 128, 0.1f), d.delay.feedback * 128.0f, &t.delay.feedback, Kind::Float, 1.0f / 128.0f, 0, "", 1);
    v.number("delay_wettone", "Delay Wet Tone", Range(-64, 64, 0.1f), d.delay.wetTone * 64.0f, &t.delay.wetTone, Kind::Float, 1.0f / 64.0f, 0, "", 1);
    v.number("delay_fbtone", "Delay Feedback Tone", Range(0, 128, 0.1f), d.delay.feedbackTone * 128.0f, &t.delay.feedbackTone, Kind::Float, 1.0f / 128.0f, 0, "", 1);
    v.number("delay_mix", "Delay Dry/Wet", Range(0, 100, 0.1f), d.delay.mix * 100.0f, &t.delay.mix, Kind::Float, 0.01f, 0, "%", 1);
    v.endGroup();

    v.beginGroup("reverb", "Reverb");
    v.toggle("reverb_on", "Reverb On", d.reverb.enabled, &t.reverb.enabled);
    v.choice("reverb_type", "Reverb Type", names({ "Hall", "Plate", "Room", "Cloud" }), static_cast<int>(d.reverb.type),
             enumPtr(t.reverb.type), Kind::Enum8, 0);
    v.number("reverb_predelay", "Reverb Pre-Delay", skewed(0.5f, 250.0f, 30.0f), d.reverb.preDelayMs, &t.reverb.preDelayMs, Kind::Float, 1, 0, "ms", 1);
    v.number("reverb_time", "Reverb Time", skewed(0.12f, 90.0f, 4.0f), d.reverb.timeSec, &t.reverb.timeSec, Kind::Float, 1, 0, "s", 2);
    v.toggle("reverb_freeze", "Reverb Freeze", d.reverb.freeze, &t.reverb.freeze);
    v.number("reverb_tone", "Reverb Tone", Range(-64, 64, 0.1f), d.reverb.tone * 64.0f, &t.reverb.tone, Kind::Float, 1.0f / 64.0f, 0, "", 1);
    v.number("reverb_hidamp", "Reverb Hi Damp", Range(0, 128, 0.1f), d.reverb.hiDamp * 128.0f, &t.reverb.hiDamp, Kind::Float, 1.0f / 128.0f, 0, "", 1);
    v.number("reverb_lodamp", "Reverb Lo Damp", Range(0, 128, 0.1f), d.reverb.loDamp * 128.0f, &t.reverb.loDamp, Kind::Float, 1.0f / 128.0f, 0, "", 1);
    v.number("reverb_mix", "Reverb Dry/Wet", Range(0, 100, 0.1f), d.reverb.mix * 100.0f, &t.reverb.mix, Kind::Float, 0.01f, 0, "%", 1);
    v.endGroup();

    describeInsert(v, "postfx_", "Post-FX", t.post, d.post);
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

    describeFx(v, t.fx, d.fx);

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
