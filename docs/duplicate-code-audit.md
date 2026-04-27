# Duplicate Code Audit — RedShipBlueShip

**Branch:** `claude/audit-duplicate-code-yWcrJ`
**Date:** 2026-04-27
**Scope:** `games/oot/`, `games/mm/`, `combo/`, `rsbs/`, `src/common/`. Submodules
(`libultraship`, `ZAPDTR`, `OTRExporter`) are out of scope.

This audit catalogs three classes of redundancy that arise from RedShipBlueShip
merging two upstream forks (Ship of Harkinian and 2Ship2Harkinian) into a single
binary:

1. **Parallel-named files** present under both `games/oot/soh/` and
   `games/mm/2s2h/` whose bodies overlap heavily.
2. **Parallel manager/framework classes** that solve the same problem twice
   under different names (`SohGui`/`BenGui`, `OTRGlobals`/`BenPort`,
   `CustomMessageManager`/`CustomMessage`, `SaveManager` × 2, `GameInteractor` × 2).
3. **Per-TU `extern` re-declarations** of cross-game globals
   (`OoT_gPlayState`, `OoT_gSaveContext`, etc.) instead of a single shared header.

Methodology: filename intersection between `games/oot/soh/**` and
`games/mm/2s2h/**`, byte-for-byte `cmp`, line-count `diff` to estimate shared
lines, manual inspection of high-risk areas, plus `rg`/`grep` sweeps for repeated
`extern` declarations and macros.

Findings are graded:

- **HIGH** — large block (>= 100 lines shared) or many sites; consolidating
  removes meaningful maintenance burden, OR drift between copies has already
  caused or is likely to cause bugs.
- **MED** — 20–100 shared lines, contained risk, but worth deduping
  opportunistically.
- **LOW** — small or naturally-divergent (game-specific business logic) where
  some shared scaffolding could be extracted but the duplication is mostly
  cosmetic.


## HIGH severity

### H1. `resource/` factories and types are forked nearly line-for-line

**Locations:**
- `games/oot/soh/resource/importer/**` (≈ 4 700 lines)
- `games/oot/soh/resource/type/**` (≈ 2 850 lines)
- `games/mm/2s2h/resource/importer/**` (≈ 3 800 lines)
- `games/mm/2s2h/resource/type/**` (≈ 1 900 lines)

156 files in `resource/importer/` and `resource/type/` exist under both
trees with the same basename. 33 are byte-identical (`type/Animation.cpp`,
`type/Animation.h`, `type/Background.cpp`, `type/Background.h`,
`type/AudioSample.cpp`, `type/CollisionHeader.cpp`, `type/CollisionHeader.h`,
`type/Cutscene.cpp`, `type/Scene.cpp`, `type/SkeletonLimb.cpp`,
`type/Array.h`, … 33 total — 646 lines copied verbatim).

The remaining 123 files each differ only by a 1- to 50-line diff that is
overwhelmingly: include path (`soh/resource/...` vs `2s2h/resource/...`),
namespace (`SOH::` vs `MM::` / `Ship::`), or a slightly different version-bump
constant. Examples:

- `resource/type/Path.cpp` — 8 diff lines / 11 total. (`games/oot/soh/resource/type/Path.cpp`, `games/mm/2s2h/resource/type/Path.cpp`)
- `resource/type/Skeleton.h` — 9 diff lines / 102 total.
- `resource/importer/AnimationFactory.cpp` — 13 diff lines / 95 total.
- `resource/importer/AudioSequenceFactory.cpp` — 21 diff lines / 394 total.
- `resource/importer/SkeletonFactory.cpp` — 9 diff lines / 134 total.
- `resource/importer/CollisionHeaderFactory.cpp` — 25 diff lines / 250 total.
- `resource/importer/scenecommand/SetActorListFactory.{cpp,h}` — 42 + 10 diff
  lines on 68 / 17 line files; 22 sibling `Set*Factory` files exhibit the
  identical pattern.

Of the 13 269 lines across both tree halves, ≈ 10 800 lines are textual
duplicates. This is the single biggest source of duplicated code in the
repository.

Why it bites: a bug fix (e.g. an OOB read in `CutsceneFactory`) has to be
re-implemented twice. Some files (`SkeletonFactory.cpp` lines 16–95 vs the MM
counterpart at lines 16–96) drift by exactly one constant, which is precisely
the situation that produces "fixed in OoT, still broken in MM" tickets.

---

### H2. `GameInteractor` framework forked under two names

