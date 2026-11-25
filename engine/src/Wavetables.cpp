#include "kmi/Wavetables.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <string>

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

// ---- Morphing families ---------------------------------------------------------------------------------------
// Each family is kFramesPerFamily frames of one idea with a control t running 0 -> 1, so Wave Scan through a
// family morphs smoothly. A family is defined either in time (x = phase 0..1) or as sine amplitudes per harmonic.
using TimeFn = float (*)(float x, float t);
using SpecFn = double (*)(int k, float t);
struct FamilyDef
{
    const char* name;
    TimeFn time;
    SpecFn spec;
};

float foldTri(float v)
{
    float u = (v + 1.0f) * 0.25f;
    u -= std::floor(u);
    return u < 0.5f ? 4.0f * u - 1.0f : 3.0f - 4.0f * u;
}

double hashUnit(int a, int b) // deterministic 0..1
{
    uint32_t h = static_cast<uint32_t>(a) * 2654435761u ^ static_cast<uint32_t>(b) * 2246822519u;
    h ^= h >> 15;
    h *= 3266489917u;
    h ^= h >> 13;
    return static_cast<double>(h & 0xFFFFFF) / 16777215.0;
}

// Vowel formants (Hz) on a 110 Hz voice, morphing a -> e -> i -> o -> u.
double vowelAmp(int k, float t)
{
    static const double F[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
    static const double bw[3] = { 90, 110, 160 }, gain[3] = { 1.0, 0.55, 0.3 };
    if (k > 48) return 0.0;
    const double pos = static_cast<double>(t) * 4.0;
    const int v = std::min(3, static_cast<int>(pos));
    const double f = pos - v;
    const double hz = 110.0 * k;
    double a = 0.0;
    for (int j = 0; j < 3; ++j)
    {
        const double fj = F[v][j] + (F[v + 1][j] - F[v][j]) * f;
        const double d = (hz - fj) / bw[j];
        a += gain[j] / (1.0 + d * d);
    }
    return a / std::sqrt(static_cast<double>(k));
}

// Drawbar registrations on 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1' (harmonics 1 2 3 4 5 6 8); the 8' is always drawn.
double organAmp(int k, float t)
{
    static const char* const reg[16] = { "8000000", "8800000", "8080000", "8008000", "8880000", "8808000", "8088000", "8888000",
                                         "6800008", "8060600", "7654321", "8800888", "8848000", "5876000", "8888888", "4808080" };
    static const int harmonic[7] = { 1, 2, 3, 4, 5, 6, 8 };
    const char* r = reg[std::min(15, static_cast<int>(t * 15.0f + 0.5f))];
    for (int d = 0; d < 7; ++d)
        if (harmonic[d] == k) return (r[d] - '0') / 8.0;
    return 0.0;
}

double spectraAmp(int k, float t)
{
    if (k > 64) return 0.0;
    if (k == 1) return 1.0;
    const double pos = static_cast<double>(t) * 3.0;
    const int seg = std::min(2, static_cast<int>(pos));
    const double f = pos - seg;
    const double a = hashUnit(seg, k), b = hashUnit(seg + 1, k);
    const double r = a + (b - a) * f;
    return r * r * std::pow(static_cast<double>(k), -0.6);
}

// Append only: the frames' wave indices are stored in presets.
const FamilyDef kFamilies[] = {
    { "Pulse Width", [](float x, float t) { return x < 0.5f - 0.47f * t ? 1.0f : -1.0f; }, nullptr },
    { "Saw Bright", nullptr, [](int k, float t) { return std::pow(static_cast<double>(k), -(3.0 - 2.1 * t)); } },
    { "Square to Saw", nullptr, [](int k, float t) { return (k % 2 ? 1.0 : static_cast<double>(t)) / k; } },
    { "Harmonic Build", nullptr, [](int k, float t) { return k <= 1 + static_cast<int>(t * 15.0f + 0.5f) ? 1.0 / k : 0.0; } },
    { "Formant Sweep", nullptr, [](int k, float t) {
          const double c = std::pow(48.0, static_cast<double>(t)), w = 1.0 + 0.15 * c, d = (k - c) / w;
          return (k == 1 ? 0.15 : 0.0) + std::exp(-0.5 * d * d);
      } },
    { "Vowel Morph", nullptr, vowelAmp },
    { "FM Index", [](float x, float t) { return std::sin(kTwoPi * x + 10.0f * t * sinT(x)); }, nullptr },
    { "FM Octave", [](float x, float t) { return std::sin(kTwoPi * x + 8.0f * t * harm(x, 2)); }, nullptr },
    { "Hard Sync", [](float x, float t) { return sawT(wrap01((1.0f + 7.0f * t) * x)); }, nullptr },
    { "Wavefold", [](float x, float t) { return foldTri((1.0f + 7.0f * t) * sinT(x)); }, nullptr },
    { "Tube Drive", [](float x, float t) {
          const float g = 1.0f + 24.0f * t * t;
          return std::tanh(g * (sinT(x) + 0.25f)) - std::tanh(g * 0.25f);
      }, nullptr },
    { "Phase Bend", [](float x, float t) {
          const float d = 0.5f - 0.48f * t;
          const float ph = x < d ? 0.5f * x / d : 0.5f + 0.5f * (x - d) / (1.0f - d);
          return -std::cos(kTwoPi * ph);
      }, nullptr },
    { "Resonant Sweep", nullptr, [](int k, float t) { // a saw through a resonant 2-pole low-pass sweeping up
          const double c = 1.5 * std::pow(32.0, static_cast<double>(t)), r = k / c, q = 6.0;
          return 1.0 / (k * std::sqrt((1.0 - r * r) * (1.0 - r * r) + (r / q) * (r / q)));
      } },
    { "Crusher", [](float x, float t) {
          const float levels = std::round(std::exp(std::log(32.0f) * (1.0f - t)));
          const float steps = 256.0f * (1.0f - t) + 12.0f * t;
          const float xq = std::floor(x * steps) / steps;
          return std::round(sinT(xq) * levels) / levels;
      }, nullptr },
    { "Comb Sweep", nullptr, [](int k, float t) {
          const double c = std::cos(3.141592653589793 * k * (0.02 + 0.46 * t));
          return (0.1 + 0.9 * c * c) / k;
      } },
    { "Organ Stops", nullptr, organAmp },
    { "Spectra", nullptr, spectraAmp },
};
constexpr int kNumFamilyDefs = static_cast<int>(sizeof(kFamilies) / sizeof(kFamilies[0]));
static_assert(kNumFamilyDefs == WavetableBank::kNumFamilies, "update kNumFamilies with the family list");
static_assert(kNumDefs == WavetableBank::kUserWaveFirst, "the original waves fill indices 0-49");

// ---- FFT ------------------------------------------------------------------------------------------------------
struct Fft
{
    static constexpr int N = WavetableBank::kTableSize;
    std::vector<std::complex<double>> twiddle; // e^{-2 pi i n / N}
    std::vector<int> rev;
    Fft() : twiddle(N / 2), rev(N)
    {
        for (int n = 0; n < N / 2; ++n) twiddle[static_cast<size_t>(n)] = std::polar(1.0, -2.0 * 3.141592653589793 * n / N);
        int bits = 0;
        while ((1 << bits) < N) ++bits;
        for (int n = 0; n < N; ++n)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b) r |= ((n >> b) & 1) << (bits - 1 - b);
            rev[static_cast<size_t>(n)] = r;
        }
    }
    // In place, unnormalised; inverse uses e^{+2 pi i k n / N}.
    void run(std::complex<double>* a, bool inverse) const
    {
        for (int n = 0; n < N; ++n)
            if (n < rev[static_cast<size_t>(n)]) std::swap(a[n], a[rev[static_cast<size_t>(n)]]);
        for (int len = 2; len <= N; len <<= 1)
        {
            const int half = len / 2, step = N / len;
            for (int i = 0; i < N; i += len)
                for (int j = 0; j < half; ++j)
                {
                    std::complex<double> w = twiddle[static_cast<size_t>(j * step)];
                    if (inverse) w = std::conj(w);
                    const std::complex<double> u = a[i + j], v = a[i + j + half] * w;
                    a[i + j] = u + v;
                    a[i + j + half] = u - v;
                }
        }
    }
};
const Fft& fft()
{
    static const Fft f;
    return f;
}
} // namespace

