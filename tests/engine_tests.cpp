// Offline engine tests: deterministic renders checked for pitch, stability, voice handling and CPU.
// Dependency-free on purpose; exits non-zero on failure (run via ctest).
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "kmi/Arpeggiator.h"
#include "kmi/Effects.h"
#include "kmi/Engine.h"
#include "kmi/Envelope.h"
#include "kmi/Modulation.h"
#include "kmi/Presets.h"
#include "kmi/Wavetables.h"

using namespace kmi;

namespace {
int g_failures = 0;
int g_checks = 0;

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        ++g_checks;                                                         \
        if (!(cond)) {                                                      \
            ++g_failures;                                                   \
            std::printf("  FAIL %s:%d: %s -- ", __FILE__, __LINE__, #cond); \
            std::printf(__VA_ARGS__);                                       \
            std::printf("\n");                                              \
        }                                                                   \
    } while (0)

constexpr double kRate = 48000.0;

struct Render
{
    std::vector<float> l, r;
    float peak() const
    {
        float p = 0.0f;
        for (size_t i = 0; i < l.size(); ++i) p = std::fmax(p, std::fmax(std::fabs(l[i]), std::fabs(r[i])));
        return p;
    }
    bool finite() const
    {
        for (size_t i = 0; i < l.size(); ++i)
            if (!std::isfinite(l[i]) || !std::isfinite(r[i])) return false;
        return true;
    }
};

Render render(Engine& e, int samples, int block = 128)
{
    Render out;
    out.l.resize(static_cast<size_t>(samples));
    out.r.resize(static_cast<size_t>(samples));
    for (int i = 0; i < samples; i += block)
    {
        const int n = std::min(block, samples - i);
        e.render(out.l.data() + i, out.r.data() + i, n);
    }
    return out;
}

int zeroCrossingsUp(const std::vector<float>& x, size_t from, size_t to)
{
    int n = 0;
    for (size_t i = from + 1; i < to; ++i)
        if (x[i - 1] <= 0.0f && x[i] > 0.0f) ++n;
    return n;
}

// A patch with all 8 oscillators audible/modulating, bright filter, used for stress tests.
Patch busyPatch(int algorithm, OscMode mode)
{
    Patch p = makeInitPatch();
    p.algo.index = algorithm;
    for (int i = 0; i < kNumOscs; ++i)
    {
        auto& o = p.osc[static_cast<size_t>(i)];
        o.initLevel = 0.8f;
        o.mode = mode;
        o.wave = i % WavetableBank::instance().numWaves();
        o.pitchMode = PitchMode::Ratio;
        o.ratio = static_cast<float>(1 + i % 4);
        o.feedback = 0.5f;
        o.envLevel = 0.2f;
    }
    p.dfilt.cutoff = 0.8f;
    p.dfilt.resonance = 0.5f;
    p.afilt.cutoff = 0.9f;
    return p;
}


// Records arpeggiator output as (sample, note, on) events.
struct ArpLog final : Arpeggiator::Sink
{
    struct Ev { int64_t sample; int note; bool on; float vel; };
    std::vector<Ev> ev;
    int64_t now = 0;
    void arpNoteOn(int note, float velocity) noexcept override { ev.push_back({ now, note, true, velocity }); }
    void arpNoteOff(int note) noexcept override { ev.push_back({ now, note, false, 0.0f }); }
    std::vector<int> ons() const
    {
        std::vector<int> v;
        for (const auto& e : ev)
            if (e.on) v.push_back(e.note);
        return v;
    }
    std::vector<int64_t> onTimes() const
    {
        std::vector<int64_t> v;
        for (const auto& e : ev)
            if (e.on) v.push_back(e.sample);
        return v;
    }
};

// Runs the arp for `samples` in host blocks of `block`, as the engine does.
void runArp(Arpeggiator& a, ArpLog& log, const ArpParams& p, int samples, double bpm = 120.0, double ppq = -1.0, int block = 512)
{
    for (int done = 0; done < samples;)
    {
        const int len = std::min(block, samples - done);
        a.beginBlock(p, bpm, kRate, ppq);
        for (int i = 0; i < len;)
        {
            const int n = a.process(log, len - i);
            a.advance(n);
            log.now += n;
            i += n;
        }
        if (ppq >= 0.0) ppq += len * bpm / (60.0 * kRate);
        done += len;
    }
}

template <typename T>
std::string join(const std::vector<T>& v)
{
    std::string s;
    for (T x : v) s += std::to_string(x) + " ";
    return s;
}
void test(const char* name, const std::function<void()>& fn)
{
    std::printf("[ RUN ] %s\n", name);
    const int before = g_failures;
    fn();
    std::printf("[ %s ] %s\n", g_failures == before ? " OK " : "FAIL", name);
}
} // namespace

