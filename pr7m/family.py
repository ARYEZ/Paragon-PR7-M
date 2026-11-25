"""The Paragon family's other apps on this computer (no Qt): found through their installers' uninstall entries
(each AppId is permanent), else the default per-user folder; started with files or folders. Used for the
family's links between apps, and to offer PR7-U (the family installer) from Check for Updates."""

from __future__ import annotations

import os
import subprocess
import sys

APPS = {"PR7-Z": "{9914C6C8-01EC-476F-845D-98F814A0426E}", "PR7-W": "{D8F51204-6E9E-4CB7-BF2E-58F4908B7937}",
        "PR7-L": "{4C0E3F59-8A0B-4E7D-9B5A-2F7C61D3A9E1}", "PR7-F": "{34F163CD-E4F2-4415-9865-1175C28F5408}",
        "PR7-E": "{6B8E2F14-3C5A-4D97-A1E0-7F29C4B85D36}",
        "PR7-R": "{1BC53F2C-70E7-45EA-B97E-FC9294E87FC3}", "PR7-P": "{02CE0A66-B141-47AD-9489-ACCB0C53E81C}",
        "PR7-M": "{CAA852D1-CDD0-4246-8ADB-50307D44F360}", "PR7-Q": "{D7ED5F6F-F80A-403C-8739-AA5BA3676C97}",
        "PR7-U": "{12226380-56E6-4D77-8F5C-8052A0AE4183}",
        "PR7-DU": "{9DFF1917-658D-4BE5-8C19-A88E7150AC24}",
        "PR7-CB": "{5F4E0C50-1C11-4E53-A835-221E3BBB7509}",
        "PR7-IS": "{187DC039-E485-4BF2-9C1B-6026AF54C9F8}",
        "PR7-PM": "{18BE3275-9FB9-46DF-9C29-060A544E4632}",
        "PR7-SM": "{DA18598E-10B7-4EA2-B221-71F0CFC56AA8}",
        "PR7-HB": "{5A2D8918-1C5C-456F-AFCF-1FBA498EA280}",
        "PR7-FS": "{D573AEAF-B102-484A-AD03-42624DC18A25}",
        "PR7-AR": "{5DD7DA56-83DC-421E-8D0F-E7FB868B3860}",
        "PR7-WM": "{2E52DD0D-51E7-40B7-B5C4-F0EDC2C9E8B9}"}
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
