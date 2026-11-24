"""The Paragon family look for Qt, in its two finishes: OBSIDIAN (dark, default) and AURORA (light).
(Paragon PR7-Z's theme.py plus AURORA, as in PR7-W.)

Every value here comes from Paragon Photo's docs/paragon-finish-spec.md, which was
extracted from the PR7 plugins' ParagonLookAndFeel / Finish sources. Colour rule of
the family: only hues ~318 deg -> 0 -> 42 deg (pink, red, orange) plus pearl white
and onyx greys. No green, blue, purple, yellow or cyan anywhere, Qt defaults included.
"""

from __future__ import annotations

from pathlib import Path

from PySide6.QtGui import QColor, QFontDatabase, QLinearGradient, QPalette

FONTS_DIR = Path(__file__).parent / "fonts"

# 1a. Palette
BACKGROUND = "#060407"
PANEL = "#0C0A0D"
PANEL_EDGE = "#27222A"
ACCENT = "#F4353A"
ACCENT_DIM = "#4E1016"
METER_RED = "#FF3B2F"
TEXT = "#E8E8EA"
TEXT_DIM = "#8A8A92"
GLOW = "#FF4A3A"
WIDGET = "#151217"
MENU = "#050405"
BUTTON = "#1C1C20"
BUTTON_HOVER = "#303034"
LED_ORANGE = "#FF7A1A"
DOCK_TOP = "#0E0B0F"
DOCK_BOTTOM = "#09070A"
TAB_FRAME = "#0D0A0E"
SCROLL_TRACK = "#0B090C"

ON_DARK = "#E8E8EA"     # text and icons on things that stay dark in both finishes (keys, menus, buttons)

# ---------------------------------------------------------------- the finishes (spec section 0, 1d, 1f, 5)
# OBSIDIAN is the values above. AURORA: warm pearl surfaces dimmed 10% (pearl x 0.9), ink lettering;
# the screens, neon, menus, buttons and black keys stay as they are on both.
_PEARL = 0.9
FINISHES = {
    "obsidian": dict(BACKGROUND="#060407", PANEL_EDGE="#27222A", TEXT="#E8E8EA", TEXT_DIM="#8A8A92",
                     WIDGET="#151217", DOCK_TOP="#0E0B0F", DOCK_BOTTOM="#09070A", TAB_FRAME="#0D0A0E",
                     SCROLL_TRACK="#0B090C", ALT_ROW="#09070A"),
    "aurora": dict(BACKGROUND="#D9D0D4", PANEL_EDGE="#A8999F", TEXT="#26252A", TEXT_DIM="#6B6569",
                   WIDGET="#E6E4E5", DOCK_TOP="#E0DBDD", DOCK_BOTTOM="#D6CCD2", TAB_FRAME="#D3C9CE",
                   SCROLL_TRACK="#D0C6CB", ALT_ROW="#D3CACE"),
}
ALT_ROW = "#09070A"

FINISH = "obsidian"


def set_finish(name: str) -> None:
    """Switch the module's colours to a finish. Style sheets and painting read them afresh."""
    global FINISH
    FINISH = name if name in FINISHES else "obsidian"
    globals().update(FINISHES[FINISH])




# 1b. Family gradient stops (DE0639 -> CD6619), and the two strengths it's drawn at.
FAMILY_STOPS = [(0.0, "#DE0639"), (0.25, "#D90738"), (0.5, "#D42E2A"), (0.75, "#CF5C1C"), (1.0, "#CD6619")]
LIT = 1.18
DIM = 0.42
NEON = 0.75


def _scaled(hex_colour: str, k: float, alpha: float = 1.0) -> QColor:
    c = QColor(hex_colour)
    return QColor(min(255, round(c.red() * k)), min(255, round(c.green() * k)), min(255, round(c.blue() * k)),
                  round(alpha * 255))


def family_gradient(x1: float, y1: float, x2: float, y2: float, brightness: float = LIT,
                    alpha: float = 1.0) -> QLinearGradient:
    g = QLinearGradient(x1, y1, x2, y2)
    for pos, colour in FAMILY_STOPS:
        g.setColorAt(pos, _scaled(colour, brightness, alpha))
    return g


def qss_gradient(brightness: float = LIT, alpha: float = 1.0, vertical: bool = False) -> str:
    """The family gradient as a Qt style-sheet brush."""
    stops = ", ".join(
        f"stop:{pos} rgba({c.red()}, {c.green()}, {c.blue()}, {c.alpha()})"
        for pos, c in ((p, _scaled(h, brightness, alpha)) for p, h in FAMILY_STOPS))
    end = "x2:0, y2:1" if vertical else "x2:1, y2:0"
    return f"qlineargradient(x1:0, y1:0, {end}, {stops})"


def load_fonts() -> None:
    for ttf in FONTS_DIR.glob("*.ttf"):
        QFontDatabase.addApplicationFont(str(ttf))


