// Voice filters. The maths is written once as templates over V = float | F4:
//   - scalar (V = float): DigitalFilter / AnalogFilter wrappers, used by tests and benchmarks
//   - SIMD   (V = F4):    FilterBank4, which runs the filters of four voices side by side
// Per-voice parameters are computed per control block into FilterLaneParams.
#pragma once

#include "kmi/Dsp.h"
#include "kmi/Patch.h"
#include "kmi/Simd.h"

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

inline float cubicClip(float x) noexcept { return cubicClipT(x); }

// ---- Primitives ------------------------------------------------------------------------------------
template <class V>
struct SvfT
{
    V ic1{ 0.0f }, ic2{ 0.0f };
    void reset() noexcept { ic1 = ic2 = V(0.0f); }
    void zero(int lane) noexcept { zeroLane(ic1, lane); zeroLane(ic2, lane); }
    static V a1For(V g, V k) noexcept { return V(1.0f) / (V(1.0f) + g * (g + k)); }
    // g = tan(pi*fc/fs); k = damping (2 = none, -> 0 self-oscillates); a1 = a1For(g, k)
    void process(V x, V g, V k, V a1, V& lp, V& bp, V& hp) noexcept
    {
        const V a2 = g * a1;
        const V a3 = g * a2;
        const V v3 = x - ic2;
        const V v1 = a1 * ic1 + a2 * v3;
        const V v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = V(2.0f) * v1 - ic1;
        ic2 = V(2.0f) * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
};

// Zero-delay-feedback 4-pole ladder (linear solve, saturation on the feedback-summed input).
template <class V>
struct LadderT
{
    V s[4] = {};
    void reset() noexcept { s[0] = s[1] = s[2] = s[3] = V(0.0f); }
    void zero(int lane) noexcept { for (auto& v : s) zeroLane(v, lane); }
    static V bigG(V g) noexcept { return g / (V(1.0f) + g); }
    static V invDen(V G, V k) noexcept
    {
        const V G2 = G * G;
        return V(1.0f) / (V(1.0f) + k * G2 * G2);
    }
    // G = g/(1+g), inv = invDen(G, k). k: feedback (~4 self-oscillates). comp: 0 = bass loss, 1 = unity DC.
    void process(V x, V G, V inv, V k, float comp, float satScale, V y[4]) noexcept
    {
        const V b = V(1.0f) - G; // = 1/(1+g)
        const V G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const V S = b * (G3 * s[0] + G2 * s[1] + G * s[2] + s[3]);
        const V xin = comp != 0.0f ? x * (V(1.0f) + k * comp) : x;
        const V y4 = (G4 * xin + S) * inv;
        V u = xin - k * y4;
        if (satScale > 0.0f) u = cubicClipT(u * V(satScale)) * V(1.0f / satScale);
        V in = u;
        for (int i = 0; i < 4; ++i)
        {
            const V v = (in - s[i]) * G;
            const V out = v + s[i];
            s[i] = out + v;
            y[i] = out;
            in = out;
        }
    }
};

// Scalar conveniences (effects, tests).
struct Svf : SvfT<float>
{
    using SvfT<float>::process;
    void process(float x, float g, float k, float& lp, float& bp, float& hp) noexcept
    {
        SvfT<float>::process(x, g, k, a1For(g, k), lp, bp, hp);
    }
};
struct Ladder : LadderT<float>
{
    using LadderT<float>::process;
    void process(float x, float g, float k, float comp, float satScale, float y[4]) noexcept
    {
        const float G = bigG(g);
        LadderT<float>::process(x, G, invDen(G, k), k, comp, satScale, y);
    }
};

// ---- Per-voice, per-block filter settings ------------------------------------------------------------
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

struct FilterLaneParams
{
    bool resetState = true;  // voice started from silence: clear this lane's filter state
    // Digital filter
    float gFrom = 0.5f, gTo = 0.5f; // cutoff coefficient ramp over the block
    float svfK = 2.0f, ladderK = 0.0f, morph = 0.0f;
    float driveGain = 1.0f, driveComp = 1.0f, driveOn = 0.0f, gateGain = 1.0f;
    float vowelG[3] = { 0.1f, 0.1f, 0.1f }, vowelA1[3] = { 1.0f, 1.0f, 1.0f }, vowelK = 0.5f;
    float level = 1.0f;      // digital filter output gain
    // Analog filter
    float aGFrom = 0.5f, aGTo = 0.5f;
    float aK = 0.0f, aDriveGain = 1.0f, aDriveComp = 1.0f;
};

void computeDigitalLane(const DigitalFilterBlock& b, float sampleRate, FilterLaneParams& out) noexcept;
void computeAnalogLane(float resonance, float preDrive, FilterLaneParams& out) noexcept;

// Coefficients as seen by the filter core, with per-sample ramps.
template <class V>
struct FilterCoeffs
{
    V g, G, a1, inv, dg, dG, da1, dinv;
    V k, lk, morph, driveGain, driveComp, driveOn, gateGain, level;
    V vG[3], vA1[3], vK;
    V aG, aInv, daG, daInv, aK, aDrive, aComp;
    void advance() noexcept
    {
        g += dg;
        G += dG;
        a1 += da1;
        inv += dinv;
        aG += daG;
        aInv += daInv;
    }
};

// Digital -> level -> analog filter chain for one (V = float) or four (V = F4) voices.
template <class V>
struct VoiceFilterCore
{
    SvfT<V> svf[3];
    LadderT<V> lad[2];
    LadderT<V> analog;
    V onePole{ 0.0f };

