/**
 * @file mm_scene_flag_freeze_test.cpp
 * ROM-free, display-free lock for #638 (the agent tracker for the reporter's
 * #635) and #626: everything MM must fold into gSaveContext BEFORE a cross-game
 * departure freezes it, on BOTH freeze drivers. CTest label "redship", row
 * MMSceneFlagFreeze in CMake/SingleExecutable.cmake, dispatch
 * "mm-scene-flag-freeze" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN (#638). A pickup writes the live PlayState only:
 * MM_Flags_SetCollectible ORs into play->actorCtx.sceneFlags.collectible[].
 * The only copier from there into gSaveContext.cycleSceneFlags is
 * Play_SaveCycleSceneFlags, reached from Actor_CleanupContext on a normal scene
 * transition and from the save routes -- nowhere else. Walking into the Clock
 * Tower door ran Combo_CheckEntranceSwitch the instant nextEntrance was
 * assigned (z_player.c), which froze gSaveContext BEFORE that copy, and the
 * switch then broke MM's graph loop without ever running Play_Destroy; the F10
 * hot swap had the same shape one frame later. So every flag set during the
 * final scene visit was frozen as unset, the return leg restored the stale
 * blob, Actor_InitContext reseeded actorCtx.sceneFlags from it, and South
 * Clock Town's heart piece respawned -- indefinitely. This is a class, not an
 * instance: chests, switches and cleared rooms in the departure scene all go
 * the same way, in both games (the OoT twin is oot_scene_flag_freeze_test.cpp).
 *
 * WHAT WAS BROKEN (#626). PR #625 made the kaleido "don't continue" exit revive
 * a dead bar before the launcher froze it. But F10 is polled ungated every
 * frame, and a press while the game-over screen is up froze health == 0 into
 * MM's shadow without touching that leg. The Game_Suspend harvest then read the
 * dead bar verbatim and the far side's apply ASSIGNED it: the shared
 * CONSUMABLE health bar arrived in OoT at zero, and (under #612) the frozen
 * dead bar was durable, so the next MM arrival re-entered the game-over.
 *
 * THE FIX UNDER TEST. Combo_FlushLiveStateForFreeze (src/common/switch.cpp) is
 * called by both production freeze drivers immediately before they capture
 * gSaveContext: Combo_CheckEntranceSwitch (entrance path) and
 * Combo_FreezeActiveGameForHotSwap (F10 / launcher re-freeze / owl and
 * game-over exits). For MM it runs MM_Combo_FlushSceneFlagsForFreeze (the same
 * Play_SaveCycleSceneFlags call Actor_CleanupContext would have made) and
 * MM_Combo_ReviveDeadHealthForFreeze (#625's revive, gated on gameMode).
 *
 * THE INVARIANTS THIS LOCKS, each against the REAL driver, reading the frozen
 * blob back through Context_RestoreState:
 *
 *   1. Entrance driver: a live collectible / chest bit reaches the frozen
 *      blob's cycleSceneFlags. Guarded against vacuity -- the cycle word is
 *      asserted ZERO before the act, so a pre-populated save cannot fake it.
 *      The per-visit temp word (collectible[1]) does NOT leak: the flush is
 *      exactly Actor_CleanupContext's copy, nothing wider.
 *   2. Hot-swap driver: the same, through Combo_FreezeActiveGameForHotSwap.
 *   3. Hot-swap driver with a dead bar (#626): the frozen half AND the live
 *      gSaveContext carry a revived bar with the pending killing blow cleared,
 *      the switch is not refused, and the suspend harvest that follows the
 *      freeze in production publishes the revived value to the shared pool.
 *      A live bar passes through untouched, accumulator included. The gate is
 *      gameMode, not fileNum: the 0xFF cross-game sentinel IS revived, a
 *      TITLE_SCREEN save is not.
 *   4. No PlayState (owl-save / game-over exits reach the launcher freeze with
 *      none; so do the headless rows): both drivers still freeze, nothing is
 *      dereferenced, and the cycle flags the last transition left are kept.
 *
 * COUNTERFACTUALS, run before landing: remove the Combo_FlushLiveStateForFreeze
 * call from either driver and its leg of 1/2 goes red; make
 * MM_Combo_ReviveDeadHealthForFreeze unconditional and 3's live-bar leg goes
 * red; drop its gameMode gate and 3's TITLE_SCREEN leg goes red.
 */

#include "global.h"

