"""The family's finish: OBSIDIAN (dark) or AURORA (light), one choice for every Paragon app. No Qt.

Choosing a finish in any family app saves it here; every app reads it when it starts and checks it every couple of
seconds while it runs (`follow`), so switching in one app switches all of them, open ones included. Windows keeps it in
HKEY_CURRENT_USER\\Software\\Paragon\\Family ("Finish"); elsewhere in ~/.config/paragon/family-finish.
PARAGON_FAMILY_FILE points it at a file instead (tests always set it, so they never touch the registry).

The same file is in every family app: keep them in step."""

from __future__ import annotations

import os
import sys

NAMES = ("obsidian", "aurora")
KEY = r"Software\Paragon\Family"


def _file() -> str | None:
    f = os.environ.get("PARAGON_FAMILY_FILE")
    if f:
        return f
    if sys.platform != "win32":
        return os.path.join(os.path.expanduser("~"), ".config", "paragon", "family-finish")
    return None


def get() -> str | None:
    """The family's finish, or None when no app has chosen one yet."""
    f = _file()
    if f is not None:
        try:
            with open(f, encoding="utf-8") as fh:
                v = fh.read().strip().lower()
        except OSError:
            return None
    else:
        try:
            import winreg
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, KEY) as k:
                v = str(winreg.QueryValueEx(k, "Finish")[0]).strip().lower()
        except OSError:
            return None
    return v if v in NAMES else None


def set(name: str) -> None:  # noqa: A001 - finish.set reads naturally
    """Save the family's finish (every family app follows within a couple of seconds)."""
    name = name.lower()
    if name not in NAMES or get() == name:
        return
    f = _file()
    try:
        if f is not None:
            os.makedirs(os.path.dirname(f) or ".", exist_ok=True)
            with open(f, "w", encoding="utf-8") as fh:
                fh.write(name)
        else:
            import winreg
            with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, KEY, 0, winreg.KEY_SET_VALUE) as k:
                winreg.SetValueEx(k, "Finish", 0, winreg.REG_SZ, name)
    except OSError:
        pass                                     # the finish stays this app's own; nothing to report


def start(own: str = "obsidian") -> str:
    """The finish to start with: the family's, else this app's own saved one."""
    return get() or (own if own in NAMES else "obsidian")


def follow(current: str, apply) -> bool:
    """Call every couple of seconds: when another app changed the family's finish, `apply(new)`. True if it did."""
    new = get()
    if new and new != current:
        apply(new)
        return True
    return False
