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
    bool producesMidi() const override { return false; }
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
    juce::UndoManager& undoManager() noexcept { return undo_; }
    juce::MidiKeyboardState& keyboardState() noexcept { return keyboard_; }

    kmi::Algorithm getCustomAlgorithm() const { return extra_.customAlgorithm; }
    void setCustomAlgorithm(const kmi::Algorithm& a);
    LfoSteps getLfoSteps(int lfo) const;
    void setLfoSteps(int lfo, const LfoSteps& steps);

    // Most recent `n` output samples (mono), oldest first. n <= kScopeSize.
    void copyScope(float* dest, int n) const noexcept;
    int activeVoices() const noexcept { return activeVoices_.load(std::memory_order_relaxed); }

    // Presets (R-PRE-2): human-readable XML files, extension .kmip.
    void initPatch();
    bool savePreset(const juce::File& file);
    bool loadPreset(const juce::File& file);
    juce::String presetName() const { return presetName_; }
    static juce::String presetExtension() { return ".kmip"; }

    // Factory presets (built into the engine, see kmi/Presets.h).
    void loadFactoryPreset(int index);
    int factoryPresetIndex() const noexcept { return factoryIndex_; } // -1 unless a factory preset is loaded

private:
    // Non-automatable patch data (custom algorithm, step-LFO values). Owned by the message thread;
    // handed to the audio thread through a try-locked copy.
    struct ExtraState
    {
        kmi::Algorithm customAlgorithm{};
        std::array<LfoSteps, kmi::kNumLfos> lfoSteps{};
    };

    void handleMidi(const juce::MidiMessage& m) noexcept;
    void publishExtraState(const ExtraState& s);
    juce::ValueTree extraToTree(const ExtraState& s) const;
    ExtraState extraFromTree(const juce::ValueTree& t) const;
    juce::ValueTree fullState();
    void restoreFullState(juce::ValueTree tree);

    juce::UndoManager undo_;
    juce::AudioProcessorValueTreeState apvts_;
    kmiplug::ParameterBinding binding_;
    kmi::Engine engine_;
    juce::MidiKeyboardState keyboard_;

    ExtraState extra_{};            // message thread copy (source of truth for saving)
    ExtraState pendingExtra_{};     // guarded by extraLock_
    juce::SpinLock extraLock_;
    std::atomic<bool> extraDirty_{ false };

    std::array<std::atomic<float>, kScopeSize> scope_{};
    std::atomic<int> scopeWrite_{ 0 };
    std::atomic<int> activeVoices_{ 0 };
    juce::String presetName_{ "Init" };
    int factoryIndex_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KMISynthProcessor)
};
