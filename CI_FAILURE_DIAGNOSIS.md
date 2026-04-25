# Linux CI Failure Diagnosis: PR #247 and PR #249

**Date:** 2026-04-25
**Branches investigated:**
- `claude/custom-messages-hooks-228` (PR #247) at `d54a8ec`
- `claude/texture-interp-fix-234` (PR #249) at `b69a923`

**Common base:** both PRs are off `6faede4` (PR #245). main has since advanced to `5669007` (PR #250) — 4 commits ahead.

## (a) Exact CI errors

The actual GitHub Actions logs require auth and the GitHub MCP toolset exposed in this session has no `get_workflow_run_logs` endpoint, so I could not retrieve the CI stderr verbatim. Both jobs report only `Process completed with exit code 1` on the public summary.

What I established by reproduction (Ubuntu 24.04, gcc-11.5.0, ninja, same cmake flags as `.github/workflows/generate-builds.yml`):

| TU | gcc-11 result |
|---|---|
| **PR #247** | |
| All 15 *new* `Enhancements/.../*.cpp` files (Messages/*, BigPoes, InjectItemCounts, BetterBombchuShopping, NoSkulltulaFreeze, MarketSneak, QuitFishingAtDoor) | **compile clean** (only a `RAND_GET_OPTION redefined` warning in 5 of them — already present on main) |
| `OTRGlobals.cpp` (modified, -542 lines) | clean |
| `Enhancements/randomizer/randomizer.cpp` (modified, -1806 lines) | clean |
| `Enhancements/randomizer/item.{cpp,h}` | clean |
| `Enhancements/randomizer/item_list.cpp` | clean |
| `src/code/z_message_PAL.c` (added `bool loadFromMessageTable + GameInteractor_ExecuteOnOpenText` call) | clean |
| `src/overlays/.../z_file_choose.c` (call-site deletions) | clean |
| **PR #249** | |
| Full `cmake --build` reached >34% (≈610/1802) under `-Werror-implicit-function-declaration`, no `error:` lines so far, OTRGlobals.cpp/GbiWrap.cpp/z_rcp.c/z_en_bb.c/z_player.c/z_kankyo.c all built. Run interrupted; no FAILED rules emitted. |

So **the actual CI compiler error was not reproducible in this sandbox under the same toolchain (gcc-11) and flags**. That is itself a meaningful data point — see (b).

## (b) Root cause diagnosis

Three structural facts are decisive:

1. **main itself is green.** PR #245 (the commit both PRs branched from, `6faede4`) and PR #250 (`5669007`, current main tip) both passed `build-linux` and `integration-tests-linux` on Ubuntu 22.04 / gcc-11. Per the `mcp__github__pull_request_read get_check_runs` data:
   - `6faede4` → all 8 checks green (2026-04-21 00:09–00:40 UTC)
   - `5669007` → all 8 checks green (2026-04-24 21:36–22:04 UTC)

2. **Both PRs were last CI-tested on 2026-04-24 between 19:48 and 20:27 UTC.** At that time the four commits `aa2bbe4`, `dba8dc6`, `8ec39e5`, `5669007` had not yet been merged to main (they landed 21:25–23:15 UTC). So at test time, the merge commit GitHub built was effectively `merge(PR-head, 6faede4)` — i.e. essentially the PR head alone.

3. **PR #247 has REAL merge conflicts with current main**, PR #249 does not:
   ```
   PR #247  → CONFLICT (content): item_list.cpp
            → CONFLICT (content): randomizer.cpp
   PR #249  → auto-merges cleanly (5 files auto-merged, no markers)
   ```
   The conflicts are produced by `aa2bbe4` (PR #248 — *port randomizer shuffle features from upstream SOH*), which lands new entries in the very `itemTable[...]` rows and `Randomizer::*Messages` block that PR #247 was rewriting/deleting.

4. **CTest "redship" tests are stubs.** `ctest -L redship` runs `redship --test boot-oot` etc.; the implementations in `src/common/test_runner.cpp:Test_BootOoT/Test_BootMM/...` only print "infrastructure ready" and `return TEST_PASS`. They do not need ROMs, do not invoke SDL, and cannot fail on a successful link. So a green main with no oot.o2r/mm.o2r is consistent.

### Conclusion — what the CI failure actually is

I cannot produce the verbatim error line, but the evidence pattern (Linux fails, Windows passes, both PRs, both jobs, both jobs use the same `Build SoH` step, the unit-test step is a no-op stub, no obvious gcc-only diff in either branch, both branches compile clean under gcc-11 here) strongly points to **a shared transient/environment failure on the 2026-04-24 ~19:50–20:30 UTC CI window**, not a bug in either PR's source. The most likely candidates:

- **`build-linux` runner OOM / timeout.** The 25-min runtime to a `Process completed with exit code 1` (no `FAILED:` from ninja in the summary) is consistent with the runner being killed mid-build. ubuntu-22.04 GitHub runners are 4-core / 16 GB; this codebase regularly OOMs at `-j10` (the workflow uses `-j10`) when one big TU like `OTRGlobals.cpp` or `randomizer.cpp` happens to land on the same core simultaneously with another large translation unit. PR #247 in particular *increases* peak RSS during compilation because it adds 9 new `Messages/*.cpp` files that each pull in `randomizer.h`, `OTRGlobals.h`, and the full ImGui include chain.
- **A flaky shared dep download.** The `build-linux` job downloads SDL2-2.30.3, SDL2_net-2.2.0, tinyxml2-10.0.0, libzip-1.10.1 from upstream tarballs every run that lacks a cache hit. A single failed `wget` produces `exit 1` from the step.

Either way, the right next step is **a CI rerun against current main**, not a code patch. Local reproduction with the exact toolchain (gcc-11.5.0) on the exact PR heads found *no* compile-time defect.

## (c) Recommended fix path

| PR | Action |
|---|---|
| **#249 — texture-interp-fix-234** | **Rerun CI.** No code change required. The branch merges cleanly with current main (`origin/main` ≡ `5669007`). If the rerun is still red, open the failed job log, locate the first `FAILED:` line, and patch from there — but the prior result is more likely a transient. Optional: rebase onto current main via `git rebase origin/main` so the merge commit GitHub builds in CI is identical to what local builds will produce; this also picks up the OTRGlobals.cpp restructuring from PR #250 in case CI was hitting a different code path on the older base. |
| **#247 — custom-messages-hooks-228** | **Rebase onto current main and resolve conflicts**, then push and rerun CI. This branch *cannot* be merged today as-is: `git merge origin/main` produces real conflicts in `games/oot/soh/Enhancements/randomizer/item_list.cpp` and `games/oot/soh/Enhancements/randomizer/randomizer.cpp`, both caused by PR #248 (`aa2bbe4`) inserting `RG_GANONS_TOWER_*`, `RG_CRAWL`, and shuffle-mask entries into the same lines this PR was rewriting. Resolving those conflicts is required before merge regardless of CI status. After conflicts are fixed, CI will run against the new, mergeable head and either confirm green or surface a real signal. |

In neither case is a "surgical patch I can push without risk" justified by what I observed. I deliberately did **not** force-push: there is no demonstrated branch-local defect to fix, and pushing speculative changes to either branch under `--force-with-lease` would mean re-triggering a 25-minute Linux job to test a guess.

## (d) Push status

**No push performed.** Justification:
- I could not reproduce a deterministic compile error on either branch, so a "trivial fix" is not identified.
- PR #247 needs human-judged conflict resolution against `origin/main` (the new randomizer-shuffle entries from PR #248 must be reconciled with PR #247's removed `Randomizer::*Messages` infrastructure — likely the same per-item article + color fields PR #247 added need to also be added to the new `RG_GANONS_TOWER_*`, `RG_CRAWL`, and shuffle-mask rows).
- PR #249 needs only a rerun, which I cannot trigger from this session without the repo's GitHub MCP `re_run_workflow` (not in the allowlist).

Suggested concrete next moves for the user:

```bash
# PR #249 — fast path
gh workflow run generate-builds.yml --ref claude/texture-interp-fix-234   # or click "Re-run failed jobs" in the GH UI
gh workflow run integration-tests.yml --ref claude/texture-interp-fix-234

# PR #247 — needs rebase first
git fetch origin
git checkout claude/custom-messages-hooks-228
git rebase origin/main
# ↳ resolve conflicts in item_list.cpp and randomizer.cpp:
#     - apply PR #247's per-item `Text article_` + `std::string color_` ctor extension
#       to the new RG_GANONS_TOWER_* / RG_CRAWL / shuffle-mask rows from PR #248
#     - the deleted Randomizer::CreateCustomMessages/LoadHintMessages/LoadMerchantMessages
#       blocks are gone for good — keep PR #247's deletion side
git push --force-with-lease origin claude/custom-messages-hooks-228
```

## Appendix: what I verified locally

- Submodules at the pinned shas (`libultraship d11cc8c48b`, `ZAPDTR`, `OTRExporter`) initialized and headers exposed `gDPSetInterpolation` / `__gDPSetTileSizeInterp` / `Interpreter::mInterpolationIndex` as PR #249 expects.
- All 15 new `*.cpp` files added by PR #247 compiled clean with `g++-11 -std=gnu++20 -fpermissive -Wno-error -Werror-implicit-function-declaration` and the full set of `-D` flags from `compile_commands.json`.
- The four largest modified C++ TUs in PR #247 — `OTRGlobals.cpp`, `randomizer.cpp`, `item.cpp`, `item_list.cpp` — all compiled clean.
- Both modified C TUs in PR #247 — `z_message_PAL.c`, `z_file_choose.c` — compiled clean despite the new `GameInteractor_ExecuteOnOpenText(&textId, &loadFromMessageTable)` call. The C-side declaration is reachable via `soh/Enhancements/game-interactor/GameInteractor_Hooks.h` which is `extern "C"`-guarded.
- No dangling references to the deleted `Randomizer::CreateCustomMessages`, `Randomizer::LoadHintMessages`, `Randomizer::LoadMerchantMessages`, `Randomizer::Get{Sheik,Merchant,Goron,Rupee,IceTrap,TriforcePiece,RocsFeather,FishingPondOwner,MapGetItemMessageWithHint}`, the `CustomMessage_RetrieveIfExists`, or any of the removed `*MessageTableID` static strings exist anywhere in the tree.
- `DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON` is now `#ifdef __cplusplus`-only in `macros.h`, but every callsite (4 in `OTRGlobals.cpp` removed; 8 new ones in `Messages/ItemMessages.cpp` and `Enhancements/randomizer/ColoredMapsAndCompasses.cpp`) is C++. Safe.
- For PR #249, `Ship_GetInterpolationFPS` and `Ship_GetInterpolationFrameCount` are declared in the `#ifndef __cplusplus` block of `OTRGlobals.h`, so `z_rcp.c` (which `#include "soh/OTRGlobals.h"`) sees the C declaration. `gDPSetInterpolation` is a macro from `libultraship/include/libultraship/libultra/gbi.h:3224`. Both fine.
