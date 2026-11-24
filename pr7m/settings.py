"""PR7-M's settings (HKCU\\Software\\Paragon\\PR7-M on Windows), with typed getters: QSettings returns
strings from the registry and INI files, so every read says what type it wants."""

from __future__ import annotations

import json

from PySide6.QtCore import QSettings


def _s() -> QSettings:
    # The default format, not QSettings("Paragon", "PR7-M"): that constructor always uses the native format
    # (the registry), so the tests' switch to INI files in a temp folder wouldn't reach it.
    return QSettings(QSettings.defaultFormat(), QSettings.UserScope, "Paragon", "PR7-M")


def get(key: str, default: str = "") -> str:
    v = _s().value(key)
    return default if v is None else str(v)


def get_bool(key: str, default: bool = False) -> bool:
    v = _s().value(key)
    if v is None:
        return default
    return v is True or str(v).lower() in ("true", "1", "yes")


def get_int(key: str, default: int = 0) -> int:
    try:
        return int(_s().value(key, default))
    except (TypeError, ValueError):
        return default


def get_list(key: str) -> list:
    try:
        v = json.loads(get(key, "[]"))
        return v if isinstance(v, list) else []
    except ValueError:
        return []


def set(key: str, value) -> None:  # noqa: A001 - settings.set reads naturally
    s = _s()
    s.setValue(key, json.dumps(value) if isinstance(value, (list, dict)) else value)
    s.sync()


def get_float(key: str, default: float = 0.0) -> float:
    try:
        return float(_s().value(key, default))
    except (TypeError, ValueError):
        return default
