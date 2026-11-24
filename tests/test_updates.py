"""Check for updates: GitHub's answer stood in by a function, the download from a real local web server."""
import http.server
import json
import os
import sys
import threading

import pytest

from pr7m import __version__, updates


def answer(version, assets=None, **extra):
    data = {"tag_name": "v" + version, "html_url": f"https://github.com/{updates.REPO}/releases/tag/v{version}",
            "body": "- **Something new**\r\n- A fix", "draft": False, "prerelease": False,
            "assets": assets if assets is not None else [
                {"name": f"PR7-M-v{version}-windows-x64.zip", "browser_download_url": "https://x/zip", "size": 9},
                {"name": f"PR7-M-Setup-{version}.exe", "browser_download_url": "https://x/setup.exe", "size": 5}]}
    data.update(extra)
    return lambda url: json.dumps(data).encode()


def test_versions():
    assert updates.version_tuple("v1.2.10") == (1, 2, 10, 0)
    assert updates.is_newer("1.10.0", "1.9.3") and not updates.is_newer("1.0.0", "1.0.0")
    assert updates.is_newer("2.0", "1.99.99") and not updates.is_newer("0.0.1", "1.0")


def test_latest_release_and_its_installer():
    rel = updates.latest(answer("9.0.0"))
    assert rel.version == "9.0.0" and rel.installer == "https://x/setup.exe" and rel.size == 5
    assert rel.notes == "- **Something new**\n- A fix"
    assert updates.check(__version__, answer("9.0.0")) is not None
    assert updates.check(__version__, answer(__version__)) is None          # up to date
    assert updates.check(__version__, answer("0.0.1")) is None
    assert updates.latest(answer("9.0.0", prerelease=True)) is None          # test builds aren't offered
    assert updates.latest(answer("9.0.0", assets=[])) is None                # no installer yet


def test_github_unreachable_says_so():
    def down(url):
        raise OSError("network is unreachable")
    with pytest.raises(updates.UpdateError, match="couldn’t be reached"):
        updates.latest(down)


def test_the_request_names_only_the_app(monkeypatch):
    seen = {}

    class Resp:
        def __enter__(self):
            return self

        def __exit__(self, *a):
            return False

        def read(self):
            return b"{}"

    def fake(req, timeout):
        seen.update(url=req.full_url, headers=dict(req.header_items()))
        return Resp()
    monkeypatch.setattr(updates.urllib.request, "urlopen", fake)
    updates.latest()
    assert seen["url"] == f"https://api.github.com/repos/ARYEZ/Paragon-PR7-M/releases/latest"
    assert seen["headers"]["User-agent"] == "Paragon-PR7-M-updater"


@pytest.fixture
def web(tmp_path):
    payload = os.urandom(300_000)
    (tmp_path / "site").mkdir()
    (tmp_path / "site" / "PR7-M-Setup-9.0.0.exe").write_bytes(payload)

    class Quiet(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *a, **k):
            super().__init__(*a, directory=str(tmp_path / "site"), **k)

        def log_message(self, *a):
            pass
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Quiet)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    yield f"http://127.0.0.1:{srv.server_port}/PR7-M-Setup-9.0.0.exe", payload
    srv.shutdown()


def test_download(web, tmp_path):
    url, payload = web
    rel = updates.Release("9.0.0", "page", url, len(payload), "")
    seen = []
    path = updates.download(rel, str(tmp_path / "dl"), progress=lambda got, total: seen.append(got))
    assert open(path, "rb").read() == payload and path.endswith("PR7-M-Setup-9.0.0.exe")
    assert seen[-1] == len(payload)
    # An incomplete download is never kept (it would be run).
    bad = updates.Release("9.0.0", "page", url, len(payload) + 1, "")
    with pytest.raises(updates.UpdateError, match="incomplete"):
        updates.download(bad, str(tmp_path / "dl2"))
    assert os.listdir(tmp_path / "dl2") == []


def test_the_dialog(qapp, web, monkeypatch):
    from pr7m import update_ui
    from PySide6.QtWidgets import QWidget
    said = []
    monkeypatch.setattr(update_ui, "notify", lambda kind, text: said.append((kind, text)))
    s = update_ui._settings()
    s.remove("updates")
    w = QWidget()
    # By hand, nothing newer: says so.
    update_ui.check(w, manual=True, fetch=answer(__version__), sync=True)
    assert said[-1][0] == "info" and __version__ in said[-1][1]
    # Something newer: the dialog, with its notes.
    url, payload = web
    fetch = answer("9.0.0", assets=[{"name": "PR7-M-Setup-9.0.0.exe", "browser_download_url": url,
                                     "size": len(payload)}])
    update_ui.check(w, manual=False, fetch=fetch, sync=True)
    dlg = w.update_dialog
    assert dlg.rel.version == "9.0.0" and "Something new" in dlg.notes.toPlainText()
    # Install Now: downloads, then runs the installer (stood in here).
    ran = []
    dlg.install(sync=True, run=ran.append)
    assert ran and open(ran[0], "rb").read() == payload
    # Skip This Version: not offered again automatically (still by hand).
    update_ui.check(w, manual=False, fetch=fetch, sync=True)
    dlg2 = w.update_dialog
    assert dlg2 is not dlg
    dlg2._skip()
    w.update_dialog = None
    update_ui.check(w, manual=False, fetch=fetch, sync=True)
    assert w.update_dialog is None
    update_ui.check(w, manual=True, fetch=fetch, sync=True)
    assert w.update_dialog is not None
    # The automatic check can be turned off.
    w.update_dialog.auto.setChecked(False)
    assert not update_ui.auto_enabled()
    update_ui.set_auto(True)
    # GitHub down, by hand: the reason.
    update_ui.check(w, manual=True, fetch=lambda u: (_ for _ in ()).throw(OSError("offline")), sync=True)
    assert said[-1][0] == "warning" and "reached" in said[-1][1]
    for d in (dlg, dlg2, w.update_dialog):
        d.done(0)
    s.remove("updates")


@pytest.mark.skipif(sys.platform == "win32", reason="elsewhere installing isn't possible")
def test_install_is_windows_only():
    with pytest.raises(updates.UpdateError):
        updates.install("x.exe")
