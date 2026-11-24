// The KMISynth sound engine: 16 voices, note allocation, shared LFOs, oversampling.
// Framework-agnostic: the plugin wrapper owns one Engine, writes engine.patch() on the audio
// thread, forwards MIDI as method calls and calls render().
#pragma once

#include <array>
#include <cstdint>

#include "kmi/Decimator.h"
#include "kmi/Effects.h"
#include "kmi/Lfo.h"
#include "kmi/Patch.h"
#include "kmi/Voice.h"

namespace kmi {

class Engine
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

    void noteOn(int note, float velocity) noexcept;
    void noteOff(int note, float releaseVelocity = 0.0f) noexcept;
    void polyAftertouch(int note, float value) noexcept;
    void channelAftertouch(float value) noexcept { mod_.channelAT = value; }
    void pitchBend(float bipolar) noexcept { mod_.pitchBend = bipolar; }
    void controlChange(int cc, float value01) noexcept;
    void allNotesOff() noexcept;
    void allSoundOff() noexcept;

    // Writes numSamples of stereo output (replaces buffer contents).
    void render(float* left, float* right, int numSamples) noexcept;

    int activeVoiceCount() const noexcept;
    double sampleRate() const noexcept { return sampleRate_; }

private:
    void configureOversampling(int factor) noexcept;
    void renderChunk(float* left, float* right, int n) noexcept;
    RenderContext makeContext() const noexcept;

    bool isMonoMode() const noexcept;
    int monoPriorityNote() const noexcept;
    void startPoly(int note, float velocity) noexcept;
    void startMono(int note, float velocity, bool legato) noexcept;
    Voice& pickPolyVoice(int note) noexcept;
    void launch(Voice& v, int note, float velocity, float spread) noexcept;
    void retriggerSharedLfos() noexcept;
    void releaseSustained() noexcept;

    Patch patch_;
    std::array<Voice, kMaxVoices> voices_{};
    ModState mod_{};
    std::array<Lfo, kNumLfos> sharedLfo_{};
    Lfo vibratoLfo_;
    Decimator2x decL_, decR_;
    FxChain fx_;

    double sampleRate_ = 48000.0;
    double bpm_ = 120.0;
    double transportPpq_ = -1.0;
    int osFactor_ = 2;
    uint64_t ageCounter_ = 0;
    int rotateNext_ = 0;

    // Held keys, in press order (for mono priority and Unison Poly).
    std::array<int, 128> held_{};
    int heldCount_ = 0;
    std::array<float, 128> keyVelocity_{};
    int monoNote_ = -1;
    float lastPitch_ = -1.0f;

    Algorithm algo_{};
    float patchGain_ = 1.0f;

    float sharedLfoBuf_[kNumLfos][kMaxChunk * 2] = {};
    float vibratoBuf_[kMaxChunk * 2] = {};
    float osL_[kMaxChunk * 2] = {}, osR_[kMaxChunk * 2] = {};
};

} // namespace kmi
