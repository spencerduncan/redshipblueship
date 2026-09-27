/**
 * @file oot_gameover_revive_test.cpp
 * ROM-free, display-free lock for #664: an F10 from EITHER game's game-over
 * screen must hand the other game a live shared health bar, not a dead one.
 * CTest label "redship", row ComboGameOverRevive in
 * CMake/SingleExecutable.cmake, dispatch "combo-gameover-revive" in
 * src/common/test_runner.cpp. MM-layout access goes through the helpers in
 * games/mm/2s2h/mm_gameover_revive_test.cpp.
 *
 * WHAT WAS BROKEN. PR #650 (#626) revived MM's dead bar at the pre-freeze seam
 * (Combo_FlushLiveStateForFreeze -> MM_Combo_ReviveDeadHealthForFreeze), but the
 * seam's GAME_OOT branch flushed scene flags only. OoT's game-over is vanilla:
 * gSaveContext.health sits at 0 from the killing blow through the whole kaleido
 * chain (z_kaleido_scope_PAL.c states 8..0x11) until the "Continue playing?"
 * continue leg writes STARTING_HEALTH, and F10 is polled ungated from OoT's
 * graph loop (graph.c -> Combo_CheckHotSwap). A press there froze health == 0
 * into OoT's shadow; OoT_HarvestSharedResources (Game_Suspend, after the
 * freeze) debited the shared CONSUMABLE bar to 0 against its watermark; and
 * MM's arrival apply assigned it -- only its one-heart floor kept the player
 * from spawning dead. The player arrived in MM with ONE heart where the
 * continue prompt would have given three, and the frozen OoT half was dead.
 *
 * THE FIX UNDER TEST. OoT_Combo_ReviveDeadHealthForFreeze
 * (games/oot/soh/GameExports_SingleExe.cpp), dispatched from the GAME_OOT
 * branch: health <= 0 -> OoT's own continue value (STARTING_HEALTH, or the
 * capacity under FullHealthSpawn), heal accumulator cleared, gated on gameMode.
 *
 * THE INVARIANTS, each through the REAL freeze driver
 * (Combo_FreezeActiveGameForHotSwap), the REAL harvest and the REAL far-side
 * apply, in production order (freeze -> Game_Suspend harvest -> arrival apply):
 *
 *   1. OoT -> MM, Link dead (#664): the switch is not refused; the FROZEN and
 *      the LIVE OoT bar are revived to STARTING_HEALTH with the accumulator
 *      cleared; the harvest publishes the revived value, not 0; and MM's
 *      arrival apply shows three hearts -- not the one-heart floor. A second
 *      freeze (the launcher's re-freeze after an exit's own) is a no-op.
 *   2. MM -> OoT, Link dead (#626, the reverse direction PR #650 fixed): the
 *      same contract end to end, ending in OoT's arrival apply. Locked here so
 *      the two directions are one row and cannot drift apart.
 *   3. A live OoT bar passes through untouched, pending heal included.
 *   4. The gate is gameMode: a dead bar under TITLE_SCREEN is left alone.
 *   5. FullHealthSpawn on: the revive gives the full capacity, exactly as OoT's
 *      continue leg and Sram_OpenSave do.
 *
 * COUNTERFACTUALS, each run against a rebuilt binary before landing: with the
 * GAME_OOT dispatch removed from Combo_FlushLiveStateForFreeze (main's shape),
 * leg 1 fails at the frozen-bar assertion; with the hook's gameMode gate
 * removed, leg 4 fails; with the live-bar early return removed, leg 3 fails;
 * with the FullHealthSpawn read ignored, leg 5 fails.
 */

#include <z64.h>

#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

#include "context.h"
#include "game.h"
#include "shared_resources.h"

#include <cstdio>
#include <cstring>

extern "C" SaveContext gSaveContext;
extern "C" PlayState* OoT_gPlayState;
// The REAL freeze driver (games/oot/soh/GameExports_SingleExe.cpp), shared by
// both games in the single-exe link.
extern "C" int Combo_FreezeActiveGameForHotSwap(GameId departing);
// Each game's suspend-time harvest and arrival-time apply.
extern "C" void OoT_HarvestSharedResources(void);
extern "C" void OoT_ApplySharedResources(void);
extern "C" void MM_HarvestSharedResources(void);
extern "C" void MM_ApplySharedResources(void);
// MM-layout helpers (games/mm/2s2h/mm_gameover_revive_test.cpp).
extern "C" void MM_GameOverReviveTest_ArmLive(int16_t health, int16_t capacity);
extern "C" void MM_GameOverReviveTest_SetHealth(int16_t health);
extern "C" int MM_GameOverReviveTest_Health(void);
extern "C" int MM_GameOverReviveTest_FrozenHealth(void);

