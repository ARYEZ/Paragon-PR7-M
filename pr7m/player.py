"""PR7-M's own player widgets, in family colours:

- VideoView: the picture (frames from a QVideoSink, painted by QVideoFrame.paint, so subtitles can be drawn over them
  in Inter), and for music the cover, the tags and a level meter worked out from the audio itself;
- SeekBar: played part in the family gradient, click / drag to seek, the time under the pointer, A-B marks;
- VolumeSlider; PlaylistModel / PlaylistView (a plain Python list model: never QStandardItemModel)."""

from __future__ import annotations

import math
import os

from PySide6.QtCore import QAbstractListModel, QModelIndex, QPointF, QRectF, QSize, Qt, Signal
from PySide6.QtGui import QColor, QFont, QImage, QPainter, QPainterPath, QPen
from PySide6.QtMultimedia import QAudioBuffer, QVideoFrame, QVideoSink
from PySide6.QtWidgets import QListView, QStyle, QStyledItemDelegate, QToolTip, QWidget

from . import theme, widgets
from .media import hms

BARS = 28


class VideoView(QWidget):
    clicked = Signal()
    doubleClicked = Signal()
    wheeled = Signal(int)
    mouseMoved = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.sink = QVideoSink(self)
        self.sink.videoFrameChanged.connect(self._frame)
        self.frame = QVideoFrame()
        self.subtitle = ""
        self.audio_only = False
        self.cover = QImage()
        self.tags: dict[str, str] = {}
        self.levels = [0.0] * BARS
        self.frames_seen = 0
        self.setMouseTracking(True)
        self.setAttribute(Qt.WidgetAttribute.WA_OpaquePaintEvent)

    def _frame(self, f: QVideoFrame):
        self.frame = f
        self.frames_seen += 1
        self.update()

    def clear(self):
        self.frame = QVideoFrame()
        self.subtitle = ""
        self.levels = [0.0] * BARS
        self.update()

    def set_levels(self, buf: QAudioBuffer):
        """A level per bar from the samples just played (how loud each slice of the buffer is), smoothed."""
        try:
            data = buf.constData()
            fmt = buf.format()
            n = buf.sampleCount()
            if not n:
                return
            import array
            sample_bytes = fmt.bytesPerSample()
            if sample_bytes == 2:
                values = array.array("h", bytes(data)[: n * 2])
                scale = 32768.0
            elif sample_bytes == 4 and fmt.sampleFormat().name == "Float":
                values = array.array("f", bytes(data)[: n * 4])
                scale = 1.0
            else:
                return
            step = max(1, len(values) // BARS)
            new = []
            for i in range(BARS):
                chunk = values[i * step:(i + 1) * step]
                rms = math.sqrt(sum(v * v for v in chunk) / len(chunk)) / scale if len(chunk) else 0
                new.append(min(1.0, rms * 3))
            self.levels = [max(n_, o * 0.8) for n_, o in zip(new, self.levels)]
            if self.audio_only:
                self.update()
        except Exception:  # noqa: BLE001 - a format we don't measure: no meter
            pass

    def video_rect(self) -> QRectF:
        s = self.frame.size() if self.frame.isValid() else QSize()
        if s.isEmpty():
            return QRectF()
        k = min(self.width() / s.width(), self.height() / s.height())
        w, h = s.width() * k, s.height() * k
        return QRectF((self.width() - w) / 2, (self.height() - h) / 2, w, h)

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)
        p.fillRect(self.rect(), QColor("#000000"))
        if self.audio_only:
            self._paint_audio(p)
            return
        r = self.video_rect()
        if not r.isEmpty():
            self.frame.paint(p, r, QVideoFrame.PaintOptions())
        elif not self.tags.get("title"):
            self._paint_idle(p)
        if self.subtitle:
            self._paint_subtitle(p, r if not r.isEmpty() else QRectF(self.rect()))

    def _paint_idle(self, p: QPainter):
        h = min(self.height() * .16, 90)
        widgets.draw_mark(p, QRectF(self.width() / 2 - h * 2.3, self.height() / 2 - h / 2, h * 4.6, h), glow=True)

    def _paint_subtitle(self, p: QPainter, r: QRectF):
        size = max(14, int(r.height() * .045))
        f = QFont("Inter"); f.setPixelSize(size); f.setWeight(QFont.Weight.DemiBold)
        p.setFont(f)
        lines = self.subtitle.split("\n")
        lh = p.fontMetrics().height()
        y = r.bottom() - r.height() * .07 - lh * len(lines)
        for ln in lines:
            path = QPainterPath()
            w = p.fontMetrics().horizontalAdvance(ln)
            path.addText(QPointF(r.center().x() - w / 2, y + p.fontMetrics().ascent()), f, ln)
            p.strokePath(path, QPen(QColor(0, 0, 0, 230), max(3, size / 7), Qt.PenStyle.SolidLine,
                                    Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin))
            p.fillPath(path, QColor("#FFFFFF"))
            y += lh

    def _paint_audio(self, p: QPainter):
        side = min(self.height() * .62, self.width() * .4)
        r = QRectF(self.width() / 2 - side - 30, (self.height() - side) / 2, side, side)
        if not self.cover.isNull():
            p.fillRect(r.translated(0, 8), QColor(0, 0, 0, 200))
            p.drawImage(r, self.cover)
        else:
            p.fillRect(r, QColor(theme.PANEL))
            widgets.draw_icon(p, "audiotrack", r.adjusted(side * .3, side * .3, -side * .3, -side * .3),
                              QColor(theme.TEXT_DIM), 2.0)
        x = r.right() + 48
        y = r.top() + side * .28
        f = QFont("Inter"); f.setPixelSize(30); f.setWeight(QFont.Weight.DemiBold); p.setFont(f)
        p.setPen(QColor("#FFFFFF"))
        width = max(200, self.width() - x - 30)
        p.drawText(QRectF(x, y, width, 44), Qt.AlignmentFlag.AlignLeft,
                   p.fontMetrics().elidedText(self.tags.get("title", ""), Qt.TextElideMode.ElideRight, int(width)))
        f.setPixelSize(18); f.setWeight(QFont.Weight.Normal); p.setFont(f)
        p.setPen(QColor(theme.TEXT))
        p.drawText(QRectF(x, y + 52, width, 28), Qt.AlignmentFlag.AlignLeft, self.tags.get("artist", ""))
        p.setPen(QColor(theme.TEXT_DIM))
        p.drawText(QRectF(x, y + 84, width, 28), Qt.AlignmentFlag.AlignLeft, self.tags.get("album", ""))
        f.setPixelSize(12); f.setWeight(QFont.Weight.DemiBold); p.setFont(f)
        p.drawText(QRectF(x, y + 130, width, 20), Qt.AlignmentFlag.AlignLeft, self.tags.get("format", "").upper())
        for i, level in enumerate(self.levels):
            h = 4 + level * 66
            rr = QRectF(x + i * 12, y + 220 - h, 8, h)
            p.fillRect(rr, theme.family_gradient(0, y + 220, 0, y + 150, theme.LIT))

    def mouseReleaseEvent(self, e):
        if e.button() == Qt.MouseButton.LeftButton:
            self.clicked.emit()

    def mouseDoubleClickEvent(self, e):
        if e.button() == Qt.MouseButton.LeftButton:
            self.doubleClicked.emit()

    def wheelEvent(self, e):
        steps = e.angleDelta().y() // 120
        if steps:
            self.wheeled.emit(int(steps))

    def mouseMoveEvent(self, e):
        self.mouseMoved.emit()


