// Mod matrix source and destination indices. These are part of the preset format: append only.
#pragma once

#include <string>
#include <vector>

#include "kmi/Constants.h"

namespace kmi {

namespace src {
constexpr int None = 0;
constexpr int Env1 = 1;                         // .. Env5
constexpr int OscEnv1 = Env1 + kNumEnvs;        // .. OscEnv8
constexpr int Lfo1 = OscEnv1 + kNumOscs;        // .. Lfo5 (bipolar)
constexpr int Lfo1Uni = Lfo1 + kNumLfos;        // .. Lfo5+ (unipolar)
constexpr int Keytrack = Lfo1Uni + kNumLfos;
constexpr int Velocity = Keytrack + 1;
constexpr int ReleaseVelocity = Velocity + 1;
constexpr int PolyAT = ReleaseVelocity + 1;
constexpr int ChannelAT = PolyAT + 1;
constexpr int ModWheel = ChannelAT + 1;
constexpr int PitchBend = ModWheel + 1;
constexpr int VoiceMod = PitchBend + 1;
constexpr int VoiceModUni = VoiceMod + 1;
constexpr int Expression = VoiceModUni + 1;
constexpr int SustainPedal = Expression + 1;
constexpr int Macro1 = SustainPedal + 1;        // .. Macro8
constexpr int RandomPerNote = Macro1 + kNumMacros;
constexpr int Cc0 = RandomPerNote + 1;          // .. Cc127
constexpr int Count = Cc0 + 128;
} // namespace src

namespace dst {
constexpr int None = 0;

enum OscParam { OscLevel, OscEnvLevel, OscPitch, OscFine, OscFeedback, OscPan, kOscParams };
constexpr int OscBase = 1;                                  // + osc * kOscParams + param
constexpr int AllOscBase = OscBase + kNumOscs * kOscParams; // + param
constexpr int CarrierLevel = AllOscBase + kOscParams;
constexpr int CarrierEnvLevel = CarrierLevel + 1;
constexpr int ModulatorLevel = CarrierLevel + 2;
constexpr int ModulatorEnvLevel = CarrierLevel + 3;

enum DFiltParam { DfCutoff, DfResonance, DfMorph, DfDrive, DfEnvAmt, DfLfoAmt, DfLevel, kDFiltParams };
constexpr int DFiltBase = CarrierLevel + 4;

enum AFiltParam { AfCutoff, AfResonance, AfPreDrive, AfEnvAmt, AfLfoAmt, kAFiltParams };
constexpr int AFiltBase = DFiltBase + kDFiltParams;

enum VcaParam { VcaLevel, VcaLfoAmt, kVcaParams };
constexpr int VcaBase = AFiltBase + kAFiltParams;

enum EnvParam { EnvAttack, EnvDecay, EnvSustain, EnvRelease, EnvLevel, kEnvParams };
constexpr int EnvBase = VcaBase + kVcaParams;               // + env * kEnvParams + param

enum LfoParam { LfoRate, LfoLevel, kLfoParams };
constexpr int LfoBase = EnvBase + kNumEnvs * kEnvParams;    // + lfo * kLfoParams + param

enum VoiceParam { VoiceUnisonDetune, VoicePan, VoiceVibratoAmt, VoiceGlideTime, kVoiceParams };
constexpr int VoiceBase = LfoBase + kNumLfos * kLfoParams;

constexpr int AlgoMorph = VoiceBase + kVoiceParams;
constexpr int ModDepthBase = AlgoMorph + 1;                 // + slot

// Effects are global: their destinations take values from the most recently started voice.
enum FxDest {
    PreFxParam1, PreFxParam2, PreFxMix,
    DelayTime, DelayFeedback, DelayWetTone, DelayFeedbackTone, DelayMix,
    ReverbTime, ReverbTone, ReverbHiDamp, ReverbLoDamp, ReverbMix,
    PostFxParam1, PostFxParam2, PostFxMix,
    kFxDests
};
constexpr int FxBase = ModDepthBase + kNumModSlots;
constexpr int Count = FxBase + kFxDests;

constexpr bool isModDepth(int d) noexcept { return d >= ModDepthBase && d < FxBase; }

constexpr int osc(int oscIndex, OscParam p) noexcept { return OscBase + oscIndex * kOscParams + p; }
constexpr int env(int envIndex, EnvParam p) noexcept { return EnvBase + envIndex * kEnvParams + p; }
constexpr int lfo(int lfoIndex, LfoParam p) noexcept { return LfoBase + lfoIndex * kLfoParams + p; }
} // namespace dst

const std::vector<std::string>& modSourceNames();
const std::vector<std::string>& modDestNames();

} // namespace kmi
