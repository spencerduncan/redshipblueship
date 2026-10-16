#!/usr/bin/env python3
"""Gate verify: build and test one branch merged onto origin/main, in the main
checkout, under the gate lock.

    verify.cmd <branch> <tag> [flags] [--dry-run] [--keep-going]
               [--main=<DIR>] [--state-dir=<DIR>] [--build-dir=<NAME>]

<branch> is pushed (origin/<branch>), or `main` to verify origin/main alone
(the closing smoke). <tag> must hold the gate lock (gate-lock.cmd tag=<tag>).
[flags] is any of:

    u   ui tier + the UI parity lint (plain and --no-grow-against origin/main)
    i   integration tier + integration-soak, after waiting for other builds
    a   regenerate the archives (extract both ROMs, soh/2ship/redship halves)

The redship and rando tiers always run. Every child inherits
SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN=1. The full log is
<state>/logs/verify-<tag>.log; stdout carries the step lines and the summary.
A background heartbeat process refreshes the lock every 60 s and stops with
this process. Exit 0 = every requested tier green, 1 = a failure (the FAIL
lines say which), 2 = usage or lock error. See README.md.
"""

from __future__ import annotations

import csv
import io
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import wavelib as w  # noqa: E402

HERE = Path(__file__).resolve().parent
FLAG_CHARS = {"u": "ui tier + parity lint", "i": "integration + soak", "a": "regenerate archives"}
# The order that works (GenerateRedshipOtr needs both extractions first).
ARCHIVE_TARGETS = ("ExtractAssets", "ExtractMMAssets", "GenerateSohOtr", "Generate2ShipOtr", "GenerateRedshipOtr")
DEFAULT_VCVARS = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
DEFAULT_CMAKE_DIR = r"C:\Program Files\CMake\bin"
NO_FOCUS_VAR = "SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN"


def _env_minutes(name: str, default: float) -> float:
    try:
        return float(os.environ.get(name, default))
    except ValueError:
        return default


class VerifyFail(Exception):
    """Stops the verify; the message is the FAIL line."""


class Usage(Exception):
    pass


def parse_flags(text: str) -> set:
    text = (text or "").strip()
    if text in ("", "-"):
        return set()
    unknown = sorted(set(text) - set(FLAG_CHARS))
    if unknown:
        raise Usage(f"unknown flag(s) {''.join(unknown)!r}; flags are any of u, i, a")
    return set(text)


def normalize_branch(branch: str) -> str:
    branch = branch.strip()
    for prefix in ("refs/heads/", "refs/remotes/origin/", "origin/"):
        if branch.startswith(prefix):
            branch = branch[len(prefix) :]
    return branch


# ---------------------------------------------------------------------------
# Preconditions
# ---------------------------------------------------------------------------


def check_gl_backend(build: Path, label: str = "build-cmake") -> str | None:
    """None if the staged config selects OpenGL. DXGI ignores the no-activation
    hint (test windows would steal focus) and deadlocks cross-game switches."""
    if os.name != "nt":
        return None
    path = build / "shipofharkinian.json"
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return f"precondition: {label}/shipofharkinian.json is missing (needs Window.Backend.Id 1, OpenGL)"
    except (OSError, ValueError) as exc:
        return f"precondition: {label}/shipofharkinian.json is unreadable: {exc}"
    backend = (data.get("Window") or {}).get("Backend") or {}
    if backend.get("Id") != 1:
        return (
            f"precondition: {label}/shipofharkinian.json selects backend {backend!r}, not OpenGL (Id 1); "
            "DXGI ignores the no-focus hint"
        )
    return None


# ---------------------------------------------------------------------------
# Load wait
# ---------------------------------------------------------------------------


def list_processes() -> list:
    """[(pid, image name lowercased)] for every process on the machine."""
    if os.name == "nt":
        out = subprocess.run(
            ["tasklist", "/FO", "CSV", "/NH"], capture_output=True, text=True, errors="replace"
        ).stdout
        procs = []
        for row in csv.reader(io.StringIO(out)):
            if len(row) >= 2 and row[1].strip().isdigit():
                procs.append((int(row[1]), row[0].strip().lower()))
        return procs
    out = subprocess.run(["ps", "-A", "-o", "pid=,comm="], capture_output=True, text=True, errors="replace").stdout
    procs = []
    for line in out.splitlines():
        parts = line.strip().split(None, 1)
        if len(parts) == 2 and parts[0].isdigit():
            procs.append((int(parts[0]), os.path.basename(parts[1]).lower()))
    return procs


