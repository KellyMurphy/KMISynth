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

// ---- Per-lane coefficient computation ------------------------------------------------------------------
void computeDigitalLane(const DigitalFilterBlock& b, float sampleRate, FilterLaneParams& out) noexcept
{
    using T = DigitalFilterType;
    const float r = clampf(b.resonance, 0.0f, 1.0f);
    const float svfK = 2.0f - 1.98f * r;
    const float ladderK = 4.1f * r;

    switch (b.type)
    {
        case T::HpDiode: out.svfK = svfK * 0.9f; break;
        case T::BpModular: out.svfK = svfK * 0.8f; break;
        case T::LpDiode: out.svfK = svfK * 0.85f; break;
        default: out.svfK = svfK; break;
    }
    switch (b.type)
    {
        case T::LpGate: out.ladderK = ladderK * 0.5f; break;
        case T::LpModular: out.ladderK = ladderK * 1.05f; break;
        default: out.ladderK = ladderK; break;
    }
    out.morph = clampf(b.morph, 0.0f, 1.0f);
    out.driveGain = 1.0f + 15.0f * b.drive * b.drive;
    out.driveComp = 1.0f / std::sqrt(out.driveGain);
    out.driveOn = b.drive > 0.0f ? 1.0f : 0.0f;
    out.gateGain = std::sqrt(clampf(b.cutoffNorm, 0.0f, 1.0f));

    if (b.type == T::Vowel)
    {
        const int order = b.vowelOrder < 0 || b.vowelOrder >= kNumVowelOrders ? 0 : b.vowelOrder;
        const float pos = clampf(b.cutoffNorm, 0.0f, 1.0f) * 4.0f;
        const int i0 = static_cast<int>(pos);
        const int i1 = i0 >= 4 ? 4 : i0 + 1;
        const float t = pos - static_cast<float>(i0);
        const float shift = std::exp2((out.morph - 0.5f) * 2.0f); // +-1 octave
        const int v0 = kVowelOrders[order][i0];
        const int v1 = kVowelOrders[order][i1];
        out.vowelK = 0.6f - 0.5f * r; // more resonance -> narrower formants
        for (int f = 0; f < 3; ++f)
        {
            const float hz = clampf(lerpf(kFormants[v0][f], kFormants[v1][f], t) * shift, 40.0f, 0.45f * sampleRate);
            out.vowelG[f] = hzToG(hz, sampleRate);
            out.vowelA1[f] = SvfT<float>::a1For(out.vowelG[f], out.vowelK);
        }
    }
}

void computeAnalogLane(float resonance, float preDrive, FilterLaneParams& out) noexcept
{
    // Self-oscillation begins around resonance 110/128.
    out.aK = 4.0f * clampf(resonance, 0.0f, 1.0f) * (128.0f / 110.0f);
    const float gain = 0.6f + 9.0f * preDrive * preDrive + 1.5f * preDrive;
    if (gain != out.aDriveGain || out.aDriveComp <= 0.0f) // pow only when Pre-Drive moves
    {
        out.aDriveGain = gain;
        out.aDriveComp = 1.0f / std::pow(gain, 0.6f);
    }
}

