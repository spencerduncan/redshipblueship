#!/usr/bin/env python3
"""Hermetic lock for the curated-archive split (#577 M1).

scripts/make_redship_otr.py carves the curated cross-game archives out of the
extracted game archives.  Since #577 M1 it writes TWO of them, one per host:

  redship-oot.o2r  <- manifest `mm->oot` entries (MM content OoT draws)
  redship-mm.o2r   <- manifest `oot->mm` entries (OoT content MM draws)

Each half is mounted with its host's identity (rsbs/src/main.cpp), so a
resource landing in the wrong half would be parsed by the wrong game's
per-archive dispatcher.  The ROM-staged CuratedArchiveGenerator CTest row runs
the generator against the REAL extracted archives; this file runs it against
synthetic ones so the direction split is locked in the one CI job that has no
ROM (python-tests), and needs neither a build nor a display.

The synthetic resources carry a 64-byte OTR header whose type tag is none of
the kinds the generator inspects (display list, array, dispatched Room/
Cutscene/Path), so only the manifest parsing, the direction split, the
collision guard and the empty-half refusal are exercised here.  The
content-walking guards keep their ROM-staged lock.
"""

import subprocess
import sys
import zipfile
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
GENERATOR = REPO_ROOT / "scripts" / "make_redship_otr.py"
SHIPPED_MANIFEST = REPO_ROOT / "assets" / "crossgame" / "manifest.txt"

# A resource the generator's content guards all pass over: a little-endian
# header (byte 0 == 0) with an 'OTEX' type tag, padded to the 64-byte header.
_HEADER = bytes([0, 0, 0, 0]) + b"XETO" + bytes(56)


def _resource(tag):
    return _HEADER + tag.encode("ascii")


# Source archives.  MM_ONLY / OOT_ONLY are exclusive to one game; SHARED sits
# in both, i.e. a colliding path.
MM_ONLY = ["objects/object_mm_only/gMmOnlyDL", "objects/object_mm_only/gMmOnlyVtx"]
OOT_ONLY = ["objects/object_oot_only/gOotOnlyDL"]
SHARED = "objects/object_shared/gSharedDL"


@pytest.fixture
def archives(tmp_path):
    oot = tmp_path / "oot.o2r"
    mm = tmp_path / "mm.o2r"
    with zipfile.ZipFile(oot, "w") as z:
        for name in OOT_ONLY + [SHARED]:
            z.writestr(name, _resource("oot:" + name))
    with zipfile.ZipFile(mm, "w") as z:
        for name in MM_ONLY + [SHARED]:
            z.writestr(name, _resource("mm:" + name))
    return oot, mm


def _run(tmp_path, archives, manifest_text, extra_args=()):
    oot, mm = archives
    manifest = tmp_path / "manifest.txt"
    manifest.write_text(manifest_text, encoding="utf-8")
    out_oot = tmp_path / "out" / "redship-oot.o2r"
    out_mm = tmp_path / "out" / "redship-mm.o2r"
    proc = subprocess.run(
        [
            sys.executable,
            str(GENERATOR),
            "--oot-archive", str(oot),
            "--mm-archive", str(mm),
            "--manifest", str(manifest),
            "--out-oot", str(out_oot),
            "--out-mm", str(out_mm),
            *extra_args,
        ],
        capture_output=True,
        text=True,
    )
    return proc, out_oot, out_mm


def _names(path):
    with zipfile.ZipFile(path) as z:
        return sorted(z.namelist())


def _payload(path, name):
    with zipfile.ZipFile(path) as z:
        return z.read(name)


def test_each_direction_lands_in_its_hosts_half(tmp_path, archives):
    proc, out_oot, out_mm = _run(
        tmp_path, archives, "mm->oot objects/object_mm_only/\noot->mm objects/object_oot_only/\n")
    assert proc.returncode == 0, proc.stdout + proc.stderr
    # MM-origin content is what OoT draws: it lands in OoT's half, and only there.
    assert _names(out_oot) == sorted(MM_ONLY)
    assert _names(out_mm) == sorted(OOT_ONLY)
    # Copied VERBATIM from the source archive (constraint 1): the bytes are the
    # source game's, not the host's.
    assert _payload(out_oot, MM_ONLY[0]) == _resource("mm:" + MM_ONLY[0])
    assert _payload(out_mm, OOT_ONLY[0]) == _resource("oot:" + OOT_ONLY[0])


def test_single_path_entries_and_overlapping_prefixes(tmp_path, archives):
    # One exact path plus a whole-directory prefix that also covers it: the
    # path is written once, not twice.
    proc, out_oot, out_mm = _run(
        tmp_path, archives,
        "mm->oot %s\nmm->oot objects/object_mm_only/\noot->mm %s\n" % (MM_ONLY[0], OOT_ONLY[0]))
    assert proc.returncode == 0, proc.stdout + proc.stderr
    with zipfile.ZipFile(out_oot) as z:
        names = [info.filename for info in z.infolist()]
    assert sorted(names) == sorted(MM_ONLY)
    assert _names(out_mm) == sorted(OOT_ONLY)


