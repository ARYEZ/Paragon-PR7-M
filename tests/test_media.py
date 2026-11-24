import os

from pr7m import media
from pr7m.media import ABRepeat, History


def test_media_files_and_folder_order(tmp_path):
    for n in ("ep10.mkv", "ep2.mkv", "Song.FLAC", "notes.txt", ".hidden.mp4", "cover.jpg"):
        (tmp_path / n).write_bytes(b"x")
    assert [os.path.basename(p) for p in media.media_in(str(tmp_path))] == ["ep2.mkv", "ep10.mkv", "Song.FLAC"]
    assert media.is_audio("a.flac") and not media.is_audio("a.mkv") and media.is_media("x.WEBM")


def test_time_text():
    assert media.hms(3_723_000) == "01:02:03" and media.hms(277_000, always_hours=False) == "4:37"
    assert media.hms(6_300_000, always_hours=False) == "1:45:00"
    assert media.parse_time("1:02:03") == 3_723_000 and media.parse_time("62:03") == 3_723_000
    assert media.parse_time("95") == 95_000 and media.parse_time("0:00:01.5") == 1_500
    assert media.parse_time("abc") is None and media.parse_time("") is None


def test_rates():
    assert media.step_rate(1.0, True) == 1.25 and media.step_rate(1.0, False) == 0.75
    assert media.step_rate(4.0, True) == 4.0 and media.step_rate(0.25, False) == 0.25


def test_subrip():
    text = "﻿1\r\n00:00:01,000 --> 00:00:02,500\r\n<i>Hello</i>\r\n\r\n2\r\n00:00:03.000 --> 00:00:04,000\r\n" \
           "{\\an8}Two\r\nlines\r\n\r\n3\r\n00:00:05,000 --> 00:00:06,000\r\n\r\n"
    cues = media.parse_srt(text)
    assert [(c.start, c.end, c.text) for c in cues] == [(1000, 2500, "Hello"), (3000, 4000, "Two\nlines")]
    assert media.cue_at(cues, 1200) == "Hello" and media.cue_at(cues, 2600) == ""


def test_sidecar_subtitles(tmp_path):
    for n in ("Film.mkv", "Film.srt", "Film.en.srt", "Other.srt"):
        (tmp_path / n).write_text("x")
    found = [os.path.basename(p) for p in media.sidecar_subtitles(str(tmp_path / "Film.mkv"))]
    assert found == ["Film.srt", "Film.en.srt"]


def test_m3u_round_trip(tmp_path):
    (tmp_path / "sub").mkdir()
    files = [str(tmp_path / "a.mp4"), str(tmp_path / "sub" / "b é.mkv")]
    pl = tmp_path / "list.m3u8"
    media.write_m3u(str(pl), files, {files[0]: 125_000})
    text = pl.read_text(encoding="utf-8")
    assert text.startswith("#EXTM3U") and "#EXTINF:125,a" in text and os.path.join("sub", "b é.mkv") in text
    assert media.read_m3u(str(pl)) == files


def test_ab_repeat():
    ab = ABRepeat()
    assert ab.press(1000) == "A" and ab.jump(5000) is None
    assert ab.press(3000) == "AB" and ab.active
    assert ab.jump(2000) is None and ab.jump(3000) == 1000
    assert ab.press(4000) == "" and not ab.active


def test_resume_rules(tmp_path):
    h = History(tmp_path / "h.json")
    f = str(tmp_path / "film.mkv")
    h.remember(f, 600_000, 6_000_000)
    assert History(tmp_path / "h.json").position(f) == 600_000
    h.remember(f, 5_990_000, 6_000_000)          # nearly finished: starts again
    assert h.position(f) == 0
    short = str(tmp_path / "clip.mp4")
    h.remember(short, 20_000, 40_000)            # under a minute: always from the start
    assert h.position(short) == 0
