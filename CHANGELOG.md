# Changelog

Each version's notes appear on its GitHub Release page.

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
