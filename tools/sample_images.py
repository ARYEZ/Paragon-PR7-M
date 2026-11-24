"""Stand-in photos drawn with Qt (no outside files), for the mock-up, screenshots and tests:
python tools/sample_images.py OUT_DIR  writes a folder of JPEG / PNG pictures."""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path

from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QColor, QImage, QLinearGradient, QPainter, QPainterPath, QRadialGradient

KINDS = ["hills", "sea", "city", "dunes", "lake", "flowers"]
NAMES = ["Dusk over the ridge", "Harbour at sunset", "City lights", "Dunes at noon", "Mountain lake", "Poppies",
         "Last light", "Evening tide", "Night market", "Red sands", "Still water", "Wildflowers"]


def _sky(p, w, h, top, mid, low, horizon=.65):
    g = QLinearGradient(0, 0, 0, h * horizon)
    g.setColorAt(0, QColor(top)); g.setColorAt(.7, QColor(mid)); g.setColorAt(1, QColor(low))
    p.fillRect(QRectF(0, 0, w, h), g)


def _sun(p, x, y, r, colour="#FFE2C2", glow=(255, 200, 150)):
    rg = QRadialGradient(QPointF(x, y), r * 4)
    rg.setColorAt(0, QColor(*glow, 170)); rg.setColorAt(1, QColor(*glow, 0))
    p.fillRect(QRectF(x - r * 4, y - r * 4, r * 8, r * 8), rg)
    p.setPen(Qt.PenStyle.NoPen); p.setBrush(QColor(colour)); p.drawEllipse(QPointF(x, y), r, r)


def _ridge(p, rnd, w, h, base, rough, colour, steps=8):
    path = QPainterPath(QPointF(0, h))
    x = 0.0
    while x <= w + 1:
        path.lineTo(x, base - rnd.uniform(0, rough))
        x += w / rnd.randint(steps, steps + 6)
    path.lineTo(w, h); path.closeSubpath()
    p.setPen(Qt.PenStyle.NoPen); p.setBrush(QColor(colour)); p.drawPath(path)


def photo(seed: int, kind: str | None = None, w: int = 1800, h: int = 1200) -> QImage:
    rnd = random.Random(seed)
    kind = kind or KINDS[seed % len(KINDS)]
    img = QImage(w, h, QImage.Format.Format_RGB32)
    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.Antialiasing)
    if kind == "hills":
        _sky(p, w, h, "#2A0A1E", rnd.choice(["#C2183F", "#D83A6A"]), "#FFB070")
        _sun(p, rnd.uniform(.3, .7) * w, h * .45, h * .07)
        for i, c in enumerate(("#5A1426", "#33101A", "#160A0E")):
            _ridge(p, rnd, w, h, h * (.6 + .12 * i), h * .16, c)
    elif kind == "sea":
        _sky(p, w, h, "#1A0610", "#E2502A", "#FFC890", .6)
        _sun(p, w * rnd.uniform(.35, .65), h * .55, h * .08)
        sea = QLinearGradient(0, h * .6, 0, h)
        sea.setColorAt(0, QColor("#7A2230")); sea.setColorAt(1, QColor("#1A0810"))
        p.fillRect(QRectF(0, h * .6, w, h * .4), sea)
        p.setPen(QColor(255, 210, 170, 120))
        for i in range(60):
            y = h * .62 + i * h * .006 + rnd.uniform(0, 4)
            x = w / 2 + rnd.uniform(-1, 1) * (40 + i * 6)
            p.drawLine(QPointF(x - 20 - i, y), QPointF(x + 20 + i, y))
    elif kind == "city":
        _sky(p, w, h, "#05030A", "#2A0A1E", "#6A1A2A", .75)
        p.setPen(Qt.PenStyle.NoPen)
        x = 0.0
        while x < w:
            bw, bh = rnd.uniform(.04, .09) * w, rnd.uniform(.2, .55) * h
            p.setBrush(QColor(rnd.choice(["#0E080C", "#140A10", "#1A0C12"])))
            p.drawRect(QRectF(x, h - bh, bw, bh))
            for wy in range(int(h - bh + 12), int(h - 10), 22):
                for wx in range(int(x + 8), int(x + bw - 8), 16):
                    if rnd.random() < .45:
                        p.setBrush(QColor(rnd.choice(["#FFC070", "#FF8A50", "#FFE0B0"])))
                        p.drawRect(QRectF(wx, wy, 7, 10))
            x += bw + rnd.uniform(2, 12)
    elif kind == "dunes":
        _sky(p, w, h, "#E89060", "#F6C08A", "#FFE2C2", .5)
        _sun(p, w * .78, h * .18, h * .05, "#FFF4E8", (255, 240, 220))
        for i, c in enumerate(("#D9783A", "#C25A28", "#9A3A1A", "#6A2412")):
            path = QPainterPath(QPointF(0, h))
            base = h * (.5 + .13 * i)
            for k in range(0, w + 40, 40):
                path.lineTo(k, base + math.sin(k / w * math.pi * (2 + i) + seed) * h * .05)
            path.lineTo(w, h); path.closeSubpath()
            p.setBrush(QColor(c)); p.drawPath(path)
    elif kind == "lake":
        _sky(p, w, h, "#3A1022", "#B8405A", "#F4A070", .55)
        _ridge(p, rnd, w, h, h * .55, h * .25, "#2A0E18", 5)
        water = QLinearGradient(0, h * .55, 0, h)
        water.setColorAt(0, QColor("#C86A5A")); water.setColorAt(1, QColor("#2A0E18"))
        p.fillRect(QRectF(0, h * .55, w, h * .45), water)
        p.setPen(QColor(255, 230, 210, 70))
        for i in range(40):
            y = h * .58 + rnd.uniform(0, h * .4)
            x = rnd.uniform(0, w)
            p.drawLine(QPointF(x, y), QPointF(x + rnd.uniform(30, 160), y))
    else:  # flowers
        g = QLinearGradient(0, 0, 0, h)
        g.setColorAt(0, QColor("#F4C8A0")); g.setColorAt(1, QColor("#5A2A1A"))
        p.fillRect(QRectF(0, 0, w, h), g)
        for _ in range(70):
            x, y = rnd.uniform(0, w), rnd.uniform(h * .3, h)
            r = rnd.uniform(.015, .05) * h * (y / h + .3)
            p.setPen(QColor(90, 40, 20, 160)); p.drawLine(QPointF(x, y), QPointF(x + rnd.uniform(-10, 10), h))
            p.setPen(Qt.PenStyle.NoPen)
            p.setBrush(QColor(rnd.choice(["#E0202A", "#F04A2A", "#D81840", "#F07A2A"])))
            for a in range(5):
                ang = a / 5 * math.tau + rnd.random()
                p.drawEllipse(QPointF(x + math.cos(ang) * r * .6, y + math.sin(ang) * r * .6), r * .6, r * .6)
            p.setBrush(QColor("#1A0808")); p.drawEllipse(QPointF(x, y), r * .3, r * .3)
    p.end()
    return img


