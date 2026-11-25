from common import preset

PRESETS = [
    # ---------------------------------------------------------------------------------------------- Keys (13)
    preset("Tine Ladder", "Keys", "Electric Piano algorithm: a bright tine pair over a three-operator body. Macro 1: tine.", 60, False,
           "fm, electric piano, tremolo, bright", r'''
Patch p = blank("Electric Piano");
osc(p, 1, Sine, 1.0f, 0.45f);
strike(p, 2, Sine, 14.0f, 0.12f, 180.0f, 0.9f);
osc(p, 3, Sine, 1.0f, 0.4f).ratioFine = 0.15f;
strike(p, 4, Sine, 1.0f, 0.3f, 1400.0f, 0.6f);
modOsc(p, 5, Sine, 3.0f, 0.06f);
strike(p, 6, Sine, 7.0f, 0.1f, 300.0f, 0.7f);
amp(p, 1.0f, 4500.0f, 0.0f, 400.0f);
p.env[2].decayCurve = 0.65f;
p.vca.velEnv = 0.65f;
tremolo(p, 4.5f, 0.25f, false, 120.0f);
room(p, ReverbType::Room, 1.3f, 0.18f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::PostFxParam2), 0.5f) });
'''),
    preset("Funk Clav", "Keys", "Clav algorithm: two modulators bite a pulse; a second pair adds the pluck. Wheel: wah.", 60, False,
           "clav, funky, pluck, wah", r'''
Patch p = blank("Clav");
osc(p, 1, Pulse25, 1.0f, 0.45f);
strike(p, 2, Sine, 1.0f, 0.3f, 240.0f, 0.8f);
strike(p, 3, Sine, 3.0f, 0.2f, 120.0f, 0.8f);
strike(p, 4, Sine, 5.0f, 0.1f, 80.0f, 0.9f);
osc(p, 5, Pulse12, 2.0f, 0.15f);
strike(p, 6, Sine, 1.0f, 0.2f, 150.0f, 0.7f);
amp(p, 0.0f, 700.0f, 0.2f, 80.0f);
analogLp(p, 1400.0f, 0.4f, 0.4f, 0.0f, 200.0f, 0.1f, 100.0f);
p.afilt.velEnv = 0.5f;
p.vca.velEnv = 0.6f;
wheel(p, afCut, 0.45f);
phaser(p, 0.6f, 0.3f, 0.25f);
room(p, ReverbType::Room, 0.7f, 0.1f);
macro(p, 1, 0.0f, { to(afRes, 0.4f) });
macro(p, 2, 0.0f, { to(fx(dst::PostFxMix), 0.5f) });
'''),
    preset("Bark Reed", "Keys", "Square to Saw table reed piano: velocity scans toward the saw for a barking attack.", 60, False,
           "wavetable, scan, electric piano, velocity", r'''
Patch p = blank("Two Pairs, Four Free");
famOsc(p, 1, SquareToSawF, 1.0f, 0.45f, 0.1f);
strike(p, 2, Sine, 1.0f, 0.25f, 900.0f, 0.7f);
osc(p, 3, Sine, 1.0f, 0.25f).ratioFine = -0.12f;
strike(p, 4, Sine, 4.0f, 0.12f, 200.0f, 0.8f);
route(p, src::Velocity, oscScan(1), 0.6f);
amp(p, 1.0f, 3800.0f, 0.1f, 300.0f);
p.env[2].decayCurve = 0.6f;
p.vca.velEnv = 0.6f;
analogLp(p, 5000.0f, 0.1f);
chorus(p, 0.3f);
room(p, ReverbType::Plate, 1.2f, 0.15f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.6f) });
macro(p, 2, 0.0f, { to(fx(dst::PostFxMix), 0.4f) });
'''),
    preset("Vowel Keys", "Keys", "Vowel Morph table piano: each note says a different vowel as it decays.", 60, False,
           "wavetable, vowel, scan, voice", r'''
Patch p = blank("One Pair");
famOsc(p, 1, VowelMorphF, 1.0f, 0.55f);
strike(p, 2, Sine, 2.0f, 0.15f, 400.0f, 0.6f);
osc(p, 3, Sine, 1.0f, 0.2f);
osc(p, 4, Sine, 2.0f, 0.08f);
adsr(p.env[3], 0.0f, 2500.0f, 0.0f, 400.0f);
route(p, envSrc(4), oscScan(1), 1.0f);
amp(p, 2.0f, 3500.0f, 0.15f, 400.0f);
p.vca.velEnv = 0.5f;
chorus(p, 0.35f);
room(p, ReverbType::Room, 1.5f, 0.2f);
macro(p, 1, 0.0f, { to(dst::env(3, dst::EnvDecay), 0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Glass Piano", "Keys", "Glass algorithm: three modulators shade one carrier; a second pair rings an octave up.", 60, False,
           "fm, piano, glassy, bright", r'''
Patch p = blank("Glass");
osc(p, 1, Sine, 1.0f, 0.45f);
strike(p, 2, Sine, 1.0f, 0.25f, 2000.0f, 0.6f);
strike(p, 3, Sine, 4.0f, 0.12f, 500.0f, 0.7f);
strike(p, 4, Sine, 11.0f, 0.06f, 120.0f, 0.9f);
osc(p, 5, Sine, 2.0f, 0.2f);
strike(p, 6, Sine, 5.0f, 0.1f, 350.0f, 0.7f);
for (int i : { 0, 4 }) { p.osc[static_cast<size_t>(i)].keyscaleBreak = 72; p.osc[static_cast<size_t>(i)].keyscaleHiDepth = 0.3f; }
amp(p, 0.0f, 5500.0f, 0.0f, 500.0f);
p.env[2].decayCurve = 0.75f;
p.vca.velEnv = 0.7f;
room(p, ReverbType::Hall, 2.0f, 0.2f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Harmonic Upright", "Keys", "Harmonic Build table: each note starts with many harmonics and sheds them as it decays.", 60, False,
           "wavetable, scan, piano, additive", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, HarmonicBuildF, 1.0f, 0.5f, 0.1f);
osc(p, 2, fam(HarmonicBuildF, 3), 2.0f, 0.12f).ratioFine = 0.08f;
adsr(p.env[3], 0.0f, 1800.0f, 0.0f, 300.0f);
route(p, envSrc(4), oscScan(1), 0.9f);
route(p, src::Velocity, oscScan(1), 0.3f);
amp(p, 0.0f, 5000.0f, 0.0f, 450.0f);
p.env[2].decayCurve = 0.7f;
p.vca.velEnv = 0.65f;
room(p, ReverbType::Room, 1.4f, 0.2f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.4f) });
macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.4f) });
'''),
    preset("Toy Upright", "Keys", "Pentagon algorithm: a small detuned toy piano with a woody knock.", 72, False,
           "fm, toy, piano, detuned", r'''
