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
 *
 * #850 rides the same seam (OoT_Combo_StampSavedSceneForFreeze): since #837 a
 * crossing commits the departure freeze, and a reload of it places Link by the
 * frozen savedSceneNum (OoT_Sram_OpenSave), which only a save wrote. Leg 2c
 * locks that the departure records the scene Link stands in, as a save there
 * would: F10 in a grotto, a fairy fountain, a boss room and the tower collapse,
 * the door from the Market, and Play_PerformSave's own guard (a debug save
 * records nothing); leg 3 locks that no PlayState keeps the last save's scene.
 */

#include <z64.h>

#include "context.h"
#include "entrance.h"
#include "game.h"
#include "save.h" // RsbsSave_Get/SetActiveSlot: the door leg restores the slot it publishes (#850)

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
//
// ComboEntrance_Init(), the same reset the MM twin uses -- and, since #665, part
// of that issue's lock.
//
// Until #665 the combo reset was spelled Entrance_Init and this TU could not
// name it. <z64.h> reaches soh/Enhancements/randomizer/randomizer_entrance.h
// through games/oot/include/z64save.h, and that header declares
// `extern "C" void Entrance_Init(void)` -- SoH's ENTRANCE-SHUFFLE initializer,
// a different function with the same spelling and signature. With <z64.h> first
// (the only order MSVC accepted) the call bound to SoH's init, which walks
// Randomizer_GetEntranceOverrides() and gEntranceTable with no rando context:
// an access violation, which is how this row first failed (PR #650). With
// entrance.h first MSVC rejected the file with C2732. The two were distinct
// link symbols (C vs mangled C++), so no link-time gate could see the pair.
// The row used to compose the reset from primitives instead.
//
// This TU includes <z64.h> AND entrance.h and calls the combo reset by name, so
// a regression to a port's spelling fails here: it binds to the port's function
// (the checks below then see an uncleared table, or the call crashes) or it does
// not compile. The source-level guard is the FUNC scan in
// .github/scripts/check-odr-declaration-collisions.py.
//
// Arranges every piece of state the reset must undo -- a link, a pending
// entrance switch, a startup entrance, an F10 request and both games'
// cross-game arrival latches -- and checks each one really is armed before the
// call, so every clause after it is observed, not assumed. Returns false if any
// of it was not armed, if any of it survived, or if the production links could
// not be registered afterwards: Entrance_RegisterDefaultLinks refuses a door
// that some link already claims, so a true return is proof the link table
// really was cleared by a call that reached the combo entrance module.
//
// Wiping the arrival latch on every call (including the final cleanup) is a
// change from the old composed reset, which left it alone. The MM twin always
// did this, and it is what ComboEntrance_Init means: session state starts clean.
bool ResetEntranceTable(void) {
    if (Entrance_GetLinkCount() == 0) {
        (void)Entrance_RegisterDefaultLinks();
    }
    gPendingSwitch.requested = true;
    gPendingSwitch.targetGame = GAME_MM;
    Combo_SetStartupEntrance(OOT_ENTR_MARKET_FROM_MASK_SHOP);
    Combo_RequestGameSwitch();
    Entrance_NoteCrossGameArrival(GAME_OOT);
    Entrance_NoteCrossGameArrival(GAME_MM);

    if (Entrance_GetLinkCount() == 0 || !gPendingSwitch.requested || !Combo_HasStartupEntrance() ||
        !Combo_IsGameSwitchRequested() || !Entrance_IsCrossGameHalf(GAME_OOT) || !Entrance_IsCrossGameHalf(GAME_MM)) {
        printf("[TEST] ResetEntranceTable could not arm the state it checks: links=%zu pending=%d startup=%d "
               "f10=%d halfOoT=%d halfMM=%d\n",
               Entrance_GetLinkCount(), (int)gPendingSwitch.requested, (int)Combo_HasStartupEntrance(),
               (int)Combo_IsGameSwitchRequested(), (int)Entrance_IsCrossGameHalf(GAME_OOT),
               (int)Entrance_IsCrossGameHalf(GAME_MM));
        return false;
    }

    ComboEntrance_Init();

    if (Entrance_GetLinkCount() != 0 || gPendingSwitch.requested || Combo_HasStartupEntrance() ||
        Combo_IsGameSwitchRequested() || Entrance_IsCrossGameHalf(GAME_OOT) || Entrance_IsCrossGameHalf(GAME_MM)) {
        printf("[TEST] ComboEntrance_Init left state behind: links=%zu pending=%d startup=%d f10=%d halfOoT=%d "
               "halfMM=%d\n",
               Entrance_GetLinkCount(), (int)gPendingSwitch.requested, (int)Combo_HasStartupEntrance(),
               (int)Combo_IsGameSwitchRequested(), (int)Entrance_IsCrossGameHalf(GAME_OOT),
               (int)Entrance_IsCrossGameHalf(GAME_MM));
        return false;
    }
    return Entrance_RegisterDefaultLinks();
}

