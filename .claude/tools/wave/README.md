# Wave toolkit

The checked-in tooling for a multi-lane wave: lanes implement in their own
worktrees, a reviewer and a fix pass follow, and a gate verifies the merged
combination in the main checkout under a lock before it squash-merges.

The 2026-09-30 wave kept all of this in a session scratchpad and lost it with
the session. Its post-mortem found four failures this toolkit closes:

- Four workflow deaths left no workflow running for about 27 hours.
- A gate killed mid-tier left a lock nothing could prove dead.
- Each recovery was a hand inventory that turned up unpushed commits.
- A gate ran with `build-cmake/mm.o2r` renamed away, so MM rows skipped.

So everything that has to survive a restart lives in the repository (the
scripts) or in the main checkout's gitignored `.wave/` directory (the state).
Nothing lives in a session scratchpad.

| File | What it is |
|---|---|
| `gate-lock.cmd`, `gate-unlock.cmd` | Take and release the gate merge lock (wrappers for `gatelock.py`) |
| `gatelock.py` | Lock CLI: `acquire`, `release`, `heartbeat`, `heartbeat-loop`, `status` |
| `verify.cmd`, `verify.py` | Merge a branch onto `origin/main` in the main checkout, build, run the tiers, write `verify-<tag>.log` |
| `preflight.py` | First node of every workflow: sweep a dead lock, restore the main checkout, print every lane's resume stage |
| `lane-manifest.py` | Per-lane resume manifest `lanes/<KEY>.json`: `write` at every stage change, `read` as a table |
| `wavelib.py` | The shared core all of the above call; the tests exercise it directly |
| `_python.cmd` | Finds Python for the wrappers (`%WAVE_PYTHON%`, then `py -3`, then `python`) |
| `tests/` | Unit tests; CI runs them in the `python-tests` job |

Worker-facing rules (triage, evidence, unpushed commits) are in
`.claude/worker-prompts.md`, "Triage and evidence rules".

## Where state lives

```
%WAVE_MAIN%                     main checkout (default C:/Users/penpen/src/redshipblueship)
└── .wave/                      %WAVE_STATE_DIR% (gitignored, machine-local)
    ├── gate.lock/owner.txt     the lock, while held
    ├── lock-history.log        one line per acquire / release / sweep, with the owner
    ├── lanes/<KEY>.json        one resume manifest per lane
    └── logs/
        ├── verify-<tag>.log    full output of the last verify for <tag> (.prev = the one before)
        └── heartbeat-<tag>.log one line per beat; the last line dates a death
```

Every script takes `--state-dir=` and `--main=` overrides. The tests and any
dry run point them at a temp directory and never touch the real tree.

## The gate lock

**Acquire.** `gate-lock.cmd tag=<TAG> pr=<N> run=<WORKFLOW RUN ID> [--wait=<MIN>]`

- The lock is the directory `gate.lock/`. `mkdir` is atomic, so exactly one
  caller wins.
- The winner writes `owner.txt`, one `key=value` per line:
  - `tag`, `pr`, `run`;
  - `pid` and `pidStarted`, the process's creation time, so a recycled pid is
    not mistaken for the holder;
  - `pidRole`, `host`, `acquired`;
  - `heartbeat`, refreshed while the holder works.
- `--wait` polls every 15 s. It sweeps a stale lock by the rule below, so a
  surviving gate is not stuck behind a dead one.
- Exit codes: 0 locked, 1 held (the line names the owner), 2 usage.

**Which pid.**

- At acquire, `pid` is the wrapper that called `gatelock.py`. It exits at
  once, which is honest: nothing long-lived holds the lock yet.
- `verify` registers its own pid (`pidRole=verify`) when it starts. It keeps
  it there while it runs, and leaves it behind when it exits.

So `pid` alive means a verify is running right now.

**Heartbeat.**

- `verify` starts a background `gatelock.py heartbeat-loop` process. It
  rewrites `heartbeat=` every 60 s and exits within a second of verify's
  death, even a `TerminateProcess` or a session restart.
- `verify` stops it at the end and writes one final beat, so the stale window
  starts when verify ends.
- A gate that will spend more than 5 minutes holding the lock outside verify
  (waiting on a slow `gh` call, say) refreshes it by hand:
  `py -3 .claude\tools\wave\gatelock.py heartbeat tag=<TAG>`.

**Release.** `gate-unlock.cmd tag=<TAG>`, or `owner=<TAG>`, or a bare `<TAG>`.

- It refuses a lock held by another tag.
- It refuses a lock whose `pid` is alive (a verify still running) unless you
  pass `--force`.
- `--force` is the orchestrator's tool for a process it has confirmed wedged.
  It is never a gate's.