namespace {

#define GOR_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

const char* const kFullHealthSpawn = CVAR_ENHANCEMENT("FullHealthSpawn");

// Ten hearts: comfortably above the three-heart continue value, so a revive
// to capacity and a revive to STARTING_HEALTH cannot be confused.
const int16_t kCapacity = 10 * 0x10;
// What the player had before dying, and what the arriving half's restored blob
// carried before its apply. Distinct from every value asserted below.
const int16_t kAlive = 0x50;
const int16_t kStaleArrival = 0x70;
// MM's own continue literal (#625 / #626).
const int kMMContinue = 0x30;
// The one-heart floor both apply sides keep as a safety net.
const int kOneHeartFloor = 0x10;

// Frozen-blob readback target. File-static: SoH's runtime SaveContext carries
// the ship.* extensions and is over 100 KB.
SaveContext sScratch;

// A live cross-game OoT session: the 0xFF sentinel a cross-game session is
// pinned to and GAMEMODE_NORMAL, which is what the gate reads.
void ArmLiveOoT(int16_t health) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.healthCapacity = kCapacity;
    gSaveContext.health = health;
}

int FrozenOoTHealth(void) {
    memset(&sScratch, 0, sizeof(SaveContext));
    if (Context_RestoreState(GAME_OOT, &sScratch, sizeof(SaveContext)) == 0) {
        return -1;
    }
    return (int)sScratch.health;
}

void ResetPool(void) {
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
}

bool PooledHealth(uint16_t* out) {
    return Combo_GetSharedResource(RSBS_SHARED_RES_HEALTH_CURRENT, out);
}

