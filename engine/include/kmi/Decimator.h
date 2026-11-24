// 2x decimating FIR (Kaiser-windowed sinc). Passband to ~0.38 fs_out, stopband from 0.5 fs_out.
#pragma once

namespace kmi {

class Decimator2x
{
public:
    static constexpr int kTaps = 95;

    Decimator2x();
    void reset() noexcept;
    // in: 2 * numOut samples at the oversampled rate; out: numOut samples.
    void process(const float* in, float* out, int numOut) noexcept;

private:
    void push(float x) noexcept
    {
        pos_ = (pos_ == 0 ? kTaps : pos_) - 1;
        buf_[pos_] = buf_[pos_ + kTaps] = x;
    }

    float coeffs_[kTaps] = {};
    float buf_[2 * kTaps] = {};
    int pos_ = 0;
};

} // namespace kmi
