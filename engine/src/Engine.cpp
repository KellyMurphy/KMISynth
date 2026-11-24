#include "kmi/Engine.h"

#include <algorithm>
#include <cmath>

#include "kmi/Dsp.h"
#include "kmi/Wavetables.h"

namespace kmi {

namespace {
LfoWave vibratoToLfoWave(VibratoWave w) noexcept
{
    switch (w)
    {
        case VibratoWave::Triangle: return LfoWave::Triangle;
        case VibratoWave::Square: return LfoWave::Square;
        case VibratoWave::SampleHold: return LfoWave::SampleHold;
        case VibratoWave::Noise: return LfoWave::Noise;
        default: return LfoWave::Sine;
    }
}

bool isUnisonMode(PolyMode m) noexcept
{
    return m == PolyMode::Unison || m == PolyMode::UnisonLo || m == PolyMode::UnisonHi;
}
} // namespace

Engine::Engine()
{
    patch_ = makeInitPatch();
    for (int i = 0; i < kMaxVoices; ++i) voices_[static_cast<size_t>(i)].init(i, 0x1234567u + static_cast<uint32_t>(i) * 7919u);
    for (int k = 0; k < kNumLfos; ++k) sharedLfo_[static_cast<size_t>(k)].reset(0xBADC0DEu + static_cast<uint32_t>(k));
    vibratoLfo_.reset(0xC0FFEEu);
}

void Engine::prepare(double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    (void)WavetableBank::instance(); // build tables outside the audio callback
    fx_.prepare(sampleRate_);        // allocates delay lines
    osFactor_ = 0;
    configureOversampling(patch_.oversampling);
    reset();
}

void Engine::configureOversampling(int factor) noexcept
{
    const int f = factor == 1 ? 1 : 2;
    if (f == osFactor_) return;
    osFactor_ = f;
    const float rate = static_cast<float>(sampleRate_ * f);
    for (auto& v : voices_) v.prepare(rate);
    decL_.reset();
    decR_.reset();
}

void Engine::reset() noexcept
{
    for (auto& v : voices_) v.kill();
    for (auto& b : banks_) b.reset();
    arp_.reset();
    arp_.setPedal(false);
    heldCount_ = 0;
    monoNote_ = -1;
    lastPitch_ = -1.0f;
    mod_ = ModState{};
    chBend_.fill(0.0f);
    chPressure_.fill(0.0f);
    chTimbre_.fill(0.0f);
    keyChannel_.fill(0);
    decL_.reset();
    decR_.reset();
    fx_.reset();
    patchGain_ = patch_.vca.patchLevel;
}

RenderContext Engine::makeContext() const noexcept
{
    RenderContext ctx;
    ctx.patch = &patch_;
    ctx.sampleRate = static_cast<float>(sampleRate_ * osFactor_);
    ctx.bpm = bpm_;
    ctx.algorithm = &algo_;
    ctx.mod = &mod_;
    ctx.otherNotesHeld = heldCount_ > 1;
    return ctx;
}

bool Engine::isMonoMode() const noexcept
{
    const PolyMode m = patch_.voice.polyMode;
    return m == PolyMode::Mono || m == PolyMode::MonoLo || m == PolyMode::MonoHi || isUnisonMode(m);
}

int Engine::monoPriorityNote() const noexcept
{
    if (heldCount_ == 0) return -1;
    const PolyMode m = patch_.voice.polyMode;
    if (m == PolyMode::MonoLo || m == PolyMode::UnisonLo)
        return *std::min_element(held_.begin(), held_.begin() + heldCount_);
    if (m == PolyMode::MonoHi || m == PolyMode::UnisonHi)
        return *std::max_element(held_.begin(), held_.begin() + heldCount_);
    return held_[static_cast<size_t>(heldCount_ - 1)]; // last note priority
}

void Engine::retriggerSharedLfos() noexcept
{
    const float rate = static_cast<float>(sampleRate_ * osFactor_);
    for (int k = 0; k < kNumLfos; ++k)
    {
        const LfoParams& lp = patch_.lfo[static_cast<size_t>(k)];
        if (lp.trig != LfoTrig::Single) continue;
        if (lp.oneShot == LfoOneShot::Step) sharedLfo_[static_cast<size_t>(k)].advanceStep();
        else sharedLfo_[static_cast<size_t>(k)].retrigger(lp, rate);
    }
}

void Engine::applyExpression(Voice& v) noexcept
{
    const auto ch = static_cast<size_t>(v.channel);
    v.setNoteBend(chBend_[ch]);
    v.setPolyAT(chPressure_[ch]);
    v.setTimbre(chTimbre_[ch]);
}

void Engine::launch(Voice& v, int note, float velocity, float spread) noexcept
{
    const float glideFrom = v.isActive() ? v.currentPitch() : lastPitch_;
    v.age = ++ageCounter_;
    v.channel = keyChannel_[static_cast<size_t>(note & 127)];
    applyExpression(v);
    if (v.isActive()) v.steal(note, velocity, spread, glideFrom);
    else v.start(makeContext(), note, velocity, spread, glideFrom);
}

Voice& Engine::pickPolyVoice(int note) noexcept
{
    const int limit = std::clamp(patch_.voice.voiceLimit, 1, kMaxVoices);
    const PolyMode mode = patch_.voice.polyMode;

    if (mode == PolyMode::PolyReassign)
    {
        for (int i = 0; i < limit; ++i)
        {
            Voice& v = voices_[static_cast<size_t>(i)];
            if (v.isActive() && v.note() == note) return v; // same note reuses its voice
        }
        for (int i = 0; i < limit; ++i)
            if (!voices_[static_cast<size_t>(i)].isActive()) return voices_[static_cast<size_t>(i)];
    }
    else
    {
        // Rotate: next free voice in rotation order, then next released one.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int k = 0; k < limit; ++k)
            {
                const int i = (rotateNext_ + k) % limit;
                Voice& v = voices_[static_cast<size_t>(i)];
                const bool ok = pass == 0 ? !v.isActive() : !v.isGateOn();
                if (ok)
                {
                    rotateNext_ = (i + 1) % limit;
                    return v;
                }
            }
        }
    }

