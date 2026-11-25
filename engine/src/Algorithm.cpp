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

// KMISynth's own preset algorithm library (148).
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
    // 21-30: partial stacks with free carriers
    { "Organ Plus Mod", "8>1 8>2 8>3", "1 2 3 4 5 6 7" },
    { "Stack Three, Five Free", "3>2 2>1", "1 4 5 6 7 8" },
    { "Stack Four, Four Free", "4>3 3>2 2>1", "1 5 6 7 8" },
    { "Stack Five, Three Free", "5>4 4>3 3>2 2>1", "1 6 7 8" },
    { "Seven Stack", "7>6 6>5 5>4 4>3 3>2 2>1", "1 8" },
    { "Two Into One Twice", "2>1 3>1 5>4 6>4", "1 4 7 8" },
    { "Three Into One Twice", "2>1 3>1 4>1 6>5 7>5 8>5", "1 5" },
    { "Pair Chains", "2>1 4>3 3>1 6>5 8>7 7>5", "1 5" },
    { "Alternating Pairs", "2>1 4>3 6>5 8>7 2>3 4>5 6>7", "1 3 5 7" },
    { "Split Buses", "8>6 8>7 6>1 6>2 6>3 7>4 7>5", "1 2 3 4 5" },
    // 31-40: trees and fans
    { "Deep Pair", "3>2 2>1 4>2 5>1", "1 6 7 8" },
    { "Bright Bell", "2>1 3>1 4>3 6>5 7>5 8>5", "1 5" },
    { "Wide Fan", "5>1 5>2 5>3 5>4 8>7 7>6", "1 2 3 4 6" },
    { "Mirror", "4>3 3>1 2>1 8>6 7>6 6>5", "1 5" },
    { "Binary Tree", "2>1 3>1 4>2 5>2 6>3 7>3", "1 8" },
    { "Wide Tree", "2>1 3>1 4>1 5>2 6>3 7>4", "1 8" },
    { "Long Arm", "2>1 3>1 4>1 8>7 7>6 6>5 5>1", "1" },
    { "Split Choir", "2>1 2>3 2>5 2>7 4>3 6>5 8>7", "1 3 5 7" },
    { "Shared Top", "8>1 8>3 8>5 8>7 2>1 4>3 6>5", "1 3 5 7" },
    { "Crossed Threes", "3>2 2>1 6>5 5>4 3>5 8>7", "1 4 7" },
    // 41-50: sparse, bells and rings
    { "Two Pairs, Four Free", "2>1 4>3", "1 3 5 6 7 8" },
    { "Pyramid", "5>2 5>3 6>3 6>4 2>1 3>1 4>1", "1 7 8" },
    { "Twin Bells", "3>1 3>2 4>2 7>5 7>6 8>6", "1 2 5 6" },
    { "Zigzag", "2>1 3>2 3>4 5>4 5>6 7>6 7>8", "1 4 6 8" },
    { "Ring of Four", "2>1 3>2 4>3 1>4", "1 5 6 7 8" },
    { "Ring of Eight", "2>1 3>2 4>3 5>4 6>5 7>6 8>7 1>8", "1" },
    { "Mutual Pairs", "2>1 1>2 4>3 3>4", "1 3 5 6 7 8" },
    { "Bass and Lead", "3>2 2>1 4>1 8>7 7>6 6>5", "1 5" },
    { "Pad Layers", "5>1 6>2 7>3 8>4 5>2 6>3 7>4", "1 2 3 4" },
    { "Master Modulator", "7>1 7>2 7>3 7>4 7>5 7>6 8>7", "1 2 3 4 5 6" },
    // 51-64: dense networks
    { "Six Plus Tower", "8>7 7>1", "1 2 3 4 5 6" },
    { "Two Fans", "4>1 4>2 4>3 8>5 8>6 8>7", "1 2 3 5 6 7" },
    { "Dense Four", "2>1 3>1 3>2 4>2 4>3", "1 5 6 7 8" },
    { "Full Cascade",
      "2>1 3>1 4>1 5>1 6>1 7>1 8>1 3>2 4>2 5>2 6>2 7>2 8>2 4>3 5>3 6>3 7>3 8>3 5>4 6>4 7>4 8>4 6>5 7>5 8>5 7>6 8>6 8>7",
      "1" },
    { "Dense Fours Twice", "2>1 3>1 4>1 3>2 4>2 4>3 6>5 7>5 8>5 7>6 8>6 8>7", "1 5" },
    { "Modulated Modulators", "3>2 4>2 2>1 5>1 6>5 7>5 8>5", "1" },
    { "Choir of Six", "7>1 7>3 7>5 8>2 8>4 8>6", "1 2 3 4 5 6" },
    { "Leaning Tower", "8>7 7>6 6>5 5>4 4>3 3>2 2>1 8>1", "1" },
    { "Mixed Pairs", "2>1 4>3 6>5 8>7 8>5 4>1", "1 3 5 7" },
    { "Sideband Trio", "4>1 4>2 4>3", "1 2 3 5 6 7 8" },
    { "Formant Banks", "2>1 3>1 5>4 6>4 8>7", "1 4 7" },
    { "Twin Cascades Crossed", "4>3 3>2 2>1 8>7 7>6 6>5 4>5 8>1", "1 5" },
    { "Reverse Tower", "1>2 2>3 3>4 4>5 5>6 6>7 7>8", "1 8" },
    { "Four Carrier Web", "5>1 5>2 6>2 6>3 7>3 7>4 8>4 8>1", "1 2 3 4" },
    // 65-148: rings and loops, towers with shortcuts, sideband matrices, trees, light FM, dense, instrument shapes.
    // Each is checked to be a different shape (not a relabelling) from every earlier one.
    { "Ring of Three", "2>1 3>2 1>3", "1 4 5 6 7 8" },
    { "Ring of Five", "2>1 3>2 4>3 5>4 1>5", "1 6 7 8" },
    { "Ring of Six", "2>1 3>2 4>3 5>4 6>5 1>6", "1 7 8" },
    { "Two Rings of Four", "2>1 3>2 4>3 1>4 6>5 7>6 8>7 5>8", "1 5" },
    { "Ring Feeds Carrier", "3>2 4>3 2>4 2>1", "1 5 6 7 8" },
    { "Mutual Pair Into One", "2>3 3>2 2>1 3>1", "1 4 5 6 7 8" },
    { "Two Mutual Pairs", "2>3 3>2 3>1 5>6 6>5 6>4", "1 4 7 8" },
    { "Triangle Loops", "1>2 2>3 3>1 4>5 5>6 6>4", "1 4 7 8" },
    { "Ring Over Stack", "6>7 7>8 8>6 6>5 5>4 4>3 3>2 2>1", "1" },
    { "Cross-Modulated Pairs", "1>2 2>1 3>4 4>3 5>6 6>5 7>8 8>7", "1 2 3 4 5 6 7 8" },
    { "Feedback Fan", "8>1 8>2 8>3 8>4 1>8", "1 2 3 4 5 6 7" },
    { "Ring Into Fan", "6>7 7>8 8>6 8>1 8>2 8>3 8>4 8>5", "1 2 3 4 5" },
    { "Quad Ring", "2>1 4>3 6>5 8>7 1>3 3>5 5>7 7>1", "1 3 5 7" },
    { "Carrier Cross", "2>1 4>3 1>3 3>1", "1 3 5 6 7 8" },
    { "Tower Top Loop", "8>7 7>8 7>6 6>5 5>4 4>3 3>2 2>1", "1" },
    { "Two-Way Chain", "2>1 1>2 3>2 2>3 4>3 3>4", "1 5 6 7 8" },
    { "Star Loop", "8>1 1>8 8>2 2>8 8>3 3>8", "1 2 3 4 5 6 7" },
    { "Skip Tower", "8>7 7>6 6>5 5>4 4>3 3>2 2>1 8>6 6>4 4>2", "1" },
    { "Odd and Even Chains", "3>1 5>3 7>5 2>1 4>2 6>4 8>6", "1" },
    { "Leaning Pairs", "2>1 4>3 6>5 8>7 3>1 5>3 7>5", "1" },
    { "Ladder", "2>1 4>3 6>5 8>7 4>2 6>4 8>6", "1 3 5 7" },
    { "Double Ladder", "2>1 4>3 6>5 8>7 3>1 5>3 7>5 4>2 6>4 8>6", "1" },
    { "Twin Leaning Towers", "4>3 3>2 2>1 4>1 8>7 7>6 6>5 8>5", "1 5" },
    { "Shortcut Tower", "8>7 7>6 6>5 5>4 4>3 3>2 2>1 5>1", "1" },
    { "Express Tower", "8>7 7>6 6>5 5>4 4>3 3>2 2>1 8>1 7>1 6>1", "1" },
    { "Cascade Steps", "3>2 2>1 3>1 6>5 5>4 6>4 8>7", "1 4 7" },
    { "Stack Six, Pair", "6>5 5>4 4>3 3>2 2>1 8>7", "1 7" },
    { "Stack Five, Three", "5>4 4>3 3>2 2>1 8>7 7>6", "1 6" },
    { "Four, Three, One", "4>3 3>2 2>1 7>6 6>5", "1 5 8" },
    { "Tower, Every Other Heard", "8>7 7>6 6>5 5>4 4>3 3>2 2>1", "1 3 5 7" },
    { "Tower, Middle Heard", "8>7 7>6 6>5 5>4 4>3 3>2 2>1", "1 5" },
    { "Tower, All Heard", "8>7 7>6 6>5 5>4 4>3 3>2 2>1", "1 2 3 4 5 6 7 8" },
    { "Twin Towers Crossed Top", "4>3 3>2 2>1 8>7 7>6 6>5 4>7 8>3", "1 5" },
    { "Two Into Three", "7>1 7>2 7>3 8>1 8>2 8>3", "1 2 3 4 5 6" },
    { "Three Into Two", "3>1 4>1 5>1 3>2 4>2 5>2", "1 2 6 7 8" },
    { "Four By Four", "5>1 6>1 7>1 8>1 5>2 6>2 7>2 8>2 5>3 6>3 7>3 8>3 5>4 6>4 7>4 8>4", "1 2 3 4" },
    { "Braid", "3>1 3>2 4>1 4>2 5>3 5>4 6>3 6>4 7>5 8>6", "1 2" },
    { "Lattice", "3>1 4>1 3>2 4>2 5>3 6>3 5>4 6>4 7>5 8>5 7>6 8>6", "1 2" },
    { "Diamond Stack", "2>1 3>1 4>2 4>3 5>4 6>5 7>5 8>6", "1" },
    { "Hourglass", "5>4 6>4 7>4 4>2 4>3 2>1 3>1", "1 8" },
    { "Wide Diamond", "5>2 5>3 5>4 2>1 3>1 4>1 8>7 7>6 6>5", "1" },
    { "Two Webs", "3>1 4>1 3>2 4>2 7>5 8>5 7>6 8>6", "1 2 5 6" },
    { "Full Ternary", "2>1 3>1 4>1 5>2 6>3 7>4 8>2", "1" },
    { "Root Fan Stack", "3>2 2>1 4>1 5>1 6>1 7>6 8>7", "1" },
    { "Broom", "2>1 3>2 4>3 5>4 6>4 7>4 8>4", "1" },
    { "Cascade Into Pairs", "8>7 7>2 7>4 7>6 2>1 4>3 6>5", "1 3 5" },
    { "Organ With Two Mods", "7>1 7>2 8>3 8>4", "1 2 3 4 5 6" },
    { "Five Carriers, Three Mods", "6>1 6>2 6>3 7>4 8>5", "1 2 3 4 5" },
    { "Pairs With Shared Top", "2>1 4>3 6>5 8>1 8>3 8>5", "1 3 5 7" },
    { "Fan, All Heard", "8>1 8>2 8>3 8>4 8>5 8>6 8>7", "1 2 3 4 5 6 7 8" },
    { "Pairs, All Heard", "2>1 4>3 6>5 8>7", "1 2 3 4 5 6 7 8" },
    { "One Mod, Four Carriers", "2>1 2>3 2>4 2>5", "1 3 4 5 6 7 8" },
    { "Upward Pairs", "1>2 3>4 5>6 7>8", "1 2 4 6 8" },
    { "Stack Over Three", "8>7 7>1 7>2 7>3", "1 2 3 4 5 6" },
    { "Linked Fans", "4>1 4>2 4>3 8>5 8>6 8>7 4>8", "1 2 3 5 6 7" },
    { "Dense Six", "2>1 3>1 4>1 5>1 6>1 3>2 4>2 5>2 6>2 4>3 5>3 6>3 5>4 6>4 6>5", "1 7 8" },
    { "Full Mesh Four", "1>2 1>3 1>4 2>1 2>3 2>4 3>1 3>2 3>4 4>1 4>2 4>3", "1 5 6 7 8" },
    { "Dense Top", "6>5 7>5 8>5 7>6 8>6 8>7 5>4 4>3 3>2 2>1", "1" },
    { "Gong", "4>3 3>1 4>2 2>1 6>5 8>7 7>5", "1 5" },
    { "Chimes", "3>1 3>2 5>4 6>4 8>7", "1 2 4 7" },
    { "Glass", "2>1 3>1 4>1 6>5", "1 5 7 8" },
    { "Kalimba", "2>1 3>1 5>4 7>6", "1 4 6 8" },
    { "Lead Stack Plus Sub", "4>3 3>2 2>1 5>1 8>7", "1 6 7" },
    { "Bass Fork", "3>2 4>2 2>1 5>1 6>5", "1 7 8" },
    { "Sync Lead", "2>1 3>2 4>3 6>5 7>5 8>5", "1 5" },
    { "String Section", "5>1 5>2 6>3 6>4 7>5 8>6", "1 2 3 4" },
    { "Pentagon", "2>1 3>1 4>2 4>3 5>4", "1 6 7 8" },
    { "Long and Short", "8>7 7>6 6>5 5>1 3>2 2>1 4>1", "1" },
    { "Two Into Tower", "7>6 8>6 6>5 5>4 4>3 3>2 2>1", "1" },
    { "Tower Into Two", "8>7 7>6 6>5 5>4 4>3 3>1 3>2", "1 2" },
    { "Split Ends", "8>7 7>6 6>5 5>4 4>2 4>3 2>1 3>1", "1" },
    { "Chain Into Fan", "8>7 7>6 6>1 6>2 6>3 6>4 6>5", "1 2 3 4 5" },
    { "Mirrored Fans", "2>1 3>1 4>1 8>5 8>6 8>7", "1 5 6 7" },
    { "Loop Atop Pairs", "7>8 8>7 8>2 8>4 8>6 2>1 4>3 6>5", "1 3 5" },
    { "Ring of Seven", "2>1 3>2 4>3 5>4 6>5 7>6 1>7", "1 8" },
    { "Ring of Four, Fan", "2>1 3>2 4>3 1>4 4>5 4>6 4>7 4>8", "1 5 6 7 8" },
    { "Twin Triangles", "2>1 3>1 3>2 5>4 6>4 6>5", "1 4 7 8" },
    { "Bell Ladder", "2>1 4>3 6>5 4>1 6>3 7>5 8>5", "1 3 5" },
    { "Gamelan", "3>1 3>2 4>1 4>2 5>3 5>4", "1 2 6 7 8" },
    { "Electric Piano", "2>1 4>3 5>4 6>3", "1 3 7 8" },
    { "Clav", "2>1 3>2 4>2 6>5", "1 5 7 8" },
    { "Brass Stack", "2>1 3>1 4>3 5>1", "1 6 7 8" },
    { "Breathy Lead", "2>1 3>1 4>2 4>3", "1 5 6 7 8" },
    { "Bass Pair Stack", "4>3 3>2 2>1 6>5", "1 5 7 8" },
};
constexpr int kNumDefs = static_cast<int>(sizeof(kDefs) / sizeof(kDefs[0]));

struct Library
{
    std::vector<Algorithm> algos;
    bool allValid = true;
    Library()
    {
        algos.resize(kNumDefs);
        for (int i = 0; i < kNumDefs; ++i)
            allValid = parseAlgorithm(kDefs[i].edges, kDefs[i].carriers, algos[static_cast<size_t>(i)]) && allValid;
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

bool presetAlgorithmsValid() noexcept { return library().allValid; }

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