**Locations:**
- `games/oot/soh/Enhancements/game-interactor/GameInteractor.h` (586 lines)
- `games/oot/soh/Enhancements/game-interactor/GameInteractor.cpp` (107 lines)
- `games/oot/soh/Enhancements/game-interactor/GameInteractor_HookTable.h` (94 lines)
- `games/mm/2s2h/GameInteractor/GameInteractor.h` (588 lines)
- `games/mm/2s2h/GameInteractor/GameInteractor.cpp` (546 lines)
- `games/mm/2s2h/GameInteractor/GameInteractor_HookTable.h` (61 lines)

Both files declare `class GameInteractor` at line 192 (OoT) and line 171 (MM).
The hook-registration template machinery (`RegisterGameHook`,
`UnregisterGameHook`, `ExecuteHooks`, `RegisterGameHookForID`,
`ExecuteHooksForID`, `RegisterGameHookForPtr`, `RegisterGameHookForFilter`,
plus the `RegisteredGameHooks<H>` and `HooksToUnregister<H>` traits) is
character-for-character identical between the two — roughly **200 lines of
template code** that have only differing whitespace.

```
games/oot/soh/Enhancements/game-interactor/GameInteractor.h:231-348
games/mm/2s2h/GameInteractor/GameInteractor.h:184-330
```

The `COND_VB_SHOULD` macro is also defined twice, once in each file
(`GameInteractor.h:182` OoT, `GameInteractor.h:569` MM). The bodies differ only
in whether they unregister `OnVanillaBehavior` (OoT) or `ShouldVanillaBehavior`
(MM); the surrounding scaffolding is the same.

What does diverge legitimately is the **hook list** (`GameInteractor_HookTable.h`
contents — different events for each game) and a handful of game-specific state
fields (`State::PacifistModeActive` in OoT, `events`/`currentEvent` in MM). Those
must stay separate, but the registration engine itself does not.

---

### H3. `ShipUtils.{cpp,h}` — partial fork with renamed but identical helpers

**Locations:**
- `games/oot/soh/ShipUtils.cpp` (155 lines) / `ShipUtils.h` (78 lines)
- `games/mm/2s2h/ShipUtils.cpp` (533 lines) / `ShipUtils.h` (63 lines)

The "extended culling" / quad-vertex / IsCStringEmpty cluster is present in
both, byte-for-byte identical except for one `_NES` suffix. Specifically:

- `Ship_GetExtendedAspectRatioMultiplier` — OoT line 20, MM line 330.
- `Ship_ExtendedCullingActorAdjustProjectedZ` — OoT 28, MM 338.
- `Ship_ExtendedCullingActorAdjustProjectedX` — OoT 39, MM 349.
- `Ship_IsCStringEmpty` — OoT 52, MM 362.
- `Ship_CreateQuadVertexGroup` — OoT 59, MM 369.

Five of the six helpers in OoT's `ShipUtils.cpp` are duplicated verbatim in MM's
`ShipUtils.cpp`. ~80 lines of identical code.

The `MM` copy is (deliberately) a superset that adds `Ship_GetItemNameById`,
`Ship_GetItemColorTint`, `Ship_FormatTimeDisplay`, `Ship_RemoveSpecialCharacters`,
`Ship_Hash`, `convertEnumToReadableName`, `GetActorDescription`,
`GetActorDebugName`, `GetActorCategoryName`, `LoadGuiTextures`, plus the
`Ship_Random*` PCG family. The OoT copy is (deliberately) a superset that adds
the `ShipUtils::Random` / `ShipUtils::Shuffle` template namespace.

Net effect: each tree has the union it needs, but the intersection (~80
non-game-specific lines) is forked. Worse, the rando code in OoT uses
`ShipUtils::Random<T>(…)` while the equivalent in MM uses `Ship_Random(min,max)`
— two RNG APIs in one binary.

---

### H4. `SaveManager` parallel implementation under different shapes

**Locations:**
- `games/oot/soh/SaveManager.cpp` (2 846 lines) / `SaveManager.h` (220 lines)
- `games/mm/2s2h/SaveManager/SaveManager.cpp` (536 lines) / `SaveManager.h` (19 lines)
- `games/mm/2s2h/SaveManager/BinarySaveConverter.cpp` (749 lines)
- `games/mm/2s2h/SaveManager/Migrations/`

The two SaveManagers are not byte-similar (OoT is a `class SaveManager` with
sectioned save handling; MM is a flat `SaveManager_*` C-style API), but they
solve the same problem and overlap on:

