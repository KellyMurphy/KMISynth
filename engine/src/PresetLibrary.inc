// Factory preset library, part 2: six more presets per category. Included by Presets.cpp inside its
// anonymous namespace, so it uses the same helpers (blank, osc, envLevel, adsr, route, macro, room, echo...).
// Every preset is original; names describe the sound, not any other instrument.

// A modulator with its own decaying envelope: the usual FM "bright attack, mellow body".
OscParams& strike(Patch& p, int number, int wave, float ratio, float depth, float decayMs, float vel = 0.6f)
{
    OscParams& o = osc(p, number, wave, ratio, 0.0f);
    envLevel(o, depth, 0.0f, decayMs, 0.0f, decayMs * 0.5f, vel);
    return o;
}

void vibrato(Patch& p, float rateHz, float depth, float delayMs = 400.0f)
{
    p.lfo[3].rateHz = rateHz;
    p.lfo[3].delayMs = delayMs;
    p.lfo[3].fadeMs = delayMs * 1.5f;
    route(p, src::Lfo1 + 3, allFine, depth);
}

void mono(Patch& p, float glide)
{
    p.voice.polyMode = PolyMode::Mono;
    p.voice.glideMode = GlideMode::Glide;
    p.voice.glideTime = glide;
    p.voice.glideLegato = true;
}

void arp(Patch& p, ArpMode mode, int octaves, const char* div, float gate)
{
    p.arp.enabled = true;
    p.arp.mode = mode;
    p.arp.octaves = octaves;
    p.arp.syncDiv = syncDiv(div);
    p.arp.gate = gate;
}

void chorus(Patch& p, float mix, bool pre = false)
{
    InsertFxParams& slot = pre ? p.fx.pre : p.fx.post;
    slot.type = FxType::Chorus;
    slot.chorus.mix = mix;
}

void scan(OscParams& o, float position, int range)
{
    o.scan = position;
    o.scanRange = range;
}

// ================================================================================================
// Keys
// ================================================================================================

// A rounder tine piano: low modulation index, long body, gentle chorus.
Patch mellowTines()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.5f);
    strike(p, 2, Sine, 1.0f, 0.25f, 1200.0f, 0.6f).initLevel = 0.04f;
    osc(p, 3, Sine, 2.0f, 0.12f).ratioFine = 0.1f;
    strike(p, 4, Sine, 7.0f, 0.18f, 160.0f, 0.8f);
    adsr(p.env[2], 1.0f, 4200.0f, 0.0f, 380.0f);
    p.env[2].decayCurve = 0.6f;
    p.vca.velEnv = 0.6f;
    chorus(p, 0.35f);
    room(p, ReverbType::Room, 1.4f, 0.2f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.35f) });                                        // Bark
    macro(p, 2, 0.0f, { to(fx(dst::PostFxMix), 0.4f) });                                     // Chorus
    p.vca.patchLevel = 1.20f; // level-matched across the bank
    return p;
}

// A soft, felted piano: a triangle body with a short noise-free hammer from a high modulator.
Patch feltHammer()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Triangle, 1.0f, 0.45f);
    strike(p, 2, Sine, 3.0f, 0.2f, 90.0f, 0.7f);
    osc(p, 3, Sine, 2.0f, 0.15f).ratioFine = -0.08f;
    strike(p, 4, Sine, 1.0f, 0.2f, 1500.0f, 0.5f);
    p.afilt.cutoff = cutoff(2600.0f);
    p.afilt.keytrack = 0.8f;
    adsr(p.env[2], 2.0f, 3000.0f, 0.0f, 300.0f);
    p.env[2].decayCurve = 0.7f;
    p.vca.velEnv = 0.75f;
    for (int i : { 0, 2 }) p.osc[static_cast<size_t>(i)].keyscaleHiDepth = 0.3f;
    room(p, ReverbType::Room, 1.1f, 0.18f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Felt
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Sustain
    p.vca.patchLevel = 1.30f; // level-matched across the bank
    return p;
}

// A percussive clavinet-style key: PD Square bite through a resonant filter, quick decay.
Patch hammerClav()
{
    Patch p = blank("One Pair");
    osc(p, 1, Pulse25, 1.0f, 0.5f);
    OscParams& m = strike(p, 2, Sine, 1.0f, 0.5f, 260.0f, 0.8f);
    m.mode = OscMode::PDSquare;
    p.afilt.cutoff = cutoff(1200.0f);
    p.afilt.resonance = 0.35f;
    p.afilt.envAmt = 0.45f;
    p.afilt.velEnv = 0.4f;
    adsr(p.env[1], 0.0f, 220.0f, 0.15f, 120.0f);
    adsr(p.env[2], 0.0f, 900.0f, 0.3f, 90.0f);
    p.vca.velEnv = 0.6f;
    p.fx.pre.type = FxType::Phaser;
    p.fx.pre.phaser.mix = 0.3f;
    room(p, ReverbType::Room, 0.8f, 0.12f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Bite
    macro(p, 2, 0.0f, { to(fx(dst::PreFxMix), 0.5f) });                                      // Phase
    p.vca.oscsLevel = 1.00f;
    p.vca.patchLevel = 2.00f; // level-matched across the bank
    return p;
}

// A small music box: inharmonic high partials, short tines and a long echo.
Patch musicBox()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 2.0f, 0.45f);
    strike(p, 2, Sine, 5.19f, 0.2f, 500.0f, 0.5f);
    osc(p, 3, Sine, 4.0f, 0.12f);
    strike(p, 4, Sine, 9.3f, 0.15f, 200.0f, 0.5f);
    adsr(p.env[2], 0.0f, 1800.0f, 0.0f, 1400.0f);
    p.env[2].decayCurve = 0.7f;
    p.vca.velEnv = 0.5f;
    echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.2f);
    room(p, ReverbType::Hall, 2.5f, 0.25f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.25f), to(oscEnvLevel(4), 0.2f) });              // Metal
    macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });                                      // Echo
    p.vca.patchLevel = 1.40f; // level-matched across the bank
    return p;
}

// A reedy electric piano: Reed wave body, PW-modulated bark, auto-pan tremolo.
Patch reedPiano()
{
    Patch p = blank("One Pair");
    osc(p, 1, Reed, 1.0f, 0.45f);
    OscParams& m = strike(p, 2, Sine, 1.0f, 0.35f, 700.0f, 0.8f);
    m.mode = OscMode::PWMod;
    osc(p, 3, Sine, 1.0f, 0.2f);
    adsr(p.env[2], 1.0f, 3500.0f, 0.1f, 300.0f);
    p.env[2].decayCurve = 0.6f;
    p.vca.velEnv = 0.65f;
    p.fx.pre.type = FxType::Tremolo;
    p.fx.pre.tremolo = { 5.2f, 0.35f, false, 180.0f, 0.0f, 1.0f };
    room(p, ReverbType::Plate, 1.2f, 0.15f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Bark
    macro(p, 2, 0.0f, { to(fx(dst::PreFxParam2), 0.5f) });                                   // Tremolo
    p.vca.patchLevel = 1.12f; // level-matched across the bank
    return p;
}

// A bright digital grand: three FM pairs at 1, 2 and 3 with staggered decays, a touch of detune.
Patch digitalGrand()
{
    Patch p = blank("Four Pairs");
    osc(p, 1, Sine, 1.0f, 0.4f);
    strike(p, 2, Sine, 1.0f, 0.3f, 1800.0f, 0.7f);
    osc(p, 3, Sine, 2.0f, 0.2f).ratioFine = 0.06f;
    strike(p, 4, Sine, 3.0f, 0.2f, 600.0f, 0.8f);
    osc(p, 5, Sine, 3.0f, 0.1f).ratioFine = -0.05f;
    strike(p, 6, Sine, 9.0f, 0.12f, 150.0f, 0.9f);
    for (int i : { 0, 2, 4 })
    {
        p.osc[static_cast<size_t>(i)].keyscaleBreak = 72;
        p.osc[static_cast<size_t>(i)].keyscaleHiDepth = 0.35f;
    }
    adsr(p.env[2], 0.0f, 6000.0f, 0.0f, 600.0f);
    p.env[2].decayCurve = 0.75f;
    p.vca.velEnv = 0.7f;
    room(p, ReverbType::Hall, 2.2f, 0.22f);
    macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });                                 // Brightness
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                     // Space
    p.vca.patchLevel = 0.84f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Bells
// ================================================================================================

// A struck singing bowl: two close inharmonic partials beat slowly as it rings.
Patch templeBowl()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.45f);
    strike(p, 2, Sine, 2.76f, 0.2f, 2500.0f, 0.5f);
    osc(p, 3, Sine, 1.0f, 0.35f).ratioFine = 0.35f;
    strike(p, 4, Sine, 5.4f, 0.1f, 600.0f, 0.6f);
    adsr(p.env[2], 2.0f, 9000.0f, 0.0f, 6000.0f);
    p.env[2].decayCurve = 0.6f;
    room(p, ReverbType::Hall, 5.0f, 0.3f, 40.0f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.25f) });                                        // Strike
    macro(p, 2, 0.0f, { to(dst::osc(2, dst::OscFine), 0.3f) });                              // Beating
    p.vca.patchLevel = 0.49f; // level-matched across the bank
    return p;
}

// Tubular bells in a hall: a strong 3.5 partial and a long, slow decay.
Patch tubularHall()
{
    Patch p = blank("Bell Tree");
    osc(p, 1, Sine, 1.0f, 0.4f);
    osc(p, 2, Sine, 2.0f, 0.2f);
    strike(p, 3, Sine, 3.5f, 0.3f, 3000.0f, 0.6f);
    osc(p, 4, Sine, 0.5f, 0.2f);
    osc(p, 5, Sine, 4.24f, 0.1f);
    strike(p, 6, Sine, 7.13f, 0.15f, 900.0f, 0.6f);
    adsr(p.env[2], 0.0f, 8000.0f, 0.0f, 5000.0f);
    p.env[2].decayCurve = 0.65f;
    room(p, ReverbType::Hall, 4.0f, 0.3f, 30.0f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(3), 0.25f) });                                        // Clang
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Ring
    p.vca.patchLevel = 0.63f; // level-matched across the bank
    return p;
}

