from common import preset

PRESETS = [
    # ---------------------------------------------------------------------------------------------- Arp (9)
    preset("Phrase Runner", "Arp", "Phrase mode arpeggio: a built-in melodic phrase played over the held chord. Hold a chord.", 60, True,
           "arp, phrase, fm, tempo", r'''
Patch p = blank("Two Pairs, Four Free");
osc(p, 1, Sine, 1.0f, 0.45f);
strike(p, 2, Sine, 2.0f, 0.2f, 200.0f, 0.6f);
osc(p, 3, Triangle, 2.0f, 0.12f);
amp(p, 0.0f, 400.0f, 0.2f, 150.0f);
arp(p, ArpMode::Phrase, 2, "1/16", 0.6f);
p.arp.phrase = 5;
echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.18f);
room(p, ReverbType::Room, 1.2f, 0.15f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
''', style="arp"),
    preset("Ratchet Rain", "Arp", "Up arpeggio whose steps randomly ratchet into rolls of 2, 4 and 8. Hold a chord.", 60, True,
           "arp, ratchet, tempo, pluck", r'''
Patch p = blank("One Pair");
osc(p, 1, fam(SawBrightF, 10), 1.0f, 0.45f);
strike(p, 2, Sine, 1.0f, 0.15f, 120.0f, 0.6f);
analogLp(p, 1500.0f, 0.3f, 0.4f, 0.0f, 150.0f, 0.0f, 100.0f);
amp(p, 0.0f, 250.0f, 0.0f, 120.0f);
arp(p, ArpMode::Up, 2, "1/8", 0.5f);
p.arp.ratchet = 0.45f;
echo(p, DelayType::Stereo, "1/8D", 0.3f, 0.15f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(afRes, 0.3f) });
''', style="arp"),
    preset("Maybe Steps", "Arp", "Random-octave arpeggio where each step plays only 70% of the time. Hold a chord.", 60, True,
           "arp, random, chance, glassy", r'''
Patch p = blank("Glass");
osc(p, 1, Sine, 1.0f, 0.4f);
strike(p, 2, Sine, 3.0f, 0.15f, 200.0f, 0.6f);
strike(p, 3, Sine, 7.0f, 0.06f, 100.0f, 0.7f);
modOsc(p, 4, Sine, 1.0f, 0.03f);
amp(p, 0.0f, 500.0f, 0.0f, 300.0f);
arp(p, ArpMode::Up, 3, "1/16", 0.5f);
p.arp.octaveMode = ArpOctaveMode::Random;
p.arp.chance = 0.7f;
echo(p, DelayType::PingPong, "1/8", 0.4f, 0.2f);
room(p, ReverbType::Hall, 2.5f, 0.25f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayFeedback), 0.3f) });
''', style="arp"),
    preset("Leaping Octaves", "Arp", "Alternating octave mode: each note jumps between low and high octaves. Hold a chord.", 55, True,
           "arp, octaves, leap, analog", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, Saw, 1.0f, 0.35f);
osc(p, 2, Square, 1.0f, 0.25f).ratioFine = 0.3f;
analogLp(p, 1200.0f, 0.35f, 0.4f, 0.0f, 180.0f, 0.1f, 100.0f);
amp(p, 0.0f, 300.0f, 0.3f, 100.0f);
arp(p, ArpMode::Up, 3, "1/16", 0.55f);
p.arp.octaveMode = ArpOctaveMode::Alt;
echo(p, DelayType::Stereo, "1/8D", 0.25f, 0.15f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(afRes, 0.3f) });
''', style="arp"),
    preset("Mirror Arp", "Arp", "Up-and-down arpeggio that repeats the top and bottom notes, over two octaves. Hold a chord.", 60, True,
           "arp, updown, fm, bell", r'''