def with_exif(path: Path, orientation: int = 1, camera: tuple[str, str] = ("FUJIFILM", "X-T5")) -> Path:
    """Give a JPEG camera details (Pillow): make / model, lens, date taken, exposure 1/250 s, f/5.6, ISO 200, 35 mm,
    flash off, GPS, and an orientation tag (6 = the camera was turned: show it turned a quarter right)."""
    from PIL import Image
    from PIL.TiffImagePlugin import IFDRational as Fraction   # EXIF fractions must be Pillow's own type
    with Image.open(path) as im:
        im.load()
        pixels = im.copy()
    ex = Image.Exif()
    ex[0x010F], ex[0x0110], ex[0x0112] = camera[0], camera[1], orientation
    ex[0x0132] = "2026:10:03 18:41:07"
    sub = ex.get_ifd(0x8769)
    sub.update({0x9003: "2026:10:03 18:41:07", 0x829A: Fraction(1, 250), 0x829D: Fraction(56, 10), 0x8827: 200,
                0x920A: Fraction(35, 1), 0xA405: 53, 0x9209: 16, 0xA434: "XF16-55mmF2.8 R LM WR"})
    gps = ex.get_ifd(0x8825)
    gps.update({1: "N", 2: (Fraction(54), Fraction(35), Fraction(503, 10)), 3: "W",
                4: (Fraction(5), Fraction(55), Fraction(484, 10))})
    pixels.save(path, "JPEG", quality=90, exif=ex)
    return path


def folder(out: Path, count: int = 12, size: tuple[int, int] = (1800, 1200)) -> list[Path]:
    """A folder of pictures named like a camera's, some upright, one PNG."""
    out.mkdir(parents=True, exist_ok=True)
    files = []
    for i in range(count):
        w, h = (size[1], size[0]) if i % 5 == 3 else size
        img = photo(i, KINDS[i % len(KINDS)], w, h)
        name = out / (f"IMG_{2041 + i}.jpg" if i != 7 else "Night market.png")
        img.save(str(name), quality=88)
        files.append(name)
    return files


if __name__ == "__main__":
    from PySide6.QtGui import QGuiApplication
    app = QGuiApplication([])
    for f in folder(Path(sys.argv[1] if len(sys.argv) > 1 else "pictures")):
        print(f)
