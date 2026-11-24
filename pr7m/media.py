"""The parts of a media player that are just rules (no Qt): which files are media, time as text, SubRip subtitles,
M3U playlists, MPC-HC's speed steps, A-B repeat, and where each file was left (resume)."""

from __future__ import annotations

import json
import os
import re
import time
import unicodedata
from dataclasses import dataclass
from pathlib import Path

VIDEO_EXTS = {"mp4", "m4v", "mkv", "webm", "avi", "wmv", "mov", "mpg", "mpeg", "ts", "m2ts", "mts", "flv", "ogv",
              "3gp", "vob", "divx", "asf"}
AUDIO_EXTS = {"mp3", "m4a", "aac", "flac", "ogg", "oga", "opus", "wav", "wma", "ac3", "dts", "mka", "aiff", "aif",
              "ape", "wv", "mpc"}
MEDIA_EXTS = VIDEO_EXTS | AUDIO_EXTS
PLAYLIST_EXTS = {"m3u", "m3u8"}
RATES = [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0]
RESUME_MIN_MS = 60_000      # files shorter than a minute always start at the start
RESUME_END_MS = 30_000      # and nearly finished ones start again too (MPC-HC's rule of thumb)


def ext(path: str) -> str:
    return os.path.splitext(path)[1][1:].lower()


def is_media(path: str) -> bool:
    return ext(path) in MEDIA_EXTS


def is_audio(path: str) -> bool:
    return ext(path) in AUDIO_EXTS


def natural_key(name: str):
    plain = unicodedata.normalize("NFKD", name).encode("ascii", "ignore").decode().lower() or name.lower()
    return [(0, int(t), t) if t.isdigit() else (1, 0, t) for t in re.split(r"(\d+)", plain) if t != ""]


def media_in(folder: str) -> list[str]:
    """The media files in a folder, sorted by name (MPC-HC fills the playlist with the folder's files)."""
    try:
        names = os.listdir(folder)
    except OSError:
        return []
    out = [os.path.join(folder, n) for n in names if not n.startswith(".") and is_media(n)
           and os.path.isfile(os.path.join(folder, n))]
    out.sort(key=lambda p: natural_key(os.path.basename(p)))
    return out


