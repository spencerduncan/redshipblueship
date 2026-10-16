#!/usr/bin/env python3
"""Workflow preflight: the first node of every wave workflow.

    preflight.py [--main=<DIR>] [--state-dir=<DIR>] [--stale-min=<N>]
                 [--held-by=<TAG>] [--no-restore] [--no-manifests] [--no-fetch]
                 [--dry-run]

1. Gate lock: releases it if its heartbeat is older than --stale-min (default
   $WAVE_STALE_MIN or 5) AND its recorded pid is dead. A lock that is still
   held (fresh heartbeat, or a live pid) is left alone, and then step 2 is
   skipped: the checkout belongs to that gate.
2. Main checkout: under the lock (tag=preflight), aborts an interrupted merge,
   refuses if anything but the four build stamps is modified, then
   `git checkout -q main`, fast-forwards to origin/main, syncs submodules and
   re-checks that `git status` shows only build stamps.
3. Archives: warns about a missing base archive in build-cmake (verify fails
   on it; preflight only reports).
4. Lanes: prints every lanes/*.json manifest with the stage a restarted
   workflow should resume at, and flags heads that are not on origin.

--held-by=<TAG> is for a gate that already holds the lock (the fast-forward
after its merge): it skips the sweep and restores under the caller's lock.

Exit 0 = checkout restored (or deliberately skipped because a live gate holds
it), 1 = the checkout cannot be restored (the FAIL line says why),
2 = usage. Unpushed lanes are flagged but do not change the exit code.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import wavelib as w  # noqa: E402

OPTIONS = ("main", "state-dir", "stale-min", "held-by", "no-restore", "no-manifests", "no-fetch", "dry-run")


def run(opts: dict, log=w.log_line) -> int:
    main = w.main_checkout(opts.get("main"))
    state = w.state_dir(opts.get("state-dir"), main)
    dry = bool(opts.get("dry-run"))
    try:
        stale_s = float(opts["stale-min"]) * 60.0 if opts.get("stale-min") else w.stale_after_s()
    except ValueError:
        log("FAIL preflight: --stale-min takes minutes")
        return 2
    log(f"preflight: main={main} state={state} stale-after={w.age_text(stale_s)}{' DRY RUN' if dry else ''}")

    restore = not opts.get("no-restore")
    took_lock = False
    held_by = opts.get("held-by")
    if held_by:
        decision = w.lock_status(state, stale_s=stale_s)
        if (decision.owner or {}).get("tag") != held_by:
            log(f"FAIL preflight: --held-by={held_by} but the lock is {decision.verdict} {w.describe_owner(decision.owner)}")
            return 1
        log(f"lock: held by the caller ({w.describe_owner(decision.owner)})")
    else:
        if dry:
            decision = w.lock_status(state, stale_s=stale_s)
            if decision.verdict == w.STALE:
                log(f"lock: would RELEASE stale lock ({decision.reason}): {w.describe_owner(decision.owner)}")
        else:
            decision = w.sweep_stale(state, by="preflight", stale_s=stale_s)
            if decision.verdict == w.STALE:
                log(f"lock: RELEASED stale lock ({decision.reason}): {w.describe_owner(decision.owner)}")
        if decision.verdict == w.FREE:
            log("lock: free")
        elif decision.verdict in (w.LIVE, w.KEEP):
            log(f"lock: HELD [{decision.verdict}: {decision.reason}] {w.describe_owner(decision.owner)}")
            if restore:
                log("checkout: SKIPPED (a live gate owns the main checkout; re-run preflight after it unlocks)")
            restore = False
        if restore and not dry:
            try:
                w.acquire(state, "preflight", pid=os.getpid(), pid_role="preflight", stale_s=stale_s)
                took_lock = True
            except w.LockError as exc:
                log(f"checkout: SKIPPED (lost the race for the lock: {exc})")
                restore = False

    rc = 0
    try:
        if restore:
            if dry:
                log(f"checkout: would abort any interrupted merge, require only build stamps modified, "
                    f"checkout main, fast-forward to origin/main and sync submodules in {main}")
                if (main / ".git").exists():
                    report = w.checkout_status(main)
                    for line in report.blocking:
                        log(f"checkout: would FAIL on: {line}")
            else:
                head = w.restore_checkout(main, log, fetch=not opts.get("no-fetch"))
                log(f"checkout: OK main at {head}")
    except w.RestoreError as exc:
        log(f"FAIL preflight: cannot restore {main}: {exc}")
        rc = 1
    finally:
        if took_lock:
            try:
                w.release(state, "preflight", by="preflight", self_pids=(os.getpid(),))
            except w.LockError as exc:
                log(f"WARN preflight: could not release its own lock: {exc}")

    build = main / "build-cmake"
    if build.is_dir():
        problems = w.check_archives(build)
        for problem in problems:
            log(f"WARN {problem}")
        if not problems:
            log("archives: build-cmake has " + ", ".join(w.REQUIRED_ARCHIVES))
    else:
        log(f"WARN archives: {build} does not exist")

    if not opts.get("no-manifests"):
        origin = None
        if (main / ".git").exists():
            origin = w.OriginIndex(main, fetch=not opts.get("no-fetch") and not dry)
        rows = w.manifest_rows(w.list_manifests(state), origin)
        log(f"lanes: {w.lanes_path(state)}")
        log(w.format_table(rows))
        flagged = [r["key"] for r in rows if r["flags"]]
        if flagged:
            log(f"WARN lanes need attention before resuming: {', '.join(flagged)}")
        if origin is not None and origin.error:
            log(f"WARN lanes: origin check incomplete ({origin.error})")
    return rc


def main(argv: list | None = None) -> int:
    argv = sys.argv if argv is None else argv
    if len(argv) > 1 and argv[1] in ("-h", "--help"):
        print(__doc__.strip())
        return 0
    pairs, flags, positional = w.split_args(argv[1:])
    if pairs or positional:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    opts = {}
    for flag in flags:
        name = flag[2:].split("=", 1)[0]
        if name not in OPTIONS:
            print(f"FAIL preflight: unknown option {flag}", file=sys.stderr)
            return 2
        opts[name] = w.flag_value(flags, name)
    return run(opts)


if __name__ == "__main__":
    sys.exit(main())
