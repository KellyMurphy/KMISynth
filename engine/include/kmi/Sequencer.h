// Step sequencer: two polyphonic note tracks and a macro track (8 lanes driving Macros 1-8), each with its own
// length, rate and play mode. Runs with the host transport (Host trigger) or while a key is held (Key trigger,
// transposed by the key relative to C3 = 60). Sample accurate in the same way as the arpeggiator: the engine
// renders up to the next event, then advances.
#pragma once

#include <array>
#include <cstdint>

#include "kmi/Patch.h"

namespace kmi {

class Sequencer
{
public:
    struct Sink
    {
        virtual ~Sink() = default;
        virtual void seqNoteOn(int note, float velocity) noexcept = 0;
        virtual void seqNoteOff(int note) noexcept = 0;
    };

    static constexpr int kMaxSounding = 64;
    static constexpr int kMaxKeys = 16;

    void reset() noexcept;            // forget everything without note-offs (the caller silences voices)
    void stop(Sink& sink) noexcept;   // note-offs for everything sounding; stop stepping

    // Key trigger: held keys start the sequence and transpose it (most recent key, relative to 60).
    void keyDown(int note, float velocity) noexcept;
    void keyUp(int note) noexcept;
    int heldKeys() const noexcept { return keyCount_; }

    // Once per host block. transportPpq < 0 = the transport is stopped.
    void beginBlock(const SeqParams& p, const SeqData& data, double bpm, double sampleRate, double transportPpq) noexcept;
    int process(Sink& sink, int maxSamples) noexcept;
    void advance(int samples) noexcept { clock_ += samples * ppqPerSample_; }

    bool isRunning() const noexcept { return running_; }
    // The step each track is on (for the editor's playhead), or -1 when not running.
    int position(int track) const noexcept { return running_ ? tracks_[static_cast<size_t>(track)].pos : -1; }
    // Macro track output: true (and the value) when the lane is driving its macro right now.
    bool laneValue(int lane, float& value) const noexcept
    {
        if (!laneActive_[static_cast<size_t>(lane)]) return false;
        value = laneValue_[static_cast<size_t>(lane)];
        return true;
    }

private:
    struct Sounding { int note; double off; };
    struct Hit { double time; int step; float velScale; double noteCap; };
    struct Track
    {
        double nextStep = 0.0;
        int64_t index = 0;   // absolute step count (the grid position when following the host)
        int pos = -1;        // step in the pattern
        std::array<Hit, 4> hits{};
        int hitCount = 0;
    };

    double stepQuarters(int track) const noexcept;
    int mapPosition(int track, int64_t k) noexcept;
    void startTracks() noexcept;
    void fireStep(int track) noexcept;
    void play(Sink& sink, int track, const Hit& h) noexcept;
    void noteOff(Sink& sink, int index) noexcept;
    float randomUnit() noexcept;

    SeqParams params_{};
    const SeqData* data_ = nullptr;
    double ppqPerSample_ = 120.0 / (60.0 * 48000.0);
    double clock_ = 0.0;
    bool hostMode_ = true, transportRunning_ = false, running_ = false;

    std::array<Track, kSeqTracks> tracks_{};
    std::array<Sounding, kMaxSounding> sounding_{};
    int soundingCount_ = 0;
    std::array<bool, kSeqLanes> laneActive_{};
    std::array<float, kSeqLanes> laneValue_{};

    std::array<int, kMaxKeys> keys_{};
    int keyCount_ = 0;
    uint32_t rng_ = 0x6C8E9CF5u;
};

} // namespace kmi
