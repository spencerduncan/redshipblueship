"""verify.py's decisions that need no build: flags, the archive and GL
preconditions, CTest summary parsing, the load wait before the integration
tier, and the wrapper contract (CRLF .cmd files naming scripts that exist)."""

from __future__ import annotations

import ast
import json
import os
import re
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from wavetest import WAVE_DIR, load_script, wavelib as w

verify = load_script("verify.py")


class FlagsTest(unittest.TestCase):
    def test_letters(self):
        self.assertEqual(verify.parse_flags("uia"), {"u", "i", "a"})
        self.assertEqual(verify.parse_flags(""), set())
        self.assertEqual(verify.parse_flags("-"), set())

    def test_unknown_letter_is_a_usage_error(self):
        with self.assertRaises(verify.Usage):
            verify.parse_flags("ux")

    def test_branch_prefixes_are_dropped(self):
        for raw in ("claude/x", "origin/claude/x", "refs/heads/claude/x", "refs/remotes/origin/claude/x"):
            self.assertEqual(verify.normalize_branch(raw), "claude/x")

    def test_cli_usage_errors_exit_2(self):
        self.assertEqual(verify.main(["verify.py"]), 2)
        self.assertEqual(verify.main(["verify.py", "claude/x"]), 2)
        self.assertEqual(verify.main(["verify.py", "claude/x", "K9", "uz"]), 2)
        self.assertEqual(verify.main(["verify.py", "claude/x", "K9", "--bogus"]), 2)


class ArchivePreconditionTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="wave-build-")
        self.build = Path(self.tmp.name)
        for name in w.REQUIRED_ARCHIVES:
            (self.build / name).write_bytes(b"o2r")

    def tearDown(self):
        self.tmp.cleanup()

    def test_all_present_is_clean(self):
        self.assertEqual(w.check_archives(self.build), [])

    def test_missing_mm_archive_is_one_clear_line(self):
        (self.build / "mm.o2r").unlink()
        problems = w.check_archives(self.build)
        self.assertEqual(len(problems), 1)
        self.assertTrue(problems[0].startswith("precondition: build-cmake/mm.o2r is missing"), problems)

    def test_a_renamed_away_archive_is_named(self):
        (self.build / "mm.o2r").rename(self.build / "mm.o2r.manual-bak")
        (problem,) = w.check_archives(self.build)
        self.assertIn("found mm.o2r.manual-bak beside it: rename it back", problem)

    def test_missing_oot_archive_and_empty_file(self):
        (self.build / "oot.o2r").unlink()
        (self.build / "soh.o2r").write_bytes(b"")
        problems = w.check_archives(self.build)
        self.assertTrue(any("oot.o2r is missing" in p for p in problems), problems)
        self.assertTrue(any("soh.o2r is empty" in p for p in problems), problems)

    @unittest.skipUnless(os.name == "nt", "the GL check only applies on Windows")
    def test_gl_backend_check(self):
        cfg = self.build / "shipofharkinian.json"
        self.assertIn("missing", verify.check_gl_backend(self.build))
        cfg.write_text(json.dumps({"Window": {"Backend": {"Id": 0, "Name": "DirectX"}}}), encoding="utf-8")
        self.assertIn("not OpenGL", verify.check_gl_backend(self.build))
        cfg.write_text(json.dumps({"Window": {"Backend": {"Id": 1, "Name": "OpenGL"}}}), encoding="utf-8")
        self.assertIsNone(verify.check_gl_backend(self.build))


PASS_OUTPUT = """\
      Start  1: BootOoT
 1/3 Test  #1: BootOoT ..........................   Passed    1.20 sec
 2/3 Test  #2: BootMM ...........................   Passed    1.10 sec
 3/3 Test  #3: Context ..........................   Passed    0.30 sec

100% tests passed, 0 tests failed out of 3

Label Time Summary:
redship    =   2.60 sec*proc (3 tests)
"""

FAIL_OUTPUT = """\
 1/4 Test  #1: BootOoT ..........................***Failed    1.20 sec
 2/4 Test  #2: BootMM ...........................   Passed    1.10 sec
 3/4 Test  #3: CrossGameModel ...................***Skipped   0.10 sec
 4/4 Test  #4: Roundtrip ........................***Timeout  60.00 sec

50% tests passed, 2 tests failed out of 4

The following tests did not run:
\t  3 - CrossGameModel (Skipped)

The following tests FAILED:
\t  1 - BootOoT (Failed)
\t  4 - Roundtrip (Timeout)
Errors while running CTest
"""

