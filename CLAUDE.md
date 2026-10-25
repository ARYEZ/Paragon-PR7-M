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
  - Parameters so far: `panel.bypass` (version hint 1), `dyn.volume` (2; 0 to 15 dB in 0.1 steps, default 0), `dyn.push` (3; 0 to 15 in 0.1 steps, default 0), `tone.air` (4) and `tone.focus` (5) (−15 to +15, default 0, on `markedRange`: straight between the marks printed evenly round the knob, `Parameters::toneMarks` −15, −10, −6, −3, 0, 3, 6, 10, 15, snapped to 0.1), `tone.body` (6; −5 to +5 in 0.1 steps, default 0), `dyn.bassPunch` (7; 0 to 10), `dyn.transients` (8; −10 to +10), `colour.analog` (9; 0 to 10), `stereo.width` (10; −5 to +5), all in 0.1 steps, default 0. Every knob now has its parameter. Input and output peak sources (`LevelMeterSource`) are kept (no meter uses them on the panel now).
  - **Signal path in `processChunk`:** the input is copied to `dry` and delayed by the chain's latency (`effects::FixedDelay`), the modules run in place in `effects::MasteringChain` (`dsp/Chain.h`: the tone, BASS PUNCH, TRANSIENTS, PUSH, ANALOG, WIDTH, then VOLUME's clipper and limiter; `Settings` holds the knobs; the assistant runs its own copy offline), then `effects::MatchedBypass` (`dsp/MatchedBypass.h`) mixes, then the **output** is metered (what's heard). Hosts sending bigger blocks than `prepareToPlay` said are split into chunks. A module that adds latency must add it to `setLatencySamples` and to `dryDelay`.
  - **VOLUME = `effects::VolumeClipper` (`dsp/Clipper.h`) then `effects::TruePeakLimiter`, clean first.** History: 0.3.0–0.5.0 (a slow limiter alone) "barely raises the volume"; 0.6.0–0.7.0 (a clipper doing the work from the start) "just causing overload now... can't get the master loud without it completely distorting". Measured (`VolumeTests.cpp`: `denseMix`, a dense test mix peaking at −1 dBFS, −15.5 LUFS; `residualDb`, the waveform change left after taking out the best gain per 1.3 ms, i.e. distortion): every limiter design tried (slow, fast, PR7-C AUTO, two bands with the lows limited apart) tops out cleanly at about −8.7 LUFS; more loudness only comes from cutting into the waveform. So: the limiter does the work through most of the travel (−8.7 LUFS by VOLUME 9, distortion −41 dB), then the clipper adds loudness and edge evenly: −8.2 LUFS at 10 (−31 dB), −7.5 at 12 (−26), −5.9 at 15 (−21). (0.7.0 was −30 dB by 6, −18 at 15.) The clipper does the gain (0 to +15 dB, smoothed 50 ms; the limiter's own gain stays 0 dB). Its law (`ceilingDbFor`): a −1 dBFS peak turned up by VOLUME goes 6 dB over its ceiling at 10 and 12 dB over at 15; below 10 the ceiling rises 2 dB a step (first touches such a peak at 7, inaudibly: shaving under ~6 dB of kick peaks changes neither loudness nor sound); 3 dB soft knee; continuous, falling. At 4× (JUCE FIR equiripple, integer latency 61): only the shaving (clipped − unclipped) is oversampled and added to the audio delayed to match, so nothing under the knee changes (bit for bit, later); aliasing −59 dB at full push (−22 dB clipping at the sample rate). ~1.7% of a core. VOLUME's LEDs show the clipper's shaving plus the limiter's reduction. Latency = ANALOG + clipper + limiter (61 + 61 + 159 = 281 at 48 kHz, about 5.9 ms), reported from the constructor too. (Careful: `std::copysign` takes only the first argument's size: the shaving is `-copysign (level - shaped, sample)`; the other way round it doubled the peaks.) Our guess, unconfirmed: PR7-C's OUTPUT TRIM sounds louder because it goes past 0 dBFS, which plays back louder in BeatMaker/Windows than any file can hold.
  - **The limiter (`dsp/Limiter.h`, `effects::TruePeakLimiter`):** a look-ahead true-peak limiter (its own gain input is still there and tested, but unused by the processor), ceiling **−1 dBTP** (aims at −1.06 for what detection misses), always on (a mix peaking over −1 dBTP is caught at VOLUME 0). Detection is `TruePeak::next` at **8×** (the meter stays 4×, the standard's); a hold over the 3 ms look-ahead + 1 sample, a release that follows the music (15 ms after a short peak, stretching up to 8× = 120 ms the longer and harder it has limited, over about a second, 6 dB counting as hard), then a 3 ms moving average, applied to audio delayed by `lookAhead − 1 + TruePeak::delay` (16) samples: 159 at 48 kHz, fixed whatever VOLUME is, reported from the constructor (48 kHz guess) and in `prepareToPlay`. Stereo linked. Its release runs in double precision (in float it got stuck 0.001 dB under 1 after any limiting; a test checks it lets go completely). Gain is exactly 1 with nothing to limit, so audio under the ceiling comes out bit for bit, only later. Measured with a 16× reference: music-like material (content to 20 kHz) stays under −1.0 dBTP; full-band white noise to Nyquist slips ~1 dB past (a known limit; a 4× meter still reads it under). Under 1% of a core. Its gain reduction drives VOLUME's LEDs (`getVolumeReduction`).
  - **PUSH (`dsp/GlueCompressor.h`, `effects::GlueCompressor`):** a glue compressor, our design after PR7-C's (its soft-knee curve, RMS detector, stretching release). Along p = PUSH/15 (`curveFor`): threshold −6 → −24 dB, ratio 1:1 → 3:1, attack 30 → 10 ms, release 300 → 150 ms (stretching up to 2× after long, hard work), make-up half the reduction of a −6 dBFS peak (0 → +6 dB). RMS detector (5 ms, +3 dB so thresholds read as peak levels) on the louder channel, with an 80 Hz high-pass in the sidechain; soft knee 6 dB; stereo linked. PUSH smoothed over 100 ms. At 0 (and done releasing) it doesn't run: bit for bit. No latency. Drives PUSH's LEDs (`getPushReduction`). Its slow attack lets a hit's start through with the make-up gain; VOLUME's limiter catches it.
  - **BODY / FOCUS / AIR (`dsp/ToneShaper.h`, `effects::ToneShaper`):** our design, three biquads (`dsp/Biquad.h`), no latency, first in the chain. AIR: high shelf, half its gain at 12 kHz; FOCUS: bell at 2.5 kHz, Q 0.6 (about two octaves); both 0.4 dB per knob step (±6 dB at ±15). BODY: low shelf, half its gain at 200 Hz, ±5 dB, with the whole mix turned the other way by the loudness the shelf adds (`bodyLoudnessDb`: 0.138 g + 0.0102 g² LU, fitted on pink noise through the K-weighting), so it tilts rather than gets louder: a test holds pink noise within 0.15 LU across its travel. Each knob smoothed over 50 ms (coefficients worked out every sample while moving); at 0 a band doesn't run (and starts fresh): bit for bit. Measure high tones by RMS in tests (a 12 kHz tone's highest sample can miss its peak by 3 dB).
  - **BASS PUNCH (`dsp/BassPunch.h`, `effects::BassPunch`):** our design, a dynamic EQ: one low shelf (half its gain at 100 Hz) lifting up to +6 dB (0.6 dB a step), with a compressor on the lows (two 120 Hz low-passes, RMS 10 ms read as peak, louder channel) taking the lift back: threshold −12 → −24 dB, ratio 1:1 → 3:1 (`curveFor`), 25 ms attack, 150 ms release, down to a −6 dB cut at most. One filter rather than a band split (a split shifts the phase round the crossover). Coefficients worked out every sample. At 0 and back to flat it doesn't run: bit for bit. No latency.
  - **TRANSIENTS (`dsp/TransientShaper.h`, `effects::TransientShaper`):** our design: a fast follower of the louder channel (0.5 ms up, 100 ms down) and a slow one following the fast one (25 ms up, 100 ms down); the gap (dB, up to 12) × ½ × TRANSIENTS/10 is the gain (±6 dB on attacks), smoothed over 2 ms. The slow one follows the fast one (not the raw audio) and both let go alike, so held notes are untouched: a test holds a steady 50 Hz tone within 0.05 dB (the first design, both following the audio with no smoothing, wobbled it 1.5 dB). At 0 it doesn't run: bit for bit. No latency.
  - **ANALOG (`dsp/Analog.h`, `effects::AnalogColour`):** our design after PR7-C's saturation (its `evenCurve` valve-like and `oddCurve` tanh, ¾ even + ¼ odd). Along a = ANALOG/10 the drive rises 0 → +12 dB (the bend at +6 dBFS before it, `headroom` 2) and the amount heard 0 → 100%, so it fades in from nothing; the drive is taken back off after the curve, so quiet sounds keep their level. Like the clipper, at 4× (FIR, integer latency 61) and only on what the curve adds, DC-blocked (5 Hz) and added to the audio delayed to match: at 0 bit for bit (only the delay runs once it has been at 0 for 200 ms and the tails are gone). Measured on a 1 kHz tone at −6 dBFS: 2nd harmonic −28 dB at 5, −18 dB at 10 (3rd −27 dB); at −30 dBFS the 2nd is about −39 dB (the valve curve's warmth falls with level but never quite goes); aliasing −110 dB.
  - **WIDTH (`dsp/Width.h`, `effects::StereoWidth`):** our design after PR7-C's stereo tools: mid/side, the mid never touched (the mono sum stays as it was). Above 120 Hz the side is scaled ×(1 + WIDTH/5): 0 at −5 (mono), 2 at +5. The side's bass (side minus a 24 dB/octave high-pass of it) is never widened and is taken out as WIDTH moves off 0 (`bassMonoFor`: all of it from ±1). At 0 it doesn't run: bit for bit. Mono tracks untouched. No latency.
  - **BYPASS at matched loudness:** measures the short-term loudness of `dry` and of the processed mix and, bypassed, plays `dry` turned to the processed loudness (20 ms crossfade, match smoothed over 0.5 s, held below −60 LUFS, ±24 dB, a boost capped so `dry`'s peaks over the last 3 s stay under −0.3 dBFS). The modules keep running while bypassed. A 0 dB match is exactly 1: with nothing processing, bypass is bit for bit. The host's own bypass is left as JUCE's plain bypass (not linked to `panel.bypass`).
  - **Metering (`dsp/Loudness.h`, `dsp/TruePeak.h`, `dsp/VuBallistics.h`):** BS.1770-4 K-weighting and short-term loudness (3 s, 100 ms steps; mono counts as one channel); true peak, 4× oversampled below 96 kHz (2× to 192 kHz), 32 taps per phase; the VU needle as a spring (damping 0.8127, 13.51 rad/s: 99% in 300 ms, 1.25% overshoot), run per channel at the sample rate, the louder side shown, scaled so a sine reads its peak. Handed to the UI by atomics (`getVuLevel`, `getShortTermLufs`, `getTruePeak` via `HeldPeakSource`, cleared on a click and when the host starts playing).
  - **0 VU reference:** −18, −14 (default) or −10 dBFS, in the gear menu, saved in the project (a property of the parameter state, `vuReference`, not a host parameter). Printed small on the VU face.
  - **RESET** (`resetKnobs`): loads the Default preset: every parameter but `panel.bypass` back to its default, each as a host gesture, and the name reads "Default".
  - **Presets (`source/PresetManager`, carried over from PR7-C):** factory presets (in code, each listing only what differs from the defaults; starting points for a whole mix, our own settings) and user presets (XML `.pr7mpreset` files in Documents/Paragon PR7-M/Presets). The processor owns one (`getPresets()`); the current name lives in the state (`presetName`). A preset sets every knob; `isKept`: only `panel.bypass` (the 0 VU reference isn't a parameter, so presets don't touch it either). Not exposed to the host as programs (hosts calling setCurrentProgram (0) at song load would reset the settings). Factory VOLUMEs assume a mix around −18 to −14 LUFS; measured on `denseMix` (−15.1 / −17.6 LUFS in): Gentle −13.2 / −15.4, Streaming −12.7 / −14.7, Pop −9.3 / −10.8, EDM −8.5 / −8.6, Maximum Loudness (VOLUME 12, gritty on purpose) −7.1 / −7.6.
  - **The assistant (`source/Assistant`, step 8, our design):** LISTEN → pick a target (Streaming −14, Loud −10, Maximum −8 LUFS) → it records the **input** (before processing; blocks under −50 dBFS skipped, so silence before the song doesn't count) until it has 15 s (`capture`, audio thread, no allocation: the buffer is sized in `start` on the message thread) → thinking on a `std::thread` (`update`, from the editor's timer) → `applyAssistant` sets every knob but BYPASS via `PresetManager::loadValues`, named "Assistant - Streaming" etc. (so " *" works). A sample-rate change while listening cancels it. The thinking (`decide`):
    - `analyse`: averaged FFT (4096, Hann, half overlap) of mid and side; each band (lows 30–120, body 120–400, presence 1.5–5k, air 8–16k) against the mids (400–1.5k), compared with `referenceDb` (**our approximation** of a typical modern master's long-term average spectrum, octave-band power +7 dB at 63 Hz … 0 at 1 kHz, −3, −7, −12, −21 at 16 kHz: to tune by ear); side vs mid above 120 Hz; K-weighted loudness (no gating) and 4× true peak → peak-to-loudness.
    - `suggest`: AIR −0.6 × air deviation (in 0.4 dB steps, ±10), FOCUS likewise (±7.5), BODY −0.5 × body − 0.25 × excess lows (±3), BASS PUNCH 2 − 0.8 × lows (0–6), PUSH 0.6 × (PLR − 9) + 0.3 × dB past −14 (1.5–7.5), TRANSIENTS 0.6 × (13 − PLR) (+1 for Loud/Maximum, 0–4), WIDTH 0.2 × (−12 − side dB) (−1 to +2; 0 if side under −35 dB: mono), ANALOG 1.5 (+0.15 per dB past −14), all rounded to 0.5.
    - `findVolume`: runs the real chain on the recording (everything before VOLUME once, then VOLUME by bisection to 0.15) to land the K-weighted loudness (after 0.5 s) on the target; capped by `mostVolumeFor`: the recording's peak before VOLUME may go at most 3 dB (Streaming, Loud: clean, = VOLUME 9 for a −1 dBFS peak) or 8.4 dB (Maximum, = VOLUME 12) over the clipper's ceiling. A quiet mix can go further up the knob. Too spiky to reach the target cleanly: it stops short and the display says so. (More PUSH was tried for that and doesn't help: its slow attack lets the peaks through. Removed.) About 4–5 s of thinking for 15 s at 48 kHz.
    - The preset display shows "Listening: play the loudest part", "Listening... 11 s", "Thinking...", then "Done: about −14.0 LUFS" (or "as loud as it goes cleanly") for 5 s. LISTEN glows while it works; clicking it while listening offers "Stop listening".
- **`source/ui/MainView`:** a fixed **1448 × 640** canvas (the header, then the faceplate `MainView::faceplate()`), scaled by the editor. Every control of the agreed layout is placed and attached to its parameter. Controls are registered by ID (`tone.air`, `tone.focus`, `tone.body`, `dyn.push`, `dyn.volume`, `panel.reset`, `panel.bypass`, `stereo.width`, `dyn.transients`, `colour.analog`, `dyn.bassPunch`).
  - **`VuMeter`**: cream face, −20 to +3 VU on a real VU's voltage spacing, red from 0 up, needle, glass. Shows the **output** (owner decision). It only draws (`setNeedle`, `setReference`); the ballistics are the processor's. `MainView::updateMeters` (60 Hz timer; tests call it directly, since JUCE's timer thread can't be relied on in tests) reads the processor and lets the needle fall by itself if the host stops sending audio.
  - **`LoudnessDisplay`** (`meter.loudness`), under the VU: short-term LUFS on the left, max true peak (dBTP, red above −1) on the right, seven-segment. A click clears the true peak.
  - **Levers:** BYPASS up = bypassed (its tip glows), attached to `panel.bypass`. RESET is momentary: up, then back down after 250 ms.
  - **PUSH and VOLUME's green LED rings** (`setGainReduction`): 12 LEDs, 1 per dB, on the knob's dark rim along the top end of its travel, drawn over the knob (`paintOverChildren`).
  - **Every knob** is attached to its parameter (`MainView::attach`; double-click: the default).
  - **LISTEN** (`assistant.listen`, between the preset display and SAVE): the assistant. `updateAssistant` (timer; tests call it) lights it and applies the result.
  - **The header** (as PR7-C's, same positions): the preset name centred over the VU (click it for the list: Factory, then "Yours" A to Z; " *" = changed since loaded, checked in the 60 Hz timer) with ◀ ▶ (step through, wrapping); **SAVE** (asks for a name; asks before replacing one of yours; factory names refused); **"…"** (back to the defaults, delete the current user preset, show the presets folder); the gear.
  - `PARAGON_SNAPSHOT_DEMO=1` loads "Pop - Polished" and sets the needle, the readout (−9.8 LUFS, −0.3 dBTP) and the LEDs for a render; `PARAGON_SNAPSHOT_LISTENING=1` shows the assistant 4 s into listening.
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
2. **VOLUME:** the look-ahead true-peak limiter. Released in 0.3.0; reworked in 0.6.0 (too little loudness: a clipper doing the work) and again after 0.7.0 (that distorted): clean first, the limiter with a music-following release, the clipper only from about 10.
3. **PUSH:** the glue compressor macro. Built and released in 0.4.0; the owner is to try it (the step 3 decisions below are still to confirm).
4. **BODY, FOCUS, AIR.** Built and released in 0.5.0; the owner is to try them (the step 4 decisions below are still to confirm).
5. **BASS PUNCH, TRANSIENTS.** Built and released in 0.6.0 (with VOLUME's clipper rework); the owner is to try them.
6. **ANALOG, WIDTH.** Built and released in 0.7.0; the owner is to try them.
7. **Presets.** Released in 0.8.0 (PR7-C's system, mastering factory presets); the owner is to try them (the step 7 decisions below are still to confirm).
8. **The assistant.** Built (LISTEN, a loudness target, every knob); the owner is to try it (the step 8 decisions below are still to confirm).

## Decisions (step 1, agreed with the owner)

- **0 VU** is choosable (−18 / −14 / −10 dBFS), −14 by default, saved with the project.
- **The readout** is an LED strip under the VU: **short-term LUFS** and **max true peak** only (not integrated, momentary or LRA).
- **BYPASS** matches loudness automatically; **RESET** is momentary, no confirmation.

## Decisions (step 2, our defaults, to confirm with the owner)

- **VOLUME** 0 to 15 = 0 to +15 dB, clean (limiter only) to about 9, then the clipper adds loudness and edge evenly to 15 (the owner wants it loud like PR7-C's OUTPUT TRIM, but not distorting; on dense material about −8.7 LUFS is the clean limit).
- **Ceiling** fixed at −1 dBTP (streaming's recommendation; the readout's red line).
- **The limiter is always on**, even at VOLUME 0 (switching it off would change the latency).

## Decisions (step 3, our defaults, to confirm with the owner)

- **PUSH's curve** as above (threshold −6 → −24 dB, ratio 1 → 3, attack 30 → 10 ms, release 300 → 150 ms): our guess at "gentle to hard" for a mix bus.
- **Make-up gain** half the reduction at a −6 dBFS peak, so PUSH makes the mix denser, not just quieter.
- **The sidechain leaves out the bass under 80 Hz**, so kick and bass don't pump the mix (BASS PUNCH handles the low end).

## Decisions (step 4, our defaults, to confirm with the owner)

- **AIR and FOCUS's ±15 are knob steps, not dB:** ±6 dB at full turn (0.4 dB a step), on the knob's printed, finer-near-0 scale.
- **AIR** a shelf from about 12 kHz; **FOCUS** a broad bell at 2.5 kHz; **BODY** a low shelf at 200 Hz, ±5 dB, level-compensated (a tilt).
- **Minimum-phase filters, no latency**, not linear-phase (gentle shelves don't need it).

## Decisions (step 5, our defaults, to confirm with the owner)

- **BASS PUNCH** as a dynamic low shelf (100 Hz): up to +6 dB lift, taken back on loud bass (down to −6 dB), reacting in 25 ms so a kick's first thump gets through.
- **TRANSIENTS** up to ±6 dB on attacks only; hits closer than about 100 ms apart are lifted a little less (the price of not wobbling held low notes).

## Decisions (step 6, our defaults, to confirm with the owner)

- **ANALOG** mostly 2nd harmonic (warmth) with a quarter 3rd (edge), up to +12 dB of drive: subtle at 5, clearly coloured at 10. It adds 61 samples (1.3 ms) of delay, to stay clean.
- **WIDTH** −5 mono to +5 twice as wide; the bass under 120 Hz kept in the middle (taken out of the side as soon as WIDTH moves off 0).

## Decisions (step 7, our defaults, to confirm with the owner)

- **The same preset system as PR7-C:** factory and user presets (SAVE), the name with ◀ ▶ in the header, no A/B. RESET = the Default preset.
- **A preset holds every knob, including VOLUME** (so a preset gets you a finished loudness); BYPASS and the 0 VU reference aren't part of one.
- **Factory presets** by style and by character (13 beside Default), our own starting points: Gentle, Streaming (about −14 LUFS from a typical mix), Pop, Hip-Hop, EDM, Rock, Acoustic, Vocal Forward, Warm Analog, Bright and Open, Soft and Smooth, Lo-Fi, Maximum Loudness.

## Decisions (step 8, owner's choices and our defaults)

- **Owner chose:** a **LISTEN button** (play the loudest part for about 15 s), **pick a loudness first** (Streaming −14, Loud −10, Maximum −8 LUFS), and it sets **all the knobs** (ANALOG gently).
- **Our defaults, to confirm:** it corrects the tone only part of the way towards a typical master's balance (our approximation of one); it never pushes VOLUME into audible clipping for Streaming and Loud (it stops short instead and says so); Maximum may add edge; it doesn't touch BYPASS or the 0 VU setting; it names the result like a preset so SAVE keeps it.

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

- **Version 0.0.0 is never released** (CI skips it). 0.1.0 (a panel preview, knobs inert) was released so the owner could see the panel in BeatMaker; 0.2.0 (step 1: the metering, BYPASS and RESET; knobs still inert) for the owner to try them; 0.3.0 (step 2: VOLUME, the limiter); 0.4.0 (step 3: PUSH, and the limiter's release fix); 0.5.0 (step 4: BODY, FOCUS, AIR); 0.6.0 (step 5: BASS PUNCH, TRANSIENTS, and VOLUME reworked with the clipper); 0.7.0 (step 6: ANALOG, WIDTH; every knob works); 0.7.1 (VOLUME clean first, the clipper only from about 10); 0.8.0 (step 7: presets).
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
