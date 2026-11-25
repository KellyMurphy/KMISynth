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
// Envelope trigger sources (up to 4 per envelope). Without Note On, an envelope waits for its other triggers.
enum class EnvTrig : uint8_t { Off, NoteOn, Lfo1, Lfo2, Lfo3, Lfo4, Lfo5, SustainDown, CcThreshold, Tap, Count };
enum class OscPanTarget : uint8_t { Carriers, Modulators, Both, Count };
constexpr int kNumEnvTrigs = 4;
constexpr int kNumPannerSequences = 16;
constexpr int kNumVoiceMods = 16;

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
    std::array<EnvTrig, kNumEnvTrigs> trig{ EnvTrig::NoteOn, EnvTrig::Off, EnvTrig::Off, EnvTrig::Off };

    bool hasTrigger(EnvTrig t) const noexcept
    {
        for (EnvTrig x : trig)
            if (x == t) return true;
        return false;
    }
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
    // Keyscale: level falls away from the breakpoint (over 4 octaves at full depth). Curve -1 exp .. 1 log.
    int keyscaleBreak = 60;        // MIDI note 0..127
    float keyscaleLoDepth = 0.0f, keyscaleHiDepth = 0.0f;  // 0..1
    float keyscaleLoCurve = 0.0f, keyscaleHiCurve = 0.0f;  // -1..1
    float pan = 0.0f;              // -1..1, Osc Stereo only
    // Wavetable Scan: sweep from the selected wave through the next scanRange waves (in Waveform-menu order),
    // crossfading between neighbours. scan 0 = the selected wave, 1 = scanRange waves on. scanRange 0 = off.
    float scan = 0.0f;             // 0..1
    int scanRange = 0;             // 0..57
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
    bool mpe = false;              // MIDI channel 1 global, 2-16 one note each with its own bend/pressure/timbre
    int mpeBendRange = 48;         // semitones for per-note pitch bend (1..96)

    // Osc Stereo: each note plays on a left/right voice pair (halving polyphony); oscillator pans differ per side.
    bool oscStereo = false;
    OscPanTarget oscPanTarget = OscPanTarget::Carriers;
    float carrierDetune = 0.0f;    // cents, -20..20, spread between the two sides

    // Panner: each new note takes the next position of a stereo sequence. 0 = off, 1..16 = sequence.
    int panner = 0;
    float pannerWidth = 1.0f;      // 0..1
    bool pannerReset = false;      // restart the sequence when all keys are up (else keep stepping)

    std::array<float, kNumVoiceMods> voiceMod{}; // -1..1 per voice, Voice Mod / Voice Mod+ sources

    int bitDepth = 24;             // 4..24 (24 = off), applied before the filters
    bool vintage = false;          // Vintage Digital: raw (aliasing) wavetables, stepped reads

    int trigCc = 1;                // MIDI CC for the CC Threshold envelope trigger
    float trigThreshold = 0.5f;    // 0..1
};

// Append only (stored in presets). UpAndDown repeats both ends (1 2 3 3 2 1); RandomOnce shuffles the chord once
// and repeats that order until the keys change; Phrase plays one of the factory phrases over the held chord.
enum class ArpMode : uint8_t { Up, Down, UpDown, DownUp, Played, Random, Chord, UpAndDown, RandomOnce, Phrase, Count };
// How the octave range is walked. Follow: the mode walks the whole range as one set of notes (the original
// behaviour). The others play the mode's one-octave pattern and move between octaves: Up, Down, Up/Down,
// Alt (each note through its octaves), Alt 2 (octave changes every step), Random (a random octave per step).
enum class ArpOctaveMode : uint8_t { Follow, Up, Down, UpDown, Alt, Alt2, Random, Count };