class SeekBar(QWidget):
    seek = Signal(int)

    def __init__(self, parent=None, dark: bool = False):
        super().__init__(parent)
        self.duration = 0
        self.position = 0
        self.buffered = 0.0
        self.a = self.b = None
        self.dark = dark
        self.hover = None
        self._dragging = False
        self.setFixedHeight(30)
        self.setMouseTracking(True)
        self.setCursor(Qt.CursorShape.PointingHandCursor)

    def track(self) -> QRectF:
        return QRectF(16, self.height() / 2 - 3, self.width() - 32, 6)

    def _x(self, ms) -> float:
        t = self.track()
        return t.left() + t.width() * (ms / self.duration if self.duration else 0)

    def ms_at(self, x: float) -> int:
        t = self.track()
        return int(min(max((x - t.left()) / t.width(), 0), 1) * self.duration) if self.duration else 0

    def set_position(self, ms: int):
        if not self._dragging:
            self.position = ms
            self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        if not self.dark:
            p.fillRect(self.rect(), QColor(theme.BACKGROUND))
        t = self.track()
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(theme.WIDGET) if not self.dark else QColor("#2A262C"))
        p.drawRoundedRect(t, 3, 3)
        if self.duration:
            if self.buffered:
                p.setBrush(QColor(theme.PANEL_EDGE))
                p.drawRoundedRect(QRectF(t.left(), t.top(), t.width() * min(1, self.buffered), t.height()), 3, 3)
            if self.a is not None:
                x1 = self._x(self.a)
                x2 = self._x(self.b) if self.b is not None else x1 + 2
                ab = QColor(theme.LED_ORANGE); ab.setAlphaF(.35)
                p.setBrush(ab); p.drawRect(QRectF(x1, t.top() - 4, max(2, x2 - x1), t.height() + 8))
            played = QRectF(t.left(), t.top(), self._x(self.position) - t.left(), t.height())
            p.setBrush(theme.family_gradient(played.left(), 0, max(played.right(), played.left() + 1), 0))
            p.drawRoundedRect(played, 3, 3)
            knob = QPointF(played.right(), t.center().y())
            p.setBrush(QColor("#FFFFFF")); p.drawEllipse(knob, 7, 7)
            p.setBrush(QColor(theme.ACCENT)); p.drawEllipse(knob, 3, 3)
            if self.hover is not None:
                p.setPen(QPen(QColor(255, 255, 255, 160), 1))
                p.drawLine(QPointF(self.hover, t.top() - 4), QPointF(self.hover, t.bottom() + 4))

    def mousePressEvent(self, e):
        if e.button() == Qt.MouseButton.LeftButton and self.duration:
            self._dragging = True
            self.position = self.ms_at(e.position().x())
            self.seek.emit(self.position)
            self.update()

    def mouseMoveEvent(self, e):
        self.hover = e.position().x()
        if self.duration:
            QToolTip.showText(e.globalPosition().toPoint(), hms(self.ms_at(self.hover)), self)
        if self._dragging:
            self.position = self.ms_at(e.position().x())
            self.seek.emit(self.position)
        self.update()

    def mouseReleaseEvent(self, e):
        self._dragging = False

    def leaveEvent(self, e):
        self.hover = None
        self.update()


