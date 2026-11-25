// Band-limited single-cycle wavetables, mip-mapped per octave. Built once, then read-only.
#pragma once

#include <string>
#include <vector>

namespace kmi {

class WavetableBank
{
public:
    static constexpr int kTableSize = 2048;
    static constexpr int kNumMips = 11; // mip m holds harmonics up to 1024 >> m (mip 0: 1023)
    static constexpr int kMipFloats = kTableSize + 1;
    static constexpr int kTableFloats = kNumMips * kMipFloats; // one wave, all mips

    // Wave indices: 0-49 the original waves; kUserWaveFirst .. +kNumUserWaves-1 the per-instance User slots
    // (see UserWavetables); then the morphing wavetable families (16 frames each). Indices are stored in
    // presets, so new built-in waves are only ever appended.
    static constexpr int kUserWaveFirst = 50;
    static constexpr int kNumUserWaves = 8;
    static constexpr int kFamilyWaveFirst = kUserWaveFirst + kNumUserWaves;
    static constexpr int kFramesPerFamily = 16;
    static constexpr int kNumFamilies = 17;
    static constexpr int kNumFamilyWaves = kNumFamilies * kFramesPerFamily;
    static constexpr int kNumWaveIndices = kFamilyWaveFirst + kNumFamilyWaves; // the Wave parameter's range
    static constexpr int kNumBuiltInWaves = kUserWaveFirst + kNumFamilyWaves;
    static constexpr bool isUserWave(int wave) noexcept { return wave >= kUserWaveFirst && wave < kFamilyWaveFirst; }
    static constexpr bool isBuiltIn(int wave) noexcept { return wave >= 0 && wave < kNumWaveIndices && !isUserWave(wave); }
    // The i-th built-in wave (0 .. kNumBuiltInWaves-1), skipping the User slots.
    static constexpr int builtInWave(int i) noexcept { return i < kUserWaveFirst ? i : i + kNumUserWaves; }

    static const WavetableBank& instance(); // thread-safe lazy construction

    int numWaves() const noexcept { return kNumBuiltInWaves; } // built-in waves (see builtInWave)
    const char* waveName(int wave) const noexcept;
    const char* waveGroup(int wave) const noexcept; // Basic, Analog, ..., one group per family, User

    // Waveform menu order: built-in waves grouped by first appearance of their group (so the original waves
    // keep menu positions 0-49 and each family follows as its own group), then User 1-8.
    // Wavetable Scan sweeps through the waves in this order.
    int menuSize() const noexcept { return static_cast<int>(menuOrder_.size()); }
    int waveAtMenuPosition(int pos) const noexcept;
    int menuPosition(int wave) const noexcept;

    // Band-limits one cycle (kTableSize samples) into kTableFloats (all mips; DC removed, peak-normalised).
    // Returns false (and writes silence) if the cycle is silent. Not real-time safe.
    static bool buildMips(const float* cycle, float* out);
    // The same from a spectrum: harmonic k (1 .. kTableSize/2-1) is a[k] cos + b[k] sin; a[0], b[0] ignored.
    static bool buildMipsFromSpectrum(const double* a, const double* b, float* out);

    // Table of kTableSize + 1 samples (last = first, for interpolation). Built-in waves only: a user slot
    // or unknown index reads Sine (the voice resolves user slots through UserWavetables).
    const float* table(int wave, int mip) const noexcept
    {
        if (!isBuiltIn(wave)) wave = 0;
        const int slot = wave < kUserWaveFirst ? wave : wave - kNumUserWaves;
        return data_.data() + (static_cast<size_t>(slot) * kNumMips + static_cast<size_t>(mip)) * (kTableSize + 1);
    }

    // Choose the mip whose highest harmonic stays below Nyquist for |inc| cycles/sample.
    static int mipForIncrement(float inc) noexcept
    {
        if (inc < 0.0f) inc = -inc;
        if (inc <= 1.0e-9f) return 0;
        const float maxHarmonic = 0.5f / inc;
        int m = 0;
        while (m < kNumMips - 1 && static_cast<float>(m == 0 ? 1023 : (1024 >> m)) > maxHarmonic) ++m;
        return m;
    }

    // Linear-interpolated read; phase must be in [0, 1).
    static float read(const float* t, float phase) noexcept
    {
        const float pos = phase * static_cast<float>(kTableSize);
        int i = static_cast<int>(pos);
        if (i >= kTableSize) i = kTableSize - 1;
        const float frac = pos - static_cast<float>(i);
        return t[i] + frac * (t[i + 1] - t[i]);
    }

private:
    WavetableBank();
    std::vector<float> data_;
    std::vector<std::string> names_, groups_; // by wave index
    std::vector<int> menuOrder_, menuPos_;
};

} // namespace kmi