def palette() -> QPalette:
    """Qt's colour roles set to the family scheme, so nothing falls back to Qt's blue."""
    p = QPalette()
    roles = {
        QPalette.ColorRole.Window: BACKGROUND, QPalette.ColorRole.WindowText: TEXT, QPalette.ColorRole.Base: WIDGET,
        QPalette.ColorRole.AlternateBase: "#100D11", QPalette.ColorRole.Text: TEXT, QPalette.ColorRole.Button: BUTTON,
        QPalette.ColorRole.ButtonText: TEXT, QPalette.ColorRole.BrightText: "#FFFFFF", QPalette.ColorRole.Highlight: ACCENT_DIM,
        QPalette.ColorRole.HighlightedText: "#FFFFFF", QPalette.ColorRole.ToolTipBase: PANEL, QPalette.ColorRole.ToolTipText: TEXT,
        QPalette.ColorRole.PlaceholderText: TEXT_DIM, QPalette.ColorRole.Link: LED_ORANGE, QPalette.ColorRole.LinkVisited: GLOW,
        QPalette.ColorRole.Mid: PANEL_EDGE, QPalette.ColorRole.Dark: MENU, QPalette.ColorRole.Shadow: "#000000",
    }
    for role, colour in roles.items():
        p.setColor(role, QColor(colour))
    for role in (QPalette.ColorRole.WindowText, QPalette.ColorRole.Text, QPalette.ColorRole.ButtonText):
        p.setColor(QPalette.ColorGroup.Disabled, role, QColor(TEXT_DIM))
    return p


def _chevron() -> str:
    """A small down chevron in textDim, written once to a temp file (Qt style sheets need a file URL)."""
    import tempfile
    path = Path(tempfile.gettempdir()) / "pr7m_chevron.svg"
    if not path.exists():
        path.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="10" height="6" viewBox="0 0 10 6">'
                        f'<path d="M1 1 L5 5 L9 1" fill="none" stroke="{TEXT_DIM}" stroke-width="1.6" '
                        'stroke-linecap="round" stroke-linejoin="round"/></svg>')
    return path.as_posix()


