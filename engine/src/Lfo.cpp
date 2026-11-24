#include "kmi/Lfo.h"

#include <cmath>

namespace kmi {

void Lfo::reset(uint32_t seed) noexcept
{
    rng_.seed(seed);
    phase_ = 0.0f;
    smoothed_ = out_ = 0.0f;
    fade_ = 1.0f;
    delayLeft_ = 0;
    done_ = false;
    held_ = rng_.bipolar();
    randPrev_ = rng_.bipolar();
    randNext_ = rng_.bipolar();
    stepIndex_ = 0;
}

void Lfo::update(const LfoParams& p, const float* steps, float rateMul, float levelAdd, float sampleRate,
                 double bpm) noexcept
{
    wave_ = p.wave;
    oneShot_ = p.oneShot;
    stepValues_ = steps;
    steps_ = p.steps < 2 ? 2 : (p.steps > kMaxLfoSteps ? kMaxLfoSteps : p.steps);

    float hz = p.bpmSync ? 1.0f / syncDivisionSeconds(p.syncDiv, bpm) : p.rateHz;
    hz = clampf(hz * rateMul, 0.0f, 150.0f);
    inc_ = hz / sampleRate;

    level_ = clampf(p.level + levelAdd, 0.0f, 1.0f);

    // Smooth 0..1 -> one-pole time constant 0..~250 ms.
    if (p.smooth <= 0.0f) smoothCoef_ = 1.0f;
    else
    {
        const float tau = 0.25f * p.smooth * p.smooth;
        smoothCoef_ = 1.0f - std::exp(-1.0f / (tau * sampleRate));
    }
}

void Lfo::retrigger(const LfoParams& p, float sampleRate) noexcept
{
    phase_ = wrap01(p.phaseDeg / 360.0f);
    done_ = false;
    stepIndex_ = 0;
    delayLeft_ = static_cast<int>(p.delayMs * 0.001f * sampleRate);
    if (p.fadeMs > 0.0f)
    {
        fade_ = 0.0f;
        fadeInc_ = 1000.0f / (p.fadeMs * sampleRate);
    }
    else
    {
        fade_ = 1.0f;
        fadeInc_ = 1.0f;
    }
    newCycle();
}

void Lfo::advanceStep() noexcept
{
    ++stepIndex_;
    if (stepIndex_ >= steps_) stepIndex_ = 0;
    newCycle();
}

void Lfo::newCycle() noexcept
{
    held_ = rng_.bipolar();
    randPrev_ = randNext_;
    randNext_ = rng_.bipolar();
}

float Lfo::waveAt(float ph) noexcept
{
    switch (wave_)
    {
        case LfoWave::Sine: return std::sin(kTwoPi * ph);
        case LfoWave::Triangle: return ph < 0.25f ? 4.0f * ph : (ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f);
        case LfoWave::SawUp: return 2.0f * ph - 1.0f;
        case LfoWave::SawDown: return 1.0f - 2.0f * ph;
        case LfoWave::Square: return ph < 0.5f ? 1.0f : -1.0f;
        case LfoWave::Pulse27: return ph < 0.27f ? 1.0f : -1.0f;
        case LfoWave::Pulse13: return ph < 0.13f ? 1.0f : -1.0f;
        case LfoWave::SampleHold: return held_;
        case LfoWave::Noise: return rng_.bipolar();
        case LfoWave::Random:
        {
            const float t = 0.5f - 0.5f * std::cos(kPi * ph);
            return lerpf(randPrev_, randNext_, t);
        }
        case LfoWave::Step:
        {
            if (!stepValues_) return 0.0f;
            int i = oneShot_ == LfoOneShot::Step ? stepIndex_ : static_cast<int>(ph * static_cast<float>(steps_));
            if (i >= steps_) i = steps_ - 1;
            return stepValues_[i];
        }
        default: return 0.0f;
    }
}

float Lfo::tick() noexcept
{
    if (delayLeft_ > 0)
    {
        --delayLeft_;
        out_ = 0.0f;
        return out_;
    }

    if (fade_ < 1.0f) fade_ = fade_ + fadeInc_ > 1.0f ? 1.0f : fade_ + fadeInc_;

    float raw;
    if (oneShot_ == LfoOneShot::Step && wave_ != LfoWave::Step)
    {
        // Subdivided waveform advanced by triggers: hold the centre of the current section.
        raw = waveAt((static_cast<float>(stepIndex_) + 0.5f) / static_cast<float>(steps_));
    }
    else if (done_)
    {
        raw = waveAt(0.9999f);
    }
    else
    {
        phase_ += inc_;
        if (phase_ >= 1.0f)
        {
            phase_ -= std::floor(phase_);
            if (oneShot_ == LfoOneShot::On) done_ = true;
            newCycle();
        }
        raw = waveAt(done_ ? 0.9999f : phase_);
    }

    smoothed_ += (raw - smoothed_) * smoothCoef_;
    out_ = smoothed_ * level_ * fade_;
    return out_;
}

} // namespace kmi
