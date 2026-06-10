# ADR 0001: `rsbs/` is N64 ABI shims, `src/common/` is the unified host layer

- Status: **Accepted** (2026-05-12)
- Closes: #272 (Phase 1 cleanup follow-up)

## Context

RedShipBlueShip currently has three host-side directories that all sound like
they could be "the unified game engine layer":

- `rsbs/` — header (`rsbs.h`) claims "unified game engine; OoT mode and MM
  mode, like the OoTMM romhack". In practice it contains ~8 `libultra/os`
  thread/event-message shims, 2 `libultra/gu` graphics-utility functions,
  `combo_context.c` (cross-game state struct), and a `placeholder.c`.
- `src/common/` — the *actual* unified layer. ~14 files covering game
  lifecycle (`game_lifecycle.c`), cross-game state freeze/restore
  (`context.cpp`), entrance-based switching (`entrance.cpp`), unified
  `gSaveContext` storage (`unified_save.c`), MM stubs (`mm_stubs.c`),
  integration test hooks, the unified menu bar, etc.
- `combo/` — legacy directory holding `SharedGraphics.cpp` and a small test
  suite; on track for removal (T10 in the Phase 2 plan).

The `rsbs/` README and CMakeLists narrate it as the future home of all
cross-game logic. Every cross-game change since the architecture landed has
gone into `src/common/` instead. `rsbs/context.h` even re-declares
`ComboContext`, so the same struct exists in two headers and is the same
storage at runtime — the duplication is real, not just on-paper.

Phase 2 will add more cross-game work (HMS↔CT switching, archive hot-swap
regression tests, shared flag plumbing, SharedGraphics migration out of
`combo/`). Without picking a side now, Phase 2 will either grow the
`src/common/` vs. `rsbs/` split or revive the original "unified engine"
intent. Both are reasonable; sliding into whichever happens first is not.

## Decision

**`rsbs/` is N64 ABI compatibility shims; `src/common/` is the unified
host-side layer.**

- `rsbs/` keeps `src/libultra/os/*.c` and `src/libultra/gu/*.c` — code that
  emulates the N64 OS/graphics ABI and is identical between the two games.
  Adding *more* libultra shims to `rsbs/` is the natural growth path.
- `src/common/` owns everything that lives above `libultraship` and
  coordinates the two games: game lifecycle, cross-game state, save
  unification, entrance switching, shared menu, test infrastructure.
- The aspirational "unified game engine" framing in `rsbs/rsbs.h` and
  `rsbs/CMakeLists.txt` is retired. It described a future that never
  materialized and that the codebase has spent a year voting against with
  every cross-game PR.

This is Option B (Demotion) from the issue.

## Rationale

1. **Match the code as it actually is.** `src/common/` is where every
   cross-game change has landed since the architecture was established.
   Telling future contributors to look in `rsbs/` would send them to the
   wrong place. Telling them `rsbs/` is N64 ABI shims is accurate and easy
   to verify by reading the directory.
2. **`rsbs/` has a coherent narrower mission.** The libultra OS/gu shims are
   genuinely identical between OoT and MM and don't depend on host-side
   game lifecycle. They're a clean unit. Pulling additional things in would
   weaken that.
3. **Lower migration cost.** Option A (move `src/common/` into `rsbs/`)
   would touch every cross-game file, every CMake target that depends on
   `redship_common`, every include directive in OoT/MM sources, and every
   open Phase 2 branch. Option B leaves the working layer where it is and
   only touches `rsbs/`'s self-narration and the duplicate `combo_context`
   wiring.
4. **Removes the duplicate `ComboContext` definition.** The struct currently
   exists in both `rsbs/include/rsbs/combo_context.h` and
   `src/common/context.h`. The src/common/ copy is the one Combo\_/
   Context\_ APIs (`switch.cpp`, `context.cpp`, `main.cpp`) actually use.
   Demoting the `rsbs/` copy resolves the duplication in the natural
   direction.
5. **Points T10 (combo/ removal) at `src/common/`.** When `combo/`'s
   `SharedGraphics.cpp` migrates out, it goes to `src/common/` (where
   `shared_graphics_win.cpp` already lives), not to `rsbs/`. That keeps
   host-side graphics coordination next to the other host-side
   coordinators.

## Consequences

### Now (this PR, follow-up #272)

- Update `rsbs/include/rsbs/rsbs.h` and `rsbs/CMakeLists.txt` to drop the
  "unified game engine" framing in favor of "N64 ABI compatibility shims".
- Leave `combo_context.c` in place for now (moving it is mechanical but
  touches both games' includes; doing it standalone is cleaner than rolling
  it into this ADR PR).

### Later (Phase 2 follow-ups, not in this PR)

- ~~Migrate `rsbs/src/combo_context.c` and `rsbs/include/rsbs/combo_context.h`
  to `src/common/`. Remove the duplicate struct definition from
  `src/common/context.h` (or remove it from `rsbs/include/rsbs/` if the
  src/common/ form is the canonical one). Pick a single home.~~ Resolved by
  T8 (#274): the `rsbs/` copy was the duplicate and is removed; the
  canonical declaration lives in `src/common/context.{h,cpp}`.
- ~~When `combo/src/SharedGraphics.cpp` is removed (T10), its replacement
  goes to `src/common/` (next to `shared_graphics_win.cpp`).~~ Resolved by
  T9 (#275): `SharedGraphics.cpp` and its header now live in `src/common/`;
  T10 (#265) can delete the remainder of `combo/`.
- Continue adding `libultra/*` shims to `rsbs/` if more are unified out of
  OoT/MM. Continue adding cross-game host logic to `src/common/`.

### What the ADR explicitly does *not* do

- Move `combo_context.c` in this PR. The duplication should be resolved,
  but doing it here would balloon this change into a refactor and obscure
  the Phase 1 cleanup boundary. Tracked as a Phase 2 follow-up.
- Rename `rsbs/` itself. The name is fine and used in build artifacts,
  branch prefixes (`claude/rsbs-*`), and the project's public identity.
  Only its self-description changes.

## Alternatives considered

- **Option A — Migration:** move `src/common/` contents into `rsbs/` over a
  series of PRs, formalizing `rsbs/` as the unified engine layer. Rejected:
  high mechanical cost, would conflict with every open Phase 2 branch, and
  the working layer is already coherent where it lives.
- **Option C — Status quo with documentation:** keep `rsbs/`'s aspirational
  framing and document the split as intentional. Rejected: the framing is
  inaccurate and would keep generating "where does this go?" decisions for
  every new cross-game file.