- Releasing a free lock is a no-op that exits 0.
- Every release appends the old owner to `lock-history.log`.

**The stale rule.** One function (`wavelib.decide`) answers this for
preflight, `acquire --wait` and `gatelock.py status`. A lock is swept only
when BOTH of these hold:

- its heartbeat is older than 5 minutes (`WAVE_STALE_MIN`, or
  `preflight --stale-min`);
- AND its pid is dead.

| Heartbeat | pid | Verdict |
|---|---|---|
| fresh | any | `live`: held, leave it |
| stale | alive | `keep`: never swept automatically. A wedged verify is the orchestrator's call (`--force`) |
| stale | dead or absent | `stale`: swept, and logged with the reason |

The heartbeat falls back to `acquired=`, then to the directory's mtime.

- An acquirer killed between `mkdir` and writing `owner.txt` ages out the same
  way.
- Owner files in the wave-8 format (`tag=`/`owner=`, `pr=`, `time=`) are read
  too: no pid, so they are swept once 5 minutes old.

## The gate, step by step

Run the wave scripts from your own worktree, never from the main checkout.
`verify` checks out other commits in the main checkout while it runs, and
cmd.exe reads a batch file from disk line by line as it executes.

```bat
rem CI is green first. Then, under the lock:
.claude\tools\wave\gate-lock.cmd tag=K9 pr=901 run=wf_0adef3c3-c13 --wait=60
.claude\tools\wave\verify.cmd claude/k9-bag-composition K9 uia
rem   run it in the background, then read the summary lines it prints last
gh pr merge 901 --squash
py -3 .claude\tools\wave\preflight.py --held-by=K9 --no-manifests
rem   fetches and fast-forwards the main checkout to the merge, under your lock
git -C C:/Users/penpen/src/redshipblueship rev-parse HEAD^{tree}
rem   must equal the tree= in "VERIFY PASS ... tree=<T>": the lock is what
rem   guarantees main did not move between verify and merge
py -3 .claude\tools\wave\lane-manifest.py write K9 stage=merged
.claude\tools\wave\gate-unlock.cmd tag=K9
```

A red verify does not merge. The gate unlocks and the lane goes back to its
fix pass, with the `FAIL` lines as its findings.

## verify.cmd

```
verify.cmd <branch> <tag> [flags] [--dry-run] [--keep-going] [--main=<DIR>] [--state-dir=<DIR>] [--build-dir=<NAME>]
```

`<branch>` must be pushed (`origin/<branch>`). Pass `main` to verify
`origin/main` alone; the closing smoke is `verify.cmd main FINAL uia`. `<tag>`
must hold the lock. `[flags]` is any of these letters, or `-` for none:

| Flag | Adds |
|---|---|
| `u` | the `ui` tier, then the UI parity lint, both plain and `--no-grow-against origin/main` |
| `i` | the `integration` and `integration-soak` tiers, after the load wait below |
| `a` | archive regeneration after the build. Targets `ExtractAssets`, `ExtractMMAssets`, `GenerateSohOtr`, `Generate2ShipOtr`, `GenerateRedshipOtr`, in that order; then `oot.o2r`/`mm.o2r` are copied from `games/*` into `build-cmake` |

The `redship` and `rando` tiers always run. In order:

1. **Lock.** The lock must be held by `<tag>`, or verify exits 2 at once.
   Verify registers its pid and starts the heartbeat process.
2. **Preconditions, before anything is touched.** Each problem is one `FAIL
   precondition:` line:
   - `build-cmake/{oot,mm,soh,2ship}.o2r` must exist and be non-empty (skipped
     with `a`, which regenerates them). A renamed-away archive is named:
     `found mm.o2r.manual-bak beside it: rename it back`.
   - `build-cmake/shipofharkinian.json` must select OpenGL
     (`Window.Backend.Id 1`).
3. **Toolchain.** Verify loads `vcvars64.bat` itself (`WAVE_VCVARS`). Then it
   sets `VCPKG_ROOT` to the checkout's `vcpkg/`, because vcvars points it at
   Visual Studio's own. It sets `SCCACHE_CACHE_SIZE=40G` and puts CMake and
   sccache on `PATH`.
4. **Checkout.**
   - Abort an interrupted merge.
   - Refuse if anything but the four build stamps is modified.
   - `git fetch`, `checkout --detach origin/main`, `merge origin/<branch>`.
     A conflict aborts and names the files.
   - `submodule update`.
   - The log records `HEAD`, its tree and `origin/main`.
