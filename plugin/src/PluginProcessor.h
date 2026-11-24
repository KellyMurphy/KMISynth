#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "kmi/Engine.h"

class KMISynthProcessor final : public juce::AudioProcessor
{
public:
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
    const juce::String getProgramName(int) override { return "Init"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return apvts_; }

private:
    // Non-automatable patch data (custom algorithm, step-LFO values). Owned by the message thread;
    // handed to the audio thread through a try-locked copy.
    struct ExtraState
    {
        kmi::Algorithm customAlgorithm{};
        std::array<std::array<float, kmi::kMaxLfoSteps>, kmi::kNumLfos> lfoSteps{};
    };

    void handleMidi(const juce::MidiMessage& m) noexcept;
    void publishExtraState(const ExtraState& s);
    juce::ValueTree extraToTree(const ExtraState& s) const;
    ExtraState extraFromTree(const juce::ValueTree& t) const;

    juce::AudioProcessorValueTreeState apvts_;
    kmiplug::ParameterBinding binding_;
    kmi::Engine engine_;

    ExtraState extra_{};            // message thread copy (source of truth for saving)
    ExtraState pendingExtra_{};     // guarded by extraLock_
    juce::SpinLock extraLock_;
    std::atomic<bool> extraDirty_{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KMISynthProcessor)
};