- `WriteSaveFile` (`SaveManager.cpp:32` OoT vs `SaveManager.cpp:107` MM) —
  ~20 lines of buffered JSON write boilerplate.
- `ReadSaveFile` (OoT `:40`, MM `:131`).
- `GetFileName` / `GetFileTempName` (OoT `:54-65`, MM `:199`).
- `HandleFileDropped` (OoT vs MM `SaveManager.cpp:247`,
  `BinarySaveConverter.cpp`).
- `MoveInvalidSaveFile` and the migration version-jump enum / loop
  (`SaveManager_MigrateSave`).
- `SysFlashrom_ReadData` / `SysFlashrom_WriteData` are forked C entry
  points (defined in both, plus a third stub in `src/common/mm_stubs.c:167-168`).

`gSaveContext` itself is held in two completely separate buffers (5 160 B
OoT, 18 632 B MM, see `src/common/context.h`); that part is genuinely
two-game and not duplication. But the **JSON serializer, file IO, migration
runner, and file-slot bookkeeping** are forked and could share an interface.

---

### H5. `GameExports_SingleExe.cpp` — parallel `Game_*` lifecycle hooks

**Locations:**
- `games/oot/soh/GameExports_SingleExe.cpp` (348 lines)
- `games/mm/2s2h/GameExports_SingleExe.cpp` (433 lines)

Both files implement the lifecycle ABI declared in
`src/common/game_lifecycle.h`. The files differ in 577 of ~780 lines but the
**skeleton** of every entry point is the same:

- `OoT_Game_Init` (line 91) and `MM_Game_Init` (line 255) both: parse argv,
  call `*_Heaps_Alloc()`, optionally `InitOTR()` / register resource
  factories, log progress.
- `OoT_Game_Run` (128) / `MM_Game_Run` (341) — both invoke the per-game main
  thread and log entry/exit.
- `OoT_Game_Suspend` (141) / `MM_Game_Suspend` (356) — audio cleanup +
  state freeze.
- `OoT_Game_Resume` (171) / `MM_Game_Resume` (362) — restore SaveContext
  via `Context_RestoreState`.
- `OoT_Game_Shutdown` (211) / `MM_Game_Shutdown` (392) — heaps free + log.
- `*_GetGameOps()` returns a `GameOps` struct populated identically.

The `WEAK_SYMBOL` stubs at `src/common/game_stubs.cpp:28-69` are *also*
parallel pairs. The `[OoT]`/`[MM]` `fprintf(stderr, ...)` log lines are nearly
identical with only the prefix swapped.

---

### H6. `mixer.{c,h}`, `framebuffer_effects.{c,h}`, `gu_pc.c` — copied with minor renames

**Locations:**
- `games/oot/soh/mixer.c` (818 lines) / `games/mm/2s2h/mixer.c` (819 lines)
  — diff = **7 lines** (one DMEM-size constant set + 1 comment).
- `games/oot/soh/mixer.h` (93 lines) / `games/mm/2s2h/mixer.h` (93 lines)
  — diff = **0 lines, byte-identical**.
- `games/oot/soh/gu_pc.c` (88 lines) / `games/mm/2s2h/gu_pc.c` (88 lines)
  — diff = **2 lines** (`sqrtf` → `MM_sqrtf` in `guNormalize`).
- `games/oot/soh/framebuffer_effects.c` (177 lines) /
  `games/mm/2s2h/framebuffer_effects.c` (177 lines) — diff = **14 lines**
  (`OoT_gScreenWidth` ↔ `MM_gScreenWidth`, `gMtxClear` ↔ `gIdentityMtx`,
  `OTRGlobals.h` ↔ `BenPort.h`).
- `games/oot/soh/framebuffer_effects.h` (17 lines) /
  `games/mm/2s2h/framebuffer_effects.h` (17 lines) — diff = **2 lines**
  (one include only).

That is **5 files, 1 192 lines of code, ≈ 25 lines of real difference**.
This is the closest the repo has to "copy paste with `s/OoT/MM/`" and is the
quickest HIGH-severity win — all five could be moved to `combo/src/` (or
`src/common/`) with a couple of `extern` shims, with no design work.

## MED severity

### M1. `ShipInit.hpp` — same struct in two places, diverging comment-only

**Locations:**
- `games/oot/soh/ShipInit.hpp` (75 lines)
- `games/mm/2s2h/ShipInit.hpp` (44 lines)

