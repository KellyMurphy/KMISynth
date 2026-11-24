#pragma once

namespace kmi {

constexpr int kNumOscs = 8;
constexpr int kMaxVoices = 16;
constexpr int kNumEnvs = 5;       // Env 1-5 (Env 1 -> digital filter, 2 -> analog filter, 3 -> VCA)
constexpr int kNumLfos = 5;       // LFO 1-5 (LFO 1 -> digital filter, 2 -> analog filter, 3 -> VCA)
constexpr int kNumModSlots = 32;
constexpr int kNumMacros = 8;
constexpr int kNumMacroRoutes = 8;
constexpr int kNumMorphSlots = 8;
constexpr int kMaxLfoSteps = 64;

} // namespace kmi
