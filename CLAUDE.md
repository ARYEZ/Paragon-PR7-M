# Paragon PR7-M: notes for Claude

**Paragon PR7-M** is a **mastering** plugin (AGPLv3, JUCE, VST3). It's the fifth member of the Paragon PR7 family:

| Plugin | Repo |
|---|---|
| PR7-G (guitar) | `ARYEZ/Paragon-X1` |
| PR7-B (bass) | `ARYEZ/Paragon-PR7-B` |
| PR7-V (vocals) | `ARYEZ/Paragon-PR7-V` |
| PR7-C (channel strip) | `ARYEZ/Paragon-PR7-C` |

It was **seeded lean from PR7-C**: the build, test and release setup, the family look (finishes, knobs, buttons, meters, black menus), and a stereo processor that passes audio through. The panel (step 1's layout) is drawn; the sound is built one module at a time (see "The design" and "Build order").

## Why a separate plugin, not part of PR7-C

PR7-C runs on many tracks, so it adds no delay and stays light. Mastering runs once, on the whole mix, where a few milliseconds of delay is normal and buys look-ahead limiting, linear-phase EQ, more oversampling and multiband processing. It also needs loudness (LUFS) and true-peak metering. Keep PR7-M free to use latency (report it with `setLatencySamples`), and keep PR7-C's zero-latency design out of it where it would hold the sound back.

## What's here

- **The plugin:** true stereo (stereo in/out, or mono in/out). Identity: `Paragon PR7-M`, plugin code `Pr7m`, bundle `com.paragon.paragonpr7m`, maker code `Prgn`, VST3 category Fx / Mastering.
  - Parameters so far: `panel.bypass` (version hint 1), `dyn.volume` (2; 0 to 15 dB in 0.1 steps, default 0). Each step adds its own. Input and output peak sources (`LevelMeterSource`) are kept (no meter uses them on the panel now).
  - **Signal path in `processChunk`:** the input is copied to `dry` and delayed by the chain's latency (`effects::FixedDelay`), the modules run in place (so far VOLUME's limiter), then `effects::MatchedBypass` (`dsp/MatchedBypass.h`) mixes, then the **output** is metered (what's heard). Hosts sending bigger blocks than `prepareToPlay` said are split into chunks. A module that adds latency must add it to `setLatencySamples` and to `dryDelay`.
  - **VOLUME (`dsp/Limiter.h`, `effects::TruePeakLimiter`):** gain (0 to +15 dB, smoothed 50 ms) into a look-ahead true-peak limiter, ceiling **−1 dBTP** (aims at −1.06 for what detection misses), always on (a mix peaking over −1 dBTP is caught at VOLUME 0). Detection is `TruePeak::next` at **8×** (the meter stays 4×, the standard's); a hold over the 3 ms look-ahead + 1 sample, a 100 ms release, then a 3 ms moving average, applied to audio delayed by `lookAhead − 1 + TruePeak::delay` (16) samples: 159 at 48 kHz, fixed whatever VOLUME is, reported from the constructor (48 kHz guess) and in `prepareToPlay`. Stereo linked. Gain is exactly 1 with nothing to limit, so audio under the ceiling comes out bit for bit, only later. Measured with a 16× reference: music-like material (content to 20 kHz) stays under −1.0 dBTP; full-band white noise to Nyquist slips ~1 dB past (a known limit; a 4× meter still reads it under). Under 1% of a core. Its gain reduction drives VOLUME's LEDs (`getVolumeReduction`).
  - **BYPASS at matched loudness:** measures the short-term loudness of `dry` and of the processed mix and, bypassed, plays `dry` turned to the processed loudness (20 ms crossfade, match smoothed over 0.5 s, held below −60 LUFS, ±24 dB, a boost capped so `dry`'s peaks over the last 3 s stay under −0.3 dBFS). The modules keep running while bypassed. A 0 dB match is exactly 1: with nothing processing, bypass is bit for bit. The host's own bypass is left as JUCE's plain bypass (not linked to `panel.bypass`).
  - **Metering (`dsp/Loudness.h`, `dsp/TruePeak.h`, `dsp/VuBallistics.h`):** BS.1770-4 K-weighting and short-term loudness (3 s, 100 ms steps; mono counts as one channel); true peak, 4× oversampled below 96 kHz (2× to 192 kHz), 32 taps per phase; the VU needle as a spring (damping 0.8127, 13.51 rad/s: 99% in 300 ms, 1.25% overshoot), run per channel at the sample rate, the louder side shown, scaled so a sine reads its peak. Handed to the UI by atomics (`getVuLevel`, `getShortTermLufs`, `getTruePeak` via `HeldPeakSource`, cleared on a click and when the host starts playing).
  - **0 VU reference:** −18, −14 (default) or −10 dBFS, in the gear menu, saved in the project (a property of the parameter state, `vuReference`, not a host parameter). Printed small on the VU face.
  - **RESET** (`resetKnobs`): every parameter but `panel.bypass` back to its default, each as a host gesture.
- **`source/ui/MainView`:** a fixed **1448 × 640** canvas (the header, then the faceplate `MainView::faceplate()`), scaled by the editor. Every control of the agreed layout is placed, **inert** until its step: tooltips say which step brings it. Controls are registered by ID (`tone.air`, `tone.focus`, `tone.body`, `dyn.push`, `dyn.volume`, `panel.reset`, `panel.bypass`, `stereo.width`, `dyn.transients`, `colour.analog`, `dyn.bassPunch`).
  - **`VuMeter`**: cream face, −20 to +3 VU on a real VU's voltage spacing, red from 0 up, needle, glass. Shows the **output** (owner decision). It only draws (`setNeedle`, `setReference`); the ballistics are the processor's. `MainView::updateMeters` (60 Hz timer; tests call it directly, since JUCE's timer thread can't be relied on in tests) reads the processor and lets the needle fall by itself if the host stops sending audio.
  - **`LoudnessDisplay`** (`meter.loudness`), under the VU: short-term LUFS on the left, max true peak (dBTP, red above −1) on the right, seven-segment. A click clears the true peak.
  - **Levers:** BYPASS up = bypassed (its tip glows), attached to `panel.bypass`. RESET is momentary: up, then back down after 250 ms.
  - **PUSH and VOLUME's green LED rings** (`setGainReduction`): 12 LEDs, 1 per dB, on the knob's dark rim along the top end of its travel, drawn over the knob (`paintOverChildren`).
  - **VOLUME's knob** is attached to `dyn.volume` (double-click: 0). The other knobs are still inert.
  - `PARAGON_SNAPSHOT_DEMO=1` sets the needle, the readout (−9.8 LUFS, −0.3 dBTP), VOLUME (6 dB) and the LEDs for a render.
- **Carried over from PR7-C** (see its CLAUDE.md for the history): `ParagonLookAndFeel` (incl. black menus and dialogs), `Finish` (Pearl White / White + Neon / Pearl Black; settings file "Paragon PR7-M"; Pearl Black is PR7-G's face gradient), `Controls` (`Knob`, `Led`, `LeverSwitch`, `RoundButton`, `GainReductionMeter`), `LevelMeter` (orange-to-pink LEDs; not on the panel now), `LedDisplay`, `Logo` (now "-M"), `dsp/EffectUtils.h` (`BypassFade`), `dsp/Biquad.h`.
- **`reference/pr7c/dsp/`:** PR7-C's EQ, compressor, limiter, saturation, console colour and stereo tools. **Not compiled.** Starting points to copy into `source/dsp/` and adapt when a module needs them (the limiter would want look-ahead here).

## The design (agreed with the owner)

The owner wants PR7-M to work like a well-known all-in-one mastering processor: a few big, musical knobs instead of technical ones. They supplied its front panel and its manual's one-page description. **We take the idea, the layout and the plain control words; the sound is our own design** (we can't measure or hear the original), and its name, maker and look are never used. The panel is in the Paragon look, not the reference's orange faceplate.

**The layout** follows the reference: AIR, FOCUS and BODY down the left; big **PUSH** and **VOLUME** either side of the VU meter, with **RESET** and **BYPASS** levers above them; **WIDTH** under the meter; **TRANSIENTS**, **ANALOG** and **BASS PUNCH** down the right; "MASTERING PROCESSOR" and the PR7-M mark low on the plate.

**The controls** (the reference manual's intent in our words, then our plan; ranges from its panel):

| Control | Range | Intent | Our plan (to design and test) |
|---|---|---|---|
| **AIR** | −15 to +15 | Boost or cut the highs, gently | A soft high shelf around 10–16 kHz |
| **FOCUS** | −15 to +15 | The intelligibility of the mids | A broad presence band around 1–4 kHz |
| **BODY** | −5 to +5 | The amount of lows without upsetting the overall balance | A balanced tilt in the low mids (around 150–300 Hz), level-compensated |
| **BASS PUNCH** | 0 to 10 | Low-end boost and tightness at once | A low boost plus a low-band compressor (below ~120 Hz) on one knob |
| **TRANSIENTS** | −10 to +10 | Enhance or smooth the attacks | A transient shaper |
| **PUSH** | 0 to 15 | A macro: all the compressor's settings, gentle to hard | A glue compressor whose threshold, ratio, attack and release move together along one curve; the green LEDs show its gain reduction |
| **ANALOG** | 0 to 10 | Harmonics for dimension and colour | Saturation (from PR7-C), gentler for a full mix |
| **WIDTH** | −5 to +5 | The stereo image | Mid/side width, the bass always kept mono |
| **VOLUME** | 0 to 15 | Louder without artifacts | Gain into a look-ahead **true-peak limiter** (latency reported); the green LEDs show its gain reduction |
| **VU** | −20 to +3 | Output level | The output, with VU ballistics; LUFS and true peak to add |
| **BYPASS** | lever | Compare | Bypass at **matched loudness** |
| **RESET** | lever | Start over | Every knob back to its starting point |

**Signal chain:** input → BODY / FOCUS / AIR → BASS PUNCH → TRANSIENTS → PUSH → ANALOG → WIDTH → VOLUME (limiter) → output and meters.

**Later:** an assistant that listens to the song and sets the knobs (owner: "assistant later").

## Build order

One step at a time, each built, tested, rendered and shown to the owner first.

1. **The panel (approved) and the metering:** the VU needle's movement, LUFS and true-peak readouts, BYPASS (matched loudness) and RESET. Built; awaiting the owner's listening test.
2. **VOLUME:** the look-ahead true-peak limiter. Built; to show the owner.
3. **PUSH:** the glue compressor macro.
4. **BODY, FOCUS, AIR.**
5. **BASS PUNCH, TRANSIENTS.**
6. **ANALOG, WIDTH.**
7. **Presets.**
8. **The assistant** (later).

## Decisions (step 1, agreed with the owner)

- **0 VU** is choosable (−18 / −14 / −10 dBFS), −14 by default, saved with the project.
- **The readout** is an LED strip under the VU: **short-term LUFS** and **max true peak** only (not integrated, momentary or LRA).
- **BYPASS** matches loudness automatically; **RESET** is momentary, no confirmation.

## Decisions (step 2, our defaults, to confirm with the owner)

- **VOLUME** 0 to 15 = 0 to +15 dB into the limiter.
- **Ceiling** fixed at −1 dBTP (streaming's recommendation; the readout's red line).
- **The limiter is always on**, even at VOLUME 0 (switching it off would change the latency).

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

- **Version 0.0.0 is never released** (CI skips it). 0.1.0 (a panel preview, knobs inert) was released so the owner could see the panel in BeatMaker; 0.2.0 (step 1: the metering, BYPASS and RESET; knobs still inert) for the owner to try them.
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
