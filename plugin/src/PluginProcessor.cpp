#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <cstring>
#include "kmi/Presets.h"
#include "kmi/Tuning.h"

namespace {
constexpr const char* kExtraTag = "KMIExtra";
constexpr const char* kMidiMapTag = "KMIMidiMap";
constexpr const char* kUserWavesTag = "KMIUserWaves";
} // namespace

KMISynthProcessor::KMISynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, &undo_, "KMISynth", kmiplug::ParameterBinding::createLayout())
{
    binding_.bind(apvts_, engine_.patch());
    const kmi::Patch init = kmi::makeInitPatch();
    extra_.customAlgorithm = init.customAlgorithm;
    extra_.lfoSteps = init.lfoSteps;
    extra_.seqData = init.seqData;
    extra_.scalaTable = init.scalaTable;
    extra_.sclText = extra_.kbmText = extra_.scalaName = {};
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
    midiOut_.ensureSize(4096); // no allocation on the audio thread for arp MIDI output
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

void KMISynthProcessor::setSeqData(const kmi::SeqData& d)
{
    extra_.seqData = d;
    publishExtraState(extra_);
}

int KMISynthProcessor::popRecordedNotes(RecordedNote* dest, int max) noexcept
{
    const auto scope = recordFifo_.read(juce::jmin(max, recordFifo_.getNumReady()));
    int n = 0;
    for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = recordBuf_[static_cast<size_t>(scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = recordBuf_[static_cast<size_t>(scope.startIndex2 + i)];
    return n;
}

juce::String KMISynthProcessor::loadScalaFile(const juce::File& file)
{
    const juce::String text = file.loadFileAsString();
    if (text.isEmpty()) return "Could not read " + file.getFileName() + ".";
    const bool isMapping = file.hasFileExtension("kbm");
    juce::String scl = isMapping ? extra_.sclText : text;
    juce::String kbm = isMapping ? text : juce::String();
    if (scl.isEmpty()) return "Load a .scl scale first, then its .kbm keyboard mapping.";
    if (!isMapping) // a mapping with the same name next to the scale is picked up automatically
    {
        const auto sibling = file.withFileExtension("kbm");
        if (sibling.existsAsFile()) kbm = sibling.loadFileAsString();
    }
    kmi::TuningTable table{};
    std::string name, error;
    if (!kmi::parseScala(scl.toStdString(), kbm.toStdString(), table, name, error)) return juce::String(error);
    extra_.sclText = scl;
    extra_.kbmText = kbm;
    extra_.scalaName = juce::String(name).isNotEmpty() ? juce::String(name) : file.getFileNameWithoutExtension();
    extra_.scalaTable = table;
    publishExtraState(extra_);
    return {};
}

void KMISynthProcessor::clearScala()
{
    extra_.sclText = extra_.kbmText = extra_.scalaName = {};
    extra_.scalaTable = kmi::identityPitches();
    publishExtraState(extra_);
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

    if (m.isNoteOn())
    {
        engine_.noteOn(m.getNoteNumber(), m.getFloatVelocity(), noteChannel);
        if (seqRecording_.load(std::memory_order_relaxed)) // queue it for the editor's sequencer recording
        {
            const auto scope = recordFifo_.write(1);
            if (scope.blockSize1 > 0) recordBuf_[static_cast<size_t>(scope.startIndex1)] = { m.getNoteNumber(), m.getFloatVelocity() };
        }
    }
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
            engine_.patch().seqData = pendingExtra_.seqData;
            engine_.patch().scalaTable = pendingExtra_.scalaTable;
            extraDirty_.store(false, std::memory_order_relaxed);
        }
    }

    binding_.apply();
    if (tapPending_.exchange(false)) engine_.tapTrigger();
    engine_.setSequencerRecording(seqRecording_.load(std::memory_order_relaxed));

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

    // Sample-accurate MIDI: render up to each event, then apply it. Notes the arpeggiator sends to the host
    // (Arp MIDI Out) are collected with their sample positions and replace the incoming MIDI at the end.
    midiOut_.clear();
    auto render = [&](int start, int n) {
        engine_.render(left + start, right + start, n);
        for (int e = 0; e < engine_.numNoteEvents(); ++e)
        {
            const auto& ev = engine_.noteEvent(e);
            const auto msg = ev.velocity > 0 ? juce::MidiMessage::noteOn(1, ev.note, static_cast<juce::uint8>(ev.velocity))
                                             : juce::MidiMessage::noteOff(1, ev.note);
            midiOut_.addEvent(msg, juce::jlimit(0, numSamples - 1, start + ev.offset));
        }
    };
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit(0, numSamples, meta.samplePosition);
        if (t > pos)
        {
            render(pos, t - pos);
            pos = t;
        }
        handleMidi(meta.getMessage());
    }
    if (pos < numSamples) render(pos, numSamples - pos);
    midi.swapWith(midiOut_);

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
    for (int t = 0; t < kmi::kSeqTracks; ++t)
        seqPos_[static_cast<size_t>(t)].store(engine_.sequencerPosition(t), std::memory_order_relaxed);
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
    // Sequencer: only the steps that hold notes or differ from the defaults, as
    // "step:probability,ratchet,drift:note/velocity/length,note/velocity/length ..." (steps 0-based).
    const kmi::SeqStep blankStep{};
    for (int tr = 0; tr < kmi::kSeqNoteTracks; ++tr)
    {
        juce::String steps;
        for (int k = 0; k < kmi::kSeqSteps; ++k)
        {
            const auto& st = s.seqData.steps[static_cast<size_t>(tr)][static_cast<size_t>(k)];
            if (st.count == 0 && st.probability == blankStep.probability && st.ratchet == blankStep.ratchet && st.drift == blankStep.drift)
                continue;
            steps << k << ":" << (int) st.probability << "," << (int) st.ratchet << "," << (int) st.drift << ":";
            for (int n = 0; n < st.count; ++n)
            {
                const auto& sn = st.notes[static_cast<size_t>(n)];
                steps << (n ? "," : "") << (int) sn.note << "/" << (int) sn.velocity << "/" << (int) sn.length;
            }
            steps << " ";
        }
        if (steps.isNotEmpty()) t.setProperty("seqTrack" + juce::String(tr + 1), steps.trim(), nullptr);
    }
    if (s.sclText.isNotEmpty()) // the Scala files themselves; the table is rebuilt from them on load
    {
        t.setProperty("scalaScl", s.sclText, nullptr);
        if (s.kbmText.isNotEmpty()) t.setProperty("scalaKbm", s.kbmText, nullptr);
    }
    for (int l = 0; l < kmi::kSeqLanes; ++l)
    {
        const auto& lane = s.seqData.lanes[static_cast<size_t>(l)];
        bool any = s.seqData.laneOn[static_cast<size_t>(l)];
        for (float v : lane) any = any || v != 0.0f;
        if (!any) continue;
        juce::String values;
        for (float v : lane) values << juce::String(v, 3) << " ";
        t.setProperty("seqLane" + juce::String(l + 1), (s.seqData.laneOn[static_cast<size_t>(l)] ? "on " : "off ") + values.trim(), nullptr);
    }
    return t;
}

