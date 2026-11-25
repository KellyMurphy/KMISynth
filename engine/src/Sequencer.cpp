#include "kmi/Sequencer.h"

#include <algorithm>
#include <cmath>

#include "kmi/Dsp.h"

namespace kmi {

namespace {
constexpr double kEps = 1e-9;
} // namespace

void Sequencer::reset() noexcept
{
    soundingCount_ = 0;
    keyCount_ = 0;
    running_ = false;
    laneActive_.fill(false);
    for (auto& t : tracks_) t = Track{};
}

void Sequencer::stop(Sink& sink) noexcept
{
    while (soundingCount_ > 0) noteOff(sink, soundingCount_ - 1);
    running_ = false;
    laneActive_.fill(false);
    for (auto& t : tracks_)
    {
        t.hitCount = 0;
        t.pos = -1;
    }
}

void Sequencer::keyDown(int note, float /*velocity*/) noexcept
{
    keyUp(note);
    if (keyCount_ < kMaxKeys) keys_[static_cast<size_t>(keyCount_++)] = note;
}

void Sequencer::keyUp(int note) noexcept
{
    auto end = std::remove(keys_.begin(), keys_.begin() + keyCount_, note);
    keyCount_ = static_cast<int>(end - keys_.begin());
}

float Sequencer::randomUnit() noexcept
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_ >> 8) * (1.0f / 16777216.0f);
}

double Sequencer::stepQuarters(int track) const noexcept
{
    const int div = std::clamp(params_.track[static_cast<size_t>(track)].syncDiv, 0, kNumSyncDivisions - 1);
    return kSyncDivisions[div].quarters;
}

int Sequencer::mapPosition(int track, int64_t k) noexcept
{
    const SeqTrackParams& tp = params_.track[static_cast<size_t>(track)];
    const int len = std::clamp(tp.length, 1, kSeqSteps);
    switch (tp.mode)
    {
        case SeqPlayMode::Backward: return len - 1 - static_cast<int>(k % len);
        case SeqPlayMode::PingPong:
        {
            if (len == 1) return 0;
            const int period = 2 * len - 2;
            const auto z = static_cast<int>(k % period);
            return z < len ? z : period - z;
        }
        case SeqPlayMode::Random: return std::min(len - 1, static_cast<int>(randomUnit() * static_cast<float>(len)));
        default: return static_cast<int>(k % len);
    }
}

void Sequencer::beginBlock(const SeqParams& p, const SeqData& data, double bpm, double sampleRate, double transportPpq) noexcept
{
    params_ = p;
    data_ = &data;
    hostMode_ = p.trigger == SeqTrigger::Host;
    const double b = bpm > 1.0 ? bpm : 120.0;
    ppqPerSample_ = b / (60.0 * (sampleRate > 0.0 ? sampleRate : 48000.0));
    const bool playing = transportPpq >= 0.0;

    if (playing)
    {
        const bool jumped = !transportRunning_ || std::fabs(transportPpq - clock_) > 1e-3;
        clock_ = transportPpq;
        transportRunning_ = true;
        if (jumped && running_) startTracks(); // relocate to the host grid
    }
    else
    {
        transportRunning_ = false;
    }
}

void Sequencer::startTracks() noexcept
{
    for (int t = 0; t < kSeqTracks; ++t)
    {
        Track& tr = tracks_[static_cast<size_t>(t)];
        const double q = stepQuarters(t);
        tr.hitCount = 0;
        if (transportRunning_)
        {
            // On the host grid: the step at (or just before, within 15%) the current position.
            const double k = std::floor((clock_ + 0.15 * q) / q);
            tr.index = static_cast<int64_t>(k);
            tr.nextStep = std::max(k * q, clock_);
        }
        else
        {
            tr.index = 0;
            tr.nextStep = clock_;
        }
        tr.pos = -1;
    }
}

void Sequencer::noteOff(Sink& sink, int index) noexcept
{
    sink.seqNoteOff(sounding_[static_cast<size_t>(index)].note);
    sounding_[static_cast<size_t>(index)] = sounding_[static_cast<size_t>(--soundingCount_)];
}

void Sequencer::play(Sink& sink, int track, const Hit& h) noexcept
{
    const SeqStep& st = data_->steps[static_cast<size_t>(track)][static_cast<size_t>(h.step)];
    const double q = stepQuarters(track);
    const int keyShift = !hostMode_ && keyCount_ > 0 ? keys_[static_cast<size_t>(keyCount_ - 1)] - 60 : 0;
    const int shift = params_.track[static_cast<size_t>(track)].transpose + keyShift;
    for (int n = 0; n < std::min<int>(st.count, kSeqNotesPerStep); ++n)
    {
        const SeqNote& sn = st.notes[static_cast<size_t>(n)];
        const int note = static_cast<int>(sn.note) + shift;
        if (note < 0 || note > 127) continue;
        for (int i = 0; i < soundingCount_; ++i) // a note played again is released first
            if (sounding_[static_cast<size_t>(i)].note == note)
            {
                noteOff(sink, i);
                break;
            }
        if (soundingCount_ >= kMaxSounding) return;
        const float vel = std::clamp(static_cast<float>(sn.velocity) / 127.0f * h.velScale, 0.01f, 1.0f);
        sink.seqNoteOn(note, vel);
        const double length = std::min(std::max(1, static_cast<int>(sn.length)) * 0.25 * q, h.noteCap);
        sounding_[static_cast<size_t>(soundingCount_++)] = { note, h.time + length };
    }
}

