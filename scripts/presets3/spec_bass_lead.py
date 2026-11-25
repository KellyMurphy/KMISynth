from common import preset

PRESETS = [
    # ---------------------------------------------------------------------------------------------- Bass (12)
    preset("Fork Bass", "Bass", "Bass Fork algorithm: a forked stack and a feedback pair punch a sine; mono glide.", 36, True,
           "bass, fm, mono, punchy", r'''
Patch p = blank("Bass Fork");
osc(p, 1, Sine, 1.0f, 0.6f);
strike(p, 2, Sine, 1.0f, 0.3f, 300.0f, 0.7f);
strike(p, 3, Sine, 2.0f, 0.15f, 150.0f, 0.7f);
modOsc(p, 4, Sine, 0.5f, 0.05f);
strike(p, 5, Sine, 1.0f, 0.2f, 500.0f, 0.5f).feedback = 0.3f;
modOsc(p, 6, Sine, 3.0f, 0.04f);
osc(p, 7, Sine, 0.5f, 0.3f);
amp(p, 0.0f, 600.0f, 0.7f, 120.0f);
mono(p, 0.12f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(oscLevel(7), 0.3f) });
'''),
    preset("Stacked Punch", "Bass", "Bass Pair Stack: a four-high stack for the attack and a pair for the body.", 36, True,
           "bass, fm, punchy, digital", r'''
Patch p = blank("Bass Pair Stack");
osc(p, 1, Sine, 1.0f, 0.5f);
strike(p, 2, Sine, 1.0f, 0.3f, 200.0f, 0.7f);
strike(p, 3, Sine, 2.0f, 0.2f, 120.0f, 0.7f);
strike(p, 4, Sine, 5.0f, 0.15f, 60.0f, 0.8f);
osc(p, 5, Triangle, 0.5f, 0.35f);
modOsc(p, 6, Sine, 1.0f, 0.08f);
amp(p, 0.0f, 500.0f, 0.75f, 100.0f);
analogLp(p, 3000.0f, 0.1f);
mono(p, 0.08f);
compress(p, 4.0f, -18.0f, 3.0f);
macro(p, 1, 0.0f, { to(oscEnvLevel(3), 0.3f) });
macro(p, 2, 0.0f, { to(afCut, -0.3f) });
'''),
    preset("Width Bass", "Bass", "Pulse Width table bass: an LFO sweeps the pulse width through a warm ladder.", 36, True,
           "wavetable, pwm, bass, analog", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, PulseWidthF, 1.0f, 0.5f, 0.2f);
osc(p, 2, Sine, 0.5f, 0.3f);
lfo(p, 4, LfoWave::Triangle, 0.6f);
route(p, lfoUni(4), oscScan(1), 0.6f);
analogLp(p, 500.0f, 0.3f, 0.5f, 0.0f, 300.0f, 0.2f, 120.0f);
p.afilt.velEnv = 0.4f;
amp(p, 0.0f, 400.0f, 0.8f, 100.0f);
mono(p, 0.1f);
macro(p, 1, 0.0f, { to(afCut, 0.4f) });
macro(p, 2, 0.0f, { to(dst::LfoBase + 3 * dst::kLfoParams + dst::LfoRate, 0.3f) });
'''),
    preset("Tube Bass", "Bass", "Tube Drive table: harder playing drives the wave toward a square.", 36, True,
           "wavetable, drive, bass, velocity", r'''
Patch p = blank("One Pair");
famOsc(p, 1, TubeDriveF, 1.0f, 0.55f, 0.1f);
strike(p, 2, Sine, 1.0f, 0.15f, 200.0f, 0.6f);
osc(p, 3, Sine, 0.5f, 0.3f);
route(p, src::Velocity, oscScan(1), 0.7f);
analogLp(p, 1800.0f, 0.15f, 0.3f, 0.0f, 250.0f, 0.3f, 100.0f);
amp(p, 0.0f, 600.0f, 0.75f, 100.0f);
mono(p, 0.08f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.6f) });
macro(p, 2, 0.0f, { to(afCut, 0.3f) });
'''),
    preset("Table Acid", "Bass", "Resonant Sweep table: the envelope sweeps a baked-in resonant filter; accents with velocity.", 36, True,
           "wavetable, acid, resonant, bass", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, ResonantSweepF, 1.0f, 0.55f, 0.15f);
adsr(p.env[3], 0.0f, 260.0f, 0.0f, 100.0f);
route(p, envSrc(4), oscScan(1), 0.75f);
route(p, src::Velocity, oscScan(1), 0.2f);
amp(p, 0.0f, 300.0f, 0.6f, 60.0f);
mono(p, 0.15f);
distort(p, 0.25f, 0.0f);
echo(p, DelayType::Stereo, "1/8D", 0.25f, 0.12f);
macro(p, 1, 0.0f, { to(dst::env(3, dst::EnvDecay), 0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::PreFxParam1), 0.4f) });
''', style="line"),
    preset("Octave Growl", "Bass", "FM Octave table: the envelope scans from sine to a growling 2:1 FM.", 36, True,
           "wavetable, fm, growl, bass", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, FmOctaveF, 1.0f, 0.55f, 0.1f);
