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

// ---- Phrases ----------------------------------------------------------------------------------------
// Original phrases, written as steps over the held chord (sorted low to high). Tokens, space separated:
//   0-9    chord tone (wraps upward by octaves past the chord size); join tones with '&' for a chord
//   + / -  octave up / down (repeatable), as a suffix
//   !      accent (louder)
//   .      rest           _  tie (the previous step's notes keep sounding)
struct PhraseDef
{
    const char* name;
    const char* steps;
};
constexpr PhraseDef kPhraseDefs[kNumArpPhrases] = {
    { "Climb", "0 1 2 3" },
    { "Fall", "3 2 1 0" },
    { "Bounce", "0 2 1 3" },
    { "Pedal Root", "0 1 0 2 0 3 0 2" },
    { "Gallop", "0 0 1 . 0 0 2 ." },
    { "Skip", "0 2 4 1 3 5" },
    { "Octave Hop", "0 0+ 1 1+ 2 2+ 1 1+" },
    { "Pendulum", "0 1 2 3 2 1" },
    { "Waltz", "0- 1&2 1&2" },
    { "Offbeat", ". 0 . 1 . 2 . 1" },
    { "Stutter", "0 0 0 1 2 2 2 3" },
    { "Accent Run", "0! 1 2 3 1! 2 3 0+" },
    { "Long Short", "0 _ 1 2 _ 3" },
    { "Stabs", "0&1&2! . . 0&1&2 . 0&1&2 . ." },
    { "Bass Pedal", "0- 1 0- 2 0- 3 0- 2" },
    { "Spiral", "0 1 2 0+ 1+ 2+ 0++ 1+" },
    { "Tumble", "3+ 2+ 1+ 0+ 3 2 1 0" },
    { "Ping", "0 3 0 3 1 3 1 3" },
    { "Shuffle", "0 _ 1 2 _ 1 3 _ 2 1 _ 2" },
    { "Clave", "0! . . 1 . . 2 . . . 1 . 2 . . ." },
    { "Tresillo", "0! . . 1 . . 2 ." },
    { "Heartbeat", "0! 0 . . . . . . 1! 1 . . . . . ." },
    { "Arch", "0 1 2 3 4 3 2 1" },
    { "Valley", "4 3 2 1 0 1 2 3" },
    { "Zigzag", "0 2 1 3 2 4 3 5" },
    { "Fifths", "0 2 0+ 2+ 0++ 2+ 0+ 2" },
    { "Broken Chord", "0 2 1 2 0 2 1 2" },
    { "Alberti", "0 2 1 2" },
    { "Drone Top", "0 3 1 3 2 3 1 3" },
    { "Call", "0 1 2 _ _ . . ." },
    { "Answer", ". . . . 2 1 0 _" },
    { "Question", "0 1 2 3 3+ _ _ ." },
    { "Echo", "0! . 0 . 1! . 1 . 2! . 2 . 1! . 1 ." },
    { "Triplets", "0 1 2 1 2 3 2 3 4 3 2 1" },
    { "Syncopate", "0 . 1 2 . 3 . 2" },
    { "Push", ". 0 1 . 2 . 3 _" },
    { "Hammer", "0! 0 0 0 1! 1 1 1" },
    { "Dotted", "0 _ _ 1 2 _ _ 3 1 _ _ 2 3 _ 0+ _" },
    { "Cascade", "3+ 2+ 1+ 0+ 2+ 1+ 0+ 3 1+ 0+ 3 2" },
    { "Lift", "0 0 1 1 2 2 3 3+" },
    { "Hocket", "0 . 2 . . 1 . 3" },
    { "Low High", "0- 3+ 1- 2+ 0- 3+ 2- 1+" },
    { "Twinkle", "3+ 1+ 2+ 0+ 3+ 1+ 2+ 3++" },
    { "March", "0! . 1 . 2! . 1 ." },
    { "Polka", "0- 1&2 0 1&2" },
    { "Reggae Skank", ". 0&1&2! . 0&1&2" },
    { "Funk", "0! . 0 1 . 2 0 . 3 . 2 1 . 0 . ." },
    { "House Stab", ". . 0&1&2! . . . 0&1&2 . . . 0&1&2! . . . . ." },
    { "Trance", "0 0+ 1 0+ 2 0+ 1 0+" },
    { "Ostinato", "0 1 2 0 1 2 0 1" },
    { "Fanfare", "0! 0 0 2! _ _ 1 2 3! _ _ _" },
    { "Lullaby", "2 _ 1 0 _ 1 2 _ _ . . ." },
    { "Ripple", "0 1 2 3 4 5 6 7" },
    { "Undertow", "0 0- 1 1- 2 2- 1 1-" },
    { "Sparkle", "0++ 2+ 1++ 3+ 0++ 1+ 2++ 3+" },
    { "Hesitate", "0 1 . 2 3 . . 0+" },
    { "Rolling", "0 1 2 3 1 2 3 0+ 2 3 0+ 1+" },
    { "Rocking", "0 2 0 2 1 3 1 3" },
    { "Cluster", "0&1 2&3 0&1 2&3" },
    { "Open Voicing", "0- 2 0+ 2+ 3+ 1+ 2 0" },
    { "Gamelan", "0 2 3 2 0 2 3 4 3 2 0 2" },
    { "Bell Tower", "0+! _ 2 _ 1+ _ 0 _" },
    { "Glitch", "0 0 . 0 1 . . 1 2 2 2 . 3 . 3 ." },
    { "Finale", "0 1 2 3 0+ 1+ 2+ 3+ 0++! _ _ _" },
};