Patch p = blank("Kalimba");
osc(p, 1, Sine, 1.0f, 0.4f);
strike(p, 2, Sine, 3.0f, 0.15f, 120.0f, 0.6f);
strike(p, 3, Sine, 7.0f, 0.06f, 60.0f, 0.7f);
osc(p, 4, Sine, 2.0f, 0.12f);
strike(p, 5, Sine, 1.0f, 0.08f, 150.0f, 0.5f);
amp(p, 0.0f, 600.0f, 0.0f, 300.0f);
arp(p, ArpMode::UpAndDown, 2, "1/16", 0.6f);
echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.2f);
room(p, ReverbType::Room, 1.2f, 0.18f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
''', style="arp"),
    preset("Shuffled Chord", "Arp", "Random Once: the chord is shuffled once and that order repeats until it changes. Hold a chord.", 60, True,
           "arp, random, wavetable, pluck", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, PhaseBendF, 1.0f, 0.4f);
adsr(p.env[3], 0.0f, 200.0f, 0.0f, 100.0f);
route(p, envSrc(4), oscScan(1), 0.8f);
amp(p, 0.0f, 400.0f, 0.0f, 150.0f);
arp(p, ArpMode::RandomOnce, 2, "1/16", 0.5f);
echo(p, DelayType::Stereo, "1/8D", 0.3f, 0.18f);
room(p, ReverbType::Room, 1.0f, 0.15f);
macro(p, 1, 0.0f, { to(depthOf(0), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
''', style="arp"),
    preset("Spectral Arp", "Arp", "Every arpeggio note lands on a random frame of the Spectra table. Hold a chord.", 60, True,
           "arp, wavetable, random, spectral", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, SpectraF, 1.0f, 0.45f);
osc(p, 2, Sine, 0.5f, 0.15f);
route(p, src::RandomPerNote, oscScan(1), 1.0f);
amp(p, 0.0f, 300.0f, 0.1f, 150.0f);
arp(p, ArpMode::Up, 2, "1/16", 0.5f);
echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.2f);
room(p, ReverbType::Hall, 2.0f, 0.2f);
macro(p, 1, 0.0f, { to(depthOf(0), -0.5f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
''', style="arp"),
    preset("Overlap Arp", "Arp", "Gate over 100%: each arpeggio note rings into the next for a legato, padlike arp. Hold a chord.", 60, True,
           "arp, legato, pad, soft", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, fam(SawBrightF, 6), 1.0f, 0.35f);
osc(p, 2, Triangle, 1.0f, 0.25f).ratioFine = 0.3f;
analogLp(p, 2000.0f, 0.1f);
amp(p, 40.0f, 600.0f, 0.6f, 600.0f);
arp(p, ArpMode::UpDown, 2, "1/8", 1.3f);
chorus(p, 0.4f);
room(p, ReverbType::Hall, 3.0f, 0.3f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
''', style="arp"),
    preset("Chord Hits", "Arp", "Chord mode: the whole held chord restruck in a tight 8th-note rhythm. Hold a chord.", 60, True,
           "arp, chord, stab, rhythmic", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, Saw, 1.0f, 0.3f);
osc(p, 2, Saw, 1.0f, 0.3f).ratioFine = 0.4f;
analogLp(p, 900.0f, 0.25f, 0.5f, 0.0f, 150.0f, 0.0f, 100.0f);
amp(p, 0.0f, 200.0f, 0.0f, 100.0f);
arp(p, ArpMode::Chord, 1, "1/8", 0.4f);
p.arp.swing = 0.15f;
room(p, ReverbType::Plate, 1.4f, 0.2f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(dst::env(1, dst::EnvDecay), 0.4f) });
''', style="arp"),

    # ---------------------------------------------------------------------------------------------- Sequence (10)
    preset("Key Bassline", "Sequence", "Step sequencer bassline started and transposed by the key you hold. Hold a note.", 48, True,
           "sequencer, bass, tempo, key", r'''
Patch p = blank("One Pair");
osc(p, 1, Sine, 1.0f, 0.5f);
strike(p, 2, Sine, 1.0f, 0.25f, 150.0f, 0.7f);
osc(p, 3, Saw, 1.0f, 0.15f);
analogLp(p, 1200.0f, 0.3f, 0.3f, 0.0f, 150.0f, 0.0f, 100.0f);
amp(p, 0.0f, 250.0f, 0.3f, 80.0f);
keySeq(p, 16, "1/16");
seqPattern(p, 0, { 60, -1, 60, 72, -1, 60, 67, -1, 60, -1, 63, 60, -1, 70, 67, 65 }, 100, 3);
for (int s : { 0, 6, 12 }) seqStep(p, 0, s).notes[0].velocity = 127;
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(oscEnvLevel(2), 0.3f) });
'''),
    preset("Mallet Pattern", "Sequence", "A 12-step marimba pattern in 16ths from the step sequencer. Hold a note.", 60, True,
           "sequencer, mallet, fm, pattern", r'''