#include "context.h"
#include "entrance.h"
#include "game.h"
#include "shared_resources.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" SaveContext gSaveContext;
// The two REAL freeze drivers (games/oot/soh/GameExports_SingleExe.cpp). Both
// are shared between the games' TUs in the single-exe link and resolve the
// departing game from Context_GetCurrentGame.
extern "C" uint16_t Combo_CheckEntranceSwitch(uint16_t entranceIndex);
extern "C" int Combo_FreezeActiveGameForHotSwap(GameId departing);
// The suspend-time harvest that, in production, runs AFTER the launcher freeze.
extern "C" void MM_HarvestSharedResources(void);

namespace {

#define SFF_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// South Clock Town's persistent flags (games/mm/include/tables/scene_table.h:
// PERSISTENT_CYCLE_FLAGS_SET((1 << 20), 0, 0, (1 << 10)) -- chest bit 20,
// collectible bit 10). The collectible is the heart piece #635 reported; the
// bit is inferred from the persistent mask rather than read from the scene's
// actor list, which this ROM-free row cannot open.
const uint32_t kHeartPieceBit = 1u << 10;
const uint32_t kChestBit = 1u << 20;

// Frozen-blob readback target. File-static rather than a stack local: MM's
// runtime SaveContext carries the 2s2h extensions and is tens of KB.
SaveContext sScratch;

// Put gSaveContext into the shape a live cross-game MM session runs in: the
// 0xFF "no real flash slot" sentinel and GAMEMODE_NORMAL. The gate under test
// (MM_SaveIsLiveFile / Combo_SaveIsLiveFile) reads gameMode only, and this is
// the sentinel a real cross-game session is pinned to for its whole life -- so
// a fileNum-based gate would wrongly skip exactly the session that matters. A
// zeroed context already reads as NORMAL; the assignment is written out so a
// renumber upstream fails here rather than silently disarming the checks.
void ArmLiveMMSession(int16_t health) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.saveInfo.playerData.health = health;
}

// Fresh entrance table with the production links, no pending switch, no F10
// request. Combo_CheckEntranceSwitch suppresses its freeze when a switch is
// already pending (wasAlreadyPending), so every entrance leg needs this first.
void ResetEntranceTable(void) {
    Entrance_Init();
    Entrance_RegisterDefaultLinks();
    Combo_ClearGameSwitchRequest();
}

// Read the frozen MM half back. Context_RestoreState copies without retiring
// the blob (Combo_ConsumeFrozenState is the retiring path), so the blob is
// still there for a HasFrozenState check afterwards.
bool ReadFrozenMM(void) {
    memset(&sScratch, 0, sizeof(SaveContext));
    return Context_RestoreState(GAME_MM, &sScratch, sizeof(SaveContext)) != 0;
}

