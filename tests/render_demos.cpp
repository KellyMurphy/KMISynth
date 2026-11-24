// Renders every factory preset playing a short phrase to a 24-bit stereo WAV, and prints levels.
// Usage: kmi_render <output-folder> [preset-index]
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "kmi/Engine.h"
#include "kmi/Presets.h"

using namespace kmi;

namespace {
constexpr double kRate = 48000.0;

struct Note
{
    float start, length; // seconds
    int note;
    float velocity;
};

// Phrases are original and simple: enough to show each sound's character.
std::vector<Note> phraseFor(const FactoryPresetInfo& info)
{
    std::string cat = info.category;
    if (info.demoStyle)
    {
        const std::string st = info.demoStyle;
        cat = st == "line" ? "Bass" : st == "lead" ? "Lead" : st == "pad" ? "Pad" : st == "chords" ? "Organ" : st == "hits" ? "Hits" : st == "arp" ? "Arp" : "Keys";
    }
    const int r = info.demoNote;
    std::vector<Note> notes;
    auto chord = [&](float t, float len, std::initializer_list<int> ivs, float vel) {
        for (int iv : ivs) notes.push_back({ t, len, r + iv, vel });
    };
    if (cat == "Bass" || cat == "Sequence")
    {
        const int line[] = { 0, 0, 12, 0, 7, 10, 0, 3, 0, 0, 12, 0, 5, 7, 3, 2 };
        const float step = cat == "Sequence" ? 0.5f : 0.25f;
        const int count = cat == "Sequence" ? 8 : 16;
        for (int i = 0; i < count; ++i)
            notes.push_back({ i * step, step * (i % 4 == 3 ? 1.0f : 0.8f), r + (cat == "Sequence" ? line[i * 2] % 12 : line[i]),
                              i % 4 == 0 ? 0.95f : 0.7f });
        if (cat == "Sequence") notes.back().length = 4.0f - 7 * step; // hold the last note into the pattern
    }
    else if (cat == "Lead")
    {
        const int line[] = { 0, 3, 5, 7, 10, 7, 12, 10, 7 };
        const float lens[] = { 0.4f, 0.2f, 0.2f, 0.6f, 0.3f, 0.3f, 1.2f, 0.3f, 1.4f };
        float t = 0.0f;
        for (int i = 0; i < 9; ++i)
        {
            notes.push_back({ t, lens[i] * 1.02f, r + line[i] - 12, 0.85f }); // slight overlap: legato glide
            t += lens[i];
        }
    }
    else if (cat == "Pad" || cat == "Drone")
    {
        chord(0.0f, 4.0f, { -12, 0, 4, 7, 11 }, 0.7f);
        chord(4.0f, 4.0f, { -15, -3, 0, 4, 9 }, 0.7f);
    }
    else if (cat == "Organ" || cat == "Brass")
    {
        chord(0.0f, 1.4f, { -12, 0, 4, 7 }, 0.8f);
        chord(1.5f, 0.6f, { -10, 2, 5, 9 }, 0.8f);
        chord(2.2f, 2.0f, { -8, 0, 4, 7, 11 }, 0.85f);
    }
    else if (cat == "Hits")
    {
        // Two bars of a simple pattern: low hits on the beat, mid hits off the beat, high ticks.
        for (int i = 0; i < 16; ++i)
        {
            const float t = i * 0.25f;
            if (i % 4 == 0) notes.push_back({ t, 0.1f, r - 12, 1.0f });
            if (i % 4 == 2) notes.push_back({ t, 0.1f, r + 7, 0.8f });
            notes.push_back({ t + 0.125f, 0.05f, r + 24, i % 2 ? 0.5f : 0.3f });
        }
    }
    else if (cat == "Arp")
    {
        // Held chords: the arpeggiator does the playing.
        chord(0.0f, 2.0f, { -12, 0, 4, 7 }, 0.85f);
        chord(2.0f, 2.0f, { -15, -3, 0, 4 }, 0.8f);
        chord(4.0f, 2.0f, { -17, -5, 2, 7 }, 0.8f);
    }
    else if (cat == "FX")
    {
        notes.push_back({ 0.0f, 0.5f, r, 0.9f });
        notes.push_back({ 0.7f, 0.5f, r + 7, 0.7f });
        notes.push_back({ 1.4f, 2.0f, r - 5, 1.0f });
    }
    else // Keys, Bells, Pluck: a broken chord, then the chord
    {
        const int arp[] = { 0, 4, 7, 11, 14, 11, 7, 4 };
        for (int i = 0; i < 8; ++i) notes.push_back({ i * 0.25f, 0.3f, r + arp[i] - 12, i % 2 == 0 ? 0.9f : 0.55f });
        chord(2.2f, 1.8f, { -24, -12, -5, 4, 9 }, 0.8f);
    }
    return notes;
}

void put24(std::vector<uint8_t>& out, float v)
{
    const int32_t s = static_cast<int32_t>(std::lround(std::clamp(v, -1.0f, 1.0f) * 8388607.0f));
    out.push_back(static_cast<uint8_t>(s & 0xFF));
    out.push_back(static_cast<uint8_t>((s >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((s >> 16) & 0xFF));
}

bool writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r)
{
    std::vector<uint8_t> data;
    data.reserve(l.size() * 6);
    for (size_t i = 0; i < l.size(); ++i)
    {
        put24(data, l[i]);
        put24(data, r[i]);
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f);
    u32(static_cast<uint32_t>(36 + data.size()));
    std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(2);
    u32(static_cast<uint32_t>(kRate));
    u32(static_cast<uint32_t>(kRate) * 6);
    u16(6);
    u16(24);
    std::fwrite("data", 1, 4, f);
    u32(static_cast<uint32_t>(data.size()));
    std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return true;
}

double dbfs(double x) { return 20.0 * std::log10(std::max(x, 1e-9)); }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf("usage: kmi_render <output-folder> [preset-index]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int only = argc > 2 ? std::atoi(argv[2]) : -1;

    std::printf("%-3s %-16s %-9s %8s %8s %8s\n", "#", "preset", "category", "peak dB", "rms dB", "tail dB");
    for (int i = 0; i < numFactoryPresets(); ++i)
    {
        if (only >= 0 && i != only) continue;
        const FactoryPresetInfo& info = factoryPresetInfo(i);
        Engine e;
        e.patch() = makeFactoryPreset(i);
        e.prepare(kRate);
        e.setTempo(120.0);

        auto notes = phraseFor(info);
        float end = 0.0f;
        for (const Note& n : notes) end = std::max(end, n.start + n.length);
        const float seconds = end + 3.5f;
        const int total = static_cast<int>(seconds * kRate);

        struct Event { int sample; bool on; int note; float vel; };
        std::vector<Event> events;
        for (const Note& n : notes)
        {
            events.push_back({ static_cast<int>(n.start * kRate), true, n.note, n.velocity });
            events.push_back({ static_cast<int>((n.start + n.length) * kRate), false, n.note, 0.0f });
        }
        std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
            return a.sample != b.sample ? a.sample < b.sample : (!a.on && b.on); // offs first at the same time
        });

        std::vector<float> l(static_cast<size_t>(total)), r(static_cast<size_t>(total));
        int pos = 0;
        size_t ev = 0;
        while (pos < total)
        {
            while (ev < events.size() && events[ev].sample <= pos)
            {
                const Event& x = events[ev++];
                if (x.on) e.noteOn(x.note, x.vel);
                else e.noteOff(x.note);
            }
            int n = std::min(128, total - pos);
            if (ev < events.size()) n = std::min(n, std::max(1, events[ev].sample - pos));
            e.render(l.data() + pos, r.data() + pos, n);
            pos += n;
        }

        double peak = 0.0, sum = 0.0, tail = 0.0;
        bool finite = true;
        const size_t tailFrom = l.size() - static_cast<size_t>(0.25 * kRate);
        for (size_t s = 0; s < l.size(); ++s)
        {
            if (!std::isfinite(l[s]) || !std::isfinite(r[s])) finite = false;
            const double a = std::max(std::fabs(l[s]), std::fabs(r[s]));
            peak = std::max(peak, a);
            sum += 0.5 * (l[s] * l[s] + r[s] * r[s]);
            if (s >= tailFrom) tail = std::max(tail, a);
        }
        const double rms = std::sqrt(sum / static_cast<double>(l.size()));
        std::printf("%-3d %-16s %-9s %8.1f %8.1f %8.1f%s\n", i, info.name, info.category, dbfs(peak), dbfs(rms), dbfs(tail),
                    finite ? "" : "  NON-FINITE");

        char file[256];
        std::snprintf(file, sizeof(file), "%s/%02d %s.wav", dir.c_str(), i + 1, info.name);
        if (!writeWav(file, l, r)) std::printf("    could not write %s\n", file);
    }
    return 0;
}