// Small, glassy chimes with shimmering ping-pong echoes.
Patch iceChimes()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 3.0f, 0.4f);
    strike(p, 2, Sine, 7.0f, 0.25f, 700.0f, 0.7f);
    osc(p, 3, Sine, 4.24f, 0.2f);
    strike(p, 4, Sine, 11.0f, 0.15f, 300.0f, 0.7f);
    adsr(p.env[2], 0.0f, 2500.0f, 0.0f, 2000.0f);
    p.env[2].decayCurve = 0.7f;
    p.vca.velEnv = 0.6f;
    echo(p, DelayType::PingPong, "1/16", 0.45f, 0.25f);
    room(p, ReverbType::Cloud, 5.0f, 0.35f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(4), 0.25f) });                                        // Sparkle
    macro(p, 2, 0.0f, { to(fx(dst::DelayFeedback), 0.3f) });                                 // Trail
    p.vca.patchLevel = 1.62f; // level-matched across the bank
    return p;
}

// A toy glockenspiel: bright, short and a little out of tune.
Patch toyGlock()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 2.0f, 0.5f);
    strike(p, 2, Sine, 6.0f, 0.3f, 250.0f, 0.8f);
    osc(p, 3, Sine, 5.4f, 0.12f);
    adsr(p.env[2], 0.0f, 1200.0f, 0.0f, 800.0f);
    p.env[2].decayCurve = 0.7f;
    p.vca.velEnv = 0.6f;
    room(p, ReverbType::Room, 1.2f, 0.2f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Strike
    macro(p, 2, 0.0f, { to(oscLevel(3), 0.2f) });                                            // Tin
    p.vca.patchLevel = 0.95f; // level-matched across the bank
    return p;
}

// A deep gong: low inharmonic ratios, a slow swell of upper partials after the hit.
Patch gamelanGong()
{
    Patch p = blank("Many Into One");
    osc(p, 1, Sine, 0.5f, 0.5f);
    strike(p, 2, Sine, 1.41f, 0.3f, 4000.0f, 0.5f);
    OscParams& swell = osc(p, 3, Sine, 2.13f, 0.0f);
    envLevel(swell, 0.25f, 900.0f, 3000.0f, 0.0f, 2000.0f);
    strike(p, 4, Sine, 3.7f, 0.15f, 300.0f, 0.8f);
    adsr(p.env[2], 0.0f, 9000.0f, 0.0f, 6000.0f);
    p.env[2].decayCurve = 0.6f;
    room(p, ReverbType::Hall, 6.0f, 0.3f, 50.0f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(3), 0.25f) });                                        // Bloom
    macro(p, 2, 0.0f, { to(oscEnvLevel(4), 0.2f) });                                         // Hit
    p.vca.patchLevel = 0.47f; // level-matched across the bank
    return p;
}

// Wave Scan bells: the strike scans from Bell Partials through the next additive tables as it decays.
Patch crystalBloom()
{
    Patch p = blank("Eight Carriers");
    OscParams& b = osc(p, 1, BellPartials, 1.0f, 0.45f);
    scan(b, 0.0f, 4);
    route(p, src::OscEnv1, dst::OscScanBase + 0, 1.0f); // Osc 1's own envelope sweeps the scan
    adsr(b.env, 0.0f, 2500.0f, 0.0f, 1500.0f);
    osc(p, 2, Sine, 4.0f, 0.12f);
    adsr(p.env[2], 0.0f, 4000.0f, 0.0f, 3000.0f);
    p.env[2].decayCurve = 0.6f;
    p.vca.velEnv = 0.5f;
    echo(p, DelayType::Stereo, "1/4D", 0.3f, 0.2f);
    room(p, ReverbType::Hall, 4.0f, 0.3f);
    macro(p, 1, 0.0f, { to(dst::OscScanBase + 0, 0.4f) });                                   // Scan
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                     // Space
    p.vca.patchLevel = 1.26f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Organ
// ================================================================================================

// Jazz drawbars: 16', 8', 4' and a little 2 2/3', fast rotary on the wheel.
Patch jazzDrawbars()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Sine, 0.5f, 0.35f);
    osc(p, 2, Sine, 1.0f, 0.4f);
    osc(p, 3, Sine, 2.0f, 0.25f);
    osc(p, 4, Sine, 3.0f, 0.12f);
    adsr(p.env[2], 2.0f, 100.0f, 1.0f, 60.0f);
    p.vca.snap = true;
    p.fx.pre.type = FxType::Distort;
    p.fx.pre.distort.drive = 0.15f;
    p.fx.post.type = FxType::Rotary;
    route(p, src::ModWheel, fx(dst::PostFxParam1), 0.6f);
    room(p, ReverbType::Room, 1.4f, 0.12f);
    macro(p, 1, 0.0f, { to(oscLevel(4), 0.25f) });                                           // Upper bars
    macro(p, 2, 0.0f, { to(fx(dst::PreFxParam1), 0.5f) });                                   // Drive
    p.vca.patchLevel = 0.25f; // level-matched across the bank
    return p;
}

// A pipe organ in a big room: Organ Full plus a quiet octave below, slow attack.
Patch cathedralPipes()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, OrganFull, 1.0f, 0.4f);
    osc(p, 2, Sine, 0.5f, 0.3f);
    osc(p, 3, Organ84, 2.0f, 0.15f);
    adsr(p.env[2], 90.0f, 200.0f, 1.0f, 900.0f);
    room(p, ReverbType::Hall, 6.0f, 0.38f, 40.0f);
    macro(p, 1, 0.0f, { to(oscLevel(3), 0.25f) });                                           // Mixture
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                     // Nave
    p.vca.patchLevel = 0.51f; // level-matched across the bank
    return p;
}

// A harmonium-like reed organ, gently wavering.
Patch reedOrgan()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Reed, 1.0f, 0.4f);
    osc(p, 2, Reed, 2.0f, 0.15f).ratioFine = 0.15f;
    p.afilt.cutoff = cutoff(3500.0f);
    adsr(p.env[2], 40.0f, 200.0f, 1.0f, 250.0f);
    p.lfo[2].rateHz = 5.5f;
    p.vca.lfoAmt = 0.06f;
    room(p, ReverbType::Room, 1.2f, 0.15f);
    macro(p, 1, 0.0f, { to(afCut, 0.3f) });                                                  // Open
    macro(p, 2, 0.0f, { to(dst::VcaBase + dst::VcaLfoAmt, 0.2f) });                          // Waver
    p.vca.patchLevel = 0.93f; // level-matched across the bank
    return p;
}

// A buzzy combo organ: square and pulse, vibrato and a spring-ish plate.
Patch comboOrgan()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Square, 1.0f, 0.3f);
    osc(p, 2, Pulse25, 2.0f, 0.15f);
    p.afilt.cutoff = cutoff(2800.0f);
    adsr(p.env[2], 2.0f, 100.0f, 1.0f, 60.0f);
    p.vca.snap = true;
    vibrato(p, 6.5f, 0.05f, 0.0f);
    room(p, ReverbType::Plate, 1.8f, 0.2f);
    macro(p, 1, 0.0f, { to(afCut, 0.3f) });                                                  // Tone
    macro(p, 2, 0.0f, { to(allFine, 0.1f) });                                                // Vibrato
    p.vca.patchLevel = 0.47f; // level-matched across the bank
    return p;
}

// Percussive organ: a decaying 2nd-harmonic click on every note over steady drawbars.
Patch percOrgan()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Sine, 1.0f, 0.4f);
    osc(p, 2, Sine, 0.5f, 0.25f);
    envLevel(osc(p, 3, Sine, 3.0f, 0.0f), 0.3f, 0.0f, 350.0f, 0.0f, 100.0f);
    adsr(p.env[2], 2.0f, 100.0f, 1.0f, 60.0f);
    p.vca.snap = true;
    p.fx.post.type = FxType::Rotary;
    p.fx.post.rotary.wooferHz = 0.7f;
    p.fx.post.rotary.hornHz = 0.9f;
    route(p, src::ModWheel, fx(dst::PostFxParam1), 0.6f);
    room(p, ReverbType::Room, 1.2f, 0.12f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(3), 0.3f) });                                         // Percussion
    macro(p, 2, 0.0f, { to(fx(dst::PostFxParam1), 0.6f) });                                  // Rotor
    p.vca.patchLevel = 0.36f; // level-matched across the bank
    return p;
}

// A swelling gospel organ: Organ 8+4 with expression on the mod wheel and a drive stage.
Patch gospelSwell()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Organ84, 1.0f, 0.4f);
    osc(p, 2, OrganFull, 0.5f, 0.2f);
    adsr(p.env[2], 25.0f, 200.0f, 1.0f, 120.0f);
    p.vca.vcaLevel = 0.7f;
    route(p, src::ModWheel, dst::VcaBase + dst::VcaLevel, 0.3f);
    p.fx.pre.type = FxType::Distort;
    p.fx.pre.distort.drive = 0.3f;
    p.fx.post.type = FxType::Rotary;
    room(p, ReverbType::Hall, 2.0f, 0.18f);
    macro(p, 1, 0.0f, { to(fx(dst::PostFxParam1), 0.6f) });                                  // Rotor
    macro(p, 2, 0.0f, { to(fx(dst::PreFxParam1), 0.5f) });                                   // Growl
    p.vca.patchLevel = 0.30f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Bass
// ================================================================================================

// A sine sub with a short FM thump on the attack.
Patch subThump()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 1.0f, 0.65f);
    strike(p, 2, Sine, 1.0f, 0.45f, 120.0f, 0.7f);
    route(p, src::Env1 + 3, allPitch, 0.12f); // Env 4: pitch drop
    adsr(p.env[3], 0.0f, 60.0f, 0.0f, 40.0f);
    adsr(p.env[2], 0.0f, 900.0f, 0.7f, 150.0f);
    mono(p, 0.05f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Thump
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvSustain), 0.3f) });                           // Body
    p.vca.patchLevel = 0.91f; // level-matched across the bank
    return p;
}

// A squelchy mono line: saw through a resonant ladder, short filter envelope, glide on legato.
Patch acidLine()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Saw, 1.0f, 0.55f);
    p.afilt.cutoff = cutoff(350.0f);
    p.afilt.resonance = 0.7f;
    p.afilt.envAmt = 0.55f;
    p.afilt.velEnv = 0.5f;
    p.afilt.preDrive = 0.45f;
    adsr(p.env[1], 0.0f, 260.0f, 0.0f, 120.0f);
    adsr(p.env[2], 0.0f, 400.0f, 0.8f, 80.0f);
    mono(p, 0.08f);
    p.fx.pre.type = FxType::Distort;
    p.fx.pre.distort.drive = 0.3f;
    echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.15f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Cutoff
    macro(p, 2, 0.0f, { to(afRes, 0.3f) });                                                  // Squelch
    p.vca.patchLevel = 0.87f; // level-matched across the bank
    return p;
}

