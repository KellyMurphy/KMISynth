#include "kmi/Filters.h"

#include <cmath>

namespace kmi {

namespace {
// Formant centre frequencies (Hz) and relative gains for the five vowels A E I O U.
constexpr float kFormants[5][3] = {
    { 730.0f, 1090.0f, 2440.0f }, // A
    { 530.0f, 1840.0f, 2480.0f }, // E
    { 270.0f, 2290.0f, 3010.0f }, // I
    { 570.0f, 840.0f, 2410.0f },  // O
    { 300.0f, 870.0f, 2240.0f },  // U
};
constexpr float kFormantGain[3] = { 1.0f, 0.55f, 0.3f };

// Our vowel sweep orders (indices into kFormants).
constexpr int kVowelOrders[kNumVowelOrders][5] = {
    { 0, 1, 2, 3, 4 }, { 4, 3, 2, 1, 0 }, { 0, 3, 4, 1, 2 }, { 2, 1, 0, 3, 4 },
    { 3, 0, 4, 2, 1 }, { 1, 2, 4, 3, 0 }, { 4, 0, 1, 3, 2 }, { 0, 2, 3, 1, 4 },
};
constexpr const char* kVowelOrderNames[kNumVowelOrders] = {
    "A-E-I-O-U", "U-O-I-E-A", "A-O-U-E-I", "I-E-A-O-U", "O-A-U-I-E", "E-I-U-O-A", "U-A-E-O-I", "A-I-O-E-U",
};
} // namespace

const char* vowelOrderName(int order) noexcept
{
    return kVowelOrderNames[order < 0 || order >= kNumVowelOrders ? 0 : order];
}

void DigitalFilter::reset() noexcept
{
    for (auto& s : svf_) s.reset();
    for (auto& l : ladder_) l.reset();
    onePole_.reset();
}

void DigitalFilter::prepareBlock(const DigitalFilterBlock& b, float sampleRate) noexcept
{
    if (b.type != blk_.type) reset(); // avoid state blow-ups when switching topology
    blk_ = b;

    const float r = clampf(b.resonance, 0.0f, 1.0f);
    svfK_ = 2.0f - 1.98f * r;
    ladderK_ = 4.1f * r;
    driveGain_ = 1.0f + 15.0f * b.drive * b.drive;
    driveComp_ = 1.0f / std::sqrt(driveGain_);
    gateGain_ = std::sqrt(clampf(b.cutoffNorm, 0.0f, 1.0f));

    using T = DigitalFilterType;
    switch (b.type)
    {
        case T::HpDiode: svfKEff_ = svfK_ * 0.9f; break;
        case T::BpModular: svfKEff_ = svfK_ * 0.8f; break;
        case T::LpDiode: svfKEff_ = svfK_ * 0.85f; break;
        default: svfKEff_ = svfK_; break;
    }
    switch (b.type)
    {
        case T::LpGate: ladderKEff_ = ladderK_ * 0.5f; break;
        case T::LpModular: ladderKEff_ = ladderK_ * 1.05f; break;
        default: ladderKEff_ = ladderK_; break;
    }

    if (b.type == DigitalFilterType::Vowel)
    {
        const int order = b.vowelOrder < 0 || b.vowelOrder >= kNumVowelOrders ? 0 : b.vowelOrder;
        const float pos = clampf(b.cutoffNorm, 0.0f, 1.0f) * 4.0f;
        const int i0 = static_cast<int>(pos);
        const int i1 = i0 >= 4 ? 4 : i0 + 1;
        const float t = pos - static_cast<float>(i0);
        const float shift = std::exp2((clampf(b.morph, 0.0f, 1.0f) - 0.5f) * 2.0f); // +-1 octave
        const int v0 = kVowelOrders[order][i0];
        const int v1 = kVowelOrders[order][i1];
        for (int f = 0; f < 3; ++f)
        {
            const float hz = clampf(lerpf(kFormants[v0][f], kFormants[v1][f], t) * shift, 40.0f, 0.45f * sampleRate);
            vowelG_[f] = hzToG(hz, sampleRate);
            vowelGain_[f] = kFormantGain[f];
        }
        vowelK_ = 0.6f - 0.5f * r; // more resonance -> narrower formants
        for (int f = 0; f < 3; ++f) vowelA1_[f] = Svf::a1For(vowelG_[f], vowelK_);
    }
}

void DigitalFilter::beginRamp(float gFrom, float gTo, int len) noexcept
{
    g_.set(gFrom, gTo, len);
    const float G0 = Ladder::bigG(gFrom), G1 = Ladder::bigG(gTo);
    G_.set(G0, G1, len);
    a1_.set(Svf::a1For(gFrom, svfKEff_), Svf::a1For(gTo, svfKEff_), len);
    inv_.set(Ladder::invDen(G0, ladderKEff_), Ladder::invDen(G1, ladderKEff_), len);
}

float DigitalFilter::applyDrive(float x) const noexcept
{
    if (blk_.drive <= 0.0f) return x;
    return softClip(x * driveGain_) * driveComp_;
}

float DigitalFilter::process(float x) noexcept
{
    using T = DigitalFilterType;
    const T type = blk_.type;
    const bool morphType = type == T::SvfLpBpHp || type == T::SvfLpNotchHp || type == T::Vowel;
    if (!morphType && !blk_.drivePost) x = applyDrive(x);

    const float g = g_.next(), G = G_.next(), a1 = a1_.next(), inv = inv_.next();
    const float k = svfKEff_, lk = ladderKEff_;
    float lp, bp, hp, y = 0.0f;
    float ly[4];

    switch (type)
    {
        case T::SvfLpBpHp:
        case T::SvfLpNotchHp:
        {
            svf_[0].process(x, g, k, a1, lp, bp, hp);
            const float mid = type == T::SvfLpBpHp ? bp * k : lp + hp; // normalised BP / notch
            const float m = clampf(blk_.morph, 0.0f, 1.0f);
            y = m < 0.5f ? lerpf(lp, mid, m * 2.0f) : lerpf(mid, hp, (m - 0.5f) * 2.0f);
            break;
        }
        case T::HpDiode:
            svf_[0].process(x, g, k, a1, lp, bp, hp);
            svf_[0].ic1 = cubicClip(svf_[0].ic1 * 0.7f) * (1.0f / 0.7f); // saturating resonance
            y = hp;
            break;
        case T::HpModular:
            svf_[0].process(x, g, k, a1, lp, bp, hp);
            svf_[1].process(hp, g, 1.6f, lp, bp, y);
            y = cubicClip(y * 0.8f) * 1.25f;
            break;
        case T::Hp12:
            svf_[0].process(x, g, k, a1, lp, bp, hp);
            y = hp;
            break;
        case T::BpModular:
            svf_[0].process(cubicClip(x * 0.8f) * 1.25f, g, k, a1, lp, bp, hp);
            y = bp * k * 1.2f;
            break;
        case T::Bp12:
            svf_[0].process(x, g, k, a1, lp, bp, hp);
            y = bp * k;
            break;
        case T::LpLadder12: ladder_[0].process(x, G, inv, lk, 0.0f, 0.5f, ly); y = ly[1]; break;
        case T::LpLadder24: ladder_[0].process(x, G, inv, lk, 0.0f, 0.5f, ly); y = ly[3]; break;
        case T::LpFat12: ladder_[0].process(x, G, inv, lk, 1.0f, 0.5f, ly); y = ly[1]; break;
        case T::LpFat24: ladder_[0].process(x, G, inv, lk, 1.0f, 0.5f, ly); y = ly[3]; break;
        case T::LpGate:
            ladder_[0].process(x, G, inv, lk, 0.0f, 0.5f, ly);
            y = ly[1] * gateGain_;
            break;
        case T::LpDiode:
            svf_[0].process(x, g, k, a1, lp, bp, hp);
            svf_[0].ic1 = cubicClip(svf_[0].ic1 * 0.6f) * (1.0f / 0.6f);
            y = lp;
            break;
        case T::LpModular: ladder_[0].process(x, G, inv, lk, 0.5f, 1.0f, ly); y = ly[3]; break;
        case T::Lp12:
            svf_[0].process(x, g, k, a1, lp, bp, hp);
            y = lp;
            break;
        case T::Lp1Pole:
        {
            const float v = (x - onePole_.s) * G;
            y = v + onePole_.s;
            onePole_.s = y + v;
            break;
        }
        case T::Lp8Pole:
            ladder_[0].process(x, G, inv, lk, 0.0f, 0.5f, ly);
            ladder_[1].process(ly[3], G, 1.0f, 0.0f, 0.0f, 0.0f, ly);
            y = ly[3];
            break;
        case T::Vowel:
            for (int f = 0; f < 3; ++f)
            {
                svf_[f].process(x, vowelG_[f], vowelK_, vowelA1_[f], lp, bp, hp);
                y += bp * vowelK_ * vowelGain_[f] * 2.0f;
            }
            break;
        default: y = x; break;
    }

    if (!morphType && blk_.drivePost) y = applyDrive(y);
    return y;
}

void AnalogFilter::prepareBlock(float resonance, float preDrive) noexcept
{
    // Self-oscillation begins around resonance 110/128.
    k_ = 4.0f * clampf(resonance, 0.0f, 1.0f) * (128.0f / 110.0f);
    driveGain_ = 0.6f + 9.0f * preDrive * preDrive + 1.5f * preDrive;
    driveComp_ = 1.0f / std::pow(driveGain_, 0.6f);
}

void AnalogFilter::beginRamp(float gFrom, float gTo, int len) noexcept
{
    const float G0 = Ladder::bigG(gFrom), G1 = Ladder::bigG(gTo);
    G_.set(G0, G1, len);
    inv_.set(Ladder::invDen(G0, k_), Ladder::invDen(G1, k_), len);
}

float AnalogFilter::process(float x) noexcept
{
    const float driven = cubicClip(x * driveGain_) * driveComp_;
    float y[4];
    ladder_.process(driven, G_.next(), inv_.next(), k_, 0.15f, 0.7f, y);
    return y[3];
}

} // namespace kmi
