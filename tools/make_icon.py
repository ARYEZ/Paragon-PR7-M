"""Draw PR7-M's icon: the "7M" mark on a piano-black key with a neon edge.

    python tools/make_icon.py        writes pr7m/icons/app.ico (16-256 px) and app.png
"""
import os
import sys
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from PySide6.QtCore import QRectF, Qt  # noqa: E402
from PySide6.QtGui import QColor, QGuiApplication, QImage, QLinearGradient, QPainter, QPainterPath, QPen, QRegion  # noqa: E402

from pr7m import theme  # noqa: E402
from pr7m.widgets import CUTS, SEVEN_SHAPE, SLANT, M_SHAPE, _lean, _poly  # noqa: E402

OUT = Path(__file__).resolve().parents[1] / "pr7m" / "icons"
WIDTH = 0.8 + 0.1 + 1.0  # 7, gap, M


def draw(n: int) -> QImage:
    img = QImage(n, n, QImage.Format.Format_ARGB32_Premultiplied)
    img.fill(Qt.GlobalColor.transparent)
    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.Antialiasing)
    r = QRectF(0, 0, n, n).adjusted(n * .04, n * .04, -n * .04, -n * .04)
    rad = n * .2
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(theme.family_gradient(r.left(), 0, r.right(), 0, theme.NEON * 1.25))
    p.drawRoundedRect(r, rad, rad)
    face = r.adjusted(n * .05, n * .05, -n * .05, -n * .05)
    g = QLinearGradient(face.topLeft(), face.bottomLeft())
    g.setColorAt(0, QColor("#1C1B20"))
    g.setColorAt(1, QColor("#060607"))
    p.setBrush(g)
    p.drawRoundedRect(face, rad * .75, rad * .75)
    if n >= 32:
        p.setBrush(QColor(255, 255, 255, 22))
        p.save()
        p.setClipRect(QRectF(face.left(), face.top(), face.width(), face.height() * .5))
        p.drawRoundedRect(face, rad * .75, rad * .75)
        p.restore()
    h = face.height() * .42
    w = (WIDTH + SLANT) * h
    x0, y0 = face.center().x() - w / 2, face.center().y() - h / 2
    seven, lpath = QPainterPath(), QPainterPath()
    _poly(seven, SEVEN_SHAPE, 0)
    lpath.setFillRule(Qt.FillRule.OddEvenFill)
    for part in (M_SHAPE,):
        _poly(lpath, part, 0.9)
    from PySide6.QtGui import QTransform
    place = QTransform().translate(x0, y0).scale(h, h)
    seven, lpath = place.map(_lean().map(seven)), place.map(_lean().map(lpath))
    if n >= 48:
        for ring in (3, 2, 1):
            c = QColor(theme.GLOW)
            c.setAlphaF(.1 * (4 - ring))
            p.setPen(QPen(c, h * .07 * ring, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap,
                          Qt.PenJoinStyle.RoundJoin))
            p.setBrush(Qt.BrushStyle.NoBrush)
            p.drawPath(seven)
    if n >= 32:
        clip = QRegion(face.toAlignedRect())
        for top, bottom in CUTS:
            clip -= QRegion(QRectF(face.left(), y0 + h * top, face.width(), h * (bottom - top)).toAlignedRect())
        p.setClipRegion(clip)
    p.setPen(Qt.PenStyle.NoPen)
    p.fillPath(seven, theme.family_gradient(x0, 0, x0 + w * .5, 0))
    p.fillPath(lpath, QColor(theme.TEXT))
    p.end()
    return img


def main():
    app = QGuiApplication([])
    OUT.mkdir(parents=True, exist_ok=True)
    sizes = [16, 24, 32, 48, 64, 128, 256]
    files = []
    for s in sizes:
        f = OUT / f"_icon{s}.png"
        draw(s).save(str(f))
        files.append(f)
    draw(256).save(str(OUT / "app.png"))
    from PIL import Image
    Image.open(files[-1]).save(OUT / "app.ico", sizes=[(s, s) for s in sizes],
                               append_images=[Image.open(f) for f in files[:-1]])
    for f in files:
        f.unlink()
    print("wrote", OUT / "app.ico")
    del app


if __name__ == "__main__":
    main()