Patch p = blank("Two Pairs, Four Free");
osc(p, 1, Sine, 1.0f, 0.45f);
strike(p, 2, Sine, 4.0f, 0.2f, 80.0f, 0.7f);
osc(p, 3, Sine, 3.0f, 0.1f);
strike(p, 4, Sine, 10.0f, 0.06f, 40.0f, 0.8f);
amp(p, 0.0f, 500.0f, 0.0f, 300.0f);
keySeq(p, 12, "1/16");
seqPattern(p, 0, { 60, 67, 72, 67, 63, 67, 70, 67, 60, 65, 72, 74 }, 100, 3);
echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.18f);
room(p, ReverbType::Room, 1.2f, 0.18f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Ratchet Line", "Sequence", "Sequencer line with ratcheted steps that roll in 3s and 4s. Hold a note.", 48, True,
           "sequencer, ratchet, acid, tempo", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, ResonantSweepF, 1.0f, 0.5f, 0.2f);
adsr(p.env[3], 0.0f, 180.0f, 0.0f, 80.0f);
route(p, envSrc(4), oscScan(1), 0.6f);
amp(p, 0.0f, 200.0f, 0.3f, 60.0f);
keySeq(p, 16, "1/8");
seqPattern(p, 0, { 60, 60, 63, 60, 67, 60, 70, 72, 60, 60, 63, 65, 67, 63, 60, 58 }, 100, 3);
seqStep(p, 0, 3).ratchet = 3;
seqStep(p, 0, 7).ratchet = 4;
seqStep(p, 0, 13).ratchet = 2;
distort(p, 0.25f, 0.0f);
macro(p, 1, 0.0f, { to(depthOf(0), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::PreFxParam1), 0.4f) });
'''),
    preset("Probable Pulse", "Sequence", "Sequencer pulses where each step has its own probability: it never repeats exactly. Hold a note.", 60, True,
           "sequencer, probability, generative, pluck", r'''
Patch p = blank("One Pair");
osc(p, 1, fam(PulseWidthF, 6), 1.0f, 0.45f);
strike(p, 2, Sine, 2.0f, 0.15f, 120.0f, 0.6f);
analogLp(p, 2500.0f, 0.2f);
amp(p, 0.0f, 300.0f, 0.0f, 150.0f);
keySeq(p, 16, "1/16");
seqPattern(p, 0, { 60, 72, 67, 72, 60, 70, 67, 75, 60, 72, 67, 72, 63, 70, 67, 79 }, 100, 2);
for (int s = 1; s < 16; s += 2) seqStep(p, 0, s).probability = 55;
seqStep(p, 0, 15).probability = 30;
echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.2f);
room(p, ReverbType::Room, 1.2f, 0.15f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Lane Filter Groove", "Sequence", "A macro lane steps the filter cutoff under a steady sequencer line. Hold a note.", 48, True,
           "sequencer, macro lane, filter, groove", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, Saw, 1.0f, 0.4f);
osc(p, 2, Square, 0.5f, 0.2f);
analogLp(p, 400.0f, 0.4f, 0.2f, 0.0f, 120.0f, 0.0f, 80.0f);
amp(p, 0.0f, 200.0f, 0.5f, 60.0f);
keySeq(p, 8, "1/16");
seqPattern(p, 0, { 60, 60, 72, 60, 67, 60, 70, 60 }, 100, 3);
seqLane(p, 1, { 0.1f, 0.3f, 0.6f, 0.9f, 0.4f, 0.2f, 0.7f, 0.5f, 0.1f, 0.5f, 0.8f, 0.3f }, 12, "1/16");
macro(p, 1, 0.0f, { to(afCut, 0.5f) });
macro(p, 2, 0.0f, { to(afRes, 0.3f) });
'''),
    preset("Ping Pong Steps", "Sequence", "Sequencer in Ping-Pong mode: the pattern runs forward then back. Hold a note.", 60, True,
           "sequencer, pingpong, bell, pattern", r'''
