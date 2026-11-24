#include "kmi/Presets.h"

#include <cmath>
#include <cstring>
#include <initializer_list>

#include "kmi/Dsp.h"
#include "kmi/Modulation.h"

namespace kmi {

namespace {

// ---- Wave indices (see Wavetables.cpp; order is part of the preset format) --------------------
enum Wave : int {
    Sine = 0, Triangle = 1, TriSaw = 2, Saw = 3, Square = 4, Pulse25 = 5, Pulse12 = 6, HalfSine = 7,
    RectSine = 8, OctavePair = 9, Drawbars = 10, CubedSine = 11, SineRamp = 12, Hollow = 13, Formant = 14,
    TwinSaw = 15, SyncSweep = 16, Stairs = 17, SoftSquare = 18, BellPartials = 19,
    SoftSaw = 20, Pulse40 = 21, Pulse5 = 22, SawStack = 23, ExpRamp = 24, OddSoft = 25, Organ84 = 26,
    OrganFull = 27, Reed = 28, Strings = 29, ChoirAh = 30, ChoirOo = 31, SquarePartials = 32, PrimePartials = 33,
    EvenPartials = 34, SineFold = 35, SineClip = 36, Parabola = 37, HalfSaw = 38, LeanTriangle = 39, Bump = 40,
    SineSquash = 41, SineSkew = 42, Stairs8 = 43, StepSaw = 44, PulseTrain = 45, Sync25 = 46, Grain = 47,
    FmBright = 48, FmHollow = 49
};

// Normalised cutoff (0..1, 20 Hz..20 kHz log) for a frequency in Hz.
float cutoff(float hz) { return clampf(std::log2(hz / 20.0f) / 9.965784f, 0.0f, 1.0f); }

int syncDiv(const char* name)
{
    for (int i = 0; i < kNumSyncDivisions; ++i)
        if (std::strcmp(kSyncDivisions[i].name, name) == 0) return i;
    return 11; // 1/4
}

// A blank canvas: every oscillator muted and silent. Presets switch on what they use, which also
// keeps CPU down (muted oscillators are skipped entirely).
Patch blank(const char* algorithm)
{
    Patch p = makeInitPatch();
    for (auto& o : p.osc)
    {
        o.initLevel = 0.0f;
        o.mute = true;
        o.pitchMode = PitchMode::Ratio;
    }
    p.algo.index = findPresetAlgorithm(algorithm);
    return p;
}

OscParams& osc(Patch& p, int number, int wave, float ratio, float level)
{
    OscParams& o = p.osc[static_cast<size_t>(number - 1)];
    o.mute = false;
    o.wave = wave;
    o.ratio = ratio;
    o.initLevel = level;
    return o;
}

void adsr(EnvParams& e, float a, float d, float s, float r)
{
    e.attackMs = a;
    e.decayMs = d;
    e.sustain = s;
    e.releaseMs = r;
}

// An envelope-driven oscillator level: initial + env * depth.
void envLevel(OscParams& o, float depth, float a, float d, float s, float r, float velAmount = 0.0f)
{
    o.envLevel = depth;
    adsr(o.env, a, d, s, r);
    o.env.velAmount = velAmount;
}

// Adds a mod-matrix route in the first free slot; returns the slot (for depth-of-depth routes).
int route(Patch& p, int source, int dest, float depth)
{
    for (int i = 0; i < kNumModSlots; ++i)
    {
        ModSlot& m = p.mod[static_cast<size_t>(i)];
        if (m.source == src::None && m.dest == dst::None)
        {
            m = { source, dest, depth };
            return i;
        }
    }
    return -1;
}

// Destination: the depth of another mod-matrix slot ("depth of depth").
int depthOf(int slot) { return dst::ModDepthBase + slot; }

// Custom-algorithm connection "m>c" with a weight (oscillator numbers 1-8).
void connect(Patch& p, int m, int c, float weight = 1.0f)
{
    p.customAlgorithm.conn[m - 1][c - 1] = weight;
}

// Tempo-synced envelope that repeats Delay > Decay while the key is held.
void loopEnv(EnvParams& e, int loop = kEnvLoopInfinite)
{
    e.loop = loop;
    e.bpmSync = true;
    e.stageLoop = 2;
}

void macro(Patch& p, int number, float value, std::initializer_list<ModSlot> routes)
{
    MacroParams& m = p.macro[static_cast<size_t>(number - 1)];
    m.value = value;
    size_t r = 0;
    for (const ModSlot& s : routes)
        if (r < m.routes.size()) m.routes[r++] = s;
}

ModSlot to(int dest, float depth) { return { 0, dest, depth }; }

constexpr int dfCut = dst::DFiltBase + dst::DfCutoff;
constexpr int dfRes = dst::DFiltBase + dst::DfResonance;
constexpr int dfMorph = dst::DFiltBase + dst::DfMorph;
constexpr int afCut = dst::AFiltBase + dst::AfCutoff;
constexpr int afRes = dst::AFiltBase + dst::AfResonance;
constexpr int afDrive = dst::AFiltBase + dst::AfPreDrive;
constexpr int allPitch = dst::AllOscBase + dst::OscPitch;
constexpr int allFine = dst::AllOscBase + dst::OscFine;
constexpr int allFeedback = dst::AllOscBase + dst::OscFeedback;
int oscLevel(int number) { return dst::osc(number - 1, dst::OscLevel); }
int oscEnvLevel(int number) { return dst::osc(number - 1, dst::OscEnvLevel); }
int fx(int d) { return dst::FxBase + d; }

void room(Patch& p, ReverbType type, float seconds, float mix, float preDelayMs = 20.0f)
{
    p.fx.reverb.enabled = true;
    p.fx.reverb.type = type;
    p.fx.reverb.timeSec = seconds;
    p.fx.reverb.mix = mix;
    p.fx.reverb.preDelayMs = preDelayMs;
}

void echo(Patch& p, DelayType type, const char* division, float feedback, float mix)
{
    p.fx.delay.enabled = true;
    p.fx.delay.type = type;
    p.fx.delay.bpmSync = true;
    p.fx.delay.syncDiv = syncDiv(division);
    p.fx.delay.feedback = feedback;
    p.fx.delay.mix = mix;
}

// ================================================================================================
// The presets. Every one is original; names describe the sound, not any other instrument.
// ================================================================================================

// Two FM pairs: a soft body (1:1, slow decay) and a bright tine (1:14, fast decay), both velocity-scaled.
Patch velvetTines()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.5f).velocity = 0.3f;
    envLevel(osc(p, 2, Sine, 1.0f, 0.1f), 0.45f, 1.0f, 1600.0f, 0.2f, 400.0f, 0.7f);
    osc(p, 3, Sine, 1.0f, 0.32f).ratioFine = 0.12f;
    envLevel(osc(p, 4, Sine, 14.0f, 0.0f), 0.42f, 0.0f, 220.0f, 0.0f, 100.0f, 0.9f);
    adsr(p.env[2], 1.0f, 5200.0f, 0.0f, 450.0f);
    p.env[2].decayCurve = 0.6f;
    p.vca.velEnv = 0.7f;
    p.fx.pre.type = FxType::Tremolo;
    p.fx.pre.tremolo = { 4.6f, 0.3f, false, 180.0f, 0.0f, 1.0f };
    p.fx.post.type = FxType::Chorus;
    p.fx.post.chorus.mix = 0.3f;
    room(p, ReverbType::Room, 1.2f, 0.18f);
    route(p, src::ModWheel, oscEnvLevel(4), 0.35f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.4f), to(oscEnvLevel(4), 0.25f) });          // Bark
    macro(p, 2, 0.0f, { to(fx(dst::PreFxParam2), 0.5f) });                                 // Pan depth
    return p;
}

// Inharmonic FM ratios with independently decaying partials and a long hall.
Patch glassCarillon()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.45f);
    envLevel(osc(p, 2, Sine, 3.5f, 0.05f), 0.5f, 0.0f, 2600.0f, 0.0f, 2000.0f, 0.6f);
    envLevel(osc(p, 3, Sine, 2.76f, 0.0f), 0.3f, 0.0f, 1400.0f, 0.0f, 1200.0f);
    envLevel(osc(p, 4, Sine, 5.19f, 0.0f), 0.32f, 0.0f, 900.0f, 0.0f, 800.0f, 0.5f);
    osc(p, 5, Sine, 0.5f, 0.12f);
    adsr(p.env[2], 0.0f, 9000.0f, 0.0f, 3500.0f);
    p.env[2].decayCurve = 0.7f;
    p.vca.velEnv = 0.6f;
    room(p, ReverbType::Hall, 4.5f, 0.32f, 30.0f);
    route(p, src::Velocity, oscLevel(2), 0.15f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.35f), to(oscEnvLevel(4), 0.3f) });          // Strike
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.4f), to(fx(dst::ReverbTime), 0.5f) });   // Space
    p.vca.patchLevel = 0.75f; // level-matched across the bank
    return p;
}

// Six sine drawbars plus a percussive third harmonic, overdrive and a rotary speaker.
Patch drawbarChapel()
{
    Patch p = blank("Eight Carriers");
    const float ratios[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f };
    const float levels[] = { 0.5f, 0.55f, 0.32f, 0.38f, 0.22f, 0.18f };
    for (int i = 0; i < 6; ++i) osc(p, i + 1, Sine, ratios[i], levels[i]);
    envLevel(osc(p, 7, Sine, 3.0f, 0.0f), 0.4f, 0.0f, 260.0f, 0.0f, 40.0f);
    p.vca.oscsLevel = 0.4f;
    adsr(p.env[2], 3.0f, 10.0f, 1.0f, 60.0f);
    p.vca.velEnv = 0.0f;
    p.vca.snap = true;
    p.fx.pre.type = FxType::Distort;
    p.fx.pre.distort = { 0.22f, -0.2f, 0.1f, 0.25f, -2.0f, 0.5f };
    p.fx.post.type = FxType::Rotary;
    room(p, ReverbType::Room, 1.6f, 0.15f);
    route(p, src::ModWheel, fx(dst::PostFxParam1), 0.5f); // wheel: fast rotor
    route(p, src::ModWheel, fx(dst::PostFxParam2), 0.35f);
    macro(p, 1, 0.0f, { to(oscLevel(5), 0.3f), to(oscLevel(6), 0.35f) });                  // Upper bars
    macro(p, 2, 0.0f, { to(fx(dst::PreFxParam1), 0.6f) });                                 // Drive
    p.vca.patchLevel = 0.45f; // level-matched across the bank
    return p;
}

