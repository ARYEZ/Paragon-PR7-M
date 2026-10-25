# Reference code (not compiled)

`pr7c/dsp/` holds PR7-C's DSP modules as they were when PR7-M was seeded: the five-band EQ and cut filters (`ChannelEq`), compressor, limiter, saturation, console colour (`ConsoleColour`, `ConsoleChannel`) and stereo tools (mono maker, width).

They're starting points, not part of the build. When a PR7-M module needs one, copy it into `source/dsp/`, adapt it for mastering (look-ahead, more oversampling, linear phase where it helps), and give it its own tests. See PR7-C's CLAUDE.md (`ARYEZ/Paragon-PR7-C`) for how each one works and why.
