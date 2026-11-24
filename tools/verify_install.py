"""Check a real install of PR7-M-Setup.exe (CI, Windows): python tools/verify_install.py installed|uninstalled

`installed` runs the installed PR7-M.exe offscreen on a folder with a video (Motion-JPEG AVI) and a WAV
(--selftest): it plays each and reports its length, frames shown and tracks, which shows the frozen program has Qt
Multimedia, its FFmpeg plugin and the Inter fonts. It also checks "Open with" and Default apps. `uninstalled` checks
all of it is gone."""
import json
import os
import subprocess
import sys
import tempfile
import time
import winreg
from pathlib import Path

HKCU = winreg.HKEY_CURRENT_USER
APP = Path(os.environ["LOCALAPPDATA"]) / "Programs" / "PR7-M"
SHORTCUT = Path(os.environ["APPDATA"]) / "Microsoft" / "Windows" / "Start Menu" / "Programs" / "Paragon PR7-M.lnk"
UNINSTALL = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\{CAA852D1-CDD0-4246-8ADB-50307D44F360}_is1"
PROGID = r"Software\Classes\Paragon.PR7M.Media"


def value(path, name=""):
    try:
        with winreg.OpenKey(HKCU, path) as k:
            return winreg.QueryValueEx(k, name)[0]
    except OSError:
        return None


def exists(path, name):
    """Whether a value is there (an empty REG_NONE value, as Open with's entries are, reads back as None)."""
    try:
        with winreg.OpenKey(HKCU, path) as k:
            winreg.QueryValueEx(k, name)
            return True
    except OSError:
        return False


def check(ok, what, p):
    print(("ok    " if ok else "FAIL  ") + what)
    if not ok:
        p.append(what)


def installed():
    p = []
    exe = APP / "PR7-M.exe"
    for f in (exe, APP / "unins000.exe", APP / "_internal" / "pr7m" / "fonts" / "Inter-Regular.ttf",
              APP / "_internal" / "pr7m" / "icons" / "app.ico"):
        check(f.is_file(), f"file {f}", p)
    check(SHORTCUT.is_file(), "Start menu shortcut", p)
    check(value(UNINSTALL, "DisplayName") == "Paragon PR7-M", "Installed apps entry", p)
    cmd = value(PROGID + r"\shell\open\command") or ""
    check(str(exe).lower() in cmd.lower() and cmd.endswith('"%1"'), f"media open command: {cmd}", p)
    for ext in ("mp4", "mkv", "mp3"):
        check(exists(rf"Software\Classes\.{ext}\OpenWithProgids", "Paragon.PR7M.Media"), f"Open with lists PR7-M for .{ext}", p)
    check(value(r"Software\RegisteredApplications", "Paragon PR7-M") == r"Software\Paragon\PR7-M\Capabilities",
          "Default apps lists PR7-M", p)
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from PySide6.QtGui import QGuiApplication
    app = QGuiApplication.instance() or QGuiApplication(["x", "-platform", "offscreen"])
    import sample_media
    with tempfile.TemporaryDirectory() as d:
        folder = Path(d) / "Media"
        folder.mkdir()
        sample_media.avi(folder / "clip.avi", seconds=3)
        sample_media.wav(folder / "tone.wav", seconds=2)
        out = Path(d) / "out.json"
        subprocess.run([str(exe), "--selftest", str(folder), str(out)], timeout=180, check=False)
        got = json.loads(out.read_text(encoding="utf-8")) if out.exists() else {}
        played = got.get("played", {})
        clip, tone = played.get("clip.avi", {}), played.get("tone.wav", {})
        check(clip.get("duration") == 3000 and clip.get("video") and clip.get("frames", 0) > 0,
              f"PR7-M.exe plays a video ({clip or got})", p)
        check(tone.get("duration") == 2000 and tone.get("audio") and tone.get("position", 0) > 0,
              f"PR7-M.exe plays audio ({tone or got})", p)
        check(got.get("fonts") is True, "Inter fonts", p)
    del app
    return p


def uninstalled():
    deadline = time.time() + 60
    while True:
        p = []
        check(not (APP / "PR7-M.exe").exists(), "program files removed", p)
        check(not SHORTCUT.exists(), "Start menu shortcut removed", p)
        check(value(UNINSTALL, "DisplayName") is None, "Installed apps entry removed", p)
        check(value(PROGID) is None, "media type removed", p)
        check(value(r"Software\RegisteredApplications", "Paragon PR7-M") is None, "Default apps entry removed", p)
        if not p or time.time() > deadline:
            return p
        print("... waiting for the uninstaller to finish")
        time.sleep(3)


if __name__ == "__main__":
    problems = {"installed": installed, "uninstalled": uninstalled}[sys.argv[1]]()
    print("\nALL OK" if not problems else f"\n{len(problems)} problem(s)")
    sys.exit(1 if problems else 0)
