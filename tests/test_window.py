import json
import os
import sys
import time

import pytest
from PySide6.QtGui import QColor, QImage
from PySide6.QtMultimedia import QMediaPlayer

import sample_media
from pr7m import __version__, app, media, settings, theme

Playing = QMediaPlayer.PlaybackState.PlayingState
Paused = QMediaPlayer.PlaybackState.PausedState


def colour_near(c: QColor, hex_: str, tol=40) -> bool:
    w = QColor(hex_)
    return abs(c.red() - w.red()) < tol and abs(c.green() - w.green()) < tol and abs(c.blue() - w.blue()) < tol


def shown_colour(window) -> QColor:
    img = window.video.frame.toImage()
    return img.pixelColor(10, 10)


def test_starts_empty(window):
    assert window.current is None and window.windowTitle() == "Paragon PR7-M"
    assert not window.acts["stop"].isEnabled() and window.time.text() == "00:00:00 / 00:00:00"


def test_open_fills_the_playlist_with_the_folder_and_plays(window, films, wait):
    window.open_paths([str(films[0])])
    assert window.model.files == [str(f) for f in films] and window.model.current == 0
    assert window.windowTitle() == "01 Clip.avi - Paragon PR7-M"
    assert wait(lambda: window.player.duration() == 3000 and window.video.frames_seen > 3)
    assert window.player.playbackState() == Playing and window.st_state.text() == "PLAYING"
    assert colour_near(shown_colour(window), sample_media.COLOURS[0])          # the first second's colour
    assert window.side_label.text().startswith("PLAYLIST   ·   3 FILES")


def test_seek_jump_and_go_to(window, films, wait):
    window.open_paths([str(films[0])])
    assert wait(lambda: window.player.duration() == 3000)
    window.player.pause()
    window.seek(2200)
    assert wait(lambda: abs(window.player.position() - 2200) < 150)
    assert wait(lambda: colour_near(shown_colour(window), sample_media.colour_of(22, 10)))   # the third second's
    window.acts["back"].trigger()                                   # 10 s back: the start
    assert wait(lambda: window.player.position() < 150)
    window.answers["goto"] = "0:00:01.5"
    window.acts["goto"].trigger()
    assert wait(lambda: abs(window.player.position() - 1500) < 150)


def test_frame_step(window, films, wait):
    window.open_paths([str(films[0])])
    assert wait(lambda: window.player.duration() == 3000)
    window.seek(1000)
    assert wait(lambda: abs(window.player.position() - 1000) < 120)
    before = window.player.position()
    window.acts["frame_step"].trigger()
    assert window.player.playbackState() == Paused
    assert wait(lambda: window.player.position() > before)
    assert abs(window.player.position() - before - 100) < 60                       # one frame at 10 fps


def test_next_previous_and_after_playback(window, films, wait):
    window.open_paths([str(films[0])])
    window.acts["next"].trigger()
    assert window.current == str(films[1]) and window.model.current == 1
    window.acts["prev"].trigger()
    assert window.current == str(films[0])
    window.step_file(1)                                               # 02 Second.avi, 2 s
    assert wait(lambda: window.current == str(films[2]), 12)          # played to the end: the next one starts
    assert window.video.audio_only
    settings.set("after_playback", "repeat_file")
    assert wait(lambda: window.player.duration() == 2000)
    window.seek(1800)
    assert wait(lambda: window.player.position() < 600 and window.player.playbackState() == Playing, 8)
    assert window.current == str(films[2])                            # repeated, not moved on


def test_resume_where_the_file_was_left(window, tmp_path, wait):
    long_clip = sample_media.avi(tmp_path / "Long.avi", seconds=70, fps=2, size=(64, 36))
    window.open_paths([str(long_clip)])
    assert wait(lambda: window.player.duration() == 70_000)
    window.seek(30_000)
    assert wait(lambda: abs(window.player.position() - 30_000) < 600)
    window.close_file()
    window.open_paths([str(long_clip)])
    assert wait(lambda: abs(window.player.position() - 30_000) < 1500)


def test_subtitles_from_the_srt_next_to_the_film(window, films, wait):
    window.open_paths([str(films[0])])
    assert window.sub_file == str(films[0].with_suffix(".srt"))
    assert wait(lambda: window.player.duration() == 3000)
    window.player.pause()
    window.seek(1800)
    assert wait(lambda: window.video.subtitle == "Second line\nwith two rows")
    assert "SUBTITLES: 01 Clip.srt" in [l.text() for l in window.st_parts]
    window.subtitles_off()
    assert window.video.subtitle == "" and not window.cues
    own = films[0].parent / "own.srt"
    sample_media.srt(own, [(0.0, 3.0, "Mine")])
    window.answers["subtitles"] = str(own)
    window.acts["load_subs"].trigger()
    window.seek(500)
    assert wait(lambda: window.video.subtitle == "Mine")