    // Steal: quietest released voice, otherwise the oldest.
    Voice* best = nullptr;
    for (int i = 0; i < limit; ++i)
    {
        Voice& v = voices_[static_cast<size_t>(i)];
        if (!v.isGateOn() && (!best || v.loudness() < best->loudness())) best = &v;
    }
    if (best) return *best;
    best = &voices_[0];
    for (int i = 1; i < limit; ++i)
        if (voices_[static_cast<size_t>(i)].age < best->age) best = &voices_[static_cast<size_t>(i)];
    return *best;
}

void Engine::startPoly(int note, float velocity) noexcept
{
    int count = 1;
    if (patch_.voice.polyMode == PolyMode::UnisonPoly)
        count = std::max(1, std::clamp(patch_.voice.density, 1, kMaxVoices) / std::max(1, heldCount_));
    for (int u = 0; u < count; ++u)
    {
        const float spread = count > 1 ? 2.0f * static_cast<float>(u) / static_cast<float>(count - 1) - 1.0f : 0.0f;
        Voice& v = (count > 1) ? [&]() -> Voice& {
            // Unison Poly: never reuse a voice already given to this note in this call.
            for (int pass = 0; pass < 2; ++pass)
                for (auto& cand : voices_)
                    if (pass == 0 ? !cand.isActive() : !cand.isGateOn()) return cand;
            return pickPolyVoice(note);
        }()
                               : pickPolyVoice(note);
        launch(v, note, velocity, spread);
    }
}

void Engine::startMono(int note, float velocity, bool legato) noexcept
{
    const PolyMode m = patch_.voice.polyMode;
    const int count = isUnisonMode(m) ? std::clamp(patch_.voice.density, 1, kMaxVoices) : 1;
    const RenderContext ctx = makeContext();
    for (int u = 0; u < count; ++u)
    {
        Voice& v = voices_[static_cast<size_t>(u)];
        const float spread = count > 1 ? 2.0f * static_cast<float>(u) / static_cast<float>(count - 1) - 1.0f : 0.0f;
        if (legato && v.isActive() && v.isGateOn() && !v.isStealing())
        {
            v.channel = keyChannel_[static_cast<size_t>(note & 127)];
            applyExpression(v);
            v.legatoTo(ctx, note, velocity);
        }
        else launch(v, note, velocity, spread);
    }
    for (int u = count; u < kMaxVoices; ++u)
        if (voices_[static_cast<size_t>(u)].isGateOn()) voices_[static_cast<size_t>(u)].release(0.0f);
    monoNote_ = note;
}

