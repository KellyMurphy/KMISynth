// Algorithms: how the 8 oscillators of a voice modulate each other and which reach the output.
#pragma once

#include "kmi/Constants.h"

namespace kmi {

struct Algorithm
{
    // conn[m][c] > 0 means oscillator m modulates oscillator c (weight 0..1, fractional when morphing).
    float conn[kNumOscs][kNumOscs] = {};
    // out[i] > 0 means oscillator i is routed to the audio path (carrier / Direct Out).
    float out[kNumOscs] = {};

    bool isCarrier(int i) const noexcept { return out[i] > 0.0f; }
    bool isModulator(int i) const noexcept
    {
        for (int c = 0; c < kNumOscs; ++c)
            if (conn[i][c] > 0.0f) return true;
        return false;
    }
};

int numPresetAlgorithms() noexcept;
bool presetAlgorithmsValid() noexcept; // every preset definition parsed cleanly
const Algorithm& presetAlgorithm(int index) noexcept;
const char* presetAlgorithmName(int index) noexcept;

// Morph slot encoding (AlgoParams::morphSlots): 0 = Off, 1 = Silence, 2+ = preset index + 2.
constexpr int kMorphSlotOff = 0;
constexpr int kMorphSlotSilence = 1;
constexpr int kMorphSlotFirstPreset = 2;

void lerpAlgorithm(const Algorithm& a, const Algorithm& b, float t, Algorithm& out) noexcept;

// Parses "8>7 7>6 2>1" edge lists and "1 3 5" carrier lists (1-based). Returns false on malformed input.
bool parseAlgorithm(const char* edges, const char* carriers, Algorithm& out) noexcept;

} // namespace kmi
