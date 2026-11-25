"""PR7-M's side of Check for updates: a check in the background a few seconds after starting (at most once a day,
unless turned off), "Check for updates" by hand, and the dialog that offers the new version: what's new, Install
now (download with progress, then the installer runs and PR7-M closes), Later, Skip this version. Settings:
updates/auto, updates/last, updates/skip in PR7-M's own settings."""

from __future__ import annotations

import threading
import time

from PySide6.QtCore import QObject, QSettings, Qt, QTimer, Signal
from PySide6.QtWidgets import (QApplication, QCheckBox, QDialog, QHBoxLayout, QLabel, QMessageBox, QProgressBar,
                               QPushButton, QTextBrowser, QVBoxLayout)

from . import __version__, theme, updates

ORG, NAME = "Paragon", "PR7-M"
_keep: list = []   # dialogs and bridges kept alive while shown
notify = None      # tests: a function(kind, text) instead of a message box
answer = None      # tests: a function(text) -> bool instead of a yes / no question


def _say(window, kind: str, text: str) -> None:
    if notify is not None:
        notify(kind, text)
    elif kind == "warning":
        QMessageBox.warning(window, "Check for Updates", text)
    else:
        QMessageBox.information(window, "Check for Updates", text)



def _confirm(window, text: str) -> bool:
    if answer is not None:
        return bool(answer(text))
    return QMessageBox.question(window, "Check for Updates", text) == QMessageBox.StandardButton.Yes


def _family_app() -> bool:
    """PR7-U (the family installer, which updates every Paragon app at once) is installed, and this isn't it."""
    if NAME == "PR7-U":
        return False
    try:
        from . import family
        return family.find("PR7-U") is not None
    except Exception:  # noqa: BLE001 - an offer, never an error
        return False


def open_family() -> bool:
    from . import family
    return family.launch("PR7-U", [])

def _settings() -> QSettings:
    return QSettings(QSettings.defaultFormat(), QSettings.Scope.UserScope, ORG, NAME)


def _get_bool(key, default):
    v = _settings().value(key)
    return default if v is None else (v is True or str(v).lower() in ("true", "1"))


def auto_enabled() -> bool:
    return _get_bool("updates/auto", True)


def set_auto(on: bool) -> None:
    s = _settings()
    s.setValue("updates/auto", bool(on))
    s.sync()


class _Bridge(QObject):
    """Results of the worker thread back on the GUI thread."""
    done = Signal(object)


def _in_background(fn, then, sync=False):
    if sync:
        try:
            r = (fn(), None)
        except Exception as e:  # noqa: BLE001 - shown to the user
            r = (None, e)
        then(*r)
        return
    b = _Bridge()
    _keep.append(b)

    def finish(box):
        _keep.remove(b)
        then(*box)
    b.done.connect(finish, Qt.ConnectionType.QueuedConnection)

    def go():
        try:
            box = (fn(), None)
        except Exception as e:  # noqa: BLE001
            box = (None, e)
        b.done.emit(box)
    threading.Thread(target=go, daemon=True).start()


def start(window, delay_ms: int = 4000) -> None:
    """When PR7-M starts: check a few seconds later if it's due (once a day) and not turned off."""
    if auto_enabled() and updates.due(float(_settings().value("updates/last") or 0)):
        QTimer.singleShot(delay_ms, lambda: check(window, manual=False))


def check(window, manual: bool = True, fetch=None, sync: bool = False):
    """Ask GitHub; offer a newer version. By hand, also say when there's none (or why it couldn't check)."""
    def then(rel, error):
        s = _settings()
        if error is None:
            s.setValue("updates/last", time.time())
            s.sync()
        if error is not None:
            if manual:
                _say(window, "warning", str(error))
            return
        if rel is None or (not manual and str(s.value("updates/skip") or "") == rel.version):
            if manual:
                text = f"You’re using the newest version of {NAME} ({__version__})."
                if _family_app():
                    if _confirm(window, text + "\n\nOpen PR7-U to check the rest of the Paragon family?"):
                        open_family()
                else:
                    _say(window, "info", text)
            return
        dlg = UpdateDialog(rel, window)
        _keep.append(dlg)
        dlg.finished.connect(lambda _r: _keep.remove(dlg) if dlg in _keep else None)
        dlg.show()
        window.update_dialog = dlg  # tests look at it
    _in_background(lambda: updates.check(__version__, fetch), then, sync)


