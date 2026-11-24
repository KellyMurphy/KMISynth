#include "PluginProcessor.h"

namespace {
constexpr const char* kExtraTag = "KMIExtra";
} // namespace

KMISynthProcessor::KMISynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KMISynth", kmiplug::ParameterBinding::createLayout())
{
    binding_.bind(apvts_, engine_.patch());
    const kmi::Patch init = kmi::makeInitPatch();
    extra_.customAlgorithm = init.customAlgorithm;
    extra_.lfoSteps = init.lfoSteps;
    publishExtraState(extra_);
}

void KMISynthProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    binding_.apply();
    engine_.prepare(sampleRate);
}

bool KMISynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void KMISynthProcessor::publishExtraState(const ExtraState& s)
{
    const juce::SpinLock::ScopedLockType lock(extraLock_);
    pendingExtra_ = s;
    extraDirty_.store(true, std::memory_order_release);
}

void KMISynthProcessor::handleMidi(const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn()) engine_.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) engine_.noteOff(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isAftertouch()) engine_.polyAftertouch(m.getNoteNumber(), static_cast<float>(m.getAfterTouchValue()) / 127.0f);
    else if (m.isChannelPressure()) engine_.channelAftertouch(static_cast<float>(m.getChannelPressureValue()) / 127.0f);
    else if (m.isPitchWheel()) engine_.pitchBend(static_cast<float>(m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isController()) engine_.controlChange(m.getControllerNumber(), static_cast<float>(m.getControllerValue()) / 127.0f);
}

void KMISynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto pos = hostPlayHead->getPosition())
        {
            if (auto bpm = pos->getBpm()) engine_.setTempo(*bpm);
            const auto ppq = pos->getPpqPosition();
            engine_.setTransport(ppq ? *ppq : -1.0, pos->getIsPlaying() && ppq.hasValue());
        }
    }

    // Non-parameter patch data from the message thread (never blocks the audio thread).
    if (extraDirty_.load(std::memory_order_acquire))
    {
        const juce::SpinLock::ScopedTryLockType lock(extraLock_);
        if (lock.isLocked())
        {
            engine_.patch().customAlgorithm = pendingExtra_.customAlgorithm;
            engine_.patch().lfoSteps = pendingExtra_.lfoSteps;
            extraDirty_.store(false, std::memory_order_relaxed);
        }
    }

    binding_.apply();

    const int numSamples = buffer.getNumSamples();
    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

    // Sample-accurate MIDI: render up to each event, then apply it.
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit(0, numSamples, meta.samplePosition);
        if (t > pos)
        {
            engine_.render(left + pos, right + pos, t - pos);
            pos = t;
        }
        handleMidi(meta.getMessage());
    }
    if (pos < numSamples) engine_.render(left + pos, right + pos, numSamples - pos);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, numSamples);
}

juce::AudioProcessorEditor* KMISynthProcessor::createEditor()
{
    // Phase 1: host-generic editor. The custom UI (R-UI-*) replaces this.
    return new juce::GenericAudioProcessorEditor(*this);
}

juce::ValueTree KMISynthProcessor::extraToTree(const ExtraState& s) const
{
    juce::ValueTree t(kExtraTag);
    juce::String algo;
    for (int m = 0; m < kmi::kNumOscs; ++m)
        for (int c = 0; c < kmi::kNumOscs; ++c) algo << juce::String(s.customAlgorithm.conn[m][c], 4) << " ";
    juce::String out;
    for (int i = 0; i < kmi::kNumOscs; ++i) out << juce::String(s.customAlgorithm.out[i], 4) << " ";
    t.setProperty("customConn", algo.trim(), nullptr);
    t.setProperty("customOut", out.trim(), nullptr);
    for (int k = 0; k < kmi::kNumLfos; ++k)
    {
        juce::String steps;
        for (float v : s.lfoSteps[static_cast<size_t>(k)]) steps << juce::String(v, 4) << " ";
        t.setProperty("lfo" + juce::String(k + 1) + "Steps", steps.trim(), nullptr);
    }
    return t;
}

KMISynthProcessor::ExtraState KMISynthProcessor::extraFromTree(const juce::ValueTree& t) const
{
    ExtraState s = extra_;
    auto parse = [](const juce::var& v) {
        juce::StringArray tokens;
        tokens.addTokens(v.toString(), " ", "");
        tokens.removeEmptyStrings();
        return tokens;
    };
    const auto conn = parse(t.getProperty("customConn"));
    if (conn.size() == kmi::kNumOscs * kmi::kNumOscs)
        for (int i = 0; i < conn.size(); ++i) s.customAlgorithm.conn[i / kmi::kNumOscs][i % kmi::kNumOscs] = conn[i].getFloatValue();
    const auto out = parse(t.getProperty("customOut"));
    if (out.size() == kmi::kNumOscs)
        for (int i = 0; i < out.size(); ++i) s.customAlgorithm.out[i] = out[i].getFloatValue();
    for (int k = 0; k < kmi::kNumLfos; ++k)
    {
        const auto steps = parse(t.getProperty("lfo" + juce::String(k + 1) + "Steps"));
        if (steps.size() == kmi::kMaxLfoSteps)
            for (int i = 0; i < steps.size(); ++i) s.lfoSteps[static_cast<size_t>(k)][static_cast<size_t>(i)] = steps[i].getFloatValue();
    }
    return s;
}

void KMISynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    state.setProperty("formatVersion", 1, nullptr);
    state.removeChild(state.getChildWithName(kExtraTag), nullptr);
    state.appendChild(extraToTree(extra_), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void KMISynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (!xml || !xml->hasTagName(apvts_.state.getType())) return;
    auto tree = juce::ValueTree::fromXml(*xml);
    const auto extraTree = tree.getChildWithName(kExtraTag);
    if (extraTree.isValid())
    {
        extra_ = extraFromTree(extraTree);
        publishExtraState(extra_);
        tree.removeChild(extraTree, nullptr);
    }
    apvts_.replaceState(tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KMISynthProcessor();
}