int main()
{
    test("tables and names", [] {
        const auto& bank = WavetableBank::instance();
        CHECK(bank.numWaves() >= 20, "waves=%d", bank.numWaves());
        CHECK(static_cast<int>(modSourceNames().size()) == src::Count, "sources");
        CHECK(static_cast<int>(modDestNames().size()) == dst::Count, "dests");
        CHECK(numPresetAlgorithms() == 64, "algos=%d", numPresetAlgorithms());
        CHECK(presetAlgorithmsValid(), "a preset algorithm definition failed to parse");
        for (int a = 0; a < numPresetAlgorithms(); ++a)
            for (int b = 0; b < a; ++b)
            {
                bool same = true;
                for (int m = 0; m < kNumOscs && same; ++m)
                {
                    same = presetAlgorithm(a).out[m] == presetAlgorithm(b).out[m];
                    for (int c = 0; c < kNumOscs && same; ++c) same = presetAlgorithm(a).conn[m][c] == presetAlgorithm(b).conn[m][c];
                }
                CHECK(!same, "algorithms %d and %d are identical", a + 1, b + 1);
            }
        for (int a = 0; a < numPresetAlgorithms(); ++a)
            CHECK(presetAlgorithm(a).out[0] > 0.0f, "algorithm %d must route Osc 1", a);
        // Band-limiting: the sine table must be clean at every mip.
        const float* t = bank.table(0, 0);
        float err = 0.0f;
        for (int n = 0; n < WavetableBank::kTableSize; ++n)
            err = std::fmax(err, std::fabs(t[n] - std::sin(6.2831853f * n / WavetableBank::kTableSize)));
        CHECK(err < 1e-3f, "sine error %f", err);
    });

    test("init patch plays A440 at the right pitch", [] {
        Engine e;
        e.prepare(kRate);
        e.noteOn(69, 1.0f);
        Render r = render(e, static_cast<int>(kRate));
        const int zc = zeroCrossingsUp(r.l, 4800, 4800 + 48000 / 2); // 0.5 s window after attack
        CHECK(zc >= 218 && zc <= 222, "zero crossings=%d (expect ~220)", zc);
        CHECK(r.peak() > 0.1f && r.peak() < 1.5f, "peak=%f", r.peak());
        CHECK(r.finite(), "non-finite output");
    });

    test("1x and 2x oversampling agree on pitch", [] {
        for (int os : { 1, 2 })
        {
            Engine e;
            e.patch().oversampling = os;
            e.prepare(kRate);
            e.noteOn(57, 1.0f);
            Render r = render(e, static_cast<int>(kRate / 2));
            const int zc = zeroCrossingsUp(r.l, 2400, 2400 + 12000);
            CHECK(zc >= 54 && zc <= 56, "os=%d zero crossings=%d (expect ~55)", os, zc);
        }
    });

    test("release ends the voice", [] {
        Engine e;
        e.prepare(kRate);
        e.noteOn(60, 0.8f);
        render(e, 4800);
        CHECK(e.activeVoiceCount() == 1, "voices=%d", e.activeVoiceCount());
        e.noteOff(60);
        render(e, static_cast<int>(kRate)); // default release 250 ms
        CHECK(e.activeVoiceCount() == 0, "voices=%d after release", e.activeVoiceCount());
        Render tail = render(e, 512);
        CHECK(tail.peak() < 1e-4f, "tail peak=%f", tail.peak());
    });

    test("sustain pedal holds then releases", [] {
        Engine e;
        e.prepare(kRate);
        e.controlChange(64, 1.0f);
        e.noteOn(60, 0.8f);
        render(e, 2400);
        e.noteOff(60);
        render(e, static_cast<int>(kRate));
        CHECK(e.activeVoiceCount() == 1, "pedal should hold the note");
        e.controlChange(64, 0.0f);
        render(e, static_cast<int>(kRate));
        CHECK(e.activeVoiceCount() == 0, "pedal up should release");
    });

    test("all algorithms x all oscillator modes stay finite and bounded", [] {
        for (int a = 0; a < numPresetAlgorithms(); ++a)
            for (int m = 0; m < static_cast<int>(OscMode::Count); ++m)
            {
                Engine e;
                e.patch() = busyPatch(a, static_cast<OscMode>(m));
                e.prepare(kRate);
                e.noteOn(48, 1.0f);
                e.noteOn(67, 0.7f);
                Render r = render(e, 9600);
                CHECK(r.finite(), "algo %d mode %d non-finite", a, m);
                CHECK(r.peak() < 8.0f, "algo %d mode %d peak %f", a, m, r.peak());
                CHECK(r.peak() > 1e-3f, "algo %d mode %d silent", a, m);
            }
    });

    test("every digital filter type is stable at full resonance", [] {
        for (int t = 0; t < static_cast<int>(DigitalFilterType::Count); ++t)
            for (float cutoff : { 0.05f, 0.5f, 1.0f })
            {
                Engine e;
                Patch p = busyPatch(2, OscMode::PhaseMod);
                p.dfilt.type = static_cast<DigitalFilterType>(t);
                p.dfilt.resonance = 1.0f;
                p.dfilt.drive = 0.7f;
                p.dfilt.cutoff = cutoff;
                p.afilt.resonance = 1.0f;
                e.patch() = p;
                e.prepare(kRate);
                e.noteOn(36, 1.0f);
                Render r = render(e, 9600);
                CHECK(r.finite(), "type %d cutoff %.2f non-finite", t, cutoff);
                CHECK(r.peak() < 16.0f, "type %d cutoff %.2f peak %f", t, cutoff, r.peak());
            }
    });

    test("custom algorithm with feedback loops stays finite", [] {
        Engine e;
        Patch p = busyPatch(0, OscMode::FreqMod);
        p.algo.mode = AlgoMode::Custom;
        Algorithm a{};
        for (int m = 0; m < kNumOscs; ++m)
            for (int c = 0; c < kNumOscs; ++c)
                a.conn[m][c] = m != c ? 1.0f : 0.0f; // fully connected, including upward edges
        a.out[0] = 1.0f;
        p.customAlgorithm = a;
        e.patch() = p;
        e.prepare(kRate);
        e.noteOn(60, 1.0f);
        Render r = render(e, 9600);
        CHECK(r.finite(), "non-finite");
    });

    test("algorithm morph sweeps smoothly", [] {
        Engine e;
        Patch p = busyPatch(0, OscMode::PhaseMod);
        p.algo.mode = AlgoMode::Morph;
        p.algo.morphSlots = { 2, 6, 9, 0, 0, 0, 0, 0 }; // presets 0, 4, 7
        e.patch() = p;
        e.prepare(kRate);
        e.noteOn(60, 1.0f);
        bool ok = true;
        for (int step = 0; step <= 20; ++step)
        {
            e.patch().algo.morphPos = 2.0f * step / 20.0f;
            Render r = render(e, 1024);
            ok = ok && r.finite();
        }
        CHECK(ok, "non-finite during morph");
    });

    test("polyphony is capped at 16 and stealing is clean", [] {
        Engine e;
        e.prepare(kRate);
        for (int n = 0; n < 24; ++n)
        {
            e.noteOn(40 + n, 0.9f);
            render(e, 256);
        }
        render(e, 2048);
        CHECK(e.activeVoiceCount() <= kMaxVoices, "voices=%d", e.activeVoiceCount());
        CHECK(e.activeVoiceCount() == kMaxVoices, "expected all voices busy, got %d", e.activeVoiceCount());
    });

    test("voice limit is respected", [] {
        Engine e;
        e.patch().voice.voiceLimit = 5;
        e.prepare(kRate);
        for (int n = 0; n < 10; ++n) e.noteOn(50 + n, 0.9f);
        render(e, 4096);
        CHECK(e.activeVoiceCount() == 5, "voices=%d", e.activeVoiceCount());
    });

    test("mono legato uses one voice; unison stacks density voices", [] {
        {
            Engine e;
            e.patch().voice.polyMode = PolyMode::Mono;
            e.prepare(kRate);
            e.noteOn(60, 1.0f);
            render(e, 1024);
            e.noteOn(64, 1.0f);
            render(e, 1024);
            CHECK(e.activeVoiceCount() == 1, "mono voices=%d", e.activeVoiceCount());
            e.noteOff(64);
            render(e, 1024);
            CHECK(e.activeVoiceCount() == 1, "falls back to held note");
        }
        {
            Engine e;
            e.patch().voice.polyMode = PolyMode::Unison;
            e.patch().voice.density = 6;
            e.prepare(kRate);
            e.noteOn(60, 1.0f);
            Render r = render(e, 4096);
            CHECK(e.activeVoiceCount() == 6, "unison voices=%d", e.activeVoiceCount());
            CHECK(r.finite(), "non-finite");
        }
    });

    test("mod matrix routes change the sound", [] {
        auto renderWith = [](bool route) {
            Engine e;
            if (route)
            {
                e.patch().lfo[3].rateHz = 6.0f;
                e.patch().mod[0] = ModSlot{ src::Lfo1 + 3, dst::osc(0, dst::OscPitch), 1.0f };
            }
            e.prepare(kRate);
            e.noteOn(69, 1.0f);
            return render(e, 24000);
        };
        Render a = renderWith(false), b = renderWith(true);
        double diff = 0.0;
        for (size_t i = 0; i < a.l.size(); ++i) diff += std::fabs(a.l[i] - b.l[i]);
        CHECK(diff > 100.0, "routing had no audible effect (diff=%f)", diff);
    });

    test("macro routes apply", [] {
        Engine e;
        e.patch().macro[0].value = 1.0f;
        e.patch().macro[0].routes[0] = ModSlot{ 0, dst::VcaBase + dst::VcaLevel, -1.0f };
        e.prepare(kRate);
        e.noteOn(69, 1.0f);
        Render r = render(e, 4800);
        CHECK(r.peak() < 1e-3f, "macro should have closed the VCA, peak=%f", r.peak());
    });

    test("envelope timing", [] {
        Envelope env;
        EnvParams p;
        p.attackMs = 10.0f;
        p.attackCurve = 0.0f;
        p.decayMs = 10.0f;
        p.sustain = 0.5f;
        env.update(p, EnvMods{}, 48000.0f, 120.0);
        env.noteOn(p, 1.0f, false, false);
        int toPeak = 0;
        while (env.tick() < 0.999f && toPeak < 48000) ++toPeak;
        CHECK(std::abs(toPeak - 480) <= 2, "attack samples=%d (expect 480)", toPeak);
        for (int i = 0; i < 2000; ++i) env.tick();
        CHECK(std::fabs(env.value() - 0.5f) < 1e-3f, "sustain=%f", env.value());
        env.noteOff();
        for (int i = 0; i < 48000; ++i) env.tick();
        CHECK(env.isIdle(), "release did not finish");
    });

    test("deterministic rendering", [] {
        auto run = [] {
            Engine e;
            e.patch() = busyPatch(5, OscMode::PhaseMod);
            e.prepare(kRate);
            e.noteOn(60, 0.9f);
            e.noteOn(63, 0.6f);
            return render(e, 8192);
        };
        Render a = run(), b = run();
        bool same = a.l == b.l && a.r == b.r;
        CHECK(same, "two identical runs differ");
    });

    test("SIMD filter bank matches the scalar filters lane by lane", [] {
        const float sr = 96000.0f;
        for (int t = 0; t < static_cast<int>(DigitalFilterType::Count); ++t)
        {
            const auto type = static_cast<DigitalFilterType>(t);
            FilterBank4 bank;
            DigitalFilter df[4];
            AnalogFilter af[4];
            FilterLaneParams lanes[4];
            DigitalFilterBlock blk[4];
            float aRes[4], aDrive[4], gD[4], gA[4];
            for (int l = 0; l < 4; ++l)
            {
                blk[l].type = type;
                blk[l].resonance = 0.2f * l;
                blk[l].morph = 0.3f * l;
                blk[l].drive = l == 2 ? 0.0f : 0.25f * l; // lane 2 drive off: tests the per-lane blend
                blk[l].cutoffNorm = 0.3f + 0.15f * l;
                aRes[l] = 0.1f * l;
                aDrive[l] = 0.05f + 0.2f * l;
                gD[l] = hzToG(cutoffToHz(blk[l].cutoffNorm, sr), sr);
                gA[l] = hzToG(cutoffToHz(0.5f + 0.1f * l, sr), sr);
                computeDigitalLane(blk[l], sr, lanes[l]);
                computeAnalogLane(aRes[l], aDrive[l], lanes[l]);
                lanes[l].gFrom = lanes[l].gTo = gD[l];
                lanes[l].aGFrom = lanes[l].aGTo = gA[l];
                lanes[l].level = 1.0f;
                df[l].prepareBlock(blk[l], sr);
                af[l].prepareBlock(aRes[l], aDrive[l]);
            }
            const FilterLaneParams* ptrs[4] = { &lanes[0], &lanes[1], &lanes[2], &lanes[3] };
            float maxErr = 0.0f;
            Rng rng;
            for (int block = 0; block < 64; ++block)
            {
                bank.prepareBlock(type, false, ptrs, 32);
                for (int l = 0; l < 4; ++l)
                {
                    df[l].beginRamp(gD[l], gD[l], 32);
                    af[l].beginRamp(gA[l], gA[l], 32);
                }
                for (int s = 0; s < 32; ++s)
                {
                    float in[4], simd[4];
                    for (float& v : in) v = rng.bipolar() * 0.8f;
                    bank.process(F4::load(in)).store(simd);
                    for (int l = 0; l < 4; ++l)
                    {
                        const float ref = af[l].process(df[l].process(in[l]));
                        maxErr = std::fmax(maxErr, std::fabs(ref - simd[l]));
                    }
                }
            }
            CHECK(maxErr < 1e-4f, "type %d: SIMD differs from scalar by %g", t, maxErr);
        }
    });

    test("bypassed FX chain is transparent", [] {
        FxChain fx;
        fx.prepare(kRate);
        FxParams p; // inserts Bypass, delay/reverb disabled
        std::vector<float> l(1024), r(1024);
        for (int i = 0; i < 1024; ++i)
        {
            l[static_cast<size_t>(i)] = std::sin(0.01f * i);
            r[static_cast<size_t>(i)] = std::cos(0.013f * i);
        }
        auto l0 = l, r0 = r;
        fx.process(l.data(), r.data(), 1024, p, nullptr, 120.0, -1.0);
        CHECK(l == l0 && r == r0, "bypass changed the signal");
    });

    test("delay echoes arrive at the set time", [] {
        for (int t = 0; t < static_cast<int>(DelayType::Count); ++t)
        {
            if (static_cast<DelayType>(t) == DelayType::Reverse) continue;
            StereoDelay d;
            d.prepare(kRate);
            DelayParams p;
            p.enabled = true;
            p.type = static_cast<DelayType>(t);
            p.timeMs = 100.0f;
            p.feedback = 0.0f;
            p.mix = 1.0f;
            p.feedbackTone = 1.0f;
            std::vector<float> l(9600, 0.0f), r(9600, 0.0f);
            l[0] = r[0] = 1.0f;
            d.process(l.data(), r.data(), 9600, p, nullptr, 120.0);
            int peakAt = 0;
            float peak = 0.0f;
            for (int i = 0; i < 9600; ++i)
            {
                const float v = std::fabs(l[static_cast<size_t>(i)]) + std::fabs(r[static_cast<size_t>(i)]);
                if (v > peak) { peak = v; peakAt = i; }
            }
            CHECK(std::abs(peakAt - 4800) <= 2, "type %d echo at %d (expect 4800)", t, peakAt);
        }
    });

    test("reverb decays, and freeze sustains", [] {
        auto energy = [](const std::vector<float>& x, int from, int to) {
            double e = 0.0;
            for (int i = from; i < to; ++i) e += static_cast<double>(x[static_cast<size_t>(i)]) * x[static_cast<size_t>(i)];
            return e;
        };
        for (int t = 0; t < static_cast<int>(ReverbType::Count); ++t)
        {
            Reverb rv;
            rv.prepare(kRate);
            ReverbParams p;
            p.enabled = true;
            p.type = static_cast<ReverbType>(t);
            p.timeSec = 1.0f;
            p.mix = 1.0f;
            const int n = static_cast<int>(kRate) * 4;
            std::vector<float> l(static_cast<size_t>(n), 0.0f), r(static_cast<size_t>(n), 0.0f);
            l[0] = r[0] = 1.0f;
            rv.process(l.data(), r.data(), n, p, nullptr);
            const double early = energy(l, 2400, 24000), late = energy(l, 3 * 48000, 4 * 48000);
            bool finite = true;
            for (float v : l) finite = finite && std::isfinite(v);
            CHECK(finite, "type %d non-finite", t);
            CHECK(early > 1e-4 && late < early * 1e-3, "type %d early=%g late=%g", t, early, late);
        }
        {
            Reverb rv;
            rv.prepare(kRate);
            ReverbParams p;
            p.enabled = true;
            p.timeSec = 1.0f;
            p.mix = 1.0f;
            const int n = static_cast<int>(kRate) * 3;
            std::vector<float> l(static_cast<size_t>(n), 0.0f), r(static_cast<size_t>(n), 0.0f);
            for (int i = 0; i < 480; ++i) l[static_cast<size_t>(i)] = r[static_cast<size_t>(i)] = std::sin(0.05f * i);
            rv.process(l.data(), r.data(), 9600, p, nullptr);
            p.freeze = true;
            rv.process(l.data() + 9600, r.data() + 9600, n - 9600, p, nullptr);
            const double a = energy(l, 48000, 72000), b = energy(l, 120000, 144000);
            CHECK(b > a * 0.5, "freeze lost energy: %g -> %g", a, b);
        }
    });

    test("every insert, delay and reverb type stays finite in the engine", [] {
        for (int slot = 0; slot < 2; ++slot)
            for (int t = 1; t < static_cast<int>(FxType::Count); ++t)
            {
                Engine e;
                Patch p = busyPatch(2, OscMode::PhaseMod);
                InsertFxParams& ins = slot == 0 ? p.fx.pre : p.fx.post;
                ins.type = static_cast<FxType>(t);
                ins.comp.sidechain = t % 2 ? CompSidechain::TempoDuck : CompSidechain::Off;
                ins.lofi.outputDb = 36.0f;
                ins.distort.drive = 1.0f;
                ins.eq.lowDb = 24.0f;
                ins.flanger.feedback = 1.0f;
                ins.phaser.feedback = 1.0f;
                p.fx.delay.enabled = true;
                p.fx.delay.type = static_cast<DelayType>(t % static_cast<int>(DelayType::Count));
                p.fx.delay.feedback = 1.0f;
                p.fx.reverb.enabled = true;
                p.fx.reverb.type = static_cast<ReverbType>(t % static_cast<int>(ReverbType::Count));
                p.fx.reverb.timeSec = 90.0f;
                p.fx.reverb.mix = 1.0f;
                e.patch() = p;
                e.prepare(kRate);
                e.noteOn(48, 1.0f);
                e.noteOn(60, 0.8f);
                Render r = render(e, 48000);
                CHECK(r.finite(), "slot %d type %d non-finite", slot, t);
                CHECK(r.peak() < 16.0f, "slot %d type %d peak %f", slot, t, r.peak());
                CHECK(r.peak() > 1e-3f, "slot %d type %d silent", slot, t);
            }
    });

    test("mod matrix reaches FX destinations", [] {
        auto run = [](bool route) {
            Engine e;
            Patch p = makeInitPatch();
            p.fx.reverb.enabled = true;
            p.fx.reverb.mix = 0.0f; // dry unless the macro opens it
            if (route)
            {
                p.macro[0].value = 1.0f;
                p.macro[0].routes[0] = ModSlot{ 0, dst::FxBase + dst::ReverbMix, 1.0f };
            }
            e.patch() = p;
            e.prepare(kRate);
            e.noteOn(69, 1.0f);
            return render(e, 9600);
        };
        Render dry = run(false), wet = run(true);
        double diff = 0.0;
        for (size_t i = 4800; i < dry.l.size(); ++i) diff += std::fabs(dry.l[i] - wet.l[i]);
        CHECK(diff > 50.0, "macro -> reverb mix had no effect (diff=%f)", diff);
    });

    test("factory presets are valid, audible, unclipped and finite", [] {
        CHECK(numFactoryPresets() >= 16, "presets=%d", numFactoryPresets());
        for (int i = 0; i < numFactoryPresets(); ++i)
        {
            const FactoryPresetInfo& info = factoryPresetInfo(i);
            for (int j = 0; j < i; ++j) CHECK(std::string(info.name) != factoryPresetInfo(j).name, "duplicate name %s", info.name);
            const Patch p = makeFactoryPreset(i);
            CHECK(p.algo.index >= 0 && p.algo.index < numPresetAlgorithms(), "%s: unknown algorithm", info.name);
            for (int s : p.algo.morphSlots)
                CHECK(s >= 0 && s < kMorphSlotFirstPreset + numPresetAlgorithms(), "%s: bad morph slot %d", info.name, s);
            CHECK(p.vca.patchLevel > 0.0f && p.vca.patchLevel <= 2.0f, "%s: patch level out of range", info.name);
            int audible = 0;
            for (const auto& o : p.osc) audible += !o.mute;
            CHECK(audible >= 1, "%s: no oscillators", info.name);

            // A three-note chord (one note for mono presets), held for 1.5 s, then released.
            Engine e;
            e.patch() = p;
            e.prepare(kRate);
            for (int n : { 0, 4, 7 }) e.noteOn(info.demoNote + n, 0.9f);
            Render held = render(e, static_cast<int>(kRate * 1.5));
            for (int n : { 0, 4, 7 }) e.noteOff(info.demoNote + n);
            Render tail = render(e, static_cast<int>(kRate * 20.0));
            CHECK(held.finite() && tail.finite(), "%s: non-finite output", info.name);
            CHECK(held.peak() < 1.0f, "%s: clips while held (peak %.2f)", info.name, held.peak());
            CHECK(tail.peak() < 1.0f, "%s: clips in release (peak %.2f)", info.name, tail.peak());
            double sum = 0.0;
            for (size_t k = 0; k < held.l.size(); ++k) sum += held.l[k] * held.l[k];
            const double rmsDb = 10.0 * std::log10(sum / static_cast<double>(held.l.size()) + 1e-20);
            CHECK(rmsDb > -40.0, "%s: too quiet (%.1f dBFS)", info.name, rmsDb);
            // Voices end after the release, and the reverb/delay tails die away.
            CHECK(e.activeVoiceCount() == 0, "%s: %d voices still running 20 s after release", info.name, e.activeVoiceCount());
            float last = 0.0f;
            for (size_t k = tail.l.size() - 4800; k < tail.l.size(); ++k) last = std::fmax(last, std::fabs(tail.l[k]));
            CHECK(last < 1.0e-3f, "%s: tail still at %.4f after 20 s", info.name, last);
        }
    });

    test("showcase presets cover the feature set; Grand Tour uses all of it", [] {
        int tour = -1;
        bool custom = false, envLoop = false, directOut = false, freqPitch = false, oneShotStep = false;
        for (int i = 0; i < numFactoryPresets(); ++i)
        {
            const Patch p = makeFactoryPreset(i);
            if (std::string(factoryPresetInfo(i).name) == "Grand Tour") tour = i;
            custom = custom || p.algo.mode == AlgoMode::Custom;
            for (const auto& o : p.osc)
            {
                envLoop = envLoop || (!o.mute && o.env.loop > 0);
                directOut = directOut || (!o.mute && o.directOut);
                freqPitch = freqPitch || (!o.mute && o.pitchMode == PitchMode::Frequency);
            }
            for (const auto& l : p.lfo) oneShotStep = oneShotStep || l.oneShot == LfoOneShot::Step;
        }
        CHECK(custom && envLoop && directOut && freqPitch && oneShotStep, "showcase coverage: custom=%d loop=%d direct=%d freq=%d step=%d",
              custom, envLoop, directOut, freqPitch, oneShotStep);
        CHECK(tour >= 0, "Grand Tour missing");
        if (tour < 0) return;

        const Patch p = makeFactoryPreset(tour);
        bool mode[static_cast<int>(OscMode::Count)] = {}, pitch[static_cast<int>(PitchMode::Count)] = {};
        bool oscEnvSource = false;
        for (const auto& o : p.osc)
        {
            CHECK(!o.mute, "Grand Tour: every oscillator plays");
            mode[static_cast<int>(o.mode)] = pitch[static_cast<int>(o.pitchMode)] = true;
            oscEnvSource = oscEnvSource || o.envSource != EnvSource::Self;
        }
        for (int m = 0; m < static_cast<int>(OscMode::Count); ++m) CHECK(mode[m], "Grand Tour: oscillator mode %d unused", m);
        for (int m = 0; m < static_cast<int>(PitchMode::Count); ++m) CHECK(pitch[m], "Grand Tour: pitch mode %d unused", m);
        CHECK(oscEnvSource, "Grand Tour: shared envelope source");
        CHECK(p.algo.mode == AlgoMode::Morph, "Grand Tour: algorithm morph");
        CHECK(p.dfilt.envAmt != 0.0f && p.afilt.envAmt != 0.0f, "Grand Tour: both filter envelopes");
        CHECK(p.voice.polyMode == PolyMode::UnisonPoly, "Grand Tour: Unison Poly");
        CHECK(p.fx.pre.type != FxType::Bypass && p.fx.post.type != FxType::Bypass && p.fx.delay.enabled && p.fx.reverb.enabled,
              "Grand Tour: full FX chain");

        const ModUsage u = analyzeModUsage(p);
        for (int k = 0; k < kNumLfos; ++k) CHECK(u.lfo[k], "Grand Tour: LFO %d unused", k + 1);
        bool depthOfDepth = false, fxDest = false, steps = false;
        for (const auto& s : p.mod)
        {
            depthOfDepth = depthOfDepth || dst::isModDepth(s.dest);
            fxDest = fxDest || s.dest >= dst::FxBase;
        }
        for (const auto& l : p.lfo) steps = steps || l.wave == LfoWave::Step;
        CHECK(depthOfDepth && fxDest && steps, "Grand Tour: depth-of-depth=%d fx-dest=%d step-lfo=%d", depthOfDepth, fxDest, steps);
        for (int m = 0; m < kNumMacros; ++m)
            CHECK(p.macro[static_cast<size_t>(m)].routes[0].dest != dst::None, "Grand Tour: macro %d unassigned", m + 1);
    });

    test("arpeggiator: modes, octaves and timing", [] {
        const int step = static_cast<int>(kRate * 0.125); // 1/16 at 120 BPM
        auto play = [&](ArpMode mode, int octaves, int steps) {
            Arpeggiator a;
            ArpLog log;
            ArpParams p;
            p.enabled = true;
            p.mode = mode;
            p.octaves = octaves;
            a.beginBlock(p, 120.0, kRate, -1.0);
            a.keyDown(67, 0.5f); // pressed out of order on purpose
            a.keyDown(60, 0.9f);
            a.keyDown(64, 0.7f);
            runArp(a, log, p, step * steps - 10);
            return log;
        };
        ArpLog up = play(ArpMode::Up, 1, 6);
        CHECK(join(up.ons()) == "60 64 67 60 64 67 ", "up: %s", join(up.ons()).c_str());
        const auto t = up.onTimes();
        for (size_t i = 0; i < t.size(); ++i) CHECK(t[i] == static_cast<int64_t>(i) * step, "step %d at %lld", (int)i, (long long)t[i]);
        CHECK(up.ev[0].vel == 0.9f && up.ev.size() > 2 && up.ev[2].vel == 0.7f, "velocity as played");
        // Gate 50%: each note ends half a step after it starts.
        CHECK(!up.ev[1].on && up.ev[1].sample == step / 2, "gate off at %lld", (long long)up.ev[1].sample);

        CHECK(join(play(ArpMode::Down, 1, 4).ons()) == "67 64 60 67 ", "down: %s", join(play(ArpMode::Down, 1, 4).ons()).c_str());
        CHECK(join(play(ArpMode::UpDown, 1, 6).ons()) == "60 64 67 64 60 64 ", "updown: %s", join(play(ArpMode::UpDown, 1, 6).ons()).c_str());
        CHECK(join(play(ArpMode::DownUp, 1, 5).ons()) == "67 64 60 64 67 ", "downup: %s", join(play(ArpMode::DownUp, 1, 5).ons()).c_str());
        CHECK(join(play(ArpMode::Played, 1, 4).ons()) == "67 60 64 67 ", "played: %s", join(play(ArpMode::Played, 1, 4).ons()).c_str());
        CHECK(join(play(ArpMode::Up, 2, 7).ons()) == "60 64 67 72 76 79 60 ", "2 oct: %s", join(play(ArpMode::Up, 2, 7).ons()).c_str());
        CHECK(join(play(ArpMode::Chord, 2, 3).ons()) == "67 60 64 79 72 76 67 60 64 ", "chord: %s", join(play(ArpMode::Chord, 2, 3).ons()).c_str());
        const auto rnd = play(ArpMode::Random, 1, 32).ons();
        bool repeats = false, all[3] = {};
        for (size_t i = 0; i < rnd.size(); ++i)
        {
            repeats = repeats || (i > 0 && rnd[i] == rnd[i - 1]);
            all[rnd[i] == 60 ? 0 : rnd[i] == 64 ? 1 : 2] = true;
        }
        CHECK(rnd.size() == 32 && !repeats && all[0] && all[1] && all[2], "random: %s", join(rnd).c_str());
    });

    test("arpeggiator: swing, fixed velocity, latch, pedal and transport sync", [] {
        const double stepD = kRate * 0.125;
        {
            Arpeggiator a;
            ArpLog log;
            ArpParams p;
            p.swing = 0.5f; // 75%: the off-beat sixteenth lands three quarters through the eighth
            p.velocity = 100;
            a.beginBlock(p, 120.0, kRate, -1.0);
            a.keyDown(60, 0.3f);
            runArp(a, log, p, static_cast<int>(stepD * 4) - 10);
            const auto t = log.onTimes();
            CHECK(t.size() == 4 && t[1] == static_cast<int64_t>(std::ceil(stepD * 1.5)) && t[2] == static_cast<int64_t>(stepD * 2),
                  "swing times %s", t.size() > 2 ? (std::to_string(t[1]) + " " + std::to_string(t[2])).c_str() : "?");
            CHECK(std::fabs(log.ev[0].vel - 100.0f / 127.0f) < 1e-6f, "fixed velocity");
        }
        {
            Arpeggiator a;
            ArpLog log;
            ArpParams p;
            p.latch = true;
            a.beginBlock(p, 120.0, kRate, -1.0);
            a.keyDown(60, 0.8f);
            a.keyDown(64, 0.8f);
            runArp(a, log, p, static_cast<int>(stepD));
            a.keyUp(60);
            a.keyUp(64);
            runArp(a, log, p, static_cast<int>(stepD * 2));
            CHECK(join(log.ons()) == "60 64 60 ", "latched: %s", join(log.ons()).c_str());
            a.keyDown(72, 0.8f); // a new chord replaces the latched one
            runArp(a, log, p, static_cast<int>(stepD * 2));
            const auto ons = log.ons();
            CHECK(ons.size() >= 5 && ons[ons.size() - 1] == 72 && ons[ons.size() - 2] == 72, "replace: %s", join(ons).c_str());
            p.latch = false; // switching latch off with no keys down stops the pattern
            a.keyUp(72);
            const size_t before = log.ons().size();
            runArp(a, log, p, static_cast<int>(stepD * 3));
            CHECK(log.ons().size() == before && !a.isRunning(), "latch off stops: %s", join(log.ons()).c_str());
        }
        {
            Arpeggiator a;
            ArpLog log;
            ArpParams p;
            a.beginBlock(p, 120.0, kRate, -1.0);
            a.setPedal(true);
            a.keyDown(60, 0.8f);
            a.keyUp(60);
            runArp(a, log, p, static_cast<int>(stepD * 2));
            CHECK(log.ons().size() == 2, "pedal holds the key: %d", (int)log.ons().size());
            a.setPedal(false);
            runArp(a, log, p, static_cast<int>(stepD * 2));
            CHECK(log.ons().size() == 2, "pedal up releases: %d", (int)log.ons().size());
            const auto& last = log.ev.back();
            CHECK(!last.on, "last event is a note-off");
        }
        {
            // Host playing from ppq 0.1: the first step waits for the 1/16 grid at 0.25.
            Arpeggiator a;
            ArpLog log;
            ArpParams p;
            a.beginBlock(p, 120.0, kRate, 0.1);
            a.keyDown(60, 0.8f);
            runArp(a, log, p, static_cast<int>(stepD * 3), 120.0, 0.1);
            const auto t = log.onTimes();
            const double samplesPerQuarter = kRate * 0.5;
            CHECK(t.size() >= 2 && std::llabs(t[0] - std::llround(0.15 * samplesPerQuarter)) <= 1 &&
                      std::llabs(t[1] - std::llround(0.40 * samplesPerQuarter)) <= 1,
                  "grid: %s", join(t).c_str());
            // A key pressed just after a grid line still catches that step.
            Arpeggiator b;
            ArpLog lb;
            b.beginBlock(p, 120.0, kRate, 0.26);
            b.keyDown(60, 0.8f);
            runArp(b, lb, p, 100, 120.0, 0.26);
            CHECK(!lb.onTimes().empty() && lb.onTimes()[0] == 0, "late press plays at once");
        }
    });

    test("arpeggiator in the engine: plays, follows enable, releases cleanly", [] {
        Engine e;
        Patch p = makeInitPatch();
        p.arp.enabled = true;
        p.arp.syncDiv = 14; // 1/8
        p.osc[0].env.sustain = 1.0f;
        e.patch() = p;
        e.prepare(kRate);
        e.setTempo(120.0);
        e.noteOn(60, 0.8f);
        e.noteOn(67, 0.8f);
        Render r = render(e, static_cast<int>(kRate * 1.0));
        CHECK(r.finite() && r.peak() > 0.05f, "arp audible: peak %f", r.peak());
        CHECK(e.arpRunning(), "running");
        // Pitch alternates: zero-crossing rate over the first and second eighth differs (60 vs 67).
        auto crossings = [&](int from, int len) {
            int c = 0;
            for (int i = from + 1; i < from + len; ++i) c += (r.l[static_cast<size_t>(i - 1)] < 0.0f) != (r.l[static_cast<size_t>(i)] < 0.0f);
            return c;
        };
        const int eighth = static_cast<int>(kRate * 0.25);
        const int a = crossings(200, eighth / 2 - 400), b = crossings(eighth + 200, eighth / 2 - 400);
        CHECK(b > a * 1.3, "second step higher: %d vs %d crossings", a, b);

        // Switching the arp off hands the held keys back to the keyboard (two voices sustain).
        e.patch().arp.enabled = false;
        render(e, static_cast<int>(kRate * 0.5));
        CHECK(e.activeVoiceCount() >= 2, "held keys play directly: voices %d", e.activeVoiceCount());
        e.patch().arp.enabled = true;
        render(e, 1024);
        e.noteOff(60);
        e.noteOff(67);
        Render tail = render(e, static_cast<int>(kRate * 3.0));
        CHECK(!e.arpRunning() && e.activeVoiceCount() == 0, "stops after release: voices %d", e.activeVoiceCount());
        CHECK(tail.finite(), "finite");
    });
    test("MPE: per-note bend, pressure and timbre stay on their own notes", [] {
        auto crossings = [](const Render& r) {
            int c = 0;
            for (size_t i = 1; i < r.l.size(); ++i) c += (r.l[i - 1] < 0.0f) != (r.l[i] < 0.0f);
            return c;
        };
        auto rms = [](const Render& r) {
            double s = 0.0;
            for (float x : r.l) s += static_cast<double>(x) * x;
            return std::sqrt(s / static_cast<double>(r.l.size()));
        };
        auto fresh = [](Engine& e) {
            Patch p = makeInitPatch();
            p.voice.mpe = true;
            p.voice.mpeBendRange = 24;
            e.patch() = p;
            e.prepare(kRate);
        };
        const int half = static_cast<int>(kRate * 0.5);
        {
            Engine e;
            fresh(e);
            e.noteOn(60, 0.8f, 2);
            render(e, 2048);
            const int base = crossings(render(e, half));
            e.channelPitchBend(3, 1.0f); // another note's channel: no effect
            render(e, 2048);
            const int other = crossings(render(e, half));
            e.channelPitchBend(2, 0.5f); // +12 semitones at a 24-semitone range
            render(e, 2048);
            const int bent = crossings(render(e, half));
            CHECK(std::abs(other - base) <= 2, "other channel moved the note: %d vs %d", other, base);
            CHECK(std::fabs(static_cast<float>(bent) / static_cast<float>(base) - 2.0f) < 0.03f, "bend: %d vs %d", bent, base);
            // A bend sent before the note-on applies to the note (MPE controllers do this).
            e.channelPitchBend(4, 0.5f);
            e.noteOff(60, 0.0f, 2);
            render(e, static_cast<int>(kRate));
            e.noteOn(60, 0.8f, 4);
            render(e, 2048);
            const int pre = crossings(render(e, half));
            CHECK(std::fabs(static_cast<float>(pre) / static_cast<float>(base) - 2.0f) < 0.03f, "pre-bend: %d vs %d", pre, base);
        }
        {
            // Pressure drives the Poly AT routings of its own note only.
            Engine e;
            fresh(e);
            e.patch().osc[0].initLevel = 0.15f;
            e.patch().osc[0].polyAT = 0.85f;
            e.noteOn(60, 0.8f, 2);
            e.channelPressure(3, 1.0f);
            render(e, 4096);
            const double quiet = rms(render(e, half));
            e.channelPressure(2, 1.0f);
            render(e, 4096);
            const double loud = rms(render(e, half));
            CHECK(loud > quiet * 4.0, "pressure: %f -> %f", quiet, loud);
        }
        {
            // Timbre (CC74 on the note's channel) as a mod source.
            Engine e;
            fresh(e);
            e.patch().osc[0].initLevel = 0.15f;
            e.patch().mod[0] = ModSlot{ src::Timbre, dst::osc(0, dst::OscLevel), 0.85f };
            e.noteOn(60, 0.8f, 5);
            render(e, 4096);
            const double before = rms(render(e, half));
            e.channelTimbre(5, 1.0f);
            render(e, 4096);
            const double after = rms(render(e, half));
            CHECK(after > before * 3.0, "timbre: %f -> %f", before, after);
            // Without MPE, CC74 drives the same source for every note.
            Engine g;
            Patch p = makeInitPatch();
            p.osc[0].initLevel = 0.15f;
            p.mod[0] = ModSlot{ src::Timbre, dst::osc(0, dst::OscLevel), 0.85f };
            g.patch() = p;
            g.prepare(kRate);
            g.noteOn(60, 0.8f);
            render(g, 4096);
            const double g0 = rms(render(g, half));
            g.controlChange(74, 1.0f);
            render(g, 4096);
            CHECK(rms(render(g, half)) > g0 * 3.0, "global CC74 timbre");
        }
        {
            // The same key on two channels: each note-off releases only its own note.
            Engine e;
            fresh(e);
            e.noteOn(60, 0.8f, 2);
            render(e, 256);
            e.noteOff(60, 0.0f, 3); // not the owner
            render(e, static_cast<int>(kRate));
            CHECK(e.activeVoiceCount() == 1, "foreign note-off ignored: %d", e.activeVoiceCount());
            e.noteOff(60, 0.0f, 2);
            render(e, static_cast<int>(kRate * 2));
            CHECK(e.activeVoiceCount() == 0, "owner releases: %d", e.activeVoiceCount());
        }
    });
    test("benchmark: 16 voices x 8 oscillators, 2x oversampling", [] {
        Engine e;
        Patch p = busyPatch(7, OscMode::PhaseMod);
        p.dfilt.type = DigitalFilterType::LpLadder24;
        for (int k = 0; k < kNumLfos; ++k) p.lfo[static_cast<size_t>(k)].rateHz = 3.0f + k;
        p.mod[0] = ModSlot{ src::Lfo1, dst::DFiltBase + dst::DfCutoff, 0.2f };
        p.mod[1] = ModSlot{ src::Env1 + 3, dst::AllOscBase + dst::OscLevel, 0.3f };
        p.fx.pre.type = FxType::Chorus;
        p.fx.post.type = FxType::Eq;
        p.fx.delay.enabled = true;
        p.fx.reverb.enabled = true;
        e.patch() = p;
        e.prepare(kRate);
        for (int n = 0; n < kMaxVoices; ++n) e.noteOn(36 + n * 3, 0.8f);
        const int seconds = 5;
        const auto t0 = std::chrono::steady_clock::now();
        Render r = render(e, static_cast<int>(kRate) * seconds, 256);
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const double load = 100.0 * elapsed / seconds;
        std::printf("  CPU: %.1f%% of one core (%.3f s for %d s of audio)\n", load, elapsed, seconds);
        CHECK(r.finite(), "non-finite");
        CHECK(e.activeVoiceCount() == kMaxVoices, "voices=%d", e.activeVoiceCount());
    });

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
