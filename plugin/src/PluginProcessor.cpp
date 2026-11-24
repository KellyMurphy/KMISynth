#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "kmi/Presets.h"

namespace {
constexpr const char* kExtraTag = "KMIExtra";
constexpr const char* kMidiMapTag = "KMIMidiMap";
} // namespace

KMISynthProcessor::KMISynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, &undo_, "KMISynth", kmiplug::ParameterBinding::createLayout())
{
    binding_.bind(apvts_, engine_.patch());
    const kmi::Patch init = kmi::makeInitPatch();
    extra_.customAlgorithm = init.customAlgorithm;
    extra_.lfoSteps = init.lfoSteps;
    publishExtraState(extra_);
    for (auto& s : scope_) s.store(0.0f, std::memory_order_relaxed);
    for (auto* p : getParameters()) params_.add(dynamic_cast<juce::RangedAudioParameter*>(p));
    for (auto& c : ccMap_) c.store(-1, std::memory_order_relaxed);
}

void KMISynthProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    binding_.apply();
    engine_.prepare(sampleRate);
    keyboard_.reset();
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

void KMISynthProcessor::setCustomAlgorithm(const kmi::Algorithm& a)
{
    extra_.customAlgorithm = a;
    publishExtraState(extra_);
}

KMISynthProcessor::LfoSteps KMISynthProcessor::getLfoSteps(int lfo) const
{
    return extra_.lfoSteps[static_cast<size_t>(juce::jlimit(0, kmi::kNumLfos - 1, lfo))];
}

void KMISynthProcessor::setLfoSteps(int lfo, const LfoSteps& steps)
{
    extra_.lfoSteps[static_cast<size_t>(juce::jlimit(0, kmi::kNumLfos - 1, lfo))] = steps;
    publishExtraState(extra_);
}

void KMISynthProcessor::copyScope(float* dest, int n) const noexcept
{
    n = juce::jlimit(0, kScopeSize, n);
    const int w = scopeWrite_.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
        dest[i] = scope_[static_cast<size_t>((w - n + i + kScopeSize) % kScopeSize)].load(std::memory_order_relaxed);
}

void KMISynthProcessor::handleMidi(const juce::MidiMessage& m) noexcept
{
    // MPE (lower zone): channel 1 is global, channels 2-16 each carry one note and its expression.
    const int channel = m.getChannel();
    const bool member = engine_.patch().voice.mpe && channel >= 2;
    const int noteChannel = member ? channel : 0;

    if (m.isNoteOn()) engine_.noteOn(m.getNoteNumber(), m.getFloatVelocity(), noteChannel);
    else if (m.isNoteOff()) engine_.noteOff(m.getNoteNumber(), m.getFloatVelocity(), noteChannel);
    else if (m.isAftertouch()) engine_.polyAftertouch(m.getNoteNumber(), static_cast<float>(m.getAfterTouchValue()) / 127.0f);
    else if (m.isChannelPressure())
    {
        const float v = static_cast<float>(m.getChannelPressureValue()) / 127.0f;
        if (member) engine_.channelPressure(channel, v);
        else engine_.channelAftertouch(v);
    }
    else if (m.isPitchWheel())
    {
        const float v = static_cast<float>(m.getPitchWheelValue() - 8192) / 8192.0f;
        if (member) engine_.channelPitchBend(channel, v);
        else engine_.pitchBend(v);
    }
    else if (member && m.isControllerOfType(74))
    {
        engine_.channelTimbre(channel, static_cast<float>(m.getControllerValue()) / 127.0f);
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const float value = static_cast<float>(m.getControllerValue()) / 127.0f;
        if (cc >= 0 && cc < static_cast<int>(ccMap_.size()))
        {
            int target = learnTarget_.load(std::memory_order_acquire);
            if (target >= 0 && learnTarget_.compare_exchange_strong(target, -1))
            {
                for (auto& c : ccMap_)
                {
                    int expected = target;
                    c.compare_exchange_strong(expected, -1); // the parameter's previous CC lets go
                }
                ccMap_[static_cast<size_t>(cc)].store(target, std::memory_order_release);
            }
            const int idx = ccMap_[static_cast<size_t>(cc)].load(std::memory_order_acquire);
            if (idx >= 0 && idx < params_.size() && params_[idx] != nullptr) params_[idx]->setValueNotifyingHost(value);
        }
        engine_.controlChange(cc, value); // still available as a mod source
    }
}

