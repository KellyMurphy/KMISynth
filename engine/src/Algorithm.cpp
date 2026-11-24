#include "kmi/Algorithm.h"

#include <cctype>
#include <vector>

namespace kmi {

namespace {
struct AlgoDef
{
    const char* name;
    const char* edges;    // "m>c" pairs, 1-based: oscillator m modulates oscillator c
    const char* carriers; // oscillators routed to the output
};

// KMISynth's own preset algorithm library (Phase 1 starter set; target 64, then 140+).
// Append only: algorithm numbers are stored in presets.
const AlgoDef kDefs[] = {
    { "Eight Carriers", "", "1 2 3 4 5 6 7 8" },
    { "One Pair", "2>1", "1 3 4 5 6 7 8" },
    { "Four Pairs", "2>1 4>3 6>5 8>7", "1 3 5 7" },
    { "Two Towers", "4>3 3>2 2>1 8>7 7>6 6>5", "1 5" },
    { "Tall Tower", "8>7 7>6 6>5 5>4 4>3 3>2 2>1", "1" },
    { "Six Stack Plus Two", "6>5 5>4 4>3 3>2 2>1", "1 7 8" },
    { "Three Voices", "3>2 2>1 6>5 5>4 8>7", "1 4 7" },
    { "Many Into One", "2>1 3>1 4>1 5>1 6>1 7>1 8>1", "1" },
    { "Fan Out", "8>1 8>2 8>3 8>4 8>5 8>6 8>7", "1 2 3 4 5 6 7" },
    { "Twin Forks", "3>2 4>2 2>1 7>6 8>6 6>5", "1 5" },
    { "Keys", "2>1 5>4 4>3 6>3 8>7", "1 3 7" },
    { "Bell Tree", "3>1 3>2 6>4 6>5 8>7 7>1", "1 2 4 5" },
    { "Horns", "3>2 2>1 4>1 5>1 8>7 7>6", "1 6" },
    { "Double Diamond", "4>2 4>3 2>1 3>1 8>6 8>7 6>5 7>5", "1 5" },
    { "Comb", "2>1 3>1 4>3 5>1 6>5 7>1 8>7", "1" },
    { "Cross Pairs", "2>1 3>2 4>1 6>5 7>6 8>5", "1 5" },
    { "Sub Layers", "5>1 6>2 7>3 8>4", "1 2 3 4" },
    { "Twin Stacks Plus Two", "3>2 2>1 6>5 5>4", "1 4 7 8" },
    { "Branching Cascade", "8>6 7>6 6>4 5>4 4>2 3>2 2>1", "1" },
    { "Shared Bus", "7>1 7>2 7>3 8>4 8>5 8>6", "1 2 3 4 5 6" },
};
constexpr int kNumDefs = static_cast<int>(sizeof(kDefs) / sizeof(kDefs[0]));

struct Library
{
    std::vector<Algorithm> algos;
    Library()
    {
        algos.resize(kNumDefs);
        for (int i = 0; i < kNumDefs; ++i)
            parseAlgorithm(kDefs[i].edges, kDefs[i].carriers, algos[static_cast<size_t>(i)]);
    }
};

const Library& library()
{
    static const Library lib;
    return lib;
}

bool readIndex(const char*& p, int& out) noexcept
{
    if (!std::isdigit(static_cast<unsigned char>(*p))) return false;
    out = *p - '1';
    ++p;
    return out >= 0 && out < kNumOscs;
}
} // namespace

int numPresetAlgorithms() noexcept { return kNumDefs; }

const Algorithm& presetAlgorithm(int index) noexcept
{
    const auto& lib = library();
    if (index < 0 || index >= kNumDefs) index = 0;
    return lib.algos[static_cast<size_t>(index)];
}

const char* presetAlgorithmName(int index) noexcept
{
    return kDefs[index < 0 || index >= kNumDefs ? 0 : index].name;
}

void lerpAlgorithm(const Algorithm& a, const Algorithm& b, float t, Algorithm& out) noexcept
{
    for (int m = 0; m < kNumOscs; ++m)
    {
        for (int c = 0; c < kNumOscs; ++c) out.conn[m][c] = a.conn[m][c] + (b.conn[m][c] - a.conn[m][c]) * t;
        out.out[m] = a.out[m] + (b.out[m] - a.out[m]) * t;
    }
}

bool parseAlgorithm(const char* edges, const char* carriers, Algorithm& out) noexcept
{
    out = Algorithm{};
    for (const char* p = edges; *p;)
    {
        if (*p == ' ') { ++p; continue; }
        int m = 0, c = 0;
        if (!readIndex(p, m) || *p != '>') return false;
        ++p;
        if (!readIndex(p, c) || m == c) return false;
        out.conn[m][c] = 1.0f;
    }
    for (const char* p = carriers; *p;)
    {
        if (*p == ' ') { ++p; continue; }
        int i = 0;
        if (!readIndex(p, i)) return false;
        out.out[i] = 1.0f;
    }
    out.out[0] = 1.0f; // Osc 1 always reaches the output
    return true;
}

} // namespace kmi