// A plucked FM bass: fast-decaying 1:1 modulator for the pluck, sine sub underneath.
Patch fmPluckBass()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 1.0f, 0.55f);
    strike(p, 2, Sine, 1.0f, 0.6f, 200.0f, 0.8f).feedback = 0.2f;
    osc(p, 3, Sine, 0.5f, 0.3f);
    adsr(p.env[2], 0.0f, 1200.0f, 0.35f, 120.0f);
    p.vca.velEnv = 0.5f;
    mono(p, 0.03f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Pluck
    macro(p, 2, 0.0f, { to(oscLevel(3), 0.3f) });                                            // Sub
    p.vca.patchLevel = 1.04f; // level-matched across the bank
    return p;
}

// Two detuned saws beating against each other in unison, low-passed.
Patch detunedReese()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Saw, 1.0f, 0.4f);
    osc(p, 2, Saw, 1.0f, 0.4f).ratioFine = 0.6f;
    osc(p, 3, Sine, 0.5f, 0.3f);
    p.voice.polyMode = PolyMode::Unison;
    p.voice.density = 2;
    p.voice.unisonDetune = 0.2f;
    p.voice.glideMode = GlideMode::Glide;
    p.voice.glideTime = 0.06f;
    p.voice.glideLegato = true;
    p.afilt.cutoff = cutoff(700.0f);
    p.afilt.resonance = 0.2f;
    p.afilt.preDrive = 0.4f;
    adsr(p.env[2], 2.0f, 300.0f, 1.0f, 150.0f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Open
    macro(p, 2, 0.0f, { to(dst::osc(1, dst::OscFine), 0.4f) });                              // Beat
    p.vca.patchLevel = 0.79f; // level-matched across the bank
    return p;
}

// A woody upright-style bass: soft triangle, quick FM thunk, short release.
Patch woodBass()
{
    Patch p = blank("One Pair");
    osc(p, 1, Triangle, 1.0f, 0.55f);
    strike(p, 2, Sine, 2.0f, 0.3f, 90.0f, 0.8f);
    p.afilt.cutoff = cutoff(1500.0f);
    p.afilt.keytrack = 0.5f;
    adsr(p.env[2], 2.0f, 1400.0f, 0.2f, 120.0f);
    p.env[2].decayCurve = 0.6f;
    p.vca.velEnv = 0.6f;
    mono(p, 0.02f);
    room(p, ReverbType::Room, 0.8f, 0.1f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Thunk
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Tone
    p.vca.patchLevel = 1.57f; // level-matched across the bank
    return p;
}

// A growling bass: an LFO scans the carrier through the Analog tables, tempo-synced.
Patch growlScan()
{
    Patch p = blank("Eight Carriers");
    OscParams& o = osc(p, 1, SoftSaw, 1.0f, 0.5f);
    scan(o, 0.0f, 5);
    osc(p, 2, Sine, 0.5f, 0.3f);
    p.lfo[4].bpmSync = true;
    p.lfo[4].syncDiv = syncDiv("1/8");
    p.lfo[4].wave = LfoWave::Triangle;
    route(p, src::Lfo1Uni + 4, dst::OscScanBase + 0, 1.0f);
    p.afilt.cutoff = cutoff(1200.0f);
    p.afilt.resonance = 0.3f;
    p.afilt.preDrive = 0.5f;
    adsr(p.env[2], 1.0f, 300.0f, 1.0f, 120.0f);
    mono(p, 0.05f);
    macro(p, 1, 0.0f, { to(dst::lfo(4, dst::LfoLevel), -0.5f) });                            // Calm
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Bright
    p.vca.patchLevel = 1.64f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Lead
// ================================================================================================

// A classic square lead with delayed vibrato and a bright filter envelope.
Patch squareHero()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Square, 1.0f, 0.45f);
    osc(p, 2, Square, 1.0f, 0.2f).ratioFine = 0.3f;
    p.afilt.cutoff = cutoff(1800.0f);
    p.afilt.envAmt = 0.3f;
    p.afilt.resonance = 0.2f;
    adsr(p.env[1], 5.0f, 600.0f, 0.4f, 200.0f);
    adsr(p.env[2], 3.0f, 300.0f, 0.9f, 200.0f);
    mono(p, 0.08f);
    vibrato(p, 5.5f, 0.1f);
    echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.2f);
    room(p, ReverbType::Plate, 1.5f, 0.15f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Bright
    macro(p, 2, 0.0f, { to(dst::osc(1, dst::OscFine), 0.3f) });                              // Detune
    p.vca.patchLevel = 1.32f; // level-matched across the bank
    return p;
}

// A pure whistle: sine with breath noise, slow vibrato.
Patch whistle()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 2.0f, 0.55f);
    OscParams& breath = osc(p, 3, Sine, 4.0f, 0.04f);
    breath.feedback = 0.9f;
    adsr(p.env[2], 60.0f, 300.0f, 0.95f, 250.0f);
    mono(p, 0.1f);
    vibrato(p, 5.0f, 0.12f, 300.0f);
    echo(p, DelayType::Stereo, "1/4", 0.3f, 0.2f);
    room(p, ReverbType::Hall, 2.5f, 0.25f);
    macro(p, 1, 0.0f, { to(oscLevel(3), 0.06f) });                                           // Breath
    macro(p, 2, 0.0f, { to(allFine, 0.15f) });                                               // Vibrato
    p.vca.patchLevel = 1.10f; // level-matched across the bank
    return p;
}

// A screaming feedback lead: self-modulating oscillator into drive.
Patch feedbackScream()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.45f);
    OscParams& m = osc(p, 2, Sine, 1.0f, 0.2f);
    m.feedback = 0.45f;
    envLevel(m, 0.25f, 300.0f, 800.0f, 0.6f, 300.0f);
    p.afilt.cutoff = cutoff(3000.0f);
    p.afilt.preDrive = 0.6f;
    adsr(p.env[2], 5.0f, 300.0f, 0.9f, 250.0f);
    mono(p, 0.12f);
    vibrato(p, 6.0f, 0.1f);
    p.fx.pre.type = FxType::Distort;
    p.fx.pre.distort.drive = 0.4f;
    echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.2f);
    room(p, ReverbType::Plate, 1.8f, 0.15f);
    macro(p, 1, 0.0f, { to(dst::osc(1, dst::OscFeedback), 0.4f) });                          // Scream
    macro(p, 2, 0.0f, { to(fx(dst::PreFxParam1), 0.4f) });                                   // Drive
    p.vca.patchLevel = 0.29f; // level-matched across the bank
    return p;
}

// A long-glide saw lead: every note slides, with a soft filter swell.
Patch glideSaw()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, SawStack, 1.0f, 0.35f);
    osc(p, 2, Saw, 1.0f, 0.2f).ratioFine = -0.4f;
    p.voice.polyMode = PolyMode::Mono;
    p.voice.glideMode = GlideMode::Glide;
    p.voice.glideTime = 0.3f;
    p.afilt.cutoff = cutoff(2200.0f);
    p.afilt.envAmt = 0.2f;
    adsr(p.env[1], 150.0f, 800.0f, 0.6f, 300.0f);
    adsr(p.env[2], 10.0f, 300.0f, 1.0f, 300.0f);
    echo(p, DelayType::Stereo, "1/4D", 0.35f, 0.2f);
    room(p, ReverbType::Hall, 2.2f, 0.18f);
    macro(p, 1, 0.0f, { to(dst::VoiceBase + dst::VoiceGlideTime, 0.4f) });                   // Glide
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Bright
    p.vca.patchLevel = 1.78f; // level-matched across the bank
    return p;
}

// A talking lead: the vowel filter follows the mod wheel and a slow LFO.
Patch vowelLead()
{
    Patch p = blank("One Pair");
    osc(p, 1, SawStack, 1.0f, 0.45f);
    osc(p, 2, Sine, 1.0f, 0.15f);
    p.dfilt.type = DigitalFilterType::Vowel;
    p.dfilt.cutoff = 0.3f;
    p.dfilt.resonance = 0.6f;
    p.dfilt.lfoAmt = 0.25f;
    p.lfo[0].rateHz = 0.7f;
    route(p, src::ModWheel, dfCut, 0.5f);
    adsr(p.env[2], 10.0f, 300.0f, 0.9f, 250.0f);
    mono(p, 0.1f);
    vibrato(p, 5.2f, 0.08f);
    echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.18f);
    room(p, ReverbType::Plate, 1.5f, 0.15f);
    macro(p, 1, 0.0f, { to(dfCut, 0.5f) });                                                  // Vowel
    macro(p, 2, 0.0f, { to(dst::DFiltBase + dst::DfLfoAmt, 0.3f) });                         // Talk
    p.vca.patchLevel = 1.68f; // level-matched across the bank
    return p;
}

// A ribbon of pulse: PW Mod movement from a slow LFO, soft attack, stereo pair.
Patch pulseRibbon()
{
    Patch p = blank("One Pair");
    osc(p, 1, Square, 1.0f, 0.45f);
    OscParams& m = osc(p, 2, Sine, 1.0f, 0.3f);
    m.mode = OscMode::PWMod;
    p.lfo[4].rateHz = 0.4f;
    p.lfo[4].trig = LfoTrig::Free;
    route(p, src::Lfo1 + 4, oscLevel(2), 0.25f);
    p.voice.oscStereo = true;
    p.voice.carrierDetune = 10.0f;
    p.afilt.cutoff = cutoff(2500.0f);
    adsr(p.env[2], 40.0f, 400.0f, 0.9f, 300.0f);
    mono(p, 0.1f);
    echo(p, DelayType::PingPong, "1/4", 0.3f, 0.18f);
    room(p, ReverbType::Hall, 2.0f, 0.18f);
    macro(p, 1, 0.0f, { to(dst::lfo(4, dst::LfoRate), 0.4f) });                              // Speed
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Bright
    p.vca.patchLevel = 0.67f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Brass
// ================================================================================================

// A horn section: three detuned PD Saw voices with a blaring attack.
Patch hornSection()
{
    Patch p = blank("Four Pairs");
    for (int k = 0; k < 3; ++k)
    {
        osc(p, 1 + 2 * k, Sine, 1.0f, 0.3f).ratioFine = 0.15f * static_cast<float>(k - 1);
        OscParams& m = osc(p, 2 + 2 * k, Sine, 1.0f, 0.12f);
        m.mode = OscMode::PDSaw;
        envLevel(m, 0.5f, 60.0f + 20.0f * k, 500.0f, 0.55f, 250.0f, 0.5f);
    }
    p.afilt.cutoff = cutoff(3000.0f);
    p.afilt.envAmt = 0.25f;
    adsr(p.env[1], 60.0f, 500.0f, 0.5f, 250.0f);
    adsr(p.env[2], 30.0f, 300.0f, 0.85f, 200.0f);
    vibrato(p, 5.0f, 0.06f, 500.0f);
    room(p, ReverbType::Hall, 2.0f, 0.2f);
    macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });                                 // Blare
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvAttack), 0.4f) });                            // Swell
    p.vca.patchLevel = 0.41f; // level-matched across the bank
    return p;
}

