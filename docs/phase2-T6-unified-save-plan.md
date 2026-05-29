# PLAN — Phase 2 T6 / Issue #35: Unified Save System

## 0. TL;DR

Add a host-side `SaveManager` (in `src/common/save.{h,cpp}`) that writes one binary `.redsave` file per slot containing three tiers — a fixed header (`REDSHIP1` + version + slot), the cross-game `ComboContext`, the full OoT `gSaveContext`, and the full MM `gSaveContext`. It is fed by the existing in-memory shadow copies (`Context_GetOoTSaveContext` / `Context_GetMMSaveContext` / `gComboCtx`) that the freeze/restore system already maintains, so it does **not** parse either game's JSON save format. Both games' existing per-game save/load paths are left intact and **mirrored** into the unified file via their `OnSaveFile` / `OnLoadFile` GameInteractor hooks. A unified file-select panel is added to `ComboMenuBar` reading the three-tier headers. Round-trip + version unit tests register as new CTest targets next to the existing ones.

---

## 1. Ground truth gathered from the code (read before coding)

| Fact | Source | Implication for T6 |
|---|---|---|
| `ComboContext` canonical def: `magic[8]="OoT+MM<3"`, `uint32_t version`, switch fields, `uint32_t sharedFlags[64]`, `uint16_t sharedItems[32]`, `int32_t saveSlot`, `bool sourceIsRando`, `uint32_t sharedRandoSeed` | `src/common/context.h:91-110` | This is the Tier-1 payload. **`sizeof(ComboContext)` ≈ 8+4+1+4+2+4+2 + 256 + 64 + 4 + 1 + 4 ≈ 360–440B with padding, NOT 256B.** The issue's "~256 bytes" is a stale estimate — the plan sizes the tier from `sizeof(ComboContext)` at compile time, not a magic number. |
| Canonical sizes: `OOT_SAVE_CONTEXT_SIZE 0x1428` (5160B), `MM_SAVE_CONTEXT_SIZE 0x48C8` (18632B) | `src/common/game.h:32-33`; OoT `z64save.h:354` `} SaveContext; // size = 0x1428` | These match the issue's "~5KB / ~18KB". **The on-disk OoT tier uses the real N64 struct size only if we serialize the N64 struct. But SoH's runtime `gSaveContext` is larger (`ship.*` extension).** See the size-conflict resolution in §3.1 — this is the single biggest correctness risk. |
| **Conflicting macro**: `unified_save.c:30` redefines `OOT_SAVE_CONTEXT_SIZE` as `0x22000` (~138KB) and allocates `gSaveContext[UNIFIED_SAVE_SIZE]` at that size | `src/common/unified_save.c:30-43` | `game.h` and `unified_save.c` disagree on `OOT_SAVE_CONTEXT_SIZE`. They are not co-included today (different TUs), so no compile error — but T6 will include both transitively and **must reconcile them** (§3.1). |
| In-memory shadow copies of both SaveContexts already exist and are kept current | `context.cpp` `FrozenStateManager`; `Context_GetOoTSaveContext()`/`GetMMSaveContext()` (`context.h:75-85`), `Context_UpdateShadowCopy()` | SaveManager reads these for serialization instead of touching the live `gSaveContext` directly. This reuses T-existing freeze/restore plumbing as required. |
| Both games are **JSON** SaveManagers writing `file{N}.sav`, not raw SRAM | OoT `SaveManager::GetFileName` → `Save/file{N+1}.sav` (`SaveManager.cpp:54-57`); MM `SaveManager_GetFileName` (`2s2h/SaveManager/SaveManager.cpp:199`), `type:"2S2H_SAVE"` | The unified `.redsave` is a **separate, parallel** binary file; we do not replace per-game JSON saves. The `.redsave` is the cross-game superset that lets a single slot restore both games. |
| OoT save entry points (C bridge): `Save_SaveFile` → `SaveManager::Instance->SaveFile(gSaveContext.fileNum)` (`SaveManager.cpp:2801`); `Save_LoadFile`/`LoadFile` (`:1249`); `Save_Exist`/`SaveFile_Exist` (`:1335`) | `SaveManager.cpp:2790-2845`, `SaveManager.h:208-219` | Concrete OoT integration anchor. |
| OoT GameInteractor hooks: `OnSaveFile(fileNum, sectionID)`, `OnLoadFile(fileNum)`, `OnExitGame(fileNum)` | `games/oot/soh/Enhancements/game-interactor/GameInteractor_HookTable.h:11,58,59` | Cleanest non-invasive OoT hook surface — register callbacks, no edits to save core logic. |
| MM has matching GameInteractor hook table | `games/mm/2s2h/GameInteractor/GameInteractor_HookTable.h` (confirm `OnSaveFile`/`OnLoadFile` names there) | MM integration mirrors OoT via its own GI. |
| MM save persistence funnels through `SaveManager_SysFlashrom_WriteData(buf,page,count)` and `..._ReadData` (the emulated SRAM flush) | `2s2h/SaveManager/SaveManager.cpp:305,430` | If MM's GI lacks an `OnSaveFile`, this is the fallback hook point (wrap these). |
| File-select screens (per game, N64 gamestates): OoT `games/oot/src/overlays/gamestates/ovl_file_choose/z_file_choose.c`; MM `games/mm/src/overlays/gamestates/ovl_file_choose/z_file_choose_NES.c` | `find` results | Native file-select is per-game and N64-style. The unified file-select per the issue lands in the **host** menu (`ComboMenuBar`), not the N64 overlay (§6). |
| Host menu: `ComboMenuBar : Ship::GuiMenuBar` with `DrawComboSettings()` | `src/common/ComboMenuBar.{h,cpp}` | Home for the unified file-select panel. |
| Playtime/name fields: OoT `gSaveContext.playTime` (u32) + `totalDays` (`z64save.h:246`), `playerName[8]` (`:250`); MM `gSaveContext.save.day`/`time` (`z64save.h:419`), `playerData.playerName[8]` (`:308`) | grep | Source fields for "combined playtime / slot name / last game" file-select metadata. |
| CTest pattern: `add_test(NAME X COMMAND redship --test <name>)` + `set_tests_properties(... TIMEOUT 60 LABELS "redship")` | `CMake/SingleExecutable.cmake:186-226` | New tests register here. |
| Test registry: `gTests[]` array of `{name, description, runFunc}` (`TestDescriptor`), dispatched by `--test <name>` | `src/common/test_runner.cpp:333-344`, `test_runner.h:34` | New round-trip/version tests add entries here. |
| `unified_save.c` is compiled by `SingleExecutable.cmake:28`; `src/common/CMakeLists.txt` `COMMON_SOURCES` does **not** list it | `CMakeLists.txt` both | `save.cpp` must be added to **both** build paths (single-exe target list + the standalone `redship_common` lib so headless unit tests can link it). |
| T10 (#265) "remove `combo/`" depends on T6 + T8 + T9; `combo/include/{Export.h,GameExports.h}` still referenced by `SingleExecutable.cmake:69,112`; `combo/tests/context_test.cpp` still uses `OOT_/MM_SAVE_CONTEXT_SIZE` | `gh issue 265`; `combo/` listing | §9 details how finishing T6 clears the last blocker. |

---

## 2. Design decisions (resolve before writing code)

1. **Binary, little-endian, host-native, fixed layout.** The file is read/written only by `redship` on the same machine that wrote it; both shipped targets are LE (x86-64 / arm64). We do **not** byteswap. Endianness is documented in the header (`endian` byte = `1` for LE) and asserted on load so a future BE port fails loudly rather than silently corrupting. No struct is sent over the wire or shared between architectures.
2. **Serialize the raw in-memory `gSaveContext` byte blobs, not JSON.** The two games already persist their own semantic JSON saves. T6's job is cross-game *coexistence in one slot*, and the freeze/restore system already round-trips the raw bytes correctly (proven by `Test_Roundtrip`). Re-parsing JSON would duplicate both SaveManagers and couple T6 to their schemas. **Consequence:** a `.redsave` is tied to the exact `gSaveContext` struct layout of the build that wrote it; the header `version` + per-tier `oot_struct_size`/`mm_struct_size` fields guard against loading a mismatched build (§5).
3. **`.redsave` is a parallel superset, not a replacement.** Per-game JSON saves keep working unchanged (preserves backward compat, lets users who never cross games behave exactly as before, and de-risks T6). The unified file is written *in addition*, triggered off the same save events.
4. **Source of truth for serialization = the shadow copies**, not live `gSaveContext`. On save we pull from `Context_GetOoTSaveContext()` / `Context_GetMMSaveContext()` (updated for the inactive game at freeze time) and `memcpy` the *active* game's live `gSaveContext` into its shadow first. On load we write both shadow copies, restore `gComboCtx`, then `Context_RestoreState` pushes the active game's bytes into its live `gSaveContext`. This keeps T6 layered strictly above the existing context API (matches ADR-0001: `src/common/` host layer).
5. **C++ class with a C shim.** Game code is C and uses C bridges (`Save_*`); the SaveManager is C++ (matches OoT/MM SaveManagers and the menu). Expose `extern "C"` `RsbsSave_*` wrappers for the hook callbacks.

---

## 3. On-disk format (Tier 0–3)

### 3.1 Size-conflict resolution (DO THIS FIRST — blocks everything)

`game.h` says OoT save is `0x1428`; `unified_save.c` allocates `0x22000`. SoH's *runtime* `gSaveContext` is the larger SoH struct (extra `ship.*`). The N64 `// size = 0x1428` comment refers to the original sub-struct, not SoH's full object.

**Resolution:**
- Introduce **`SaveContextBlobSizes`** resolved at compile time from the actual storage, not hardcoded. Add to `save.h`:
  - `OOT_SAVE_BLOB_SIZE` = the size the OoT shadow buffer is allocated at (today `OOT_SAVE_CONTEXT_SIZE` as seen by `context.cpp`, i.e. the `game.h` value `0x1428`). **Verify at runtime** that this equals `sizeof(SaveContext)` as OoT sees it — see action below.
  - `MM_SAVE_BLOB_SIZE` = `MM_SAVE_CONTEXT_SIZE` (`0x48C8`), confirmed consistent everywhere.
- **Action item (sequenced as W1 in §8):** reconcile the two `OOT_SAVE_CONTEXT_SIZE` definitions. Add a `_Static_assert` / runtime check in a TU that *can* see OoT's real `sizeof(SaveContext)` (e.g. a new check in `combo/`→`src/common` test or an OoT-side init export) confirming the shadow-buffer size ≥ `sizeof(SaveContext)`. If SoH's real struct is `> 0x1428`, then **`context.cpp` is currently truncating OoT saves at freeze time** (it `memcpy`s `min(size, 0x1428)`), which is a latent bug T6 must surface. The plan's W1 step is: measure the real size, set `OOT_SAVE_BLOB_SIZE` to cover it, and make `game.h` + `unified_save.c` agree (single source of truth in `game.h`, `unified_save.c` `#include "game.h"`).
- The `.redsave` stores each tier's actual byte length in the header so the file is self-describing regardless of which constant a given build used.

> This conflict is called out explicitly because the issue assumes "~5KB" but the running code allocates 138KB for the same tier. The plan does not guess; W1 measures and unifies.

### 3.2 Header (Tier 0) — fixed 32 bytes

```c
// src/common/save.h
#define RSBS_SAVE_MAGIC      "REDSHIP1"   // 8 bytes, NOT null-terminated on disk
#define RSBS_SAVE_VERSION    1            // bump on any layout change
#define RSBS_SAVE_ENDIAN_LE  1

#pragma pack(push, 1)
typedef struct RsbsSaveHeader {
    char     magic[8];        // "REDSHIP1"
    uint32_t version;         // RSBS_SAVE_VERSION
    uint8_t  endian;          // 1 = little-endian
    uint8_t  slot;            // 0..2
    uint16_t headerSize;      // sizeof(RsbsSaveHeader) == 32 (forward-compat)
    uint32_t comboSize;       // bytes of Tier 1 actually written = sizeof(ComboContext)
    uint32_t ootSize;         // bytes of Tier 2 = OOT_SAVE_BLOB_SIZE at write time
    uint32_t mmSize;          // bytes of Tier 3 = MM_SAVE_BLOB_SIZE at write time
    uint32_t crc32;           // CRC32 over Tiers 1..3 (0 = unchecked, see §5)
} RsbsSaveHeader;             // = 8+4+1+1+2+4+4+4+4 = 32 bytes
#pragma pack(pop)
_Static_assert(sizeof(RsbsSaveHeader) == 32, "RsbsSaveHeader must be 32 bytes");
```

### 3.3 Full file layout

```
offset 0x00                      RsbsSaveHeader            (32 B, packed)
offset 0x20                      ComboContext              (comboSize B)
offset 0x20 + comboSize          OoT gSaveContext blob     (ootSize B)
offset 0x20 + comboSize+ootSize  MM gSaveContext blob      (mmSize B)
EOF
```

- All multi-byte integers little-endian. `RsbsSaveHeader` is `#pragma pack(1)` so its 32-byte size is stable across compilers; the three payload blobs are opaque raw struct images (their internal padding is whatever the writing build produced — fine because only the same build version reads them, gated by `version` + sizes).
- **File location:** `Ship::Context::GetPathRelativeToAppDirectory("Save") / ("redship_slot" + N + ".redsave")`, reusing the same `Save/` directory both games already use (`SaveManager.cpp:55`). Slots `0..2` to match `SaveManager::MaxFiles == 3` (`SaveManager.h:154`).
- **Atomic write:** write to `redship_slotN.redsave.tmp`, `fflush`+`close`, then `std::filesystem::rename` over the real file (mirrors OoT's temp-then-rename in `SaveFileThreaded`, `SaveManager.cpp:1173-1201`). Guarantees no half-written slot.

---

## 4. SaveManager API (`src/common/save.h` / `save.cpp`)

```cpp
// src/common/save.h
#pragma once
#include "context.h"          // ComboContext, Context_* shadow accessors
#include "game.h"             // GameId, OOT/MM size macros (single source post-W1)
#include <cstdint>
#include <iosfwd>
#include <string>

namespace rsbs {

struct SlotMeta {             // cheap header+name read for file-select (§6)
    bool     exists      = false;
    bool     valid       = false;   // magic+version+size checks passed
    uint8_t  slot        = 0;
    GameId   lastGame    = GAME_NONE;   // from ComboContext.sourceGame/targetGame
    char     ootName[9]  = {0};         // OoT playerName, NUL-terminated
    char     mmName[9]   = {0};         // MM playerName
    uint32_t ootPlayTime = 0;           // gSaveContext.playTime
    uint32_t mmPlayTime  = 0;           // derived from MM day/time
    uint32_t combinedPlayTimeSec = 0;   // see §6 for unit normalization
    bool     ootStarted  = false;       // progress indicator (file valid flag)
    bool     mmStarted   = false;
};

class SaveManager {
public:
    static SaveManager& Instance();     // simple function-local static singleton

    bool Save(int slot);                // serialize all three tiers -> slotN.redsave
    bool Load(int slot);                // deserialize + push into shadows & gComboCtx
    bool HasSave(int slot) const;       // file exists AND header valid
    void DeleteSave(int slot);          // remove file (+ .tmp/.bak siblings)

    SlotMeta ReadMeta(int slot) const;  // lightweight, for file-select

private:
    // per-tier serialize (issue-mandated names)
    void SerializeHeader(std::ostream& out, const RsbsSaveHeader& h);
    void SerializeComboContext(std::ostream& out);
    void SerializeOoTSave(std::ostream& out);
    void SerializeMMSave(std::ostream& out);

    // per-tier deserialize
    bool DeserializeHeader(std::istream& in, RsbsSaveHeader& outHeader);
    bool DeserializeComboContext(std::istream& in, const RsbsSaveHeader& h);
    bool DeserializeOoTSave(std::istream& in, const RsbsSaveHeader& h);
    bool DeserializeMMSave(std::istream& in, const RsbsSaveHeader& h);

    std::string SlotPath(int slot) const;       // Save/redship_slotN.redsave
    static uint32_t Crc32(const uint8_t* data, size_t len);
};

} // namespace rsbs

// ---- C shim for game hook code (C TUs) ----
#ifdef __cplusplus
extern "C" {
#endif
int  RsbsSave_Save(int slot);
int  RsbsSave_Load(int slot);
int  RsbsSave_HasSave(int slot);
void RsbsSave_DeleteSave(int slot);
#ifdef __cplusplus
}
#endif
```

**`save.cpp` behavior:**

- `Save(slot)`:
  1. `Context_UpdateShadowCopy(gCurrentGame, &<active gSaveContext>, <size>)` — refresh the *active* game's shadow from live memory. (The active game's live `gSaveContext` is the unified storage from `unified_save.c`; the inactive game's shadow is already current from the last freeze.) The hook callbacks (§5) supply the live pointer/size since `save.cpp` itself must not include either game's `z64save.h`.
  2. Build `RsbsSaveHeader` (fill sizes from `sizeof(ComboContext)`, `OOT_SAVE_BLOB_SIZE`, `MM_SAVE_BLOB_SIZE`).
  3. Open `.tmp` ofstream (binary). Call the four `Serialize*` in order. `SerializeComboContext` writes `&gComboCtx`; `SerializeOoTSave` writes `Context_GetOoTSaveContext()`; `SerializeMMSave` writes `Context_GetMMSaveContext()`.
  4. Compute CRC32 over tiers 1–3, backfill into header (seek to crc field), flush, close, atomic rename.
- `Load(slot)`:
  1. `DeserializeHeader` → validate `magic`, `version` (§5), `endian`, sizes (each tier size must equal the current build's expected size, else version-mismatch path §5).
  2. `DeserializeComboContext` → read into a local `ComboContext`, validate its inner `magic=="OoT+MM<3"`, then assign to `gComboCtx`.
  3. `DeserializeOoTSave` / `DeserializeMMSave` → read directly into the shadow buffers via a `Context_SetShadowCopy`-style path. **New tiny API needed:** the existing `Context_UpdateShadowCopy(game, src, size)` already does exactly this (copies bytes into the shadow). Reuse it — no new context API required.
  4. Push the *current* game's restored shadow into live `gSaveContext` via `Context_RestoreState(gCurrentGame, &<live gSaveContext>, <size>)`. The other game's bytes stay in its shadow and are applied by the existing freeze/restore flow the next time that game becomes active. **This is the key reuse of T-existing machinery** — Load just primes both shadows + `gComboCtx`; the entrance-switch path already knows how to make a shadow live.
  5. CRC check: if mismatch, log + refuse load (return false), do not clobber live state.
- `HasSave` / `ReadMeta`: open, read 32-byte header (+ for meta, also read the two `playerName` offsets and combo `sourceGame`). `ReadMeta` must read name/playtime from fixed offsets *within* the blobs; those offsets are exposed as constants supplied by each game (see §6) so `save.cpp` never includes `z64save.h`.

---

## 5. Versioning, compatibility, migration

- **Forward/back:** `version` in header is the gate. `Load` switch on `version`:
  - `case RSBS_SAVE_VERSION:` normal path.
  - `case <older>:` run a registered migration function `Migrate_vN_to_vN+1(buffer)` chain (none needed at v1; structure the code so adding one is a single function + case).
  - `default` (newer than we understand, or unknown): refuse, surface a popup ("save from a newer RedShip version"), never partial-load.
- **Per-tier size guard:** even at the same `version`, if `ootSize`/`mmSize` in the file ≠ the current build's blob sizes, treat as incompatible (struct layout changed without a version bump = a bug, but we fail safe rather than memcpy a mismatched blob). Log loudly.
- **CRC32** over tiers 1–3 detects truncation/corruption; on failure, fall back to the per-game JSON save (which still exists) and warn — no data loss because the unified file is a superset, not the only copy.
- **Migration from existing single-game saves (ties to #34):** First-run upgrade path — if no `.redsave` exists for a slot but per-game `Save/fileN.sav` (OoT JSON) and/or MM `fileN.sav` do, T6 does **not** auto-merge them in v1 (out of scope; #34 owns settings, this owns saves). Instead: the unified file is created the first time the user saves through the unified flow, capturing whatever is currently loaded. Document this in `docs/`. A future "import legacy slot" is a clean follow-up (flag via a TODO + a tracked issue). This keeps T6 at ~500 LOC as scoped and avoids depending on either game's JSON schema.
- Backup-on-corrupt mirrors OoT (`SaveManager.cpp:1310-1325`): rename bad `.redsave` to `.redsave.bak.<timestamp>`.

---

## 6. Unified file-select (host menu)

**Where:** add a `DrawFileSelect()` method to `ComboGui::ComboMenuBar` (`src/common/ComboMenuBar.{h,cpp}`), surfaced under the existing RedShip menu (`DrawRedShipMenu`/`DrawComboSettings`, `ComboMenuBar.h:39,54`). The native N64 `ovl_file_choose` overlays (`z_file_choose.c`, `z_file_choose_NES.c`) are **left untouched** — they are per-game N64 gamestates and reworking them is far beyond T6's scope and risk budget. The cross-game file-select is a host-GUI panel, consistent with ADR-0001 (host coordination lives in `src/common/`).

**What it renders** (per slot 0–2, via `SaveManager::Instance().ReadMeta(slot)`):
- Slot name — OoT `playerName` if OoT started, else MM `playerName` (both shown if both present).
- Last game played — `SlotMeta.lastGame` from `ComboContext.sourceGame`/`targetGame` → "OoT" / "MM".
- Combined playtime — normalize units: OoT `playTime` is a frame/tick counter; MM tracks `day`+`time`. Define `combinedPlayTimeSec` = secondsFromOoTPlayTime + secondsFromMM. **Exact conversion is a known unknown**; W-step exposes a tiny per-game helper (`OoT_GetPlayTimeSeconds()`, `MM_GetPlayTimeSeconds()`) compiled in each game's TU (where the struct + tick rate are known) and called through a function-pointer table registered at init. This keeps `save.cpp`/menu free of `z64save.h`.
- Progress indicators — `ootStarted`/`mmStarted` booleans (from each blob's `valid`/`newf=="ZELDAZ"` marker for OoT, `IS_VALID_FILE` equivalent for MM). v1: simple "OoT ✓ / MM ✓" badges; richer % is a follow-up.
- Buttons: **Load** (`RsbsSave_Load(slot)` then request boot of `lastGame`), **Delete** (`RsbsSave_DeleteSave`), and **Save to slot** when in-game.

**Offset-exposure mechanism (so `save.cpp`/menu never include game headers):** each game registers, at init, a small struct of byte offsets + sizes for the fields the meta reader needs (`playerNameOffset`, `playTimeOffset`, `validMarkerOffset`, …) plus the play-time helper fn ptr. Registration goes through a new `RsbsSave_RegisterGameMeta(GameId, const RsbsGameMetaDesc*)` C API in `save.h`, called from each game's existing init export (OoT `Save_Init`, MM `SaveManager` init). This is the same dependency-inversion pattern `game_lifecycle.h`'s `GameOps` already uses.

---

## 7. Concrete integration points in both games

**Principle:** hook the *events*, do not rewrite either save core. Both games already fire GameInteractor hooks at exactly the right moments.

### OoT
- **Save:** register an `OnSaveFile` callback (`GameInteractor_HookTable.h:58`). On full-base saves (`sectionID == SECTION_ID_BASE`) call `RsbsSave_Save(gSaveContext.fileNum)` after OoT's own JSON write completes (the hook fires at end of `SaveFileThreaded`, `SaveManager.cpp:1205`). The callback passes OoT's live `&gSaveContext` + `sizeof(SaveContext)` into the shadow-refresh (since OoT's TU knows the type).
  - Registration site: alongside the existing `Save_Init`/`SaveManager::Init` wiring (`SaveManager.cpp:2792`), or in OoT's GI hook-registration init. Pick the GI init so no edits land in `SaveFile` itself.
- **Load:** register `OnLoadFile` (`:59`). After OoT finishes loading a slot's JSON into `gSaveContext`, also call `RsbsSave_Load(fileNum)` **only if a `.redsave` exists for that slot** (`RsbsSave_HasSave`) — this restores the MM shadow + ComboContext so a subsequent cross-game switch is correct. If no `.redsave`, no-op (pure-OoT slot).
- **Exit/quit-to-menu:** `OnExitGame(fileNum)` (`:11`) → `RsbsSave_Save(fileNum)` to capture final state (covers the issue's "quit and restart" gate step).

### MM
- **Save:** MM's GI hook table (`games/mm/2s2h/GameInteractor/GameInteractor_HookTable.h`) — register the equivalent `OnSaveFile`. **If MM lacks an `OnSaveFile` hook**, use the persistence funnel `SaveManager_SysFlashrom_WriteData` (`2s2h/SaveManager/SaveManager.cpp:305`): after it writes a `FLASH_SAVE_FILE_*_NEW_CYCLE_SAVE`, call `RsbsSave_Save(<derived slot>)`. (Verify hook availability in W-step; prefer GI to keep edits out of the flashrom path.)
- **Load:** `SaveManager_SysFlashrom_ReadData` (`:430`) / MM's load completion → `RsbsSave_Load(slot)` when `.redsave` present.
- The MM TU passes `&gSaveContext` + `sizeof(gSaveContext)` (the MM struct) into the shadow refresh.

### Shared glue
- A new `RsbsSave_RegisterGameMeta` call from each game's init (§6) supplies offsets/sizes/playtime-helpers.
- No changes to `Context_*`/`Combo_*` signatures — T6 consumes them as-is. The only context-side touch is confirming `Context_UpdateShadowCopy` is usable for the load-time write (it is).

---

## 8. Ordered, file-by-file work breakdown

> Each step ends building green; tests added incrementally. Branch `claude/t6-unified-save`.

**W1 — Reconcile sizes (blocker, no behavior change).** Modify `src/common/game.h` to be the single source for `OOT_SAVE_CONTEXT_SIZE`/`MM_SAVE_CONTEXT_SIZE`; make `src/common/unified_save.c` `#include "game.h"` and drop its local redefines (or, if the larger SoH size is real, set `game.h`'s value to cover `sizeof(SaveContext)` and add a runtime/static assert). Add `OOT_SAVE_BLOB_SIZE`/`MM_SAVE_BLOB_SIZE` aliases. **Risk:** if this reveals `context.cpp` was truncating OoT, fix the buffer size there too. *Files: `game.h`, `unified_save.c`, possibly `context.cpp`.*

**W2 — Create `src/common/save.h`.** Header struct, magic/version constants, `SaveManager` class decl, `SlotMeta`, `RsbsGameMetaDesc`, C shim + `RsbsSave_RegisterGameMeta`. Static-assert header size. *Create: `save.h`.*

**W3 — Create `src/common/save.cpp`.** Implement Serialize/Deserialize tiers, `Save/Load/HasSave/DeleteSave`, `ReadMeta`, CRC32, atomic temp-rename, version switch + per-tier size guard, C shim. Pull tier data from `gComboCtx` + `Context_Get*SaveContext()`; restore via `Context_UpdateShadowCopy` + `Context_RestoreState`. *Create: `save.cpp`.*

**W4 — Wire into builds.** Add `save.cpp`/`save.h` to `CMake/SingleExecutable.cmake` (next to `unified_save.c`, line ~28) **and** to `src/common/CMakeLists.txt` `COMMON_SOURCES`/`COMMON_HEADERS` so the standalone `redship_common` lib (used by headless tests) links it. *Files: `SingleExecutable.cmake`, `src/common/CMakeLists.txt`.*

**W5 — Headless unit tests + CTest.** Add round-trip & version tests to `src/common/test_runner.cpp` `gTests[]` (§10). Register CTest targets in `SingleExecutable.cmake:190-205`. *Files: `test_runner.cpp`, `SingleExecutable.cmake`.* — green gate before touching game code.

**W6 — OoT integration.** Register `OnSaveFile`/`OnLoadFile`/`OnExitGame` callbacks calling `RsbsSave_*`; call `RsbsSave_RegisterGameMeta` from OoT init; add `OoT_GetPlayTimeSeconds`. *Files: an OoT GI-hooks init TU (e.g. near `games/oot/soh/SaveManager.cpp:2792` or the GI registration file), small new helper.* No edits inside `SaveFile`/`LoadFile` core.

**W7 — MM integration.** Mirror W6 via MM GI hooks (or `SaveManager_SysFlashrom_*` fallback); register MM meta + `MM_GetPlayTimeSeconds`. *Files: `games/mm/2s2h/SaveManager/SaveManager.cpp` or MM GI init, `BenPort.cpp` if needed.*

**W8 — Unified file-select.** Add `DrawFileSelect()` to `ComboMenuBar`, render `ReadMeta` per slot, Load/Delete/Save buttons. *Files: `src/common/ComboMenuBar.{h,cpp}`.*

**W9 — Integration test + docs.** Extend/confirm `Test_Roundtrip` (or add `int-save-roundtrip`) to exercise real Save→switch→Save→Load (§10). Add a short `docs/` note on the `.redsave` format + legacy-save behavior. *Files: `test_runner.cpp`/`integration_test_hooks.cpp`, `SingleExecutable.cmake`, `docs/`.*

**W10 — clang-format + CI.** Run `./run-clang-format.ps1`; ensure both OoT and MM targets build (single-exe) green.

---

## 9. How T6 unblocks T10 (`combo/` removal, #265)

Per `gh issue 265`, T10 is the *last* Phase-2 step and is gated on "the unified save layout being in place." Concretely:

1. **Settles the canonical save-tier sizes / source of truth (W1).** `combo/tests/context_test.cpp` is the only remaining consumer that pulls `OOT_/MM_SAVE_CONTEXT_SIZE` from the `combo/`-era include path. Once T6 fixes `game.h` as the single definition and the unified `.redsave` exists, that test's coverage is superseded by the new `src/common` round-trip tests (W5/W9). T10 can then delete `combo/tests/` without losing coverage.
2. **Removes the last *semantic* reason to keep `combo/include`.** `SingleExecutable.cmake:69,112` still add `combo/include` (for `Export.h`/`GameExports.h`). After T8 (ComboContext) and T9 (SharedGraphics) moved out, T6 establishes the unified-save home in `src/common/`, so there is nothing cross-game left that needs a `combo/` header. T10's "redirect all callers" reduces to dropping those two `include_directories` lines and the `combo/src/placeholder.cpp`.
3. **Confirms the post-T6 layout** that #265's acceptance criterion references ("SharedGraphics lands in `rsbs/` or `src/common/` — pick the destination that fits the rest of the post-T6 layout"). T6 cements `src/common/` as the cross-game host home (save, context, menu, lifecycle all there), so T10 has an unambiguous target.

So after T6 merges, T10 is a mechanical delete-and-unwire: remove `combo/`, drop the two `combo/include` lines, and delete the now-redundant `combo/tests` (coverage replaced by §10 tests).

---

## 10. Test plan

### Headless unit tests (`src/common/test_runner.cpp` `gTests[]`, run as `redship --test <name>`; register in `SingleExecutable.cmake`)

These link `redship_common` + `save.cpp` (W4) and need **no display** — same class as `context`/`lifecycle`:

| Test name | What it asserts |
|---|---|
| `save-roundtrip-tiers` | Fill `gComboCtx` (markers in `sharedFlags`/`saveSlot`) + both shadow buffers (head/tail byte markers like `Test_Roundtrip` uses, `test_runner.cpp:146-198`). `Save(0)` → zero everything → `Load(0)` → assert ComboContext, OoT blob, MM blob byte-identical. |
| `save-header` | After `Save(0)`, read raw file: assert `magic=="REDSHIP1"`, `version==1`, `endian==1`, `slot==0`, `headerSize==32`, sizes == expected blob sizes, CRC nonzero & verifies. |
| `save-has-delete` | `HasSave(0)` false → `Save(0)` → true → `DeleteSave(0)` → false; `.tmp`/`.bak` siblings cleaned. |
| `save-version-reject` | Hand-craft a file with `version==0xFFFF`; assert `Load` returns false and leaves shadows untouched. |
| `save-size-mismatch` | Craft a file with wrong `ootSize`; assert `Load` fails safe (no clobber). |
| `save-crc-corrupt` | Flip a payload byte after writing; assert CRC check fails and `Load` refuses. |
| `save-meta` | Write blobs with known name/playtime offset bytes; assert `ReadMeta` returns expected `lastGame`, names, `combinedPlayTimeSec`. |

CTest registration (append to `SingleExecutable.cmake:190` block, `LABELS "redship"`, `TIMEOUT 60`):
```cmake
add_test(NAME SaveRoundtripTiers COMMAND redship --test save-roundtrip-tiers)
add_test(NAME SaveHeader         COMMAND redship --test save-header)
add_test(NAME SaveHasDelete      COMMAND redship --test save-has-delete)
add_test(NAME SaveVersionReject  COMMAND redship --test save-version-reject)
add_test(NAME SaveSizeMismatch   COMMAND redship --test save-size-mismatch)
add_test(NAME SaveCrcCorrupt     COMMAND redship --test save-crc-corrupt)
add_test(NAME SaveMeta           COMMAND redship --test save-meta)
# add all 7 to the set_tests_properties(...) list
```

### Integration test (boots games; `--integration-test`, `LABELS "integration"`, `TIMEOUT 120`)

Implements the issue's Test Gate (#35) end-to-end. Add `int-save-roundtrip` to `gIntegrationTests[]` (`test_runner.cpp:354`) + hook logic in `integration_test_hooks.cpp`:
1. Boot OoT, start/seed a file, trigger `RsbsSave_Save` via the OoT save hook.
2. Cross-switch to MM (reuse the existing `int-switch-oot-hms-to-mm` machinery), seed + save.
3. Tear down and re-create `SaveManager`; `Load(slot)`.
4. Assert both shadows + `gComboCtx` (entrance, a `sharedFlags` bit) match what was written.

Register:
```cmake
add_test(NAME IntSaveRoundtrip COMMAND redship --integration-test int-save-roundtrip)
# add to the integration set_tests_properties(...) list
```

The issue's named gate `redship --test roundtrip` continues to pass unchanged (T6 is additive); `save-roundtrip-tiers` is the focused unit version of it.

---

## 11. Risks & mitigations

| Risk | Mitigation |
|---|---|
| **OoT blob size truncation** (`game.h` 0x1428 vs `unified_save.c` 0x22000) corrupts saves | W1 measures real `sizeof(SaveContext)`, unifies the macro, adds static/runtime assert. Highest-priority step; gates everything. |
| `ComboContext` not 256B as issue claims | Tier sized from `sizeof(ComboContext)` at compile time + stored in header; never hardcoded. Documented. |
| Struct layout drift between builds silently loads garbage | Header `version` + per-tier size guards + CRC32; fail-safe refuse, fall back to per-game JSON (which still exists). |
| MM may lack a clean `OnSaveFile` GI hook | Fallback to `SaveManager_SysFlashrom_WriteData`/`ReadData` funnel (verified present at `2s2h/SaveManager/SaveManager.cpp:305,430`). W7 picks whichever exists. |
| `save.cpp` accidentally pulling in `z64save.h` (build coupling, breaks headless lib) | Strict rule: `save.cpp`/menu use only raw byte blobs + offsets registered via `RsbsGameMetaDesc`. Enforced by W5 building `redship_common` standalone *without* either game. |
| Playtime unit mismatch (OoT ticks vs MM day/time) | Per-game `*_GetPlayTimeSeconds` helpers compiled where the struct/tick-rate is known; menu only sums seconds. |
| Active vs shadow desync at save time | `Save` refreshes the active game's shadow first (hook passes live ptr); inactive game's shadow already current from freeze. |
| File-select scope creep into N64 overlays | Explicitly out of scope — unified select is host-GUI only (ComboMenuBar), per ADR-0001. |

---

## 12. Out of scope (tracked as follow-ups, not in T6)

- Auto-merging pre-existing per-game JSON saves into a `.redsave` ("import legacy slot") — note as TODO + new issue; depends on each game's JSON schema.
- Reworking the in-game N64 `ovl_file_choose` overlays to be cross-game aware.
- Settings (CVar) migration — owned by #34; T6 only references it for the migration-philosophy alignment.
- Rich progress-percentage indicators (v1 ships boolean started/✓ badges).

---

**Key files** — create: `src/common/save.h`, `src/common/save.cpp`. Modify: `src/common/game.h`, `src/common/unified_save.c` (+ maybe `src/common/context.cpp`) [W1]; `CMake/SingleExecutable.cmake`, `src/common/CMakeLists.txt` [build+tests]; `src/common/test_runner.cpp`, `src/common/integration_test_hooks.cpp` [tests]; `src/common/ComboMenuBar.{h,cpp}` [file-select]; an OoT GI-hooks init TU near `games/oot/soh/SaveManager.cpp:2790-2845`; MM `games/mm/2s2h/SaveManager/SaveManager.cpp` (or MM GI init / `BenPort.cpp`). Anchors verified: OoT `Save_SaveFile`→`SaveFile(gSaveContext.fileNum)` (`SaveManager.cpp:2801`), GI hooks `OnSaveFile/OnLoadFile/OnExitGame` (`GameInteractor_HookTable.h:11,58,59`), MM persistence funnel `SaveManager_SysFlashrom_WriteData/ReadData` (`2s2h/SaveManager/SaveManager.cpp:305,430`), CTest block (`SingleExecutable.cmake:186-226`), test registry `gTests[]` (`test_runner.cpp:333`).
