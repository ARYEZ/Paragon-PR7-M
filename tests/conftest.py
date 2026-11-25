import os
import sys
import tempfile
import time
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
os.environ["PARAGON_FAMILY_FILE"] = os.path.join(__import__("tempfile").mkdtemp(prefix="family-"), "finish")  # never the real choice
os.environ.setdefault("QT_LOGGING_RULES", "qt.multimedia.*=false")
os.environ["PR7M_DATA"] = tempfile.mkdtemp(prefix="pr7m-data-")


@pytest.fixture(scope="session")
def qapp(tmp_path_factory):
    # Tests never write the real settings: QSettings goes to INI files in a temp folder.
    from PySide6.QtCore import QSettings, QThreadPool
    from PySide6.QtWidgets import QApplication
    from pr7m import theme
    QSettings.setDefaultFormat(QSettings.Format.IniFormat)
    QSettings.setPath(QSettings.Format.IniFormat, QSettings.Scope.UserScope, str(tmp_path_factory.mktemp("settings")))
    app = QApplication.instance() or QApplication([])
    theme.load_fonts()
    from pr7m.main_window import apply_look
    apply_look(app)
    yield app
    QThreadPool.globalInstance().waitForDone(15000)
    for w in app.topLevelWidgets():
        w.close()
    app.clipboard().clear()
    app.processEvents()


@pytest.fixture(autouse=True)
def own_data(tmp_path, monkeypatch):
    monkeypatch.setenv("PR7M_DATA", str(tmp_path / "_data"))
    yield


@pytest.fixture
def clean_settings(qapp):
    from PySide6.QtCore import QSettings
    s = QSettings(QSettings.defaultFormat(), QSettings.Scope.UserScope, "Paragon", "PR7-M")
    s.clear(); s.sync()
    yield
    s.clear(); s.sync()


@pytest.fixture
def films(qapp, tmp_path):
    """A folder: two short videos (10 fps, a colour a second, numbered frames), a tone, subtitles for the first."""
    import sample_media
    d = tmp_path / "Films"
    d.mkdir()
    a = sample_media.avi(d / "01 Clip.avi", seconds=3)
    b = sample_media.avi(d / "02 Second.avi", seconds=2)
    t = sample_media.wav(d / "03 Tone.wav", seconds=2)
    sample_media.srt(d / "01 Clip.srt")
    (d / "notes.txt").write_text("not media")
    return [a, b, t]


def wait_until(qapp, cond, timeout=10.0):
    end = time.time() + timeout
    while time.time() < end:
        qapp.processEvents()
        if cond():
            return True
        time.sleep(0.01)
    return cond()


@pytest.fixture
def wait(qapp):
    return lambda cond, timeout=10.0: wait_until(qapp, cond, timeout)


@pytest.fixture
def window(qapp, clean_settings):
    from pr7m.main_window import MainWindow
    w = MainWindow()
    w.answers["error"] = lambda text: pytest.fail(f"PR7-M showed an error: {text}")
    w.resize(1100, 700)
    w.show()
    qapp.processEvents()
    yield w
    w.player.stop()
    w.close()
    w.deleteLater()
    qapp.processEvents()
