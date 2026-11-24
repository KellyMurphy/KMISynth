// Factory presets: original demo patches that show off the engine. Built in code so the tests can
// render every one of them; the plugin converts them to parameter values when loaded.
#pragma once

#include "kmi/Patch.h"

namespace kmi {

struct FactoryPresetInfo
{
    const char* name;
    const char* category;
    const char* description;
    int demoNote;      // a representative MIDI note for auditioning (bass presets sit low, etc.)
    bool sustained;    // true if the sound keeps going while a key is held (pads, organs, drones)
    // Optional audition phrase for the demo renderer ("line", "lead", "pad", "chords", "keys", "hits");
    // null = chosen from the category.
    const char* demoStyle = nullptr;
};

int numFactoryPresets() noexcept;
const FactoryPresetInfo& factoryPresetInfo(int index) noexcept;
Patch makeFactoryPreset(int index);

// Index of a preset algorithm by name, or -1 (used by the preset definitions; exposed for tests).
int findPresetAlgorithm(const char* name) noexcept;

} // namespace kmi