const WavetableBank& WavetableBank::instance()
{
    static const WavetableBank bank;
    return bank;
}

const char* WavetableBank::waveName(int wave) const noexcept
{
    return names_[static_cast<size_t>(wave < 0 || wave >= kNumWaveIndices ? 0 : wave)].c_str();
}

const char* WavetableBank::waveGroup(int wave) const noexcept
{
    return groups_[static_cast<size_t>(wave < 0 || wave >= kNumWaveIndices ? 0 : wave)].c_str();
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

bool WavetableBank::buildMips(const float* cycle, float* out)
{
    constexpr int N = kTableSize;
    std::vector<std::complex<double>> x(N);
    for (int n = 0; n < N; ++n) x[static_cast<size_t>(n)] = cycle[n];
    fft().run(x.data(), false);
    // X[k] = sum x (cos - i sin): the cosine part is Re X, the sine part -Im X.
    std::vector<double> a(N / 2), b(N / 2);
    for (int k = 1; k < N / 2; ++k)
    {
        a[static_cast<size_t>(k)] = 2.0 * x[static_cast<size_t>(k)].real() / N;
        b[static_cast<size_t>(k)] = -2.0 * x[static_cast<size_t>(k)].imag() / N;
    }
    return buildMipsFromSpectrum(a.data(), b.data(), out);
}

bool WavetableBank::buildMipsFromSpectrum(const double* a, const double* b, float* out)
{
    constexpr int N = kTableSize;
    constexpr int H = N / 2 - 1; // highest harmonic kept (below table Nyquist)
    std::vector<std::complex<double>> z(N);
    for (int m = 0; m < kNumMips; ++m)
    {
        const int limit = m == 0 ? H : (1024 >> m);
        std::fill(z.begin(), z.end(), std::complex<double>());
        // a cos + b sin = Re((a - i b) e^{i theta}): put half on +k and the conjugate on -k.
        for (int k = 1; k <= limit; ++k)
        {
            const std::complex<double> c(0.5 * a[k], -0.5 * b[k]);
            z[static_cast<size_t>(k)] = c;
            z[static_cast<size_t>(N - k)] = std::conj(c);
        }
        fft().run(z.data(), true);
        float* t = out + m * kMipFloats;
        for (int n = 0; n < N; ++n) t[n] = static_cast<float>(z[static_cast<size_t>(n)].real());
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
    data_.assign(static_cast<size_t>(kNumBuiltInWaves) * kTableFloats, 0.0f);
    names_.resize(kNumWaveIndices);
    groups_.resize(kNumWaveIndices);
    std::vector<float> x(kTableSize);
    auto storage = [this](int wave) { return data_.data() + static_cast<size_t>(wave < kUserWaveFirst ? wave : wave - kNumUserWaves) * kTableFloats; };

    for (int w = 0; w < kNumDefs; ++w)
    {
        names_[static_cast<size_t>(w)] = kWaves[w].name;
        groups_[static_cast<size_t>(w)] = kWaves[w].group;
        for (int n = 0; n < kTableSize; ++n) x[static_cast<size_t>(n)] = kWaves[w].fn(static_cast<float>(n) / kTableSize);
        buildMips(x.data(), storage(w));
    }
    for (int u = 0; u < kNumUserWaves; ++u)
    {
        names_[static_cast<size_t>(kUserWaveFirst + u)] = "User " + std::to_string(u + 1);
        groups_[static_cast<size_t>(kUserWaveFirst + u)] = "User";
    }
    std::vector<double> a(kTableSize / 2, 0.0), b(kTableSize / 2, 0.0);
    for (int f = 0; f < kNumFamilies; ++f)
        for (int i = 0; i < kFramesPerFamily; ++i)
        {
            const int w = kFamilyWaveFirst + f * kFramesPerFamily + i;
            const FamilyDef& def = kFamilies[f];
            const float t = static_cast<float>(i) / static_cast<float>(kFramesPerFamily - 1);
            names_[static_cast<size_t>(w)] = std::string(def.name) + " " + std::to_string(i + 1);
            groups_[static_cast<size_t>(w)] = def.name;
            if (def.spec)
            {
                for (int k = 1; k < kTableSize / 2; ++k) b[static_cast<size_t>(k)] = def.spec(k, t);
                buildMipsFromSpectrum(a.data(), b.data(), storage(w));
            }
            else
            {
                for (int n = 0; n < kTableSize; ++n) x[static_cast<size_t>(n)] = def.time(static_cast<float>(n) / kTableSize, t);
                buildMips(x.data(), storage(w));
            }
        }

    // Menu order: groups in order of first appearance, waves by index within a group, then the user slots.
    std::vector<std::string> groups;
    for (int i = 0; i < kNumBuiltInWaves; ++i)
    {
        const std::string& g = groups_[static_cast<size_t>(builtInWave(i))];
        if (std::find(groups.begin(), groups.end(), g) == groups.end()) groups.push_back(g);
    }
    for (const auto& g : groups)
        for (int i = 0; i < kNumBuiltInWaves; ++i)
            if (groups_[static_cast<size_t>(builtInWave(i))] == g) menuOrder_.push_back(builtInWave(i));
    for (int u = 0; u < kNumUserWaves; ++u) menuOrder_.push_back(kUserWaveFirst + u);
    menuPos_.assign(kNumWaveIndices, 0);
    for (size_t p = 0; p < menuOrder_.size(); ++p) menuPos_[static_cast<size_t>(menuOrder_[p])] = static_cast<int>(p);
}

} // namespace kmi
