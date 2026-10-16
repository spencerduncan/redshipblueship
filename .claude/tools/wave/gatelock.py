#!/usr/bin/env python3
"""Gate lock CLI: the merge lock every gate holds from verify to unlock.

    gatelock.py acquire  tag=<TAG> [pr=<N>] [run=<WORKFLOW RUN ID>] [pid=<PID>] [--wait=<MIN>]
    gatelock.py release  tag=<TAG> | owner=<TAG> | <TAG>  [--force]
    gatelock.py heartbeat tag=<TAG> [pid=<PID>] [role=<ROLE>]
    gatelock.py heartbeat-loop tag=<TAG> --watch-pid=<PID> [--interval=<SEC>]
    gatelock.py status

Common options: --state-dir=<DIR> (default $WAVE_STATE_DIR, else
<$WAVE_MAIN>/.wave), --dry-run (report, change nothing).

gate-lock.cmd and gate-unlock.cmd are the Windows wrappers for acquire and
release. Exit codes: 0 done, 1 refused (held / pid alive / tag mismatch),
2 usage error. The protocol is in README.md.
"""

from __future__ import annotations

import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import wavelib as w  # noqa: E402


def _state(flags: list) -> Path:
    return w.state_dir(w.flag_value(flags, "state-dir"))


def _usage() -> int:
    print(__doc__.strip(), file=sys.stderr)
    return 2


def cmd_acquire(pairs: dict, flags: list, positional: list) -> int:
    tag = pairs.get("tag") or pairs.get("owner") or (positional[0] if positional else "")
    if not tag:
        print("FAIL gate-lock: tag=<TAG> is required", file=sys.stderr)
        return 2
    state = _state(flags)
    try:
        wait_min = float(w.flag_value(flags, "wait", "0") or 0)
    except ValueError:
        print("FAIL gate-lock: --wait takes minutes", file=sys.stderr)
        return 2
    pid = int(pairs["pid"]) if pairs.get("pid", "").isdigit() else None
    if w.flag_value(flags, "dry-run"):
        decision = w.lock_status(state)
        print(f"dry-run: lock directory {w.lock_path(state)}")
        print(f"dry-run: current state {decision.verdict}: {decision.reason} {w.describe_owner(decision.owner)}")
        print(
            f"dry-run: would write tag={tag} pr={pairs.get('pr', '')} run={pairs.get('run', '')} "
            f"pid={pid if pid is not None else os.getppid()} pidRole=gate-lock acquired=<now> heartbeat=<now>"
        )
        return 0
    try:
        owner = w.acquire(
            state,
            tag,
            pr=pairs.get("pr", ""),
            run=pairs.get("run", ""),
            pid=pid,
            wait_s=wait_min * 60.0,
            log=lambda m: print(f"gate-lock: {m}", flush=True),
        )
    except w.LockError as exc:
        print(f"HELD gate-lock: {exc}")
        return 1
    print(f"LOCKED {w.describe_owner(owner)} dir={w.lock_path(state)}")
    return 0


def cmd_release(pairs: dict, flags: list, positional: list) -> int:
    tag = pairs.get("tag") or pairs.get("owner") or (positional[0] if positional else None)
    force = bool(w.flag_value(flags, "force"))
    state = _state(flags)
    if w.flag_value(flags, "dry-run"):
        exists, owner, _ = w.read_lock(state)
        if not exists:
            print("dry-run: gate lock is free; nothing to release")
            return 0
        pid = w.owner_pid(owner)
        alive = bool(pid) and w.pid_alive(pid, (owner or {}).get("pidStarted") or None)
        verdict = "would release"
        if not force and (owner or {}).get("tag") not in (None, tag):
            verdict = "would REFUSE (tag mismatch)"
        elif not force and alive:
            verdict = f"would REFUSE (pid {pid} alive)"
        print(f"dry-run: {verdict}: {w.describe_owner(owner)}")
        return 0
    try:
        print(w.release(state, tag, force=force, by=f"gate-unlock:{tag or 'force'}"))
    except w.LockError as exc:
        print(f"REFUSED gate-unlock: {exc}")
        return 1
    return 0


def cmd_heartbeat(pairs: dict, flags: list, positional: list) -> int:
    tag = pairs.get("tag") or (positional[0] if positional else "")
    if not tag:
        return _usage()
    pid = int(pairs["pid"]) if pairs.get("pid", "").isdigit() else None
    if w.touch_heartbeat(_state(flags), tag, pid=pid, pid_role=pairs.get("role")):
        print(f"heartbeat tag={tag} {w.iso()}")
        return 0
    print(f"FAIL heartbeat: the gate lock is not held by tag={tag}")
    return 1


def cmd_heartbeat_loop(pairs: dict, flags: list, positional: list) -> int:
    tag = pairs.get("tag") or (positional[0] if positional else "")
    watch = w.flag_value(flags, "watch-pid")
    if not tag or not watch or not str(watch).isdigit():
        return _usage()
    interval = float(w.flag_value(flags, "interval", "60") or 60)
    return w.heartbeat_loop(
        _state(flags),
        tag,
        int(watch),
        interval_s=interval,
        log=lambda m: print(f"{w.iso()} {m}", flush=True),
    )


def cmd_status(pairs: dict, flags: list, positional: list) -> int:
    state = _state(flags)
    decision = w.lock_status(state)
    print(f"gate lock {w.lock_path(state)}: {decision.verdict} ({decision.reason})")
    if decision.owner:
        print(w.format_owner(decision.owner).rstrip())
    return 0


COMMANDS = {
    "acquire": cmd_acquire,
    "release": cmd_release,
    "heartbeat": cmd_heartbeat,
    "heartbeat-loop": cmd_heartbeat_loop,
    "status": cmd_status,
}


def main(argv: list | None = None) -> int:
    argv = sys.argv if argv is None else argv
    if len(argv) < 2 or argv[1] not in COMMANDS:
        return _usage()
    pairs, flags, positional = w.split_args(argv[2:])
    return COMMANDS[argv[1]](pairs, flags, positional)


if __name__ == "__main__":
    sys.exit(main())
