"""Mock-up pictures of PR7-M, drawn offscreen with real Qt widgets: python tools/mockup.py OUT_DIR"""
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
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))

from PySide6.QtCore import QPointF, QRectF, Qt  # noqa: E402
from PySide6.QtGui import QColor, QFont, QImage, QLinearGradient, QPainter, QPen  # noqa: E402
from PySide6.QtWidgets import (QApplication, QFrame, QHBoxLayout, QLabel, QMainWindow, QMenu, QVBoxLayout,  # noqa: E402
                               QWidget)

from pr7m import theme, widgets  # noqa: E402
import sample_images  # noqa: E402

out = Path(sys.argv[1] if len(sys.argv) > 1 else "docs/mockup")
out.mkdir(parents=True, exist_ok=True)
app = QApplication([])
theme.load_fonts()
FRAME = sample_images.photo(2, "city", 1920, 1080)
COVER = sample_images.photo(1, "sea", 1000, 1000)

MENUS = {
    "&File": ["&Quick open file...\tCtrl+Q", "&Open file...\tCtrl+O", "Open &folder...", "Open &URL...\tCtrl+U",
              "-", "Recent &files", "-", "&Close\tCtrl+C", "-", "Save &image...\tAlt+I", "&Properties\tShift+F10",
              "-", "E&xit\tAlt+X"],
    "&View": ["&Playlist\tCtrl+7", "&Controls", "&Seek bar", "&Status bar", "-", "&Full screen\tAlt+Enter",
              "&Zoom", "&Video frame", "-", "&On top", "-", "&Finish", "&Options...\tO"],
    "&Play": ["&Play / Pause\tSpace", "&Stop\tPeriod", "&Frame step\tCtrl+Right", "Frame step &back\tCtrl+Left", "-",
              "&Decrease rate\tCtrl+Down", "&Increase rate\tCtrl+Up", "&Reset rate\tCtrl+R", "-", "&Audio",
              "Su&btitles", "-", "&Volume", "A-B &repeat\tCtrl+B", "&After playback"],
    "&Navigate": ["&Previous\tPgUp", "&Next\tPgDn", "-", "&Chapters", "&Go to...\tCtrl+G", "-",
                  "Jump &forward 10 s\tRight", "Jump &backward 10 s\tLeft", "Jump forward 1 &min\tCtrl+Shift+Right"],
    "F&avorites": ["&Add to favorites...", "&Organize favorites...", "-", "The Northern Line.mkv  00:12:34"],
    "&Help": ["&Keyboard shortcuts\tF1", "Check for &updates...", "&About Paragon PR7-M"],
}
CHECKED = {"&Playlist\tCtrl+7", "&Controls", "&Seek bar", "&Status bar"}
PLAYLIST = [("The Northern Line.mkv", "1:45:00"), ("Harbour at Dusk.mp4", "0:22:18"), ("City Lights (2024).mkv", "1:38:41"),
            ("Dunes - Trailer.webm", "0:02:31"), ("Behind the scenes.mp4", "0:14:05"), ("Mountain Lake.avi", "0:48:12")]


