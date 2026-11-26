#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "Parameters.h"
#include "kmi/Engine.h"

class KMISynthProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kScopeSize = 4096;
    using LfoSteps = std::array<float, kmi::kMaxLfoSteps>;

    KMISynthProcessor();
    ~KMISynthProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; } // the arpeggiator can send its notes to the host
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return presetName_; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- Editor API (message thread) -----------------------------------------------------------
    juce::AudioProcessorValueTreeState& state() noexcept { return apvts_; }
    // Editor view state (selected page and sub-tabs), kept while the plugin is loaded so it survives page
    // switches and closing the editor window. Not saved with the session.
    juce::NamedValueSet& uiState() noexcept { return uiState_; }
    juce::UndoManager& undoManager() noexcept { return undo_; }
    juce::MidiKeyboardState& keyboardState() noexcept { return keyboard_; }

    kmi::Algorithm getCustomAlgorithm() const { return extra_.customAlgorithm; }
    void setCustomAlgorithm(const kmi::Algorithm& a);
    LfoSteps getLfoSteps(int lfo) const;
    void setLfoSteps(int lfo, const LfoSteps& steps);
    // Step-sequencer data (notes per step, macro lanes): saved with the patch.
    const kmi::SeqData& getSeqData() const { return extra_.seqData; }
    void setSeqData(const kmi::SeqData& d);
    // Step each sequencer track is on (editor playhead), or -1.
    int sequencerPosition(int track) const noexcept { return seqPos_[static_cast<size_t>(juce::jlimit(0, kmi::kSeqTracks - 1, track))].load(std::memory_order_relaxed); }
    // Recording: while armed, keys play normally (even in Key trigger mode) and every note-on is queued for
    // the editor, which writes it into the sequence.
    void setSequencerRecording(bool on) noexcept { seqRecording_.store(on, std::memory_order_relaxed); }
    bool sequencerRecording() const noexcept { return seqRecording_.load(std::memory_order_relaxed); }
    struct RecordedNote { int note; float velocity; };

    // Scala tuning: load a .scl scale or a .kbm keyboard mapping (for the loaded scale). Saved with the patch.
    // Returns an error message, or empty on success.
    juce::String loadScalaFile(const juce::File& file);
    void clearScala();
    juce::String scalaName() const { return extra_.scalaName; }
    bool scalaHasMapping() const { return extra_.kbmText.isNotEmpty(); }
    const std::array<float, 128>& scalaTable() const { return extra_.scalaTable; }
    int popRecordedNotes(RecordedNote* dest, int max) noexcept; // message thread

    // Most recent `n` output samples (mono), oldest first. n <= kScopeSize.
    void copyScope(float* dest, int n) const noexcept;
    int activeVoices() const noexcept { return activeVoices_.load(std::memory_order_relaxed); }
    // Live modulation offset of a mod destination for the newest voice (modulation rings), and whether
    // a voice is playing at all.
    float liveMod(int dest) const noexcept
    {
        return dest > 0 && dest < kmi::dst::Count ? liveMods_[static_cast<size_t>(dest)].load(std::memory_order_relaxed) : 0.0f;
    }
    bool hasLiveMods() const noexcept { return liveModsValid_.load(std::memory_order_relaxed); }

    // Presets (R-PRE-2): human-readable XML files, extension .kmip.
    void initPatch();
    bool savePreset(const juce::File& file);
    bool loadPreset(const juce::File& file);
    juce::String presetName() const { return presetName_; }
    juce::String presetTags() const { return presetTags_; }   // comma-separated, saved in .kmip files
    void setPresetTags(const juce::String& tags) { presetTags_ = tags; }
    // About Patch: who made the sound and what it is. Saved in .kmip files (and the session) with the tags.
    juce::String presetAuthor() const { return presetAuthor_; }
    juce::String presetDescription() const { return presetDescription_; }
    juce::String presetCreated() const { return presetCreated_; } // ISO date of the first save, or empty
    void setPresetAuthor(const juce::String& s) { presetAuthor_ = s.trim(); }
    void setPresetDescription(const juce::String& s) { presetDescription_ = s.trim(); }

    // A preset file's About Patch fields, read from the root element only (fast enough for the browser).
    struct PresetFileInfo
    {
        juce::String tags, author, description, created, appVersion;
    };
    static PresetFileInfo readPresetFileInfo(const juce::File& file);
    // Rewrite only the tags / author / description of a preset file on disk.
    static bool writePresetFileInfo(const juce::File& file, const juce::String& tags, const juce::String& author,
                                    const juce::String& description);
    static bool writePresetFileTags(const juce::File& file, const juce::String& tags);
    static juce::String readPresetFileTags(const juce::File& file) { return readPresetFileInfo(file).tags; }
    // The name filled in as Author for new patches (remembered across sessions in the user's settings).
    static juce::String defaultAuthor();
    static void setDefaultAuthor(const juce::String& name);
    static juce::File userPresetFolder();
    static juce::String presetExtension() { return ".kmip"; }

    // Factory presets (built into the engine, see kmi/Presets.h).
    void loadFactoryPreset(int index);
    int factoryPresetIndex() const noexcept { return factoryIndex_; } // -1 unless a factory preset is loaded

    // MIDI learn: a controller (CC 0-119) drives a parameter. One CC per parameter; a CC can drive
    // only one parameter. Saved with the plugin state, not in presets, so it survives preset changes.
    void tapTrigger() noexcept { tapPending_.store(true); } // envelopes with the Tap trigger source
    void startMidiLearn(const juce::String& paramId);
    void cancelMidiLearn() noexcept { learnTarget_.store(-1); }
    juce::String midiLearnTarget() const;                // parameter ID being learned, or empty
    int midiCcFor(const juce::String& paramId) const;    // -1 if unmapped
    void clearMidiCc(const juce::String& paramId);
    void clearAllMidiCcs();
    int numMidiMappings() const;

    // User wavetables (the User 1-8 waves). Saved in the session and in user presets; Init and factory
    // presets clear them. A WAV holding one cycle (up to 8192 samples) fills `slot`; a WAV of several
    // 2048-sample frames (a wavetable file) fills consecutive slots from `slot` (up to 8 frames, evenly
    // picked), ready for Wave Scan. Returns an error message, or empty on success; `slotsUsed` is set.
    juce::String loadUserWave(int slot, const juce::File& file, int& slotsUsed);
    void clearUserWave(int slot);
    bool hasUserWave(int slot) const { return engine_.userWaves().has(slot); }
    juce::String userWaveName(int slot) const { return juce::String(engine_.userWaves().name(slot)); }
    const std::vector<float>& userWaveCycle(int slot) const { return engine_.userWaves().cycle(slot); }
    int userWaveVersion() const noexcept { return userWaveVersion_; } // changes whenever any slot changes

