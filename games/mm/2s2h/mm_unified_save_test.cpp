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
 *
 * ---------------------------------------------------------------------------
 * #530 EXTENSION: the two-site allowlist becomes one funnel.
 *
 * Checks 1-5 above lock the CAPTURE. They say nothing about which of MM's save
 * ROUTES reach it, and until #530 only two did: the owl statue and the pause
 * save-and-quit, each rescued by a capture call pasted in beside its flash
 * write. The other four -- Song of Time new-cycle, the game-over save, the two
 * special saves (first entry to Clock Town, "Dawn of the New Day"), and the
 * periodic autosave -- still bottomed out in the no-op flash stub and persisted
 * ZERO bytes while presenting a completed save.
 *
 * The fix routes them through the layer they all converge on:
 * Sram_ComboCommitUnifiedSave, called from the tails of MM's two save-buffer
 * marshallers func_8014546C and func_80145698 (games/mm/src/code/z_sram_NES.c).
 * Checks 6-10 below lock that, and each has the same counterfactual: delete the
 * Sram_ComboCommitUnifiedSave() call from the marshaller a route reaches
 * through, and that route persists nothing again -- HasSave stays 0, the
 * monotonic commit generation does not advance, Tier-3 reloads as zeros.
 *
 *   6. Every marshalling shape commits, once. func_8014546C's owl branch, its
 *      non-owl branch, and func_80145698 each produce a durable commit whose
 *      Tier-3 is non-zero and whose #537 commit generation advanced by exactly
 *      one. Exactly one matters as much as non-zero: a funnel that fires twice
 *      per save would inflate the generation the load-time skew check compares
 *      against OoT's .sav.
 *
 *   7. A whole route, end to end. Sram_SaveSpecialEnterClockTown is driven for
 *      real (it is the one funneled route with no PlayState dependency beyond
 *      &play->sramCtx) at the 0xFF cross-game fileNum, and must commit. This is
 *      also the route that proves the funnel had to sit in BOTH marshallers:
 *      it is the only one that reaches disk through func_80145698.
 *
 *   8. The commit gates still hold at the NEW call sites. A funneled marshal
 *      into a slot this session never established must not write and must not
 *      advance the generation (#533/#568 write latch), and neither must one
 *      into a slot refused for identity divergence (#570,
 *      RSBS_REFUSE_IDENTITY). Widening the set of routes that can commit is
 *      only safe if it does not widen what may be committed INTO.
 *
 *   9. MM_gPlayState stays NULL for all of the above (#516). Two of the
 *      funneled routes fire from cutscene/message state machines that can run
 *      before the play state is published, so the funnel must not have
 *      introduced a play-state dependency. The test asserts the null rather
 *      than merely relying on it.
 *
 *  10. Autosave's gate actually opens. Autosave and the hold-B pause save were
 *      not writing zero bytes -- they never ran at all, because
 *      SavingEnhancements_CanSave rejected the 0xFF fileNum outright. The
 *      widened predicate (SavingEnhancements_HasDurableDestination) must accept
 *      a cross-game session with an active unified slot, still reject one with
 *      no destination at all, and leave the vanilla flash-slot answer alone --
 *      while Sram_FileNumHasFlashSlot keeps the now-reachable autosave from
 *      indexing the flash page tables at 0xFF.
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
extern "C" int MM_Combo_CaptureSaveToUnifiedSlot(void);
// The second save-buffer marshaller. z64save.h declares func_8014546C but not
// this one (it has a single vanilla caller, Sram_SaveSpecialEnterClockTown), and
// #530 makes it one of the two places the cross-game commit funnel lives -- so
// the lock has to be able to drive it. Declared here rather than in z64save.h to
// keep a whole-tree rebuild off a header every MM TU includes.
extern "C" void func_80145698(SramContext* sramCtx);

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

// ---- #530 funnel-check scaffolding ---------------------------------------

// Backing store for the marshallers' saveBuf. Static so the 128 KiB does not
// land on the test's stack (same reason mm_flash_filenum_test.cpp does it).
u8 sSaveBuf[SAVE_BUFFER_SIZE];

// Put gSaveContext into the shape a cross-game MM session actually runs in --
// the 0xFF "no real flash slot" sentinel, flash reported available (Title_Init
// sets it true and nothing clears it) -- and stamp a byte past sizeof(Save) so
// the commit that follows is identifiable in the reloaded Tier-3.
void ArmCrossGameSaveContext(u8 stamp) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    reinterpret_cast<uint8_t*>(&gSaveContext)[TailProbeOffset()] = stamp;
}

// Poison the MM shadow so "Tier-3 is non-zero" can never pass against leftovers
// and so a reload that restored nothing is visible as zeros.
void PoisonMMShadow(uint8_t value) {
    std::vector<uint8_t> poison(MM_SAVE_CONTEXT_SIZE, value);
    Context_UpdateShadowCopy(GAME_MM, poison.data(), poison.size());
}

// Did `what` produce a real durable commit? Requires: the monotonic #537
// generation advanced by EXACTLY one (a funnel that double-fires would inflate
// the generation the load-time skew check compares against OoT's .sav), a slot
// file exists, and a fresh Load brings back a non-zero Tier-3 carrying `stamp`.
// Prints its own diagnosis and returns false on failure.
bool CommitReached(int slot, uint32_t genBefore, u8 stamp, const char* what) {
    const uint32_t genAfter = gComboCtx.commitGeneration;
    if (genAfter != genBefore + 1) {
        printf("[TEST] FAIL: %s must advance the commit generation by exactly 1 (%u -> %u)\n", what,
               (unsigned)genBefore, (unsigned)genAfter);
        return false;
    }
    if (RsbsSave_HasSave(slot) != 1) {
        printf("[TEST] FAIL: %s must have produced a readable slot file\n", what);
        return false;
    }

    // Make the check about the FILE: wipe every trace from memory first.
    Context_ClearAllFrozenStates();
    if (RsbsSave_Load(slot) != 1) {
        printf("[TEST] FAIL: %s produced a slot file that will not load back\n", what);
        return false;
    }

    const uint8_t* mm = static_cast<const uint8_t*>(Context_GetMMSaveContext());
    if (mm == nullptr) {
        printf("[TEST] FAIL: %s left no MM shadow after the reload\n", what);
        return false;
    }
    if (mm[TailProbeOffset()] != stamp) {
        printf("[TEST] FAIL: %s did not round-trip its Tier-3 stamp (got 0x%02X, want 0x%02X)\n", what,
               (unsigned)mm[TailProbeOffset()], (unsigned)stamp);
        return false;
    }

    size_t nonZero = 0;
    for (size_t i = 0; i < MM_SAVE_CONTEXT_SIZE; i++) {
        if (mm[i] != 0) {
            nonZero++;
        }
    }
    if (nonZero == 0) {
        printf("[TEST] FAIL: %s wrote an all-zero Tier-3 -- the #530 shape exactly\n", what);
        return false;
    }
    return true;
}

} // namespace

