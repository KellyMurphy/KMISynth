#include "kmi/Arpeggiator.h"

#include <algorithm>
#include <cmath>

#include "kmi/Dsp.h"

namespace kmi {

namespace {
constexpr double kEps = 1e-9;

double divisionQuarters(int index) noexcept
{
    return kSyncDivisions[std::clamp(index, 0, kNumSyncDivisions - 1)].quarters;
}
} // namespace

void Arpeggiator::reset() noexcept
{
    physCount_ = 0;
    keyCount_ = 0;
    soundingCount_ = 0;
    running_ = false;
    latchedChord_ = false;
    gateOff_ = -1.0;
}

void Arpeggiator::stop(Sink& sink) noexcept
{
    releaseSounding(sink);
    running_ = false;
}

void Arpeggiator::addKey(int note, float velocity) noexcept
{
    for (int i = 0; i < keyCount_; ++i)
        if (keys_[static_cast<size_t>(i)].note == note)
        {
            keys_[static_cast<size_t>(i)].velocity = velocity;
            return;
        }
    if (keyCount_ < kMaxKeys) keys_[static_cast<size_t>(keyCount_++)] = { note, velocity };
}

void Arpeggiator::keyDown(int note, float velocity) noexcept
{
    if (note < 0 || note > 127) return;
    auto end = std::remove(phys_.begin(), phys_.begin() + physCount_, note);
    physCount_ = static_cast<int>(end - phys_.begin());
    if (physCount_ >= kMaxKeys) return;
    if (latchedChord_)
    {
        keyCount_ = 0; // a new chord after a full release replaces the latched one
        latchedChord_ = false;
    }
    phys_[static_cast<size_t>(physCount_)] = note;
    physVel_[static_cast<size_t>(physCount_)] = velocity;
    ++physCount_;
    addKey(note, velocity);
}

void Arpeggiator::keyUp(int note) noexcept
{
    for (int i = 0; i < physCount_; ++i)
    {
        if (phys_[static_cast<size_t>(i)] != note) continue;
        for (int j = i; j + 1 < physCount_; ++j)
        {
            phys_[static_cast<size_t>(j)] = phys_[static_cast<size_t>(j + 1)];
            physVel_[static_cast<size_t>(j)] = physVel_[static_cast<size_t>(j + 1)];
        }
        --physCount_;
        break;
    }
    if (params_.latch)
    {
        if (physCount_ == 0) latchedChord_ = true;
        return;
    }
    if (!pedal_) syncKeysToPhysical();
}

void Arpeggiator::setPedal(bool down) noexcept
{
    pedal_ = down;
    if (!down && !params_.latch) syncKeysToPhysical();
}

void Arpeggiator::syncKeysToPhysical() noexcept
{
    int out = 0;
    for (int i = 0; i < keyCount_; ++i)
    {
        const Key k = keys_[static_cast<size_t>(i)];
        if (std::find(phys_.begin(), phys_.begin() + physCount_, k.note) != phys_.begin() + physCount_)
            keys_[static_cast<size_t>(out++)] = k;
    }
    keyCount_ = out;
}

void Arpeggiator::beginBlock(const ArpParams& p, double bpm, double sampleRate, double transportPpq) noexcept
{
    const bool wasLatched = params_.latch;
    params_ = p;
    params_.octaves = std::clamp(p.octaves, 1, 4);
    params_.gate = std::clamp(p.gate, 0.01f, 1.0f);
    params_.swing = std::clamp(p.swing, 0.0f, 0.5f);
    if (wasLatched && !p.latch)
    {
        latchedChord_ = false;
        if (!pedal_) syncKeysToPhysical();
    }

    const double b = bpm > 1.0 ? bpm : 120.0;
    ppqPerSample_ = b / (60.0 * (sampleRate > 0.0 ? sampleRate : 48000.0));

    if (transportPpq >= 0.0)
    {
        const bool jumped = !synced_ || std::fabs(transportPpq - clock_) > 1e-3;
        clock_ = transportPpq;
        synced_ = true;
        if (jumped && running_)
        {
            nextStep_ = gridStep(clock_, false, stepIndex_);
            if (soundingCount_ > 0) gateOff_ = clock_; // release now; the new grid takes over
        }
    }
    else
    {
        synced_ = false;
    }
}

double Arpeggiator::stepLength(int64_t index) const noexcept
{
    const double q = divisionQuarters(params_.syncDiv);
    return (index & 1) ? q * (1.0 - params_.swing) : q * (1.0 + params_.swing);
}

double Arpeggiator::gridStep(double ppq, bool strict, int64_t& index) const noexcept
{
    const double q = divisionQuarters(params_.syncDiv);
    const double pairLen = 2.0 * q;
    const int64_t pair = static_cast<int64_t>(std::floor(ppq / pairLen));
    for (int64_t c = pair * 2; c <= pair * 2 + 2; ++c)
    {
        const int64_t pc = c >= 0 ? c / 2 : -((-c + 1) / 2);
        const double start = static_cast<double>(pc) * pairLen + ((c & 1) ? q * (1.0 + params_.swing) : 0.0);
        if (strict ? start > ppq + kEps : start >= ppq - kEps)
        {
            index = c;
            return start;
        }
    }
    index = pair * 2 + 4;
    return static_cast<double>(pair + 2) * pairLen;
}

float Arpeggiator::randomUnit() noexcept
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_ >> 8) * (1.0f / 16777216.0f);
}

