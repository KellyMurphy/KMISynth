#include "kmi/Randomise.h"

#include <cmath>

#include "kmi/Algorithm.h"
#include "kmi/Dsp.h"
#include "kmi/Wavetables.h"

namespace kmi {

namespace {
class PatchRng
{
public:
    explicit PatchRng(uint32_t seed) : s_(seed ? seed : 0x9E3779B9u) {}
    float next() noexcept // 0..1
    {
        s_ ^= s_ << 13;
        s_ ^= s_ >> 17;
        s_ ^= s_ << 5;
        return static_cast<float>(s_ & 0xFFFFFF) / 16777216.0f;
    }
    float range(float lo, float hi) noexcept { return lo + (hi - lo) * next(); }
    float logRange(float lo, float hi) noexcept { return lo * std::pow(hi / lo, next()); }
    int below(int n) noexcept { return static_cast<int>(next() * static_cast<float>(n)) % (n > 0 ? n : 1); }
    bool chance(float p) noexcept { return next() < p; }

private:
    uint32_t s_;
};

struct Mixer
{
    PatchRng& rng;
    float a;
    // Continuous values move part of the way; discrete values switch with probability `amount`.
    void f(float& v, float target) const noexcept { v += a * (target - v); }
    template <class T>
    void d(T& v, T target) const noexcept
    {
        if (rng.chance(a)) v = target;
    }
    void logf(float& v, float target) const noexcept // times and rates: interpolate in the log domain
    {
        if (v <= 0.0f || target <= 0.0f) f(v, target);
        else v = v * std::pow(target / v, a);
    }
};

// Mostly harmonic ratios, sometimes an inharmonic one (bells, metal).
float randomRatio(PatchRng& r)
{
    static const float harmonic[] = { 0.5f, 1.0f, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 14.0f };
    static const float odd[] = { 1.41f, 2.76f, 3.5f, 4.24f, 5.19f, 7.13f, 9.3f };
    if (r.chance(0.8f)) return harmonic[r.below(static_cast<int>(std::size(harmonic)))];
    return odd[r.below(static_cast<int>(std::size(odd)))];
}

void randomEnv(const Mixer& m, PatchRng& r, EnvParams& e, bool percussive)
{
    m.logf(e.attackMs, percussive ? r.logRange(0.5f, 30.0f) : r.logRange(1.0f, 2500.0f));
    m.logf(e.decayMs, r.logRange(80.0f, 5000.0f));
    m.f(e.sustain, percussive ? r.range(0.0f, 0.3f) : r.range(0.2f, 1.0f));
    m.logf(e.releaseMs, r.logRange(60.0f, 3000.0f));
    m.f(e.decayCurve, r.range(0.0f, 0.8f));
}
} // namespace

void randomisePatch(Patch& p, const RandomiseOptions& o, uint32_t seed)
{
    PatchRng r(seed);
    const float a = clampf(o.amount, 0.0f, 1.0f);
    const Mixer m{ r, a };
    const bool percussive = r.chance(0.4f); // one character for the whole patch

    if (o.algorithm && r.chance(a))
    {
        p.algo.mode = AlgoMode::Single;
        p.algo.index = r.below(numPresetAlgorithms());
    }

    Algorithm alg;
    resolveAlgorithm(p, 0.0f, alg);
    int carriers = 0;
    for (int i = 0; i < kNumOscs; ++i) carriers += alg.isCarrier(i) ? 1 : 0;
    const float carrierLevel = 0.75f / std::sqrt(static_cast<float>(carriers > 0 ? carriers : 1));

    if (o.oscillators)
    {
        const auto& bank = WavetableBank::instance();
        for (int i = 0; i < kNumOscs; ++i)
        {
            OscParams& op = p.osc[static_cast<size_t>(i)];
            const bool carrier = alg.isCarrier(i), modulator = alg.isModulator(i);
            if (!carrier && !modulator) continue;
            // Carriers often stay sine-like (FM character comes from the modulators); any built-in wave is possible.
            const int wave = carrier && r.chance(0.5f) ? 0 : WavetableBank::builtInWave(r.below(bank.numWaves()));
            m.d(op.wave, wave);
            m.d(op.mode, modulator ? static_cast<OscMode>(r.chance(0.6f) ? 0 : r.below(static_cast<int>(OscMode::Count))) : op.mode);
            m.d(op.pitchMode, PitchMode::Ratio);
            m.d(op.ratio, carrier && r.chance(0.6f) ? 1.0f : randomRatio(r));
            m.f(op.ratioFine, r.chance(0.3f) ? r.range(-0.6f, 0.6f) : 0.0f);
            m.d(op.mute, false);
            if (carrier)
            {
                m.f(op.initLevel, carrierLevel * r.range(0.7f, 1.0f));
                m.f(op.envLevel, 0.0f);
            }
            else
            {
                m.f(op.initLevel, r.range(0.0f, 0.35f));
                m.f(op.envLevel, r.range(-0.1f, 0.45f));
                randomEnv(m, r, op.env, percussive || r.chance(0.5f));
                m.f(op.env.velAmount, r.range(0.0f, 0.8f));
            }
            m.f(op.feedback, r.chance(0.25f) ? r.range(0.0f, 0.5f) : 0.0f);
            if (r.chance(0.3f))
            {
                m.d(op.scanRange, 1 + r.below(8));
                m.f(op.scan, r.next());
            }
            else
                m.d(op.scanRange, 0);
        }
    }

    if (o.envelopes)
        for (int e = 0; e < 3; ++e) randomEnv(m, r, p.env[static_cast<size_t>(e)], percussive && e == 2);

    if (o.filters)
    {
        m.f(p.dfilt.cutoff, r.range(0.55f, 1.0f));
        m.f(p.dfilt.resonance, r.range(0.0f, 0.55f));
        m.f(p.dfilt.envAmt, r.range(-0.1f, 0.5f));
        m.f(p.afilt.cutoff, r.range(0.5f, 1.0f));
        m.f(p.afilt.resonance, r.range(0.0f, 0.5f));
        m.f(p.afilt.envAmt, r.range(0.0f, 0.5f));
        m.f(p.afilt.preDrive, r.range(0.05f, 0.45f));
    }

    if (o.modulation)
        for (auto& l : p.lfo)
        {
            m.logf(l.rateHz, r.logRange(0.05f, 8.0f));
            m.d(l.wave, static_cast<LfoWave>(r.below(static_cast<int>(LfoWave::Step)))); // Step needs drawn values
        }

    if (o.effects)
    {
        static const FxType inserts[] = { FxType::Bypass, FxType::Bypass, FxType::Chorus, FxType::Flanger, FxType::Phaser,
                                          FxType::Rotary, FxType::Tremolo, FxType::Distort, FxType::LoFi };
        m.d(p.fx.pre.type, inserts[r.below(static_cast<int>(std::size(inserts)))]);
        m.d(p.fx.post.type, r.chance(0.6f) ? FxType::Bypass : FxType::Chorus);
        m.d(p.fx.delay.enabled, r.chance(0.6f));
        m.d(p.fx.delay.type, static_cast<DelayType>(r.below(static_cast<int>(DelayType::Reverse))));
        m.f(p.fx.delay.feedback, r.range(0.15f, 0.5f));
        m.f(p.fx.delay.mix, r.range(0.1f, 0.3f));
        m.d(p.fx.reverb.enabled, r.chance(0.8f));
        m.d(p.fx.reverb.type, static_cast<ReverbType>(r.below(static_cast<int>(ReverbType::Count))));
        m.logf(p.fx.reverb.timeSec, r.logRange(0.8f, 6.0f));
        m.f(p.fx.reverb.mix, r.range(0.1f, 0.35f));
    }
}

} // namespace kmi
