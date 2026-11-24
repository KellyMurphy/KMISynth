# KMISynth: Requirements Specification

| | |
|---|---|
| **Product** | KMISynth, a hybrid algorithmic polyphonic synthesizer |
| **Format** | VST3 instrument plugin, Windows 10/11 x64 |
| **Reference design** | ASM Leviasynth (desktop/keyboard hardware, 2024–2025) |
| **Status** | Draft v0.1 (2026-11-23) |

---

## 1. Purpose and scope

KMISynth is a software instrument whose **synthesis architecture follows the ASM Leviasynth**:

- 8 multi-mode oscillators per voice, wired by selectable, morphable or user-built **algorithms**.
- A digital multimode filter feeding a modelled analog 4-pole low-pass.
- A deep modulation system: 13 envelopes, 5 LFOs, a 32-slot matrix and 8 macros.
- Pre-FX, delay, reverb and post-FX.

The reference numbers in this document come from the Leviasynth owner's manual. They are **targets for capability parity**. They are not a commitment to clone the product.

### 1.1 Originality / IP constraints (mandatory)

KMISynth is an *architecture-inspired* instrument, not a replica. The following must be **original work**:

- **R-IP-1** Product name, branding, UI layout and visual design. Do not use ASM, Leviasynth or Hydrasynth names, logos or trade dress.
- **R-IP-2** All waveforms. Design our own set of 300+ single-cycle waves. Do **not** copy ASM's wave tables or wave-group names.
- **R-IP-3** The preset algorithm library. Design our own algorithms. Do not transcribe ASM's 144 algorithm charts.
- **R-IP-4** Factory presets, arp phrases, microtuning scale content and documentation.
- **R-IP-5** Trademarked terms in the UI must be replaced with generic names. For example, "Polytouch" becomes "Poly Aftertouch", and "HTE Sync" becomes "Hard Sync" or a name of our own.

### 1.2 Out of scope (hardware-only features)

The following Leviasynth features have no meaning in a plugin and are excluded:

- CV/Gate/Clock I/O
- RGB pads
- Touch-sensitive encoders
- Rack/VESA mounting
- Analog-filter calibration
- MIDI Thru
- Two-unit "Overflow" voice linking
- Librarian app

The real analog filter and VCA become **DSP models** of them.

### 1.3 Priority key

- **P1 (MVP):** required for first playable release.
- **P2:** required for v1.0.
- **P3:** post-1.0 / stretch.

---

## 2. Platform and technical requirements

| ID | Requirement | Pri |
|---|---|---|
| R-PLT-1 | VST3 instrument (`kVstAudioEffectClass`, `Instrument\|Synth` subcategory), Windows 10/11 x64. | P1 |
| R-PLT-2 | Stereo output (main bus). No audio input required. | P1 |
| R-PLT-3 | Sample rates 44.1, 48, 88.2, 96, 176.4 and 192 kHz. Buffer sizes 16–4096, plus variable block sizes. | P1 |
| R-PLT-4 | C++20, CMake build. Framework: **JUCE 8** (recommended) or raw Steinberg VST3 SDK. See Open Question Q1. | P1 |
| R-PLT-5 | Standalone app build (same codebase) for testing and sound design. | P2 |
| R-PLT-6 | Passes Steinberg `validator` and `pluginval` at strictness level 8 or above. | P1 |
| R-PLT-7 | Tested hosts: Reaper, Ableton Live, Cubase, FL Studio, Bitwig. | P2 |
| R-PLT-8 | Installer (Inno Setup or WiX) installs to `C:\Program Files\Common Files\VST3`. | P2 |
| R-PLT-9 | Code signing of the installer and binary. | P3 |
| R-PLT-10 | CLAP and AU/macOS builds are possible later, so the DSP core must stay framework-agnostic. | P3 |

### 2.1 Real-time and performance

| ID | Requirement | Pri |
|---|---|---|
| R-RT-1 | No heap allocation, locks, file I/O or logging on the audio thread. | P1 |
| R-RT-2 | Denormal protection (FTZ/DAZ) on the audio thread. | P1 |
| R-RT-3 | Sample-accurate note events. Parameter changes smoothed with no zipper noise. Automation applied at a control rate of at most 32 samples. | P1 |
| R-RT-4 | Zero plugin latency at default settings (no lookahead). | P1 |
| R-RT-5 | **CPU target:** a full patch (16 voices × 8 oscillators, both filters, all FX) at 48 kHz/2× oversampling uses ≤ 25% of one core on a 2022-class desktop CPU. Validate in Phase 1 and revise if needed. | P1 |
| R-RT-6 | SIMD voice processing (SSE2 baseline, AVX2 path optional). | P2 |
| R-RT-7 | Selectable oscillator oversampling: 1×, 2×, 4×. Separate "render quality" for offline bounce. | P2 |
| R-RT-8 | Voices idle with zero cost once their VCA envelope and FX tails finish. | P1 |