void Sequencer::fireStep(int track) noexcept
{
    Track& tr = tracks_[static_cast<size_t>(track)];
    const double q = stepQuarters(track);
    const double start = tr.nextStep;
    tr.pos = mapPosition(track, tr.index);
    ++tr.index;
    tr.nextStep = transportRunning_ ? static_cast<double>(tr.index) * q : start + q;

    if (track == kSeqNoteTracks) // macro track: set the lanes
    {
        for (int l = 0; l < kSeqLanes; ++l)
        {
            laneActive_[static_cast<size_t>(l)] = data_->laneOn[static_cast<size_t>(l)];
            laneValue_[static_cast<size_t>(l)] = std::clamp(data_->lanes[static_cast<size_t>(l)][static_cast<size_t>(tr.pos)], 0.0f, 1.0f);
        }
        return;
    }

    const SeqStep& st = data_->steps[static_cast<size_t>(track)][static_cast<size_t>(tr.pos)];
    if (st.count == 0) return;
    if (st.probability < 100 && randomUnit() * 100.0f >= static_cast<float>(st.probability)) return;
    const float drift = static_cast<float>(std::min<int>(st.drift, 100)) / 100.0f;
    const double late = drift > 0.0f ? randomUnit() * drift * 0.3 * q : 0.0;
    const float velScale = 1.0f + (randomUnit() * 2.0f - 1.0f) * drift * 0.25f;
    const int ratchet = std::clamp<int>(st.ratchet, 1, 4);
    const double slot = q / ratchet;
    tr.hitCount = 0;
    for (int r = 0; r < ratchet; ++r)
        tr.hits[static_cast<size_t>(tr.hitCount++)] = { start + late + r * slot, tr.pos, velScale,
                                                        ratchet > 1 ? slot * 0.9 : 1e9 };
}

int Sequencer::process(Sink& sink, int maxSamples) noexcept
{
    if (!data_) return maxSamples;
    // Running: Host trigger follows the transport; Key trigger runs while a key is held.
    const bool shouldRun = params_.enabled && (hostMode_ ? transportRunning_ : keyCount_ > 0);
    if (shouldRun && !running_)
    {
        running_ = true;
        if (!transportRunning_) clock_ = 0.0; // free-running from the first key
        startTracks();
    }
    else if (!shouldRun && running_)
    {
        running_ = false; // sounding notes finish their lengths
        laneActive_.fill(false);
        for (auto& t : tracks_)
        {
            t.hitCount = 0;
            t.pos = -1;
        }
    }

    for (int guard = 0; guard < 64; ++guard)
    {
        bool fired = false;
        for (int i = soundingCount_ - 1; i >= 0; --i)
            if (sounding_[static_cast<size_t>(i)].off <= clock_ + kEps)
            {
                noteOff(sink, i);
                fired = true;
            }
        if (running_)
            for (int t = 0; t < kSeqTracks; ++t)
            {
                Track& tr = tracks_[static_cast<size_t>(t)];
                for (int h = 0; h < tr.hitCount;)
                    if (tr.hits[static_cast<size_t>(h)].time <= clock_ + kEps)
                    {
                        play(sink, t, tr.hits[static_cast<size_t>(h)]);
                        tr.hits[static_cast<size_t>(h)] = tr.hits[static_cast<size_t>(--tr.hitCount)];
                        fired = true;
                    }
                    else
                        ++h;
                if (tr.nextStep <= clock_ + kEps)
                {
                    if (params_.track[static_cast<size_t>(t)].enabled) fireStep(t);
                    else
                    {
                        tr.pos = mapPosition(t, tr.index++);
                        tr.nextStep = transportRunning_ ? static_cast<double>(tr.index) * stepQuarters(t) : tr.nextStep + stepQuarters(t);
                        if (t == kSeqNoteTracks) laneActive_.fill(false);
                    }
                    fired = true;
                }
            }
        if (!fired) break;
    }

    double next = 1e300;
    for (int i = 0; i < soundingCount_; ++i) next = std::min(next, sounding_[static_cast<size_t>(i)].off);
    if (running_)
        for (const Track& tr : tracks_)
        {
            next = std::min(next, tr.nextStep);
            for (int h = 0; h < tr.hitCount; ++h) next = std::min(next, tr.hits[static_cast<size_t>(h)].time);
        }
    if (next >= 1e299) return maxSamples;
    const double samples = (next - clock_) / ppqPerSample_;
    if (samples >= static_cast<double>(maxSamples)) return maxSamples;
    return std::clamp(static_cast<int>(std::ceil(samples - 1e-6)), 1, maxSamples);
}

} // namespace kmi
