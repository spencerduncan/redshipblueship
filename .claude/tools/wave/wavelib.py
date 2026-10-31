#!/usr/bin/env python3
"""Shared core of the wave toolkit: paths, the gate lock, pid liveness, the
main checkout's git state and the per-lane resume manifests.

Every CLI in this directory (gatelock.py, verify.py, preflight.py,
lane-manifest.py) is a thin argument layer over this module, so the protocol
lives in one place and the unit tests in tests/ exercise exactly what the
gates run. Portable: the Windows-only calls (ctypes kernel32, tasklist) are
made inside functions behind `os.name == "nt"`, never at import time, so the
module imports and tests on the Linux CI runner. See README.md for the
protocol itself.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import socket
import subprocess
import sys
import time
import uuid
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

DEFAULT_MAIN = "C:/Users/penpen/src/redshipblueship"

# Rewritten by every build (the commit/time stamp and the two configure_file
# outputs), so they are "modified" in any checkout that has built. They are the
# only tracked changes the main checkout may carry between gates.
BUILD_STAMPS = (
    "games/mm/src/boot/build.c",
    "games/oot/src/boot/build.c",
    "games/mm/windows/properties.h",
    "games/oot/properties.h",
)

# An untracked file under one of these can end up in the build (games/mm/2s2h
# is globbed with CONFIGURE_DEPENDS), so it blocks a restore. Untracked files
# anywhere else (.agent/, editor droppings) are reported and tolerated.
BUILD_INPUT_ROOTS = (
    "games/",
    "src/",
    "rsbs/",
    "libultraship/",
    "ZAPDTR/",
    "OTRExporter/",
    "CMake/",
    "assets/",
    "scripts/",
)

LOCK_DIR_NAME = "gate.lock"
OWNER_FILE = "owner.txt"
HISTORY_FILE = "lock-history.log"


def main_checkout(override: str | os.PathLike | None = None) -> Path:
    """The checkout the gates build in: --main, then $WAVE_MAIN, then the default."""
    return Path(override or os.environ.get("WAVE_MAIN") or DEFAULT_MAIN)


def state_dir(override: str | os.PathLike | None = None, main: Path | None = None) -> Path:
    """Where the lock, manifests and logs live: --state-dir, $WAVE_STATE_DIR, or
    <main checkout>/.wave (gitignored). Deliberately not a session scratchpad:
    the whole point is surviving a session or machine restart."""
    if override:
        return Path(override)
    env = os.environ.get("WAVE_STATE_DIR")
    if env:
        return Path(env)
    return (main or main_checkout()) / ".wave"


def lock_path(state: Path) -> Path:
    return state / LOCK_DIR_NAME


def lanes_path(state: Path) -> Path:
    return state / "lanes"


def logs_path(state: Path) -> Path:
    return state / "logs"


def stale_after_s() -> float:
    """Heartbeat age after which a lock with a dead pid may be swept."""
    try:
        return float(os.environ.get("WAVE_STALE_MIN", "5")) * 60.0
    except ValueError:
        return 300.0


# ---------------------------------------------------------------------------
# Time
# ---------------------------------------------------------------------------


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


def iso(dt: datetime | None = None) -> str:
    """UTC, second resolution, `Z` suffix: unambiguous across machines and DST."""
    return (dt or utcnow()).astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def parse_iso(text: str | None) -> datetime | None:
    if not text:
        return None
    text = text.strip()
    if text.endswith("Z"):
        text = text[:-1] + "+00:00"
    try:
        dt = datetime.fromisoformat(text)
    except ValueError:
        return None
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return dt


def age_text(seconds: float | None) -> str:
    if seconds is None:
        return "?"
    seconds = max(0, int(seconds))
    if seconds < 120:
        return f"{seconds}s"
    if seconds < 7200:
        return f"{seconds // 60}m"
    if seconds < 172800:
        return f"{seconds // 3600}h{(seconds % 3600) // 60:02d}m"
    return f"{seconds // 86400}d"


# ---------------------------------------------------------------------------
# Atomic file writes
# ---------------------------------------------------------------------------


def atomic_write(path: Path, text: str, attempts: int = 40) -> None:
    """Write via a sibling temp file and os.replace.

    On Windows os.replace fails with a sharing violation while another process
    has the target open (Python opens without FILE_SHARE_DELETE), and readers
    here hold the file for microseconds, so retry for up to ~2 s. A missing
    parent directory is NOT retried: for the lock that means it was released.
    """
    tmp = path.with_name(f"{path.name}.{os.getpid()}.{uuid.uuid4().hex[:8]}.tmp")
    tmp.write_text(text, encoding="utf-8", newline="\n")
    try:
        for i in range(attempts):
            try:
                os.replace(tmp, path)
                return
            except PermissionError:
                if i == attempts - 1:
                    raise
                time.sleep(0.05)
    finally:
        if tmp.exists():
            try:
                tmp.unlink()
            except OSError:
                pass


# ---------------------------------------------------------------------------
# Process liveness
# ---------------------------------------------------------------------------

_STILL_ACTIVE = 259
_PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
_ERROR_ACCESS_DENIED = 5


def _win_kernel32():
    import ctypes
    from ctypes import wintypes

    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    k32.OpenProcess.restype = wintypes.HANDLE
    k32.CloseHandle.argtypes = [wintypes.HANDLE]
    k32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    k32.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    return ctypes, wintypes, k32


def _win_probe(pid: int) -> tuple[bool, str | None]:
    """(alive, start token) for a Windows pid."""
    ctypes, wintypes, k32 = _win_kernel32()
    handle = k32.OpenProcess(_PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        # Access denied means the process exists but is protected from us.
        return ctypes.get_last_error() == _ERROR_ACCESS_DENIED, None
    try:
        code = wintypes.DWORD()
        if not k32.GetExitCodeProcess(handle, ctypes.byref(code)):
            return False, None
        if code.value != _STILL_ACTIVE:
            return False, None
        times = [wintypes.FILETIME() for _ in range(4)]
        token = None
        if k32.GetProcessTimes(handle, *[ctypes.byref(t) for t in times]):
            created = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
            token = f"win:{created}"
        return True, token
    finally:
        k32.CloseHandle(handle)


def _posix_probe(pid: int) -> tuple[bool, str | None]:
    eperm = False
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False, None
    except PermissionError:
        eperm = True  # exists, owned by someone else
    except OSError:
        return False, None
    stat = Path(f"/proc/{pid}/stat")
    try:
        raw = stat.read_text()
    except OSError:
        # No /proc (macOS, BSD): kill(pid, 0) succeeds on a zombie, so ask ps.
        try:
            out = subprocess.run(
                ["ps", "-o", "stat=", "-o", "lstart=", "-p", str(pid)],
                capture_output=True,
                text=True,
            ).stdout.strip()
        except OSError:
            return True, None
        if not out:
            return eperm, None
        state, *start = out.split(None, 1)
        if state.startswith(("Z", "X")):
            return False, None
        token = f"posix:{'_'.join(start[0].split())}" if start else None
        return True, token
    # Field 2 (comm) may contain spaces and parentheses; everything after the
    # LAST ')' is space-separated starting at field 3 (state).
    rest = raw[raw.rfind(")") + 2 :].split()
    if rest and rest[0] in ("Z", "X"):
        return False, None
    token = f"linux:{rest[19]}" if len(rest) > 19 else None
    return True, token


def probe_pid(pid: int | None) -> tuple[bool, str | None]:
    if not pid or pid <= 0:
        return False, None
    if os.name == "nt":
        return _win_probe(pid)
    return _posix_probe(pid)


def pid_start_token(pid: int | None) -> str | None:
    """An opaque process-creation token, so a reused pid is not mistaken for
    the recorded process (Windows recycles pids within minutes)."""
    return probe_pid(pid)[1]


def pid_alive(pid: int | None, token: str | None = None) -> bool:
    alive, current = probe_pid(pid)
    if not alive:
        return False
    if token and current and token != current:
        return False  # same number, different process
    return True


# ---------------------------------------------------------------------------
# Owner file
# ---------------------------------------------------------------------------

OWNER_KEYS = ("tag", "pr", "run", "pid", "pidStarted", "pidRole", "host", "acquired", "heartbeat")


def format_owner(owner: dict) -> str:
    keys = list(OWNER_KEYS) + sorted(k for k in owner if k not in OWNER_KEYS)
    return "".join(f"{k}={owner.get(k, '')}\n" for k in keys if k in owner)


def parse_owner(text: str) -> dict:
    """key=value lines. Accepts the wave-8 scratchpad format too (`owner=` for
    the tag, `time=` for the acquire time, no heartbeat)."""
    owner: dict = {}
    for line in text.splitlines():
        if "=" not in line:
            continue
        key, _, value = line.partition("=")
        owner[key.strip()] = value.strip()
    if "tag" not in owner and "owner" in owner:
        owner["tag"] = owner["owner"]
    if "acquired" not in owner and "time" in owner:
        owner["acquired"] = owner["time"]
    return owner


def owner_pid(owner: dict | None) -> int | None:
    try:
        return int((owner or {}).get("pid", ""))
    except ValueError:
        return None


def describe_owner(owner: dict | None) -> str:
    if not owner:
        return "(no owner.txt)"
    parts = [f"tag={owner.get('tag', '?')}"]
    for key in ("pr", "run", "pid", "pidRole", "acquired", "heartbeat"):
        if owner.get(key):
            parts.append(f"{key}={owner[key]}")
    return " ".join(parts)


# ---------------------------------------------------------------------------
# Lock decision
# ---------------------------------------------------------------------------

FREE = "free"  # no lock directory
LIVE = "live"  # heartbeat fresh
KEEP = "keep"  # heartbeat stale but the pid is alive: never swept automatically
STALE = "stale"  # heartbeat stale and pid dead: safe to sweep


@dataclass
class Decision:
    verdict: str
    reason: str
    heartbeat_age_s: float | None = None
    owner: dict | None = None


def decide(
    owner: dict | None,
    dir_mtime: float | None,
    now: float,
    stale_s: float,
    alive=pid_alive,
    exists: bool = True,
) -> Decision:
    """The one stale-lock rule, shared by preflight, acquire --wait and status.

    A lock is swept only when BOTH its heartbeat is older than `stale_s` AND
    its recorded pid is dead. The heartbeat falls back to the acquire time and
    then to the directory's mtime (an acquirer killed between mkdir and writing
    owner.txt leaves an ownerless directory, which ages out the same way).
    """
    if not exists:
        return Decision(FREE, "no lock", None, None)
    stamp = None
    source = "heartbeat"
    for key in ("heartbeat", "acquired"):
        dt = parse_iso((owner or {}).get(key))
        if dt is not None:
            stamp = dt.timestamp()
            source = key
            break
    if stamp is None and dir_mtime is not None:
        stamp = dir_mtime
        source = "directory mtime"
    age = None if stamp is None else max(0.0, now - stamp)
    pid = owner_pid(owner)
    token = (owner or {}).get("pidStarted") or None
    if age is not None and age <= stale_s:
        return Decision(LIVE, f"{source} {age_text(age)} old (fresh)", age, owner)
    is_alive = bool(pid) and alive(pid, token)
    if is_alive:
        return Decision(
            KEEP,
            f"{source} {age_text(age)} old but pid {pid} is alive; not swept (use gate-unlock --force if it is wedged)",
            age,
            owner,
        )
    pid_text = f"pid {pid} dead" if pid else "no pid recorded"
    return Decision(STALE, f"{source} {age_text(age)} old and {pid_text}", age, owner)


# ---------------------------------------------------------------------------
# Lock operations
# ---------------------------------------------------------------------------


class LockError(Exception):
    """A lock operation was refused; the message is the reason."""


def read_lock(state: Path) -> tuple[bool, dict | None, float | None]:
    lock = lock_path(state)
    try:
        mtime = lock.stat().st_mtime
    except FileNotFoundError:
        return False, None, None
    try:
        owner = parse_owner((lock / OWNER_FILE).read_text(encoding="utf-8"))
    except FileNotFoundError:
        owner = None
    except OSError:
        owner = None
    return True, owner, mtime


def lock_status(state: Path, now: float | None = None, stale_s: float | None = None, alive=pid_alive) -> Decision:
    exists, owner, mtime = read_lock(state)
    return decide(
        owner,
        mtime,
        time.time() if now is None else now,
        stale_after_s() if stale_s is None else stale_s,
        alive=alive,
        exists=exists,
    )


def _append_history(state: Path, line: str) -> None:
    try:
        state.mkdir(parents=True, exist_ok=True)
        with open(state / HISTORY_FILE, "a", encoding="utf-8") as fh:
            fh.write(line.rstrip("\n") + "\n")
    except OSError:
        pass


def _same_owner(a: dict | None, b: dict | None) -> bool:
    keys = ("tag", "acquired", "pid", "run")
    return (a or {}).get("tag") is not None and all((a or {}).get(k) == (b or {}).get(k) for k in keys)


def _remove_lock(state: Path, expected: dict | None, by: str, reason: str) -> bool:
    """Atomically retire the lock directory: rename it to a tombstone first, so
    no observer ever sees a half-deleted lock, then delete the tombstone. If
    the tombstone turns out to hold a DIFFERENT owner (someone released and
    re-acquired between our read and our rename), put it back."""
    lock = lock_path(state)
    tomb = state / f"{LOCK_DIR_NAME}.released-{os.getpid()}-{uuid.uuid4().hex[:8]}"
    for attempt in range(40):
        try:
            os.rename(lock, tomb)
            break
        except FileNotFoundError:
            return False
        except PermissionError:
            # Windows: a reader has owner.txt open for a moment.
            if attempt == 39:
                raise
            time.sleep(0.05)
    try:
        got = parse_owner((tomb / OWNER_FILE).read_text(encoding="utf-8"))
    except OSError:
        got = None
    if expected is not None and got is not None and not _same_owner(expected, got):
        try:
            os.rename(tomb, lock)
        except OSError:
            _append_history(state, f"{iso()} ERROR {by}: raced a new owner and could not restore {tomb}")
        return False
    _append_history(state, f"{iso()} released by={by} reason={reason} | {describe_owner(got)}")
    shutil.rmtree(tomb, ignore_errors=True)
    return True


def sweep_stale(state: Path, by: str, now: float | None = None, stale_s: float | None = None, alive=pid_alive) -> Decision:
    """Release the lock iff decide() says STALE. Returns the decision taken."""
    decision = lock_status(state, now=now, stale_s=stale_s, alive=alive)
    if decision.verdict == STALE:
        if not _remove_lock(state, decision.owner, by, f"stale: {decision.reason}"):
            # Someone else released or replaced it first; report what is there now.
            return lock_status(state, now=now, stale_s=stale_s, alive=alive)
    return decision


def acquire(
    state: Path,
    tag: str,
    pr: str = "",
    run: str = "",
    pid: int | None = None,
    pid_role: str = "gate-lock",
    wait_s: float = 0.0,
    poll_s: float = 15.0,
    sweep: bool = True,
    log=None,
    alive=pid_alive,
    stale_s: float | None = None,
) -> dict:
    """mkdir is the atomic test-and-set; owner.txt is written right after.
    Raises LockError naming the current owner if the lock stays held."""
    if not tag or not re.fullmatch(r"[A-Za-z0-9._-]+", tag):
        raise LockError(f"invalid tag {tag!r}: use letters, digits, '.', '_' or '-'")
    lock = lock_path(state)
    state.mkdir(parents=True, exist_ok=True)
    deadline = time.time() + max(0.0, wait_s)
    waited_for = None
    while True:
        try:
            os.mkdir(lock)
        except FileExistsError:
            decision = (
                sweep_stale(state, by=f"acquire:{tag}", stale_s=stale_s, alive=alive)
                if sweep
                else lock_status(state, stale_s=stale_s, alive=alive)
            )
            if decision.verdict == FREE or (sweep and decision.verdict == STALE):
                if log and decision.verdict == STALE:
                    log(f"swept stale lock ({decision.reason}): {describe_owner(decision.owner)}")
                continue
            if time.time() >= deadline:
                raise LockError(
                    f"gate lock held: {describe_owner(decision.owner)} [{decision.verdict}: {decision.reason}]"
                )
            if log and describe_owner(decision.owner) != waited_for:
                waited_for = describe_owner(decision.owner)
                log(f"waiting for gate lock: {waited_for}")
            time.sleep(min(poll_s, max(0.1, deadline - time.time())))
            continue
        holder = pid if pid is not None else os.getppid()
        now = iso()
        owner = {
            "tag": tag,
            "pr": pr or "",
            "run": run or "",
            "pid": str(holder),
            "pidStarted": pid_start_token(holder) or "",
            "pidRole": pid_role,
            "host": socket.gethostname(),
            "acquired": now,
            "heartbeat": now,
        }
        atomic_write(lock / OWNER_FILE, format_owner(owner))
        _append_history(state, f"{now} acquired | {describe_owner(owner)}")
        return owner


def release(
    state: Path,
    tag: str | None,
    force: bool = False,
    by: str = "gate-unlock",
    alive=pid_alive,
    self_pids: tuple[int, ...] = (),
) -> str:
    """Release the lock held by `tag`. Refuses a tag mismatch, or a lock whose
    recorded pid is alive (a verify still running), unless force. A pid in
    `self_pids` (the caller itself) never blocks. Returns a status line."""
    exists, owner, _ = read_lock(state)
    if not exists:
        return "gate lock already free"
    held_by = (owner or {}).get("tag")
    if not force:
        if not tag:
            raise LockError("refusing to unlock without tag= or owner= (or --force)")
        if held_by is not None and held_by != tag:
            raise LockError(f"refusing: lock is held by tag={held_by}, not {tag} ({describe_owner(owner)})")
        pid = owner_pid(owner)
        if pid and pid not in self_pids and alive(pid, (owner or {}).get("pidStarted") or None):
            raise LockError(
                f"refusing: pid {pid} ({(owner or {}).get('pidRole', '?')}) is still alive; "
                f"wait for it or pass --force ({describe_owner(owner)})"
            )
    reason = "forced" if force else "unlock"
    if not _remove_lock(state, owner, by, reason):
        exists_now, owner_now, _ = read_lock(state)
        if exists_now:
            raise LockError(f"lock changed hands while unlocking: {describe_owner(owner_now)}")
    return f"released gate lock {describe_owner(owner)}"


def touch_heartbeat(state: Path, tag: str, pid: int | None = None, pid_role: str | None = None) -> bool:
    """Refresh heartbeat= (and optionally take over the pid). False if the lock
    is gone or held by another tag; never creates a lock."""
    lock = lock_path(state)
    try:
        owner = parse_owner((lock / OWNER_FILE).read_text(encoding="utf-8"))
    except OSError:
        return False
    if owner.get("tag") != tag:
        return False
    owner["heartbeat"] = iso()
    if pid is not None:
        owner["pid"] = str(pid)
        owner["pidStarted"] = pid_start_token(pid) or ""
    if pid_role:
        owner["pidRole"] = pid_role
    # Re-check right before replacing: a release + re-acquire in between must
    # not have its owner file overwritten by ours.
    try:
        again = parse_owner((lock / OWNER_FILE).read_text(encoding="utf-8"))
    except OSError:
        return False
    if again.get("tag") != tag or again.get("acquired") != owner.get("acquired"):
        return False
    try:
        atomic_write(lock / OWNER_FILE, format_owner(owner))
    except (FileNotFoundError, NotADirectoryError):
        return False
    return True


def heartbeat_loop(state: Path, tag: str, watch_pid: int | None, interval_s: float = 60.0, log=None) -> int:
    """Refresh the heartbeat every interval until the watched process exits or
    the lock leaves `tag`. Polls the watched pid every second so a killed
    verify stops refreshing within a second, not a minute."""
    token = pid_start_token(watch_pid) if watch_pid else None
    next_beat = 0.0
    while True:
        if watch_pid and not pid_alive(watch_pid, token):
            if log:
                log(f"heartbeat: watched pid {watch_pid} exited; stopping")
            return 0
        if time.time() >= next_beat:
            if not touch_heartbeat(state, tag):
                if log:
                    log(f"heartbeat: lock is no longer held by tag={tag}; stopping")
                return 1
            if log:
                log(f"heartbeat: beat tag={tag}")  # the last line dates a death
            next_beat = time.time() + interval_s
        time.sleep(1.0)


# ---------------------------------------------------------------------------
# Git helpers for the main checkout
# ---------------------------------------------------------------------------


class RestoreError(Exception):
    """The main checkout could not be restored; the message is the reason."""


def git(repo: Path, *args: str, check: bool = False, timeout: float = 600) -> subprocess.CompletedProcess:
    proc = subprocess.run(
        ["git", "-C", str(repo), *args],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
    )
    if check and proc.returncode != 0:
        raise RestoreError(f"git {' '.join(args)} failed: {(proc.stderr or proc.stdout).strip()}")
    return proc


@dataclass
class StatusReport:
    stamps: list
    untracked_ok: list
    blocking: list

    @property
    def clean(self) -> bool:
        return not self.blocking


def classify_status(porcelain: str) -> StatusReport:
    """Split `git status --porcelain=v1` into allowed build stamps, tolerated
    untracked paths, and entries that block a restore (any other tracked
    change, a conflict, or an untracked path under a build input root)."""
    stamps, untracked_ok, blocking = [], [], []
    for line in porcelain.splitlines():
        if len(line) < 4:
            continue
        xy, path = line[:2], line[3:]
        if " -> " in path:
            path = path.split(" -> ", 1)[1]
        path = path.strip().strip('"')
        if xy == "??":
            if any(path.startswith(root) for root in BUILD_INPUT_ROOTS):
                blocking.append(line)
            else:
                untracked_ok.append(path)
        elif path in BUILD_STAMPS and "U" not in xy:
            stamps.append(path)
        else:
            blocking.append(line)
    return StatusReport(stamps, untracked_ok, blocking)


def checkout_status(repo: Path) -> StatusReport:
    proc = git(repo, "status", "--porcelain=v1", "--ignore-submodules=untracked")
    if proc.returncode != 0:
        raise RestoreError(f"git status failed in {repo}: {proc.stderr.strip()}")
    return classify_status(proc.stdout)


def abort_in_progress(repo: Path, log) -> None:
    """A verify killed mid-merge leaves MERGE_HEAD; abort it before anything else."""
    for marker, cmd in (("MERGE_HEAD", ("merge", "--abort")), ("CHERRY_PICK_HEAD", ("cherry-pick", "--abort"))):
        path = git(repo, "rev-parse", "--git-path", marker).stdout.strip()
        if path:
            full = Path(path) if Path(path).is_absolute() else repo / path
            if full.exists():
                log(f"checkout: aborting an interrupted {marker.split('_')[0].lower()} ({marker} present)")
                git(repo, *cmd, check=True)


def switch_checkout(repo: Path, args: tuple, log) -> None:
    proc = git(repo, *args)
    if proc.returncode == 0:
        return
    # A build stamp the target commit also changes blocks the switch. Stamps are
    # regenerated by the next build, so discarding them is safe; nothing else is.
    report = checkout_status(repo)
    if report.stamps and report.clean:
        log(f"checkout: discarding build stamps to switch: {', '.join(report.stamps)}")
        git(repo, "checkout", "--", *report.stamps, check=True)
        proc = git(repo, *args)
        if proc.returncode == 0:
            return
    raise RestoreError(f"git {' '.join(args)} failed: {(proc.stderr or proc.stdout).strip()}")


def restore_checkout(repo: Path, log, fetch: bool = True, branch: str = "main", remote: str = "origin") -> str:
    """Put the main checkout back on `branch`, fast-forwarded to remote/branch,
    submodules synced, with nothing but build stamps modified. Raises
    RestoreError with a reason instead of discarding anything that is not a
    build stamp. Returns the new HEAD as `<sha> <subject>`."""
    if not (repo / ".git").exists():
        raise RestoreError(f"{repo} is not a git checkout")
    abort_in_progress(repo, log)
    report = checkout_status(repo)
    if report.blocking:
        raise RestoreError(
            "uncommitted changes outside the build stamps (commit, move or discard them by hand): "
            + "; ".join(report.blocking)
        )
    current = git(repo, "rev-parse", "--abbrev-ref", "HEAD").stdout.strip()
    if current not in ("HEAD", branch):
        log(f"checkout: was on branch {current} (left intact); switching to {branch}")
    if fetch:
        git(repo, "fetch", "-q", "--prune", remote, check=True)
    switch_checkout(repo, ("checkout", "-q", branch), log)
    proc = git(repo, "merge", "--ff-only", "-q", f"{remote}/{branch}")
    if proc.returncode != 0:
        raise RestoreError(
            f"{branch} cannot fast-forward to {remote}/{branch} (local commits?): {(proc.stderr or proc.stdout).strip()}"
        )
    git(repo, "submodule", "update", "--init", "--recursive", "-q", check=True)
    report = checkout_status(repo)
    if report.blocking:
        raise RestoreError("checkout restored but still dirty: " + "; ".join(report.blocking))
    for path in report.untracked_ok:
        log(f"checkout: note: untracked {path} (outside the build inputs; tolerated)")
    return git(repo, "log", "-1", "--format=%H %s").stdout.strip()


# ---------------------------------------------------------------------------
# Staged archives
# ---------------------------------------------------------------------------

# The base archives every tier needs in the build root. A missing one turns
# its game's rows into skips or failures, never into a clear error.
REQUIRED_ARCHIVES = ("oot.o2r", "mm.o2r", "soh.o2r", "2ship.o2r")
# The curated cross-game halves; rows that need them SKIP (code 77) without.
CURATED_ARCHIVES = ("redship-oot.o2r", "redship-mm.o2r")


def check_archives(build: Path, label: str = "build-cmake") -> list:
    """FAIL lines for missing base archives (empty list = all present). A
    renamed-away archive is named, because that is how the #945 manual check
    left mm.o2r and turned every MM row into a skip."""
    problems = []
    for name in REQUIRED_ARCHIVES:
        path = build / name
        if path.is_file() and path.stat().st_size > 0:
            continue
        siblings = sorted(p.name for p in build.glob(f"{name}.*")) if build.is_dir() else []
        if siblings:
            hint = f"found {', '.join(siblings)} beside it: rename it back"
        else:
            hint = "regenerate with flag a, or copy it from games/oot or games/mm"
        state = "is empty" if path.is_file() else "is missing"
        problems.append(f"precondition: {label}/{name} {state} ({hint})")
    return problems


# ---------------------------------------------------------------------------
# Lane manifests
# ---------------------------------------------------------------------------

# The stage a lane has ENTERED. A lane rewrites its manifest at every change.
STAGES = ("queued", "implement", "review", "fix", "gate", "merged", "blocked", "abandoned")
DONE_STAGES = ("merged", "abandoned")
MANIFEST_FIELDS = ("key", "stage", "branch", "pr", "headSha", "pushed", "findingsPath", "worktree", "note", "updatedAt")


def manifest_file(state: Path, key: str) -> Path:
    if not re.fullmatch(r"[A-Za-z0-9._-]+", key or ""):
        raise ValueError(f"invalid lane key {key!r}: use letters, digits, '.', '_' or '-'")
    return lanes_path(state) / f"{key}.json"


def validate_manifest(m: dict) -> list:
    problems = []
    if m.get("stage") not in STAGES:
        problems.append(f"stage {m.get('stage')!r} is not one of {', '.join(STAGES)}")
    pr = m.get("pr")
    if pr not in (None, "") and not (isinstance(pr, int) and pr > 0):
        problems.append(f"pr {pr!r} is not a positive integer")
    sha = m.get("headSha")
    if sha not in (None, "") and not re.fullmatch(r"[0-9a-f]{40}", str(sha)):
        problems.append(f"headSha {sha!r} is not a full 40-hex commit id")
    if m.get("pushed") not in (None, True, False):
        problems.append(f"pushed {m.get('pushed')!r} is not true/false")
    if parse_iso(m.get("updatedAt")) is None:
        problems.append("updatedAt is missing or not ISO-8601")
    return problems


def read_manifest(state: Path, key: str) -> dict | None:
    path = manifest_file(state, key)
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return None


def write_manifest(state: Path, key: str, fields: dict) -> dict:
    """Merge `fields` over the existing manifest, stamp updatedAt, validate,
    write atomically. A key present in `fields` replaces the old value even
    when it is None (pushed=None means "unknown now", not "as before"); a key
    absent from `fields` keeps its old value. Raises ValueError on an invalid
    result."""
    path = manifest_file(state, key)
    current = read_manifest(state, key) or {}
    merged = {k: current.get(k) for k in MANIFEST_FIELDS if k in current}
    merged.update({k: v for k, v in fields.items() if k in MANIFEST_FIELDS})
    merged["key"] = key
    merged["updatedAt"] = iso()
    problems = validate_manifest(merged)
    if problems:
        raise ValueError(f"manifest {key}: " + "; ".join(problems))
    ordered = {k: merged.get(k) for k in MANIFEST_FIELDS if k in merged}
    path.parent.mkdir(parents=True, exist_ok=True)
    atomic_write(path, json.dumps(ordered, indent=2) + "\n")
    return ordered


def list_manifests(state: Path) -> list:
    """Every lanes/*.json, sorted by key. An unreadable file is returned as
    {'key': name, 'error': reason} so the table shows it instead of hiding it."""
    out = []
    folder = lanes_path(state)
    if not folder.is_dir():
        return out
    for path in sorted(folder.glob("*.json")):
        try:
            m = json.loads(path.read_text(encoding="utf-8"))
            if not isinstance(m, dict):
                raise ValueError("not a JSON object")
            m.setdefault("key", path.stem)
        except (OSError, ValueError) as exc:
            m = {"key": path.stem, "error": str(exc)}
        out.append(m)
    return out


class OriginIndex:
    """Answers "is this commit on origin?" against one repository: first by
    containment in any refs/remotes/<remote>/* ref (after a fetch), then by an
    exact match against the remote's branch heads and refs/pull/*/head from a
    single ls-remote (a squash-merged lane's branch may be deleted, but its PR
    ref still names the head)."""

    def __init__(self, repo: Path, remote: str = "origin", fetch: bool = True):
        self.repo = repo
        self.remote = remote
        self._remote_heads = None
        self.error = None
        if fetch:
            proc = git(repo, "fetch", "-q", "--prune", remote)
            if proc.returncode != 0:
                self.error = f"fetch failed: {proc.stderr.strip()}"

    def _ls_remote(self) -> dict:
        if self._remote_heads is None:
            self._remote_heads = {}
            proc = git(self.repo, "ls-remote", self.remote, "refs/heads/*", "refs/pull/*/head")
            if proc.returncode == 0:
                for line in proc.stdout.splitlines():
                    sha, _, ref = line.partition("\t")
                    self._remote_heads.setdefault(sha.strip(), []).append(ref.strip())
            else:
                self.error = f"ls-remote failed: {proc.stderr.strip()}"
        return self._remote_heads

    def contains(self, sha: str) -> bool | None:
        """True/False, or None when the answer is unknown (no sha, no git)."""
        if not sha:
            return None
        proc = git(
            self.repo,
            "for-each-ref",
            "--count=1",
            "--format=%(refname)",
            "--contains",
            sha,
            f"refs/remotes/{self.remote}",
        )
        if proc.returncode == 0 and proc.stdout.strip():
            return True
        return sha in self._ls_remote()


def resume_stage(m: dict, findings_exist=lambda p: Path(p).exists()) -> str:
    """Where a restarted workflow should pick this lane up."""
    if "error" in m:
        return "unreadable: rewrite it by hand"
    stage = m.get("stage")
    if stage in DONE_STAGES:
        return "done"
    if stage == "queued":
        return "implement (fresh)"
    if stage == "implement":
        return "implement (inherit worktree)" if m.get("branch") else "implement (fresh)"
    if stage == "review":
        return "review" if m.get("pr") else "implement (inherit: no PR recorded)"
    if stage == "fix":
        path = m.get("findingsPath")
        if path and findings_exist(path):
            return "fix (findings saved)"
        return "review (findings file missing: re-review)"
    if stage == "gate":
        return "gate (preflight swept the lock)"
    if stage == "blocked":
        return "blocked: orchestrator decides"
    return f"unknown stage {stage!r}"


def manifest_rows(manifests: list, origin: OriginIndex | None, now: datetime | None = None) -> list:
    """Table rows plus a flag per lane. A lane is flagged when its head is not
    on origin (and it is not done), or when its manifest is invalid."""
    now = now or utcnow()
    rows = []
    for m in manifests:
        flags = []
        if "error" in m:
            flags.append(f"UNREADABLE ({m['error']})")
        else:
            flags.extend(f"INVALID ({p})" for p in validate_manifest(m))
        sha = m.get("headSha") or ""
        on_origin = None
        if origin is not None and sha and m.get("stage") not in DONE_STAGES:
            on_origin = origin.contains(sha)
            if on_origin is False:
                claim = " (manifest says pushed)" if m.get("pushed") is True else ""
                flags.append(f"UNPUSHED: {sha[:10]} is not on origin{claim}")
        updated = parse_iso(m.get("updatedAt"))
        age = (now - updated).total_seconds() if updated else None
        shown = on_origin if on_origin is not None else m.get("pushed")
        rows.append(
            {
                "key": str(m.get("key", "?")),
                "stage": str(m.get("stage", "?")),
                "resume": resume_stage(m),
                "branch": str(m.get("branch") or ""),
                "pr": str(m.get("pr") or ""),
                "head": str(sha)[:10],
                "pushed": "yes" if shown is True else "NO" if shown is False else "?",
                "updated": age_text(age),
                "flags": flags,
            }
        )
    return rows


def format_table(rows: list) -> str:
    if not rows:
        return "(no lane manifests)"
    cols = ("key", "stage", "resume", "branch", "pr", "head", "pushed", "updated")
    width = {c: max(len(c), *(len(r[c]) for r in rows)) for c in cols}
    lines = ["  ".join(c.upper().ljust(width[c]) for c in cols).rstrip()]
    for r in rows:
        lines.append("  ".join(r[c].ljust(width[c]) for c in cols).rstrip())
        for flag in r["flags"]:
            lines.append(f"  !! {r['key']}: {flag}")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Small CLI helpers shared by the scripts
# ---------------------------------------------------------------------------


def split_args(argv: list) -> tuple[dict, list, list]:
    """key=value pairs, --flags (with optional =value), and bare positionals."""
    pairs, flags, positional = {}, [], []
    for arg in argv:
        if arg.startswith("--"):
            flags.append(arg)
        elif "=" in arg and re.match(r"^[A-Za-z][A-Za-z0-9_]*=", arg):
            key, _, value = arg.partition("=")
            pairs[key] = value
        else:
            positional.append(arg)
    return pairs, flags, positional


def flag_value(flags: list, name: str, default=None):
    for flag in flags:
        if flag == f"--{name}":
            return True
        if flag.startswith(f"--{name}="):
            return flag.split("=", 1)[1]
    return default


def log_line(msg: str, stream=None) -> None:
    print(msg, file=stream or sys.stdout, flush=True)
