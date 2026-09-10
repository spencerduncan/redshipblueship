You are Lane 8 of eight in the 2026-09-10 wave: **#639 option A — re-author the new-file clock on the first MM arrival so vanilla's dawn telop runs.**

- **Branch:** `claude/639-first-arrival-clock`
- **Serves:** #639 (agent tracker for the human-filed #636; `Fixes #639`, `Refs #636` — never a closing keyword on #636). The symptom holds; the reporter's attribution (an OoT time leak) does not — no OoT time value crosses; the shared-resource harvest/apply covers rupees, wallet, health, magic and ammo only.

## Scope

A first MM entry cold-boots Setup → ConsoleLogo → TitleSetup → Play. `TitleSetup_SetupTitleScreen` authors the title-demo save (`MM_Sram_InitNewSave()` then the vanilla override `save.time = CLOCK_TIME(8,0); save.day = 1;`, `ovl_opening/z_opening.c:29-30`). `MM_Play_ConsumeStartupEntrance` (`games/mm/src/code/z_play.c:2316-2486`) treats the first entry as "spawn on the bootstrap save": it neutralizes entrance, cutscene index, game mode, respawn flag, `nextDayTime`, timers and audio ids — but never `save.time` or `save.day`, and `nextDayTime = NEXT_TIME_NONE` skips `Play_Init`'s rewrite (`:2646-2648`), `respawnFlag = 0` skips the DayTelop branch (`:2521-2527`), and `En_Test4`'s dawn ceremony is gated on `CURRENT_DAY == 0`. Result: Day 1 08:00, no dawn, exactly `CLOCK_TIME(8,0) − CLOCK_TIME(6,0)`.

**Option A:** in the first-entry leg (`hadFrozenState == 0`), re-author the clock to what a vanilla new file carries into `Play_Init` — the `Sram_InitNewSave` values plus whatever the file-select-start path sets so the DayTelop / dawn ceremony run — rather than the title-demo override. Do **not** touch the frozen-state leg: a resumed MM session keeps its own clock.

## Owns

`MM_Play_ConsumeStartupEntrance` in `games/mm/src/code/z_play.c`, and `games/mm/2s2h/mm_resume_state_test.cpp` (add the first-arrival clock assertion there; the resume-contract tests already drive this function).

## Do not touch

The freeze path and `Combo_CheckEntranceSwitch` (lane 6). `MM_Combo_*ExitToOoT` (lane 1). `z_opening.c`'s title override is vanilla behaviour for the attract demo — leave it.

## Verify

Local ROM-staged build plus both ctest tiers; the `MMPairSwitchEntry` / resume rows must still pass. If you can, cross once by hand from a new OoT file and confirm 06:00 with the dawn telop; the lock must assert the authored time through the real consume path, not by writing `save.time` in the test.
