#include "kmi/Effects.h"

#include <algorithm>
#include <cmath>

#include "kmi/Dsp.h"

namespace kmi {

namespace {
inline float dbToGain(float db) noexcept { return std::pow(10.0f, db * 0.05f); }
inline float onePoleCoef(float hz, float sr) noexcept { return 1.0f - std::exp(-kTwoPi * hz / sr); }
inline float mixf(float dry, float wet, float mix) noexcept { return dry + (wet - dry) * mix; }
inline float fxMod(const float* fm, int which) noexcept { return fm ? fm[which] : 0.0f; }

// Bipolar tone control on a signal: t < 0 low-passes (darker), t > 0 high-passes (thinner).
struct ToneControl
{
    float coef = 1.0f;
    bool highPass = false, active = false;
    void set(float t, float sr) noexcept
    {
        active = std::fabs(t) > 1.0e-3f;
        highPass = t > 0.0f;
        const float hz = highPass ? 20.0f * std::exp2(t * 6.6f) : 20000.0f * std::exp2(t * 5.3f);
        coef = onePoleCoef(std::min(hz, 0.45f * sr), sr);
    }
    float process(float x, float& z) const noexcept
    {
        if (!active) return x;
        z += (x - z) * coef;
        return highPass ? x - z : z;
    }
};
} // namespace

// ---- DelayLine ---------------------------------------------------------------------------------------
void DelayLine::prepare(int maxDelaySamples)
{
    int size = 2;
    while (size < maxDelaySamples + 4) size <<= 1;
    buf_.assign(static_cast<size_t>(size), 0.0f);
    mask_ = size - 1;
    w_ = 0;
}

void DelayLine::reset() noexcept
{
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    w_ = 0;
}

// ---- Insert effects ------------------------------------------------------------------------------------
void InsertFx::prepare(double sampleRate)
{
    sr_ = static_cast<float>(sampleRate);
    const int len = static_cast<int>(0.06 * sampleRate);
    lineL_.prepare(len);
    lineR_.prepare(len);
    line2L_.prepare(len);
    line2R_.prepare(len);
    reset();
}

void InsertFx::reset() noexcept
{
    lineL_.reset();
    lineR_.reset();
    line2L_.reset();
    line2R_.reset();
    phase_ = phase2_ = 0.0f;
    for (auto& ch : ap_)
        for (float& s : ch) s = 0.0f;
    fbL_ = fbR_ = 0.0f;
    for (int i = 0; i < 4; ++i) zL_[i] = zR_[i] = 0.0f;
    for (int i = 0; i < 2; ++i)
    {
        svfL_[i].reset();
        svfR_[i].reset();
    }
    holdL_ = holdR_ = 0.0f;
    holdAcc_ = 1.0f;
    envDb_ = -120.0f;
    duckDb_ = 0.0f;
    noteDuck_ = false;
}

void InsertFx::process(float* l, float* r, int n, const InsertFxParams& p, const InsertMods& m, float beatPhase,
                       float beatInc) noexcept
{
    if (p.type != lastType_)
    {
        reset();
        lastType_ = p.type;
    }
    switch (p.type)
    {
        case FxType::Chorus: chorus(l, r, n, p.chorus, m, false); break;
        case FxType::Flanger: chorus(l, r, n, p.flanger, m, true); break;
        case FxType::Rotary: rotary(l, r, n, p.rotary, m); break;
        case FxType::Phaser: phaser(l, r, n, p.phaser, m); break;
        case FxType::LoFi: lofi(l, r, n, p.lofi, m); break;
        case FxType::Tremolo: tremolo(l, r, n, p.tremolo, m); break;
        case FxType::Eq: eq(l, r, n, p.eq, m); break;
        case FxType::Compressor: compressor(l, r, n, p.comp, m, beatPhase, beatInc); break;
        case FxType::Distort: distort(l, r, n, p.distort, m); break;
        default: break;
    }
    noteDuck_ = false;
}

void InsertFx::chorus(float* l, float* r, int n, const ModFxParams& p, const InsertMods& m, bool flanger) noexcept
{
    const float rate = clampf(p.rateHz * std::exp2(m.p1 * 4.0f), 0.02f, 10.0f);
    const float depth = clampf(p.depth + m.p2, 0.0f, 1.0f);
    const float mix = clampf(p.mix + m.mix, 0.0f, 1.0f);
    const float base = (flanger ? 0.0006f : 0.008f) * sr_;
    const float swing = (flanger ? 0.004f : 0.006f) * depth * sr_;
    const float fb = clampf(p.feedback, -1.0f, 1.0f) * (flanger ? 0.92f : 0.5f);
    const float inc = rate / sr_;
    const float off = p.offsetDeg / 360.0f;
    for (int i = 0; i < n; ++i)
    {
        phase_ = wrap01(phase_ + inc);
        const float lfoL = std::sin(kTwoPi * phase_);
        const float lfoR = std::sin(kTwoPi * (phase_ + off));
        const float inL = p.stereo ? l[i] : 0.5f * (l[i] + r[i]);
        const float inR = p.stereo ? r[i] : inL;
        const float yL = lineL_.read(base + swing * (0.5f + 0.5f * lfoL));
        const float yR = lineR_.read(base + swing * (0.5f + 0.5f * lfoR));
        lineL_.push(inL + fb * yL);
        lineR_.push(inR + fb * yR);
        l[i] = mixf(l[i], yL, mix);
        r[i] = mixf(r[i], yR, mix);
    }
}

void InsertFx::rotary(float* l, float* r, int n, const RotaryParams& p, const InsertMods& m) noexcept
{
    const float woofHz = clampf(p.wooferHz * std::exp2(m.p1 * 4.0f), 0.02f, 10.0f);
    const float hornHz = clampf(p.hornHz * std::exp2(m.p2 * 4.0f), 0.02f, 10.0f);
    const float mix = clampf(p.mix + m.mix, 0.0f, 1.0f);
    const float xc = onePoleCoef(800.0f, sr_);
    const float bal = clampf(p.balance, -1.0f, 1.0f);
    const float gLow = bal > 0.0f ? 1.0f - bal : 1.0f;
    const float gHigh = bal < 0.0f ? 1.0f + bal : 1.0f;
    const float hd = clampf(p.hiDepth, 0.0f, 1.0f), ld = clampf(p.loDepth, 0.0f, 1.0f);
    for (int i = 0; i < n; ++i)
    {
        const float x = 0.5f * (l[i] + r[i]);
        zL_[0] += (x - zL_[0]) * xc;
        const float low = zL_[0];
        const float high = x - low;

        phase_ = wrap01(phase_ + hornHz / sr_);
        phase2_ = wrap01(phase2_ + woofHz / sr_);
        const float hs = std::sin(kTwoPi * phase_), hc = std::cos(kTwoPi * phase_);
        const float ws = std::sin(kTwoPi * phase2_);

        lineL_.push(high);
        const float hornL = lineL_.read((0.0012f + 0.0005f * hd * (1.0f + hs)) * sr_) * (1.0f - 0.35f * hd * (0.5f + 0.5f * hc));
        const float hornR = lineL_.read((0.0012f + 0.0005f * hd * (1.0f + hc)) * sr_) * (1.0f - 0.35f * hd * (0.5f - 0.5f * hc));
        line2L_.push(low);
        const float woof = line2L_.read((0.001f + 0.0003f * ld * (1.0f + ws)) * sr_);
        const float woofL = woof * (1.0f - 0.25f * ld * (0.5f + 0.5f * ws));
        const float woofR = woof * (1.0f - 0.25f * ld * (0.5f - 0.5f * ws));

        l[i] = mixf(l[i], gLow * woofL + gHigh * hornL, mix);
        r[i] = mixf(r[i], gLow * woofR + gHigh * hornR, mix);
    }
}

void InsertFx::phaser(float* l, float* r, int n, const PhaserParams& p, const InsertMods& m) noexcept
{
    const float rate = clampf(p.rateHz * std::exp2(m.p1 * 4.0f), 0.02f, 10.0f);
    const float depth = clampf(p.depth + m.p2, 0.0f, 1.0f);
    const float mix = clampf(p.mix + m.mix, 0.0f, 1.0f);
    const float fLow = 80.0f * std::exp2(clampf(p.lowFreq, 0.0f, 1.0f) * 5.0f);
    const float span = 1.0f + depth * 5.0f;
    const float fb = clampf(p.feedback, -1.0f, 1.0f) * 0.85f;
    const float inc = rate / sr_;
    const float off = p.offsetDeg / 360.0f;
    float aL = 0.0f, aR = 0.0f;
    auto coef = [&](float lfo01) {
        const float f = std::min(fLow * std::exp2(span * lfo01), 0.45f * sr_);
        const float t = std::tan(kPi * f / sr_);
        return (t - 1.0f) / (t + 1.0f);
    };
    for (int i = 0; i < n; ++i)
    {
        if ((i & 7) == 0)
        {
            aL = coef(0.5f + 0.5f * std::sin(kTwoPi * phase_));
            aR = coef(0.5f + 0.5f * std::sin(kTwoPi * (phase_ + off)));
        }
        phase_ = wrap01(phase_ + inc);
        float xL = l[i] + fbL_ * fb, xR = r[i] + fbR_ * fb;
        for (int s = 0; s < 6; ++s)
        {
            const float yL = aL * xL + ap_[0][s];
            ap_[0][s] = xL - aL * yL;
            xL = yL;
            const float yR = aR * xR + ap_[1][s];
            ap_[1][s] = xR - aR * yR;
            xR = yR;
        }
        fbL_ = xL;
        fbR_ = xR;
        l[i] = mixf(l[i], xL, mix);
        r[i] = mixf(r[i], xR, mix);
    }
}

void InsertFx::lofi(float* l, float* r, int n, const LoFiParams& p, const InsertMods& m) noexcept
{
    const float cutoff = clampf(p.cutoffHz * std::exp2(m.p1 * 6.0f), 20.0f, 0.45f * sr_);
    const float q = clampf(p.resonance + m.p2 * 11.0f, 1.0f, 12.0f);
    const float k = 1.0f / q;
    const float mix = clampf(p.mix + m.mix, 0.0f, 1.0f);
    const float holdInc = clampf(p.sampleRate, 2756.0f, 44100.0f) / sr_;
    const float gOut = dbToGain(clampf(p.outputDb, -6.0f, 36.0f));
    const float g = hzToG(cutoff, sr_);
    const float gTelHp = hzToG(400.0f, sr_), gTelLp = hzToG(std::min(3000.0f, 0.45f * sr_), sr_);
    auto filt = [&](float x, Svf* s) {
        float lp, bp, hp;
        switch (p.filter)
        {
            case LoFiFilter::LowPass: s[0].process(x, g, k, lp, bp, hp); return lp;
            case LoFiFilter::Radio: s[0].process(x, g, k, lp, bp, hp); return bp * std::max(k, 0.25f) * 2.0f;
            case LoFiFilter::Telephone:
                s[0].process(x, gTelHp, 1.4f, lp, bp, hp);
                s[1].process(hp, gTelLp, k, lp, bp, hp);
                return lp;
            case LoFiFilter::HighPass: s[0].process(x, g, k, lp, bp, hp); return hp;
            default: return x;
        }
    };
    for (int i = 0; i < n; ++i)
    {
        holdAcc_ += holdInc;
        if (holdAcc_ >= 1.0f)
        {
            holdAcc_ -= std::floor(holdAcc_);
            holdL_ = l[i];
            holdR_ = r[i];
        }
        const float yL = softClip(filt(holdL_, svfL_) * gOut);
        const float yR = softClip(filt(holdR_, svfR_) * gOut);
        l[i] = mixf(l[i], yL, mix);
        r[i] = mixf(r[i], yR, mix);
    }
}

void InsertFx::tremolo(float* l, float* r, int n, const TremoloParams& p, const InsertMods& m) noexcept
{
    const float rate = clampf(p.rateHz * std::exp2(m.p1 * 4.0f), 0.02f, 10.0f);
    const float depth = clampf(p.depth + m.p2, 0.0f, 1.0f);
    const float mix = clampf(p.mix + m.mix, 0.0f, 1.0f);
    const float pm = clampf(p.pitchMod, 0.0f, 1.0f);
    const float inc = rate / sr_;
    const float off = p.phaseDeg / 360.0f;
    auto wave = [&](float ph) {
        const float s = std::sin(kTwoPi * ph);
        return p.square ? std::tanh(4.0f * s) * 1.0007f : s;
    };
    for (int i = 0; i < n; ++i)
    {
        phase_ = wrap01(phase_ + inc);
        const float lfoL = wave(phase_), lfoR = wave(phase_ + off);
        lineL_.push(l[i]);
        lineR_.push(r[i]);
        float xL = l[i], xR = r[i];
        if (pm > 0.0f)
        {
            xL = lineL_.read((0.002f + 0.0015f * pm * lfoL) * sr_);
            xR = lineR_.read((0.002f + 0.0015f * pm * lfoR) * sr_);
        }
        l[i] = mixf(l[i], xL * (1.0f - depth * (0.5f + 0.5f * lfoL)), mix);
        r[i] = mixf(r[i], xR * (1.0f - depth * (0.5f + 0.5f * lfoR)), mix);
    }
}

void InsertFx::eq(float* l, float* r, int n, const EqParams& p, const InsertMods& m) noexcept
{
    const float gl = dbToGain(clampf(p.lowDb + m.p1 * 30.0f, -36.0f, 24.0f));
    const float gh = dbToGain(clampf(p.highDb + m.p2 * 30.0f, -36.0f, 24.0f));
    const float gm = dbToGain(clampf(p.midDb, -36.0f, 24.0f));
    const float mix = clampf(p.mix + m.mix, 0.0f, 1.0f);
    const float xlo = clampf(p.xoverLoHz, 32.0f, 2000.0f);
    const float xhi = clampf(std::max(p.xoverHiHz, xlo * 1.5f), 512.0f, std::min(16000.0f, 0.45f * sr_));
    const float cLo = onePoleCoef(xlo, sr_), cHi = onePoleCoef(xhi, sr_);
    auto band = [&](float x, float* z) {
        z[0] += (x - z[0]) * cLo;
        z[1] += (x - z[1]) * cHi;
        const float low = z[0], high = x - z[1], mid = z[1] - z[0];
        return low * gl + mid * gm + high * gh;
    };
    for (int i = 0; i < n; ++i)
    {
        l[i] = mixf(l[i], band(l[i], zL_), mix);
        r[i] = mixf(r[i], band(r[i], zR_), mix);
    }
}

void InsertFx::compressor(float* l, float* r, int n, const CompParams& p, const InsertMods& m, float beatPhase,
                          float beatInc) noexcept
{
    const float ratio = clampf(p.ratio + m.p1 * 19.0f, 1.0f, 20.0f);
    const float thr = clampf(p.thresholdDb + m.p2 * 64.0f, -64.0f, 0.0f);
    const float atk = std::exp(-1.0f / (clampf(p.attackMs, 1.0f, 400.0f) * 0.001f * sr_));
    const float rel = std::exp(-1.0f / (clampf(p.releaseMs, 5.0f, 560.0f) * 0.001f * sr_));
    const float makeup = clampf(p.makeupDb, -12.0f, 24.0f);
    const float mix = clampf(p.mix + m.mix, 0.0f, 1.0f);

    if (p.sidechain != CompSidechain::Off)
    {
        // Ducking: each trigger drops the gain to the threshold depth, then it recovers with Release.
        if (p.sidechain == CompSidechain::NoteDuck && noteDuck_) duckDb_ = thr;
        for (int i = 0; i < n; ++i)
        {
            if (p.sidechain == CompSidechain::TempoDuck)
            {
                const float ph = wrap01(beatPhase + static_cast<float>(i) * beatInc);
                if (ph < lastBeatPhase_) duckDb_ = thr;
                lastBeatPhase_ = ph;
            }
            duckDb_ *= rel; // target recovers toward 0 dB
            // Smooth (Attack) into the duck, follow the recovery directly.
            envDb_ = duckDb_ < envDb_ ? duckDb_ + (envDb_ - duckDb_) * atk : duckDb_;
            const float g = dbToGain(envDb_ + makeup);
            l[i] = mixf(l[i], l[i] * g, mix);
            r[i] = mixf(r[i], r[i] * g, mix);
        }
        return;
    }

    for (int i = 0; i < n; ++i)
    {
        const float level = std::max(std::fabs(l[i]), std::fabs(r[i]));
        const float lvlDb = 20.0f * std::log10(level + 1.0e-9f);
        envDb_ = lvlDb + (envDb_ - lvlDb) * (lvlDb > envDb_ ? atk : rel);
        const float over = envDb_ - thr;
        const float gr = over > 0.0f ? -over * (1.0f - 1.0f / ratio) : 0.0f;
        const float g = dbToGain(gr + makeup);
        l[i] = mixf(l[i], l[i] * g, mix);
        r[i] = mixf(r[i], r[i] * g, mix);
    }
}

void InsertFx::distort(float* l, float* r, int n, const DistortParams& p, const InsertMods& m) noexcept
{
    const float drive = clampf(p.drive + m.p1, 0.0f, 1.0f);
    const float tone = clampf(p.tone + m.p2, -1.0f, 1.0f);
    const float mix = clampf(p.mix + m.mix, 0.0f, 1.0f);
    const float pre = 1.0f + drive * drive * 40.0f;
    const float bias = clampf(p.asym, 0.0f, 1.0f) * 0.6f;
    const float curve = clampf(p.curve, 0.0f, 1.0f);
    const float gOut = dbToGain(clampf(p.outputDb, -36.0f, 24.0f)) / (1.0f + drive * 1.5f);
    ToneControl tc;
    tc.set(tone, sr_);
    auto shape = [curve](float x) { return (1.0f - curve) * softClip(x) + curve * clampf(x, -1.0f, 1.0f); };
    const float dc = shape(bias);
    for (int i = 0; i < n; ++i)
    {
        const float yL = tc.process(shape(l[i] * pre + bias) - dc, zL_[0]) * gOut;
        const float yR = tc.process(shape(r[i] * pre + bias) - dc, zR_[0]) * gOut;
        l[i] = mixf(l[i], yL, mix);
        r[i] = mixf(r[i], yR, mix);
    }
}

// ---- Delay -----------------------------------------------------------------------------------------------
void StereoDelay::prepare(double sampleRate)
{
    sr_ = static_cast<float>(sampleRate);
    const int len = static_cast<int>(6.5 * sampleRate); // Reverse reads up to 2x the 3 s delay time
    lineL_.prepare(len);
    lineR_.prepare(len);
    reset();
}

void StereoDelay::reset() noexcept
{
    lineL_.reset();
    lineR_.reset();
    timeSmooth_ = -1.0f;
    fbLpL_ = fbLpR_ = toneL_ = toneR_ = 0.0f;
    revPhase_ = 0.0f;
}

void StereoDelay::process(float* l, float* r, int n, const DelayParams& p, const float* mods, double bpm) noexcept
{
    const float* fm = mods ? mods + dst::FxBase : nullptr;
    if (p.type != lastType_)
    {
        reset();
        lastType_ = p.type;
    }

    float ms = p.bpmSync ? syncDivisionSeconds(p.syncDiv, bpm) * 1000.0f : p.timeMs;
    ms = clampf(ms * std::exp2(fxMod(fm, dst::DelayTime) * 2.0f), 1.0f, p.type == DelayType::LRC ? 2100.0f : 3000.0f);
    const float target = ms * 0.001f * sr_;
    if (timeSmooth_ < 0.0f) timeSmooth_ = target;
    const float smoothC = onePoleCoef(6.0f, sr_);
    const float fb = clampf(p.feedback + fxMod(fm, dst::DelayFeedback), 0.0f, 1.0f) * 0.98f;
    const float fbTone = clampf(p.feedbackTone + fxMod(fm, dst::DelayFeedbackTone), 0.0f, 1.0f);
    const float cFb = onePoleCoef(std::min(800.0f * std::exp2(fbTone * 4.6f), 0.45f * sr_), sr_);
    ToneControl wet;
    wet.set(clampf(p.wetTone + fxMod(fm, dst::DelayWetTone), -1.0f, 1.0f), sr_);
    const float mix = clampf(p.mix + fxMod(fm, dst::DelayMix), 0.0f, 1.0f);

    for (int i = 0; i < n; ++i)
    {
        timeSmooth_ += (target - timeSmooth_) * smoothC;
        const float d = std::max(1.0f, timeSmooth_);
        const float mono = 0.5f * (l[i] + r[i]);
        float wl = 0.0f, wr = 0.0f;
        switch (p.type)
        {
            case DelayType::Mono:
            {
                const float y = lineL_.read(d);
                fbLpL_ += (y - fbLpL_) * cFb;
                lineL_.push(mono + fbLpL_ * fb);
                wl = wr = y;
                break;
            }
            case DelayType::PingPong:
            {
                const float yL = lineL_.read(d), yR = lineR_.read(d);
                fbLpL_ += (yR - fbLpL_) * cFb;
                fbLpR_ += (yL - fbLpR_) * cFb;
                lineL_.push(mono + fbLpL_ * fb);
                lineR_.push(fbLpR_ * fb);
                wl = yL;
                wr = yR;
                break;
            }
            case DelayType::LRC:
            {
                const float y1 = lineL_.read(d), y2 = lineL_.read(2.0f * d), y3 = lineL_.read(3.0f * d);
                fbLpL_ += (y3 - fbLpL_) * cFb;
                lineL_.push(mono + fbLpL_ * fb);
                wl = y1 + 0.7071f * y3;
                wr = y2 + 0.7071f * y3;
                break;
            }
            case DelayType::Reverse:
            {
                // Two Hann-windowed grains, half a grain apart, each reading the last grain backwards.
                revPhase_ += 1.0f / d;
                revPhase_ -= std::floor(revPhase_);
                float yL = 0.0f, yR = 0.0f;
                for (int h = 0; h < 2; ++h)
                {
                    const float t = wrap01(revPhase_ + 0.5f * static_cast<float>(h));
                    const float s = std::sin(kPi * t);
                    const float w = s * s;
                    yL += w * lineL_.read(2.0f * t * d);
                    yR += w * lineR_.read(2.0f * t * d);
                }
                fbLpL_ += (yL - fbLpL_) * cFb;
                fbLpR_ += (yR - fbLpR_) * cFb;
                lineL_.push(l[i] + fbLpL_ * fb);
                lineR_.push(r[i] + fbLpR_ * fb);
                wl = yL;
                wr = yR;
                break;
            }
            case DelayType::Stereo:
            default:
            {
                const float yL = lineL_.read(d), yR = lineR_.read(d);
                fbLpL_ += (yL - fbLpL_) * cFb;
                fbLpR_ += (yR - fbLpR_) * cFb;
                lineL_.push(l[i] + fbLpL_ * fb);
                lineR_.push(r[i] + fbLpR_ * fb);
                wl = yL;
                wr = yR;
                break;
            }
        }
        l[i] = mixf(l[i], wet.process(wl, toneL_), mix);
        r[i] = mixf(r[i], wet.process(wr, toneR_), mix);
    }
}

// ---- Reverb (8-line feedback delay network) -------------------------------------------------------------
namespace {
struct ReverbShape
{
    float lines[Reverb::kLines]; // ms
    float diffusers[4];          // ms
    float diffCoef;
    float modMs;
};
constexpr ReverbShape kShapes[] = {
    { { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 61.9f, 71.5f, 83.1f }, { 4.7f, 3.6f, 12.7f, 9.3f }, 0.70f, 0.25f },      // Hall
    { { 11.3f, 13.7f, 17.9f, 19.3f, 23.1f, 27.7f, 31.3f, 33.9f }, { 2.1f, 3.1f, 5.3f, 7.9f }, 0.75f, 0.10f },       // Plate
    { { 7.1f, 9.7f, 11.3f, 13.9f, 15.1f, 17.3f, 19.7f, 23.3f }, { 1.9f, 2.9f, 4.1f, 6.1f }, 0.65f, 0.08f },         // Room
    { { 47.1f, 59.3f, 67.9f, 79.1f, 89.7f, 97.3f, 111.1f, 127.9f }, { 8.3f, 11.9f, 17.3f, 23.7f }, 0.78f, 1.5f },  // Cloud
};

inline void hadamard8(float* x) noexcept
{
    for (int h = 1; h < 8; h <<= 1)
        for (int i = 0; i < 8; i += h << 1)
            for (int j = i; j < i + h; ++j)
            {
                const float a = x[j], b = x[j + h];
                x[j] = a + b;
                x[j + h] = a - b;
            }
    constexpr float norm = 0.35355339f; // 1/sqrt(8): keeps the matrix orthonormal (lossless)
    for (int i = 0; i < 8; ++i) x[i] *= norm;
}
} // namespace

void Reverb::prepare(double sampleRate)
{
    sr_ = static_cast<float>(sampleRate);
    for (auto& l : lines_) l.prepare(static_cast<int>(0.2 * sampleRate));
    for (auto& d : diffusers_) d.prepare(static_cast<int>(0.03 * sampleRate));
    pre_.prepare(static_cast<int>(0.26 * sampleRate));
    configured_ = false;
    reset();
}

void Reverb::reset() noexcept
{
    for (auto& l : lines_) l.reset();
    for (auto& d : diffusers_) d.reset();
    pre_.reset();
    for (int i = 0; i < kLines; ++i) lp_[i] = hp_[i] = 0.0f;
    toneL_ = toneR_ = 0.0f;
}

void Reverb::configure(ReverbType type) noexcept
{
    const ReverbShape& s = kShapes[static_cast<int>(type) < 4 ? static_cast<int>(type) : 0];
    for (int i = 0; i < kLines; ++i)
    {
        delay_[i] = s.lines[i] * 0.001f * sr_;
        modPhase_[i] = static_cast<float>(i) / kLines;
        modInc_[i] = (0.21f + 0.13f * static_cast<float>(i)) / sr_;
    }
    for (int k = 0; k < 4; ++k) diffDelay_[k] = std::max(1.0f, s.diffusers[k] * 0.001f * sr_);
    diffCoef_ = s.diffCoef;
    modDepth_ = s.modMs * 0.001f * sr_;
    reset();
    type_ = type;
    configured_ = true;
}

void Reverb::process(float* l, float* r, int n, const ReverbParams& p, const float* mods) noexcept
{
    const float* fm = mods ? mods + dst::FxBase : nullptr;
    if (!configured_ || p.type != type_) configure(p.type);

    const bool freeze = p.freeze;
    const float t60 = clampf(p.timeSec * std::exp2(fxMod(fm, dst::ReverbTime) * 3.0f), 0.12f, 90.0f);
    float gain[kLines];
    for (int i = 0; i < kLines; ++i) gain[i] = freeze ? 1.0f : std::pow(10.0f, -3.0f * delay_[i] / (t60 * sr_));
    const float hiDamp = clampf(p.hiDamp + fxMod(fm, dst::ReverbHiDamp), 0.0f, 1.0f);
    const float loDamp = clampf(p.loDamp + fxMod(fm, dst::ReverbLoDamp), 0.0f, 1.0f);
    const float cLp = onePoleCoef(std::min(20000.0f * std::exp2(-hiDamp * 4.3f), 0.45f * sr_), sr_);
    const float cHp = onePoleCoef(10.0f * std::exp2(loDamp * 5.3f), sr_);
    ToneControl tone;
    tone.set(clampf(p.tone + fxMod(fm, dst::ReverbTone), -1.0f, 1.0f), sr_);
    const float mix = clampf(p.mix + fxMod(fm, dst::ReverbMix), 0.0f, 1.0f);
    const float preD = clampf(p.preDelayMs, 0.5f, 250.0f) * 0.001f * sr_;

    for (int i = 0; i < n; ++i)
    {
        float x = freeze ? 0.0f : 0.5f * (l[i] + r[i]);
        pre_.push(x);
        x = pre_.read(preD);
        for (int k = 0; k < 4; ++k)
        {
            const float delayed = diffusers_[static_cast<size_t>(k)].read(diffDelay_[k] - 1.0f);
            const float w = x + diffCoef_ * delayed;
            x = delayed - diffCoef_ * w;
            diffusers_[static_cast<size_t>(k)].push(w);
        }

        float y[kLines], fbv[kLines];
        for (int j = 0; j < kLines; ++j)
        {
            modPhase_[j] += modInc_[j];
            if (modPhase_[j] >= 1.0f) modPhase_[j] -= 1.0f;
            const float mod = modDepth_ * (0.5f + 0.5f * fastSin01(modPhase_[j]));
            y[j] = lines_[static_cast<size_t>(j)].read(delay_[j] + mod - 1.0f);
            float v = y[j];
            if (!freeze)
            {
                lp_[j] += (v - lp_[j]) * cLp;
                v = lp_[j];
                hp_[j] += (v - hp_[j]) * cHp;
                v -= hp_[j];
            }
            fbv[j] = v * gain[j];
        }
        hadamard8(fbv);
        for (int j = 0; j < kLines; ++j)
            lines_[static_cast<size_t>(j)].push(fbv[j] + x * ((j & 1) ? -0.35f : 0.35f));

        const float wl = 0.5f * (y[0] - y[2] + y[4] - y[6]);
        const float wr = 0.5f * (y[1] - y[3] + y[5] - y[7]);
        l[i] = mixf(l[i], tone.process(wl, toneL_), mix);
        r[i] = mixf(r[i], tone.process(wr, toneR_), mix);
    }
}

// ---- Chain ---------------------------------------------------------------------------------------------
void FxChain::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    pre_.prepare(sampleRate);
    post_.prepare(sampleRate);
    delay_.prepare(sampleRate);
    reverb_.prepare(sampleRate);
    beatPhase_ = 0.0;
}

