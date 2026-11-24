// CPU breakdown benchmark: renders 16 held voices under progressively heavier configurations.
// Usage: kmi_bench [seconds]   (reports % of one core at 48 kHz)
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>

#include "kmi/Dsp.h"
#include "kmi/Engine.h"
#include "kmi/Envelope.h"
#include "kmi/Filters.h"
#include "kmi/Lfo.h"
#include "kmi/Wavetables.h"

using namespace kmi;

namespace {
constexpr double kRate = 48000.0;

// Best of 3 runs: laptop boost clocks make single runs swing by 10+ points.
double measure(const Patch& p, int seconds)
{
    double best = 1e9;
    for (int run = 0; run < 3; ++run)
    {
        Engine e;
        e.patch() = p;
        e.prepare(kRate);
        for (int n = 0; n < kMaxVoices; ++n) e.noteOn(36 + n * 3, 0.8f);
        std::vector<float> l(256), r(256);
        const int blocks = static_cast<int>(kRate) * seconds / 256;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) e.render(l.data(), r.data(), 256);
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        best = std::min(best, 100.0 * elapsed / seconds);
    }
    return best;
}

Patch eightOscs(OscMode mode)
{
    Patch p = makeInitPatch();
    p.algo.index = 7; // Many Into One
    for (int i = 0; i < kNumOscs; ++i)
    {
        auto& o = p.osc[static_cast<size_t>(i)];
        o.initLevel = 0.8f;
        o.mode = mode;
        o.wave = i % WavetableBank::instance().numWaves();
        o.pitchMode = PitchMode::Ratio;
        o.ratio = static_cast<float>(1 + i % 4);
        o.feedback = 0.5f;
    }
    return p;
}
} // namespace

int main(int argc, char** argv)
{
    const int seconds = argc > 1 ? std::max(1, std::atoi(argv[1])) : 3;
    struct Case
    {
        const char* name;
        std::function<Patch()> make;
    };
    const std::vector<Case> cases = {
        { "1 osc (init patch)", [] { return makeInitPatch(); } },
        { "1 osc, 1x oversampling", [] { Patch p = makeInitPatch(); p.oversampling = 1; return p; } },
        { "1 osc, digital LP 1-pole", [] { Patch p = makeInitPatch(); p.dfilt.type = DigitalFilterType::Lp1Pole; return p; } },
        { "1 osc, digital SVF", [] { Patch p = makeInitPatch(); p.dfilt.type = DigitalFilterType::SvfLpBpHp; return p; } },
        { "8 oscs PM", [] { return eightOscs(OscMode::PhaseMod); } },
        { "8 oscs FM", [] { return eightOscs(OscMode::FreqMod); } },
        { "8 oscs PD Saw", [] { return eightOscs(OscMode::PDSaw); } },
        { "8 oscs PM + 5 poly LFOs", [] {
              Patch p = eightOscs(OscMode::PhaseMod);
              for (int k = 0; k < kNumLfos; ++k) p.lfo[static_cast<size_t>(k)].rateHz = 3.0f + k;
              p.mod[0] = ModSlot{ src::Lfo1, dst::DFiltBase + dst::DfCutoff, 0.2f };
              return p;
          } },
        { "8 oscs PM + 4 mod routes", [] {
              Patch p = eightOscs(OscMode::PhaseMod);
              p.mod[0] = ModSlot{ src::Lfo1, dst::DFiltBase + dst::DfCutoff, 0.2f };
              p.mod[1] = ModSlot{ src::Env1 + 3, dst::AllOscBase + dst::OscLevel, 0.3f };
              p.mod[2] = ModSlot{ src::ModWheel, dst::osc(1, dst::OscPitch), 0.1f };
              p.mod[3] = ModSlot{ src::Velocity, dst::AFiltBase + dst::AfCutoff, 0.3f };
              return p;
          } },
        { "8 oscs PM + full FX", [] {
              Patch p = eightOscs(OscMode::PhaseMod);
              p.fx.pre.type = FxType::Chorus;
              p.fx.post.type = FxType::Eq;
              p.fx.delay.enabled = true;
              p.fx.reverb.enabled = true;
              return p;
          } },
    };

    std::printf("%-32s %8s\n", "configuration (16 voices)", "CPU %");
    for (const auto& c : cases) std::printf("%-32s %8.1f\n", c.name, measure(c.make(), seconds));

    // Per-call cost of the per-sample building blocks.
    std::printf("\n%-32s %8s\n", "component (per sample)", "ns");
    const int N = 20'000'000;
    volatile float sink = 0.0f;
    auto time = [&](const char* name, const std::function<float(int)>& fn) {
        const DenormalGuard g;
        float acc = 0.0f;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < N; ++i) acc += fn(i);
        const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / N;
        sink = sink + acc;
        std::printf("%-32s %8.2f\n", name, ns);
    };
    {
        Envelope env;
        EnvParams p;
        p.attackMs = 1e6f; // stays in a curved stage
        p.attackCurve = 0.5f;
        env.update(p, EnvMods{}, 96000.0f, 120.0);
        env.noteOn(p, 1.0f, false, false);
        time("Envelope::tick (curved)", [&](int) { return env.tick(); });
    }
    {
        Lfo lfo;
        lfo.reset(1);
        LfoParams p;
        lfo.update(p, nullptr, 1.0f, 0.0f, 96000.0f, 120.0);
        time("Lfo::tick (sine)", [&](int) { return lfo.tick(); });
    }
    {
        const float* t = WavetableBank::instance().table(3, 0);
        float ph = 0.0f;
        time("wavetable read + wrap", [&](int) { ph = wrap01(ph + 0.0123f); return WavetableBank::read(t, ph); });
    }
    {
        Ladder lad;
        float y[4];
        const float g = hzToG(2000.0f, 96000.0f);
        time("Ladder::process", [&](int i) { lad.process((i & 64) ? 0.5f : -0.5f, g, 2.0f, 0.0f, 0.5f, y); return y[3]; });
    }
    {
        DigitalFilter f;
        DigitalFilterBlock b;
        b.type = DigitalFilterType::LpLadder24;
        f.prepareBlock(b, 96000.0f);
        const float g = hzToG(2000.0f, 96000.0f);
        time("DigitalFilter (LP Ladder 24)", [&](int i) {
            if ((i & 31) == 0) f.beginRamp(g, g, 32);
            return f.process((i & 64) ? 0.5f : -0.5f);
        });
    }
    {
        AnalogFilter f;
        f.prepareBlock(0.3f, 0.125f);
        const float g = hzToG(2000.0f, 96000.0f);
        time("AnalogFilter", [&](int i) {
            if ((i & 31) == 0) f.beginRamp(g, g, 32);
            return f.process((i & 64) ? 0.5f : -0.5f);
        });
    }
    return 0;
}
