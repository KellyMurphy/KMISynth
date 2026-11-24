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

struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    void reset() noexcept { ic1 = ic2 = 0.0f; }
    // g = tan(pi*fc/fs); k = damping (2 = none, -> 0 self-oscillates)
    void process(float x, float g, float k, float& lp, float& bp, float& hp) noexcept
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
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
    // k: feedback (self-oscillates around 4). comp: 0 = classic bass loss, 1 = unity DC gain.
    void process(float x, float g, float k, float comp, float satScale, float y[4]) noexcept
    {
        const float G = g / (1.0f + g);
        const float b = 1.0f / (1.0f + g);
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float S = G3 * b * s[0] + G2 * b * s[1] + G * b * s[2] + b * s[3];
        const float xin = x * (1.0f + comp * k);
        const float y4 = (G4 * xin + S) / (1.0f + k * G4);
        float u = xin - k * y4;
        if (satScale > 0.0f) u = softClip(u * satScale) / satScale;
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

class DigitalFilter
{
public:
    void reset() noexcept;
    void prepareBlock(const DigitalFilterBlock& b, float sampleRate) noexcept; // per control block
    float process(float x, float g) noexcept;                                    // g ramps per sample

private:
    float applyDrive(float x) const noexcept;

    DigitalFilterBlock blk_{};
    float svfK_ = 2.0f, ladderK_ = 0.0f, driveGain_ = 1.0f, driveComp_ = 1.0f, gateGain_ = 1.0f;
    Svf svf_[3];
    Ladder ladder_[2];
    OnePole onePole_;
    float vowelG_[3] = {}, vowelGain_[3] = {}, vowelK_ = 0.5f;
};

class AnalogFilter
{
public:
    void reset() noexcept { ladder_.reset(); }
    void prepareBlock(float resonance, float preDrive) noexcept;
    float process(float x, float g) noexcept;

private:
    Ladder ladder_;
    float k_ = 0.0f, driveGain_ = 1.0f, driveComp_ = 1.0f;
};

const char* vowelOrderName(int order) noexcept;
constexpr int kNumVowelOrders = 8;

} // namespace kmi