struct PhraseStep
{
    std::array<int8_t, 6> tones{};
    int8_t count = 0;
    int8_t octave = 0;
    bool rest = false, tie = false, accent = false;
};
struct Phrase
{
    std::array<PhraseStep, 32> steps{};
    int length = 0;
};

// Parsed once when the library loads (no work on the audio thread).
struct PhraseTable
{
    std::array<Phrase, kNumArpPhrases> phrases{};
    PhraseTable()
    {
        for (int p = 0; p < kNumArpPhrases; ++p)
        {
            Phrase& ph = phrases[static_cast<size_t>(p)];
            const char* c = kPhraseDefs[p].steps;
            while (*c && ph.length < static_cast<int>(ph.steps.size()))
            {
                while (*c == ' ') ++c;
                if (!*c) break;
                PhraseStep st;
                for (; *c && *c != ' '; ++c)
                {
                    if (*c >= '0' && *c <= '9' && st.count < static_cast<int>(st.tones.size()))
                        st.tones[static_cast<size_t>(st.count++)] = static_cast<int8_t>(*c - '0');
                    else if (*c == '+') ++st.octave;
                    else if (*c == '-') --st.octave;
                    else if (*c == '!') st.accent = true;
                    else if (*c == '.') st.rest = true;
                    else if (*c == '_') st.tie = true;
                }
                ph.steps[static_cast<size_t>(ph.length++)] = st;
            }
        }
    }
};
const PhraseTable kPhrases;
} // namespace

const char* arpPhraseName(int index) noexcept
{
    return kPhraseDefs[std::clamp(index, 0, kNumArpPhrases - 1)].name;
}

void Arpeggiator::reset() noexcept
{
    physCount_ = 0;
    keyCount_ = 0;
    soundingCount_ = 0;
    stepNoteCount_ = 0;
    ratchetLeft_ = 0;
    nextRatchet_ = -1.0;
    running_ = false;
    latchedChord_ = false;
    keysChanged();
}

void Arpeggiator::stop(Sink& sink) noexcept
{
    releaseAll(sink);
    ratchetLeft_ = 0;
    nextRatchet_ = -1.0;
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
    if (keyCount_ < kMaxKeys)
    {
        keys_[static_cast<size_t>(keyCount_++)] = { note, velocity };
        keysChanged();
    }
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
        keysChanged();
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
    if (out != keyCount_) keysChanged();
    keyCount_ = out;
}

