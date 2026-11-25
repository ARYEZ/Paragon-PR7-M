"""One finish for the family (finish.py, the same in every family app): choosing OBSIDIAN or AURORA in one app
switches every app, open ones too. The conftest points PARAGON_FAMILY_FILE at a temp file: never the registry."""
import os

import pytest

from pr7m import finish, theme


@pytest.fixture
def family(tmp_path, monkeypatch):
    f = tmp_path / "family-finish"
    monkeypatch.setenv("PARAGON_FAMILY_FILE", str(f))
    return f


@pytest.fixture
def subject(qapp, tmp_path, request):
    """This app's real window (or tray), the thing that switches."""
    yield request.getfixturevalue("window")


def test_nothing_chosen_yet_starts_with_the_apps_own(family):
    assert finish.get() is None and finish.start("aurora") == "aurora" and finish.start("nonsense") == "obsidian"


def test_a_choice_is_shared_and_followed(family):
    finish.set("aurora")
    assert family.read_text() == "aurora" and finish.get() == "aurora" and finish.start("obsidian") == "aurora"
    seen = []
    assert finish.follow("obsidian", seen.append) and seen == ["aurora"]      # another app switched: follow it
    assert not finish.follow("aurora", seen.append) and seen == ["aurora"]    # already there: nothing to do
    finish.set("NOT-A-FINISH")
    assert finish.get() == "aurora"                                          # only the family's two finishes
    family.write_text("garbage")
    assert finish.get() is None


def test_this_app_shares_its_choice_and_follows_the_family(family, subject):
    try:
        subject.set_finish("obsidian")
        assert family.read_text() == "obsidian"                               # chosen here: saved for the family
        family.write_text("aurora")                                           # chosen in another family app
        assert finish.follow(theme.FINISH, subject.set_finish) and theme.FINISH == "aurora"
        subject.set_finish("obsidian")                                        # and back, from here
        assert family.read_text() == "obsidian" and theme.FINISH == "obsidian"
    finally:
        theme.set_finish("obsidian")