// A three-operator FM stack with a sub carrier, mono with legato glide, through the analog ladder.
Patch rubberFloor()
{
    Patch p = blank("Stack Three, Five Free");
    osc(p, 1, Sine, 1.0f, 0.62f);
    envLevel(osc(p, 2, Sine, 1.0f, 0.2f), 0.45f, 0.0f, 350.0f, 0.15f, 150.0f, 0.5f);
    osc(p, 3, Sine, 2.0f, 0.18f).feedback = 0.35f;
    osc(p, 4, Sine, 0.5f, 0.4f);
    p.afilt.cutoff = cutoff(320.0f);
    p.afilt.resonance = 0.25f;
    p.afilt.envAmt = 0.35f;
    p.afilt.keytrack = 0.5f;
    p.afilt.preDrive = 0.35f;
    adsr(p.env[1], 0.0f, 300.0f, 0.2f, 120.0f);
    adsr(p.env[2], 1.0f, 600.0f, 1.0f, 90.0f);
    p.voice.polyMode = PolyMode::Mono;
    p.voice.glideMode = GlideMode::Glide;
    p.voice.glideTime = 0.18f;
    p.voice.glideLegato = true;
    macro(p, 1, 0.0f, { to(afCut, 0.25f), to(oscLevel(2), 0.2f) });                         // Bite
    macro(p, 2, 0.0f, { to(oscLevel(4), 0.3f) });                                            // Sub
    p.vca.patchLevel = 1.6f; // level-matched across the bank
    return p;
}

// A phase-distortion saw (PD Saw modulator) that closes with its envelope, plus a square sub.
Patch sawPressure()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 1.0f, 0.7f);
    OscParams& m = osc(p, 2, Sine, 1.0f, 0.25f);
    m.mode = OscMode::PDSaw;
    envLevel(m, 0.5f, 0.0f, 480.0f, 0.1f, 200.0f, 0.4f);
    osc(p, 3, Square, 0.5f, 0.2f);
    p.afilt.cutoff = cutoff(650.0f);
    p.afilt.resonance = 0.45f;
    p.afilt.envAmt = 0.3f;
    p.afilt.preDrive = 0.45f;
    adsr(p.env[1], 0.0f, 260.0f, 0.0f, 120.0f);
    adsr(p.env[2], 1.0f, 400.0f, 0.9f, 80.0f);
    p.voice.polyMode = PolyMode::Mono;
    p.voice.glideMode = GlideMode::Glide;
    p.voice.glideTime = 0.2f;
    p.voice.glideLegato = true;
    p.fx.post.type = FxType::Distort;
    p.fx.post.distort = { 0.3f, -0.25f, 0.2f, 0.3f, -3.0f, 0.4f };
    macro(p, 1, 0.0f, { to(afCut, 0.3f), to(afRes, 0.2f) });                                 // Open
    macro(p, 2, 0.0f, { to(fx(dst::PostFxParam1), 0.5f), to(fx(dst::PostFxMix), 0.4f) });   // Grit
    p.vca.patchLevel = 1.6f; // level-matched across the bank
    return p;
}

// Hard sync: Osc 2 is the master; its level (swept by an envelope, the mod wheel and a slow LFO)
// sets how far the saw carrier is pushed past it.
Patch syncSweeper()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.5f);
    OscParams& m = osc(p, 2, Sine, 1.0f, 0.12f);
    m.mode = OscMode::HardSync;
    envLevel(m, 0.6f, 0.0f, 900.0f, 0.25f, 300.0f);
    OscParams& layer = osc(p, 3, Saw, 1.0f, 0.22f);
    layer.ratioFine = 0.4f;
    p.dfilt.cutoff = cutoff(3800.0f);
    p.dfilt.resonance = 0.2f;
    p.dfilt.keytrack = 0.5f;
    adsr(p.env[2], 2.0f, 300.0f, 0.9f, 220.0f);
    p.lfo[3].rateHz = 0.35f;
    route(p, src::Lfo1 + 3, oscLevel(2), 0.08f);
    route(p, src::ModWheel, oscLevel(2), 0.25f);
    p.voice.polyMode = PolyMode::Mono;
    p.voice.glideMode = GlideMode::Glide;
    p.voice.glideTime = 0.12f;
    p.voice.glideLegato = true;
    echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.22f);
    room(p, ReverbType::Plate, 1.8f, 0.15f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.35f) });                                       // Sweep
    macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f), to(fx(dst::DelayFeedback), 0.25f) }); // Echo
    p.vca.patchLevel = 2.0f; // level-matched across the bank
    return p;
}

// PW Mod: a modulator warps the square carrier's duty cycle; a slow LFO on its level gives PWM.
Patch narrowGauge()
{
    Patch p = blank("One Pair");
    osc(p, 1, Square, 1.0f, 0.5f);
    OscParams& m = osc(p, 2, Sine, 1.0f, 0.5f);
    m.mode = OscMode::PWMod;
    p.lfo[3].rateHz = 0.7f;
    route(p, src::Lfo1 + 3, oscLevel(2), 0.35f);
    osc(p, 3, Square, 0.5f, 0.18f);
    p.dfilt.type = DigitalFilterType::LpFat24;
    p.dfilt.cutoff = cutoff(2600.0f);
    p.dfilt.resonance = 0.3f;
    p.dfilt.envAmt = 0.22f;
    p.dfilt.keytrack = 0.6f;
    adsr(p.env[0], 0.0f, 420.0f, 0.3f, 250.0f);
    adsr(p.env[2], 4.0f, 400.0f, 0.85f, 260.0f);
    p.fx.pre.type = FxType::Chorus;
    p.fx.pre.chorus.mix = 0.35f;
    echo(p, DelayType::Stereo, "1/4", 0.3f, 0.18f);
    room(p, ReverbType::Plate, 1.4f, 0.12f);
    macro(p, 1, 0.0f, { to(dfCut, 0.3f) });                                                  // Tone
    macro(p, 2, 0.0f, { to(dst::lfo(3, dst::LfoRate), 0.4f) });                              // PWM speed
    p.vca.patchLevel = 0.9f; // level-matched across the bank
    return p;
}

// Two PD Saw pairs with a slow-ish attack, analog filter swell and delayed LFO vibrato.
Patch phaseBrass()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.5f);
    OscParams& m1 = osc(p, 2, Sine, 1.0f, 0.15f);
    m1.mode = OscMode::PDSaw;
    envLevel(m1, 0.55f, 90.0f, 600.0f, 0.6f, 250.0f, 0.5f);
    m1.env.attackCurve = -0.3f;
    osc(p, 3, Sine, 1.0f, 0.36f).ratioFine = 0.25f;
    OscParams& m2 = osc(p, 4, Sine, 1.0f, 0.15f);
    m2.mode = OscMode::PDSaw;
    envLevel(m2, 0.5f, 130.0f, 700.0f, 0.55f, 250.0f, 0.5f);
    p.afilt.cutoff = cutoff(3200.0f);
    p.afilt.envAmt = 0.25f;
    p.afilt.preDrive = 0.3f;
    adsr(p.env[1], 80.0f, 500.0f, 0.5f, 250.0f);
    adsr(p.env[2], 40.0f, 400.0f, 0.85f, 220.0f);
    p.lfo[3].rateHz = 5.2f;
    p.lfo[3].delayMs = 450.0f;
    p.lfo[3].fadeMs = 700.0f;
    route(p, src::Lfo1 + 3, allPitch, 0.012f);
    room(p, ReverbType::Hall, 2.2f, 0.2f);
    macro(p, 1, 0.0f, { to(afCut, 0.3f), to(oscEnvLevel(2), 0.2f), to(oscEnvLevel(4), 0.2f) }); // Blare
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvAttack), 0.4f) });                                 // Swell
    p.vca.patchLevel = 0.45f; // level-matched across the bank
    return p;
}

// Algorithm morph: an LFO glides the voice through four algorithms, slowly re-wiring the timbre.
Patch slowTide()
{
    Patch p = blank("Four Pairs");
    p.algo.mode = AlgoMode::Morph;
    const char* slots[] = { "Four Pairs", "Twin Forks", "Many Into One", "Eight Carriers" };
    p.algo.morphSlots.fill(kMorphSlotOff);
    for (int i = 0; i < 4; ++i) p.algo.morphSlots[static_cast<size_t>(i)] = kMorphSlotFirstPreset + findPresetAlgorithm(slots[i]);
    const float ratios[] = { 1.0f, 2.0f, 1.0f, 3.0f, 1.004f, 0.5f, 2.0f, 4.0f };
    const int waves[] = { Sine, Sine, Triangle, Sine, Sine, Sine, HalfSine, Sine };
    for (int i = 0; i < 8; ++i) osc(p, i + 1, waves[i], ratios[i], i % 2 == 0 ? 0.3f : 0.26f);
    p.vca.oscsLevel = 0.55f;
    p.lfo[3].rateHz = 0.07f;
    p.lfo[3].trig = LfoTrig::Free;
    p.lfo[3].wave = LfoWave::Triangle;
    route(p, src::Lfo1Uni + 3, dst::AlgoMorph, 3.0f / 7.0f);
    p.dfilt.type = DigitalFilterType::LpLadder12;
    p.dfilt.cutoff = cutoff(3200.0f);
    adsr(p.env[2], 1400.0f, 2000.0f, 0.9f, 2600.0f);
    p.env[2].attackCurve = -0.4f;
    p.fx.pre.type = FxType::Chorus;
    p.fx.pre.chorus = { 0.25f, 0.6f, 90.0f, 0.0f, true, 0.45f };
    room(p, ReverbType::Hall, 5.0f, 0.35f, 40.0f);
    macro(p, 1, 0.0f, { to(dfCut, 0.3f) });                                                  // Brightness
    macro(p, 2, 0.0f, { to(dst::lfo(3, dst::LfoRate), 0.6f) });                              // Tide speed
    p.vca.patchLevel = 0.85f; // level-matched across the bank
    return p;
}

// Detuned saws into the vowel filter; an LFO slides the vowel, the wheel opens it.
Patch vowelChoir()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Saw, 1.0f, 0.3f);
    osc(p, 2, Saw, 1.0f, 0.3f).ratioFine = 0.35f;
    osc(p, 3, TwinSaw, 0.5f, 0.22f);
    p.dfilt.type = DigitalFilterType::Vowel;
    p.dfilt.cutoff = 0.55f;
    p.dfilt.morph = 0.3f;
    p.dfilt.resonance = 0.4f;
    p.lfo[3].rateHz = 0.22f;
    p.lfo[3].wave = LfoWave::Triangle;
    route(p, src::Lfo1 + 3, dfMorph, 0.3f);
    route(p, src::ModWheel, dfCut, 0.25f);
    p.voice.analogFeel = 0.4f;
    adsr(p.env[2], 600.0f, 800.0f, 1.0f, 1500.0f);
    p.fx.post.type = FxType::Phaser;
    p.fx.post.phaser.mix = 0.35f;
    room(p, ReverbType::Cloud, 6.0f, 0.4f, 35.0f);
    macro(p, 1, 0.0f, { to(dfCut, 0.3f) });                                                  // Mouth
    macro(p, 2, 0.0f, { to(dfRes, 0.4f) });                                                  // Focus
    p.vca.patchLevel = 0.85f; // level-matched across the bank
    return p;
}