class VolumeSlider(QWidget):
    changed = Signal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.value = 100
        self.muted = False
        self.setFixedSize(110, 34)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setToolTip("Volume")

    def set_value(self, v: int, emit: bool = True):
        self.value = int(min(max(v, 0), 100))
        self.update()
        if emit:
            self.changed.emit(self.value)

    def _from(self, x: float):
        self.set_value(round((x - 6) / (self.width() - 12) * 100))

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        t = QRectF(6, self.height() / 2 - 2.5, self.width() - 12, 5)
        p.setPen(Qt.PenStyle.NoPen); p.setBrush(QColor(theme.WIDGET)); p.drawRoundedRect(t, 2.5, 2.5)
        fill = QRectF(t.left(), t.top(), t.width() * self.value / 100, t.height())
        if self.muted:
            p.setBrush(QColor(theme.TEXT_DIM))
        else:
            p.setBrush(theme.family_gradient(fill.left(), 0, max(fill.right(), fill.left() + 1), 0))
        p.drawRoundedRect(fill, 2.5, 2.5)
        p.setBrush(QColor("#FFFFFF")); p.drawEllipse(QPointF(fill.right(), t.center().y()), 6, 6)

    def mousePressEvent(self, e):
        self._from(e.position().x())

    def mouseMoveEvent(self, e):
        if e.buttons() & Qt.MouseButton.LeftButton:
            self._from(e.position().x())

    def wheelEvent(self, e):
        self.set_value(self.value + (5 if e.angleDelta().y() > 0 else -5))