// Soft horns: rounded FM, slow attack, a warm pad-like section.
Patch softHorns()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.4f);
    envLevel(osc(p, 2, Sine, 1.0f, 0.08f), 0.25f, 200.0f, 800.0f, 0.5f, 400.0f, 0.4f);
    osc(p, 3, Sine, 1.0f, 0.3f).ratioFine = 0.2f;
    envLevel(osc(p, 4, Sine, 1.0f, 0.06f), 0.2f, 250.0f, 900.0f, 0.5f, 400.0f, 0.4f);
    p.afilt.cutoff = cutoff(2000.0f);
    adsr(p.env[2], 180.0f, 600.0f, 0.9f, 500.0f);
    vibrato(p, 4.8f, 0.05f, 700.0f);
    room(p, ReverbType::Hall, 2.5f, 0.22f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.2f), to(oscEnvLevel(4), 0.2f) });               // Brass
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Open
    p.vca.patchLevel = 0.51f; // level-matched across the bank
    return p;
}

// A short brass stab: fast blare, quick decay, punchy.
Patch brassStab()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Saw, 1.0f, 0.35f);
    OscParams& m = osc(p, 2, Sine, 1.0f, 0.1f);
    m.mode = OscMode::PDSaw;
    envLevel(m, 0.55f, 20.0f, 250.0f, 0.1f, 150.0f, 0.6f);
    osc(p, 3, Saw, 1.0f, 0.25f).ratioFine = 0.3f;
    p.afilt.cutoff = cutoff(2200.0f);
    p.afilt.envAmt = 0.4f;
    adsr(p.env[1], 15.0f, 250.0f, 0.2f, 150.0f);
    adsr(p.env[2], 10.0f, 350.0f, 0.3f, 150.0f);
    p.vca.velEnv = 0.6f;
    room(p, ReverbType::Plate, 1.4f, 0.18f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Blare
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Length
    p.vca.patchLevel = 1.70f; // level-matched across the bank
    return p;
}

// A muted horn: band-passed, nasal and close.
Patch mutedHorn()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.45f);
    envLevel(osc(p, 2, Sine, 2.0f, 0.05f), 0.2f, 40.0f, 400.0f, 0.4f, 200.0f, 0.5f);
    p.dfilt.type = DigitalFilterType::BpModular;
    p.dfilt.cutoff = cutoff(1300.0f);
    p.dfilt.resonance = 0.4f;
    p.dfilt.envAmt = 0.2f;
    adsr(p.env[0], 30.0f, 400.0f, 0.5f, 200.0f);
    adsr(p.env[2], 30.0f, 300.0f, 0.85f, 200.0f);
    mono(p, 0.06f);
    vibrato(p, 5.0f, 0.06f, 400.0f);
    room(p, ReverbType::Room, 1.2f, 0.15f);
    macro(p, 1, 0.0f, { to(dfCut, 0.3f) });                                                  // Mute
    macro(p, 2, 0.0f, { to(allFine, 0.1f) });                                                // Vibrato
    p.vca.oscsLevel = 1.00f;
    p.vca.patchLevel = 2.00f; // level-matched across the bank
    return p;
}

// Synth brass that swells in and opens up the longer you hold.
Patch swellBrass()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Saw, 1.0f, 0.35f);
    osc(p, 2, Saw, 1.0f, 0.35f).ratioFine = 0.4f;
    osc(p, 3, Pulse40, 0.5f, 0.15f);
    p.afilt.cutoff = cutoff(600.0f);
    p.afilt.envAmt = 0.5f;
    p.afilt.resonance = 0.2f;
    adsr(p.env[1], 700.0f, 1500.0f, 0.6f, 500.0f);
    adsr(p.env[2], 300.0f, 800.0f, 1.0f, 500.0f);
    chorus(p, 0.3f);
    room(p, ReverbType::Hall, 2.5f, 0.22f);
    macro(p, 1, 0.0f, { to(dst::env(1, dst::EnvAttack), 0.5f) });                            // Swell
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Bright
    p.vca.patchLevel = 1.26f; // level-matched across the bank
    return p;
}

// Low brass: a tuba-like FM stack with a breathy attack, mono.
Patch lowBrass()
{
    Patch p = blank("Stack Three, Five Free");
    osc(p, 1, Sine, 1.0f, 0.55f);
    envLevel(osc(p, 2, Sine, 1.0f, 0.1f), 0.3f, 80.0f, 600.0f, 0.4f, 300.0f, 0.5f);
    envLevel(osc(p, 3, Sine, 2.0f, 0.0f), 0.2f, 50.0f, 200.0f, 0.0f, 100.0f, 0.6f);
    adsr(p.env[2], 50.0f, 400.0f, 0.9f, 200.0f);
    mono(p, 0.06f);
    vibrato(p, 4.5f, 0.05f, 500.0f);
    room(p, ReverbType::Hall, 1.8f, 0.15f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.25f) });                                        // Blat
    macro(p, 2, 0.0f, { to(oscEnvLevel(3), 0.25f) });                                        // Breath
    p.vca.patchLevel = 1.02f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Pad
// ================================================================================================

// A warm analog pad: detuned saws, slow filter swell, chorus.
Patch warmBlanket()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, SoftSaw, 1.0f, 0.3f);
    osc(p, 2, SoftSaw, 1.0f, 0.3f).ratioFine = 0.35f;
    osc(p, 3, Triangle, 0.5f, 0.2f);
    p.afilt.cutoff = cutoff(900.0f);
    p.afilt.envAmt = 0.3f;
    adsr(p.env[1], 1200.0f, 2000.0f, 0.5f, 1500.0f);
    adsr(p.env[2], 600.0f, 1000.0f, 1.0f, 1500.0f);
    chorus(p, 0.4f);
    room(p, ReverbType::Hall, 3.5f, 0.28f);
    macro(p, 1, 0.0f, { to(afCut, 0.3f) });                                                  // Warmth
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvAttack), 0.5f) });                            // Swell
    p.vca.patchLevel = 0.58f; // level-matched across the bank
    return p;
}

// A glassy pad whose carrier slowly scans the Additive tables on a free LFO.
Patch glassHorizon()
{
    Patch p = blank("Eight Carriers");
    OscParams& a = osc(p, 1, OctavePair, 1.0f, 0.35f);
    scan(a, 0.0f, 6);
    p.lfo[4].rateHz = 0.06f;
    p.lfo[4].trig = LfoTrig::Free;
    route(p, src::Lfo1Uni + 4, dst::OscScanBase + 0, 1.0f);
    osc(p, 2, Sine, 2.0f, 0.12f).ratioFine = 0.2f;
    adsr(p.env[2], 900.0f, 1500.0f, 1.0f, 2500.0f);
    chorus(p, 0.3f);
    echo(p, DelayType::Stereo, "1/4D", 0.35f, 0.18f);
    room(p, ReverbType::Cloud, 6.0f, 0.35f, 40.0f);
    macro(p, 1, 0.0f, { to(dst::lfo(4, dst::LfoRate), 0.4f) });                              // Drift speed
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                     // Space
    p.vca.patchLevel = 1.05f; // level-matched across the bank
    return p;
}

// An "ah" choir pad with a soft vowel shimmer and ensemble chorus.
Patch ahChoir()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, ChoirAh, 1.0f, 0.35f);
    osc(p, 2, ChoirAh, 1.0f, 0.25f).ratioFine = 0.25f;
    osc(p, 3, ChoirOo, 2.0f, 0.1f);
    p.voice.analogFeel = 0.4f;
    adsr(p.env[2], 700.0f, 1000.0f, 1.0f, 1800.0f);
    vibrato(p, 4.6f, 0.04f, 600.0f);
    chorus(p, 0.45f);
    room(p, ReverbType::Hall, 4.0f, 0.32f);
    macro(p, 1, 0.0f, { to(oscLevel(3), 0.2f) });                                            // Oo
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                     // Space
    p.vca.patchLevel = 1.04f; // level-matched across the bank
    return p;
}

// A shimmering veil: high FM partials, long cloud reverb, slow pan movement.
Patch shimmerVeil()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.35f);
    envLevel(osc(p, 2, Sine, 2.0f, 0.05f), 0.15f, 1500.0f, 3000.0f, 0.5f, 2000.0f);
    osc(p, 3, Sine, 4.0f, 0.12f).ratioFine = 0.3f;
    envLevel(osc(p, 4, Sine, 7.0f, 0.0f), 0.12f, 2000.0f, 3000.0f, 0.6f, 2000.0f);
    adsr(p.env[2], 1200.0f, 1500.0f, 1.0f, 3000.0f);
    p.voice.panner = 6;
    p.voice.pannerWidth = 0.6f;
    room(p, ReverbType::Cloud, 9.0f, 0.45f, 60.0f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(4), 0.15f) });                                        // Shimmer
    macro(p, 2, 0.0f, { to(fx(dst::ReverbTime), 0.5f) });                                    // Size
    p.vca.patchLevel = 0.59f; // level-matched across the bank
    return p;
}

// Dark strings: the Strings wave low-passed, slow attack, ensemble.
Patch darkStrings()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Strings, 1.0f, 0.35f);
    osc(p, 2, Strings, 1.0f, 0.25f).ratioFine = -0.3f;
    osc(p, 3, Strings, 0.5f, 0.15f);
    p.afilt.cutoff = cutoff(1400.0f);
    adsr(p.env[2], 800.0f, 1200.0f, 1.0f, 2000.0f);
    chorus(p, 0.35f);
    room(p, ReverbType::Hall, 4.0f, 0.3f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Light
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvAttack), 0.5f) });                            // Bow
    p.vca.patchLevel = 0.90f; // level-matched across the bank
    return p;
}