SKIP_ONLY_OUTPUT = """\
100% tests passed, 0 tests failed out of 2

The following tests did not run:
\t  2 - MMModsMount (Skipped)
"""


class CtestParseTest(unittest.TestCase):
    def test_all_green(self):
        r = verify.parse_ctest("redship", 0, PASS_OUTPUT.splitlines())
        self.assertTrue(r.ok())
        self.assertEqual((r.total, r.failed, r.skipped), (3, [], []))
        self.assertTrue(r.line().startswith("redship 3/3"))

    def test_failures_and_skips_are_named(self):
        r = verify.parse_ctest("redship", 8, FAIL_OUTPUT.splitlines())
        self.assertFalse(r.ok())
        self.assertEqual(r.failed, ["BootOoT", "Roundtrip"])
        self.assertEqual(r.skipped, ["CrossGameModel"])
        self.assertIn("FAILED: BootOoT, Roundtrip", r.line())

    def test_a_skip_in_the_staged_tree_fails_unless_allowed(self):
        r = verify.parse_ctest("rando", 0, SKIP_ONLY_OUTPUT.splitlines())
        self.assertFalse(r.ok())
        self.assertTrue(r.ok(allow_skips=True))
        self.assertIn("1 skipped: MMModsMount", r.line())

    def test_an_empty_tier_is_never_green(self):
        r = verify.parse_ctest("ui", 8, ["No tests were found!!!"])
        self.assertFalse(r.ok())
        self.assertTrue(r.no_tests)
        self.assertFalse(verify.parse_ctest("ui", 0, []).ok(), "zero rows parsed must not pass")

    def test_bad_command_is_detected(self):
        r = verify.parse_ctest("redship", 8, ["\t  1 - BootOoT (BAD_COMMAND)", "The following tests FAILED:",
                                              "\t  1 - BootOoT (BAD_COMMAND)"])
        self.assertTrue(r.bad_command)
        self.assertEqual(r.failed, ["BootOoT"])


class FakeMachine:
    """A process table that changes over fake time."""

    def __init__(self, timeline):
        self.t = 0.0
        self.timeline = timeline  # [(until_t, [(pid, name), ...]), ...]

    def clock(self):
        return self.t

    def sleep(self, s):
        self.t += s

    def procs(self):
        for until, procs in self.timeline:
            if self.t < until:
                return procs + [(4, "system"), (9000, "python.exe")]
        return [(4, "system")]


class LoadWaitTest(unittest.TestCase):
    def test_no_builds_starts_at_once(self):
        m = FakeMachine([])
        lines = []
        quiet, waited, seen = verify.wait_for_quiet(lines.append, list_procs=m.procs, clock=m.clock, sleep=m.sleep)
        self.assertTrue(quiet)
        self.assertEqual(waited, 0)
        self.assertIn("starting at once", lines[-1])

    def test_waits_for_ninja_and_cl_and_logs_them(self):
        m = FakeMachine([(120, [(100, "ninja.exe"), (101, "cl.exe"), (102, "cl.exe")]), (400, [(100, "ninja.exe")])])
        lines = []
        quiet, waited, seen = verify.wait_for_quiet(lines.append, list_procs=m.procs, clock=m.clock, sleep=m.sleep)
        self.assertTrue(quiet)
        self.assertGreaterEqual(waited, 400)
        self.assertEqual(seen, {"ninja.exe": {100}, "cl.exe": {101, 102}})
        self.assertTrue(any("waiting for cl.exe x2" in l and "ninja.exe x1" in l for l in lines), lines)
        self.assertTrue(any(l.startswith("load-wait: waiting for ninja.exe x1") for l in lines), lines)
        self.assertIn("clear after", lines[-1])
        self.assertIn("ninja.exe (1 process)", lines[-1])

    def test_gives_up_after_thirty_minutes_and_says_so(self):
        m = FakeMachine([(10**9, [(100, "ninja.exe")])])
        lines = []
        quiet, waited, _ = verify.wait_for_quiet(lines.append, list_procs=m.procs, clock=m.clock, sleep=m.sleep)
        self.assertFalse(quiet)
        self.assertGreaterEqual(waited, 1800)
        self.assertLess(waited, 1800 + 60)
        self.assertIn("TIMEOUT", lines[-1])
        self.assertIn("UNDER LOAD", lines[-1])
        # A report at least every five minutes while waiting.
        self.assertGreaterEqual(sum("load-wait: waiting for" in l for l in lines), 6)

    def test_posix_names_without_exe_match(self):
        m = FakeMachine([(30, [(7, "ninja")])])
        quiet, _, seen = verify.wait_for_quiet(lambda _: None, list_procs=m.procs, clock=m.clock, sleep=m.sleep)
        self.assertTrue(quiet)
        self.assertEqual(seen, {"ninja.exe": {7}})

    def test_excluded_pids_are_ignored(self):
        m = FakeMachine([(10**9, [(55, "cl.exe")])])
        quiet, _, _ = verify.wait_for_quiet(lambda _: None, list_procs=m.procs, clock=m.clock, sleep=m.sleep,
                                            exclude_pids=(55,))
        self.assertTrue(quiet)