def test_rate_volume_mute_and_ab(window, films, wait):
    window.open_paths([str(films[0])])
    assert wait(lambda: window.player.duration() == 3000)
    window.acts["rate_up"].trigger()
    assert window.player.playbackRate() == 1.25
    window.acts["rate_reset"].trigger()
    assert window.player.playbackRate() == 1.0
    window.set_volume(40)
    assert round(window.audio.volume(), 2) == 0.4 and settings.get_int("volume") == 40
    window.acts["vol_down"].trigger()
    assert window.volume.value == 35
    window.toggle_mute()
    assert window.audio.isMuted() and any("MUTED" in lab.text() for lab in window.st_parts)
    window.toggle_mute()
    window.player.pause()
    window.seek(500)
    assert wait(lambda: abs(window.player.position() - 500) < 120)
    window.ab_press()
    window.seek(1200)
    assert wait(lambda: abs(window.player.position() - 1200) < 120)
    window.ab_press()
    assert window.ab.active and (window.ab.a, window.ab.b) == (pytest.approx(500, abs=120), pytest.approx(1200, abs=120))
    window.player.play()
    assert wait(lambda: window.player.position() < 1000 and window.player.position() > 400, 5)   # back to A


def test_music_shows_its_cover_and_tags(window, films, wait):
    QImage(64, 64, QImage.Format.Format_RGB32).save(str(films[0].parent / "cover.jpg"))
    window.open_paths([str(films[2])])
    assert window.video.audio_only
    assert wait(lambda: window.player.duration() == 2000)
    assert wait(lambda: window.video.tags.get("title") == "03 Tone")
    assert not window.video.cover.isNull()


def test_save_image_and_playlist(window, films, wait, tmp_path):
    window.open_paths([str(films[0])])
    assert wait(lambda: window.video.frames_seen > 2)
    window.player.pause()
    out = tmp_path / "frame.png"
    window.answers["save_image"] = str(out)
    window.acts["save_image"].trigger()
    img = QImage(str(out))
    assert img.width() == 320 and img.height() == 180
    pl = tmp_path / "Saved.m3u8"
    window.answers["save_playlist"] = str(pl)
    window.acts["save_playlist"].trigger()
    assert media.read_m3u(str(pl)) == [str(f) for f in films]
    window.open_paths([str(pl)])                       # a playlist opens as the playlist
    assert window.model.files == [str(f) for f in films]


def test_fullscreen_floats_the_controls(window, films, wait):
    window.open_paths([str(films[0])])
    window.toggle_fullscreen()
    assert window.fullscreen and window.controls.parent() is window.video and window.controls.floating
    assert not window.menuBar().isVisible() and not window.side.isVisible()
    window._escape()
    assert not window.fullscreen and window.controls.parent() is not window.video and window.menuBar().isVisible()


def test_properties_and_finish(window, films, wait):
    window.answers["properties"] = True
    window.open_paths([str(films[0])])
    assert wait(lambda: window.player.duration() == 3000)
    window.properties()
    v = window.properties_dialog.values()
    assert v["File name"] == "01 Clip.avi" and v["Length"] == "00:00:03"
    window.set_finish("aurora")
    assert theme.FINISH == "aurora"
    window.set_finish("obsidian")


# ---------------------------------------------------------------- the command line
def test_version(capsys):
    assert app.main(["--version"]) == 0 and capsys.readouterr().out.strip() == __version__


def test_a_second_launch_plays_in_the_open_window(window, films, qapp, wait):
    import subprocess
    assert app.listen(window) is not None
    code = ("import sys; sys.path.insert(0, sys.argv[1]); from PySide6.QtCore import QCoreApplication; "
            "a = QCoreApplication([]); from pr7m import app; sys.exit(0 if app.hand_over(sys.argv[2]) else 1)")
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    proc = subprocess.Popen([sys.executable, "-c", code, root, str(films[1])])
    assert wait(lambda: window.current == str(films[1]) and proc.poll() is not None, 20)
    assert proc.wait(5) == 0
    window.instance_server.close()


def test_selftest(films, tmp_path, qapp):
    out = tmp_path / "out.json"
    assert app.selftest(str(films[0].parent), str(out)) == 0
    got = json.loads(out.read_text())
    clip, tone = got["played"]["01 Clip.avi"], got["played"]["03 Tone.wav"]
    assert clip["duration"] == 3000 and clip["video"] and clip["frames"] > 0
    assert tone["duration"] == 2000 and tone["audio"] and not tone["video"] and got["fonts"]
