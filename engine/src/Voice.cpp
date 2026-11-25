#include "kmi/Voice.h"

#include <algorithm>
#include <cmath>

#include "kmi/Dsp.h"
#include "kmi/Simd.h"
#include "kmi/Tuning.h"
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

// Keyscale: amplitude falls with distance from the breakpoint (full depth over 4 octaves).
// Curve > 0 drops fast near the breakpoint (log), < 0 slowly (exp), 0 linear.
float keyscaleGain(const OscParams& op, float pitch) noexcept
{
    float d = pitch - static_cast<float>(op.keyscaleBreak);
    const bool below = d < 0.0f;
    const float depth = below ? op.keyscaleLoDepth : op.keyscaleHiDepth;
    if (depth <= 0.0f) return 1.0f;
    const float curve = below ? op.keyscaleLoCurve : op.keyscaleHiCurve;
    const float x = clampf(std::fabs(d) / 48.0f, 0.0f, 1.0f);
    const float shaped = curve >= 0.0f ? 1.0f - std::pow(1.0f - x, 1.0f + 3.0f * curve) : std::pow(x, 1.0f - 3.0f * curve);
    return clampf(1.0f - depth * shaped, 0.0f, 1.0f);
}

// Equal-power side gain for an oscillator pan in Osc Stereo, 1 at centre.
float sideGain(float pan, int side) noexcept
{
    const float a = (clampf(pan, -1.0f, 1.0f) + 1.0f) * kPi * 0.25f;
    return 1.41421356f * (side < 0 ? std::cos(a) : std::sin(a));
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

    // Envelope trigger sources that follow LFO cycles keep those LFOs running.
    auto markTrig = [&](const EnvParams& e) {
        for (EnvTrig t : e.trig)
            if (t >= EnvTrig::Lfo1 && t <= EnvTrig::Lfo5)
            {
                const int k = static_cast<int>(t) - static_cast<int>(EnvTrig::Lfo1);
                u.trigLfo[k] = u.lfo[k] = true;
            }
    };
    for (const auto& e : p.env) markTrig(e);
    for (const auto& o : p.osc) markTrig(o.env);

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
    targetPitch_ = ctx.tuning ? ctx.tuning[note & 127] : static_cast<float>(note);

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
        e.noteOn(op.env, velocity_, wasActive, ctx.otherNotesHeld, op.env.hasTrigger(EnvTrig::NoteOn));
    }
    for (int k = 0; k < kNumEnvs; ++k)
    {
        auto& e = env_[static_cast<size_t>(k)];
        const EnvParams& ep = p.env[static_cast<size_t>(k)];
        e.update(ep, none, sampleRate_, ctx.bpm);
        e.noteOn(ep, velocity_, wasActive, ctx.otherNotesHeld, ep.hasTrigger(EnvTrig::NoteOn));
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
    // LFO-cycle triggers count from here.
    for (int k = 0; k < kNumLfos; ++k) lfoCyclesSeen_[k] = lfoCycles(k, ctx);
}

uint32_t Voice::lfoCycles(int k, const RenderContext& ctx) const noexcept
{
    if (usesOwnLfo(ctx.patch->lfo[static_cast<size_t>(k)])) return lfo_[static_cast<size_t>(k)].cycles();
    return ctx.globalLfoState[k] ? ctx.globalLfoState[k]->cycles() : 0u;
}

void Voice::triggerEnvelopes(const Patch& p, EnvTrig t) noexcept
{
    if (!active_ || !gate_) return;
    for (int i = 0; i < kNumOscs; ++i)
        if (p.osc[static_cast<size_t>(i)].env.hasTrigger(t)) oscEnv_[static_cast<size_t>(i)].trigger();
    for (int k = 0; k < kNumEnvs; ++k)
        if (p.env[static_cast<size_t>(k)].hasTrigger(t)) env_[static_cast<size_t>(k)].trigger();
}