class Video(QWidget):
    def __init__(self, frame: QImage | None = None, audio: bool = False, overlay: bool = False):
        super().__init__()
        self.frame, self.audio, self.overlay = frame, audio, overlay

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)
        p.fillRect(self.rect(), QColor("#000000"))
        if self.audio:
            side = min(self.height() * .62, self.width() * .4)
            r = QRectF(self.width() / 2 - side - 30, (self.height() - side) / 2, side, side)
            p.fillRect(r.translated(0, 8), QColor(0, 0, 0, 200))
            p.drawImage(r, COVER)
            x = r.right() + 48
            f = QFont("Inter"); f.setPixelSize(30); f.setWeight(QFont.Weight.DemiBold); p.setFont(f)
            p.setPen(QColor("#FFFFFF"))
            p.drawText(QRectF(x, r.top() + side * .28, 600, 44), Qt.AlignmentFlag.AlignLeft, "Evening Tide")
            f.setPixelSize(18); f.setWeight(QFont.Weight.Normal); p.setFont(f); p.setPen(QColor(theme.TEXT))
            p.drawText(QRectF(x, r.top() + side * .28 + 52, 600, 28), Qt.AlignmentFlag.AlignLeft, "Harbour Lights")
            p.setPen(QColor(theme.TEXT_DIM))
            p.drawText(QRectF(x, r.top() + side * .28 + 84, 600, 28), Qt.AlignmentFlag.AlignLeft, "Coastlines  ·  2025")
            f.setPixelSize(12); f.setWeight(QFont.Weight.DemiBold); p.setFont(f)
            p.drawText(QRectF(x, r.top() + side * .28 + 130, 600, 20), Qt.AlignmentFlag.AlignLeft,
                       "FLAC  ·  44.1 KHZ  ·  16 BIT  ·  STEREO")
            bars = 28
            for i in range(bars):          # a level meter in the family gradient
                h = (0.25 + 0.75 * abs(((i * 37) % 17) / 17 - .4)) * 70
                rr = QRectF(x + i * 12, r.top() + side * .28 + 220 - h, 8, h)
                p.fillRect(rr, theme.family_gradient(0, rr.bottom(), 0, rr.bottom() - 70, theme.LIT))
            return
        k = min(self.width() / self.frame.width(), self.height() / self.frame.height())
        w, h = self.frame.width() * k, self.frame.height() * k
        r = QRectF((self.width() - w) / 2, (self.height() - h) / 2, w, h)
        p.drawImage(r, self.frame)
        f = QFont("Inter"); f.setPixelSize(int(h * .042)); f.setWeight(QFont.Weight.DemiBold); p.setFont(f)
        sub = "You never told me the city looked like this at night."
        rr = QRectF(r.left(), r.bottom() - h * .16, r.width(), h * .07)
        p.setPen(QPen(QColor(0, 0, 0, 230), 4))
        for dx, dy in ((-2, 0), (2, 0), (0, -2), (0, 2)):
            p.drawText(rr.translated(dx, dy), Qt.AlignmentFlag.AlignCenter, sub)
        p.setPen(QColor("#FFFFFF"))
        p.drawText(rr, Qt.AlignmentFlag.AlignCenter, sub)
        if self.overlay:
            g = QLinearGradient(0, self.height() - 150, 0, self.height())
            g.setColorAt(0, QColor(0, 0, 0, 0)); g.setColorAt(1, QColor(0, 0, 0, 220))
            p.fillRect(QRectF(0, self.height() - 150, self.width(), 150), g)
            f.setPixelSize(16); p.setFont(f); p.setPen(QColor("#FFFFFF"))
            p.drawText(QRectF(24, 18, 900, 30), Qt.AlignmentFlag.AlignLeft, "The Northern Line.mkv")


class Seek(QWidget):
    """The seek bar: played part in the family gradient, chapter ticks, the time under the mouse."""

    def __init__(self, pos=.12, hover=.41, dark=False):
        super().__init__()
        self.pos, self.hover, self.dark = pos, hover, dark
        self.setFixedHeight(30)

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        if not self.dark:
            p.fillRect(self.rect(), QColor(theme.BACKGROUND))
        m = 16
        track = QRectF(m, self.height() / 2 - 3, self.width() - 2 * m, 6)
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(theme.WIDGET if not self.dark else "#2A262C"))
        p.drawRoundedRect(track, 3, 3)
        buf = QRectF(track.left(), track.top(), track.width() * (self.pos + .06), track.height())
        p.setBrush(QColor(theme.PANEL_EDGE)); p.drawRoundedRect(buf, 3, 3)
        played = QRectF(track.left(), track.top(), track.width() * self.pos, track.height())
        p.setBrush(theme.family_gradient(played.left(), 0, played.right(), 0)); p.drawRoundedRect(played, 3, 3)
        for c in (.08, .23, .41, .58, .77, .9):          # chapters
            x = track.left() + track.width() * c
            p.fillRect(QRectF(x - .75, track.top(), 1.5, track.height()), QColor(0, 0, 0, 160))
        knob = QPointF(played.right(), track.center().y())
        p.setBrush(QColor("#FFFFFF")); p.drawEllipse(knob, 7, 7)
        p.setBrush(QColor(theme.ACCENT)); p.drawEllipse(knob, 3, 3)
        if self.hover is not None:
            x = track.left() + track.width() * self.hover
            p.setPen(QPen(QColor(255, 255, 255, 160), 1)); p.drawLine(QPointF(x, track.top() - 4), QPointF(x, track.bottom() + 4))