`struct ShipInit` (lines 12–28 in both) and `struct RegisterShipInitFunc`
(lines 61–71 OoT, 30–41 MM) are character-identical except for one `#include
<string>` placement. The OoT copy carries an extra 30-line Doxygen comment;
that is the *entire* difference. There are 50+ call sites of
`RegisterShipInitFunc` across both trees (`grep -rn RegisterShipInitFunc
games/oot/soh games/mm/2s2h | wc -l` = 50+).

This is one `#pragma once` header that should be moved to `src/common/`
(or under `combo/include/combo/`) and included from both trees unchanged.

---

### M2. `Notification.{cpp,h}` — same UI window, different namespace

**Locations:**
- `games/oot/soh/Notification/Notification.cpp` (140 lines) / `.h` (38)
- `games/mm/2s2h/BenGui/Notification.cpp` (141 lines) / `.h` (38)

29 diff lines on 140 (`Notification.cpp`); 8 diff lines on 38
(`Notification.h`). The differences are: (a) include of `OTRGlobals.h` vs
not, (b) `CVAR_SETTING("Notifications.Position")` macro vs literal
`"gNotifications.Position"`, and (c) MM adds two `PushStyleVar` lines for a
configurable size. The actual `Notification::Window::DrawElement`,
`AddNotification`, `Update`, and the `notifications` deque are a single
implementation copied twice.

Estimated shared lines: ~140.

---

### M3. `InputViewer.{cpp,h}` — same controller-overlay code, drifting

**Locations:**
- `games/oot/soh/Enhancements/controls/InputViewer.cpp` (740 lines) /
  `.h` (46)
- `games/mm/2s2h/BenGui/InputViewer.cpp` (820 lines) / `.h` (40)

334 diff lines on 740–820. The button-rendering geometry, `InputViewer`
class, joystick/c-stick overlay logic, and CVar wiring are recognizably the
same code, with MM having additional features bolted on. ~500 shared lines.

---

### M4. `ResolutionEditor.{cpp,h}` — same advanced-resolution UI, copied

**Locations:**
- `games/oot/soh/SohGui/ResolutionEditor.cpp` (602 lines) / `.h` (12)
- `games/mm/2s2h/BenGui/ResolutionEditor.cpp` (571 lines) / `.h` (10)

225 diff lines on 602 (`ResolutionEditor.cpp`). The diff is overwhelmingly
namespace renames (`SohGui::` → `BenGui::`, `mSohMenu` → `mBenMenu`,
`SohMenu.h` → `BenMenu.h`) and one `std::map` → `std::unordered_map`
swap. `aspectRatioPresetLabels`, `aspectRatioPresetsX/Y`, the per-CVar
update lambda, and the `UIWidgets::CVarCheckbox` calls match. ~370 shared
lines.

---

### M5. `MessageViewer.{cpp,h}` — debug message viewer

**Locations:**
- `games/oot/soh/Enhancements/debugger/MessageViewer.cpp` (267) / `.h` (59)
- `games/mm/2s2h/DeveloperTools/MessageViewer.cpp` (302) / `.h` (59)

419 diff lines on 267/302. Less convergent than M3/M4 because the message
schemas differ between games, but the parser scaffolding, ImGui rendering,
and selection state are forked rather than shared. ~150 shared lines.

---

### M6. `UIWidgets.{cpp,hpp}` — sibling rendering helpers

**Locations:**
- `games/oot/soh/SohGui/UIWidgets.cpp` (1 179 lines) / `.hpp` (1 064)
- `games/mm/2s2h/BenGui/UIWidgets.cpp` (1 343 lines) / `.hpp` (1 168)

500 diff lines on the headers, 377 on the .cpp. MM's copy diverges more
intentionally (it adds `Colors`, `WidgetOptions::Color()`, additional widget
variants) but the core widgets — `CVarCheckbox`, `CVarSlider`, `CVarCombo`,
`CVarRGBAPicker`, `EnhancementCheckbox`, `Spacer`, `PushStyleSlider`,
`Tooltip` — are pairwise the same algorithm. Header guard `UIWidgets2_hpp`
(OoT) vs `UIWidgets_hpp` (MM) is a tell.

Estimated shared lines: ~1 100 (large but heavily reorganized; harder to
mechanically merge than M3/M4).

---

### M7. `CrashHandlerExt.cpp` — actor-list dump and build-info banner

**Locations:**
- `games/oot/soh/CrashHandlerExt.cpp` (94 lines)
- `games/mm/2s2h/CrashHandlerExt.cpp` (79 lines)

