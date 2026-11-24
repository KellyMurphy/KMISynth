#include "kmi/Modulation.h"

#include <cassert>

namespace kmi {

namespace {
std::string n(int i) { return std::to_string(i); }

std::vector<std::string> buildSources()
{
    std::vector<std::string> s;
    s.reserve(src::Count);
    s.push_back("None");
    for (int i = 1; i <= kNumEnvs; ++i) s.push_back("Env " + n(i));
    for (int i = 1; i <= kNumOscs; ++i) s.push_back("Osc " + n(i) + " Env");
    for (int i = 1; i <= kNumLfos; ++i) s.push_back("LFO " + n(i));
    for (int i = 1; i <= kNumLfos; ++i) s.push_back("LFO " + n(i) + "+");
    s.push_back("Keytrack");
    s.push_back("Velocity");
    s.push_back("Release Velocity");
    s.push_back("Poly Aftertouch");
    s.push_back("Channel Aftertouch");
    s.push_back("Mod Wheel");
    s.push_back("Pitch Bend");
    s.push_back("Voice Mod");
    s.push_back("Voice Mod+");
    s.push_back("Expression");
    s.push_back("Sustain Pedal");
    for (int i = 1; i <= kNumMacros; ++i) s.push_back("Macro " + n(i));
    s.push_back("Random per Note");
    for (int i = 0; i < 128; ++i) s.push_back("CC " + n(i));
    s.push_back("Timbre (MPE CC74)");
    assert(static_cast<int>(s.size()) == src::Count);
    return s;
}

std::vector<std::string> buildDests()
{
    static const char* oscP[] = { "Level", "Env Level", "Pitch", "Fine", "Feedback", "Pan" };
    static const char* dfP[] = { "Cutoff", "Resonance", "Morph", "Drive", "Env 1 Amt", "LFO 1 Amt", "Level" };
    static const char* afP[] = { "Cutoff", "Resonance", "Pre-Drive", "Env 2 Amt", "LFO 2 Amt" };
    static const char* envP[] = { "Attack", "Decay", "Sustain", "Release", "Level" };
    static const char* lfoP[] = { "Rate", "Level" };
    static const char* voiceP[] = { "Unison Detune", "Pan", "Vibrato Amount", "Glide Time" };

    std::vector<std::string> d;
    d.reserve(dst::Count);
    d.push_back("None");
    for (int o = 1; o <= kNumOscs; ++o)
        for (const char* p : oscP) d.push_back("Osc " + n(o) + " " + p);
    for (const char* p : oscP) d.push_back(std::string("All Osc ") + p);
    d.push_back("Carriers Level");
    d.push_back("Carriers Env Level");
    d.push_back("Modulators Level");
    d.push_back("Modulators Env Level");
    for (const char* p : dfP) d.push_back(std::string("D.Filter ") + p);
    for (const char* p : afP) d.push_back(std::string("A.Filter ") + p);
    d.push_back("VCA Level");
    d.push_back("VCA LFO 3 Amt");
    for (int e = 1; e <= kNumEnvs; ++e)
        for (const char* p : envP) d.push_back("Env " + n(e) + " " + p);
    for (int l = 1; l <= kNumLfos; ++l)
        for (const char* p : lfoP) d.push_back("LFO " + n(l) + " " + p);
    for (const char* p : voiceP) d.push_back(std::string("Voice ") + p);
    d.push_back("Algorithm Morph");
    for (int m = 1; m <= kNumModSlots; ++m) d.push_back("Mod " + n(m) + " Depth");
    for (const char* f : { "Pre-FX Param 1", "Pre-FX Param 2", "Pre-FX Dry/Wet", "Delay Time", "Delay Feedback",
                           "Delay Wet Tone", "Delay Feedback Tone", "Delay Dry/Wet", "Reverb Time", "Reverb Tone",
                           "Reverb Hi Damp", "Reverb Lo Damp", "Reverb Dry/Wet", "Post-FX Param 1", "Post-FX Param 2",
                           "Post-FX Dry/Wet" })
        d.push_back(f);
    assert(static_cast<int>(d.size()) == dst::Count);
    return d;
}
} // namespace

const std::vector<std::string>& modSourceNames()
{
    static const std::vector<std::string> names = buildSources();
    return names;
}

const std::vector<std::string>& modDestNames()
{
    static const std::vector<std::string> names = buildDests();
    return names;
}

} // namespace kmi
