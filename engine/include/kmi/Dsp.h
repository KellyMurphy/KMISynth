// Small DSP helpers shared across the engine. Header-only, no allocations.
#pragma once

#include <cmath>
#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86_FP)
#include <xmmintrin.h>
#define KMI_HAS_SSE 1
#endif

namespace kmi {

// RAII: enable flush-to-zero / denormals-are-zero, restore the previous mode on exit.
class DenormalGuard
{
public:
#ifdef KMI_HAS_SSE
    DenormalGuard() noexcept : saved_(_mm_getcsr()) { _mm_setcsr(saved_ | 0x8040u); } // FTZ | DAZ
    ~DenormalGuard() noexcept { _mm_setcsr(saved_); }
private:
    unsigned int saved_;
#else
    DenormalGuard() noexcept = default;
#endif
public:
    DenormalGuard(const DenormalGuard&) = delete;
    DenormalGuard& operator=(const DenormalGuard&) = delete;
};

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float clampf(float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerpf(float a, float b, float t) noexcept { return a + (b - a) * t; }

// floor() without a library call (std::floor is not inlined on the SSE2 baseline). |x| < 2^31.
inline float fastFloor(float x) noexcept
{
    const float t = static_cast<float>(static_cast<int>(x));
    return t > x ? t - 1.0f : t;
}
inline float wrap01(float p) noexcept { return p - fastFloor(p); }

// sin(2*pi*phase) for phase in [0, 1); max error ~1e-3. For modulation signals, not audio oscillators.
inline float fastSin01(float phase) noexcept
{
    const float t = phase < 0.5f ? phase : phase - 1.0f; // [-0.5, 0.5)
    float y = 8.0f * t - 16.0f * t * (t < 0.0f ? -t : t);
    y = 0.225f * (y * (y < 0.0f ? -y : y) - y) + y;
    return y;
}
inline float midiToHz(float note) noexcept { return 440.0f * std::exp2((note - 69.0f) * (1.0f / 12.0f)); }

// Rational tanh approximation; monotonic, exactly +-1 beyond |x| >= 3.
inline float softClip(float x) noexcept
{
    if (x >= 3.0f) return 1.0f;
    if (x <= -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Curve shaping of a 0..1 ramp. curve in -1..1: 0 = linear, >0 = fast start (log-like),
// <0 = slow start (exp-like). Used for velocity curves and glide.
inline float shapeCurve(float t, float curve) noexcept
{
    const float k = curve * 6.0f;
    if (std::fabs(k) < 1.0e-3f) return t;
    return (1.0f - std::exp(-k * t)) / (1.0f - std::exp(-k));
}

// xorshift32: cheap, deterministic, real-time safe.
struct Rng
{
    uint32_t state = 0x9E3779B9u;
    void seed(uint32_t s) noexcept { state = s ? s : 0x9E3779B9u; }
    uint32_t nextU32() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    float next01() noexcept { return static_cast<float>(nextU32() >> 8) * (1.0f / 16777216.0f); }
    float bipolar() noexcept { return next01() * 2.0f - 1.0f; }
};

// Tempo-synced note divisions, longest first. Shared by envelopes (quantize), LFOs and the arp/delay later.
struct SyncDivision
{
    const char* name;
    float quarters; // length in quarter notes
};

inline constexpr SyncDivision kSyncDivisions[] = {
    { "48 bars", 192.0f }, { "32 bars", 128.0f }, { "16 bars", 64.0f }, { "8 bars", 32.0f },
    { "4 bars", 16.0f },   { "2 bars", 8.0f },    { "1/1", 4.0f },      { "1/2D", 3.0f },
    { "1/2", 2.0f },       { "1/2T", 4.0f / 3.0f }, { "1/4D", 1.5f },   { "1/4", 1.0f },
    { "1/4T", 2.0f / 3.0f }, { "1/8D", 0.75f },  { "1/8", 0.5f },      { "1/8T", 1.0f / 3.0f },
    { "1/16D", 0.375f },   { "1/16", 0.25f },     { "1/16T", 1.0f / 6.0f }, { "1/32D", 0.1875f },
    { "1/32", 0.125f },    { "1/32T", 1.0f / 12.0f }, { "1/64", 0.0625f }, { "1/64T", 1.0f / 24.0f },
};
inline constexpr int kNumSyncDivisions = static_cast<int>(sizeof(kSyncDivisions) / sizeof(kSyncDivisions[0]));

inline float syncDivisionSeconds(int index, double bpm) noexcept
{
    if (index < 0) index = 0;
    if (index >= kNumSyncDivisions) index = kNumSyncDivisions - 1;
    const double b = bpm > 1.0 ? bpm : 120.0;
    return static_cast<float>(kSyncDivisions[index].quarters * 60.0 / b);
}

// Snap a duration to the nearest tempo division (log distance). 0 stays 0.
inline float quantizeMsToDivision(float ms, double bpm) noexcept
{
    if (ms <= 0.0f) return 0.0f;
    float best = syncDivisionSeconds(0, bpm) * 1000.0f;
    float bestDist = std::fabs(std::log2(ms / best));
    for (int i = 1; i < kNumSyncDivisions; ++i)
    {
        const float d = syncDivisionSeconds(i, bpm) * 1000.0f;
        const float dist = std::fabs(std::log2(ms / d));
        if (dist < bestDist) { bestDist = dist; best = d; }
    }
    return best;
}

} // namespace kmi
