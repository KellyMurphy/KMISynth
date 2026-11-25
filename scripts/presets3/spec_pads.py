from common import preset

PRESETS = [
    # ---------------------------------------------------------------------------------------------- Pad (14)
    preset("Lattice Pad", "Pad", "Lattice algorithm: three tiers of modulators, the top ones drifting on slow LFOs.", 60, True,
           "pad, fm, evolving, slow", r'''
Patch p = blank("Lattice");
osc(p, 1, Sine, 1.0f, 0.4f);
osc(p, 2, Sine, 2.0f, 0.25f).ratioFine = 0.2f;
modOsc(p, 3, Sine, 1.0f, 0.08f);
modOsc(p, 4, Sine, 2.0f, 0.06f);
modOsc(p, 5, Sine, 3.0f, 0.05f);
modOsc(p, 6, Sine, 1.0f, 0.05f).ratioFine = 0.3f;
modOsc(p, 7, Sine, 0.5f, 0.04f);
modOsc(p, 8, Sine, 4.0f, 0.03f);
lfo(p, 4, LfoWave::Sine, 0.07f, LfoTrig::Free);
lfo(p, 5, LfoWave::Triangle, 0.11f, LfoTrig::Free);
route(p, lfoUni(4), oscLevel(7), 0.08f);
route(p, lfoUni(5), oscLevel(8), 0.06f);
amp(p, 900.0f, 1000.0f, 0.9f, 2500.0f);
chorus(p, 0.4f);
room(p, ReverbType::Hall, 4.0f, 0.35f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.1f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Braided Strings", "Pad", "Braid algorithm: two string carriers woven from six crossing modulators.", 60, True,
           "pad, strings, fm, warm", r'''
Patch p = blank("Braid");
osc(p, 1, Saw, 1.0f, 0.3f);
osc(p, 2, Saw, 1.0f, 0.3f).ratioFine = 0.4f;
modOsc(p, 3, Sine, 1.0f, 0.05f);
modOsc(p, 4, Sine, 2.0f, 0.04f);
modOsc(p, 5, Sine, 1.0f, 0.04f).ratioFine = -0.2f;
modOsc(p, 6, Sine, 3.0f, 0.03f);
modOsc(p, 7, Sine, 0.5f, 0.03f);
modOsc(p, 8, Sine, 5.0f, 0.02f);
analogLp(p, 2500.0f, 0.1f);
amp(p, 600.0f, 1000.0f, 0.9f, 1500.0f);
vibrato(p, 4.8f, 0.03f, 800.0f);
chorus(p, 0.45f);
room(p, ReverbType::Hall, 3.0f, 0.3f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(dst::ModulatorLevel, 0.1f) });
'''),
    preset("Section Strings", "Pad", "String Section algorithm: four voices, each pair sharing a bowing modulator.", 60, True,
           "pad, strings, ensemble, fm", r'''
Patch p = blank("String Section");
osc(p, 1, fam(SawBrightF, 9), 1.0f, 0.3f);
osc(p, 2, fam(SawBrightF, 9), 1.0f, 0.3f).ratioFine = 0.35f;
osc(p, 3, fam(SawBrightF, 7), 2.0f, 0.15f).ratioFine = -0.25f;
osc(p, 4, fam(SawBrightF, 7), 0.5f, 0.2f);
modOsc(p, 5, Sine, 1.0f, 0.05f);
modOsc(p, 6, Sine, 2.0f, 0.04f);
modOsc(p, 7, Sine, 3.0f, 0.03f);
modOsc(p, 8, Sine, 1.0f, 0.03f);
amp(p, 400.0f, 800.0f, 0.9f, 900.0f);
analogLp(p, 3500.0f, 0.1f);
vibrato(p, 5.2f, 0.04f, 600.0f);
chorus(p, 0.4f);
room(p, ReverbType::Hall, 2.8f, 0.3f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvAttack), 0.4f) });
'''),
    preset("Sideband Choir", "Pad", "Four By Four: four modulators feed four carriers, slowly trading places on free LFOs.", 60, True,
           "pad, fm, choir, evolving", r'''
