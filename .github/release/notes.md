**Release candidate 3.** It is feature-complete for this version and ready for wider testing. Please report problems in [Issues](../../issues).

### New in RC3

- **About Patch.** Every patch now records its **author**, **description** and **tags** (plus the date it was first saved and the KMISynth version) inside the `.kmip` file. The new **About...** button shows them, with an **Edit** button to change them. **Save...** asks for any that are missing before you pick the file, and remembers your name for new patches. The preset browser shows the description and author, searches by author, and its right-click **Edit info...** edits them in the file.
- **Pages remember their tab.** Leaving and returning to a page brings back the oscillator, envelope, LFO or sequencer track you were on, and the editor reopens on the page you left.

### New in RC2

- **Redesigned editor layout.** Panels fill each row with no ragged gaps, controls line up on a common grid, and the page tabs and pickers are segmented bars. The Filters / VCA page now lets you edit the three filter and VCA envelopes, the Effects page shows the chain in signal order above an output scope, and the Envelopes and Mod Matrix pages use the full width.
- **Drag handles on the graphs.** Shape any envelope (delay, attack, hold, decay/sustain, release, and Level on the shared envelopes) and any LFO (level, phase, delay, fade-in) directly on its graph.
- **LFO preview.** Each LFO page draws three cycles of the LFO as it will play from a note-on.
- **Irrelevant controls are greyed out.** Settings that do nothing because of another setting (Morph controls outside Morph mode, delay knobs while the delay is off, Loop Stages with Loop off, and many more) are disabled, so it is always clear what will make a difference.
- **Algorithm Morph** runs from 1 to the number of enabled morph slots.
- **Community patches.** The preset browser's **Community...** button downloads the [community patch library](https://github.com/KellyMurphy/KMISynth-UserPatches) into User > Community, and updates it on later clicks.

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