KMISynthProcessor::ExtraState KMISynthProcessor::extraFromTree(const juce::ValueTree& t) const
{
    ExtraState s = extra_;
    s.seqData = kmi::SeqData{}; // patches without sequencer data have an empty sequence
    s.sclText = t.getProperty("scalaScl").toString();
    s.kbmText = t.getProperty("scalaKbm").toString();
    s.scalaName = {};
    s.scalaTable = kmi::identityPitches();
    if (s.sclText.isNotEmpty())
    {
        std::string name, error;
        if (kmi::parseScala(s.sclText.toStdString(), s.kbmText.toStdString(), s.scalaTable, name, error)) s.scalaName = juce::String(name);
        else s.sclText = s.kbmText = {};
    }
    for (int tr = 0; tr < kmi::kSeqNoteTracks; ++tr)
    {
        juce::StringArray steps;
        steps.addTokens(t.getProperty("seqTrack" + juce::String(tr + 1)).toString(), " ", "");
        steps.removeEmptyStrings();
        for (const auto& token : steps)
        {
            juce::StringArray parts;
            parts.addTokens(token, ":", "");
            if (parts.size() < 2) continue;
            const int k = parts[0].getIntValue();
            if (k < 0 || k >= kmi::kSeqSteps) continue;
            auto& st = s.seqData.steps[static_cast<size_t>(tr)][static_cast<size_t>(k)];
            juce::StringArray settings;
            settings.addTokens(parts[1], ",", "");
            if (settings.size() == 3)
            {
                st.probability = static_cast<uint8_t>(juce::jlimit(0, 100, settings[0].getIntValue()));
                st.ratchet = static_cast<uint8_t>(juce::jlimit(1, 4, settings[1].getIntValue()));
                st.drift = static_cast<uint8_t>(juce::jlimit(0, 100, settings[2].getIntValue()));
            }
            juce::StringArray notes;
            if (parts.size() > 2) notes.addTokens(parts[2], ",", "");
            notes.removeEmptyStrings();
            st.count = 0;
            for (const auto& n : notes)
            {
                juce::StringArray f;
                f.addTokens(n, "/", "");
                if (f.size() != 3 || st.count >= kmi::kSeqNotesPerStep) continue;
                st.notes[st.count++] = { static_cast<uint8_t>(juce::jlimit(0, 127, f[0].getIntValue())),
                                         static_cast<uint8_t>(juce::jlimit(1, 127, f[1].getIntValue())),
                                         static_cast<uint8_t>(juce::jlimit(1, 64, f[2].getIntValue())) };
            }
        }
    }
    for (int l = 0; l < kmi::kSeqLanes; ++l)
    {
        juce::StringArray tokens;
        tokens.addTokens(t.getProperty("seqLane" + juce::String(l + 1)).toString(), " ", "");
        tokens.removeEmptyStrings();
        if (tokens.size() != kmi::kSeqSteps + 1) continue;
        s.seqData.laneOn[static_cast<size_t>(l)] = tokens[0] == "on";
        for (int k = 0; k < kmi::kSeqSteps; ++k)
            s.seqData.lanes[static_cast<size_t>(l)][static_cast<size_t>(k)] = juce::jlimit(0.0f, 1.0f, tokens[k + 1].getFloatValue());
    }
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
    state.removeChild(state.getChildWithName(kUserWavesTag), nullptr);
    if (auto waves = userWavesToTree(); waves.getNumChildren() > 0) state.appendChild(waves, nullptr);
    return state;
}