def _matches(name: str, wanted: tuple) -> str | None:
    for want in wanted:
        if name == want or name == want.removesuffix(".exe"):
            return want
    return None


def wait_for_quiet(
    log,
    names: tuple = ("ninja.exe", "cl.exe"),
    max_wait_s: float = 1800.0,
    poll_s: float = 15.0,
    report_every_s: float = 300.0,
    list_procs=list_processes,
    clock=time.monotonic,
    sleep=time.sleep,
    exclude_pids: tuple = (),
) -> tuple[bool, float, dict]:
    """Wait until no process named in `names` runs (other than exclude_pids),
    up to max_wait_s. Logs what it waits for when the set of busy names
    changes and every report_every_s. Returns (quiet, waited_s, seen) where
    seen maps each name to the set of pids observed."""
    start = clock()
    seen: dict = {}
    last_names = None
    last_report = start
    while True:
        busy: dict = {}
        for pid, name in list_procs():
            want = _matches(name, names)
            if want and pid not in exclude_pids:
                busy.setdefault(want, set()).add(pid)
        waited = clock() - start
        for want, pids in busy.items():
            seen.setdefault(want, set()).update(pids)
        if not busy:
            if seen:
                log(f"load-wait: clear after {w.age_text(waited)}; waited for {_seen_text(seen)}")
            else:
                log(f"load-wait: no {'/'.join(names)} running; starting at once")
            return True, waited, seen
        now_names = tuple(sorted(busy))
        if now_names != last_names or clock() - last_report >= report_every_s:
            desc = ", ".join(f"{n} x{len(p)} (pids {', '.join(map(str, sorted(p)[:6]))})" for n, p in sorted(busy.items()))
            log(f"load-wait: waiting for {desc}; {w.age_text(waited)} elapsed of {w.age_text(max_wait_s)}")
            last_names = now_names
            last_report = clock()
        if waited >= max_wait_s:
            log(
                f"load-wait: TIMEOUT after {w.age_text(waited)}: still running {', '.join(now_names)}; "
                f"the tier starts UNDER LOAD (wall-clock rows can fail, #882): re-run before trusting a red row"
            )
            return False, waited, seen
        sleep(poll_s)


def _seen_text(seen: dict) -> str:
    return ", ".join(f"{n} ({len(p)} process{'es' if len(p) != 1 else ''})" for n, p in sorted(seen.items()))


# ---------------------------------------------------------------------------
# CTest output
# ---------------------------------------------------------------------------

_SUMMARY = re.compile(r"(\d+)% tests passed, (\d+) tests? failed out of (\d+)")
_ROW = re.compile(r"^\s*\d+\s+-\s+(\S+)\s+\((.+)\)\s*$")


@dataclass
class TierResult:
    label: str
    rc: int | None
    total: int = 0
    failed: list = field(default_factory=list)
    skipped: list = field(default_factory=list)
    seconds: float = 0.0
    under_load: bool = False
    bad_command: bool = False
    no_tests: bool = False

    def ok(self, allow_skips: bool = False) -> bool:
        return (
            self.rc == 0
            and self.total > 0
            and not self.failed
            and (allow_skips or not self.skipped)
        )

    def line(self) -> str:
        passed = self.total - len(self.failed) - len(self.skipped)
        text = f"{self.label} {passed}/{self.total}"
        if self.skipped:
            text += f" ({len(self.skipped)} skipped: {', '.join(self.skipped)})"
        if self.failed:
            text += f" FAILED: {', '.join(self.failed)}"
        if self.rc is None:
            text += " TIMED OUT"
        if self.no_tests:
            text += " NO TESTS FOUND"
        if self.under_load:
            text += " [under load]"
        return text + f" {int(self.seconds)}s"


