// Low-frequency oscillator: 11 waveforms incl. user Step sequences, delay/fade, one-shot, smoothing.
#pragma once

#include <cstdint>

#include "kmi/Dsp.h"
#include "kmi/Patch.h"

namespace kmi {

class Lfo
{
public:
    void reset(uint32_t seed) noexcept;

    // Per control block. rateMul / levelAdd come from the mod matrix.
    void update(const LfoParams& p, const float* steps, float rateMul, float levelAdd, float sampleRate,
                double bpm) noexcept;

    void retrigger(const LfoParams& p, float sampleRate) noexcept; // phase reset + delay/fade restart
    void advanceStep() noexcept;                                   // One-Shot = Step trigger

    float tick() noexcept;
    float value() const noexcept { return out_; }
    float level() const noexcept { return level_; }
    float phase() const noexcept { return phase_; }
    float increment() const noexcept { return inc_; }
    uint32_t cycles() const noexcept { return cycles_; } // completed cycles (envelope LFO-cycle triggers)

    // Free-running per-voice copies follow a shared master with a phase offset (Phase Stagger).
    void followPhase(float masterPhase, float offset) noexcept { phase_ = wrap01(masterPhase + offset); }

private:
    float waveAt(float ph) noexcept;
    void newCycle() noexcept;

    LfoWave wave_ = LfoWave::Sine;
    LfoOneShot oneShot_ = LfoOneShot::Off;
    float phase_ = 0.0f, inc_ = 0.0f;
    float level_ = 1.0f;
    float smoothCoef_ = 1.0f, smoothed_ = 0.0f, out_ = 0.0f;
    float fade_ = 1.0f, fadeInc_ = 1.0f;
    int delayLeft_ = 0;
    bool done_ = false;
    float held_ = 0.0f, randPrev_ = 0.0f, randNext_ = 0.0f;
    int steps_ = 16, stepIndex_ = 0;
    uint32_t cycles_ = 0;
    const float* stepValues_ = nullptr;
    Rng rng_;
};

} // namespace kmi
