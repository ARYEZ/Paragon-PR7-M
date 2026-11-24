"""Check for updates (no Qt): the family's apps publish releases on GitHub (public repositories), so the newest
version is asked for at GitHub's public "latest release" address. Nothing about the user is sent: the request
names only the app ("Paragon-PR7-M-updater"). The installer is downloaded to a temporary folder and run with
/SILENT; "/relaunch=1" makes it start PR7-M again when it's done (the installer's Relaunch check)."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
import time
import urllib.request
from dataclasses import dataclass

APP = "PR7-M"
REPO = "ARYEZ/Paragon-PR7-M"
API = f"https://api.github.com/repos/{REPO}/releases/latest"
RELEASES = f"https://github.com/{REPO}/releases"
EVERY = 24 * 3600          # automatic checks: at most once a day
INSTALLER = re.compile(rf"^{re.escape(APP)}-Setup-[\w.\-]+\.exe$", re.I)


class UpdateError(Exception):
    """Shown to the user as it is."""


@dataclass
class Release:
    version: str               # "1.1.0"
    page: str                  # the release's web page
    installer: str             # the installer's download address
    size: int
    notes: str                 # the release notes (the version's CHANGELOG section)


def version_tuple(v: str) -> tuple[int, ...]:
    """ "v1.2.10" -> (1, 2, 10, 0): compared number by number (1.10 is newer than 1.9)."""
    nums = [int(n) for n in re.findall(r"\d+", (v or "").lstrip("vV").split("-")[0])]
    return tuple((nums + [0, 0, 0, 0])[:4])


def is_newer(latest: str, current: str) -> bool:
    return version_tuple(latest) > version_tuple(current)


def _fetch(url: str, timeout: float = 15) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": f"Paragon-{APP}-updater",
                                               "Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:  # noqa: S310 - a fixed https address
        return r.read()


def latest(fetch=None) -> Release | None:
    """The newest published release with an installer, or None when there's none yet."""
    try:
        data = json.loads((fetch or _fetch)(API))
    except OSError as e:
        raise UpdateError(f"GitHub couldn’t be reached ({getattr(e, 'reason', e)}).") from e
    except ValueError as e:
        raise UpdateError("GitHub’s answer couldn’t be read.") from e
    if not isinstance(data, dict) or data.get("draft") or data.get("prerelease"):
        return None
    asset = next((a for a in data.get("assets", []) if INSTALLER.match(a.get("name", ""))), None)
    if asset is None:
        return None
    return Release(data.get("tag_name", "").lstrip("vV"), data.get("html_url", RELEASES),
                   asset.get("browser_download_url", ""), int(asset.get("size") or 0),
                   (data.get("body") or "").replace("\r\n", "\n").strip())


def check(current: str, fetch=None) -> Release | None:
    """A release newer than `current`, or None."""
    rel = latest(fetch)
    return rel if rel is not None and is_newer(rel.version, current) else None


def due(last: float, now: float | None = None) -> bool:
    return (now or time.time()) - (last or 0) >= EVERY


def download(rel: Release, folder: str | None = None, progress=None, cancelled=lambda: False,
             opener=None) -> str:
    """The installer, to a temporary folder (".part" until it's whole, so a broken download is never run)."""
    folder = folder or os.path.join(tempfile.gettempdir(), f"{APP}-update")
    os.makedirs(folder, exist_ok=True)
    name = rel.installer.rsplit("/", 1)[-1] or f"{APP}-Setup.exe"
    path = os.path.join(folder, name)
    part = path + ".part"
    req = urllib.request.Request(rel.installer, headers={"User-Agent": f"Paragon-{APP}-updater"})
    try:
        with (opener or urllib.request.urlopen)(req, timeout=30) as r, open(part, "wb") as fh:
            got = 0
            while True:
                if cancelled():
                    raise UpdateError("Cancelled.")
                buf = r.read(1 << 16)
                if not buf:
                    break
                fh.write(buf)
                got += len(buf)
                if progress:
                    progress(got, rel.size)
        if rel.size and os.path.getsize(part) != rel.size:
            raise UpdateError("The download was incomplete. Try again.")
        os.replace(part, path)
    except OSError as e:
        raise UpdateError(f"The download failed ({getattr(e, 'reason', e)}).") from e
    finally:
        if os.path.exists(part):
            os.remove(part)
    return path


def install(path: str, relaunch: bool = True) -> None:
    """Run the installer over this copy: /SILENT shows only its progress; Inno Setup closes PR7-M first
    (CloseApplications) and, with /relaunch=1, starts it again afterwards."""
    if sys.platform != "win32":
        raise UpdateError("Installing updates is for Windows.")
    args = [path, "/SILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CLOSEAPPLICATIONS"]
    if relaunch:
        args.append("/relaunch=1")
    subprocess.Popen(args, close_fds=True)