@pytest.mark.parametrize(
    "manifest_text, reason",
    [
        # The pre-split two-column form: ambiguous now that there are two halves.
        ("mm objects/object_mm_only/\noot->mm objects/object_oot_only/\n", "pre-split"),
        # A direction that does not cross games.
        ("mm->mm objects/object_mm_only/\noot->mm objects/object_oot_only/\n", "unknown direction"),
        # Either half empty: the runtime mounts both.
        ("mm->oot objects/object_mm_only/\n", "redship-mm.o2r"),
        ("oot->mm objects/object_oot_only/\n", "redship-oot.o2r"),
        # A path the HOST also ships (constraint 2), in each direction.
        ("mm->oot %s\noot->mm objects/object_oot_only/\n" % SHARED, "COLLISION"),
        ("mm->oot objects/object_mm_only/\noot->mm %s\n" % SHARED, "COLLISION"),
        # A prefix that selects nothing.
        ("mm->oot objects/object_missing/\noot->mm objects/object_oot_only/\n", "matched nothing"),
    ],
)
def test_refusals_write_neither_half(tmp_path, archives, manifest_text, reason):
    proc, out_oot, out_mm = _run(tmp_path, archives, manifest_text)
    assert proc.returncode != 0, "accepted a manifest it must refuse (%s)" % reason
    assert reason in proc.stdout + proc.stderr
    # A refusal never leaves a half-written archive behind for a build step to copy.
    assert not out_oot.exists()
    assert not out_mm.exists()


_SPLIT_MANIFEST = "mm->oot objects/object_mm_only/\noot->mm objects/object_oot_only/\n"


def _port_archive(tmp_path, name, paths):
    path = tmp_path / name
    with zipfile.ZipFile(path, "w") as z:
        for p in paths:
            z.writestr(p, _resource("port:" + p))
    return path


@pytest.mark.parametrize(
    "flag, game, name, curated",
    [
        # soh.o2r / oot-mq.o2r sit under OoT's half, 2ship.o2r under MM's
        # (Combo_EnsureGameArchivesLoaded mounts them before the curated half).
        ("--host-archive", "oot", "soh.o2r", MM_ONLY[0]),
        ("--optional-host-archive", "oot", "oot-mq.o2r", MM_ONLY[1]),
        ("--host-archive", "mm", "2ship.o2r", OOT_ONLY[0]),
    ],
)
def test_collision_with_any_host_base_archive_is_refused(tmp_path, archives, flag, game, name, curated):
    port = _port_archive(tmp_path, name, [curated])
    proc, out_oot, out_mm = _run(tmp_path, archives, _SPLIT_MANIFEST, [flag, "%s=%s" % (game, port)])
    assert proc.returncode != 0, "accepted a curated path the host's %s also ships" % name
    assert "COLLISION" in proc.stdout + proc.stderr
    assert name in proc.stderr
    assert not out_oot.exists()
    assert not out_mm.exists()


def test_host_base_archive_on_the_other_side_is_not_a_collision(tmp_path, archives):
    # MM's 2ship.o2r carrying an MM-origin path that lands in OoT's half is not
    # a collision: OoT never mounts 2ship.o2r.
    port = _port_archive(tmp_path, "2ship.o2r", [MM_ONLY[0]])
    proc, out_oot, _out_mm = _run(tmp_path, archives, _SPLIT_MANIFEST, ["--host-archive", "mm=%s" % port])
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert _names(out_oot) == sorted(MM_ONLY)


def test_missing_required_host_archive_is_refused(tmp_path, archives):
    missing = tmp_path / "soh.o2r"
    proc, out_oot, out_mm = _run(tmp_path, archives, _SPLIT_MANIFEST, ["--host-archive", "oot=%s" % missing])
    assert proc.returncode != 0
    assert "not present" in proc.stdout + proc.stderr
    assert not out_oot.exists()
    assert not out_mm.exists()


def test_missing_optional_host_archive_is_noted_not_refused(tmp_path, archives):
    missing = tmp_path / "oot-mq.o2r"
    proc, out_oot, out_mm = _run(tmp_path, archives, _SPLIT_MANIFEST,
                                 ["--optional-host-archive", "oot=%s" % missing])
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "not checked" in proc.stdout
    assert out_oot.exists() and out_mm.exists()


def test_same_output_path_is_refused(tmp_path, archives):
    oot, mm = archives
    manifest = tmp_path / "manifest.txt"
    manifest.write_text("mm->oot objects/object_mm_only/\noot->mm objects/object_oot_only/\n", encoding="utf-8")
    same = tmp_path / "both.o2r"
    proc = subprocess.run(
        [sys.executable, str(GENERATOR), "--oot-archive", str(oot), "--mm-archive", str(mm),
         "--manifest", str(manifest), "--out-oot", str(same), "--out-mm", str(same)],
        capture_output=True, text=True)
    assert proc.returncode != 0
    assert not same.exists()


def test_shipped_manifest_fills_both_halves():
    """The real manifest parses and names both directions -- a manifest that
    fills only one half would build nothing at GenerateRedshipOtr time."""
    directions = set()
    for raw in SHIPPED_MANIFEST.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if line:
            directions.add(line.split(None, 1)[0])
    assert directions == {"mm->oot", "oot->mm"}
