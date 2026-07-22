You are Lane 3 of six on RedShipBlueShip Phase 3.1 (#492): **live P0-class correctness** in MM's save path. Lane 5's tracker work cannot be validated until you land, so land early.

Read #492, then #487, then #491. Read merged PR #485 — the moon-crash leg of this exact class, whose fix shape you reuse. **Rebase onto open PR #486**, which touches `sys_flashrom.c` and `z_sram_NES.c` before you.

## Step 1 — #487

In the `redship` single-exe build MM's `SaveManager` TU is filtered out of the link (`games/mm/CMakeLists.txt:301`), so `SaveManager_SysFlashrom_ReadData` resolves to the stub at `src/common/mm_stubs.c:260` — which **returns 0 (success) without filling the caller's buffer**. `Sram_UpdateWriteToFlashOwlSave` (`z_sram_NES.c:2175-2180`) memsets the buffer, calls that read, ignores the return, and memcpy's the zeroed buffer over `gSaveContext` for `offsetof(SaveContext, fileNum)` — i.e. all of `struct Save`, including the 2S2H `shipSaveInfo` block. (Do not trust the `0x3CA0` annotation in the header; that is the vanilla N64 offset, and the port's struct is far larger.)

**Do #487's step 1 before writing any fix**, but note the standing convention forbids local builds while other agents run and CI emits no link map. The static evidence is already conclusive — `games/mm/CMakeLists.txt:301` is the only reference to `2s2h/SaveManager` in any CMake file. Record that as the confirmation, or arrange who runs the build; do not block on it.

**Enumerate the readback sites by grep — do not trust a count.** The two unguarded owl-save readback-then-commit sites are `Sram_UpdateWriteToFlashOwlSave` (`:2179-2180`) and `func_80147414` (`:2225-2234`). `Sram_ResetSaveFromMoonCrash` (`:1301-1314`) is #485's, already guarded. **`func_80147314` is not one of them** — it copies the live context *into* the flash buffer and contains no `SysFlashrom_ReadData` call at all. There are further readback-then-commit sites at `:1404`, `:1588`, `:1682`, `:1706`, `:1801`, `:1862`, `:1929`; a helper written for "three sites" will silently under-cover.

#487 step 6 is in scope and lives in a file you own: `MM_Sram_InitSave` dispatches `OnSaveInit` at `z_sram_NES.c:1992` and never `OnSaveLoad` (#467 recommendation 1). Per #469 the dispatch goes *after* the flash-buffer memcpy, not between the checksum and it — pick a position and write down which.

## Step 2 — #491

Strengthen the arming probe. `MMPairSwitchEntry` probes arm state by `CountForTest<OnFlagSet>` at **two** sites — the arrival leg (`:788`, `:795-800`) and the return leg (`:891`, `:916-921`) — and the assertion is a relative comparison against a boot-chain baseline, not a literal `0 -> 1`. Both must move to the VB verdict. `MMReloadArmState` already carries a VB-verdict leg at `:1118-1143` to copy.

## The sequencing fact that makes or breaks your lock

Rando `COND_HOOK`s are re-evaluated **only** inside `OnSaveLoadHandler` (`Rando.cpp:13-21`, registered `:41`), and the owl readback dispatches no `OnSaveLoad`. So immediately after the readback the hooks are still registered and a VB probe reports **ARMED** even though `saveType` is already 0. The disarm happens at the next `OnSaveLoad`.

Your lock must therefore dispatch `GameInteractor_ExecuteOnSaveLoad` (or drive a path that does) after the readback before asserting arm state. Without that, the arm-state leg is itself vacuous and only the `saveType` / `finalSeed` / check-entry assertions carry weight.

## Files you own

`games/mm/src/code/z_sram_NES.c`, `src/common/mm_stubs.c` (and `mm_stubs.cpp` if the header check lands there), `games/mm/2s2h/mm_rando_gen_test.cpp`, `docs/unified-surface-findings.md`, and you may **append** rows in the MM block of `CMake/SingleExecutable.cmake` as well as edit the existing `MMPairSwitchEntry` / `MMReloadArmState` / `MMMoonCrashArmState` rows.

Lane 2 needs `mm_rando_gen_test.cpp:296-303` (the `FAIL(12)` post-condition) and Lane 4 wants assertions there for #499. You land first; both rebase onto you.

## Do not touch

`context.h` / `foreign_items.*` (Lane 1), `Rando/Foreign.cpp` (Lane 2), `MiscBehavior/OnFileCreate.cpp` and `Rando/Menu.cpp` (Lane 4), `TrackersGuiSingleExe.cpp` / `Rando/CheckTracker/*` (Lane 5), `CheckQueue.cpp` (Lane 6). In `games/mm/2s2h/GameExports_SingleExe.cpp` you have `MM_Rando_PairOnCrossGameArrival` only — Lanes 1, 5 and 6 hold other functions in that file.

You probably do **not** need `sys_flashrom.c` (its redirect at `:83` already forwards the return faithfully; it has other callers at `:226`) or `mm_resume_state_test.cpp` (nothing in either issue lands there). Do not claim them by habit.

## Non-negotiables

- Prefer **one shared helper** over per-site guards, and cover the sites you enumerated — not just the one you were pointed at.
- `return -1` applies to the **read** stub only. The write twin at `mm_stubs.c:261` is `int(void*, int, int)` against a real `void(u8*, u32, u32)` — once header-checked it cannot return anything. Its header-checked fix is becoming `void`.
- Including `SaveManager.h` from `mm_stubs.c` takes the C branch, which needs `u8`/`u32`/`s32`; that TU currently pulls no MM ultra64 typedefs. Decide whether MM include paths belong in that C file or whether the header check moves to `mm_stubs.cpp`, and say which.
- Tier: attempt a display-free `redship` row first, per #491 step 1. Fall back to `LABEL rando` only if `MM_Rando_Init` / `Ship::Context` proves necessary, and record which in the CMake comment. #487's lock is specified as `redship`; do not hardcode `rando` and foreclose the tier question that is #491's whole point.
- Non-vacuity: first **disarm** against a vanilla bootstrap and assert the disarm took, *then* assert the re-arm — a blanket "always force rando" fix must fail that phase. A third phase asserts the rando block survives, so a fix preserving only the type byte also fails.
- End your rows with the explicit cleanup block `MMReloadArmState` uses. MM and OoT share one `gSaveContext` storage; without it you flake the `AllTests` aggregate.

## Stop and report if

The link check shows the real `SaveManager.cpp` in the binary, or you conclude MM saves in single-exe persist nothing at all (the write stub is also a no-op). The latter is #487 step 5's scoping question for the operator — whether `redship` should have a real MM save path — not something to settle inside this fix.