// ---- Filter core (shared scalar / SIMD maths) ------------------------------------------------------------
template <class V>
V VoiceFilterCore<V>::processDigital(V x, DigitalFilterType type, bool drivePost, const FilterCoeffs<V>& c) noexcept
{
    using T = DigitalFilterType;
    const bool morphType = type == T::SvfLpBpHp || type == T::SvfLpNotchHp || type == T::Vowel;
    // Drive is blended in per lane (driveOn is 0 or 1), so voices with and without drive share a bank.
    auto drive = [&c](V in) { return in + (softClipT(in * c.driveGain) * c.driveComp - in) * c.driveOn; };
    if (!morphType && !drivePost) x = drive(x);

    const V g = c.g, G = c.G, a1 = c.a1, inv = c.inv, k = c.k, lk = c.lk;
    V lp, bp, hp, y(0.0f);
    V ly[4];

    switch (type)
    {
        case T::SvfLpBpHp:
        case T::SvfLpNotchHp:
        {
            svf[0].process(x, g, k, a1, lp, bp, hp);
            const V mid = type == T::SvfLpBpHp ? bp * k : lp + hp; // normalised BP / notch
            // Branchless LP -> mid -> HP crossfade (morph can differ per lane).
            const V a = vclamp(c.morph * V(2.0f), V(0.0f), V(1.0f));
            const V b = vclamp(c.morph * V(2.0f) - V(1.0f), V(0.0f), V(1.0f));
            y = lp + (mid - lp) * a + (hp - mid) * b;
            break;
        }
        case T::HpDiode:
            svf[0].process(x, g, k, a1, lp, bp, hp);
            svf[0].ic1 = cubicClipT(svf[0].ic1 * V(0.7f)) * V(1.0f / 0.7f); // saturating resonance
            y = hp;
            break;
        case T::HpModular:
        {
            svf[0].process(x, g, k, a1, lp, bp, hp);
            const V k2(1.6f);
            svf[1].process(hp, g, k2, SvfT<V>::a1For(g, k2), lp, bp, y);
            y = cubicClipT(y * V(0.8f)) * V(1.25f);
            break;
        }
        case T::Hp12:
            svf[0].process(x, g, k, a1, lp, bp, hp);
            y = hp;
            break;
        case T::BpModular:
            svf[0].process(cubicClipT(x * V(0.8f)) * V(1.25f), g, k, a1, lp, bp, hp);
            y = bp * k * V(1.2f);
            break;
        case T::Bp12:
            svf[0].process(x, g, k, a1, lp, bp, hp);
            y = bp * k;
            break;
        case T::LpLadder12: lad[0].process(x, G, inv, lk, 0.0f, 0.5f, ly); y = ly[1]; break;
        case T::LpLadder24: lad[0].process(x, G, inv, lk, 0.0f, 0.5f, ly); y = ly[3]; break;
        case T::LpFat12: lad[0].process(x, G, inv, lk, 1.0f, 0.5f, ly); y = ly[1]; break;
        case T::LpFat24: lad[0].process(x, G, inv, lk, 1.0f, 0.5f, ly); y = ly[3]; break;
        case T::LpGate:
            lad[0].process(x, G, inv, lk, 0.0f, 0.5f, ly);
            y = ly[1] * c.gateGain;
            break;
        case T::LpDiode:
            svf[0].process(x, g, k, a1, lp, bp, hp);
            svf[0].ic1 = cubicClipT(svf[0].ic1 * V(0.6f)) * V(1.0f / 0.6f);
            y = lp;
            break;
        case T::LpModular: lad[0].process(x, G, inv, lk, 0.5f, 1.0f, ly); y = ly[3]; break;
        case T::Lp12:
            svf[0].process(x, g, k, a1, lp, bp, hp);
            y = lp;
            break;
        case T::Lp1Pole:
        {
            const V v = (x - onePole) * G;
            y = v + onePole;
            onePole = y + v;
            break;
        }
        case T::Lp8Pole:
            lad[0].process(x, G, inv, lk, 0.0f, 0.5f, ly);
            lad[1].process(ly[3], G, V(1.0f), V(0.0f), 0.0f, 0.0f, ly);
            y = ly[3];
            break;
        case T::Vowel:
            for (int f = 0; f < 3; ++f)
            {
                svf[f].process(x, c.vG[f], c.vK, c.vA1[f], lp, bp, hp);
                y += bp * c.vK * V(kFormantGain[f] * 2.0f);
            }
            break;
        default: y = x; break;
    }

    if (!morphType && drivePost) y = drive(y);
    return y;
}

template <class V>
V VoiceFilterCore<V>::processAnalog(V x, const FilterCoeffs<V>& c) noexcept
{
    const V driven = cubicClipT(x * c.aDrive) * c.aComp;
    V y[4];
    analog.process(driven, c.aG, c.aInv, c.aK, 0.15f, 0.7f, y);
    return y[3];
}

template struct VoiceFilterCore<float>;
template struct VoiceFilterCore<F4>;

// ---- Scalar wrappers --------------------------------------------------------------------------------------
void DigitalFilter::prepareBlock(const DigitalFilterBlock& b, float sampleRate) noexcept
{
    if (b.type != blk_.type) core_.reset(); // avoid state blow-ups when switching topology
    blk_ = b;
    computeDigitalLane(b, sampleRate, lane_);
    c_.k = lane_.svfK;
    c_.lk = lane_.ladderK;
    c_.morph = lane_.morph;
    c_.driveGain = lane_.driveGain;
    c_.driveComp = lane_.driveComp;
    c_.driveOn = lane_.driveOn;
    c_.gateGain = lane_.gateGain;
    c_.vK = lane_.vowelK;
    for (int f = 0; f < 3; ++f)
    {
        c_.vG[f] = lane_.vowelG[f];
        c_.vA1[f] = lane_.vowelA1[f];
    }
}

void DigitalFilter::beginRamp(float gFrom, float gTo, int len) noexcept
{
    const float n = static_cast<float>(len);
    const float G0 = LadderT<float>::bigG(gFrom), G1 = LadderT<float>::bigG(gTo);
    const float a0 = SvfT<float>::a1For(gFrom, c_.k), a1 = SvfT<float>::a1For(gTo, c_.k);
    const float i0 = LadderT<float>::invDen(G0, c_.lk), i1 = LadderT<float>::invDen(G1, c_.lk);
    c_.g = gFrom;
    c_.dg = (gTo - gFrom) / n;
    c_.G = G0;
    c_.dG = (G1 - G0) / n;
    c_.a1 = a0;
    c_.da1 = (a1 - a0) / n;
    c_.inv = i0;
    c_.dinv = (i1 - i0) / n;
}