def stylesheet() -> str:
    lit = qss_gradient(LIT)
    row_soft = qss_gradient(LIT, 0.08)
    return f"""
* {{ font-family: "Inter"; font-size: 13px; color: {TEXT}; outline: 0; }}
QMainWindow, QDialog {{ background: {BACKGROUND}; }}
QToolTip {{ background: {PANEL}; color: {ON_DARK}; border: 1px solid {PANEL_EDGE}; border-radius: 4px; padding: 5px 9px; }}

/* Menu bar and popup menus: black, white text, the family gradient under the mouse. */
QMenuBar {{ background: {BACKGROUND}; border-bottom: 1px solid {PANEL_EDGE}; padding: 2px 6px; }}
QMenuBar::item {{ background: transparent; padding: 5px 10px; border-radius: 4px; color: {TEXT}; }}
QMenuBar::item:selected {{ background: rgba(255,255,255,0.06); }}
QMenu {{ background: {MENU}; border: 1px solid {PANEL_EDGE}; padding: 4px; }}
QMenu::item {{ padding: 6px 28px 6px 8px; border-radius: 3px; color: #FFFFFF; }}
QMenu::item:selected {{ background: {lit}; }}
QMenu::item:disabled {{ color: rgba(255,255,255,0.35); }}
QMenu::separator {{ height: 1px; background: {PANEL_EDGE}; margin: 4px 6px; }}

/* Buttons: 1C1C20, 1 px panelEdge, radius 4; pressed = dim family gradient. */
QPushButton {{ background: {BUTTON}; color: {ON_DARK}; border: 1px solid {PANEL_EDGE}; border-radius: 4px; padding: 7px 16px;
              font-weight: 600; font-size: 12px; }}
QPushButton:hover {{ background: {BUTTON_HOVER}; }}
QPushButton:pressed {{ background: {qss_gradient(DIM)}; }}
QPushButton:disabled {{ color: rgba(232,232,234,0.4); }}
QComboBox QAbstractItemView, QComboBox QAbstractItemView::item {{ color: {ON_DARK}; }}
QPushButton:default {{ border: 1px solid {ACCENT}; }}

/* Text fields and drop-downs: widget background, accent caret/text per spec. */
QLineEdit, QComboBox, QSpinBox {{ background: {WIDGET}; border: 1px solid {PANEL_EDGE}; border-radius: 4px;
              padding: 5px 8px; selection-background-color: {ACCENT_DIM}; selection-color: #FFFFFF; min-height: 18px; }}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus {{ border: 1px solid {ACCENT}; }}
QComboBox::drop-down {{ border: none; width: 22px; }}
QComboBox::down-arrow {{ image: url("{_chevron()}"); width: 10px; height: 6px; margin-right: 8px; }}
QComboBox QAbstractItemView {{ background: {MENU}; border: 1px solid {PANEL_EDGE}; padding: 4px;
              selection-background-color: {ACCENT_DIM}; }}

QCheckBox, QRadioButton {{ spacing: 8px; font-size: 12px; }}
QCheckBox::indicator, QRadioButton::indicator {{ width: 12px; height: 12px; border-radius: 6px;
              border: 1px solid #000000; background: {qss_gradient(DIM * 0.5)}; }}
QCheckBox::indicator:checked, QRadioButton::indicator:checked {{ background: {lit}; }}
QCheckBox::indicator:hover, QRadioButton::indicator:hover {{ border: 1px solid {TEXT}; }}
QCheckBox:disabled, QRadioButton:disabled {{ color: {TEXT_DIM}; }}
QCheckBox::indicator:disabled, QRadioButton::indicator:disabled {{ background: {WIDGET}; border: 1px solid {PANEL_EDGE}; }}

/* The file list: selected row = family gradient @.22 with a gradient border. */
QTableView, QListView {{ background: {BACKGROUND}; alternate-background-color: {ALT_ROW}; border: 1px solid {PANEL_EDGE};
              border-radius: 6px; gridline-color: {PANEL_EDGE}; }}
QTableView::item, QListView::item {{ padding: 4px 6px; }}
QTableView::item:selected, QListView::item:selected {{ background: {qss_gradient(LIT, 0.22)}; color: #FFFFFF; }}
QTableCornerButton::section {{ background: {TAB_FRAME}; border: none; }}
QPlainTextEdit, QTextEdit {{ background: {WIDGET}; border: 1px solid {PANEL_EDGE}; border-radius: 6px; padding: 6px;
              font-family: "Consolas", "Cascadia Mono", "DejaVu Sans Mono", monospace; font-size: 12px;
              selection-background-color: {ACCENT_DIM}; }}
QTreeView {{ background: {BACKGROUND}; alternate-background-color: {ALT_ROW}; border: 1px solid {PANEL_EDGE};
              border-radius: 6px; padding: 2px; }}
QTreeView::item {{ height: 24px; padding: 0 4px; border: 1px solid transparent; }}
QTreeView::item:hover {{ background: {row_soft}; }}
QTreeView::item:selected {{ background: transparent; color: #FFFFFF; }}  /* the row band is painted by FileView */
QHeaderView {{ background: {BACKGROUND}; }}
QHeaderView::section {{ background: {TAB_FRAME}; color: {TEXT_DIM}; border: none; border-right: 1px solid {PANEL_EDGE};
              border-bottom: 1px solid {PANEL_EDGE}; padding: 6px 8px; font-size: 10.5px; font-weight: 600; }}

/* Scroll bars: thumb only, accent gradient. */
QScrollBar:vertical {{ background: {SCROLL_TRACK}; width: 10px; margin: 0; border-left: 1px solid {PANEL_EDGE}; }}
QScrollBar:horizontal {{ background: {SCROLL_TRACK}; height: 10px; margin: 0; border-top: 1px solid {PANEL_EDGE}; }}
QScrollBar::handle:vertical {{ background: {qss_gradient(LIT, vertical=True)}; border-radius: 4px; min-height: 30px; margin: 1px; }}
QScrollBar::handle:horizontal {{ background: {lit}; border-radius: 4px; min-width: 30px; margin: 1px; }}
QScrollBar::add-line, QScrollBar::sub-line {{ width: 0; height: 0; }}
QScrollBar::add-page, QScrollBar::sub-page {{ background: none; }}

/* Dock panels (dialog sections). */
QGroupBox {{ background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 {DOCK_TOP}, stop:1 {DOCK_BOTTOM});
              border: 1px solid {PANEL_EDGE}; border-radius: 8px; margin-top: 22px; padding: 14px 12px 10px 12px;
              font-size: 12.5px; font-weight: 600; }}
QGroupBox::title {{ subcontrol-origin: margin; left: 4px; top: 2px; color: rgba(232,232,234,0.9); }}
QLabel[role="dim"] {{ color: {TEXT_DIM}; }}
QLabel[role="value"] {{ color: #FFFFFF; font-size: 13px; }}
QLabel[role="heading"] {{ font-size: 16px; font-weight: 600; }}
QLabel[role="error"] {{ color: {METER_RED}; }}
QLabel[role="ok"] {{ color: {LED_ORANGE}; font-weight: 600; }}
QLabel[role="field"] {{ color: {TEXT_DIM}; font-size: 10.5px; font-weight: 600; }}

QStatusBar {{ background: {TAB_FRAME}; border-top: 1px solid {PANEL_EDGE}; color: {TEXT_DIM}; font-size: 12px; }}
QStatusBar QLabel {{ color: {TEXT_DIM}; font-size: 12px; padding: 0 10px; border-right: 1px solid {PANEL_EDGE}; }}
QProgressBar {{ background: {WIDGET}; border: 1px solid {PANEL_EDGE}; border-radius: 4px; height: 8px; text-align: center; }}
QProgressBar::chunk {{ background: {lit}; border-radius: 3px; }}
"""
