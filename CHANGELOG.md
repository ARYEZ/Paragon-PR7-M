# Changelog

Each version's notes appear on its GitHub Release page.

## 0.7.0

**Every knob works: ANALOG and WIDTH.** The whole chain is in place. Presets come next.

- **ANALOG** (0–10): warmth and colour from harmonics, mostly the warm 2nd with a little edge from the 3rd (PR7-C's saturation curves, made gentler for a whole mix).
  - It fades in from nothing as you turn it up: a subtle warmth around 5, a clearly audible colour at 10, like tape pushed hard.
  - It changes the colour, not the loudness. Quiet parts get much less colour than loud ones.
  - At 0 your mix comes through exactly as it was. It works at 4× the sample rate, so it stays clean (no harsh digital grit).
- **WIDTH** (−5 to +5): narrower or wider stereo. −5 is mono, +5 is twice as wide.
  - It only changes the difference between left and right, so your mix sounds the same in mono at any setting.
  - The bass below 120 Hz always stays in the middle: widening never spreads the low end.
- **A little more delay:** ANALOG adds 1.3 ms to stay clean, about 5.9 ms in all. PR7-M tells BeatMaker, so your track stays in time.

## 0.6.0

**VOLUME gets loud, and BASS PUNCH and TRANSIENTS work.** ANALOG and WIDTH come next.

- **VOLUME, reworked after your test:** it now keeps getting louder all the way up the knob, like pushing PR7-C's OUTPUT TRIM. Before, it barely got louder past about 9.
  - **Why it stalled:** it turned the mix up into a clean limiter, and on a dense mix (a peak every few milliseconds) the limiter just turned down harder.
  - **Now:** the mix goes into a soft clipper that rounds the peaks off, then the limiter catches what's left. On a test mix, full turn is about 10 LU louder than 0, and peaks still stay under −1 dBTP, so it's safe for streaming.
  - **At 0 it leaves your mix untouched.** The harder you push, the rounder it shaves. At high settings you'll hear some grit, by design.
  - The delay grows slightly, to about 4.6 ms (PR7-M tells BeatMaker, so your track stays in time). The green LEDs show how much is shaved off the peaks.
- **BASS PUNCH** (0–10): more low end and tighter at once. It lifts the lows up to 6 dB, and holds back on loud bass notes, so the low end comes out fuller and more even. A kick's first thump gets through before it acts.
- **TRANSIENTS** (−10 to +10): turn right for sharper attacks, left for softer ones, up to 6 dB on the start of each hit. Held notes are left alone.

## 0.5.0

**AIR, FOCUS and BODY work: the tone.** The three knobs down the left now shape the sound, first in the chain. BASS PUNCH and TRANSIENTS come next.

- **AIR** lifts or lowers the very top, from about 12 kHz up: the air above the cymbals. Up to 6 dB either way at full turn.
- **FOCUS** lifts or lowers a broad region around 2.5 kHz, where voices and leads come through. Up to 6 dB either way.
- **BODY** gives more or less low end (from about 200 Hz down), up to 5 dB, and turns the rest of the mix the other way so the **loudness stays the same**: you hear more or less body, not just louder or quieter.
- **The scales read true:** AIR's and FOCUS's markings are finer near 0 (−15, −10, −6, −3, 0, +3, +6, +10, +15), and the knobs follow them exactly.
- **Untouched at 0, no delay:** at 0 each knob leaves the sound exactly as it was. Double-click a knob to send it back to 0. Turning them doesn't click.

## 0.4.0

**PUSH works: the compressor that holds the mix together.** The second knob that changes the sound. BODY, FOCUS and AIR come next.

- **PUSH** compresses the whole mix, gentle to hard on one knob. Turning it up makes the mix denser and more even: it starts turning down lower, harder and quicker, all along one curve. At 0 it does nothing.
- **The green LEDs** round PUSH show how much it's turning down, one LED per dB.
- **Drum hits keep their snap:** it reacts slowly at first (30 ms at gentle settings, 10 ms at 15), so the start of each hit gets through before it acts.
- **It doesn't pump:** it lets go more slowly after long, hard passages. And the deep bass (under 80 Hz) doesn't set it off, so the kick and bass don't make the whole mix duck.
- **Denser, not quieter:** it turns the result back up by about half of what it takes off at typical levels (up to +6 dB at 15), so pushing doesn't just make the mix quieter.
- **With VOLUME:** PUSH comes first, then VOLUME's limiter, which catches the start of any hit that PUSH lets through too loud.
- **Fixed:** after VOLUME's limiter had turned down even once, it stayed turned down by about 0.001 dB for good. Far too little to hear, but quiet passages are now exactly untouched again, as they should be.

## 0.3.0

**VOLUME works: louder, without clipping.** The first knob that changes the sound. The other knobs still do nothing; PUSH (the compressor) comes next.

- **VOLUME** turns the mix up, 0 to +15 dB, into a limiter that keeps every peak under **−1 dBTP**, the level streaming services ask for (where the readout turns red). Double-click the knob to go back to 0.
- **The green LEDs** round VOLUME show how much the limiter is turning down, one LED per dB.
- **No clicks or crackle:** the limiter looks 3 ms ahead, so it's already turning down when a peak arrives, then lets go over about a tenth of a second. Both sides move together, so the stereo picture stays put.
- **It catches peaks between the samples too**, the ones that clip on conversion or as MP3/AAC.
- **A small delay:** the look-ahead delays the audio by about 3.3 ms. PR7-M tells BeatMaker, so your track stays in time.
- **BYPASS now has something to compare:** with VOLUME up, flick BYPASS to hear your mix as it came in, at the same loudness.
- **Below the ceiling, the sound is untouched:** if nothing reaches −1 dBTP, the audio comes out exactly as it went in, only 3.3 ms later.
- **The limiter is always on**, even at VOLUME 0: a mix that already peaks above −1 dBTP gets caught.

## 0.2.0

**The meter, BYPASS and RESET work.** The VU meter now moves, there's a loudness readout under it, and the two levers do their jobs. **The knobs still don't change the sound**: the audio passes through unchanged. VOLUME (the limiter) comes next.

- **VU meter:** the needle moves like a real VU meter's. It takes about a third of a second to rise to a level, swings just past it, and falls back at the same speed. It shows the louder of left and right.
- **0 VU setting** (gear menu): 0 VU can stand for −18, −14 or −10 dBFS. The default is −14, so a master at streaming loudness swings around 0 instead of sitting pinned on the right. It's saved with your project and printed small on the meter.
- **Loudness readout** under the meter:
  - **Left:** short-term loudness, the last 3 seconds in LUFS, as streaming services measure it.
  - **Right:** the highest true peak in dBTP, including peaks between the samples (the ones that clip on conversion or as MP3/AAC). It turns red above −1 dBTP.
  - Click the readout to clear the true peak. Pressing play clears it too.
- **BYPASS** (flick it up; its tip glows): hear the mix without the processing, turned to the **same loudness** as the processed sound, so the louder version can't win just by being louder. It crossfades, so there's no click.
  - It settles about 3 seconds after you press play.
  - It never turns the mix up so far that it would clip, so against a very loud master the bypassed sound can stay slightly quieter.
  - Until the knobs work, there's nothing to compare: bypassed, you hear the mix exactly as it came in.
- **RESET** (flick it up, it springs back): every knob back to its starting point. BYPASS stays as it is.

## 0.1.0

**A preview of the panel.** PR7-M's first build, to see it in your DAW. **The knobs don't do anything yet**: the audio passes through unchanged. Each knob comes to life in its own step, starting with VOLUME (the limiter).


- The seed: the PR7 family's look (Pearl White, White + Neon, Pearl Black). Audio passes through untouched.
- The panel: AIR, FOCUS, BODY, PUSH, VOLUME, WIDTH, TRANSIENTS, ANALOG and BASS PUNCH, with RESET and BYPASS and an output VU meter. The controls don't work yet: each arrives with its own step.