Patch p = blank("Pentagon");
osc(p, 1, Sine, 1.0f, 0.45f);
strike(p, 2, Sine, 3.0f, 0.18f, 300.0f, 0.7f);
strike(p, 3, Sine, 1.0f, 0.2f, 900.0f, 0.6f);
strike(p, 4, Sine, 7.1f, 0.1f, 120.0f, 0.8f);
strike(p, 5, Sine, 1.5f, 0.1f, 200.0f, 0.5f);
osc(p, 6, Triangle, 2.0f, 0.15f).ratioFine = 0.6f;
amp(p, 0.0f, 1600.0f, 0.0f, 250.0f);
p.vca.velEnv = 0.6f;
room(p, ReverbType::Room, 0.9f, 0.15f);
macro(p, 1, 0.0f, { to(oscFine(6), 0.4f) });
macro(p, 2, 0.0f, { to(oscEnvLevel(4), 0.3f) });
'''),
    preset("Ladder Harpsichord", "Keys", "Ladder algorithm: four plucked pairs, each feeding the next, for a bright jangle.", 60, False,
           "fm, harpsichord, pluck, baroque", r'''
Patch p = blank("Ladder");
osc(p, 1, Saw, 1.0f, 0.3f);
strike(p, 2, Sine, 2.0f, 0.15f, 300.0f, 0.6f);
osc(p, 3, Pulse25, 2.0f, 0.18f);
strike(p, 4, Sine, 3.0f, 0.12f, 250.0f, 0.6f);
osc(p, 5, Saw, 1.0f, 0.15f).ratioFine = 0.2f;
strike(p, 6, Sine, 5.0f, 0.1f, 200.0f, 0.6f);
osc(p, 7, Pulse12, 4.0f, 0.06f);
strike(p, 8, Sine, 7.0f, 0.08f, 150.0f, 0.6f);
amp(p, 0.0f, 2200.0f, 0.0f, 180.0f);
p.env[2].decayCurve = 0.7f;
p.vca.velEnv = 0.2f;
analogLp(p, 6000.0f, 0.1f);
room(p, ReverbType::Hall, 1.6f, 0.2f);
macro(p, 1, 0.0f, { to(afCut, -0.3f) });
macro(p, 2, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });
'''),
    preset("Crushed Keys", "Keys", "Crusher table and a LoFi insert: a tired sampler piano. Macro 1: more crush.", 60, False,
           "wavetable, lo-fi, bitcrush, piano", r'''