---

## 3. Voice architecture

### 3.1 Polyphony and signal flow

| ID | Requirement | Pri |
|---|---|---|
| R-VA-1 | **16 voices** of polyphony in Single mode. Each voice has 8 oscillators, a digital filter, an analog-model filter, a VCA, 5 LFOs and 13 envelopes. | P1 |
| R-VA-2 | Polyphony is configurable upward (32/64) as a "CPU permitting" option. 16 is the default for parity. | P3 |
| R-VA-3 | Per-voice signal path (fixed order) is listed below the table. | P1 |
| R-VA-4 | Global path after voice summing is listed below the table. | P1 |

Per-voice signal path (R-VA-3):

```
Carriers (Σ, per-osc pan if Osc Stereo) → [OSCs Level] → [Bit Reduction, optional]
  → Digital Filter (drive pre/post) → [D.Filt Level, 64 = unity]
  → Analog-model Filter (Pre-Drive → 4-pole LP 24 dB/oct)
  → VCA (Env 3, LFO 3, Velocity, PolyAT, Initial Level) → Voice Pan / Panner
```

Global path after voice summing (R-VA-4):

```
Σ voices → [VCA Level] → Pre-FX → Delay → Reverb → Post-FX → [Patch Level, 64 = unity] → Out
```

### 3.2 Oscillators (×8 per voice)

| ID | Requirement | Pri |
|---|---|---|
| R-OSC-1 | Every oscillator is identical. Exception: Osc 1 is always routed to the output and has no Direct Out switch. | P1 |
| R-OSC-2 | **Mode** (how the oscillator affects the oscillators it modulates; it has no effect on its own sound). Seven modes, listed below the table. | P1 (PM, FM), P2 (rest) |
| R-OSC-3 | **Waveform:** 300+ original band-limited single-cycle waves in named groups. A per-oscillator **Invert** toggle. Classic shapes (sine, triangle, tri-saw, saw, square) are mandatory in P1. | P1 (classics), P2 (full set) |
| R-OSC-4 | **Pitch mode** per oscillator: three modes, listed below the table. | P1 |
| R-OSC-5 | **Initial Level** 0–128 and **Env Level** ±128. These sum and clamp to 0–128. For modulators, level means modulation index, pulse width, sync amount or PD amount, depending on mode. | P1 |
| R-OSC-6 | **Feedback** 0–100 (self-modulation). Active for PM/FM modulators and for carriers. Inactive for PWM/Sync/PD modes. | P1 |
| R-OSC-7 | **Keytrack** ±200% (pitch tracking). Default 100%; 0 means fixed pitch. | P1 |
| R-OSC-8 | **Phase** 0–360°: start phase applied on note-on. | P1 |
| R-OSC-9 | **Keyscale:** breakpoint C-1 to G9, lower/upper depth 0–100%, lower/upper curve ±64 (exp↔lin↔log). Controls amplitude across the keyboard. | P2 |
| R-OSC-10 | **Velocity → Initial Level** ±64 and **PolyAT → Initial Level** ±64. | P1 |
| R-OSC-11 | **Env Source:** the oscillator's own envelope, or Env 1/2/3. | P2 |
| R-OSC-12 | **Pan** ±64, visible when Osc Stereo is on (§3.7). | P2 |
| R-OSC-13 | **Mute** and **Solo** per oscillator (editing aids, not saved as sound changes). | P2 |
| R-OSC-14 | **Direct Out** (Osc 2–8): sends the *modulated* output of a modulator to the audio path as well, making it both carrier and modulator. | P1 |
| R-OSC-15 | **Random Phase** per voice (Voice module), simulating free-running oscillators. | P2 |
| R-OSC-16 | Anti-aliasing: band-limited wavetables (mip-mapped per octave) plus FM/PM oversampling. Aliasing is allowed only when **Vintage Digital** mode is on (§3.7). | P1 |