// A woody FM pluck: an odd-ratio modulator that dies away within a fraction of a second.
Patch kalimbaDrop()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 1.0f, 0.6f).velocity = 0.3f;
    envLevel(osc(p, 2, Sine, 5.0f, 0.0f), 0.38f, 0.0f, 110.0f, 0.0f, 60.0f, 0.8f);
    osc(p, 3, Sine, 3.0f, 0.0f).envLevel = 0.12f;
    adsr(p.osc[2].env, 0.0f, 250.0f, 0.0f, 150.0f);
    adsr(p.env[2], 0.0f, 1400.0f, 0.0f, 600.0f);
    p.env[2].decayCurve = 0.6f;
    p.vca.velEnv = 0.7f;
    echo(p, DelayType::Stereo, "1/8D", 0.3f, 0.25f);
    room(p, ReverbType::Room, 1.4f, 0.2f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Tine
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Ring
    return p;
}

// PD Saw pluck through a resonant analog filter with ping-pong echoes.
Patch harpMist()
{
    Patch p = blank("Four Pairs");
    osc(p, 1, Sine, 1.0f, 0.55f);
    OscParams& m = osc(p, 2, Sine, 1.0f, 0.05f);
    m.mode = OscMode::PDSaw;
    envLevel(m, 0.55f, 0.0f, 300.0f, 0.0f, 200.0f, 0.6f);
    osc(p, 3, Sine, 2.0f, 0.15f);
    p.afilt.cutoff = cutoff(750.0f);
    p.afilt.resonance = 0.35f;
    p.afilt.envAmt = 0.4f;
    adsr(p.env[1], 0.0f, 450.0f, 0.0f, 300.0f);
    adsr(p.env[2], 0.0f, 2600.0f, 0.0f, 1800.0f);
    p.env[2].decayCurve = 0.5f;
    p.vca.velEnv = 0.6f;
    echo(p, DelayType::PingPong, "1/8", 0.45f, 0.28f);
    room(p, ReverbType::Hall, 3.0f, 0.25f);
    macro(p, 1, 0.0f, { to(afCut, 0.3f), to(dst::AFiltBase + dst::AfEnvAmt, 0.2f) });       // Brightness
    macro(p, 2, 0.0f, { to(afRes, 0.4f) });                                                  // Resonance
    p.vca.patchLevel = 1.3f; // level-matched across the bank
    return p;
}

// A 16-step LFO drives the analog filter in time with the host tempo; a second step LFO accents FM.
Patch stepMachine()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.5f);
    osc(p, 2, Sine, 2.0f, 0.1f);
    osc(p, 3, Square, 0.5f, 0.3f);
    const float cutSteps[16] = { 1.0f, -0.6f, 0.2f, -0.8f, 0.7f, -0.4f, 0.4f, -1.0f,
                                 0.9f, -0.5f, 0.0f, 0.5f, -0.7f, 0.3f, 0.8f, -0.9f };
    const float fmSteps[16] = { 1.0f, -1.0f, -1.0f, 0.3f, -1.0f, -1.0f, 0.6f, -1.0f,
                                1.0f, -1.0f, 0.2f, -1.0f, -1.0f, 0.8f, -1.0f, 0.4f };
    for (int i = 0; i < 16; ++i)
    {
        p.lfoSteps[1][static_cast<size_t>(i)] = cutSteps[i];
        p.lfoSteps[3][static_cast<size_t>(i)] = fmSteps[i];
    }
    for (int k : { 1, 3 })
    {
        LfoParams& l = p.lfo[static_cast<size_t>(k)];
        l.wave = LfoWave::Step;
        l.steps = 16;
        l.bpmSync = true;
        l.syncDiv = syncDiv("1/1"); // one pass per bar: 16 steps = 16ths
        l.trig = LfoTrig::Single;
        l.smooth = 0.1f;
    }
    p.afilt.cutoff = cutoff(900.0f);
    p.afilt.resonance = 0.55f;
    p.afilt.lfoAmt = 0.25f;
    p.afilt.preDrive = 0.4f;
    route(p, src::Lfo1Uni + 3, oscLevel(2), 0.35f);
    adsr(p.env[2], 2.0f, 300.0f, 1.0f, 300.0f);
    p.voice.polyMode = PolyMode::Mono;
    echo(p, DelayType::PingPong, "1/8D", 0.4f, 0.25f);
    room(p, ReverbType::Room, 1.0f, 0.12f);
    macro(p, 1, 0.0f, { to(dst::AFiltBase + dst::AfLfoAmt, 0.3f) });                        // Step depth
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Cutoff
    p.vca.patchLevel = 1.8f; // level-matched across the bank
    return p;
}

// Eight inharmonic operators in a feedback ring; a sample-and-hold LFO shuffles modulator levels.
Patch metalRain()
{
    Patch p = blank("Ring of Eight");
    const float ratios[] = { 1.0f, 1.41f, 2.13f, 3.7f, 0.71f, 5.3f, 2.9f, 1.18f };
    for (int i = 0; i < 8; ++i) osc(p, i + 1, Sine, ratios[i], i == 0 ? 0.6f : 0.3f);
    p.lfo[3].wave = LfoWave::SampleHold;
    p.lfo[3].rateHz = 6.0f;
    route(p, src::Lfo1 + 3, dst::ModulatorLevel, 0.15f);
    adsr(p.env[2], 0.0f, 3000.0f, 0.0f, 2500.0f);
    p.env[2].decayCurve = 0.6f;
    echo(p, DelayType::PingPong, "1/16", 0.5f, 0.25f);
    room(p, ReverbType::Cloud, 8.0f, 0.45f, 60.0f);
    macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.25f) });                                   // Chaos
    macro(p, 2, 0.0f, { to(fx(dst::ReverbTime), 0.6f), to(fx(dst::ReverbMix), 0.3f) });     // Space
    p.vca.patchLevel = 1.8f; // level-matched across the bank
    return p;
}

// Unison: six detuned voices of a twin saw plus a sub, spread across the stereo field.
Patch unisonWall()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, TwinSaw, 1.0f, 0.4f);
    osc(p, 2, Square, 0.5f, 0.18f);
    p.voice.polyMode = PolyMode::Unison;
    p.voice.density = 6;
    p.voice.unisonDetune = 0.32f;
    p.voice.randomPhase = true;
    p.voice.glideMode = GlideMode::Glide;
    p.voice.glideTime = 0.12f;
    p.voice.glideLegato = true;
    p.vca.oscsLevel = 0.5f;
    p.afilt.cutoff = cutoff(3000.0f);
    p.afilt.envAmt = 0.2f;
    p.afilt.resonance = 0.15f;
    adsr(p.env[1], 0.0f, 700.0f, 0.4f, 300.0f);
    adsr(p.env[2], 5.0f, 300.0f, 1.0f, 300.0f);
    echo(p, DelayType::Stereo, "1/4", 0.3f, 0.15f);
    room(p, ReverbType::Plate, 2.0f, 0.18f);
    macro(p, 1, 0.0f, { to(dst::VoiceBase + dst::VoiceUnisonDetune, 0.4f) });               // Width
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Bright
    return p;
}

// A slow-moving drone: FM levels breathe on free-running LFOs at unrelated rates; huge cloud reverb.
Patch deepField()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.45f);
    osc(p, 2, Sine, 2.0f, 0.2f);
    osc(p, 3, Sine, 0.5f, 0.35f);
    osc(p, 4, Sine, 1.5f, 0.15f).feedback = 0.3f;
    osc(p, 5, Triangle, 2.003f, 0.12f);
    for (int k : { 3, 4 })
    {
        LfoParams& l = p.lfo[static_cast<size_t>(k)];
        l.trig = LfoTrig::Free;
        l.rateHz = k == 3 ? 0.09f : 0.057f;
    }
    route(p, src::Lfo1 + 3, oscLevel(2), 0.2f);
    route(p, src::Lfo1 + 4, oscLevel(4), 0.2f);
    route(p, src::Lfo1 + 4, allFine, 0.05f);
    p.dfilt.type = DigitalFilterType::SvfLpBpHp;
    p.dfilt.cutoff = cutoff(1500.0f);
    p.dfilt.resonance = 0.25f;
    p.dfilt.lfoAmt = 0.12f;
    p.lfo[0].rateHz = 0.05f;
    p.lfo[0].trig = LfoTrig::Free;
    adsr(p.env[2], 2500.0f, 2000.0f, 1.0f, 5000.0f);
    room(p, ReverbType::Cloud, 14.0f, 0.5f, 80.0f);
    macro(p, 1, 0.0f, { to(dfCut, 0.35f), to(allFeedback, 0.3f) });                         // Glow
    macro(p, 2, 0.0f, { to(dfMorph, 0.5f) });                                                // Sweep LP > HP
    p.vca.patchLevel = 0.45f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Showcase: several engine features working together. The last one uses all of them at once.
// ================================================================================================

// Custom algorithm with a cross-feedback loop; tempo-synced looping envelopes on two modulators
// tick at 1/8 and 1/8 dotted against each other; reverse delay.
Patch clockworkGarden()
{
    Patch p = blank("Four Pairs");
    p.algo.mode = AlgoMode::Custom;
    p.customAlgorithm = Algorithm{};
    connect(p, 3, 1);        // ticking modulator
    connect(p, 2, 1, 0.6f);  // slow FM drift
    connect(p, 6, 3, 0.7f);  // modulates the modulator
    connect(p, 5, 4);        // PD Square on the second voice
    connect(p, 1, 6, 0.3f);  // loop back (read one sample late)
    p.customAlgorithm.out[0] = 1.0f;
    p.customAlgorithm.out[3] = 0.6f;

    osc(p, 1, Sine, 1.0f, 0.5f);
    OscParams& drift = osc(p, 2, Sine, 0.5f, 0.12f);
    drift.mode = OscMode::FreqMod;
    OscParams& tick = osc(p, 3, Sine, 3.0f, 0.0f);
    envLevel(tick, 0.5f, 0.0f, 250.0f, 0.0f, 200.0f); // 1/8 at 120 BPM
    loopEnv(tick.env);
    osc(p, 4, TriSaw, 2.0f, 0.35f);
    OscParams& warp = osc(p, 5, Sine, 1.0f, 0.0f);
    warp.mode = OscMode::PDSquare;
    envLevel(warp, 0.7f, 0.0f, 375.0f, 0.0f, 200.0f); // 1/8 dotted
    loopEnv(warp.env);
    osc(p, 6, Sine, 7.0f, 0.15f);

    p.dfilt.type = DigitalFilterType::SvfLpBpHp;
    p.dfilt.cutoff = cutoff(2400.0f);
    p.dfilt.resonance = 0.35f;
    p.dfilt.morph = 0.3f;
    p.lfo[0].rateHz = 0.15f;
    p.dfilt.lfoAmt = 0.15f;
    adsr(p.env[2], 5.0f, 500.0f, 0.9f, 900.0f);
    p.fx.pre.type = FxType::Phaser;
    p.fx.pre.phaser.mix = 0.3f;
    echo(p, DelayType::Reverse, "1/4", 0.35f, 0.3f);
    room(p, ReverbType::Plate, 2.5f, 0.25f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(3), 0.35f), to(oscEnvLevel(5), 0.25f) });          // Tick
    macro(p, 2, 0.0f, { to(dfMorph, 0.6f) });                                               // LP > HP
    p.vca.patchLevel = 1.6f;
    return p;
}

