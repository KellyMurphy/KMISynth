#include "kmi/Patch.h"

#include <algorithm>

#include "kmi/Dsp.h"

namespace kmi {

Patch makeInitPatch()
{
    Patch p;
    p.osc[0].initLevel = 1.0f;
    p.customAlgorithm = presetAlgorithm(0);
    for (auto& steps : p.lfoSteps) steps.fill(0.0f);
    return p;
}

void resolveAlgorithm(const Patch& p, float morphOffset, Algorithm& out) noexcept
{
    switch (p.algo.mode)
    {
        case AlgoMode::Custom:
            out = p.customAlgorithm;
            out.out[0] = 1.0f;
            return;

        case AlgoMode::Morph:
        {
            static const Algorithm silence{};
            const Algorithm* slots[kNumMorphSlots];
            int n = 0;
            for (int i = 0; i < kNumMorphSlots; ++i)
            {
                const int s = p.algo.morphSlots[static_cast<size_t>(i)];
                if (s == kMorphSlotOff && i > 0) continue; // slot 1 cannot be Off
                if (s == kMorphSlotSilence) slots[n++] = &silence;
                else slots[n++] = &presetAlgorithm(std::max(0, s - kMorphSlotFirstPreset));
            }
            const float pos = clampf(p.algo.morphPos + morphOffset, 0.0f, static_cast<float>(n - 1));
            const int i0 = static_cast<int>(pos);
            const int i1 = std::min(i0 + 1, n - 1);
            lerpAlgorithm(*slots[i0], *slots[i1], pos - static_cast<float>(i0), out);
            return;
        }

        case AlgoMode::Single:
        default:
            out = presetAlgorithm(p.algo.index);
            return;
    }
}

} // namespace kmi