Patch p = blank("Chimes");
osc(p, 1, Sine, 1.0f, 0.35f);
osc(p, 2, Sine, 2.0f, 0.15f);
strike(p, 3, Sine, 3.5f, 0.15f, 300.0f, 0.6f);
osc(p, 4, Sine, 4.0f, 0.1f);
strike(p, 5, Sine, 1.4f, 0.08f, 200.0f, 0.6f);
amp(p, 0.0f, 700.0f, 0.0f, 500.0f);
keySeq(p, 7, "1/8");
p.seq.track[0].mode = SeqPlayMode::PingPong;
seqPattern(p, 0, { 60, 62, 64, 67, 69, 72, 76 }, 100, 4);
echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.2f);
room(p, ReverbType::Hall, 2.5f, 0.25f);
macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Chord Sequence", "Sequence", "Each sequencer step plays a chord: a four-bar progression from one held key. Hold a note.", 60, True,
           "sequencer, chords, pad, progression", r'''
Patch p = blank("Eight Carriers");
osc(p, 1, fam(SawBrightF, 7), 1.0f, 0.3f);
osc(p, 2, fam(SawBrightF, 7), 1.0f, 0.25f).ratioFine = 0.3f;
analogLp(p, 1800.0f, 0.1f, 0.3f, 10.0f, 400.0f, 0.4f, 300.0f);
amp(p, 20.0f, 600.0f, 0.6f, 400.0f);
keySeq(p, 8, "1/4");
static const int chords[8][3] = { { 60, 64, 67 }, { 60, 64, 67 }, { 57, 60, 64 }, { 57, 60, 64 },
                                   { 53, 57, 60 }, { 53, 57, 60 }, { 55, 59, 62 }, { 55, 59, 62 } };
for (int s = 0; s < 8; ++s)
{
    SeqStep& st = seqStep(p, 0, s);
    st.count = 3;
    for (int n = 0; n < 3; ++n) st.notes[static_cast<size_t>(n)] = { static_cast<uint8_t>(chords[s][n]), 90, 3 };
}
chorus(p, 0.35f);
room(p, ReverbType::Hall, 2.5f, 0.25f);
macro(p, 1, 0.0f, { to(afCut, 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Polymeter", "Sequence", "Two sequencer tracks of 5 and 7 steps drift against each other. Hold a note.", 60, True,
           "sequencer, polymeter, two tracks, generative", r'''
Patch p = blank("Two Pairs, Four Free");
osc(p, 1, Sine, 1.0f, 0.4f);
strike(p, 2, Sine, 3.0f, 0.15f, 150.0f, 0.6f);
osc(p, 3, Triangle, 1.0f, 0.15f);
amp(p, 0.0f, 400.0f, 0.0f, 250.0f);
keySeq(p, 5, "1/16");
p.seq.track[1].enabled = true;
p.seq.track[1].length = 7;
p.seq.track[1].syncDiv = syncDiv("1/16");
seqPattern(p, 0, { 60, -1, 67, 70, -1 }, 100, 2);
seqPattern(p, 1, { 72, 75, -1, 79, -1, 77, 74 }, 80, 2);
echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.2f);
room(p, ReverbType::Room, 1.3f, 0.18f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Loose Groove", "Sequence", "Sequencer drift: steps land a little late and vary in velocity, like a human. Hold a note.", 48, True,
           "sequencer, drift, humanize, electric piano", r'''
Patch p = blank("Electric Piano");
osc(p, 1, Sine, 1.0f, 0.45f);
strike(p, 2, Sine, 14.0f, 0.1f, 150.0f, 0.9f);
osc(p, 3, Sine, 1.0f, 0.3f).ratioFine = 0.15f;
strike(p, 4, Sine, 1.0f, 0.25f, 800.0f, 0.6f);
amp(p, 0.0f, 900.0f, 0.1f, 200.0f);
p.vca.velEnv = 0.6f;
keySeq(p, 16, "1/16");
seqPattern(p, 0, { 60, -1, 67, -1, -1, 70, -1, 67, 60, -1, 72, -1, 70, -1, 67, -1 }, 100, 3);
for (int s = 0; s < 16; ++s) seqStep(p, 0, s).drift = 60;
tremolo(p, 4.0f, 0.2f);
room(p, ReverbType::Room, 1.2f, 0.18f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::PostFxParam2), 0.4f) });
'''),
    preset("Table Sequence", "Sequence", "A macro lane walks the Harmonic Build table step by step under the sequence. Hold a note.", 60, True,
           "sequencer, macro lane, wavetable, scan", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, HarmonicBuildF, 1.0f, 0.45f);
