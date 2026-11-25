// The KMISynth sound engine: 16 voices, note allocation, shared LFOs, oversampling.
// Framework-agnostic: the plugin wrapper owns one Engine, writes engine.patch() on the audio
// thread, forwards MIDI as method calls and calls render().
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "kmi/Arpeggiator.h"
#include "kmi/Decimator.h"
#include "kmi/Effects.h"
#include "kmi/Lfo.h"
#include "kmi/Patch.h"
#include "kmi/Sequencer.h"
#include "kmi/Voice.h"

namespace kmi {

class Engine final : private Arpeggiator::Sink, private Sequencer::Sink
{
public:
    static constexpr int kMaxChunk = 256;

    Engine();

    // Not real-time safe (may allocate on first call). Call from prepareToPlay.
    void prepare(double sampleRate);
    void reset() noexcept; // kill all voices, clear state

    Patch& patch() noexcept { return patch_; }
    const Patch& patch() const noexcept { return patch_; }

    void setTempo(double bpm) noexcept { bpm_ = bpm > 1.0 ? bpm : 120.0; }
    // Host transport at the start of the next render() call; ppq < 0 (or not playing) = free-running.
    void setTransport(double ppq, bool playing) noexcept { transportPpq_ = playing ? ppq : -1.0; }

    // channel: 1-16 for MPE member channels (per-note expression), 0 = not channel-specific.
    void noteOn(int note, float velocity, int channel = 0) noexcept;
    void noteOff(int note, float releaseVelocity = 0.0f, int channel = 0) noexcept;
    // MPE per-note expression, sent on a note's own channel (1-16).
    void channelPitchBend(int channel, float bipolar) noexcept;
    void channelPressure(int channel, float value) noexcept;
    void channelTimbre(int channel, float value01) noexcept;
    void polyAftertouch(int note, float value) noexcept;
    void channelAftertouch(float value) noexcept { mod_.channelAT = value; }
    void pitchBend(float bipolar) noexcept { mod_.pitchBend = bipolar; }
    void controlChange(int cc, float value01) noexcept;
    void allNotesOff() noexcept;
    void tapTrigger() noexcept; // fires every envelope that lists Tap as a trigger source
    void allSoundOff() noexcept;

    // Writes numSamples of stereo output (replaces buffer contents).
    void render(float* left, float* right, int numSamples) noexcept;

    int activeVoiceCount() const noexcept;
    double sampleRate() const noexcept { return sampleRate_; }
    bool arpRunning() const noexcept { return arpActive_ && arp_.isRunning(); }
    // Step the sequencer track is on (for the editor's playhead), or -1 when the sequencer is not running.
    int sequencerPosition(int track) const noexcept { return seq_.position(track); }
    // While recording into the sequencer, keys play normally even in Key trigger mode.
    void setSequencerRecording(bool on) noexcept { seqRecording_ = on; }
    // Oscillators of four voices per SIMD bank (default) or the scalar reference kernel (for tests).
    void setSimdOscillators(bool on) noexcept { simdOsc_ = on; }
    // Mod-matrix destination offsets (dst::Count values) of the most recently started voice after the
    // last render, or nullptr if no voice is playing. Audio thread only.
    const float* liveModDestinations() const noexcept { return liveMods_; }

    // Notes the arpeggiator (and sequencer) played during the last render(), for MIDI output to the host when
    // the patch asks for it. Offsets are samples from the start of that render() call.
    struct NoteEvent
    {
        int offset;
        uint8_t note;
        uint8_t velocity; // 0 = note off
    };
    static constexpr int kMaxNoteEvents = 512;
    int numNoteEvents() const noexcept { return numNoteEvents_; }
    const NoteEvent& noteEvent(int i) const noexcept { return noteEvents_[static_cast<size_t>(i)]; }

