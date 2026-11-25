// Scales and microtuning.
//
// Scales: 44 named scales plus a Custom 12-note scale, each on a root key. Key Lock can snap played keys to the
// scale (Nearest) or play the scale's degrees from the white keys (White Keys, C = the root). Glissando steps
// through the scale.
//
// Tunings: per-key pitches. Built in: Equal and seven historical 12-note temperaments (relative to the root);
// or a Scala .scl file with an optional .kbm keyboard mapping. A key's pitch is a fractional MIDI note number
// (60.5 = a quarter tone above middle C); keys a mapping leaves out are silent (pitch < 0).
#pragma once

#include <array>
#include <string>

namespace kmi {

using TuningTable = std::array<float, 128>;

int numScales() noexcept;                 // including Custom (the last one)
const char* scaleName(int index) noexcept;
int customScaleIndex() noexcept;
// 12-bit mask of the scale's pitch classes relative to the root (bit 0 = the root).
int scaleMask(int index) noexcept;

int numTunings() noexcept;                // including "Scala File" (the last one)
const char* tuningName(int index) noexcept;
int scalaTuningIndex() noexcept;
// Cents (0..1200) of each of the 12 notes above the root for a built-in tuning.
const std::array<double, 12>& tuningCents(int index) noexcept;

// Equal temperament: table[n] = n.
TuningTable equalTuning() noexcept;
// A built-in 12-note tuning on `root` (0 = C): the root keeps its equal-tempered pitch.
TuningTable builtInTuning(int index, int root) noexcept;

// Parses a Scala scale (.scl text) and an optional keyboard mapping (.kbm text, may be empty). Without a mapping,
// degree 0 sits on middle C (60) at its equal-tempered pitch. Returns false with `error` set on bad input.
bool parseScala(const std::string& scl, const std::string& kbm, TuningTable& table, std::string& name, std::string& error);

// Key Lock helpers. `mask` is absolute (bit k = pitch class k is in the scale).
int absoluteMask(int scaleIndex, int root, int customMask) noexcept;
int snapToScale(int note, int mask) noexcept;                 // nearest scale note (ties go down)
int whiteKeyToScale(int note, int scaleIndex, int root, int customMask) noexcept; // -1 for black keys

} // namespace kmi