osc(p, 2, Sine, 0.5f, 0.15f);
amp(p, 0.0f, 250.0f, 0.2f, 100.0f);
keySeq(p, 8, "1/16");
seqPattern(p, 0, { 60, 63, 67, 72, 60, 63, 67, 70 }, 100, 2);
seqLane(p, 1, { 0.0f, 0.15f, 0.3f, 0.45f, 0.6f, 0.75f, 0.9f, 1.0f, 0.8f, 0.6f, 0.4f, 0.2f }, 12, "1/16");
echo(p, DelayType::Stereo, "1/8D", 0.3f, 0.18f);
macro(p, 1, 0.0f, { to(oscScan(1), 1.0f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),

    # ---------------------------------------------------------------------------------------------- FX (11)
    preset("Ring Siren", "FX", "Ring of Five with a slow pitch LFO: a wailing, metallic siren.", 60, True,
           "fx, siren, feedback, metallic", r'''
Patch p = blank("Ring of Five");
osc(p, 1, Sine, 1.0f, 0.4f);
for (int i = 2; i <= 5; ++i) modOsc(p, i, Sine, static_cast<float>(i - 1), 0.05f);
osc(p, 6, Sine, 2.0f, 0.1f);
lfo(p, 4, LfoWave::Triangle, 0.5f);
route(p, lfoSrc(4), allPitch, 0.1f);
amp(p, 100.0f, 500.0f, 1.0f, 800.0f);
room(p, ReverbType::Hall, 2.5f, 0.3f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.1f) });
macro(p, 2, 0.0f, { to(dst::LfoBase + 3 * dst::kLfoParams + dst::LfoRate, 0.4f) });
'''),
    preset("Quad Ring Static", "FX", "Quad Ring: four pairs in a loop, with sample-and-hold on the feedback: radio static.", 60, True,
           "fx, static, noise, feedback", r'''
Patch p = blank("Quad Ring");
for (int i = 1; i <= 7; i += 2) osc(p, i, Sine, static_cast<float>(i), 0.15f);
for (int i = 2; i <= 8; i += 2) modOsc(p, i, Sine, static_cast<float>(i) * 1.37f, 0.1f);
syncLfo(p, 4, LfoWave::SampleHold, "1/16");
route(p, lfoUni(4), dst::ModulatorLevel, 0.15f);
amp(p, 5.0f, 500.0f, 1.0f, 400.0f);
lofi(p, 11025.0f, 3000.0f, LoFiFilter::Radio);
room(p, ReverbType::Room, 1.2f, 0.2f);
macro(p, 1, 0.0f, { to(depthOf(0), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::PostFxMix), -0.5f) });
'''),
    preset("Feedback Chatter", "FX", "Two-Way Chain: four operators pass modulation back and forth, chattering.", 60, True,
           "fx, feedback, chatter, digital", r'''
Patch p = blank("Two-Way Chain");
osc(p, 1, Sine, 1.0f, 0.35f);
modOsc(p, 2, Sine, 2.0f, 0.08f);
modOsc(p, 3, Sine, 3.0f, 0.06f);
modOsc(p, 4, Sine, 5.0f, 0.05f);
osc(p, 5, Sine, 0.5f, 0.15f);
lfo(p, 4, LfoWave::Square, 6.0f);
route(p, lfoUni(4), oscLevel(3), 0.08f);
amp(p, 5.0f, 500.0f, 1.0f, 400.0f);
echo(p, DelayType::PingPong, "1/16", 0.4f, 0.2f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.1f) });
macro(p, 2, 0.0f, { to(dst::LfoBase + 3 * dst::kLfoParams + dst::LfoRate, 0.4f) });
'''),
    preset("Cross Talk", "FX", "Cross-Modulated Pairs: four pairs of carriers modulate each other: garbled voices.", 60, True,
           "fx, garbled, fm, voices", r'''
Patch p = blank("Cross-Modulated Pairs");
for (int i = 1; i <= 8; ++i) osc(p, i, Sine, i % 2 ? 1.0f : 1.5f, 0.08f).ratioFine = 0.2f * static_cast<float>(i);
LfoParams& l = lfo(p, 4, LfoWave::Random, 3.0f);
l.smooth = 0.5f;
route(p, lfoUni(4), dst::CarrierLevel, 0.06f);
route(p, lfoSrc(4), allPitch, 0.03f);
amp(p, 20.0f, 500.0f, 1.0f, 500.0f);
room(p, ReverbType::Room, 1.5f, 0.2f);
macro(p, 1, 0.0f, { to(depthOf(1), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Fan Riser", "FX", "Feedback Fan: a looping fan of operators rising in pitch and brightness while held.", 48, True,
           "fx, riser, build, feedback", r'''
Patch p = blank("Feedback Fan");
for (int i = 1; i <= 4; ++i) osc(p, i, Sine, static_cast<float>(i), 0.15f);
osc(p, 5, Saw, 1.0f, 0.1f);
modOsc(p, 8, Sine, 1.0f, 0.02f);
adsr(p.env[3], 4000.0f, 100.0f, 1.0f, 300.0f);
route(p, envSrc(4), allPitch, 0.25f);
route(p, envSrc(4), oscLevel(8), 0.12f);
amp(p, 500.0f, 500.0f, 1.0f, 600.0f);
echo(p, DelayType::PingPong, "1/8", 0.4f, 0.2f);
room(p, ReverbType::Hall, 3.0f, 0.3f);
macro(p, 1, 0.0f, { to(dst::env(3, dst::EnvAttack), 0.4f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayFeedback), 0.3f) });
'''),
    preset("Spinning Rings", "FX", "Ring Into Fan in Osc Stereo: a three-operator loop spins five carriers around the field.", 60, True,
           "fx, osc stereo, spinning, metallic", r'''
Patch p = blank("Ring Into Fan");
for (int i = 1; i <= 5; ++i) osc(p, i, Sine, static_cast<float>(i), 0.12f).pan = -1.0f + 0.5f * static_cast<float>(i - 1);
modOsc(p, 6, Sine, 1.5f, 0.04f);
modOsc(p, 7, Sine, 2.5f, 0.04f);
modOsc(p, 8, Sine, 3.5f, 0.05f);
stereoOscs(p, 10.0f);
lfo(p, 4, LfoWave::Sine, 0.3f);
route(p, lfoUni(4), oscLevel(8), 0.08f);
amp(p, 50.0f, 500.0f, 1.0f, 800.0f);
room(p, ReverbType::Hall, 2.5f, 0.3f);
macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.08f) });
macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
'''),
    preset("Circuit Bend", "FX", "Carrier Cross: two carriers modulate each other while a stepped LFO bends the pitch.", 60, True,
           "fx, circuit bend, glitch, stepped", r'''