void Engine::noteOn(int note, float velocity, int channel) noexcept
{
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f)
    {
        noteOff(note, 0.0f, channel);
        return;
    }
    keyChannel_[static_cast<size_t>(note)] = static_cast<uint8_t>(std::clamp(channel, 0, 16));
    arp_.keyDown(note, velocity); // tracked always, so switching the arp on mid-chord picks the keys up
    if (!arpActive_) voiceNoteOn(note, velocity);
}

void Engine::noteOff(int note, float releaseVelocity, int channel) noexcept
{
    if (note < 0 || note > 127) return;
    // MPE: the same key may be down on two channels; only the channel that owns it releases it.
    const int owner = keyChannel_[static_cast<size_t>(note)];
    if (channel > 0 && owner > 0 && owner != channel) return;
    arp_.keyUp(note);
    if (!arpActive_) voiceNoteOff(note, releaseVelocity, true);
}

void Engine::updateArpEnabled() noexcept
{
    if (patch_.arp.enabled == arpActive_) return;
    if (patch_.arp.enabled)
    {
        // The arp takes over the held keys: release what the keyboard was playing directly.
        for (auto& v : voices_)
            if (v.isGateOn()) v.release(0.0f);
        heldCount_ = 0;
        monoNote_ = -1;
        arpActive_ = true;
    }
    else
    {
        arp_.stop(*this);
        arpActive_ = false;
        for (int i = 0; i < arp_.physicalKeyCount(); ++i) voiceNoteOn(arp_.physicalKey(i), arp_.physicalVelocity(i));
    }
}

void Engine::voiceNoteOn(int note, float velocity) noexcept
{
    // Maintain the held-key list (press order).
    auto end = std::remove(held_.begin(), held_.begin() + heldCount_, note);
    heldCount_ = static_cast<int>(end - held_.begin());
    if (heldCount_ < 128) held_[static_cast<size_t>(heldCount_++)] = note;
    keyVelocity_[static_cast<size_t>(note)] = velocity;

    retriggerSharedLfos();
    fx_.noteOn();

    if (isMonoMode())
    {
        const int prio = monoPriorityNote();
        const bool sounding = voices_[0].isActive() && voices_[0].isGateOn();
        if (prio != monoNote_ || !sounding)
            startMono(prio, keyVelocity_[static_cast<size_t>(prio)], sounding && monoNote_ >= 0);
    }
    else
    {
        startPoly(note, velocity);
    }
    lastPitch_ = static_cast<float>(note);
}

void Engine::voiceNoteOff(int note, float releaseVelocity, bool honourPedal) noexcept
{
    auto end = std::remove(held_.begin(), held_.begin() + heldCount_, note);
    heldCount_ = static_cast<int>(end - held_.begin());
    const bool pedal = honourPedal && mod_.sustain >= 0.5f;

    if (isMonoMode())
    {
        if (heldCount_ == 0)
        {
            for (auto& v : voices_)
            {
                if (!v.isGateOn()) continue;
                if (pedal) v.sustained = true;
                else v.release(releaseVelocity);
            }
            if (!pedal) monoNote_ = -1;
        }
        else
        {
            const int prio = monoPriorityNote();
            if (prio != monoNote_) startMono(prio, keyVelocity_[static_cast<size_t>(prio)], true);
        }
        return;
    }

    for (auto& v : voices_)
    {
        if (!v.isActive() || !v.isGateOn() || v.note() != note || (honourPedal && v.sustained)) continue;
        if (pedal) v.sustained = true;
        else v.release(releaseVelocity);
    }
}

void Engine::releaseSustained() noexcept
{
    for (auto& v : voices_)
        if (v.sustained) v.release(0.0f);
    if (isMonoMode() && heldCount_ == 0) monoNote_ = -1;
}

