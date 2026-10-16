"""preflight.py: the stale-lock sweep, the main-checkout restore and the lane
table, against a temp state dir and a throwaway origin + clone."""

from __future__ import annotations

import json
import os
import tempfile
import unittest
from datetime import timedelta
from pathlib import Path

from wavetest import GitFixture, commit, dead_pid, load_script, run_git, wavelib as w

preflight = load_script("preflight.py")


def write_owner(state: Path, tag: str, pid: int, heartbeat_age_min: float) -> None:
    lock = w.lock_path(state)
    lock.mkdir(parents=True)
    stamp = w.iso(w.utcnow() - timedelta(minutes=heartbeat_age_min))
    owner = {"tag": tag, "pr": "1", "run": "wf_dead", "pid": str(pid), "acquired": stamp, "heartbeat": stamp}
    (lock / w.OWNER_FILE).write_text(w.format_owner(owner), encoding="utf-8")


class ClassifyStatusTest(unittest.TestCase):
    def test_build_stamps_are_allowed(self):
        report = w.classify_status(" M games/mm/src/boot/build.c\n M games/oot/properties.h\n")
        self.assertTrue(report.clean)
        self.assertEqual(report.stamps, ["games/mm/src/boot/build.c", "games/oot/properties.h"])

    def test_untracked_outside_build_inputs_is_tolerated(self):
        report = w.classify_status("?? .agent/\n?? .agents/\n")
        self.assertTrue(report.clean)
        self.assertEqual(report.untracked_ok, [".agent/", ".agents/"])

    def test_untracked_under_a_build_input_blocks(self):
        report = w.classify_status("?? games/mm/2s2h/Stray.cpp\n")
        self.assertFalse(report.clean)

    def test_any_other_tracked_change_blocks(self):
        for line in (" M src/common/context.c", "M  README.md", "D  games/oot/x.c", "R  a.c -> b.c", " M libultraship"):
            self.assertFalse(w.classify_status(line + "\n").clean, line)

    def test_a_conflicted_stamp_blocks(self):
        self.assertFalse(w.classify_status("UU games/oot/src/boot/build.c\n").clean)