// Unison Poly ensemble: every note gets its own detuned stack with random phases and per-note
// random pan/tuning; staggered free LFOs make the stack shimmer; release velocity sets the tail.
Patch livingStrings()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Saw, 1.0f, 0.3f);
    osc(p, 2, Saw, 1.0f, 0.25f).ratioFine = 0.3f;
    osc(p, 3, TwinSaw, 0.5f, 0.18f);
    p.voice.polyMode = PolyMode::UnisonPoly;
    p.voice.density = 12;
    p.voice.unisonDetune = 0.22f;
    p.voice.analogFeel = 0.6f;
    p.voice.randomPhase = true;
    p.vca.oscsLevel = 0.5f;
    p.dfilt.type = DigitalFilterType::LpFat12;
    p.dfilt.cutoff = cutoff(1800.0f);
    p.dfilt.keytrack = 0.5f;
    p.dfilt.envAmt = 0.2f;
    adsr(p.env[0], 300.0f, 1200.0f, 0.4f, 800.0f);
    adsr(p.env[2], 350.0f, 800.0f, 1.0f, 900.0f);
    p.lfo[3].trig = LfoTrig::Free;
    p.lfo[3].rateHz = 0.4f;
    p.lfo[3].phaseStagger = 90.0f;
    route(p, src::Lfo1 + 3, allFine, 0.05f);
    route(p, src::RandomPerNote, dst::VoiceBase + dst::VoicePan, 0.35f);
    route(p, src::RandomPerNote, allFine, 0.04f);
    route(p, src::ReleaseVelocity, dst::env(2, dst::EnvRelease), -0.4f); // fast lift = shorter tail
    route(p, src::Velocity, dst::env(2, dst::EnvAttack), -0.5f);          // hard = faster bow
    route(p, src::PolyAT, dfCut, 0.25f);
    p.fx.pre.type = FxType::Chorus;
    p.fx.pre.chorus.mix = 0.3f;
    p.fx.post.type = FxType::Eq;
    p.fx.post.eq = { -6.0f, 0.0f, 3.0f, 180.0f, 5000.0f, 1.0f };
    room(p, ReverbType::Hall, 3.5f, 0.3f, 30.0f);
    macro(p, 1, 0.0f, { to(dfCut, 0.3f) });                                                 // Brightness
    macro(p, 2, 0.0f, { to(dst::VoiceBase + dst::VoiceUnisonDetune, 0.4f) });              // Width
    p.vca.patchLevel = 0.9f;
    return p;
}

// Expression: pressure, wheel, velocity and key position all reshape the sound. Channel pressure
// scales a random-FM route (depth of depth); poly pressure opens the filter; higher keys are softer.
Patch pressureOrganism()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.5f);
    envLevel(osc(p, 2, Sine, 2.0f, 0.15f), 0.3f, 5.0f, 900.0f, 0.3f, 400.0f);
    osc(p, 3, Triangle, 1.0f, 0.35f).ratioFine = -0.2f;
    osc(p, 4, Sine, 3.0f, 0.25f);
    p.lfo[4].wave = LfoWave::SampleHold;
    p.lfo[4].rateHz = 9.0f;
    p.lfo[4].smooth = 0.3f;
    const int sparkle = route(p, src::Lfo1 + 4, oscLevel(2), 0.0f);
    route(p, src::ChannelAT, depthOf(sparkle), 0.35f);
    route(p, src::ModWheel, depthOf(sparkle), 0.2f);
    route(p, src::PolyAT, dfCut, 0.3f);
    route(p, src::Velocity, oscEnvLevel(2), 0.3f);
    route(p, src::Keytrack, oscLevel(4), -0.25f);
    route(p, src::PitchBend, dfCut, 0.1f);
    route(p, src::ChannelAT, fx(dst::ReverbMix), 0.3f);
    p.dfilt.cutoff = cutoff(3000.0f);
    adsr(p.env[2], 15.0f, 600.0f, 0.85f, 500.0f);
    echo(p, DelayType::Stereo, "1/4D", 0.3f, 0.15f);
    room(p, ReverbType::Room, 1.8f, 0.2f);
    macro(p, 1, 0.0f, { to(depthOf(sparkle), 0.3f) });                                      // Sparkle
    macro(p, 2, 0.0f, { to(dfCut, 0.3f), to(oscLevel(4), 0.2f) });                          // Brightness
    p.vca.patchLevel = 0.6f;
    return p;
}

// Percussion: fixed-frequency (Frequency pitch mode) modulators and a Direct Out noise source,
// a pitch-drop envelope, the low-pass gate, a compressor and lo-fi crunch.
Patch fixedPointKit()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.0f).envLevel = 0.8f;
    adsr(p.osc[0].env, 0.0f, 450.0f, 0.0f, 300.0f);
    OscParams& click = osc(p, 2, Sine, 1.0f, 0.0f);
    click.pitchMode = PitchMode::Frequency;
    click.freqHz = 1870.0f;
    envLevel(click, 0.45f, 0.0f, 40.0f, 0.0f, 40.0f, 0.6f);
    osc(p, 3, Square, 1.5f, 0.0f).envLevel = 0.3f;
    adsr(p.osc[2].env, 0.0f, 160.0f, 0.0f, 120.0f);
    OscParams& hiss = osc(p, 4, Sine, 1.0f, 0.0f);
    hiss.pitchMode = PitchMode::Frequency;
    hiss.freqHz = 4210.0f;
    hiss.feedback = 0.95f;
    hiss.directOut = true;
    hiss.envSource = EnvSource::Env1; // shares the filter envelope
    hiss.envLevel = 0.35f;
    route(p, src::Env1 + 3, allPitch, 0.3f); // Env 4: pitch drop
    adsr(p.env[3], 0.0f, 70.0f, 0.0f, 50.0f);
    p.dfilt.type = DigitalFilterType::LpGate;
    p.dfilt.cutoff = cutoff(500.0f);
    p.dfilt.envAmt = 0.6f;
    p.dfilt.velEnv = 0.5f;
    adsr(p.env[0], 0.0f, 220.0f, 0.0f, 150.0f);
    adsr(p.env[2], 0.0f, 500.0f, 0.0f, 300.0f);
    p.vca.velEnv = 0.8f;
    p.fx.pre.type = FxType::Compressor;
    p.fx.pre.comp = { CompSidechain::Off, 6.0f, -24.0f, 3.0f, 80.0f, 3.0f, 1.0f };
    p.fx.post.type = FxType::LoFi;
    p.fx.post.lofi = { 9000.0f, 1.5f, LoFiFilter::LowPass, 0.0f, 16000.0f, 0.35f };
    room(p, ReverbType::Room, 0.6f, 0.12f, 5.0f);
    macro(p, 1, 0.0f, { to(dst::env(3, dst::EnvDecay), 0.5f), to(dst::env(2, dst::EnvDecay), 0.4f) }); // Length
    macro(p, 2, 0.0f, { to(fx(dst::PostFxMix), 0.6f) });                                                 // Crunch
    p.vca.patchLevel = 0.75f;
    return p;
}

// Mono glitch line: a per-note step LFO (one step per key press) moves the filter, S&H shakes the
// warp depth and the lo-fi cutoff, glissando slides, the compressor ducks on the beat.
Patch glitchConveyor()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 1.0f, 0.65f);
    OscParams& m = osc(p, 2, Sine, 1.0f, 0.3f);
    m.mode = OscMode::PDSawPulse;
    osc(p, 3, Square, 0.5f, 0.2f);
    const float steps[8] = { -0.8f, 0.6f, -0.2f, 1.0f, -0.5f, 0.2f, 0.9f, -1.0f };
    for (int i = 0; i < 8; ++i) p.lfoSteps[1][static_cast<size_t>(i)] = steps[i];
    p.lfo[1].wave = LfoWave::Step;
    p.lfo[1].steps = 8;
    p.lfo[1].oneShot = LfoOneShot::Step;
    p.afilt.lfoAmt = 0.3f;
    p.afilt.cutoff = cutoff(1200.0f);
    p.afilt.resonance = 0.5f;
    p.afilt.preDrive = 0.5f;
    p.lfo[4].wave = LfoWave::SampleHold;
    p.lfo[4].bpmSync = true;
    p.lfo[4].syncDiv = syncDiv("1/16");
    p.lfo[4].trig = LfoTrig::Single;
    route(p, src::Lfo1 + 4, oscLevel(2), 0.25f);
    route(p, src::Lfo1 + 4, fx(dst::PreFxParam1), 0.3f);
    p.voice.polyMode = PolyMode::Mono;
    p.voice.glideMode = GlideMode::Glissando;
    p.voice.glideTime = 0.25f;
    adsr(p.env[2], 1.0f, 250.0f, 0.7f, 120.0f);
    p.fx.pre.type = FxType::LoFi;
    p.fx.pre.lofi = { 6000.0f, 2.0f, LoFiFilter::LowPass, 0.0f, 11025.0f, 0.6f };
    echo(p, DelayType::LRC, "1/8", 0.35f, 0.22f);
    p.fx.post.type = FxType::Compressor;
    p.fx.post.comp = { CompSidechain::TempoDuck, 4.0f, -14.0f, 2.0f, 180.0f, 2.0f, 1.0f };
    room(p, ReverbType::Room, 1.0f, 0.12f);
    macro(p, 1, 0.0f, { to(dst::AFiltBase + dst::AfLfoAmt, 0.3f) });                        // Step depth
    macro(p, 2, 0.0f, { to(fx(dst::PreFxMix), 0.4f), to(oscLevel(2), 0.2f) });              // Glitch
    p.vca.patchLevel = 2.0f;
    return p;
}

