# Cross-tree ODR-divergence audit recipe

A repeatable procedure for the bug class where a type is defined in **both**
game trees with an **identical mangled name**, with **divergent layout or
semantics**, and **one side's implementation TU is excluded from the
single-exe link**. Calls from the excluded side then silently bind to the
compiled side's code against the wrong layout.

Confirmed instances, all fixed: `FlagTable`/SaveEditor (a real static-init
crash), `UIWidgets::WidgetOptions` (#434), `Ship::Menu`/`MenuTypes`
(#446/#452). Full swept classification: **#468**.

## Why CI cannot do this for you

`.github/scripts/check-symbol-collisions.sh` (#375) intersects the *defined*
symbols of the `soh_*` and `2ship_*` archives. **It is structurally blind to
this class**: the excluded side defines no symbols, so the intersection is
empty and the gate reports OK. The same holds for `check-registrar-elision.sh`,
which asks a different question (did registrars survive the link).

Detection must compare **declarations across the two source trees**, not
linked symbols — and the three facts that decide severity (divergence,
exclusion, reachability) need scope resolution, the CMake exclusion list, and
an include-graph walk respectively. #468 records why a per-commit allowlist
gate was judged disproportionate: the raw signal is 320 same-named types and
368 duplicated include guards, the great majority of which are legitimate
per-game parallel definitions (`Actor`, every `EnXxx`) that will never
converge. Run this recipe instead, at the checkpoints below.

## When to run it

- Before any **Lane C un-elision flip** (#392) — `2ship_enh` and
  `2ship_rando_ui` are the queued ones. A flip converts LATENT hits to ARMED
  in bulk, and by construction produces no link error.
- When a previously excluded MM TU is added to the build.
- When adding an `ExecuteHooks` / `Unregister*` / `GetHookData` instantiation
  in an MM TU (see #470 — these are *not* covered by
  `games/mm/include/mm_gi_hook_guard.h`, which poisons only `Register*`).
- When a shared-shape struct that is deliberately cross-bound gains, loses,
  reorders, or retypes a field.

## The three-leg test

A candidate is **ARMED** only if all three hold. Any one missing downgrades it.

1. **Divergent** — the two definitions differ in layout or semantics.
2. **Excluded** — one side's implementation TU is dropped from the single-exe
   link (`games/mm/CMakeLists.txt`, the `list(FILTER ... EXCLUDE ...)` block
   under `if(SINGLE_EXECUTABLE_BUILD)`).
3. **Reachable** — the excluded side's header is nonetheless reachable from a
   *compiled* MM TU, whether by force-include (the `_force_include_cxx` lists)
   or by inclusion from a linked TU.

Classify each hit as **ARMED** / **LATENT** (divergent but the reaching TUs
are currently link-elided — record *which* flip arms it) / **BENIGN** (with
the reason) / **ALREADY FIXED**.

## Divergence forms to look for

Struct size is the obvious one and the least common in practice. All of these
have been observed:

- container substitution (`std::map` vs `std::unordered_map`)
- `std::function` vs raw function-pointer members (~32B vs 8B each)
- same-named enum constant with **different values** — MM `VB_SETUP_TRANSITION`
  == OoT `VB_PLAY_RAINBOW_BRIDGE_CS` == 206, see `src/common/mm_stubs.c`
- differing `std::variant` alternative counts
- extra or missing members (OoT-only `WidgetInfo::raceDisable`)
- **inline static registries that COMDAT-fold into one cross-game registry** —
  invisible to layout comparison; look for it explicitly. See #470: a nested
  `inline static` template member mangles on the enclosing class name and the
  template-argument *name* only, so two registries with divergent payload
  types fold into one object.
- **identical include guards across trees** — a cheap, high-signal proxy.
  `MENU_H`, `MENUTYPES_H`, and `GAME_INTERACTOR_H` were all real hits. A shared
  guard also means cross-inclusion silently drops one side's definitions.

## Sweep 1 — same-named types across the trees

Enumerate declarations, then intersect:

```bash
enum_tree() {
  find "$1" -type f \( -name '*.h' -o -name '*.hpp' \) | while read -r f; do
    grep -HnE '^[[:space:]]*(typedef[[:space:]]+)?(class|struct|enum)([[:space:]]+class)?[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*(:[^;]*)?\{?[[:space:]]*$' "$f"
  done
}
enum_tree games/oot > oot.raw
enum_tree games/mm  > mm.raw
```

Extract `name<TAB>path:line`, then `comm -12` the sorted name lists. Expect
~320 hits repo-wide.

**Narrow to the C++ port layers** (`games/oot/soh/` vs `games/mm/2s2h/`) —
this is where mangled-name collisions actually occur, and it cuts ~320 to
~110. The remainder are C actor/struct definitions with no C++ linkage.

## Sweep 2 — resolve the enclosing namespace (the step that matters)

**Do not skip this and do not regex it.** The `S2H` namespace split is the
project's standard fix, and it is what makes 80 of the ~110 port-layer hits
harmless. Only real scope resolution shows it.

Brace-track each declaration's enclosing scope and compare the two sides:

- `OOT=SOH` vs `MM=S2H`, `OOT=UIWidgets` vs `MM=S2H::UIWidgets`,
  `OOT=Ship` vs `MM=S2H::Ship`, `OOT=(global)` vs `MM=S2H` — **already split,
  benign**. Distinct enclosing namespace means distinct mangled names.
- **Same enclosing namespace on both sides — these are the real candidates.**
  The last full sweep left 30, of which 21 were a byte-identical vendored
  `portable-file-dialogs.h`.

A working implementation of the brace-tracker is described in #468; it is
~25 lines and does not need to be perfect, only conservative (over-reporting
scope is fine, under-reporting is not).

## Sweep 3 — duplicated include guards

```bash
grepguards() {
  find "$1" -type f \( -name '*.h' -o -name '*.hpp' \) | while read -r f; do
    g=$(grep -m1 -oP '^\s*#ifndef\s+\K[A-Za-z_][A-Za-z0-9_]*' "$f")
    [ -n "$g" ] && printf '%s\t%s\n' "$g" "$f"
  done
}
```

Intersect and filter to the port layers. Expect ~368 total, ~7 in `soh/` vs
`2s2h/`. Note that headers using `#pragma once` will not appear — check those
separately when a family looks suspicious.

## Sweep 4 — same-named registry / hook-table entries

For any table-driven registry defined in both trees (the `DEFINE_HOOK` tables
are the live example), compare **names against signatures**. Same name plus
divergent signature is the dangerous combination, because the registry symbol
keys on the name alone.

```bash
# compare games/oot/soh/Enhancements/game-interactor/GameInteractor_HookTable.h
# against games/mm/2s2h/GameInteractor/GameInteractor_HookTable.h
```

Last sweep: 16 same-named hook types, **14 with divergent signatures**;
`OnSceneInit` and `OnKaleidoUpdate` differ in arity. Tracked in #470.

## Resolving leg 2 (exclusion)

Read the `if(SINGLE_EXECUTABLE_BUILD)` block in `games/mm/CMakeLists.txt`. As
of this writing the excluded MM sources are `BenPort.cpp`, `BenGui/*.cpp`
(except `UIWidgets.cpp` and `Menu.cpp`, which build into `2ship_rando_ui`),
`DeveloperTools/*`, `GameInteractor/*`, `SaveManager/*`, `PresetManager/*`,
`NameTag/*`, `ShipInit.cpp`, `CrashHandlerExt.cpp`, the byte-identical
resource importers/types, and a few C files.

Also note the **archive semantics**, which decide LATENT vs ARMED:

| Target | Semantics | Consequence |
|---|---|---|
| `2ship_rando` | `WHOLE_ARCHIVE` | always in the link — divergence here is ARMED |
| `2ship_src`, `2ship_port` | plain archive | pulled in by reference |
| `2ship_enh`, `2ship_rando_ui` | plain archive, largely elided | **LATENT** until their flips (#392) |

## Resolving leg 3 (reachability)

Mere *declaration* of a divergent type emits no symbol and is harmless — only
**use** from a linked TU arms it. `OTRGlobals` is the worked example: its two
layouts diverge sharply and MM's allocator TU is excluded, yet every
occurrence in `GameExports_SingleExe.cpp` turns out to be a comment referring
to OoT's `OTRGlobals.cpp`. Grep for the type name, then **read the hits** —
do not count them.

Conversely, watch for force-includes: `games/mm/CMakeLists.txt` force-includes
MM's `GameInteractor.h` into every MM C++ TU, so its declarations are
reachable everywhere even where no `#include` appears.

## Fix conventions

- **Namespace split** — move MM's copy into `namespace S2H` (`#ifdef
  RSBS_SINGLE_EXECUTABLE`-guarded so upstream merges stay clean). The standard
  fix; see #434 and #452 for the worked shape.
- **`MM_` prefix** for `extern "C"` surfaces; see
  `games/mm/include/mm_audio_prefix.h` and `mm_ship_utils_prefix.h`.
- **MM-owned shim** when a namespace split would break allocation — MM's only
  allocator for a class may itself be an excluded TU, in which case an
  `S2H`-namespaced `Instance` would never be allocated. See
  `games/mm/include/mm_game_hooks.h` (#395) for that reasoning.
- **Compile-time poison guard** for member names MM must not touch — see
  `games/mm/include/mm_gi_hook_guard.h`. Note its scope: it covers
  `2ship_port`/`2ship_src`/`2ship_rando`/`2ship_rando_ui`, exempts
  `2ship_enh`, and poisons only `Register*`.

## Locking a deliberate cross-bind

Some cross-binds are intentional and desirable — MM's `Notification::Emit`
resolving to OoT's definition gives one shared toast overlay. These are safe
only by field-identical coincidence, and **no link error will ever catch a
divergence**, because exactly one definition survives.

Lock them with a ROM-free layout test in an MM translation unit, so MM's view
is compiled with MM's own headers and flags. Working examples:

- `games/mm/2s2h/mm_notification_binding_test.cpp` — layouts must be **equal**
- `games/mm/2s2h/mm_gi_shim_test.cpp` — layouts must **differ**
- `games/mm/2s2h/mm_culling_test.cpp`

Each pins field types at compile time in both views *and* compares
sizeof/offsetof fingerprints at runtime. Register new tests through
`redship_add_test()`.