void Engine::polyAftertouch(int note, float value) noexcept
{
    for (auto& v : voices_)
        if (v.isActive() && v.note() == note) v.setPolyAT(value);
}

void Engine::channelPitchBend(int channel, float bipolar) noexcept
{
    if (channel < 1 || channel > 16) return;
    chBend_[static_cast<size_t>(channel)] = bipolar;
    for (auto& v : voices_)
        if (v.isActive() && v.channel == channel) v.setNoteBend(bipolar);
}

void Engine::channelPressure(int channel, float value) noexcept
{
    if (channel < 1 || channel > 16) return;
    chPressure_[static_cast<size_t>(channel)] = value;
    for (auto& v : voices_)
        if (v.isActive() && v.channel == channel) v.setPolyAT(value);
}

void Engine::channelTimbre(int channel, float value01) noexcept
{
    if (channel < 1 || channel > 16) return;
    chTimbre_[static_cast<size_t>(channel)] = value01;
    for (auto& v : voices_)
        if (v.isActive() && v.channel == channel) v.setTimbre(value01);
}

void Engine::controlChange(int cc, float value01) noexcept
{
    if (cc < 0 || cc > 127) return;
    mod_.cc[cc] = value01;
    switch (cc)
    {
        case 1: mod_.modWheel = value01; break;
        case 11: mod_.expression = value01; break;
        case 74: // outside MPE, CC74 drives the Timbre source of every note
            chTimbre_[0] = value01;
            for (auto& v : voices_)
                if (v.channel == 0) v.setTimbre(value01);
            break;
        case 64:
        {
            const bool wasDown = mod_.sustain >= 0.5f;
            mod_.sustain = value01;
            arp_.setPedal(value01 >= 0.5f);
            if (wasDown && value01 < 0.5f) releaseSustained();
            break;
        }
        case 120: allSoundOff(); break;
        case 123: allNotesOff(); break;
        default: break;
    }
}

void Engine::allNotesOff() noexcept
{
    arp_.reset();
    heldCount_ = 0;
    monoNote_ = -1;
    for (auto& v : voices_)
        if (v.isGateOn()) v.release(0.0f);
}

void Engine::allSoundOff() noexcept
{
    arp_.reset();
    heldCount_ = 0;
    monoNote_ = -1;
    for (auto& v : voices_) v.kill();
    decL_.reset();
    decR_.reset();
    fx_.reset();
}

int Engine::activeVoiceCount() const noexcept
{
    int n = 0;
    for (const auto& v : voices_) n += v.isActive() ? 1 : 0;
    return n;
}

void Engine::render(float* left, float* right, int numSamples) noexcept
{
    // Flush denormals to zero for the duration of the render (R-RT-2), whatever the caller set.
    const DenormalGuard guard;
    if (patch_.oversampling != osFactor_) configureOversampling(patch_.oversampling);
    updateArpEnabled();
    if (arpActive_) arp_.beginBlock(patch_.arp, bpm_, sampleRate_, transportPpq_);
    int done = 0;
    while (done < numSamples)
    {
        int n = std::min(kMaxChunk, numSamples - done);
        if (arpActive_) n = arp_.process(*this, n); // split the block at arp steps and gate ends
        renderChunk(left + done, right + done, n);
        if (arpActive_) arp_.advance(n);
        done += n;
    }
}

