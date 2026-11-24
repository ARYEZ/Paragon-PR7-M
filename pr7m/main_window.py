"""PR7-M's window: MPC-HC's menus, keys and status line; the picture, seek bar, controls and playlist; full screen
with the controls over the picture. Playback is Qt Multimedia (FFmpeg).

Anything that would ask the user goes through `_ask(kind, ...)`; tests answer with `window.answers[kind]`."""

from __future__ import annotations

import os

from PySide6.QtCore import QEvent, QPoint, QRect, Qt, QTimer, QUrl
from PySide6.QtGui import QAction, QActionGroup, QColor, QImage, QKeySequence, QLinearGradient, QPainter
from PySide6.QtMultimedia import QAudioBufferOutput, QAudioOutput, QMediaMetaData, QMediaPlayer
from PySide6.QtWidgets import (QApplication, QCheckBox, QComboBox, QDialog, QDialogButtonBox, QFileDialog,
                               QFormLayout, QFrame, QGridLayout, QHBoxLayout, QInputDialog, QLabel, QLineEdit,
                               QMainWindow, QMenu, QMessageBox, QPushButton, QSplitter, QVBoxLayout, QWidget)

from . import __version__, media, settings, theme, widgets
from .media import ABRepeat, History, hms
from .player import PlaylistModel, PlaylistView, SeekBar, VideoView, VolumeSlider

OPEN_FILTER = ("Media files (" + " ".join(f"*.{e}" for e in sorted(media.MEDIA_EXTS | media.PLAYLIST_EXTS)) +
               ");;Video (" + " ".join(f"*.{e}" for e in sorted(media.VIDEO_EXTS)) + ");;Audio (" +
               " ".join(f"*.{e}" for e in sorted(media.AUDIO_EXTS)) + ");;All files (*)")
AFTER = [("play_next", "Play the &next file"), ("repeat_file", "Repeat this &file"),
         ("repeat_list", "Repeat the &playlist"), ("stop", "&Stop"), ("close", "&Close PR7-M")]
JUMP, JUMP_LONG = 10_000, 60_000

EXTRA = """
QLabel#time {{ color: {text}; font-size: 13px; font-weight: 600; }}
QWidget#status {{ background: {bg}; border-top: 1px solid {edge}; }}
QLabel#stat {{ color: {dim}; font-size: 12px; }}
QLabel#statstrong {{ color: {text}; font-size: 12px; font-weight: 600; }}
QLabel#side {{ color: {dim}; font-weight: 600; font-size: 11px; padding: 10px 16px 6px 16px; }}
QLabel#key {{ color: {dim}; font-weight: 600; font-size: 11px; }}
"""


def stylesheet() -> str:
    return theme.stylesheet() + EXTRA.format(text=theme.TEXT, dim=theme.TEXT_DIM, bg=theme.BACKGROUND,
                                             edge=theme.PANEL_EDGE)


def apply_look(app: QApplication) -> None:
    app.setPalette(theme.palette())
    app.setStyleSheet(stylesheet())


def meta(md, key):
    """A metadata value; ones PySide can't convert (the codec enums) as Qt's own text for them."""
    try:
        return md.value(key)
    except RuntimeError:
        text = md.stringValue(key)
        return text or None


