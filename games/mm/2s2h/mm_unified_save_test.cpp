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
 * write. The other five -- Song of Time new-cycle, the game-over save, the two
 * special saves (first entry to Clock Town, "Dawn of the New Day"), and the
 * periodic autosave -- still bottomed out in the no-op flash stub and persisted
 * ZERO bytes while presenting a completed save. (The autosave is the one
 * exception in kind: it did not run a ceremony and write nothing, it never ran
 * at all -- see check 10.)
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
 *  7b. The funnel's PLACEMENT, not just its presence. Sram_SaveSpecialNewDay
 *      rolls gSaveContext over to the new cycle, marshals, and then restores
 *      day/time/cutsceneIndex so the cutscene can keep playing -- so the live
 *      context after the route holds the OLD cycle while the marshalled bytes
 *      hold the NEW one. The committed Tier-3 must carry the new cycle
 *      (day 0). Committing at the CALL SITE instead of the marshal tail -- the
 *      shape the retired #527/#529 patches used -- would durably record the old
 *      cycle, and only this check goes red for it.
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
 *
 * ---------------------------------------------------------------------------
 * #532 EXTENSION: committing correctly is only half the job -- the EXIT.
 *
 * Checks 1-10 above lock that a save COMMITS. Check 11 locks that the save then
 * SURVIVES the exit taken immediately after it.
 *
 *  11. The owl-save EXIT policy (#532). MM's owl-save exit hands control to
 *      MM_TitleSetup_Init, whose MM_Sram_InitNewSave authors a fresh vanilla
 *      file over the live gSaveContext. In a cross-game session that bootstrap
 *      is what the next hop to OoT freezes, and what OoT's next save then
 *      writes into Tier-3 -- destroying the very Tier-3 every check above just
 *      proved was written correctly. The #530 funnel does not address this: it
 *      made the routes commit, and the commit is precisely what the bootstrap
 *      then eats. MM_Combo_OwlSaveExitToOoT is the decision that prevents it,
 *      and this file is where the two halves belong together: the same test
 *      writes a good MM half and then locks the exit that used to eat it.
 *
 *      The call-site half of that fix -- that the TitleSetup transitions in
 *      z_play.c / z_kaleido_scope_NES.c actually consult this function -- is
 *      NOT observable from here, and a check on the helper alone would pass
 *      whether or not it is wired in. That half is locked as a source-text
 *      invariant in tools/tests/test_repo_invariants.py
 *      (test_titlesetup_transitions_are_guarded_or_marked_exempt). Neither
 *      lock is sufficient alone; together they cover decision and wiring.
 *
 * ---------------------------------------------------------------------------
 * #589 EXTENSION: an MM save commits the WHOLE file, not MM's half of it.
 *
 * Checks 1-11 are all about MM's own half. The operator's 2026-08-04 ruling on
 * #589 says a durable save-and-quit in EITHER half commits BOTH halves, at one
 * generation, in one instant: the saving half live, the other half from its
 * frozen shadow. Check 12 locks that from MM's side, where it is checkable
 * against a REAL route rather than against the choke point in isolation.
 *
 *  12. An MM owl-shaped marshal commits OoT's half too. The OoT shadow is
 *      stamped with a pattern of its own before the marshal; the reloaded
 *      Tier-2 must carry it back WHOLE, alongside MM's Tier-3, at the single
 *      generation the commit advanced. Two ways this goes red: a commit that
 *      captured only the saving half (Tier-2 comes back zeroed or stale), and
 *      a commit that took a generation per half.
 *
 *      This is also where #589's SIBLING concern is settled: func_8014546C's
 *      owl branch memcpys only offsetof(SaveContext, fileNum) bytes. That
 *      truncation is REAL and it is vanilla MM behaviour — but its destination
 *      is sramCtx->saveBuf, MM's N64 flash staging buffer, whose writer is a
 *      no-op stub in single-exe builds. The .redsave capture is a separate,
 *      full-width Context_UpdateShadowCopy of sizeof(SaveContext), so the
 *      truncation cannot reach the unified commit. The check asserts that
 *      relationship instead of asserting the absence of a bug: the route probe
 *      must sit PAST offsetof(SaveContext, fileNum), so a commit that ever
 *      inherited the owl memcpy's width would fail checks 6 and 12 rather than
 *      pass them by accident.
 *
 * ---------------------------------------------------------------------------
 * #590 EXTENSION: the DEATH exit owes more than the save exit does.
 *
 * Check 11 locks the save exits. The game-over "don't continue" prompt carried
 * the identical TitleSetup mechanism but is reached by ordinary play rather than
 * by a deliberate save action, and it is taken from a state where Link is DEAD.
 * That second fact is the whole reason it is a separate decision:
 *
 *  13. The game-over exit policy (#590). Refusing TitleSetup is only half of it.
 *      The launcher's hot-swap freeze captures gSaveContext verbatim, so an exit
 *      that refused TitleSetup and nothing else would freeze health 0 into MM's
 *      shadow — and under #589 that is DURABLE as MM's half of the next whole
 *      commit. The next arrival restores it and re-enters the game-over it just
 *      left. So the exit must also leave the live SaveContext RESUMABLE, and
 *      13a-13d lock decision, wiring-independent behavior, and the two ways the
 *      revive could be wrong: absent (13b/13d) and over-applied (13a/13c).
 *
 *      13d is the one that makes this more than a field assignment. Current
 *      health is RSBS_SHARED_RES_HEALTH_CURRENT, a CONSUMABLE — ONE bar across
 *      both games — and the harvest that publishes it runs at Game_Suspend,
 *      after this exit. A death exit that handed over a dead bar would carry the
 *      death across the crossing into OoT. Both apply sides floor at one heart
 *      and their comments assert a departing game "cannot normally hand over a
 *      dead bar"; this path is the case that premise misses, and 13d is what
 *      keeps the premise true rather than leaving the floor to hide it.
 *
 *      As with check 11, the CALL-SITE half — that
 *      z_kaleido_scope_NES.c's PAUSE_STATE_GAMEOVER_10 leg actually consults
 *      this function — is not observable from here and is locked as a
 *      source-text invariant instead (tools/tests/test_repo_invariants.py,
 *      test_titlesetup_game_over_exit_is_guarded, which additionally pins that
 *      the exemption marker never returns to that file).
 */

#include "global.h"

#include "save.h"
#include "context.h"
#include "shared_resources.h" // check 13d: the shared health bar the death exit hands over
#include "entrance.h"
#include "game.h"

#include "2s2h/Enhancements/Saving/SavingEnhancements.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

extern "C" SaveContext gSaveContext;
extern "C" int MM_Combo_CaptureSaveToUnifiedSlot(void);
extern "C" int MM_Combo_OwlSaveExitToOoT(void);
extern "C" int MM_Combo_GameOverExitToOoT(void);
extern "C" void MM_HarvestSharedResources(void);

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

// The probe byte the ROUTE checks stamp and then look for in the reloaded
// Tier-3. Deliberately NOT TailProbeOffset(): that byte lands inside
// ShipSaveContext, and route 6 (Sram_SaveSpecialEnterClockTown) calls
// SavingEnhancements_AdvancePlaytime, which writes
// shipSaveContext.lastTimeLog -- so the route would erase its own stamp and
// the check would fail for a reason that has nothing to do with the funnel.
// masksGivenOnMoon sits at 0x48CA, four and a half KiB past sizeof(Save)
// (0x100C), so the stamp still proves the capture is full-width rather than a
// sizeof(Save) prefix write; no save route writes it. (Check 5 above keeps the
// literal last byte covered.)
size_t RouteProbeOffset() {
    return offsetof(SaveContext, masksGivenOnMoon);
}

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
    reinterpret_cast<uint8_t*>(&gSaveContext)[RouteProbeOffset()] = stamp;
}

// Poison the MM shadow so "Tier-3 is non-zero" can never pass against leftovers
// and so a reload that restored nothing is visible as zeros.
void PoisonMMShadow(uint8_t value) {
    std::vector<uint8_t> poison(MM_SAVE_CONTEXT_SIZE, value);
    Context_UpdateShadowCopy(GAME_MM, poison.data(), poison.size());
}

// The OoT half, as the frozen shadow a cross-game MM session leaves behind it
// (#589 check 12). Filled uniformly so "came back WHOLE" is a scan, not a
// spot check.
void FillOoTShadow(uint8_t value) {
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, value);
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
}

