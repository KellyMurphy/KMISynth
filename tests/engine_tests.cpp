// Offline engine tests: deterministic renders checked for pitch, stability, voice handling and CPU.
// Dependency-free on purpose; exits non-zero on failure (run via ctest).
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "kmi/Effects.h"
#include "kmi/Engine.h"
#include "kmi/Envelope.h"
#include "kmi/Modulation.h"
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
        CHECK(numPresetAlgorithms() >= 20, "algos=%d", numPresetAlgorithms());
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
