// Host-facing parameter set. One description (describe() in Parameters.cpp) drives both the
// APVTS layout and the per-block copy of parameter values into the engine's kmi::Patch.
#pragma once

#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "kmi/Patch.h"

namespace kmiplug {

class ParameterBinding
{
public:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Connect every parameter to its field in `patch`. Call once after the APVTS exists.
    void bind(juce::AudioProcessorValueTreeState& apvts, kmi::Patch& patch);

    // Audio thread: copy current parameter values into the bound patch. Lock-free.
    void apply() const noexcept;

    // Message thread: set every parameter from `patch` (the inverse of apply), notifying the host.
    static void writePatch(juce::AudioProcessorValueTreeState& apvts, const kmi::Patch& patch);

    // IDs of parameters whose current value does not reproduce `patch` (beyond the parameter's
    // own step size). Used to verify that presets survive the round trip through the parameters.
    static juce::StringArray mismatches(juce::AudioProcessorValueTreeState& apvts, const kmi::Patch& patch);

    enum class Kind : uint8_t { Float, Int, Bool, Enum8, EnvLoop };

    struct Entry
    {
        std::atomic<float>* source = nullptr;
        void* target = nullptr;
        Kind kind = Kind::Float;
        float scale = 1.0f;
        float offset = 0.0f;
    };

private:
    std::vector<Entry> entries_;
};

} // namespace kmiplug
