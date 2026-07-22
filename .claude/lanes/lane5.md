You are Lane 5 of six on RedShipBlueShip Phase 3.1 (#492): **visibility** — making the cross-game world legible to the player at all.

Read #492, then #489, then #496. Read merged PR #457, which registered MM's trackers on the shared Gui; you are fixing and extending what it landed.

## Ownership decision already made for you

#458 (the combo tracker) is **deferred for 3.1**, and **#496 owns the cross-game tracker slice outright**. Earlier drafting told you to negotiate that boundary with #458; there is no counterparty, so proceed unilaterally and note the decision on both issues.

#458 is also not a prerequisite and not a substrate: it reads the *inactive* game's frozen shadow and is staleness-labelled. Your spoiler view reads `gComboCtx.foreignPlacements` through the live `foreign_items.h` surface, which already exists.

## Step 1 — #489, and steps 1–3 land together

Three faults, and the issue is emphatic that partial landing is worse than nothing: landing step 3 alone gives named rows in a window that cannot open; landing 1–2 alone gives an openable window of blank rows.

On the `CheckNames` fault specifically — `PopulateCheckNames()` already exists and is complete (`Rando/StaticData/Checks.cpp:2326-2330`, array at `:11`). **Nothing in Checks.cpp needs to change.** The defect is that its only caller is `BenPort.cpp:874`, and `BenPort.cpp` is CMake-excluded (`games/mm/CMakeLists.txt:238`). The fix is a **new call site** — `MM_Rando_Init` in `games/mm/2s2h/GameExports_SingleExe.cpp:1372-1373`, or `TrackersGuiSingleExe.cpp`.

For #489 step 2, take **route B**. The issue offers two routes and route A (SohMenu `WIDGET_WINDOW_BUTTON` rows in `SohGui/SohMenuEnhancements.cpp`) is Lane 4's file. Route B is a `Show()`-on-persisted-CVar helper called from `MM_TrackersGui_Init` in `TrackersGuiSingleExe.cpp`, inside your ownership. Same for #496 step 4: read the CVar live in `Draw()` rather than adding a `SohGui/Menu.cpp` row.

## Step 2 — #496

The paired world's cross-game spoiler exists only as a JSON file on disk (`randomizer-mm/RSBSPAIR<masterSeed>.json`) — the operator had to be told the absolute path. Build the in-game view.

Note the generation path **cannot** be reused as a view: `GenerateFromSaveContext` reads `gSaveContext` / `RANDO_SAVE_CHECKS` through MM's layout, and the view must read `gComboCtx` instead. That is why no `Rando/Spoiler/*` file needs editing here.

#496 step 5 is a registration-seam decision that may want an ADR. Your default is the shared `Ship::Context` Gui pattern #457 established — if that is the decision, record it; do not hit it as an open question mid-task.

## Files you own

`games/mm/2s2h/TrackersGuiSingleExe.cpp` and `.h`, `games/mm/2s2h/Enhancements/Trackers/*`, `games/mm/2s2h/mm_trackers_gui_test.cpp`, new `src/common/combo_spoiler_view.{h,c}`, and its harness at **`src/common/tests/test_combo_spoiler_view.c`** (the `tests/` path, matching `test_foreign_items.c` — not `src/common/`). In `games/mm/2s2h/GameExports_SingleExe.cpp` you have `MM_Rando_Init` only; Lanes 1, 3 and 6 hold other functions there.

`Rando/CheckTracker/*` is read-mostly — `CheckTracker.cpp` already reads its CVar live at `:454-457` and needs no change under #489; only the ItemTracker does.

## Do not touch

`context.h` / `foreign_items.*` (Lane 1 — read the accessors; ask on #493 if you need a new one), `Rando/Spoiler/*` (Lane 2 holds `Apply.cpp` for #488 step 6; nothing there is yours), `Rando/Foreign.cpp` (Lane 2), `z_sram_NES.c` / `mm_rando_gen_test.cpp` (Lane 3), `SohGui/*` (Lane 4), `CheckQueue.cpp` / `DrawItem.cpp` (Lane 6).

## Non-negotiables

- **Locks stay in the default, display-free `redship` label.** `MMTrackersGui` (`CMake/SingleExecutable.cmake:475`) passes no `LABEL` and the test file's own header says `CTest label "redship"`. Do **not** add `LABEL rando` — that is the xvfb seed-generation tier, and moving there drops your rows out of the `redship` run entirely.
- The #489 bar is what the issue designs, and it is reachable in that tier: an `MM_TrackersGui_ShouldShow(name)` visibility predicate that is RED today, plus `Rando::StaticData::CheckNames[<RC_* id>]` non-empty and equal to its `convertEnumToReadableName` form. Both are pure — no ImGui, no save. Do **not** assert "non-zero checks for the active MM game": that implies a populated `RANDO_SAVE_CHECKS`, i.e. a real rando save, which this harness deliberately does not build (`mm_trackers_gui_test.cpp:20-28`).
- Guard your edit to the vendored `Enhancements/Trackers/ItemTracker/ItemTracker.cpp:383-386` with `RSBS_SINGLE_EXECUTABLE`. An unguarded divergence in a vendored 2S2H TU is an upstream-sync landmine.
- Window names stay `MM `-prefixed. SoH owns the unprefixed four on the shared Gui and `Gui::AddGuiWindow` rejects duplicates with a **silent** no-register from the caller's side.
- Keep the `MMActiveGated<Base>` wrapper on every MM window. `gSaveContext` storage is unified, so an MM tracker drawing while OoT is active reads OoT bytes through MM's layout, and the shared Gui calls `Update()`/`Draw()` every frame regardless of active game.
- Keep tracker icon textures gated on `MM_Rando_AssetsReady()` — the string-path `LoadGuiTexture` crashes on a missing resource, and the harness reaches this path windowed but archive-free.

## Validation depends on Lane 3

`CheckTrackerWindow::Draw` prints "No Rando Save Loaded" and returns when `!MM_gPlayState || !IS_RANDO` (`CheckTracker.cpp:478-486`), and the `saveType` clobber Lane 3 is fixing (#487) flips that mid-session. **Do not judge a tracker fix against a playtest that hits it.** Your operator verification waits on #487.

## Stop and report if

The no-data cause turns out to be upstream of the trackers — `RANDO_SAVE_CHECKS` genuinely empty on the arrival path rather than merely unread. That is an arrival-rehydration finding in the #482/#487 class and belongs to Lane 3.
