"""The Paragon family's other apps on this computer (no Qt): found through their installers' uninstall entries
(each AppId is permanent), else the default per-user folder; started with files or folders. Used for the
family's links (PR7-M's Show in folder opens PR7-E when it is the folder app)."""

from __future__ import annotations

import os
import subprocess
import sys

APPS = {"PR7-Z": "{9914C6C8-01EC-476F-845D-98F814A0426E}", "PR7-W": "{D8F51204-6E9E-4CB7-BF2E-58F4908B7937}",
        "PR7-L": "{4C0E3F59-8A0B-4E7D-9B5A-2F7C61D3A9E1}", "PR7-F": "{34F163CD-E4F2-4415-9865-1175C28F5408}",
        "PR7-E": "{6B8E2F14-3C5A-4D97-A1E0-7F29C4B85D36}",
        "PR7-R": "{1BC53F2C-70E7-45EA-B97E-FC9294E87FC3}", "PR7-P": "{02CE0A66-B141-47AD-9489-ACCB0C53E81C}",
        "PR7-M": "{CAA852D1-CDD0-4246-8ADB-50307D44F360}"}
UNINSTALL = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\{}_is1"


def find(app: str) -> str | None:
    """The installed app's .exe, or None."""
    if sys.platform != "win32" or app not in APPS:
        return None
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, UNINSTALL.format(APPS[app])) as k:
            exe = os.path.join(winreg.QueryValueEx(k, "InstallLocation")[0], f"{app}.exe")
            if os.path.isfile(exe):
                return exe
    except OSError:
        pass
    exe = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Programs", app, f"{app}.exe")
    return exe if os.path.isfile(exe) else None


def launch(app: str, args: list[str]) -> bool:
    exe = find(app)
    if not exe:
        return False
    subprocess.Popen([exe, *args], close_fds=True)
    return True


def pr7e_for_folders() -> bool:
    """Whether PR7-E's "Use PR7-E for Folders" is on (folders' default action is PR7-E's)."""
    if sys.platform != "win32":
        return False
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Classes\Directory\shell") as k:
            return winreg.QueryValueEx(k, "")[0] == "PR7E"
    except OSError:
        return False