struct ArpParams
{
    bool enabled = false;
    ArpMode mode = ArpMode::Up;
    int octaves = 1;        // 1..6
    int syncDiv = 17;       // index into kSyncDivisions (1/16)
    float gate = 0.5f;      // 0.05..1.5 of the step (over 1: notes overlap the next step)
    float swing = 0.0f;     // 0..0.5: even steps lengthen by this fraction, odd ones shorten (50%..75%)
    bool latch = false;     // keep playing after the keys are released; a new chord replaces it
    int velocity = 0;       // 0 = as played, 1..127 fixed
    ArpOctaveMode octaveMode = ArpOctaveMode::Follow;
    int length = 0;         // 0 = the pattern's own length, 1..32 = restart after this many steps
    int offset = 0;         // 0..31: start this many notes into the pattern
    float ratchet = 0.0f;   // 0..1: chance of repeating a step x2, x4, x8 (blended in thirds)
    float chance = 1.0f;    // 0..1: chance that a step plays
    int phrase = 0;         // Phrase mode: 0..kNumArpPhrases-1
    bool midiOut = false;   // also send the arp's notes to the host as MIDI
};

// ---- Step sequencer ------------------------------------------------------------------------------
// Two polyphonic note tracks and a macro track (8 lanes that drive Macros 1-8). The step data is non-automatable
// patch data (saved with the patch like the step-LFO values); the track settings are parameters.
constexpr int kSeqSteps = 128;
constexpr int kSeqNotesPerStep = 4;
constexpr int kSeqNoteTracks = 2;
constexpr int kSeqTracks = kSeqNoteTracks + 1; // + the macro track
constexpr int kSeqLanes = 8;

enum class SeqPlayMode : uint8_t { Forward, Backward, PingPong, Random, Count };
enum class SeqTrigger : uint8_t { Host, Key, Count }; // run with the host transport, or while a key is held

