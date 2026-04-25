# Linux CI Failure Diagnosis: PR #247 and PR #249

**Date:** 2026-04-25
**Branches investigated:**
- `claude/custom-messages-hooks-228` (PR #247) — original head `d54a8ec`, fix pushed as `2822aa8`
- `claude/texture-interp-fix-234` (PR #249) — original head `b69a923`, fix pushed as `578bf91`

**Common base:** both PRs are off `6faede4` (PR #245). main has since advanced to `5669007`.

## (a) Exact compiler/linker errors from each branch

Reproduced locally on Ubuntu 24.04, gcc-11.5.0, ninja, with the exact `cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_REMOTE_CONTROL=1 -DREDSHIP_BUILD_SHARED=ON` invocation from `.github/workflows/generate-builds.yml`. **All ~2877 compile steps succeeded; both PRs fail at the final link step.**

### PR #249 — `claude/texture-interp-fix-234`

```
[1606/1606] Linking CXX executable redship
FAILED: redship 
: && /usr/bin/g++ -O2 -DNDEBUG -rdynamic -pthread -Wl,-export-dynamic
  CMakeFiles/redship.dir/rsbs/src/main.cpp.o -o redship
  -Wl,--start-group  games/oot/libsoh_src.a games/oot/libsoh_port.a ...
/usr/bin/ld: libredship_common.a(mm_stubs.c.o): in function `Ship_GetInterpolationFPS':
mm_stubs.c:(.text+0x500): multiple definition of `Ship_GetInterpolationFPS';
games/oot/libsoh_port.a(OTRGlobals.cpp.o):OTRGlobals.cpp:(.text+0x5970): first defined here
collect2: error: ld returned 1 exit status
ninja: build stopped: cannot make progress due to previous errors.
```

### PR #247 — `claude/custom-messages-hooks-228`

Identical structural pattern. The conflicting symbol is **`GameInteractor_ExecuteOnOpenText`**:

- Old stub at `src/common/mm_stubs.c:93` — `void GameInteractor_ExecuteOnOpenText(int textId) { (void)textId; }`
- New real definition at `games/oot/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp:383` — `void GameInteractor_ExecuteOnOpenText(uint16_t* textId, bool* loadFromMessageTable) { ... }`

Same `multiple definition of 'GameInteractor_ExecuteOnOpenText'` link error.

## (b) Root cause diagnosis

**Same root cause in both PRs**: each PR adds a real implementation of a function that already had a single-exe stub in `src/common/mm_stubs.c`, and neither PR removed the now-shadowed stub. CLAUDE.md explicitly warns about this exact failure mode:

> **MM stubs**: `src/common/mm_stubs.c` has stubs for MM functions not yet ported to single-exe mode. When working on MM integration, check this file for functions that may need real implementations.

**Same TU, same pattern** — both fail in the final `Linking CXX executable redship` step with `multiple definition of <symbol>; first defined here`. Different symbols:

| PR | New real def site | Conflicting stub |
|---|---|---|
| #247 | `games/oot/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp:383` | `src/common/mm_stubs.c:93` (`GameInteractor_ExecuteOnOpenText`) |
| #249 | `games/oot/soh/OTRGlobals.cpp` (`extern "C" uint32_t Ship_GetInterpolationFPS()`) | `src/common/mm_stubs.c:156` (`int Ship_GetInterpolationFPS(void) { return 20; }`) |

**Why Windows passes**: the MSVC linker is invoked with `/FORCE:MULTIPLE` (set in `games/oot/CMakeLists.txt:562`), which silently picks one of the duplicate definitions. The gcc/ld link command on Linux has no such permissiveness and rejects the duplicate symbol outright. Hence Windows green / Linux red, with no source change to MSVC's behavior.

**Why both PRs ship at once**: each PR was authored and CI-tested independently, both off `6faede4`, and both happened to add a different real implementation of a different stubbed symbol. Neither author updated `mm_stubs.c`. The shared signature on the failure (Linux fails / Windows passes / both jobs / both branches) made it look like a common environment issue, but it's actually two independent instances of the same one-line oversight.

**This is not a "main moved on" problem.** Current main is green; PR #250 (which restructured `OTRGlobals.cpp` heavily) was tested independently and passes. Rebasing onto current main does not fix either branch — both branches additionally need the duplicate stub removed.

## (c) Recommended fix path

**Surgical patch on each branch** — exactly what was applied:

```diff
# PR #249 — src/common/mm_stubs.c
 /* Ship enhancement stubs */
-int Ship_GetInterpolationFPS(void) { return 20; }
+/* Ship_GetInterpolationFPS is now defined for real in
+ * games/oot/soh/OTRGlobals.cpp (extern "C") as part of the
+ * scrolling-texture-interpolation port (#234). */
 const char* Ship_GetSceneName(int sceneId) { (void)sceneId; return "Unknown"; }
```

```diff
# PR #247 — src/common/mm_stubs.c
 void GameInteractor_ExecuteOnSaveLoad(int fileNum) { (void)fileNum; }
-void GameInteractor_ExecuteOnOpenText(int textId) { (void)textId; }
+/* GameInteractor_ExecuteOnOpenText is now defined for real in
+ * games/oot/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp
+ * with signature (uint16_t* textId, bool* loadFromMessageTable) as part
+ * of the Custom Messages → Hooks/ShipInit refactor (#228). */
 void GameInteractor_ExecuteOnItemGive(int itemId) { (void)itemId; }
```

Both fixes one-line. PR #249 verified locally — re-ran just the failed link step and `redship` linked successfully. PR #247 fix verified by inspection (identical pattern, same `multiple definition` mode, same `mm_stubs.c` cleanup).

**Note on PR #247 + main:** PR #247 *also* has real merge conflicts against current main in `item_list.cpp` and `randomizer.cpp` from PR #248 (`aa2bbe4`). The stub fix unblocks Linux CI on the branch, but the human reviewer still needs to resolve those conflicts (apply PR #247's per-item `Text article_` + `std::string color_` ctor extension to the new `RG_GANONS_TOWER_*` / `RG_CRAWL` / shuffle-mask rows from PR #248) before merging.

## (d) Action taken

Both fixes pushed with `--force-with-lease`:

```
578bf91  fix: remove duplicate Ship_GetInterpolationFPS stub from mm_stubs.c
         → claude/texture-interp-fix-234

2822aa8  fix: remove duplicate GameInteractor_ExecuteOnOpenText stub from mm_stubs.c
         → claude/custom-messages-hooks-228
```

GitHub auto-triggers CI on both PRs from these pushes — no further action needed to retrigger.

Local verification for PR #249 after the fix:

```
[1/3] Building C object CMakeFiles/redship_common.dir/src/common/mm_stubs.c.o
[2/3] Linking CXX static library libredship_common.a
[3/3] Linking CXX executable redship
```

Build green, `redship` binary produced.