osc(p, 2, Sine, 0.5f, 0.25f);
adsr(p.env[3], 0.0f, 450.0f, 0.2f, 150.0f);
route(p, envSrc(4), oscScan(1), 0.7f);
amp(p, 0.0f, 500.0f, 0.8f, 100.0f);
analogLp(p, 2500.0f, 0.1f);
mono(p, 0.1f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.5f) });
macro(p, 2, 0.0f, { to(dst::env(3, dst::EnvSustain), 0.5f) });
'''),
    preset("Fold Wobble", "Bass", "Wavefold table on a tempo LFO: the folds open and close in 8ths.", 36, True,
           "wavetable, wobble, tempo, bass", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, WavefoldF, 1.0f, 0.5f, 0.0f);
osc(p, 2, Sine, 0.5f, 0.35f);
syncLfo(p, 4, LfoWave::Sine, "1/8");
route(p, lfoUni(4), oscScan(1), 0.8f);
analogLp(p, 3000.0f, 0.2f);
amp(p, 0.0f, 300.0f, 0.9f, 80.0f);
mono(p, 0.05f);
macro(p, 1, 0.0f, { to(depthOf(0), 0.2f) });
macro(p, 2, 0.0f, { to(afCut, -0.3f) });
'''),
    preset("Sync Pluck Bass", "Bass", "Hard Sync table bass: each note bites with high sync and settles.", 36, True,
           "wavetable, hard sync, pluck, bass", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, HardSyncF, 1.0f, 0.5f, 0.05f);
osc(p, 2, Sine, 0.5f, 0.3f);
adsr(p.env[3], 0.0f, 220.0f, 0.0f, 100.0f);
route(p, envSrc(4), oscScan(1), 0.8f);
analogLp(p, 2200.0f, 0.15f, 0.3f, 0.0f, 300.0f, 0.2f, 100.0f);
amp(p, 0.0f, 500.0f, 0.7f, 90.0f);
mono(p, 0.06f);
macro(p, 1, 0.0f, { to(depthOf(0), 0.3f) });
macro(p, 2, 0.0f, { to(afCut, 0.3f) });
''', style="line"),
    preset("Sub Pressure", "Bass", "A sine sub with a dark saw table on top, held together by a compressor.", 36, True,
           "bass, sub, compressed, dark", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, Sine, 1.0f, 0.55f);
osc(p, 2, fam(SawBrightF, 6), 1.0f, 0.25f).ratioFine = 0.3f;
osc(p, 3, fam(SawBrightF, 6), 1.0f, 0.25f).ratioFine = -0.3f;
analogLp(p, 600.0f, 0.2f, 0.35f, 0.0f, 300.0f, 0.3f, 100.0f);
amp(p, 0.0f, 500.0f, 0.85f, 120.0f);
mono(p, 0.05f);
compress(p, 6.0f, -20.0f, 4.0f);
macro(p, 1, 0.0f, { to(afCut, 0.4f) });
macro(p, 2, 0.0f, { to(oscLevel(2), 0.2f), to(oscLevel(3), 0.2f) });
'''),
    preset("Wide Reese", "Bass", "Mono unison saws, beating slowly, under a tight low-pass and phaser.", 36, True,
           "bass, reese, unison, wide", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, Saw, 1.0f, 0.4f);