5. **Build:** `cmake --build build-cmake`. Failures quote the first error lines.
6. **Tiers.** Each runs `ctest -L ^<label>$ --output-on-failure --no-tests=error`
   with `SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN=1`. A tier is green only if all
   of these hold:
   - ctest exits 0;
   - it ran at least one row;
   - no row failed;
   - no row SKIPPED. Every skip path is archive-dependent, so a skip in the
     staged main tree means a missing input. Set `WAVE_ALLOW_SKIPS=1` to make
     a skip a warning instead.

   The row count comes from CTest's summary line. That line has two forms:
   `N% tests passed, F tests failed out of T` (CTest 3.x, and any run with a
   failure), and `100% tests passed out of T` (CTest 4.x when every row
   passes). Failed and skipped names come from the `The following tests
   FAILED:` and `did not run:` lists. CTest 4.x ends each FAILED row with that
   row's labels.

   Verify stops at the first red tier unless you pass `--keep-going`.
7. **Restore.** Always: `checkout main`, then fast-forward to `origin/main`.
   The build tree then holds the merged binary, and the next verify rebuilds
   anyway.
8. **Summary.** Every tier's line, the warnings, every `FAIL` line, then one of
   `VERIFY PASS tag=… branch=… tree=<T>` or `VERIFY FAIL …`. Exit 0 pass,
   1 fail, 2 usage or lock.

