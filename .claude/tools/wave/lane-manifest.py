#!/usr/bin/env python3
"""Per-lane resume manifests: <state>/lanes/<KEY>.json.

    lane-manifest.py write <KEY> stage=<STAGE> [branch=<B>] [pr=<N>]
                     [headSha=<SHA>] [findingsPath=<PATH>] [worktree=<DIR>]
                     [note=<TEXT>] [--repo=<DIR>] [--no-fetch] [--allow-unpushed]
    lane-manifest.py read [<KEY> ...] [--json] [--no-fetch]

Common options: --state-dir=<DIR> (default $WAVE_STATE_DIR, else
<$WAVE_MAIN>/.wave), --main=<DIR>.

A lane writes its manifest at EVERY stage change, after pushing. Stages:
queued, implement, review, fix, gate, merged, blocked, abandoned.

write merges the given fields over the existing manifest and stamps
updatedAt. headSha defaults to HEAD of --repo (default: the current
directory, i.e. the lane's worktree) when that checkout is on the lane's
branch; `pushed` is always computed (is headSha on origin?), never taken from
the caller. Exit 0 written; 3 written but the head is NOT on origin (push,
then write again; --allow-unpushed makes this 0); 2 usage or invalid field.

read prints a table (KEY, STAGE, RESUME AT, BRANCH, PR, HEAD, PUSHED,
UPDATED) and a `!!` line for every lane whose head is not on origin or whose
manifest is invalid. Exit 0 nothing flagged, 3 something flagged.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import wavelib as w  # noqa: E402

FIELDS = ("stage", "branch", "pr", "headSha", "findingsPath", "worktree", "note")


def _usage() -> int:
    print(__doc__.strip(), file=sys.stderr)
    return 2


def _repo_head(repo: Path) -> tuple[str | None, str | None]:
    """(HEAD sha, current branch or None when detached) of a checkout."""
    sha = w.git(repo, "rev-parse", "HEAD")
    if sha.returncode != 0:
        return None, None
    branch = w.git(repo, "rev-parse", "--abbrev-ref", "HEAD").stdout.strip()
    return sha.stdout.strip(), (None if branch in ("", "HEAD") else branch)


def cmd_write(pairs: dict, flags: list, positional: list) -> int:
    if len(positional) != 1:
        return _usage()
    key = positional[0]
    unknown = sorted(set(pairs) - set(FIELDS))
    if unknown:
        print(f"FAIL lane-manifest: unknown field(s) {', '.join(unknown)}; fields are {', '.join(FIELDS)}")
        return 2
    state = w.state_dir(w.flag_value(flags, "state-dir"), w.main_checkout(w.flag_value(flags, "main")))
    try:
        previous = w.read_manifest(state, key) or {}
    except ValueError as exc:
        print(f"FAIL lane-manifest: {exc}")
        return 2
    fields: dict = dict(pairs)
    if "pr" in fields:
        raw = fields["pr"].lstrip("#")
        if raw == "":
            fields["pr"] = None
        elif raw.isdigit():
            fields["pr"] = int(raw)
        else:
            print(f"FAIL lane-manifest: pr={fields['pr']!r} is not a number")
            return 2
    if fields.get("findingsPath"):
        fields["findingsPath"] = str(Path(fields["findingsPath"]).resolve())
    repo = Path(w.flag_value(flags, "repo") or ".")
    sha, current_branch = _repo_head(repo)
    branch = fields.get("branch") or previous.get("branch")
    if not fields.get("headSha"):
        if sha and (branch is None or current_branch == branch):
            fields["headSha"] = sha
            if branch is None and current_branch:
                fields["branch"] = current_branch
            if not fields.get("worktree") and not previous.get("worktree"):
                top = w.git(repo, "rev-parse", "--show-toplevel").stdout.strip()
                if top:
                    fields["worktree"] = top
        elif sha:
            print(
                f"note: {repo} is on {current_branch or 'a detached HEAD'}, not {branch}; "
                "headSha kept from the previous manifest (pass headSha= to set it)"
            )
    head = fields.get("headSha") or previous.get("headSha")
    pushed = None
    if head and sha is not None:
        origin = w.OriginIndex(repo, fetch=not w.flag_value(flags, "no-fetch"))
        pushed = origin.contains(head)
    fields["pushed"] = pushed
    try:
        manifest = w.write_manifest(state, key, fields)
    except ValueError as exc:
        print(f"FAIL lane-manifest: {exc}")
        return 2
    print(f"wrote {w.manifest_file(state, key)}: stage={manifest['stage']} head={str(manifest.get('headSha') or '')[:10]} "
          f"pushed={manifest.get('pushed')}")
    if pushed is False and manifest["stage"] not in w.DONE_STAGES:
        print(
            f"UNPUSHED lane-manifest: {head[:10]} is not on origin. A lane never ends a stage with unpushed "
            "commits: push, then write the manifest again."
        )
        return 0 if w.flag_value(flags, "allow-unpushed") else 3
    return 0


def cmd_read(pairs: dict, flags: list, positional: list) -> int:
    if pairs:
        return _usage()
    main = w.main_checkout(w.flag_value(flags, "main"))
    state = w.state_dir(w.flag_value(flags, "state-dir"), main)
    manifests = w.list_manifests(state)
    if positional:
        wanted = set(positional)
        manifests = [m for m in manifests if m.get("key") in wanted]
        missing = wanted - {m.get("key") for m in manifests}
        for key in sorted(missing):
            manifests.append({"key": key, "error": "no manifest"})
    origin = w.OriginIndex(main, fetch=not w.flag_value(flags, "no-fetch")) if (main / ".git").exists() else None
    rows = w.manifest_rows(manifests, origin)
    if w.flag_value(flags, "json"):
        print(json.dumps(rows, indent=2))
    else:
        print(w.format_table(rows))
        if origin is None:
            print(f"note: {main} is not a git checkout; heads were not checked against origin")
        elif origin.error:
            print(f"note: origin check incomplete ({origin.error})")
    return 3 if any(r["flags"] for r in rows) else 0


def main(argv: list | None = None) -> int:
    argv = sys.argv if argv is None else argv
    if len(argv) < 2 or argv[1] not in ("write", "read"):
        return _usage()
    pairs, flags, positional = w.split_args(argv[2:])
    return (cmd_write if argv[1] == "write" else cmd_read)(pairs, flags, positional)


if __name__ == "__main__":
    sys.exit(main())
