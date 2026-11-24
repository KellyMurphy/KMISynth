// Filter primitives (TPT/ZDF state-variable, one-pole and 4-pole ladder) and the two voice filters.
#pragma once

#include "kmi/Dsp.h"
#include "kmi/Patch.h"

namespace kmi {

// Map normalised cutoff 0..1 to 20 Hz .. 20 kHz (exponential), clamped below Nyquist.
inline float cutoffToHz(float c, float sampleRate) noexcept
{
    const float hz = 20.0f * std::exp2(clampf(c, 0.0f, 1.0f) * 9.965784f); // log2(1000)
    return clampf(hz, 10.0f, 0.45f * sampleRate);
}
inline float hzToG(float hz, float sampleRate) noexcept { return std::tan(kPi * hz / sampleRate); }

// Normalised-cutoff units per octave (keytrack and pitch-relative modulation).
constexpr float kCutoffPerOctave = 1.0f / 9.965784f;

// Division-free saturator: cubic soft clip, exactly +-1 beyond |x| >= 1.5. Used in filter feedback.
inline float cubicClip(float x) noexcept
{
    x = clampf(x, -1.5f, 1.5f);
    return x - (4.0f / 27.0f) * x * x * x;
}

struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    void reset() noexcept { ic1 = ic2 = 0.0f; }
    static float a1For(float g, float k) noexcept { return 1.0f / (1.0f + g * (g + k)); }
    // g = tan(pi*fc/fs); k = damping (2 = none, -> 0 self-oscillates)
    void process(float x, float g, float k, float& lp, float& bp, float& hp) noexcept
    {
        process(x, g, k, a1For(g, k), lp, bp, hp);
    }
    // Same, with the per-sample division precomputed / ramped by the caller.
    void process(float x, float g, float k, float a1, float& lp, float& bp, float& hp) noexcept
    {
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
};

struct OnePole
{
    float s = 0.0f;
    void reset() noexcept { s = 0.0f; }
    float lp(float x, float g) noexcept
    {
        const float v = (x - s) * (g / (1.0f + g));
        const float y = v + s;
        s = y + v;
        return y;
    }
};

// Zero-delay-feedback 4-pole ladder (linear solve, saturation applied to the feedback-summed input).
struct Ladder
{
    float s[4] = {};
    void reset() noexcept { s[0] = s[1] = s[2] = s[3] = 0.0f; }

    static float bigG(float g) noexcept { return g / (1.0f + g); }
    static float invDen(float G, float k) noexcept
    {
        const float G2 = G * G;
        return 1.0f / (1.0f + k * G2 * G2);
    }

    // k: feedback (self-oscillates around 4). comp: 0 = classic bass loss, 1 = unity DC gain.
    void process(float x, float g, float k, float comp, float satScale, float y[4]) noexcept
    {
        const float G = bigG(g);
        process(x, G, invDen(G, k), k, comp, satScale, y);
    }

    // Division-free form: G = g/(1+g) and inv = 1/(1 + k*G^4) precomputed (ramped) by the caller.
    void process(float x, float G, float inv, float k, float comp, float satScale, float y[4]) noexcept
    {
        const float b = 1.0f - G; // = 1/(1+g)
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float S = b * (G3 * s[0] + G2 * s[1] + G * s[2] + s[3]);
        const float xin = x * (1.0f + comp * k);
        const float y4 = (G4 * xin + S) * inv;
        float u = xin - k * y4;
        if (satScale > 0.0f) u = cubicClip(u * satScale) * (1.0f / satScale);
        float in = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (in - s[i]) * G;
            const float out = v + s[i];
            s[i] = out + v;
            y[i] = out;
            in = out;
        }
    }
};

// Per-block settings for the digital filter, computed by the voice.
struct DigitalFilterBlock
{
    DigitalFilterType type = DigitalFilterType::LpLadder24;
    float resonance = 0.0f; // 0..1
    float morph = 0.0f;     // 0..1
    float drive = 0.0f;     // 0..1
    bool drivePost = false;
    float cutoffNorm = 1.0f; // used by LP Gate and Vowel
    int vowelOrder = 0;
};

// Linear per-sample ramp of a block-rate coefficient.
struct Ramp
{
    float value = 0.0f, step = 0.0f;
    void set(float from, float to, int len) noexcept
    {
        value = from;
        step = (to - from) / static_cast<float>(len);
    }
    float next() noexcept { return value += step; }
};

class DigitalFilter
{
public:
    void reset() noexcept;
    void prepareBlock(const DigitalFilterBlock& b, float sampleRate) noexcept; // per control block
    // Cutoff coefficient g = tan(pi*fc/fs) moves from gFrom to gTo over the next len samples.
    void beginRamp(float gFrom, float gTo, int len) noexcept;
    float process(float x) noexcept;

private:
    float applyDrive(float x) const noexcept;

    DigitalFilterBlock blk_{};
    float svfK_ = 2.0f, ladderK_ = 0.0f, driveGain_ = 1.0f, driveComp_ = 1.0f, gateGain_ = 1.0f;
    float svfKEff_ = 2.0f, ladderKEff_ = 0.0f; // damping / feedback actually used by the current type
    Ramp g_, G_, a1_, inv_;
    Svf svf_[3];
    Ladder ladder_[2];
    OnePole onePole_;
    float vowelG_[3] = {}, vowelA1_[3] = {}, vowelGain_[3] = {}, vowelK_ = 0.5f;
};

class AnalogFilter
{
public:
    void reset() noexcept { ladder_.reset(); }
    void prepareBlock(float resonance, float preDrive) noexcept;
    void beginRamp(float gFrom, float gTo, int len) noexcept;
    float process(float x) noexcept;

private:
    Ladder ladder_;
    Ramp G_, inv_;
    float k_ = 0.0f, driveGain_ = 1.0f, driveComp_ = 1.0f;
};

const char* vowelOrderName(int order) noexcept;
constexpr int kNumVowelOrders = 8;

} // namespace kmi