// ---- MIDI learn ------------------------------------------------------------------------------------------
int KMISynthProcessor::paramIndex(const juce::String& id) const
{
    for (int i = 0; i < params_.size(); ++i)
        if (params_[i] != nullptr && params_[i]->getParameterID() == id) return i;
    return -1;
}

void KMISynthProcessor::startMidiLearn(const juce::String& paramId) { learnTarget_.store(paramIndex(paramId)); }

juce::String KMISynthProcessor::midiLearnTarget() const
{
    const int t = learnTarget_.load();
    return t >= 0 && t < params_.size() && params_[t] != nullptr ? params_[t]->getParameterID() : juce::String();
}

int KMISynthProcessor::midiCcFor(const juce::String& paramId) const
{
    const int idx = paramIndex(paramId);
    if (idx < 0) return -1;
    for (int cc = 0; cc < static_cast<int>(ccMap_.size()); ++cc)
        if (ccMap_[static_cast<size_t>(cc)].load() == idx) return cc;
    return -1;
}

void KMISynthProcessor::clearMidiCc(const juce::String& paramId)
{
    const int idx = paramIndex(paramId);
    if (idx < 0) return;
    for (auto& c : ccMap_)
    {
        int expected = idx;
        c.compare_exchange_strong(expected, -1);
    }
}

void KMISynthProcessor::clearAllMidiCcs()
{
    for (auto& c : ccMap_) c.store(-1);
}

int KMISynthProcessor::numMidiMappings() const
{
    int n = 0;
    for (const auto& c : ccMap_) n += c.load() >= 0 ? 1 : 0;
    return n;
}

juce::ValueTree KMISynthProcessor::midiMapToTree() const
{
    juce::ValueTree t(kMidiMapTag);
    for (int cc = 0; cc < static_cast<int>(ccMap_.size()); ++cc)
    {
        const int idx = ccMap_[static_cast<size_t>(cc)].load();
        if (idx < 0 || idx >= params_.size() || params_[idx] == nullptr) continue;
        juce::ValueTree m("Map");
        m.setProperty("cc", cc, nullptr);
        m.setProperty("param", params_[idx]->getParameterID(), nullptr);
        t.appendChild(m, nullptr);
    }
    return t;
}

void KMISynthProcessor::midiMapFromTree(const juce::ValueTree& t)
{
    clearAllMidiCcs();
    for (const auto& m : t)
    {
        const int cc = static_cast<int>(m.getProperty("cc", -1));
        const int idx = paramIndex(m.getProperty("param").toString());
        if (cc >= 0 && cc < static_cast<int>(ccMap_.size()) && idx >= 0) ccMap_[static_cast<size_t>(cc)].store(idx);
    }
}

void KMISynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto pos = hostPlayHead->getPosition())
        {
            if (auto bpm = pos->getBpm()) engine_.setTempo(*bpm);
            const auto ppq = pos->getPpqPosition();
            engine_.setTransport(ppq ? *ppq : -1.0, pos->getIsPlaying() && ppq.hasValue());
        }
    }

    // Notes played on the editor's on-screen keyboard.
    keyboard_.processNextMidiBuffer(midi, 0, numSamples, true);

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
    if (tapPending_.exchange(false)) engine_.tapTrigger();

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

    // Oscilloscope feed and voice count for the editor.
    int w = scopeWrite_.load(std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        scope_[static_cast<size_t>(w)].store(0.5f * (left[i] + right[i]), std::memory_order_relaxed);
        w = (w + 1) % kScopeSize;
    }
    scopeWrite_.store(w, std::memory_order_release);
    activeVoices_.store(engine_.activeVoiceCount(), std::memory_order_relaxed);
    if (const float* mods = engine_.liveModDestinations(); mods && engine_.activeVoiceCount() > 0)
    {
        for (int d = 0; d < kmi::dst::Count; ++d) liveMods_[static_cast<size_t>(d)].store(mods[d], std::memory_order_relaxed);
        liveModsValid_.store(true, std::memory_order_relaxed);
    }
    else
    {
        liveModsValid_.store(false, std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* KMISynthProcessor::createEditor()
{
    return new KMISynthEditor(*this);
}

// ---- State ----------------------------------------------------------------------------------------------
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

juce::ValueTree KMISynthProcessor::fullState()
{
    auto state = apvts_.copyState();
    state.setProperty("formatVersion", 1, nullptr);
    state.setProperty("presetName", presetName_, nullptr);
    state.setProperty("tags", presetTags_, nullptr);
    state.setProperty("factoryIndex", factoryIndex_, nullptr);
    state.removeChild(state.getChildWithName(kExtraTag), nullptr);
    state.appendChild(extraToTree(extra_), nullptr);
    return state;
}

void KMISynthProcessor::restoreFullState(juce::ValueTree tree)
{
    const auto extraTree = tree.getChildWithName(kExtraTag);
    if (extraTree.isValid())
    {
        extra_ = extraFromTree(extraTree);
        publishExtraState(extra_);
        tree.removeChild(extraTree, nullptr);
    }
    presetName_ = tree.getProperty("presetName", "Init").toString();
    presetTags_ = tree.getProperty("tags", "").toString();
    factoryIndex_ = static_cast<int>(tree.getProperty("factoryIndex", -1));
    apvts_.replaceState(tree);
    undo_.clearUndoHistory();
}

void KMISynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = fullState();
    state.appendChild(midiMapToTree(), nullptr); // the session keeps its controller map; presets do not
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void KMISynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (!xml || !xml->hasTagName(apvts_.state.getType())) return;
    auto tree = juce::ValueTree::fromXml(*xml);
    const auto map = tree.getChildWithName(kMidiMapTag);
    midiMapFromTree(map); // an absent map (older sessions) clears it
    tree.removeChild(map, nullptr);
    restoreFullState(tree);
}

void KMISynthProcessor::initPatch()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p)) rp->setValueNotifyingHost(rp->getDefaultValue());
    const kmi::Patch init = kmi::makeInitPatch();
    extra_.customAlgorithm = init.customAlgorithm;
    extra_.lfoSteps = init.lfoSteps;
    publishExtraState(extra_);
    presetName_ = "Init";
    presetTags_ = {};
    factoryIndex_ = -1;
    undo_.clearUndoHistory();
}

void KMISynthProcessor::loadFactoryPreset(int index)
{
    if (index < 0 || index >= kmi::numFactoryPresets()) return;
    const kmi::Patch p = kmi::makeFactoryPreset(index);
    kmiplug::ParameterBinding::writePatch(apvts_, p);
    extra_.customAlgorithm = p.customAlgorithm;
    extra_.lfoSteps = p.lfoSteps;
    publishExtraState(extra_);
    presetName_ = kmi::factoryPresetInfo(index).name;
    presetTags_ = kmi::factoryPresetTags(index);
    factoryIndex_ = index;
    undo_.clearUndoHistory();
}

bool KMISynthProcessor::savePreset(const juce::File& file)
{
    presetName_ = file.getFileNameWithoutExtension();
    factoryIndex_ = -1; // a saved copy is a user preset
    auto xml = fullState().createXml();
    return xml && xml->writeTo(file);
}

bool KMISynthProcessor::loadPreset(const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(file);
    if (!xml || !xml->hasTagName(apvts_.state.getType())) return false;
    auto tree = juce::ValueTree::fromXml(*xml);
    tree.setProperty("presetName", file.getFileNameWithoutExtension(), nullptr);
    tree.setProperty("factoryIndex", -1, nullptr);
    restoreFullState(tree);
    return true;
}

juce::File KMISynthProcessor::userPresetFolder()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("KMISynth").getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

juce::String KMISynthProcessor::readPresetFileTags(const juce::File& file)
{
    // Only the root element's attributes are needed: stop parsing after the opening tag.
    juce::XmlDocument doc(file);
    if (auto xml = doc.getDocumentElementIfTagMatches("KMISynth")) return xml->getStringAttribute("tags");
    return {};
}

bool KMISynthProcessor::writePresetFileTags(const juce::File& file, const juce::String& tags)
{
    auto xml = juce::XmlDocument::parse(file);
    if (!xml) return false;
    xml->setAttribute("tags", tags);
    return xml->writeTo(file);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KMISynthProcessor();
}
