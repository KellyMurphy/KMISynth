#include "kmi/Voice.h"

#include <algorithm>
#include <cmath>

#include "kmi/Dsp.h"
#include "kmi/Wavetables.h"

namespace kmi {

namespace {
constexpr float kPmDepth = 2.0f;         // phase deviation (cycles) at full modulator level
constexpr float kFmDepth = 4.0f;         // frequency deviation (x carrier frequency) at full level
constexpr float kFeedbackDepth = 0.4f;   // self phase-modulation (cycles) at full feedback
constexpr float kPitchModRange = 12.8f;  // semitones at full-scale modulation (5 units = 1 semitone)
constexpr float kStealFadeMs = 1.5f;

inline float velScale(float amount, float velocity) noexcept
{
    if (amount >= 0.0f) return 1.0f - amount + amount * velocity;
    return 1.0f + amount * velocity;
}

inline float glideSeconds(float t) noexcept
{
    t = clampf(t, 0.0f, 1.0f);
    return 10.0f * t * t * t;
}

inline bool usesOwnLfo(const LfoParams& lp) noexcept
{
    return lp.trig == LfoTrig::Poly || (lp.trig == LfoTrig::Free && lp.phaseStagger != 0.0f);
}

// Phase-warp functions for the PW Mod and Phase Distortion modes. ph and the result are in [0, 1].
float pdWarp(OscMode mode, float ph, float amt) noexcept
{
    switch (mode)
    {
        case OscMode::PWMod:
        {
            const float w = 0.5f - 0.49f * amt;
            return ph < w ? ph * 0.5f / w : 0.5f + (ph - w) * 0.5f / (1.0f - w);
        }
        case OscMode::PDSaw:
        {
            // Read rate starts (1+c)x fast and slows to 1/(1+c) by the end of the cycle; W(0)=0, W(1)=1.
            const float c = 15.0f * amt;
            return ph * (1.0f + c) / (1.0f + c * ph);
        }
        case OscMode::PDSquare:
        {
            const float half = ph < 0.5f ? 0.0f : 0.5f;
            const float local = (ph - half) * 2.0f;
            const float w = 0.5f - 0.49f * amt;
            const float warped = local < w ? local * 0.5f / w : 0.5f + (local - w) * 0.5f / (1.0f - w);
            return half + warped * 0.5f;
        }
        case OscMode::PDSawPulse: return pdWarp(OscMode::PDSquare, pdWarp(OscMode::PDSaw, ph, amt), amt);
        default: return ph;
    }
}
} // namespace

ModUsage analyzeModUsage(const Patch& p) noexcept
{
    ModUsage u;
    bool anyOscEnvLevelMod = false, dfEnvMod = false, afEnvMod = false, dfLfoMod = false, afLfoMod = false, vcaLfoMod = false;

    auto markSource = [&](int s) {
        if (s >= src::Env1 && s < src::Env1 + kNumEnvs) u.env[s - src::Env1] = true;
        else if (s >= src::OscEnv1 && s < src::OscEnv1 + kNumOscs) u.oscEnv[s - src::OscEnv1] = true;
        else if (s >= src::Lfo1 && s < src::Lfo1 + kNumLfos) u.lfo[s - src::Lfo1] = true;
        else if (s >= src::Lfo1Uni && s < src::Lfo1Uni + kNumLfos) u.lfo[s - src::Lfo1Uni] = true;
    };
    auto markDest = [&](int d) {
        if (d == dst::AllOscBase + dst::OscEnvLevel || d == dst::CarrierEnvLevel || d == dst::ModulatorEnvLevel)
            anyOscEnvLevelMod = true;
        for (int i = 0; i < kNumOscs; ++i)
            if (d == dst::osc(i, dst::OscEnvLevel)) u.oscEnv[i] = true;
        dfEnvMod |= d == dst::DFiltBase + dst::DfEnvAmt;
        afEnvMod |= d == dst::AFiltBase + dst::AfEnvAmt;
        dfLfoMod |= d == dst::DFiltBase + dst::DfLfoAmt;
        afLfoMod |= d == dst::AFiltBase + dst::AfLfoAmt;
        vcaLfoMod |= d == dst::VcaBase + dst::VcaLfoAmt;
    };

    for (const ModSlot& s : p.mod)
    {
        if (s.source == src::None || s.dest == dst::None) continue;
        markSource(s.source);
        markDest(s.dest);
    }
    for (const MacroParams& m : p.macro)
        for (const ModSlot& r : m.routes)
            if (r.dest != dst::None && r.depth != 0.0f) markDest(r.dest);

    u.env[0] = u.env[0] || p.dfilt.envAmt != 0.0f || dfEnvMod;
    u.env[1] = u.env[1] || p.afilt.envAmt != 0.0f || afEnvMod;
    u.env[2] = true; // VCA
    u.lfo[0] = u.lfo[0] || p.dfilt.lfoAmt != 0.0f || dfLfoMod;
    u.lfo[1] = u.lfo[1] || p.afilt.lfoAmt != 0.0f || afLfoMod;
    u.lfo[2] = u.lfo[2] || p.vca.lfoAmt != 0.0f || vcaLfoMod;

    for (int i = 0; i < kNumOscs; ++i)
    {
        const OscParams& op = p.osc[static_cast<size_t>(i)];
        if (op.envSource == EnvSource::Self)
            u.oscEnv[i] = u.oscEnv[i] || op.envLevel != 0.0f || anyOscEnvLevelMod;
        else
            u.env[static_cast<int>(op.envSource) - 1] = true;
    }
    return u;
}

void Voice::init(int index, uint32_t seed) noexcept
{
    index_ = index;
    rng_.seed(seed);
    for (int k = 0; k < kNumLfos; ++k) lfo_[static_cast<size_t>(k)].reset(seed * 31u + static_cast<uint32_t>(k) + 1u);
}

void Voice::prepare(float sampleRate) noexcept
{
    sampleRate_ = sampleRate;
    ampCoef_ = 1.0f - std::exp(-1.0f / (0.0015f * sampleRate)); // ~1.5 ms "analog" VCA response
    kill();
}

void Voice::kill() noexcept
{
    active_ = gate_ = false;
    sustained = false;
    stealBlocks_ = 0;
    pendingStart_ = false;
    for (auto& e : oscEnv_) e.reset();
    for (auto& e : env_) e.reset();
    filterPrimed_ = false; // the next start() requests a filter-lane reset
    ampSmooth_ = lastAmp_ = 0.0f;
}

void Voice::start(const RenderContext& ctx, int note, float velocity, float unisonSpread, float glideFrom) noexcept
{
    const VoiceParams& vp = ctx.patch->voice;
    note_ = note;
    velocity_ = velocity;
    releaseVelocity_ = 0.0f; // pressure, bend and timbre are set by the engine when it launches the note
    unisonSpread_ = unisonSpread;
    targetPitch_ = static_cast<float>(note);

    const bool glide = vp.glideMode != GlideMode::Off && !vp.glideLegato && glideFrom >= 0.0f &&
                       glideSeconds(vp.glideTime) > 0.0f;
    currentPitch_ = glide ? glideFrom : targetPitch_;
    glideDistance_ = std::fabs(targetPitch_ - currentPitch_);

    doStart(ctx);
}

void Voice::doStart(const RenderContext& ctx) noexcept
{
    const Patch& p = *ctx.patch;
    const bool wasActive = active_;
    active_ = true;
    gate_ = true;
    sustained = false;
    randomPerNote_ = rng_.bipolar();
    driftPitch_ = rng_.bipolar();
    driftCutoff_ = rng_.bipolar();

    if (!wasActive)
    {
        lane_.resetState = true; // the engine clears this voice's lane in its filter bank
        filterPrimed_ = false;
        ampSmooth_ = 0.0f;
    }

    const EnvMods none{};
    for (int i = 0; i < kNumOscs; ++i)
    {
        const OscParams& op = p.osc[static_cast<size_t>(i)];
        OscState& o = osc_[static_cast<size_t>(i)];
        o.phase = p.voice.randomPhase ? rng_.next01() : wrap01(op.phaseDeg / 360.0f);
        o.y = o.yPrev = o.modOut = 0.0f;
        o.baseLevelPrev = o.baseLevel = 0.0f;
        o.level = 0.0f;
        auto& e = oscEnv_[static_cast<size_t>(i)];
        e.update(op.env, none, sampleRate_, ctx.bpm);
        e.noteOn(op.env, velocity_, wasActive, ctx.otherNotesHeld);
    }
    for (int k = 0; k < kNumEnvs; ++k)
    {
        auto& e = env_[static_cast<size_t>(k)];
        e.update(p.env[static_cast<size_t>(k)], none, sampleRate_, ctx.bpm);
        e.noteOn(p.env[static_cast<size_t>(k)], velocity_, wasActive, ctx.otherNotesHeld);
    }
    for (int k = 0; k < kNumLfos; ++k)
    {
        const LfoParams& lp = p.lfo[static_cast<size_t>(k)];
        if (lp.trig != LfoTrig::Poly) continue;
        auto& l = lfo_[static_cast<size_t>(k)];
        l.update(lp, p.lfoSteps[static_cast<size_t>(k)].data(), 1.0f, 0.0f, sampleRate_, ctx.bpm);
        if (lp.oneShot == LfoOneShot::Step) l.advanceStep();
        else l.retrigger(lp, sampleRate_);
    }
    // Primes the first-block level ramps so a fresh note does not fade in from the previous one.
    for (auto& o : osc_) o.baseLevelPrev = -1.0f;
}

void Voice::legatoTo(const RenderContext& ctx, int note, float velocity) noexcept
{
    const Patch& p = *ctx.patch;
    note_ = note;
    velocity_ = velocity;
    targetPitch_ = static_cast<float>(note);
    if (p.voice.glideMode != GlideMode::Off && glideSeconds(p.voice.glideTime) > 0.0f)
        glideDistance_ = std::fabs(targetPitch_ - currentPitch_);
    else
    {
        currentPitch_ = targetPitch_;
        glideDistance_ = 0.0f;
    }
    gate_ = true;
    sustained = false;
    for (int i = 0; i < kNumOscs; ++i)
        oscEnv_[static_cast<size_t>(i)].noteOn(p.osc[static_cast<size_t>(i)].env, velocity, true, true);
    for (int k = 0; k < kNumEnvs; ++k)
        env_[static_cast<size_t>(k)].noteOn(p.env[static_cast<size_t>(k)], velocity, true, true);
}

void Voice::release(float releaseVelocity) noexcept
{
    if (stealBlocks_ > 0 && pendingStart_)
    {
        pendingStart_ = false; // the pending note was released before it started: just fade out
        return;
    }
    releaseVelocity_ = releaseVelocity;
    gate_ = false;
    sustained = false;
    for (auto& e : oscEnv_) e.noteOff();
    for (auto& e : env_) e.noteOff();
}

void Voice::steal(int note, float velocity, float unisonSpread, float glideFrom) noexcept
{
    pendingNote_ = note;
    pendingVelocity_ = velocity;
    pendingSpread_ = unisonSpread;
    pendingGlideFrom_ = glideFrom;
    pendingStart_ = true;
    gate_ = true;
    if (stealBlocks_ == 0)
    {
        const int fadeSamples = static_cast<int>(kStealFadeMs * 0.001f * sampleRate_);
        stealBlocksTotal_ = std::max(1, (fadeSamples + kControlBlock - 1) / kControlBlock);
        stealBlocks_ = stealBlocksTotal_;
    }
}

float Voice::lfoValue(int k, const RenderContext& ctx, int offset) const noexcept
{
    if (usesOwnLfo(ctx.patch->lfo[static_cast<size_t>(k)])) return lfo_[static_cast<size_t>(k)].value();
    return ctx.globalLfo[k] ? ctx.globalLfo[k][offset] : 0.0f;
}

float Voice::sourceValue(int s, const RenderContext& ctx, int offset) const noexcept
{
    const ModState& ms = *ctx.mod;
    if (s >= src::Env1 && s < src::Env1 + kNumEnvs) return env_[static_cast<size_t>(s - src::Env1)].value();
    if (s >= src::OscEnv1 && s < src::OscEnv1 + kNumOscs) return oscEnv_[static_cast<size_t>(s - src::OscEnv1)].value();
    if (s >= src::Lfo1 && s < src::Lfo1 + kNumLfos) return lfoValue(s - src::Lfo1, ctx, offset);
    if (s >= src::Lfo1Uni && s < src::Lfo1Uni + kNumLfos)
    {
        const int k = s - src::Lfo1Uni;
        const float lvl = usesOwnLfo(ctx.patch->lfo[static_cast<size_t>(k)])
                              ? lfo_[static_cast<size_t>(k)].level()
                              : (ctx.globalLfoState[k] ? ctx.globalLfoState[k]->level() : 0.0f);
        return 0.5f * (lfoValue(k, ctx, offset) + lvl);
    }
    if (s >= src::Macro1 && s < src::Macro1 + kNumMacros) return ctx.patch->macro[static_cast<size_t>(s - src::Macro1)].value;
    if (s >= src::Cc0 && s < src::Cc0 + 128) return ms.cc[s - src::Cc0];
    switch (s)
    {
        case src::Keytrack: return (currentPitch_ - 60.0f) / 60.0f;
        case src::Velocity: return velocity_;
        case src::ReleaseVelocity: return releaseVelocity_;
        case src::PolyAT: return polyAT_;
        case src::ChannelAT: return ms.channelAT;
        case src::ModWheel: return ms.modWheel;
        case src::PitchBend: return ms.pitchBend;
        case src::Expression: return ms.expression;
        case src::SustainPedal: return ms.sustain;
        case src::RandomPerNote: return randomPerNote_;
        case src::Timbre: return timbre_;
        default: return 0.0f; // VoiceMod (Phase 2), None
    }
}

void Voice::computeModulation(const RenderContext& ctx, int offset) noexcept
{
    std::fill(std::begin(dst_), std::end(dst_), 0.0f);
    const Patch& p = *ctx.patch;

    // Pass 1: routes that modulate other routes' depth.
    float depthOffset[kNumModSlots] = {};
    for (const ModSlot& s : p.mod)
        if (s.source != src::None && dst::isModDepth(s.dest) && s.depth != 0.0f)
            depthOffset[s.dest - dst::ModDepthBase] += sourceValue(s.source, ctx, offset) * s.depth;
    for (const MacroParams& m : p.macro)
    {
        if (m.value == 0.0f) continue;
        for (const ModSlot& r : m.routes)
            if (dst::isModDepth(r.dest)) depthOffset[r.dest - dst::ModDepthBase] += m.value * r.depth;
    }

    // Pass 2: everything else.
    for (int k = 0; k < kNumModSlots; ++k)
    {
        const ModSlot& s = p.mod[static_cast<size_t>(k)];
        if (s.source == src::None || s.dest <= dst::None || s.dest >= dst::Count || dst::isModDepth(s.dest)) continue;
        const float depth = clampf(s.depth + depthOffset[k], -1.0f, 1.0f);
        if (depth == 0.0f) continue;
        dst_[s.dest] += sourceValue(s.source, ctx, offset) * depth;
    }
    for (const MacroParams& m : p.macro)
    {
        if (m.value == 0.0f) continue;
        for (const ModSlot& r : m.routes)
            if (r.dest > dst::None && r.dest < dst::Count && !dst::isModDepth(r.dest)) dst_[r.dest] += m.value * r.depth;
    }
}

void Voice::prepareControlBlock(const RenderContext& ctx, int offset, int len) noexcept
{
    const Patch& p = *ctx.patch;
    const VoiceParams& vp = p.voice;
    const ModState& ms = *ctx.mod;
    const float sr = sampleRate_;
    const auto& bank = WavetableBank::instance();

    computeModulation(ctx, offset);

    // Algorithm (per-voice only when the morph position is modulated).
    const float morphMod = dst_[dst::AlgoMorph];
    if (p.algo.mode == AlgoMode::Morph && morphMod != 0.0f) resolveAlgorithm(p, morphMod * 7.0f, algo_);
    else algo_ = *ctx.algorithm;
    // Direct Out (Osc 2-8) adds an oscillator to the output on top of any algorithm.
    for (int i = 1; i < kNumOscs; ++i)
        if (p.osc[static_cast<size_t>(i)].directOut && algo_.out[i] < 1.0f) algo_.out[i] = 1.0f;

    // Glide (constant time per glide, independent of distance).
    if (currentPitch_ != targetPitch_)
    {
        const float secs = glideSeconds(vp.glideTime + dst_[dst::VoiceBase + dst::VoiceGlideTime]);
        const float step = secs > 0.0f ? glideDistance_ * static_cast<float>(len) / (secs * sr) : 1.0e9f;
        const float diff = targetPitch_ - currentPitch_;
        currentPitch_ = std::fabs(diff) <= step ? targetPitch_ : currentPitch_ + (diff > 0.0f ? step : -step);
    }
    const float pitchBase = vp.glideMode == GlideMode::Glissando ? std::round(currentPitch_) : currentPitch_;

    const float vib = ctx.vibrato ? ctx.vibrato[offset] : 0.0f;
    const float vibAmt = clampf(vp.vibratoAmount + dst_[dst::VoiceBase + dst::VoiceVibratoAmt] * 12.0f, 0.0f, 12.0f) * ms.modWheel;
    const float detune = unisonSpread_ * clampf(vp.unisonDetune + dst_[dst::VoiceBase + dst::VoiceUnisonDetune], 0.0f, 1.0f) * 0.5f;
    const float played = pitchBase + ms.pitchBend * static_cast<float>(vp.pitchBendRange) +
                         (vp.mpe ? noteBend_ * static_cast<float>(vp.mpeBendRange) : 0.0f) + vib * vibAmt + detune +
                         driftPitch_ * vp.analogFeel * 0.12f;

    // Oscillators.
    for (int i = 0; i < kNumOscs; ++i)
    {
        const OscParams& op = p.osc[static_cast<size_t>(i)];
        OscState& o = osc_[static_cast<size_t>(i)];
        const bool carrier = algo_.out[i] > 0.0f;
        const bool modulator = algo_.isModulator(i);
        o.active = (carrier || modulator) && !op.mute;

        const float lvlMod = dst_[dst::osc(i, dst::OscLevel)] + dst_[dst::AllOscBase + dst::OscLevel] +
                             (carrier ? dst_[dst::CarrierLevel] : 0.0f) + (modulator ? dst_[dst::ModulatorLevel] : 0.0f);
        const float envMod = dst_[dst::osc(i, dst::OscEnvLevel)] + dst_[dst::AllOscBase + dst::OscEnvLevel] +
                             (carrier ? dst_[dst::CarrierEnvLevel] : 0.0f) + (modulator ? dst_[dst::ModulatorEnvLevel] : 0.0f);
        const float va = std::fabs(op.velocity);
        const float velF = 1.0f - va + va * (op.velocity >= 0.0f ? velocity_ : 1.0f - velocity_);
        o.baseLevel = clampf(op.initLevel * velF + op.polyAT * polyAT_ + lvlMod, -1.0f, 2.0f);
        if (o.baseLevelPrev < -0.5f) o.baseLevelPrev = o.baseLevel; // first block after a start: no ramp
        o.envLevel = clampf(op.envLevel + envMod, -1.0f, 1.0f);
        const float fb = clampf(op.feedback + dst_[dst::osc(i, dst::OscFeedback)] + dst_[dst::AllOscBase + dst::OscFeedback], 0.0f, 1.0f);
        o.feedback = fb * fb * kFeedbackDepth;
        o.invert = op.invert;
        if (!o.active)
        {
            o.modOut = o.y = o.yPrev = 0.0f;
            o.level = 0.0f;
            continue; // no pitch / table work for silent, unconnected oscillators
        }

        const float semis = (dst_[dst::osc(i, dst::OscPitch)] + dst_[dst::AllOscBase + dst::OscPitch]) * kPitchModRange +
                            (dst_[dst::osc(i, dst::OscFine)] + dst_[dst::AllOscBase + dst::OscFine]) * 0.5f;
        const float tracked = 60.0f + (played - 60.0f) * op.keytrack;
        float hz;
        switch (op.pitchMode)
        {
            case PitchMode::Ratio:
                hz = midiToHz(tracked) * op.ratio * (1.0f + op.ratioFine * 0.01f) * std::exp2(semis / 12.0f);
                break;
            case PitchMode::Frequency:
                hz = (op.freqHz + op.freqFine) * std::exp2(semis / 12.0f);
                break;
            default:
                hz = midiToHz(tracked + op.semitone + op.cents * 0.01f + semis);
                break;
        }
        o.inc = clampf(hz / sr, -0.49f, 0.49f);
        o.table = bank.table(op.wave, WavetableBank::mipForIncrement(o.inc));
    }

    // Connection lists per target oscillator.
    for (int c = 0; c < kNumOscs; ++c)
    {
        int n = 0;
        for (int m = 0; m < kNumOscs; ++m)
        {
            const float w = algo_.conn[m][c];
            if (w > 0.0f && m != c && osc_[static_cast<size_t>(m)].active)
                conns_[static_cast<size_t>(c)][static_cast<size_t>(n++)] = { static_cast<uint8_t>(m), p.osc[static_cast<size_t>(m)].mode, w };
        }
        numConns_[static_cast<size_t>(c)] = n;
    }

    // Envelopes (only the ones the patch reads; unused ones are paused, see ModUsage).
    auto timeMul = [](float d) { return d != 0.0f ? std::exp2(d * 5.0f) : 1.0f; };
    for (int k = 0; k < kNumEnvs; ++k)
    {
        if (!ctx.usage.env[k]) continue;
        EnvMods m;
        m.attackMul = timeMul(dst_[dst::env(k, dst::EnvAttack)]);
        m.decayMul = timeMul(dst_[dst::env(k, dst::EnvDecay)]);
        m.releaseMul = timeMul(dst_[dst::env(k, dst::EnvRelease)]);
        m.sustainAdd = dst_[dst::env(k, dst::EnvSustain)];
        m.levelAdd = dst_[dst::env(k, dst::EnvLevel)];
        env_[static_cast<size_t>(k)].update(p.env[static_cast<size_t>(k)], m, sr, ctx.bpm);
    }
    const EnvMods none{};
    for (int i = 0; i < kNumOscs; ++i)
        if (ctx.usage.oscEnv[i] && osc_[static_cast<size_t>(i)].active)
            oscEnv_[static_cast<size_t>(i)].update(p.osc[static_cast<size_t>(i)].env, none, sr, ctx.bpm);

    // Per-voice LFOs.
    for (int k = 0; k < kNumLfos; ++k)
    {
        const LfoParams& lp = p.lfo[static_cast<size_t>(k)];
        if (!usesOwnLfo(lp)) continue;
        auto& l = lfo_[static_cast<size_t>(k)];
        l.update(lp, p.lfoSteps[static_cast<size_t>(k)].data(), std::exp2(dst_[dst::lfo(k, dst::LfoRate)] * 5.0f),
                 dst_[dst::lfo(k, dst::LfoLevel)], sr, ctx.bpm);
        if (lp.trig == LfoTrig::Free && ctx.globalLfoState[k])
        {
            const float master = ctx.globalLfoPhaseStart[k] + ctx.globalLfoState[k]->increment() * static_cast<float>(offset);
            l.followPhase(master, static_cast<float>(index_) * lp.phaseStagger / 360.0f);
        }
    }

    // Filters (targets reached by the end of this block; g ramps per sample).
    const float lfo1 = lfoValue(0, ctx, offset), lfo2 = lfoValue(1, ctx, offset);
    const float octFromC2 = (currentPitch_ - 36.0f) / 12.0f;
    const float drift = driftCutoff_ * vp.analogFeel * 0.02f;

    const DigitalFilterParams& df = p.dfilt;
    const float dfCut = df.cutoff + df.keytrack * octFromC2 * kCutoffPerOctave +
                        env_[0].value() * clampf(df.envAmt + dst_[dst::DFiltBase + dst::DfEnvAmt], -1.0f, 1.0f) * velScale(df.velEnv, velocity_) +
                        lfo1 * clampf(df.lfoAmt + dst_[dst::DFiltBase + dst::DfLfoAmt], -1.0f, 1.0f) + df.polyAT * polyAT_ +
                        dst_[dst::DFiltBase + dst::DfCutoff] + drift;
    dfGTarget_ = hzToG(cutoffToHz(dfCut, sr), sr);
    DigitalFilterBlock blk;
    blk.type = df.type;
    blk.resonance = clampf(df.resonance + dst_[dst::DFiltBase + dst::DfResonance], 0.0f, 1.0f);
    blk.morph = clampf(df.morph + dst_[dst::DFiltBase + dst::DfMorph], 0.0f, 1.0f);
    blk.drive = clampf(df.drive + dst_[dst::DFiltBase + dst::DfDrive], 0.0f, 1.0f);
    blk.drivePost = df.drivePost;
    blk.cutoffNorm = clampf(dfCut, 0.0f, 1.0f);
    blk.vowelOrder = df.vowelOrder;
    computeDigitalLane(blk, sr, lane_);
    lane_.level = std::max(0.0f, df.level + dst_[dst::DFiltBase + dst::DfLevel] * 2.0f);

    const AnalogFilterParams& af = p.afilt;
    const float afCut = af.cutoff + af.keytrack * octFromC2 * kCutoffPerOctave +
                        env_[1].value() * clampf(af.envAmt + dst_[dst::AFiltBase + dst::AfEnvAmt], -1.0f, 1.0f) * velScale(af.velEnv, velocity_) +
                        lfo2 * clampf(af.lfoAmt + dst_[dst::AFiltBase + dst::AfLfoAmt], -1.0f, 1.0f) + af.polyAT * polyAT_ +
                        dst_[dst::AFiltBase + dst::AfCutoff] + drift;
    afGTarget_ = hzToG(cutoffToHz(afCut, sr), sr);
    computeAnalogLane(clampf(af.resonance + dst_[dst::AFiltBase + dst::AfResonance], 0.0f, 1.0f),
                      clampf(af.preDrive + dst_[dst::AFiltBase + dst::AfPreDrive], 0.0f, 1.0f), lane_);

    if (!filterPrimed_)
    {
        dfG_ = dfGTarget_;
        afG_ = afGTarget_;
        filterPrimed_ = true;
    }
    // Cutoff ramps for this block; the filter bank interpolates per sample.
    lane_.gFrom = dfG_;
    lane_.gTo = dfGTarget_;
    lane_.aGFrom = afG_;
    lane_.aGTo = afGTarget_;
    dfG_ = dfGTarget_;
    afG_ = afGTarget_;

    // VCA and pan.
    const VcaParams& v = p.vca;
    vcaLevel_ = clampf(v.vcaLevel + dst_[dst::VcaBase + dst::VcaLevel], 0.0f, 1.0f);
    vcaLfoAmt_ = clampf(v.lfoAmt + dst_[dst::VcaBase + dst::VcaLfoAmt], -1.0f, 1.0f);
    vcaVelScale_ = velScale(v.velEnv, velocity_);
    vcaPolyAT_ = v.polyAT * polyAT_;
    vcaInitial_ = v.initialLevel;
    oscsLevel_ = v.oscsLevel;

    const float pan = clampf(unisonSpread_ * 0.8f + dst_[dst::VoiceBase + dst::VoicePan], -1.0f, 1.0f);
    if (pan != lastPan_)
    {
        lastPan_ = pan;
        panL_ = std::cos((pan + 1.0f) * kPi * 0.25f);
        panR_ = std::sin((pan + 1.0f) * kPi * 0.25f);
    }
}

void Voice::renderBlock(const RenderContext& ctx, int off, int len, float* pre, float* gainL, float* gainR,
                        int stride) noexcept
{
    if (!active_) return;
    const Patch& p = *ctx.patch;

    const ModUsage& use = ctx.usage;
    bool ownLfo[kNumLfos];
    for (int k = 0; k < kNumLfos; ++k) ownLfo[k] = usesOwnLfo(p.lfo[static_cast<size_t>(k)]);
    EnvSource envSrc[kNumOscs];
    for (int i = 0; i < kNumOscs; ++i) envSrc[i] = p.osc[static_cast<size_t>(i)].envSource;

    {
        // A steal fade has finished: start the pending note from a clean state.
        if (stealBlocks_ == 0 && pendingStart_)
        {
            pendingStart_ = false;
            active_ = false;
            start(ctx, pendingNote_, pendingVelocity_, pendingSpread_, pendingGlideFrom_);
        }

        float fadeA = 1.0f, fadeB = 1.0f;
        if (stealBlocks_ > 0)
        {
            fadeA = static_cast<float>(stealBlocks_) / static_cast<float>(stealBlocksTotal_);
            fadeB = static_cast<float>(stealBlocks_ - 1) / static_cast<float>(stealBlocksTotal_);
        }

        prepareControlBlock(ctx, off, len);

        const float invLen = 1.0f / static_cast<float>(len);
        const float fadeStep = (fadeB - fadeA) * invLen;
        float fade = fadeA;
        float levelStep[kNumOscs], level[kNumOscs];
        for (int i = 0; i < kNumOscs; ++i)
        {
            const OscState& o = osc_[static_cast<size_t>(i)];
            level[i] = o.baseLevelPrev;
            levelStep[i] = (o.baseLevel - o.baseLevelPrev) * invLen;
        }

        for (int s = 0; s < len; ++s)
        {
            float oscEnvVal[kNumOscs];
            for (int i = 0; i < kNumOscs; ++i)
                oscEnvVal[i] = use.oscEnv[i] && osc_[static_cast<size_t>(i)].active ? oscEnv_[static_cast<size_t>(i)].tick() : 0.0f;
            float e[kNumEnvs];
            for (int k = 0; k < kNumEnvs; ++k)
                e[k] = use.env[k] ? env_[static_cast<size_t>(k)].tick() : env_[static_cast<size_t>(k)].value();
            float lfoNow[kNumLfos];
            for (int k = 0; k < kNumLfos; ++k)
            {
                if (!use.lfo[k]) lfoNow[k] = 0.0f;
                else if (ownLfo[k]) lfoNow[k] = lfo_[static_cast<size_t>(k)].tick();
                else lfoNow[k] = ctx.globalLfo[k] ? ctx.globalLfo[k][off + s] : 0.0f;
            }

            // Oscillators, highest first: a modulator with a higher index is read from this sample,
            // a lower index (only possible in custom algorithms) from the previous sample.
            float carrierSum = 0.0f;
            for (int i = kNumOscs - 1; i >= 0; --i)
            {
                OscState& o = osc_[static_cast<size_t>(i)];
                level[i] += levelStep[i];
                if (!o.active) continue;

                const EnvSource es = envSrc[i];
                const float envV = es == EnvSource::Self ? oscEnvVal[i] : e[static_cast<int>(es) - 1];
                o.level = clampf(level[i] + o.envLevel * envV, 0.0f, 1.0f);

                float pm = 0.0f, fm = 0.0f, warp = 0.0f;
                int syncSrc = -1;
                float syncAmt = 0.0f;
                const int nc = numConns_[static_cast<size_t>(i)];
                for (int c = 0; c < nc; ++c)
                {
                    const Conn& cn = conns_[static_cast<size_t>(i)][static_cast<size_t>(c)];
                    const OscState& m = osc_[cn.src];
                    switch (cn.mode)
                    {
                        case OscMode::PhaseMod: pm += m.modOut * cn.weight; break;
                        case OscMode::FreqMod: fm += m.modOut * cn.weight; break;
                        case OscMode::HardSync:
                            syncSrc = cn.src;
                            syncAmt = m.level * cn.weight;
                            break;
                        default:
                        {
                            const float r = std::fabs(o.inc) > 1.0e-9f ? m.inc / o.inc : 1.0f;
                            if (std::fabs(r) > 1.0e-6f)
                                warp += (pdWarp(cn.mode, m.phase, m.level * cn.weight) - m.phase) / r;
                            break;
                        }
                    }
                }

                o.phase += o.inc * (1.0f + kFmDepth * fm);
                o.phase -= fastFloor(o.phase);

                float rp = o.phase;
                if (syncSrc >= 0)
                {
                    const OscState& m = osc_[static_cast<size_t>(syncSrc)];
                    if (std::fabs(m.inc) > 1.0e-9f)
                        rp = wrap01((1.0f + 7.0f * syncAmt) * m.phase * o.inc / m.inc);
                }
                rp = wrap01(rp + warp + pm * kPmDepth + o.feedback * 0.5f * (o.y + o.yPrev));

                float w = WavetableBank::read(o.table, rp);
                if (o.invert) w = -w;
                o.yPrev = o.y;
                o.y = w;
                o.modOut = w * o.level * o.level;
                const float outW = algo_.out[i];
                if (outW > 0.0f) carrierSum += w * o.level * outW;
            }

            // Filters run in the engine's 4-voice bank; hand over the pre-filter signal.
            pre[s * stride] = carrierSum * oscsLevel_;

            // VCA.
            float amp = e[2] * vcaVelScale_;
            const float l3 = vcaLfoAmt_ >= 0.0f ? lfoNow[2] : -lfoNow[2];
            amp *= 1.0f - std::fabs(vcaLfoAmt_) * 0.5f * (1.0f - l3);
            amp = clampf(amp + vcaPolyAT_, 0.0f, 1.0f);
            if (!p.vca.snap)
            {
                ampSmooth_ += (amp - ampSmooth_) * ampCoef_;
                amp = ampSmooth_;
            }
            lastAmp_ = amp;
            fade += fadeStep;
            const float gain = (amp + vcaInitial_) * vcaLevel_ * fade;
            gainL[s * stride] = gain * panL_;
            gainR[s * stride] = gain * panR_;
        }

        for (auto& o : osc_) o.baseLevelPrev = o.baseLevel;

        if (stealBlocks_ > 0)
        {
            --stealBlocks_;
            if (stealBlocks_ == 0 && !pendingStart_)
            {
                kill();
                return;
            }
        }
        else if (env_[2].isIdle() && vcaInitial_ <= 0.0f && !pendingStart_)
        {
            kill();
            return;
        }
    }
}

} // namespace kmi