// A tidal pad: each oscillator scans its own table range at a different slow rate.
Patch tideScan()
{
    Patch p = blank("Eight Carriers");
    OscParams& a = osc(p, 1, SoftSaw, 1.0f, 0.25f);
    scan(a, 0.0f, 4);
    OscParams& b = osc(p, 2, Hollow, 1.0f, 0.25f);
    b.ratioFine = 0.25f;
    scan(b, 0.0f, 5);
    p.lfo[2].rateHz = 0.05f;
    p.lfo[2].trig = LfoTrig::Free;
    p.lfo[4].rateHz = 0.08f;
    p.lfo[4].trig = LfoTrig::Free;
    p.lfo[4].wave = LfoWave::Triangle;
    route(p, src::Lfo1Uni + 2, dst::OscScanBase + 0, 1.0f);
    route(p, src::Lfo1Uni + 4, dst::OscScanBase + 1, 1.0f);
    p.afilt.cutoff = cutoff(3500.0f);
    adsr(p.env[2], 1000.0f, 1500.0f, 1.0f, 2500.0f);
    chorus(p, 0.3f);
    room(p, ReverbType::Hall, 5.0f, 0.32f);
    macro(p, 1, 0.0f, { to(dst::AllOscScan, 0.3f) });                                        // Tide
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Bright
    p.vca.patchLevel = 1.23f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Pluck
// ================================================================================================

// A silky pluck: a soft saw through a closing filter, dotted echoes.
Patch silkPluck()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, SoftSaw, 1.0f, 0.5f);
    osc(p, 2, Sine, 2.0f, 0.12f);
    p.afilt.cutoff = cutoff(500.0f);
    p.afilt.envAmt = 0.55f;
    p.afilt.resonance = 0.25f;
    adsr(p.env[1], 0.0f, 300.0f, 0.0f, 200.0f);
    adsr(p.env[2], 0.0f, 1200.0f, 0.0f, 600.0f);
    p.vca.velEnv = 0.6f;
    echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.22f);
    room(p, ReverbType::Hall, 2.0f, 0.2f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Bright
    macro(p, 2, 0.0f, { to(dst::env(1, dst::EnvDecay), 0.5f) });                             // Snap
    p.vca.patchLevel = 1.95f; // level-matched across the bank
    return p;
}

// A nylon-string pluck: FM with a fast bright attack and a warm body.
Patch nylonPluck()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.45f);
    strike(p, 2, Sine, 1.0f, 0.35f, 600.0f, 0.7f);
    osc(p, 3, Sine, 2.0f, 0.15f);
    strike(p, 4, Sine, 5.0f, 0.15f, 60.0f, 0.8f);
    p.afilt.cutoff = cutoff(3000.0f);
    adsr(p.env[2], 0.0f, 2000.0f, 0.0f, 500.0f);
    p.env[2].decayCurve = 0.7f;
    p.vca.velEnv = 0.6f;
    room(p, ReverbType::Room, 1.2f, 0.18f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(4), 0.25f) });                                        // Pick
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Tone
    p.vca.patchLevel = 0.90f; // level-matched across the bank
    return p;
}

// Pizzicato strings: short Strings-wave plucks in a hall.
Patch pizzStrings()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Strings, 1.0f, 0.45f);
    osc(p, 2, Strings, 1.0f, 0.25f).ratioFine = 0.2f;
    p.afilt.cutoff = cutoff(1500.0f);
    p.afilt.envAmt = 0.35f;
    adsr(p.env[1], 0.0f, 150.0f, 0.0f, 120.0f);
    adsr(p.env[2], 0.0f, 450.0f, 0.0f, 300.0f);
    p.vca.velEnv = 0.6f;
    room(p, ReverbType::Hall, 2.5f, 0.28f);
    macro(p, 1, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Length
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Bright
    p.vca.patchLevel = 1.06f; // level-matched across the bank
    return p;
}

// A marimba: Bump body, a 4:1 mallet partial and a quick decay.
Patch marimbaPluck()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 1.0f, 0.5f);
    strike(p, 2, Sine, 4.0f, 0.3f, 80.0f, 0.8f);
    envLevel(osc(p, 3, Bump, 1.0f, 0.0f), 0.25f, 0.0f, 400.0f, 0.0f, 300.0f, 0.5f);
    adsr(p.env[2], 0.0f, 900.0f, 0.0f, 500.0f);
    p.env[2].decayCurve = 0.7f;
    p.vca.velEnv = 0.6f;
    room(p, ReverbType::Room, 1.3f, 0.2f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Mallet
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Ring
    p.vca.patchLevel = 1.17f; // level-matched across the bank
    return p;
}

// A digital harp: bright PD pluck with long ringing tails.
Patch digitalHarp()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 1.0f, 0.5f);
    OscParams& m = strike(p, 2, Sine, 1.0f, 0.5f, 400.0f, 0.7f);
    m.mode = OscMode::PDSaw;
    osc(p, 3, Triangle, 2.0f, 0.12f);
    adsr(p.env[2], 0.0f, 3500.0f, 0.0f, 2000.0f);
    p.env[2].decayCurve = 0.7f;
    p.vca.velEnv = 0.5f;
    echo(p, DelayType::Stereo, "1/4", 0.3f, 0.18f);
    room(p, ReverbType::Hall, 3.0f, 0.28f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Pluck
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                     // Space
    p.vca.patchLevel = 0.97f; // level-matched across the bank
    return p;
}

// A classic synth pluck: a hard-synced saw with a very short sweep.
Patch syncPluck()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.45f);
    OscParams& m = strike(p, 2, Sine, 1.0f, 0.6f, 150.0f, 0.6f);
    m.mode = OscMode::HardSync;
    p.afilt.cutoff = cutoff(1500.0f);
    p.afilt.envAmt = 0.4f;
    adsr(p.env[1], 0.0f, 200.0f, 0.0f, 150.0f);
    adsr(p.env[2], 0.0f, 600.0f, 0.0f, 300.0f);
    p.vca.velEnv = 0.6f;
    echo(p, DelayType::PingPong, "1/8", 0.35f, 0.2f);
    room(p, ReverbType::Room, 1.2f, 0.15f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Sweep
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Length
    p.vca.patchLevel = 1.84f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Sequence (hold a note or chord; tempo-synced)
// ================================================================================================

// A trance gate: the VCA chopped into 16ths by a square LFO.
Patch tranceGate()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, SawStack, 1.0f, 0.3f);
    osc(p, 2, SawStack, 1.0f, 0.3f).ratioFine = 0.3f;
    p.lfo[2].wave = LfoWave::Square;
    p.lfo[2].bpmSync = true;
    p.lfo[2].syncDiv = syncDiv("1/16");
    p.lfo[2].trig = LfoTrig::Single;
    p.vca.lfoAmt = 0.9f;
    p.afilt.cutoff = cutoff(3000.0f);
    adsr(p.env[2], 5.0f, 300.0f, 1.0f, 300.0f);
    echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.2f);
    room(p, ReverbType::Hall, 2.0f, 0.2f);
    macro(p, 1, 0.0f, { to(dst::VcaBase + dst::VcaLfoAmt, -0.5f) });                         // Gate depth
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Bright
    p.vca.patchLevel = 1.19f; // level-matched across the bank
    return p;
}

// A step bassline from one held note: 16 pitch steps (in semitones) drive the pitch.
Patch stepBassline()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.8f);
    strike(p, 2, Sine, 1.0f, 0.3f, 120.0f, 0.5f);
    const float semis[16] = { 0, 0, 12, 0, 7, 0, 10, 12, 0, 0, 5, 0, 7, 10, 0, 3 };
    for (int i = 0; i < 16; ++i) p.lfoSteps[4][static_cast<size_t>(i)] = semis[i] / 12.8f; // pitch mod spans 12.8 st
    LfoParams& l = p.lfo[4];
    l.wave = LfoWave::Step;
    l.steps = 16;
    l.bpmSync = true;
    l.syncDiv = syncDiv("1/1");
    l.trig = LfoTrig::Single;
    route(p, src::Lfo1 + 4, allPitch, 1.0f);
    p.env[2].trig = { EnvTrig::NoteOn, EnvTrig::Lfo5, EnvTrig::Off, EnvTrig::Off };
    p.afilt.cutoff = cutoff(500.0f);
    p.afilt.resonance = 0.5f;
    p.afilt.envAmt = 0.4f;
    p.env[1].trig = { EnvTrig::NoteOn, EnvTrig::Lfo5, EnvTrig::Off, EnvTrig::Off };
    adsr(p.env[1], 0.0f, 150.0f, 0.0f, 80.0f);
    adsr(p.env[2], 0.0f, 200.0f, 0.2f, 80.0f);
    p.voice.polyMode = PolyMode::Mono;
    echo(p, DelayType::PingPong, "1/8D", 0.3f, 0.15f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Cutoff
    macro(p, 2, 0.0f, { to(afRes, 0.3f) });                                                  // Squelch
    p.vca.oscsLevel = 1.00f;
    p.vca.patchLevel = 2.00f; // level-matched across the bank
    return p;
}

// Ticks in 3 against 4: two envelopes looping at different tempo divisions.
Patch polyTicks()
{
    Patch p = blank("Eight Carriers");
    envLevel(osc(p, 1, Sine, 2.0f, 0.0f), 0.5f, 0.0f, 80.0f, 0.0f, 50.0f);
    envLevel(osc(p, 2, BellPartials, 3.0f, 0.0f), 0.35f, 0.0f, 60.0f, 0.0f, 40.0f);
    OscParams& a = p.osc[0];
    OscParams& b = p.osc[1];
    a.env.delayMs = 250.0f; // 1/16 at 120 bpm with BPM sync
    b.env.delayMs = 333.0f;
    loopEnv(a.env);
    loopEnv(b.env);
    a.env.bpmSync = b.env.bpmSync = true;
    adsr(p.env[2], 0.0f, 200.0f, 1.0f, 300.0f);
    echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.2f);
    room(p, ReverbType::Room, 1.2f, 0.18f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.25f) });                                        // Bells
    macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });                                      // Echo
    p.vca.patchLevel = 1.06f; // level-matched across the bank
    return p;
}

