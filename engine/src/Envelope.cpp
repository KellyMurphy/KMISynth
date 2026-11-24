#include "kmi/Envelope.h"

#include <cmath>

#include "kmi/Dsp.h"

namespace kmi {

namespace {
constexpr float kSlowDelay = 60.0f / 32.0f;   // Delay: 32 s fast -> 60 s slow
constexpr float kSlowAttack = 600.0f / 36.0f; // Attack/Hold: 36 s -> 600 s
constexpr float kSlowDecay = 900.0f / 60.0f;  // Decay/Release: 60 s -> 900 s

inline float timeToInc(float ms, float sampleRate) noexcept
{
    return ms <= 0.0f ? 2.0f : 1000.0f / (ms * sampleRate);
}

inline int idx(Envelope::Stage s) noexcept { return static_cast<int>(s); }

float velocityScale(float amount, float curve, float velocity) noexcept
{
    const float v = shapeCurve(clampf(velocity, 0.0f, 1.0f), curve);
    if (amount >= 0.0f) return 1.0f - amount + amount * v;
    return 1.0f + amount * v; // negative: harder playing -> smaller envelope
}
} // namespace

void Envelope::reset() noexcept
{
    stage_ = Stage::Idle;
    t_ = 0.0f;
    out_ = 0.0f;
    startLevel_ = 0.0f;
    curved_ = false;
    held_ = false;
    releasePending_ = false;
}

void Envelope::update(const EnvParams& p, const EnvMods& m, float sampleRate, double bpm) noexcept
{
    auto segMs = [&](float ms, float slowMul, float mul) {
        float v = ms * (p.slow ? slowMul : 1.0f) * mul;
        if (p.bpmSync) v = quantizeMsToDivision(v, bpm);
        return v;
    };

    inc_[idx(Stage::Delay)] = timeToInc(segMs(p.delayMs, kSlowDelay, 1.0f), sampleRate);
    inc_[idx(Stage::Attack)] = timeToInc(segMs(p.attackMs, kSlowAttack, m.attackMul), sampleRate);
    inc_[idx(Stage::Hold)] = timeToInc(segMs(p.holdMs, kSlowAttack, 1.0f), sampleRate);
    inc_[idx(Stage::Decay)] = timeToInc(segMs(p.decayMs, kSlowDecay, m.decayMul), sampleRate);
    inc_[idx(Stage::Release)] = timeToInc(segMs(p.releaseMs, kSlowDecay, m.releaseMul), sampleRate);

    attackCurve_ = clampf(p.attackCurve, -1.0f, 1.0f);
    decayCurve_ = clampf(p.decayCurve, -1.0f, 1.0f);
    releaseCurve_ = clampf(p.releaseCurve, -1.0f, 1.0f);
    sustain_ = clampf(p.sustain + m.sustainAdd, 0.0f, 1.0f);
    level_ = clampf(p.level + m.levelAdd, 0.0f, 1.0f);
    scale_ = velScale_ * level_;
    freerun_ = p.freerun;
    reset_ = p.reset;
    loopMode_ = p.loop;
    stageLoop_ = p.stageLoop;

    // Keep the incremental exponential in step with a changed segment rate.
    if (curved_) eMul_ = std::exp(-k_ * inc_[idx(stage_)]);
}

void Envelope::noteOn(const EnvParams& p, float velocity, bool voiceWasActive, bool otherNotesHeld) noexcept
{
    velScale_ = velocityScale(p.velAmount, p.velCurve, velocity);
    scale_ = velScale_ * level_;
    held_ = true;
    releasePending_ = false;
    loopsLeft_ = (p.loop >= 2 && p.loop <= 50) ? p.loop - 1 : 0;

    const bool running = stage_ != Stage::Idle && stage_ != Stage::Release;
    if (p.legato && voiceWasActive && otherNotesHeld && running)
        return; // legato: keep running, no retrigger

    if (p.reset || stage_ == Stage::Idle)
        out_ = 0.0f; // Reset: restart from zero. Otherwise the attack starts from the current level.
    enter(Stage::Delay);
}

void Envelope::noteOff() noexcept
{
    held_ = false;
    if (stage_ == Stage::Idle || stage_ == Stage::Release) return;
    if (freerun_ && stage_ != Stage::Sustain)
    {
        releasePending_ = true; // finish D-A-H-D first
        return;
    }
    enter(Stage::Release);
}

void Envelope::trigger() noexcept
{
    if (reset_) out_ = 0.0f;
    enter(Stage::Delay);
}

void Envelope::enter(Stage s) noexcept
{
    stage_ = s;
    t_ = 0.0f;
    switch (s)
    {
        case Stage::Attack: startLevel_ = out_; setupCurve(attackCurve_); break;
        case Stage::Decay: setupCurve(decayCurve_); break;
        case Stage::Release: startLevel_ = out_; setupCurve(releaseCurve_); break;
        default: curved_ = false; break;
    }
}

void Envelope::setupCurve(float curve) noexcept
{
    k_ = curve * 6.0f;
    curved_ = std::fabs(k_) > 1.0e-3f;
    if (!curved_) return;
    e_ = 1.0f;
    eMul_ = std::exp(-k_ * inc_[idx(stage_)]);
    norm_ = 1.0f / (1.0f - std::exp(-k_));
}

float Envelope::shape() noexcept
{
    if (!curved_) return t_;
    e_ *= eMul_;
    return clampf((1.0f - e_) * norm_, 0.0f, 1.0f);
}

bool Envelope::tryLoop(Stage finished) noexcept
{
    if (loopMode_ == 0 || !(held_ || freerun_)) return false;
    const Stage loopEnd = stageLoop_ == 0 ? Stage::Attack : (stageLoop_ == 1 ? Stage::Hold : Stage::Decay);
    if (finished != loopEnd) return false;
    if (loopMode_ != kEnvLoopInfinite)
    {
        if (loopsLeft_ <= 0) return false;
        --loopsLeft_;
    }
    out_ = 0.0f; // each pass restarts from zero
    enter(Stage::Delay);
    return true;
}

float Envelope::tick() noexcept
{
    switch (stage_)
    {
        case Stage::Idle:
            out_ = 0.0f;
            break;

        case Stage::Delay:
            t_ += inc_[idx(Stage::Delay)];
            if (t_ >= 1.0f) enter(Stage::Attack);
            break;

        case Stage::Attack:
            t_ += inc_[idx(Stage::Attack)];
            if (t_ >= 1.0f)
            {
                out_ = 1.0f;
                if (!tryLoop(Stage::Attack)) enter(Stage::Hold);
            }
            else
            {
                out_ = startLevel_ + (1.0f - startLevel_) * shape();
            }
            break;

        case Stage::Hold:
            t_ += inc_[idx(Stage::Hold)];
            out_ = 1.0f;
            if (t_ >= 1.0f && !tryLoop(Stage::Hold)) enter(Stage::Decay);
            break;

        case Stage::Decay:
            t_ += inc_[idx(Stage::Decay)];
            if (t_ >= 1.0f)
            {
                out_ = sustain_;
                if (!tryLoop(Stage::Decay))
                {
                    if (releasePending_ || (freerun_ && !held_)) enter(Stage::Release);
                    else enter(Stage::Sustain);
                }
            }
            else
            {
                out_ = sustain_ + (1.0f - sustain_) * (1.0f - shape());
            }
            break;

        case Stage::Sustain:
            out_ = sustain_;
            break;

        case Stage::Release:
            t_ += inc_[idx(Stage::Release)];
            if (t_ >= 1.0f)
            {
                out_ = 0.0f;
                stage_ = Stage::Idle;
            }
            else
            {
                out_ = startLevel_ * (1.0f - shape());
            }
            break;

        default:
            break;
    }
    return out_ * scale_;
}

} // namespace kmi