Both define `CrashHandler_WriteActorData` (OoT line 31, MM line 19) and
both call into `Fast::g_exec_stack.disp_stack` to dump the GFX stack.
~60 shared lines. The macros `WRITE_VAR_LINE` / `WRITE_VAR_VAL`
(`CrashHandlerExt.cpp:13-17`) are duplicated verbatim. Build-info banner
(version / branch / commit / build date) writes the same six fields
through the `OoT_g*` vs `MM_g*` variants.

---

### M8. `z_message_OTR.cpp` — message-table archive loader

**Locations:**
- `games/oot/soh/z_message_OTR.cpp` (178 lines)
- `games/mm/2s2h/z_message_OTR.cpp` (66 lines)

The OoT copy carries a richer custom-message replacement loop; the MM
copy is the simplified subset. Both share the
`OTRMessage_LoadTable`/`SetMessageEntry` shape that walks an
`ArchiveManager` listing, loads a `Text`/`TextMM` resource, mallocs an
entry table, and copies messages. ~50 shared lines.

---

### M9. `ObjectExtension/` — identical implementation, near-identical header

**Locations:**
- `games/oot/soh/ObjectExtension/ObjectExtension.cpp` (25 lines)
- `games/mm/2s2h/ObjectExtension/ObjectExtension.cpp` (25 lines)
- `games/oot/soh/ObjectExtension/ObjectExtension.h` (116 lines)
- `games/mm/2s2h/ObjectExtension/ObjectExtension.h` (115 lines)
- `games/oot/soh/ObjectExtension/ActorListIndex.{cpp,h}`
- `games/mm/2s2h/ObjectExtension/ActorListIndex.{cpp,h}`

`ObjectExtension.cpp` is **byte-for-byte identical** between the two
trees. `ObjectExtension.h` differs by 12 lines (one include set + the
`extern "C"` wrapping of `ObjectExtension_Free`). `ActorListIndex.cpp`
differs by 7 lines / 15. This is a generic
"attach-typed-data-to-a-pointer" container with zero game-specific logic
— it should never have been forked.

Combined: ~165 lines, 99% identical.

---

### M10. `framebuffer_effects` and audio mixer header — already-identical files

These were also called out in **H6**. Listed here only to reinforce that
`mixer.h` (93 lines) is byte-identical between the two trees and could be
moved to `combo/include/combo/` immediately.

## LOW severity

### L1. Redundant `extern PlayState* OoT_gPlayState;` across TUs

`extern PlayState* OoT_gPlayState;` is declared in **111** translation
units across `games/oot/`, and `extern PlayState* MM_gPlayState;` in 14
under `games/mm/`. 125 sites total. The canonical definition lives once
in the per-game globals; every consumer re-declares it locally instead of
including a single header.

Sample sites (selected from `games/oot/soh/`):

- `CrashHandlerExt.cpp:18` (`extern "C" PlayState* OoT_gPlayState;`)
- `ResourceManagerHelpers.cpp:18`
- `Enhancements/enemyrandomizer.cpp:17`
- `Enhancements/RebottleBlueFire.cpp:10`
- `Enhancements/kaleido.cpp:16`
- `Enhancements/ArrowCycle.cpp:14`
- `Enhancements/gameplaystats.cpp:21`
- `Enhancements/mods.cpp:36`
- `Enhancements/customequipment.cpp:14`
- `Enhancements/AssignableTunicsAndBoots.cpp:12`
- `Enhancements/ExtraTraps.cpp:11`
- `Enhancements/debugconsole.cpp:32`
- `Enhancements/nametag.cpp:17`
- `Enhancements/DisableSandstorm.cpp:4`
- `Enhancements/savestates.cpp:25`
- `Enhancements/gameconsole.c:18` and `Enhancements/gameconsole.h:29` (defines
  it twice in the same module)
- `Enhancements/debugger/colViewer.cpp:18`, `actorViewer.cpp:29`,
  `debugSaveEditor.cpp:24`
- `Enhancements/Graphics/ToTMedallions.cpp:9`,
  `Disable2DBackgrounds.cpp:8`
- `OTRGlobals.cpp:1045` (`OoT_AudioMgr_CreateNextAudioBuffer`),
  `z_play_otr.cpp:11-12` (`OoT_Play_InitScene`, `OoT_Play_InitEnvironment`)
