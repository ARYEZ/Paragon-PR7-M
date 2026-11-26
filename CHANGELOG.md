# Changelog

## 0.3.4

- Sound follows Windows' default output, as in MPC-HC: when the default changes (another device chosen in Windows,
  headphones or a monitor's speakers waking up, a driver reset), PR7-M moves to it and carries on from the same place
  (paused stays paused). Before, it kept the device it started with and could play into one nobody hears.

## 0.3.3

- The family list (`family.py`) knows the three newest Paragon apps: PR7-HW (hardware information), PR7-BK
  (backups) and PR7-TM (terminal), so links between the apps and the PR7-U offer find them.

## 0.3.2

- The family list (`family.py`) knows the four newest Paragon apps: PR7-HX (hex editor), PR7-DB (SQLite browser),
  PR7-UN (uninstaller) and PR7-LN (launcher), so links between the apps and the PR7-U offer find them.

## 0.3.1

- The family's list knows the newest apps (PR7-HB, PR7-FS, PR7-AR, PR7-WM), so links between apps and Check for Updates find them.

## 0.3.0

- **One finish for the whole family.** Choosing OBSIDIAN or AURORA here switches every Paragon app, open ones included (within a couple of seconds), and every app starts in the family's choice.
- In AURORA, group titles and selected rows are dark and readable (they were white on the light finish).

## 0.2.1

- Knows the whole family: PR7-DU, PR7-CB, PR7-IS, PR7-PM and PR7-SM are in its list of family apps (their permanent install IDs), so the family links find them when they're installed.

## 0.2.0

- **Check for Updates knows PR7-U**, the family installer. When it's installed, the update window has **Update All in PR7-U**, and a check by hand that finds PR7-M up to date offers to open PR7-U for the rest of the family.

## 0.1.0

- First version: a media player that works the way MPC-HC works, in the Paragon family look. Playback by Qt
  Multimedia with FFmpeg: video (MP4, MKV, WebM, AVI, WMV, MOV ...) and music (MP3, AAC, FLAC, OGG, WAV, WMA ...).
- Playlist from the file's folder, folders, M3U / M3U8 and drag and drop; after playback: next, repeat, stop, close.
- Resume where each file was left; recent files; favorites.
- Seek bar with the time under the pointer; 10 s and 1 min jumps; Go to; frame step; A-B repeat.
- Speed 0.25x - 4x keeping the pitch; volume and mute.
- Subtitles: .srt next to the film or loaded by hand, and the file's own text tracks, drawn in Inter; audio tracks.
- Music shows its cover, tags and a level meter. Full screen with the controls over the picture.
- Save image, Properties, Open with / Default apps, one window for every file opened, Check for updates, OBSIDIAN
  and AURORA.