private:
    juce::NamedValueSet uiState_;
    juce::String presetAuthor_, presetDescription_, presetCreated_;
    // Non-automatable patch data (custom algorithm, step-LFO values). Owned by the message thread;
    // handed to the audio thread through a try-locked copy.
    struct ExtraState
    {
        kmi::Algorithm customAlgorithm{};
        std::array<LfoSteps, kmi::kNumLfos> lfoSteps{};
        kmi::SeqData seqData{};
        std::array<float, 128> scalaTable = kmi::identityPitches();
        juce::String sclText, kbmText, scalaName; // message thread only (the audio thread copies the table)
    };

    void handleMidi(const juce::MidiMessage& m) noexcept;
    void publishExtraState(const ExtraState& s);
    juce::ValueTree extraToTree(const ExtraState& s) const;
    ExtraState extraFromTree(const juce::ValueTree& t) const;
    juce::ValueTree fullState();
    void restoreFullState(juce::ValueTree tree);
    juce::ValueTree userWavesToTree() const;
    void userWavesFromTree(const juce::ValueTree& t); // an invalid tree clears every slot

    juce::UndoManager undo_;
    juce::AudioProcessorValueTreeState apvts_;
    kmiplug::ParameterBinding binding_;
    kmi::Engine engine_;
    juce::MidiBuffer midiOut_; // arp notes for the host (pre-allocated)
    juce::MidiKeyboardState keyboard_;

    ExtraState extra_{};            // message thread copy (source of truth for saving)
    ExtraState pendingExtra_{};     // guarded by extraLock_
    juce::SpinLock extraLock_;
    std::atomic<bool> extraDirty_{ false };

    std::array<std::atomic<float>, kScopeSize> scope_{};
    std::atomic<int> scopeWrite_{ 0 };
    std::atomic<int> activeVoices_{ 0 };
    std::array<std::atomic<float>, kmi::dst::Count> liveMods_{};
    std::atomic<bool> liveModsValid_{ false };
    juce::String presetName_{ "Init" };
    juce::String presetTags_;
    int factoryIndex_ = -1;
    int userWaveVersion_ = 0;
    std::array<std::atomic<int>, kmi::kSeqTracks> seqPos_{};
    std::atomic<bool> seqRecording_{ false };
    static constexpr int kRecordFifoSize = 256;
    juce::AbstractFifo recordFifo_{ kRecordFifoSize };
    std::array<RecordedNote, kRecordFifoSize> recordBuf_{};

    int paramIndex(const juce::String& id) const;
    juce::ValueTree midiMapToTree() const;
    void midiMapFromTree(const juce::ValueTree& t);
    juce::Array<juce::RangedAudioParameter*> params_;   // index = position in getParameters()
    std::array<std::atomic<int>, 120> ccMap_{};          // CC -> parameter index, -1 = none
    std::atomic<int> learnTarget_{ -1 };
    std::atomic<bool> tapPending_{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KMISynthProcessor)
};