struct SeqNote
{
    uint8_t note = 60;
    uint8_t velocity = 100; // 1..127
    uint8_t length = 4;     // in quarter steps: 1..64 (0.25..16 steps)
    bool operator==(const SeqNote&) const = default;
};
struct SeqStep
{
    std::array<SeqNote, kSeqNotesPerStep> notes{};
    uint8_t count = 0;        // notes on this step
    uint8_t probability = 100;// 0..100 %
    uint8_t ratchet = 1;      // 1..4 hits inside the step
    uint8_t drift = 0;        // 0..100: random lateness (up to 30% of a step) and velocity spread
    bool operator==(const SeqStep&) const = default;
};
struct SeqData
{
    std::array<std::array<SeqStep, kSeqSteps>, kSeqNoteTracks> steps{};
    std::array<std::array<float, kSeqSteps>, kSeqLanes> lanes{}; // macro values 0..1 per step
    std::array<bool, kSeqLanes> laneOn{};                         // a lane drives its macro only when on
};
struct SeqTrackParams
{
    bool enabled = true;
    int length = 16;                 // 1..128 steps
    int syncDiv = 17;                // index into kSyncDivisions (1/16)
    SeqPlayMode mode = SeqPlayMode::Forward;
    int transpose = 0;               // note tracks: -24..24 semitones
};
struct SeqParams
{
    bool enabled = false;
    SeqTrigger trigger = SeqTrigger::Host;
    bool midiOut = false;            // also send the sequencer's notes to the host as MIDI
    std::array<SeqTrackParams, kSeqTracks> track{};
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

// ---- Effects --------------------------------------------------------------------------------------
enum class FxType : uint8_t { Bypass, Chorus, Flanger, Rotary, Phaser, LoFi, Tremolo, Eq, Compressor, Distort, Count };
enum class LoFiFilter : uint8_t { Thru, LowPass, Radio, Telephone, HighPass, Count };
enum class CompSidechain : uint8_t { Off, TempoDuck, NoteDuck, Count };
enum class DelayType : uint8_t { Mono, Stereo, PingPong, LRC, Reverse, Count };
enum class ReverbType : uint8_t { Hall, Plate, Room, Cloud, Count };

struct ModFxParams   // Chorus / Flanger
{
    float rateHz = 0.6f;     // 0.02..10
    float depth = 0.5f;      // 0..1
    float offsetDeg = 90.0f; // -180..180, right-channel LFO phase
    float feedback = 0.0f;   // -1..1
    bool stereo = true;
    float mix = 0.5f;        // 0..1
};
struct RotaryParams
{
    float wooferHz = 0.8f, hornHz = 6.5f; // 0.02..10
    float loDepth = 0.5f, hiDepth = 0.7f; // 0..1
    float balance = 0.0f;                 // -1 (low) .. 1 (high)
    float mix = 1.0f;
};
struct PhaserParams
{
    float rateHz = 0.3f;
    float feedback = 0.4f;   // -1..1
    float depth = 0.7f;      // 0..1
    float lowFreq = 0.3f;    // 0..1 -> lowest sweep frequency
    float offsetDeg = 90.0f;
    float mix = 0.5f;
};
struct LoFiParams
{
    float cutoffHz = 4000.0f;   // 160..20000
    float resonance = 1.0f;     // Q 1..12
    LoFiFilter filter = LoFiFilter::LowPass;
    float outputDb = 0.0f;      // -6..36
    float sampleRate = 22050.0f;// 2756..44100
    float mix = 1.0f;
};
struct TremoloParams
{
    float rateHz = 4.0f;
    float depth = 0.5f;
    bool square = false;
    float phaseDeg = 0.0f;   // L/R phase, -180..180
    float pitchMod = 0.0f;   // 0..1
    float mix = 1.0f;
};
struct EqParams
{
    float lowDb = 0.0f, midDb = 0.0f, highDb = 0.0f; // -36..24
    float xoverLoHz = 250.0f;   // 32..2000
    float xoverHiHz = 3000.0f;  // 512..16000
    float mix = 1.0f;
};
struct CompParams
{
    CompSidechain sidechain = CompSidechain::Off;
    float ratio = 4.0f;          // 1..20
    float thresholdDb = -18.0f;  // -64..0 (duck depth when side-chained)
    float attackMs = 10.0f;      // 1..400
    float releaseMs = 120.0f;    // 5..560
    float makeupDb = 0.0f;       // -12..24
    float mix = 1.0f;
};
struct DistortParams
{
    float drive = 0.3f;    // 0..1
    float tone = 0.0f;     // -1 (high cut) .. 1 (low cut)
    float asym = 0.0f;     // 0..1
    float curve = 0.3f;    // 0 soft .. 1 hard
    float outputDb = 0.0f; // -36..24
    float mix = 1.0f;
};
struct InsertFxParams
{
    FxType type = FxType::Bypass;
    ModFxParams chorus{};
    ModFxParams flanger{ 0.2f, 0.6f, 90.0f, 0.5f, true, 0.5f };
    RotaryParams rotary{};
    PhaserParams phaser{};
    LoFiParams lofi{};
    TremoloParams tremolo{};
    EqParams eq{};
    CompParams comp{};
    DistortParams distort{};
};
struct DelayParams
{
    bool enabled = false;
    DelayType type = DelayType::Stereo;
    float timeMs = 375.0f;     // 1..3000
    bool bpmSync = false;
    int syncDiv = 13;          // 1/8D
    float feedback = 0.35f;    // 0..1
    float wetTone = 0.0f;      // -1 (low pass) .. 1 (high pass)
    float feedbackTone = 0.7f; // 0 dark .. 1 bright
    float mix = 0.3f;
};
struct ReverbParams
{
    bool enabled = false;
    ReverbType type = ReverbType::Hall;
    float preDelayMs = 20.0f;  // 0.5..250
    float timeSec = 2.5f;      // 0.12..90
    bool freeze = false;
    float tone = 0.0f;         // -1..1
    float hiDamp = 0.4f;       // 0..1
    float loDamp = 0.1f;       // 0..1
    float mix = 0.25f;
};
struct FxParams
{
    InsertFxParams pre{}, post{};
    DelayParams delay{};
    ReverbParams reverb{};
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
    ArpParams arp{};
    SeqParams seq{};
    SeqData seqData{};
    std::array<ModSlot, kNumModSlots> mod{};
    std::array<MacroParams, kNumMacros> macro{};
    AlgoParams algo{};
    FxParams fx{};

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