- `Enhancements/debugger/debugSaveEditor.cpp:58, 60` (`OoT_gAreaGsFlags[]`,
  `OoT_gAmmoItems[]`) — both *also* declared in
  `games/oot/include/variables.h:31-33` and
  `games/oot/src/overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h:7-13`,
  i.e. the same array is `extern`-declared three times.

In total `grep -rn "extern .* OoT_g\|extern .* MM_g" games src`
turns up ~250 single-line redundant `extern` declarations. Drift risk is
real: any signature change (e.g. adding a new field to `PlayState` or
renaming `OoT_gAmmoItems` → `OoT_gAmmoItemsTable`) forces sweeping edits.

Fix: a single header in `combo/include/combo/cross_game_globals.h` (or
similar) that forwards `OoT_gPlayState`, `OoT_gSaveContext`,
`MM_gPlayState`, `MM_gSaveContext`, build/version arrays, and the
weather/equip globals. Delete the per-TU redeclarations.

---

### L2. `RegisterShipInitFunc` boilerplate scattered across enhancements

There are 50+ files of the shape

```cpp
static RegisterShipInitFunc initFunc(RegisterFooFeature, { CVAR_FOO_NAME });
```

(see `games/oot/soh/Enhancements/RemoveSpinAttackDarkness.cpp:45`,
`Enhancements/ArrowCycle.cpp:290`, `Enhancements/UnsheatheWithoutSlashing.cpp:18`,
`Enhancements/kaleido.cpp:521`, `ObjectExtension/ActorMaximumHealth.cpp:29`,
many more). Each file duplicates the same `static RegisterShipInitFunc
initFunc(...)` line, and many also duplicate the same
`COND_VB_SHOULD(VB_X, CVAR_VALUE, { ... })` macro invocation. Each of these
local copies of the macro is fine in isolation, but the macro is declared
twice (once per `GameInteractor.h` — see H2), so the same call site
expands to subtly different code in each tree.

Tagging this LOW because it is symptomatic of H2 rather than an
independent issue.

---

### L3. `WEAK_SYMBOL` test-stubs duplicate the real ABI

`src/common/game_stubs.cpp:28-69` defines stubs for `OoT_Game_Init`,
`OoT_Game_Run`, `OoT_Game_Shutdown`, `OoT_Game_GetName`, `OoT_Game_GetId`
and the four `MM_Game_*` mirrors. The strings logged are pairwise
"`[OoT STUB]`" / "`[MM STUB]`" with no other meaningful difference.

This is intentional (test mode) but is itself a forked pair. A small
`X(OoT) X(MM)` X-macro would make the stub set one declaration.

---

### L4. `CVAR_PREFIX_*` / `CVAR_SETTING(...)` naming forks

`cvar_prefixes.h` exists only in `games/oot/soh/` (50+ macros). The MM
side spells the same prefixes inline as string literals
(`"gNotifications.Position"`, `"gAdvancedResolution.Enabled"`, etc.).
This is part of why `Notification.cpp` (M2) and `ResolutionEditor.cpp`
(M4) cannot be merged byte-for-byte: the OoT copy uses the macros, the MM
copy uses the literal strings. Either the macro header should move to
`combo/include/` and MM should adopt it, or the macros should be removed
from OoT in favor of the literals.

---

### L5. `EasyFrameAdvance.cpp`, `MoonJump.cpp`, `HyperEnemies.cpp`,
`UnrestrictedItems.cpp`, `WeirdAnimation.cpp`, `ItemUnequip.cpp` — small cheats

These are all tiny (~25–80 line) cheat / debug toggles forked into both
trees. Several are byte-identical (`WeirdAnimation.cpp` — 65 lines,
identical). The rest differ in 25–150 diff lines on small total budgets
because they tap different game-specific hooks. ~250 lines forked total.
Worth dealing with only after H1–H6 land.


## Total duplicated LOC

A "shared line" below means a line of source that appears, with at most
trivial whitespace/include/symbol-rename differences, in both trees. Counts
are approximate (line-count minus diff-line-count, summed per pair).