bool ReadFrozenOoT(void) {
    memset(&sScratch, 0, sizeof(SaveContext));
    return Context_RestoreState(GAME_OOT, &sScratch, sizeof(SaveContext)) != 0;
}

// #850. A real file (fileNum 0) whose last save was in `lastSave`, playing in
// `scene`. The scene flags are not under test here and stay clear.
void ArmStampLeg(PlayState* play, s16 lastSave, s16 scene) {
    ArmLiveOoTSession();
    gSaveContext.fileNum = 0;
    gSaveContext.savedSceneNum = lastSave;
    memset(&play->actorCtx.flags, 0, sizeof(play->actorCtx.flags));
    play->sceneNum = scene;
}

int StampChecks(PlayState* play) {
    const s16 kLastSave = SCENE_KOKIRI_FOREST;
    // (a) F10, the only trigger that can stand in these scenes: each is one
    // OoT_Sram_OpenSave sends somewhere safe (a grotto or fountain to Link's
    // House even with Remember Save Location; a boss room to its dungeon's
    // entrance; the collapse to Ganon's Tower).
    const struct {
        s16 scene;
        const char* name;
    } kInteriors[] = {
        { SCENE_GROTTOS, "a grotto" },
        { SCENE_FAIRYS_FOUNTAIN, "a fairy fountain" },
        { SCENE_DEKU_TREE_BOSS, "a boss room" },
        { SCENE_GANONS_TOWER_COLLAPSE_INTERIOR, "the tower collapse" },
    };
    for (const auto& leg : kInteriors) {
        ArmStampLeg(play, kLastSave, leg.scene);
        OSFF_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1, "#850 setup: the F10 departure freezes");
        OSFF_ASSERT(ReadFrozenOoT(), "#850 setup: the F10 frozen OoT half reads back");
        printf("[TEST] #850: F10 in %s (scene %d), last save in scene %d: frozen savedSceneNum %d\n", leg.name,
               (int)leg.scene, (int)kLastSave, (int)sScratch.savedSceneNum);
        OSFF_ASSERT(sScratch.savedSceneNum == leg.scene,
                    "#850: the departure freeze must record the scene Link departs from as the save's scene, as a "
                    "save made there would; it carried the last save's scene, so a reload of this crossing commit "
                    "with Remember Save Location woke Link inside the interior");
        Context_ClearFrozenState(GAME_OOT);
    }

    // (b) The door, through the entrance driver: Link walks into the Happy
    // Mask Shop from the Market with his last save in a dungeon. A save in the
    // Market would not send him back to the Deku Tree. The driver publishes
    // fileNum 0 as the active slot; the row puts the previous one back.
    const int prevSlot = RsbsSave_GetActiveSlot();
    ArmStampLeg(play, SCENE_DEKU_TREE, SCENE_MARKET_DAY);
    Combo_CheckEntranceSwitch(OOT_ENTR_HAPPY_MASK_SHOP);
    RsbsSave_SetActiveSlot(prevSlot);
    OSFF_ASSERT(Combo_IsCrossGameSwitch() && Context_HasFrozenState(GAME_OOT) == 1,
                "#850 setup: the Happy Mask Shop door freezes the departing OoT half");
    OSFF_ASSERT(ReadFrozenOoT(), "#850 setup: the door's frozen OoT half reads back");
    printf("[TEST] #850: door from the Market (scene %d), last save in scene %d: frozen savedSceneNum %d\n",
           (int)SCENE_MARKET_DAY, (int)SCENE_DEKU_TREE, (int)sScratch.savedSceneNum);
    OSFF_ASSERT(sScratch.savedSceneNum == SCENE_MARKET_DAY,
                "#850: the door's departure freeze must record the Market, not the last save's dungeon");
    OSFF_ASSERT(ResetEntranceTable(), "the combo entrance table must reset between legs");
    Context_ClearFrozenState(GAME_OOT);

    // (c) Play_PerformSave's own guard: a debug save (fileNum 0xFF) is never
    // saved, so its departure records nothing either.
    ArmStampLeg(play, kLastSave, SCENE_GROTTOS);
    gSaveContext.fileNum = 0xFF;
    OSFF_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1 && ReadFrozenOoT(),
                "#850 setup: a debug-save F10 freezes");
    OSFF_ASSERT(sScratch.savedSceneNum == kLastSave,
                "#850: a debug save (fileNum 0xFF) is never saved, so its departure must not record a scene");
    Context_ClearFrozenState(GAME_OOT);

    play->sceneNum = SCENE_MARKET_DAY;
    return 0;
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
    OSFF_ASSERT(ResetEntranceTable(), "the combo entrance table must reset between legs");
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
    OSFF_ASSERT(Context_FrozenStateIsLiveFile(GAME_OOT) == 1,
                "#837: a GAMEMODE_NORMAL departure must freeze as a live file, or the crossing commit would skip "
                "every real OoT crossing");
    Context_ClearFrozenState(GAME_OOT);

    // ---- 2b. (#837) The flush notes liveness for the crossing commit --------
    // F10 is polled ungated, so a title-screen or file-select OoT can depart.
    // The production OoT_Combo_DepartureIsLiveFile, reached through the real
    // driver's flush, must mark that freeze NOT live so Switch_CommitCrossing
    // never writes it into the player's slot. Delete the flush's OoT note, or
    // make OoT_Combo_DepartureIsLiveFile answer 1, and this goes red; make it
    // answer 0 and leg 2's live assertion goes red.
    ArmLiveOoTSession();
    gSaveContext.gameMode = GAMEMODE_TITLE_SCREEN;
    OSFF_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1, "a title-screen hot swap must still freeze");
    OSFF_ASSERT(Context_FrozenStateIsLiveFile(GAME_OOT) == 0,
                "#837: a TITLE_SCREEN departure must freeze as NOT a live file, or the crossing commit writes the "
                "title screen's save into the player's armed slot");
    Context_ClearFrozenState(GAME_OOT);

    // ---- 2c. (#850) The departure records the scene Link stands in --------
    // A reload of a crossing commit boots OoT through OoT_Sram_OpenSave, which
    // places Link by the frozen half's savedSceneNum (z_sram.c). Only a save
    // wrote that field, so an F10 inside a grotto, a fountain, a boss room or the
    // tower collapse froze the scene of the LAST SAVE, and with Remember Save
    // Location the reload woke Link inside the interior. Each scene below is one
    // OpenSave sends somewhere safe; the last save was in Kokiri Forest, which
    // OpenSave would not, so a frozen Kokiri Forest is the bug.
    if (int rc = StampChecks(play)) {
        return rc;
    }

    // ---- 3. No PlayState: both drivers still freeze, nothing is dereferenced
    OoT_gPlayState = NULL;
    ArmLiveOoTSession();
    gSaveContext.savedSceneNum = SCENE_KOKIRI_FOREST;                // the last save's scene
    gSaveContext.sceneFlags[SCENE_MARKET_DAY].collect = kCollectBit; // what the last transition left
    OSFF_ASSERT(Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1,
                "a hot-swap freeze with no PlayState must not crash and must still record a blob");
    OSFF_ASSERT(ReadFrozenOoT(), "the no-PlayState frozen OoT half must read back");
    OSFF_ASSERT(sScratch.sceneFlags[SCENE_MARKET_DAY].collect == kCollectBit,
                "with no PlayState there is nothing to flush; the saved flags the last transition left must be "
                "frozen as they are");
    OSFF_ASSERT(sScratch.savedSceneNum == SCENE_KOKIRI_FOREST,
                "#850: with no PlayState there is no scene to record; the last save's scene must be frozen as is");
    Context_ClearFrozenState(GAME_OOT);
    Combo_CheckEntranceSwitch(OOT_ENTR_HAPPY_MASK_SHOP);
    OSFF_ASSERT(Context_HasFrozenState(GAME_OOT) == 1,
                "an entrance freeze with no PlayState must not crash and must still record a blob");
    OSFF_ASSERT(ResetEntranceTable(), "the combo entrance table must reset between legs");
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
    if (!ResetEntranceTable()) {
        // A false return means the link table was not cleared, i.e. these calls
        // did not reach the combo entrance module. See ResetEntranceTable.
        printf("[TEST] FAIL: the combo entrance table did not reset (%s:%d)\n", __FILE__, __LINE__);
        Context_SetCurrentGame(prevGame);
        return 1;
    }

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
    (void)ResetEntranceTable();
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] oot-scene-flag-freeze: PASS\n");
    }
    return rc;
}