Patch p = blank("One Pair");
famOsc(p, 1, CrusherF, 1.0f, 0.5f, 0.35f);
strike(p, 2, Sine, 1.0f, 0.2f, 900.0f, 0.6f);
osc(p, 3, Sine, 2.0f, 0.12f);
amp(p, 0.0f, 3000.0f, 0.0f, 300.0f);
p.env[2].decayCurve = 0.6f;
p.vca.velEnv = 0.5f;
lofi(p, 11025.0f, 5000.0f);
chorus(p, 0.3f, true);
room(p, ReverbType::Room, 1.0f, 0.15f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.6f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Celesta Dust", "Keys", "Bell Ladder algorithm: a soft celesta with sparkling upper pairs and echoes.", 72, False,
           "fm, celesta, bell, echo", r'''
Patch p = blank("Bell Ladder");
osc(p, 1, Sine, 1.0f, 0.4f);
strike(p, 2, Sine, 4.0f, 0.12f, 400.0f, 0.7f);
osc(p, 3, Sine, 2.0f, 0.25f);
strike(p, 4, Sine, 1.0f, 0.15f, 1200.0f, 0.6f);
osc(p, 5, Sine, 4.0f, 0.12f).ratioFine = 0.1f;
strike(p, 6, Sine, 3.5f, 0.1f, 300.0f, 0.7f);
strike(p, 7, Sine, 9.0f, 0.06f, 150.0f, 0.8f);
strike(p, 8, Sine, 1.0f, 0.1f, 600.0f, 0.5f);
amp(p, 0.0f, 2600.0f, 0.0f, 900.0f);
p.env[2].decayCurve = 0.7f;
p.vca.velEnv = 0.6f;
echo(p, DelayType::PingPong, "1/8", 0.3f, 0.18f);
room(p, ReverbType::Hall, 2.4f, 0.25f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Phased Tines", "Keys", "A classic tine piano through a slow phaser with gentle drive.", 60, False,
           "fm, electric piano, phaser, warm", r'''
Patch p = blank("Two Pairs, Four Free");
osc(p, 1, Sine, 1.0f, 0.5f);
strike(p, 2, Sine, 1.0f, 0.3f, 1500.0f, 0.65f);
osc(p, 3, Sine, 1.0f, 0.2f).ratioFine = 0.18f;
strike(p, 4, Sine, 13.0f, 0.1f, 150.0f, 0.9f);
amp(p, 1.0f, 4200.0f, 0.05f, 380.0f);
p.env[2].decayCurve = 0.6f;
p.vca.velEnv = 0.65f;
distort(p, 0.15f, 0.2f, 0.5f);
phaser(p, 0.25f, 0.45f, 0.45f);
room(p, ReverbType::Room, 1.2f, 0.15f);
macro(p, 1, 0.0f, { to(fx(dst::PreFxParam1), 0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::PostFxMix), 0.4f) });
'''),
    preset("Comb Piano", "Keys", "Comb Sweep table: notches drift through the tone as each note fades.", 60, False,
           "wavetable, scan, piano, comb", r'''
Patch p = blank("One Pair");
famOsc(p, 1, CombSweepF, 1.0f, 0.5f, 0.0f);
strike(p, 2, Sine, 1.0f, 0.15f, 1000.0f, 0.6f);
osc(p, 3, Sine, 1.0f, 0.15f);
adsr(p.env[3], 0.0f, 4000.0f, 0.0f, 400.0f);
p.env[3].attackMs = 3000.0f;
route(p, envSrc(4), oscScan(1), 0.8f);
amp(p, 0.0f, 4500.0f, 0.0f, 450.0f);
p.env[2].decayCurve = 0.7f;
p.vca.velEnv = 0.6f;
chorus(p, 0.25f);
room(p, ReverbType::Plate, 1.6f, 0.2f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Night Keys", "Keys", "Dark Saw Bright table through a soft low-pass, wide chorus and a long plate.", 57, False,
           "wavetable, dark, soft, chorus", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, fam(SawBrightF, 3), 1.0f, 0.45f);