| Bucket | Files | Shared LOC (approx) |
|---|---|---|
| H1 — `resource/` factories and types | 156 | **≈ 10 800** |
| H2 — `GameInteractor` framework + hook table | 6 | ≈ 700 |
| H3 — `ShipUtils.{cpp,h}` cluster | 4 | ≈ 100 |
| H4 — `SaveManager` (overlap only — not full) | 4 | ≈ 200 |
| H5 — `GameExports_SingleExe.cpp` skeleton | 2 | ≈ 200 |
| H6 — mixer / framebuffer_effects / gu_pc | 5 | ≈ 1 170 |
| M1 — `ShipInit.hpp` | 2 | ≈ 40 |
| M2 — `Notification.{cpp,h}` | 4 | ≈ 140 |
| M3 — `InputViewer.{cpp,h}` | 4 | ≈ 500 |
| M4 — `ResolutionEditor.{cpp,h}` | 4 | ≈ 370 |
| M5 — `MessageViewer.{cpp,h}` | 4 | ≈ 150 |
| M6 — `UIWidgets.{cpp,hpp}` | 4 | ≈ 1 100 |
| M7 — `CrashHandlerExt.cpp` | 2 | ≈ 60 |
| M8 — `z_message_OTR.cpp` | 2 | ≈ 50 |
| M9 — `ObjectExtension/`, `ActorListIndex.{cpp,h}` | 6 | ≈ 165 |
| L1 — extern `OoT_g*` / `MM_g*` redeclarations | ~125 sites | ≈ 250 |
| L3 — `game_stubs.cpp` weak stubs | 1 | ≈ 25 |
| L5 — small cheat/restoration files | 6 | ≈ 250 |
| **Total** | | **≈ 16 270** |

Plus 33 byte-identical files in `resource/` (646 lines, already counted under
H1) and 2 byte-identical top-level files (`portable-file-dialogs.h` 1 770 lines,
`FastCrc32C.c` 147 lines, `mixer.h` 93 lines, `WeirdAnimation.cpp` 65 lines,
`ObjectExtension.cpp` 25 lines — counted under their respective findings).

Putting a slightly conservative bound on it: **~16 000 lines of source code in
the repo are textual duplicates between the OoT and MM trees**. Out of an
overall codebase of roughly 220 k lines under `games/`, that is ~7 %.

---

## Recommended consolidations

The five recommendations below are sized so that each one is a tractable PR.
Effort estimates are calendar-day estimates for a single engineer familiar
with libultraship. They assume CTest (`BootOoT`, `BootMM`, `SwitchOoTMM`,
`SwitchMMOoT`, `Roundtrip`, `Context`, `AllTests`) is run after each.

### R1. Move identical/near-identical low-level helpers into `src/common/`  *(2–3 days)*

Targets: H6 (`mixer.{c,h}`, `framebuffer_effects.{c,h}`, `gu_pc.c`), M1
(`ShipInit.hpp`), M9 (`ObjectExtension/`, `ActorListIndex.{cpp,h}`).

Why first: byte-level overlap is high, ABI is small, no design decisions.

Plan:
1. Create `src/common/audio/mixer.{c,h}` from OoT copy. Replace the MM-only
   DMEM constants with `#ifdef MM_AUDIO_DMEM_LAYOUT` or pass via a config
   struct.
2. Create `src/common/graphics/framebuffer_effects.{c,h}`. Add
   `gfx_get_screen_width(GameId)` shim so the MM/OoT split on
   `OoT_gScreenWidth` vs `MM_gScreenWidth` can be done at call time.
3. Create `src/common/graphics/gu_pc.c`. Make `MM_sqrtf` a one-line alias in
   `mm_stubs.c` (already is) so the shared file uses plain `sqrtf`.
4. Move `ShipInit.hpp` to `src/common/ShipInit.hpp` (header-only, no work
   beyond updating ~50 includes).
5. Move `ObjectExtension/{ObjectExtension.cpp, ObjectExtension.h,
   ActorListIndex.cpp, ActorListIndex.h}` to `src/common/ObjectExtension/`.
   Delete the duplicates from the two trees.

Estimated LOC removed: **~1 400**.

### R2. Cross-game globals header  *(1 day)*

Target: L1.

Create `combo/include/combo/cross_game_globals.h` with one-time `extern`
declarations of: `OoT_gPlayState`, `OoT_gSaveContext`, `OoT_gBuildVersion`,
`OoT_gGitBranch`, `OoT_gGitCommitHash`, `OoT_gBuildDate`, `OoT_gWeatherMode`,
`OoT_gPlayerModelTypes`, `OoT_gAmmoItems`, `OoT_gEquipMasks`,
`OoT_gEquipShifts`, `OoT_gAreaGsFlags`, plus the `MM_*` mirrors. Delete
~125 redundant `extern` lines across the trees.

Estimated LOC removed: **~250** (and a meaningful drop in cognitive load —
right now adding a field to `PlayState` requires a sweep of 100+ files to
ensure no stale signature is hiding in a per-TU `extern`).