Patch p = blank("Carrier Cross");
osc(p, 1, Square, 1.0f, 0.25f);
modOsc(p, 2, Sine, 2.0f, 0.08f);
osc(p, 3, Saw, 1.5f, 0.2f);
modOsc(p, 4, Sine, 3.0f, 0.06f);
syncLfo(p, 4, LfoWave::Step, "1/16");
for (int s = 0; s < 16; ++s) p.lfoSteps[3][static_cast<size_t>(s)] = (s * 7 % 11) / 5.5f - 1.0f;
route(p, lfoSrc(4), allPitch, 0.15f);
amp(p, 5.0f, 500.0f, 1.0f, 300.0f);
p.voice.bitDepth = 10;
room(p, ReverbType::Room, 1.0f, 0.15f);
macro(p, 1, 0.0f, { to(depthOf(0), 0.3f) });
macro(p, 2, 0.0f, { to(dst::ModulatorLevel, 0.1f) });
'''),
    preset("Bit Sweep", "FX", "Crusher table swept downward while the pitch falls: a digital downlifter.", 72, False,
           "wavetable, fx, downlifter, bitcrush", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, CrusherF, 1.0f, 0.45f);
osc(p, 2, fam(CrusherF, 8), 0.5f, 0.2f);
adsr(p.env[3], 0.0f, 0.0f, 1.0f, 3000.0f);
p.env[3].attackMs = 2500.0f;
route(p, envSrc(4), oscScan(1), 1.0f);
route(p, envSrc(4), allPitch, -0.4f);
amp(p, 0.0f, 3000.0f, 0.0f, 500.0f);
echo(p, DelayType::PingPong, "1/8D", 0.4f, 0.2f);
macro(p, 1, 0.0f, { to(depthOf(1), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
''', style="hits"),
    preset("Data Stream", "FX", "Spectra table on sample-and-hold, in 16ths: a stream of computer chatter.", 72, True,
           "wavetable, fx, data, random", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, SpectraF, 1.0f, 0.4f);
