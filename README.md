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

## Layout

| Path | What |
|---|---|
| `engine/` | Framework-agnostic C++20 DSP engine (`kmi::Engine`). No JUCE dependency. |
| `plugin/` | JUCE wrapper: parameters (`Parameters.cpp` is the single parameter description), MIDI and state. |
| `tests/` | Offline engine tests and a CPU benchmark (`kmi_engine_tests`, run by ctest). |
| `docs/` | Requirements. |

## Status: Phase 1 (engine MVP)

Done:

- **Voices:** 16 voices × 8 oscillators.
- **Oscillators:**
  - All 7 modulation modes: Phase Mod, Freq Mod, PW Mod, Hard Sync, PD Square/Saw/Saw-Pulse.
  - Semitone, Ratio and Frequency pitch modes.
  - Feedback, keytrack, phase, velocity and poly aftertouch.
  - Direct Out, mute.
  - 20 band-limited wavetables.
- **Algorithms:** 20 preset algorithms, Custom (8×8 matrix) and Morph across 8 slots.
- **Filters:** 18 digital filter types, the analog-model ladder with pre-drive, and gain staging (OSCs level, filter level, VCA level, patch level).
- **Envelopes:** 13 per voice, DAHDSR with curves, Fast/Slow, tempo sync, loops, and Legato/Reset/Freerun.
- **LFOs:** 5, with Poly/Single/Free trigger, delay/fade, step mode, one-shot, smoothing and phase stagger. Plus a vibrato LFO.
- **Modulation:** 32-slot mod matrix (with modulation of route depths) and 8 macros with 8 routes each.
- **Voice modes:** Poly Rotate/Reassign, Mono, Mono Lo/Hi, Unison, Unison Lo/Hi, Unison Poly. Voice limit, glide and glissando, sustain pedal, analog drift.
- **Plugin:** 2× oversampling, sample-accurate MIDI, poly aftertouch, state save/load, generic host editor.

Next:

- FX chain: Pre-FX → Delay → Reverb → Post-FX.
- CPU optimisation. The benchmark currently uses about 27% of one core for a full 16 × 8 patch; the target is 25% *including* FX.
- Preset algorithm library from 20 to 64.
- Custom UI.
- pluginval/validator in CI.

### Implementation notes / deviations from the requirements

- **LFO rate** uses one continuous 0–150 Hz range instead of separate Fast/Slow ranges.
- **Envelope Slow mode** stretches the time parameters, which show their Fast-mode value.
- **Envelope BPM sync** snaps each segment to the nearest note division, so there are no separate division parameters.
- **Oscillator evaluation order** is 8 → 1. A modulator with a higher number is read from the current sample. A modulator with a lower number (custom algorithms only) is read from the previous sample.
- **PW Mod / PD / Sync** use our own phase-warp formulation. The modulator's pitch sets how often the process happens per carrier cycle, and its level sets the amount.
- **Not yet implemented:** oscillator Keyscale, envelope trigger sources other than note-on, VoiceMod offsets, Osc Stereo, Panner and Bit Depth/Vintage (all Phase 2).

## License

KMISynth is licensed under the [GNU Affero General Public License v3.0](LICENSE). It is built on [JUCE](https://juce.com), which is used under its AGPLv3 licence.
