#include "kmi/Wavetables.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include "kmi/Dsp.h"

namespace kmi {

namespace {
using WaveFn = float (*)(float);
struct WaveDef
{
    const char* name;
    const char* group;
    WaveFn fn;
};

float sinT(float x) { return std::sin(kTwoPi * x); }
float harm(float x, int h) { return std::sin(kTwoPi * x * static_cast<float>(h)); }
float sawT(float x) { return x < 0.5f ? 2.0f * x : 2.0f * x - 2.0f; }

// Additive helper: harmonics 1..n with weight w(k).
template <class W>
float additive(float x, int n, W w)
{
    float s = 0.0f;
    for (int k = 1; k <= n; ++k)
    {
        const float a = w(k);
        if (a != 0.0f) s += a * harm(x, k);
    }
    return s;
}
float bumpWeight(int k, float centre, float width) { return std::exp(-0.5f * ((k - centre) / width) * ((k - centre) / width)); }

// Smooth deterministic "grain": 32 hashed values joined by cosine interpolation.
float grainValue(int i)
{
    uint32_t h = static_cast<uint32_t>(i & 31) * 2654435761u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return static_cast<float>(h & 0xFFFF) / 32767.5f - 1.0f;
}

// Original waves in named groups. Append only: the wave index is stored in presets.
const WaveDef kWaves[] = {
    { "Sine", "Basic", [](float x) { return sinT(x); } },
    { "Triangle", "Basic", [](float x) { return x < 0.25f ? 4.0f * x : (x < 0.75f ? 2.0f - 4.0f * x : 4.0f * x - 4.0f); } },
    { "TriSaw", "Basic", [](float x) { return x < 0.85f ? -1.0f + 2.0f * x / 0.85f : 1.0f - 2.0f * (x - 0.85f) / 0.15f; } },
    { "Saw", "Basic", [](float x) { return x < 0.5f ? 2.0f * x : 2.0f * x - 2.0f; } },
    { "Square", "Basic", [](float x) { return x < 0.5f ? 1.0f : -1.0f; } },
    { "Pulse 25", "Basic", [](float x) { return x < 0.25f ? 1.0f : -1.0f; } },
    { "Pulse 12", "Basic", [](float x) { return x < 0.125f ? 1.0f : -1.0f; } },
    { "Half Sine", "Shaped", [](float x) { return std::fmax(sinT(x), 0.0f) * 2.0f - 0.6366f; } },
    { "Rect Sine", "Shaped", [](float x) { return std::fabs(sinT(x)) * 2.0f - 1.2732f; } },
    { "Octave Pair", "Additive", [](float x) { return sinT(x) + 0.5f * harm(x, 2); } },
    { "Drawbars", "Additive", [](float x) {
          return sinT(x) + 0.7f * harm(x, 2) + 0.5f * harm(x, 3) + 0.35f * harm(x, 4) + 0.25f * harm(x, 6) +
                 0.2f * harm(x, 8);
      } },
    { "Cubed Sine", "Shaped", [](float x) { const float s = sinT(x); return s * s * s; } },
    { "Sine Ramp", "Shaped", [](float x) { return 0.6f * sinT(x) + 0.4f * (x < 0.5f ? 2.0f * x : 2.0f * x - 2.0f); } },
    { "Hollow", "Additive", [](float x) { return sinT(x) + 0.6f * harm(x, 3) + 0.4f * harm(x, 5) + 0.2f * harm(x, 7); } },
    { "Formant", "Digital", [](float x) { const float w = (1.0f - x) * (1.0f - x); return std::sin(kTwoPi * 5.0f * x) * w; } },
    { "Twin Saw", "Analog", [](float x) {
          const float a = x < 0.5f ? 2.0f * x : 2.0f * x - 2.0f;
          const float y = wrap01(2.0f * x);
          return 0.6f * a + 0.4f * (y < 0.5f ? 2.0f * y : 2.0f * y - 2.0f);
      } },
    { "Sync Sweep", "Digital", [](float x) { return std::sin(kTwoPi * 3.5f * x) * (1.0f - x); } },
    { "Stairs", "Digital", [](float x) { return std::round(sinT(x) * 3.0f) / 3.0f; } },
    { "Soft Square", "Analog", [](float x) { return std::tanh(4.0f * sinT(x)); } },
    { "Bell Partials", "Additive", [](float x) {
          return sinT(x) + 0.5f * harm(x, 3) + 0.35f * harm(x, 7) + 0.25f * harm(x, 11) + 0.15f * harm(x, 17);
      } },

    // ---- 20-49 ----
    { "Soft Saw", "Analog", [](float x) { return additive(x, 64, [](int k) { return std::pow(static_cast<float>(k), -1.45f); }); } },
    { "Pulse 40", "Analog", [](float x) { return x < 0.4f ? 1.0f : -1.0f; } },
    { "Pulse 5", "Analog", [](float x) { return x < 0.05f ? 1.0f : -1.0f; } },
    { "Saw Stack", "Analog", [](float x) { return sawT(x) + 0.5f * sawT(wrap01(2.0f * x)) + 0.33f * sawT(wrap01(3.0f * x)); } },
    { "Exp Ramp", "Analog", [](float x) { return (std::exp(3.0f * x) - 1.0f) / (std::exp(3.0f) - 1.0f) * 2.0f - 1.0f; } },

    { "Odd Soft", "Additive", [](float x) { return additive(x, 31, [](int k) { return k % 2 ? 1.0f / static_cast<float>(k * k) * 4.0f : 0.0f; }); } },
    { "Organ 8+4", "Additive", [](float x) { return sinT(x) + 0.8f * harm(x, 2) + 0.5f * harm(x, 4); } },
    { "Organ Full", "Additive", [](float x) {
          return sinT(x) + 0.9f * harm(x, 2) + 0.8f * harm(x, 3) + 0.7f * harm(x, 4) + 0.5f * harm(x, 5) + 0.5f * harm(x, 6) +
                 0.4f * harm(x, 8);
      } },
    { "Reed", "Additive", [](float x) {
          return additive(x, 16, [](int k) { return (k % 2 ? 1.0f : 0.35f) / std::sqrt(static_cast<float>(k)); });
      } },
    { "Strings", "Additive", [](float x) {
          return additive(x, 40, [](int k) { return (1.0f / static_cast<float>(k)) * (1.0f + 1.5f * bumpWeight(k, 7.0f, 2.5f)); });
      } },
    { "Choir Ah", "Additive", [](float x) {
          return additive(x, 32, [](int k) { return bumpWeight(k, 1.0f, 1.0f) + 0.8f * bumpWeight(k, 3.0f, 1.0f) + 0.4f * bumpWeight(k, 9.0f, 2.0f); });
      } },
    { "Choir Oo", "Additive", [](float x) {
          return additive(x, 24, [](int k) { return bumpWeight(k, 1.0f, 0.8f) + 0.5f * bumpWeight(k, 2.0f, 0.7f) + 0.25f * bumpWeight(k, 6.0f, 1.5f); });
      } },
    { "Square Partials", "Additive", [](float x) { return sinT(x) + 0.6f * harm(x, 4) + 0.4f * harm(x, 9) + 0.3f * harm(x, 16) + 0.2f * harm(x, 25); } },
    { "Prime Partials", "Additive", [](float x) {
          return sinT(x) + 0.6f * harm(x, 2) + 0.5f * harm(x, 3) + 0.4f * harm(x, 5) + 0.3f * harm(x, 7) + 0.25f * harm(x, 11) +
                 0.2f * harm(x, 13);
      } },
    { "Even Partials", "Additive", [](float x) { return sinT(x) + additive(x, 24, [](int k) { return k % 2 ? 0.0f : 1.2f / static_cast<float>(k); }); } },

    { "Sine Fold", "Shaped", [](float x) { return std::sin(3.0f * sinT(x)); } },
    { "Sine Clip", "Shaped", [](float x) { return std::fmax(-1.0f, std::fmin(1.0f, 1.8f * sinT(x))); } },
    { "Parabola", "Shaped", [](float x) { return 1.0f - 8.0f * (x - 0.5f) * (x - 0.5f); } },
    { "Half Saw", "Shaped", [](float x) { return std::fmax(sawT(x), 0.0f) * 2.0f - 0.5f; } },
    { "Lean Triangle", "Shaped", [](float x) { return x < 0.2f ? -1.0f + 2.0f * x / 0.2f : 1.0f - 2.0f * (x - 0.2f) / 0.8f; } },
    { "Bump", "Shaped", [](float x) { return 2.0f * std::exp(-((x - 0.5f) / 0.1f) * ((x - 0.5f) / 0.1f)) - 0.35f; } },
    { "Sine Squash", "Shaped", [](float x) { const float s = sinT(x); return (s < 0.0f ? -1.0f : 1.0f) * std::pow(std::fabs(s), 0.3f); } },
    { "Sine Skew", "Shaped", [](float x) { return std::sin(kTwoPi * std::pow(x, 1.6f)); } },

    { "Stairs 8", "Digital", [](float x) { return std::round(sinT(x) * 8.0f) / 8.0f; } },
    { "Step Saw", "Digital", [](float x) { return std::floor(x * 8.0f) / 3.5f - 1.0f; } },
    { "Pulse Train", "Digital", [](float x) { return (x < 0.1f || (x >= 0.5f && x < 0.6f)) ? 1.0f : -0.25f; } },
    { "Sync 2.5", "Digital", [](float x) { return std::sin(kTwoPi * 2.5f * x); } },
    { "Grain", "Digital", [](float x) {
          const float pos = x * 32.0f;
          const int i = static_cast<int>(pos);
          const float f = 0.5f - 0.5f * std::cos(3.14159265f * (pos - static_cast<float>(i)));
          return grainValue(i) + (grainValue(i + 1) - grainValue(i)) * f;
      } },

    { "FM Bright", "FM", [](float x) { return std::sin(kTwoPi * x + 3.0f * sinT(x)); } },
    { "FM Hollow", "FM", [](float x) { return std::sin(kTwoPi * x + 2.0f * harm(x, 3)); } },
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
    static const char* const user[kNumUserWaves] = { "User 1", "User 2", "User 3", "User 4",
                                                     "User 5", "User 6", "User 7", "User 8" };
    if (isUserWave(wave)) return user[wave - kUserWaveFirst];
    return kWaves[wave < 0 || wave >= kNumDefs ? 0 : wave].name;
}

const char* WavetableBank::waveGroup(int wave) const noexcept
{
    if (isUserWave(wave)) return "User";
    return kWaves[wave < 0 || wave >= kNumDefs ? 0 : wave].group;
}

int WavetableBank::waveAtMenuPosition(int pos) const noexcept
{
    const int n = menuSize();
    return menuOrder_[static_cast<size_t>(pos < 0 ? 0 : (pos >= n ? n - 1 : pos))];
}

int WavetableBank::menuPosition(int wave) const noexcept
{
    return wave >= 0 && wave < static_cast<int>(menuPos_.size()) ? menuPos_[static_cast<size_t>(wave)] : 0;
}

namespace {
struct DftTables
{
    static constexpr int N = WavetableBank::kTableSize;
    std::vector<double> cosT, sinT;
    DftTables() : cosT(N), sinT(N)
    {
        for (int n = 0; n < N; ++n)
        {
            cosT[static_cast<size_t>(n)] = std::cos(2.0 * 3.141592653589793 * n / N);
            sinT[static_cast<size_t>(n)] = std::sin(2.0 * 3.141592653589793 * n / N);
        }
    }
};
} // namespace

bool WavetableBank::buildMips(const float* cycle, float* out)
{
    static const DftTables tabs;
    constexpr int N = kTableSize;
    constexpr int H = N / 2 - 1; // highest harmonic kept (below table Nyquist)
    std::vector<double> a(H + 1), b(H + 1), acc(N, 0.0);

    // Forward DFT (harmonics 1..H; DC is discarded).
    for (int k = 1; k <= H; ++k)
    {
        double re = 0.0, im = 0.0;
        for (int n = 0; n < N; ++n)
        {
            const auto j = static_cast<size_t>((k * n) & (N - 1));
            re += cycle[n] * tabs.cosT[j];
            im += cycle[n] * tabs.sinT[j];
        }
        a[static_cast<size_t>(k)] = 2.0 * re / N;
        b[static_cast<size_t>(k)] = 2.0 * im / N;
    }

    // Inverse per mip, built incrementally from the band-limited top level down:
    // accumulate harmonics in ascending order and snapshot at each mip's limit.
    int done = 0;
    for (int m = kNumMips - 1; m >= 0; --m)
    {
        const int limit = m == 0 ? H : (1024 >> m);
        for (int k = done + 1; k <= limit; ++k)
        {
            const double ak = a[static_cast<size_t>(k)], bk = b[static_cast<size_t>(k)];
            if (std::fabs(ak) < 1e-9 && std::fabs(bk) < 1e-9) continue;
            for (int n = 0; n < N; ++n)
            {
                const auto j = static_cast<size_t>((k * n) & (N - 1));
                acc[static_cast<size_t>(n)] += ak * tabs.cosT[j] + bk * tabs.sinT[j];
            }
        }
        done = limit;
        float* t = out + m * kMipFloats;
        for (int n = 0; n < N; ++n) t[n] = static_cast<float>(acc[static_cast<size_t>(n)]);
        t[N] = t[0];
    }

    // Normalise every mip by the peak of mip 0.
    float peak = 0.0f;
    for (int n = 0; n < N; ++n) peak = std::fmax(peak, std::fabs(out[n]));
    const bool audible = peak > 1.0e-6f;
    const float gain = audible ? 1.0f / peak : 0.0f;
    for (int n = 0; n < kTableFloats; ++n) out[n] *= gain;
    return audible;
}

WavetableBank::WavetableBank()
{
    static_assert(kNumDefs <= kUserWaveFirst, "built-in waves past index 49 go after the user slots");
    numWaves_ = kNumDefs;
    data_.assign(static_cast<size_t>(numWaves_) * kTableFloats, 0.0f);
    std::vector<float> x(kTableSize);
    for (int w = 0; w < numWaves_; ++w)
    {
        for (int n = 0; n < kTableSize; ++n) x[static_cast<size_t>(n)] = kWaves[w].fn(static_cast<float>(n) / kTableSize);
        buildMips(x.data(), data_.data() + static_cast<size_t>(w) * kTableFloats);
    }

    // Menu order: groups in order of first appearance, waves by index within a group, then the user slots.
    std::vector<const char*> groups;
    for (int w = 0; w < numWaves_; ++w)
    {
        bool seen = false;
        for (const char* g : groups) seen = seen || std::strcmp(g, kWaves[w].group) == 0;
        if (!seen) groups.push_back(kWaves[w].group);
    }
    for (const char* g : groups)
        for (int w = 0; w < numWaves_; ++w)
            if (std::strcmp(g, kWaves[w].group) == 0) menuOrder_.push_back(w);
    for (int u = 0; u < kNumUserWaves; ++u) menuOrder_.push_back(kUserWaveFirst + u);
    menuPos_.assign(kNumWaveIndices, 0);
    for (size_t p = 0; p < menuOrder_.size(); ++p) menuPos_[static_cast<size_t>(menuOrder_[p])] = static_cast<int>(p);
}

} // namespace kmi