class StaleLockSweepTest(unittest.TestCase):
    """The preflight decision on a lock left by a dead workflow."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="wave-pf-")
        self.state = Path(self.tmp.name) / "state"
        self.not_a_repo = Path(self.tmp.name) / "no-checkout"
        self.not_a_repo.mkdir()
        self.lines = []

    def tearDown(self):
        self.tmp.cleanup()

    def run_preflight(self, **opts):
        base = {"main": str(self.not_a_repo), "state-dir": str(self.state), "no-restore": True, "no-fetch": True}
        base.update(opts)
        return preflight.run(base, log=self.lines.append)

    def test_old_heartbeat_and_dead_pid_is_released(self):
        write_owner(self.state, "XTOAST", dead_pid(), heartbeat_age_min=6)
        self.assertEqual(self.run_preflight(), 0)
        self.assertFalse(w.lock_path(self.state).exists())
        self.assertTrue(any("RELEASED stale lock" in l and "tag=XTOAST" in l for l in self.lines), self.lines)
        history = (self.state / w.HISTORY_FILE).read_text(encoding="utf-8")
        self.assertIn("released by=preflight reason=stale", history)

    def test_fresh_heartbeat_with_dead_pid_is_kept(self):
        write_owner(self.state, "K9", dead_pid(), heartbeat_age_min=1)
        self.assertEqual(self.run_preflight(), 0)
        self.assertTrue(w.lock_path(self.state).exists())
        self.assertTrue(any("lock: HELD [live" in l for l in self.lines), self.lines)

    def test_old_heartbeat_with_live_pid_is_kept(self):
        write_owner(self.state, "K9", os.getpid(), heartbeat_age_min=90)
        self.assertEqual(self.run_preflight(), 0)
        self.assertTrue(w.lock_path(self.state).exists())
        self.assertTrue(any("lock: HELD [keep" in l for l in self.lines), self.lines)

    def test_stale_min_option_moves_the_threshold(self):
        write_owner(self.state, "K9", dead_pid(), heartbeat_age_min=3)
        self.assertEqual(self.run_preflight(**{"stale-min": "2"}), 0)
        self.assertFalse(w.lock_path(self.state).exists())

    def test_dry_run_reports_but_keeps_a_stale_lock(self):
        write_owner(self.state, "K9", dead_pid(), heartbeat_age_min=30)
        self.assertEqual(self.run_preflight(**{"dry-run": True}), 0)
        self.assertTrue(w.lock_path(self.state).exists())
        self.assertTrue(any("would RELEASE stale lock" in l for l in self.lines), self.lines)


class RestoreCheckoutTest(unittest.TestCase):
    def setUp(self):
        self.git = GitFixture()
        self.main = self.git.main
        self.state = self.git.root / "state"
        self.lines = []

    def tearDown(self):
        self.git.cleanup()

    def preflight(self, **opts):
        base = {"main": str(self.main), "state-dir": str(self.state)}
        base.update(opts)
        return preflight.run(base, log=self.lines.append)

    def head(self) -> tuple[str, str]:
        return run_git(self.main, "rev-parse", "--abbrev-ref", "HEAD"), run_git(self.main, "rev-parse", "HEAD")

    def detach_at_a_verify_merge(self) -> str:
        """What a killed verify leaves: detached at a local merge commit."""
        run_git(self.main, "checkout", "-q", "-b", "lane")
        commit(self.main, {"src/lane.c": "int lane;\n"}, "lane work")
        run_git(self.main, "checkout", "-q", "--detach", "main")
        run_git(self.main, "merge", "-q", "--no-ff", "--no-edit", "lane")
        return run_git(self.main, "rev-parse", "HEAD")

    def test_detached_verify_state_is_restored_and_fast_forwarded(self):
        self.detach_at_a_verify_merge()
        (self.main / "games/oot/src/boot/build.c").write_text("/* rebuilt */\n", encoding="utf-8")
        tip = self.git.advance_origin({"docs.md": "moved\n"}, "origin moved")
        self.assertEqual(self.preflight(), 0, self.lines)
        self.assertEqual(self.head(), ("main", tip))
        self.assertTrue(w.checkout_status(self.main).clean)
        self.assertTrue(any(l.startswith("checkout: OK main at " + tip) for l in self.lines), self.lines)
        self.assertFalse(w.lock_path(self.state).exists(), "preflight must release its own lock")

    def test_a_stale_lock_is_swept_then_the_checkout_restored(self):
        self.detach_at_a_verify_merge()
        write_owner(self.state, "XTOAST", dead_pid(), heartbeat_age_min=20)
        self.assertEqual(self.preflight(), 0, self.lines)
        self.assertEqual(self.head()[0], "main")

    def test_a_live_lock_leaves_the_checkout_alone(self):
        merge = self.detach_at_a_verify_merge()
        write_owner(self.state, "K9", dead_pid(), heartbeat_age_min=0)
        self.assertEqual(self.preflight(), 0, self.lines)
        self.assertEqual(self.head(), ("HEAD", merge))
        # The first guard, not the backstop (acquire refusing tag=preflight).
        self.assertTrue(any("checkout: SKIPPED (a live gate owns the main checkout" in l for l in self.lines), self.lines)
        self.assertFalse(any("lost the race" in l for l in self.lines), self.lines)

    def test_held_by_restores_under_the_callers_lock(self):
        self.detach_at_a_verify_merge()
        w.acquire(self.state, "K9", pid=os.getpid())
        self.assertEqual(self.preflight(**{"held-by": "K9"}), 0, self.lines)
        self.assertEqual(self.head()[0], "main")
        self.assertEqual(w.lock_status(self.state).owner["tag"], "K9", "the caller's lock must survive")

    def test_held_by_a_tag_that_does_not_hold_it_fails(self):
        self.assertEqual(self.preflight(**{"held-by": "K9"}), 1)

    def test_a_tracked_edit_outside_the_stamps_fails_and_changes_nothing(self):
        merge = self.detach_at_a_verify_merge()
        (self.main / "README.md").write_text("hand edit\n", encoding="utf-8")
        self.assertEqual(self.preflight(), 1)
        self.assertEqual(self.head(), ("HEAD", merge))
        self.assertTrue(any("FAIL preflight: cannot restore" in l and "README.md" in l for l in self.lines), self.lines)
        self.assertEqual((self.main / "README.md").read_text(encoding="utf-8"), "hand edit\n")

    def test_untracked_source_file_fails_but_root_droppings_do_not(self):
        (self.main / ".agent").mkdir()
        (self.main / ".agent" / "x").write_text("x", encoding="utf-8")
        self.assertEqual(self.preflight(), 0, self.lines)
        (self.main / "games" / "mm" / "2s2h").mkdir(parents=True)
        (self.main / "games" / "mm" / "2s2h" / "Stray.cpp").write_text("x", encoding="utf-8")
        self.assertEqual(self.preflight(), 1)

    def test_an_interrupted_merge_is_aborted_first(self):
        run_git(self.main, "checkout", "-q", "-b", "lane")
        commit(self.main, {"README.md": "lane\n"}, "lane edits README")
        run_git(self.main, "checkout", "-q", "--detach", "main")
        commit(self.main, {"README.md": "main\n"}, "conflicting edit")
        proc = w.git(self.main, "merge", "--no-edit", "lane")
        self.assertNotEqual(proc.returncode, 0, "the fixture must leave a conflicted merge")
        self.assertEqual(self.preflight(), 0, self.lines)
        self.assertEqual(self.head()[0], "main")
        self.assertTrue(any("aborting an interrupted merge" in l for l in self.lines), self.lines)

    def test_a_diverged_local_main_fails(self):
        commit(self.main, {"local.txt": "unpushed\n"}, "local only")
        self.git.advance_origin({"remote.txt": "pushed\n"}, "remote only")
        self.assertEqual(self.preflight(), 1)
        self.assertTrue(any("cannot fast-forward" in l for l in self.lines), self.lines)

    def test_lane_table_shows_resume_stage_and_flags_unpushed_heads(self):
        lanes = w.lanes_path(self.state)
        lanes.mkdir(parents=True)
        run_git(self.main, "checkout", "-q", "-b", "claude/k9")
        local = commit(self.main, {"src/k9.c": "int k9;\n"}, "k9 local only")
        run_git(self.main, "checkout", "-q", "main")
        for key, stage, sha in (("K9", "fix", local), ("K10", "review", self.git.base)):
            (lanes / f"{key}.json").write_text(
                json.dumps({"key": key, "stage": stage, "branch": f"claude/{key.lower()}", "pr": 7,
                            "headSha": sha, "pushed": True, "findingsPath": None, "updatedAt": w.iso()}),
                encoding="utf-8",
            )
        self.assertEqual(self.preflight(), 0, self.lines)
        table = "\n".join(self.lines)
        self.assertIn("review (findings file missing: re-review)", table)
        self.assertIn("K9: UNPUSHED", table)
        self.assertIn("(manifest says pushed)", table)
        self.assertNotIn("K10: UNPUSHED", table)
        self.assertTrue(any("WARN lanes need attention before resuming: K9" in l for l in self.lines), self.lines)


if __name__ == "__main__":
    unittest.main()