    // The instance's User 1-8 wavetables (message thread: load/clear; read by the voices).
    UserWavetables& userWaves() noexcept { return userWaves_; }
    const UserWavetables& userWaves() const noexcept { return userWaves_; }

private:
    // Notes reach the voices through these; the public noteOn/noteOff go via the arpeggiator when it is on.
    void voiceNoteOn(int note, float velocity) noexcept;
    void voiceNoteOff(int note, float releaseVelocity, bool honourPedal) noexcept;
    void arpNoteOn(int note, float velocity) noexcept override
    {
        keyChannel_[static_cast<size_t>(note & 127)] = 0; // arp notes carry no per-note expression
        voiceNoteOn(note, velocity);
        if (patch_.arp.midiOut)
            recordNoteEvent(note, static_cast<uint8_t>(std::clamp(static_cast<int>(velocity * 127.0f + 0.5f), 1, 127)));
    }
    void arpNoteOff(int note) noexcept override
    {
        voiceNoteOff(note, 0.0f, false);
        if (patch_.arp.midiOut) recordNoteEvent(note, 0);
    }
    void seqNoteOn(int note, float velocity) noexcept override
    {
        keyChannel_[static_cast<size_t>(note & 127)] = 0;
        voiceNoteOn(note, velocity);
        if (patch_.seq.midiOut)
            recordNoteEvent(note, static_cast<uint8_t>(std::clamp(static_cast<int>(velocity * 127.0f + 0.5f), 1, 127)));
    }
    void seqNoteOff(int note) noexcept override
    {
        voiceNoteOff(note, 0.0f, false);
        if (patch_.seq.midiOut) recordNoteEvent(note, 0);
    }
    bool seqTakesKeys() const noexcept { return patch_.seq.enabled && patch_.seq.trigger == SeqTrigger::Key && !seqRecording_; }
    void applySequencerLanes() noexcept;
    void recordNoteEvent(int note, uint8_t velocity) noexcept
    {
        if (numNoteEvents_ < kMaxNoteEvents)
            noteEvents_[static_cast<size_t>(numNoteEvents_++)] = { renderOffset_, static_cast<uint8_t>(note & 127), velocity };
    }
    void updateArpEnabled() noexcept;

    void configureOversampling(int factor) noexcept;
    void renderChunk(float* left, float* right, int n) noexcept;
    RenderContext makeContext() const noexcept;

    bool isMonoMode() const noexcept;
    int monoPriorityNote() const noexcept;
    void startPoly(int note, float velocity) noexcept;
    void startMono(int note, float velocity, bool legato) noexcept;
    Voice& pickPolyVoice(int note) noexcept;
    void launch(Voice& v, int note, float velocity, float spread, int stereoSide) noexcept;
    float nextPannerPosition() noexcept;
    void triggerAll(EnvTrig t) noexcept;
    void applyExpression(Voice& v) noexcept;
    void retriggerSharedLfos() noexcept;
    void releaseSustained() noexcept;

    Patch patch_;
    std::array<Voice, kMaxVoices> voices_{};
    ModState mod_{};
    std::array<NoteEvent, kMaxNoteEvents> noteEvents_{};
    int numNoteEvents_ = 0;
    int renderOffset_ = 0;
    Sequencer seq_;
    bool seqRecording_ = false;
    std::array<bool, 128> seqKey_{}; // keys that went to the sequencer (Key trigger)
    UserWavetables userWaves_;
    std::array<Lfo, kNumLfos> sharedLfo_{};
    Lfo vibratoLfo_;
    Decimator2x decL_, decR_;
    FxChain fx_;
    Arpeggiator arp_;
    bool arpActive_ = false;
    bool simdOsc_ = true;
    const float* liveMods_ = nullptr;

    double sampleRate_ = 48000.0;
    double bpm_ = 120.0;
    double transportPpq_ = -1.0;
    int osFactor_ = 2;
    uint64_t ageCounter_ = 0;
    int rotateNext_ = 0;
    int pannerStep_ = 0;
    float notePan_ = 0.0f;   // panner position chosen for the note being started
    Rng pannerRng_;

    // Held keys, in press order (for mono priority and Unison Poly).
    std::array<int, 128> held_{};
    int heldCount_ = 0;
    std::array<float, 128> keyVelocity_{};
    std::array<uint8_t, 128> keyChannel_{};   // channel of each key's latest note-on (0 = none)
    // Per-channel expression (index = MIDI channel 1-16; 0 = keys without a channel).
    std::array<float, 17> chBend_{}, chPressure_{}, chTimbre_{};
    int monoNote_ = -1;
    float lastPitch_ = -1.0f;

    Algorithm algo_{};
    float patchGain_ = 1.0f;

    float sharedLfoBuf_[kNumLfos][kMaxChunk * 2] = {};
    float vibratoBuf_[kMaxChunk * 2] = {};
    float osL_[kMaxChunk * 2] = {}, osR_[kMaxChunk * 2] = {};

    // SIMD filter banks: voices 4b..4b+3 share bank b (one lane each).
    static constexpr int kNumBanks = kMaxVoices / 4;
    std::array<FilterBank4, kNumBanks> banks_{};
    float bankPre_[kNumBanks][Voice::kControlBlock * 4] = {};
    float bankGainL_[kNumBanks][Voice::kControlBlock * 4] = {};
    float bankGainR_[kNumBanks][Voice::kControlBlock * 4] = {};
};

} // namespace kmi