void Engine::renderChunk(float* left, float* right, int n) noexcept
{
    const int os = osFactor_;
    const int nOs = n * os;
    const float osRate = static_cast<float>(sampleRate_ * os);

    std::fill(osL_, osL_ + nOs, 0.0f);
    std::fill(osR_, osR_ + nOs, 0.0f);

    resolveAlgorithm(patch_, 0.0f, algo_);
    RenderContext ctx = makeContext();
    ctx.usage = analyzeModUsage(patch_);

    for (int k = 0; k < kNumLfos; ++k)
    {
        const LfoParams& lp = patch_.lfo[static_cast<size_t>(k)];
        Lfo& l = sharedLfo_[static_cast<size_t>(k)];
        ctx.globalLfoState[k] = &l;
        if (lp.trig == LfoTrig::Poly || !ctx.usage.lfo[k])
        {
            ctx.globalLfo[k] = nullptr;
            continue;
        }
        l.update(lp, patch_.lfoSteps[static_cast<size_t>(k)].data(), 1.0f, 0.0f, osRate, bpm_);
        ctx.globalLfoPhaseStart[k] = l.phase();
        for (int s = 0; s < nOs; ++s) sharedLfoBuf_[k][s] = l.tick();
        ctx.globalLfo[k] = sharedLfoBuf_[k];
    }

    {
        const VoiceParams& vp = patch_.voice;
        LfoParams vl;
        vl.wave = vibratoToLfoWave(vp.vibratoWave);
        vl.rateHz = vp.vibratoRateHz;
        vl.bpmSync = vp.vibratoBpmSync;
        vl.syncDiv = vp.vibratoSyncDiv;
        vl.trig = LfoTrig::Free;
        vibratoLfo_.update(vl, nullptr, 1.0f, 0.0f, osRate, bpm_);
        for (int s = 0; s < nOs; ++s) vibratoBuf_[s] = vibratoLfo_.tick();
        ctx.vibrato = vibratoBuf_;
    }

    // Voices: per control block, each active voice renders oscillators/envelopes/VCA gain; the filter
    // chains of four voices then run together in one SIMD bank.
    for (int off = 0; off < nOs; off += Voice::kControlBlock)
    {
        const int len = std::min(Voice::kControlBlock, nOs - off);
        for (int b = 0; b < kNumBanks; ++b)
        {
            float* pre = bankPre_[b];
            float* gl = bankGainL_[b];
            float* gr = bankGainR_[b];
            const FilterLaneParams* lanes[4] = {};
            bool any = false;
            for (int lane = 0; lane < 4; ++lane)
            {
                Voice& v = voices_[static_cast<size_t>(b * 4 + lane)];
                if (!v.isActive()) continue;
                if (!any)
                {
                    std::fill(pre, pre + len * 4, 0.0f);
                    std::fill(gl, gl + len * 4, 0.0f);
                    std::fill(gr, gr + len * 4, 0.0f);
                    any = true;
                }
                v.renderBlock(ctx, off, len, pre + lane, gl + lane, gr + lane, 4);
                FilterLaneParams& lp = v.filterLane();
                if (lp.resetState)
                {
                    banks_[b].resetLane(lane);
                    lp.resetState = false;
                }
                lanes[lane] = &lp;
            }
            if (!any) continue;

            FilterBank4& bank = banks_[b];
            bank.prepareBlock(patch_.dfilt.type, patch_.dfilt.drivePost, lanes, len);
            for (int s = 0; s < len; ++s)
            {
                const F4 y = bank.process(F4::load(pre + 4 * s));
                osL_[off + s] += (y * F4::load(gl + 4 * s)).sum();
                osR_[off + s] += (y * F4::load(gr + 4 * s)).sum();
            }
        }
    }

    if (os == 2)
    {
        decL_.process(osL_, left, n);
        decR_.process(osR_, right, n);
    }
    else
    {
        std::copy(osL_, osL_ + n, left);
        std::copy(osR_, osR_ + n, right);
    }

    // Effects. Their mod-matrix destinations follow the most recently started voice.
    const float* fxMods = nullptr;
    uint64_t newest = 0;
    for (const auto& v : voices_)
        if (v.isActive() && v.age >= newest)
        {
            newest = v.age;
            fxMods = v.modDestinations();
        }
    fx_.process(left, right, n, patch_.fx, fxMods, bpm_, transportPpq_);
    if (transportPpq_ >= 0.0) transportPpq_ += static_cast<double>(n) * bpm_ / (60.0 * sampleRate_);

    // Patch level, ramped across the chunk.
    const float target = std::max(0.0f, patch_.vca.patchLevel);
    const float step = (target - patchGain_) / static_cast<float>(n);
    for (int s = 0; s < n; ++s)
    {
        patchGain_ += step;
        left[s] *= patchGain_;
        right[s] *= patchGain_;
    }
    patchGain_ = target;
}

} // namespace kmi