// Everything at once: all seven oscillator modes and three pitch modes, algorithm morph, both
// filters, looping and shared envelopes, all five LFOs (delayed, step, staggered, S&H, unipolar),
// depth-of-depth routes, expression sources, Unison Poly, all eight macros and the full FX chain.
Patch grandTour()
{
    Patch p = blank("Eight Carriers");
    p.algo.mode = AlgoMode::Morph;
    // Osc 8 (a fixed 2 Hz source) is a modulator in every slot, never heard directly.
    const char* slots[] = { "Four Pairs", "Double Diamond", "Pair Chains", "Branching Cascade", "Tall Tower" };
    p.algo.morphSlots.fill(kMorphSlotOff);
    for (int i = 0; i < 5; ++i) p.algo.morphSlots[static_cast<size_t>(i)] = kMorphSlotFirstPreset + findPresetAlgorithm(slots[i]);
    p.algo.morphPos = 0.6f;

    // Oscillators: one per mode.
    osc(p, 1, Sine, 1.0f, 0.32f);                                      // Phase Mod
    OscParams& o2 = osc(p, 2, Sine, 2.0f, 0.2f);
    o2.mode = OscMode::FreqMod;
    envLevel(o2, 0.2f, 0.0f, 500.0f, 0.0f, 300.0f, 0.5f);
    loopEnv(o2.env, 4);                                                // four tempo-synced pulses per note
    OscParams& o3 = osc(p, 3, Square, 1.0f, 0.22f);
    o3.mode = OscMode::PWMod;
    o3.invert = true;
    OscParams& o4 = osc(p, 4, Saw, 1.0f, 0.18f);
    o4.mode = OscMode::HardSync;
    o4.phaseDeg = 90.0f;
    OscParams& o5 = osc(p, 5, Sine, 1.0f, 0.2f);
    o5.mode = OscMode::PDSquare;
    o5.pitchMode = PitchMode::Semitone;
    o5.semitone = 7.0f;
    o5.cents = 4.0f;
    OscParams& o6 = osc(p, 6, Sine, 1.0f, 0.15f);
    o6.mode = OscMode::PDSaw;
    o6.envSource = EnvSource::Env1;                                    // shares the filter envelope
    o6.envLevel = 0.25f;
    OscParams& o7 = osc(p, 7, HalfSine, 0.5f, 0.2f);
    o7.mode = OscMode::PDSawPulse;
    o7.velocity = 0.5f;
    OscParams& o8 = osc(p, 8, Sine, 1.0f, 0.15f);
    o8.pitchMode = PitchMode::Frequency;                               // fixed 2 Hz wobble source
    o8.freqHz = 2.0f;
    o8.feedback = 0.2f;
    p.vca.oscsLevel = 0.55f;

    // Filters.
    p.dfilt.type = DigitalFilterType::SvfLpBpHp;
    p.dfilt.cutoff = cutoff(2600.0f);
    p.dfilt.resonance = 0.3f;
    p.dfilt.morph = 0.15f;
    p.dfilt.envAmt = 0.2f;
    p.dfilt.velEnv = 0.4f;
    p.dfilt.keytrack = 0.4f;
    p.dfilt.lfoAmt = 0.08f;
    p.dfilt.polyAT = 0.2f;
    p.afilt.cutoff = cutoff(5000.0f);
    p.afilt.resonance = 0.2f;
    p.afilt.preDrive = 0.25f;
    p.afilt.envAmt = 0.15f;
    p.afilt.lfoAmt = 0.06f;
    adsr(p.env[0], 400.0f, 1800.0f, 0.3f, 1200.0f);
    adsr(p.env[1], 20.0f, 900.0f, 0.5f, 900.0f);
    adsr(p.env[2], 600.0f, 1500.0f, 0.9f, 2200.0f);
    p.env[2].attackCurve = -0.3f;
    p.vca.lfoAmt = 0.1f;

    // LFOs: 1 delayed sine (filter), 2 step (analog), 3 triangle (VCA), 4 S&H, 5 staggered free.
    p.lfo[0].rateHz = 0.3f;
    p.lfo[0].delayMs = 800.0f;
    p.lfo[0].fadeMs = 2000.0f;
    const float steps[8] = { 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, -0.5f, -1.0f, -0.5f };
    for (int i = 0; i < 8; ++i) p.lfoSteps[1][static_cast<size_t>(i)] = steps[i];
    p.lfo[1].wave = LfoWave::Step;
    p.lfo[1].steps = 8;
    p.lfo[1].bpmSync = true;
    p.lfo[1].syncDiv = syncDiv("2 bars");
    p.lfo[1].smooth = 0.6f;
    p.lfo[2].wave = LfoWave::Triangle;
    p.lfo[2].rateHz = 4.5f;
    p.lfo[3].wave = LfoWave::SampleHold;
    p.lfo[3].rateHz = 5.0f;
    p.lfo[3].smooth = 0.4f;
    p.lfo[4].trig = LfoTrig::Free;
    p.lfo[4].rateHz = 0.05f;
    p.lfo[4].phaseStagger = 45.0f;
    p.lfo[4].wave = LfoWave::Triangle;

    // Mod matrix.
    route(p, src::Lfo1Uni + 4, dst::AlgoMorph, 0.15f);                  // each voice drifts through algorithms
    const int shimmer = route(p, src::Lfo1 + 3, dst::ModulatorLevel, 0.05f);
    route(p, src::ModWheel, depthOf(shimmer), 0.2f);                    // wheel: more S&H shimmer
    route(p, src::ChannelAT, depthOf(shimmer), 0.15f);
    route(p, src::Velocity, dst::ModulatorEnvLevel, 0.2f);
    route(p, src::Keytrack, dst::ModulatorLevel, -0.1f);                // tame the top of the keyboard
    route(p, src::RandomPerNote, dst::VoiceBase + dst::VoicePan, 0.3f);
    route(p, src::ReleaseVelocity, dst::env(2, dst::EnvRelease), -0.3f);
    route(p, src::OscEnv1 + 1, dfRes, 0.15f);                           // Osc 2's looping env pings the filter
    route(p, src::Lfo1 + 2, fx(dst::DelayTime), 0.01f);                 // tape-style wobble
    route(p, src::ChannelAT, fx(dst::ReverbMix), 0.2f);

    // Voice.
    p.voice.polyMode = PolyMode::UnisonPoly;
    p.voice.density = 10;
    p.voice.unisonDetune = 0.15f;
    p.voice.analogFeel = 0.5f;
    p.voice.randomPhase = true;
    p.voice.vibratoAmount = 0.3f;

    // FX: chorus > ping-pong delay > hall > glue compressor.
    p.fx.pre.type = FxType::Chorus;
    p.fx.pre.chorus.mix = 0.3f;
    echo(p, DelayType::PingPong, "1/4D", 0.35f, 0.2f);
    p.fx.delay.wetTone = -0.2f;
    room(p, ReverbType::Hall, 4.0f, 0.3f, 35.0f);
    p.fx.post.type = FxType::Compressor;
    p.fx.post.comp = { CompSidechain::Off, 2.5f, -16.0f, 20.0f, 200.0f, 2.0f, 1.0f };

    // All eight macros.
    macro(p, 1, 0.0f, { to(dst::AlgoMorph, 0.57f) });                                        // Morph
    macro(p, 2, 0.0f, { to(dfCut, 0.3f), to(afCut, 0.2f) });                                 // Brightness
    macro(p, 3, 0.0f, { to(dst::lfo(3, dst::LfoRate), 0.5f), to(dst::lfo(4, dst::LfoRate), 0.8f) }); // Motion
    macro(p, 4, 0.0f, { to(fx(dst::ReverbMix), 0.3f), to(fx(dst::ReverbTime), 0.5f) });       // Space
    macro(p, 5, 0.0f, { to(afDrive, 0.5f), to(allFeedback, 0.3f) });                         // Grit
    macro(p, 6, 0.0f, { to(dst::VoiceBase + dst::VoiceUnisonDetune, 0.4f) });               // Width
    macro(p, 7, 0.0f, { to(dst::env(2, dst::EnvAttack), -0.6f), to(dst::env(0, dst::EnvAttack), -0.6f) }); // Snap
    macro(p, 8, 0.0f, { to(fx(dst::DelayMix), 0.3f), to(fx(dst::DelayFeedback), 0.3f) });     // Echo
    p.vca.patchLevel = 1.4f;
    return p;
}

// Arpeggiated FM-into-ladder pluck: up/down over two octaves in swung sixteenths.
Patch pulseLattice()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.8f);
    envLevel(osc(p, 2, Sine, 2.0f, 0.05f), 0.4f, 0.0f, 180.0f, 0.0f, 120.0f, 0.7f);
    osc(p, 3, Square, 0.5f, 0.35f);
    p.afilt.cutoff = cutoff(1600.0f);
    p.afilt.resonance = 0.45f;
    p.afilt.envAmt = 0.45f;
    adsr(p.env[1], 0.0f, 220.0f, 0.0f, 150.0f);
    adsr(p.env[2], 1.0f, 260.0f, 0.4f, 180.0f);
    p.vca.velEnv = 0.5f;
    p.arp.enabled = true;
    p.arp.mode = ArpMode::UpDown;
    p.arp.octaves = 2;
    p.arp.syncDiv = syncDiv("1/16");
    p.arp.gate = 0.55f;
    p.arp.swing = 0.1f;
    echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.22f);
    room(p, ReverbType::Room, 1.2f, 0.15f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f), to(dst::AFiltBase + dst::AfEnvAmt, 0.2f) });     // Brightness
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                           // Length
    p.vca.patchLevel = 2.0f; // level-matched across the bank
    return p;
}

// Random arpeggio over three octaves of a glassy FM tone; the echoes blur it into a shimmer.
Patch glassStairs()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 1.0f, 0.55f).velocity = 0.3f;
    envLevel(osc(p, 2, Sine, 3.5f, 0.0f), 0.3f, 0.0f, 400.0f, 0.0f, 300.0f, 0.8f);
    adsr(p.osc[1].env, 0.0f, 400.0f, 0.0f, 300.0f);
    adsr(p.env[2], 0.0f, 900.0f, 0.0f, 500.0f);
    p.vca.velEnv = 0.6f;
    p.arp.enabled = true;
    p.arp.mode = ArpMode::Random;
    p.arp.octaves = 3;
    p.arp.syncDiv = syncDiv("1/16");
    p.arp.gate = 0.35f;
    echo(p, DelayType::Stereo, "1/8D", 0.4f, 0.3f);
    room(p, ReverbType::Hall, 3.5f, 0.3f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                       // Shimmer
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                   // Space
    return p;
}

// ---- Presets 25-34: the voice features, new waves and MPE ------------------------------------------

// Osc Stereo electric piano: the tine pair leans right, the body left, with a little carrier detune.
Patch stereoGlass()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.5f).pan = -0.6f;
    envLevel(osc(p, 2, Sine, 1.0f, 0.08f), 0.4f, 1.0f, 1400.0f, 0.15f, 400.0f, 0.7f);
    osc(p, 3, Sine, 1.0f, 0.3f).pan = 0.7f;
    envLevel(osc(p, 4, FmBright, 7.0f, 0.0f), 0.3f, 0.0f, 260.0f, 0.0f, 120.0f, 0.9f);
    p.osc[3].pan = 0.8f;
    p.voice.oscStereo = true;
    p.voice.oscPanTarget = OscPanTarget::Both;
    p.voice.carrierDetune = 8.0f;
    adsr(p.env[2], 1.0f, 4800.0f, 0.0f, 450.0f);
    p.env[2].decayCurve = 0.6f;
    p.vca.velEnv = 0.7f;
    room(p, ReverbType::Plate, 1.6f, 0.2f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(4), 0.3f) });                                         // Tine
    macro(p, 2, 0.0f, { to(dst::AllOscBase + dst::OscPan, 0.5f) });                          // Width
    p.vca.patchLevel = 0.55f; // level-matched across the bank
    return p;
}

