#include "kmi/Wavetables.h"

#include <cmath>

#include "kmi/Dsp.h"

namespace kmi {

namespace {
using WaveFn = float (*)(float);
struct WaveDef
{
    const char* name;
    WaveFn fn;
};

float sinT(float x) { return std::sin(kTwoPi * x); }
float harm(float x, int h) { return std::sin(kTwoPi * x * static_cast<float>(h)); }

// Original starter set. Append only (wave index is stored in presets). The full 300+ set is Phase 2.
const WaveDef kWaves[] = {
    { "Sine", [](float x) { return sinT(x); } },
    { "Triangle", [](float x) { return x < 0.25f ? 4.0f * x : (x < 0.75f ? 2.0f - 4.0f * x : 4.0f * x - 4.0f); } },
    { "TriSaw", [](float x) { return x < 0.85f ? -1.0f + 2.0f * x / 0.85f : 1.0f - 2.0f * (x - 0.85f) / 0.15f; } },
    { "Saw", [](float x) { return x < 0.5f ? 2.0f * x : 2.0f * x - 2.0f; } },
    { "Square", [](float x) { return x < 0.5f ? 1.0f : -1.0f; } },
    { "Pulse 25", [](float x) { return x < 0.25f ? 1.0f : -1.0f; } },
    { "Pulse 12", [](float x) { return x < 0.125f ? 1.0f : -1.0f; } },
    { "Half Sine", [](float x) { return std::fmax(sinT(x), 0.0f) * 2.0f - 0.6366f; } },
    { "Rect Sine", [](float x) { return std::fabs(sinT(x)) * 2.0f - 1.2732f; } },
    { "Octave Pair", [](float x) { return sinT(x) + 0.5f * harm(x, 2); } },
    { "Drawbars", [](float x) {
          return sinT(x) + 0.7f * harm(x, 2) + 0.5f * harm(x, 3) + 0.35f * harm(x, 4) + 0.25f * harm(x, 6) +
                 0.2f * harm(x, 8);
      } },
    { "Cubed Sine", [](float x) { const float s = sinT(x); return s * s * s; } },
    { "Sine Ramp", [](float x) { return 0.6f * sinT(x) + 0.4f * (x < 0.5f ? 2.0f * x : 2.0f * x - 2.0f); } },
    { "Hollow", [](float x) { return sinT(x) + 0.6f * harm(x, 3) + 0.4f * harm(x, 5) + 0.2f * harm(x, 7); } },
    { "Formant", [](float x) { const float w = (1.0f - x) * (1.0f - x); return std::sin(kTwoPi * 5.0f * x) * w; } },
    { "Twin Saw", [](float x) {
          const float a = x < 0.5f ? 2.0f * x : 2.0f * x - 2.0f;
          const float y = wrap01(2.0f * x);
          return 0.6f * a + 0.4f * (y < 0.5f ? 2.0f * y : 2.0f * y - 2.0f);
      } },
    { "Sync Sweep", [](float x) { return std::sin(kTwoPi * 3.5f * x) * (1.0f - x); } },
    { "Stairs", [](float x) { return std::round(sinT(x) * 3.0f) / 3.0f; } },
    { "Soft Square", [](float x) { return std::tanh(4.0f * sinT(x)); } },
    { "Bell Partials", [](float x) {
          return sinT(x) + 0.5f * harm(x, 3) + 0.35f * harm(x, 7) + 0.25f * harm(x, 11) + 0.15f * harm(x, 17);
      } },
};
constexpr int kNumDefs = static_cast<int>(sizeof(kWaves) / sizeof(kWaves[0]));
} // namespace

const WavetableBank& WavetableBank::instance()
{
    static const WavetableBank bank;
    return bank;
}

const char* WavetableBank::waveName(int wave) const noexcept
{
    return kWaves[wave < 0 || wave >= kNumDefs ? 0 : wave].name;
}

WavetableBank::WavetableBank()
{
    constexpr int N = kTableSize;
    constexpr int H = N / 2 - 1; // highest harmonic kept (below table Nyquist)
    numWaves_ = kNumDefs;
    data_.assign(static_cast<size_t>(numWaves_) * kNumMips * (N + 1), 0.0f);

    std::vector<double> cosT(N), sinTab(N), x(N), a(H + 1), b(H + 1), acc(N);
    for (int n = 0; n < N; ++n)
    {
        cosT[n] = std::cos(2.0 * 3.141592653589793 * n / N);
        sinTab[n] = std::sin(2.0 * 3.141592653589793 * n / N);
    }

    for (int w = 0; w < numWaves_; ++w)
    {
        for (int n = 0; n < N; ++n) x[n] = kWaves[w].fn(static_cast<float>(n) / N);

        // Forward DFT (harmonics 1..H; DC is discarded).
        for (int k = 1; k <= H; ++k)
        {
            double re = 0.0, im = 0.0;
            for (int n = 0; n < N; ++n)
            {
                const int j = (k * n) & (N - 1);
                re += x[n] * cosT[j];
                im += x[n] * sinTab[j];
            }
            a[k] = 2.0 * re / N;
            b[k] = 2.0 * im / N;
        }

        // Inverse per mip, built incrementally from the band-limited top level down:
        // accumulate harmonics in ascending order and snapshot at each mip's limit.
        std::fill(acc.begin(), acc.end(), 0.0);
        int done = 0;
        for (int m = kNumMips - 1; m >= 0; --m)
        {
            const int limit = m == 0 ? H : (1024 >> m);
            for (int k = done + 1; k <= limit; ++k)
            {
                if (std::fabs(a[k]) < 1e-9 && std::fabs(b[k]) < 1e-9) continue;
                for (int n = 0; n < N; ++n)
                {
                    const int j = (k * n) & (N - 1);
                    acc[n] += a[k] * cosT[j] + b[k] * sinTab[j];
                }
            }
            done = limit;
            float* t = data_.data() + (static_cast<size_t>(w) * kNumMips + m) * (N + 1);
            for (int n = 0; n < N; ++n) t[n] = static_cast<float>(acc[n]);
            t[N] = t[0];
        }

        // Normalise every mip of this wave by the peak of mip 0.
        const float* top = table(w, 0);
        float peak = 1.0e-9f;
        for (int n = 0; n < N; ++n) peak = std::fmax(peak, std::fabs(top[n]));
        const float gain = 1.0f / peak;
        for (int m = 0; m < kNumMips; ++m)
        {
            float* t = data_.data() + (static_cast<size_t>(w) * kNumMips + m) * (N + 1);
            for (int n = 0; n <= N; ++n) t[n] *= gain;
        }
    }
}

} // namespace kmi