osc(p, 2, Saw, 1.0f, 0.4f).ratioFine = 0.6f;
osc(p, 3, Sine, 0.5f, 0.3f);
unison(p, 3, 0.3f, PolyMode::Unison);
p.voice.glideMode = GlideMode::Glide;
p.voice.glideTime = 0.1f;
analogLp(p, 700.0f, 0.2f);
amp(p, 0.0f, 400.0f, 0.9f, 120.0f);
phaser(p, 0.15f, 0.3f, 0.3f);
macro(p, 1, 0.0f, { to(afCut, 0.35f) });
macro(p, 2, 0.0f, { to(dst::VoiceBase + dst::VoiceUnisonDetune, 0.3f) });
'''),
    preset("Digital Thump", "Bass", "Long and Short algorithm: a long chain and a short stack hit one sine for a hard digital thump.", 36, True,
           "bass, fm, digital, hard", r'''
Patch p = blank("Long and Short");
osc(p, 1, Sine, 1.0f, 0.55f);
strike(p, 2, Sine, 1.0f, 0.25f, 180.0f, 0.7f);
strike(p, 3, Sine, 3.0f, 0.15f, 90.0f, 0.8f);
modOsc(p, 4, Sine, 0.5f, 0.06f);
strike(p, 5, Sine, 2.0f, 0.15f, 250.0f, 0.6f);
strike(p, 6, Sine, 1.0f, 0.1f, 200.0f, 0.6f);
strike(p, 7, Sine, 7.0f, 0.08f, 60.0f, 0.8f);
modOsc(p, 8, Sine, 1.0f, 0.03f);
amp(p, 0.0f, 500.0f, 0.7f, 100.0f);
mono(p, 0.05f);
compress(p, 4.0f, -16.0f, 2.0f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });
macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvSustain), -0.4f) });
'''),
    preset("Bent Bass", "Bass", "Phase Bend table: the envelope bends the phase for a resonant, talking bass.", 36, True,
           "wavetable, phase distortion, bass, resonant", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, PhaseBendF, 1.0f, 0.5f, 0.1f);
osc(p, 2, Sine, 0.5f, 0.3f);
adsr(p.env[3], 0.0f, 350.0f, 0.15f, 150.0f);
route(p, envSrc(4), oscScan(1), 0.8f);
amp(p, 0.0f, 500.0f, 0.8f, 100.0f);
analogLp(p, 3000.0f, 0.1f);
mono(p, 0.12f);
macro(p, 1, 0.0f, { to(depthOf(0), 0.2f) });
macro(p, 2, 0.0f, { to(oscLevel(2), 0.3f) });
''', style="line"),

    # ---------------------------------------------------------------------------------------------- Lead (12)
    preset("Twin Engine Lead", "Lead", "Sync Lead algorithm: a three-high stack and a three-into-one fan, mono with glide.", 67, True,
           "lead, fm, mono, glide", r'''
Patch p = blank("Sync Lead");
osc(p, 1, Saw, 1.0f, 0.35f);
modOsc(p, 2, Sine, 1.0f, 0.12f);
modOsc(p, 3, Sine, 2.0f, 0.06f);
strike(p, 4, Sine, 3.0f, 0.1f, 300.0f, 0.5f);
osc(p, 5, Square, 1.0f, 0.25f).ratioFine = 0.3f;
modOsc(p, 6, Sine, 1.0f, 0.08f);
modOsc(p, 7, Sine, 2.0f, 0.05f);
strike(p, 8, Sine, 5.0f, 0.08f, 200.0f, 0.6f);
amp(p, 5.0f, 300.0f, 0.85f, 200.0f);
analogLp(p, 4000.0f, 0.2f);
mono(p, 0.2f);
vibrato(p, 5.5f, 0.06f);
echo(p, DelayType::Stereo, "1/8D", 0.3f, 0.2f);
wheel(p, dst::ModulatorLevel, 0.2f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.2f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Breath Lead", "Lead", "Breathy Lead algorithm: a diamond of modulators with an airy noise layer; pressure: breath.", 72, True,
           "lead, breathy, flute, aftertouch", r'''
Patch p = blank("Breathy Lead");
osc(p, 1, Sine, 1.0f, 0.5f);
modOsc(p, 2, Sine, 1.0f, 0.08f);
modOsc(p, 3, Sine, 2.0f, 0.05f);
modOsc(p, 4, Sine, 7.31f, 0.04f);
osc(p, 5, fam(SpectraF, 9), 4.0f, 0.04f);
osc(p, 6, Sine, 2.0f, 0.1f);
amp(p, 60.0f, 300.0f, 0.9f, 250.0f);
lfo(p, 5, LfoWave::Noise, 30.0f);
route(p, lfoSrc(5), oscLevel(4), 0.05f);
route(p, src::ChannelAT, dst::ModulatorLevel, 0.2f);
mono(p, 0.15f);
vibrato(p, 5.0f, 0.05f, 600.0f);
room(p, ReverbType::Hall, 2.2f, 0.25f);
macro(p, 1, 0.0f, { to(oscLevel(5), 0.1f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Solo Stack", "Lead", "Lead Stack Plus Sub: a four-high stack sings over a sub and a pair; wheel opens it up.", 67, True,
           "lead, fm, bright, mono", r'''
Patch p = blank("Lead Stack Plus Sub");
osc(p, 1, Sine, 1.0f, 0.45f);
modOsc(p, 2, Sine, 1.0f, 0.15f);
modOsc(p, 3, Sine, 2.0f, 0.08f);
modOsc(p, 4, Sine, 3.0f, 0.05f);
strike(p, 5, Sine, 4.0f, 0.12f, 200.0f, 0.6f);
osc(p, 6, Sine, 0.5f, 0.25f);
osc(p, 7, Saw, 1.0f, 0.15f).ratioFine = 0.4f;
modOsc(p, 8, Sine, 2.0f, 0.06f);
amp(p, 3.0f, 300.0f, 0.9f, 200.0f);
mono(p, 0.18f);
vibrato(p, 5.5f, 0.06f);
wheel(p, oscLevel(3), 0.2f);
echo(p, DelayType::PingPong, "1/4", 0.3f, 0.2f);
macro(p, 1, 0.0f, { to(oscLevel(3), 0.2f), to(oscLevel(4), 0.15f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Formant Solo", "Lead", "Formant Sweep table: the wheel and pressure move the formant through the harmonics.", 67, True,
           "wavetable, formant, lead, expressive", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, FormantSweepF, 1.0f, 0.5f, 0.25f);
osc(p, 2, Saw, 1.0f, 0.15f).ratioFine = 0.3f;
wheel(p, oscScan(1), 0.6f);
route(p, src::ChannelAT, oscScan(1), 0.3f);
amp(p, 5.0f, 300.0f, 0.9f, 200.0f);
mono(p, 0.15f);
vibrato(p, 5.5f, 0.05f);
echo(p, DelayType::Stereo, "1/8D", 0.3f, 0.2f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.6f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Sync Table Lead", "Lead", "Hard Sync table on a slow LFO plus the envelope: the classic sweeping sync lead.", 67, True,
           "wavetable, hard sync, lead, sweep", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, HardSyncF, 1.0f, 0.45f, 0.1f);
famOsc(p, 2, HardSyncF, 1.0f, 0.2f, 0.3f).ratioFine = 0.4f;
lfo(p, 4, LfoWave::Triangle, 0.4f);
adsr(p.env[3], 0.0f, 600.0f, 0.3f, 200.0f);
route(p, envSrc(4), oscScan(1), 0.5f);
route(p, lfoUni(4), oscScan(1), 0.3f);
route(p, lfoUni(4), oscScan(2), 0.4f);
amp(p, 2.0f, 300.0f, 0.9f, 200.0f);
analogLp(p, 5000.0f, 0.1f);
mono(p, 0.15f);
wheel(p, oscScan(1), 0.4f);
echo(p, DelayType::Stereo, "1/8", 0.25f, 0.18f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.5f) });
macro(p, 2, 0.0f, { to(afCut, -0.3f) });
'''),
    preset("Pressure Index", "Lead", "FM Index table: velocity and aftertouch raise the FM index for a brighter, harder tone.", 67, True,
           "wavetable, fm, lead, aftertouch", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, FmIndexF, 1.0f, 0.5f, 0.1f);
osc(p, 2, Sine, 2.0f, 0.12f);
route(p, src::Velocity, oscScan(1), 0.35f);
route(p, src::ChannelAT, oscScan(1), 0.5f);
wheel(p, oscScan(1), 0.4f);
amp(p, 4.0f, 300.0f, 0.9f, 220.0f);
mono(p, 0.12f);
vibrato(p, 5.5f, 0.05f);
room(p, ReverbType::Plate, 1.6f, 0.2f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Thin Pulse Lead", "Lead", "Pulse Width table: a narrow, nasal pulse that slowly widens and narrows.", 72, True,
           "wavetable, pwm, lead, nasal", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, PulseWidthF, 1.0f, 0.45f, 0.6f);
lfo(p, 4, LfoWave::Sine, 0.3f);
route(p, lfoSrc(4), oscScan(1), 0.3f);
analogLp(p, 4500.0f, 0.15f);
amp(p, 3.0f, 300.0f, 0.9f, 200.0f);
mono(p, 0.12f);
vibrato(p, 5.8f, 0.05f, 300.0f);
echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.2f);
macro(p, 1, 0.0f, { to(oscScan(1), -0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Bent Phase Lead", "Lead", "Phase Bend table lead with a resonant sweep on every note; wheel bends further.", 67, True,
           "wavetable, phase distortion, lead, resonant", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, PhaseBendF, 1.0f, 0.5f, 0.2f);
osc(p, 2, Saw, 1.0f, 0.12f).ratioFine = -0.3f;
adsr(p.env[3], 0.0f, 400.0f, 0.4f, 200.0f);
route(p, envSrc(4), oscScan(1), 0.5f);
wheel(p, oscScan(1), 0.4f);
amp(p, 2.0f, 300.0f, 0.9f, 200.0f);
mono(p, 0.15f);
vibrato(p, 5.5f, 0.05f);
echo(p, DelayType::Stereo, "1/8D", 0.3f, 0.2f);
macro(p, 1, 0.0f, { to(depthOf(0), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Chip Lead", "Lead", "Crusher table and 8-bit Vintage Digital: a chiptune square with fast vibrato and arpeggio glide.", 72, True,
           "wavetable, chiptune, lo-fi, 8-bit", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, fam(CrusherF, 14), 1.0f, 0.4f);
osc(p, 2, Pulse25, 1.0f, 0.2f);
p.voice.vintage = true;
p.voice.bitDepth = 8;
amp(p, 0.0f, 200.0f, 0.8f, 80.0f);
mono(p, 0.05f);
vibrato(p, 8.0f, 0.06f, 200.0f);
echo(p, DelayType::Mono, "1/8", 0.25f, 0.15f);
macro(p, 1, 0.0f, { to(oscLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Folding Solo", "Lead", "Wavefold table lead: pressure and the wheel fold the sine into bright overtones.", 67, True,
           "wavetable, wavefold, lead, aftertouch", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, WavefoldF, 1.0f, 0.5f, 0.15f);
osc(p, 2, Sine, 0.5f, 0.2f);
route(p, src::ChannelAT, oscScan(1), 0.5f);
wheel(p, oscScan(1), 0.5f);
route(p, src::Velocity, oscScan(1), 0.2f);
amp(p, 4.0f, 300.0f, 0.9f, 220.0f);
mono(p, 0.15f);
vibrato(p, 5.5f, 0.05f);
room(p, ReverbType::Plate, 1.5f, 0.2f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Scream Lead", "Lead", "Star Loop: one operator and three carriers modulate each other; turn it up for chaos.", 67, True,
           "lead, fm, feedback, aggressive", r'''
Patch p = blank("Star Loop");
osc(p, 1, Saw, 1.0f, 0.3f);
osc(p, 2, Saw, 1.0f, 0.2f).ratioFine = 0.5f;
osc(p, 3, Square, 2.0f, 0.1f);
modOsc(p, 8, Sine, 1.0f, 0.06f);
amp(p, 2.0f, 300.0f, 0.9f, 200.0f);
analogLp(p, 3500.0f, 0.3f);
mono(p, 0.12f);
distort(p, 0.4f, 0.1f);
wheel(p, oscLevel(8), 0.2f);
echo(p, DelayType::Stereo, "1/8D", 0.3f, 0.18f);
macro(p, 1, 0.0f, { to(oscLevel(8), 0.2f) });
macro(p, 2, 0.0f, { to(fx(dst::PreFxParam1), 0.4f) });
'''),
    preset("Whistle Lead", "Lead", "A pure whistled sine with long glide, delayed vibrato and a hall.", 79, True,
           "lead, whistle, sine, glide", r'''
Patch p = blank("One Pair");
osc(p, 1, Sine, 1.0f, 0.55f);
modOsc(p, 2, Sine, 2.0f, 0.02f);
amp(p, 30.0f, 300.0f, 0.9f, 250.0f);
mono(p, 0.3f);
vibrato(p, 6.0f, 0.08f, 500.0f);
room(p, ReverbType::Hall, 2.5f, 0.3f);
macro(p, 1, 0.0f, { to(oscLevel(2), 0.08f) });
macro(p, 2, 0.0f, { to(dst::VoiceBase + dst::VoiceGlideTime, 0.3f) });
'''),

    # ---------------------------------------------------------------------------------------------- Brass (9)
    preset("Stacked Brass", "Brass", "Brass Stack algorithm: three modulators open with the envelope like real lips.", 60, True,
           "brass, fm, ensemble, swell", r'''
Patch p = blank("Brass Stack");
osc(p, 1, Sine, 1.0f, 0.5f);
OscParams& a = osc(p, 2, Sine, 1.0f, 0.05f);
envLevel(a, 0.25f, 80.0f, 300.0f, 0.6f, 200.0f, 0.4f);
modOsc(p, 3, Sine, 2.0f, 0.05f);
strike(p, 4, Sine, 3.0f, 0.1f, 300.0f, 0.5f);
OscParams& c = osc(p, 5, Sine, 1.0f, 0.03f);
envLevel(c, 0.12f, 100.0f, 400.0f, 0.5f, 200.0f, 0.4f);
osc(p, 6, Saw, 1.0f, 0.12f).ratioFine = 0.3f;
amp(p, 60.0f, 400.0f, 0.85f, 250.0f);
vibrato(p, 5.0f, 0.05f, 600.0f);
room(p, ReverbType::Hall, 2.0f, 0.2f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.15f) });
macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvAttack), 0.3f) });
'''),
    preset("Bright Horns", "Brass", "Saw Bright table horns: the brightness follows the envelope and velocity.", 60, True,
           "wavetable, brass, horns, swell", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, SawBrightF, 1.0f, 0.4f, 0.1f);
famOsc(p, 2, SawBrightF, 1.0f, 0.3f, 0.1f).ratioFine = 0.35f;
adsr(p.env[3], 80.0f, 500.0f, 0.5f, 250.0f);
p.env[3].velAmount = 0.5f;
route(p, envSrc(4), oscScan(1), 0.7f);
route(p, envSrc(4), oscScan(2), 0.7f);
amp(p, 50.0f, 400.0f, 0.85f, 250.0f);
vibrato(p, 5.0f, 0.05f, 700.0f);
room(p, ReverbType::Hall, 2.0f, 0.2f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.3f), to(oscScan(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Solo Trumpet", "Brass", "A mono trumpet: PD Saw bite on the attack, lip vibrato and short glide.", 67, True,
           "brass, trumpet, phase distortion, mono", r'''
Patch p = blank("One Pair");
osc(p, 1, Sine, 1.0f, 0.5f);
OscParams& m = osc(p, 2, Sine, 1.0f, 0.1f);
m.mode = OscMode::PDSaw;
envLevel(m, 0.35f, 60.0f, 300.0f, 0.5f, 150.0f, 0.5f);
amp(p, 40.0f, 300.0f, 0.85f, 180.0f);
mono(p, 0.08f);
vibrato(p, 5.5f, 0.07f, 400.0f);
room(p, ReverbType::Hall, 1.8f, 0.2f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
''', style="lead"),
    preset("Cup Mute Horn", "Brass", "A cup-muted horn: the Formant Sweep table held low, with a small wah on each note.", 60, True,
           "wavetable, brass, muted, formant", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, FormantSweepF, 1.0f, 0.5f, 0.2f);
osc(p, 2, Sine, 1.0f, 0.15f);
adsr(p.env[3], 80.0f, 300.0f, 0.2f, 200.0f);
route(p, envSrc(4), oscScan(1), 0.25f);
amp(p, 40.0f, 300.0f, 0.85f, 200.0f);
vibrato(p, 5.0f, 0.04f, 500.0f);
room(p, ReverbType::Room, 1.3f, 0.2f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.4f) });
macro(p, 2, 0.0f, { to(depthOf(0), 0.3f) });
'''),
    preset("Soft Brass", "Brass", "A mellow unison brass pad with slow swells and gentle chorus.", 60, True,
           "brass, pad, unison, soft", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, Saw, 1.0f, 0.35f);
osc(p, 2, fam(SawBrightF, 8), 1.0f, 0.25f).ratioFine = 0.25f;
unison(p, 3, 0.2f);
analogLp(p, 900.0f, 0.1f, 0.4f, 300.0f, 800.0f, 0.5f, 400.0f);
amp(p, 250.0f, 600.0f, 0.85f, 500.0f);
chorus(p, 0.35f);
room(p, ReverbType::Hall, 2.5f, 0.25f);
macro(p, 1, 0.0f, { to(afCut, 0.35f) });
macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvAttack), 0.4f) });
'''),
    preset("Tuba Weight", "Brass", "Tuba and trombone weight: a dark FM pair with a growl as it opens.", 48, True,
           "brass, low, tuba, fm", r'''
Patch p = blank("Two Pairs, Four Free");
osc(p, 1, Sine, 1.0f, 0.5f);
OscParams& m = osc(p, 2, Sine, 1.0f, 0.06f);
envLevel(m, 0.25f, 120.0f, 400.0f, 0.5f, 200.0f, 0.5f);
osc(p, 3, Sine, 0.5f, 0.25f);
modOsc(p, 4, Sine, 1.0f, 0.05f);
analogLp(p, 1500.0f, 0.1f);
amp(p, 80.0f, 400.0f, 0.85f, 250.0f);
vibrato(p, 4.5f, 0.04f, 700.0f);
room(p, ReverbType::Hall, 2.0f, 0.2f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.25f) });
macro(p, 2, 0.0f, { to(afCut, 0.3f) });
'''),
    preset("Stab Brass", "Brass", "A short, punchy synth-brass stab: unison saws through a snappy low-pass envelope.", 60, False,
           "brass, stab, analog, punchy", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, Saw, 1.0f, 0.35f);
osc(p, 2, Saw, 1.0f, 0.35f).ratioFine = 0.5f;
osc(p, 3, Saw, 2.0f, 0.1f);
analogLp(p, 600.0f, 0.2f, 0.6f, 5.0f, 300.0f, 0.2f, 200.0f);
p.afilt.velEnv = 0.4f;
amp(p, 3.0f, 400.0f, 0.4f, 200.0f);
room(p, ReverbType::Plate, 1.5f, 0.2f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(dst::env(1, dst::EnvDecay), 0.4f) });
''', style="chords"),
    preset("Brass Choir", "Brass", "Two Into Three algorithm: two shared modulators color three horns; three flutes support.", 60, True,
           "brass, fm, ensemble, choir", r'''
Patch p = blank("Two Into Three");
osc(p, 1, Sine, 1.0f, 0.35f);
osc(p, 2, Sine, 1.0f, 0.3f).ratioFine = 0.3f;
osc(p, 3, Sine, 2.0f, 0.2f);
osc(p, 4, Sine, 0.5f, 0.15f);
osc(p, 5, Triangle, 1.0f, 0.12f).ratioFine = -0.3f;
osc(p, 6, Sine, 3.0f, 0.06f);
OscParams& m1 = osc(p, 7, Sine, 1.0f, 0.04f);
envLevel(m1, 0.2f, 120.0f, 500.0f, 0.5f, 250.0f, 0.4f);
OscParams& m2 = osc(p, 8, Sine, 2.0f, 0.02f);
envLevel(m2, 0.1f, 150.0f, 500.0f, 0.4f, 250.0f, 0.4f);
amp(p, 80.0f, 400.0f, 0.85f, 300.0f);
vibrato(p, 5.0f, 0.05f, 600.0f);
room(p, ReverbType::Hall, 2.5f, 0.25f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.15f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Fanfare", "Brass", "Bright unison horns in a big hall, made for held chords.", 60, True,
           "brass, fanfare, unison, bright", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, SawBrightF, 1.0f, 0.35f, 0.75f);
famOsc(p, 2, SawBrightF, 2.0f, 0.12f, 0.6f);
unison(p, 4, 0.25f);
adsr(p.env[3], 40.0f, 600.0f, 0.4f, 300.0f);
route(p, envSrc(4), oscScan(1), 0.25f);
amp(p, 40.0f, 500.0f, 0.85f, 400.0f);
room(p, ReverbType::Hall, 3.0f, 0.3f);
macro(p, 1, 0.0f, { to(oscScan(1), -0.4f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
''', style="chords"),
]