// Reed lead on a stereo voice pair: Reed wave carriers detuned apart, a breathy Grain layer, delayed vibrato.
Patch wideReed()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Reed, 1.0f, 0.5f);
    osc(p, 2, Grain, 2.0f, 0.08f);
    p.voice.oscStereo = true;
    p.voice.carrierDetune = 14.0f;
    p.voice.polyMode = PolyMode::Mono;
    p.voice.glideMode = GlideMode::Glide;
    p.voice.glideTime = 0.25f;
    p.voice.glideLegato = true;
    p.afilt.cutoff = cutoff(3200.0f);
    p.afilt.envAmt = 0.15f;
    adsr(p.env[1], 30.0f, 600.0f, 0.4f, 300.0f);
    adsr(p.env[2], 25.0f, 400.0f, 0.9f, 220.0f);
    p.lfo[3].rateHz = 5.2f;
    p.lfo[3].delayMs = 450.0f;
    p.lfo[3].fadeMs = 600.0f;
    route(p, src::Lfo1 + 3, allFine, 0.12f);
    echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.18f);
    room(p, ReverbType::Hall, 2.2f, 0.18f);
    macro(p, 1, 0.0f, { to(afCut, 0.3f) });                                                  // Brightness
    macro(p, 2, 0.0f, { to(oscLevel(2), 0.25f) });                                           // Breath
    p.vca.patchLevel = 1.6f; // level-matched across the bank
    return p;
}

// Two choir waves at 10 bits: soft "ah" and "oo" layers crossfaded by an LFO, a grainy early-digital air.
Patch bitChoir()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, ChoirAh, 1.0f, 0.35f);
    osc(p, 2, ChoirOo, 1.0f, 0.3f).ratioFine = 0.3f;
    osc(p, 3, ChoirAh, 2.0f, 0.12f).ratioFine = -0.25f;
    p.voice.bitDepth = 10;
    p.voice.analogFeel = 0.35f;
    p.lfo[3].rateHz = 0.18f;
    p.lfo[3].wave = LfoWave::Triangle;
    p.lfo[3].trig = LfoTrig::Free;
    route(p, src::Lfo1 + 3, oscLevel(1), 0.2f);
    route(p, src::Lfo1 + 3, oscLevel(2), -0.2f);
    adsr(p.env[2], 900.0f, 1200.0f, 1.0f, 2200.0f);
    p.fx.post.type = FxType::Chorus;
    p.fx.post.chorus.mix = 0.4f;
    room(p, ReverbType::Cloud, 6.0f, 0.35f, 40.0f);
    macro(p, 1, 0.0f, { to(oscLevel(3), 0.3f) });                                            // Air
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                     // Space
    p.vca.patchLevel = 1.0f; // level-matched across the bank
    return p;
}

// Vintage Digital bells: raw stepped tables with aliasing shimmer; keyscale keeps the top octaves polite.
Patch retroBells()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.45f);
    envLevel(osc(p, 2, BellPartials, 3.0f, 0.05f), 0.45f, 0.0f, 1800.0f, 0.0f, 1400.0f, 0.6f);
    osc(p, 3, Triangle, 2.0f, 0.18f);
    envLevel(osc(p, 4, Sine, 7.0f, 0.0f), 0.3f, 0.0f, 600.0f, 0.0f, 500.0f, 0.5f);
    for (int i : { 0, 1, 2, 3 })
    {
        p.osc[static_cast<size_t>(i)].keyscaleBreak = 72;
        p.osc[static_cast<size_t>(i)].keyscaleHiDepth = 0.6f;
        p.osc[static_cast<size_t>(i)].keyscaleHiCurve = 0.4f;
    }
    p.voice.vintage = true;
    adsr(p.env[2], 0.0f, 5000.0f, 0.0f, 2500.0f);
    p.env[2].decayCurve = 0.7f;
    echo(p, DelayType::Stereo, "1/4D", 0.3f, 0.2f);
    room(p, ReverbType::Hall, 3.0f, 0.25f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.35f) });                                        // Strike
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Ring
    p.vca.patchLevel = 0.85f; // level-matched across the bank
    return p;
}

// A soft-saw pluck whose notes walk across the stereo field with the Panner.
Patch panPluck()
{
    Patch p = blank("One Pair");
    osc(p, 1, SoftSaw, 1.0f, 0.55f);
    envLevel(osc(p, 2, Sine, 2.0f, 0.0f), 0.3f, 0.0f, 180.0f, 0.0f, 100.0f, 0.6f);
    p.afilt.cutoff = cutoff(900.0f);
    p.afilt.resonance = 0.3f;
    p.afilt.envAmt = 0.45f;
    adsr(p.env[1], 0.0f, 350.0f, 0.0f, 250.0f);
    adsr(p.env[2], 0.0f, 1600.0f, 0.0f, 900.0f);
    p.env[2].decayCurve = 0.5f;
    p.vca.velEnv = 0.6f;
    p.voice.panner = 6; // Rotate L>R>L
    p.voice.pannerWidth = 0.9f;
    echo(p, DelayType::Stereo, "1/8", 0.35f, 0.2f);
    room(p, ReverbType::Room, 1.4f, 0.18f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Brightness
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Length
    p.vca.patchLevel = 2.0f; // level-matched across the bank
    return p;
}

// The VCA envelope retriggers on every cycle of a tempo-synced LFO 4: a held chord becomes a gated pulse.
Patch gatedPulse()
{
    Patch p = blank("One Pair");
    osc(p, 1, Pulse40, 1.0f, 0.5f);
    osc(p, 2, Sine, 1.0f, 0.12f);
    osc(p, 3, SawStack, 0.5f, 0.25f);
    p.afilt.cutoff = cutoff(1400.0f);
    p.afilt.resonance = 0.35f;
    p.afilt.lfoAmt = 0.2f;
    p.lfo[1].wave = LfoWave::Triangle;
    p.lfo[1].bpmSync = true;
    p.lfo[1].syncDiv = syncDiv("2 bars");
    p.lfo[1].trig = LfoTrig::Free;
    p.lfo[3].bpmSync = true;
    p.lfo[3].syncDiv = syncDiv("1/16");
    p.lfo[3].trig = LfoTrig::Single;
    adsr(p.env[2], 1.0f, 110.0f, 0.0f, 60.0f);
    p.env[2].trig = { EnvTrig::NoteOn, EnvTrig::Lfo4, EnvTrig::Off, EnvTrig::Off };
    echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.2f);
    room(p, ReverbType::Room, 1.0f, 0.12f);
    macro(p, 1, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.6f) });                             // Gate length
    macro(p, 2, 0.0f, { to(afCut, 0.35f) });                                                 // Cutoff
    p.vca.patchLevel = 1.5f; // level-matched across the bank
    return p;
}

// Strings wave ensemble; Voice Mod gives each of the 16 voices its own fine tuning and pan.
Patch strataStrings()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Strings, 1.0f, 0.4f);
    osc(p, 2, Strings, 2.0f, 0.12f).ratioFine = 0.2f;
    const float offsets[kNumVoiceMods] = { -0.9f, 0.7f, -0.3f, 0.5f, 0.9f, -0.6f, 0.2f, -0.8f,
                                           0.6f, -0.2f, 0.8f, -0.5f, 0.3f, -0.7f, 0.1f, 0.4f };
    for (int i = 0; i < kNumVoiceMods; ++i) p.voice.voiceMod[static_cast<size_t>(i)] = offsets[i];
    route(p, src::VoiceMod, allFine, 0.2f);
    route(p, src::VoiceMod, dst::VoiceBase + dst::VoicePan, 0.6f);
    p.afilt.cutoff = cutoff(5000.0f);
    adsr(p.env[2], 450.0f, 900.0f, 1.0f, 1400.0f);
    p.fx.post.type = FxType::Chorus;
    p.fx.post.chorus.mix = 0.25f;
    room(p, ReverbType::Hall, 3.5f, 0.28f);
    macro(p, 1, 0.0f, { to(afCut, 0.3f) });                                                  // Brightness
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvAttack), 0.5f) });                            // Swell
    p.vca.patchLevel = 1.0f; // level-matched across the bank
    return p;
}

// Full organ wave with keyscale: lighter in the bass, softer at the very top, rotary and drive.
Patch scaledOrgan()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, OrganFull, 1.0f, 0.45f);
    osc(p, 2, Organ84, 2.0f, 0.2f);
    for (int i : { 0, 1 })
    {
        OscParams& o = p.osc[static_cast<size_t>(i)];
        o.keyscaleBreak = 60;
        o.keyscaleLoDepth = 0.45f;
        o.keyscaleLoCurve = -0.4f;
        o.keyscaleHiDepth = 0.35f;
    }
    adsr(p.env[2], 3.0f, 100.0f, 1.0f, 80.0f);
    p.vca.snap = true;
    p.fx.pre.type = FxType::Distort;
    p.fx.pre.distort.drive = 0.25f;
    p.fx.post.type = FxType::Rotary;
    route(p, src::ModWheel, fx(dst::PostFxParam1), 0.6f);
    room(p, ReverbType::Room, 1.6f, 0.15f);
    macro(p, 1, 0.0f, { to(fx(dst::PostFxParam1), 0.6f) });                                  // Rotor
    macro(p, 2, 0.0f, { to(fx(dst::PreFxParam1), 0.5f) });                                   // Drive
    p.vca.patchLevel = 0.4f; // level-matched across the bank
    return p;
}

// Mono bass: a folded sine under a saw stack, filter envelope and legato glide.
Patch foldBass()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, SineFold, 1.0f, 0.5f);
    osc(p, 2, SawStack, 1.0f, 0.25f);
    osc(p, 3, Sine, 0.5f, 0.3f);
    p.voice.polyMode = PolyMode::Mono;
    p.voice.glideMode = GlideMode::Glide;
    p.voice.glideTime = 0.15f;
    p.voice.glideLegato = true;
    p.afilt.cutoff = cutoff(320.0f);
    p.afilt.resonance = 0.3f;
    p.afilt.envAmt = 0.4f;
    p.afilt.preDrive = 0.35f;
    adsr(p.env[1], 0.0f, 320.0f, 0.15f, 150.0f);
    adsr(p.env[2], 1.0f, 500.0f, 0.85f, 120.0f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Cutoff
    macro(p, 2, 0.0f, { to(oscLevel(1), 0.3f) });                                            // Fold
    p.vca.patchLevel = 1.5f; // level-matched across the bank
    return p;
}

// MPE lead: per-note bend, pressure swells each note, CC74 (Timbre) opens its filter. Plays normally on
// any keyboard too (channel 1 is global).
Patch pressureGlide()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.6f);
    osc(p, 2, Sine, 1.0f, 0.1f);
    osc(p, 3, FmHollow, 1.0f, 0.2f);
    p.voice.mpe = true;
    p.osc[0].polyAT = 0.3f;
    p.afilt.cutoff = cutoff(1100.0f);
    p.afilt.resonance = 0.3f;
    p.afilt.polyAT = 0.25f;
    route(p, src::Timbre, afCut, 0.35f);
    route(p, src::PolyAT, oscLevel(2), 0.25f);
    adsr(p.env[2], 20.0f, 600.0f, 0.8f, 400.0f);
    echo(p, DelayType::Stereo, "1/4", 0.3f, 0.18f);
    room(p, ReverbType::Plate, 2.0f, 0.2f);
    macro(p, 1, 0.0f, { to(afCut, 0.3f) });                                                  // Brightness
    macro(p, 2, 0.0f, { to(oscLevel(3), 0.3f) });                                            // Hollow
    p.vca.patchLevel = 2.0f; // level-matched across the bank
    return p;
}

