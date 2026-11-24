"""Screenshots of the real window (offscreen), playing stand-in media: python tools/screenshots.py OUT_DIR"""
import json
import os
import sys
import tempfile
import time
from pathlib import Path

_cfg = Path(tempfile.gettempdir()) / "pr7m_offscreen.json"
_cfg.write_text(json.dumps({"screens": [{"name": "s", "x": 0, "y": 0, "width": 3200, "height": 2200,
                                          "logicalDpi": 96, "logicalBaseDpi": 96, "dpr": 1}]}))
os.environ.setdefault("QT_QPA_PLATFORM", f"offscreen:configfile={_cfg}")
os.environ.setdefault("QT_LOGGING_RULES", "qt.multimedia.*=false")
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))
work = Path(tempfile.mkdtemp())
os.environ["PR7M_DATA"] = str(work / "data")

from PySide6.QtCore import QSettings  # noqa: E402
from PySide6.QtWidgets import QApplication  # noqa: E402

QSettings.setDefaultFormat(QSettings.Format.IniFormat)
QSettings.setPath(QSettings.Format.IniFormat, QSettings.Scope.UserScope, str(work / "settings"))
import sample_images  # noqa: E402
import sample_media  # noqa: E402
from pr7m import theme  # noqa: E402
from pr7m.main_window import MainWindow, apply_look  # noqa: E402

out = Path(sys.argv[1] if len(sys.argv) > 1 else "docs/screenshots")
out.mkdir(parents=True, exist_ok=True)
app = QApplication([])
theme.load_fonts()
apply_look(app)
films = work / "Films"
films.mkdir()
sample_media.avi_from(films / "The Northern Line.avi", sample_images.photo(2, "city", 1800, 1012), 4, 10, (1280, 720))
sample_media.srt(films / "The Northern Line.srt", [(0.0, 4.0, "You never told me the city looked like this at night.")])
sample_media.avi_from(films / "Harbour at Dusk.avi", sample_images.photo(1, "sea", 1600, 900), 2, 10, (1280, 720))
music = work / "Coastlines"
music.mkdir()
sample_media.wav(music / "02 Evening Tide.wav", seconds=6, freq=330)
sample_media.wav(music / "01 Low Water.wav", seconds=4)
sample_images.photo(1, "sea", 800, 800).save(str(music / "cover.jpg"))


def settle(sec=1.0):
    end = time.time() + sec
    while time.time() < end:
        app.processEvents(); time.sleep(.02)


def shot(w, name):
    settle(.3)
    w.grab().save(str(out / f"{name}.png"))
    print(out / f"{name}.png")


w = MainWindow()
w.answers["error"] = lambda t: print("error:", t)
w.resize(1280, 800)
w.show()
w.open_paths([str(films / "The Northern Line.avi")])
settle(1.5)
w.player.pause()
shot(w, "main_window")
w.open_paths([str(music / "02 Evening Tide.wav")])
settle(1.5)
shot(w, "music")
w.open_paths([str(films / "The Northern Line.avi")])
settle(1.0)
w.set_finish("aurora")
w.player.pause()
shot(w, "main_window_aurora")
w.set_finish("obsidian")
w.close()