# ---------------------------------------------------------------- time
def hms(ms: int | float, always_hours: bool = True) -> str:
    """00:12:34 (MPC-HC shows hours always); `always_hours=False` gives 4:37 / 1:45:00 (the playlist's)."""
    s = max(0, int(ms // 1000))
    h, m, sec = s // 3600, s // 60 % 60, s % 60
    if always_hours:
        return f"{h:02d}:{m:02d}:{sec:02d}"
    return f"{h}:{m:02d}:{sec:02d}" if h else f"{m}:{sec:02d}"


def parse_time(text: str) -> int | None:
    """Go To's box: "1:02:03", "62:03", "95" (seconds), "1:02:03.5" -> milliseconds."""
    t = text.strip().replace(",", ".")
    if not t:
        return None
    try:
        parts = [float(x) for x in t.split(":")]
    except ValueError:
        return None
    if len(parts) > 3 or any(p < 0 for p in parts):
        return None
    secs = 0.0
    for p in parts:
        secs = secs * 60 + p
    return int(round(secs * 1000))


def step_rate(rate: float, up: bool) -> float:
    if up:
        bigger = [r for r in RATES if r > rate + 1e-6]
        return bigger[0] if bigger else RATES[-1]
    smaller = [r for r in RATES if r < rate - 1e-6]
    return smaller[-1] if smaller else RATES[0]


# ---------------------------------------------------------------- subtitles (SubRip)
@dataclass
class Cue:
    start: int      # ms
    end: int
    text: str


_TIME = r"(\d+):(\d{1,2}):(\d{1,2})[,.](\d{1,3})"


def _ms(h, m, s, f) -> int:
    return ((int(h) * 60 + int(m)) * 60 + int(s)) * 1000 + int(f.ljust(3, "0")[:3])


def parse_srt(text: str) -> list[Cue]:
    """SubRip cues; formatting tags (<i>, <b>, {\\an8}) are taken out."""
    text = text.lstrip("﻿").replace("\r\n", "\n").replace("\r", "\n")
    cues = []
    for block in re.split(r"\n\s*\n", text):
        lines = [ln for ln in block.split("\n") if ln.strip() != ""]
        for i, ln in enumerate(lines):
            m = re.match(_TIME + r"\s*-->\s*" + _TIME, ln.strip())
            if m:
                body = "\n".join(lines[i + 1:])
                body = re.sub(r"</?[a-zA-Z][^>]*>|\{\\[^}]*\}", "", body).strip()
                if body:
                    cues.append(Cue(_ms(*m.groups()[:4]), _ms(*m.groups()[4:]), body))
                break
    cues.sort(key=lambda c: c.start)
    return cues


def read_srt(path: str) -> list[Cue]:
    raw = Path(path).read_bytes()
    for enc in ("utf-8-sig", "cp1252", "latin-1"):
        try:
            return parse_srt(raw.decode(enc))
        except UnicodeDecodeError:
            continue
    return []


def cue_at(cues: list[Cue], ms: int) -> str:
    return "\n".join(c.text for c in cues if c.start <= ms < c.end)


def sidecar_subtitles(media: str) -> list[str]:
    """Subtitle files next to a film with its name ("Film.srt", "Film.en.srt"), the plain one first."""
    folder, stem = os.path.dirname(media), os.path.splitext(os.path.basename(media))[0].lower()
    try:
        names = os.listdir(folder)
    except OSError:
        return []
    found = [n for n in names if n.lower().endswith(".srt") and
             (n.lower() == stem + ".srt" or n.lower().startswith(stem + "."))]
    found.sort(key=lambda n: (len(n), n.lower()))
    return [os.path.join(folder, n) for n in found]


# ---------------------------------------------------------------- playlists (M3U)
def read_m3u(path: str) -> list[str]:
    """Files listed in an M3U / M3U8 (relative ones are relative to the playlist; #-lines are comments)."""
    base = os.path.dirname(os.path.abspath(path))
    raw = Path(path).read_bytes()
    text = raw.decode("utf-8-sig", errors="replace") if path.lower().endswith("8") or raw.startswith(b"\xef\xbb\xbf") \
        else raw.decode("utf-8", errors="replace")
    out = []
    for ln in text.splitlines():
        ln = ln.strip()
        if not ln or ln.startswith("#"):
            continue
        if ln.lower().startswith("file:///"):
            ln = ln[8:] if os.name == "nt" else ln[7:]
        out.append(os.path.normpath(ln if os.path.isabs(ln) else os.path.join(base, ln)))
    return out


def write_m3u(path: str, files: list[str], durations: dict[str, int] | None = None) -> None:
    """An extended M3U8 (UTF-8): #EXTINF with the length when known; paths relative when they're below it."""
    base = os.path.dirname(os.path.abspath(path))
    lines = ["#EXTM3U"]
    for f in files:
        secs = int((durations or {}).get(f, -1000) // 1000)
        lines.append(f"#EXTINF:{secs},{os.path.splitext(os.path.basename(f))[0]}")
        try:
            rel = os.path.relpath(f, base)
            lines.append(f if rel.startswith("..") else rel)
        except ValueError:          # another drive
            lines.append(f)
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")


# ---------------------------------------------------------------- A-B repeat
class ABRepeat:
    """Ctrl+B sets A, then B, then clears; between B and A the player jumps back."""

    def __init__(self):
        self.a: int | None = None
        self.b: int | None = None

    def press(self, pos: int) -> str:
        if self.a is None:
            self.a = pos
            return "A"
        if self.b is None and pos > self.a:
            self.b = pos
            return "AB"
        self.a = self.b = None
        return ""

    def jump(self, pos: int) -> int | None:
        """Where to go when the player is at `pos` (None: carry on)."""
        if self.a is not None and self.b is not None and pos >= self.b:
            return self.a
        return None

    @property
    def active(self) -> bool:
        return self.a is not None and self.b is not None


# ---------------------------------------------------------------- where files were left
def data_dir() -> Path:
    env = os.environ.get("PR7M_DATA")
    if env:
        return Path(env)
    base = os.environ.get("APPDATA") or os.path.join(Path.home(), ".config")
    return Path(base) / "Paragon" / "PR7-M"


class History:
    """Recent files and the place each was left at (%APPDATA%\\Paragon\\PR7-M\\history.json; PR7M_DATA in tests)."""
    KEEP = 300

    def __init__(self, path: Path | None = None):
        self.path = path or data_dir() / "history.json"
        try:
            raw = json.loads(self.path.read_text(encoding="utf-8"))
            self.files: dict[str, dict] = raw.get("files", {}) if isinstance(raw, dict) else {}
        except (OSError, ValueError):
            self.files = {}

    @staticmethod
    def _key(path: str) -> str:
        return os.path.normcase(os.path.abspath(path))

    def save(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        keep = dict(sorted(self.files.items(), key=lambda kv: -kv[1].get("last", 0))[: self.KEEP])
        tmp = self.path.with_suffix(".tmp")
        tmp.write_text(json.dumps({"files": keep}, indent=1), encoding="utf-8")
        os.replace(tmp, self.path)

    def opened(self, path: str) -> None:
        e = self.files.setdefault(self._key(path), {"path": os.path.abspath(path)})
        e["last"] = time.time()
        self.save()

    def remember(self, path: str, position: int, duration: int) -> None:
        """Keep where a file was left, unless it's short or nearly finished (then it starts at the start)."""
        e = self.files.setdefault(self._key(path), {"path": os.path.abspath(path), "last": time.time()})
        if duration and (duration < RESUME_MIN_MS or position > duration - RESUME_END_MS or position < 5000):
            e.pop("position", None)
        else:
            e["position"] = int(position)
        self.save()

    def position(self, path: str) -> int:
        return int(self.files.get(self._key(path), {}).get("position", 0))

    def recent(self, n: int = 10, exists=os.path.exists) -> list[str]:
        es = sorted(self.files.values(), key=lambda e: -e.get("last", 0))
        return [e["path"] for e in es if exists(e["path"])][:n]
