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
content-walking guards keep their ROM-staged lock, except the escaping-reference
guard and the admission report (#577 M6), which are also locked below with
synthetic display lists.
"""

import struct
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


def _sources_line(archive, game, name):
    with zipfile.ZipFile(archive) as z:
        info = z.getinfo(name)
    return "%s\t%s\t%08x\t%d\n" % (game, name, info.CRC, info.file_size)


def _stamp_of(path):
    with zipfile.ZipFile(path) as z:
        return z.comment.decode("ascii")


def test_both_halves_carry_the_manifest_stamp(tmp_path, archives):
    # #806: the game regenerates a half whose archive comment does not name the
    # manifest compiled into it AND the source entries on disk, so the comment
    # must be exactly this -- src/common/curated_archives.cpp spells the same
    # format, and the curated-archive-inapp CTest row compares the two
    # generators' output.
    oot, mm = archives
    manifest_text = "mm->oot objects/object_mm_only/\r\noot->mm objects/object_oot_only/\r\n"
    proc, out_oot, out_mm = _run(tmp_path, archives, manifest_text)
    assert proc.returncode == 0, proc.stdout + proc.stderr
    # Selection order: manifest order, sorted within a prefix.
    sources = "".join([_sources_line(mm, "mm", n) for n in sorted(MM_ONLY)] +
                      [_sources_line(oot, "oot", n) for n in OOT_ONLY])
    want = "redship-curated v2 manifest-crc64=%016x sources-crc64=%016x" % (
        _crc64(manifest_text.replace("\r", "")), _crc64(sources))
    for out in (out_oot, out_mm):
        assert _stamp_of(out) == want
    # The stamp lives in the comment, not in an entry: the path sets are unchanged.
    assert _names(out_oot) == sorted(MM_ONLY)


def test_a_re_extracted_source_changes_the_stamp_only_when_a_curated_entry_changed(tmp_path, archives):
    # #806 review: SoH deletes an oot.o2r made by an incompatible version and
    # re-extracts it; a player may re-extract from another ROM revision. The
    # halves carved out of the old archive must then stamp differently.
    oot, mm = archives
    manifest_text = "mm->oot objects/object_mm_only/\noot->mm objects/object_oot_only/\n"
    proc, out_oot, _ = _run(tmp_path, archives, manifest_text)
    assert proc.returncode == 0, proc.stdout + proc.stderr
    first = _stamp_of(out_oot)

    # An entry the manifest does not select changes: same stamp.
    with zipfile.ZipFile(mm, "w") as z:
        for name in MM_ONLY:
            z.writestr(name, _resource("mm:" + name))
        z.writestr(SHARED, _resource("mm:re-extracted:" + SHARED))
    proc, out_oot, _ = _run(tmp_path, archives, manifest_text)
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert _stamp_of(out_oot) == first

    # A curated entry changes: a different stamp.
    with zipfile.ZipFile(mm, "w") as z:
        z.writestr(MM_ONLY[0], _resource("mm:re-extracted:" + MM_ONLY[0]))
        z.writestr(MM_ONLY[1], _resource("mm:" + MM_ONLY[1]))
        z.writestr(SHARED, _resource("mm:" + SHARED))
    proc, out_oot, _ = _run(tmp_path, archives, manifest_text)
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert _stamp_of(out_oot) != first
    assert _stamp_of(out_oot).split(" manifest-crc64=")[1].split(" ")[0] == \
        first.split(" manifest-crc64=")[1].split(" ")[0]


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


# ---------------------------------------------------------------------------
# #577 M6: the escaping-reference guard (constraint 7) and the admission report.
# ---------------------------------------------------------------------------

def _crc64(path):
    """libultraship's CRC64(const char*) (src/ship/utils/StrHash64.cpp),
    bit by bit and independent of the generator's table: MSB first, ECMA-182
    polynomial, all-ones initial value, no final XOR."""
    crc = 0xFFFFFFFFFFFFFFFF
    for byte in path.encode("utf-8"):
        crc ^= byte << 56
        for _ in range(8):
            crc = ((crc << 1) ^ 0x42F0E1EBA9EA3693) if crc & (1 << 63) else (crc << 1)
            crc &= 0xFFFFFFFFFFFFFFFF
    return crc


def test_crc64_is_libultraships():
    sys.path.insert(0, str(REPO_ROOT / "scripts"))
    import make_redship_otr  # noqa: E402

    # CRC-64/WE("123456789") is 0x62EC59E3F1A4F00A; libultraship's variant
    # skips the final XOR.
    assert _crc64("123456789") == 0x62EC59E3F1A4F00A ^ 0xFFFFFFFFFFFFFFFF
    for path in ("", "123456789", "objects/gameplay_keep/gEffUnknown10Tex"):
        assert make_redship_otr.crc64(path) == _crc64(path)


def _display_list(*referenced_paths, opcode=0x20):
    """A little-endian F3DEX2 ODLT resource naming each path with the expanded
    hash command `opcode` (G_SETTIMG_OTR_HASH by default), then G_ENDDL."""
    body = bytes([4]) + bytes(7)  # ucode byte, padded to 8
    for path in referenced_paths:
        h = _crc64(path)
        body += struct.pack("<II", opcode << 24, 0) + struct.pack("<II", h >> 32, h & 0xFFFFFFFF)
    body += struct.pack("<II", 0xDF << 24, 0)
    return bytes([0, 0, 0, 0]) + b"TLDO" + bytes(56) + body


SHARED_TEX = "objects/gameplay_keep/gSharedTex"
ESCAPING_DL = "objects/object_gi_oot_only/gGiOotOnlyDL"


@pytest.fixture
def gi_archives(tmp_path):
    """OoT carries two get-item directories, one of them also in MM (so only
    one is host-exclusive for oot->mm), and a display list that names a
    gameplay_keep texture outside its own directory; MM carries one
    exclusive get-item directory."""
    oot = tmp_path / "oot.o2r"
    mm = tmp_path / "mm.o2r"
    with zipfile.ZipFile(oot, "w") as z:
        z.writestr(ESCAPING_DL, _display_list("objects/object_gi_oot_only/gGiOotOnlyTex", SHARED_TEX))
        z.writestr("objects/object_gi_oot_only/gGiOotOnlyTex", _resource("oot:tex"))
        z.writestr(SHARED_TEX, _resource("oot:keep"))
        z.writestr("objects/object_gi_both/gGiBothDL", _resource("oot:both"))
    with zipfile.ZipFile(mm, "w") as z:
        z.writestr("objects/object_gi_mm_only/gGiMmOnlyDL", _display_list("objects/object_gi_mm_only/gGiMmOnlyTex"))
        z.writestr("objects/object_gi_mm_only/gGiMmOnlyTex", _resource("mm:tex"))
        z.writestr("objects/object_gi_both/gGiBothDL", _resource("mm:both"))
    return oot, mm


def test_reference_escaping_its_half_is_refused(tmp_path, gi_archives):
    proc, out_oot, out_mm = _run(
        tmp_path, gi_archives, "mm->oot objects/object_gi_mm_only/\noot->mm objects/object_gi_oot_only/\n")
    assert proc.returncode != 0, "accepted a display list whose texture is outside its curated half"
    assert "ESCAPING REFERENCE" in proc.stderr
    assert ESCAPING_DL in proc.stderr and SHARED_TEX in proc.stderr
    assert not out_oot.exists()
    assert not out_mm.exists()


def test_movemem_light_reference_escaping_its_half_is_refused(tmp_path, gi_archives):
    # G_MOVEMEM_OTR (0x42): libultraship's gfx_movemem_handler_otr loads the
    # Lights resource its payload hash names, so it is a path reference too.
    oot, mm = gi_archives
    lights_dl = "objects/object_gi_mm_only/gGiMmOnlyLightsDL"
    shared_lights = "objects/gameplay_keep/gSharedLights"
    with zipfile.ZipFile(mm, "a") as z:
        z.writestr(lights_dl, _display_list(shared_lights, opcode=0x42))
        z.writestr(shared_lights, _resource("mm:lights"))
    proc, out_oot, out_mm = _run(
        tmp_path, gi_archives,
        "mm->oot objects/object_gi_mm_only/\noot->mm objects/object_gi_oot_only/\noot->mm %s\n" % SHARED_TEX)
    assert proc.returncode != 0, "accepted a display list whose G_MOVEMEM_OTR light is outside its curated half"
    assert "G_MOVEMEM_OTR" in proc.stderr
    assert lights_dl in proc.stderr and shared_lights in proc.stderr
    assert not out_oot.exists()
    assert not out_mm.exists()


def test_reference_to_a_host_owned_path_is_refused(tmp_path, gi_archives):
    # MM carrying the texture's path itself: the host would draw ITS copy.
    oot, mm = gi_archives
    with zipfile.ZipFile(mm, "a") as z:
        z.writestr(SHARED_TEX, _resource("mm:keep"))
    proc, _out_oot, _out_mm = _run(
        tmp_path, gi_archives, "mm->oot objects/object_gi_mm_only/\noot->mm objects/object_gi_oot_only/\n")
    assert proc.returncode != 0
    assert "host's own base archives also carry" in proc.stderr


def test_curating_the_referenced_path_admits_the_model_and_reports_counts(tmp_path, gi_archives):
    report = tmp_path / "report.txt"
    proc, out_oot, out_mm = _run(
        tmp_path, gi_archives,
        "mm->oot objects/object_gi_mm_only/\noot->mm objects/object_gi_oot_only/\noot->mm %s\n" % SHARED_TEX,
        ["--report", str(report)])
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert SHARED_TEX in _names(out_mm)
    # object_gi_both is in both archives, so it is host-exclusive in neither
    # direction; each direction's one exclusive directory is admitted.
    assert report.read_text(encoding="utf-8").splitlines() == [
        "direction mm->oot entries 1 resources 2 gi_exclusive 1 gi_admitted 1",
        "direction oot->mm entries 2 resources 3 gi_exclusive 1 gi_admitted 1",
    ]


def test_report_names_an_uncovered_exclusive_directory(tmp_path, gi_archives):
    oot, mm = gi_archives
    with zipfile.ZipFile(mm, "a") as z:
        z.writestr("objects/object_gi_mm_other/gGiMmOtherDL", _resource("mm:other"))
    report = tmp_path / "report.txt"
    proc, _out_oot, _out_mm = _run(
        tmp_path, gi_archives,
        "mm->oot objects/object_gi_mm_only/\noot->mm objects/object_gi_oot_only/\noot->mm %s\n" % SHARED_TEX,
        ["--report", str(report)])
    assert proc.returncode == 0, proc.stdout + proc.stderr
    lines = report.read_text(encoding="utf-8").splitlines()
    assert "direction mm->oot entries 1 resources 2 gi_exclusive 2 gi_admitted 1" in lines
    assert "missing mm->oot objects/object_gi_mm_other/" in lines


def test_refusal_leaves_no_report(tmp_path, gi_archives):
    report = tmp_path / "report.txt"
    report.write_text("stale\n", encoding="utf-8")
    proc, _out_oot, _out_mm = _run(
        tmp_path, gi_archives, "mm->oot objects/object_gi_mm_only/\noot->mm objects/object_gi_oot_only/\n",
        ["--report", str(report)])
    assert proc.returncode != 0
    assert not report.exists()


def test_shipped_manifest_fills_both_halves():
    """The real manifest parses and names both directions -- a manifest that
    fills only one half would build nothing at GenerateRedshipOtr time."""
    directions = set()
    for raw in SHIPPED_MANIFEST.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if line:
            directions.add(line.split(None, 1)[0])
    assert directions == {"mm->oot", "oot->mm"}