syncLfo(p, 4, LfoWave::SampleHold, "1/16");
syncLfo(p, 5, LfoWave::SampleHold, "1/8");
route(p, lfoUni(4), oscScan(1), 1.0f);
route(p, lfoSrc(5), allPitch, 0.2f);
amp(p, 2.0f, 500.0f, 1.0f, 300.0f);
p.voice.vintage = true;
echo(p, DelayType::PingPong, "1/16", 0.3f, 0.15f);
macro(p, 1, 0.0f, { to(depthOf(1), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
'''),
    preset("Tower Laser", "FX", "Tower Top Loop: a looping tower fired by a fast pitch drop: retro laser shots.", 72, False,
           "fx, laser, zap, retro", r'''
Patch p = blank("Tower Top Loop");
osc(p, 1, Sine, 1.0f, 0.45f);
for (int i = 2; i <= 8; ++i) strike(p, i, Sine, i % 2 ? 2.0f : 1.0f, 0.06f, 200.0f, 0.5f);
adsr(p.env[3], 0.0f, 250.0f, 0.0f, 100.0f);
route(p, envSrc(4), allPitch, 0.5f);
amp(p, 0.0f, 300.0f, 0.0f, 100.0f);
echo(p, DelayType::PingPong, "1/16", 0.35f, 0.2f);
macro(p, 1, 0.0f, { to(dst::env(3, dst::EnvDecay), 0.5f) });
macro(p, 2, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });
''', style="hits"),
    preset("Alarm Loop", "FX", "Loop Atop Pairs: a mutual pair strikes three pairs, gated by a square LFO: a klaxon.", 67, True,
           "fx, alarm, gated, metallic", r'''
Patch p = blank("Loop Atop Pairs");
for (int i = 1; i <= 5; i += 2) osc(p, i, Square, static_cast<float>((i + 1) / 2), 0.15f);
for (int i = 2; i <= 6; i += 2) modOsc(p, i, Sine, 1.5f, 0.04f);
modOsc(p, 7, Sine, 2.0f, 0.04f);
modOsc(p, 8, Sine, 3.0f, 0.04f);
lfo(p, 3, LfoWave::Square, 2.5f);
p.vca.lfoAmt = 0.6f;
amp(p, 5.0f, 500.0f, 1.0f, 300.0f);
room(p, ReverbType::Hall, 2.0f, 0.2f);
macro(p, 1, 0.0f, { to(dst::VcaBase + dst::VcaLfoAmt, -0.5f) });
macro(p, 2, 0.0f, { to(dst::ModulatorLevel, 0.1f) });
'''),

    # ---------------------------------------------------------------------------------------------- Showcase (4)
    preset("Wavetable Tour", "Showcase", "Four families scanned four ways: velocity, an LFO, an envelope and pressure. Macros 1-8 steer them.", 60, True,
           "wavetable, scan, families, everything", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, VowelMorphF, 1.0f, 0.25f);
famOsc(p, 2, PulseWidthF, 1.0f, 0.15f).ratioFine = 0.3f;
famOsc(p, 3, HarmonicBuildF, 2.0f, 0.12f);
famOsc(p, 4, FormantSweepF, 0.5f, 0.2f);
route(p, src::Velocity, oscScan(1), 0.8f);
lfo(p, 4, LfoWave::Triangle, 0.2f);
route(p, lfoUni(4), oscScan(2), 0.8f);
adsr(p.env[3], 1500.0f, 1500.0f, 0.5f, 1000.0f);
route(p, envSrc(4), oscScan(3), 1.0f);
route(p, src::ChannelAT, oscScan(4), 0.8f);
wheel(p, dst::AllOscScan, 0.4f);
amp(p, 200.0f, 1000.0f, 0.9f, 1500.0f);
chorus(p, 0.35f);
echo(p, DelayType::PingPong, "1/4", 0.3f, 0.15f);
room(p, ReverbType::Hall, 3.0f, 0.3f);
macro(p, 1, 0.0f, { to(oscScan(1), 0.5f) });
macro(p, 2, 0.0f, { to(oscScan(2), 0.5f) });
macro(p, 3, 0.0f, { to(oscScan(3), 0.5f) });
macro(p, 4, 0.0f, { to(oscScan(4), 0.5f) });
macro(p, 5, 0.0f, { to(dst::LfoBase + 3 * dst::kLfoParams + dst::LfoRate, 0.4f) });
macro(p, 6, 0.0f, { to(fx(dst::PostFxMix), 0.4f) });
macro(p, 7, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
macro(p, 8, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
''', style="pad"),
    preset("Algorithm Gallery", "Showcase", "Morphs through eight of the new algorithm shapes in turn; Macro 1 moves through them by hand.", 60, True,
           "morph, algorithms, evolving, everything", r'''
Patch p = blank("Ring of Three");
static const char* const slots[8] = { "Ring of Three", "Lattice", "Broom", "Hourglass", "Gamelan", "Tower, Middle Heard",
                                      "Mirrored Fans", "Bell Ladder" };
p.algo.mode = AlgoMode::Morph;
for (int i = 0; i < 8; ++i) p.algo.morphSlots[static_cast<size_t>(i)] = kMorphSlotFirstPreset + findPresetAlgorithm(slots[i]);
for (int i = 1; i <= 8; ++i) osc(p, i, Sine, static_cast<float>((i - 1) % 4 + 1), i == 1 ? 0.3f : 0.06f).ratioFine = 0.05f * static_cast<float>(i);
lfo(p, 4, LfoWave::SawUp, 0.03f, LfoTrig::Free);
route(p, lfoUni(4), dst::AlgoMorph, 1.0f);
amp(p, 300.0f, 1000.0f, 0.9f, 2000.0f);
chorus(p, 0.3f);
room(p, ReverbType::Hall, 3.5f, 0.3f);
macro(p, 1, 0.0f, { to(dst::AlgoMorph, 0.5f) });
macro(p, 2, 0.0f, { to(dst::ModulatorLevel, 0.1f) });
''', style="pad"),
    preset("Sequenced Scales", "Showcase", "Step sequencer locked to A minor pentatonic in Werckmeister III; any key transposes in key.", 57, True,
           "sequencer, scale, tuning, key lock", r'''
Patch p = blank("Kalimba");
osc(p, 1, Sine, 1.0f, 0.4f);
strike(p, 2, Sine, 3.0f, 0.15f, 120.0f, 0.6f);
strike(p, 3, Sine, 7.0f, 0.05f, 60.0f, 0.7f);
osc(p, 4, Sine, 2.0f, 0.12f);
strike(p, 5, Sine, 1.0f, 0.08f, 150.0f, 0.5f);
amp(p, 0.0f, 600.0f, 0.0f, 400.0f);
p.scale.scale = 11;
p.scale.root = 9;
p.scale.keyLock = KeyLock::Nearest;
p.scale.tuning = 4;
keySeq(p, 16, "1/16");
seqPattern(p, 0, { 57, 60, 62, 64, 67, 64, 62, 60, 69, 67, 64, 62, 60, 62, 64, 67 }, 100, 2);
for (int s : { 3, 11 }) seqStep(p, 0, s).ratchet = 2;
echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.2f);
room(p, ReverbType::Hall, 2.5f, 0.25f);
macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });
macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });
''', style="line"),
    preset("Living Tables", "Showcase", "Unison Poly with Voice Mod and per-note randomness on four family scans: every voice its own wave.", 60, True,
           "wavetable, unison poly, voice mod, random", r'''
Patch p = blank("Eight Carriers");
famOsc(p, 1, SawBrightF, 1.0f, 0.25f, 0.4f);
famOsc(p, 2, CombSweepF, 1.0f, 0.2f, 0.3f).ratioFine = 0.25f;
famOsc(p, 3, OrganStopsF, 2.0f, 0.1f, 0.3f);
famOsc(p, 4, SpectraF, 0.5f, 0.15f, 0.3f);
unison(p, 3, 0.2f);
route(p, src::VoiceMod, oscScan(1), 0.4f);
route(p, src::RandomPerNote, oscScan(2), 0.5f);
route(p, src::VoiceMod, oscScan(3), -0.4f);
route(p, src::RandomPerNote, oscScan(4), 0.6f);
lfo(p, 4, LfoWave::Sine, 0.15f);
route(p, lfoSrc(4), dst::AllOscScan, 0.15f);
amp(p, 300.0f, 1000.0f, 0.9f, 1500.0f);
chorus(p, 0.3f);
room(p, ReverbType::Hall, 3.0f, 0.3f);
macro(p, 1, 0.0f, { to(depthOf(0), 0.3f) });
macro(p, 2, 0.0f, { to(depthOf(1), 0.3f) });
macro(p, 3, 0.0f, { to(dst::AllOscScan, 0.4f) });
macro(p, 4, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });
''', style="pad"),
]
