"""Real media files made here (no downloads), for tests and screenshots: python tools/sample_media.py OUT_DIR

- wav(): a tone (16-bit PCM WAV, Python's wave module);
- avi(): a Motion-JPEG AVI written by hand (Qt draws each frame as a JPEG: its own colour and number, so tests can
  tell which frame is showing); FFmpeg, inside Qt Multimedia, plays it;
- srt(): subtitles for it."""
from __future__ import annotations

import math
import struct
import sys
import wave
from pathlib import Path

COLOURS = ["#C2183F", "#E2502A", "#F07A2A", "#7A2230", "#D83A6A", "#FFB070"]


def wav(path: Path, seconds: float = 2.0, freq: float = 440.0, rate: int = 22050) -> Path:
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(rate)
        frames = bytearray()
        for i in range(int(seconds * rate)):
            v = int(9000 * math.sin(2 * math.pi * freq * i / rate))
            frames += struct.pack("<hh", v, v)
        w.writeframes(bytes(frames))
    return path


def colour_of(frame: int, fps: int) -> str:
    """The colour of the second a frame is in (each second of the test video has its own colour)."""
    return COLOURS[(frame // fps) % len(COLOURS)]


def _jpeg(index: int, fps: int, size: tuple[int, int]) -> bytes:
    from PySide6.QtCore import QBuffer, QIODevice, QRectF, Qt
    from PySide6.QtGui import QColor, QFont, QImage, QPainter
    img = QImage(size[0], size[1], QImage.Format.Format_RGB32)
    img.fill(QColor(colour_of(index, fps)))
    p = QPainter(img)
    f = QFont(); f.setPixelSize(size[1] // 3); f.setBold(True)
    p.setFont(f)
    p.setPen(QColor("#FFFFFF"))
    p.drawText(QRectF(0, 0, size[0], size[1]), Qt.AlignmentFlag.AlignCenter, str(index))
    p.end()
    buf = QBuffer()
    buf.open(QIODevice.OpenModeFlag.WriteOnly)
    img.save(buf, "JPG", 85)
    return bytes(buf.data())


def _chunk(fourcc: bytes, data: bytes) -> bytes:
    return fourcc + struct.pack("<I", len(data)) + data + (b"\0" if len(data) % 2 else b"")


def _list(kind: bytes, data: bytes) -> bytes:
    return b"LIST" + struct.pack("<I", len(data) + 4) + kind + data


def avi(path: Path, seconds: float = 3.0, fps: int = 10, size: tuple[int, int] = (320, 180)) -> Path:
    """A video of `seconds`: each second a different colour, every frame numbered."""
    count = int(seconds * fps)
    return write_avi(path, [_jpeg(i, fps, size) for i in range(count)], fps, size)


def avi_from(path: Path, image, seconds: float = 4.0, fps: int = 10, size: tuple[int, int] = (1280, 720)) -> Path:
    """A slow pan across a picture (the screenshots' film)."""
    from PySide6.QtCore import QBuffer, QIODevice, QRect
    frames = []
    count = int(seconds * fps)
    for i in range(count):
        dx = int((image.width() - size[0]) * i / max(1, count - 1)) if image.width() > size[0] else 0
        part = image.copy(QRect(dx, max(0, (image.height() - size[1]) // 2), size[0], size[1]))
        buf = QBuffer(); buf.open(QIODevice.OpenModeFlag.WriteOnly); part.save(buf, "JPG", 88)
        frames.append(bytes(buf.data()))
    return write_avi(path, frames, fps, size)


def write_avi(path: Path, frames: list[bytes], fps: int, size: tuple[int, int]) -> Path:
    count = len(frames)
    w, h = size
    biggest = max(len(f) for f in frames)
    avih = struct.pack("<10I4I", 1_000_000 // fps, biggest * fps, 0, 0x10, count, 0, 1, biggest, w, h, 0, 0, 0, 0)
    strh = b"vids" + b"MJPG" + struct.pack("<IHHIIIIIIiI4h", 0, 0, 0, 0, 1, fps, 0, count, biggest, -1, 0, 0, 0, w, h)
    strf = struct.pack("<IiiHH4sIiiII", 40, w, h, 1, 24, b"MJPG", w * h * 3, 0, 0, 0, 0)
    hdrl = _list(b"hdrl", _chunk(b"avih", avih) + _list(b"strl", _chunk(b"strh", strh) + _chunk(b"strf", strf)))
    movi_data, index, offset = b"", b"", 4
    for f in frames:
        c = _chunk(b"00dc", f)
        index += b"00dc" + struct.pack("<III", 0x10, offset, len(f))
        movi_data += c
        offset += len(c)
    body = b"AVI " + hdrl + _list(b"movi", movi_data) + _chunk(b"idx1", index)
    path.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)
    return path


def srt(path: Path, cues: list[tuple[float, float, str]] | None = None) -> Path:
    cues = cues or [(0.2, 1.2, "First line"), (1.5, 2.6, "Second line\nwith two rows")]

    def t(s):
        ms = int(round(s * 1000))
        return f"{ms // 3600000:02d}:{ms // 60000 % 60:02d}:{ms // 1000 % 60:02d},{ms % 1000:03d}"
    text = "".join(f"{i}\n{t(a)} --> {t(b)}\n{txt}\n\n" for i, (a, b, txt) in enumerate(cues, 1))
    path.write_text(text, encoding="utf-8")
    return path


if __name__ == "__main__":
    from PySide6.QtGui import QGuiApplication
    app = QGuiApplication([])
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "media")
    out.mkdir(parents=True, exist_ok=True)
    for p in (avi(out / "clip.avi"), wav(out / "tone.wav"), srt(out / "clip.srt")):
        print(p)
