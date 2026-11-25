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
#include "kmi/UserWavetables.h"

namespace kmi {

struct ModState
{
    float modWheel = 0.0f, pitchBend = 0.0f, channelAT = 0.0f, expression = 1.0f, sustain = 0.0f;
    float cc[128] = {};
};

// Which modulators the current patch can hear. Unused ones are not ticked (they pause), which is
// the main per-voice saving: most patches use a handful of the 13 envelopes and 5 LFOs.
struct ModUsage
{
    bool env[kNumEnvs] = {};
    bool oscEnv[kNumOscs] = {};
    bool lfo[kNumLfos] = {};
    bool trigLfo[kNumLfos] = {}; // some envelope is triggered by this LFO's cycle
};
ModUsage analyzeModUsage(const Patch& p) noexcept;

struct RenderContext
{
    const Patch* patch = nullptr;
    ModUsage usage{};
    float sampleRate = 96000.0f;                 // internal (oversampled) rate
    double bpm = 120.0;
    const float* globalLfo[kNumLfos] = {};       // per-sample output of shared (Single/Free) LFOs
    const Lfo* globalLfoState[kNumLfos] = {};
    float globalLfoPhaseStart[kNumLfos] = {};    // shared LFO phase at the first sample of the chunk
    const float* vibrato = nullptr;              // per-sample vibrato LFO, -1..1
    const Algorithm* algorithm = nullptr;        // resolved, unmodulated algorithm
    const ModState* mod = nullptr;
    const UserWavetables* userWaves = nullptr;   // the instance's User 1-8 slots (may be null)
    const float* tuning = nullptr;               // 128 per-key pitches (fractional MIDI notes); null = equal
    int scaleMask = 0xFFF;                        // absolute pitch classes of the scale (glissando)
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
    void setNoteBend(float bipolar) noexcept { noteBend_ = bipolar; } // MPE per-note bend, x mpeBendRange
    void setTimbre(float v) noexcept { timbre_ = v; }
    // Retrigger every envelope (oscillator and Env 1-5) listing `t` among its trigger sources (while held).
    void triggerEnvelopes(const Patch& p, EnvTrig t) noexcept;

    bool isActive() const noexcept { return active_; }
    bool isGateOn() const noexcept { return gate_; }
    bool isStealing() const noexcept { return stealBlocks_ > 0; }
    int note() const noexcept { return stealBlocks_ > 0 ? pendingNote_ : note_; }
    float loudness() const noexcept { return lastAmp_; }
    const float* modDestinations() const noexcept { return dst_; } // last control block's values
    float currentPitch() const noexcept { return currentPitch_; }

    uint64_t age = 0;
    bool sustained = false; // released key held by the sustain pedal
    int channel = 0;        // MIDI channel of the note (MPE), 0 = not channel-specific
    int stereoSide = 0;     // Osc Stereo: -1 left / +1 right voice of a pair, 0 = off
    float pannerPos = 0.0f; // Panner position for this note, -1..1

    // Renders one control block (len <= kControlBlock samples starting at `offset` in the chunk).
    // Writes the pre-filter signal and the post-filter gains (VCA x pan) at index s * stride; the
    // engine then runs the filters of four voices at once (FilterBank4) and applies the gains.
    void renderBlock(const RenderContext& ctx, int offset, int len, float* pre, float* gainL, float* gainR,
                     int stride) noexcept;

    // The same work in three steps, so the oscillators of four voices can run together in SIMD lanes:
    // beginBlock (per voice: modulation, envelopes/LFOs, VCA gains; false if inactive), then
    // renderOscBank (all lanes; pre[s * 4 + lane]) or renderOscScalar (one voice), then endBlock.
    bool beginBlock(const RenderContext& ctx, int offset, int len, float* gainL, float* gainR, int stride) noexcept;
    static void renderOscBank(Voice* const lanes[4], const RenderContext& ctx, int len, float* pre) noexcept;
    void renderOscScalar(const RenderContext& ctx, int len, float* pre, int stride) noexcept;
    void endBlock(const RenderContext& ctx) noexcept;

    FilterLaneParams& filterLane() noexcept { return lane_; }

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
        const float* table2 = nullptr;    // Wavetable Scan: the next wave (null = no crossfade)
        float tableMix = 0.0f;            // crossfade towards table2
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
    uint32_t lfoCycles(int k, const RenderContext& ctx) const noexcept;

    int index_ = 0;
    float sampleRate_ = 96000.0f;

    bool active_ = false, gate_ = false;
    int note_ = 60;
    float velocity_ = 0.0f, releaseVelocity_ = 0.0f, polyAT_ = 0.0f;
    float noteBend_ = 0.0f, timbre_ = 0.0f; // set by the engine at launch and by MPE messages
    uint32_t lfoCyclesSeen_[kNumLfos] = {};
    float envBuf_[kNumOscs][kControlBlock] = {}; // each oscillator's envelope (own or Env 1-3) for the block
    float keyscale_[kNumOscs] = {};
    float carrierSide_[kNumOscs] = {}, modSide_[kNumOscs] = {}; // Osc Stereo per-side gains (1 when off)
    float bitScale_ = 0.0f;   // 0 = no bit reduction
    bool vintage_ = false;
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
    FilterLaneParams lane_{};

    // Per-block derived state
    Algorithm algo_{};
    std::array<std::array<Conn, kNumOscs>, kNumOscs> conns_{};
    std::array<int, kNumOscs> numConns_{};
    float dst_[dst::Count] = {};
    float dfG_ = 0.0f, dfGTarget_ = 0.0f, afG_ = 0.0f, afGTarget_ = 0.0f;
    bool filterPrimed_ = false;
    float vcaLevel_ = 1.0f, vcaLfoAmt_ = 0.0f, vcaVelScale_ = 1.0f, vcaPolyAT_ = 0.0f, vcaInitial_ = 0.0f;
    float oscsLevel_ = 0.75f;
    float panL_ = 0.7071f, panR_ = 0.7071f, lastPan_ = 0.0f;
    float ampSmooth_ = 0.0f, ampCoef_ = 1.0f;
    float lastAmp_ = 0.0f;
    Rng rng_;
};

} // namespace kmi