bool OoTShadowIsUniform(uint8_t value) {
    const uint8_t* oot = static_cast<const uint8_t*>(Context_GetOoTSaveContext());
    if (oot == nullptr) {
        return false;
    }
    for (size_t i = 0; i < OOT_SAVE_CONTEXT_SIZE; i++) {
        if (oot[i] != value) {
            return false;
        }
    }
    return true;
}

// Did `what` produce a real durable commit? Requires: the monotonic #537
// generation advanced by EXACTLY one (a funnel that double-fires would inflate
// the generation the load-time skew check compares against OoT's .sav), a slot
// file exists, and a fresh Load brings back a non-zero Tier-3 carrying `stamp`.
// Prints its own diagnosis and returns false on failure.
//
// `stamp` < 0 skips the stamp comparison, for a route that legitimately writes
// the probe field itself: Sram_SaveSpecialNewDay (check 7b) runs
// Sram_SaveEndOfCycle, which zeroes masksGivenOnMoon (z_sram_NES.c), so the
// route erases its own stamp exactly the way route 6 erased a lastTimeLog-based
// one. That check carries its own, stronger content evidence instead -- the
// committed cycle -- and full-width capture is already locked by checks 5-7.
bool CommitReached(int slot, uint32_t genBefore, int stamp, const char* what) {
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
    if (stamp >= 0 && mm[RouteProbeOffset()] != (uint8_t)stamp) {
        printf("[TEST] FAIL: %s did not round-trip its Tier-3 stamp (got 0x%02X, want 0x%02X)\n", what,
               (unsigned)mm[RouteProbeOffset()], (unsigned)stamp);
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

    // ---- 7b. The INSTANT, not merely the route -----------------------------
    // Sram_SaveSpecialNewDay is the other headless-drivable route (it touches
    // nothing on PlayState but sramCtx, sceneId and actorCtx.sceneFlags), and it
    // is the one that makes the funnel's PLACEMENT falsifiable rather than just
    // asserted in a comment.
    //
    // The route rolls gSaveContext over to the new cycle (Sram_SaveEndOfCycle
    // sets day = 0), marshals, and then RESTORES day/time/cutsceneIndex right
    // after the marshal so the "Dawn of the New Day" cutscene can keep playing.
    // So once the route returns, the live gSaveContext holds the OLD cycle while
    // the bytes the route meant to make durable hold the NEW one. Committing
    // from the marshal tail captures the new cycle; committing at the call site
    // -- the "simpler" hookup, and the one the retired #527/#529 patches used --
    // would durably record the old cycle instead. Song of Time
    // (MSGMODE_NEW_CYCLE_0) has the identical shape but is buried in a message
    // state machine that cannot run headless, so this route stands in for both.
    {
        std::vector<uint8_t> playMem(sizeof(PlayState), uint8_t(0));
        PlayState* play = reinterpret_cast<PlayState*>(playMem.data());
        play->sramCtx.saveBuf = sSaveBuf;

        const s32 kOldCycleDay = 3;
        ArmCrossGameSaveContext(0xB7);
        gSaveContext.save.day = kOldCycleDay;
        const s32 resetsBefore = gSaveContext.save.saveInfo.playerData.threeDayResetCount;
        PoisonMMShadow(0x00);
        const uint32_t gen = gComboCtx.commitGeneration;
        Sram_SaveSpecialNewDay(play);
        // NOTE: no tail stamp for this route -- Sram_SaveEndOfCycle zeroes
        // masksGivenOnMoon, the probe field, so the route would erase its own
        // stamp. The committed cycle below is the stronger evidence anyway.

        // The route really did do both halves: roll the cycle over, then rewind
        // the LIVE context back to the old day. Without this the check below
        // could pass vacuously on a route that simply never restored.
        USAVE_ASSERT(gSaveContext.save.saveInfo.playerData.threeDayResetCount == resetsBefore + 1,
                     "Sram_SaveSpecialNewDay must actually roll the cycle over (end-of-cycle writes)");
        USAVE_ASSERT(gSaveContext.save.day == kOldCycleDay,
                     "Sram_SaveSpecialNewDay must restore the live day after the marshal -- if it does not, "
                     "this check cannot tell marshal-time from call-site-time");

        // CommitReached reloads the committed Tier-3 from disk into the shadow.
        USAVE_ASSERT(CommitReached(kSlot, gen, -1, "Sram_SaveSpecialNewDay"),
                     "the Dawn-of-the-New-Day special save must persist (#530)");

        const uint8_t* mm = static_cast<const uint8_t*>(Context_GetMMSaveContext());
        USAVE_ASSERT(mm != nullptr, "MM shadow must exist after the reload");
        s32 committedDay = -1;
        memcpy(&committedDay, mm + offsetof(SaveContext, save.day), sizeof(committedDay));
        USAVE_ASSERT(committedDay == 0,
                     "the commit must capture the NEW cycle the marshal holds (day 0), not the old cycle the "
                     "route restores immediately after it -- this is exactly why the funnel sits at the "
                     "marshal tail and not at the call site (#530)");
        // Corroborate from a second, independent field, so a zeroed/garbage
        // blob cannot pass the day check by accident.
        USAVE_ASSERT(mm[offsetof(SaveContext, save.saveInfo.playerData.threeDayResetCount)] ==
                         (uint8_t)(resetsBefore + 1),
                     "the committed blob must carry the rolled-over cycle's reset count too");
    }

    // ---- 12. An MM save commits the WHOLE file (#589) ---------------------
    // Runs here, on the owl-shaped marshal, because the owl statue is the
    // route the ruling was written about ("a save-and-quit in one half"). The
    // OoT half is stamped with a pattern of its own before the marshal — that
    // is what a cross-game MM session actually has resident: the frozen shadow
    // the crossing left behind. The commit must carry it, whole, at the SAME
    // single generation advance CommitReached already requires.
    //
    // Without this, an MM save is a HALF-file commit: MM's half persists, OoT's
    // half stays at whatever the .redsave last held, and the two halves of the
    // ONE save drift apart — which is #589's title and #531's mechanism.
    {
        const uint8_t kOoTHalfPattern = 0x6D;
        ArmCrossGameSaveContext(0xB8);
        gSaveContext.save.isOwlSave = true;
        PoisonMMShadow(0x00);
        FillOoTShadow(kOoTHalfPattern);

        const uint32_t gen = gComboCtx.commitGeneration;
        func_8014546C(&sramCtx);
        // CommitReached clears every shadow and reloads from the FILE, so both
        // assertions below are about what reached disk, not about memory.
        USAVE_ASSERT(CommitReached(kSlot, gen, 0xB8, "func_8014546C (owl branch, whole-file commit)"),
                     "the owl route must still commit MM's half (#530)");
        USAVE_ASSERT(OoTShadowIsUniform(kOoTHalfPattern),
                     "an MM-side save must commit OoT's half too, WHOLE, at the same generation -- a save "
                     "that persists only the saving half is a half-file commit (#589)");

        // #589's sibling concern, settled by construction rather than by
        // assertion-of-absence. func_8014546C's owl branch memcpys only
        // offsetof(SaveContext, fileNum) bytes into sramCtx->saveBuf -- real,
        // vanilla, and confined to MM's N64 flash staging buffer, whose writer
        // is a no-op stub in single-exe. The unified capture is a separate
        // full-width Context_UpdateShadowCopy(sizeof(SaveContext)). Pinning the
        // route probe PAST that width is what makes the distinction
        // falsifiable: if the commit ever inherited the owl memcpy's bound, the
        // stamp check inside CommitReached above would fail rather than pass by
        // accident.
        USAVE_ASSERT(RouteProbeOffset() > offsetof(SaveContext, fileNum),
                     "the route probe must sit past the owl marshal's truncated memcpy width, or check 12 "
                     "cannot distinguish a full-width commit from an owl-width one (#589)");
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
        // A day the last PERMITTED commit (check 12's, day 0) cannot have, so
        // the reload below can tell the refused world from the kept one. The
        // tail stamp alone cannot: 7b's end-of-cycle zeroes masksGivenOnMoon,
        // so the probe field is not a reliable discriminator here.
        const s32 kRefusedDay = 9;
        gSaveContext.save.day = kRefusedDay;
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
        s32 keptDay = -1;
        memcpy(&keptDay, mm + offsetof(SaveContext, save.day), sizeof(keptDay));
        USAVE_ASSERT(keptDay != kRefusedDay, "an identity-refused slot must not have absorbed the refused world");
        USAVE_ASSERT(keptDay == 0, "an identity-refused slot must still hold the LAST permitted commit (check 12's), "
                                   "not the refused one");
        USAVE_ASSERT(mm[RouteProbeOffset()] != 0xB6, "the refused commit's tail must not have reached the file either");
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

    // ---- 11. The owl-save exit policy (#532) ------------------------------
    // Ordering is the point: every check above proved a save COMMITS, and the
    // .redsave those commits produced is now GOOD. Everything below is about
    // the exit taken immediately after such a save -- the exit that used to
    // author a vanilla bootstrap straight over it. #530's funnel made the
    // commit happen; it is exactly the commit this exit used to destroy.
    Combo_ClearGameSwitchRequest();

    // 11a. Standalone MM. No OoT session exists to return to, so the vanilla
    //      2ship exit (TitleSetup) must stand and no switch may be requested.
    //      This is the counterfactual that stops the fix from being "always
    //      switch": an implementation that dropped the Context_HasFrozenState
    //      gate would hijack upstream's own save-and-quit and strand a
    //      standalone player in a game they never asked for.
    Context_ClearAllFrozenStates();
    USAVE_ASSERT(Context_HasFrozenState(GAME_OOT) == 0, "test setup: no OoT session must be frozen here");
    USAVE_ASSERT(MM_Combo_OwlSaveExitToOoT() == 0,
                 "a standalone MM session must keep the vanilla TitleSetup exit (#532 gate)");
    USAVE_ASSERT(Combo_IsGameSwitchRequested() == false,
                 "a standalone MM owl-save exit must not request a game switch");

    // 11b. Cross-game MM. An OoT session is frozen — the player walked in
    //      through the Happy Mask Shop and OoT is waiting behind them. The exit
    //      must refuse TitleSetup (return 1) AND actually drive the launcher:
    //      the return value alone is inert, it is the switch request that makes
    //      MM's graph loop break before MM_Sram_InitNewSave can author a
    //      bootstrap over the save the checks above just verified.
    {
        std::vector<uint8_t> ootSession(OOT_SAVE_CONTEXT_SIZE, 0x11);
        Context_FreezeState(GAME_OOT, OOT_ENTR_MARKET_FROM_MASK_SHOP, ootSession.data(), ootSession.size());
    }
    USAVE_ASSERT(Context_HasFrozenState(GAME_OOT) == 1, "test setup: the OoT session must be frozen here");
    USAVE_ASSERT(MM_Combo_OwlSaveExitToOoT() == 1,
                 "a cross-game MM owl-save exit must refuse MM's TitleSetup chain (#532)");
    USAVE_ASSERT(Combo_IsGameSwitchRequested() == true,
                 "refusing TitleSetup is not enough: the exit must request the switch that breaks MM's "
                 "graph loop, or MM keeps running and reaches TitleSetup anyway");

    // 11c. The decision must not depend on the write having landed. A slot
    //      write-latched by #533 persists nothing, and that is precisely when
    //      entering TitleSetup would be worst — the player would lose the
    //      durable half AND have the live session replaced by a vanilla file.
    //      Slot 2 carries a healthy file from checks 3-5, and
    //      RsbsSave_ResetSlotSessionState drops the arming that permitted it,
    //      which is the shape #533 refuses.
    Combo_ClearGameSwitchRequest();
    RsbsSave_ResetSlotSessionState();
    RsbsSave_SetActiveSlot(2);
    USAVE_ASSERT(MM_Combo_CaptureSaveToUnifiedSlot() == 0, "test setup: the capture must be latch-refused here");
    USAVE_ASSERT(MM_Combo_OwlSaveExitToOoT() == 1,
                 "a refused capture must still take the cross-game exit, never MM's TitleSetup (#533 x #532)");
    USAVE_ASSERT(Combo_IsGameSwitchRequested() == true, "a refused capture must still drive the launcher");

    // ---- 13. The game-over "don't continue" exit policy (#590) ------------
    // The same TitleSetup mechanism as check 11, on a DEATH path — and the
    // reason it is a separate decision rather than a second caller of the owl
    // guard is the part this check exists for: refusing TitleSetup is only half
    // of a correct death exit, because the state the launcher would freeze has
    // Link DEAD in it. The other half is leaving gSaveContext resumable, and
    // that half is invisible to the source-text invariant in
    // tools/tests/test_repo_invariants.py — which can see that the call site is
    // wired, and nothing about what the callee owes.
    Combo_ClearGameSwitchRequest();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    ComboContext_Init();

    // 13a. Standalone MM. The vanilla 2ship exit must stand, no switch may be
    //      requested, and — the counterfactual that keeps the revive from
    //      hijacking upstream — the dead SaveContext must be left ALONE. MM's
    //      own TitleSetup chain is about to replace it wholesale; a port that
    //      reached into a standalone player's save here would be editing a game
    //      it was not asked to change.
    Context_ClearAllFrozenStates();
    gSaveContext.save.saveInfo.playerData.health = 0;
    USAVE_ASSERT(MM_Combo_GameOverExitToOoT() == 0,
                 "a standalone MM game-over must keep the vanilla TitleSetup exit (#590 gate)");
    USAVE_ASSERT(Combo_IsGameSwitchRequested() == false, "a standalone MM game-over must not request a game switch");
    USAVE_ASSERT(gSaveContext.save.saveInfo.playerData.health == 0,
                 "a standalone MM game-over must not touch the save at all");

    // 13b. Cross-game MM, Link dead. Refuse TitleSetup, drive the launcher, AND
    //      hand back a bar that can be resumed into. Without the revive the
    //      launcher freezes health 0 into MM's shadow, #589's whole-file commit
    //      makes that durable as MM's half, and the next arrival re-enters the
    //      game-over it just left — the bootstrap's data loss by another route.
    {
        std::vector<uint8_t> ootSession(OOT_SAVE_CONTEXT_SIZE, 0x22);
        Context_FreezeState(GAME_OOT, OOT_ENTR_MARKET_FROM_MASK_SHOP, ootSession.data(), ootSession.size());
    }
    gSaveContext.save.saveInfo.playerData.health = 0;
    gSaveContext.healthAccumulator = -8; // the blow that killed Link, still pending
    USAVE_ASSERT(MM_Combo_GameOverExitToOoT() == 1,
                 "a cross-game game-over exit must refuse MM's TitleSetup chain (#590)");
    USAVE_ASSERT(Combo_IsGameSwitchRequested() == true,
                 "refusing TitleSetup is not enough: the exit must request the switch that breaks MM's "
                 "graph loop, or MM keeps running and reaches TitleSetup anyway");
    USAVE_ASSERT(gSaveContext.save.saveInfo.playerData.health == 0x30,
                 "the death exit must leave MM's live SaveContext RESUMABLE — a frozen dead bar is durable "
                 "under #589 and re-enters the game-over on the next arrival");
    USAVE_ASSERT(gSaveContext.healthAccumulator == 0,
                 "the pending killing blow must not ride the revived bar into the next MM frame");

    // 13c. The revive is CONDITIONAL, not a blanket assignment. The same
    //      PAUSE_STATE_GAMEOVER_10 state is reachable with Link alive (the
    //      !flashSaveAvailable arm of the save prompt), and 0x30 there would
    //      DEMOTE a player who had more than three hearts.
    Combo_ClearGameSwitchRequest();
    gSaveContext.save.saveInfo.playerData.health = 0x50; // five hearts, alive
    USAVE_ASSERT(MM_Combo_GameOverExitToOoT() == 1, "a live-bar cross-game game-over exit still takes the switch");
    USAVE_ASSERT(Combo_IsGameSwitchRequested() == true, "a live-bar game-over exit must still drive the launcher");
    USAVE_ASSERT(gSaveContext.save.saveInfo.playerData.health == 0x50,
                 "the revive must not demote a bar that is already alive");

    // 13d. The SHARED-POOL consequence, which is what makes 13b more than a
    //      field assignment. Current health is a CONSUMABLE — one bar across
    //      both games — and MM_HarvestSharedResources reads playerData.health
    //      verbatim at Game_Suspend, i.e. AFTER this exit runs. Against a fresh
    //      pool the revived 0x30 is published; a dead bar would publish NOTHING
    //      (delta zero from a zero seed), and once a watermark exists it debits
    //      the shared bar outright. Both apply sides floor at one heart, so the
    //      symptom without this is a silent demotion rather than a crash —
    //      exactly the kind that ships.
    Combo_ClearGameSwitchRequest();
    ComboContext_Init();
    // The RAM watermarks are NOT part of gComboCtx, and checks 6-12 above left
    // MM owning this kind's. Without the reset the delta below would be measured
    // against whatever health those checks last harvested, and the assertion
    // would depend on their setup instead of on this exit.
    Combo_ResetSharedResourceWatermarks();
    gSaveContext.gameMode = GAMEMODE_NORMAL; // the harvest gate (Combo_SaveIsLiveFile)
    gSaveContext.save.saveInfo.playerData.health = 0;
    gSaveContext.healthAccumulator = 0;
    USAVE_ASSERT(MM_Combo_GameOverExitToOoT() == 1, "test setup: the cross-game death exit must be taken here");
    MM_HarvestSharedResources();
    {
        uint16_t pooled = 0;
        USAVE_ASSERT(Combo_GetSharedResource(RSBS_SHARED_RES_HEALTH_CURRENT, &pooled),
                     "the departing death exit must publish a health bar at all — a dead bar publishes nothing");
        USAVE_ASSERT(pooled == 0x30, "the shared health bar must carry the revived value, not the dead one");
    }

    // Leave global state clean for whatever runs next.
    Combo_ClearGameSwitchRequest();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    // The watermark table lives outside gComboCtx, so ComboContext_Init above
    // does not reach it. AllTests runs every row in ONE process and check 13d
    // harvests, so leaving MM owning a watermark here would seed the next row's
    // first harvest against this row's leftovers.
    Combo_ResetSharedResourceWatermarks();
    RsbsSave_SetActiveSlot(-1);
    RsbsSave_ResetSlotSessionState();
    std::filesystem::remove_all(dir, ec);

    printf("[TEST] mm-unified-save-capture: PASS\n");
    return 0;
}
