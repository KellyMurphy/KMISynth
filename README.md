# KMISynth

A hybrid algorithmic polyphonic synthesizer (VST3, Windows). Its architecture follows the ASM Leviasynth: 8 multi-mode oscillators per voice, morphable algorithms, a digital multimode filter feeding a modelled analog 4-pole low-pass, and deep modulation. All content is original. See [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md).

## Build

You need Visual Studio 2022 with the C++ workload. Its bundled CMake is enough. JUCE 8.0.15 is fetched automatically on the first configure.

```powershell
.\scripts\build.ps1            # Release: VST3 + Standalone + tests
.\scripts\build.ps1 -Test      # build, then run the engine tests
.\scripts\build.ps1 -EngineOnly -Test   # DSP only, no JUCE (fast iteration)
```

Outputs are in `build\plugin\KMISynth_artefacts\Release\`:

- `VST3\KMISynth.vst3`: copy it to `C:\Program Files\Common Files\VST3` to load it in a DAW.
- `Standalone\KMISynth.exe`

### Installer

```powershell
.\scripts\make-installer.ps1   # needs Inno Setup 6; writes build\installer\KMISynth-<version>-Windows-Setup.exe
```

The installer puts the VST3 bundle in `Common Files\VST3` and, optionally, the standalone app in `Program Files\KMISynth` with a Start-menu entry. It never touches your presets in `Documents\KMISynth`, including on uninstall.

### Continuous integration

`.github/workflows/ci.yml` runs on every push and pull request (Windows, VS 2022). It:

1. builds Release;
2. runs the engine tests through ctest;
3. validates the VST3 with pluginval at strictness 10;
4. builds the installer;
5. uploads the VST3, the installer and the pluginval logs as artifacts.

## Layout

| Path | What |
|---|---|
| `engine/` | Framework-agnostic C++20 DSP engine (`kmi::Engine`). No JUCE dependency. |
| `plugin/` | JUCE wrapper: parameters (`Parameters.cpp` is the single parameter description), MIDI and state. |
| `tests/` | Offline engine tests and a CPU benchmark (`kmi_engine_tests`, run by ctest). |
| `docs/` | Requirements. |

## Status: Phase 1 complete (engine, effects, custom UI); Phase 2 under way

Done:

- **Voices:** 16 voices × 8 oscillators.
- **Oscillators:**
  - All 7 modulation modes: Phase Mod, Freq Mod, PW Mod, Hard Sync, PD Square/Saw/Saw-Pulse.
  - Semitone, Ratio and Frequency pitch modes.
  - Feedback, keytrack, phase, velocity and poly aftertouch.
  - Direct Out, mute.
  - 50 original band-limited wavetables, mip-mapped per octave, in named groups (Basic, Analog, Additive, Shaped, Digital, FM). The picker lists them under group headings.
- **Algorithms:** 64 original preset algorithms (stacks, trees, fans, rings and dense networks), Custom (8×8 matrix) and Morph across 8 slots. Direct Out adds any oscillator to the output on top of the selected algorithm.
- **Filters:** 18 digital filter types, the analog-model ladder with pre-drive, and gain staging (OSCs level, filter level, VCA level, patch level).
- **Envelopes:** 13 per voice, DAHDSR with curves, Fast/Slow, tempo sync, loops, and Legato/Reset/Freerun.
  - Up to 4 trigger sources each: Note On, LFO 1–5 cycle, Sustain pedal down, a MIDI CC crossing a threshold, and a Tap button on the Envelopes page.
  - An envelope without Note On waits for its other triggers. If that is the VCA envelope, the note stays silent until it is triggered.
- **LFOs:** 5, with Poly/Single/Free trigger, delay/fade, step mode, one-shot, smoothing and phase stagger. Plus a vibrato LFO.
- **Modulation:** 32-slot mod matrix (with modulation of route depths) and 8 macros with 8 routes each.
- **Voice modes:** Poly Rotate/Reassign, Mono, Mono Lo/Hi, Unison, Unison Lo/Hi, Unison Poly. Voice limit, glide and glissando, sustain pedal, analog drift.
- **Voice features:**
  - **Keyscale** per oscillator: a breakpoint, plus depth and curve (exp/lin/log) below and above it. Full depth reaches silence 4 octaves from the breakpoint.
  - **Osc Stereo:** each note plays on a left/right voice pair, which halves polyphony. Oscillator Pan (also a mod destination) applies to carriers, modulators or both, and Carrier Detune spreads the two sides by up to ±20 cents.
  - **Panner:** 15 stereo-position sequences plus Random. Each new note takes the next position. Width and Sequential/Reset mode are adjustable.
  - **Voice Mod:** 16 per-voice offsets that feed the Voice Mod and Voice Mod+ sources.
  - **Bit Depth:** 4–24 bits, applied before the filters.
  - **Vintage Digital:** raw (aliasing) wavetables read without interpolation, for an early-digital character.
- **Arpeggiator:**
  - Modes: Up, Down, Up/Down, Down/Up, As Played, Random (no immediate repeats) and Chord.
  - 1 to 4 octaves, any tempo division from 1/64T to 48 bars, gate 5–100%, swing 50–75%.
  - Latch (a new chord replaces the latched one), the sustain pedal holds keys, and velocity can be as played or fixed.
  - Steps are sample accurate. While the host plays, they lock to its bar grid; a key pressed just after a grid line still catches that step. Otherwise the arp runs free from the first key.
  - Switching it on or off while holding keys hands them over cleanly.
- **Plugin:** 2× oversampling, sample-accurate MIDI, poly aftertouch, state save/load, undo/redo, `.kmip` preset files (XML).
- **MPE (lower zone):**
  - With **MPE** on (Voice page), channel 1 is global and channels 2–16 each carry one note.
  - A note's own pitch bend uses the **MPE Bend Range** (default 48 semitones).
  - Its channel pressure drives every Poly Aftertouch routing (oscillator levels, both filters, VCA, mod matrix) for that note only.
  - Its CC74 drives the new **Timbre** mod source. Outside MPE, CC74 drives Timbre for all notes.
  - Bends sent before a note-on apply to that note, and the same key held on two channels releases independently.
  - VST3 Note Expression is not supported yet. Hosts with MPE support send MPE instead.
- **MIDI learn:**
  - Right-click any knob, switch or choice and pick **MIDI Learn**, then move a controller (CC 0–119). Esc cancels.
  - Each parameter answers to one CC, and each CC drives one parameter; learning again moves the mapping.
  - Learned CCs still reach the mod matrix as CC sources.
  - Mappings are saved with the host session, not in presets, so changing presets keeps your controller layout. Remove them with **Forget MIDI CC** on the control, or with **Clear all MIDI mappings** in the preset menu.
  - The header shows the mapping count, and the knob menu shows the assigned CC.
- **Effects:** Pre-FX → Delay → Reverb → Post-FX.
  - Insert slots: Chorus, Flanger, Rotary, Phaser, Lo-Fi, Tremolo, EQ, Compressor (with tempo or note ducking), Distort.
  - Delay: Mono, Stereo, Ping-Pong, L-R-C, Reverse, with tempo sync.
  - Reverb: Hall, Plate, Room, Cloud, up to 90 s, with Freeze.
  - 16 FX mod-matrix destinations.
- **Custom UI:** scales from 75% to 200% with a fixed aspect ratio.
  - **Pages:** Home, Algorithm, Oscillators, Filters / VCA, Envelopes, LFOs, Mod Matrix, Macros, Effects, Voice.
  - **Algorithm page:** a live carrier/modulator diagram and a clickable 8×8 matrix. Editing switches the patch to Custom.
  - **Oscillators page:** single-oscillator editing, plus group edit (one parameter across all 8 oscillators, with their envelopes overlaid).
  - **Visual editors:** envelope drawings, a step-LFO editor (draw with the mouse), and an oscilloscope.
  - **Controls:**
    - Double-click resets a control. Ctrl-drag adjusts finely.
    - Right-click opens Reset, Copy/Paste and "Modulate with…", which adds a mod-matrix route.
    - Tooltips.
    - **Modulation rings:** a knob whose destination is routed in the mod matrix or a macro shows the reachable range as an orange outer arc. Bipolar sources (LFOs, pitch bend, Voice Mod, random) extend both ways. A dot follows the live modulated value of the newest voice.
  - **Voice page:** polyphony, glide, pitch/vibrato and the arpeggiator. The Home page has an Arp switch.
  - **Header:** preset name (opens the browser), previous/next factory preset, Init/Load/Save, Undo/Redo, voice count.
  - **Preset browser:**
    - Search matches name, folder, tags and description; every word must match.
    - A tag filter.
    - A folder tree: Factory and its categories, plus User with your own subfolders of `Documents\KMISynth\Presets`, and **New Folder**.
    - Double-click or Enter loads a preset; the browser stays open for auditioning. Esc closes it.
    - Right-click a user preset to edit its tags (stored in the `.kmip` file), show it in Explorer, or delete it (to the Recycle Bin).
  - **Keyboard:** on-screen, for playing without a controller.
  - Pages show only the controls that apply. For example, an effects slot shows the selected type's parameters, and an oscillator shows pitch controls for its pitch mode.
- **Factory presets:** 42 original demo patches, each tagged for the browser. The first 16 each show one part of the engine, the six Showcase presets combine features, two Arp presets demonstrate the arpeggiator, and ten more show the voice features, new waves and MPE, and the last eight show different ways to use the wavetables:

  | Preset | Category | Shows |
  |---|---|---|
  | Velvet Tines | Keys | Two FM pairs, velocity-scaled modulator envelopes, tremolo auto-pan |
  | Glass Carillon | Bells | Inharmonic ratios with partials that decay independently |
  | Drawbar Chapel | Organ | Eight Carriers as additive drawbars, key click, drive, rotary (wheel = fast) |
  | Rubber Floor | Bass | Three-operator stack with feedback, sub, mono legato glide, analog ladder |
  | Saw Pressure | Bass | PD Saw modulator that closes with its envelope |
  | Sync Sweeper | Lead | Hard Sync swept by envelope, LFO and mod wheel |
  | Narrow Gauge | Lead | PW Mod pulse-width movement |
  | Phase Brass | Brass | PD Saw ensemble, filter swell, delayed and faded LFO vibrato |
  | Slow Tide | Pad | Algorithm Morph across four algorithms, driven by an LFO |
  | Vowel Choir | Pad | Vowel filter swept by an LFO (wheel opens it) |
  | Kalimba Drop | Pluck | Short FM pluck with dotted echoes |
  | Harp Mist | Pluck | Resonant PD pluck with ping-pong delay |
  | Step Machine | Sequence | Tempo-synced step LFOs on the filter and FM depth (hold a note) |
  | Metal Rain | FX | Feedback ring of eight operators, S&H on modulator levels |
  | Unison Wall | Lead | Six-voice detuned unison |
  | Deep Field | Drone | Free-running LFOs on FM levels, long cloud reverb |
  | Clockwork Garden | Showcase | Custom algorithm with a feedback loop; looping tempo-synced envelopes tick 3 against 2; reverse delay |
  | Living Strings | Showcase | Unison Poly ensemble; per-note random pan and tuning; staggered free LFOs; release velocity sets the tail |
  | Pressure Organism | Showcase | Expression: channel pressure and the wheel scale a random-FM route (depth of depth); poly pressure; key scaling |
  | Fixed Point Kit | Showcase | Percussion from fixed-frequency modulators, a Direct Out noise source, a pitch envelope and the low-pass gate |
  | Glitch Conveyor | Showcase | Step LFO that advances one step per note, S&H on warp and lo-fi, glissando, tempo-ducked compressor |
  | Grand Tour | Showcase | Everything at once (see below) |
  | Pulse Lattice | Arp | Up/Down over two octaves in swung 16ths into a resonant ladder (hold a chord) |
  | Glass Stairs | Arp | Random three-octave arpeggio of glassy FM through long echoes (hold a chord) |
  | Stereo Glass | Keys | Osc Stereo electric piano: body left, tine right, carrier detune |
  | Wide Reed | Lead | Reed wave on a stereo voice pair, delayed vibrato, legato glide |
  | Bit Choir | Pad | Two choir waves at 10 bits, slowly trading places |
  | Retro Bells | Bells | Vintage Digital FM bells with a keyscaled top end |
  | Pan Pluck | Pluck | The Panner walks each new note across the stereo field |
  | Gated Pulse | Sequence | LFO 4 cycles retrigger the VCA envelope every 16th (hold a chord) |
  | Strata Strings | Pad | Voice Mod gives each of the 16 voices its own tuning and pan |
  | Scaled Organ | Organ | Organ waves with keyscale, drive and rotary |
  | Fold Bass | Bass | Folded sine under a saw stack, mono legato |
  | Pressure Glide | Lead | MPE: per-note bend, pressure and CC74 timbre (works on any keyboard) |
  | Wave Scanner | Pad | A wavetable scan built from layers: four tables cross-faded by two slow LFOs |
  | Partial Bloom | Pad | Additive tables (odd, even, prime, square partials) fading in one after another |
  | Formant Tines | Keys | Tables as modulators: Formant and Bell Partials phase-modulating sines |
  | Table Sync | Lead | Hard sync through shaped tables (Sine Fold, Pulse Train) |
  | Stepped Arp | Arp | Stair tables read raw with Vintage Digital at 8 bits, arpeggiated |
  | Grain Field | Drone | Grain-table layers on random LFOs over a Pulse Train sub |
  | Skew Bass | Bass | A Skewed Sine carrier bent further by a PD Saw-Pulse modulator |
  | Table Mallets | Pluck | Bump, Bell Partials and FM Hollow tables as body, strike and ring |

  **Grand Tour** uses the whole engine in one patch:
  - all 7 oscillator modes and all 3 pitch modes;
  - an algorithm morph across five algorithms that each voice drifts through;
  - both filters with envelopes, LFOs, keytracking and pressure;
  - a looping envelope, and an oscillator that follows a shared envelope;
  - all 5 LFOs (delayed and faded, step, S&H, staggered free-running);
  - depth-of-depth routes and FX destinations in the mod matrix;
  - Unison Poly with analog feel;
  - all 8 macros (Morph, Brightness, Motion, Space, Grit, Width, Snap, Echo);
  - the full Pre-FX > Delay > Reverb > Post-FX chain.

  A test checks that it still covers all of these.

  Every preset sets Macro 1 and Macro 2 to useful controls (for example Brightness, Sweep, Space). The bank is loudness-matched.
  The presets are defined in code (`engine/src/Presets.cpp`). The tests render each one and check it for clipping, silence and stuck tails.
- **Performance:**
  - The filter chains of four voices run together in SSE registers (`FilterBank4`, R-RT-6).
  - Scalar and SIMD paths share one templated implementation, and a test checks they match lane by lane.
  - The oscillators of the same four voices also run together (`Voice::renderOscBank`). Each voice first computes its modulation, envelopes, LFOs and VCA gain for the control block; the 8 oscillators then run for all four lanes at once, with masks for inactive oscillators and per-lane connection weights. A test checks it against the scalar reference kernel across every oscillator mode, custom back edges, per-voice morph, Osc Stereo, keyscale, bit depth and Vintage Digital. This cut 8-oscillator patches by about 30%.
  - A full patch (16 voices × 8 oscillators, 2× oversampling, all FX) uses about 14–15% of one core on a Ryzen 9 7940HS. The target is ≤ 25%.
  - Run `kmi_bench` for a per-configuration and per-component breakdown.

Next (Phase 2):

- Grow the wavetable set from 50 toward 300+. The bank builds in about 130 ms at startup today; an FFT build would keep a larger set fast.
- VST3 Note Expression. JUCE's VST3 wrapper does not implement it, so it needs changes to JUCE itself. Until then, MPE (done) covers expressive controllers; Cubase, Bitwig and other hosts send MPE to VST3 plugins.




### Demo renders

`kmi_render` plays a short phrase through every factory preset and writes 24-bit WAVs, so you can hear the presets without a DAW. It also prints peak, RMS and tail levels:

```powershell
.\build\tests\Release\kmi_render.exe "$env:USERPROFILE\Music\KMISynth demos"
```

### UI development aid

If `KMI_UI_SNAPSHOT_DIR` is set, the editor renders every page (plus a few non-default states) to PNG files in that folder. It also loads every factory preset through the plugin parameters and writes `presets-check.txt`, which confirms that each preset survives the round trip unchanged. It also writes `midi-check.txt`, which exercises MIDI learn (learn, relearn, cancel, session save/restore, preset changes) on a separate headless processor. Use it to review layout changes without clicking through the UI:

```powershell
$env:KMI_UI_SNAPSHOT_DIR = "$env:TEMP\kmi-snap"; .\build\plugin\KMISynth_artefacts\Release\Standalone\KMISynth.exe
```

Snapshot mode changes parameters for its second pass, so use it on a throwaway instance.

### Implementation notes / deviations from the requirements

- **LFO rate** uses one continuous 0–150 Hz range instead of separate Fast/Slow ranges.
- **Envelope Slow mode** stretches the time parameters, which show their Fast-mode value.
- **Envelope BPM sync** snaps each segment to the nearest note division, so there are no separate division parameters.
- **Oscillator evaluation order** is 8 → 1. A modulator with a higher number is read from the current sample. A modulator with a lower number (custom algorithms only) is read from the previous sample.
- **PW Mod / PD / Sync** use our own phase-warp formulation. The modulator's pitch sets how often the process happens per carrier cycle, and its level sets the amount.
- **Effects are global.** Their mod-matrix destinations take values from the most recently started voice, so per-voice sources such as poly LFOs behave as they would on a mono synth.
- **Unused modulators pause.** Envelopes and LFOs that nothing in the patch listens to are not ticked, which saves CPU. They resume from where they stopped if a route starts using them mid-note.
- **Compressor sidechain** offers Off, Tempo Duck and Note Duck. The hardware's CV and tap inputs don't apply to a plugin.
- **Global FX bypass** (R-FX-6) and the per-type FX preset templates (R-FX-5) are not done yet.
- **Not yet implemented:** oscillator Keyscale, envelope trigger sources other than note-on, VoiceMod offsets, Osc Stereo, Panner and Bit Depth/Vintage (all Phase 2).

## License

KMISynth is licensed under the [GNU Affero General Public License v3.0](LICENSE). It is built on [JUCE](https://juce.com), which is used under its AGPLv3 licence.