def tip(win: QWidget, seek: Seek, text: str, chapter: str):
    """The tooltip over the seek bar (drawn on the screenshot)."""
    pos = seek.mapTo(win, seek.rect().topLeft())
    x = pos.x() + 16 + (seek.width() - 32) * seek.hover

    def paint(p: QPainter):
        f = QFont("Inter"); f.setPixelSize(12); f.setWeight(QFont.Weight.DemiBold); p.setFont(f)
        lines = [text, chapter]
        w = max(p.fontMetrics().horizontalAdvance(t) for t in lines) + 20
        r = QRectF(x - w / 2, pos.y() - 50, w, 44)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.setPen(QPen(QColor(theme.PANEL_EDGE), 1)); p.setBrush(QColor(theme.MENU)); p.drawRoundedRect(r, 5, 5)
        p.setPen(QColor("#FFFFFF")); p.drawText(r.adjusted(0, 4, 0, -22), Qt.AlignmentFlag.AlignCenter, text)
        p.setPen(QColor(theme.TEXT_DIM)); p.drawText(r.adjusted(0, 22, 0, -4), Qt.AlignmentFlag.AlignCenter, chapter)
    return paint


def controls(time_text: str, playing=True, dark=False) -> QWidget:
    bar = QWidget()
    if dark:
        bar.setStyleSheet("background: transparent;")
    lay = QHBoxLayout(bar)
    lay.setContentsMargins(12, 2, 12, 8)
    lay.setSpacing(4)

    def gap():
        line = QFrame(); line.setFixedSize(1, 22); line.setStyleSheet(f"background: {theme.PANEL_EDGE};")
        lay.addSpacing(6); lay.addWidget(line); lay.addSpacing(6)

    lay.addWidget(widgets.SmallKey("pause" if playing else "play", "Play / Pause (Space)"))
    lay.addWidget(widgets.SmallKey("stop", "Stop (.)"))
    gap()
    for name, tipt in (("skipback", "Previous (PgUp)"), ("skipnext", "Next (PgDn)"), ("framestep", "Frame step")):
        lay.addWidget(widgets.SmallKey(name, tipt))
    gap()
    lay.addWidget(widgets.SmallKey("volume", "Mute (Ctrl+M)"))
    vol = Volume(); lay.addWidget(vol)
    lay.addSpacing(10)
    t = QLabel(time_text); t.setObjectName("time"); lay.addWidget(t)
    lay.addStretch(1)
    for name, tipt, on in (("subtitles", "Subtitles", True), ("audiotrack", "Audio track", False),
                           ("playlist", "Playlist (Ctrl+7)", not dark), ("fullscreen", "Full screen", dark)):
        k = widgets.SmallKey(name, tipt, True); k.setChecked(on); lay.addWidget(k)
    return bar


class Volume(QWidget):
    def __init__(self, level=.72):
        super().__init__()
        self.level = level
        self.setFixedSize(110, 34)

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        track = QRectF(6, self.height() / 2 - 2.5, self.width() - 12, 5)
        p.setPen(Qt.PenStyle.NoPen); p.setBrush(QColor(theme.WIDGET)); p.drawRoundedRect(track, 2.5, 2.5)
        fill = QRectF(track.left(), track.top(), track.width() * self.level, track.height())
        p.setBrush(theme.family_gradient(fill.left(), 0, fill.right(), 0)); p.drawRoundedRect(fill, 2.5, 2.5)
        p.setBrush(QColor("#FFFFFF")); p.drawEllipse(QPointF(fill.right(), track.center().y()), 6, 6)


