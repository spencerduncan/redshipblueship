"""lane-manifest.py: lanes/<KEY>.json written at every stage change, and the
read table that flags heads which are not on origin."""

from __future__ import annotations

import contextlib
import io
import json
import os
import tempfile
import unittest
from pathlib import Path

from wavetest import GitFixture, commit, load_script, run_git, wavelib as w

cli = load_script("lane-manifest.py")


def call(*args: str) -> tuple[int, str]:
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        rc = cli.main(["lane-manifest.py", *args])
    return rc, out.getvalue()


class ManifestStoreTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="wave-lanes-")
        self.state = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_round_trip_has_every_field(self):
        sha = "a" * 40
        written = w.write_manifest(
            self.state,
            "K9",
            {"stage": "review", "branch": "claude/k9", "pr": 901, "headSha": sha, "pushed": True,
             "findingsPath": "/tmp/k9-findings.md"},
        )
        on_disk = json.loads(w.manifest_file(self.state, "K9").read_text(encoding="utf-8"))
        self.assertEqual(written, on_disk)
        for key in ("stage", "branch", "pr", "headSha", "pushed", "findingsPath", "updatedAt"):
            self.assertIn(key, on_disk)
        self.assertEqual(on_disk["key"], "K9")
        self.assertIsNotNone(w.parse_iso(on_disk["updatedAt"]))

    def test_a_stage_change_keeps_fields_it_does_not_name(self):
        w.write_manifest(self.state, "K9", {"stage": "implement", "branch": "claude/k9", "headSha": "b" * 40})
        w.write_manifest(self.state, "K9", {"stage": "review", "pr": 12})
        m = w.read_manifest(self.state, "K9")
        self.assertEqual((m["stage"], m["branch"], m["pr"], m["headSha"]), ("review", "claude/k9", 12, "b" * 40))

    def test_an_explicit_none_replaces_the_old_value(self):
        w.write_manifest(self.state, "K9", {"stage": "review", "pushed": True})
        w.write_manifest(self.state, "K9", {"stage": "fix", "pushed": None})
        self.assertIsNone(w.read_manifest(self.state, "K9")["pushed"])

    def test_invalid_fields_are_rejected_and_nothing_is_written(self):
        for fields in ({"stage": "done-ish"}, {"stage": "fix", "pr": -3}, {"stage": "fix", "headSha": "abc"},
                       {"stage": "fix", "pushed": "yes"}):
            with self.assertRaises(ValueError, msg=fields):
                w.write_manifest(self.state, "BAD", fields)
        self.assertFalse(w.manifest_file(self.state, "BAD").exists())

    def test_keys_are_file_name_safe(self):
        with self.assertRaises(ValueError):
            w.manifest_file(self.state, "../escape")

    def test_resume_stage_mapping(self):
        findings = self.state / "findings.md"
        findings.write_text("1. a finding\n", encoding="utf-8")
        cases = [
            ({"stage": "queued"}, "implement (fresh)"),
            ({"stage": "implement", "branch": "claude/x"}, "implement (inherit worktree)"),
            ({"stage": "review", "pr": 5}, "review"),
            ({"stage": "review"}, "implement (inherit: no PR recorded)"),
            ({"stage": "fix", "findingsPath": str(findings)}, "fix (findings saved)"),
            ({"stage": "fix", "findingsPath": str(self.state / "gone.md")}, "review (findings file missing: re-review)"),
            ({"stage": "gate"}, "gate (preflight swept the lock)"),
            ({"stage": "merged"}, "done"),
            ({"stage": "blocked"}, "blocked: orchestrator decides"),
        ]
        for manifest, expected in cases:
            self.assertEqual(w.resume_stage(manifest), expected, manifest)

    def test_an_unreadable_manifest_is_listed_not_hidden(self):
        lanes = w.lanes_path(self.state)
        lanes.mkdir(parents=True)
        (lanes / "K9.json").write_text("{not json", encoding="utf-8")
        rows = w.manifest_rows(w.list_manifests(self.state), origin=None)
        self.assertEqual(rows[0]["key"], "K9")
        self.assertTrue(rows[0]["flags"][0].startswith("UNREADABLE"))