// A filter-step chord sequence: hold a chord, a 16-step pattern plays the ladder cutoff.
Patch stepChords()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Saw, 1.0f, 0.3f);
    osc(p, 2, Pulse40, 1.0f, 0.2f).ratioFine = 0.2f;
    const float steps[16] = { 1.0f, -0.8f, 0.3f, -0.8f, 0.8f, -0.8f, 0.0f, 0.6f,
                              1.0f, -0.8f, 0.3f, -0.8f, 0.5f, 0.9f, -0.8f, 0.2f };
    for (int i = 0; i < 16; ++i) p.lfoSteps[1][static_cast<size_t>(i)] = steps[i];
    LfoParams& l = p.lfo[1];
    l.wave = LfoWave::Step;
    l.steps = 16;
    l.bpmSync = true;
    l.syncDiv = syncDiv("1/1");
    l.trig = LfoTrig::Single;
    l.smooth = 0.15f;
    p.afilt.cutoff = cutoff(900.0f);
    p.afilt.resonance = 0.45f;
    p.afilt.lfoAmt = 0.3f;
    adsr(p.env[2], 5.0f, 300.0f, 1.0f, 300.0f);
    echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.22f);
    room(p, ReverbType::Hall, 1.8f, 0.18f);
    macro(p, 1, 0.0f, { to(dst::AFiltBase + dst::AfLfoAmt, 0.3f) });                         // Pattern
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Cutoff
    p.vca.patchLevel = 1.60f; // level-matched across the bank
    return p;
}

// Random blips: sample-and-hold pitch steps in 16ths, retriggering the envelope.
Patch randomBlips()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 2.0f, 0.5f);
    strike(p, 2, Sine, 3.0f, 0.3f, 80.0f, 0.5f);
    LfoParams& l = p.lfo[4];
    l.wave = LfoWave::SampleHold;
    l.bpmSync = true;
    l.syncDiv = syncDiv("1/16");
    l.trig = LfoTrig::Single;
    route(p, src::Lfo1 + 4, allPitch, 0.25f);
    p.env[2].trig = { EnvTrig::NoteOn, EnvTrig::Lfo5, EnvTrig::Off, EnvTrig::Off };
    adsr(p.env[2], 0.0f, 100.0f, 0.0f, 60.0f);
    echo(p, DelayType::PingPong, "1/8D", 0.45f, 0.3f);
    room(p, ReverbType::Hall, 2.0f, 0.2f);
    macro(p, 1, 0.0f, { to(dst::lfo(4, dst::LfoLevel), -0.5f) });                            // Range
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Length
    p.vca.oscsLevel = 1.00f;
    p.vca.patchLevel = 1.40f; // level-matched across the bank
    return p;
}

// A pulsing pad: the VCA envelope retriggers on every dotted 8th from LFO 5.
Patch dottedPulse()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Hollow, 1.0f, 0.35f);
    osc(p, 2, Hollow, 2.0f, 0.12f).ratioFine = 0.2f;
    LfoParams& l = p.lfo[4];
    l.bpmSync = true;
    l.syncDiv = syncDiv("1/8D");
    l.trig = LfoTrig::Single;
    p.env[2].trig = { EnvTrig::NoteOn, EnvTrig::Lfo5, EnvTrig::Off, EnvTrig::Off };
    adsr(p.env[2], 5.0f, 300.0f, 0.1f, 200.0f);
    p.afilt.cutoff = cutoff(2500.0f);
    echo(p, DelayType::PingPong, "1/4", 0.35f, 0.22f);
    room(p, ReverbType::Hall, 2.5f, 0.22f);
    macro(p, 1, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Length
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Bright
    p.vca.patchLevel = 1.82f; // level-matched across the bank
    return p;
}

// ================================================================================================
// FX
// ================================================================================================

// A long riser: pitch and filter climb over several seconds while held.
Patch riser()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, SawStack, 1.0f, 0.3f);
    OscParams& n = osc(p, 2, Sine, 3.0f, 0.15f);
    n.feedback = 0.9f; // noise-like
    route(p, src::Env1 + 3, allPitch, 0.6f);
    adsr(p.env[3], 6000.0f, 100.0f, 1.0f, 300.0f);
    p.afilt.cutoff = cutoff(300.0f);
    p.afilt.envAmt = 0.6f;
    adsr(p.env[1], 6000.0f, 100.0f, 1.0f, 300.0f);
    adsr(p.env[2], 50.0f, 100.0f, 1.0f, 800.0f);
    echo(p, DelayType::PingPong, "1/8D", 0.45f, 0.25f);
    room(p, ReverbType::Cloud, 5.0f, 0.35f);
    macro(p, 1, 0.0f, { to(dst::env(3, dst::EnvAttack), 0.5f), to(dst::env(1, dst::EnvAttack), 0.5f) }); // Length
    macro(p, 2, 0.0f, { to(oscLevel(2), 0.2f) });                                                         // Noise
    p.vca.oscsLevel = 1.00f;
    p.vca.patchLevel = 2.00f; // level-matched across the bank
    return p;
}

// A laser zap: a fast downward pitch sweep with FM grit.
Patch laserZap()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 1.0f, 0.5f);
    strike(p, 2, Sine, 1.5f, 0.5f, 250.0f, 0.5f);
    route(p, src::Env1 + 3, allPitch, 0.9f);
    adsr(p.env[3], 0.0f, 250.0f, 0.0f, 100.0f);
    adsr(p.env[2], 0.0f, 350.0f, 0.0f, 200.0f);
    echo(p, DelayType::PingPong, "1/16", 0.4f, 0.25f);
    room(p, ReverbType::Plate, 1.5f, 0.18f);
    macro(p, 1, 0.0f, { to(dst::env(3, dst::EnvDecay), 0.5f) });                             // Sweep
    macro(p, 2, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Grit
    p.vca.oscsLevel = 1.00f;
    p.vca.patchLevel = 2.00f; // level-matched across the bank
    return p;
}

// Wind: feedback noise through a slowly swept band-pass.
Patch windNoise()
{
    Patch p = blank("Eight Carriers");
    OscParams& n = osc(p, 1, Sine, 1.0f, 0.45f);
    n.feedback = 1.0f;
    OscParams& n2 = osc(p, 2, Sine, 1.37f, 0.3f);
    n2.feedback = 1.0f;
    p.dfilt.type = DigitalFilterType::BpModular;
    p.dfilt.cutoff = cutoff(800.0f);
    p.dfilt.resonance = 0.55f;
    p.dfilt.lfoAmt = 0.35f;
    p.lfo[0].rateHz = 0.12f;
    p.lfo[0].wave = LfoWave::Random;
    p.lfo[0].smooth = 0.9f;
    p.lfo[0].trig = LfoTrig::Free;
    adsr(p.env[2], 1500.0f, 1000.0f, 1.0f, 2500.0f);
    room(p, ReverbType::Cloud, 6.0f, 0.4f);
    macro(p, 1, 0.0f, { to(dfCut, 0.35f) });                                                 // Pitch
    macro(p, 2, 0.0f, { to(dst::lfo(0, dst::LfoRate), 0.5f) });                              // Gusts
    p.vca.patchLevel = 1.29f; // level-matched across the bank
    return p;
}

// An alarm: a square LFO flips the pitch between two notes.
Patch alarm()
{
    Patch p = blank("One Pair");
    osc(p, 1, Square, 1.0f, 0.35f);
    strike(p, 2, Sine, 2.0f, 0.15f, 300.0f, 0.3f);
    LfoParams& l = p.lfo[4];
    l.wave = LfoWave::Square;
    l.rateHz = 3.0f;
    route(p, src::Lfo1Uni + 4, allPitch, 0.39f); // ~5 semitones
    p.afilt.cutoff = cutoff(3000.0f);
    adsr(p.env[2], 5.0f, 300.0f, 1.0f, 200.0f);
    room(p, ReverbType::Room, 1.2f, 0.18f);
    macro(p, 1, 0.0f, { to(dst::lfo(4, dst::LfoRate), 0.5f) });                              // Speed
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Tone
    p.vca.patchLevel = 1.00f; // level-matched across the bank
    return p;
}

// Underwater: a muffled, wobbling low-pass with a slow phaser.
Patch underwater()
{
    Patch p = blank("One Pair");
    osc(p, 1, SawStack, 1.0f, 0.4f);
    osc(p, 2, Sine, 0.5f, 0.2f);
    p.dfilt.cutoff = cutoff(500.0f);
    p.dfilt.resonance = 0.5f;
    p.dfilt.lfoAmt = 0.3f;
    p.lfo[0].rateHz = 0.5f;
    p.fx.pre.type = FxType::Phaser;
    p.fx.pre.phaser.rateHz = 0.15f;
    p.fx.pre.phaser.feedback = 0.6f;
    adsr(p.env[2], 300.0f, 600.0f, 1.0f, 1200.0f);
    echo(p, DelayType::Stereo, "1/4D", 0.4f, 0.2f);
    room(p, ReverbType::Cloud, 5.0f, 0.35f);
    macro(p, 1, 0.0f, { to(dfCut, 0.35f) });                                                 // Surface
    macro(p, 2, 0.0f, { to(dst::lfo(0, dst::LfoRate), 0.5f) });                              // Current
    p.vca.oscsLevel = 1.00f;
    p.vca.patchLevel = 2.00f; // level-matched across the bank
    return p;
}

// Glitch bits: Vintage Digital at 6 bits, S&H on the scan position and pitch.
Patch glitchBits()
{
    Patch p = blank("Eight Carriers");
    OscParams& o = osc(p, 1, Stairs8, 1.0f, 0.45f);
    scan(o, 0.0f, 8);
    p.voice.vintage = true;
    p.voice.bitDepth = 6;
    LfoParams& l = p.lfo[4];
    l.wave = LfoWave::SampleHold;
    l.bpmSync = true;
    l.syncDiv = syncDiv("1/16");
    route(p, src::Lfo1Uni + 4, dst::OscScanBase + 0, 1.0f);
    route(p, src::Lfo1 + 4, allPitch, 0.1f);
    p.afilt.cutoff = cutoff(4000.0f);
    adsr(p.env[2], 2.0f, 300.0f, 1.0f, 300.0f);
    p.fx.post.type = FxType::LoFi;
    p.fx.post.lofi = { 6000.0f, 1.5f, LoFiFilter::LowPass, 0.0f, 11025.0f, 0.5f };
    echo(p, DelayType::PingPong, "1/16", 0.4f, 0.2f);
    macro(p, 1, 0.0f, { to(fx(dst::PostFxMix), 0.5f) });                                     // Crush
    macro(p, 2, 0.0f, { to(dst::lfo(4, dst::LfoLevel), -0.5f) });                            // Calm
    p.vca.patchLevel = 1.19f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Drone
// ================================================================================================

// A low orbit: sub octaves with slowly beating fifths.
Patch lowOrbit()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Sine, 0.5f, 0.35f);
    osc(p, 2, Sine, 0.75f, 0.2f).ratioFine = 0.15f;
    osc(p, 3, Triangle, 1.0f, 0.2f);
    osc(p, 4, Sine, 1.5f, 0.12f).ratioFine = -0.1f;
    p.lfo[2].rateHz = 0.07f;
    p.lfo[2].trig = LfoTrig::Free;
    p.vca.lfoAmt = 0.2f;
    adsr(p.env[2], 2500.0f, 1000.0f, 1.0f, 4000.0f);
    room(p, ReverbType::Cloud, 10.0f, 0.4f, 60.0f);
    macro(p, 1, 0.0f, { to(oscLevel(4), 0.15f) });                                           // Fifth
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                     // Space
    p.vca.patchLevel = 0.64f; // level-matched across the bank
    return p;
}

