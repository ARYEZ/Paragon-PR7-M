# Paragon PR7-M

A media player for Windows 10/11 that works the way **MPC-HC** (Media Player Classic - Home Cinema) works, in the
Paragon family look (PR7-Z, PR7-W, PR7-L, PR7-F, PR7-E, PR7-R, PR7-P). Python, PySide6 and Qt Multimedia, which
plays through FFmpeg: no codec packs to install.

![PR7-M](docs/screenshots/main_window.png)

## What it does

- **Plays** MP4, MKV, WebM, AVI, WMV, MOV, MPEG, TS and more; MP3, AAC, FLAC, OGG / Opus, WAV, WMA.
- **Playlist**: opening a file fills it with its folder (MPC-HC); open folders, M3U / M3U8, drag and drop; move,
  remove, sort; save as M3U8. After playback: next file, repeat the file, repeat the playlist, stop or close.
- **Resume**: each file starts where it was left (unless short or nearly finished); recent files; favorites (a
  file at a time).
- **Moving about**: the seek bar (click / drag, the time under the pointer), Left / Right 10 s, Ctrl+Shift+Left /
  Right 1 min, Ctrl+G go to a time, Ctrl+Left / Ctrl+Right frame step, A-B repeat (Ctrl+B).
- **Speed** 0.25x - 4x keeping the pitch (Ctrl+Up / Ctrl+Down / Ctrl+R); volume (Up / Down, the wheel), mute.
- **Subtitles**: a .srt with the film's name loads by itself, or Ctrl+L; the file's own text subtitle tracks; drawn
  over the picture in Inter. Audio track choice for files with several.
- **Music**: the cover (embedded, or cover.jpg / folder.jpg), title, artist, album and a level meter.
- **Full screen** (Alt+Enter, double-click): the controls float over the picture when the mouse moves.
- Save image (the frame on screen), Properties, Open with / Default apps, one window for every file opened, Check for
  updates, OBSIDIAN and AURORA.

Not in this version: DVD / Blu-ray discs and web streams (files only); chapters (Qt doesn't read them); volume above
100% (Qt's audio output stops at 100%); picture-based subtitles inside files (DVD / PGS) may not show.

## Develop

```
pip install -r requirements.txt -r requirements-dev.txt
python -m pr7m some-film.mkv
python -m pytest
QT_SCALE_FACTOR=2 python tools/screenshots.py OUT
```
