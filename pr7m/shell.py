"""Windows: PR7-M under "Open with" for video and music and in Settings > Default apps (all per-user, HKEY_CURRENT_USER).

Windows 10/11 don't let a program make itself the default for a file type (the choice is the user's, kept under a
hash only Windows writes), so PR7-M registers as a media player and the Options dialog opens Default apps at PR7-M's
page, where one click makes it the default for each media type. unregister() removes exactly what register() wrote.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

PROGID = "Paragon.PR7M.Media"
APP_NAME = "Paragon PR7-M"                          # its name in Windows' Default apps
CAPABILITIES = r"Software\Paragon\PR7-M\Capabilities"
from .media import MEDIA_EXTS

EXTS = sorted(MEDIA_EXTS)


def available() -> bool:
    return os.name == "nt"


def launcher() -> list[str]:
    if getattr(sys, "frozen", False):
        return [sys.executable]
    py = Path(sys.executable)
    pyw = py.with_name("pythonw.exe")
    return [str(pyw if pyw.exists() else py), "-m", "pr7m"]


def command() -> str:
    return " ".join(f'"{p}"' if (" " in p or p.lower().endswith(".exe")) else p for p in launcher()) + ' "%1"'


def icon() -> str:
    return f"{sys.executable},0" if getattr(sys, "frozen", False) else str(Path(__file__).parent / "icons" / "app.ico")


def _set(path: str, values: dict):
    import winreg
    with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, path, 0, winreg.KEY_WRITE) as k:
        for name, val in values.items():
            winreg.SetValueEx(k, name, 0, winreg.REG_NONE if val is None else winreg.REG_SZ,
                              b"" if val is None else val)


def _get(path: str, name: str = ""):
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path) as k:
            return winreg.QueryValueEx(k, name)[0]
    except OSError:
        return None


def _delete_value(path: str, name: str):
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path, 0, winreg.KEY_SET_VALUE) as k:
            winreg.DeleteValue(k, name)
    except OSError:
        pass


def _delete_tree(path: str):
    import winreg
    try:
        k = winreg.OpenKey(winreg.HKEY_CURRENT_USER, path, 0, winreg.KEY_ALL_ACCESS)
    except OSError:
        return
    while True:
        try:
            sub = winreg.EnumKey(k, 0)
        except OSError:
            break
        _delete_tree(f"{path}\\{sub}")
    winreg.CloseKey(k)
    winreg.DeleteKey(winreg.HKEY_CURRENT_USER, path)


def register() -> None:
    _set(rf"Software\Classes\{PROGID}", {"": "Media file (PR7-M)", "FriendlyTypeName": "Media file"})
    _set(rf"Software\Classes\{PROGID}\DefaultIcon", {"": icon()})
    _set(rf"Software\Classes\{PROGID}\shell\open", {"": "Open with PR7-M"})
    _set(rf"Software\Classes\{PROGID}\shell\open\command", {"": command()})
    for ext in EXTS:
        _set(rf"Software\Classes\.{ext}\OpenWithProgids", {PROGID: None})
    # The program's own entry, so "Open with > Choose another app" names it and opens files with it.
    _set(r"Software\Classes\Applications\PR7-M.exe", {"FriendlyAppName": APP_NAME})
    _set(r"Software\Classes\Applications\PR7-M.exe\shell\open\command", {"": command()})
    _set(r"Software\Classes\Applications\PR7-M.exe\SupportedTypes", {f".{e}": "" for e in EXTS})
    _set(CAPABILITIES, {"ApplicationName": APP_NAME, "ApplicationDescription": "Media player (works like MPC-HC)",
                        "ApplicationIcon": icon()})
    _set(CAPABILITIES + r"\FileAssociations", {f".{e}": PROGID for e in EXTS})
    _set(r"Software\RegisteredApplications", {APP_NAME: CAPABILITIES})
    _notify()


def unregister() -> None:
    for ext in EXTS:
        _delete_value(rf"Software\Classes\.{ext}\OpenWithProgids", PROGID)
    _delete_tree(rf"Software\Classes\{PROGID}")
    _delete_tree(r"Software\Classes\Applications\PR7-M.exe")
    _delete_tree(CAPABILITIES)
    _delete_value(r"Software\RegisteredApplications", APP_NAME)
    _notify()


def registered() -> bool:
    return available() and _get(r"Software\RegisteredApplications", APP_NAME) == CAPABILITIES


def open_default_apps() -> None:
    """Settings > Default apps at PR7-M's page (Windows 11; Windows 10 shows the Default apps list)."""
    if available():
        os.startfile("ms-settings:defaultapps?registeredAppUser=Paragon%20PR7-M")  # noqa: S606 - Windows only


def _notify():
    try:
        import ctypes
        ctypes.windll.shell32.SHChangeNotify(0x08000000, 0, None, None)  # SHCNE_ASSOCCHANGED
    except Exception:  # noqa: BLE001 - not on Windows
        pass


def run(action: str) -> int:
    """The installer's and the command line's --register / --unregister."""
    if not available():
        print("Explorer integration is for Windows.")
        return 1
    try:
        {"--register": register, "--unregister": unregister}[action]()
    except OSError as e:
        print(f"{action} failed: {e}")
        return 2
    return 0