osc(p, 2, fam(SawBrightF, 2), 1.0f, 0.3f).ratioFine = 0.25f;
osc(p, 3, Sine, 0.5f, 0.15f);
analogLp(p, 900.0f, 0.15f, 0.35f, 0.0f, 900.0f, 0.0f, 400.0f);
p.afilt.velEnv = 0.5f;
amp(p, 2.0f, 3500.0f, 0.1f, 600.0f);
p.vca.velEnv = 0.5f;
chorus(p, 0.5f);
room(p, ReverbType::Plate, 2.8f, 0.3f);
macro(p, 1, 0.0f, { to(afCut, 0.4f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),

    # ---------------------------------------------------------------------------------------------- Bells (10)
    preset("Gong Bloom", "Bells", "Gong algorithm: a deep crossed stack and a second struck group swell after the hit.", 45, False,
           "fm, gong, inharmonic, long", r'''
Patch p = blank("Gong");
osc(p, 1, Sine, 1.0f, 0.45f);
strike(p, 2, Sine, 1.41f, 0.2f, 4000.0f, 0.5f);
strike(p, 3, Sine, 2.76f, 0.15f, 3000.0f, 0.5f);
modOsc(p, 4, Sine, 0.53f, 0.12f);
osc(p, 5, Sine, 2.0f, 0.2f).ratioFine = 0.4f;
strike(p, 6, Sine, 3.17f, 0.12f, 2500.0f, 0.5f);
strike(p, 7, Sine, 5.43f, 0.1f, 1500.0f, 0.5f);
modOsc(p, 8, Sine, 1.07f, 0.05f);
envLevel(p.osc[4], 0.15f, 600.0f, 4000.0f, 0.0f, 3000.0f);
amp(p, 2.0f, 9000.0f, 0.0f, 5000.0f);
p.env[2].decayCurve = 0.6f;
room(p, ReverbType::Hall, 5.0f, 0.3f, 40.0f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.25f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Chime Tree", "Bells", "Chimes algorithm: four bell voices at stretched ratios, scattered with ping-pong echoes.", 79, False,
           "fm, chimes, bright, echo", r'''
Patch p = blank("Chimes");
osc(p, 1, Sine, 1.0f, 0.3f);
osc(p, 2, Sine, 2.73f, 0.2f);
strike(p, 3, Sine, 3.5f, 0.2f, 1200.0f, 0.6f);
osc(p, 4, Sine, 5.19f, 0.15f);
strike(p, 5, Sine, 1.4f, 0.12f, 800.0f, 0.6f);
strike(p, 6, Sine, 7.1f, 0.08f, 400.0f, 0.7f);
osc(p, 7, Sine, 8.3f, 0.08f);
strike(p, 8, Sine, 2.1f, 0.1f, 600.0f, 0.6f);
amp(p, 0.0f, 3500.0f, 0.0f, 2500.0f);
p.env[2].decayCurve = 0.65f;
p.vca.velEnv = 0.6f;
echo(p, DelayType::PingPong, "1/8D", 0.4f, 0.22f);
room(p, ReverbType::Hall, 3.5f, 0.3f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.25f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayFeedback), 0.3f) });
'''),
    preset("Gamelan Keys", "Bells", "Gamelan algorithm: two shared modulators ring two carriers a quarter-tone apart.", 64, False,
           "fm, gamelan, metallic, beating", r'''
Patch p = blank("Gamelan");
osc(p, 1, Sine, 1.0f, 0.4f);
osc(p, 2, Sine, 1.0f, 0.35f).ratioFine = 1.5f;
strike(p, 3, Sine, 2.4f, 0.2f, 1500.0f, 0.6f);
strike(p, 4, Sine, 3.9f, 0.15f, 900.0f, 0.6f);
strike(p, 5, Sine, 6.3f, 0.1f, 300.0f, 0.8f);
osc(p, 6, Sine, 2.0f, 0.1f);
amp(p, 0.0f, 3000.0f, 0.0f, 1800.0f);
p.env[2].decayCurve = 0.6f;
p.vca.velEnv = 0.6f;
room(p, ReverbType::Hall, 2.5f, 0.25f);
macro(p, 1, 0.0f, { to(oscFine(2), 0.4f) });
macro(p, 2, 0.0f, { to(oscEnvLevel(5), 0.3f) });
'''),
    preset("Ladder Bells", "Bells", "Bell Ladder algorithm: three tuned bells, the upper ones feeding the lower.", 72, False,
           "fm, bell, tubular, long", r'''