int Arpeggiator::buildPattern() noexcept
{
    std::array<Key, kMaxKeys> base{};
    std::copy(keys_.begin(), keys_.begin() + keyCount_, base.begin());
    if (params_.mode != ArpMode::Played)
        std::sort(base.begin(), base.begin() + keyCount_, [](const Key& a, const Key& b) { return a.note < b.note; });

    int n = 0;
    for (int o = 0; o < params_.octaves; ++o)
        for (int i = 0; i < keyCount_; ++i)
        {
            const int note = base[static_cast<size_t>(i)].note + 12 * o;
            if (note <= 127) pattern_[static_cast<size_t>(n++)] = { note, base[static_cast<size_t>(i)].velocity };
        }

    switch (params_.mode)
    {
        case ArpMode::Down: std::reverse(pattern_.begin(), pattern_.begin() + n); break;
        case ArpMode::UpDown:
        case ArpMode::DownUp:
        {
            if (params_.mode == ArpMode::DownUp) std::reverse(pattern_.begin(), pattern_.begin() + n);
            // Return leg without repeating either end: 1 2 3 4 -> 1 2 3 4 3 2.
            for (int i = n - 2; i >= 1; --i) pattern_[static_cast<size_t>(n + (n - 2 - i))] = pattern_[static_cast<size_t>(i)];
            n += std::max(0, n - 2);
            break;
        }
        default: break;
    }
    return n;
}

void Arpeggiator::releaseSounding(Sink& sink) noexcept
{
    for (int i = 0; i < soundingCount_; ++i) sink.arpNoteOff(sounding_[static_cast<size_t>(i)]);
    soundingCount_ = 0;
    gateOff_ = -1.0;
}

void Arpeggiator::fireStep(Sink& sink) noexcept
{
    releaseSounding(sink);
    stepStart_ = nextStep_;
    if (keyCount_ == 0)
    {
        running_ = false;
        return;
    }

    const auto velocityOf = [&](const Key& k) {
        return params_.velocity > 0 ? static_cast<float>(std::min(params_.velocity, 127)) / 127.0f : k.velocity;
    };
    const auto play = [&](const Key& k) {
        if (soundingCount_ >= kMaxChord) return;
        for (int i = 0; i < soundingCount_; ++i)
            if (sounding_[static_cast<size_t>(i)] == k.note) return;
        sink.arpNoteOn(k.note, velocityOf(k));
        sounding_[static_cast<size_t>(soundingCount_++)] = k.note;
    };

    if (params_.mode == ArpMode::Chord)
    {
        const int octave = static_cast<int>(position_ % params_.octaves);
        for (int i = 0; i < keyCount_; ++i)
        {
            Key k = keys_[static_cast<size_t>(i)];
            k.note += 12 * octave;
            if (k.note <= 127) play(k);
        }
    }
    else
    {
        const int n = buildPattern();
        if (n > 0)
        {
            int idx = static_cast<int>(position_ % n);
            if (params_.mode == ArpMode::Random)
            {
                idx = std::min(n - 1, static_cast<int>(randomUnit() * static_cast<float>(n)));
                if (n > 1 && soundingLast_ == pattern_[static_cast<size_t>(idx)].note) // avoid immediate repeats
                    idx = (idx + 1 + static_cast<int>(randomUnit() * static_cast<float>(n - 1))) % n;
            }
            play(pattern_[static_cast<size_t>(idx)]);
            soundingLast_ = pattern_[static_cast<size_t>(idx)].note;
        }
    }

    const double len = stepLength(stepIndex_);
    gateOff_ = stepStart_ + len * params_.gate;
    ++position_;
    if (synced_)
    {
        nextStep_ = gridStep(stepStart_, true, stepIndex_);
    }
    else
    {
        nextStep_ = stepStart_ + len;
        ++stepIndex_;
    }
}

int Arpeggiator::process(Sink& sink, int maxSamples) noexcept
{
    for (int guard = 0; guard < 8; ++guard)
    {
        bool fired = false;
        if (soundingCount_ > 0 && gateOff_ >= 0.0 && gateOff_ <= clock_ + kEps)
        {
            releaseSounding(sink);
            fired = true;
        }
        if (!running_ && keyCount_ > 0)
        {
            running_ = true;
            position_ = 0;
            if (synced_)
            {
                // Start on the grid. A key pressed just after a step boundary still catches that step.
                const double tolerance = 0.15 * divisionQuarters(params_.syncDiv);
                nextStep_ = gridStep(clock_ - tolerance, false, stepIndex_);
                if (nextStep_ < clock_) nextStep_ = clock_;
            }
            else
            {
                clock_ = 0.0;
                nextStep_ = 0.0;
                stepIndex_ = 0;
            }
        }
        if (running_ && nextStep_ <= clock_ + kEps)
        {
            fireStep(sink);
            fired = true;
        }
        if (!fired) break;
    }

    double next = 1e300;
    if (soundingCount_ > 0 && gateOff_ >= 0.0) next = std::min(next, gateOff_);
    if (running_) next = std::min(next, nextStep_);
    if (next >= 1e299) return maxSamples;
    const double samples = (next - clock_) / ppqPerSample_;
    if (samples >= static_cast<double>(maxSamples)) return maxSamples;
    return std::clamp(static_cast<int>(std::ceil(samples - 1e-6)), 1, maxSamples);
}

} // namespace kmi
