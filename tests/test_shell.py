"""Open with / Default apps registration. It writes HKEY_CURRENT_USER, so it runs only on Windows CI (tests never
write the registry on the owner's computer)."""
import os
import sys

import pytest

from pr7m import shell

pytestmark = pytest.mark.skipif(sys.platform != "win32" or not os.environ.get("CI"),
                                reason="writes the registry: Windows CI only")


def test_register_and_unregister():
    import winreg
    shell.register()
    try:
        assert shell.registered()
        assert shell._get(rf"Software\Classes\{shell.PROGID}\shell\open\command").endswith('"%1"')
        for ext in ("mp4", "mkv", "mp3"):
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\.{ext}\OpenWithProgids") as k:
                winreg.QueryValueEx(k, shell.PROGID)
            assert shell._get(shell.CAPABILITIES + r"\FileAssociations", f".{ext}") == shell.PROGID
    finally:
        shell.unregister()
    assert not shell.registered() and shell._get(rf"Software\Classes\{shell.PROGID}") is None
