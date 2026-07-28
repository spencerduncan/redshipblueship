/**
 * @file mm_unified_save_test.cpp
 * ROM-free, display-free lock for MM's redship-native unified-save capture
 * (#35 follow-up). CTest label "redship", row MMUnifiedSaveCapture in
 * CMake/SingleExecutable.cmake, dispatch "mm-unified-save-capture" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. In single-exe builds MM persisted nothing at all:
 * games/mm/CMakeLists.txt filters out 2s2h/SaveManager/*.cpp, and the linked
 * replacement (games/mm/2s2h/mm_save_manager_stubs.c) returns -1 from
 * SaveManager_SysFlashrom_ReadData and has an EMPTY WriteData body. Every MM
 * save route bottoms out there, so the only thing that ever deposited real MM
 * bytes into the cross-game shadow was the departure freeze on a portal
 * crossing. An operator-supplied redship_slot0.redsave confirmed the
 * consequence directly: Tier-2 (OoT) held 16647 non-zero bytes while Tier-3
 * (MM) was 65536 bytes of ZERO, with ComboContext.sourceGame pinned to
 * GAME_OOT because the only GAME_MM writer lived in the excluded file.
 *
 * THE INVARIANTS THIS LOCKS, and how each one fails without the fix:
 *
 *   1. Slot normalization. RsbsSave_SetActiveSlot must reject out-of-range
 *      values -- above all MM's 0xFF fileNum sentinel -- as "no slot" rather
 *      than clamping. Clamping 0xFF to a real slot would let one session
 *      overwrite an unrelated one.
 *
 *   2. No slot means no write. With no active slot the capture must be an
 *      honest no-op returning 0, not a guess at slot 0. (An MM session started
 *      from a never-saved new file is exactly this case.)
 *
 *   3. Capture reaches the FILE, not just memory. With a slot established, the
 *      capture must produce a .redsave whose Tier-3 round-trips back through a
 *      real Load. Before the fix there was no capture entry point at all, so
 *      this cannot even be expressed.
 *
 *   4. sourceGame says MM. Before the fix the only two linked writers both
 *      stamped GAME_OOT unconditionally, so no save could ever record that the
 *      player was in MM -- the durable half of "remember which game".
 *
 *   5. FULL-WIDTH capture. This is the subtle one. The excluded SaveManager.cpp
 *      mirrored only sizeof(Save) (0x100C) of the 0x10000 blob, and
 *      Context_UpdateShadowCopy deliberately does NOT zero the tail -- so a
 *      prefix write leaves everything past Save (eventInf, cycleSceneFlags, the
 *      timer arrays, the runtime respawn table, ShipSaveContext) at whatever a
 *      DIFFERENT point in time left there. The test stamps a byte near the end
 *      of MM's real SaveContext and requires it to survive the round trip, so
 *      re-introducing a prefix write fails here instead of silently shipping
 *      stale tail bytes. Note the tail is pre-poisoned with a distinct value
 *      first: without that, a prefix write would leave zeros there and a
 *      "did the byte survive" check could pass vacuously against a zeroed
 *      buffer rather than against genuinely stale data.
 */

#include "global.h"

#include "save.h"
#include "context.h"
#include "game.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

extern "C" SaveContext gSaveContext;
extern "C" int MM_Combo_CaptureSaveToUnifiedSlot(void);

namespace {

#define USAVE_ASSERT(cond, msg)                                           \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// Offset well past sizeof(Save), used to prove the capture is full-width.
size_t TailProbeOffset() {
    return sizeof(SaveContext) - 1;
}

} // namespace