def parse_ctest(label: str, rc: int | None, lines: list) -> TierResult:
    result = TierResult(label, rc)
    section = None
    for line in lines:
        m = _SUMMARY.search(line)
        if m:
            result.total = int(m.group(3))
            continue
        if "The following tests FAILED" in line:
            section = "failed"
            continue
        if "The following tests did not run" in line:
            section = "skipped"
            continue
        if "No tests were found" in line:
            result.no_tests = True
        if "BAD_COMMAND" in line:
            result.bad_command = True
        row = _ROW.match(line)
        if row and section:
            name, why = row.group(1), row.group(2)
            if section == "skipped" or why.strip().lower() in ("skipped", "not run", "disabled"):
                if name not in result.skipped:
                    result.skipped.append(name)
            elif name not in result.failed:
                result.failed.append(name)
        elif line.strip() and not line.startswith(" ") and section and not row:
            section = None
    return result


# ---------------------------------------------------------------------------
# The run
# ---------------------------------------------------------------------------


class Log:
    def __init__(self, path: Path | None):
        self.path = path
        self.fh = None
        if path is not None:
            path.parent.mkdir(parents=True, exist_ok=True)
            if path.exists():
                prev = path.with_name(path.name + ".prev")
                try:
                    os.replace(path, prev)
                except OSError:
                    pass
            self.fh = open(path, "w", encoding="utf-8", errors="replace")
        self.lock = threading.Lock()

    def _write(self, line: str) -> None:
        if self.fh:
            with self.lock:
                self.fh.write(line + "\n")
                self.fh.flush()

    def step(self, msg: str) -> None:
        line = f"{time.strftime('%Y-%m-%d %H:%M:%S')} [verify] {msg}"
        print(line, flush=True)
        self._write(line)

    def raw(self, line: str) -> None:
        self._write(line)

    def close(self) -> None:
        if self.fh:
            self.fh.close()


def kill_tree(pid: int) -> None:
    if os.name == "nt":
        subprocess.run(["taskkill", "/T", "/F", "/PID", str(pid)], capture_output=True)
    else:
        try:
            os.killpg(pid, signal.SIGKILL)
        except OSError:
            pass