Patch p = blank("Four By Four");
osc(p, 1, Sine, 1.0f, 0.3f);
osc(p, 2, Sine, 1.0f, 0.25f).ratioFine = 0.3f;
osc(p, 3, Sine, 2.0f, 0.15f);
osc(p, 4, Sine, 3.0f, 0.1f);
modOsc(p, 5, Sine, 1.0f, 0.03f);
modOsc(p, 6, Sine, 2.0f, 0.03f);
modOsc(p, 7, Sine, 3.0f, 0.02f);
modOsc(p, 8, Sine, 5.0f, 0.02f);
lfo(p, 4, LfoWave::Sine, 0.09f, LfoTrig::Free);
lfo(p, 5, LfoWave::Sine, 0.13f, LfoTrig::Free).phaseDeg = 120.0f;
route(p, lfoUni(4), oscLevel(5), 0.05f);
route(p, lfoUni(5), oscLevel(7), 0.04f);
route(p, lfoSrc(4), oscLevel(8), 0.03f);
amp(p, 800.0f, 1000.0f, 0.9f, 2000.0f);
room(p, ReverbType::Hall, 4.0f, 0.35f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.08f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Choir Drift", "Pad", "Two Vowel Morph tables drift through the vowels at different rates; Unison Poly.", 60, True,
           "wavetable, choir, vowel, evolving", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, VowelMorphF, 1.0f, 0.35f);