extern "C" int MM_UnifiedSaveCapture_RunHeadless(void) {
    // Isolated save directory so this never touches a real player's slots.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "rsbs_mm_unified_save_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    rsbs::SaveManager::Instance().SetSaveDirectory(dir.string());

    Context_InitFrozenStates();
    ComboContext_Init();

    // ---- 1. Slot normalization -------------------------------------------
    RsbsSave_SetActiveSlot(1);
    USAVE_ASSERT(RsbsSave_GetActiveSlot() == 1, "an in-range slot must be accepted verbatim");
    RsbsSave_SetActiveSlot(0xFF);
    USAVE_ASSERT(RsbsSave_GetActiveSlot() == -1,
                 "MM's 0xFF fileNum sentinel must normalize to 'no slot', never clamp to a real one");
    RsbsSave_SetActiveSlot(-7);
    USAVE_ASSERT(RsbsSave_GetActiveSlot() == -1, "a negative slot must normalize to 'no slot'");
    RsbsSave_SetActiveSlot(RSBS_SAVE_MAX_SLOTS);
    USAVE_ASSERT(RsbsSave_GetActiveSlot() == -1, "one past the last slot must normalize to 'no slot'");

    // ---- 2. No slot means no write ---------------------------------------
    USAVE_ASSERT(MM_Combo_CaptureSaveToUnifiedSlot() == 0, "capture with no active slot must be an honest no-op");
    USAVE_ASSERT(RsbsSave_HasSave(0) == 0, "a no-slot capture must not have invented slot 0");

    // ---- 3/4/5. Full-width capture that round-trips through the file ------
    memset(&gSaveContext, 0, sizeof(SaveContext));

    const char kPlayerName[8] = { 'O', 'W', 'L', 'T', 'E', 'S', 'T', '\0' };
    memcpy(gSaveContext.save.saveInfo.playerData.playerName, kPlayerName, sizeof(kPlayerName));
    gSaveContext.save.isOwlSave = true;
    gSaveContext.save.owlWarpId = 5;

    // Poison the shadow's tail with a value that is neither zero nor the byte
    // we are about to stamp, so check 5 cannot pass vacuously: if the capture
    // regressed to a sizeof(Save) prefix write, the tail would still read as
    // this poison and the assertion below would fail loudly.
    const uint8_t kPoison = 0xA5;
    const uint8_t kTailStamp = 0x3C;
    {
        std::vector<uint8_t> poison(MM_SAVE_CONTEXT_SIZE, kPoison);
        Context_UpdateShadowCopy(GAME_MM, poison.data(), poison.size());
    }

    reinterpret_cast<uint8_t*>(&gSaveContext)[TailProbeOffset()] = kTailStamp;

    RsbsSave_SetActiveSlot(2);
    USAVE_ASSERT(MM_Combo_CaptureSaveToUnifiedSlot() == 1, "capture with an active slot must commit");
    USAVE_ASSERT(gComboCtx.sourceGame == GAME_MM, "an MM-authored save must record sourceGame == GAME_MM");
    USAVE_ASSERT(RsbsSave_HasSave(2) == 1, "capture must have produced a readable slot file");

    // Wipe every trace from memory, then reload from disk. This is what makes
    // the check about the FILE rather than about the shadow we just wrote.
    Context_ClearAllFrozenStates();
    {
        const uint8_t* cleared = static_cast<const uint8_t*>(Context_GetMMSaveContext());
        USAVE_ASSERT(cleared != nullptr, "MM shadow must exist after a clear");
        USAVE_ASSERT(cleared[TailProbeOffset()] == 0, "clear must have zeroed the MM shadow tail");
    }

    USAVE_ASSERT(RsbsSave_Load(2) == 1, "the captured slot must load back");

    {
        const uint8_t* mm = static_cast<const uint8_t*>(Context_GetMMSaveContext());
        USAVE_ASSERT(mm != nullptr, "MM shadow must exist after a load");
        USAVE_ASSERT(memcmp(mm + offsetof(SaveContext, save.saveInfo.playerData.playerName), kPlayerName,
                            sizeof(kPlayerName)) == 0,
                     "MM player name must survive the .redsave round trip");
        USAVE_ASSERT(mm[offsetof(SaveContext, save.isOwlSave)] == 1,
                     "isOwlSave must survive the round trip (owl-statue resume depends on it)");
        USAVE_ASSERT(mm[TailProbeOffset()] == kTailStamp,
                     "a byte past sizeof(Save) must survive: the capture must be full-width, not a "
                     "sizeof(Save) prefix write over a non-zero tail");
    }

    // Leave global state clean for whatever runs next.
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    RsbsSave_SetActiveSlot(-1);
    std::filesystem::remove_all(dir, ec);

    printf("[TEST] mm-unified-save-capture: PASS\n");
    return 0;
}
