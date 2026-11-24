// Band-limited single-cycle wavetables, mip-mapped per octave. Built once, then read-only.
#pragma once

#include <vector>

namespace kmi {

class WavetableBank
{
public:
    static constexpr int kTableSize = 2048;
    static constexpr int kNumMips = 11; // mip m holds harmonics up to 1024 >> m (mip 0: 1023)

    static const WavetableBank& instance(); // thread-safe lazy construction

    int numWaves() const noexcept { return numWaves_; }
    const char* waveName(int wave) const noexcept;

    // Table of kTableSize + 1 samples (last = first, for interpolation).
    const float* table(int wave, int mip) const noexcept
    {
        if (wave < 0 || wave >= numWaves_) wave = 0;
        return data_.data() + (static_cast<size_t>(wave) * kNumMips + static_cast<size_t>(mip)) * (kTableSize + 1);
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
    int numWaves_ = 0;
    std::vector<float> data_;
};

} // namespace kmi