juce::ValueTree KMISynthProcessor::userWavesToTree() const
{
    juce::ValueTree t(kUserWavesTag);
    const auto& uw = engine_.userWaves();
    for (int i = 0; i < kmi::UserWavetables::kSlots; ++i)
    {
        if (!uw.has(i)) continue;
        const auto& cycle = uw.cycle(i);
        juce::MemoryBlock data(cycle.data(), cycle.size() * sizeof(float)); // float32, little-endian (x64)
        juce::ValueTree w("Wave");
        w.setProperty("slot", i + 1, nullptr);
        w.setProperty("name", juce::String(uw.name(i)), nullptr);
        w.setProperty("data", data.toBase64Encoding(), nullptr);
        t.appendChild(w, nullptr);
    }
    return t;
}

void KMISynthProcessor::userWavesFromTree(const juce::ValueTree& t)
{
    auto& uw = engine_.userWaves();
    uw.clearAll();
    for (const auto& w : t)
    {
        const int slot = static_cast<int>(w.getProperty("slot", 0)) - 1;
        juce::MemoryBlock data;
        if (slot < 0 || !data.fromBase64Encoding(w.getProperty("data").toString())) continue;
        std::vector<float> cycle(data.getSize() / sizeof(float));
        std::memcpy(cycle.data(), data.getData(), cycle.size() * sizeof(float));
        uw.set(slot, cycle, w.getProperty("name").toString().toStdString());
    }
    ++userWaveVersion_;
}

juce::String KMISynthProcessor::loadUserWave(int slot, const juce::File& file, int& slotsUsed)
{
    slotsUsed = 0;
    if (slot < 0 || slot >= kmi::UserWavetables::kSlots) return "No such user slot.";
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader) return "Could not read " + file.getFileName() + " as audio.";
    constexpr juce::int64 kMaxSamples = 2048 * 256;
    const auto len = reader->lengthInSamples;
    if (len < 8 || len > kMaxSamples) return "Expected a single cycle (up to 8192 samples) or 2048-sample wavetable frames.";
    juce::AudioBuffer<float> buf(static_cast<int>(reader->numChannels), static_cast<int>(len));
    reader->read(&buf, 0, static_cast<int>(len), 0, true, true);
    std::vector<float> mono(static_cast<size_t>(len), 0.0f);
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int n = 0; n < static_cast<int>(len); ++n)
            mono[static_cast<size_t>(n)] += buf.getSample(ch, n) / static_cast<float>(buf.getNumChannels());

    const juce::String base = file.getFileNameWithoutExtension();
    const int frames = len % 2048 == 0 ? static_cast<int>(len / 2048) : 1;
    auto& uw = engine_.userWaves();
    if (frames <= 1)
    {
        if (len > 8192) return "Expected a single cycle (up to 8192 samples) or 2048-sample wavetable frames.";
        if (!uw.set(slot, mono, base.toStdString())) return file.getFileName() + " is silent.";
        slotsUsed = 1;
    }
    else
    {
        const int count = juce::jmin(frames, kmi::UserWavetables::kSlots - slot);
        for (int k = 0; k < count; ++k)
        {
            const int frame = count == 1 ? 0 : juce::roundToInt(static_cast<double>(k) * (frames - 1) / (count - 1));
            std::vector<float> cycle(mono.begin() + frame * 2048, mono.begin() + (frame + 1) * 2048);
            if (uw.set(slot + k, cycle, (base + " " + juce::String(frame + 1) + "/" + juce::String(frames)).toStdString()))
                ++slotsUsed;
        }
        if (slotsUsed == 0) return file.getFileName() + " is silent.";
    }
    ++userWaveVersion_;
    return {};
}

void KMISynthProcessor::clearUserWave(int slot)
{
    engine_.userWaves().clear(slot);
    ++userWaveVersion_;
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
    const auto waves = tree.getChildWithName(kUserWavesTag);
    userWavesFromTree(waves); // a preset without user waves clears them
    tree.removeChild(waves, nullptr);
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
    extra_.seqData = init.seqData;
    extra_.scalaTable = init.scalaTable;
    extra_.sclText = extra_.kbmText = extra_.scalaName = {};
    publishExtraState(extra_);
    userWavesFromTree({});
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
    extra_.seqData = p.seqData;
    extra_.scalaTable = p.scalaTable;
    extra_.sclText = extra_.kbmText = extra_.scalaName = {};
    publishExtraState(extra_);
    userWavesFromTree({}); // factory presets use only the built-in waves
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
