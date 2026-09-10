/**
 * @file oot_scene_flag_freeze_test.cpp
 * ROM-free, display-free lock for the OoT half of #638: the current scene's
 * live flags must be folded into gSaveContext BEFORE a cross-game departure
 * freezes it, on BOTH freeze drivers. CTest label "redship", row
 * OoTSceneFlagFreeze in CMake/SingleExecutable.cmake, dispatch
 * "oot-scene-flag-freeze" in src/common/test_runner.cpp. The MM twin, which
 * also carries #626, is games/mm/2s2h/mm_scene_flag_freeze_test.cpp.
 *
 * WHAT WAS BROKEN. OoT keeps the flags of the scene being played in
 * play->actorCtx.flags and copies them into gSaveContext.sceneFlags[sceneNum]
 * only from Actor_CleanupContext (z_actor.c) on a scene transition, via
 * Play_SaveSceneFlags. Both cross-game departures skip that copy: the entrance
 * path freezes from inside Play_Update's transition check (z_play.c, before the
 * transition runs) and then stops the gamestate with init/destroy nulled; the
 * F10 path breaks the graph loop with the PlayState still live and never
 * destroys it. So a chest opened, a switch pressed or a collectible taken in
 * the Market on the way into the Happy Mask Shop was frozen as unset and the
 * return leg restored it that way. Latent today only because the Market has no
 * persistent pickup at the portal the way South Clock Town does (#635); the
 * shape is identical, so the lock is too.
 *
 * THE FIX UNDER TEST. Combo_FlushLiveStateForFreeze (src/common/switch.cpp),
 * called by Combo_CheckEntranceSwitch before its Combo_FreezeState and by
 * Combo_FreezeActiveGameForHotSwap before Switch_PrepareHotSwap, runs
 * OoT_Combo_FlushSceneFlagsForFreeze: the same Play_SaveSceneFlags call
 * Actor_CleanupContext would have made.
 *
 * THE INVARIANTS: (1) entrance driver -- live collect/chest bits reach the
 * frozen blob, guarded against vacuity (the saved words are asserted ZERO
 * before the act), and the per-visit temp words do NOT leak; (2) hot-swap
 * driver -- the same; (3) no PlayState -- both drivers still freeze, nothing is
 * dereferenced, the saved flags are kept as they were.
 */

#include <z64.h>

#include "context.h"
#include "entrance.h"
#include "game.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" SaveContext gSaveContext;
// games/oot/src/code/z_play.c: set by Play_Init, nulled by Play_Destroy and
// OoT_Graph_ResetRunFrameContext. The flush reads it; this row publishes it.
extern "C" PlayState* OoT_gPlayState;
// The two REAL freeze drivers (games/oot/soh/GameExports_SingleExe.cpp).
extern "C" uint16_t Combo_CheckEntranceSwitch(uint16_t entranceIndex);
extern "C" int Combo_FreezeActiveGameForHotSwap(GameId departing);

namespace {

#define OSFF_ASSERT(cond, msg)                                            \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// Arbitrary Market-scene bits; the flush copies whole words, so any bit locks
// the copy. Distinct values per word so a swapped field would be caught.
const uint32_t kCollectBit = 1u << 3;
const uint32_t kChestBit = 1u << 5;
const uint32_t kSwitchBit = 1u << 7;

// Frozen-blob readback target. File-static: SoH's runtime SaveContext carries
// the ship.* extensions and is over 100 KB.
SaveContext sScratch;

// A live OoT gameplay session: a real slot and GAMEMODE_NORMAL.
// Combo_CheckEntranceSwitch publishes an in-range fileNum as the unified save
// slot; 0xFF is the title-screen sentinel and is out of range, so this row
// never touches slot session state.
void ArmLiveOoTSession(void) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
}

// Fresh entrance table with the production links, no pending switch, no F10
// request. Combo_CheckEntranceSwitch suppresses its freeze when a switch is
// already pending (wasAlreadyPending), so every entrance leg needs this first.
void ResetEntranceTable(void) {
    Entrance_Init();
    Entrance_RegisterDefaultLinks();
    Combo_ClearGameSwitchRequest();
}

bool ReadFrozenOoT(void) {
    memset(&sScratch, 0, sizeof(SaveContext));
    return Context_RestoreState(GAME_OOT, &sScratch, sizeof(SaveContext)) != 0;
}

