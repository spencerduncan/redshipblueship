/**
 * @file mm_save_route_funnel_test.cpp
 * ROM-free, display-free lock for MM's save-commit FUNNEL (#530, chain B part
 * 2). CTest label "redship", row MMSaveRouteFunnel in
 * CMake/SingleExecutable.cmake, dispatch "mm-save-route-funnel" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. MM's persistence in single-exe is the unified `.redsave`:
 * games/mm/2s2h/SaveManager/*.cpp is filtered out of the link and the
 * replacement stubs are a -1 read plus an EMPTY write, and every slot-addressed
 * flash path is additionally gated on a real 0..2 fileNum that a cross-game
 * session — pinned to the 0xFF sentinel for its entire life — can never
 * satisfy. #527 and #529 hooked the unified capture at TWO call sites (the owl
 * statue, pause save-and-quit). The other FIVE routes kept presenting a
 * completed save while persisting zero bytes:
 *
 *   1. Song of Time's new cycle   (z_message.c, MSGMODE_NEW_CYCLE_0)
 *   2. the game-over save         (z_kaleido_scope_NES.c, GAMEOVER_SAVE_PROMPT)
 *   3. special save: first entry to South Clock Town (Sram_SaveSpecialEnterClockTown)
 *   4. special save: "Dawn of the New Day"           (Sram_SaveSpecialNewDay)
 *   5. autosave                   (2s2h/Enhancements/Saving/SavingEnhancements.cpp)
 *
 * THE FIX SHAPE, and therefore what this file locks. The commit is no longer
 * hooked per call site; it is funneled into MM's save SERIALIZATION layer —
 * func_8014546C and func_80145698, the step that folds the cycle scene flags
 * into the permanent ones, checksums the save and lays the durable image into
 * sramCtx->saveBuf. Every save route in the game calls exactly one of them,
 * immediately before its flash write and ABOVE the fileNum gate, and nothing
 * else calls them at all. So:
 *
 *   - Checks 2/3/4 drive the two funnel functions directly. Those are the whole
 *     mechanism for six of the seven routes: new cycle, owl statue, pause
 *     save-and-quit, game over and "Dawn of the New Day" through
 *     func_8014546C (both of its branches are exercised), first-entry-to-Clock-
 *     Town through func_80145698. Revert the funnel hookup (drop
 *     RSBS_COMMIT_UNIFIED_SAVE() from either function) and those routes persist
 *     nothing again — exactly the #530 defect — and these checks go red.
 *   - Check 5 drives one of those routes END TO END at its real C entry point,
 *     Sram_SaveSpecialEnterClockTown, under the 0xFF sentinel: the route
 *     commits, and it does not touch the flash page tables.
 *   - Check 6 locks the autosave gate. Autosave never even reached the
 *     serialization layer: SavingEnhancements_CanSave rejected fileNum == 255
 *     outright, so the interval elapsed, the icon never drew, and nothing was
 *     persisted. Revert that clause and the "0xFF plus a unified slot" row goes
 *     red.
 *   - Checks 7/8 hold the #533/#568 write latch and the "no active slot" case
 *     at the NEW commit sites: a latched or slotless route must leave the slot
 *     file untouched AND must not advance the monotonic commit generation (a
 *     phantom advance manufactures the cross-artifact skew #531 refuses on).
 *
 * The lock is deliberately the pair "non-zero Tier-3 in the FILE" + "the
 * generation reached the artifact": either alone is weak. A generation that
 * advances in memory proves nothing was written; a non-zero Tier-3 alone could
 * be a leftover from an earlier row.
 *
 * #516 discipline: MM_gPlayState is NULL for this whole test (asserted up
 * front). Everything the funnel reaches must therefore be play-state free —
 * which is the point, since three of the seven routes fire from cutscene and
 * pause-menu code where a headless build has no PlayState at all.
 */

#include "global.h"

#include "save.h"
#include "context.h"
#include "game.h"

#include "2s2h/Enhancements/Saving/SavingEnhancements.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

extern "C" SaveContext gSaveContext;
extern "C" PlayState* MM_gPlayState;