int RunChecks(PlayState* play) {
    // ---- 1. Entrance driver: live scene flags reach the frozen blob --------
    ArmLiveMMSession(0x50);
    play->actorCtx.sceneFlags.collectible[0] = kHeartPieceBit;
    play->actorCtx.sceneFlags.chest = kChestBit;
    // A per-visit TEMP word. Actor_CleanupContext never copies it, and neither
    // may the flush -- a wider copy would be a different bug.
    play->actorCtx.sceneFlags.collectible[1] = 0xFFFFFFFFu;
    SFF_ASSERT(gSaveContext.cycleSceneFlags[SCENE_CLOCKTOWER].collectible == 0,
               "non-vacuity: the cycle collectible word must be ZERO before the act, or the assertion below "
               "could pass against a save that already held the bit");
    SFF_ASSERT(gSaveContext.cycleSceneFlags[SCENE_CLOCKTOWER].chest == 0,
               "non-vacuity: the cycle chest word must be ZERO before the act");
    SFF_ASSERT(Context_HasFrozenState(GAME_MM) == 0, "test setup: no MM blob may exist before the entrance act");

    Combo_CheckEntranceSwitch(MM_ENTR_CLOCK_TOWER_INTERIOR_1);
    SFF_ASSERT(Combo_IsCrossGameSwitch(), "test setup: the Clock Tower door must resolve to a cross-game switch");
    SFF_ASSERT(Context_HasFrozenState(GAME_MM) == 1, "the entrance driver must freeze the departing MM half");
    SFF_ASSERT(ReadFrozenMM(), "the frozen MM half must read back");
    SFF_ASSERT((sScratch.cycleSceneFlags[SCENE_CLOCKTOWER].collectible & kHeartPieceBit) != 0,
               "the heart piece collected in the departure scene must be in the FROZEN blob -- the entrance "
               "switch froze gSaveContext before the scene-transition flush and never ran it (#638 / #635)");
    SFF_ASSERT((sScratch.cycleSceneFlags[SCENE_CLOCKTOWER].chest & kChestBit) != 0,
               "the chest opened in the departure scene must be in the frozen blob too -- this is a class, "
               "not a heart-piece special case (#638)");
    SFF_ASSERT(sScratch.cycleSceneFlags[SCENE_CLOCKTOWER].collectible == kHeartPieceBit,
               "the per-visit temp word (collectible[1]) must NOT leak into the cycle flags -- the flush "
               "mirrors Actor_CleanupContext's copy exactly");
    SFF_ASSERT((gSaveContext.cycleSceneFlags[SCENE_CLOCKTOWER].collectible & kHeartPieceBit) != 0,
               "the LIVE gSaveContext must carry the flushed bit as well: Game_Suspend's harvest and any "
               "later capture read the live struct, not the blob");
    SFF_ASSERT(sScratch.save.saveInfo.playerData.health == 0x50,
               "a live bar must pass through the entrance freeze untouched (#626's revive is conditional)");
    ResetEntranceTable();
    Context_ClearFrozenState(GAME_MM);

    // ---- 2. Hot-swap driver: the same, through the launcher's bridge -------
    ArmLiveMMSession(0x50);
    play->actorCtx.sceneFlags.collectible[0] = kHeartPieceBit;
    play->actorCtx.sceneFlags.collectible[1] = 0;
    play->actorCtx.sceneFlags.chest = 0;
    SFF_ASSERT(gSaveContext.cycleSceneFlags[SCENE_CLOCKTOWER].collectible == 0,
               "non-vacuity: the cycle collectible word must be ZERO before the hot-swap act");
    SFF_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_MM) == 1, "the hot-swap driver must record a fresh MM blob");
    SFF_ASSERT(Context_GetFrozenReturnEntrance(GAME_MM) == MM_ENTR_SOUTH_CLOCK_TOWN_0,
               "test setup: the hot-swap freeze must still tag MM's own portal return (#364)");
    SFF_ASSERT(ReadFrozenMM(), "the hot-swap frozen MM half must read back");
    SFF_ASSERT((sScratch.cycleSceneFlags[SCENE_CLOCKTOWER].collectible & kHeartPieceBit) != 0,
               "the heart piece collected in the departure scene must be in the F10 blob -- the hot swap "
               "breaks the graph loop with the flags still only in the PlayState (#638)");
    Context_ClearFrozenState(GAME_MM);

    // ---- 3. Hot-swap driver, Link DEAD (#626) ------------------------------
    // The game-over screen is a PlayState with health 0; F10 there never
    // touches the kaleido leg #625 guarded.
    ArmLiveMMSession(0);
    gSaveContext.healthAccumulator = -8; // the blow that killed Link, still pending
    play->actorCtx.sceneFlags.collectible[0] = 0;
    SFF_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_MM) == 1,
               "an F10 from the game-over screen is a switch, not a refusal -- the same departure #625 made "
               "the death prompt take (#626)");
    SFF_ASSERT(ReadFrozenMM(), "the dead-bar frozen MM half must read back");
    SFF_ASSERT(sScratch.save.saveInfo.playerData.health == 0x30,
               "the FROZEN MM half must carry a revived bar -- a frozen dead bar is durable under #612 and "
               "re-enters the game-over on the next arrival (#626)");
    SFF_ASSERT(sScratch.healthAccumulator == 0,
               "the pending killing blow must not ride the revived bar into the next MM frame");
    SFF_ASSERT(gSaveContext.save.saveInfo.playerData.health == 0x30,
               "the LIVE bar must be revived too: MM_HarvestSharedResources runs at Game_Suspend, AFTER the "
               "launcher freeze, and reads gSaveContext verbatim");
    SFF_ASSERT(gSaveContext.healthAccumulator == 0, "the live accumulator must be cleared with the live bar");
    // The shared-pool consequence, in production order: freeze, then the
    // suspend harvest. Against a fresh pool a dead bar publishes NOTHING (delta
    // zero from a zero seed) and once a watermark exists it debits the shared
    // bar outright; both apply sides floor at one heart, so the symptom is a
    // silent demotion in OoT, not a crash.
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    MM_HarvestSharedResources();
    {
        uint16_t pooled = 0;
        SFF_ASSERT(Combo_GetSharedResource(RSBS_SHARED_RES_HEALTH_CURRENT, &pooled),
                   "the F10 departure must publish a health bar at all -- a dead bar publishes nothing");
        SFF_ASSERT(pooled == 0x30, "the shared health bar must carry the revived value, not the dead one (#626)");
    }
    Context_ClearFrozenState(GAME_MM);

    // 3b. A live bar passes through untouched, in-flight accumulator included:
    //     the revive is conditional, never a blanket assignment.
    ArmLiveMMSession(0x50);
    gSaveContext.healthAccumulator = -4; // a hit landing, Link alive
    SFF_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_MM) == 1, "a live-bar hot swap must still freeze");
    SFF_ASSERT(ReadFrozenMM(), "the live-bar frozen MM half must read back");
    SFF_ASSERT(sScratch.save.saveInfo.playerData.health == 0x50,
               "the revive must not demote a bar that is already alive (#626)");
    SFF_ASSERT(sScratch.healthAccumulator == -4,
               "a live bar's pending accumulator belongs to the player, not to the revive");
    Context_ClearFrozenState(GAME_MM);

    // 3c. The gate is gameMode, NOT fileNum. Leg 3 already revived under the
    //     0xFF sentinel a cross-game session is pinned to; here the same dead
    //     bar under TITLE_SCREEN -- the attract demo's bootstrap save, which
    //     the harvest gate (Combo_SaveIsLiveFile) also skips -- must be left
    //     alone: it is not a session anyone can resume into.
    ArmLiveMMSession(0);
    gSaveContext.gameMode = GAMEMODE_TITLE_SCREEN;
    SFF_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_MM) == 1, "a title-screen hot swap must still freeze");
    SFF_ASSERT(ReadFrozenMM(), "the title-screen frozen MM half must read back");
    SFF_ASSERT(sScratch.save.saveInfo.playerData.health == 0,
               "the revive is gated on gameMode: a non-live save is not a session to make resumable, and "
               "editing it would be the harvest/apply gate-asymmetry class in a new coat");
    Context_ClearFrozenState(GAME_MM);

    // ---- 4. No PlayState: both drivers still freeze, nothing is dereferenced
    // The owl-save and game-over exits reach the launcher freeze after MM's
    // gamestate is gone, and MM_gPlayState is NULL there.
    MM_gPlayState = NULL;
    ArmLiveMMSession(0x50);
    gSaveContext.cycleSceneFlags[SCENE_CLOCKTOWER].collectible = kHeartPieceBit; // what the last transition left
    SFF_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_MM) == 1,
               "a hot-swap freeze with no PlayState must not crash and must still record a blob");
    SFF_ASSERT(ReadFrozenMM(), "the no-PlayState frozen MM half must read back");
    SFF_ASSERT(sScratch.cycleSceneFlags[SCENE_CLOCKTOWER].collectible == kHeartPieceBit,
               "with no PlayState there is nothing to flush; the cycle flags the last transition left must be "
               "frozen as they are");
    Context_ClearFrozenState(GAME_MM);
    Combo_CheckEntranceSwitch(MM_ENTR_CLOCK_TOWER_INTERIOR_1);
    SFF_ASSERT(Context_HasFrozenState(GAME_MM) == 1,
               "an entrance freeze with no PlayState must not crash and must still record a blob");
    ResetEntranceTable();
    Context_ClearFrozenState(GAME_MM);

    return 0;
}

} // namespace