void Arpeggiator::beginBlock(const ArpParams& p, double bpm, double sampleRate, double transportPpq) noexcept
{
    const bool wasLatched = params_.latch;
    params_ = p;
    params_.octaves = std::clamp(p.octaves, 1, kMaxOctaves);
    params_.gate = std::clamp(p.gate, 0.01f, 1.5f);
    params_.swing = std::clamp(p.swing, 0.0f, 0.5f);
    params_.length = std::clamp(p.length, 0, 32);
    params_.offset = std::clamp(p.offset, 0, 31);
    params_.ratchet = std::clamp(p.ratchet, 0.0f, 1.0f);
    params_.chance = std::clamp(p.chance, 0.0f, 1.0f);
    params_.phrase = std::clamp(p.phrase, 0, kNumArpPhrases - 1);
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
            for (int i = 0; i < soundingCount_; ++i) sounding_[static_cast<size_t>(i)].off = clock_; // release now
            ratchetLeft_ = 0;
            nextRatchet_ = -1.0;
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

int Arpeggiator::octaveFor(int64_t k) noexcept
{
    const int n = params_.octaves;
    if (n <= 1) return 0;
    const auto m = static_cast<int>(k % n);
    switch (params_.octaveMode)
    {
        case ArpOctaveMode::Down: return n - 1 - m;
        case ArpOctaveMode::UpDown:
        {
            const int period = 2 * n - 2;
            const auto z = static_cast<int>(k % period);
            return z < n ? z : period - z;
        }
        case ArpOctaveMode::Alt: return (k & 1) ? n - 1 : 0;
        case ArpOctaveMode::Random: return std::min(n - 1, static_cast<int>(randomUnit() * static_cast<float>(n)));
        default: return m; // Follow, Up, Alt 2
    }
}

int Arpeggiator::buildPattern() noexcept
{
    std::array<Key, kMaxKeys> base{};
    std::copy(keys_.begin(), keys_.begin() + keyCount_, base.begin());
    const ArpMode mode = params_.mode;
    if (mode != ArpMode::Played)
        std::sort(base.begin(), base.begin() + keyCount_, [](const Key& a, const Key& b) { return a.note < b.note; });

    const bool follow = params_.octaveMode == ArpOctaveMode::Follow;
    randomOctave_ = !follow && params_.octaveMode == ArpOctaveMode::Random;

    // The note set the mode walks: the whole octave range (Follow) or one octave.
    std::array<Key, kMaxKeys * kMaxOctaves> set{};
    int n = 0;
    for (int o = 0; o < (follow ? params_.octaves : 1); ++o)
        for (int i = 0; i < keyCount_; ++i)
        {
            const int note = base[static_cast<size_t>(i)].note + 12 * o;
            if (note <= 127) set[static_cast<size_t>(n++)] = { note, base[static_cast<size_t>(i)].velocity };
        }

    // Order by mode.
    std::array<Key, kMaxKeys * kMaxOctaves * 2> ordered{};
    int m = 0;
    switch (mode)
    {
        case ArpMode::Down:
            for (int i = n - 1; i >= 0; --i) ordered[static_cast<size_t>(m++)] = set[static_cast<size_t>(i)];
            break;
        case ArpMode::UpDown:
        case ArpMode::DownUp:
        case ArpMode::UpAndDown:
        {
            std::array<Key, kMaxKeys * kMaxOctaves> leg = set;
            if (mode == ArpMode::DownUp) std::reverse(leg.begin(), leg.begin() + n);
            for (int i = 0; i < n; ++i) ordered[static_cast<size_t>(m++)] = leg[static_cast<size_t>(i)];
            // Return leg: Up/Down and Down/Up skip both ends (1 2 3 4 3 2); Up&Down repeats them (1 2 3 4 4 3 2 1).
            const bool repeatEnds = mode == ArpMode::UpAndDown;
            for (int i = repeatEnds ? n - 1 : n - 2; i >= (repeatEnds ? 0 : 1); --i)
                ordered[static_cast<size_t>(m++)] = leg[static_cast<size_t>(i)];
            break;
        }
        case ArpMode::RandomOnce:
        {
            // A shuffle that stays the same until the keys change.
            for (int i = 0; i < n; ++i) ordered[static_cast<size_t>(i)] = set[static_cast<size_t>(i)];
            m = n;
            uint32_t s = keysVersion_ * 2654435761u + 0x9E3779B9u;
            for (int i = m - 1; i > 0; --i)
            {
                s ^= s << 13;
                s ^= s >> 17;
                s ^= s << 5;
                std::swap(ordered[static_cast<size_t>(i)], ordered[s % static_cast<uint32_t>(i + 1)]);
            }
            break;
        }
        default:
            for (int i = 0; i < n; ++i) ordered[static_cast<size_t>(m++)] = set[static_cast<size_t>(i)];
            break;
    }

    // Walk the octaves (non-Follow modes).
    int out = 0;
    const int octs = params_.octaves;
    auto put = [&](Key k, int o) {
        k.note += 12 * o;
        if (k.note <= 127 && out < static_cast<int>(pattern_.size())) pattern_[static_cast<size_t>(out++)] = k;
    };
    if (follow || randomOctave_ || octs <= 1)
    {
        for (int i = 0; i < m; ++i) put(ordered[static_cast<size_t>(i)], 0);
    }
    else
    {
        switch (params_.octaveMode)
        {
            case ArpOctaveMode::Alt: // each note through its octaves
                for (int i = 0; i < m; ++i)
                    for (int o = 0; o < octs; ++o) put(ordered[static_cast<size_t>(i)], o);
                break;
            case ArpOctaveMode::Alt2: // the octave changes every step
                for (int k = 0; k < m * octs; ++k) put(ordered[static_cast<size_t>(k % m)], k % octs);
                break;
            default:
            {
                const int passes = params_.octaveMode == ArpOctaveMode::UpDown ? std::max(1, 2 * octs - 2) : octs;
                for (int pass = 0; pass < passes; ++pass)
                {
                    const int o = octaveFor(pass);
                    for (int i = 0; i < m; ++i) put(ordered[static_cast<size_t>(i)], o);
                }
                break;
            }
        }
    }
    return out;
}

void Arpeggiator::chooseNotes(int64_t step) noexcept
{
    stepNoteCount_ = 0;
    stepTie_ = false;
    const auto velocityOf = [&](const Key& k, bool accent) {
        float v = params_.velocity > 0 ? static_cast<float>(std::min(params_.velocity, 127)) / 127.0f : k.velocity;
        if (accent) v = std::min(1.0f, v * 1.3f + 0.1f);
        return v;
    };
    const auto add = [&](int note, float velocity) {
        if (note < 0 || note > 127 || stepNoteCount_ >= kMaxChord) return;
        for (int i = 0; i < stepNoteCount_; ++i)
            if (stepNotes_[static_cast<size_t>(i)].note == note) return;
        stepNotes_[static_cast<size_t>(stepNoteCount_++)] = { note, velocity };
    };

    if (params_.mode == ArpMode::Chord)
    {
        const int octave = octaveFor(step);
        for (int i = 0; i < keyCount_; ++i)
        {
            const Key& k = keys_[static_cast<size_t>(i)];
            add(k.note + 12 * octave, velocityOf(k, false));
        }
        return;
    }

    if (params_.mode == ArpMode::Phrase)
    {
        const Phrase& ph = kPhrases.phrases[static_cast<size_t>(params_.phrase)];
        const int len = params_.length > 0 ? params_.length : std::max(1, ph.length);
        const int64_t local = step % len;
        const int64_t pass = step / len;
        const PhraseStep& st = ph.steps[static_cast<size_t>((local + params_.offset) % std::max(1, ph.length))];
        if (st.rest) return;
        if (st.tie)
        {
            stepTie_ = true;
            return;
        }
        std::array<Key, kMaxKeys> sorted{};
        std::copy(keys_.begin(), keys_.begin() + keyCount_, sorted.begin());
        std::sort(sorted.begin(), sorted.begin() + keyCount_, [](const Key& a, const Key& b) { return a.note < b.note; });
        const int octave = octaveFor(pass);
        for (int t = 0; t < st.count; ++t)
        {
            const int tone = st.tones[static_cast<size_t>(t)];
            const Key& k = sorted[static_cast<size_t>(tone % keyCount_)];
            add(k.note + 12 * (tone / keyCount_ + st.octave + octave), velocityOf(k, st.accent));
        }
        return;
    }

    const int n = buildPattern();
    if (n <= 0) return;
    const int64_t local = params_.length > 0 ? step % params_.length : step;
    int idx = static_cast<int>((local + params_.offset) % n);
    if (params_.mode == ArpMode::Random)
    {
        idx = std::min(n - 1, static_cast<int>(randomUnit() * static_cast<float>(n)));
        if (n > 1 && soundingLast_ == pattern_[static_cast<size_t>(idx)].note) // avoid immediate repeats
            idx = (idx + 1 + static_cast<int>(randomUnit() * static_cast<float>(n - 1))) % n;
    }
    Key k = pattern_[static_cast<size_t>(idx)];
    if (randomOctave_) k.note += 12 * octaveFor(0);
    add(k.note, velocityOf(k, false));
    soundingLast_ = k.note;
}

int Arpeggiator::ratchetCount() noexcept
{
    const float r = params_.ratchet;
    if (r <= 0.0f) return 1;
    const float seg = r * 3.0f, u = randomUnit();
    if (seg < 1.0f) return u < seg ? 2 : 1;
    if (seg < 2.0f) return u < seg - 1.0f ? 4 : 2;
    return u < seg - 2.0f ? 8 : 4;
}

void Arpeggiator::noteOff(Sink& sink, int index) noexcept
{
    sink.arpNoteOff(sounding_[static_cast<size_t>(index)].note);
    sounding_[static_cast<size_t>(index)] = sounding_[static_cast<size_t>(--soundingCount_)];
}

void Arpeggiator::releaseAll(Sink& sink) noexcept
{
    while (soundingCount_ > 0) noteOff(sink, soundingCount_ - 1);
}

void Arpeggiator::hit(Sink& sink, double start, double slot, double gate) noexcept
{
    for (int s = 0; s < stepNoteCount_; ++s)
    {
        const Key& k = stepNotes_[static_cast<size_t>(s)];
        for (int i = 0; i < soundingCount_; ++i) // a note played again is released first
            if (sounding_[static_cast<size_t>(i)].note == k.note)
            {
                noteOff(sink, i);
                break;
            }
        if (soundingCount_ >= kMaxChord) break;
        sink.arpNoteOn(k.note, k.velocity);
        sounding_[static_cast<size_t>(soundingCount_++)] = { k.note, start + slot * gate };
    }
}

void Arpeggiator::fireStep(Sink& sink) noexcept
{
    stepStart_ = nextStep_;
    ratchetLeft_ = 0;
    nextRatchet_ = -1.0;
    if (keyCount_ == 0)
    {
        releaseAll(sink);
        running_ = false;
        return;
    }

    const double len = stepLength(stepIndex_);
    const double gate = params_.gate;
    chooseNotes(position_);
    if (params_.chance < 1.0f && randomUnit() >= params_.chance)
    {
        stepNoteCount_ = 0; // a skipped step (a tie still holds)
    }
    if (stepTie_)
    {
        for (int i = 0; i < soundingCount_; ++i) // hold what is sounding through this step
            sounding_[static_cast<size_t>(i)].off = std::max(sounding_[static_cast<size_t>(i)].off, stepStart_ + len * gate);
    }
    else if (stepNoteCount_ > 0)
    {
        const int count = ratchetCount();
        if (count > 1)
        {
            ratchetSlot_ = len / count;
            ratchetGate_ = std::min(gate, 0.9);
            ratchetLeft_ = count - 1;
            nextRatchet_ = stepStart_ + ratchetSlot_;
            hit(sink, stepStart_, ratchetSlot_, ratchetGate_);
        }
        else
        {
            hit(sink, stepStart_, len, gate);
        }
    }

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
    for (int guard = 0; guard < 64; ++guard)
    {
        bool fired = false;
        for (int i = soundingCount_ - 1; i >= 0; --i) // gate ends first, so a step can re-strike the same note
            if (sounding_[static_cast<size_t>(i)].off <= clock_ + kEps)
            {
                noteOff(sink, i);
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
        if (ratchetLeft_ > 0 && nextRatchet_ >= 0.0 && nextRatchet_ <= clock_ + kEps)
        {
            hit(sink, nextRatchet_, ratchetSlot_, ratchetGate_);
            --ratchetLeft_;
            nextRatchet_ = ratchetLeft_ > 0 ? nextRatchet_ + ratchetSlot_ : -1.0;
            fired = true;
        }
        if (running_ && nextStep_ <= clock_ + kEps)
        {
            fireStep(sink);
            fired = true;
        }
        if (!fired) break;
    }

    double next = 1e300;
    for (int i = 0; i < soundingCount_; ++i) next = std::min(next, sounding_[static_cast<size_t>(i)].off);
    if (ratchetLeft_ > 0 && nextRatchet_ >= 0.0) next = std::min(next, nextRatchet_);
    if (running_) next = std::min(next, nextStep_);
    if (next >= 1e299) return maxSamples;
    const double samples = (next - clock_) / ppqPerSample_;
    if (samples >= static_cast<double>(maxSamples)) return maxSamples;
    return std::clamp(static_cast<int>(std::ceil(samples - 1e-6)), 1, maxSamples);
}

} // namespace kmi