namespace {

#define FUNNEL_ASSERT(cond, msg)                                          \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// Backing store for the flash funnel's saveBuf, and a PlayState for the one
// route driven at its real entry point. Static so neither lands on the stack.
u8 sSaveBuf[SAVE_BUFFER_SIZE];
PlayState sPlay;

constexpr int kSlot = 0;

// A marker that is not zero and not any value the save paths write, stamped
// where only a real full-width capture can carry it: past sizeof(Save), in the
// SaveContext tail that Context_UpdateShadowCopy does not zero.
size_t TailProbeOffset() {
    return sizeof(SaveContext) - 1;
}

size_t CountNonZero(const uint8_t* data, size_t len) {
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        if (data[i] != 0) {
            n++;
        }
    }
    return n;
}

// Put the live SaveContext into a known state for a route, with `stamp` in the
// tail so the round trip can be attributed to THIS commit and not a prior row.
void SeedLiveSave(uint8_t stamp, bool owlSave) {
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    gSaveContext.fileNum = 0xFF; // the cross-game sentinel: no flash slot at all
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.save.isOwlSave = owlSave;
    gSaveContext.save.day = 2;
    reinterpret_cast<uint8_t*>(&gSaveContext)[TailProbeOffset()] = stamp;
}

// Clear both shadows, reload the slot from disk, and report what the FILE holds
// for MM. Returns false if the load itself failed.
bool ReloadAndInspect(uint8_t& outTailByte, size_t& outNonZero) {
    Context_ClearAllFrozenStates();
    if (RsbsSave_Load(kSlot) != 1) {
        return false;
    }
    const uint8_t* mm = static_cast<const uint8_t*>(Context_GetMMSaveContext());
    if (mm == nullptr) {
        return false;
    }
    outTailByte = mm[TailProbeOffset()];
    outNonZero = CountNonZero(mm, MM_SAVE_CONTEXT_SIZE);
    return true;
}

} // namespace