// ---- Presets 35-42: ways to use the wavetables -----------------------------------------------------

// A wavetable scan built from layers: four tables (Sine, Formant, Hollow, Saw Stack) cross-faded by two
// slow free-running LFOs, so the timbre drifts from pure to vocal to hollow to buzzy and back.
Patch waveScanner()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Sine, 1.0f, 0.22f);
    osc(p, 2, Formant, 1.0f, 0.2f);
    osc(p, 3, Hollow, 1.0f, 0.2f).ratioFine = 0.15f;
    osc(p, 4, SawStack, 1.0f, 0.14f).ratioFine = -0.15f;
    p.lfo[2].rateHz = 0.07f;
    p.lfo[2].trig = LfoTrig::Free;
    p.lfo[4].rateHz = 0.11f;
    p.lfo[4].wave = LfoWave::Triangle;
    p.lfo[4].trig = LfoTrig::Free;
    route(p, src::Lfo1 + 2, oscLevel(1), 0.2f);
    route(p, src::Lfo1 + 2, oscLevel(4), -0.14f);
    route(p, src::Lfo1 + 4, oscLevel(2), 0.2f);
    route(p, src::Lfo1 + 4, oscLevel(3), -0.2f);
    p.afilt.cutoff = cutoff(6000.0f);
    adsr(p.env[2], 700.0f, 1500.0f, 1.0f, 1800.0f);
    p.fx.post.type = FxType::Chorus;
    p.fx.post.chorus.mix = 0.3f;
    room(p, ReverbType::Hall, 4.0f, 0.3f);
    macro(p, 1, 0.0f, { to(dst::lfo(2, dst::LfoRate), 0.4f), to(dst::lfo(4, dst::LfoRate), 0.4f) }); // Scan speed
    macro(p, 2, 0.0f, { to(oscLevel(4), 0.2f) });                                                     // Buzz
    p.vca.patchLevel = 0.7f; // level-matched across the bank
    return p;
}

// Additive tables that arrive one after another: odd harmonics first, then even, prime and square
// partials bloom in over four seconds while the key is held.
Patch partialBloom()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, OddSoft, 1.0f, 0.3f);
    envLevel(osc(p, 2, EvenPartials, 1.0f, 0.0f), 0.22f, 1200.0f, 500.0f, 1.0f, 1500.0f);
    envLevel(osc(p, 3, PrimePartials, 1.0f, 0.0f), 0.16f, 2500.0f, 500.0f, 1.0f, 1500.0f);
    p.osc[2].ratioFine = 0.2f;
    envLevel(osc(p, 4, SquarePartials, 2.0f, 0.0f), 0.1f, 4000.0f, 500.0f, 1.0f, 1500.0f);
    p.osc[3].ratioFine = -0.2f;
    p.osc[1].env.attackCurve = -0.4f;
    p.osc[2].env.attackCurve = -0.4f;
    p.osc[3].env.attackCurve = -0.4f;
    adsr(p.env[2], 300.0f, 1000.0f, 1.0f, 1600.0f);
    room(p, ReverbType::Cloud, 5.0f, 0.3f, 30.0f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.2f), to(oscEnvLevel(3), 0.15f), to(oscEnvLevel(4), 0.1f) }); // Bloom
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                                                   // Space
    p.vca.patchLevel = 0.4f; // level-matched across the bank
    return p;
}

// Tables as modulators: a Formant wave phase-modulates one sine for a vocal bark, Bell Partials
// modulate a second for a metallic edge. Velocity sets how hard both strike.
Patch formantTines()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.45f);
    envLevel(osc(p, 2, Formant, 1.0f, 0.04f), 0.35f, 0.0f, 900.0f, 0.1f, 300.0f, 0.8f);
    osc(p, 3, Sine, 2.0f, 0.2f).ratioFine = 0.2f;
    envLevel(osc(p, 4, BellPartials, 4.0f, 0.0f), 0.25f, 0.0f, 300.0f, 0.0f, 150.0f, 0.9f);
    adsr(p.env[2], 1.0f, 3500.0f, 0.2f, 400.0f);
    p.env[2].decayCurve = 0.5f;
    p.vca.velEnv = 0.6f;
    p.fx.post.type = FxType::Chorus;
    p.fx.post.chorus.mix = 0.2f;
    room(p, ReverbType::Plate, 1.6f, 0.18f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Vowel
    macro(p, 2, 0.0f, { to(oscEnvLevel(4), 0.3f) });                                         // Metal
    p.vca.patchLevel = 1.0f; // level-matched across the bank
    return p;
}

// Hard sync through shaped tables: a folded sine and a pulse train restarted by a sync modulator whose
// pitch sweeps with an envelope. Mono with legato glide; the wheel pushes the sweep further.
Patch tableSync()
{
    Patch p = blank("One Pair");
    osc(p, 1, SineFold, 1.0f, 0.5f);
    OscParams& m = osc(p, 2, Sine, 1.0f, 0.15f);
    m.mode = OscMode::HardSync;
    envLevel(m, 0.55f, 0.0f, 1200.0f, 0.3f, 300.0f, 0.4f);
    osc(p, 3, PulseTrain, 1.0f, 0.18f).ratioFine = 0.3f;
    p.dfilt.cutoff = cutoff(4200.0f);
    p.dfilt.resonance = 0.2f;
    p.dfilt.keytrack = 0.5f;
    adsr(p.env[2], 2.0f, 400.0f, 0.85f, 250.0f);
    route(p, src::ModWheel, oscLevel(2), 0.3f);
    p.voice.polyMode = PolyMode::Mono;
    p.voice.glideMode = GlideMode::Glide;
    p.voice.glideTime = 0.1f;
    p.voice.glideLegato = true;
    echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.2f);
    room(p, ReverbType::Plate, 1.5f, 0.15f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.35f) });                                        // Sweep
    macro(p, 2, 0.0f, { to(oscLevel(3), 0.25f) });                                           // Pulse
    p.vca.patchLevel = 2.0f; // level-matched across the bank
    return p;
}

// Digital stair tables played raw: Vintage Digital reads Stairs 8 and Step Saw without smoothing,
// at 8 bits, through an up arpeggio. Hold a chord.
Patch steppedArp()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Stairs8, 1.0f, 0.6f);
    osc(p, 2, StepSaw, 2.0f, 0.22f).ratioFine = 0.15f;
    osc(p, 3, Square, 0.5f, 0.3f);
    p.voice.vintage = true;
    p.voice.bitDepth = 8;
    p.afilt.cutoff = cutoff(2400.0f);
    p.afilt.resonance = 0.35f;
    p.afilt.envAmt = 0.35f;
    adsr(p.env[1], 0.0f, 200.0f, 0.0f, 120.0f);
    adsr(p.env[2], 1.0f, 240.0f, 0.3f, 160.0f);
    p.vca.velEnv = 0.5f;
    p.arp.enabled = true;
    p.arp.mode = ArpMode::Up;
    p.arp.octaves = 2;
    p.arp.syncDiv = syncDiv("1/16");
    p.arp.gate = 0.5f;
    echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.22f);
    room(p, ReverbType::Room, 1.2f, 0.15f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Brightness
    macro(p, 2, 0.0f, { to(oscLevel(2), 0.25f) });                                           // Steps
    p.vca.patchLevel = 2.0f; // level-matched across the bank
    return p;
}

// A textural drone from the Grain table: three grain layers at drifting ratios over a Pulse Train
// sub, their levels wandering on random LFOs, into a long cloud.
Patch grainField()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Grain, 1.0f, 0.22f);
    osc(p, 2, Grain, 1.5f, 0.14f).ratioFine = 0.3f;
    osc(p, 3, Grain, 2.0f, 0.1f).ratioFine = -0.4f;
    osc(p, 4, PulseTrain, 0.5f, 0.18f);
    p.lfo[2].wave = LfoWave::Random;
    p.lfo[2].rateHz = 0.3f;
    p.lfo[2].smooth = 0.8f;
    p.lfo[2].trig = LfoTrig::Free;
    p.lfo[4].wave = LfoWave::Random;
    p.lfo[4].rateHz = 0.45f;
    p.lfo[4].smooth = 0.8f;
    p.lfo[4].trig = LfoTrig::Free;
    route(p, src::Lfo1 + 2, oscLevel(2), 0.12f);
    route(p, src::Lfo1 + 4, oscLevel(3), 0.1f);
    route(p, src::Lfo1 + 4, dst::osc(0, dst::OscFine), 0.05f);
    p.afilt.cutoff = cutoff(3000.0f);
    p.afilt.resonance = 0.2f;
    adsr(p.env[2], 1500.0f, 2000.0f, 1.0f, 3000.0f);
    room(p, ReverbType::Cloud, 8.0f, 0.4f, 60.0f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Brightness
    macro(p, 2, 0.0f, { to(oscLevel(4), 0.2f) });                                            // Sub
    p.vca.patchLevel = 1.4f; // level-matched across the bank
    return p;
}

// Bass from shaped tables: a Skewed Sine carrier bent further by a PD Saw-Pulse modulator on a
// filter-like envelope, with a Parabola sub an octave down.
Patch skewBass()
{
    Patch p = blank("One Pair");
    osc(p, 1, SineSkew, 1.0f, 0.5f);
    OscParams& m = osc(p, 2, Sine, 1.0f, 0.1f);
    m.mode = OscMode::PDSawPulse;
    envLevel(m, 0.5f, 0.0f, 350.0f, 0.15f, 150.0f, 0.5f);
    osc(p, 3, Parabola, 0.5f, 0.35f);
    p.voice.polyMode = PolyMode::Mono;
    p.voice.glideMode = GlideMode::Glide;
    p.voice.glideTime = 0.08f;
    p.voice.glideLegato = true;
    p.afilt.cutoff = cutoff(1800.0f);
    p.afilt.preDrive = 0.3f;
    adsr(p.env[2], 1.0f, 400.0f, 0.85f, 120.0f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.35f) });                                        // Bite
    macro(p, 2, 0.0f, { to(oscLevel(3), 0.25f) });                                           // Sub
    p.vca.patchLevel = 0.9f; // level-matched across the bank
    return p;
}