class Verify:
    def __init__(self, branch: str, tag: str, flags: set, opts: dict):
        self.branch = normalize_branch(branch)
        self.tag = tag
        self.flags = flags
        self.dry = bool(opts.get("dry-run"))
        self.keep_going = bool(opts.get("keep-going"))
        self.main = w.main_checkout(opts.get("main"))
        self.state = w.state_dir(opts.get("state-dir"), self.main)
        self.build_name = opts.get("build-dir") or "build-cmake"
        self.build = self.main / self.build_name
        self.allow_skips = os.environ.get("WAVE_ALLOW_SKIPS") == "1"
        self.log = Log(None if self.dry else w.logs_path(self.state) / f"verify-{tag}.log")
        self.env = dict(os.environ)
        self.env[NO_FOCUS_VAR] = "1"
        self.failures: list = []
        self.warnings: list = []
        self.results: list = []
        self.heartbeat = None
        self.touched_checkout = False
        self.tree = None
        self.head = None
        self.under_load = False
        self.started = time.time()

    # -- plumbing ----------------------------------------------------------

    def run_cmd(self, cmd: list, timeout_min: float, cwd: Path | None = None) -> tuple[int | None, list]:
        shown = " ".join(f'"{c}"' if " " in str(c) else str(c) for c in cmd)
        if self.dry:
            self.log.step(f"would run: {shown}")
            return 0, []
        self.log.step(f"$ {shown}")
        kwargs = {}
        if os.name == "nt":
            kwargs["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP
        else:
            kwargs["start_new_session"] = True
        proc = subprocess.Popen(
            [str(c) for c in cmd],
            cwd=str(cwd or self.main),
            env=self.env,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            **kwargs,
        )
        lines: list = []

        def pump():
            for raw in proc.stdout:
                line = raw.rstrip("\r\n")
                lines.append(line)
                self.log.raw(line)

        reader = threading.Thread(target=pump, daemon=True)
        reader.start()
        try:
            rc = proc.wait(timeout=timeout_min * 60.0)
        except subprocess.TimeoutExpired:
            self.log.step(f"TIMEOUT after {timeout_min:g} min: killing process tree {proc.pid}")
            kill_tree(proc.pid)
            try:
                proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                pass
            rc = None
        reader.join(timeout=10)
        return rc, lines

    def git(self, *args: str, check: bool = True) -> subprocess.CompletedProcess:
        if self.dry:
            self.log.step(f"would run: git -C {self.main} {' '.join(args)}")
            return subprocess.CompletedProcess(args, 0, "", "")
        proc = w.git(self.main, *args)
        if check and proc.returncode != 0:
            raise VerifyFail(f"git {' '.join(args)}: {(proc.stderr or proc.stdout).strip()}")
        return proc

    # -- steps -------------------------------------------------------------

    def check_lock(self) -> None:
        decision = w.lock_status(self.state)
        held = (decision.owner or {}).get("tag")
        if self.dry:
            self.log.step(f"lock: {decision.verdict} {w.describe_owner(decision.owner)} (needs tag={self.tag})")
            return
        if decision.verdict == w.FREE or held != self.tag:
            raise Usage(
                f"the gate lock is not held by tag={self.tag} "
                f"({'free' if decision.verdict == w.FREE else w.describe_owner(decision.owner)}); "
                f"take it first: gate-lock.cmd tag={self.tag} pr=<N> run=<RUN>"
            )
        w.touch_heartbeat(self.state, self.tag, pid=os.getpid(), pid_role="verify")
        self.log.step(f"lock: held by tag={self.tag}; registered verify pid {os.getpid()}")

    def start_heartbeat(self) -> None:
        interval = os.environ.get("WAVE_HEARTBEAT_S", "60")
        cmd = [
            sys.executable,
            str(HERE / "gatelock.py"),
            "heartbeat-loop",
            f"tag={self.tag}",
            f"--watch-pid={os.getpid()}",
            f"--interval={interval}",
            f"--state-dir={self.state}",
        ]
        if self.dry:
            self.log.step(f"would start heartbeat: {' '.join(cmd)}")
            return
        out = open(w.logs_path(self.state) / f"heartbeat-{self.tag}.log", "w", encoding="utf-8")
        kwargs = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
        self.heartbeat = subprocess.Popen(cmd, stdout=out, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, **kwargs)
        out.close()
        self.log.step(f"heartbeat: process {self.heartbeat.pid} refreshes the lock every {interval} s")

    def stop_heartbeat(self) -> None:
        if self.heartbeat is None:
            return
        self.heartbeat.terminate()
        try:
            self.heartbeat.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.heartbeat.kill()
        # One last beat: the 5-minute stale window starts when verify ends.
        w.touch_heartbeat(self.state, self.tag)
        self.log.step(f"heartbeat: process {self.heartbeat.pid} stopped")
        self.heartbeat = None

    def preconditions(self) -> None:
        if not self.build.is_dir() and not self.dry:
            raise VerifyFail(f"precondition: {self.build} does not exist (configure it first)")
        problems = [] if "a" in self.flags else w.check_archives(self.build, self.build_name)
        gl = check_gl_backend(self.build, self.build_name)
        if gl:
            problems.append(gl)
        if problems and not self.dry:
            raise VerifyFail("; ".join(problems))
        for p in problems:
            self.log.step(f"dry-run: would FAIL {p}")
        for name in w.CURATED_ARCHIVES:
            if not (self.build / name).is_file() and "a" not in self.flags:
                self.warn(f"{self.build_name}/{name} is missing; its rows will SKIP (flag a regenerates it)")

    def toolchain(self) -> None:
        if os.name != "nt":
            return
        vcvars = os.environ.get("WAVE_VCVARS", DEFAULT_VCVARS)
        if self.dry:
            self.log.step(f"would load the MSVC environment from {vcvars}")
            return
        if "VSCMD_VER" not in self.env:
            if not Path(vcvars).is_file():
                raise VerifyFail(f"toolchain: vcvars64.bat not found at {vcvars} (set WAVE_VCVARS)")
            proc = subprocess.run(
                f'cmd /d /s /c ""{vcvars}" >nul && set"', capture_output=True, text=True, errors="replace"
            )
            if proc.returncode != 0:
                raise VerifyFail(f"toolchain: {vcvars} failed: {proc.stderr.strip()}")
            for line in proc.stdout.splitlines():
                key, sep, value = line.partition("=")
                if sep and key and not key.startswith("="):
                    self.env[key.upper()] = value
        # Always after vcvars: vcvars64 points VCPKG_ROOT at Visual Studio's own
        # bundled vcpkg, and the build needs the checkout's.
        vcpkg = os.environ.get("WAVE_VCPKG_ROOT") or str(self.main / "vcpkg").replace("\\", "/")
        if not Path(vcpkg).is_dir():
            self.warn(f"toolchain: VCPKG_ROOT {vcpkg} does not exist (set WAVE_VCPKG_ROOT)")
        self.env["VCPKG_ROOT"] = vcpkg
        self.env.setdefault("SCCACHE_CACHE_SIZE", "40G")
        extra = []
        cmake_dir = os.environ.get("WAVE_CMAKE_DIR", DEFAULT_CMAKE_DIR)
        if Path(cmake_dir).is_dir():
            extra.append(cmake_dir)
        sccache = os.environ.get("WAVE_SCCACHE_DIR")
        if not sccache and not shutil.which("sccache", path=self.env.get("PATH")):
            root = Path(os.environ.get("LOCALAPPDATA", "")) / "Microsoft" / "WinGet" / "Packages"
            found = sorted(root.glob("Mozilla.sccache_*/sccache-*/sccache.exe"), key=lambda p: p.stat().st_mtime)
            if found:
                sccache = str(found[-1].parent)
        if sccache:
            extra.append(sccache)
        if extra:
            self.env["PATH"] = os.pathsep.join(extra + [self.env.get("PATH", "")])
        for tool in ("cmake", "ctest", "ninja", "sccache"):
            if not shutil.which(tool, path=self.env.get("PATH")):
                self.warn(f"toolchain: {tool} is not on PATH")
        self.log.step(f"toolchain: MSVC {self.env.get('VSCMD_VER', '?')}, VCPKG_ROOT={self.env['VCPKG_ROOT']}")

    def prepare_checkout(self) -> None:
        if not self.dry:
            w.abort_in_progress(self.main, self.log.step)
            report = w.checkout_status(self.main)
            if report.blocking:
                raise VerifyFail(
                    "checkout: the main checkout has changes outside the build stamps: " + "; ".join(report.blocking)
                )
        self.git("fetch", "-q", "--prune", "origin")
        if self.branch != "main":
            probe = self.git("rev-parse", "--verify", "-q", f"origin/{self.branch}^{{commit}}", check=False)
            if probe.returncode != 0:
                raise VerifyFail(f"checkout: origin/{self.branch} does not exist (push the branch first)")
        self.touched_checkout = True
        if self.dry:
            self.git("checkout", "-q", "--detach", "origin/main")
        else:
            try:
                w.switch_checkout(self.main, ("checkout", "-q", "--detach", "origin/main"), self.log.step)
            except w.RestoreError as exc:
                raise VerifyFail(f"checkout: {exc}") from exc
        if self.branch != "main":
            merge = self.git(
                "-c",
                "user.name=wave-verify",
                "-c",
                "user.email=wave-verify@localhost",
                "merge",
                "--no-edit",
                "-q",
                f"origin/{self.branch}",
                check=False,
            )
            if merge.returncode != 0:
                conflicted = self.git("diff", "--name-only", "--diff-filter=U", check=False).stdout.split()
                self.git("merge", "--abort", check=False)
                where = ", ".join(conflicted) if conflicted else (merge.stderr or merge.stdout).strip()
                raise VerifyFail(f"merge: origin/{self.branch} does not merge cleanly onto origin/main: {where}")
        self.git("submodule", "update", "--init", "--recursive", "-q")
        if not self.dry:
            self.head = self.git("rev-parse", "HEAD").stdout.strip()
            self.tree = self.git("rev-parse", "HEAD^{tree}").stdout.strip()
            base = self.git("rev-parse", "origin/main").stdout.strip()
            self.log.step(f"checkout: verifying HEAD {self.head} tree {self.tree} (origin/main {base[:10]})")

    def build_all(self) -> None:
        rc, lines = self.run_cmd(["cmake", "--build", self.build], _env_minutes("WAVE_BUILD_TIMEOUT_MIN", 120))
        if rc != 0:
            errors = [l.strip() for l in lines if re.search(r"(: error|: fatal error|^FAILED:|error LNK)", l)][:5]
            raise VerifyFail(
                f"build: cmake --build exited {rc if rc is not None else 'TIMEOUT'}"
                + (f": {' | '.join(errors)}" if errors else "; see the log")
            )

    def regenerate_archives(self) -> None:
        for target in ARCHIVE_TARGETS:
            rc, _ = self.run_cmd(
                ["cmake", "--build", self.build, "--target", target], _env_minutes("WAVE_BUILD_TIMEOUT_MIN", 120)
            )
            if rc != 0:
                raise VerifyFail(f"archives: target {target} exited {rc if rc is not None else 'TIMEOUT'}")
        for name, sources in (
            ("oot.o2r", ("games/oot/oot.o2r", "oot.o2r")),
            ("mm.o2r", ("games/mm/mm.o2r", "mm.o2r")),
        ):
            src = next((self.main / s for s in sources if (self.main / s).is_file()), None)
            if self.dry:
                self.log.step(f"would copy {sources[0]} -> {self.build_name}/{name}")
                continue
            if src is None:
                raise VerifyFail(f"archives: no {name} was produced (looked in {', '.join(sources)})")
            shutil.copyfile(src, self.build / name)
            self.log.step(f"archives: staged {self.build_name}/{name} ({src.stat().st_size} bytes)")
        problems = w.check_archives(self.build, self.build_name)
        if problems and not self.dry:
            raise VerifyFail("; ".join(problems))

    def tier(self, label: str) -> bool:
        started = time.time()
        rc, lines = self.run_cmd(
            ["ctest", "--test-dir", self.build, "-L", f"^{label}$", "--output-on-failure", "--no-tests=error"],
            _env_minutes("WAVE_TIER_TIMEOUT_MIN", 90),
        )
        if self.dry:
            return True
        result = parse_ctest(label, rc, lines)
        result.seconds = time.time() - started
        result.under_load = self.under_load and label.startswith("integration")
        self.results.append(result)
        ok = result.ok(self.allow_skips)
        self.log.step(f"tier: {result.line()}")
        if result.bad_command:
            self.warn("BAD_COMMAND rows: Application Control blocked this binary; relink for a new hash and re-run")
        if result.skipped and self.allow_skips:
            self.warn(f"{label}: {len(result.skipped)} skipped rows tolerated (WAVE_ALLOW_SKIPS=1)")
        if not ok:
            why = result.line()
            if result.skipped and not result.failed and result.rc == 0:
                why += " (a skip in the staged main tree means a missing input)"
            self.failures.append(f"tier {why}")
        return ok

    def parity_lint(self) -> bool:
        script = self.main / ".github" / "scripts" / "check-ui-parity-lint.py"
        ok = True
        for extra, name in (([], "parity lint"), (["--no-grow-against", "origin/main"], "parity lint no-grow")):
            rc, _ = self.run_cmd([sys.executable, script, *extra], 10)
            if self.dry:
                continue
            self.log.step(f"tier: {name} {'OK' if rc == 0 else f'FAILED (exit {rc})'}")
            if rc != 0:
                self.failures.append(f"{name} exited {rc}")
                ok = False
        return ok

    def load_wait(self) -> None:
        names = tuple(n.strip().lower() for n in os.environ.get("WAVE_LOAD_PROCS", "ninja.exe,cl.exe").split(",") if n.strip())
        max_wait = _env_minutes("WAVE_LOAD_WAIT_MIN", 30) * 60.0
        if self.dry:
            self.log.step(f"would wait up to {w.age_text(max_wait)} for no {'/'.join(names)} before the integration tier")
            return
        quiet, _, _ = wait_for_quiet(self.log.step, names=names, max_wait_s=max_wait)
        if not quiet:
            self.under_load = True
            self.warn("integration tier ran under load: a red int-* row needs a quiet re-run before it counts (#882)")

    def restore(self) -> None:
        if not self.touched_checkout or self.dry:
            if self.dry:
                self.log.step(f"would restore {self.main} to main (fast-forward to origin/main)")
            return
        try:
            head = w.restore_checkout(self.main, self.log.step, fetch=False)
            self.log.step(f"checkout: restored main at {head}")
        except (w.RestoreError, subprocess.SubprocessError, OSError) as exc:
            self.warn(f"checkout: could not restore main ({exc}); preflight.py will")

    def warn(self, msg: str) -> None:
        self.warnings.append(msg)
        self.log.step(f"WARN {msg}")

    # -- driver ------------------------------------------------------------

    def run(self) -> int:
        self.log.step(
            f"verify branch={self.branch} tag={self.tag} flags={''.join(sorted(self.flags)) or '-'} "
            f"main={self.main} pid={os.getpid()}{' DRY RUN' if self.dry else ''}"
        )
        if self.log.path:
            self.log.step(f"log: {self.log.path}")
        try:
            self.check_lock()
        except Usage as exc:
            self.log.step(f"FAIL lock: {exc}")
            self.log.close()
            return 2
        try:
            self.start_heartbeat()
            self.preconditions()
            self.toolchain()
            self.prepare_checkout()
            self.build_all()
            if "a" in self.flags:
                self.regenerate_archives()
            tiers = ["redship", "rando"]
            if "u" in self.flags:
                tiers.append("ui")
            if "i" in self.flags:
                tiers += ["integration", "integration-soak"]
            for label in tiers:
                if label == "integration":
                    self.load_wait()
                if not self.tier(label) and not self.keep_going:
                    self.log.step(f"stopping after the red {label} tier (pass --keep-going to run the rest)")
                    break
                if label == "ui" and not self.parity_lint() and not self.keep_going:
                    break
        except VerifyFail as exc:
            self.failures.append(str(exc))
        except Exception as exc:  # a crash must still stop the heartbeat and restore
            self.failures.append(f"internal error: {type(exc).__name__}: {exc}")
            raise
        finally:
            self.restore()
            self.stop_heartbeat()
            self.summary()
            self.log.close()
        return 0 if not self.failures else 1

    def summary(self) -> None:
        took = w.age_text(time.time() - self.started)
        for result in self.results:
            self.log.step(f"result: {result.line()}")
        for msg in self.warnings:
            self.log.step(f"summary WARN {msg}")
        if self.dry:
            self.log.step("DRY RUN complete: nothing was changed")
            return
        if self.failures:
            for msg in self.failures:
                self.log.step(f"FAIL {msg}")
            self.log.step(f"VERIFY FAIL tag={self.tag} branch={self.branch} tree={self.tree or '?'} in {took}")
        else:
            self.log.step(f"VERIFY PASS tag={self.tag} branch={self.branch} tree={self.tree} in {took}")


def main(argv: list | None = None) -> int:
    argv = sys.argv if argv is None else argv
    pairs, flags, positional = w.split_args(argv[1:])
    if len(argv) < 3 or len(positional) < 2 or len(positional) > 3 or pairs:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    opts = {}
    for name in ("dry-run", "keep-going", "main", "state-dir", "build-dir"):
        value = w.flag_value(flags, name)
        if value is not None:
            opts[name] = value
    known = {"--dry-run", "--keep-going"}
    for flag in flags:
        if flag not in known and flag.split("=", 1)[0] not in ("--main", "--state-dir", "--build-dir"):
            print(f"FAIL verify: unknown option {flag}", file=sys.stderr)
            return 2
    try:
        letters = parse_flags(positional[2] if len(positional) == 3 else "")
    except Usage as exc:
        print(f"FAIL verify: {exc}", file=sys.stderr)
        return 2
    return Verify(positional[0], positional[1], letters, opts).run()


if __name__ == "__main__":
    sys.exit(main())