class UpdateDialog(QDialog):
    """The new version: what's new, Install now / Later / Skip this version, and the automatic-check switch."""

    def __init__(self, rel, parent=None):
        super().__init__(parent)
        self.rel = rel
        self.setWindowTitle(f"{NAME} {rel.version}")
        self.resize(620, 480)
        lay = QVBoxLayout(self)
        lay.setContentsMargins(22, 18, 22, 16)
        head = QLabel(f"{NAME} {rel.version} is available")
        head.setStyleSheet("font-size: 17px; font-weight: 600;")
        lay.addWidget(head)
        sub = QLabel(f"You have {__version__}. Install it now, and {NAME} reopens when it’s done.")
        sub.setStyleSheet(f"color: {theme.TEXT_DIM};")
        sub.setWordWrap(True)
        lay.addWidget(sub)
        new = QLabel("WHAT’S NEW")
        new.setStyleSheet(f"color: {theme.TEXT_DIM}; font-size: 10.5px; font-weight: 600; margin-top: 8px;")
        lay.addWidget(new)
        self.notes = QTextBrowser()
        self.notes.setOpenExternalLinks(True)
        self.notes.setMarkdown(rel.notes or "No notes for this version.")
        lay.addWidget(self.notes, 1)
        self.bar = QProgressBar()
        self.bar.setRange(0, 1000)
        self.bar.setTextVisible(False)
        self.bar.setFixedHeight(8)
        self.bar.hide()
        lay.addWidget(self.bar)
        self.status = QLabel("")
        self.status.setStyleSheet(f"color: {theme.TEXT_DIM};")
        lay.addWidget(self.status)
        self.auto = QCheckBox("Check for updates automatically")
        self.auto.setChecked(auto_enabled())
        self.auto.toggled.connect(set_auto)
        lay.addWidget(self.auto)
        row = QHBoxLayout()
        row.addStretch(1)
        self.skip = QPushButton("Skip This Version")
        self.later = QPushButton("Later")
        self.go = QPushButton("Install Now")
        self.go.setDefault(True)
        self.family = None
        if _family_app():                     # PR7-U updates every installed family app in one go
            self.family = QPushButton("Update All in PR7-U")
            self.family.clicked.connect(lambda: (open_family(), self.reject()))
            row.insertWidget(0, self.family)
        for b in (self.skip, self.later, self.go):
            row.addWidget(b)
        lay.addLayout(row)
        self.skip.clicked.connect(self._skip)
        self.later.clicked.connect(self.reject)
        self.go.clicked.connect(self.install)
        self._cancel = False
        self.installer_path = ""

    def _skip(self):
        s = _settings()
        s.setValue("updates/skip", self.rel.version)
        s.sync()
        self.reject()

    def install(self, sync: bool = False, run=None):
        """Download (with progress), then run the installer and close PR7-M."""
        self.go.setEnabled(False)
        self.skip.setEnabled(False)
        self.bar.show()
        self.status.setText("Downloading…")
        state = {"got": 0, "total": self.rel.size}

        def progress(got, total):
            state["got"], state["total"] = got, total or state["total"]

        timer = QTimer(self)
        timer.timeout.connect(lambda: self.bar.setValue(int(1000 * state["got"] / state["total"]))
                              if state["total"] else None)
        timer.start(100)

        def then(path, error):
            timer.stop()
            if error is not None:
                self.status.setText(str(error))
                self.go.setEnabled(True)
                self.bar.hide()
                return
            self.installer_path = path
            self.bar.setValue(1000)
            self.status.setText("Installing…")
            try:
                (run or updates.install)(path)
            except updates.UpdateError as e:
                self.status.setText(str(e))
                self.go.setEnabled(True)
                return
            self.accept()
            app = QApplication.instance()
            QTimer.singleShot(300, app.quit)  # the installer waits for PR7-M to close
        _in_background(lambda: updates.download(self.rel, progress=progress, cancelled=lambda: self._cancel),
                       then, sync)

    def reject(self):
        self._cancel = True
        super().reject()