**Load wait before the integration tier.** The int-* rows are wall-clock
sensitive (#882).

- Verify waits up to 30 minutes (`WAVE_LOAD_WAIT_MIN`) until no other
  `ninja.exe` or `cl.exe` (`WAVE_LOAD_PROCS`) is running.
- It logs what it is waiting for when that changes, and at least every 5
  minutes. When the wait ends it logs what it waited for and how long.
- On timeout the tier runs anyway, marked `[under load]`, with a warning: a
  red int-* row from that run needs a quiet re-run before it counts.

**Timeouts.** The build is killed after `WAVE_BUILD_TIMEOUT_MIN` (120) and
each tier after `WAVE_TIER_TIMEOUT_MIN` (90), process tree and all. Without
them a hung run would keep the heartbeat, and so the lock, alive forever.

**BAD_COMMAND rows.** These mean Application Control blocked the fresh
binary. Verify prints a warning; relink for a new hash and re-run.

`--dry-run` prints every command it would run and changes nothing. It needs no
lock, no toolchain and no state directory.

## preflight.py

The first node of every workflow, before any lane starts:

```
py -3 .claude\tools\wave\preflight.py
```

1. **Lock.** Sweeps a lock by the stale rule and logs what it released and
   why. If a live gate holds the lock, preflight leaves both the lock and the
   checkout alone and says so (exit 0); re-run it after that gate unlocks.
2. **Checkout.** Takes the lock as `tag=preflight`, then:
   - aborts an interrupted merge;
   - refuses if a tracked file other than the four build stamps is modified,
     or an untracked file sits under a build input (`games/`, `src/`, `rsbs/`,
     `libultraship/`, …). Untracked droppings elsewhere (`.agent/`) are only
     noted. Preflight never discards anything but a build stamp, and only to
     let a checkout proceed;
   - `git checkout -q main`, fast-forwards to `origin/main` (refusing if local
     `main` has diverged) and syncs submodules;
   - checks again that `git status` shows only build stamps;
   - releases the lock.

   The four build stamps are `games/{mm,oot}/src/boot/build.c`,
   `games/mm/windows/properties.h` and `games/oot/properties.h`.
3. **Archives.** Warns if a base archive is missing from `build-cmake`.
4. **Lanes.** Prints the manifest table, and flags lanes to handle before
   resuming.

Exit 0 means the checkout is restored, or deliberately skipped because a live
gate holds it. Exit 1 means it cannot be restored: stop the workflow and
report the `FAIL preflight:` line, which names the reason. Exit 2 is a usage
error. A flagged lane does not change the exit code; the orchestrator decides.

`--held-by=<TAG>` is the gate's post-merge fast-forward: it skips the sweep and
restores under the caller's lock. `--no-restore`, `--no-manifests`,
`--no-fetch` and `--dry-run` do what they say.

## Lane manifests

A lane writes its manifest at **every** stage change, after pushing, from its
own worktree:

```
py -3 .claude\tools\wave\lane-manifest.py write K9 stage=review branch=claude/k9-bag-composition pr=901
py -3 .claude\tools\wave\lane-manifest.py write K9 stage=fix findingsPath=C:\...\k9-findings.md
py -3 .claude\tools\wave\lane-manifest.py read
```

`lanes/<KEY>.json` holds:

```json
{"key": "K9", "stage": "fix", "branch": "claude/k9-bag-composition", "pr": 901,
 "headSha": "<40 hex>", "pushed": true, "findingsPath": "C:/.../k9-findings.md",
 "worktree": "C:/.../worktrees/k9", "updatedAt": "2026-10-16T16:15:07Z"}
```

How `write` fills it:

- Fields you name replace the stored ones; fields you leave out are kept.
- `headSha` and `worktree` come from the current checkout when it is on the
  lane's branch. Otherwise the recorded head is kept, with a note saying so.
- `findingsPath` is stored absolute.
- `pushed` is always computed, never taken from you. It is true when
  `headSha` is contained in an `origin/*` ref, or names a remote branch head
  or `refs/pull/*/head` (a squash-merged lane's branch may be gone).
- Exit 3 means the manifest was written but the head is not on origin. Push,
  then write again. A lane never ends a stage with unpushed commits.

Stages are the stage the lane has entered. `read` and preflight print where a
restarted workflow picks the lane up:

| `stage` | Resume at |
|---|---|
| `queued` | implement (fresh) |
| `implement` | implement, inheriting the worktree (uncommitted drafts may be there) |
| `review` | review; or implement (inherit) when no PR is recorded |
| `fix` | fix, with the saved findings; or review again when the findings file is gone |
| `gate` | gate (preflight has already swept a dead gate's lock) |
| `merged`, `abandoned` | done (not checked against origin) |
| `blocked` | the orchestrator decides |

`read` flags each of these with a `!!` line:

- a head that is not on origin (`UNPUSHED`, noting when the manifest claimed
  it was pushed);
- an invalid manifest;
- an unreadable manifest.

It exits 3 when anything is flagged, else 0.

## Machine rules

- **One build slot while the operator uses the PC.** Implement, fix and gate
  agents share it; reviews and docs-only lanes run outside it. More slots only
  when the operator says so (overnight). The integration tier's load wait is a
  backstop for this rule, not a substitute.
- **Test windows never take focus.** This rule is unconditional.
  - Every ctest or redship run gets `SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN=1`.
    `verify` sets it for its children, and so does `verify.cmd`.
  - It works only on the OpenGL backend: DXGI ignores the hint. That is why
    verify refuses a staged config that does not select OpenGL.
- **The lock covers every merge.** Main can only move under the lock, which is
  what makes the tree comparison after the merge meaningful. The docs-only
  fast path merges under the lock too.
- **Never a skip in the main tree.** A verify with a skip is red unless
  someone set `WAVE_ALLOW_SKIPS=1` and said why.

## Recovery after a death

A session restart, a bugcheck or a quota kill:

1. Run `preflight.py`.
   - It sweeps the dead gate's lock once its heartbeat is 5 minutes old.
   - It restores the checkout, or tells you exactly why it cannot.
   - It prints every lane's resume stage.
2. Handle each `!!` line before relaunching. An `UNPUSHED` head usually means
   a lane worktree with local commits; push them from that worktree, or
   record why not.
3. Relaunch the workflow with each lane at the stage the table names.

## Environment

| Variable | Default | Used by |
|---|---|---|
| `WAVE_MAIN` | `C:/Users/penpen/src/redshipblueship` | everything: the main checkout |
| `WAVE_STATE_DIR` | `%WAVE_MAIN%/.wave` | everything: lock, manifests, logs |
| `WAVE_STALE_MIN` | `5` | stale rule |
| `WAVE_PYTHON` | (`py -3`, then `python`) | the `.cmd` wrappers |
| `WAVE_HEARTBEAT_S` | `60` | verify's heartbeat process |
| `WAVE_VCVARS` | VS 2022 BuildTools `vcvars64.bat` | verify |
| `WAVE_VCPKG_ROOT` | `%WAVE_MAIN%/vcpkg` | verify |
| `WAVE_CMAKE_DIR` | `C:\Program Files\CMake\bin` | verify |
| `WAVE_SCCACHE_DIR` | found on `PATH` or under WinGet's packages | verify |
| `WAVE_LOAD_WAIT_MIN` / `WAVE_LOAD_PROCS` | `30` / `ninja.exe,cl.exe` | verify's load wait |
| `WAVE_BUILD_TIMEOUT_MIN` / `WAVE_TIER_TIMEOUT_MIN` | `120` / `90` | verify |
| `WAVE_ALLOW_SKIPS` | unset | verify |

## Tests

```bash
python -m pytest .claude/tools/wave/tests/ -v
python -m unittest discover -s .claude/tools/wave/tests -t .claude/tools/wave/tests
```

The tests need git and Python 3.11+, and nothing else: no build, no ROMs, no
Windows. They run against temp directories and throwaway git repositories (a
bare origin plus a clone that stands in for the main checkout). CI runs them
in the `python-tests` job beside `tools/tests/`.

`python .claude/tools/wave/tests/test_verify.py` runs verify's checks on its
own, in a few seconds. Those checks include the CTest parser tests, which use
CTest 3.x and 4.4 output captured verbatim.