void FxChain::reset() noexcept
{
    pre_.reset();
    post_.reset();
    delay_.reset();
    reverb_.reset();
}

void FxChain::process(float* l, float* r, int n, const FxParams& p, const float* mods, double bpm,
                      double transportPpq) noexcept
{
    const float* fm = mods ? mods + dst::FxBase : nullptr;
    const InsertMods preM{ fxMod(fm, dst::PreFxParam1), fxMod(fm, dst::PreFxParam2), fxMod(fm, dst::PreFxMix) };
    const InsertMods postM{ fxMod(fm, dst::PostFxParam1), fxMod(fm, dst::PostFxParam2), fxMod(fm, dst::PostFxMix) };

    if (transportPpq >= 0.0) beatPhase_ = transportPpq - std::floor(transportPpq);
    const double beatInc = (bpm > 1.0 ? bpm : 120.0) / 60.0 / sampleRate_;
    const float bp = static_cast<float>(beatPhase_), bi = static_cast<float>(beatInc);

    pre_.process(l, r, n, p.pre, preM, bp, bi);
    if (p.delay.enabled) delay_.process(l, r, n, p.delay, mods, bpm);
    if (p.reverb.enabled) reverb_.process(l, r, n, p.reverb, mods);
    post_.process(l, r, n, p.post, postM, bp, bi);

    beatPhase_ += beatInc * n;
    beatPhase_ -= std::floor(beatPhase_);
}

} // namespace kmi
