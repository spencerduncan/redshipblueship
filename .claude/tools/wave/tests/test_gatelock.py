"""The gate lock protocol: atomic acquire, owner file, heartbeat, unlock rules
and the stale-lock decision preflight sweeps by. Runs in a temp state dir."""

from __future__ import annotations

import contextlib
import io
import os
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from datetime import timedelta
from pathlib import Path
from unittest import mock

from wavetest import dead_pid, load_script, wavelib as w

FIVE_MIN = 300.0


def always(alive: bool):
    return lambda pid, token=None: alive


class StaleDecisionTest(unittest.TestCase):
    """decide() is the one rule: sweep only when the heartbeat is old AND the pid is dead."""

    def setUp(self):
        self.now = w.utcnow()

    def owner(self, heartbeat_age_s: float | None, pid: int = 4242, **extra) -> dict:
        owner = {"tag": "K9", "pr": "901", "run": "wf_test", "pid": str(pid), **extra}
        if heartbeat_age_s is not None:
            owner["heartbeat"] = w.iso(self.now - timedelta(seconds=heartbeat_age_s))
        return owner

    def decide(self, owner, alive, mtime=None):
        return w.decide(owner, mtime, self.now.timestamp(), FIVE_MIN, alive=always(alive))

    def test_fresh_heartbeat_dead_pid_is_live(self):
        self.assertEqual(self.decide(self.owner(60), alive=False).verdict, w.LIVE)

    def test_old_heartbeat_dead_pid_is_stale(self):
        d = self.decide(self.owner(6 * 60), alive=False)
        self.assertEqual(d.verdict, w.STALE)
        self.assertIn("pid 4242 dead", d.reason)

    def test_old_heartbeat_live_pid_is_kept(self):
        d = self.decide(self.owner(60 * 60), alive=True)
        self.assertEqual(d.verdict, w.KEEP)
        self.assertIn("alive", d.reason)

    def test_threshold_is_inclusive_of_five_minutes(self):
        self.assertEqual(self.decide(self.owner(FIVE_MIN - 1), alive=False).verdict, w.LIVE)
        self.assertEqual(self.decide(self.owner(FIVE_MIN + 1), alive=False).verdict, w.STALE)

    def test_missing_heartbeat_falls_back_to_acquired(self):
        owner = self.owner(None, acquired=w.iso(self.now - timedelta(minutes=10)))
        self.assertEqual(self.decide(owner, alive=False).verdict, w.STALE)

    def test_ownerless_directory_ages_by_mtime(self):
        old = self.now.timestamp() - 600
        new = self.now.timestamp() - 10
        self.assertEqual(self.decide(None, alive=False, mtime=old).verdict, w.STALE)
        self.assertEqual(self.decide(None, alive=False, mtime=new).verdict, w.LIVE)

    def test_wave8_owner_format_is_understood(self):
        legacy = w.parse_owner(f"owner=XTOAST\npr=887\ntime={w.iso(self.now - timedelta(hours=2))}\n")
        self.assertEqual(legacy["tag"], "XTOAST")
        d = self.decide(legacy, alive=False)
        self.assertEqual(d.verdict, w.STALE)
        self.assertIn("no pid recorded", d.reason)

    def test_future_heartbeat_counts_as_fresh(self):
        self.assertEqual(self.decide(self.owner(-120), alive=False).verdict, w.LIVE)

    def test_no_lock_is_free(self):
        self.assertEqual(w.decide(None, None, 0, FIVE_MIN, exists=False).verdict, w.FREE)


class PidLivenessTest(unittest.TestCase):
    def test_own_pid_is_alive(self):
        self.assertTrue(w.pid_alive(os.getpid()))

    def test_exited_pid_is_dead(self):
        self.assertFalse(w.pid_alive(dead_pid()))

    def test_reused_pid_is_dead_when_the_start_token_differs(self):
        token = w.pid_start_token(os.getpid())
        if token is None:
            self.skipTest("no process start token on this platform")
        self.assertTrue(w.pid_alive(os.getpid(), token))
        self.assertFalse(w.pid_alive(os.getpid(), token + "0"))

    def test_nonsense_pids_are_dead(self):
        self.assertFalse(w.pid_alive(None))
        self.assertFalse(w.pid_alive(0))
        self.assertFalse(w.pid_alive(-5))

    def test_posix_probe_falls_back_to_ps_without_proc(self):
        def fake_ps(stdout: str):
            return subprocess.CompletedProcess(["ps"], 0, stdout=stdout, stderr="")

        with (
            mock.patch.object(w.os, "kill", return_value=None),
            mock.patch.object(w.Path, "read_text", side_effect=FileNotFoundError),
        ):
            with mock.patch.object(w.subprocess, "run", return_value=fake_ps("Ss   Sat Oct 31 00:12:01 2026\n")) as run:
                self.assertEqual(w._posix_probe(4242), (True, "posix:Sat_Oct_31_00:12:01_2026"))
                run.assert_called_once_with(
                    ["ps", "-o", "stat=", "-o", "lstart=", "-p", "4242"],
                    capture_output=True,
                    text=True,
                )
            with mock.patch.object(w.subprocess, "run", return_value=fake_ps("Z+   Sat Oct 31 00:12:01 2026\n")):
                self.assertEqual(w._posix_probe(4242), (False, None))
            with mock.patch.object(w.subprocess, "run", return_value=fake_ps("")):
                self.assertEqual(w._posix_probe(4242), (False, None))
            with mock.patch.object(w.subprocess, "run", side_effect=FileNotFoundError):
                self.assertEqual(w._posix_probe(4242), (True, None))

        with (
            mock.patch.object(w.os, "kill", side_effect=PermissionError),
            mock.patch.object(w.Path, "read_text", side_effect=PermissionError),
            mock.patch.object(w.subprocess, "run", return_value=fake_ps("")),
        ):
            self.assertEqual(w._posix_probe(4242), (True, None))


class LockProtocolTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="wave-lock-")
        self.state = Path(self.tmp.name) / "state"

    def tearDown(self):
        self.tmp.cleanup()

    def owner_file(self) -> dict:
        return w.parse_owner((w.lock_path(self.state) / w.OWNER_FILE).read_text(encoding="utf-8"))

    def test_owner_file_records_every_field(self):
        w.acquire(self.state, "K9", pr="901", run="wf_abc", pid=os.getpid())
        owner = self.owner_file()
        for key in ("tag", "pr", "run", "pid", "pidStarted", "pidRole", "host", "acquired", "heartbeat"):
            self.assertIn(key, owner)
        self.assertEqual((owner["tag"], owner["pr"], owner["run"]), ("K9", "901", "wf_abc"))
        self.assertEqual(owner["pid"], str(os.getpid()))
        self.assertIsNotNone(w.parse_iso(owner["heartbeat"]))

    def test_second_acquire_is_refused_with_the_owner_named(self):
        w.acquire(self.state, "K9", pid=os.getpid())
        with self.assertRaises(w.LockError) as cm:
            w.acquire(self.state, "K10", pid=os.getpid())
        self.assertIn("tag=K9", str(cm.exception))

    def test_only_one_of_many_concurrent_acquirers_wins(self):
        winners, losers = [], []

        def contend(i):
            try:
                w.acquire(self.state, f"L{i}", pid=os.getpid())
                winners.append(i)
            except w.LockError:
                losers.append(i)

        threads = [threading.Thread(target=contend, args=(i,)) for i in range(12)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        self.assertEqual(len(winners), 1)
        self.assertEqual(len(losers), 11)

    def test_unlock_refuses_another_tag(self):
        w.acquire(self.state, "K9", pid=dead_pid())
        with self.assertRaises(w.LockError) as cm:
            w.release(self.state, "K10")
        self.assertIn("held by tag=K9", str(cm.exception))
        self.assertTrue(w.lock_path(self.state).exists())

    def test_unlock_refuses_a_live_pid_unless_forced(self):
        w.acquire(self.state, "K9", pid=os.getpid())
        with self.assertRaises(w.LockError) as cm:
            w.release(self.state, "K9")
        self.assertIn("still alive", str(cm.exception))
        self.assertTrue(w.lock_path(self.state).exists())
        w.release(self.state, "K9", force=True)
        self.assertFalse(w.lock_path(self.state).exists())

    def test_unlock_by_tag_or_owner_alias_with_dead_pid(self):
        gatelock = load_script("gatelock.py")
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))
        w.acquire(self.state, "K9", pid=dead_pid())
        self.assertEqual(gatelock.main(["gatelock.py", "release", "owner=K9", f"--state-dir={self.state}"]), 0)
        self.assertFalse(w.lock_path(self.state).exists())
        w.acquire(self.state, "K9", pid=dead_pid())
        self.assertEqual(gatelock.main(["gatelock.py", "release", "tag=K9", f"--state-dir={self.state}"]), 0)
        self.assertFalse(w.lock_path(self.state).exists())

    def test_self_pid_does_not_block_its_own_release(self):
        w.acquire(self.state, "preflight", pid=os.getpid())
        w.release(self.state, "preflight", self_pids=(os.getpid(),))
        self.assertFalse(w.lock_path(self.state).exists())

    def test_release_of_a_free_lock_is_a_no_op(self):
        self.assertIn("already free", w.release(self.state, "K9"))

    def test_history_records_acquire_and_release(self):
        w.acquire(self.state, "K9", pid=dead_pid())
        w.release(self.state, "K9")
        history = (self.state / w.HISTORY_FILE).read_text(encoding="utf-8")
        self.assertIn("acquired | tag=K9", history)
        self.assertIn("released by=gate-unlock reason=unlock | tag=K9", history)

    def test_heartbeat_touches_only_the_holding_tag(self):
        w.acquire(self.state, "K9", pid=dead_pid())
        before = self.owner_file()
        old = w.iso(w.utcnow() - timedelta(minutes=30))
        w.atomic_write(w.lock_path(self.state) / w.OWNER_FILE, w.format_owner(dict(before, heartbeat=old)))
        self.assertFalse(w.touch_heartbeat(self.state, "K10"))
        self.assertEqual(self.owner_file()["heartbeat"], old)
        self.assertTrue(w.touch_heartbeat(self.state, "K9", pid=os.getpid(), pid_role="verify"))
        after = self.owner_file()
        self.assertNotEqual(after["heartbeat"], old)
        self.assertEqual(after["pid"], str(os.getpid()))
        self.assertEqual(after["pidRole"], "verify")
        self.assertEqual(after["acquired"], before["acquired"])

    def test_heartbeat_never_creates_a_lock(self):
        self.assertFalse(w.touch_heartbeat(self.state, "K9"))
        self.assertFalse(w.lock_path(self.state).exists())

    def test_acquire_sweeps_a_stale_lock(self):
        w.acquire(self.state, "DEAD", pid=dead_pid())
        owner = self.owner_file()
        old = w.iso(w.utcnow() - timedelta(minutes=10))
        w.atomic_write(w.lock_path(self.state) / w.OWNER_FILE, w.format_owner(dict(owner, heartbeat=old)))
        swept = []
        w.acquire(self.state, "K9", pid=os.getpid(), log=swept.append)
        self.assertEqual(self.owner_file()["tag"], "K9")
        self.assertTrue(any("swept stale lock" in line and "tag=DEAD" in line for line in swept))

    def test_acquire_without_sweep_does_not_spin_on_a_stale_lock(self):
        w.acquire(self.state, "DEAD", pid=dead_pid())
        owner = self.owner_file()
        old = w.iso(w.utcnow() - timedelta(minutes=10))
        w.atomic_write(w.lock_path(self.state) / w.OWNER_FILE, w.format_owner(dict(owner, heartbeat=old)))
        with self.assertRaises(w.LockError):
            w.acquire(self.state, "K9", pid=os.getpid(), sweep=False)
        self.assertEqual(self.owner_file()["tag"], "DEAD")

    def test_acquire_waits_for_a_release(self):
        w.acquire(self.state, "K9", pid=dead_pid())
        timer = threading.Timer(0.5, lambda: w.release(self.state, "K9"))
        timer.start()
        try:
            w.acquire(self.state, "K10", pid=os.getpid(), wait_s=10, poll_s=0.1)
        finally:
            timer.join()
        self.assertEqual(self.owner_file()["tag"], "K10")

    def test_heartbeat_loop_stops_when_the_watched_process_exits(self):
        w.acquire(self.state, "K9", pid=dead_pid())
        child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(1.5)"])
        started = time.time()
        rc = w.heartbeat_loop(self.state, "K9", child.pid, interval_s=0.2)
        child.wait()
        self.assertEqual(rc, 0)
        self.assertLess(time.time() - started, 10)
        beat = w.parse_iso(self.owner_file()["heartbeat"])
        self.assertLess((w.utcnow() - beat).total_seconds(), 5)

    def test_heartbeat_loop_stops_when_the_lock_changes_hands(self):
        w.acquire(self.state, "K9", pid=dead_pid())
        other = dead_pid()
        timer = threading.Timer(0.3, lambda: (w.release(self.state, "K9"), w.acquire(self.state, "K10", pid=other)))
        timer.start()
        rc = w.heartbeat_loop(self.state, "K9", os.getpid(), interval_s=0.1)
        timer.join()
        self.assertEqual(rc, 1)
        self.assertEqual(self.owner_file()["tag"], "K10")

    def test_cli_exit_codes(self):
        gatelock = load_script("gatelock.py")
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))
        sd = f"--state-dir={self.state}"
        self.assertEqual(gatelock.main(["gatelock.py", "acquire", "tag=K9", "pr=1", "run=r", f"pid={os.getpid()}", sd]), 0)
        self.assertEqual(gatelock.main(["gatelock.py", "acquire", "tag=K10", sd]), 1)
        self.assertEqual(gatelock.main(["gatelock.py", "release", "tag=K9", sd]), 1)  # pid alive
        self.assertEqual(gatelock.main(["gatelock.py", "release", "tag=K9", "--dry-run", sd]), 0)
        self.assertTrue(w.lock_path(self.state).exists())
        self.assertEqual(gatelock.main(["gatelock.py", "heartbeat", "tag=K9", sd]), 0)
        self.assertEqual(gatelock.main(["gatelock.py", "heartbeat", "tag=K10", sd]), 1)
        self.assertEqual(gatelock.main(["gatelock.py", "release", "tag=K9", "--force", sd]), 0)
        self.assertEqual(gatelock.main(["gatelock.py"]), 2)
        self.assertEqual(gatelock.main(["gatelock.py", "acquire", sd]), 2)


if __name__ == "__main__":
    unittest.main()
