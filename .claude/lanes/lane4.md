You are Lane 4 of six on RedShipBlueShip Phase 3.1 (#492): the **settings and menu surface**. It is unblocked today, contrary to earlier planning — but two of your steps reach into other lanes' files, so read the asks below before starting.

Read #492 (including the correction comment on #451), then #497, then #499, then `docs/adr/0004-menu-information-architecture.md` (Status: Proposed) and `docs/adr/0003-settings-namespace.md` (also Proposed).

## Decide the scope before writing anything

#497 offers two scopes "roughly an order of magnitude" apart and flags it as an open maintainer call:

- **ADR 0004 §4.1 minimum** (`:186-189`) — a read-only "Paired" summary plus an MM logic-mode picker inside the existing OoT rando pane.
- **The full 46-option MM pane.**

Pick one and record it on #497 before step 1. Do not discover this mid-implementation.

## The #451 premise, stated correctly

The four `kMenuIndexKeys` (`src/common/cvar_shared_keys.h:390-394`) have **no compiled MM-side reader** — MM's readers are `BenGui/BenMenu.cpp:298,684,1792` (a TU in no CMake target) and `BenGui/Menu.cpp:599` (in the elided `2ship_rando_ui`). OoT's SohMenu *does* read all four from the live link (`SohMenuSettings.cpp:127`, `SohMenuEnhancements.cpp:148`, `SohMenuDevTools.cpp:35`, `Menu.cpp:568`).

So #451 is a *two shells indexing one key* hazard whose arming condition is a **second, MM-side** shell entering the link — not any reader. The `2ship_enh` WHOLE_ARCHIVE flip does not cause that, and MM's option table (`Rando::StaticData::Options`) is already linked via WHOLE_ARCHIVE'd `2ship_rando`. Take ADR 0004's route — extend the live SohMenu — rather than reviving BenMenu.

**This distinction constrains your own work:** `SohMenuEnhancements.cpp` is both a linked reader of one of these keys *and* a file you will edit. #497 step 2's invariant ("assert every reader of a `kMenuIndexKeys` entry is on a not-linked allowlist") is RED at head as literally worded. Scope it over MM-side readers.

## Three hazards that will bite silently

- **Hints are hard-blocked by #438.** All six MM hint families register on `OnOpenText` (`EnGs.cpp:73,84`; `EnSsh.cpp:52-57`; `EnZow.cpp:27-35`; `EnTalk.cpp:68`; `DmStk.cpp:116`), which has no MM dispatch point. `RO_SHUFFLE_SHOPS` and the `OnActorKill` leg of `RO_SHUFFLE_ENEMY_DROPS` are likewise blocked. You are not blocked on #438 for the **IA**; you very much are for the **hints** half of #499.
- **Half-armed options are worse than off.** Enabling an option whose hook type is dormant widens the pool (checks become `shuffled` via `Logic/GeneratePools.cpp:60-130`) but the behaviour never arms — items sit on checks the game cannot award. Audit every row you enable against #438's dispatch table.
- **A generation throw silently reverts the world to vanilla** with no retry (`Foreign.cpp:20-27`, `OnFileCreate.cpp:293-307`) and the player just sees vanilla MM. Measure dead-end rates before shipping a raised profile.

Also unresolved and blocking anything that iterates the option id space: `RO_ACCESS_MAJORA_REMAINS` is declared at `Rando/Types.h:2875` with no row in `Options.cpp` — 47 ids, 46 rows. Both #497 step 4 and #499 step 5 need this settled first.

**Timing constraint** from #499: on the real cross-game path the profile is snapshotted when `MM_Rando_PairOnCrossGameArrival` dispatches `OnSaveInit` at arrival (`GameExports_SingleExe.cpp:1819,1857`), and an existing MM save is never regenerated (`:1832-1848`). Any chooser must therefore be reachable **while OoT is active, before the switch**. That constrains where the pane can live.

## Two cross-lane asks you must make before you start

- **`src/common/context.h`** (Lane 1). #497 step 7 and #499 step 4 both carve an MM-profile digest from the front of `reserved[]`, and #499's Tier-1 lock assertion (b) does not compile until it lands. Lane 1 owns `context.h` and is budgeting `reserved[264]` across five claimants — **give Lane 1 your digest size early** so it lands in one format version rather than a second re-versioning.
- **`games/mm/2s2h/Rando/Foreign.cpp` / `Foreign.h`** (Lane 2, until #488 merges). #499 step 2 extracts the `OnFileCreate.cpp:112-120` loop and the `:132-137` pins into a callable `Rando::ResolvePairedProfile()` there, and your Tier-1 lock drives that function directly. Lane 2's claim ends when #488 merges; agree the handoff explicitly.

Lane 2 also needs `MiscBehavior/OnFileCreate.cpp:245` (#488 step 5, a throw inside the existing try) — a different region of a file you own. Agree an order with Lane 2.

## Files you own

`games/oot/soh/SohGui/*` (including `SohMenuRandomizer.cpp`, the likeliest edit target, and `MenuTypes.h`), `docs/adr/0004-menu-information-architecture.md`, `docs/adr/0003-settings-namespace.md`, `src/common/cvar_shared_keys.h`, `src/common/tests/test_cvar_classification.c`, `games/mm/2s2h/Rando/MiscBehavior/OnFileCreate.cpp`, `Rando/StaticData/Options.cpp`, `Rando/Types.h`, `games/mm/CMakeLists.txt`, `.github/scripts/check-registrar-elision.sh`, and `src/common/ComboMenuBar.cpp` **and `.h`** (#497 step 6's deletion also touches its two references in `CMake/SingleExecutable.cmake:48,94` — it cannot be completed without them).

`games/mm/2s2h/Rando/Menu.cpp` is **read-only reference** for the 9-group taxonomy at `:988-1028`. Do not claim or link it: it is elided into `2ship_rando_ui`, #497's decision forecloses reviving it, and pulling it into the link drags `BenGui/Menu.cpp` — which is exactly the #451 arming condition.

## Do not touch

`foreign_items.*` (Lane 1), `mm_rando_gen_test.cpp` (Lane 3 — it lands first; rebase, or add a separate TU), `TrackersGuiSingleExe.cpp` (Lane 5), `CheckQueue.cpp` / `DrawItem.cpp` (Lane 6). Lane 5 registers its windows on the shared `Ship::Context` Gui, deliberately bypassing the menu shell — do not route it through your new IA without agreeing that with Lane 5.

## Non-negotiables

- **Do not move MM's rando hook registration to the raw GameInteractor surface** (#467). If you do, OoT's file-select load starts re-evaluating MM's `IS_RANDO` conditions against OoT's `gSaveContext`.
- Do not "fix #451 along the way." If your work causes an **MM** TU that reads one of those four keys to enter the link, that arms it — say so on #451 rather than absorbing it.
- #497 step 1 is more than flipping a status line: it folds #454's resolution into ADR 0003 §4.1 and ADR 0004 §2d, updates `kDisputedClassificationKeys` (`cvar_shared_keys.h:370-372`, which still lists `gDeveloperTools.DebugSaveFileMode`) and its count static_assert, and resolves the ADR's five open maintainer calls. ADR 0003 is itself still Proposed with no issue tracking acceptance.
- Locks register via `redship_add_test(NAME ... COMMAND ... LABEL rando TIMEOUT 180 ENVIRONMENT ...)` — the argument is `LABEL`, singular. Default (omitted) is `redship`, the display-free tier. Menu *appearance* is operator-verified; do not claim a visual result CI did not prove.

## Stop and report if

The #451 premise above is wrong and MM's options genuinely cannot be hosted without the WHOLE_ARCHIVE flip — that reopens #451 as a real gate and changes the phase's sequencing. Also report if, once `context.h` and `Foreign.cpp` are excluded pending the asks above, #499's remaining deliverable is too thin to be worth a PR on its own; say so rather than working around the ownership.
