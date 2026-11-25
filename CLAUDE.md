# Paragon PR7-M: notes for Claude

**Paragon PR7-M** is the owner's personal media player: Python (**PySide6** + **Qt Multimedia / FFmpeg**) that
**works the way MPC-HC works**, in the Paragon family look (PR7-Z, PR7-W, PR7-L, PR7-F, PR7-E, PR7-R, PR7-P
`ARYEZ/Paragon-PR7-P`). Windows 10/11 x64 is the target; it runs on Linux for tests. The owner writes Python; explain
the why of changes.

## How it's built

- `pr7m/media.py` (no Qt): `MEDIA_EXTS`, `media_in` (natural sort), `hms` / `parse_time`, `RATES` / `step_rate`,
  SubRip (`parse_srt`, `read_srt` tries UTF-8 then cp1252, `cue_at`, `sidecar_subtitles`), M3U (`read_m3u`,
  `write_m3u` relative paths + #EXTINF), `ABRepeat`, `History` (recent + resume in `<data>/history.json`, **PR7M_DATA**
  in tests; no resume for files under a minute or within 30 s of the end).
- `pr7m/player.py`: `VideoView` (a QVideoSink; frames painted with QVideoFrame.paint so our subtitles in Inter go over
  them; music: cover, tags, a level meter from QAudioBufferOutput), `SeekBar`, `VolumeSlider` (0-100: Qt's
  QAudioOutput can't go above 100%), `PlaylistModel` (plain Python list model, never QStandardItemModel).
- `pr7m/main_window.py`: QMediaPlayer + QAudioOutput + QAudioBufferOutput; MPC-HC's menus and keys (`_act(...,
  key=)` into `acts`); `_ask(kind)` for every question (tests: `window.answers`). `meta(md, key)` reads metadata
  (PySide can't convert the codec enums from `value()`: falls back to `stringValue`). **Resume / favorite seeks wait
  for BufferedMedia** (`_apply_pending`): a seek during loading is dropped when playback starts. Frame step = pause +
  one frame's time (Qt has no frame step). Full screen re-parents `Controls` into the VideoView (floating, auto-hide).
  Qt exposes no chapters.
- `pr7m/app.py`: one window per user (QLocalSocket hand-off, PR7-R's Windows fix), `--register` / `--unregister`
  (shell.py: Open with + Capabilities for MEDIA_EXTS, HKCU), `--selftest FOLDER OUT.json` (plays each file).
- `tools/sample_media.py`: real test media written here: `wav` (wave module), `avi` / `avi_from` / `write_avi`
  (a Motion-JPEG AVI by hand; test frames are numbered, a colour a second: `colour_of`), `srt`.
- Tests play for real (offscreen; no audio device is fine: Qt plays on the system clock). `wait(cond)` spins events.
- Installer `installer/pr7m.iss` (per user, AppId {CAA852D1-CDD0-4246-8ADB-50307D44F360} permanent); CI checks the
  FFmpeg plugin and libraries are in the build, installs, `tools/verify_install.py installed` plays an AVI and a WAV
  with the installed exe.
- Updates: `pr7m/updates.py` (no Qt; GitHub's public releases/latest, User-Agent names only the app, installer asset `PR7-M-Setup-*.exe`, run with /SILENT /relaunch=1) and `update_ui.py` (once-a-day check from app.py, the menu's Check for Updates, UpdateDialog; settings updates/auto|last|skip; `notify` for tests). The installer's [Code] Relaunch starts PR7-M again only for /relaunch=1. Same files in every family app (PR7-W's update_ui imports PyQt6): keep them in step. With PR7-U (the family installer) installed, update_ui offers it (`_family_app`, `open_family`; `answer` for tests); every app has the shared family.py with all ten AppIds. `family.py`: the other family apps by their permanent AppIds (uninstall InstallLocation).

## The look (family rules)

- Theme is PR7-P's (OBSIDIAN + AURORA via `theme.set_finish`). Colours: the family's only (hues ~318 -> 0 -> 42 deg,
  pearl and onyx greys), seek bar, volume and playlist marks included; Inter; labels UPPER CASE; file names keep case.
- The PR7-M mark: PR7 `Logo.cpp` geometry plus an M (`widgets.M_SHAPE`); icon by `tools/make_icon.py`, wizard art by
  `tools/make_installer_art.py`. Check visuals with `QT_SCALE_FACTOR=2 python tools/screenshots.py OUT`.

## Rules

- Behaviour questions: do what MPC-HC does (menus, keys, defaults).
- Tests: `python -m pytest` (offscreen) on real media from `tools/sample_media.py`. Never the real settings (conftest),
  data folder (PR7M_DATA) or, outside CI, the registry.
- Releases: bump `__version__` in `pr7m/__init__.py` and add a `## x.y.z` section to CHANGELOG.md, then push:
  CI publishes that version by itself once every check has passed (the "Is this version released yet?"
  step; this session can't start workflows or push tags). Run workflow / `v*` tags still work.