// A cathedral hum: organ tables and a huge hall, nearly frozen.
Patch cathedralHum()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, Organ84, 0.5f, 0.3f);
    osc(p, 2, OrganFull, 1.0f, 0.2f).ratioFine = 0.1f;
    osc(p, 3, ChoirOo, 2.0f, 0.1f);
    p.afilt.cutoff = cutoff(1800.0f);
    adsr(p.env[2], 3000.0f, 1000.0f, 1.0f, 5000.0f);
    room(p, ReverbType::Hall, 12.0f, 0.45f, 80.0f);
    macro(p, 1, 0.0f, { to(oscLevel(3), 0.15f) });                                           // Voices
    macro(p, 2, 0.0f, { to(fx(dst::ReverbTime), 0.5f) });                                    // Size
    p.vca.patchLevel = 0.72f; // level-matched across the bank
    return p;
}

// An iron hum: inharmonic FM drone with a slow chorus and tremble.
Patch ironHum()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 0.5f, 0.45f);
    OscParams& m = osc(p, 2, Sine, 0.707f, 0.15f);
    m.feedback = 0.25f;
    p.lfo[3].rateHz = 0.1f;
    p.lfo[3].trig = LfoTrig::Free;
    route(p, src::Lfo1 + 3, oscLevel(2), 0.1f);
    osc(p, 3, Sine, 1.0f, 0.15f);
    adsr(p.env[2], 2000.0f, 1000.0f, 1.0f, 4000.0f);
    chorus(p, 0.4f);
    room(p, ReverbType::Cloud, 8.0f, 0.4f, 40.0f);
    macro(p, 1, 0.0f, { to(oscLevel(2), 0.15f) });                                           // Rust
    macro(p, 2, 0.0f, { to(dst::osc(1, dst::OscFeedback), 0.3f) });                          // Grit
    p.vca.patchLevel = 0.93f; // level-matched across the bank
    return p;
}

// A night choir drone: choir tables crossfading on two slow LFOs.
Patch nightChoir()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, ChoirAh, 0.5f, 0.3f);
    osc(p, 2, ChoirOo, 1.0f, 0.25f).ratioFine = 0.2f;
    osc(p, 3, ChoirAh, 1.5f, 0.1f);
    p.lfo[2].rateHz = 0.05f;
    p.lfo[2].trig = LfoTrig::Free;
    route(p, src::Lfo1 + 2, oscLevel(1), 0.15f);
    route(p, src::Lfo1 + 2, oscLevel(2), -0.15f);
    adsr(p.env[2], 2500.0f, 1000.0f, 1.0f, 4000.0f);
    p.voice.bitDepth = 12;
    room(p, ReverbType::Cloud, 10.0f, 0.45f, 60.0f);
    macro(p, 1, 0.0f, { to(oscLevel(3), 0.15f) });                                           // Fifth
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                     // Space
    p.vca.patchLevel = 0.82f; // level-matched across the bank
    return p;
}

// A tidal drone: one oscillator scans the whole Shaped group over half a minute.
Patch tidalDrone()
{
    Patch p = blank("Eight Carriers");
    OscParams& o = osc(p, 1, HalfSine, 0.5f, 0.4f);
    scan(o, 0.0f, 12);
    p.lfo[2].rateHz = 0.03f;
    p.lfo[2].trig = LfoTrig::Free;
    p.lfo[2].wave = LfoWave::Triangle;
    route(p, src::Lfo1Uni + 2, dst::OscScanBase + 0, 1.0f);
    osc(p, 2, Sine, 1.0f, 0.15f);
    p.afilt.cutoff = cutoff(2000.0f);
    adsr(p.env[2], 2000.0f, 1000.0f, 1.0f, 4000.0f);
    room(p, ReverbType::Cloud, 9.0f, 0.4f, 50.0f);
    macro(p, 1, 0.0f, { to(dst::lfo(2, dst::LfoRate), 0.5f) });                              // Tide speed
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Bright
    p.vca.patchLevel = 0.68f; // level-matched across the bank
    return p;
}

// Beating fifths: two pairs a fifth apart, detuned so the drone breathes.
Patch beatingFifths()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 0.5f, 0.35f);
    osc(p, 2, Sine, 1.0f, 0.12f);
    osc(p, 3, Sine, 0.75f, 0.3f).ratioFine = 0.25f;
    osc(p, 4, Sine, 1.5f, 0.1f);
    p.lfo[3].rateHz = 0.09f;
    p.lfo[3].trig = LfoTrig::Free;
    route(p, src::Lfo1 + 3, dst::ModulatorLevel, 0.08f);
    adsr(p.env[2], 2000.0f, 1000.0f, 1.0f, 4000.0f);
    room(p, ReverbType::Hall, 8.0f, 0.38f, 50.0f);
    macro(p, 1, 0.0f, { to(dst::ModulatorLevel, 0.2f) });                                    // Bright
    macro(p, 2, 0.0f, { to(dst::osc(2, dst::OscFine), 0.3f) });                              // Beat
    p.vca.patchLevel = 0.54f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Showcase
// ================================================================================================

// Scan Orchestra: four oscillators scan different table groups; the macros conduct them.
Patch scanOrchestra()
{
    Patch p = blank("Eight Carriers");
    scan(osc(p, 1, SoftSaw, 1.0f, 0.2f), 0.2f, 5);
    scan(osc(p, 2, OctavePair, 1.0f, 0.2f), 0.0f, 8);
    p.osc[1].ratioFine = 0.2f;
    scan(osc(p, 3, HalfSine, 2.0f, 0.1f), 0.5f, 6);
    scan(osc(p, 4, Sine, 0.5f, 0.15f), 0.0f, 3);
    p.lfo[2].rateHz = 0.1f;
    p.lfo[2].trig = LfoTrig::Free;
    p.lfo[4].rateHz = 0.17f;
    p.lfo[4].trig = LfoTrig::Free;
    route(p, src::Lfo1Uni + 2, dst::OscScanBase + 1, 0.8f);
    route(p, src::Lfo1 + 4, dst::OscScanBase + 2, 0.4f);
    route(p, src::Velocity, dst::OscScanBase + 0, 0.5f);
    adsr(p.env[2], 500.0f, 1000.0f, 1.0f, 2000.0f);
    chorus(p, 0.3f);
    room(p, ReverbType::Hall, 4.0f, 0.3f);
    macro(p, 1, 0.0f, { to(dst::AllOscScan, 0.5f) });                                        // Scan all
    macro(p, 2, 0.0f, { to(dst::lfo(2, dst::LfoRate), 0.4f), to(dst::lfo(4, dst::LfoRate), 0.4f) }); // Motion
    macro(p, 3, 0.0f, { to(oscLevel(3), 0.2f) });                                            // Air
    macro(p, 4, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                     // Space
    p.vca.patchLevel = 1.02f; // level-matched across the bank
    return p;
}

// Expressive brass: MPE bend, pressure opens the filter and adds blare, CC74 adds brightness.
Patch expressiveBrass()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.45f);
    OscParams& m = osc(p, 2, Sine, 1.0f, 0.1f);
    m.mode = OscMode::PDSaw;
    m.polyAT = 0.35f;
    p.voice.mpe = true;
    p.afilt.cutoff = cutoff(900.0f);
    p.afilt.polyAT = 0.35f;
    route(p, src::Timbre, afCut, 0.3f);
    route(p, src::Velocity, oscLevel(2), 0.2f);
    adsr(p.env[2], 40.0f, 400.0f, 0.9f, 300.0f);
    vibrato(p, 5.0f, 0.05f, 500.0f);
    room(p, ReverbType::Hall, 2.0f, 0.2f);
    macro(p, 1, 0.0f, { to(afCut, 0.3f) });                                                  // Bright
    macro(p, 2, 0.0f, { to(oscLevel(2), 0.25f) });                                           // Blare
    p.vca.patchLevel = 0.72f; // level-matched across the bank
    return p;
}

// Stereo universe: Osc Stereo pairs, per-oscillator pan, the Panner and a ping-pong delay.
Patch stereoUniverse()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.4f).pan = -0.7f;
    strike(p, 2, Sine, 2.0f, 0.3f, 1500.0f, 0.6f).pan = -0.4f;
    osc(p, 3, Sine, 1.0f, 0.3f).pan = 0.7f;
    strike(p, 4, Sine, 3.0f, 0.25f, 900.0f, 0.6f).pan = 0.4f;
    p.voice.oscStereo = true;
    p.voice.oscPanTarget = OscPanTarget::Both;
    p.voice.carrierDetune = 12.0f;
    p.voice.panner = 3;
    p.voice.pannerWidth = 0.5f;
    adsr(p.env[2], 5.0f, 3000.0f, 0.3f, 1500.0f);
    echo(p, DelayType::PingPong, "1/8D", 0.4f, 0.22f);
    room(p, ReverbType::Cloud, 5.0f, 0.3f);
    macro(p, 1, 0.0f, { to(dst::AllOscBase + dst::OscPan, 0.5f) });                          // Width
    macro(p, 2, 0.0f, { to(dst::ModulatorEnvLevel, 0.2f) });                                 // Bright
    p.vca.patchLevel = 0.61f; // level-matched across the bank
    return p;
}