float DigitalFilter::process(float x) noexcept
{
    c_.g += c_.dg;
    c_.G += c_.dG;
    c_.a1 += c_.da1;
    c_.inv += c_.dinv;
    return core_.processDigital(x, blk_.type, blk_.drivePost, c_);
}

void AnalogFilter::prepareBlock(float resonance, float preDrive) noexcept
{
    computeAnalogLane(resonance, preDrive, lane_);
}

void AnalogFilter::beginRamp(float gFrom, float gTo, int len) noexcept
{
    const float n = static_cast<float>(len);
    const float G0 = LadderT<float>::bigG(gFrom), G1 = LadderT<float>::bigG(gTo);
    G_ = G0;
    dG_ = (G1 - G0) / n;
    inv_ = LadderT<float>::invDen(G0, lane_.aK);
    dInv_ = (LadderT<float>::invDen(G1, lane_.aK) - inv_) / n;
}

float AnalogFilter::process(float x) noexcept
{
    G_ += dG_;
    inv_ += dInv_;
    const float driven = cubicClipT(x * lane_.aDriveGain) * lane_.aDriveComp;
    float y[4];
    ladder_.process(driven, G_, inv_, lane_.aK, 0.15f, 0.7f, y);
    return y[3];
}

// ---- SIMD bank -----------------------------------------------------------------------------------------------
void FilterBank4::prepareBlock(DigitalFilterType type, bool drivePost, const FilterLaneParams* const lanes[4], int len) noexcept
{
    if (!typeSet_ || type != type_) core_.reset();
    typeSet_ = true;
    type_ = type;
    drivePost_ = drivePost;

    static const FilterLaneParams idle{};
    const FilterLaneParams* L[4];
    for (int i = 0; i < 4; ++i) L[i] = lanes[i] ? lanes[i] : &idle;

    auto pack = [&](auto field) { return F4::set(field(*L[0]), field(*L[1]), field(*L[2]), field(*L[3])); };
    const float n = static_cast<float>(len);
    float g0[4], g1[4], G0[4], G1[4], a10[4], a11[4], i0[4], i1[4], aG0[4], aG1[4], ai0[4], ai1[4];
    for (int i = 0; i < 4; ++i)
    {
        const FilterLaneParams& p = *L[i];
        g0[i] = p.gFrom;
        g1[i] = p.gTo;
        G0[i] = LadderT<float>::bigG(p.gFrom);
        G1[i] = LadderT<float>::bigG(p.gTo);
        a10[i] = SvfT<float>::a1For(p.gFrom, p.svfK);
        a11[i] = SvfT<float>::a1For(p.gTo, p.svfK);
        i0[i] = LadderT<float>::invDen(G0[i], p.ladderK);
        i1[i] = LadderT<float>::invDen(G1[i], p.ladderK);
        aG0[i] = LadderT<float>::bigG(p.aGFrom);
        aG1[i] = LadderT<float>::bigG(p.aGTo);
        ai0[i] = LadderT<float>::invDen(aG0[i], p.aK);
        ai1[i] = LadderT<float>::invDen(aG1[i], p.aK);
    }
    auto ramp = [n](const float* a, const float* b, F4& value, F4& step) {
        value = F4::load(a);
        step = (F4::load(b) - value) * F4(1.0f / n);
    };
    ramp(g0, g1, c_.g, c_.dg);
    ramp(G0, G1, c_.G, c_.dG);
    ramp(a10, a11, c_.a1, c_.da1);
    ramp(i0, i1, c_.inv, c_.dinv);
    ramp(aG0, aG1, c_.aG, c_.daG);
    ramp(ai0, ai1, c_.aInv, c_.daInv);

    c_.k = pack([](const FilterLaneParams& p) { return p.svfK; });
    c_.lk = pack([](const FilterLaneParams& p) { return p.ladderK; });
    c_.morph = pack([](const FilterLaneParams& p) { return p.morph; });
    c_.driveGain = pack([](const FilterLaneParams& p) { return p.driveGain; });
    c_.driveComp = pack([](const FilterLaneParams& p) { return p.driveComp; });
    c_.driveOn = pack([](const FilterLaneParams& p) { return p.driveOn; });
    c_.gateGain = pack([](const FilterLaneParams& p) { return p.gateGain; });
    c_.level = pack([](const FilterLaneParams& p) { return p.level; });
    c_.vK = pack([](const FilterLaneParams& p) { return p.vowelK; });
    for (int f = 0; f < 3; ++f)
    {
        c_.vG[f] = F4::set(L[0]->vowelG[f], L[1]->vowelG[f], L[2]->vowelG[f], L[3]->vowelG[f]);
        c_.vA1[f] = F4::set(L[0]->vowelA1[f], L[1]->vowelA1[f], L[2]->vowelA1[f], L[3]->vowelA1[f]);
    }
    c_.aK = pack([](const FilterLaneParams& p) { return p.aK; });
    c_.aDrive = pack([](const FilterLaneParams& p) { return p.aDriveGain; });
    c_.aComp = pack([](const FilterLaneParams& p) { return p.aDriveComp; });
}

} // namespace kmi