void Voice::legatoTo(const RenderContext& ctx, int note, float velocity) noexcept
{
    const Patch& p = *ctx.patch;
    note_ = note;
    velocity_ = velocity;
    targetPitch_ = ctx.tuning ? ctx.tuning[note & 127] : static_cast<float>(note);
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
    {
        const EnvParams& ep = p.osc[static_cast<size_t>(i)].env;
        oscEnv_[static_cast<size_t>(i)].noteOn(ep, velocity, true, true, ep.hasTrigger(EnvTrig::NoteOn));
    }
    for (int k = 0; k < kNumEnvs; ++k)
    {
        const EnvParams& ep = p.env[static_cast<size_t>(k)];
        env_[static_cast<size_t>(k)].noteOn(ep, velocity, true, true, ep.hasTrigger(EnvTrig::NoteOn));
    }
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
        case src::VoiceMod: return ctx.patch->voice.voiceMod[static_cast<size_t>(index_ % kNumVoiceMods)];
        case src::VoiceModUni: return 0.5f * (ctx.patch->voice.voiceMod[static_cast<size_t>(index_ % kNumVoiceMods)] + 1.0f);
        default: return 0.0f; // None
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

namespace {
// A wave's mip table: built-in waves from the shared bank, User 1-8 from the instance (silent when empty).
const float* waveTable(const RenderContext& ctx, int wave, int mip) noexcept
{
    static const float kSilence[WavetableBank::kMipFloats] = {};
    if (WavetableBank::isUserWave(wave))
    {
        const float* t = ctx.userWaves ? ctx.userWaves->table(wave - WavetableBank::kUserWaveFirst, mip) : nullptr;
        return t ? t : kSilence;
    }
    return WavetableBank::instance().table(wave, mip);
}
} // namespace

void Voice::prepareControlBlock(const RenderContext& ctx, int offset, int len) noexcept
{
    const Patch& p = *ctx.patch;
    const VoiceParams& vp = p.voice;
    const ModState& ms = *ctx.mod;
    const float sr = sampleRate_;
    const auto& bank = WavetableBank::instance();

    computeModulation(ctx, offset);

    // Envelope triggers from LFO cycles (detected per control block).
    for (int k = 0; k < kNumLfos; ++k)
    {
        if (!ctx.usage.trigLfo[k]) continue;
        const uint32_t c = lfoCycles(k, ctx);
        if (c != lfoCyclesSeen_[k])
        {
            lfoCyclesSeen_[k] = c;
            triggerEnvelopes(p, static_cast<EnvTrig>(static_cast<int>(EnvTrig::Lfo1) + k));
        }
    }

    // Bit depth reduction and Vintage Digital.
    bitScale_ = vp.bitDepth < 24 ? std::exp2(static_cast<float>(std::clamp(vp.bitDepth, 4, 24) - 1)) : 0.0f;
    vintage_ = vp.vintage;
    const bool stereo = vp.oscStereo && stereoSide != 0;

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
    float pitchBase = currentPitch_;
    if (vp.glideMode == GlideMode::Glissando) // step through the scale's notes (in the current tuning)
    {
        const int n = snapToScale(std::clamp(static_cast<int>(std::lround(currentPitch_)), 0, 127), ctx.scaleMask);
        pitchBase = ctx.tuning && ctx.tuning[n] >= 0.0f ? ctx.tuning[n] : static_cast<float>(n);
    }

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
        keyscale_[i] = keyscaleGain(op, currentPitch_);
        carrierSide_[i] = modSide_[i] = 1.0f;
        if (stereo)
        {
            const float g = sideGain(op.pan + dst_[dst::osc(i, dst::OscPan)] + dst_[dst::AllOscBase + dst::OscPan], stereoSide);
            if (vp.oscPanTarget != OscPanTarget::Modulators) carrierSide_[i] = g;
            if (vp.oscPanTarget != OscPanTarget::Carriers) modSide_[i] = g;
        }
        if (!o.active)
        {
            o.modOut = o.y = o.yPrev = 0.0f;
            o.level = 0.0f;
            continue; // no pitch / table work for silent, unconnected oscillators
        }

        const float semis = (dst_[dst::osc(i, dst::OscPitch)] + dst_[dst::AllOscBase + dst::OscPitch]) * kPitchModRange +
                            (dst_[dst::osc(i, dst::OscFine)] + dst_[dst::AllOscBase + dst::OscFine]) * 0.5f +
                            (stereo && carrier ? static_cast<float>(stereoSide) * vp.carrierDetune * 0.005f : 0.0f);
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
        const int mip = vintage_ ? 0 : WavetableBank::mipForIncrement(o.inc); // Vintage: no band-limiting
        o.table2 = nullptr;
        o.tableMix = 0.0f;
        if (op.scanRange > 0)
        {
            const float scan = clampf(op.scan + dst_[dst::OscScanBase + i] + dst_[dst::AllOscScan], 0.0f, 1.0f);
            const float pos = std::fmin(static_cast<float>(bank.menuPosition(op.wave)) + scan * static_cast<float>(op.scanRange),
                                        static_cast<float>(bank.menuSize() - 1));
            const int p0 = static_cast<int>(pos);
            o.table = waveTable(ctx, bank.waveAtMenuPosition(p0), mip);
            o.tableMix = pos - static_cast<float>(p0);
            if (o.tableMix > 1.0e-4f) o.table2 = waveTable(ctx, bank.waveAtMenuPosition(p0 + 1), mip);
        }
        else
            o.table = waveTable(ctx, op.wave, mip);
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

    const float pan = stereo ? static_cast<float>(stereoSide)
                             : clampf(unisonSpread_ * 0.8f + pannerPos * vp.pannerWidth + dst_[dst::VoiceBase + dst::VoicePan], -1.0f, 1.0f);
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
    if (!beginBlock(ctx, off, len, gainL, gainR, stride)) return;
    renderOscScalar(ctx, len, pre, stride);
    endBlock(ctx);
}

bool Voice::beginBlock(const RenderContext& ctx, int off, int len, float* gainL, float* gainR, int stride) noexcept
{
    if (!active_) return false;
    const Patch& p = *ctx.patch;
    const ModUsage& use = ctx.usage;

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

    bool ownLfo[kNumLfos];
    for (int k = 0; k < kNumLfos; ++k) ownLfo[k] = usesOwnLfo(p.lfo[static_cast<size_t>(k)]);

    // Modulators for the whole block: oscillator envelope sources into envBuf_, VCA gain into gainL/R.
    const float fadeStep = (fadeB - fadeA) / static_cast<float>(len);
    float fade = fadeA;
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
        for (int i = 0; i < kNumOscs; ++i)
        {
            const EnvSource es = p.osc[static_cast<size_t>(i)].envSource;
            envBuf_[i][s] = es == EnvSource::Self ? oscEnvVal[i] : e[static_cast<int>(es) - 1];
        }

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
    return true;
}

void Voice::endBlock(const RenderContext& ctx) noexcept
{
    for (auto& o : osc_) o.baseLevelPrev = o.baseLevel;
    if (stealBlocks_ > 0)
    {
        --stealBlocks_;
        if (stealBlocks_ == 0 && !pendingStart_) kill();
    }
    else if (env_[2].isIdle() && vcaInitial_ <= 0.0f && !pendingStart_ &&
             !(gate_ && !ctx.patch->env[2].hasTrigger(EnvTrig::NoteOn))) // VCA envelope waiting for a trigger
    {
        kill();
    }
}

// Reference oscillator kernel (one voice). The engine normally runs renderOscBank; tests compare the two.
void Voice::renderOscScalar(const RenderContext& ctx, int len, float* pre, int stride) noexcept
{
    const Patch& p = *ctx.patch;
    const float invLen = 1.0f / static_cast<float>(len);
    float levelStep[kNumOscs], level[kNumOscs];
    for (int i = 0; i < kNumOscs; ++i)
    {
        const OscState& o = osc_[static_cast<size_t>(i)];
        level[i] = o.baseLevelPrev;
        levelStep[i] = (o.baseLevel - o.baseLevelPrev) * invLen;
    }

    for (int s = 0; s < len; ++s)
    {
        // Oscillators, highest first: a modulator with a higher index is read from this sample,
        // a lower index (only possible in custom algorithms) from the previous sample.
        float carrierSum = 0.0f;
        for (int i = kNumOscs - 1; i >= 0; --i)
        {
            OscState& o = osc_[static_cast<size_t>(i)];
            level[i] += levelStep[i];
            if (!o.active) continue;

            o.level = clampf(level[i] + o.envLevel * envBuf_[i][s], 0.0f, 1.0f) * keyscale_[i];

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

            // Vintage Digital reads the nearest table sample (stepped, like early digital oscillators).
            float w = vintage_ ? o.table[static_cast<int>(rp * static_cast<float>(WavetableBank::kTableSize))]
                               : WavetableBank::read(o.table, rp);
            if (o.table2)
            {
                const float w2 = vintage_ ? o.table2[static_cast<int>(rp * static_cast<float>(WavetableBank::kTableSize))]
                                          : WavetableBank::read(o.table2, rp);
                w += o.tableMix * (w2 - w);
            }
            if (o.invert) w = -w;
            o.yPrev = o.y;
            o.y = w;
            o.modOut = w * o.level * o.level * modSide_[i];
            const float outW = algo_.out[i];
            if (outW > 0.0f) carrierSum += w * o.level * outW * carrierSide_[i];
        }

        // Filters run in the engine's 4-voice bank; hand over the pre-filter signal.
        float x = carrierSum * oscsLevel_;
        if (bitScale_ > 0.0f) x = std::floor(x * bitScale_ + 0.5f) / bitScale_; // Bit Depth, before the filters
        pre[s * stride] = x;
    }
    (void)p;
}

namespace {
// Vector form of pdWarp (same formulas, lane-wise).
F4 pdWarpV(OscMode mode, F4 ph, F4 amt) noexcept
{
    auto pw = [](F4 x, F4 a) {
        const F4 w = F4(0.5f) - F4(0.49f) * a;
        const F4 lo = x * F4(0.5f) / w;
        const F4 hi = F4(0.5f) + (x - w) * F4(0.5f) / (F4(1.0f) - w);
        return vselect(vlt(x, w), lo, hi);
    };
    auto saw = [](F4 x, F4 a) {
        const F4 c = F4(15.0f) * a;
        return x * (F4(1.0f) + c) / (F4(1.0f) + c * x);
    };
    auto square = [&](F4 x, F4 a) {
        const F4 upper = vlt(x, F4(0.5f));
        const F4 half = vselect(upper, F4(0.0f), F4(0.5f));
        const F4 local = (x - half) * F4(2.0f);
        return half + pw(local, a) * F4(0.5f);
    };
    switch (mode)
    {
        case OscMode::PWMod: return pw(ph, amt);
        case OscMode::PDSaw: return saw(ph, amt);
        case OscMode::PDSquare: return square(ph, amt);
        case OscMode::PDSawPulse: return square(saw(ph, amt), amt);
        default: return ph;
    }
}

inline F4 wrap01V(F4 x) noexcept { return x - vfloor(x); }
} // namespace

// The oscillators of up to four voices at once, one voice per SIMD lane. Empty lanes (nullptr) and
// inactive oscillators are computed on dummy data and masked out, so every lane follows exactly the
// arithmetic of renderOscScalar. Writes pre[s * 4 + lane].
void Voice::renderOscBank(Voice* const lanes[4], const RenderContext& ctx, int len, float* pre) noexcept
{
    const Patch& p = *ctx.patch;
    const float* fallbackTable = WavetableBank::instance().table(0, 0);
    static const float kZeros[kControlBlock] = {};
    const float invLen = 1.0f / static_cast<float>(len);
    alignas(16) float t4[4];
    auto gather = [&](auto fn) {
        for (int l = 0; l < 4; ++l) t4[l] = lanes[l] ? fn(*lanes[l]) : 0.0f;
        return F4::load(t4);
    };

    struct OscV
    {
        F4 phase, inc, y, yPrev, modOut, level, lvl, lvlStep, envLevel, fb, ks, cside, mside, outW, active;
        const float* table[4];
        const float* table2[4];
        float tableMix[4];
        bool scanning;
        const float* env[4];
        bool any, invert;
        OscMode mode;
    };
    OscV o[kNumOscs];
    for (int i = 0; i < kNumOscs; ++i)
    {
        OscV& v = o[i];
        const auto si = static_cast<size_t>(i);
        bool act[4];
        for (int l = 0; l < 4; ++l) act[l] = lanes[l] && lanes[l]->osc_[si].active;
        v.active = maskFromBools(act[0], act[1], act[2], act[3]);
        v.any = act[0] || act[1] || act[2] || act[3];
        v.phase = gather([&](Voice& x) { return x.osc_[si].phase; });
        v.inc = gather([&](Voice& x) { return x.osc_[si].inc; });
        v.y = gather([&](Voice& x) { return x.osc_[si].y; });
        v.yPrev = gather([&](Voice& x) { return x.osc_[si].yPrev; });
        v.modOut = gather([&](Voice& x) { return x.osc_[si].modOut; });
        v.level = gather([&](Voice& x) { return x.osc_[si].level; });
        v.lvl = gather([&](Voice& x) { return x.osc_[si].baseLevelPrev; });
        v.lvlStep = gather([&](Voice& x) { return (x.osc_[si].baseLevel - x.osc_[si].baseLevelPrev) * invLen; });
        v.envLevel = gather([&](Voice& x) { return x.osc_[si].envLevel; });
        v.fb = gather([&](Voice& x) { return x.osc_[si].feedback; });
        v.ks = gather([&](Voice& x) { return x.keyscale_[i]; });
        v.cside = gather([&](Voice& x) { return x.carrierSide_[i]; });
        v.mside = gather([&](Voice& x) { return x.modSide_[i]; });
        v.outW = gather([&](Voice& x) { return x.algo_.out[i] > 0.0f ? x.algo_.out[i] : 0.0f; });
        for (int l = 0; l < 4; ++l)
        {
            v.table[l] = act[l] && lanes[l]->osc_[si].table ? lanes[l]->osc_[si].table : fallbackTable;
            v.table2[l] = act[l] ? lanes[l]->osc_[si].table2 : nullptr;
            v.tableMix[l] = v.table2[l] ? lanes[l]->osc_[si].tableMix : 0.0f;
            v.env[l] = lanes[l] ? lanes[l]->envBuf_[i] : kZeros;
        }
        v.scanning = v.table2[0] || v.table2[1] || v.table2[2] || v.table2[3];
        v.invert = p.osc[si].invert;
        v.mode = p.osc[si].mode;
    }

    // Connection weights per lane (0 where a lane has no such connection); list per target of the
    // modulators connected in any lane, in ascending order as in the scalar kernel.
    F4 w[kNumOscs][kNumOscs];
    int conn[kNumOscs][kNumOscs], nconn[kNumOscs] = {};
    for (int c = 0; c < kNumOscs; ++c)
        for (int m = 0; m < kNumOscs; ++m)
        {
            if (m == c) continue;
            bool anyLane = false;
            for (int l = 0; l < 4; ++l)
            {
                float wt = 0.0f;
                if (lanes[l] && lanes[l]->osc_[static_cast<size_t>(m)].active)
                {
                    const float a = lanes[l]->algo_.conn[m][c];
                    if (a > 0.0f) wt = a;
                }
                t4[l] = wt;
                anyLane = anyLane || wt > 0.0f;
            }
            if (!anyLane) continue;
            w[m][c] = F4::load(t4);
            conn[c][nconn[c]++] = m;
        }

    const F4 oscsLevel = gather([](Voice& x) { return x.oscsLevel_; });
    const bool vintage = p.voice.vintage;
    float bitScale = 0.0f;
    for (int l = 0; l < 4; ++l)
        if (lanes[l]) bitScale = lanes[l]->bitScale_;
    const float tableSize = static_cast<float>(WavetableBank::kTableSize);

    for (int s = 0; s < len; ++s)
    {
        F4 carrierSum(0.0f);
        for (int i = kNumOscs - 1; i >= 0; --i)
        {
            OscV& v = o[i];
            v.lvl += v.lvlStep;
            if (!v.any) continue;

            for (int l = 0; l < 4; ++l) t4[l] = v.env[l][s];
            const F4 level = vclamp(v.lvl + v.envLevel * F4::load(t4), F4(0.0f), F4(1.0f)) * v.ks;

            F4 pm(0.0f), fm(0.0f), warp(0.0f), syncAmt(0.0f), syncPh(0.0f), syncInc(0.0f), hasSync(0.0f);
            bool anySync = false;
            for (int k = 0; k < nconn[i]; ++k)
            {
                const int m = conn[i][k];
                const F4 wt = w[m][i];
                const OscV& src = o[m];
                switch (src.mode)
                {
                    case OscMode::PhaseMod: pm += src.modOut * wt; break;
                    case OscMode::FreqMod: fm += src.modOut * wt; break;
                    case OscMode::HardSync:
                    {
                        const F4 on = vgt(wt, F4(0.0f));
                        syncAmt = vselect(on, src.level * wt, syncAmt);
                        syncPh = vselect(on, src.phase, syncPh);
                        syncInc = vselect(on, src.inc, syncInc);
                        hasSync = vor(hasSync, on);
                        anySync = true;
                        break;
                    }
                    default:
                    {
                        const F4 r = vselect(vgt(vabs(v.inc), F4(1.0e-9f)), src.inc / v.inc, F4(1.0f));
                        const F4 contrib = (pdWarpV(src.mode, src.phase, src.level * wt) - src.phase) / r;
                        warp += vselect(vand(vgt(vabs(r), F4(1.0e-6f)), vgt(wt, F4(0.0f))), contrib, F4(0.0f));
                        break;
                    }
                }
            }

            F4 phase = v.phase + v.inc * (F4(1.0f) + F4(kFmDepth) * fm);
            phase = phase - vfloor(phase);

            F4 rp = phase;
            if (anySync)
            {
                const F4 use = vand(hasSync, vgt(vabs(syncInc), F4(1.0e-9f)));
                rp = vselect(use, wrap01V((F4(1.0f) + F4(7.0f) * syncAmt) * syncPh * v.inc / syncInc), rp);
            }
            rp = wrap01V(rp + warp + pm * F4(kPmDepth) + v.fb * F4(0.5f) * (v.y + v.yPrev));

            alignas(16) float rpa[4], wa[4];
            rp.store(rpa);
            for (int l = 0; l < 4; ++l)
                wa[l] = vintage ? v.table[l][static_cast<int>(rpa[l] * tableSize)] : WavetableBank::read(v.table[l], rpa[l]);
            if (v.scanning)
                for (int l = 0; l < 4; ++l)
                    if (v.table2[l])
                    {
                        const float w2 = vintage ? v.table2[l][static_cast<int>(rpa[l] * tableSize)]
                                                 : WavetableBank::read(v.table2[l], rpa[l]);
                        wa[l] += v.tableMix[l] * (w2 - wa[l]);
                    }
            F4 wv = F4::load(wa);
            if (v.invert) wv = -wv;

            const F4 act = v.active;
            v.yPrev = vselect(act, v.y, v.yPrev);
            v.y = vselect(act, wv, v.y);
            v.phase = vselect(act, phase, v.phase);
            v.level = vselect(act, level, v.level);
            v.modOut = vselect(act, wv * level * level * v.mside, F4(0.0f));
            carrierSum += vselect(act, wv * level * v.outW * v.cside, F4(0.0f));
        }

        F4 x = carrierSum * oscsLevel;
        if (bitScale > 0.0f) x = vfloor(x * F4(bitScale) + F4(0.5f)) / F4(bitScale);
        x.store(pre + 4 * s);
    }

    // Write the oscillator state back to the voices.
    for (int i = 0; i < kNumOscs; ++i)
    {
        alignas(16) float ph[4], y[4], yp[4], mo[4], lv[4];
        o[i].phase.store(ph);
        o[i].y.store(y);
        o[i].yPrev.store(yp);
        o[i].modOut.store(mo);
        o[i].level.store(lv);
        for (int l = 0; l < 4; ++l)
        {
            if (!lanes[l]) continue;
            OscState& st = lanes[l]->osc_[static_cast<size_t>(i)];
            st.phase = ph[l];
            st.y = y[l];
            st.yPrev = yp[l];
            st.modOut = mo[l];
            st.level = lv[l];
        }
    }
}
} // namespace kmi