extern "C" int MM_SceneFlagFreeze_RunHeadless(void) {
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    const GameId prevGame = Context_GetCurrentGame();
    // Combo_CheckEntranceSwitch resolves the departing game -- and therefore
    // which entrance table and which blob -- from the current-game tracker.
    Context_SetCurrentGame(GAME_MM);
    ResetEntranceTable();

    // A minimal live PlayState: the flush reads sceneId and actorCtx.sceneFlags
    // and nothing else. calloc'd on the C heap, not MM's arena, so this row is
    // independent of the arena rows that run after it.
    PlayState* play = (PlayState*)calloc(1, sizeof(PlayState));
    int rc = 1;
    if (play == NULL) {
        printf("[TEST] FAIL: could not allocate a PlayState (%s:%d)\n", __FILE__, __LINE__);
    } else {
        play->sceneId = SCENE_CLOCKTOWER;
        MM_gPlayState = play;
        rc = RunChecks(play);
    }

    // Leave global state clean for whatever runs next in this process, on
    // pass or fail: the published PlayState above must never outlive this row.
    MM_gPlayState = NULL;
    free(play);
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    // The watermark table lives outside gComboCtx; leg 3 harvested into it.
    Combo_ResetSharedResourceWatermarks();
    ResetEntranceTable();
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] mm-scene-flag-freeze: PASS\n");
    }
    return rc;
}
