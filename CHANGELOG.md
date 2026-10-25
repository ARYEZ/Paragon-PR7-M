# Changelog

Each version's notes appear on its GitHub Release page.

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
