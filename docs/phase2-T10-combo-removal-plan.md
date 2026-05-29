I now have everything needed. The investigation is conclusive:

- The `combo_tests` gtest suite (`context_test.cpp`, `sanity_test.cpp`) covers the same `Context_*`/`Entrance_*`/`Combo_*`/`Game_*` APIs that the single-exe `test_runner.cpp` (`--test context`, `roundtrip`, `switch-*`, `lifecycle`) already exercises via CTest — but with finer-grained gtest assertions. The gtest suite ONLY builds in the retired dynamic-library path (`add_subdirectory(combo)` in the `else()` branch, further gated by `NOT WIN32`), so in the default `SINGLE_EXECUTABLE_BUILD=ON` it is dead and never compiled.
- `Export.h` is live (one consumer: `src/common/SharedGraphics.h:12`). `GameExports.h` and `placeholder.cpp` are fully dead.
- `COMBO_API` expands to empty on non-Windows; on Windows it's `__declspec(dllexport)` when `COMBO_BUILDING_DLL` is defined (which `redship_common` does at `SingleExecutable.cmake:81`), else `dllimport`.

I have enough to produce the plan. No file writes, no execution.

# T10 / #265 — Remove `combo/` directory (PLAN ONLY — GATED ON T6)

> **Status: BLOCKED — do not execute.** This is the last Phase 2 task. It must run **after T6 (#35)** lands (unified-save layout) and after T8 (#274, done) and T9 (#275, done). The plan below is a pure, no-semantic-change relocation + dead-code deletion. Nothing here should be applied until T6 is merged. See *Execution gate* for why.

## 1. Inventory of remaining `combo/` files + every external reference

After T9, `combo/` contains exactly seven files:

| File | Status | Why |
|---|---|---|
| `combo/CMakeLists.txt` | **Dead in default build** | Defines the `combo` target, only added via `add_subdirectory(combo)` in the `SINGLE_EXECUTABLE_BUILD=OFF` branch (`CMakeLists.txt:284`). Re-compiles `src/common/{game.c,context.cpp,entrance.cpp,SharedGraphics.cpp}` into a separate legacy `combo` lib. |
| `combo/include/combo/Export.h` | **LIVE (1 consumer)** | Provides the `COMBO_API` macro. Included by `src/common/SharedGraphics.h:12`. |
| `combo/include/combo/GameExports.h` | **DEAD** | DLL-loading interface (`GameExports` struct, `GameSymbols`, `GameInitFn`, …). Zero `#include` consumers anywhere in the tree. |
| `combo/src/placeholder.cpp` | **DEAD** | `Combo::GetVersion()` stub. No callers. |
| `combo/tests/CMakeLists.txt` | **Dead in default build** | Builds the `combo_tests` gtest exe; only reached via `combo/CMakeLists.txt:73` `if(BUILD_TESTING AND NOT WIN32)`, itself only added in the OFF branch. |
| `combo/tests/context_test.cpp` | **Dead in default build** | gtest coverage of `Context_*`/`ComboContext_*`/`Entrance_*`/`Combo_*` (incl. #170 return-path regression). |
| `combo/tests/sanity_test.cpp` | **Dead in default build** | gtest sanity + `Game_FromString/ToString/GetOther`. |

**Every external reference to `combo/` (file:line), excluding `combo/` itself and `libultraship/`:**

Root build wiring:
- `CMakeLists.txt:284` — `add_subdirectory(combo)` (inside the `else()` / `SINGLE_EXECUTABLE_BUILD=OFF` legacy branch only)
- `CMakeLists.txt:283` — comment "Dynamic library architecture (combo/ approach)"
- `CMakeLists.txt:239` — comment "combo/ launcher loading game DLLs"
- `CMakeLists.txt:21` — comment "unified combo project"

Include-path wiring (`combo/include` on the include path):
- `CMake/SingleExecutable.cmake:69` — `redship_common` PUBLIC include dir
- `CMake/SingleExecutable.cmake:112` — `redship` PRIVATE include dir
- `games/oot/CMakeLists.txt:305` — per-target `combo/include` (compile targets loop)
- `games/oot/CMakeLists.txt:416` — `../../combo/include`
- `games/mm/CMakeLists.txt:356` — per-target `combo/include` (compile targets loop)
- `games/mm/CMakeLists.txt:428` — `../../combo/include`

Header consumer (the one live source dependency):
- `src/common/SharedGraphics.h:12` — `#include "combo/Export.h"` (for `COMBO_API`)

Macro plumbing that interacts with `Export.h`/`GameExports.h`:
- `CMake/SingleExecutable.cmake:81` — `target_compile_definitions(redship_common PRIVATE COMBO_BUILDING_DLL)` (makes `COMBO_API` = `dllexport` on Windows)
- `games/oot/CMakeLists.txt:272`, `games/mm/CMakeLists.txt:323` — `GAME_BUILDING_DLL` (only consumed by the dead `GameExports.h`; harmless after deletion)

Stale comments mentioning `combo/` (cosmetic, no build impact):
- `games/oot/CMakeLists.txt:303`, `:414`; `games/mm/CMakeLists.txt:354`, `:426`

Non-code references (informational only, not part of the build — leave alone):
- `.claude/tmp/pr-t9-body.md`, `.claude/worker-prompts.md`, `docs/adr/0001-rsbs-vs-src-common.md` (already states T10 deletes the remainder).

> Note: matches for `GameExports_SingleExe.cpp` in `games/*` and `src/common/*` are a **different file** (the single-exe game lifecycle), unrelated to `combo/GameExports.h`. No action.

## 2. `Export.h` / `GameExports.h` disposition

**`GameExports.h` → DELETE outright (dead).** It is the DLL hot-load contract from the abandoned dynamic-library architecture (`GameExports` struct, `HasRequiredExports`, `GameSymbols`, function-pointer typedefs). It has no `#include` consumers. The single-exe build links games statically and dispatches lifecycle through `src/common/game_lifecycle.*` + each game's `GameExports_SingleExe.cpp`, not through these pointers. Do not relocate.

**`Export.h` → RELOCATE content into `src/common/`, then delete `combo/include/combo/Export.h`.** It has exactly one live consumer, `src/common/SharedGraphics.h`. Per ADR-0001 (host-side graphics coordination lives in `src/common/`), the `COMBO_API` macro should move next to its sole consumer. Two acceptable options:

- **Option A (recommended): inline the macro into `SharedGraphics.h`.** Replace `#include "combo/Export.h"` with the macro block directly (or a tiny `src/common/Export.h` if you prefer a separate header). This removes the last reason `combo/include` exists on any include path.
- **Option B: create `src/common/Export.h`** with identical contents and change the include to `#include "Export.h"` (resolved via the existing `src/common` include dir, present on `redship_common`, `redship`, and both games).

Either way the macro definition is byte-identical:
```c
#ifdef _WIN32
    #ifdef COMBO_BUILDING_DLL
        #define COMBO_API __declspec(dllexport)
    #else
        #define COMBO_API __declspec(dllimport)
    #endif
#else
    #define COMBO_API
#endif
```
**No-semantic-change proof:** `COMBO_BUILDING_DLL` stays defined on `redship_common` (`SingleExecutable.cmake:81`), so `COMBO_API` still expands to `__declspec(dllexport)` for the `Combo_*SharedGraphics` symbols on Windows and to nothing on Linux/macOS — identical to today. The macro name `COMBO_API` need not be renamed (out of scope; rename is a separate cosmetic follow-up if desired).

## 3. Where the tests go + CTest/CMake wiring to preserve coverage

**Relocation target:** `src/common/tests/` (matches CLAUDE.md "Test source: `src/common/tests/`"; the dir already holds `test_game_lifecycle.c`).

- `combo/tests/sanity_test.cpp` → `src/common/tests/sanity_test.cpp`
- `combo/tests/context_test.cpp` → `src/common/tests/context_test.cpp`

Both compile against `src/common` headers as-is (`#include "game.h"`, `"context.h"`, `"entrance.h"`) — no source edits needed; those headers are on the `redship_common` include path.

**Coverage-preservation analysis (do this assessment, then pick one path):**

The single-exe CTest suite already covers the same surface as the gtest files, via `src/common/test_runner.cpp` run through `redship --test <name>` (registered in `SingleExecutable.cmake:190-205`):
- `context_test.cpp`'s `Context_*`/`Combo_*` freeze/restore + #170 return-path → covered by `--test context` + `--test roundtrip` (`test_runner.cpp` `Test_Context`, `Test_Roundtrip`, which exercise the same `Combo_CheckCrossGameEntrance` both-sides regression).
- `context_test.cpp`'s `Entrance_*` → covered by `--test switch-oot-mm`, `--test switch-mm-oot`, `--test midos-house`, `--test startup-entrance`.
- `sanity_test.cpp`'s `Game_FromString/ToString/GetOther` → covered by `--test lifecycle` / general boot; equivalent assertions exist.

The gtest suite is **finer-grained** (per-assertion `ComboContext_*` magic/field tests, independent freeze tests) but is **not built at all** in the default `SINGLE_EXECUTABLE_BUILD=ON` configuration (it lives behind the OFF-branch `add_subdirectory(combo)` and an additional `NOT WIN32` gate). So today it provides **zero** coverage in the shipping build.

Given that, choose one:

- **Path 1 (recommended — minimal, matches "pure move"): delete the gtest files with the directory.** Coverage is already preserved by the live `test_runner.cpp` CTest targets. This keeps T10 a clean teardown of dead code and avoids standing up a gtest/FetchContent dependency in the single-exe build that doesn't currently exist there. If you take this path, **first** confirm no assertion in `context_test.cpp` lacks a `test_runner.cpp` equivalent; the only gaps are the `ComboContext_*` struct-field tests (magic string `"OoT+MM<3"`, `sourceIsRando`/`sharedRandoSeed` init). If you want to keep those, port just those few asserts into a new case in `test_runner.cpp` (e.g. extend `Test_Context`) rather than dragging in gtest.

- **Path 2 (preserve gtest verbatim): relocate the suite and wire a `redship_common_tests` gtest target in `src/common/`.** Move both files to `src/common/tests/`, then add to `CMake/SingleExecutable.cmake` under the existing `if(BUILD_TESTING)` block (guard with `NOT WIN32` to mirror current behavior, since internal classes/symbols aren't exported on Windows):
  ```cmake
  if(BUILD_TESTING AND NOT WIN32)
      include(FetchContent)
      FetchContent_Declare(googletest
          GIT_REPOSITORY https://github.com/google/googletest.git
          GIT_TAG v1.14.0)
      set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
      FetchContent_MakeAvailable(googletest)

      add_executable(redship_common_tests
          ${CMAKE_SOURCE_DIR}/src/common/tests/sanity_test.cpp
          ${CMAKE_SOURCE_DIR}/src/common/tests/context_test.cpp)
      target_link_libraries(redship_common_tests redship_common GTest::gtest_main)
      include(GoogleTest)
      gtest_discover_tests(redship_common_tests)
  endif()
  ```
  Caveat: this introduces a network FetchContent fetch into the single-exe build that isn't there today — relevant for the network-isolated Docker build (CLAUDE.md). Path 1 avoids that.

**Recommendation: Path 1**, because the coverage is already live in CTest, the gtest suite is currently dead in the shipping build, and Path 1 keeps T10 a pure teardown with no new build dependency. Port the 3 `ComboContext` field asserts into `test_runner.cpp` only if you want to retain them.

## 4. Exact CMake changes to drop the `combo` subdir/target

All edits are removals of dead/legacy wiring; none affects the live `redship` or test build on OoT or MM.

**a. `CMakeLists.txt`**
- Delete line 284 `add_subdirectory(combo)` (in the `else()` legacy branch). After this, the `else()` branch references no `combo` target. Since `SINGLE_EXECUTABLE_BUILD` defaults ON and upstream guidance is "use upstream repos for standalone," either (i) leave the rest of the `else()` branch intact minus `combo`, or (ii) if T6/cleanup decides to drop the dead OFF branch entirely, remove the whole `else()` block — **but that is a larger decision; keep T10 scoped to just removing the `combo` line** unless explicitly broadened.
- Optional cosmetic: update comments at lines 21, 239, 283 that describe the "combo launcher" architecture.

**b. `CMake/SingleExecutable.cmake`**
- Delete line 69 `${CMAKE_SOURCE_DIR}/combo/include` from `redship_common` includes.
- Delete line 112 `${CMAKE_SOURCE_DIR}/combo/include` from `redship` includes.
- **Keep line 81** (`COMBO_BUILDING_DLL` on `redship_common`) — still required so the relocated `COMBO_API` macro exports `Combo_*SharedGraphics` on Windows. (Rename to e.g. `REDSHIP_COMMON_BUILDING_DLL` only if you also rename `COMBO_API`; out of scope.)

**c. `games/oot/CMakeLists.txt`**
- Delete the `combo/include` include block at lines 304-306 (the `foreach(_t ...) target_include_directories(... combo/include)`).
- Delete the `combo/include` include at lines 415-417 (`../../combo/include`).
- Optional: drop now-inaccurate comments at 303, 414. **Keep line 272 (`GAME_BUILDING_DLL`)** — harmless once `GameExports.h` is gone; removing it is a separate cleanup.

**d. `games/mm/CMakeLists.txt`**
- Delete the `combo/include` include block at lines 355-357.
- Delete the `combo/include` include at lines 427-429.
- Optional: drop comments at 354, 426. Keep line 323 (`GAME_BUILDING_DLL`).

**e. Delete files/dir**
- Remove `combo/` entirely (`combo/CMakeLists.txt`, `combo/include/`, `combo/src/`, `combo/tests/` — including `Export.h` and `GameExports.h` after the `Export.h` content has been relocated per §2, and the test files per the §3 path chosen).

**Why this doesn't break the build:** In the default `SINGLE_EXECUTABLE_BUILD=ON` path, the `combo` target is never added (`add_subdirectory(combo)` is in the OFF branch only), so removing it cannot affect `redship`. The only live cross-cutting dependency is the `combo/include` path serving `combo/Export.h` to `SharedGraphics.h`; §2 relocates that header into `src/common` (already on every relevant include path), so dropping `combo/include` from the four include lists is safe. `GameExports.h`/`placeholder.cpp` have no consumers. The CTest targets in `SingleExecutable.cmake:190-218` are unaffected.

## 5. Safe ordered execution checklist (run AFTER T6)

**Execution gate — why this waits for T6 (#35):** Per the issue, T10 is the *last* Phase 2 task: it runs only after the unified-save layout (T6) is in place and after `combo/`'s remaining contents are relocated (T8 ✅ #274, T9 ✅ #275). T6 touches `src/common/` save/context plumbing (`unified_save`, `gSaveContext`), which is adjacent to `context.cpp`/`SharedGraphics.*` that this task also moves around. Sequencing T10 last avoids merge churn against T6 in `src/common/` and guarantees the directory being deleted has already had everything live extracted from it. T10 itself is a **pure no-semantic-change** change: it relocates one macro header, deletes proven-dead code, and removes dead include paths — no behavior, no symbol, no test result changes.

Ordered steps (do not start until T6 is merged to `main`):

1. **Branch:** `git checkout -b claude/t10-remove-combo` off post-T6 `main`.
2. **Relocate `Export.h`** (§2): inline `COMBO_API` into `src/common/SharedGraphics.h` (Option A) or create `src/common/Export.h` (Option B); update the `#include`.
3. **Handle tests** (§3): per Path 1, port the 3 `ComboContext` field asserts into `src/common/test_runner.cpp` `Test_Context` if you want to retain them (otherwise nothing). (Per Path 2 instead: move both gtest files to `src/common/tests/` and add the `redship_common_tests` target.)
4. **Drop CMake include paths**: edit `CMake/SingleExecutable.cmake` (lines 69, 112), `games/oot/CMakeLists.txt` (304-306, 415-417), `games/mm/CMakeLists.txt` (355-357, 427-429).
5. **Drop the subdir**: remove `add_subdirectory(combo)` at `CMakeLists.txt:284`.
6. **Delete `combo/`**: `git rm -r combo/`.
7. **Configure + build (default config, both games):** `cmake -B build -S .` then `cmake --build build --parallel`. Must succeed with no `combo/Export.h`-not-found or undefined-`COMBO_API` errors.
8. **Run CTest:** `ctest --test-dir build` — `BootOoT`, `BootMM`, `SwitchOoTMM`, `SwitchMMOoT`, `Roundtrip`, `Context`, `AllTests`, and the four integration tests must pass exactly as before. (If Path 2: confirm `redship_common_tests` is discovered and green on Linux.)
9. **Grep guard:** re-run a tree search for `combo/` and `combo/include`/`combo/Export.h`/`combo/GameExports.h` (excluding `libultraship/` and `.claude/tmp` notes) — expect zero remaining build references; only doc/ADR mentions may remain.
10. **clang-format** on touched files (`run-clang-format.ps1` / `.sh`).
11. **Docs:** optionally update ADR-0001 consequences to mark T10 done; CLAUDE.md "Key Directories" still lists `combo/` (line in the directory map) — update it to drop `combo/`.
12. **Commit on the branch; do not open a PR unless asked** (per CLAUDE.md). Cross-check against T6 once more for any new `src/common/` include of `combo/` introduced after this plan was written.

**Files this plan touches (absolute paths):**
- `C:\Users\whokn\redshipblueship\CMakeLists.txt`
- `C:\Users\whokn\redshipblueship\CMake\SingleExecutable.cmake`
- `C:\Users\whokn\redshipblueship\games\oot\CMakeLists.txt`
- `C:\Users\whokn\redshipblueship\games\mm\CMakeLists.txt`
- `C:\Users\whokn\redshipblueship\src\common\SharedGraphics.h` (relocate `COMBO_API`)
- `C:\Users\whokn\redshipblueship\src\common\test_runner.cpp` (Path 1, optional assert port) **or** new `C:\Users\whokn\redshipblueship\src\common\tests\{context_test.cpp,sanity_test.cpp}` (Path 2)
- Deleted: `C:\Users\whokn\redshipblueship\combo\` (all 7 files)
- Optional docs: `C:\Users\whokn\redshipblueship\docs\adr\0001-rsbs-vs-src-common.md`, `C:\Users\whokn\redshipblueship\CLAUDE.md`
