// Arpeggiator: turns the held keys into a tempo-locked stream of notes. Sample accurate: the engine
// asks how many samples remain until the next event, renders exactly that many, then advances.
// Follows the host transport grid when it is playing, otherwise runs free from the first key.
//
// Each step picks its notes by mode (Up, Down, ..., Chord, Phrase), walks the octave range by octave mode,
// may be skipped (Chance) or repeated inside the step (Ratchet), and ends after Gate x the step length.
// Gates over 100% overlap the next step; a note that is played again is released first.
#pragma once

#include <array>
#include <cstdint>

#include "kmi/Patch.h"

namespace kmi {

constexpr int kNumArpPhrases = 64;
const char* arpPhraseName(int index) noexcept;

class Arpeggiator
{
public:
    struct Sink
    {
        virtual ~Sink() = default;
        virtual void arpNoteOn(int note, float velocity) noexcept = 0;
        virtual void arpNoteOff(int note) noexcept = 0;
    };

    static constexpr int kMaxKeys = 32;
    static constexpr int kMaxChord = kMaxKeys;
    static constexpr int kMaxOctaves = 6;

    // Forget all keys and sounding notes without sending note-offs (the caller silences voices).
    void reset() noexcept;
    // Send note-offs for anything sounding and stop stepping; keys are kept.
    void stop(Sink& sink) noexcept;

    void keyDown(int note, float velocity) noexcept;
    void keyUp(int note) noexcept;
    void setPedal(bool down) noexcept;

    // Call once per host block before process(). transportPpq < 0 = free-running.
    void beginBlock(const ArpParams& p, double bpm, double sampleRate, double transportPpq) noexcept;
    // Fires every event that is due now, then returns how many samples (1..maxSamples) can be rendered
    // before the next one.
    int process(Sink& sink, int maxSamples) noexcept;
    void advance(int samples) noexcept { clock_ += samples * ppqPerSample_; }

    bool isRunning() const noexcept { return running_; }
    int physicalKeyCount() const noexcept { return physCount_; }
    int physicalKey(int i) const noexcept { return phys_[static_cast<size_t>(i)]; }
    float physicalVelocity(int i) const noexcept { return physVel_[static_cast<size_t>(i)]; }
    int keyCount() const noexcept { return keyCount_; }

private:
    struct Key { int note; float velocity; };
    struct Sounding { int note; double off; };

    void syncKeysToPhysical() noexcept;
    void addKey(int note, float velocity) noexcept;
    void keysChanged() noexcept { ++keysVersion_; }
    void fireStep(Sink& sink) noexcept;
    void hit(Sink& sink, double start, double slot, double gate) noexcept;
    void noteOff(Sink& sink, int index) noexcept; // index into sounding_
    void releaseAll(Sink& sink) noexcept;
    void chooseNotes(int64_t step) noexcept;      // fills stepNotes_ / stepTie_
    int buildPattern() noexcept;                  // fills pattern_, returns its length
    int octaveFor(int64_t k) noexcept;            // octave index for the k-th pass / step
    int ratchetCount() noexcept;
    double stepLength(int64_t index) const noexcept;
    // First grid step at or after (strict: after) ppq; returns its start and index.
    double gridStep(double ppq, bool strict, int64_t& index) const noexcept;
    float randomUnit() noexcept;

    ArpParams params_{};
    double ppqPerSample_ = 120.0 / (60.0 * 48000.0);
    double clock_ = 0.0;        // quarter notes
    bool synced_ = false;
    bool pedal_ = false;
    bool running_ = false;

    std::array<int, kMaxKeys> phys_{};       // physically held, press order
    std::array<float, kMaxKeys> physVel_{};
    int physCount_ = 0;
    std::array<Key, kMaxKeys> keys_{};       // what the arp plays (press order); may include latched keys
    int keyCount_ = 0;
    bool latchedChord_ = false;              // all physical keys were released while latched
    uint32_t keysVersion_ = 0;

    double nextStep_ = 0.0, stepStart_ = 0.0;
    int64_t stepIndex_ = 0;
    int64_t position_ = 0;                   // pattern position
    std::array<Sounding, kMaxChord> sounding_{};
    int soundingCount_ = 0;
    int soundingLast_ = -1;

    // The current step's notes (re-struck by ratchets).
    std::array<Key, kMaxChord> stepNotes_{};
    int stepNoteCount_ = 0;
    bool stepTie_ = false;
    int ratchetLeft_ = 0;
    double ratchetSlot_ = 0.0, nextRatchet_ = -1.0, ratchetGate_ = 0.5;

    std::array<Key, kMaxKeys * kMaxOctaves * 2> pattern_{};
    bool randomOctave_ = false;
    uint32_t rng_ = 0x2545F491u;
};

} // namespace kmi
