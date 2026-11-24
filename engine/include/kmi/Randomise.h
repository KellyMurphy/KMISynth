// Patch randomiser: moves a patch part of the way (amount) towards a random but playable patch. Groups
// can be locked. The result is musical on purpose: carriers keep a sensible level for the algorithm,
// modulators get harmonic ratios most of the time, filters never close completely.
#pragma once

#include <cstdint>

#include "kmi/Patch.h"

namespace kmi {

struct RandomiseOptions
{
    float amount = 0.5f; // 0 = no change .. 1 = fully random
    bool algorithm = true;   // algorithm choice
    bool oscillators = true; // waves, modes, ratios, levels, feedback, oscillator envelopes, wave scan
    bool envelopes = true;   // Env 1-3 (filters and VCA)
    bool filters = true;     // digital and analog filter settings
    bool modulation = true;  // LFO rates and shapes
    bool effects = true;     // insert types, delay and reverb
};

void randomisePatch(Patch& p, const RandomiseOptions& options, uint32_t seed);

} // namespace kmi