class DryRunTest(unittest.TestCase):
    def test_dry_run_changes_nothing_and_needs_no_lock(self):
        with tempfile.TemporaryDirectory(prefix="wave-dry-") as tmp:
            main = Path(tmp) / "main"
            (main / "build-cmake").mkdir(parents=True)
            state = Path(tmp) / "state"
            with mock.patch("sys.stdout") as out:
                rc = verify.main(["verify.py", "claude/x", "K9", "uia", "--dry-run", f"--main={main}",
                                  f"--state-dir={state}"])
            printed = "".join(c.args[0] for c in out.write.call_args_list if c.args)
        self.assertEqual(rc, 0, printed)
        for expected in ("would run: git", "merge", "--target GenerateRedshipOtr", "-L ^redship$", "-L ^ui$",
                         "check-ui-parity-lint.py", "would wait up to 30m", "-L ^integration-soak$",
                         "would start heartbeat", "DRY RUN complete"):
            self.assertIn(expected, printed)
        self.assertFalse(state.exists(), "a dry run must not create the state dir")


class WrapperContractTest(unittest.TestCase):
    CMD_FILES = ("gate-lock.cmd", "gate-unlock.cmd", "verify.cmd", "_python.cmd")

    def test_wrappers_name_scripts_that_exist(self):
        for name in self.CMD_FILES:
            text = (WAVE_DIR / name).read_text(encoding="utf-8")
            for ref in re.findall(r"%~dp0([A-Za-z0-9_.-]+)", text):
                self.assertTrue((WAVE_DIR / ref).is_file(), f"{name} calls missing {ref}")

    def test_wrappers_are_crlf(self):
        """cmd.exe misparses labels and long lines in LF-only batch files;
        .gitattributes pins *.cmd to CRLF and this keeps it honest."""
        for name in self.CMD_FILES:
            data = (WAVE_DIR / name).read_bytes()
            self.assertNotIn(b"\n", data.replace(b"\r\n", b""), f"{name} has bare LF line endings")

    def test_scripts_guard_main_and_bind_no_windows_api_at_import(self):
        for path in sorted(WAVE_DIR.glob("*.py")):
            tree = ast.parse(path.read_text(encoding="utf-8"))
            self.assertIsNotNone(ast.get_docstring(tree), f"{path.name} has no docstring")
            top_calls = [n for n in tree.body if isinstance(n, ast.Expr) and isinstance(n.value, ast.Call)
                         and isinstance(n.value.func, ast.Name) and n.value.func.id == "main"]
            self.assertFalse(top_calls, f"{path.name} calls main() at import")
            src = path.read_text(encoding="utf-8")
            for node in tree.body:
                segment = ast.get_source_segment(src, node) or ""
                if not isinstance(node, (ast.FunctionDef, ast.ClassDef)):
                    self.assertNotIn("windll", segment, f"{path.name} binds windll at import")
                    self.assertNotIn("WinDLL", segment, f"{path.name} binds WinDLL at import")


if __name__ == "__main__":
    unittest.main()