extern "C" int MM_SaveRouteFunnel_RunHeadless(void) {
    printf("[TEST] mm-save-route-funnel: every MM save route commits through one funnel\n");

    // ---- 0. Headless preflight (#516) -------------------------------------
    FUNNEL_ASSERT(MM_gPlayState == nullptr,
                  "this lock must run with no PlayState: the funnel and everything it reaches "
                  "must be play-state free");

    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "rsbs_mm_save_route_funnel_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    rsbs::SaveManager::Instance().SetSaveDirectory(dir.string());
    RsbsSave_ResetSlotSessionState();

    Context_InitFrozenStates();
    ComboContext_Init();

    SramContext sramCtx;
    memset(&sramCtx, 0, sizeof(sramCtx));
    memset(sSaveBuf, 0, sizeof(sSaveBuf));
    sramCtx.saveBuf = sSaveBuf;

    // ---- 1. Establish the slot the way production does ---------------------
    // Opening an empty slot IS the create path; it arms the #533 latch.
    FUNNEL_ASSERT(RsbsSave_LoadSlot(kSlot) == RSBS_LOAD_ABSENT, "an empty slot must open as ABSENT and arm");
    RsbsSave_SetActiveSlot(kSlot);
    FUNNEL_ASSERT(RsbsSave_GetActiveSlot() == kSlot, "the unified slot must be established for this session");

    // ---- 2. func_80145698 — the special-save serializer ---------------------
    // Route covered: Sram_SaveSpecialEnterClockTown (first entry to South Clock
    // Town from the Clock Tower).
    {
        const uint8_t kStamp = 0x51;
        SeedLiveSave(kStamp, /*owlSave=*/false);
        const uint32_t before = gComboCtx.commitGeneration;

        func_80145698(&sramCtx);

        FUNNEL_ASSERT(gComboCtx.commitGeneration == before + 1,
                      "func_80145698 must produce exactly one commit with an advanced generation");
        uint8_t tail = 0;
        size_t nonZero = 0;
        FUNNEL_ASSERT(ReloadAndInspect(tail, nonZero), "the special-save commit must have produced a loadable slot");
        FUNNEL_ASSERT(nonZero > 0, "the special-save commit must leave a NON-ZERO Tier-3 (MM) blob");
        FUNNEL_ASSERT(tail == kStamp, "the special-save commit must carry this route's live SaveContext, full width");
        FUNNEL_ASSERT(gComboCtx.commitGeneration == before + 1,
                      "the advanced generation must have reached the artifact, not just memory");
        FUNNEL_ASSERT(gComboCtx.sourceGame == GAME_MM, "an MM-authored commit must record sourceGame == GAME_MM");
    }

    // ---- 3. func_8014546C, non-owl branch ----------------------------------
    // Routes covered: Song of Time's new cycle, the game-over save, and
    // Sram_SaveSpecialNewDay ("Dawn of the New Day").
    {
        const uint8_t kStamp = 0x52;
        SeedLiveSave(kStamp, /*owlSave=*/false);
        const uint32_t before = gComboCtx.commitGeneration;

        func_8014546C(&sramCtx);

        FUNNEL_ASSERT(gComboCtx.commitGeneration == before + 1,
                      "func_8014546C (non-owl) must produce exactly one commit with an advanced generation");
        uint8_t tail = 0;
        size_t nonZero = 0;
        FUNNEL_ASSERT(ReloadAndInspect(tail, nonZero), "the new-cycle/game-over commit must have produced a loadable slot");
        FUNNEL_ASSERT(nonZero > 0, "the new-cycle/game-over commit must leave a NON-ZERO Tier-3 (MM) blob");
        FUNNEL_ASSERT(tail == kStamp,
                      "the new-cycle/game-over commit must carry this route's live SaveContext, full width");
        FUNNEL_ASSERT(gComboCtx.commitGeneration == before + 1,
                      "the advanced generation must have reached the artifact, not just memory");
    }

    // ---- 4. func_8014546C, owl branch --------------------------------------
    // Routes covered: the owl statue (#527), pause save-and-quit (#529) and the
    // autosave — all three set isOwlSave before serializing.
    {
        const uint8_t kStamp = 0x53;
        SeedLiveSave(kStamp, /*owlSave=*/true);
        const uint32_t before = gComboCtx.commitGeneration;

        func_8014546C(&sramCtx);

        FUNNEL_ASSERT(gComboCtx.commitGeneration == before + 1,
                      "func_8014546C (owl) must produce exactly one commit with an advanced generation");
        uint8_t tail = 0;
        size_t nonZero = 0;
        FUNNEL_ASSERT(ReloadAndInspect(tail, nonZero), "the owl/pause/autosave commit must have produced a loadable slot");
        FUNNEL_ASSERT(nonZero > 0, "the owl/pause/autosave commit must leave a NON-ZERO Tier-3 (MM) blob");
        FUNNEL_ASSERT(tail == kStamp, "the owl/pause/autosave commit must carry the live SaveContext, full width");
        {
            const uint8_t* mm = static_cast<const uint8_t*>(Context_GetMMSaveContext());
            FUNNEL_ASSERT(mm != nullptr && mm[offsetof(SaveContext, save.isOwlSave)] == 1,
                          "isOwlSave must reach the artifact (the owl/pause resume position depends on it)");
        }
    }

    // ---- 5. A real route, end to end, under the 0xFF sentinel ---------------
    // Sram_SaveSpecialEnterClockTown is the one of the five uncaptured routes
    // whose entry point is a plain function; the other four fire from message /
    // pause-menu state machines and cutscene commands that a headless build
    // cannot step. Driving it here proves the funnel is reached through the
    // ROUTE, not only when the serializer is called directly, and that the
    // route stays clear of the flash page tables at fileNum 0xFF.
    {
        const uint8_t kStamp = 0x54;
        SeedLiveSave(kStamp, /*owlSave=*/false);
        gSaveContext.save.isFirstCycle = false;
        const uint32_t before = gComboCtx.commitGeneration;

        memset(&sPlay, 0, sizeof(sPlay));
        sPlay.sramCtx.saveBuf = sSaveBuf;

        Sram_SaveSpecialEnterClockTown(&sPlay);

        FUNNEL_ASSERT(gSaveContext.save.isFirstCycle,
                      "the route itself must still have run (isFirstCycle is its own side effect)");
        FUNNEL_ASSERT(gComboCtx.commitGeneration == before + 1,
                      "Sram_SaveSpecialEnterClockTown must commit exactly once through the funnel");
        uint8_t tail = 0;
        size_t nonZero = 0;
        FUNNEL_ASSERT(ReloadAndInspect(tail, nonZero), "the enter-Clock-Town route must have produced a loadable slot");
        FUNNEL_ASSERT(nonZero > 0, "the enter-Clock-Town route must leave a NON-ZERO Tier-3 (MM) blob");
        FUNNEL_ASSERT(tail == kStamp, "the enter-Clock-Town route must carry the live SaveContext, full width");
    }

    // ---- 6. The autosave gate ----------------------------------------------
    // SavingEnhancements_HasSaveTarget is the clause that made autosave a
    // zero-byte route: "fileNum == 255 -> no". A unified slot is a save target.
    {
        gSaveContext.flashSaveAvailable = true;

        gSaveContext.fileNum = 1;
        RsbsSave_SetActiveSlot(-1);
        FUNNEL_ASSERT(SavingEnhancements_HasSaveTarget(), "a real flash slot must remain a save target");

        gSaveContext.fileNum = 0xFF;
        FUNNEL_ASSERT(!SavingEnhancements_HasSaveTarget(),
                      "the 0xFF sentinel with NO unified slot must not be a save target");

        RsbsSave_SetActiveSlot(kSlot);
        FUNNEL_ASSERT(SavingEnhancements_HasSaveTarget(),
                      "the 0xFF sentinel WITH a unified slot must be a save target — this is the clause that "
                      "made autosave persist nothing in a cross-game session (#530)");

        gSaveContext.flashSaveAvailable = false;
        FUNNEL_ASSERT(!SavingEnhancements_HasSaveTarget(),
                      "no flash buffer means no save target either way: func_8014546C's owl branch memcpys into "
                      "sramCtx->saveBuf unconditionally and Sram_Alloc only allocates it when flash is available");
        gSaveContext.flashSaveAvailable = true;
    }

    // ---- 7. The #533/#568 write latch, at the NEW commit sites -------------
    // A refused slot stays unwritten, and a refused write must not advance the
    // monotonic commit generation.
    {
        const uint8_t kStamp = 0x55;
        SeedLiveSave(kStamp, /*owlSave=*/false);
        RsbsSave_SetActiveSlot(kSlot);
        RsbsSave_RefuseSlotIdentity(kSlot); // the #498/#564 divergent-arrival producer
        FUNNEL_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 0, "an identity refusal must latch the slot against writes");

        const uint32_t before = gComboCtx.commitGeneration;
        func_8014546C(&sramCtx);
        FUNNEL_ASSERT(gComboCtx.commitGeneration == before,
                      "a latched route must not advance the commit generation (a phantom advance fakes the "
                      "cross-artifact skew #531 refuses on)");

        uint8_t tail = 0;
        size_t nonZero = 0;
        (void)nonZero;
        // The slot file is still the previous row's commit; loading it also
        // clears the refusal and re-arms the slot for the rows below.
        FUNNEL_ASSERT(ReloadAndInspect(tail, nonZero), "the latched route must have left the earlier slot file intact");
        FUNNEL_ASSERT(tail == 0x54, "the latched route must NOT have overwritten the slot with its own state");
    }

    // ---- 8. No active slot ---------------------------------------------------
    {
        const uint8_t kStamp = 0x56;
        SeedLiveSave(kStamp, /*owlSave=*/false);
        RsbsSave_SetActiveSlot(-1);

        const uint32_t before = gComboCtx.commitGeneration;
        func_8014546C(&sramCtx);
        FUNNEL_ASSERT(gComboCtx.commitGeneration == before,
                      "a route with no established slot must be an honest no-op, not a guess at slot 0");

        uint8_t tail = 0;
        size_t nonZero = 0;
        (void)nonZero;
        FUNNEL_ASSERT(ReloadAndInspect(tail, nonZero), "the slotless route must have left the slot file intact");
        FUNNEL_ASSERT(tail == 0x54, "the slotless route must NOT have written anything");
    }

    // Leave global state clean for whatever runs next.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    RsbsSave_SetActiveSlot(-1);
    RsbsSave_ResetSlotSessionState();
    std::filesystem::remove_all(dir, ec);

    printf("[TEST] mm-save-route-funnel: PASS\n");
    return 0;
}
