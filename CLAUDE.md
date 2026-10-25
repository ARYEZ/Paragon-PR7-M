# Paragon PR7-M: notes for Claude

**Paragon PR7-M** is a **mastering** plugin (AGPLv3, JUCE, VST3). It's the fifth member of the Paragon PR7 family:

| Plugin | Repo |
|---|---|
| PR7-G (guitar) | `ARYEZ/Paragon-X1` |
| PR7-B (bass) | `ARYEZ/Paragon-PR7-B` |
| PR7-V (vocals) | `ARYEZ/Paragon-PR7-V` |
| PR7-C (channel strip) | `ARYEZ/Paragon-PR7-C` |

It was **seeded lean from PR7-C**: the build, test and release setup, the family look (finishes, knobs, buttons, meters, black menus), and a stereo processor with only INPUT, OUTPUT and the meters. **Nothing is designed yet.** The modules come from the owner's concept design and references, one at a time (see "Next").

## Why a separate plugin, not part of PR7-C

PR7-C runs on many tracks, so it adds no delay and stays light. Mastering runs once, on the whole mix, where a few milliseconds of delay is normal and buys look-ahead limiting, linear-phase EQ, more oversampling and multiband processing. It also needs loudness (LUFS) and true-peak metering. Keep PR7-M free to use latency (report it with `setLatencySamples`), and keep PR7-C's zero-latency design out of it where it would hold the sound back.

## What's here

- **The plugin:** true stereo (stereo in/out, or mono in/out). Identity: `Paragon PR7-M`, plugin code `Pr7m`, bundle `com.paragon.paragonpr7m`, maker code `Prgn`, VST3 category Fx / Mastering.
  - Parameters: `inputGain`, `outputGain` (−24 to +12 dB, 0 dB at the top of the knob).
  - Input and output peak meters (`LevelMeterSource`).
- **`source/ui/MainView`:** a **placeholder** panel on a fixed 1448 × 1086 canvas (PR7-C's size): the header (the PR7-M mark, "MASTERING", the finish gear), the body frame, an empty plate where the modules will go, and IN / OUT (INPUT, OUTPUT, four meters). When the concept arrives, **resize the canvas to the concept image's exact size** (so coordinates read straight off it) and build the panel to match.
- **Carried over from PR7-C** (see its CLAUDE.md for the history): `ParagonLookAndFeel` (incl. black menus and dialogs), `Finish` (Pearl White / White + Neon / Pearl Black; settings file "Paragon PR7-M"; Pearl Black is PR7-G's face gradient), `Controls` (`Knob`, `Led`, `LeverSwitch`, `RoundButton`, `GainReductionMeter`), `LevelMeter` (orange-to-pink LEDs), `LedDisplay`, `Logo` (now "-M"), `dsp/EffectUtils.h` (`BypassFade`), `dsp/Biquad.h`.
- **`reference/pr7c/dsp/`:** PR7-C's EQ, compressor, limiter, saturation, console colour and stereo tools. **Not compiled.** Starting points to copy into `source/dsp/` and adapt when a module needs them (the limiter would want look-ahead here).

## Next (waiting on the owner)

1. **A concept image** (`docs/concept.png`).
2. **References** for the feel they want (they mentioned a commercial all-in-one mastering plugin with an assistant). Describe everything in our own words.
3. **The module list and build order**, agreed with the owner, as PR7-C's was.
4. **Metering calibration:** `LevelMeter` still uses PR7-C's console calibration (0 on the scale = −20 dBFS). Mastering usually wants dBFS, plus LUFS and true peak. Decide with the owner.

## Names

**No trademarked names or branding** on our modules or controls: not the reference product's name (nor anything that sounds like it), not its maker's. Describe things as "inspired by" or by what they do. The owner's reference manuals are **not** committed: they're someone else's copyrighted documents.

## Build, test, check

- **Build:** `cmake -B build && cmake --build build -j8`. JUCE 9.0.4 and Catch2 are fetched by CMake.
- **Tests:** `xvfb-run -a build/tests/ParagonPR7MTests`. All must pass. **Test names must be plain ASCII** (no Ø, ∞, ±): Windows mangles them on CI. New DSP gets tests that measure what it does to the sound, and that defaults pass audio through untouched (bit for bit).
- **pluginval:** `xvfb-run -a <pluginval> --strictness-level 10 --validate-in-process --validate "build/ParagonPR7M_artefacts/Release/VST3/Paragon PR7-M.vst3"`. Check the **exit code**. CI uses the official pluginval v1.0.4 release.
- **Renders:** `PARAGON_SNAPSHOT=out.png xvfb-run -a build/tests/ParagonPR7MTests "[snapshot]"`; `PARAGON_SNAPSHOT_WIDTH=2896` for 2×, `PARAGON_SNAPSHOT_FINISH=black|neon` for the finish. Look, crop and zoom before claiming a visual change is right.
- **Real-time safety:** no allocation or locks on the audio thread. Smooth every parameter so changes don't click.
- **Parameters:**
  - IDs live in `source/Parameters.h` and are saved in projects: never rename a shipped one. Choices are saved by **index**: only append. Each new parameter gets the next version hint.
  - Stepped ranges use `steppedRange` (snaps to whole steps counted from 0), never `NormalisableRange (min, max, step)`: JUCE counts its steps from the minimum, and a fused multiply-add (Apple silicon) lands a hair off, so 0 isn't exactly 0. A test checks every default of 0 reads back exactly.
  - To check bit-exact tests the way macOS runs them, build the tests separately with `-DCMAKE_CXX_FLAGS="-march=haswell -ffp-contract=fast"`, in a separate build folder with its own `FETCHCONTENT_BASE_DIR`.
- **CI:** after every push, check the run went green on all three systems. A red macOS or Windows run blocks the release, and nothing else says so.

## Releases

- **Version 0.0.0 is never released** (the seed; CI skips it). The first real release is whenever the owner asks.
- To release: bump `project(ParagonPR7M VERSION x.y.z)` in CMakeLists.txt, add a matching `## x.y.z` section at the top of CHANGELOG.md, and push. CI builds Windows, macOS and Linux, runs the tests and pluginval, and publishes a GitHub Release with the three zips, using that changelog section as the notes. About 15 minutes later, check all three zips are there.
- The owner installs from GitHub Releases on **Windows** and works in **BeatMaker**.

## How the owner likes to work

- **Explain the why**, specifically: what was wrong, what changed, and the trade-offs. Plain language; the owner is a musician, not a DSP engineer.
- **Be honest about limits:** say when something is our design rather than a copy, when a detail is a guess, and what can't be checked here (audio by ear). Ask for source material rather than invent specifics.
- **Show it:** a rendered screenshot of every visual change.
- One module at a time: built, tested, rendered, and shown before moving on.
- When fixes are ready during the owner's testing, ask whether to release them straight away.
- **"merge it"** means: open a PR from the working branch to main and merge it (merge commit), then reset the working branch to the new main.
- Commit messages explain what and why.