extern "C" int MM_UnifiedSaveCapture_RunHeadless(void) {
    // Isolated save directory so this never touches a real player's slots.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "rsbs_mm_unified_save_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    rsbs::SaveManager::Instance().SetSaveDirectory(dir.string());
    // #533: start from process-start latch state so earlier tests in the same
    // process cannot have pre-armed the slots this test writes.
    RsbsSave_ResetSlotSessionState();

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
    // #533 armed-session latch: an active slot alone is NOT enough — the slot
    // must have been loaded, created, or erased THIS session, or MM's capture
    // could destroy a refused slot's evidence.
    USAVE_ASSERT(MM_Combo_CaptureSaveToUnifiedSlot() == 0,
                 "capture into a slot this session never loaded/created/erased must be refused (#533 latch)");
    USAVE_ASSERT(RsbsSave_HasSave(2) == 0, "a latched capture must not have written a slot file");
    // Establish the slot the way production does (opening an empty slot arms
    // it for its first write), then the capture must commit.
    USAVE_ASSERT(RsbsSave_LoadSlot(2) == RSBS_LOAD_ABSENT, "an empty slot must open as ABSENT and arm");
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

    // ======================================================================
    // #530: the routes. Everything below runs on slot 0, established the way
    // production establishes one, and with the cross-game 0xFF fileNum.
    // ======================================================================

    // ---- 9. #516: no play state, for every check that follows -------------
    // The Song of Time and "Dawn of the New Day" routes fire from message and
    // cutscene state machines that can run before MM_gPlayState is published.
    // Assert the null instead of merely happening to have one.
    USAVE_ASSERT(MM_gPlayState == NULL, "the #530 funnel checks must run with no play state (#516)");

    const int kSlot = 0;
    RsbsSave_SetActiveSlot(kSlot);
    USAVE_ASSERT(RsbsSave_LoadSlot(kSlot) == RSBS_LOAD_ABSENT, "an empty slot must open as ABSENT and arm");

    SramContext sramCtx;
    memset(&sramCtx, 0, sizeof(sramCtx));
    sramCtx.saveBuf = sSaveBuf;

    // ---- 6. Every marshalling shape commits, once -------------------------
    // func_8014546C, owl branch. Reached by the owl statue, the pause
    // save-and-quit, and (newly) the autosave.
    {
        ArmCrossGameSaveContext(0xB1);
        gSaveContext.save.isOwlSave = true;
        PoisonMMShadow(0x00);
        const uint32_t gen = gComboCtx.commitGeneration;
        func_8014546C(&sramCtx);
        USAVE_ASSERT(CommitReached(kSlot, gen, 0xB1, "func_8014546C (owl branch)"),
                     "the owl-shaped marshal must reach the commit funnel (#530)");
    }

    // func_8014546C, non-owl branch. Reached by Song of Time's new cycle, the
    // game-over save, and Sram_SaveSpecialNewDay.
    {
        ArmCrossGameSaveContext(0xB2);
        gSaveContext.save.isOwlSave = false;
        PoisonMMShadow(0x00);
        const uint32_t gen = gComboCtx.commitGeneration;
        func_8014546C(&sramCtx);
        USAVE_ASSERT(CommitReached(kSlot, gen, 0xB2, "func_8014546C (non-owl branch)"),
                     "the non-owl marshal must reach the commit funnel -- this is the Song of Time / "
                     "game-over / new-day shape (#530)");
    }

    // func_80145698, the other marshaller. Reached only by
    // Sram_SaveSpecialEnterClockTown, which is why the funnel is in both.
    {
        ArmCrossGameSaveContext(0xB3);
        PoisonMMShadow(0x00);
        const uint32_t gen = gComboCtx.commitGeneration;
        func_80145698(&sramCtx);
        USAVE_ASSERT(CommitReached(kSlot, gen, 0xB3, "func_80145698"),
                     "the second marshaller must reach the commit funnel too (#530)");
    }

    // ---- 7. A whole route, end to end -------------------------------------
    // Sram_SaveSpecialEnterClockTown touches nothing on PlayState but
    // &play->sramCtx, so the real route runs headless. At the 0xFF sentinel its
    // flash write is skipped (Sram_FileNumHasFlashSlot) -- the commit inside
    // func_80145698 is the ONLY thing that persists anything at all here.
    {
        std::vector<uint8_t> playMem(sizeof(PlayState), uint8_t(0));
        PlayState* play = reinterpret_cast<PlayState*>(playMem.data());
        play->sramCtx.saveBuf = sSaveBuf;

        ArmCrossGameSaveContext(0xB4);
        PoisonMMShadow(0x00);
        const uint32_t gen = gComboCtx.commitGeneration;
        Sram_SaveSpecialEnterClockTown(play);
        USAVE_ASSERT(CommitReached(kSlot, gen, 0xB4, "Sram_SaveSpecialEnterClockTown"),
                     "the first-entry-to-Clock-Town special save must persist (#530)");
        USAVE_ASSERT(gSaveContext.save.isFirstCycle, "the route's own save-state writes must still happen");
    }

    // ---- 8. The commit gates hold at the NEW call sites --------------------
    // (a) #533/#568 write latch: a slot this session never loaded, created, or
    //     erased stays unwritten, and the monotonic generation must not move --
    //     RsbsSave_Save checks the latch BEFORE staging for exactly that reason.
    {
        const int kUnestablished = 1;
        RsbsSave_SetActiveSlot(kUnestablished);
        ArmCrossGameSaveContext(0xB5);
        PoisonMMShadow(0x00);
        const uint32_t gen = gComboCtx.commitGeneration;
        func_8014546C(&sramCtx);
        USAVE_ASSERT(gComboCtx.commitGeneration == gen,
                     "a funneled marshal into an un-established slot must not advance the commit generation");
        USAVE_ASSERT(RsbsSave_HasSave(kUnestablished) == 0,
                     "a funneled marshal into an un-established slot must not write a slot file (#533/#568)");
    }

    // (b) #570 identity refusal: the slot FILE is healthy, the running session
    //     diverged from the identity frozen at the pair's creation. It latches
    //     through the same gate, so the funnel must stay silent there too.
    {
        RsbsSave_SetActiveSlot(kSlot);
        USAVE_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 1, "the slot must still be writable before the refusal");
        RsbsSave_RefuseSlotIdentity(kSlot);
        USAVE_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 0, "an identity refusal must latch the slot (#570)");

        ArmCrossGameSaveContext(0xB6);
        PoisonMMShadow(0x00);
        const uint32_t gen = gComboCtx.commitGeneration;
        func_8014546C(&sramCtx);
        USAVE_ASSERT(gComboCtx.commitGeneration == gen,
                     "a funneled marshal into an identity-refused slot must not advance the commit generation");

        // The healthy file is still there (identity refusal quarantines
        // nothing), so prove it was not REWRITTEN: its Tier-3 must still carry
        // the previous phase's stamp, not this one's.
        RsbsSave_ResetSlotSessionState();
        Context_ClearAllFrozenStates();
        USAVE_ASSERT(RsbsSave_Load(kSlot) == 1, "the identity-refused slot's healthy file must still load");
        const uint8_t* mm = static_cast<const uint8_t*>(Context_GetMMSaveContext());
        USAVE_ASSERT(mm != nullptr, "MM shadow must exist after the reload");
        USAVE_ASSERT(mm[TailProbeOffset()] == 0xB4,
                     "an identity-refused slot must still hold the LAST permitted commit, not the refused one");
    }

    // ---- 10. Autosave's gate actually opens -------------------------------
    // Autosave and the hold-B pause save never ran at all cross-game: the
    // fileNum == 255 clause rejected them before any marshal. The widened
    // predicate must accept a cross-game session that has somewhere to write.
    {
        memset(&gSaveContext, 0, sizeof(SaveContext));
        gSaveContext.flashSaveAvailable = true;
        gSaveContext.fileNum = 0xFF;

        RsbsSave_SetActiveSlot(-1);
        USAVE_ASSERT(!SavingEnhancements_HasDurableDestination(),
                     "no flash slot and no unified slot means no durable destination");

        RsbsSave_SetActiveSlot(kSlot);
        USAVE_ASSERT(SavingEnhancements_HasDurableDestination(),
                     "a cross-game session with an active unified slot HAS a durable destination -- without "
                     "this, autosave and the hold-B pause save are gated off entirely (#530)");

        // The vanilla answer is unchanged in both directions.
        RsbsSave_SetActiveSlot(-1);
        gSaveContext.fileNum = 0;
        USAVE_ASSERT(SavingEnhancements_HasDurableDestination(), "a real flash slot is still a durable destination");
        gSaveContext.flashSaveAvailable = false;
        USAVE_ASSERT(!SavingEnhancements_HasDurableDestination(),
                     "no flash available and no unified slot is still no destination");

        // ...and the guard that keeps the now-reachable autosave from indexing
        // the flash page tables at the sentinel it just started accepting.
        USAVE_ASSERT(!Sram_FileNumHasFlashSlot(0xFF),
                     "0xFF must still be rejected as a flash slot -- the autosave's page-table index depends on it");
    }

    // Leave global state clean for whatever runs next.
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    RsbsSave_SetActiveSlot(-1);
    RsbsSave_ResetSlotSessionState();
    std::filesystem::remove_all(dir, ec);

    printf("[TEST] mm-unified-save-capture: PASS\n");
    return 0;
}