// A mallet built from three tables: Bump for the woody body, a fast Bell Partials strike and an
// FM Hollow resonance that rings a little longer. Keyscale tames the top octaves.
Patch tableMallets()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Bump, 1.0f, 0.0f);
    envLevel(p.osc[0], 0.5f, 0.0f, 900.0f, 0.0f, 600.0f, 0.4f);
    envLevel(osc(p, 2, BellPartials, 4.0f, 0.0f), 0.2f, 0.0f, 120.0f, 0.0f, 80.0f, 0.8f);
    envLevel(osc(p, 3, FmHollow, 1.0f, 0.0f), 0.25f, 0.0f, 1800.0f, 0.0f, 1000.0f, 0.5f);
    for (int i : { 0, 1, 2 })
    {
        OscParams& o = p.osc[static_cast<size_t>(i)];
        o.keyscaleBreak = 72;
        o.keyscaleHiDepth = 0.5f;
        o.keyscaleHiCurve = 0.3f;
    }
    adsr(p.env[2], 0.0f, 2500.0f, 0.0f, 1200.0f);
    p.vca.velEnv = 0.6f;
    echo(p, DelayType::Stereo, "1/8D", 0.3f, 0.18f);
    room(p, ReverbType::Room, 1.6f, 0.2f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.25f) });                                        // Strike
    macro(p, 2, 0.0f, { to(oscEnvLevel(3), 0.25f) });                                        // Ring
    p.vca.patchLevel = 1.35f; // level-matched across the bank
    return p;
}

struct Entry
{    FactoryPresetInfo info;
    Patch (*make)();
};

const Entry kPresets[] = {
    { { "Velvet Tines", "Keys", "FM electric piano; velocity brings in the bark. Wheel: tine.", 60, false }, velvetTines },
    { { "Glass Carillon", "Bells", "Inharmonic FM bell with a long hall.", 72, false }, glassCarillon },
    { { "Drawbar Chapel", "Organ", "Sine drawbars, key click, overdrive and rotary. Wheel: fast rotor.", 60, true }, drawbarChapel },
    { { "Rubber Floor", "Bass", "Mono FM bass with sub and legato glide.", 36, true }, rubberFloor },
    { { "Saw Pressure", "Bass", "Phase-distortion saw bass through the analog ladder.", 36, true }, sawPressure },
    { { "Sync Sweeper", "Lead", "Hard-sync lead; envelope and wheel push the sync.", 72, true }, syncSweeper },
    { { "Narrow Gauge", "Lead", "PW Mod square lead with slow pulse-width movement.", 67, true }, narrowGauge },
    { { "Phase Brass", "Brass", "PD saw brass ensemble with delayed vibrato.", 60, true }, phaseBrass },
    { { "Slow Tide", "Pad", "Algorithm morph: the voice slowly rewires itself.", 60, true }, slowTide },
    { { "Vowel Choir", "Pad", "Detuned saws through a moving vowel filter. Wheel: open.", 60, true }, vowelChoir },
    { { "Kalimba Drop", "Pluck", "Woody FM pluck with dotted echoes.", 72, false }, kalimbaDrop },
    { { "Harp Mist", "Pluck", "Resonant PD pluck with ping-pong delay.", 60, false }, harpMist },
    { { "Step Machine", "Sequence", "Tempo-synced 16-step filter and FM pattern. Hold a note.", 48, true }, stepMachine },
    { { "Metal Rain", "FX", "Feedback ring of eight inharmonic operators.", 60, false }, metalRain },
    { { "Unison Wall", "Lead", "Six-voice detuned unison with a sub.", 60, true }, unisonWall },
    { { "Deep Field", "Drone", "Breathing FM drone with a huge cloud reverb.", 48, true }, deepField },
    { { "Clockwork Garden", "Showcase", "Custom algorithm with a feedback loop; looping tempo envelopes tick 3 against 2; reverse delay.", 60, true, "pad" }, clockworkGarden },
    { { "Living Strings", "Showcase", "Unison Poly ensemble: per-note randomness, staggered LFOs, release velocity sets the tail.", 60, true, "pad" }, livingStrings },
    { { "Pressure Organism", "Showcase", "Expressive: pressure scales a random-FM route (depth of depth); poly pressure; key scaling.", 60, true, "chords" }, pressureOrganism },
    { { "Fixed Point Kit", "Showcase", "Percussion: fixed-frequency modulators, Direct Out noise, pitch envelope, low-pass gate.", 48, false, "hits" }, fixedPointKit },
    { { "Glitch Conveyor", "Showcase", "Per-note step LFO, S&H warp and lo-fi, glissando, tempo-ducked compressor.", 48, true, "line" }, glitchConveyor },
    { { "Grand Tour", "Showcase", "Every feature at once: all oscillator modes, morph, both filters, all LFOs, all 8 macros, full FX.", 60, true, "pad" }, grandTour },
    { { "Pulse Lattice", "Arp", "Up/down arpeggio over two octaves in swung 16ths. Hold a chord.", 60, true, "arp" }, pulseLattice },
    { { "Glass Stairs", "Arp", "Random three-octave arpeggio of glassy FM through long echoes. Hold a chord.", 60, true, "arp" }, glassStairs },
    { { "Stereo Glass", "Keys", "Osc Stereo electric piano: body left, tine right, carrier detune. Macro 2: width.", 60, false }, stereoGlass },
    { { "Wide Reed", "Lead", "Reed wave on a stereo voice pair with delayed vibrato and legato glide.", 67, true }, wideReed },
    { { "Bit Choir", "Pad", "Two choir waves at 10 bits, slowly trading places.", 60, true }, bitChoir },
    { { "Retro Bells", "Bells", "Vintage Digital FM bells (aliasing shimmer) with keyscaled top end.", 72, false }, retroBells },
    { { "Pan Pluck", "Pluck", "Soft-saw pluck; the Panner walks each new note across the stereo field.", 60, false }, panPluck },
    { { "Gated Pulse", "Sequence", "LFO 4 retriggers the VCA envelope every 16th: hold a chord.", 48, true, "pad" }, gatedPulse },
    { { "Strata Strings", "Pad", "Strings wave ensemble; Voice Mod gives every voice its own tuning and place.", 60, true }, strataStrings },
    { { "Scaled Organ", "Organ", "Full organ wave, keyscaled; wheel speeds the rotor.", 60, true }, scaledOrgan },
    { { "Fold Bass", "Bass", "Folded sine under a saw stack, mono legato glide.", 36, true }, foldBass },
    { { "Pressure Glide", "Lead", "MPE lead: per-note bend, pressure and CC74 timbre. Works on any keyboard.", 64, true }, pressureGlide },
    { { "Wave Scanner", "Pad", "Four tables (Sine, Formant, Hollow, Saw Stack) cross-faded by slow LFOs. Macro 1: scan speed.", 60, true }, waveScanner },
    { { "Partial Bloom", "Pad", "Additive tables arrive in turn: odd, even, prime, then square partials. Hold a chord.", 60, true }, partialBloom },
    { { "Formant Tines", "Keys", "Tables as modulators: Formant and Bell Partials phase-modulate two sines.", 60, false }, formantTines },
    { { "Table Sync", "Lead", "Hard sync through Sine Fold and Pulse Train tables; wheel pushes the sweep.", 67, true }, tableSync },
    { { "Stepped Arp", "Arp", "Stairs 8 and Step Saw read raw (Vintage Digital, 8 bits) in an up arpeggio. Hold a chord.", 60, true, "arp" }, steppedArp },
    { { "Grain Field", "Drone", "Three Grain-table layers over a Pulse Train sub, wandering on random LFOs.", 48, true }, grainField },
    { { "Skew Bass", "Bass", "Skewed Sine bent by a PD Saw-Pulse modulator, Parabola sub, mono glide.", 36, true }, skewBass },
    { { "Table Mallets", "Pluck", "Bump body, Bell Partials strike and FM Hollow ring, keyscaled.", 60, false }, tableMallets },
};
constexpr int kNumPresets = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

} // namespace

int findPresetAlgorithm(const char* name) noexcept
{
    for (int i = 0; i < numPresetAlgorithms(); ++i)
        if (std::strcmp(presetAlgorithmName(i), name) == 0) return i;
    return -1;
}

int numFactoryPresets() noexcept { return kNumPresets; }

const char* factoryPresetTags(int index) noexcept
{
    static const char* const tags[] = {
        "fm, electric piano, velocity, soft",        // Velvet Tines
        "fm, bell, inharmonic, long",                // Glass Carillon
        "organ, additive, rotary, drive",            // Drawbar Chapel
        "fm, mono, glide, sub",                      // Rubber Floor
        "phase distortion, ladder, punchy",          // Saw Pressure
        "hard sync, sweep, bright",                  // Sync Sweeper
        "pwm, square, vintage",                      // Narrow Gauge
        "phase distortion, ensemble, vibrato",       // Phase Brass
        "morph, evolving, slow",                     // Slow Tide
        "vowel filter, voice, evolving",             // Vowel Choir
        "fm, short, echo",                           // Kalimba Drop
        "phase distortion, resonant, echo",          // Harp Mist
        "step lfo, tempo, rhythmic",                 // Step Machine
        "fm, inharmonic, chaotic, ambient",          // Metal Rain
        "unison, wide, saw",                         // Unison Wall
        "fm, ambient, dark, evolving",               // Deep Field
        "custom algorithm, looping envelopes, rhythmic", // Clockwork Garden
        "unison poly, ensemble, expressive",         // Living Strings
        "aftertouch, expressive, fm",                // Pressure Organism
        "percussion, fixed frequency, drums",        // Fixed Point Kit
        "step lfo, glitch, lo-fi, rhythmic",         // Glitch Conveyor
        "morph, evolving, everything",               // Grand Tour
        "arp, tempo, resonant",                      // Pulse Lattice
        "arp, random, echo, fm",                     // Glass Stairs
        "osc stereo, fm, electric piano, wide",      // Stereo Glass
        "osc stereo, reed, mono, glide",             // Wide Reed
        "bit depth, choir, lo-fi, evolving",         // Bit Choir
        "vintage digital, fm, bell, aliasing",       // Retro Bells
        "panner, pluck, stereo, echo",               // Pan Pluck
        "envelope triggers, gate, tempo, rhythmic",  // Gated Pulse
        "voice mod, strings, ensemble, wide",        // Strata Strings
        "keyscale, organ, rotary, drive",            // Scaled Organ
        "mono, glide, wavefold, sub",                // Fold Bass
        "mpe, expressive, aftertouch, timbre",       // Pressure Glide
        "wavetable, scan, crossfade, evolving",      // Wave Scanner
        "wavetable, additive, evolving, slow",       // Partial Bloom
        "wavetable, fm, formant, electric piano",    // Formant Tines
        "wavetable, hard sync, mono, glide",         // Table Sync
        "wavetable, arp, vintage digital, lo-fi",    // Stepped Arp
        "wavetable, grain, texture, ambient",        // Grain Field
        "wavetable, phase distortion, mono, sub",    // Skew Bass
        "wavetable, mallet, percussive, keyscale",   // Table Mallets
    };
    constexpr int n = static_cast<int>(sizeof(tags) / sizeof(tags[0]));
    return index >= 0 && index < n ? tags[index] : "";
}

const FactoryPresetInfo& factoryPresetInfo(int index) noexcept
{
    return kPresets[index < 0 || index >= kNumPresets ? 0 : index].info;
}

Patch makeFactoryPreset(int index)
{
    if (index < 0 || index >= kNumPresets) return makeInitPatch();
    return kPresets[index].make();
}

} // namespace kmi