int RunChecks(void) {
    uint16_t pooled = 0;

    // ---- 1. OoT -> MM, Link dead on OoT's game-over screen (#664) ----------
    ResetPool();
    Context_SetCurrentGame(GAME_OOT);
    ArmLiveOoT(kAlive);
    // An earlier crossing: the pool and OoT's watermark hold the live bar, so
    // the death below is a real debit rather than a first-harvest seed.
    OoT_HarvestSharedResources();
    GOR_ASSERT(PooledHealth(&pooled) && pooled == (uint16_t)kAlive,
               "test setup: an earlier OoT harvest must have published the live bar");
    gSaveContext.health = 0; // the killing blow; the game-over screen is up
    gSaveContext.healthAccumulator = 0;
    GOR_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1,
               "an F10 from OoT's game-over screen is a switch, not a refusal -- the same departure MM's twin "
               "takes (#626 / #664)");
    GOR_ASSERT(FrozenOoTHealth() == STARTING_HEALTH,
               "the FROZEN OoT half must carry a revived bar (OoT's continue value, three hearts) -- a frozen "
               "dead bar re-enters the game-over on the next OoT arrival (#664)");
    GOR_ASSERT(gSaveContext.health == STARTING_HEALTH,
               "the LIVE OoT bar must be revived too: OoT_HarvestSharedResources runs at Game_Suspend, AFTER "
               "the launcher freeze, and reads gSaveContext verbatim (#664)");
    GOR_ASSERT(gSaveContext.healthAccumulator == 0, "the revive clears the heal accumulator as the continue leg does");
    // The launcher re-freezes after an exit that already froze; the revive
    // must be idempotent across it.
    GOR_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1 && FrozenOoTHealth() == STARTING_HEALTH,
               "a second freeze of an already-revived bar must be a no-op");
    OoT_HarvestSharedResources();
    GOR_ASSERT(PooledHealth(&pooled), "the OoT departure must leave a shared health bar in the pool");
    GOR_ASSERT(pooled == (uint16_t)STARTING_HEALTH,
               "the shared health bar must carry the revived value -- without the OoT revive the harvest debits "
               "it to 0 against the watermark (#664)");
    // MM arrives. Its restored blob carried some other bar; the apply ASSIGNS
    // the shared one.
    MM_GameOverReviveTest_ArmLive(kStaleArrival, kCapacity);
    MM_ApplySharedResources();
    GOR_ASSERT(MM_GameOverReviveTest_Health() == STARTING_HEALTH,
               "MM must arrive with the three hearts OoT's continue prompt would have given, not the one-heart "
               "apply floor that caught the dead bar before #664");
    GOR_ASSERT(MM_GameOverReviveTest_Health() != kOneHeartFloor,
               "arriving at the one-heart floor means the dead bar crossed and only the safety net caught it");

    // ---- 2. MM -> OoT, Link dead on MM's game-over screen (#626) -----------
    ResetPool();
    Context_SetCurrentGame(GAME_MM);
    MM_GameOverReviveTest_ArmLive(kAlive, kCapacity);
    MM_HarvestSharedResources();
    GOR_ASSERT(PooledHealth(&pooled) && pooled == (uint16_t)kAlive,
               "test setup: an earlier MM harvest must have published the live bar");
    MM_GameOverReviveTest_SetHealth(0);
    GOR_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_MM) == 1, "an F10 from MM's game-over screen is a switch");
    GOR_ASSERT(MM_GameOverReviveTest_FrozenHealth() == kMMContinue,
               "the frozen MM half must carry MM's revived bar (#626)");
    GOR_ASSERT(MM_GameOverReviveTest_Health() == kMMContinue,
               "the live MM bar must be revived before the harvest (#626)");
    MM_HarvestSharedResources();
    GOR_ASSERT(PooledHealth(&pooled) && pooled == (uint16_t)kMMContinue,
               "the MM departure must publish the revived bar (#626)");
    Context_SetCurrentGame(GAME_OOT);
    ArmLiveOoT(kStaleArrival);
    OoT_ApplySharedResources();
    GOR_ASSERT(gSaveContext.health == kMMContinue,
               "OoT must arrive with the three hearts MM's revive handed over, not the one-heart floor (#626)");

    // ---- 3. A live OoT bar passes through untouched ------------------------
    ResetPool();
    Context_SetCurrentGame(GAME_OOT);
    ArmLiveOoT(kAlive);
    gSaveContext.healthAccumulator = 0x10; // a heal in flight, Link alive
    GOR_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1, "a live-bar OoT hot swap must still freeze");
    GOR_ASSERT(FrozenOoTHealth() == kAlive, "the revive must never touch a bar that is alive (#664)");
    GOR_ASSERT(sScratch.healthAccumulator == 0x10,
               "a live bar's pending heal belongs to the player, not to the revive");

    // ---- 4. The gate is gameMode, not fileNum ------------------------------
    // Leg 1 revived under the 0xFF sentinel a cross-game session is pinned to;
    // the same dead bar under TITLE_SCREEN is the attract demo's save, which
    // the harvest gate also skips.
    ResetPool();
    ArmLiveOoT(0);
    gSaveContext.gameMode = GAMEMODE_TITLE_SCREEN;
    GOR_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1, "a title-screen OoT hot swap must still freeze");
    GOR_ASSERT(FrozenOoTHealth() == 0,
               "the revive is gated on gameMode: a title-screen save is not a session anyone resumes into");

    // ---- 5. FullHealthSpawn: the continue value is the full capacity -------
    ResetPool();
    CVarSetInteger(kFullHealthSpawn, 1);
    ArmLiveOoT(0);
    const int frozen = Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1 ? FrozenOoTHealth() : -1;
    CVarClear(kFullHealthSpawn);
    GOR_ASSERT(frozen == kCapacity,
               "with FullHealthSpawn on, OoT's own continue leg (and Sram_OpenSave) respawn at full capacity; "
               "the revive must give the same value (#664)");

    return 0;
}

} // namespace

extern "C" int OoT_GameOverRevive_RunHeadless(void) {
    Context_InitFrozenStates();
    const GameId prevGame = Context_GetCurrentGame();
    // The OoT flush half reads OoT_gPlayState; none is live when the launcher
    // re-freezes after a game-over exit, and none is needed here.
    PlayState* const prevPlay = OoT_gPlayState;
    OoT_gPlayState = NULL;
    const int prevFullHealth = CVarGetInteger(kFullHealthSpawn, 0);
    CVarClear(kFullHealthSpawn);

    const int rc = RunChecks();

    // Leave global state clean for whatever runs next in this process, on pass
    // or fail.
    if (prevFullHealth != 0) {
        CVarSetInteger(kFullHealthSpawn, prevFullHealth);
    } else {
        CVarClear(kFullHealthSpawn);
    }
    OoT_gPlayState = prevPlay;
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] combo-gameover-revive: PASS\n");
    }
    return rc;
}
