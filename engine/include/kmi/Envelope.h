// DAHDSR envelope with curves, Fast/Slow speed, tempo quantize, loops and Legato/Reset/Freerun.
#pragma once

#include <cstdint>

#include "kmi/Patch.h"

namespace kmi {

struct EnvMods
{
    float attackMul = 1.0f, decayMul = 1.0f, releaseMul = 1.0f;
    float sustainAdd = 0.0f, levelAdd = 0.0f;
};

class Envelope
{
public:
    enum class Stage : uint8_t { Idle, Delay, Attack, Hold, Decay, Sustain, Release, Count };

    void reset() noexcept;

    // Recompute segment rates. Call once per control block, and before noteOn().
    void update(const EnvParams& p, const EnvMods& m, float sampleRate, double bpm) noexcept;

    // start = false arms the envelope for the note (velocity, held state) without running it: used when
    // Note On is not one of its trigger sources.
    void noteOn(const EnvParams& p, float velocity, bool voiceWasActive, bool otherNotesHeld, bool start = true) noexcept;
    void noteOff() noexcept;
    void trigger() noexcept; // retrigger from a trigger source while the voice is held

    float tick() noexcept;   // advance one sample; returns the scaled output (0..1)
    float value() const noexcept { return out_ * scale_; }
    bool isIdle() const noexcept { return stage_ == Stage::Idle; }
    Stage stage() const noexcept { return stage_; }

private:
    void enter(Stage s) noexcept;
    void setupCurve(float curve) noexcept;
    float shape() noexcept;
    bool tryLoop(Stage finished) noexcept;

    static constexpr int kStages = static_cast<int>(Stage::Count);

    Stage stage_ = Stage::Idle;
    float t_ = 0.0f;
    float inc_[kStages] = {};
    float attackCurve_ = 0.0f, decayCurve_ = 0.0f, releaseCurve_ = 0.0f;
    float sustain_ = 1.0f, level_ = 1.0f, velScale_ = 1.0f, scale_ = 1.0f;
    float startLevel_ = 0.0f, out_ = 0.0f;

    // Incremental exponential: e_ = exp(-k * t_), so curved segments cost one multiply per sample.
    bool curved_ = false;
    float k_ = 0.0f, e_ = 1.0f, eMul_ = 1.0f, norm_ = 1.0f;

    bool held_ = false, releasePending_ = false, freerun_ = false, reset_ = true;
    int loopMode_ = 0, stageLoop_ = 2, loopsLeft_ = 0, loopsInit_ = 0;
};

} // namespace kmi
