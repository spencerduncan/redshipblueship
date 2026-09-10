/**
 * @file mm_death_decline_autosave_test.cpp
 * ROM-free, display-free lock for ADR 0009 decision 4b (#590 / PR #625): MM's
 * cross-game game-over "don't continue" exit is an AUTOSAVE POINT if and only
 * if the Autosave enhancement is on. CTest label "redship", row
 * MMDeathDeclineAutosave in CMake/SingleExecutable.cmake, dispatch
 * "mm-death-decline-autosave" in src/common/test_runner.cpp.
 *
 * THE RULING (operator, 2026-09-06; ADR 0009 decision 4b). When the player
 * declines the "continue?" prompt on MM's game-over screen in a cross-game
 * session, that exit is an autosave point iff the MM Autosave enhancement is
 * on: a whole-file commit through the #569 choke point, taken AFTER the #625
 * revive so the committed health bar is resumable, and it resets the autosave
 * timer. With Autosave off it writes NOTHING at the death moment; the revived
 * MM half rides in RAM until OoT's next commit (the behaviour #625 shipped).
 * It is explicitly NOT a rollback of MM's half.
 *
 * WHY THIS IS ITS OWN ROW rather than a check 14 in mm_unified_save_test.cpp,
 * which already owns the exit's revive (check 13): the "on" half of the iff
 * needs the REAL RegisterAutosave driven through the REAL Autosave CVar, and
 * CVarSetInteger dereferences the Ship::Context singleton -- so this row takes
 * the display-free shared bring-up, which the unified-save row deliberately
 * does not. The exit itself needs no Context (it reads the registrar's armed
 * state, not the CVar), which is what lets check 13 keep driving it
 * Context-free; this row is where the decision that state feeds is locked.
 *
 * THE INVARIANTS THIS LOCKS, and how each fails without the code it names:
 *
 *   1. Autosave OFF writes NOTHING at the death moment. The commit generation
 *      does not advance, no slot file appears, the MM shadow is not even
 *      refreshed, the interval clock is untouched, and the shared-resource
 *      pool is untouched (no harvest at a death). An implementation that
 *      committed unconditionally -- the "4a answer" -- fails every one of
 *      these; one that harvested without committing fails the last.
 *
 *   2. Autosave ON commits the WHOLE file, ONCE, AFTER the revive. The
 *      generation advances by exactly one, the reloaded Tier-3 carries the
 *      REVIVED bar (0x30, accumulator 0) and this leg's probe stamp, the
 *      reloaded Tier-2 is OoT's frozen half WHOLE (decision 4), and the pool
 *      carries 0x30 -- the harvest inside the commit ran after the revive.
 *      A commit placed before the revive fails on the reloaded health; a bare
 *      RsbsSave_Save (no capture) fails on the stamp, because StageCommit
 *      serializes the SHADOW and only the capture refreshes it from live
 *      state; a commit that also fired the periodic route would fail "exactly
 *      one".
 *
 *   3. The clock. With Autosave on and a commit that landed, the enhancement's
 *      own lastSaveTimestamp moves forward -- the same field HandleAutoSave
 *      resets -- and in every other leg (off, gated, refused, standalone) it
 *      does not move at all. A second timer, or a reset on a refused commit,
 *      fails here.
 *
 *   4. gameMode, not fileNum. Every leg runs at fileNum 0xFF (a cross-game
 *      session's permanent value) and the ON leg commits anyway; a death under
 *      GAMEMODE_TITLE_SCREEN with Autosave on still takes the switch and the
 *      revive but commits nothing. A fileNum gate fails leg 2; a missing
 *      gameMode gate fails this leg and would write a bootstrap into a real
 *      player's slot (#532/#590 by one more route).
 *
 *   5. The #533/#568 write latch is respected, and a refused commit is NOT an
 *      autosave: with Autosave on but the slot un-established this session,
 *      nothing is written, the clock does not reset, and the pool is untouched
 *      (#591: the latch is checked before the harvest).
 *
 *   6. Standalone 2ship is untouched. With no frozen OoT session the exit
 *      returns 0, requests no switch, leaves the dead save alone, and commits
 *      nothing even with Autosave on -- the autosave point sits BEHIND the
 *      cross-game gate, not beside it.
 *
 * NON-VACUITY. Each leg first proves the exit was actually reached and ran
 * through the revive -- return value 1, a pending switch request, and the bar
 * at 0x30 -- before asserting anything about what it did or did not commit.
 * "Nothing was committed" is trivially true of a leg that returned early, so
 * the OFF/gated/refused legs are only evidence because the ON leg, driven
 * through the identical entry point, commits. The precondition that
 * MM_gPlayState is NULL is asserted rather than assumed: the commit must not
 * have acquired a PlayState dependency (its scene-flag flush is gated on one).
 *
 * As with checks 11 and 13 of the unified-save row, the CALL-SITE half -- that
 * z_kaleido_scope_NES.c's PAUSE_STATE_GAMEOVER_10 leg consults this function --
 * is locked as a source-text invariant (tools/tests/test_repo_invariants.py,
 * test_titlesetup_game_over_exit_is_guarded).
 */

