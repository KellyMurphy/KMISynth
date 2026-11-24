#include "kmi/Decimator.h"

#include <cmath>

namespace kmi {

namespace {
double besselI0(double x)
{
    double sum = 1.0, term = 1.0;
    const double q = x * x / 4.0;
    for (int k = 1; k < 50; ++k)
    {
        term *= q / (static_cast<double>(k) * k);
        sum += term;
        if (term < 1e-12 * sum) break;
    }
    return sum;
}
} // namespace

Decimator2x::Decimator2x()
{
    constexpr double fc = 0.22; // cutoff in cycles/sample at the oversampled rate
    constexpr double beta = 9.0;
    constexpr double pi = 3.141592653589793;
    const double mid = (kTaps - 1) / 2.0;
    const double i0b = besselI0(beta);
    double sum = 0.0;
    double h[kTaps];
    for (int n = 0; n < kTaps; ++n)
    {
        const double t = n - mid;
        const double sinc = t == 0.0 ? 2.0 * fc : std::sin(2.0 * pi * fc * t) / (pi * t);
        const double r = t / mid;
        const double w = besselI0(beta * std::sqrt(std::fmax(0.0, 1.0 - r * r))) / i0b;
        h[n] = sinc * w;
        sum += h[n];
    }
    for (int n = 0; n < kTaps; ++n) coeffs_[n] = static_cast<float>(h[n] / sum);
}

void Decimator2x::reset() noexcept
{
    for (float& v : buf_) v = 0.0f;
    pos_ = 0;
}

void Decimator2x::process(const float* in, float* out, int numOut) noexcept
{
    for (int i = 0; i < numOut; ++i)
    {
        push(in[2 * i]);
        push(in[2 * i + 1]);
        const float* b = buf_ + pos_;
        float acc = 0.0f;
        for (int k = 0; k < kTaps; ++k) acc += coeffs_[k] * b[k];
        out[i] = acc;
    }
}

} // namespace kmi