Patch p = blank("Bell Ladder");
osc(p, 1, Sine, 1.0f, 0.4f);
strike(p, 2, Sine, 3.5f, 0.2f, 2500.0f, 0.5f);
osc(p, 3, Sine, 2.0f, 0.25f);
strike(p, 4, Sine, 2.76f, 0.15f, 2000.0f, 0.5f);
osc(p, 5, Sine, 3.0f, 0.15f).ratioFine = 0.3f;
strike(p, 6, Sine, 5.4f, 0.1f, 1500.0f, 0.6f);
strike(p, 7, Sine, 7.0f, 0.08f, 800.0f, 0.6f);
strike(p, 8, Sine, 1.5f, 0.1f, 1200.0f, 0.5f);
amp(p, 0.0f, 7000.0f, 0.0f, 4000.0f);
p.env[2].decayCurve = 0.6f;
p.vca.velEnv = 0.5f;
room(p, ReverbType::Hall, 4.0f, 0.3f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.25f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.25f) });
'''),
    preset("Ring Bell", "Bells", "Ring of Three: three operators modulate each other in a loop for a wild, clanging bell.", 67, False,
           "fm, feedback, bell, chaotic", r'''
Patch p = blank("Ring of Three");
osc(p, 1, Sine, 1.0f, 0.4f);
strike(p, 2, Sine, 1.41f, 0.15f, 1800.0f, 0.6f);
strike(p, 3, Sine, 2.23f, 0.12f, 1200.0f, 0.6f);
osc(p, 4, Sine, 2.0f, 0.15f);
amp(p, 0.0f, 5000.0f, 0.0f, 2500.0f);
p.env[2].decayCurve = 0.6f;
p.vca.velEnv = 0.6f;
room(p, ReverbType::Hall, 3.0f, 0.28f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.2f), to(oscEnvLevel(3), 0.2f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Spectral Bell", "Bells", "Spectra table bell: the strike scans through random partial sets as it rings.", 72, False,
           "wavetable, scan, bell, spectral", r'''
Patch p = blank("One Pair");
famOsc(p, 1, SpectraF, 1.0f, 0.45f, 0.0f);
strike(p, 2, Sine, 3.5f, 0.15f, 800.0f, 0.6f);
osc(p, 3, Sine, 1.0f, 0.2f);
osc(p, 4, Sine, 2.76f, 0.08f);
adsr(p.env[3], 0.0f, 0.0f, 1.0f, 300.0f);
p.env[3].attackMs = 4000.0f;
route(p, envSrc(4), oscScan(1), 1.0f);
amp(p, 0.0f, 6000.0f, 0.0f, 3000.0f);
p.env[2].decayCurve = 0.6f;
p.vca.velEnv = 0.5f;
room(p, ReverbType::Hall, 3.5f, 0.3f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.25f) });
'''),
    preset("Glass Harmonica", "Bells", "Bowed glass: slow attack, soft upper partials and a breathing tremolo.", 72, True,
           "fm, glass, bowed, soft", r'''
Patch p = blank("Glass");
osc(p, 1, Sine, 1.0f, 0.5f);
modOsc(p, 2, Sine, 2.0f, 0.06f);
modOsc(p, 3, Sine, 3.0f, 0.03f);
modOsc(p, 4, Sine, 5.0f, 0.02f);
osc(p, 5, Sine, 2.0f, 0.15f).ratioFine = 0.2f;
modOsc(p, 6, Sine, 1.0f, 0.04f);
amp(p, 300.0f, 2000.0f, 0.8f, 1500.0f);
lfo(p, 3, LfoWave::Sine, 4.5f);
p.vca.lfoAmt = 0.12f;
room(p, ReverbType::Hall, 3.0f, 0.3f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.15f) });
macro(p, 2, 0.0f, { to(dst::VcaBase + dst::VcaLfoAmt, 0.3f) });
'''),
    preset("Steel Drum", "Bells", "A bright pan drum: short 1:2 FM with a pitch blip on the strike.", 64, False,
           "fm, steel drum, tropical, short", r'''
Patch p = blank("Two Pairs, Four Free");
osc(p, 1, Sine, 1.0f, 0.45f);
strike(p, 2, Sine, 2.0f, 0.25f, 260.0f, 0.7f);
osc(p, 3, Sine, 1.0f, 0.2f).ratioFine = 0.5f;
strike(p, 4, Sine, 3.0f, 0.15f, 120.0f, 0.8f);
adsr(p.env[3], 0.0f, 40.0f, 0.0f, 40.0f);
route(p, envSrc(4), allPitch, 0.04f);
amp(p, 0.0f, 900.0f, 0.0f, 300.0f);
p.vca.velEnv = 0.6f;
room(p, ReverbType::Room, 1.2f, 0.18f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });
'''),
    preset("Shimmer Bells", "Bells", "Soft bells into a frozen-sounding cloud reverb with octave echoes.", 72, False,
           "fm, bell, ambient, cloud", r'''
Patch p = blank("Four Pairs");
osc(p, 1, Sine, 1.0f, 0.35f);
strike(p, 2, Sine, 3.5f, 0.15f, 1500.0f, 0.6f);
osc(p, 3, Sine, 2.0f, 0.25f).ratioFine = 0.15f;
strike(p, 4, Sine, 5.4f, 0.1f, 1000.0f, 0.6f);
osc(p, 5, Sine, 4.0f, 0.12f);
strike(p, 6, Sine, 1.0f, 0.1f, 2000.0f, 0.5f);
amp(p, 2.0f, 4000.0f, 0.0f, 3000.0f);
p.vca.velEnv = 0.5f;
echo(p, DelayType::PingPong, "1/4", 0.45f, 0.25f);
room(p, ReverbType::Cloud, 8.0f, 0.4f, 60.0f);
macro(p, 1, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayFeedback), 0.3f) });
'''),
    preset("Clock Tower", "Bells", "Tower, Every Other Heard: a deep tower bell whose middle partials sound too.", 52, False,
           "fm, bell, tower, deep", r'''
Patch p = blank("Tower, Every Other Heard");
osc(p, 1, Sine, 1.0f, 0.4f);
strike(p, 2, Sine, 2.0f, 0.15f, 3000.0f, 0.5f);
osc(p, 3, Sine, 2.4f, 0.15f);
strike(p, 4, Sine, 3.0f, 0.1f, 2500.0f, 0.5f);
osc(p, 5, Sine, 3.0f, 0.1f).ratioFine = 0.4f;
strike(p, 6, Sine, 4.2f, 0.08f, 2000.0f, 0.5f);
osc(p, 7, Sine, 4.5f, 0.06f);
strike(p, 8, Sine, 5.4f, 0.06f, 1500.0f, 0.5f);
amp(p, 0.0f, 9000.0f, 0.0f, 5000.0f);
p.env[2].decayCurve = 0.6f;
room(p, ReverbType::Hall, 4.5f, 0.3f, 50.0f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),

    # ---------------------------------------------------------------------------------------------- Organ (10)
    preset("Stop Changer", "Organ", "Organ Stops table: the wheel pulls out drawbar registrations one after another; rotary.", 60, True,
           "wavetable, organ, drawbars, rotary", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, OrganStopsF, 1.0f, 0.55f, 0.2f);
osc(p, 2, Sine, 0.5f, 0.2f);
wheel(p, oscScan(1), 0.8f);
amp(p, 2.0f, 100.0f, 1.0f, 60.0f);
rotary(p);
room(p, ReverbType::Room, 1.2f, 0.15f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.8f) });
macro(p, 2, 0.0f, { to(oscLevel(2), 0.3f) });
'''),
    preset("Grit Organ", "Organ", "Organ With Two Mods: drawbar sines roughened by two FM operators and drive.", 60, True,
           "organ, fm, drive, rock", r'''
Patch p = blank("Organ With Two Mods");
osc(p, 1, Sine, 1.0f, 0.35f);
osc(p, 2, Sine, 2.0f, 0.3f);
osc(p, 3, Sine, 3.0f, 0.2f);
osc(p, 4, Sine, 4.0f, 0.2f);
osc(p, 5, Sine, 0.5f, 0.25f);
osc(p, 6, Sine, 8.0f, 0.08f);
modOsc(p, 7, Sine, 1.0f, 0.08f);
modOsc(p, 8, Sine, 2.0f, 0.06f);
amp(p, 2.0f, 100.0f, 1.0f, 60.0f);
distort(p, 0.35f, 0.1f);
rotary(p);
room(p, ReverbType::Room, 1.0f, 0.12f);
wheel(p, dst::ModulatorLevel, 0.15f);
macro(p, 1, 0.0f, { to(fx(dst::PreFxParam1), 0.4f) });
macro(p, 2, 0.0f, { to(dst::ModulatorLevel, 0.15f) });
'''),
    preset("Five Rank Pipes", "Organ", "Five Carriers, Three Mods: five pipe ranks, three of them with breathy FM edge.", 60, True,
           "organ, pipe, church, hall", r'''
Patch p = blank("Five Carriers, Three Mods");
osc(p, 1, Sine, 1.0f, 0.35f);
osc(p, 2, Triangle, 2.0f, 0.25f);
osc(p, 3, Sine, 4.0f, 0.15f);
osc(p, 4, fam(OrganStopsF, 15), 0.5f, 0.25f);
osc(p, 5, Sine, 3.0f, 0.12f);
modOsc(p, 6, Sine, 1.0f, 0.04f);
modOsc(p, 7, Sine, 2.0f, 0.05f);
modOsc(p, 8, Sine, 6.0f, 0.03f);
amp(p, 40.0f, 200.0f, 1.0f, 400.0f);
room(p, ReverbType::Hall, 4.5f, 0.35f, 40.0f);
macro(p, 1, 0.0f, { to(oscLevel(4), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Buzz Organ", "Organ", "Fan, All Heard: one operator buzzes all seven pipes and sounds itself.", 60, True,
           "organ, buzzy, fm, vintage", r'''
Patch p = blank("Fan, All Heard");
osc(p, 1, Sine, 1.0f, 0.3f);
osc(p, 2, Sine, 2.0f, 0.2f);
osc(p, 3, Sine, 3.0f, 0.15f);
osc(p, 4, Sine, 4.0f, 0.12f);
osc(p, 5, Sine, 6.0f, 0.08f);
osc(p, 6, Sine, 8.0f, 0.06f);
osc(p, 7, Sine, 0.5f, 0.2f);
modOsc(p, 8, Square, 1.0f, 0.06f);
amp(p, 2.0f, 100.0f, 1.0f, 60.0f);
vibrato(p, 6.0f, 0.04f, 0.0f);
tremolo(p, 6.0f, 0.15f);
room(p, ReverbType::Room, 1.0f, 0.15f);
macro(p, 1, 0.0f, { to(oscLevel(8), 0.15f) });
macro(p, 2, 0.0f, { to(fx(dst::PostFxParam2), 0.4f) });
'''),
    preset("Pipe Flute", "Organ", "A soft flute stop: low harmonics, a little breath on the attack.", 72, True,
           "organ, flute, pipe, soft", r'''
Patch p = blank("One Pair");
osc(p, 1, fam(HarmonicBuildF, 2), 1.0f, 0.5f);
strike(p, 2, Sine, 3.0f, 0.1f, 120.0f, 0.3f);
osc(p, 3, Sine, 2.0f, 0.12f);
amp(p, 60.0f, 200.0f, 0.95f, 300.0f);
vibrato(p, 5.0f, 0.03f, 500.0f);
room(p, ReverbType::Hall, 3.0f, 0.3f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.2f) });
macro(p, 2, 0.0f, { to(oscLevel(3), 0.2f) });
'''),
    preset("Theatre Tremulant", "Organ", "Rounded tibia-style pipes with a deep tremulant and wide vibrato.", 60, True,
           "organ, theatre, tremolo, vibrato", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, Sine, 1.0f, 0.4f);
osc(p, 2, Triangle, 2.0f, 0.2f);
osc(p, 3, Sine, 0.5f, 0.25f);
osc(p, 4, Sine, 4.0f, 0.06f);
amp(p, 20.0f, 200.0f, 1.0f, 350.0f);
vibrato(p, 6.5f, 0.1f, 0.0f);
tremolo(p, 6.5f, 0.35f, false, 40.0f);
room(p, ReverbType::Hall, 2.5f, 0.3f);
macro(p, 1, 0.0f, { to(fx(dst::PostFxParam2), 0.4f) });
macro(p, 2, 0.0f, { to(oscLevel(4), 0.2f) });
'''),
    preset("Chapel Reed", "Organ", "Reed stop: Square to Saw table with a nasal pair and a stone-church reverb.", 60, True,
           "organ, reed, church, nasal", r'''
Patch p = blank("Two Pairs, Four Free");
osc(p, 1, fam(SquareToSawF, 6), 1.0f, 0.4f);
modOsc(p, 2, Sine, 2.0f, 0.08f);
osc(p, 3, Reed, 2.0f, 0.2f);
modOsc(p, 4, Sine, 3.0f, 0.05f);
osc(p, 5, Sine, 0.5f, 0.2f);
analogLp(p, 3500.0f, 0.1f);
amp(p, 30.0f, 200.0f, 1.0f, 450.0f);
room(p, ReverbType::Hall, 5.0f, 0.35f, 50.0f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(dst::ModulatorLevel, 0.1f) });
'''),
    preset("Vowel Organ", "Organ", "Vowel Morph table pipes: a slow LFO makes the chord sing ah-eh-ee-oh-oo.", 60, True,
           "wavetable, organ, vowel, evolving", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, VowelMorphF, 1.0f, 0.45f);