MUSIC = [("01 Low Water.flac", "3:58"), ("02 Evening Tide.flac", "4:37"), ("03 Harbour Lights.flac", "5:12"),
         ("04 Salt Road.flac", "3:41"), ("05 North Pier.flac", "6:03"), ("06 Lanterns.flac", "4:20"),
         ("07 Coastlines.flac", "7:15")]


class Playlist(QWidget):
    def __init__(self, current=0, items=None):
        super().__init__()
        self.current = current
        self.items = items or PLAYLIST
        self.setFixedWidth(330)

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillRect(self.rect(), QColor(theme.BACKGROUND))
        p.setPen(QPen(QColor(theme.PANEL_EDGE), 1)); p.drawLine(0, 0, 0, self.height())
        f = QFont("Inter"); f.setPixelSize(11); f.setWeight(QFont.Weight.DemiBold); p.setFont(f)
        p.setPen(QColor(theme.TEXT_DIM))
        head = "PLAYLIST   ·   6 FILES   ·   5:10:47" if self.items is PLAYLIST else "PLAYLIST   ·   7 FILES   ·   35:06"
        p.drawText(QRectF(16, 10, 300, 20), Qt.AlignmentFlag.AlignLeft, head)
        f2 = QFont("Inter"); f2.setPixelSize(13)
        for i, (name, dur) in enumerate(self.items):
            r = QRectF(4, 40 + i * 34, self.width() - 8, 30)
            if i == self.current:
                p.setPen(Qt.PenStyle.NoPen)
                p.setBrush(theme.family_gradient(r.left(), 0, r.right(), 0, theme.LIT))
                p.drawRoundedRect(r, 4, 4)
            p.setFont(f2)
            p.setPen(QColor("#FFFFFF") if i == self.current else QColor(theme.TEXT))
            if i == self.current:
                widgets.draw_icon(p, "play", QRectF(r.left() + 8, r.top() + 7, 16, 16), QColor("#FFFFFF"), 1.6)
            p.drawText(r.adjusted(32, 0, -70, 0), Qt.AlignmentFlag.AlignVCenter,
                       p.fontMetrics().elidedText(name, Qt.TextElideMode.ElideRight, int(r.width()) - 104))
            p.setPen(QColor("#FFFFFF") if i == self.current else QColor(theme.TEXT_DIM))
            p.drawText(r.adjusted(0, 0, -10, 0), Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignRight, dur)


EXTRA = """
QLabel#time {{ color: {text}; font-size: 13px; font-weight: 600; }}
QWidget#status {{ background: {bg}; border-top: 1px solid {edge}; }}
QLabel#stat {{ color: {dim}; font-size: 12px; }}
QLabel#statstrong {{ color: {text}; font-size: 12px; font-weight: 600; }}
"""


def status(parts, state="PLAYING") -> QWidget:
    bar = QWidget(); bar.setObjectName("status")
    lay = QHBoxLayout(bar)
    lay.setContentsMargins(14, 4, 14, 6)
    lay.setSpacing(24)
    s = QLabel(state); s.setObjectName("statstrong"); lay.addWidget(s)
    for t in parts:
        lab = QLabel(t); lab.setObjectName("stat"); lay.addWidget(lab)
    lay.addStretch(1)
    return bar


def window(video: Video, time_text: str, parts, title: str, seek_pos=.12, hover=.41, playlist=True, current=0,
           items=None):
    win = QMainWindow()
    win.resize(1280, 800)
    for t in MENUS:
        win.menuBar().addMenu(t)
    w = QWidget()
    col = QVBoxLayout(w)
    col.setContentsMargins(0, 0, 0, 0)
    col.setSpacing(0)
    col.addWidget(widgets.Header("MEDIA PLAYER"))
    mid = QHBoxLayout()
    mid.setSpacing(0)
    mid.addWidget(video, 1)
    if playlist:
        mid.addWidget(Playlist(current, items))
    col.addLayout(mid, 1)
    seek = Seek(seek_pos, hover)
    col.addWidget(seek)
    col.addWidget(controls(time_text))
    col.addWidget(status(parts))
    win.setCentralWidget(w)
    win.seek = seek
    return win