int RunChecks(PlayState* play) {
    // ---- 1. Entrance driver: live scene flags reach the frozen blob --------
    ArmLiveOoTSession();
    play->actorCtx.flags.collect = kCollectBit;
    play->actorCtx.flags.chest = kChestBit;
    play->actorCtx.flags.swch = kSwitchBit;
    // Per-visit TEMP words. Actor_CleanupContext never copies them, and
    // neither may the flush.
    play->actorCtx.flags.tempCollect = 0xFFFFFFFFu;
    play->actorCtx.flags.tempSwch = 0xFFFFFFFFu;
    OSFF_ASSERT(gSaveContext.sceneFlags[SCENE_MARKET_DAY].collect == 0,
                "non-vacuity: the saved collect word must be ZERO before the act, or the assertion below could "
                "pass against a save that already held the bit");
    OSFF_ASSERT(gSaveContext.sceneFlags[SCENE_MARKET_DAY].chest == 0,
                "non-vacuity: the saved chest word must be ZERO before the act");
    OSFF_ASSERT(Context_HasFrozenState(GAME_OOT) == 0, "test setup: no OoT blob may exist before the entrance act");

    Combo_CheckEntranceSwitch(OOT_ENTR_HAPPY_MASK_SHOP);
    OSFF_ASSERT(Combo_IsCrossGameSwitch(), "test setup: the Happy Mask Shop door must resolve to a cross-game switch");
    OSFF_ASSERT(Context_HasFrozenState(GAME_OOT) == 1, "the entrance driver must freeze the departing OoT half");
    OSFF_ASSERT(ReadFrozenOoT(), "the frozen OoT half must read back");
    OSFF_ASSERT(sScratch.sceneFlags[SCENE_MARKET_DAY].collect == kCollectBit,
                "the collectible taken in the departure scene must be in the FROZEN blob, and the temp word must "
                "not leak into it -- the entrance switch froze gSaveContext before the scene-transition flush "
                "and never ran it (#638)");
    OSFF_ASSERT(sScratch.sceneFlags[SCENE_MARKET_DAY].chest == kChestBit,
                "the chest opened in the departure scene must be in the frozen blob (#638)");
    OSFF_ASSERT(sScratch.sceneFlags[SCENE_MARKET_DAY].swch == kSwitchBit,
                "the switch pressed in the departure scene must be in the frozen blob, temp word excluded (#638)");
    OSFF_ASSERT(gSaveContext.sceneFlags[SCENE_MARKET_DAY].collect == kCollectBit,
                "the LIVE gSaveContext must carry the flushed bit as well: OoT's suspend-time capture reads the "
                "live struct, not the blob");
    ResetEntranceTable();
    Context_ClearFrozenState(GAME_OOT);

    // ---- 2. Hot-swap driver: the same, through the launcher's bridge -------
    ArmLiveOoTSession();
    play->actorCtx.flags.collect = kCollectBit;
    play->actorCtx.flags.chest = 0;
    play->actorCtx.flags.swch = 0;
    play->actorCtx.flags.tempCollect = 0;
    play->actorCtx.flags.tempSwch = 0;
    OSFF_ASSERT(gSaveContext.sceneFlags[SCENE_MARKET_DAY].collect == 0,
                "non-vacuity: the saved collect word must be ZERO before the hot-swap act");
    OSFF_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1, "the hot-swap driver must record a fresh OoT blob");
    OSFF_ASSERT(Context_GetFrozenReturnEntrance(GAME_OOT) == OOT_ENTR_MARKET_FROM_MASK_SHOP,
                "test setup: the hot-swap freeze must still tag OoT's own portal return (#364)");
    OSFF_ASSERT(ReadFrozenOoT(), "the hot-swap frozen OoT half must read back");
    OSFF_ASSERT(sScratch.sceneFlags[SCENE_MARKET_DAY].collect == kCollectBit,
                "the collectible taken in the departure scene must be in the F10 blob -- the hot swap breaks the "
                "graph loop with the flags still only in the PlayState (#638)");
    Context_ClearFrozenState(GAME_OOT);

    // ---- 3. No PlayState: both drivers still freeze, nothing is dereferenced
    OoT_gPlayState = NULL;
    ArmLiveOoTSession();
    gSaveContext.sceneFlags[SCENE_MARKET_DAY].collect = kCollectBit; // what the last transition left
    OSFF_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1,
                "a hot-swap freeze with no PlayState must not crash and must still record a blob");
    OSFF_ASSERT(ReadFrozenOoT(), "the no-PlayState frozen OoT half must read back");
    OSFF_ASSERT(sScratch.sceneFlags[SCENE_MARKET_DAY].collect == kCollectBit,
                "with no PlayState there is nothing to flush; the saved flags the last transition left must be "
                "frozen as they are");
    Context_ClearFrozenState(GAME_OOT);
    Combo_CheckEntranceSwitch(OOT_ENTR_HAPPY_MASK_SHOP);
    OSFF_ASSERT(Context_HasFrozenState(GAME_OOT) == 1,
                "an entrance freeze with no PlayState must not crash and must still record a blob");
    ResetEntranceTable();
    Context_ClearFrozenState(GAME_OOT);

    return 0;
}

} // namespace

extern "C" int OoT_SceneFlagFreeze_RunHeadless(void) {
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    const GameId prevGame = Context_GetCurrentGame();
    // Combo_CheckEntranceSwitch resolves the departing game -- and therefore
    // which entrance table and which blob -- from the current-game tracker.
    Context_SetCurrentGame(GAME_OOT);
    ResetEntranceTable();

    // A minimal live PlayState: the flush reads sceneNum and actorCtx.flags and
    // nothing else. calloc'd on the C heap, not OoT's arena.
    PlayState* play = (PlayState*)calloc(1, sizeof(PlayState));
    int rc = 1;
    if (play == NULL) {
        printf("[TEST] FAIL: could not allocate a PlayState (%s:%d)\n", __FILE__, __LINE__);
    } else {
        play->sceneNum = SCENE_MARKET_DAY;
        OoT_gPlayState = play;
        rc = RunChecks(play);
    }

    // Leave global state clean for whatever runs next in this process, on
    // pass or fail: the published PlayState above must never outlive this row.
    OoT_gPlayState = NULL;
    free(play);
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    ResetEntranceTable();
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] oot-scene-flag-freeze: PASS\n");
    }
    return rc;
}