Oscillator modes (R-OSC-2):

- **Phase Mod**
- **Freq Mod** (linear, through-zero)
- **PW Mod** (pulse-width-style warp of any carrier wave)
- **Hard Sync** (modulator resets the carrier phase; the amount is set by level)
- **PD Square**
- **PD Saw**
- **PD Saw-Pulse** (phase-distortion functions applied to the carrier's read rate)

For PWM, Sync and PD, the modulator's pitch sets how many times per carrier cycle the process happens (0.125× to 64×). Its waveform is irrelevant.

Pitch modes (R-OSC-4):

- **Semitone:** ±36 st, plus Cent ±50.
- **Ratio:** 0.25–64.00, following the harmonic series plus sub-harmonics, with Fine giving ±1 octave.
- **Frequency:** fixed 0.00–10,000.00 Hz, with Fine 0.00–0.99 Hz.

### 3.3 Algorithms

| ID | Requirement | Pri |
|---|---|---|
| R-ALG-1 | The algorithm is an **8×8 modulation connection matrix** (modulator row → carrier column; no self-connection, because feedback is a separate parameter) **plus** a Direct Out vector. Any oscillator may modulate any other, cycles included. The evaluation order and one-sample delay on back-edges must be defined and deterministic. | P1 |
| R-ALG-2 | **Preset algorithm library:** original algorithms covering classic 6-op-style stacks, parallel "3-osc subtractive" layouts, split layers (e.g. 4+4) and complex networks. Target 64 in P1 and 140+ by v1.0. | P1/P2 |
| R-ALG-3 | **Algorithm modes** per patch: three modes, listed below the table. | P1 (Single, Custom), P2 (Morph) |
| R-ALG-4 | The morph position and the algorithm number are mod-matrix and macro destinations, and are host-automatable. | P2 |
| R-ALG-5 | **Algorithm view:** a graphical diagram (carriers on the bottom row, modulators stacked above) plus an editable matrix grid. Includes a solo-carrier-chain audition. | P1 |
| R-ALG-6 | A custom algorithm is saved with the patch. The user can also save custom algorithms to a user algorithm library. | P2 |

Algorithm modes (R-ALG-3):

- **Single:** pick a preset algorithm.
- **Custom:** free edit of the matrix and Direct Out. Editing a preset algorithm converts it to Custom.
- **Morph:** choose up to 8 preset algorithms (or Silence) in slots and morph continuously across them (position 1.00–8.00, at least 0.01 resolution). Morphing interpolates connection weights and carrier output gains.

### 3.4 Filters

| ID | Requirement | Pri |
|---|---|---|
| R-FLT-1 | **Digital filter: 18 types**, listed below the table. | P1 (SVF, ladder 12/24, 1-pole), P2 (all) |
| R-FLT-2 | Digital filter params: Cutoff 0–128, Resonance 0–128, Morph (SVF/Vowel) or Drive (others) 0–128, Drive Pre/Post, Env 1 Amt ±64, Velocity→Env ±64, PolyAT ±64, Keytrack ±200% (centre C2), LFO 1 Amt ±64, Output Level (64 = unity). Most types self-oscillate. | P1 |
| R-FLT-3 | **Analog-model filter:** a virtual-analog 4-pole 24 dB/oct low-pass with nonlinear, gain-compensated **Pre-Drive** (0–128, default 16). Self-oscillates around resonance 110/128. Optional component-tolerance variation per voice, linked to Analog Feel. | P1 |
| R-FLT-4 | Analog-model params: Pre-Drive, Cutoff, Resonance, Env 2 Amt ±64, Velocity→Env ±64, PolyAT ±64, Keytrack ±200% (centre C2, default 100%), LFO 2 Amt ±64. | P1 |
| R-FLT-5 | Filters are always in series (digital → analog). Each filter can be set wide open or bypassed. | P1 |

Digital filter types (R-FLT-1):

- 2-pole morphing state-variable, two variants: **LP-BP-HP** and **LP-Notch-HP**.
- **3 high-pass:** aggressive/diode-style, "boutique modular", 12 dB smooth.
- **2 band-pass:** modular, 12 dB.
- **10 low-pass:**
  - ladder 12 and 24 dB (uncompensated)
  - "fat" ladder 12 and 24 dB (bass-compensated)
  - LP gate
  - aggressive/diode-style
  - boutique modular
  - 12 dB SVF
  - 1-pole 6 dB
  - 8-pole 48 dB
- **Vowel/formant** with 8 vowel orders (cutoff controls the vowel; morph controls the formant character).

### 3.5 VCA and gain staging

| ID | Requirement | Pri |
|---|---|---|
| R-VCA-1 | Parameters: OSCs Level, D.Filt Level, VCA Level, Patch Level (64 = unity), LFO 3 Amt ±64, Velocity→Env 3 ±64, PolyAT ±64, Initial Level (a global VCA offset, i.e. a drone). | P1 |
| R-VCA-2 | Gain-staging meter view showing level at each of the 5 stages. | P2 |
| R-VCA-3 | **Snap:** off gives an "analog VCA" with slightly softened attack transients; on gives an instant digital VCA. | P2 |

### 3.6 Envelopes (13 per voice: 8 oscillator envelopes and Env 1–5)

| ID | Requirement | Pri |
|---|---|---|
| R-ENV-1 | **DAHDSR** with Delay, Attack, Hold, Decay, Sustain (0–128) and Release. | P1 |
| R-ENV-2 | **Speed mode** Fast/Slow. The ranges are listed below the table. | P1 |
| R-ENV-3 | **BPM sync** per envelope: 1/64T up to 64 quarters (Fast) or 192 quarters (Slow), with dotted and triplet values. | P1 |
| R-ENV-4 | Attack, Decay and Release **curves**, each ±64 (exp ↔ lin ↔ log). | P1 |
| R-ENV-5 | **Loop:** Off, 2–50 repeats, or Infinite. The loop range is Delay→Attack, Delay→Hold or Delay→Decay. | P2 |
| R-ENV-6 | Trigger behaviour: **Legato**, **Reset** and **Freerun**. | P1 |
| R-ENV-7 | Up to **4 trigger sources**, listed below the table. | P2 |
| R-ENV-8 | Velocity curve ±64, Velocity→Env ±64, a master **Level** 0–128 (Env 1–5), and **Quantize** (stepped output). | P1 (vel), P2 (rest) |
| R-ENV-9 | Fixed routings: Env 1 → digital filter, Env 2 → analog filter, Env 3 → VCA. Env 4–5 and all 13 envelopes are available as mod-matrix sources. | P1 |
| R-ENV-10 | Each envelope stage time, level and curve is a modulation destination. | P2 |
| R-ENV-11 | **Bias controls:** four global relative offsets (±128) applied to all 8 oscillator envelopes at once: Env Level, Attack, Decay, Release. Times scale multiplicatively. On save, the user can **convert** the bias into the stored values or **ignore** it. | P2 |

Envelope speed ranges (R-ENV-2):

| Stage | Fast | Slow |
|---|---|---|
| Delay | 0–32 s | 0–60 s |
| Attack, Hold | 0–36 s | 0–600 s |
| Decay, Release | 0–60 s | 0–900 s |

Envelope trigger behaviour (R-ENV-6):

- **Legato:** no retrigger until all notes are released.
- **Reset:** restart from zero when retriggered, rather than from the current level.
- **Freerun:** always run through the stages, even after an early release.

Envelope trigger sources (R-ENV-7): Off, Note On, LFO 1–5 cycle, Sustain pedal down, MIDI CC threshold, and a UI "tap trigger".

### 3.7 LFOs (5 per voice, plus a vibrato LFO)

| ID | Requirement | Pri |
|---|---|---|
| R-LFO-1 | Waveforms: Sine, Triangle, Saw Up, Saw Down, Square, Pulse 27%, Pulse 13%, S&H, Noise, Random (smooth), **Step**. | P1 |
| R-LFO-2 | Rate: **Slow** 0–25 Hz, **Fast** 5–150 Hz. BPM sync from 272 quarters to 1/64T. | P1 |
| R-LFO-3 | **Trig Sync**, with three options listed below the table. | P1 |
| R-LFO-4 | Delay and Fade-In (ranges as for envelopes), Phase 0–360°, Level 0–128, Smooth (slew) 0–127, Quantize. | P1 (delay, fade, phase, level), P2 (rest) |
| R-LFO-5 | **One-Shot:** Off, On (one cycle), or **Step** (advance one step per trigger). Steps 2–64. | P2 |
| R-LFO-6 | **Step LFO:** up to 64 user-drawn steps (±64), with an optional **Semi Lock** that snaps to semitones. Includes a step editor in the UI. | P2 |
| R-LFO-7 | Fixed routings: LFO 1 → digital filter, LFO 2 → analog filter, LFO 3 → VCA. Bipolar and unipolar ("+") source variants. | P1 |
| R-LFO-8 | **Vibrato LFO** (6th, global to all oscillator pitches), controlled by the mod wheel. Amount 0–12 st, Rate 0.3–10 Hz or synced, waves Sine/Tri/Square/S&H/Noise. | P1 |

LFO Trig Sync options (R-LFO-3):

- **Poly:** an LFO per voice, retriggered on note-on.
- **Single:** one shared LFO, retriggered on every note.
- **Off:** shared and free-running, with **Phase Stagger** 0–360° across voices.

### 3.8 Voice module

| ID | Requirement | Pri |
|---|---|---|
| R-VOI-1 | **Polyphony modes:** Poly Rotate, Poly Reassign, Mono, Mono Lo, Mono Hi, Unison, Unison Lo, Unison Hi, Unison Poly. | P1 (Poly, Mono, Unison), P2 (rest) |
| R-VOI-2 | **Voice Limit** 1–16 (Poly modes). **Density** 1–16 stacked voices and **Unison Detune** 0–127 (Unison modes). Unison Poly divides Density among the held notes. | P1 |
| R-VOI-3 | Voice stealing: oldest released voice first, then quietest, then oldest. Steal with a short fade (no clicks). | P1 |
| R-VOI-4 | **Glide:** Off, Glide or Glissando (glissando follows the selected scale). Time 0–127, Time vs. Distance mode, curve ±64, Legato-only option. | P1 (glide), P2 (gliss, curve) |
| R-VOI-5 | **Pitch bend range** 0–24 semitones. | P1 |
| R-VOI-6 | **Panner:** 16 stereo-position sequences (alternate, rotate and sides families, with mirror and reverse variants, plus Random), Width 0–127, and a Sequential/Reset mode. | P2 |
| R-VOI-7 | **Osc Stereo:** each oscillator becomes a stereo pair, which halves polyphony. Osc Pan applies to Carriers Only, Modulators Only or Both. Carrier Detune ±20 cents. | P2 |
| R-VOI-8 | **Analog Feel** 0–127: per-voice drift of pitch, filter cutoff, envelope times and so on. | P2 |
| R-VOI-9 | **VoiceMod:** a user-set offset per voice (16 values, ±) exposed as the *Voice Mod* / *Voice Mod+* mod sources. | P2 |
| R-VOI-10 | **Vintage Digital** mode (intentional 1980s-style aliasing/converter character) and **Bit Depth** reduction (down to 4 bits, before the filters). | P2 |
| R-VOI-11 | **Scales:** Chromatic, 38+ preset scales with Key Lock, a Custom 12-tone scale, and **microtuning**. Import Scala `.scl`/`.kbm`. MTS-ESP client support. | P2 (scales), P3 (MTS-ESP) |
| R-VOI-12 | Sustain pedal modes: Sustain, Sostenuto, Mod-Only. | P2 |
| R-VOI-13 | **Chord mode:** one key plays a stored chord. | P3 |

---

## 4. Modulation

### 4.1 Mod matrix

| ID | Requirement | Pri |
|---|---|---|
| R-MOD-1 | **32 routes** per patch. Each route has a Source, a Destination and a bipolar Depth (±128, fine resolution). | P1 |
| R-MOD-2 | Sources are listed below the table. | P1 |
| R-MOD-3 | Destinations: every continuous synthesis parameter. At minimum, the groups listed below the table. | P1 |
| R-MOD-4 | **Mod-of-mod:** any source can modulate the depth of any route. | P2 |
| R-MOD-5 | Quick-route creation in the UI (drag a source onto a control, or right-click "Modulate with…"). Copy/paste routes. Sort routes. | P2 |
| R-MOD-6 | Modulation is evaluated per voice at control rate (≤ 32 samples) with smoothing. Oscillator pitch/level from audio-rate sources is evaluated at audio rate. | P1 |

Mod sources (R-MOD-2):

- Env 1–5
- Osc Env 1–8
- LFO 1–5 (bipolar), plus LFO 1–5+ (unipolar)
- Keytrack
- Velocity On and Velocity Off
- Poly Aftertouch and Channel Aftertouch
- Mod Wheel and Pitch Bend
- Voice Mod and Voice Mod+
- MPE X, MPE Y (relative and absolute), MPE Z
- Expression and Sustain pedal
- Macros 1–8
- MIDI CC 0–127
- Random-per-note (new for KMISynth)

Mod destinations (R-MOD-3):

- **Osc 1–8, All Osc, Carriers, Modulators:** Initial Level, Env Level, Pitch, Ratio, Fine, Feedback, Phase, Pan, Wave, and the oscillator envelope ADHSR.
- **Digital filter:** Cutoff, Resonance, Morph, Drive, Env 1 Amt, LFO 1 Amt, Level.
- **Analog filter:** Cutoff, Resonance, Pre-Drive, Env 2 Amt, LFO 2 Amt.
- **VCA:** Level, LFO 3 Amt.
- **Env 1–5:** times, Sustain, Level, curves.
- **LFO 1–5:** Rate, Level, Smooth, Steps.
- **Voice:** Unison Detune, Pan, Analog Feel, Pitch Bend, Vibrato Amount, Vibrato Rate, Glide on/time/curve, Panner Width.
- **Algorithm:** Number, Morph.
- **FX:** each module's marked parameters and its Dry/Wet.
- **Arp parameters.**
- **Mod-route depth 1–32.**

### 4.2 Macros

| ID | Requirement | Pri |
|---|---|---|
| R-MAC-1 | **8 macros** per patch. Each has a knob value 0–128, up to **8 destinations** with an individual depth for each, and an 8-character name. | P1 |
| R-MAC-2 | Each macro also has a **button** with a "Button Value" per destination. The button can act as Trigger or Toggle. | P2 |
| R-MAC-3 | The 8 macros are exposed as fixed **host-automatable parameters** (stable IDs), plus host MIDI-learn and CC mapping. | P1 |

### 4.3 MIDI and expression

| ID | Requirement | Pri |
|---|---|---|
| R-MIDI-1 | Note on/off with velocity, release velocity, pitch bend, mod wheel, channel pressure and sustain (CC64). | P1 |
| R-MIDI-2 | **Polyphonic aftertouch** (VST3 `PolyPressureEvent`) driving every PolyAT parameter. | P1 |
| R-MIDI-3 | **MPE** mode (per-note pitch bend, timbre CC74 and pressure), plus VST3 **Note Expression** (tuning, volume, pan, timbre). | P2 |
| R-MIDI-4 | MIDI learn for any parameter. Map CC 0–127 as a mod source. | P2 |
| R-MIDI-5 | Host tempo, transport and PPQ position drive all synced elements (envelopes, LFOs, delay, arp, sequencer). | P1 |

---

## 5. Effects

| ID | Requirement | Pri |
|---|---|---|
| R-FX-1 | Fixed chain: **Pre-FX → Delay → Reverb → Post-FX**. Each slot has a bypass. | P1 |
| R-FX-2 | Pre-FX and Post-FX slot types are listed below the table. | P1 (Chorus, EQ, Distort), P2 (rest) |
| R-FX-3 | **Delay:** five types (Mono, Stereo, Ping-Pong/Pan, L-R-C, Reverse). Time 1 ms–3 s or synced 1/64T to dotted 1/1. Feedback 0–128, Wet Tone ±64 (LP↔HP), Feedback Tone, Dry/Wet. | P1 (Mono, Stereo, Pan), P2 (rest) |
| R-FX-4 | **Reverb:** four types (Hall, Plate, Room, Cloud/shimmer-diffuse). Pre-delay 0.5–250 ms, Time 120 ms–90 s plus **Freeze**, Tone ±64, Hi Damp, Lo Damp, Dry/Wet. | P1 (Hall, Room), P2 (rest) |
| R-FX-5 | Each type ships with 2–3 preset "templates" (starting settings). | P2 |
| R-FX-6 | Global FX bypass, which is a non-patch setting. | P2 |

Pre-FX and Post-FX slot types (R-FX-2):

| Type | Parameters |
|---|---|
| Chorus | Rate 0.02–10 Hz, Depth, Offset ±180°, Feedback ±63, Mono/Stereo, Dry/Wet |
| Flanger | Same parameters as Chorus |
| Rotary | Woofer/Horn rate, Lo/Hi depth, Balance |
| Phaser | Rate, Feedback, Depth, Phase, Offset |
| Lo-Fi | Filter model, Cutoff, Resonance, sample-rate reduction 44.1–2.7 kHz, Output |
| Tremolo | Rate, Depth, Sine/Square, L/R phase, Pitch Mod |
| EQ | 3-band, −36/+24 dB, two crossovers |
| Compressor | Ratio 1–20, Threshold, Attack, Release, Output. Sidechain: Off, or tempo duck |
| Distort | Drive, Tone ±63, Asymmetry, Curve, Output |

---

## 6. Arpeggiator and sequencer

| ID | Requirement | Pri |
|---|---|---|
| R-ARP-1 | **Arpeggiator:** Division (18 values, 64 quarters to 1/32T), Octave Range 1–6, Octave Mode (Up, Down, UpDown, Alt, Alt2, Random), Gate 5–150%, Mode, Length (Default or 1–32), Swing 25–75%, Step Offset ±64. | P2 |
| R-ARP-2 | Arp modes: Up, Down, Up/Down, Up&Down, Order, Random-Once, Random, Chord, Phrase. | P2 |
| R-ARP-3 | **Ratchet** (×1/×2/×4/×8 probability blend, 0–127) with **Chance** 0–100%. **Entropy Shuffle**: probabilistic step-variation rules (our own design). | P2 |
| R-ARP-4 | **Phrases:** 64 original factory phrases plus 64 user phrases. A phrase can be captured from a sequencer track. | P3 |
| R-ARP-5 | Latch, host sync, and an option to output arp notes as MIDI to the host (VST3 event output). | P2 |
| R-SEQ-1 | **Sequencer:** two polyphonic note tracks plus one 8-lane macro automation track per patch. Up to 128 steps. Per-step multi-trigger, drift and probability. Real-time and step record. Per-track length, rate and play mode. | P3 |

---

## 7. Patches and presets

| ID | Requirement | Pri |
|---|---|---|
| R-PRE-1 | A patch stores every sound parameter, the custom algorithm, the mod matrix, macros, FX, arp/sequencer data and the scale. Host project state is saved through VST3 `getState`/`setState` and is versioned for forward compatibility. | P1 |
| R-PRE-2 | Preset file format: human-readable JSON or XML, extension `.kmip`, with a schema version field. | P1 |
| R-PRE-3 | Browser features are listed below the table. | P2 |
| R-PRE-4 | Factory library: 8 banks of 128 slots. The initial target is ≥ 256 original presets at v1.0. | P2 |
| R-PRE-5 | **INIT** patch (user-definable default) and **Randomize** at four scopes: parameter, module, whole patch, and percentage-limited. | P2 |
| R-PRE-6 | Copy/paste of modules (oscillator → oscillator, envelope → envelope, LFO → LFO, mod route → route). Copying is refused across incompatible pitch modes. | P2 |
| R-PRE-7 | Undo/redo for edits. | P2 |

Browser features (R-PRE-3):

- Banks
- Categories (Bass, Lead, Pad, Keys, FX, etc.)
- Sound-designer tag
- Favourites
- Search
- Previous/next navigation

---

## 8. User interface

| ID | Requirement | Pri |
|---|---|---|
| R-UI-1 | Resizable, DPI-aware vector UI. Default 1280×800, scalable 75–200%. | P1 |
| R-UI-2 | Pages and panels are listed below the table. | P1 (Home, Algorithm, Osc, Filters, Env, LFO, Matrix, FX), P2 (rest) |
| R-UI-3 | Group-edit view: one parameter shown across all 8 oscillators, with an overlaid display of all 8 oscillator envelopes. | P1 |
| R-UI-4 | Real-time **oscilloscope** of the voice output, plus waveform previews. Envelope and LFO graphs show live playhead positions. | P2 |
| R-UI-5 | Every control has a tooltip/value readout, double-click to reset, Ctrl-drag for fine adjustment and a right-click menu (MIDI learn, modulate, reset, copy/paste value). | P1 |
| R-UI-6 | Visual indicators for active modulation on each control (ring display of the modulation range). | P2 |
| R-UI-7 | An on-screen keyboard with velocity and poly-aftertouch simulation, for the standalone build and for testing. | P2 |
| R-UI-8 | The UI thread never blocks audio. Communication uses lock-free FIFOs and atomics. | P1 |

UI pages and panels (R-UI-2):

- **Home:** macros, patch name, algorithm thumbnail, master controls.
- **Algorithm:** diagram plus 8×8 matrix, morph slots.
- **Oscillators:** per-oscillator detail view plus a **group edit** view (one parameter across all 8).
- **Filters**
- **VCA/gain staging**
- **Envelopes**
- **LFOs** (with step editor)
- **Mod Matrix**
- **Macros**
- **FX**
- **Voice**
- **Arp/Seq**
- **Settings**

---

## 9. Quality and verification

| ID | Requirement | Pri |
|---|---|---|
| R-QA-1 | Unit tests for DSP blocks (oscillator modes, filters, envelopes, LFOs) using deterministic offline render and golden-file or spectral checks. | P1 |
| R-QA-2 | CPU benchmark harness (worst-case patch) run in CI. Fails on regressions above 10%. | P2 |
| R-QA-3 | State round-trip test: save, load and compare bit-identical renders. | P1 |
| R-QA-4 | Automated pluginval and VST3 validator runs in CI (GitHub Actions, Windows runner). | P2 |
| R-QA-5 | No denormal spikes, NaN or Inf under fuzzed parameter automation (pluginval fuzz). | P1 |

---

## 10. Phasing (proposed)

1. **Phase 1, Engine MVP:**
   - VST3 shell
   - 16 voices × 8 oscillators (PM/FM, classic waves, all pitch modes)
   - Custom + 64 preset algorithms
   - Digital filter (core types) + analog-model filter
   - VCA, Env 1–5 + 8 oscillator envelopes, 5 LFOs
   - 32-slot matrix, 8 macros
   - Delay + reverb + one Pre/Post FX slot
   - Patch save/load
   - Basic UI
2. **Phase 2, Parity v1.0:**
   - The remaining oscillator modes (PWM/Sync/PD)
   - Full 300+ wave set
   - Algorithm morph, all 18 filter types
   - Envelope loops/triggers, Step LFO
   - Full voice module (panner, Osc Stereo, Analog Feel, VoiceMod, scales/microtuning)
   - All FX
   - Arpeggiator, MPE, preset browser, factory library, installer
3. **Phase 3, Beyond:**
   - Sequencer
   - Multi mode (two 8-voice layered/split parts with velocity/key crossfade)
   - Chord mode
   - MTS-ESP
   - Higher polyphony
   - CLAP/macOS

---

## 11. Open questions

- **Q1. Framework:** *Resolved (2026-11-24):* JUCE 8, used under AGPLv3.
- **Q2. Distribution:** *Resolved (2026-11-24):* open source under AGPLv3, in the public repository github.com/KellyMurphy/KMISynth.
- **Q3. Multi mode:** include it in v1.0 or defer it to Phase 3 (as proposed)?
- **Q4. Sequencer:** is it needed at all, given that the host DAW already sequences? (Currently P3.)
- **Q5. Analog filter fidelity:** zero-delay-feedback ladder with tanh saturation (cheaper), or a component-level model (more CPU)?
- **Q6. Name:** is "KMISynth" final?

---

## Appendix A: Reference specifications (ASM Leviasynth, from the owner's manual)

| Area | Reference value |
|---|---|
| Polyphony | 16 voices (Single). 2 × 8 (Multi). Halved with Osc Stereo |
| Oscillators | 8 per voice. 7 modes (PM, FM, PWM, HTE Sync, PD Square, PD Saw, PD Saw Pulse). 323 waveforms |
| Pitch modes | Semitone ±36. Ratio 0.25–64. Frequency 0–10 kHz |
| Algorithms | 144 presets, Custom 8×8, Morph across 8 |
| Filters | Digital, 18 types. Analog 4-pole LP 24 dB/oct with pre-drive, self-oscillating |
| Envelopes | 13 per voice (8 oscillator + 5), DAHDSR, curves, loop, 4 trigger sources, Fast/Slow, BPM sync |
| LFOs | 5 per voice + vibrato. 11 waves incl. 64-step Step LFO. Up to 150 Hz |
| Mod matrix | 32 routes. ~30 source types (+128 CCs). Several hundred destinations |
| Macros | 8 per patch (16 in Multi), 8 destinations each |
| FX | Pre/Post (9 types each), Delay (5 types), Reverb (4 types, to 90 s + Freeze) |
| Arp | 9 modes, ratchet/chance/entropy, 64 + 64 phrases, swing 25–75% |
| Sequencer | 2 poly note tracks + macro track, 128 steps |
| Patches | 1,024 Single (8 × 128). 640 Multi (5 × 128) |
| Scales | 38 presets + custom + 32 microtuning slots |