famOsc(p, 2, VowelMorphF, 2.0f, 0.15f);
osc(p, 3, Sine, 0.5f, 0.2f);
lfo(p, 4, LfoWave::Triangle, 0.12f, LfoTrig::Free);
route(p, lfoUni(4), oscScan(1), 1.0f);
route(p, lfoUni(4), oscScan(2), 0.7f);
amp(p, 20.0f, 200.0f, 1.0f, 400.0f);
chorus(p, 0.3f);
room(p, ReverbType::Hall, 3.0f, 0.3f);
macro(p, 1, 0.0f, { to(dst::LfoBase + 3 * dst::kLfoParams + dst::LfoRate, 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Rock Organ Drive", "Organ", "Full drawbars into a hot drive and a fast rotor. Wheel: slow/fast.", 60, True,
           "organ, rock, drive, rotary", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, Sine, 1.0f, 0.3f);
osc(p, 2, Sine, 2.0f, 0.3f);
osc(p, 3, Sine, 3.0f, 0.25f);
osc(p, 4, Sine, 0.5f, 0.3f);
osc(p, 5, Sine, 4.0f, 0.2f);
osc(p, 6, Sine, 6.0f, 0.12f);
osc(p, 7, Sine, 8.0f, 0.1f);
strike(p, 8, Sine, 10.0f, 0.08f, 30.0f, 0.0f);
amp(p, 2.0f, 100.0f, 1.0f, 60.0f);
distort(p, 0.55f, 0.15f);
rotary(p);
p.fx.post.rotary.hornHz = 6.5f;
route(p, src::ModWheel, fx(dst::PostFxParam1), 0.6f);
room(p, ReverbType::Room, 1.0f, 0.12f);
macro(p, 1, 0.0f, { to(fx(dst::PreFxParam1), 0.3f) });
macro(p, 2, 0.0f, { to(oscEnvLevel(8), 0.2f) });
'''),
    preset("Calliope", "Organ", "A breathy steam-whistle organ: flutey sines with noise-like flutter and wide vibrato.", 72, True,
           "organ, calliope, whistle, vintage", r'''
Patch p = blank("Two Pairs, Four Free");
osc(p, 1, Sine, 1.0f, 0.45f);
modOsc(p, 2, Sine, 7.13f, 0.03f);
osc(p, 3, Sine, 2.0f, 0.15f);
modOsc(p, 4, Sine, 11.3f, 0.02f);
amp(p, 25.0f, 100.0f, 1.0f, 150.0f);
vibrato(p, 7.0f, 0.12f, 0.0f);
lfo(p, 5, LfoWave::Noise, 20.0f);
route(p, lfoSrc(5), oscLevel(2), 0.03f);
room(p, ReverbType::Room, 1.5f, 0.2f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.06f) });
macro(p, 2, 0.0f, { to(dst::VoiceBase + dst::VoiceVibratoAmt, 0.3f) });
'''),
]