class PlaylistModel(QAbstractListModel):
    def __init__(self):
        super().__init__()
        self.files: list[str] = []
        self.durations: dict[str, int] = {}
        self.current = -1

    def rowCount(self, parent=QModelIndex()):
        return 0 if parent.isValid() else len(self.files)

    def data(self, index, role=Qt.ItemDataRole.DisplayRole):
        if not index.isValid():
            return None
        f = self.files[index.row()]
        if role == Qt.ItemDataRole.DisplayRole:
            return os.path.basename(f)
        if role == Qt.ItemDataRole.ToolTipRole:
            return f
        if role == Qt.ItemDataRole.UserRole:
            d = self.durations.get(f)
            return hms(d, always_hours=False) if d else ""
        return None

    def set_files(self, files: list[str], current: int = -1):
        self.beginResetModel()
        self.files, self.current = list(files), current
        self.endResetModel()

    def set_current(self, i: int):
        self.current = i
        self.dataChanged.emit(self.index(0), self.index(max(0, len(self.files) - 1)))

    def set_duration(self, path: str, ms: int):
        if ms > 0 and self.durations.get(path) != ms:
            self.durations[path] = ms
            if path in self.files:
                i = self.files.index(path)
                self.dataChanged.emit(self.index(i), self.index(i))

    def total(self) -> int:
        return sum(self.durations.get(f, 0) for f in self.files)

    def move(self, src: int, dst: int):
        if 0 <= src < len(self.files) and 0 <= dst < len(self.files) and src != dst:
            cur = self.files[self.current] if 0 <= self.current < len(self.files) else None
            self.beginResetModel()
            self.files.insert(dst, self.files.pop(src))
            self.current = self.files.index(cur) if cur else -1
            self.endResetModel()

    def remove(self, rows: list[int]):
        cur = self.files[self.current] if 0 <= self.current < len(self.files) else None
        self.beginResetModel()
        for r in sorted(set(rows), reverse=True):
            if 0 <= r < len(self.files):
                self.files.pop(r)
        self.current = self.files.index(cur) if cur in self.files else -1
        self.endResetModel()


class _Delegate(QStyledItemDelegate):
    def paint(self, p: QPainter, option, index):
        model = index.model()
        r = QRectF(option.rect).adjusted(4, 2, -4, -2)
        current = index.row() == model.current
        selected = bool(option.state & QStyle.StateFlag.State_Selected)
        p.save()
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        if current:
            p.setPen(Qt.PenStyle.NoPen)
            p.setBrush(theme.family_gradient(r.left(), 0, r.right(), 0, theme.LIT))
            p.drawRoundedRect(r, 4, 4)
        elif selected:
            c = QColor(theme.ACCENT); c.setAlphaF(.18)
            p.setPen(Qt.PenStyle.NoPen); p.setBrush(c); p.drawRoundedRect(r, 4, 4)
        on_band = QColor("#FFFFFF") if current else None
        if current:
            widgets.draw_icon(p, "play", QRectF(r.left() + 8, r.center().y() - 8, 16, 16), on_band, 1.6)
        f = QFont("Inter"); f.setPixelSize(13); p.setFont(f)
        dur = index.data(Qt.ItemDataRole.UserRole) or ""
        dw = p.fontMetrics().horizontalAdvance(dur) + 12
        p.setPen(on_band or QColor(theme.TEXT))
        name = p.fontMetrics().elidedText(index.data(), Qt.TextElideMode.ElideRight, int(r.width() - 40 - dw))
        p.drawText(r.adjusted(32, 0, -dw, 0), Qt.AlignmentFlag.AlignVCenter, name)
        p.setPen(on_band or QColor(theme.TEXT_DIM))
        p.drawText(r.adjusted(0, 0, -10, 0), Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignRight, dur)
        p.restore()

    def sizeHint(self, option, index):
        return QSize(200, 34)


class PlaylistView(QListView):
    activated_row = Signal(int)

    def __init__(self, model: PlaylistModel, parent=None):
        super().__init__(parent)
        self.setModel(model)
        self.setItemDelegate(_Delegate(self))
        self.setSelectionMode(QListView.SelectionMode.ExtendedSelection)
        self.setFrameShape(QListView.Shape.NoFrame)
        self.setDragDropMode(QListView.DragDropMode.NoDragDrop)
        self.doubleClicked.connect(lambda idx: self.activated_row.emit(idx.row()))
        self.setStyleSheet(f"QListView {{ background: {theme.BACKGROUND}; border: none; }}")

    def keyPressEvent(self, e):
        if e.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter) and self.currentIndex().isValid():
            self.activated_row.emit(self.currentIndex().row())
        else:
            super().keyPressEvent(e)
