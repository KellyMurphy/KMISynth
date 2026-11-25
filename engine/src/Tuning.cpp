#include "kmi/Tuning.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace kmi {

namespace {
struct ScaleDef
{
    const char* name;
    std::array<int, 12> steps; // semitones above the root, -1 terminated
};
// Standard music-theory scales (append only: the index is stored in presets).
constexpr ScaleDef kScales[] = {
    { "Chromatic", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    { "Major (Ionian)", { 0, 2, 4, 5, 7, 9, 11, -1 } },
    { "Natural Minor (Aeolian)", { 0, 2, 3, 5, 7, 8, 10, -1 } },
    { "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11, -1 } },
    { "Melodic Minor", { 0, 2, 3, 5, 7, 9, 11, -1 } },
    { "Dorian", { 0, 2, 3, 5, 7, 9, 10, -1 } },
    { "Phrygian", { 0, 1, 3, 5, 7, 8, 10, -1 } },
    { "Lydian", { 0, 2, 4, 6, 7, 9, 11, -1 } },
    { "Mixolydian", { 0, 2, 4, 5, 7, 9, 10, -1 } },
    { "Locrian", { 0, 1, 3, 5, 6, 8, 10, -1 } },
    { "Major Pentatonic", { 0, 2, 4, 7, 9, -1 } },
    { "Minor Pentatonic", { 0, 3, 5, 7, 10, -1 } },
    { "Blues", { 0, 3, 5, 6, 7, 10, -1 } },
    { "Major Blues", { 0, 2, 3, 4, 7, 9, -1 } },
    { "Whole Tone", { 0, 2, 4, 6, 8, 10, -1 } },
    { "Diminished (Whole-Half)", { 0, 2, 3, 5, 6, 8, 9, 11, -1 } },
    { "Diminished (Half-Whole)", { 0, 1, 3, 4, 6, 7, 9, 10, -1 } },
    { "Augmented", { 0, 3, 4, 7, 8, 11, -1 } },
    { "Harmonic Major", { 0, 2, 4, 5, 7, 8, 11, -1 } },
    { "Double Harmonic", { 0, 1, 4, 5, 7, 8, 11, -1 } },
    { "Hungarian Minor", { 0, 2, 3, 6, 7, 8, 11, -1 } },
    { "Hungarian Major", { 0, 3, 4, 6, 7, 9, 10, -1 } },
    { "Neapolitan Minor", { 0, 1, 3, 5, 7, 8, 11, -1 } },
    { "Neapolitan Major", { 0, 1, 3, 5, 7, 9, 11, -1 } },
    { "Phrygian Dominant", { 0, 1, 4, 5, 7, 8, 10, -1 } },
    { "Lydian Dominant", { 0, 2, 4, 6, 7, 9, 10, -1 } },
    { "Lydian Augmented", { 0, 2, 4, 6, 8, 9, 11, -1 } },
    { "Altered (Super Locrian)", { 0, 1, 3, 4, 6, 8, 10, -1 } },
    { "Dorian #4", { 0, 2, 3, 6, 7, 9, 10, -1 } },
    { "Persian", { 0, 1, 4, 5, 6, 8, 11, -1 } },
    { "Enigmatic", { 0, 1, 4, 6, 8, 10, 11, -1 } },
    { "Hirajoshi", { 0, 2, 3, 7, 8, -1 } },
    { "In Sen", { 0, 1, 5, 7, 10, -1 } },
    { "Iwato", { 0, 1, 5, 6, 10, -1 } },
    { "Kumoi", { 0, 2, 3, 7, 9, -1 } },
    { "Pelog", { 0, 1, 3, 7, 8, -1 } },
    { "Suspended Pentatonic", { 0, 2, 5, 7, 10, -1 } },
    { "Bebop Dominant", { 0, 2, 4, 5, 7, 9, 10, 11, -1 } },
    { "Bebop Major", { 0, 2, 4, 5, 7, 8, 9, 11, -1 } },
    { "Prometheus", { 0, 2, 4, 6, 9, 10, -1 } },
    { "Tritone", { 0, 1, 4, 6, 7, 10, -1 } },
    { "Major Triad", { 0, 4, 7, -1 } },
    { "Minor Triad", { 0, 3, 7, -1 } },
    { "Fifths", { 0, 7, -1 } },
    { "Custom", { -1 } },
};
constexpr int kNumScales = static_cast<int>(sizeof(kScales) / sizeof(kScales[0]));

struct TuningDef
{
    const char* name;
    std::array<double, 12> cents;
};
// Historical 12-note tunings, in cents above the root (append only).
constexpr TuningDef kTunings[] = {
    { "Equal", { 0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100 } },
    { "Just (5-limit)", { 0, 111.731, 203.910, 315.641, 386.314, 498.045, 590.224, 701.955, 813.686, 884.359, 1017.596, 1088.269 } },
    { "Pythagorean", { 0, 90.225, 203.910, 294.135, 407.820, 498.045, 611.730, 701.955, 792.180, 905.865, 996.090, 1109.775 } },
    { "Quarter-Comma Meantone", { 0, 76.049, 193.157, 310.265, 386.314, 503.422, 579.471, 696.579, 772.627, 889.735, 1006.843, 1082.892 } },
    { "Werckmeister III", { 0, 90.225, 192.180, 294.135, 390.225, 498.045, 588.270, 696.090, 792.180, 888.270, 996.090, 1092.180 } },
    { "Kirnberger III", { 0, 90.225, 193.157, 294.135, 386.314, 498.045, 590.224, 696.579, 792.180, 889.735, 996.090, 1088.269 } },
    { "Vallotti", { 0, 94.135, 196.090, 298.045, 392.180, 501.955, 592.180, 698.045, 796.090, 894.135, 1000.000, 1090.225 } },
    { "Scala File", { 0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100 } },
};
constexpr int kNumTunings = static_cast<int>(sizeof(kTunings) / sizeof(kTunings[0]));

int floorDiv(int a, int b) noexcept { return a >= 0 ? a / b : -((-a + b - 1) / b); }
int floorMod(int a, int b) noexcept { return a - floorDiv(a, b) * b; }

std::string trim(const std::string& s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Non-comment lines of a Scala-format file ('!' starts a comment line).
std::vector<std::string> scalaLines(const std::string& text)
{
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line[0] == '!') continue;
        out.push_back(line);
    }
    return out;
}

// One .scl pitch: cents if it contains a '.', otherwise a ratio "a/b" or an integer "a".
bool parsePitch(const std::string& raw, double& cents)
{
    std::istringstream tok(trim(raw));
    std::string word;
    tok >> word;
    if (word.empty()) return false;
    try
    {
        if (word.find('.') != std::string::npos)
        {
            cents = std::stod(word);
            return true;
        }
        const auto slash = word.find('/');
        const double num = std::stod(word.substr(0, slash));
        const double den = slash == std::string::npos ? 1.0 : std::stod(word.substr(slash + 1));
        if (num <= 0.0 || den <= 0.0) return false;
        cents = 1200.0 * std::log2(num / den);
        return true;
    }
    catch (...)
    {
        return false;
    }
}
} // namespace

int numScales() noexcept { return kNumScales; }
const char* scaleName(int index) noexcept { return kScales[std::clamp(index, 0, kNumScales - 1)].name; }
int customScaleIndex() noexcept { return kNumScales - 1; }

int scaleMask(int index) noexcept
{
    int mask = 0;
    for (int s : kScales[std::clamp(index, 0, kNumScales - 1)].steps)
    {
        if (s < 0) break;
        mask |= 1 << s;
    }
    return mask;
}

int numTunings() noexcept { return kNumTunings; }
const char* tuningName(int index) noexcept { return kTunings[std::clamp(index, 0, kNumTunings - 1)].name; }
int scalaTuningIndex() noexcept { return kNumTunings - 1; }
const std::array<double, 12>& tuningCents(int index) noexcept { return kTunings[std::clamp(index, 0, kNumTunings - 1)].cents; }

TuningTable equalTuning() noexcept
{
    TuningTable t{};
    for (int n = 0; n < 128; ++n) t[static_cast<size_t>(n)] = static_cast<float>(n);
    return t;
}

TuningTable builtInTuning(int index, int root) noexcept
{
    const auto& c = tuningCents(index);
    TuningTable t{};
    for (int n = 0; n < 128; ++n)
    {
        const int pc = floorMod(n - root, 12);
        t[static_cast<size_t>(n)] = static_cast<float>(n + (c[static_cast<size_t>(pc)] - 100.0 * pc) / 100.0);
    }
    return t;
}

bool parseScala(const std::string& scl, const std::string& kbm, TuningTable& table, std::string& name, std::string& error)
{
    const auto lines = scalaLines(scl);
    if (lines.size() < 2)
    {
        error = "Not a Scala .scl file (too short).";
        return false;
    }
    name = trim(lines[0]);
    int count = 0;
    try
    {
        count = std::stoi(trim(lines[1]));
    }
    catch (...)
    {
        error = "Not a Scala .scl file (no note count).";
        return false;
    }
    if (count < 1 || count > 1024 || static_cast<int>(lines.size()) < 2 + count)
    {
        error = "The .scl note count does not match its pitches.";
        return false;
    }
    std::vector<double> cents(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        if (!parsePitch(lines[static_cast<size_t>(2 + i)], cents[static_cast<size_t>(i)]))
        {
            error = "Could not read pitch " + std::to_string(i + 1) + " of the .scl file.";
            return false;
        }
    const double period = cents.back();
    if (period <= 0.0)
    {
        error = "The .scl period (its last pitch) must be above the root.";
        return false;
    }
    auto degreeCents = [&](int degree) {
        const int oct = floorDiv(degree, count), idx = floorMod(degree, count);
        return oct * period + (idx == 0 ? 0.0 : cents[static_cast<size_t>(idx - 1)]);
    };

    // Keyboard mapping (defaults: linear, degree 0 on middle C at its equal-tempered pitch).
    int mapSize = 0, first = 0, last = 127, middle = 60, reference = 60, octaveDegree = count;
    double refFreq = 440.0 * std::pow(2.0, (60.0 - 69.0) / 12.0);
    std::vector<int> mapping; // -1 = unmapped key
    if (!trim(kbm).empty())
    {
        const auto k = scalaLines(kbm);
        std::vector<std::string> vals;
        for (const auto& l : k)
            if (!trim(l).empty()) vals.push_back(trim(l));
        try
        {
            if (vals.size() < 7) throw 0;
            mapSize = std::stoi(vals[0]);
            first = std::stoi(vals[1]);
            last = std::stoi(vals[2]);
            middle = std::stoi(vals[3]);
            reference = std::stoi(vals[4]);
            refFreq = std::stod(vals[5]);
            octaveDegree = std::stoi(vals[6]);
            if (mapSize < 0 || static_cast<int>(vals.size()) < 7 + mapSize || refFreq <= 0.0) throw 0;
            for (int i = 0; i < mapSize; ++i)
            {
                const std::string v = vals[static_cast<size_t>(7 + i)];
                mapping.push_back(v[0] == 'x' || v[0] == 'X' ? -1 : std::stoi(v));
            }
        }
        catch (...)
        {
            error = "Could not read the .kbm keyboard mapping.";
            return false;
        }
    }

    // The scale degree a key plays (or nothing), linear without a mapping.
    auto keyDegree = [&](int key, bool& mapped) {
        mapped = true;
        if (mapSize == 0) return key - middle;
        const int off = key - middle;
        const int slot = floorMod(off, mapSize);
        const int deg = mapping[static_cast<size_t>(slot)];
        if (deg < 0)
        {
            mapped = false;
            return 0;
        }
        return deg + floorDiv(off, mapSize) * octaveDegree;
    };

    // Reference: the reference key sounds at the reference frequency.
    bool refMapped = true;
    const int refDegree = keyDegree(reference, refMapped);
    const double refPitch = 69.0 + 12.0 * std::log2(refFreq / 440.0);
    const double offset = refPitch - degreeCents(refDegree) / 100.0;
    for (int n = 0; n < 128; ++n)
    {
        bool mapped = true;
        const int deg = keyDegree(n, mapped);
        const bool inRange = mapSize == 0 || (n >= first && n <= last);
        const double pitch = offset + degreeCents(deg) / 100.0;
        table[static_cast<size_t>(n)] = mapped && inRange && pitch > -24.0 && pitch < 160.0 ? static_cast<float>(pitch) : -1.0f;
    }
    error.clear();
    return true;
}

int absoluteMask(int scaleIndex, int root, int customMask) noexcept
{
    const int rel = scaleIndex == customScaleIndex() ? (customMask & 0xFFF) : scaleMask(scaleIndex);
    int abs = 0;
    for (int k = 0; k < 12; ++k)
        if (rel & (1 << k)) abs |= 1 << floorMod(k + root, 12);
    return abs == 0 ? 0xFFF : abs; // an empty custom scale behaves as chromatic
}

int snapToScale(int note, int mask) noexcept
{
    if ((mask & 0xFFF) == 0xFFF) return note;
    for (int d = 0; d <= 6; ++d)
    {
        if (note - d >= 0 && (mask & (1 << floorMod(note - d, 12)))) return note - d;
        if (note + d <= 127 && (mask & (1 << floorMod(note + d, 12)))) return note + d;
    }
    return note;
}

int whiteKeyToScale(int note, int scaleIndex, int root, int customMask) noexcept
{
    static constexpr int kWhiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    const int white = kWhiteIndex[floorMod(note, 12)];
    if (white < 0) return -1;
    const int rel = scaleIndex == customScaleIndex() ? (customMask & 0xFFF) : scaleMask(scaleIndex);
    std::array<int, 12> steps{};
    int k = 0;
    for (int b = 0; b < 12; ++b)
        if (rel & (1 << b)) steps[static_cast<size_t>(k++)] = b;
    if (k == 0) return note;
    // Middle C plays the root in octave 5 (C3 = 60); each white key above it is the next degree.
    const int degree = (floorDiv(note, 12) - 5) * 7 + white;
    const int out = 60 + root + 12 * floorDiv(degree, k) + steps[static_cast<size_t>(floorMod(degree, k))];
    return out >= 0 && out <= 127 ? out : -1;
}

} // namespace kmi
