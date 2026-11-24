"""Draw the installer's wizard images in the Paragon look.

    python tools/make_installer_art.py      writes installer/wizard-*.bmp (Inno Setup, 100% and 200%)

Large (welcome / finish pages): onyx with the red glow, the 7M icon, PARAGON and the PR7-M mark,
and the family gradient along the bottom. Small (page header): the icon on white.
"""
import os
import sys
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from PySide6.QtCore import QPointF, QRectF, Qt  # noqa: E402
from PySide6.QtGui import QColor, QFont, QGuiApplication, QImage, QLinearGradient, QPainter, QRadialGradient  # noqa: E402

from pr7m import theme  # noqa: E402
from pr7m.widgets import draw_mark  # noqa: E402

OUT = ROOT / "installer"


def large(scale: int) -> QImage:
    w, h = 164 * scale, 314 * scale
    img = QImage(w, h, QImage.Format.Format_RGB32)
    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.Antialiasing)
    p.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)
    bg = QLinearGradient(0, 0, 0, h)
    bg.setColorAt(0, QColor(theme.DOCK_TOP))
    bg.setColorAt(1, QColor(theme.BACKGROUND))
    p.fillRect(img.rect(), bg)
    glow = QRadialGradient(QPointF(w / 2, h * 0.28), w * 0.9)
    c = QColor(theme.GLOW)
    c.setAlphaF(0.16)
    glow.setColorAt(0, c)
    glow.setColorAt(1, QColor(0, 0, 0, 0))
    p.fillRect(img.rect(), glow)
    icon = QImage(str(ROOT / "pr7m" / "icons" / "app.png"))
    s = 96 * scale
    p.drawImage(QRectF((w - s) / 2, 44 * scale, s, s), icon)
    f = QFont("Inter")
    f.setPixelSize(15 * scale)
    p.setFont(f)
    p.setPen(QColor(theme.TEXT))
    p.drawText(QRectF(0, 160 * scale, w, 22 * scale), Qt.AlignmentFlag.AlignCenter, "PARAGON")
    draw_mark(p, QRectF(20 * scale, 188 * scale, w - 40 * scale, 30 * scale))
    f2 = QFont("Inter")
    f2.setPixelSize(10 * scale)
    f2.setWeight(QFont.Weight.DemiBold)
    p.setFont(f2)
    p.setPen(QColor(theme.TEXT_DIM))
    p.drawText(QRectF(0, 226 * scale, w, 16 * scale), Qt.AlignmentFlag.AlignCenter, "MEDIA PLAYER")
    p.fillRect(QRectF(0, h - 4 * scale, w, 4 * scale), theme.family_gradient(0, 0, w, 0))
    p.end()
    return img


def small(scale: int) -> QImage:
    n = 55 * scale
    img = QImage(n, n, QImage.Format.Format_RGB32)
    img.fill(QColor("#FFFFFF"))
    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)
    p.drawImage(QRectF(0, 0, n, n), QImage(str(ROOT / "pr7m" / "icons" / "app.png")))
    p.end()
    return img


def main():
    app = QGuiApplication([])
    theme.load_fonts()
    from PIL import Image
    OUT.mkdir(exist_ok=True)
    for scale in (1, 2):
        for name, img in ((f"wizard-large-{scale}x", large(scale)), (f"wizard-small-{scale}x", small(scale))):
            png = OUT / f"_{name}.png"
            img.save(str(png))
            Image.open(png).convert("RGB").save(OUT / f"{name}.bmp")  # 24-bit BMP: what Inno Setup reads
            png.unlink()
    print("wrote", sorted(p.name for p in OUT.glob("*.bmp")))
    del app


if __name__ == "__main__":
    main()