class ManifestCliTest(unittest.TestCase):
    """write computes headSha/pushed from the lane's checkout; read flags unpushed heads."""

    def setUp(self):
        self.git = GitFixture()
        self.state = self.git.root / "state"
        self.lane = self.git.main
        run_git(self.lane, "checkout", "-q", "-b", "claude/k9")
        self.cwd = os.getcwd()
        os.chdir(self.lane)
        self.sd = f"--state-dir={self.state}"

    def tearDown(self):
        os.chdir(self.cwd)
        self.git.cleanup()

    def test_write_records_head_branch_worktree_and_pushed(self):
        run_git(self.lane, "push", "-q", "-u", "origin", "claude/k9")
        rc, out = call("write", "K9", "stage=implement", self.sd)
        self.assertEqual(rc, 0, out)
        m = w.read_manifest(self.state, "K9")
        self.assertEqual(m["headSha"], run_git(self.lane, "rev-parse", "HEAD"))
        self.assertEqual(m["branch"], "claude/k9")
        self.assertTrue(m["pushed"])
        self.assertEqual(Path(m["worktree"]).resolve(), self.lane.resolve())

    def test_write_with_an_unpushed_commit_exits_3_and_still_records_it(self):
        commit(self.lane, {"src/k9.c": "int k9;\n"}, "unpushed work")
        rc, out = call("write", "K9", "stage=review", "branch=claude/k9", "pr=#901", self.sd)
        self.assertEqual(rc, 3, out)
        self.assertIn("UNPUSHED", out)
        m = w.read_manifest(self.state, "K9")
        self.assertIs(m["pushed"], False)
        self.assertEqual(m["pr"], 901)
        rc, _ = call("write", "K9", "stage=review", "--allow-unpushed", self.sd)
        self.assertEqual(rc, 0)

    def test_pushing_clears_the_flag(self):
        commit(self.lane, {"src/k9.c": "int k9;\n"}, "work")
        self.assertEqual(call("write", "K9", "stage=review", "branch=claude/k9", self.sd)[0], 3)
        run_git(self.lane, "push", "-q", "-u", "origin", "claude/k9")
        self.assertEqual(call("write", "K9", "stage=fix", self.sd)[0], 0)
        self.assertTrue(w.read_manifest(self.state, "K9")["pushed"])

    def test_read_flags_only_the_unpushed_lane(self):
        run_git(self.lane, "push", "-q", "-u", "origin", "claude/k9")
        self.assertEqual(call("write", "K9", "stage=review", "pr=5", self.sd)[0], 0)
        run_git(self.lane, "checkout", "-q", "-b", "claude/k10")
        commit(self.lane, {"src/k10.c": "int k10;\n"}, "k10 unpushed")
        self.assertEqual(call("write", "K10", "stage=implement", "--allow-unpushed", self.sd)[0], 0)
        rc, out = call("read", self.sd, f"--main={self.lane}")
        self.assertEqual(rc, 3, out)
        self.assertIn("K10: UNPUSHED", out)
        self.assertNotIn("K9: UNPUSHED", out)
        self.assertIn("KEY", out.splitlines()[0])

    def test_a_merged_lane_is_not_checked_against_origin(self):
        commit(self.lane, {"src/k9.c": "int k9;\n"}, "squash-merged then deleted")
        call("write", "K9", "stage=merged", self.sd)
        rc, out = call("read", self.sd, f"--main={self.lane}")
        self.assertEqual(rc, 0, out)
        self.assertIn("done", out)

    def test_write_from_another_branch_keeps_the_recorded_head(self):
        run_git(self.lane, "push", "-q", "-u", "origin", "claude/k9")
        call("write", "K9", "stage=review", self.sd)
        recorded = w.read_manifest(self.state, "K9")["headSha"]
        run_git(self.lane, "checkout", "-q", "main")
        commit(self.lane, {"other.txt": "x\n"}, "unrelated main commit")
        rc, out = call("write", "K9", "stage=merged", self.sd)
        self.assertEqual(rc, 0, out)
        self.assertEqual(w.read_manifest(self.state, "K9")["headSha"], recorded)
        self.assertIn("headSha kept", out)

    def test_findings_path_is_stored_absolute(self):
        Path("findings.md").write_text("x\n", encoding="utf-8")
        call("write", "K9", "stage=fix", "findingsPath=findings.md", "--allow-unpushed", self.sd)
        stored = Path(w.read_manifest(self.state, "K9")["findingsPath"])
        self.assertTrue(stored.is_absolute())
        self.assertEqual(stored, (self.lane / "findings.md").resolve())

    def test_usage_errors(self):
        self.assertEqual(call()[0], 2)
        self.assertEqual(call("write", self.sd)[0], 2)
        self.assertEqual(call("write", "K9", "colour=red", self.sd)[0], 2)
        self.assertEqual(call("write", "K9", "stage=nope", self.sd)[0], 2)
        self.assertEqual(call("write", "K9", "stage=fix", "pr=abc", self.sd)[0], 2)


if __name__ == "__main__":
    unittest.main()
