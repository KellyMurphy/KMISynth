**Release candidate 1.** It is feature-complete for this version and ready for wider testing. Please report problems in [Issues](../../issues).

KMISynth is an original hybrid algorithmic polyphonic synthesizer: 8 multi-mode oscillators per voice, 148 morphable algorithms, 322 wavetables (including 17 morphing families made for Wave Scan), a digital multimode filter into a modelled analog low-pass, deep modulation, an arpeggiator, a step sequencer, scales and microtuning, and 256 factory presets (each with an MP3 demo in the README).

### Downloads

| File | What it is |
|---|---|
| `KMISynth-…-macOS.zip` | Audio Unit, VST3 and Standalone app; one universal build for Apple Silicon and Intel Macs (macOS 11+) |
| `KMISynth-…-Windows-Setup.exe` | Windows installer: VST3 and Standalone app (Windows 10+, 64-bit) |
| `KMISynth-…-Windows.zip` | The same Windows files, to install by hand |

Each zip has an `INSTALL.txt` with step-by-step instructions.

### Before you install

- **Not signed:** the builds are not signed with an Apple Developer ID or a Windows code-signing certificate.
  - On macOS, run the `xattr` commands in `INSTALL.txt` once after copying, or macOS will refuse to load the plugins.
  - On Windows, SmartScreen may warn: choose *More info*, then *Run anyway*.
- **Checked automatically, not by hand in a DAW:** the builds pass the engine test suite, pluginval at strictness 10 (VST3 on both platforms, AU on macOS) and Apple's `auval`. They have not yet been tried by hand in every DAW. The Mac builds in particular are new in this release.
- **Apple Silicon:** on Apple Silicon Macs the engine uses its portable code path (SSE on Intel), so expect somewhat higher CPU use there than on an Intel or Windows machine of similar speed.

### License

GNU AGPLv3. All sounds, presets, algorithms and wavetables are original.