famOsc(p, 2, VowelMorphF, 1.0f, 0.3f, 0.5f).ratioFine = 0.3f;
lfo(p, 4, LfoWave::Triangle, 0.08f, LfoTrig::Free);
lfo(p, 5, LfoWave::Triangle, 0.05f, LfoTrig::Free).phaseDeg = 180.0f;
route(p, lfoUni(4), oscScan(1), 1.0f);
route(p, lfoSrc(5), oscScan(2), 0.5f);
unison(p, 2, 0.2f);
amp(p, 700.0f, 1000.0f, 0.9f, 1800.0f);
chorus(p, 0.4f);
room(p, ReverbType::Hall, 3.5f, 0.35f);
macro(p, 1, 0.0f, { to(dst::LfoBase + 3 * dst::kLfoParams + dst::LfoRate, 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Formant Dawn", "Pad", "Formant Sweep table: each chord rises slowly through its harmonics, like a sunrise.", 60, True,
           "wavetable, formant, swell, slow", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, FormantSweepF, 1.0f, 0.35f);
famOsc(p, 2, FormantSweepF, 1.0f, 0.3f).ratioFine = -0.3f;
osc(p, 3, Sine, 0.5f, 0.15f);
adsr(p.env[3], 2500.0f, 2000.0f, 0.6f, 2000.0f);
route(p, envSrc(4), oscScan(1), 0.8f);
route(p, envSrc(4), oscScan(2), 0.6f);
amp(p, 600.0f, 1000.0f, 0.9f, 2000.0f);
chorus(p, 0.35f);
room(p, ReverbType::Hall, 4.0f, 0.35f);
macro(p, 1, 0.0f, { to(dst::env(3, dst::EnvAttack), 0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Harmonic Sunrise", "Pad", "Harmonic Build table: harmonics arrive one by one as the chord is held.", 60, True,
           "wavetable, additive, swell, slow", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, HarmonicBuildF, 1.0f, 0.35f);
famOsc(p, 2, HarmonicBuildF, 2.0f, 0.15f).ratioFine = 0.15f;
osc(p, 3, Sine, 1.0f, 0.2f).ratioFine = -0.2f;
adsr(p.env[3], 3500.0f, 1000.0f, 1.0f, 2000.0f);
route(p, envSrc(4), oscScan(1), 1.0f);
route(p, envSrc(4), oscScan(2), 0.7f);
amp(p, 500.0f, 1000.0f, 0.9f, 2000.0f);
chorus(p, 0.35f);
room(p, ReverbType::Hall, 4.0f, 0.3f);
macro(p, 1, 0.0f, { to(dst::env(3, dst::EnvAttack), 0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Comb Tides", "Pad", "Comb Sweep tables scanned by slow LFOs through a phaser: a moving, jet-like wash.", 60, True,
           "wavetable, comb, phaser, evolving", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, CombSweepF, 1.0f, 0.35f);
famOsc(p, 2, CombSweepF, 1.0f, 0.3f).ratioFine = 0.3f;
lfo(p, 4, LfoWave::Sine, 0.06f, LfoTrig::Free);
lfo(p, 5, LfoWave::Sine, 0.09f, LfoTrig::Free).phaseDeg = 90.0f;
route(p, lfoUni(4), oscScan(1), 1.0f);
route(p, lfoUni(5), oscScan(2), 1.0f);
amp(p, 700.0f, 1000.0f, 0.9f, 1800.0f);
phaser(p, 0.1f, 0.5f, 0.4f);
room(p, ReverbType::Hall, 3.5f, 0.3f);
macro(p, 1, 0.0f, { to(fx(dst::PostFxMix), 0.4f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Spectral Drift", "Pad", "Spectra table on a smoothed random LFO: shifting clouds of partials.", 60, True,
           "wavetable, spectral, random, ambient", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, SpectraF, 1.0f, 0.35f, 0.3f);
famOsc(p, 2, SpectraF, 2.0f, 0.15f, 0.6f);
osc(p, 3, Sine, 0.5f, 0.2f);
LfoParams& l = lfo(p, 4, LfoWave::Random, 0.2f, LfoTrig::Free);
l.smooth = 0.8f;
route(p, lfoSrc(4), oscScan(1), 0.4f);
route(p, lfoSrc(4), oscScan(2), -0.4f);
amp(p, 800.0f, 1000.0f, 0.9f, 2000.0f);
chorus(p, 0.4f);
room(p, ReverbType::Cloud, 6.0f, 0.35f);
macro(p, 1, 0.0f, { to(dst::LfoBase + 3 * dst::kLfoParams + dst::LfoRate, 0.4f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Pulse Strings", "Pad", "Two Pulse Width tables swept in opposite directions: a lush PWM string machine.", 60, True,
           "wavetable, pwm, strings, vintage", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, PulseWidthF, 1.0f, 0.3f, 0.3f);
famOsc(p, 2, PulseWidthF, 1.0f, 0.3f, 0.3f).ratioFine = 0.3f;
lfo(p, 4, LfoWave::Triangle, 0.5f, LfoTrig::Free);
route(p, lfoSrc(4), oscScan(1), 0.3f);
route(p, lfoSrc(4), oscScan(2), -0.3f);
analogLp(p, 2500.0f, 0.1f);
amp(p, 350.0f, 800.0f, 0.9f, 900.0f);
chorus(p, 0.5f);
room(p, ReverbType::Hall, 2.5f, 0.25f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::PostFxMix), 0.4f) });
'''),
    preset("Hourglass", "Pad", "Hourglass algorithm: three modulators narrow to one, then widen again into the carrier.", 60, True,
           "pad, fm, glassy, evolving", r'''
Patch p = blank("Hourglass");
osc(p, 1, Sine, 1.0f, 0.4f);
modOsc(p, 2, Sine, 1.0f, 0.08f);
modOsc(p, 3, Sine, 2.0f, 0.06f).ratioFine = 0.2f;
modOsc(p, 4, Sine, 1.0f, 0.06f);
modOsc(p, 5, Sine, 3.0f, 0.04f);
modOsc(p, 6, Sine, 0.5f, 0.04f);
modOsc(p, 7, Sine, 5.0f, 0.03f);
osc(p, 8, Sine, 2.0f, 0.15f).ratioFine = -0.2f;
lfo(p, 4, LfoWave::Sine, 0.1f, LfoTrig::Free);
route(p, lfoUni(4), oscLevel(4), 0.06f);
amp(p, 900.0f, 1000.0f, 0.9f, 2200.0f);
room(p, ReverbType::Hall, 4.0f, 0.35f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.08f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Twin Webs", "Pad", "Two Webs algorithm: two small FM webs, each with two voices, panned in Osc Stereo.", 60, True,
           "pad, fm, osc stereo, wide", r'''
Patch p = blank("Two Webs");
osc(p, 1, Sine, 1.0f, 0.3f).pan = -0.7f;
osc(p, 2, Sine, 2.0f, 0.15f).pan = -0.4f;
modOsc(p, 3, Sine, 1.0f, 0.06f);
modOsc(p, 4, Sine, 3.0f, 0.04f);
osc(p, 5, Sine, 1.0f, 0.3f).pan = 0.7f;
osc(p, 6, Sine, 2.0f, 0.15f).pan = 0.4f;
modOsc(p, 7, Sine, 1.0f, 0.06f).ratioFine = 0.3f;
modOsc(p, 8, Sine, 4.0f, 0.03f);
stereoOscs(p, 6.0f);
amp(p, 700.0f, 1000.0f, 0.9f, 1800.0f);
room(p, ReverbType::Hall, 3.5f, 0.3f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.08f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Mirror Pad", "Pad", "Mirrored Fans: three modulators into one carrier, one modulator into three.", 60, True,
           "pad, fm, warm, slow", r'''
Patch p = blank("Mirrored Fans");
osc(p, 1, Sine, 1.0f, 0.35f);
modOsc(p, 2, Sine, 1.0f, 0.06f);
modOsc(p, 3, Sine, 2.0f, 0.04f);
modOsc(p, 4, Sine, 3.0f, 0.03f);
osc(p, 5, Triangle, 1.0f, 0.2f).ratioFine = 0.3f;
osc(p, 6, Triangle, 2.0f, 0.12f).ratioFine = -0.2f;
osc(p, 7, Sine, 0.5f, 0.15f);
modOsc(p, 8, Sine, 1.0f, 0.04f);
analogLp(p, 3000.0f, 0.1f, 0.2f, 1500.0f, 2000.0f, 0.5f, 1500.0f);
amp(p, 800.0f, 1000.0f, 0.9f, 2000.0f);
chorus(p, 0.35f);
room(p, ReverbType::Hall, 3.5f, 0.3f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(dst::ModulatorLevel, 0.08f) });
'''),
    preset("Rewired Dusk", "Pad", "Algorithm morph across four of the new shapes: the pad slowly rewires itself.", 60, True,
           "pad, morph, evolving, slow", r'''
Patch p = blank("Ring Over Stack");
static const char* const slots[4] = { "Ring Over Stack", "Hourglass", "Tower, Middle Heard", "Mirrored Fans" };
p.algo.mode = AlgoMode::Morph;
p.algo.morphSlots.fill(kMorphSlotOff);
for (int i = 0; i < 4; ++i) p.algo.morphSlots[static_cast<size_t>(i)] = kMorphSlotFirstPreset + findPresetAlgorithm(slots[i]);
osc(p, 1, Sine, 1.0f, 0.35f);
for (int i = 2; i <= 8; ++i) osc(p, i, Sine, static_cast<float>(i % 4 + 1), 0.05f).ratioFine = 0.1f * static_cast<float>(i);
p.osc[4].initLevel = 0.15f;
p.osc[7].initLevel = 0.12f;
lfo(p, 4, LfoWave::Triangle, 0.04f, LfoTrig::Free);
route(p, lfoUni(4), dst::AlgoMorph, 3.0f / 7.0f);
amp(p, 1000.0f, 1000.0f, 0.9f, 2500.0f);
chorus(p, 0.35f);
room(p, ReverbType::Hall, 4.5f, 0.35f);
macro(p, 1, 0.0f, { to(dst::AlgoMorph, 0.4f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),

    # ---------------------------------------------------------------------------------------------- Drone (10)
    preset("Six Ring Drone", "Drone", "Ring of Six: six operators modulate each other in a loop, slowly churning.", 41, True,
           "drone, fm, feedback, dark", r'''
Patch p = blank("Ring of Six");
osc(p, 1, Sine, 1.0f, 0.4f);
for (int i = 2; i <= 6; ++i) modOsc(p, i, Sine, i % 2 ? 1.0f : 2.0f, 0.04f).ratioFine = 0.07f * static_cast<float>(i);
osc(p, 7, Sine, 0.5f, 0.2f);
osc(p, 8, Sine, 2.0f, 0.08f);
lfo(p, 4, LfoWave::Sine, 0.05f, LfoTrig::Free);
route(p, lfoUni(4), dst::ModulatorLevel, 0.06f);
amp(p, 600.0f, 1000.0f, 1.0f, 4000.0f);
room(p, ReverbType::Cloud, 7.0f, 0.4f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.08f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Seven Ring Hum", "Drone", "Ring of Seven with a free sub: a deep hum with slowly beating operators.", 36, True,
           "drone, fm, hum, deep", r'''
Patch p = blank("Ring of Seven");
osc(p, 1, Sine, 1.0f, 0.45f);
for (int i = 2; i <= 7; ++i) modOsc(p, i, Sine, 1.0f, 0.03f).ratioFine = 0.05f * static_cast<float>(i);
osc(p, 8, Sine, 0.5f, 0.3f);
amp(p, 700.0f, 1000.0f, 1.0f, 4000.0f);
analogLp(p, 1200.0f, 0.1f);
lfo(p, 2, LfoWave::Sine, 0.07f, LfoTrig::Free);
p.afilt.lfoAmt = 0.15f;
room(p, ReverbType::Hall, 6.0f, 0.35f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.06f) });
macro(p, 2, 0.0f, { to(afCut, 0.3f) });
'''),
    preset("Mesh Hum", "Drone", "Full Mesh Four: four operators all modulate each other; four sines hold it down.", 41, True,
           "drone, fm, mesh, chaotic", r'''
Patch p = blank("Full Mesh Four");
osc(p, 1, Sine, 1.0f, 0.3f);
modOsc(p, 2, Sine, 1.0f, 0.03f).ratioFine = 0.2f;
modOsc(p, 3, Sine, 2.0f, 0.025f);
modOsc(p, 4, Sine, 3.0f, 0.02f);
osc(p, 5, Sine, 0.5f, 0.3f);
osc(p, 6, Sine, 1.0f, 0.15f).ratioFine = -0.3f;
osc(p, 7, Sine, 1.5f, 0.1f);
osc(p, 8, Sine, 2.0f, 0.08f);
lfo(p, 4, LfoWave::Triangle, 0.03f, LfoTrig::Free);
route(p, lfoUni(4), dst::ModulatorLevel, 0.05f);
amp(p, 600.0f, 1000.0f, 1.0f, 4000.0f);
room(p, ReverbType::Cloud, 7.0f, 0.4f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.06f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Dense Undertow", "Drone", "Dense Six: six operators in a full cascade pull a low drone under the surface.", 36, True,
           "drone, fm, dense, dark", r'''
Patch p = blank("Dense Six");
osc(p, 1, Sine, 1.0f, 0.4f);
for (int i = 2; i <= 6; ++i) modOsc(p, i, Sine, static_cast<float>(i - 1), 0.025f);
osc(p, 7, Sine, 0.5f, 0.3f);
osc(p, 8, Triangle, 1.0f, 0.1f).ratioFine = 0.4f;
analogLp(p, 900.0f, 0.2f);
lfo(p, 2, LfoWave::Sine, 0.05f, LfoTrig::Free);
p.afilt.lfoAmt = 0.2f;
amp(p, 700.0f, 1000.0f, 1.0f, 4000.0f);
room(p, ReverbType::Hall, 6.0f, 0.35f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.05f) });
macro(p, 2, 0.0f, { to(afCut, 0.3f) });
'''),
    preset("Fold Horizon", "Drone", "Wavefold table scanned by a slow LFO: the drone folds and unfolds over half a minute.", 41, True,
           "wavetable, wavefold, drone, slow", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, WavefoldF, 1.0f, 0.35f);
famOsc(p, 2, WavefoldF, 0.5f, 0.3f, 0.2f);
osc(p, 3, Sine, 1.0f, 0.15f).ratioFine = 0.3f;
lfo(p, 4, LfoWave::Triangle, 0.035f, LfoTrig::Free);
route(p, lfoUni(4), oscScan(1), 0.8f);
route(p, lfoUni(4), oscScan(2), -0.2f);
amp(p, 700.0f, 1000.0f, 1.0f, 4000.0f);
room(p, ReverbType::Cloud, 6.0f, 0.35f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Valve Hum", "Drone", "Tube Drive table on a wandering random LFO: a warm, unstable amplifier hum.", 36, True,
           "wavetable, drive, drone, warm", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, TubeDriveF, 1.0f, 0.4f, 0.3f);
osc(p, 2, Sine, 0.5f, 0.3f);
osc(p, 3, fam(TubeDriveF, 8), 1.5f, 0.1f);
LfoParams& l = lfo(p, 4, LfoWave::Random, 0.3f, LfoTrig::Free);
l.smooth = 0.9f;
route(p, lfoSrc(4), oscScan(1), 0.3f);
analogLp(p, 1500.0f, 0.1f);
amp(p, 700.0f, 1000.0f, 1.0f, 4000.0f);
room(p, ReverbType::Hall, 5.0f, 0.3f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.5f) });
macro(p, 2, 0.0f, { to(afCut, 0.3f) });
'''),
    preset("Overtone Chant", "Drone", "Vowel Morph and Formant Sweep tables: a held tone with whistling overtones moving above it.", 45, True,
           "wavetable, vowel, overtone, voice", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, VowelMorphF, 1.0f, 0.35f, 0.3f);
famOsc(p, 2, FormantSweepF, 1.0f, 0.25f, 0.5f);
osc(p, 3, Sine, 0.5f, 0.2f);
lfo(p, 4, LfoWave::Sine, 0.06f, LfoTrig::Free);
lfo(p, 5, LfoWave::Triangle, 0.11f, LfoTrig::Free);
route(p, lfoSrc(4), oscScan(1), 0.3f);
route(p, lfoSrc(5), oscScan(2), 0.4f);
amp(p, 600.0f, 1000.0f, 1.0f, 4000.0f);
room(p, ReverbType::Hall, 5.0f, 0.35f);
macro(p, 1, 0.0f, { to(oscScan(2), 0.4f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Harmonium Haze", "Drone", "Organ Stops table drifting slowly through registrations, under a hazy cloud.", 48, True,
           "wavetable, organ, drone, ambient", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, OrganStopsF, 1.0f, 0.35f);
famOsc(p, 2, OrganStopsF, 1.0f, 0.3f, 0.5f).ratioFine = 0.25f;
lfo(p, 4, LfoWave::Triangle, 0.04f, LfoTrig::Free);
route(p, lfoUni(4), oscScan(1), 1.0f);
route(p, lfoSrc(4), oscScan(2), -0.5f);
amp(p, 600.0f, 1000.0f, 1.0f, 4000.0f);
chorus(p, 0.4f);
room(p, ReverbType::Cloud, 6.0f, 0.4f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Deep Tower", "Drone", "Skip Tower: a seven-high stack with shortcuts, its top operators slowly breathing.", 36, True,
           "drone, fm, deep, evolving", r'''
Patch p = blank("Skip Tower");
osc(p, 1, Sine, 1.0f, 0.45f);
for (int i = 2; i <= 8; ++i) modOsc(p, i, Sine, i % 3 ? 1.0f : 2.0f, 0.03f).ratioFine = 0.04f * static_cast<float>(i);
lfo(p, 4, LfoWave::Sine, 0.04f, LfoTrig::Free);
lfo(p, 5, LfoWave::Sine, 0.07f, LfoTrig::Free);
route(p, lfoUni(4), oscLevel(8), 0.08f);
route(p, lfoUni(5), oscLevel(6), 0.06f);
amp(p, 700.0f, 1000.0f, 1.0f, 4000.0f);
room(p, ReverbType::Hall, 6.0f, 0.35f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.06f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Rust Drone", "Drone", "Crusher table and a LoFi insert: a corroded, buzzing drone that slowly degrades.", 41, True,
           "wavetable, lo-fi, bitcrush, drone", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, CrusherF, 1.0f, 0.3f, 0.3f);
osc(p, 2, Sine, 0.5f, 0.3f);
osc(p, 3, fam(CrusherF, 10), 1.0f, 0.12f).ratioFine = 0.5f;
lfo(p, 4, LfoWave::Triangle, 0.05f, LfoTrig::Free);
route(p, lfoUni(4), oscScan(1), 0.6f);
lofi(p, 8000.0f, 3000.0f);
amp(p, 600.0f, 1000.0f, 1.0f, 4000.0f);
room(p, ReverbType::Hall, 5.0f, 0.3f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.4f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),

    # ---------------------------------------------------------------------------------------------- Pluck (12)
    preset("Kalimba Tines", "Pluck", "Kalimba algorithm: a doubly-struck carrier and three more tines at stretched ratios.", 72, False,
           "fm, kalimba, pluck, woody", r'''
Patch p = blank("Kalimba");
osc(p, 1, Sine, 1.0f, 0.45f);
strike(p, 2, Sine, 3.0f, 0.2f, 120.0f, 0.7f);
strike(p, 3, Sine, 7.0f, 0.08f, 60.0f, 0.8f);
osc(p, 4, Sine, 5.4f, 0.1f);
strike(p, 5, Sine, 1.0f, 0.1f, 200.0f, 0.6f);
osc(p, 6, Sine, 2.0f, 0.12f);
strike(p, 7, Sine, 1.0f, 0.08f, 300.0f, 0.5f);
osc(p, 8, Sine, 0.5f, 0.06f);
amp(p, 0.0f, 900.0f, 0.0f, 400.0f);
p.vca.velEnv = 0.6f;
echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.18f);
room(p, ReverbType::Room, 1.3f, 0.2f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Buzz Sitar", "Pluck", "A droning, buzzing pluck: Comb Sweep table bridge buzz and a sympathetic octave.", 60, False,
           "wavetable, sitar, buzz, pluck", r'''
Patch p = blank("One Pair");
famOsc(p, 1, CombSweepF, 1.0f, 0.45f, 0.4f);
strike(p, 2, Sine, 1.0f, 0.2f, 600.0f, 0.6f).feedback = 0.4f;
osc(p, 3, Saw, 2.0f, 0.1f).ratioFine = 0.3f;
adsr(p.env[3], 0.0f, 1500.0f, 0.0f, 500.0f);
route(p, envSrc(4), oscScan(1), 0.5f);
amp(p, 0.0f, 2500.0f, 0.0f, 500.0f);
p.vca.velEnv = 0.5f;
room(p, ReverbType::Room, 1.4f, 0.2f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(oscLevel(3), 0.2f) });
'''),
    preset("Koto Pluck", "Pluck", "A bright koto-like string: a sharp FM pick and a fast pitch bend into the note.", 67, False,
           "fm, koto, pluck, bright", r'''
Patch p = blank("Two Pairs, Four Free");
osc(p, 1, Sine, 1.0f, 0.45f);
strike(p, 2, Sine, 3.0f, 0.25f, 200.0f, 0.7f);
osc(p, 3, Triangle, 2.0f, 0.15f);
strike(p, 4, Sine, 5.0f, 0.12f, 90.0f, 0.8f);
adsr(p.env[3], 0.0f, 50.0f, 0.0f, 50.0f);
route(p, envSrc(4), allPitch, -0.02f);
amp(p, 0.0f, 1500.0f, 0.0f, 400.0f);
p.vca.velEnv = 0.6f;
room(p, ReverbType::Hall, 2.0f, 0.22f);
macro(p, 1, 0.0f, { to(depthOf(0), -0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Nylon Harp", "Pluck", "A soft harp: round triangle strings, gentle FM pluck and a long hall.", 60, False,
           "fm, harp, soft, hall", r'''
Patch p = blank("Two Pairs, Four Free");
osc(p, 1, Triangle, 1.0f, 0.45f);
strike(p, 2, Sine, 2.0f, 0.15f, 300.0f, 0.6f);
osc(p, 3, Sine, 2.0f, 0.15f).ratioFine = 0.1f;
strike(p, 4, Sine, 1.0f, 0.12f, 500.0f, 0.5f);
amp(p, 0.0f, 2800.0f, 0.0f, 1200.0f);
p.env[2].decayCurve = 0.7f;
p.vca.velEnv = 0.6f;
room(p, ReverbType::Hall, 3.0f, 0.3f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Nylon Guitar", "Pluck", "A plucked nylon string: saw and triangle through a closing low-pass, short body.", 55, False,
           "pluck, guitar, nylon, warm", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, Saw, 1.0f, 0.35f);
osc(p, 2, Triangle, 1.0f, 0.3f).ratioFine = 0.15f;
osc(p, 3, Sine, 2.0f, 0.1f);
analogLp(p, 400.0f, 0.15f, 0.6f, 0.0f, 400.0f, 0.0f, 300.0f);
p.afilt.velEnv = 0.5f;
p.afilt.keytrack = 1.0f;
amp(p, 0.0f, 2000.0f, 0.0f, 300.0f);
p.vca.velEnv = 0.6f;
room(p, ReverbType::Room, 1.2f, 0.18f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(dst::env(1, dst::EnvDecay), 0.4f) });
'''),
    preset("Bright Banjo", "Pluck", "A twangy, metallic pick: Pulse Width table and a ringing, high modulator.", 64, False,
           "wavetable, banjo, twang, pluck", r'''
Patch p = blank("One Pair");
osc(p, 1, fam(PulseWidthF, 10), 1.0f, 0.4f);
strike(p, 2, Sine, 3.0f, 0.25f, 180.0f, 0.7f);
osc(p, 3, Sine, 2.0f, 0.1f);
analogLp(p, 2500.0f, 0.2f, 0.4f, 0.0f, 250.0f, 0.0f, 200.0f);
amp(p, 0.0f, 800.0f, 0.0f, 250.0f);
p.vca.velEnv = 0.6f;
room(p, ReverbType::Room, 1.0f, 0.15f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(afCut, 0.3f) });
'''),
    preset("Sync Drop Pluck", "Pluck", "Hard Sync table: each pluck starts at high sync and drops to the fundamental.", 60, False,
           "wavetable, hard sync, pluck, bright", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, HardSyncF, 1.0f, 0.4f);
osc(p, 2, Sine, 1.0f, 0.15f).ratioFine = 0.2f;
adsr(p.env[3], 0.0f, 300.0f, 0.0f, 100.0f);
route(p, envSrc(4), oscScan(1), 0.9f);
amp(p, 0.0f, 1200.0f, 0.0f, 300.0f);
p.vca.velEnv = 0.6f;
echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.2f);
room(p, ReverbType::Room, 1.2f, 0.18f);
macro(p, 1, 0.0f, { to(dst::env(3, dst::EnvDecay), 0.4f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Bent Pluck", "Pluck", "Phase Bend table pluck: a resonant zap that closes as the note dies.", 60, False,
           "wavetable, phase distortion, pluck, zap", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, PhaseBendF, 1.0f, 0.45f);
osc(p, 2, Sine, 0.5f, 0.15f);
adsr(p.env[3], 0.0f, 400.0f, 0.0f, 150.0f);
route(p, envSrc(4), oscScan(1), 1.0f);
amp(p, 0.0f, 1000.0f, 0.0f, 250.0f);
p.vca.velEnv = 0.6f;
echo(p, DelayType::Stereo, "1/8", 0.3f, 0.18f);
room(p, ReverbType::Room, 1.0f, 0.15f);
macro(p, 1, 0.0f, { to(depthOf(0), -0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Talking Pluck", "Pluck", "Formant Sweep table pluck: every note says wow as it decays.", 60, False,
           "wavetable, formant, pluck, talking", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, FormantSweepF, 1.0f, 0.45f, 0.1f);
osc(p, 2, Sine, 1.0f, 0.15f);
adsr(p.env[3], 80.0f, 500.0f, 0.0f, 150.0f);
route(p, envSrc(4), oscScan(1), 0.6f);
amp(p, 0.0f, 1200.0f, 0.0f, 250.0f);
p.vca.velEnv = 0.6f;
room(p, ReverbType::Room, 1.2f, 0.18f);
macro(p, 1, 0.0f, { to(depthOf(0), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Crystal Pluck", "Pluck", "Glass algorithm at high ratios: a tiny, sparkling pluck with octave echoes.", 72, False,
           "fm, crystal, pluck, bright", r'''
Patch p = blank("Glass");
osc(p, 1, Sine, 2.0f, 0.4f);
strike(p, 2, Sine, 5.0f, 0.15f, 300.0f, 0.7f);
strike(p, 3, Sine, 9.0f, 0.08f, 150.0f, 0.8f);
strike(p, 4, Sine, 1.0f, 0.12f, 500.0f, 0.5f);
osc(p, 5, Sine, 4.0f, 0.15f);
strike(p, 6, Sine, 11.0f, 0.06f, 120.0f, 0.8f);
amp(p, 0.0f, 900.0f, 0.0f, 500.0f);
p.vca.velEnv = 0.6f;
echo(p, DelayType::PingPong, "1/16", 0.4f, 0.2f);
room(p, ReverbType::Hall, 2.5f, 0.25f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Cascade Pluck", "Pluck", "Cascade Into Pairs: one modulator strikes three pairs at once for a layered pluck.", 60, False,
           "fm, pluck, layered, bright", r'''
Patch p = blank("Cascade Into Pairs");
osc(p, 1, Sine, 1.0f, 0.35f);
strike(p, 2, Sine, 1.0f, 0.2f, 400.0f, 0.6f);
osc(p, 3, Sine, 2.0f, 0.2f);
strike(p, 4, Sine, 3.0f, 0.15f, 250.0f, 0.6f);
osc(p, 5, Sine, 3.0f, 0.12f).ratioFine = 0.2f;
strike(p, 6, Sine, 5.0f, 0.1f, 200.0f, 0.6f);
strike(p, 7, Sine, 1.0f, 0.1f, 150.0f, 0.7f);
strike(p, 8, Sine, 7.0f, 0.06f, 80.0f, 0.8f);
amp(p, 0.0f, 1200.0f, 0.0f, 350.0f);
p.vca.velEnv = 0.6f;
room(p, ReverbType::Room, 1.3f, 0.2f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });
macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.4f) });
'''),
    preset("Upward Pluck", "Pluck", "Upward Pairs: the carriers modulate the operators above them, which are heard too.", 60, False,
           "fm, pluck, unusual, bright", r'''
Patch p = blank("Upward Pairs");
osc(p, 1, Sine, 1.0f, 0.35f);
OscParams& b = osc(p, 2, Sine, 2.0f, 0.0f);
envLevel(b, 0.25f, 0.0f, 600.0f, 0.0f, 300.0f, 0.5f);
modOsc(p, 3, Sine, 1.0f, 0.15f);
OscParams& d = osc(p, 4, Sine, 3.0f, 0.0f);
envLevel(d, 0.15f, 0.0f, 400.0f, 0.0f, 200.0f, 0.5f);
amp(p, 0.0f, 1100.0f, 0.0f, 300.0f);
p.vca.velEnv = 0.6f;
echo(p, DelayType::Stereo, "1/8D", 0.3f, 0.18f);
room(p, ReverbType::Room, 1.2f, 0.18f);
macro(p, 1, 0.0f, { to(oscLevel(3), 0.2f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
]