#include "global.h"

#include "save.h"
#include "context.h"
#include "shared_resources.h"
#include "entrance.h"
#include "game.h"

#include "2s2h/Enhancements/Saving/SavingEnhancements.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "mm_game_hooks.h"
#include <libultraship/bridge/consolevariablebridge.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>

extern "C" SaveContext gSaveContext;
extern "C" int MM_Combo_GameOverExitToOoT(void);

namespace {

#define DDA_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// Must match CVAR_AUTOSAVE_NAME in SavingEnhancements.cpp. A drift shows up as
// the ON leg's armed-state precondition failing, not as a silent pass.
constexpr const char* kAutosaveCVar = "gEnhancements.Autosave";
constexpr int kSlot = 3;

// Probe byte the ON leg stamps into the live SaveContext and looks for in the
// reloaded Tier-3. masksGivenOnMoon sits far past sizeof(Save) and no save
// route writes it (see mm_unified_save_test.cpp's RouteProbeOffset for why not
// the last byte).
size_t ProbeOffset() {
    return offsetof(SaveContext, masksGivenOnMoon);
}

// Byte offsets of the two revive fields inside SaveContext, computed from the
// live struct so the reloaded-shadow reads below cannot drift from the layout.
size_t HealthOffset() {
    return (size_t)(reinterpret_cast<const uint8_t*>(&gSaveContext.save.saveInfo.playerData.health) -
                    reinterpret_cast<const uint8_t*>(&gSaveContext));
}

size_t HealthAccumulatorOffset() {
    return (size_t)(reinterpret_cast<const uint8_t*>(&gSaveContext.healthAccumulator) -
                    reinterpret_cast<const uint8_t*>(&gSaveContext));
}

s16 ReadShadowS16(const void* shadow, size_t offset) {
    s16 value = 0;
    memcpy(&value, static_cast<const uint8_t*>(shadow) + offset, sizeof(value));
    return value;
}

// Drive the enhancement exactly the way the unified menu does (#614): set the
// converged CVar, then re-run the real registrar, which unregisters-then-
// registers from the CVar's current value.
void SetAutosave(bool on) {
    CVarSetInteger(kAutosaveCVar, on ? 1 : 0);
    RegisterAutosave();
}

// The state the kaleido leg hands the exit in a cross-game session: the 0xFF
// no-real-slot sentinel, flash reported available, a live file, Link dead with
// the killing blow still pending in the accumulator, and this leg's stamp.
void ArmDeathState(u8 stamp) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.saveInfo.playerData.health = 0;
    gSaveContext.healthAccumulator = -8;
    reinterpret_cast<uint8_t*>(&gSaveContext)[ProbeOffset()] = stamp;
}

// An OoT session frozen behind the player -- the cross-game gate -- filled
// uniformly so "the whole OoT half came back" is a scan, not a spot check.
void FreezeOoTSession(uint8_t fill) {
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, fill);
    Context_FreezeState(GAME_OOT, OOT_ENTR_MARKET_FROM_MASK_SHOP, oot.data(), oot.size());
}

// Poison the MM shadow so "not refreshed" is observable and "reloaded" cannot
// pass against leftovers.
void PoisonMMShadow(uint8_t value) {
    std::vector<uint8_t> poison(MM_SAVE_CONTEXT_SIZE, value);
    Context_UpdateShadowCopy(GAME_MM, poison.data(), poison.size());
}

bool ShadowIsUniform(const void* shadow, size_t size, uint8_t value) {
    if (shadow == nullptr) {
        return false;
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(shadow);
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != value) {
            return false;
        }
    }
    return true;
}

// The interval clock is Unix milliseconds. Let at least a few of them pass so
// a reset (which writes "now") is a STRICTLY later value than the one read
// before the exit, and equality afterwards can only mean "not written".
void LetTheClockTick() {
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
}

