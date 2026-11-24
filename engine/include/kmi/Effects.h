// Global effects chain: Pre-FX -> Delay -> Reverb -> Post-FX (stereo, base sample rate).
// prepare() allocates; process() is real-time safe.
#pragma once

#include <array>
#include <vector>

#include "kmi/Filters.h"
#include "kmi/Modulation.h"
#include "kmi/Patch.h"

namespace kmi {

class DelayLine
{
public:
    void prepare(int maxDelaySamples);
    void reset() noexcept;
    void push(float x) noexcept
    {
        buf_[static_cast<size_t>(w_)] = x;
        w_ = (w_ + 1) & mask_;
    }
    // d = delay in samples (0 = most recently pushed sample). Linear interpolation.
    float read(float d) const noexcept
    {
        const float pos = static_cast<float>(w_ - 1) - d;
        const float fl = std::floor(pos);
        const int i = static_cast<int>(fl);
        const float f = pos - fl;
        const float a = buf_[static_cast<size_t>(i & mask_)];
        const float b = buf_[static_cast<size_t>((i + 1) & mask_)];
        return a + (b - a) * f;
    }
    int capacity() const noexcept { return mask_; }

private:
    std::vector<float> buf_{ 0.0f, 0.0f };
    int mask_ = 1, w_ = 0;
};

// Per-block modulation offsets for one insert slot (from the mod matrix).
struct InsertMods
{
    float p1 = 0.0f, p2 = 0.0f, mix = 0.0f;
};

class InsertFx
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float* l, float* r, int n, const InsertFxParams& p, const InsertMods& m, float beatPhase,
                 float beatInc) noexcept;
    void noteOn() noexcept { noteDuck_ = true; }

private:
    void chorus(float* l, float* r, int n, const ModFxParams& p, const InsertMods& m, bool flanger) noexcept;
    void rotary(float* l, float* r, int n, const RotaryParams& p, const InsertMods& m) noexcept;
    void phaser(float* l, float* r, int n, const PhaserParams& p, const InsertMods& m) noexcept;
    void lofi(float* l, float* r, int n, const LoFiParams& p, const InsertMods& m) noexcept;
    void tremolo(float* l, float* r, int n, const TremoloParams& p, const InsertMods& m) noexcept;
    void eq(float* l, float* r, int n, const EqParams& p, const InsertMods& m) noexcept;
    void compressor(float* l, float* r, int n, const CompParams& p, const InsertMods& m, float beatPhase,
                    float beatInc) noexcept;
    void distort(float* l, float* r, int n, const DistortParams& p, const InsertMods& m) noexcept;

    float sr_ = 48000.0f;
    FxType lastType_ = FxType::Bypass;
    DelayLine lineL_, lineR_, line2L_, line2R_;
    float phase_ = 0.0f, phase2_ = 0.0f;
    // Shared state (reset on type change)
    float ap_[2][6] = {};       // phaser all-pass states
    float fbL_ = 0.0f, fbR_ = 0.0f;
    float zL_[4] = {}, zR_[4] = {}; // one-pole states
    Svf svfL_[2], svfR_[2];
    float holdL_ = 0.0f, holdR_ = 0.0f, holdAcc_ = 1.0f;
    float envDb_ = -120.0f, duckDb_ = 0.0f;
    float lastBeatPhase_ = 0.0f;
    bool noteDuck_ = false;
};

class StereoDelay
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float* l, float* r, int n, const DelayParams& p, const float* mods, double bpm) noexcept;

private:
    float sr_ = 48000.0f;
    DelayType lastType_ = DelayType::Stereo;
    DelayLine lineL_, lineR_;
    float timeSmooth_ = -1.0f;
    float fbLpL_ = 0.0f, fbLpR_ = 0.0f, toneL_ = 0.0f, toneR_ = 0.0f;
    float revPhase_ = 0.0f;
};

class Reverb
{
public:
    static constexpr int kLines = 8;
    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float* l, float* r, int n, const ReverbParams& p, const float* mods) noexcept;

private:
    void configure(ReverbType type) noexcept;

    float sr_ = 48000.0f;
    ReverbType type_ = ReverbType::Hall;
    bool configured_ = false;
    std::array<DelayLine, kLines> lines_;
    std::array<DelayLine, 4> diffusers_;
    DelayLine pre_;
    float delay_[kLines] = {}, diffDelay_[4] = {};
    float diffCoef_ = 0.7f, modDepth_ = 0.0f;
    float lp_[kLines] = {}, hp_[kLines] = {};
    float modPhase_[kLines] = {}, modInc_[kLines] = {};
    float toneL_ = 0.0f, toneR_ = 0.0f;
};

class FxChain
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    // mods: the dst::Count array of a voice (may be null). transportPpq < 0 = free-running beat clock.
    void process(float* l, float* r, int n, const FxParams& p, const float* mods, double bpm,
                 double transportPpq) noexcept;
    void noteOn() noexcept
    {
        pre_.noteOn();
        post_.noteOn();
    }

private:
    InsertFx pre_, post_;
    StereoDelay delay_;
    Reverb reverb_;
    double sampleRate_ = 48000.0;
    double beatPhase_ = 0.0;
};

} // namespace kmi
