// Minimal 4-lane float vector for processing four voices at once. SSE2 on x86/x64 (the R-RT-6
// baseline); a plain-array fallback elsewhere. DSP code is written as templates over V = float | F4
// so the scalar and SIMD paths share one implementation.
#pragma once

#include "kmi/Dsp.h"

namespace kmi {

#ifdef KMI_HAS_SSE
struct F4
{
    __m128 v;
    F4() noexcept : v(_mm_setzero_ps()) {}
    F4(float s) noexcept : v(_mm_set1_ps(s)) {} // implicit broadcast: lets "x * 0.5f" work
    explicit F4(__m128 x) noexcept : v(x) {}
    static F4 set(float a, float b, float c, float d) noexcept { return F4(_mm_setr_ps(a, b, c, d)); }
    static F4 load(const float* p) noexcept { return F4(_mm_loadu_ps(p)); }
    void store(float* p) const noexcept { _mm_storeu_ps(p, v); }
    float sum() const noexcept
    {
        __m128 s = _mm_add_ps(v, _mm_movehl_ps(v, v));
        s = _mm_add_ss(s, _mm_shuffle_ps(s, s, 1));
        return _mm_cvtss_f32(s);
    }
    // Zero one lane (used to reset the filter state of a single voice).
    void zeroLane(int lane) noexcept
    {
        alignas(16) float t[4];
        _mm_store_ps(t, v);
        t[lane & 3] = 0.0f;
        v = _mm_load_ps(t);
    }
    friend F4 operator+(F4 a, F4 b) noexcept { return F4(_mm_add_ps(a.v, b.v)); }
    friend F4 operator-(F4 a, F4 b) noexcept { return F4(_mm_sub_ps(a.v, b.v)); }
    friend F4 operator*(F4 a, F4 b) noexcept { return F4(_mm_mul_ps(a.v, b.v)); }
    friend F4 operator/(F4 a, F4 b) noexcept { return F4(_mm_div_ps(a.v, b.v)); }
    friend F4 operator-(F4 a) noexcept { return F4(_mm_sub_ps(_mm_setzero_ps(), a.v)); }
    F4& operator+=(F4 b) noexcept { v = _mm_add_ps(v, b.v); return *this; }
    F4& operator-=(F4 b) noexcept { v = _mm_sub_ps(v, b.v); return *this; }
    F4& operator*=(F4 b) noexcept { v = _mm_mul_ps(v, b.v); return *this; }
};
inline F4 vmin(F4 a, F4 b) noexcept { return F4(_mm_min_ps(a.v, b.v)); }
inline F4 vmax(F4 a, F4 b) noexcept { return F4(_mm_max_ps(a.v, b.v)); }
#else
struct F4
{
    float x[4];
    F4() noexcept : x{ 0, 0, 0, 0 } {}
    F4(float s) noexcept : x{ s, s, s, s } {}
    static F4 set(float a, float b, float c, float d) noexcept { F4 r; r.x[0] = a; r.x[1] = b; r.x[2] = c; r.x[3] = d; return r; }
    static F4 load(const float* p) noexcept { return set(p[0], p[1], p[2], p[3]); }
    void store(float* p) const noexcept { for (int i = 0; i < 4; ++i) p[i] = x[i]; }
    float sum() const noexcept { return x[0] + x[1] + x[2] + x[3]; }
    void zeroLane(int lane) noexcept { x[lane & 3] = 0.0f; }
#define KMI_F4_OP(op)                                                                  \
    friend F4 operator op(F4 a, F4 b) noexcept                                         \
    {                                                                                  \
        F4 r;                                                                          \
        for (int i = 0; i < 4; ++i) r.x[i] = a.x[i] op b.x[i];                         \
        return r;                                                                      \
    }
    KMI_F4_OP(+) KMI_F4_OP(-) KMI_F4_OP(*) KMI_F4_OP(/)
#undef KMI_F4_OP
    friend F4 operator-(F4 a) noexcept { return F4(0.0f) - a; }
    F4& operator+=(F4 b) noexcept { return *this = *this + b; }
    F4& operator-=(F4 b) noexcept { return *this = *this - b; }
    F4& operator*=(F4 b) noexcept { return *this = *this * b; }
};
inline F4 vmin(F4 a, F4 b) noexcept { F4 r; for (int i = 0; i < 4; ++i) r.x[i] = a.x[i] < b.x[i] ? a.x[i] : b.x[i]; return r; }
inline F4 vmax(F4 a, F4 b) noexcept { F4 r; for (int i = 0; i < 4; ++i) r.x[i] = a.x[i] > b.x[i] ? a.x[i] : b.x[i]; return r; }
#endif

inline float vmin(float a, float b) noexcept { return a < b ? a : b; }
inline float vmax(float a, float b) noexcept { return a > b ? a : b; }
inline void zeroLane(float& v, int) noexcept { v = 0.0f; }
inline void zeroLane(F4& v, int lane) noexcept { v.zeroLane(lane); }

template <class V>
inline V vclamp(V x, V lo, V hi) noexcept { return vmin(vmax(x, lo), hi); }

// Division-free cubic soft clip; exactly +-1 beyond |x| >= 1.5.
template <class V>
inline V cubicClipT(V x) noexcept
{
    x = vclamp(x, V(-1.5f), V(1.5f));
    return x - V(4.0f / 27.0f) * x * x * x;
}

// Rational tanh approximation; exactly +-1 beyond |x| >= 3.
template <class V>
inline V softClipT(V x) noexcept
{
    x = vclamp(x, V(-3.0f), V(3.0f));
    const V x2 = x * x;
    return x * (V(27.0f) + x2) / (V(27.0f) + V(9.0f) * x2);
}

} // namespace kmi
