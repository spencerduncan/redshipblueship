You are Lane 6 of eight in the 2026-09-10 wave: **#638 — flush live scene flags before every freeze, both games, both switch drivers — and #626 — F10 during MM's game-over hands over an empty health bar.**

- **Branch:** `claude/638-flush-before-freeze-626-dead-bar`
- **Serves:** #638 (agent tracker for the human-filed #635; `Fixes #638`, `Refs #635` — never a closing keyword on #635) and #626.

## Scope

**#638.** A cross-game departure freezes `gSaveContext` *before* the scene-flag flush a normal scene transition performs, and never performs it: in MM, `Play_SaveCycleSceneFlags` (`games/mm/src/code/z_play.c:2105-2120`) is reached only from `Actor_CleanupContext` / the save routes, and the switch breaks the graph loop before `MM_Play_Destroy` runs. Walking into the Clock Tower door assigns `nextEntrance = 0xC010` and calls `Combo_CheckEntranceSwitch` (`games/oot/soh/GameExports_SingleExe.cpp:1765-1826`, freeze at `:1822`) before any flush. Fix: flush the live PlayState's scene flags into the SaveContext immediately before **every** `Context_FreezeState` — the entrance driver (`Combo_CheckEntranceSwitch`) and the hot-swap driver (`src/common/switch.cpp:99`, `MM_Game_Suspend` at `games/mm/2s2h/GameExports_SingleExe.cpp:1554`), for MM and for OoT's equivalent (`Play_SaveSceneFlags`). The heart piece is a cycle collectible flag the scene table marks persistent, so the flush is the right fix — not a permanent-flag write.

**#626.** F10 while MM's game-over prompt is up freezes with `health == 0`, bypassing PR #625's revive-on-exit; the far side's apply ASSIGNS the shared CONSUMABLE bar. Run the same revive-or-refuse decision on the F10/switch-request path when `health <= 0` — gate the request on a live bar (refuse with a message during game-over) or apply the #625 revive before the freeze. Gate on `gameMode`, not `fileNum`.

## Owns

`src/common/switch.cpp`, the freeze path in `src/common/context.cpp`, `Combo_CheckEntranceSwitch` in `games/oot/soh/GameExports_SingleExe.cpp`, the hot-swap freeze in **both** `GameExports_SingleExe.cpp` (function-scoped; lane 1 owns the MM exit/commit functions in the same file — rebase, don't reorder), and new scene-flag-freeze tests (`src/common/tests/test_hotswap_freeze.c` extension or a new `test_scene_flag_freeze.c`, with all three registration edits per `SHARED.md`).

## Do not touch

`MM_Play_ConsumeStartupEntrance` (lane 8). `MM_Combo_*ExitToOoT` and the kaleido death leg (lane 1). The lock must drive the real freeze path, not poke the flag arrays — a test that sets a flag and reads it back is vacuous.

## Verify

Local ROM-staged build plus both ctest tiers before merge; the rando tier is what drives play-state paths (see the #516 lesson). Reproduce the heart-piece dupe once by hand if you can; otherwise the lock must show the flag surviving a freeze through the production driver.