### R3. Extract `GameInteractor` registration engine  *(3–5 days)*

Target: H2.

The hook-registration template machinery
(`RegisteredGameHooks<H>`, `HooksToUnregister<H>`, `RegisterGameHook`,
`UnregisterGameHook`, `ExecuteHooks`, plus the ID/PTR/Filter variants)
is template code with no game references. Extract it into
`src/common/game_interactor/GameInteractorBase.h` as
`class GameInteractorBase { ... }` and have `class GameInteractor` in
each tree inherit from it.

The hook **list** (`GameInteractor_HookTable.h`) and the game-specific
state (`State::PacifistModeActive`, `events`/`currentEvent`) stay in the
per-tree subclass.

Also: define `COND_VB_SHOULD` exactly once in the base header, parameterized
on the `OnVanillaBehavior` / `ShouldVanillaBehavior` type.

Estimated LOC removed: **~700**. Risk: medium — the templates are header-only
so ABI doesn't shift, but inheritance changes the symbol mangling for the
`Instance` pointer; needs careful single-exe linker check.

### R4. Move `resource/type/*` (and the byte-identical `resource/importer/*`) into a shared library  *(5–10 days)*

Target: H1, the largest single source of duplication.

The `resource/type/` files describe N64-asset POD layouts; almost none of
them depend on game logic. The 33 byte-identical files plus the ~50
"differs only in include path / version constant" files can be moved into
`combo/src/resource/` as a static library that both trees link against.

Plan:
1. Start with the byte-identical files (`type/Animation.cpp`,
   `type/Animation.h`, `type/Background.{cpp,h}`, `type/AudioSample.cpp`,
   `type/CollisionHeader.{cpp,h}`, `type/Cutscene.cpp`,
   `type/Scene.cpp`, `type/SkeletonLimb.cpp`, …). Move them under
   `combo/src/resource/type/` and add `target_link_libraries` from the OoT
   and MM CMakeLists.
2. Then the trivially-divergent ones (`Path.cpp` 8 lines diff,
   `AudioSequence.cpp` 2 lines diff, `Skeleton.h` 9 lines diff): unify the
   diff into `#ifdef GAME_OOT` / `#ifdef GAME_MM` blocks (or, better,
   parameterize the version constant).
3. The `importer/` files and `importer/scenecommand/` files have larger diffs
   but the diff is mostly the namespace declaration — bring them across
   under a `Resource::` namespace and have each tree alias to its
   pre-existing `SOH::` / `MM::` for backwards-compat.

Estimated LOC removed: **~10 000**. Risk: high — touches the asset
extraction path and `.o2r` resource loader. Must validate `BootOoT`,
`BootMM`, `Roundtrip`, and asset extraction (per `CLAUDE.md` build steps)
before merging.

### R5. Extract shared `Ship_*` C-callable utilities  *(1–2 days)*

Target: H3, M7.

The five identical `Ship_*` extern-C helpers in `ShipUtils.cpp`
(`Ship_GetExtendedAspectRatioMultiplier`,
`Ship_ExtendedCullingActorAdjustProjectedZ/X`, `Ship_IsCStringEmpty`,
`Ship_CreateQuadVertexGroup`) are pure C code that depends only on
`Actor*` and `Vtx*`. Move to `src/common/ShipUtilsCommon.{cpp,h}`. The
font-table helpers (`Ship_GetCharFontWidth`, `Ship_GetCharFontTexture`)
diverge by only one font-table symbol name; pass the table in as a
parameter.

While in here: pull `WRITE_VAR_LINE` / `WRITE_VAR_VAL` macros and
`append_line` / `append_str` from `CrashHandlerExt.cpp` into the same
shared header (M7), and provide a single `CrashHandler_PrintBuildInfo`
that takes the four version strings as arguments.

Estimated LOC removed: **~140**. Risk: low.

---

## Summary

| Priority | Effort | LOC removed | Risk |
|---|---|---|---|
| R1 | 2–3 d | ~1 400 | low |
| R2 | 1 d | ~250 | low |
| R3 | 3–5 d | ~700 | medium |
| R4 | 5–10 d | ~10 000 | high |
| R5 | 1–2 d | ~140 | low |

R1 + R2 + R5 together remove ≈ 1 800 lines in under a week of work and
unblock cleaner consolidation later. R4 is the big-ticket item but should
not be attempted before R1 (which establishes the pattern of moving
files into `src/common/`) and R3 (which simplifies the include graph the
factories sit in).
