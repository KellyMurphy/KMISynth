// The complete sound description consumed by the engine. Plain data, written by the host wrapper
// on the audio thread at the start of each block. Units are engine-normalised (see comments);
// the plugin layer converts from the user-facing 0-128 / +-64 ranges.
#pragma once

#include <array>
#include <cstdint>

#include "kmi/Algorithm.h"
#include "kmi/Constants.h"

namespace kmi {

// NOTE: enum orders are part of the preset/automation format. Append only.
enum class OscMode : uint8_t { PhaseMod, FreqMod, PWMod, HardSync, PDSquare, PDSaw, PDSawPulse, Count };
enum class PitchMode : uint8_t { Semitone, Ratio, Frequency, Count };
enum class EnvSource : uint8_t { Self, Env1, Env2, Env3, Count };
enum class AlgoMode : uint8_t { Single, Morph, Custom, Count };
enum class DigitalFilterType : uint8_t {
    SvfLpBpHp, SvfLpNotchHp,
    HpDiode, HpModular, Hp12,
    BpModular, Bp12,
    LpLadder12, LpLadder24, LpFat12, LpFat24, LpGate, LpDiode, LpModular, Lp12, Lp1Pole, Lp8Pole,
    Vowel,
    Count
};
enum class LfoWave : uint8_t { Sine, Triangle, SawUp, SawDown, Square, Pulse27, Pulse13, SampleHold, Noise, Random, Step, Count };
enum class LfoTrig : uint8_t { Poly, Single, Free, Count };
enum class LfoOneShot : uint8_t { Off, On, Step, Count };
enum class PolyMode : uint8_t { PolyRotate, PolyReassign, Mono, MonoLo, MonoHi, Unison, UnisonLo, UnisonHi, UnisonPoly, Count };
enum class GlideMode : uint8_t { Off, Glide, Glissando, Count };
enum class VibratoWave : uint8_t { Sine, Triangle, Square, SampleHold, Noise, Count };

struct EnvParams
{
    float delayMs = 0.0f, attackMs = 2.0f, holdMs = 0.0f, decayMs = 600.0f, releaseMs = 250.0f;
    float sustain = 1.0f;                                   // 0..1
    float attackCurve = 0.0f, decayCurve = 0.5f, releaseCurve = 0.5f; // -1..1 (0 = linear)
    bool slow = false;     // Slow speed mode: stretches all times
    bool bpmSync = false;  // quantize segment times to tempo divisions
    bool legato = false, reset = true, freerun = false;
    int loop = 0;          // 0 = off, 2..50 = passes, 51 = infinite
    int stageLoop = 2;     // 0 = Delay>Attack, 1 = Delay>Hold, 2 = Delay>Decay
    float velCurve = 0.0f; // -1..1
    float velAmount = 0.0f;// -1..1
    float level = 1.0f;    // 0..1 master output of this envelope
};

constexpr int kEnvLoopInfinite = 51;

struct OscParams
{
    OscMode mode = OscMode::PhaseMod;
    int wave = 0;
    bool invert = false;
    PitchMode pitchMode = PitchMode::Semitone;
    float semitone = 0.0f;   // -36..36
    float cents = 0.0f;      // -50..50
    float ratio = 1.0f;      // 0.25..64
    float ratioFine = 0.0f;  // -50..100 (percent)
    float freqHz = 440.0f;   // 0..10000
    float freqFine = 0.0f;   // 0..0.99
    float initLevel = 0.0f;  // 0..1
    float envLevel = 0.0f;   // -1..1
    float feedback = 0.0f;   // 0..1
    float keytrack = 1.0f;   // -2..2
    float phaseDeg = 0.0f;   // 0..360
    float velocity = 0.0f;   // -1..1 velocity -> initial level
    float polyAT = 0.0f;     // -1..1
    EnvSource envSource = EnvSource::Self;
    bool directOut = false;  // Osc 2-8: also route to audio
    bool mute = false;
    EnvParams env;
};

struct DigitalFilterParams
{
    DigitalFilterType type = DigitalFilterType::LpLadder24;
    float cutoff = 1.0f, resonance = 0.0f, morph = 0.0f, drive = 0.0f; // 0..1
    bool drivePost = false;
    float envAmt = 0.0f, velEnv = 0.0f, polyAT = 0.0f, lfoAmt = 0.0f;  // -1..1
    float keytrack = 0.0f;   // -2..2 (centre C2)
    float level = 1.0f;      // gain, 1 = unity (display 64)
    int vowelOrder = 0;
};

struct AnalogFilterParams
{
    float preDrive = 0.125f, cutoff = 1.0f, resonance = 0.0f;           // 0..1
    float envAmt = 0.0f, velEnv = 0.0f, polyAT = 0.0f, lfoAmt = 0.0f;  // -1..1
    float keytrack = 1.0f;   // -2..2 (centre C2)
};

struct VcaParams
{
    float oscsLevel = 0.75f;   // 0..1 gain into the digital filter
    float vcaLevel = 1.0f;     // 0..1 pre-FX level
    float patchLevel = 1.0f;   // gain, 1 = unity (display 64)
    float lfoAmt = 0.0f;       // -1..1
    float velEnv = 0.5f;       // -1..1
    float polyAT = 0.0f;       // -1..1
    float initialLevel = 0.0f; // 0..1 global offset (drone)
    bool snap = false;
};

struct LfoParams
{
    LfoWave wave = LfoWave::Sine;
    float rateHz = 2.0f;       // 0..150
    bool bpmSync = false;
    int syncDiv = 11;          // index into kSyncDivisions (1/4)
    LfoTrig trig = LfoTrig::Poly;
    float delayMs = 0.0f, fadeMs = 0.0f;
    float phaseDeg = 0.0f;
    float level = 1.0f;        // 0..1
    float smooth = 0.0f;       // 0..1
    LfoOneShot oneShot = LfoOneShot::Off;
    int steps = 16;            // 2..64
    float phaseStagger = 0.0f; // degrees, Free mode only
};

struct VoiceParams
{
    PolyMode polyMode = PolyMode::PolyRotate;
    int voiceLimit = kMaxVoices;   // 1..16
    int density = 4;               // 1..16 (unison modes)
    float unisonDetune = 0.25f;    // 0..1 (max +-50 cents)
    float analogFeel = 0.0f;       // 0..1
    bool randomPhase = false;
    int pitchBendRange = 2;        // semitones
    float vibratoAmount = 0.5f;    // semitones at full mod wheel (0..12)
    bool vibratoBpmSync = false;
    float vibratoRateHz = 5.0f;
    int vibratoSyncDiv = 17;
    VibratoWave vibratoWave = VibratoWave::Sine;
    GlideMode glideMode = GlideMode::Off;
    float glideTime = 0.2f;        // 0..1
    bool glideLegato = false;
};

struct ModSlot
{
    int source = 0;   // ModSource index
    int dest = 0;     // ModDest index
    float depth = 0.0f; // -1..1
};

struct MacroParams
{
    float value = 0.0f; // 0..1
    std::array<ModSlot, kNumMacroRoutes> routes{}; // source field unused
};

struct AlgoParams
{
    AlgoMode mode = AlgoMode::Single;
    int index = 0;                                   // preset algorithm (Single mode)
    std::array<int, kNumMorphSlots> morphSlots{ 2, 3, 0, 0, 0, 0, 0, 0 };
    float morphPos = 0.0f;                           // 0..7 (display 1.00-8.00)
};

struct Patch
{
    std::array<OscParams, kNumOscs> osc{};
    DigitalFilterParams dfilt{};
    AnalogFilterParams afilt{};
    VcaParams vca{};
    std::array<EnvParams, kNumEnvs> env{};
    std::array<LfoParams, kNumLfos> lfo{};
    VoiceParams voice{};
    std::array<ModSlot, kNumModSlots> mod{};
    std::array<MacroParams, kNumMacros> macro{};
    AlgoParams algo{};

    // Non-automatable state (edited through dedicated UI, saved with the patch).
    Algorithm customAlgorithm{};
    std::array<std::array<float, kMaxLfoSteps>, kNumLfos> lfoSteps{}; // -1..1 per step

    int oversampling = 2; // 1 or 2
};

// A sensible starting patch: Osc 1 sine carrier at full level, everything else silent.
Patch makeInitPatch();

// Resolve the patch's algorithm mode (Single / Morph / Custom) into concrete connection weights.
// morphOffset (in slots) comes from modulation of the morph position.
void resolveAlgorithm(const Patch& p, float morphOffset, Algorithm& out) noexcept;

} // namespace kmi