// Random garden: per-note randomness on ratio fine, scan, pan and decay; no two notes alike.
Patch randomGarden()
{
    Patch p = blank("One Pair");
    OscParams& c = osc(p, 1, Sine, 1.0f, 0.45f);
    scan(c, 0.0f, 6);
    strike(p, 2, Sine, 3.0f, 0.3f, 600.0f, 0.6f);
    route(p, src::RandomPerNote, dst::OscScanBase + 0, 0.8f);
    route(p, src::RandomPerNote, dst::osc(1, dst::OscFine), 0.3f);
    route(p, src::RandomPerNote, dst::VoiceBase + dst::VoicePan, 0.8f);
    route(p, src::RandomPerNote, dst::env(2, dst::EnvDecay), 0.4f);
    adsr(p.env[2], 0.0f, 1500.0f, 0.0f, 1000.0f);
    echo(p, DelayType::PingPong, "1/8D", 0.4f, 0.25f);
    room(p, ReverbType::Hall, 3.0f, 0.28f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.3f) });                                         // Bright
    macro(p, 2, 0.0f, { to(fx(dst::DelayMix), 0.3f) });                                      // Echo
    p.vca.patchLevel = 1.97f; // level-matched across the bank
    return p;
}

// Morph bells: an envelope morphs the algorithm from a bell tree into a single stack during each note.
Patch morphBells()
{
    Patch p = blank("Bell Tree");
    const float ratios[] = { 1.0f, 2.0f, 3.5f, 0.5f, 4.24f, 7.13f, 1.41f, 2.76f };
    for (int i = 0; i < 8; ++i) osc(p, i + 1, Sine, ratios[i], i < 2 ? 0.35f : 0.18f);
    p.algo.mode = AlgoMode::Morph;
    p.algo.morphSlots = { kMorphSlotFirstPreset + findPresetAlgorithm("Bell Tree"),
                          kMorphSlotFirstPreset + findPresetAlgorithm("Tall Tower"), 0, 0, 0, 0, 0, 0 };
    route(p, src::Env1 + 3, dst::AlgoMorph, 0.14f);
    adsr(p.env[3], 0.0f, 3000.0f, 1.0f, 2000.0f);
    adsr(p.env[2], 0.0f, 6000.0f, 0.0f, 3000.0f);
    p.env[2].decayCurve = 0.6f;
    room(p, ReverbType::Hall, 4.0f, 0.3f);
    macro(p, 1, 0.0f, { to(dst::AlgoMorph, 0.14f) });                                        // Morph
    macro(p, 2, 0.0f, { to(dst::ModulatorLevel, 0.2f) });                                    // Metal
    p.vca.patchLevel = 0.85f; // level-matched across the bank
    return p;
}

// Tempo machine: the arpeggiator, a step LFO on the filter and a synced ping-pong delay together.
Patch tempoMachine()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.75f);
    strike(p, 2, Sine, 2.0f, 0.3f, 150.0f, 0.7f);
    const float steps[16] = { 1.0f, 0.2f, 0.6f, -0.5f, 0.8f, 0.0f, -0.6f, 0.4f,
                              1.0f, -0.2f, 0.5f, -0.8f, 0.7f, 0.3f, -0.4f, 0.9f };
    for (int i = 0; i < 16; ++i) p.lfoSteps[1][static_cast<size_t>(i)] = steps[i];
    LfoParams& l = p.lfo[1];
    l.wave = LfoWave::Step;
    l.steps = 16;
    l.bpmSync = true;
    l.syncDiv = syncDiv("2 bars");
    l.trig = LfoTrig::Free;
    l.smooth = 0.2f;
    p.afilt.cutoff = cutoff(1100.0f);
    p.afilt.resonance = 0.45f;
    p.afilt.lfoAmt = 0.3f;
    p.afilt.envAmt = 0.3f;
    adsr(p.env[1], 0.0f, 200.0f, 0.0f, 150.0f);
    adsr(p.env[2], 0.0f, 300.0f, 0.3f, 150.0f);
    arp(p, ArpMode::UpDown, 2, "1/16", 0.5f);
    p.arp.swing = 0.12f;
    echo(p, DelayType::PingPong, "1/8D", 0.4f, 0.25f);
    room(p, ReverbType::Room, 1.2f, 0.15f);
    macro(p, 1, 0.0f, { to(dst::AFiltBase + dst::AfLfoAmt, 0.3f) });                         // Pattern
    macro(p, 2, 0.0f, { to(afCut, 0.3f) });                                                  // Cutoff
    macro(p, 3, 0.0f, { to(fx(dst::DelayFeedback), 0.3f) });                                 // Echo
    p.vca.oscsLevel = 1.00f;
    p.vca.patchLevel = 2.00f; // level-matched across the bank
    return p;
}

// ================================================================================================
// Arp (hold a chord)
// ================================================================================================

// Bright plucks going up three octaves in 16ths.
Patch risingPlucks()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.45f);
    strike(p, 2, Sine, 2.0f, 0.25f, 120.0f, 0.7f);
    p.afilt.cutoff = cutoff(1400.0f);
    p.afilt.envAmt = 0.4f;
    adsr(p.env[1], 0.0f, 180.0f, 0.0f, 120.0f);
    adsr(p.env[2], 0.0f, 250.0f, 0.0f, 150.0f);
    arp(p, ArpMode::Up, 3, "1/16", 0.5f);
    echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.22f);
    room(p, ReverbType::Hall, 2.0f, 0.18f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Bright
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Length
    p.vca.oscsLevel = 1.00f;
    p.vca.patchLevel = 2.00f; // level-matched across the bank
    return p;
}

// Bells falling down two octaves in triplet 8ths.
Patch fallingBells()
{
    Patch p = blank("One Pair");
    osc(p, 1, Sine, 2.0f, 0.5f);
    strike(p, 2, Sine, 7.0f, 0.25f, 400.0f, 0.6f);
    adsr(p.env[2], 0.0f, 1200.0f, 0.0f, 800.0f);
    p.vca.velEnv = 0.5f;
    arp(p, ArpMode::Down, 2, "1/8T", 0.6f);
    echo(p, DelayType::Stereo, "1/4", 0.35f, 0.2f);
    room(p, ReverbType::Hall, 3.0f, 0.28f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(2), 0.25f) });                                        // Strike
    macro(p, 2, 0.0f, { to(fx(dst::ReverbMix), 0.3f) });                                     // Space
    p.vca.patchLevel = 1.17f; // level-matched across the bank
    return p;
}

// Chord stabs repeated in 8ths (Chord mode), with a filter envelope on every hit.
Patch chordStabs()
{
    Patch p = blank("Eight Carriers");
    osc(p, 1, SawStack, 1.0f, 0.3f);
    osc(p, 2, Pulse40, 1.0f, 0.15f).ratioFine = 0.2f;
    p.afilt.cutoff = cutoff(900.0f);
    p.afilt.envAmt = 0.45f;
    p.afilt.resonance = 0.3f;
    adsr(p.env[1], 0.0f, 200.0f, 0.0f, 150.0f);
    adsr(p.env[2], 0.0f, 250.0f, 0.0f, 150.0f);
    arp(p, ArpMode::Chord, 1, "1/8", 0.4f);
    echo(p, DelayType::PingPong, "1/8D", 0.35f, 0.22f);
    room(p, ReverbType::Hall, 2.0f, 0.2f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Bright
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Length
    p.vca.oscsLevel = 1.00f;
    p.vca.patchLevel = 2.00f; // level-matched across the bank
    return p;
}

// Notes in the order played, softly, with swing.
Patch playedOrder()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.45f);
    strike(p, 2, Sine, 1.0f, 0.3f, 500.0f, 0.6f);
    osc(p, 3, Sine, 2.0f, 0.15f);
    strike(p, 4, Sine, 5.0f, 0.12f, 120.0f, 0.7f);
    adsr(p.env[2], 0.0f, 900.0f, 0.0f, 500.0f);
    arp(p, ArpMode::Played, 2, "1/16", 0.6f);
    p.arp.swing = 0.25f;
    chorus(p, 0.3f);
    room(p, ReverbType::Room, 1.4f, 0.2f);
    macro(p, 1, 0.0f, { to(dst::ModulatorEnvLevel, 0.25f) });                                // Bright
    macro(p, 2, 0.0f, { to(dst::env(2, dst::EnvDecay), 0.5f) });                             // Length
    p.vca.patchLevel = 1.32f; // level-matched across the bank
    return p;
}

// An octave-jumping bass arpeggio, mono-tight and punchy.
Patch octaveBass()
{
    Patch p = blank("One Pair");
    osc(p, 1, Saw, 1.0f, 0.5f);
    strike(p, 2, Sine, 1.0f, 0.4f, 150.0f, 0.7f);
    osc(p, 3, Sine, 0.5f, 0.3f);
    p.afilt.cutoff = cutoff(600.0f);
    p.afilt.resonance = 0.4f;
    p.afilt.envAmt = 0.45f;
    adsr(p.env[1], 0.0f, 180.0f, 0.0f, 100.0f);
    adsr(p.env[2], 0.0f, 250.0f, 0.4f, 100.0f);
    arp(p, ArpMode::UpDown, 2, "1/16", 0.45f);
    macro(p, 1, 0.0f, { to(afCut, 0.35f) });                                                 // Cutoff
    macro(p, 2, 0.0f, { to(afRes, 0.3f) });                                                  // Squelch
    p.vca.oscsLevel = 0.88f;
    p.vca.patchLevel = 2.00f; // level-matched across the bank
    return p;
}

// Swung keys: a random arpeggio of soft electric piano with heavy swing.
Patch swingKeys()
{
    Patch p = blank("Two Pairs, Four Free");
    osc(p, 1, Sine, 1.0f, 0.45f);
    strike(p, 2, Sine, 1.0f, 0.3f, 900.0f, 0.7f);
    osc(p, 3, Sine, 1.0f, 0.25f).ratioFine = 0.12f;
    strike(p, 4, Sine, 14.0f, 0.15f, 120.0f, 0.8f);
    adsr(p.env[2], 0.0f, 1500.0f, 0.0f, 500.0f);
    p.vca.velEnv = 0.6f;
    arp(p, ArpMode::Random, 2, "1/8", 0.55f);
    p.arp.swing = 0.35f;
    p.fx.pre.type = FxType::Tremolo;
    p.fx.pre.tremolo = { 4.6f, 0.3f, false, 180.0f, 0.0f, 1.0f };
    room(p, ReverbType::Room, 1.3f, 0.2f);
    macro(p, 1, 0.0f, { to(oscEnvLevel(4), 0.25f) });                                        // Tine
    macro(p, 2, 0.0f, { to(fx(dst::PreFxParam2), 0.5f) });                                   // Tremolo
    p.vca.patchLevel = 1.05f; // level-matched across the bank
    return p;
}
