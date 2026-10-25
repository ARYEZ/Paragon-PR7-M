# Paragon PR7-M

[![Build](https://github.com/ARYEZ/Paragon-PR7-M/actions/workflows/build.yml/badge.svg)](https://github.com/ARYEZ/Paragon-PR7-M/actions/workflows/build.yml)

A **mastering** VST3 plugin, part of the Paragon PR7 family (PR7-G guitar, PR7-B bass, PR7-V vocals, PR7-C channel strip). Built with [JUCE](https://juce.com).

*Early days: so far only the family look, INPUT, OUTPUT and the meters. The mastering modules are built one at a time from the concept design; see [CLAUDE.md](CLAUDE.md) for the plan.*

## Building

```
cmake -B build
cmake --build build --config Release
```

The VST3 is in `build/ParagonPR7M_artefacts/Release/VST3/`. Tests: `ctest --test-dir build -C Release`.

## License

GNU AGPLv3 (see [LICENSE.md](LICENSE.md)), as required by JUCE's open-source license. The Inter typeface is under the SIL Open Font License (`resources/fonts/OFL-LICENSE.txt`).