def settle():
    end = time.time() + .5
    while time.time() < end:
        app.processEvents(); time.sleep(.02)


def shoot(win, name, menu=None, active=0, extra=None):
    win.show(); settle()
    img = win.grab().toImage()
    p = QPainter(img)
    if extra:
        extra(p)
    if menu:
        m = QMenu(win)
        for text in MENUS[menu]:
            if text == "-":
                m.addSeparator()
            elif text in ("&Audio", "Su&btitles", "&Volume", "&After playback", "Recent &files", "&Chapters", "&Zoom",
                          "&Video frame", "&Finish"):
                m.addMenu(text)
            else:
                a = m.addAction(text)
                if text in CHECKED:
                    a.setCheckable(True); a.setChecked(True)
        m.resize(m.sizeHint())
        act = next(a for a in win.menuBar().actions() if a.text() == menu)
        at = win.menuBar().actionGeometry(act).bottomLeft()
        m.popup(win.mapToGlobal(at)); app.processEvents()
        m.setActiveAction([a for a in m.actions() if not a.isSeparator()][active]); app.processEvents()
        p.drawPixmap(at, m.grab()); m.hide()
    p.end()
    img.save(str(out / f"{name}.png"))
    print(out / f"{name}.png")
    win.close()


def fullscreen():
    win = QMainWindow()
    win.resize(1280, 800)
    w = QWidget()
    w.setStyleSheet("background: #000000;")
    col = QVBoxLayout(w)
    col.setContentsMargins(0, 0, 0, 0)
    col.setSpacing(0)
    v = Video(FRAME, overlay=True)
    col.addWidget(v, 1)
    win.setCentralWidget(w)
    win.show(); settle()
    img = win.grab().toImage()
    # The controls slide in over the picture when the mouse moves to the bottom (MPC-HC's full screen).
    holder = QWidget()
    holder.resize(1280, 74)
    holder.setStyleSheet("background: transparent;")
    hl = QVBoxLayout(holder); hl.setContentsMargins(0, 0, 0, 0); hl.setSpacing(0)
    hl.addWidget(Seek(.12, None, dark=True))
    hl.addWidget(controls("00:12:34 / 01:45:00", dark=True))
    holder.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
    holder.show(); settle()
    pm = holder.grab()
    p = QPainter(img)
    p.drawPixmap(0, 800 - 80, pm)
    p.end()
    img.save(str(out / "fullscreen.png"))
    print(out / "fullscreen.png")
    holder.close(); win.close()


def all_shots():
    app.setPalette(theme.palette())
    app.setStyleSheet(theme.stylesheet() + EXTRA.format(text=theme.TEXT, dim=theme.TEXT_DIM, bg=theme.BACKGROUND,
                                                        edge=theme.PANEL_EDGE))
    video_parts = ["1920 x 1080  ·  23.976 FPS", "H.264  ·  AAC 48 KHZ STEREO", "SUBTITLES: ENGLISH", "1.00x"]
    if theme.FINISH == "aurora":
        shoot(window(Video(FRAME), "00:12:34 / 01:45:00", video_parts, "The Northern Line.mkv"), "main_aurora")
        return
    w = window(Video(FRAME), "00:12:34 / 01:45:00", video_parts, "The Northern Line.mkv")
    shoot(w, "main", extra=None)
    w = window(Video(FRAME), "00:12:34 / 01:45:00", video_parts, "The Northern Line.mkv")
    w.show(); settle()
    shoot(w, "seek", extra=tip(w, w.seek, "00:43:03", "CHAPTER 3  ·  THE LAST TRAIN"))
    shoot(window(Video(FRAME), "00:12:34 / 01:45:00", video_parts, "The Northern Line.mkv", hover=None), "play_menu",
          menu="&Play", active=2)
    shoot(window(Video(audio=True), "00:01:52 / 00:04:37", ["FLAC  ·  44.1 KHZ  ·  16 BIT  ·  STEREO", "1.00x"],
                 "Evening Tide.flac", seek_pos=.4, hover=None, current=1, items=MUSIC), "music")
    fullscreen()


all_shots()
theme.set_finish("aurora")
all_shots()
