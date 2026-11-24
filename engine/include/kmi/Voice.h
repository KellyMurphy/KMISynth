// One synth voice: 8 oscillators wired by an algorithm, digital + analog filters, VCA,
// 8 oscillator envelopes, Env 1-5, LFO 1-5 and per-voice mod-matrix evaluation.
#pragma once

#include <array>
#include <cstdint>

#include "kmi/Algorithm.h"
#include "kmi/Envelope.h"
#include "kmi/Filters.h"
#include "kmi/Lfo.h"
#include "kmi/Modulation.h"
#include "kmi/Patch.h"

namespace kmi {

struct ModState
{
    float modWheel = 0.0f, pitchBend = 0.0f, channelAT = 0.0f, expression = 1.0f, sustain = 0.0f;
    float cc[128] = {};
};

struct RenderContext
{
    const Patch* patch = nullptr;
    float sampleRate = 96000.0f;                 // internal (oversampled) rate
    double bpm = 120.0;
    const float* globalLfo[kNumLfos] = {};       // per-sample output of shared (Single/Free) LFOs
    const Lfo* globalLfoState[kNumLfos] = {};
    float globalLfoPhaseStart[kNumLfos] = {};    // shared LFO phase at the first sample of the chunk
    const float* vibrato = nullptr;              // per-sample vibrato LFO, -1..1
    const Algorithm* algorithm = nullptr;        // resolved, unmodulated algorithm
    const ModState* mod = nullptr;
    bool otherNotesHeld = false;
};

class Voice
{
public:
    static constexpr int kControlBlock = 32;

    void init(int index, uint32_t seed) noexcept;
    void prepare(float sampleRate) noexcept;

    // Start (or legato-retrigger) a note. glideFrom < 0 means no glide source.
    void start(const RenderContext& ctx, int note, float velocity, float unisonSpread, float glideFrom) noexcept;
    // Mono legato: change pitch, retrigger envelopes according to their Legato setting.
    void legatoTo(const RenderContext& ctx, int note, float velocity) noexcept;
    void release(float releaseVelocity) noexcept;
    // Fade out quickly, then start the pending note (voice stealing without clicks).
    void steal(int note, float velocity, float unisonSpread, float glideFrom) noexcept;
    void kill() noexcept;

    void setPolyAT(float v) noexcept { polyAT_ = v; }

    bool isActive() const noexcept { return active_; }
    bool isGateOn() const noexcept { return gate_; }
    bool isStealing() const noexcept { return stealBlocks_ > 0; }
    int note() const noexcept { return stealBlocks_ > 0 ? pendingNote_ : note_; }
    float loudness() const noexcept { return lastAmp_; }
    float currentPitch() const noexcept { return currentPitch_; }

    uint64_t age = 0;
    bool sustained = false; // released key held by the sustain pedal

    // Adds this voice's output into outL/outR (internal rate).
    void render(float* outL, float* outR, int numSamples, const RenderContext& ctx) noexcept;

private:
    struct OscState
    {
        float phase = 0.0f;
        float inc = 0.0f;
        float y = 0.0f, yPrev = 0.0f;     // raw wave output (for feedback)
        float modOut = 0.0f;              // output used to modulate other oscillators
        float level = 0.0f;               // current amplitude (0..1)
        float baseLevel = 0.0f, baseLevelPrev = 0.0f;
        float envLevel = 0.0f;
        float feedback = 0.0f;
        const float* table = nullptr;
        bool invert = false;
        bool active = false;              // contributes audio or modulation
    };

    struct Conn
    {
        uint8_t src;
        OscMode mode;
        float weight;
    };

    void doStart(const RenderContext& ctx) noexcept;
    void computeModulation(const RenderContext& ctx, int offset) noexcept;
    void prepareControlBlock(const RenderContext& ctx, int offset, int len) noexcept;
    float sourceValue(int s, const RenderContext& ctx, int offset) const noexcept;
    float lfoValue(int k, const RenderContext& ctx, int offset) const noexcept;

    int index_ = 0;
    float sampleRate_ = 96000.0f;

    bool active_ = false, gate_ = false;
    int note_ = 60;
    float velocity_ = 0.0f, releaseVelocity_ = 0.0f, polyAT_ = 0.0f;
    float unisonSpread_ = 0.0f;
    float randomPerNote_ = 0.0f;
    float driftPitch_ = 0.0f, driftCutoff_ = 0.0f;

    float currentPitch_ = 60.0f, targetPitch_ = 60.0f, glideDistance_ = 0.0f;

    // Voice stealing
    int stealBlocks_ = 0, stealBlocksTotal_ = 0;
    int pendingNote_ = 60;
    float pendingVelocity_ = 0.0f, pendingSpread_ = 0.0f, pendingGlideFrom_ = -1.0f;
    bool pendingStart_ = false;

    std::array<OscState, kNumOscs> osc_{};
    std::array<Envelope, kNumOscs> oscEnv_{};
    std::array<Envelope, kNumEnvs> env_{};
    std::array<Lfo, kNumLfos> lfo_{};
    DigitalFilter dfilt_;
    AnalogFilter afilt_;

    // Per-block derived state
    Algorithm algo_{};
    std::array<std::array<Conn, kNumOscs>, kNumOscs> conns_{};
    std::array<int, kNumOscs> numConns_{};
    float dst_[dst::Count] = {};
    float dfG_ = 0.0f, dfGTarget_ = 0.0f, afG_ = 0.0f, afGTarget_ = 0.0f;
    bool filterPrimed_ = false;
    float vcaLevel_ = 1.0f, vcaLfoAmt_ = 0.0f, vcaVelScale_ = 1.0f, vcaPolyAT_ = 0.0f, vcaInitial_ = 0.0f;
    float oscsLevel_ = 0.75f, dfLevel_ = 1.0f;
    float panL_ = 0.7071f, panR_ = 0.7071f;
    float ampSmooth_ = 0.0f, ampCoef_ = 1.0f;
    float lastAmp_ = 0.0f;
    Rng rng_;
};

} // namespace kmi