// Every leg's non-vacuity guard: the exit was reached and ran through the
// revive. Returns nullptr on success, else the failing claim.
const char* CrossGameLegReached(int rc) {
    if (rc != 1) {
        return "the cross-game death exit must refuse TitleSetup (return 1)";
    }
    if (!Combo_IsGameSwitchRequested()) {
        return "the cross-game death exit must request the launcher switch";
    }
    if (gSaveContext.save.saveInfo.playerData.health != 0x30) {
        return "the cross-game death exit must have revived the bar before anything below is evidence";
    }
    if (gSaveContext.healthAccumulator != 0) {
        return "the pending killing blow must have been cleared by the revive";
    }
    return nullptr;
}

} // namespace

extern "C" int MM_DeathDeclineAutosave_RunHeadless(void) {
    printf("[TEST] mm-death-decline-autosave: the death-decline exit is an autosave point iff Autosave is on "
           "(ADR 0009 4b)\n");

    // Isolated save directory so this never touches a real player's slots.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "rsbs_mm_death_decline_autosave_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    rsbs::SaveManager::Instance().SetSaveDirectory(dir.string());
    // #533: start from process-start latch state so earlier rows in the same
    // process cannot have pre-armed the slot this row writes.
    RsbsSave_ResetSlotSessionState();

    Context_InitFrozenStates();
    ComboContext_Init();
    // The RAM watermark table lives outside gComboCtx; a leftover MM watermark
    // from an earlier row would turn "the pool carries 0x30" into a delta
    // against that row's balance.
    Combo_ResetSharedResourceWatermarks();
    Combo_ClearGameSwitchRequest();

    // The commit must not need a PlayState: the kaleido leg has one, this row
    // does not, and the exit's scene-flag flush is gated on it. Assert rather
    // than assume so a future MM_gPlayState dependency fails here by name.
    DDA_ASSERT(MM_gPlayState == NULL, "this row must run with no play state published");

    // Establish the slot the way production does: opening an empty slot arms
    // it for its first write.
    RsbsSave_SetActiveSlot(kSlot);
    DDA_ASSERT(RsbsSave_LoadSlot(kSlot) == RSBS_LOAD_ABSENT, "an empty slot must open as ABSENT and arm");
    DDA_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 1, "test setup: the slot must be writable for legs 1-3");

    // ---- 1. Autosave OFF: nothing is written at the death moment ----------
    SetAutosave(false);
    DDA_ASSERT(!SavingEnhancements_AutosaveArmed(),
               "test setup: RegisterAutosave with the CVar OFF must leave the enhancement disarmed");
    FreezeOoTSession(0x22);
    ArmDeathState(0x5A);
    PoisonMMShadow(0xA5);
    {
        const uint32_t genBefore = gComboCtx.commitGeneration;
        const uint64_t clockBefore = SavingEnhancements_GetLastAutosaveTimestamp();
        LetTheClockTick();

        const int rc = MM_Combo_GameOverExitToOoT();
        const char* reached = CrossGameLegReached(rc);
        DDA_ASSERT(reached == nullptr, reached);

        DDA_ASSERT(gComboCtx.commitGeneration == genBefore,
                   "Autosave OFF: the death-decline exit must not advance the commit generation");
        DDA_ASSERT(RsbsSave_HasSave(kSlot) == 0, "Autosave OFF: the death-decline exit must not write a slot file");
        DDA_ASSERT(ShadowIsUniform(Context_GetMMSaveContext(), MM_SAVE_CONTEXT_SIZE, 0xA5),
                   "Autosave OFF: the death-decline exit must not even refresh MM's shadow -- the launcher freeze "
                   "is the only publisher on this path");
        DDA_ASSERT(SavingEnhancements_GetLastAutosaveTimestamp() == clockBefore,
                   "Autosave OFF: the interval clock must not move -- there was no autosave");
        uint16_t pooled = 0;
        DDA_ASSERT(!Combo_GetSharedResource(RSBS_SHARED_RES_HEALTH_CURRENT, &pooled),
                   "Autosave OFF: no harvest may fire at the death moment -- the pool must stay empty");
    }
    Combo_ClearGameSwitchRequest();

    // ---- 2. Autosave ON: a whole-file commit, once, after the revive ------
    SetAutosave(true);
    DDA_ASSERT(SavingEnhancements_AutosaveArmed(),
               "test setup: RegisterAutosave with the CVar ON must arm the enhancement (#614)");
    FreezeOoTSession(0x22);
    ArmDeathState(0x5B);
    PoisonMMShadow(0xA5);
    {
        const uint32_t genBefore = gComboCtx.commitGeneration;
        const uint64_t clockBefore = SavingEnhancements_GetLastAutosaveTimestamp();
        LetTheClockTick();

        const int rc = MM_Combo_GameOverExitToOoT();
        const char* reached = CrossGameLegReached(rc);
        DDA_ASSERT(reached == nullptr, reached);

        DDA_ASSERT(gComboCtx.commitGeneration == genBefore + 1,
                   "Autosave ON: the death-decline exit must advance the commit generation by exactly one");
        DDA_ASSERT(RsbsSave_HasSave(kSlot) == 1, "Autosave ON: the death-decline exit must have written the slot file");
        DDA_ASSERT(gComboCtx.sourceGame == GAME_MM, "Autosave ON: the commit must record MM as the saving half");
        DDA_ASSERT(SavingEnhancements_GetLastAutosaveTimestamp() > clockBefore,
                   "Autosave ON: a committed autosave point must reset the enhancement's interval clock");

        // The harvest inside the commit body ran AFTER the revive: the shared
        // bar carries three hearts, not a dead bar and not nothing.
        uint16_t pooled = 0;
        DDA_ASSERT(Combo_GetSharedResource(RSBS_SHARED_RES_HEALTH_CURRENT, &pooled),
                   "Autosave ON: the commit's harvest must publish a health bar");
        DDA_ASSERT(pooled == 0x30, "Autosave ON: the harvested bar must be the REVIVED one (commit after revive)");

        // Make the claims about the FILE: wipe every trace from memory, reload,
        // and read the committed halves back.
        Context_ClearAllFrozenStates();
        DDA_ASSERT(RsbsSave_Load(kSlot) == 1, "Autosave ON: the committed slot file must load back");
        const void* mm = Context_GetMMSaveContext();
        DDA_ASSERT(mm != nullptr, "Autosave ON: the reload must have produced an MM shadow");
        DDA_ASSERT(ReadShadowS16(mm, HealthOffset()) == 0x30,
                   "Autosave ON: the committed Tier-3 must carry the REVIVED bar -- a commit taken before the "
                   "revive durably records a dead bar and re-enters the game-over on the next arrival");
        DDA_ASSERT(ReadShadowS16(mm, HealthAccumulatorOffset()) == 0,
                   "Autosave ON: the committed Tier-3 must not carry the pending killing blow");
        DDA_ASSERT(static_cast<const uint8_t*>(mm)[ProbeOffset()] == 0x5B,
                   "Autosave ON: the committed Tier-3 must be the LIVE state, captured through the shadow refresh, "
                   "not the stale shadow a bare RsbsSave_Save would have staged");
        DDA_ASSERT(ShadowIsUniform(Context_GetOoTSaveContext(), OOT_SAVE_CONTEXT_SIZE, 0x22),
                   "Autosave ON: the commit must be the WHOLE file -- OoT's frozen half must come back intact "
                   "(decision 4)");
    }
    Combo_ClearGameSwitchRequest();

    // ---- 3. gameMode gate: Autosave ON, but not a live file ---------------
    // Slot 3 was re-armed by the Load above. A death under the title-screen
    // mode has the bootstrap file resident (MM plays real frames there); the
    // exit still switches and revives, but must not commit that file.
    FreezeOoTSession(0x22);
    ArmDeathState(0x5C);
    gSaveContext.gameMode = GAMEMODE_TITLE_SCREEN;
    PoisonMMShadow(0xA5);
    {
        const uint32_t genBefore = gComboCtx.commitGeneration;
        const uint64_t clockBefore = SavingEnhancements_GetLastAutosaveTimestamp();
        LetTheClockTick();

        const int rc = MM_Combo_GameOverExitToOoT();
        const char* reached = CrossGameLegReached(rc);
        DDA_ASSERT(reached == nullptr, reached);

        DDA_ASSERT(gComboCtx.commitGeneration == genBefore,
                   "gameMode gate: a death under GAMEMODE_TITLE_SCREEN must not commit (bootstrap resident)");
        DDA_ASSERT(ShadowIsUniform(Context_GetMMSaveContext(), MM_SAVE_CONTEXT_SIZE, 0xA5),
                   "gameMode gate: a refused-by-mode autosave point must not refresh MM's shadow");
        DDA_ASSERT(SavingEnhancements_GetLastAutosaveTimestamp() == clockBefore,
                   "gameMode gate: a skipped autosave point must not reset the interval clock");
    }
    Combo_ClearGameSwitchRequest();

    // ---- 4. Write latch: Autosave ON, slot not established this session ---
    RsbsSave_ResetSlotSessionState();
    RsbsSave_SetActiveSlot(kSlot);
    DDA_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 0, "test setup: the slot must be write-latched here (#533/#568)");
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    FreezeOoTSession(0x22);
    ArmDeathState(0x5D);
    PoisonMMShadow(0xA5);
    {
        const uint32_t genBefore = gComboCtx.commitGeneration;
        const uint64_t clockBefore = SavingEnhancements_GetLastAutosaveTimestamp();
        LetTheClockTick();

        const int rc = MM_Combo_GameOverExitToOoT();
        const char* reached = CrossGameLegReached(rc);
        DDA_ASSERT(reached == nullptr, reached);

        DDA_ASSERT(gComboCtx.commitGeneration == genBefore,
                   "write latch: a latch-refused autosave point must not advance the commit generation");
        DDA_ASSERT(ShadowIsUniform(Context_GetMMSaveContext(), MM_SAVE_CONTEXT_SIZE, 0xA5),
                   "write latch: a latch-refused autosave point must not refresh MM's shadow");
        DDA_ASSERT(SavingEnhancements_GetLastAutosaveTimestamp() == clockBefore,
                   "write latch: a refused commit is NOT an autosave -- the interval clock must not reset");
        uint16_t pooled = 0;
        DDA_ASSERT(!Combo_GetSharedResource(RSBS_SHARED_RES_HEALTH_CURRENT, &pooled),
                   "write latch: a latch-refused autosave point must leave the shared-resource pool untouched (#591)");
    }
    Combo_ClearGameSwitchRequest();

    // ---- 5. Standalone MM: Autosave ON, no OoT session behind the player --
    // Re-establish the slot so "nothing committed" below is the cross-game
    // gate's doing and not the latch's.
    DDA_ASSERT(RsbsSave_Load(kSlot) == 1, "test setup: the slot must re-arm for the standalone leg");
    Context_ClearAllFrozenStates();
    DDA_ASSERT(Context_HasFrozenState(GAME_OOT) == 0, "test setup: no OoT session may be frozen here");
    DDA_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 1, "test setup: the slot must be writable for the standalone leg");
    ArmDeathState(0x5E);
    PoisonMMShadow(0xA5);
    {
        const uint32_t genBefore = gComboCtx.commitGeneration;
        const uint64_t clockBefore = SavingEnhancements_GetLastAutosaveTimestamp();
        LetTheClockTick();

        DDA_ASSERT(MM_Combo_GameOverExitToOoT() == 0,
                   "standalone: with no OoT session the vanilla TitleSetup exit must stand (#590 gate)");
        DDA_ASSERT(!Combo_IsGameSwitchRequested(), "standalone: no switch may be requested");
        DDA_ASSERT(gSaveContext.save.saveInfo.playerData.health == 0,
                   "standalone: the dead save must be left alone -- MM's own chain is about to replace it");
        DDA_ASSERT(gComboCtx.commitGeneration == genBefore,
                   "standalone: the autosave point sits BEHIND the cross-game gate; nothing may commit");
        DDA_ASSERT(ShadowIsUniform(Context_GetMMSaveContext(), MM_SAVE_CONTEXT_SIZE, 0xA5),
                   "standalone: MM's shadow must not be refreshed");
        DDA_ASSERT(SavingEnhancements_GetLastAutosaveTimestamp() == clockBefore,
                   "standalone: the interval clock must not reset");
    }

    // Leave global state clean for whatever runs next: the enhancement
    // disarmed (mm-registrar-coverage asserts an EMPTY OnGameStateDrawFinish
    // registry before its own bring-up; RegisterAutosave only queues the
    // unregistration, so flush it here rather than rely on the next reader
    // doing so), the save wiped, the frozen states and pool reset.
    SetAutosave(false);
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnGameStateUpdate>();
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnGameStateDrawFinish>();
    Combo_ClearGameSwitchRequest();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    RsbsSave_SetActiveSlot(-1);
    RsbsSave_ResetSlotSessionState();
    std::filesystem::remove_all(dir, ec);

    printf("[TEST] mm-death-decline-autosave: PASS\n");
    return 0;
}