class Controls(QWidget):
    """The seek bar and the row of keys; in full screen it floats over the picture on a dark gradient."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.floating = False

    def paintEvent(self, _):
        if self.floating:
            p = QPainter(self)
            g = QLinearGradient(0, 0, 0, self.height())
            g.setColorAt(0, QColor(0, 0, 0, 60)); g.setColorAt(1, QColor(0, 0, 0, 230))
            p.fillRect(self.rect(), g)


class MainWindow(QMainWindow):
    def __init__(self, path: str | None = None, history: History | None = None, audio: bool = True):
        super().__init__()
        self.answers: dict = {}
        self.hist = history or History()
        self.ab = ABRepeat()
        self.cues: list[media.Cue] = []
        self.sub_file: str | None = None
        self.fullscreen = False
        self._pending_seek: int | None = None
        self.current: str | None = None
        self.setAcceptDrops(True)
        self.resize(1280, 800)

        self.player = QMediaPlayer(self)
        self.audio = QAudioOutput(self)
        self.player.setAudioOutput(self.audio)
        self.levels = QAudioBufferOutput(self)
        self.player.setAudioBufferOutput(self.levels)
        self._build()
        self.player.setVideoSink(self.video.sink)
        self.levels.audioBufferReceived.connect(self.video.set_levels)
        self.player.positionChanged.connect(self._position)
        self.player.durationChanged.connect(self._duration)
        self.player.mediaStatusChanged.connect(self._status)
        self.player.playbackStateChanged.connect(lambda _s: self._sync())
        self.player.metaDataChanged.connect(self._metadata)
        self.player.tracksChanged.connect(self._sync)
        self.player.errorOccurred.connect(self._error)
        self.player.bufferProgressChanged.connect(lambda v: setattr(self.seekbar, "buffered", v))
        self.video.sink.subtitleTextChanged.connect(self._embedded_subtitle)
        self.audio.setVolume(settings.get_int("volume", 100) / 100)
        self.audio.setMuted(settings.get_bool("muted", False))
        self.volume.set_value(settings.get_int("volume", 100), emit=False)
        self.volume.muted = self.audio.isMuted()
        self._build_menus()
        self.apply_pitch()
        self.hide_timer = QTimer(self)
        self.hide_timer.setSingleShot(True)
        self.hide_timer.setInterval(2500)
        self.hide_timer.timeout.connect(self._hide_floating)
        self._sync()
        if path:
            self.open_paths([path])

    # ================================================================== building
    def _key(self, icon, tip, slot, checkable=False):
        k = widgets.SmallKey(icon, tip, checkable)
        k.clicked.connect(slot)
        return k

    def _build(self):
        central = QWidget()
        col = QVBoxLayout(central)
        col.setContentsMargins(0, 0, 0, 0)
        col.setSpacing(0)
        self.header = widgets.Header("MEDIA PLAYER")
        col.addWidget(self.header)
        self.split = QSplitter()
        self.split.setHandleWidth(1)
        self.video = VideoView()
        self.video.clicked.connect(self.play_pause)
        self.video.doubleClicked.connect(self.toggle_fullscreen)
        self.video.wheeled.connect(lambda n: self.set_volume(self.volume.value + 5 * n))
        self.video.mouseMoved.connect(self._mouse_moved)
        self.split.addWidget(self.video)
        self.side = QWidget()
        sl = QVBoxLayout(self.side)
        sl.setContentsMargins(0, 0, 0, 0)
        sl.setSpacing(0)
        self.side_label = QLabel("PLAYLIST"); self.side_label.setObjectName("side")
        sl.addWidget(self.side_label)
        self.model = PlaylistModel()
        self.list = PlaylistView(self.model)
        self.list.activated_row.connect(self.play_index)
        self.list.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.list.customContextMenuRequested.connect(self._playlist_menu)
        sl.addWidget(self.list)
        self.side.setMinimumWidth(220)
        self.split.addWidget(self.side)
        self.split.setStretchFactor(0, 1)
        self.split.setSizes([950, 330])
        col.addWidget(self.split, 1)

        self.controls = Controls()
        cl = QVBoxLayout(self.controls)
        cl.setContentsMargins(0, 0, 0, 0)
        cl.setSpacing(0)
        self.seekbar = SeekBar()
        self.seekbar.seek.connect(self.seek)
        cl.addWidget(self.seekbar)
        row = QWidget()
        lay = QHBoxLayout(row)
        lay.setContentsMargins(12, 2, 12, 8)
        lay.setSpacing(4)

        def gap():
            line = QFrame(); line.setFixedSize(1, 22); line.setObjectName("gap")
            line.setStyleSheet(f"background: {theme.PANEL_EDGE};")
            lay.addSpacing(6); lay.addWidget(line); lay.addSpacing(6)

        self.k_play = self._key("play", "Play / Pause (Space)", self.play_pause); lay.addWidget(self.k_play)
        self.k_stop = self._key("stop", "Stop (.)", self.stop); lay.addWidget(self.k_stop)
        gap()
        self.k_prev = self._key("skipback", "Previous (PgUp)", lambda: self.step_file(-1)); lay.addWidget(self.k_prev)
        self.k_next = self._key("skipnext", "Next (PgDn)", lambda: self.step_file(1)); lay.addWidget(self.k_next)
        self.k_frame = self._key("framestep", "Frame step (Ctrl+Right)", lambda: self.frame_step(1)); lay.addWidget(self.k_frame)
        gap()
        self.k_mute = self._key("volume", "Mute (Ctrl+M)", self.toggle_mute); lay.addWidget(self.k_mute)
        self.volume = VolumeSlider()
        self.volume.changed.connect(self.set_volume)
        lay.addWidget(self.volume)
        lay.addSpacing(10)
        self.time = QLabel("00:00:00 / 00:00:00"); self.time.setObjectName("time")
        lay.addWidget(self.time)
        lay.addStretch(1)
        self.k_subs = self._key("subtitles", "Subtitles", self._subs_menu_at_key, True); lay.addWidget(self.k_subs)
        self.k_audio = self._key("audiotrack", "Audio track", self._audio_menu_at_key); lay.addWidget(self.k_audio)
        self.k_list = self._key("playlist", "Playlist (Ctrl+7)", lambda: self.show_playlist(not self.side.isVisible()),
                                True); lay.addWidget(self.k_list)
        self.k_full = self._key("fullscreen", "Full screen (Alt+Enter)", self.toggle_fullscreen, True)
        lay.addWidget(self.k_full)
        cl.addWidget(row)
        col.addWidget(self.controls)
        self.central_layout = col

        self.statusw = QWidget(); self.statusw.setObjectName("status")
        st = QHBoxLayout(self.statusw)
        st.setContentsMargins(14, 4, 14, 6)
        st.setSpacing(24)
        self.st_state = QLabel(""); self.st_state.setObjectName("statstrong"); st.addWidget(self.st_state)
        self.st_parts = []
        for _ in range(4):
            lab = QLabel(""); lab.setObjectName("stat"); st.addWidget(lab); self.st_parts.append(lab)
        st.addStretch(1)
        col.addWidget(self.statusw)
        self.setCentralWidget(central)

    def _act(self, menu, text, slot=None, shortcut=None, checkable=False, key=None):
        a = QAction(text, self)
        if shortcut:
            a.setShortcuts([QKeySequence(s) for s in (shortcut if isinstance(shortcut, (list, tuple)) else [shortcut])])
        a.setCheckable(checkable)
        if slot:
            a.triggered.connect(slot)
        if menu is not None:
            menu.addAction(a)
        else:
            self.addAction(a)
        if key:
            self.acts[key] = a
        return a

    def _build_menus(self):
        self.acts: dict[str, QAction] = {}
        mb = self.menuBar()
        f = mb.addMenu("&File")
        self._act(f, "&Quick open file...", self.open_dialog, "Ctrl+Q", key="quick_open")
        self._act(f, "&Open file...", self.open_dialog, "Ctrl+O", key="open")
        self._act(f, "Open &folder...", self.open_folder_dialog, key="open_folder")
        self.recent_menu = f.addMenu("Recent &files")
        self.recent_menu.aboutToShow.connect(self._fill_recent)
        f.addSeparator()
        self._act(f, "&Close", self.close_file, "Ctrl+C", key="close")
        f.addSeparator()
        self._act(f, "Save &image...", self.save_image, "Alt+I", key="save_image")
        self._act(f, "Save &playlist...", self.save_playlist, key="save_playlist")
        self._act(f, "&Properties", self.properties, "Shift+F10", key="properties")
        f.addSeparator()
        self._act(f, "E&xit", self.close, "Alt+X", key="exit")

        v = mb.addMenu("&View")
        self._act(v, "&Playlist", lambda on: self.show_playlist(on), "Ctrl+7", True, key="playlist")
        self._act(v, "&Controls", lambda on: self._show("controls", on), None, True, key="show_controls")
        self._act(v, "&Status bar", lambda on: self._show("statusbar", on), None, True, key="show_statusbar")
        v.addSeparator()
        self._act(v, "&Full screen", self.toggle_fullscreen, ["Alt+Return", "F11"], True, key="fullscreen")
        self._act(v, "&On top", lambda on: self._on_top(on), None, True, key="on_top")
        v.addSeparator()
        fin = v.addMenu("&Finish")
        fg = QActionGroup(self)
        for name in theme.FINISHES:
            a = self._act(fin, name.upper(), lambda _=False, n=name: self.set_finish(n), None, True, key=f"finish_{name}")
            a.setChecked(name == theme.FINISH)
            fg.addAction(a)
        self._act(v, "&Options...", self.options, "O", key="options")

        p = mb.addMenu("&Play")
        self._act(p, "&Play / Pause", self.play_pause, "Space", key="play_pause")
        self._act(p, "&Stop", self.stop, "Period", key="stop")
        self._act(p, "&Frame step", lambda: self.frame_step(1), "Ctrl+Right", key="frame_step")
        self._act(p, "Frame step &back", lambda: self.frame_step(-1), "Ctrl+Left", key="frame_back")
        p.addSeparator()
        self._act(p, "&Decrease rate", lambda: self.set_rate(media.step_rate(self.player.playbackRate(), False)),
                  "Ctrl+Down", key="rate_down")
        self._act(p, "&Increase rate", lambda: self.set_rate(media.step_rate(self.player.playbackRate(), True)),
                  "Ctrl+Up", key="rate_up")
        self._act(p, "&Reset rate", lambda: self.set_rate(1.0), "Ctrl+R", key="rate_reset")
        p.addSeparator()
        self.audio_menu = p.addMenu("&Audio")
        self.audio_menu.aboutToShow.connect(lambda: self._fill_tracks(self.audio_menu, "audio"))
        self.subs_menu = p.addMenu("Su&btitles")
        self.subs_menu.aboutToShow.connect(lambda: self._fill_tracks(self.subs_menu, "subtitles"))
        p.addSeparator()
        vol = p.addMenu("&Volume")
        self._act(vol, "&Up", lambda: self.set_volume(self.volume.value + 5), "Up", key="vol_up")
        self._act(vol, "&Down", lambda: self.set_volume(self.volume.value - 5), "Down", key="vol_down")
        self._act(vol, "&Mute", self.toggle_mute, "Ctrl+M", True, key="mute")
        self._act(p, "A-B &repeat", self.ab_press, "Ctrl+B", key="ab")
        after = p.addMenu("&After playback")
        ag = QActionGroup(self)
        chosen = settings.get("after_playback", "play_next")
        for key, text in AFTER:
            a = self._act(after, text, lambda _=False, k=key: settings.set("after_playback", k), None, True,
                          key=f"after_{key}")
            a.setChecked(key == chosen)
            ag.addAction(a)
        self._act(p, "&Load subtitle file...", self.load_subtitles_dialog, "Ctrl+L", key="load_subs")

        n = mb.addMenu("&Navigate")
        self._act(n, "&Previous", lambda: self.step_file(-1), "PgUp", key="prev")
        self._act(n, "&Next", lambda: self.step_file(1), "PgDown", key="next")
        n.addSeparator()
        self._act(n, "&Go to...", self.go_to_dialog, "Ctrl+G", key="goto")
        n.addSeparator()
        self._act(n, "Jump &forward 10 s", lambda: self.jump(JUMP), "Right", key="fwd")
        self._act(n, "Jump &backward 10 s", lambda: self.jump(-JUMP), "Left", key="back")
        self._act(n, "Jump forward 1 &min", lambda: self.jump(JUMP_LONG), "Ctrl+Shift+Right", key="fwd_long")
        self._act(n, "Jump backward 1 m&in", lambda: self.jump(-JUMP_LONG), "Ctrl+Shift+Left", key="back_long")

        self.fav_menu = mb.addMenu("F&avorites")
        self.fav_menu.aboutToShow.connect(self._fill_favorites)

        h = mb.addMenu("&Help")
        self._act(h, "&Keyboard shortcuts", self.shortcuts, "F1", key="shortcuts")
        self._act(h, "Check for &updates...", self.check_updates, key="updates")
        self._act(h, "&About Paragon PR7-M", self.about, key="about")
        self._act(None, "", self._escape, "Escape", key="escape")
        for key, act in (("controls", "show_controls"), ("statusbar", "show_statusbar")):
            on = settings.get_bool(f"show_{key}", True)
            self.acts[act].setChecked(on)
            self._show(key, on, save=False)
        self.show_playlist(settings.get_bool("show_playlist", True), save=False)
        self.acts["mute"].setChecked(self.audio.isMuted())

    # ================================================================== asking
    def _ask(self, kind: str, *args):
        if kind in self.answers:
            a = self.answers[kind]
            return a(*args) if callable(a) else a
        if kind == "open":
            files, _ = QFileDialog.getOpenFileNames(self, "Open", settings.get("last_dir", ""), OPEN_FILTER)
            return files
        if kind == "folder":
            return QFileDialog.getExistingDirectory(self, "Open Folder", settings.get("last_dir", "")) or None
        if kind == "subtitles":
            path, _ = QFileDialog.getOpenFileName(self, "Load Subtitle File", args[0], "SubRip (*.srt);;All files (*)")
            return path or None
        if kind == "save_image":
            path, _ = QFileDialog.getSaveFileName(self, "Save Image", args[0], "PNG (*.png);;JPEG (*.jpg)")
            return path or None
        if kind == "save_playlist":
            path, _ = QFileDialog.getSaveFileName(self, "Save Playlist", args[0], "Playlist (*.m3u8);;Playlist (*.m3u)")
            return path or None
        if kind == "goto":
            text, ok = QInputDialog.getText(self, "Go To", "Time (hh:mm:ss):", QLineEdit.EchoMode.Normal, args[0])
            return text if ok else None
        if kind == "favorite":
            text, ok = QInputDialog.getText(self, "Add to Favorites", "Name:", QLineEdit.EchoMode.Normal, args[0])
            return text if ok else None
        if kind == "error":
            QMessageBox.warning(self, "Paragon PR7-M", args[0])
            return None
        return None

    # ================================================================== opening
    def open_dialog(self):
        files = self._ask("open")
        if files:
            settings.set("last_dir", os.path.dirname(files[0]))
            self.open_paths(files)

    def open_folder_dialog(self):
        folder = self._ask("folder")
        if folder:
            settings.set("last_dir", folder)
            self.open_paths([folder])

    def open_paths(self, paths: list[str], play: bool = True) -> None:
        """MPC-HC's way: one file fills the playlist with its folder's media and plays it; several files (or a
        playlist file, or a folder) become the playlist."""
        paths = [os.path.abspath(p) for p in paths if p]
        files: list[str] = []
        start = 0
        if len(paths) == 1 and os.path.isfile(paths[0]) and media.ext(paths[0]) not in media.PLAYLIST_EXTS:
            folder_files = media.media_in(os.path.dirname(paths[0])) if settings.get_bool("folder_playlist", True) else []
            key = os.path.normcase(paths[0])
            if any(os.path.normcase(f) == key for f in folder_files):
                files = folder_files
                start = next(i for i, f in enumerate(files) if os.path.normcase(f) == key)
            else:
                files = [paths[0]]
        else:
            for p in paths:
                if os.path.isdir(p):
                    files += media.media_in(p)
                elif media.ext(p) in media.PLAYLIST_EXTS:
                    files += [f for f in media.read_m3u(p) if os.path.exists(f)]
                elif os.path.isfile(p):
                    files.append(p)
        if not files:
            if paths:
                self._ask("error", "There's nothing PR7-M can play there.")
            return
        self.model.set_files(files, -1)
        self.play_index(start, play=play)

    def play_index(self, i: int, play: bool = True) -> None:
        if not (0 <= i < len(self.model.files)):
            return
        self._remember()
        path = self.model.files[i]
        self.model.set_current(i)
        self.current = path
        self.hist.opened(path)
        self.ab = ABRepeat()
        self.seekbar.a = self.seekbar.b = None
        self.video.clear()
        self.video.audio_only = media.is_audio(path)
        self.video.tags = {}
        self.video.cover = QImage()
        self.player.setActiveSubtitleTrack(-1)
        self._load_sidecar(path)
        resume = self.hist.position(path) if settings.get_bool("remember_position", True) else 0
        self._pending_seek = resume or None
        self.player.setSource(QUrl.fromLocalFile(path))
        if play:
            self.player.play()
        self._sync()

    def step_file(self, delta: int) -> None:
        n = len(self.model.files)
        if not n:
            return
        i = self.model.current + delta
        if 0 <= i < n:
            self.play_index(i)

    def close_file(self):
        self._remember()
        self.player.stop()
        self.player.setSource(QUrl())
        self.current = None
        self.model.set_current(-1)
        self.video.clear()
        self.video.audio_only = False
        self.video.tags = {}
        self.cues, self.sub_file = [], None
        self._sync()

    def _remember(self):
        if self.current and self.player.duration() > 0:
            self.hist.remember(self.current, self.player.position(), self.player.duration())

    def _fill_recent(self):
        self.recent_menu.clear()
        for i, p in enumerate(self.hist.recent(10), 1):
            a = self.recent_menu.addAction(f"&{i % 10} {os.path.basename(p)}")
            a.setToolTip(p)
            a.triggered.connect(lambda _=False, x=p: self.open_paths([x]))

    # ================================================================== playing
    def play_pause(self):
        if not self.current:
            if self.model.files:
                self.play_index(max(self.model.current, 0))
            else:
                self.open_dialog()
            return
        if self.player.playbackState() == QMediaPlayer.PlaybackState.PlayingState:
            self.player.pause()
        else:
            self.player.play()

    def stop(self):
        self._remember()
        self.player.stop()
        self.player.setPosition(0)
        self._sync()

    def seek(self, ms: int):
        if self.current:
            self.player.setPosition(int(min(max(ms, 0), max(self.player.duration(), 0))))

    def jump(self, delta: int):
        self.seek(self.player.position() + delta)

    def frame_ms(self) -> float:
        fps = meta(self.player.metaData(), QMediaMetaData.Key.VideoFrameRate)
        try:
            fps = float(fps)
        except (TypeError, ValueError):
            fps = 0
        return 1000 / fps if fps and fps > 0 else 40.0

    def frame_step(self, direction: int):
        """Ctrl+Right / Ctrl+Left: pause, and move one frame (Qt has no frame step: one frame's time)."""
        if not self.current or not self.player.hasVideo():
            return
        self.player.pause()
        self.seek(int(round(self.player.position() + direction * self.frame_ms())))

    def apply_pitch(self):
        """Keep the voice's pitch when faster / slower (Qt's pitch compensation), where the backend can."""
        try:
            if self.player.pitchCompensationAvailability() != QMediaPlayer.PitchCompensationAvailability.Unavailable:
                self.player.setPitchCompensation(settings.get_bool("keep_pitch", True))
        except AttributeError:      # an older Qt
            pass

    def set_rate(self, rate: float):
        self.player.setPlaybackRate(rate)
        self._sync()

    def set_volume(self, value: int):
        value = int(min(max(value, 0), 100))
        self.audio.setVolume(value / 100)
        if self.volume.value != value:
            self.volume.set_value(value, emit=False)
        settings.set("volume", value)
        self._sync_status()

    def toggle_mute(self):
        self.audio.setMuted(not self.audio.isMuted())
        self.volume.muted = self.audio.isMuted()
        self.volume.update()
        settings.set("muted", self.audio.isMuted())
        self.acts["mute"].setChecked(self.audio.isMuted())
        self.k_mute.icon_name = "mute" if self.audio.isMuted() else "volume"
        self.k_mute.update()
        self._sync_status()

    def ab_press(self):
        if not self.current:
            return
        self.ab.press(self.player.position())
        self.seekbar.a, self.seekbar.b = self.ab.a, self.ab.b
        self.seekbar.update()
        self._sync_status()

    def go_to_dialog(self):
        if not self.current:
            return
        text = self._ask("goto", hms(self.player.position()))
        ms = media.parse_time(text) if text else None
        if ms is not None:
            self.seek(ms)

    # ================================================================== player events
    def _position(self, ms: int):
        back = self.ab.jump(ms)
        if back is not None:
            self.player.setPosition(back)
            return
        self.seekbar.set_position(ms)
        self.time.setText(f"{hms(ms)} / {hms(self.player.duration())}")
        if self.cues:
            text = media.cue_at(self.cues, ms)
            if text != self.video.subtitle:
                self.video.subtitle = text
                self.video.update()

    def _duration(self, ms: int):
        self.seekbar.duration = ms
        if self.current:
            self.model.set_duration(self.current, ms)
        self._sync_side()
        self.time.setText(f"{hms(self.player.position())} / {hms(ms)}")
        if self.player.mediaStatus() == QMediaPlayer.MediaStatus.BufferedMedia:
            self._apply_pending()

    def _apply_pending(self):
        """Resume (or a favorite's place) once playback is under way (Qt says "buffered"): a jump asked for while
        the file is still loading is dropped when playback starts from the beginning."""
        ms = self.player.duration()
        if self._pending_seek and ms > 0:
            pos = self._pending_seek
            self._pending_seek = None
            if pos < ms:
                self.player.setPosition(pos)

    def _status(self, status):
        S = QMediaPlayer.MediaStatus
        if status == S.BufferedMedia:
            self._apply_pending()
        if status == S.EndOfMedia:
            self.hist.remember(self.current, self.player.duration(), self.player.duration()) if self.current else None
            after = settings.get("after_playback", "play_next")
            if after == "repeat_file":
                self.player.setPosition(0); self.player.play()
            elif after in ("play_next", "repeat_list"):
                n = len(self.model.files)
                if self.model.current + 1 < n:
                    self.play_index(self.model.current + 1)
                elif after == "repeat_list" and n:
                    self.play_index(0)
            elif after == "close":
                QTimer.singleShot(0, self.close)
        elif status == S.InvalidMedia:
            self._error(None, self.player.errorString())
        self._sync()

    def _error(self, _err, text: str):
        if text and self.current:
            self.st_state.setText("ERROR")
            if "error" in self.answers or settings.get_bool("show_errors", True):
                self._ask("error", f"{os.path.basename(self.current)}\n\nPR7-M can't play this file:\n{text}")

    def _metadata(self):
        md = self.player.metaData()
        K = QMediaMetaData.Key

        def val(k):
            v = meta(md, k)
            if v is None:
                return ""
            if isinstance(v, (list, tuple)):
                return ", ".join(str(x) for x in v)
            return str(v)
        if self.video.audio_only:
            title = val(K.Title) or os.path.splitext(os.path.basename(self.current or ""))[0]
            artist = val(K.ContributingArtist) or val(K.AlbumArtist)
            album = val(K.AlbumTitle)
            year = val(K.Date)[:4]
            self.video.tags = {"title": title, "artist": artist,
                               "album": "  ·  ".join(x for x in (album, year) if x), "format": self._format_text()}
            for k in (K.CoverArtImage,):
                img = meta(md, k)
                if isinstance(img, QImage) and not img.isNull():
                    self.video.cover = img
                    break
            else:
                self.video.cover = self._folder_cover()
            self.video.update()
        self._sync_status()

    def _folder_cover(self) -> QImage:
        """cover.jpg / folder.jpg next to the music, as players look for."""
        if not self.current:
            return QImage()
        folder = os.path.dirname(self.current)
        for name in ("cover.jpg", "folder.jpg", "cover.png", "folder.png", "front.jpg", "albumart.jpg"):
            p = os.path.join(folder, name)
            if os.path.exists(p):
                img = QImage(p)
                if not img.isNull():
                    return img
        return QImage()

    def _format_text(self) -> str:
        md = self.player.metaData()
        K = QMediaMetaData.Key
        parts = []
        codec = meta(md, K.AudioCodec)
        if codec is not None:
            parts.append(str(codec).split(".")[-1].upper())
        br = meta(md, K.AudioBitRate)
        if br:
            try:
                parts.append(f"{int(br) // 1000} kbps")
            except (TypeError, ValueError):
                pass
        return "  ·  ".join(parts) or media.ext(self.current or "").upper()

    # ================================================================== subtitles and tracks
    def _load_sidecar(self, path: str):
        self.cues, self.sub_file = [], None
        if media.is_audio(path):
            return
        found = media.sidecar_subtitles(path)
        if found and settings.get_bool("auto_subtitles", True):
            self.load_subtitles(found[0])

    def load_subtitles(self, path: str) -> bool:
        cues = media.read_srt(path)
        if not cues:
            return False
        self.cues, self.sub_file = cues, path
        self.player.setActiveSubtitleTrack(-1)       # ours instead of the file's own
        self._position(self.player.position())
        self._sync()
        return True

    def load_subtitles_dialog(self):
        if not self.current:
            return
        path = self._ask("subtitles", os.path.dirname(self.current))
        if path and not self.load_subtitles(path):
            self._ask("error", f"{os.path.basename(path)} has no subtitles PR7-M can read.")

    def subtitles_off(self):
        self.cues, self.sub_file = [], None
        self.player.setActiveSubtitleTrack(-1)
        self.video.subtitle = ""
        self.video.update()
        self._sync()

    def _embedded_subtitle(self, text: str):
        if not self.cues:
            self.video.subtitle = text
            self.video.update()

    def _track_name(self, md, i: int) -> str:
        K = QMediaMetaData.Key
        title = meta(md, K.Title)
        lang = meta(md, K.Language)
        lang_name = ""
        if lang is not None:
            try:
                from PySide6.QtCore import QLocale
                lang_name = QLocale.languageToString(lang)
            except Exception:  # noqa: BLE001
                lang_name = str(lang)
        name = " - ".join(x for x in (str(title) if title else "", lang_name if lang_name != "C" else "") if x)
        return name or f"Track {i + 1}"

    def _fill_tracks(self, menu: QMenu, kind: str):
        menu.clear()
        g = QActionGroup(menu)
        if kind == "audio":
            tracks, active = self.player.audioTracks(), self.player.activeAudioTrack()
            setter = self.player.setActiveAudioTrack
        else:
            tracks, active = self.player.subtitleTracks(), self.player.activeSubtitleTrack()
            setter = self._set_subtitle_track
            off = menu.addAction("&Off")
            off.setCheckable(True)
            off.setChecked(active < 0 and not self.cues)
            off.triggered.connect(self.subtitles_off)
            g.addAction(off)
            if self.sub_file:
                a = menu.addAction(os.path.basename(self.sub_file))
                a.setCheckable(True); a.setChecked(True); g.addAction(a)
        for i, md in enumerate(tracks):
            a = menu.addAction(self._track_name(md, i))
            a.setCheckable(True)
            a.setChecked(i == active and not (kind == "subtitles" and self.cues))
            a.triggered.connect(lambda _=False, n=i: setter(n))
            g.addAction(a)
        if not tracks and kind == "audio":
            menu.addAction("(one audio track)").setEnabled(False)
        if kind == "subtitles":
            menu.addSeparator()
            menu.addAction(self.acts["load_subs"])

    def _set_subtitle_track(self, i: int):
        self.cues, self.sub_file = [], None
        self.player.setActiveSubtitleTrack(i)
        self._sync()

    def _subs_menu_at_key(self):
        self._fill_tracks(self.subs_menu, "subtitles")
        self.subs_menu.exec(self.k_subs.mapToGlobal(QPoint(0, -self.subs_menu.sizeHint().height())))
        self._sync()

    def _audio_menu_at_key(self):
        self._fill_tracks(self.audio_menu, "audio")
        self.audio_menu.exec(self.k_audio.mapToGlobal(QPoint(0, -self.audio_menu.sizeHint().height())))

    # ================================================================== the window follows
    def _sync(self):
        has = self.current is not None
        playing = self.player.playbackState() == QMediaPlayer.PlaybackState.PlayingState
        self.setWindowTitle(f"{os.path.basename(self.current)} - Paragon PR7-M" if has else "Paragon PR7-M")
        self.k_play.icon_name = "pause" if playing else "play"
        self.k_play.update()
        n, i = len(self.model.files), self.model.current
        self.k_prev.setEnabled(has and i > 0)
        self.k_next.setEnabled(has and 0 <= i < n - 1)
        for k in (self.k_stop, self.k_frame):
            k.setEnabled(has)
        self.k_frame.setEnabled(has and self.player.hasVideo())
        self.k_subs.setChecked(bool(self.cues) or self.player.activeSubtitleTrack() >= 0)
        self.k_full.setChecked(self.fullscreen)
        for key in ("close", "save_image", "properties", "stop", "frame_step", "frame_back", "rate_down", "rate_up",
                    "rate_reset", "ab", "goto", "fwd", "back", "fwd_long", "back_long", "load_subs"):
            self.acts[key].setEnabled(has)
        self.acts["save_image"].setEnabled(has and self.player.hasVideo())
        self.acts["prev"].setEnabled(has and i > 0)
        self.acts["next"].setEnabled(has and 0 <= i < n - 1)
        self.acts["save_playlist"].setEnabled(n > 0)
        self.acts["fullscreen"].setChecked(self.fullscreen)
        self._sync_status()
        self._sync_side()

    def _sync_side(self):
        n = len(self.model.files)
        total = self.model.total()
        self.side_label.setText("PLAYLIST" + (f"   ·   {n} FILE{'S' if n != 1 else ''}" if n else "") +
                                (f"   ·   {hms(total, always_hours=False)}" if total else ""))

    def _sync_status(self):
        S = QMediaPlayer.PlaybackState
        state = {S.PlayingState: "PLAYING", S.PausedState: "PAUSED", S.StoppedState: "STOPPED"}[
            self.player.playbackState()]
        if not self.current:
            state = ""
        if self.player.mediaStatus() == QMediaPlayer.MediaStatus.InvalidMedia:
            state = "ERROR"
        self.st_state.setText(state)
        md = self.player.metaData()
        K = QMediaMetaData.Key
        parts = []
        res = meta(md, K.Resolution)
        fps = meta(md, K.VideoFrameRate)
        if self.player.hasVideo() and res is not None and not res.isEmpty():
            parts.append(f"{res.width()} x {res.height()}" + (f"  ·  {float(fps):.3g} FPS" if fps else ""))
        codecs = []
        for k in (K.VideoCodec, K.AudioCodec):
            c = meta(md, k)
            if c is not None:
                codecs.append(str(c).split(".")[-1].upper())
        if codecs:
            parts.append("  ·  ".join(codecs))
        if self.cues and self.sub_file:
            parts.append(f"SUBTITLES: {os.path.basename(self.sub_file)}")
        elif self.player.activeSubtitleTrack() >= 0:
            tracks = self.player.subtitleTracks()
            i = self.player.activeSubtitleTrack()
            if i < len(tracks):
                parts.append(f"SUBTITLES: {self._track_name(tracks[i], i)}")
        extra = [f"{self.player.playbackRate():g}x"] if self.current else []
        if self.ab.active:
            extra.append(f"A-B {hms(self.ab.a)} - {hms(self.ab.b)}")
        elif self.ab.a is not None:
            extra.append(f"A {hms(self.ab.a)}")
        if self.audio.isMuted():
            extra.append("MUTED")
        elif self.current:
            extra.append(f"VOL {self.volume.value}%")
        parts.append("   ".join(extra))
        for lab, text in zip(self.st_parts, parts + [""] * 4):
            lab.setText(text)

    def _show(self, what: str, on: bool, save: bool = True):
        w = {"controls": self.controls, "statusbar": self.statusw}[what]
        w.setVisible(bool(on) and not self.fullscreen)
        if save:
            settings.set(f"show_{what}", bool(on))

    def show_playlist(self, on: bool, save: bool = True):
        self.side.setVisible(bool(on) and not self.fullscreen)
        self.k_list.setChecked(bool(on))
        if "playlist" in getattr(self, "acts", {}):
            self.acts["playlist"].setChecked(bool(on))
        if save:
            settings.set("show_playlist", bool(on))

    def _on_top(self, on: bool):
        self.setWindowFlag(Qt.WindowType.WindowStaysOnTopHint, bool(on))
        self.show()

    # ================================================================== full screen
    def toggle_fullscreen(self):
        self.fullscreen = not self.fullscreen
        for w in (self.menuBar(), self.header, self.statusw):
            w.setVisible(not self.fullscreen and (w is not self.statusw or self.acts["show_statusbar"].isChecked()))
        self.side.setVisible(not self.fullscreen and self.acts["playlist"].isChecked())
        if self.fullscreen:
            self._was_max = self.isMaximized()
            # The controls float over the picture (MPC-HC): taken out of the layout, shown when the mouse moves.
            self.central_layout.removeWidget(self.controls)
            self.controls.setParent(self.video)
            self.controls.floating = True
            self.showFullScreen()
            self._place_floating()
            self._mouse_moved()
        else:
            self.hide_timer.stop()
            self.controls.floating = False
            self.controls.setParent(None)
            self.central_layout.insertWidget(self.central_layout.indexOf(self.statusw), self.controls)
            self.controls.setVisible(self.acts["show_controls"].isChecked())
            self.video.unsetCursor()
            self.showMaximized() if getattr(self, "_was_max", False) else self.showNormal()
        self.seekbar.dark = self.fullscreen
        self._sync()

    def _place_floating(self):
        h = self.controls.sizeHint().height()
        self.controls.setGeometry(0, self.video.height() - h, self.video.width(), h)
        self.controls.raise_()

    def _mouse_moved(self):
        if self.fullscreen:
            self._place_floating()
            self.controls.show()
            self.video.unsetCursor()
            self.hide_timer.start()

    def _hide_floating(self):
        if self.fullscreen and not self.controls.underMouse():
            self.controls.hide()
            self.video.setCursor(Qt.CursorShape.BlankCursor)

    def resizeEvent(self, e):
        super().resizeEvent(e)
        if self.fullscreen:
            self._place_floating()

    def _escape(self):
        if self.fullscreen:
            self.toggle_fullscreen()

    # ================================================================== file commands
    def save_image(self):
        """File > Save image: the frame on screen as a picture."""
        if not self.video.frame.isValid():
            return
        img = self.video.frame.toImage()
        base = os.path.splitext(self.current or "frame")[0]
        default = f"{base} {hms(self.player.position()).replace(':', '.')}.png"
        target = self._ask("save_image", default)
        if target and not img.save(target):
            self._ask("error", f"Couldn't save {os.path.basename(target)}.")

    def save_playlist(self):
        if not self.model.files:
            return
        start = os.path.join(os.path.dirname(self.model.files[0]), "Playlist.m3u8")
        target = self._ask("save_playlist", start)
        if target:
            media.write_m3u(target, self.model.files, self.model.durations)

    def properties(self):
        if not self.current:
            return
        dlg = PropertiesDialog(self)
        self.properties_dialog = dlg
        if "properties" not in self.answers:
            dlg.exec()

    def _playlist_menu(self, pos):
        rows = sorted({i.row() for i in self.list.selectedIndexes()})
        m = QMenu(self)
        if rows:
            m.addAction("&Play", lambda: self.play_index(rows[0]))
            m.addAction("&Remove", lambda: self._remove_rows(rows))
            m.addSeparator()
            m.addAction("Move &up", lambda: self._move(rows[0], -1))
            m.addAction("Move &down", lambda: self._move(rows[0], 1))
            m.addSeparator()
        m.addAction("&Add files...", self._add_files)
        m.addAction("&Sort by name", self._sort)
        m.addAction("&Clear", self._clear_playlist)
        m.addSeparator()
        m.addAction(self.acts["save_playlist"])
        m.exec(self.list.viewport().mapToGlobal(pos))

    def _remove_rows(self, rows):
        playing = self.current
        self.model.remove(rows)
        if playing and playing not in self.model.files:
            self.close_file()
        self._sync()

    def _move(self, row, d):
        self.model.move(row, row + d)
        self._sync()

    def _add_files(self):
        files = self._ask("open")
        if files:
            self.model.set_files(self.model.files + [f for f in files if media.is_media(f)], self.model.current)
            self._sync()

    def _sort(self):
        cur = self.current
        files = sorted(self.model.files, key=lambda p: media.natural_key(os.path.basename(p)))
        self.model.set_files(files, files.index(cur) if cur in files else -1)

    def _clear_playlist(self):
        self.close_file()
        self.model.set_files([], -1)
        self._sync()

    # ================================================================== favorites
    def _fill_favorites(self):
        m = self.fav_menu
        m.clear()
        a = m.addAction("&Add to favorites...", self.add_favorite)
        a.setEnabled(self.current is not None)
        favs = settings.get_list("favorites")
        if favs:
            m.addAction("&Clear favorites", lambda: settings.set("favorites", []))
            m.addSeparator()
        for fav in favs:
            if os.path.exists(fav.get("path", "")):
                act = m.addAction(f"{fav['name']}  {hms(fav.get('position', 0))}")
                act.triggered.connect(lambda _=False, f=fav: self.open_favorite(f))

    def add_favorite(self):
        if not self.current:
            return
        name = self._ask("favorite", os.path.basename(self.current))
        if name:
            favs = settings.get_list("favorites")
            favs.append({"name": name, "path": self.current, "position": self.player.position()})
            settings.set("favorites", favs)

    def open_favorite(self, fav: dict):
        self.open_paths([fav["path"]])
        self._pending_seek = int(fav.get("position", 0)) or None
        if self.player.duration() > 0 and self._pending_seek:
            self.player.setPosition(self._pending_seek)
            self._pending_seek = None

    # ================================================================== settings and help
    def set_finish(self, name: str):
        theme.set_finish(name)
        settings.set("finish", name)
        apply_look(QApplication.instance())
        for gap in self.controls.findChildren(QFrame, "gap"):
            gap.setStyleSheet(f"background: {theme.PANEL_EDGE};")
        self.list.setStyleSheet(f"QListView {{ background: {theme.BACKGROUND}; border: none; }}")
        self.acts[f"finish_{name}"].setChecked(True)
        for w in (self.header, self.video, self.seekbar, self.volume, self.list.viewport()):
            w.update()

    def options(self):
        dlg = OptionsDialog(self)
        self.options_dialog = dlg
        if "options" not in self.answers and dlg.exec() == QDialog.DialogCode.Accepted:
            dlg.apply()

    def check_updates(self):
        from . import update_ui
        update_ui.check(self, manual=True)

    def shortcuts(self):
        rows = [("Play / pause", "Space, click the picture"), ("Stop", "."), ("Previous / next file", "PgUp / PgDn"),
                ("Jump 10 s back / forward", "Left / Right"), ("Jump 1 min", "Ctrl+Shift+Left / Right"),
                ("Frame step back / forward", "Ctrl+Left / Ctrl+Right"), ("Go to a time", "Ctrl+G"),
                ("Slower / faster / normal speed", "Ctrl+Down / Ctrl+Up / Ctrl+R"), ("A-B repeat", "Ctrl+B"),
                ("Volume", "Up / Down, the wheel"), ("Mute", "Ctrl+M"), ("Full screen", "Alt+Enter, double-click"),
                ("Leave full screen", "Esc"), ("Playlist", "Ctrl+7"), ("Load subtitles", "Ctrl+L"),
                ("Open file / quick open", "Ctrl+O / Ctrl+Q"), ("Save image", "Alt+I"), ("Properties", "Shift+F10")]
        dlg = QDialog(self)
        dlg.setWindowTitle("Keyboard Shortcuts")
        form = QFormLayout(dlg)
        for what, keys in rows:
            k = QLabel(keys); k.setStyleSheet(f"color: {theme.TEXT_DIM};")
            form.addRow(what, k)
        bb = QDialogButtonBox(QDialogButtonBox.StandardButton.Close); bb.rejected.connect(dlg.reject)
        form.addRow(bb)
        self.shortcuts_dialog = dlg
        if "shortcuts" not in self.answers:
            dlg.exec()

    def about(self):
        if "about" not in self.answers:
            QMessageBox.about(self, "About Paragon PR7-M",
                              f"<b>Paragon PR7-M</b> {__version__}<br>A media player that works the way MPC-HC works."
                              "<br><br>Playback by Qt Multimedia with FFmpeg (LGPL).")

    # ================================================================== drag and drop, closing
    def dragEnterEvent(self, e):
        if e.mimeData().hasUrls():
            e.acceptProposedAction()

    def dropEvent(self, e):
        files = [u.toLocalFile() for u in e.mimeData().urls() if u.isLocalFile()]
        subs = [f for f in files if f.lower().endswith(".srt")]
        if subs and self.current and len(files) == 1:
            self.load_subtitles(subs[0])
        elif files:
            self.open_paths(files)
        e.acceptProposedAction()

    def closeEvent(self, e):
        self._remember()
        self.player.stop()
        super().closeEvent(e)


class PropertiesDialog(QDialog):
    """File > Properties: the file and what Qt Multimedia read from it."""

    def __init__(self, w: MainWindow):
        super().__init__(w)
        self.setWindowTitle("Properties")
        self.setMinimumWidth(480)
        g = QGridLayout(self)
        g.setHorizontalSpacing(18)
        md = w.player.metaData()
        K = QMediaMetaData.Key
        path = w.current
        size = os.path.getsize(path) if path and os.path.exists(path) else 0
        rows = [("File name", os.path.basename(path or "")), ("Folder", os.path.dirname(path or "")),
                ("Size", f"{size / 1024 / 1024:.2f} MB ({size:,} bytes)"), ("Length", hms(w.player.duration()))]
        for label, key in (("Title", K.Title), ("Artist", K.ContributingArtist), ("Album", K.AlbumTitle),
                           ("Genre", K.Genre), ("Date", K.Date), ("Container", K.FileFormat),
                           ("Video", K.VideoCodec), ("Resolution", K.Resolution), ("Frame rate", K.VideoFrameRate),
                           ("Video bit rate", K.VideoBitRate), ("Audio", K.AudioCodec), ("Audio bit rate", K.AudioBitRate)):
            v = meta(md, key)
            if v is None or v == "" or (hasattr(v, "isEmpty") and v.isEmpty()):
                continue
            if key == K.Resolution:
                v = f"{v.width()} x {v.height()}"
            elif key in (K.VideoBitRate, K.AudioBitRate):
                v = f"{int(v) // 1000} kbps"
            elif key == K.VideoFrameRate:
                v = f"{float(v):.3f} fps"
            elif hasattr(v, "toString"):
                v = v.toString()
            else:
                v = str(v).split(".")[-1] if key in (K.FileFormat, K.VideoCodec, K.AudioCodec) else str(v)
            rows.append((label, str(v)))
        rows.append(("Audio tracks", str(max(1, len(w.player.audioTracks()))) if w.player.hasAudio() else "0"))
        rows.append(("Subtitle tracks", str(len(w.player.subtitleTracks()))))
        self.rows = rows
        for i, (k, v) in enumerate(rows):
            a = QLabel(k.upper()); a.setObjectName("key")
            b = QLabel(v); b.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse); b.setWordWrap(True)
            g.addWidget(a, i, 0); g.addWidget(b, i, 1)
        bb = QDialogButtonBox(QDialogButtonBox.StandardButton.Close); bb.rejected.connect(self.reject)
        g.addWidget(bb, len(rows), 0, 1, 2)

    def values(self) -> dict:
        return dict(self.rows)


class OptionsDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Options")
        self.setMinimumWidth(460)
        form = QFormLayout(self)
        self.remember = QCheckBox("Remember where each file was left (resume)")
        self.remember.setChecked(settings.get_bool("remember_position", True))
        self.folder = QCheckBox("Opening a file fills the playlist with its folder")
        self.folder.setChecked(settings.get_bool("folder_playlist", True))
        self.subs = QCheckBox("Load a subtitle file with the film's name automatically")
        self.subs.setChecked(settings.get_bool("auto_subtitles", True))
        self.pitch = QCheckBox("Keep the pitch when playing faster or slower")
        self.pitch.setChecked(settings.get_bool("keep_pitch", True))
        from . import update_ui
        self.updates = QCheckBox("Check for updates automatically"); self.updates.setChecked(update_ui.auto_enabled())
        for w in (self.remember, self.folder, self.subs, self.pitch, self.updates):
            form.addRow(w)
        from . import shell
        if shell.available():
            b = QPushButton("Make PR7-M the default media player...")
            b.clicked.connect(shell.open_default_apps)
            form.addRow(b)
        bb = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        bb.accepted.connect(self.accept); bb.rejected.connect(self.reject)
        form.addRow(bb)

    def apply(self):
        from . import update_ui
        settings.set("remember_position", self.remember.isChecked())
        settings.set("folder_playlist", self.folder.isChecked())
        settings.set("auto_subtitles", self.subs.isChecked())
        settings.set("keep_pitch", self.pitch.isChecked())
        w = self.parent()
        if isinstance(w, MainWindow):
            w.apply_pitch()
        update_ui.set_auto(self.updates.isChecked())