    void reset() noexcept
    {
        for (auto& s : svf) s.reset();
        for (auto& l : lad) l.reset();
        analog.reset();
        onePole = V(0.0f);
    }
    void zero(int lane) noexcept
    {
        for (auto& s : svf) s.zero(lane);
        for (auto& l : lad) l.zero(lane);
        analog.zero(lane);
        zeroLane(onePole, lane);
    }

    V processDigital(V x, DigitalFilterType type, bool drivePost, const FilterCoeffs<V>& c) noexcept;
    V processAnalog(V x, const FilterCoeffs<V>& c) noexcept;
    V process(V x, DigitalFilterType type, bool drivePost, const FilterCoeffs<V>& c) noexcept
    {
        return processAnalog(processDigital(x, type, drivePost, c) * c.level, c);
    }
};

extern template struct VoiceFilterCore<float>;
extern template struct VoiceFilterCore<F4>;

// ---- Scalar wrappers ----------------------------------------------------------------------------------
class DigitalFilter
{
public:
    void reset() noexcept { core_.reset(); }
    void prepareBlock(const DigitalFilterBlock& b, float sampleRate) noexcept;
    void beginRamp(float gFrom, float gTo, int len) noexcept;
    float process(float x) noexcept; // digital stage only

private:
    DigitalFilterBlock blk_{};
    FilterLaneParams lane_{};
    FilterCoeffs<float> c_{};
    VoiceFilterCore<float> core_;
};

class AnalogFilter
{
public:
    void reset() noexcept { ladder_.reset(); }
    void prepareBlock(float resonance, float preDrive) noexcept;
    void beginRamp(float gFrom, float gTo, int len) noexcept;
    float process(float x) noexcept;

private:
    FilterLaneParams lane_{};
    LadderT<float> ladder_;
    float G_ = 0.0f, dG_ = 0.0f, inv_ = 1.0f, dInv_ = 0.0f;
};

// ---- SIMD bank: the filter chains of four voices -------------------------------------------------------
class FilterBank4
{
public:
    void reset() noexcept { core_.reset(); }
    void resetLane(int lane) noexcept { core_.zero(lane); }
    // lanes[i] may be null for an idle voice (processed with neutral settings; its input is zero).
    void prepareBlock(DigitalFilterType type, bool drivePost, const FilterLaneParams* const lanes[4], int len) noexcept;
    F4 process(F4 x) noexcept
    {
        c_.advance(); // same timing as the scalar ramps: the block ends exactly on the target
        return core_.process(x, type_, drivePost_, c_);
    }

private:
    DigitalFilterType type_ = DigitalFilterType::LpLadder24;
    bool drivePost_ = false;
    bool typeSet_ = false;
    FilterCoeffs<F4> c_{};
    VoiceFilterCore<F4> core_;
};

const char* vowelOrderName(int order) noexcept;
constexpr int kNumVowelOrders = 8;

} // namespace kmi
