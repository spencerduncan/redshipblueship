/**
 * @file oot_departure_scene_exit_test.cpp
 * ROM-free, display-free locks for #770's two persistent-flag rows: the save
 * writes two actor Destroys make on ANY exit from their scene must reach the
 * frozen OoT half when a cross-game departure skips those Destroys. CTest label
 * "redship", rows OoTDepartureWindmillFlag and OoTDepartureLakeFlag in
 * CMake/SingleExecutable.cmake, dispatch "oot-departure-windmill-flag" and
 * "oot-departure-lake-flag" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. Both cross-game departures retire OoT's Play gamestate
 * without Play_Destroy (#750, PR #767), so no actor Destroy runs for the actors
 * live at that instant. Two of them write the save unconditionally on scene
 * exit:
 *  - BgRelayObjects_Destroy (z_bg_relay_objects.c): the windmill gear unsets
 *    EVENTCHKINF_PLAYED_SONG_OF_STORMS_IN_WINDMILL when cutsceneIndex < 0xFFF0.
 *    An F10 out of the windmill froze the flag SET, so the gear (and
 *    Kakariko's sails) came back spinning fast where any door exit stops them.
 *  - BgSpot06Objects_Destroy (z_bg_spot06_objects.c): in rando, once the Water
 *    Temple blue warp is used, every Lake Hylia object sets
 *    EVENTCHKINF_RAISED_LAKE_HYLIA_WATER on exit (the water-control switch
 *    lowers the lake for one visit only), and it writes the lower river water
 *    box's zMin back on the cached collision header. An F10 out of a lowered
 *    lake froze it lowered for good.
 *
 * THE FIX UNDER TEST. Combo_FlushLiveStateForFreeze (src/common/switch.cpp),
 * called by both freeze drivers right before the blob is captured, runs
 * OoT_Combo_ApplySceneExitWritesForFreeze (games/oot/soh/GameExports_SingleExe.cpp):
 * it walks the live actor lists and applies each Destroy's write under that
 * Destroy's own condition. Each row drives the two REAL drivers
 * (Combo_FreezeActiveGameForHotSwap, the F10 path; Combo_CheckEntranceSwitch,
 * the door path) against a calloc'd PlayState whose actor lists hold calloc'd
 * actors, and reads the frozen blob back.
 *
 * THE CONTROLS (the Destroy's condition, not a scene, decides): a Dampe race
 * door or a killed duplicate gear writes nothing; a gear during a scripted
 * cutscene (cutsceneIndex >= 0xFFF0) writes nothing; a vanilla file or a rando
 * file without the blue warp keeps the lake lowered (the zMin write still
 * happens: that half of the Destroy is unconditional); an actor whose destroy
 * is NULL is skipped as OoT_Actor_Destroy skips it; and no PlayState at all
 * leaves the save as it was. Every other eventChkInf word is compared whole.
 *
 * #807 ADDS TWO ROWS ON THE SAME SEAM, the residues PR #792 recorded:
 *  - OoTDepartureSunSwitchFlag ("oot-departure-sun-switch-flag"): with SoH's
 *    Sunlight Arrows, OoT_ObjLightswitch_Destroy unsets the switch flag of a
 *    sun switch a Light Arrow lit, so the activation lasts one visit. A
 *    departure froze it set for good: the #638 scene-flag flush copied the
 *    still-set live flag. The seam now applies the Destroy's unset BEFORE the
 *    flush, the order Actor_CleanupContext uses (Destroys, then
 *    Play_SaveSceneFlags).
 *  - OoTDepartureLinkAge ("oot-departure-link-age"): between an age-change
 *    write to play->linkAgeOnLoad and the reload it triggers, a departure froze
 *    the old age. The seam now applies Play_Destroy's equipment swap and the
 *    Player Destroy's linkAge write.
 */

#include <z64.h>

#include "context.h"
#include "entrance.h"
#include "game.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" SaveContext gSaveContext;
// games/oot/src/code/z_play.c: set by Play_Init, nulled by Play_Destroy and
// OoT_Graph_ResetRunFrameContext. The seam reads it; this row publishes it.
extern "C" PlayState* OoT_gPlayState;
// The two REAL freeze drivers (games/oot/soh/GameExports_SingleExe.cpp).
extern "C" uint16_t Combo_CheckEntranceSwitch(uint16_t entranceIndex);
extern "C" int Combo_FreezeActiveGameForHotSwap(GameId departing);
// z_obj_lightswitch.c (RSBS_SINGLE_EXECUTABLE): seed / read SoH's Sunlight
// Arrows static, sunSwitchActivatedByLightArrow.
extern "C" void OoT_ObjLightswitch_SetDestroyStaticsForTest(s32 dirty);
extern "C" s32 OoT_ObjLightswitch_DestroyStaticsDirtyForTest(void);

namespace {

#define ODSE_ASSERT(cond, msg)                                            \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// The overlays' private values (their .c files): WindmillSetpiecesMode,
// LakeHyliaObjectsType, LakeHyliaWaterBoxIndices, WATER_LEVEL_RIVER_LOWER_Z.
const s16 kGear = 0;
const s16 kDampeDoor = 1;
const s16 kKilledDuplicateGear = 0xFF;
const s16 kLakeWaterPlane = 2;
const int kRiverLowerBox = 1;
const s16 kRiverLowerZMin = 2203;
// What BgSpot06Objects_Init leaves after one lowered-lake load (zMin -= 50).
const s16 kRiverLowerZMinLowered = kRiverLowerZMin - 50;

constexpr size_t kEventWords = sizeof(gSaveContext.eventChkInf) / sizeof(gSaveContext.eventChkInf[0]);
constexpr int kMaxActors = 4;

// Frozen-blob readback target. File-static: SoH's runtime SaveContext carries
// the ship.* extensions and is over 100 KB.
SaveContext sScratch;
// Snapshot of the live words before an act, to prove nothing else moved.
u16 sEventBefore[kEventWords];

void DummyDestroy(Actor* actor, PlayState* play) {
    (void)actor;
    (void)play;
}

bool EventSet(const SaveContext* save, s32 flag) {
    return (save->eventChkInf[flag >> 4] & (1 << (flag & 0xF))) != 0;
}

void SetEvent(s32 flag) {
    gSaveContext.eventChkInf[flag >> 4] |= (u16)(1 << (flag & 0xF));
}

// Every eventChkInf word of the blob equals the pre-act snapshot, except the
// one bit @p flag (pass -1 for "no bit may move"), which must read @p expected.
bool OnlyFlagMoved(const SaveContext* save, s32 flag, bool expected) {
    for (size_t i = 0; i < kEventWords; i++) {
        u16 mask = 0;
        if (flag >= 0 && (size_t)(flag >> 4) == i) {
            mask = (u16)(1 << (flag & 0xF));
        }
        if ((save->eventChkInf[i] & ~mask) != (sEventBefore[i] & ~mask)) {
            printf("[TEST] eventChkInf[%zu] moved: 0x%04X -> 0x%04X (mask 0x%04X)\n", i, sEventBefore[i],
                   save->eventChkInf[i], mask);
            return false;
        }
    }
    return flag < 0 || EventSet(save, flag) == expected;
}

void Snapshot(void) {
    memcpy(sEventBefore, gSaveContext.eventChkInf, sizeof(sEventBefore));
}

// A live OoT gameplay session: GAMEMODE_NORMAL, the 0xFF slot sentinel (out of
// range, so the entrance driver never publishes a save slot), health alive so
// the #664 revive at the same seam stays out of the picture.
void ArmLiveOoTSession(void) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.healthCapacity = 0x30;
    gSaveContext.health = 0x30;
}

// The actor pool this row owns. Linked at the head of the category list, as
// Actor_AddToCategory does.
struct Scene {
    PlayState* play;
    Actor* actors[kMaxActors];
    int count;
};

Actor* AddActor(Scene* scene, s16 id, s16 params, u8 category, bool hasDestroy) {
    Actor* actor = (Actor*)calloc(1, sizeof(Actor));
    if (actor == NULL || scene->count >= kMaxActors) {
        free(actor);
        return NULL;
    }
    actor->id = id;
    actor->params = params;
    actor->category = category;
    actor->destroy = hasDestroy ? DummyDestroy : NULL;
    ActorListEntry* list = &scene->play->actorCtx.actorLists[category];
    actor->next = list->head;
    if (list->head != NULL) {
        list->head->prev = actor;
    }
    list->head = actor;
    list->length++;
    scene->actors[scene->count++] = actor;
    return actor;
}

void ClearActors(Scene* scene) {
    for (int i = 0; i < scene->count; i++) {
        free(scene->actors[i]);
        scene->actors[i] = NULL;
    }
    scene->count = 0;
    memset(scene->play->actorCtx.actorLists, 0, sizeof(scene->play->actorCtx.actorLists));
}

// Fresh combo entrance table with the production links, no pending switch.
// Combo_CheckEntranceSwitch suppresses its freeze while a switch is pending, so
// every entrance leg needs this first. ComboEntrance_Init is the combo reset
// (not SoH's same-spelled entrance-shuffle init; see
// oot_scene_flag_freeze_test.cpp for why the spelling matters here).
bool ResetEntranceTable(void) {
    ComboEntrance_Init();
    if (Entrance_GetLinkCount() != 0 || gPendingSwitch.requested || Combo_IsGameSwitchRequested()) {
        printf("[TEST] ComboEntrance_Init left state behind\n");
        return false;
    }
    return Entrance_RegisterDefaultLinks();
}

bool ReadFrozenOoT(void) {
    memset(&sScratch, 0, sizeof(SaveContext));
    return Context_RestoreState(GAME_OOT, &sScratch, sizeof(SaveContext)) != 0;
}

// One F10 departure through the real hot-swap driver; the blob lands in sScratch.
bool DepartF10(void) {
    Context_ClearFrozenState(GAME_OOT);
    return Combo_FreezeActiveGameForHotSwap(GAME_OOT) == 1 && ReadFrozenOoT();
}

// One door departure through the real entrance driver (the Happy Mask Shop door
// is the one production cross-game door; the seam keys on the live actors, not
// on the scene, so the same rule covers any cross-game exit a future entrance
// shuffle adds).
bool DepartDoor(void) {
    Context_ClearFrozenState(GAME_OOT);
    if (!ResetEntranceTable()) {
        return false;
    }
    Combo_CheckEntranceSwitch(OOT_ENTR_HAPPY_MASK_SHOP);
    const bool ok = Combo_IsCrossGameSwitch() && Context_HasFrozenState(GAME_OOT) == 1 && ReadFrozenOoT();
    (void)ResetEntranceTable();
    return ok;
}

const s32 kStorms = EVENTCHKINF_PLAYED_SONG_OF_STORMS_IN_WINDMILL;

// Arm the windmill: a live session inside it with the Song of Storms flag set.
void ArmWindmill(Scene* scene, s32 cutsceneIndex) {
    ArmLiveOoTSession();
    ClearActors(scene);
    scene->play->sceneNum = SCENE_WINDMILL_AND_DAMPES_GRAVE;
    gSaveContext.cutsceneIndex = cutsceneIndex;
    SetEvent(kStorms);
    Snapshot();
}

int RunWindmill(Scene* scene) {
    // ---- 1. F10 out of the windmill: the gear's exit write reaches the blob --
    ArmWindmill(scene, 0);
    ODSE_ASSERT(AddActor(scene, ACTOR_BG_RELAY_OBJECTS, kGear, ACTORCAT_BG, true) != NULL, "setup: gear actor");
    ODSE_ASSERT(AddActor(scene, ACTOR_BG_RELAY_OBJECTS, kDampeDoor, ACTORCAT_BG, true) != NULL, "setup: door actor");
    ODSE_ASSERT(EventSet(&gSaveContext, kStorms),
                "non-vacuity: the Song of Storms windmill flag must be SET before the departure");
    ODSE_ASSERT(DepartF10(), "the hot-swap driver must record and read back a fresh OoT blob");
    ODSE_ASSERT(OnlyFlagMoved(&sScratch, kStorms, false),
                "an F10 out of the windmill must freeze the Song of Storms windmill flag CLEARED, as the gear's "
                "Destroy leaves it on every non-cutscene exit, and move no other eventChkInf bit (#770)");
    ODSE_ASSERT(!EventSet(&gSaveContext, kStorms), "the live gSaveContext must carry the cleared flag too");

    // ---- 2. The door driver: the same seam, the same write ---------------
    ArmWindmill(scene, 0);
    ODSE_ASSERT(AddActor(scene, ACTOR_BG_RELAY_OBJECTS, kGear, ACTORCAT_BG, true) != NULL, "setup: gear actor");
    ODSE_ASSERT(DepartDoor(), "the entrance driver must record and read back a fresh OoT blob");
    ODSE_ASSERT(OnlyFlagMoved(&sScratch, kStorms, false),
                "a cross-game door departure with the gear live must freeze the flag cleared (#770)");

    // ---- 3. Controls: the Destroy's own condition decides -----------------
    // (a) no gear: a Dampe race door and a killed duplicate gear write nothing.
    ArmWindmill(scene, 0);
    ODSE_ASSERT(AddActor(scene, ACTOR_BG_RELAY_OBJECTS, kDampeDoor, ACTORCAT_BG, true) != NULL, "setup: door");
    ODSE_ASSERT(AddActor(scene, ACTOR_BG_RELAY_OBJECTS, kKilledDuplicateGear, ACTORCAT_BG, true) != NULL,
                "setup: killed duplicate gear");
    ODSE_ASSERT(DepartF10(), "control (a): blob");
    ODSE_ASSERT(OnlyFlagMoved(&sScratch, -1, true),
                "control (a): with no gear live (a Dampe door, a killed duplicate) nothing may be written");
    // (b) a scripted cutscene is running: the Destroy writes nothing.
    ArmWindmill(scene, 0xFFF0);
    ODSE_ASSERT(AddActor(scene, ACTOR_BG_RELAY_OBJECTS, kGear, ACTORCAT_BG, true) != NULL, "setup: gear actor");
    ODSE_ASSERT(DepartF10(), "control (b): blob");
    ODSE_ASSERT(OnlyFlagMoved(&sScratch, -1, true),
                "control (b): with cutsceneIndex >= 0xFFF0 the gear's Destroy keeps the flag, and so must the seam");
    // (c) an actor OoT_Actor_Destroy would not call (destroy == NULL).
    ArmWindmill(scene, 0);
    ODSE_ASSERT(AddActor(scene, ACTOR_BG_RELAY_OBJECTS, kGear, ACTORCAT_BG, false) != NULL, "setup: gear actor");
    ODSE_ASSERT(DepartF10(), "control (c): blob");
    ODSE_ASSERT(OnlyFlagMoved(&sScratch, -1, true), "control (c): a gear with no destroy function writes nothing");
    // (d) no PlayState: nothing is dereferenced, the save is frozen as it is.
    ArmWindmill(scene, 0);
    ODSE_ASSERT(AddActor(scene, ACTOR_BG_RELAY_OBJECTS, kGear, ACTORCAT_BG, true) != NULL, "setup: gear actor");
    OoT_gPlayState = NULL;
    const bool departed = DepartF10();
    OoT_gPlayState = scene->play;
    ODSE_ASSERT(departed, "control (d): a freeze with no PlayState must still record a blob");
    ODSE_ASSERT(OnlyFlagMoved(&sScratch, -1, true), "control (d): with no PlayState nothing may be written");
    return 0;
}

const s32 kRaised = EVENTCHKINF_RAISED_LAKE_HYLIA_WATER;
const s32 kBlueWarp = EVENTCHKINF_USED_WATER_TEMPLE_BLUE_WARP;

// Arm Lake Hylia, lowered for this visit, with the given quest and blue warp.
void ArmLake(Scene* scene, WaterBox* boxes, bool rando, bool blueWarp) {
    ArmLiveOoTSession();
    ClearActors(scene);
    scene->play->sceneNum = SCENE_LAKE_HYLIA;
    gSaveContext.ship.quest.id = rando ? QUEST_RANDOMIZER : QUEST_NORMAL;
    if (blueWarp) {
        SetEvent(kBlueWarp);
    }
    for (int i = 0; i < 4; i++) {
        boxes[i].zMin = (s16)(1000 + i);
    }
    boxes[kRiverLowerBox].zMin = kRiverLowerZMinLowered;
    Snapshot();
}

int RunLake(Scene* scene) {
    WaterBox boxes[4] = {};
    CollisionHeader colHeader = {};
    colHeader.numWaterBoxes = 4;
    colHeader.waterBoxes = boxes;
    scene->play->colCtx.colHeader = &colHeader;
    int rc = 1;

    do {
        // ---- 1. F10 out of a lowered lake after the blue warp, in rando ----
        ArmLake(scene, boxes, true, true);
        if (AddActor(scene, ACTOR_BG_SPOT06_OBJECTS, kLakeWaterPlane, ACTORCAT_PROP, true) == NULL) {
            printf("[TEST] FAIL: setup: water plane actor\n");
            break;
        }
        if (EventSet(&gSaveContext, kRaised) || !IS_RANDO || !EventSet(&gSaveContext, kBlueWarp)) {
            printf("[TEST] FAIL: non-vacuity: the lake must be lowered, rando, blue warp used before the act\n");
            break;
        }
        if (!DepartF10()) {
            printf("[TEST] FAIL: the hot-swap driver must record and read back a fresh OoT blob\n");
            break;
        }
        if (!OnlyFlagMoved(&sScratch, kRaised, true)) {
            printf("[TEST] FAIL: an F10 out of Lake Hylia after the Water Temple blue warp (rando) must freeze the "
                   "lake RAISED, as every Lake Hylia object's Destroy leaves it, and move no other bit (#770)\n");
            break;
        }
        if (boxes[kRiverLowerBox].zMin != kRiverLowerZMin || boxes[0].zMin != 1000 || boxes[2].zMin != 1002 ||
            boxes[3].zMin != 1003) {
            printf("[TEST] FAIL: the Destroy's river water box write (zMin back to %d, others untouched) must be "
                   "applied on the cached collision header: box1=%d box0=%d box2=%d box3=%d (#770)\n",
                   kRiverLowerZMin, boxes[1].zMin, boxes[0].zMin, boxes[2].zMin, boxes[3].zMin);
            break;
        }

        // ---- 2. The door driver ------------------------------------------
        ArmLake(scene, boxes, true, true);
        (void)AddActor(scene, ACTOR_BG_SPOT06_OBJECTS, kLakeWaterPlane, ACTORCAT_PROP, true);
        if (!DepartDoor() || !OnlyFlagMoved(&sScratch, kRaised, true)) {
            printf("[TEST] FAIL: a cross-game door departure with the lake objects live must freeze it raised\n");
            break;
        }

        // ---- 3. Controls --------------------------------------------------
        // (a) vanilla quest: no flag write, but the zMin write is unconditional.
        ArmLake(scene, boxes, false, true);
        (void)AddActor(scene, ACTOR_BG_SPOT06_OBJECTS, kLakeWaterPlane, ACTORCAT_PROP, true);
        if (!DepartF10() || !OnlyFlagMoved(&sScratch, -1, true) || boxes[kRiverLowerBox].zMin != kRiverLowerZMin) {
            printf("[TEST] FAIL: control (a): a vanilla file keeps the lake as it is; zMin is still written back\n");
            break;
        }
        // (b) rando without the blue warp: the lake stays lowered.
        ArmLake(scene, boxes, true, false);
        (void)AddActor(scene, ACTOR_BG_SPOT06_OBJECTS, kLakeWaterPlane, ACTORCAT_PROP, true);
        if (!DepartF10() || !OnlyFlagMoved(&sScratch, -1, true)) {
            printf("[TEST] FAIL: control (b): rando without the Water Temple blue warp must not raise the lake\n");
            break;
        }
        // (c) Lake Hylia with no lake object live: nothing to destroy.
        ArmLake(scene, boxes, true, true);
        if (!DepartF10() || !OnlyFlagMoved(&sScratch, -1, true) ||
            boxes[kRiverLowerBox].zMin != kRiverLowerZMinLowered) {
            printf("[TEST] FAIL: control (c): with no Bg_Spot06_Objects live nothing may be written\n");
            break;
        }
        // (d) no PlayState.
        ArmLake(scene, boxes, true, true);
        (void)AddActor(scene, ACTOR_BG_SPOT06_OBJECTS, kLakeWaterPlane, ACTORCAT_PROP, true);
        OoT_gPlayState = NULL;
        const bool departed = DepartF10();
        OoT_gPlayState = scene->play;
        if (!departed || !OnlyFlagMoved(&sScratch, -1, true) || boxes[kRiverLowerBox].zMin != kRiverLowerZMinLowered) {
            printf("[TEST] FAIL: control (d): with no PlayState nothing may be written\n");
            break;
        }
        rc = 0;
    } while (0);

    scene->play->colCtx.colHeader = NULL;
    return rc;
}

// ============================================================================
// #807 (1): Obj_Lightswitch under SoH's Sunlight Arrows
// ============================================================================
//
// OoT_ObjLightswitch_Destroy (z_obj_lightswitch.c): while the overlay static
// sunSwitchActivatedByLightArrow is set, the first sun switch destroyed whose
// type is not BURN unsets its own switch flag (unless it sits in room 25, the
// Spirit Temple chain platform) and clears the static; a BURN switch writes
// nothing and leaves the static set. Actor_CleanupContext destroys every actor
// before Play_SaveSceneFlags copies the live flags into the save, so the unset
// is what reaches gSaveContext.sceneFlags on any exit.

const s16 kSunStayOn = 0; // OBJLIGHTSWITCH_TYPE_STAY_ON
const s16 kSunType1 = 1;  // OBJLIGHTSWITCH_TYPE_1
const s16 kSunType2 = 2;  // OBJLIGHTSWITCH_TYPE_2
const s16 kSunBurn = 3;   // OBJLIGHTSWITCH_TYPE_BURN
const s32 kSunFlag = 0x0A;
const s32 kSunFlag2 = 0x0B;
const s32 kSunFlag3 = 0x0C;
const s32 kOtherSwitch = 0x03; // an unrelated switch set during the visit
const u32 kLiveChest = 0x11;
const u32 kLiveClear = 0x22;
const u32 kLiveCollect = 0x44;
const s8 kSunRoom = 3;
const s8 kChainPlatformRoom = 25;

constexpr size_t kSceneCount = sizeof(gSaveContext.sceneFlags) / sizeof(gSaveContext.sceneFlags[0]);
SavedSceneFlags sSceneBefore[kSceneCount];

Actor* AddSunSwitch(Scene* scene, s32 flag, s16 type, s8 room, bool hasDestroy) {
    Actor* actor =
        AddActor(scene, ACTOR_OBJ_LIGHTSWITCH, (s16)((flag << 8) | (type << 4)), ACTORCAT_SWITCH, hasDestroy);
    if (actor != NULL) {
        actor->room = room;
    }
    return actor;
}

// A Spirit Temple visit: the live switch flags hold the lit sun switch(es) and
// one unrelated switch; the save still holds the previous visit (all clear).
void ArmSunSwitch(Scene* scene, bool litByLightArrow, u32 liveSwitches) {
    ArmLiveOoTSession();
    ClearActors(scene);
    scene->play->sceneNum = SCENE_SPIRIT_TEMPLE;
    scene->play->actorCtx.flags.swch = liveSwitches | (1u << kOtherSwitch);
    scene->play->actorCtx.flags.chest = kLiveChest;
    scene->play->actorCtx.flags.clear = kLiveClear;
    scene->play->actorCtx.flags.collect = kLiveCollect;
    OoT_ObjLightswitch_SetDestroyStaticsForTest(litByLightArrow ? 1 : 0);
    Snapshot();
    memcpy(sSceneBefore, gSaveContext.sceneFlags, sizeof(sSceneBefore));
}

// The blob's Spirit Temple flags are the live visit's (the #638 flush) with
// switch word @p expectedSwch, every other scene is untouched, and no
// eventChkInf bit moved.
bool SunSceneFlagsAre(const SaveContext* save, u32 expectedSwch) {
    const SavedSceneFlags* got = &save->sceneFlags[SCENE_SPIRIT_TEMPLE];
    if (got->swch != expectedSwch || got->chest != kLiveChest || got->clear != kLiveClear ||
        got->collect != kLiveCollect) {
        printf("[TEST] Spirit Temple flags: swch=0x%08X (want 0x%08X) chest=0x%X clear=0x%X collect=0x%X\n",
               (unsigned)got->swch, (unsigned)expectedSwch, (unsigned)got->chest, (unsigned)got->clear,
               (unsigned)got->collect);
        return false;
    }
    for (size_t i = 0; i < kSceneCount; i++) {
        if (i != (size_t)SCENE_SPIRIT_TEMPLE &&
            memcmp(&save->sceneFlags[i], &sSceneBefore[i], sizeof(sSceneBefore[i])) != 0) {
            printf("[TEST] sceneFlags[%zu] moved\n", i);
            return false;
        }
    }
    return OnlyFlagMoved(save, -1, true);
}

bool SunStaticSet(void) {
    return OoT_ObjLightswitch_DestroyStaticsDirtyForTest() != 0;
}

int RunSunSwitch(Scene* scene) {
    const u32 lit = 1u << kSunFlag;
    const u32 other = 1u << kOtherSwitch;

    // ---- 1. F10 out of the Spirit Temple: a STAY_ON switch lit by a Light
    //         Arrow is unset before the scene flags are frozen ----------------
    ArmSunSwitch(scene, true, lit);
    ODSE_ASSERT(AddSunSwitch(scene, kSunFlag, kSunStayOn, kSunRoom, true) != NULL, "setup: sun switch");
    ODSE_ASSERT(SunStaticSet() && (scene->play->actorCtx.flags.swch & lit),
                "non-vacuity: the switch flag and the Sunlight Arrows static must be SET before the departure");
    ODSE_ASSERT(DepartF10(), "the hot-swap driver must record and read back a fresh OoT blob");
    ODSE_ASSERT(SunSceneFlagsAre(&sScratch, other),
                "an F10 out of the Spirit Temple must freeze a sun switch lit by a Light Arrow UNSET, as its Destroy "
                "leaves it on any exit with Sunlight Arrows on, and keep every other scene flag the visit set (#807)");
    ODSE_ASSERT(!SunStaticSet(), "the seam must clear the Sunlight Arrows static, as the Destroy does");

    // ---- 2. The door driver, a TYPE_1 switch -----------------------------
    ArmSunSwitch(scene, true, lit);
    ODSE_ASSERT(AddSunSwitch(scene, kSunFlag, kSunType1, kSunRoom, true) != NULL, "setup: sun switch");
    ODSE_ASSERT(DepartDoor(), "the entrance driver must record and read back a fresh OoT blob");
    ODSE_ASSERT(SunSceneFlagsAre(&sScratch, other),
                "a cross-game door departure with a Light-Arrow-lit TYPE_1 sun switch must freeze it unset (#807)");

    // ---- 3. TYPE_2 takes the same case -----------------------------------
    ArmSunSwitch(scene, true, lit);
    ODSE_ASSERT(AddSunSwitch(scene, kSunFlag, kSunType2, kSunRoom, true) != NULL, "setup: sun switch");
    ODSE_ASSERT(DepartF10(), "leg 3: blob");
    ODSE_ASSERT(SunSceneFlagsAre(&sScratch, other), "a Light-Arrow-lit TYPE_2 sun switch must freeze unset (#807)");

    // ---- 4. Controls: the Destroy's own condition decides -----------------
    // (a) lit by sunlight (or Sunlight Arrows off): the static is clear.
    ArmSunSwitch(scene, false, lit);
    ODSE_ASSERT(AddSunSwitch(scene, kSunFlag, kSunStayOn, kSunRoom, true) != NULL, "setup: sun switch");
    ODSE_ASSERT(DepartF10(), "control (a): blob");
    ODSE_ASSERT(SunSceneFlagsAre(&sScratch, lit | other),
                "control (a): a sun switch not lit by a Light Arrow keeps its flag, as the Destroy leaves it");
    // (b) a BURN switch: the Destroy writes nothing and keeps the static.
    ArmSunSwitch(scene, true, lit);
    ODSE_ASSERT(AddSunSwitch(scene, kSunFlag, kSunBurn, kSunRoom, true) != NULL, "setup: burn switch");
    ODSE_ASSERT(DepartF10(), "control (b): blob");
    ODSE_ASSERT(SunSceneFlagsAre(&sScratch, lit | other) && SunStaticSet(),
                "control (b): a BURN sun switch's Destroy keeps the flag and the static");
    // (c) room 25 (the chain platform stays down): flag kept, static cleared.
    ArmSunSwitch(scene, true, lit);
    ODSE_ASSERT(AddSunSwitch(scene, kSunFlag, kSunStayOn, kChainPlatformRoom, true) != NULL, "setup: sun switch");
    ODSE_ASSERT(DepartF10(), "control (c): blob");
    ODSE_ASSERT(SunSceneFlagsAre(&sScratch, lit | other) && !SunStaticSet(),
                "control (c): the room-25 sun switch keeps its flag but its Destroy still clears the static");
    // (d) destroy order: a BURN head is skipped with the static kept, the next
    //     switch unsets its flag and clears the static, so the third keeps its.
    //     AddActor links at the head: the last added is destroyed first.
    ArmSunSwitch(scene, true, lit | (1u << kSunFlag2) | (1u << kSunFlag3));
    ODSE_ASSERT(AddSunSwitch(scene, kSunFlag2, kSunStayOn, kSunRoom, true) != NULL, "setup: third switch");
    ODSE_ASSERT(AddSunSwitch(scene, kSunFlag, kSunStayOn, kSunRoom, true) != NULL, "setup: second switch");
    ODSE_ASSERT(AddSunSwitch(scene, kSunFlag3, kSunBurn, kSunRoom, true) != NULL, "setup: head burn switch");
    ODSE_ASSERT(DepartF10(), "control (d): blob");
    ODSE_ASSERT(SunSceneFlagsAre(&sScratch, (1u << kSunFlag2) | (1u << kSunFlag3) | other) && !SunStaticSet(),
                "control (d): only the first non-BURN sun switch in destroy order unsets its flag");
    // (e) an actor OoT_Actor_Destroy would not call.
    ArmSunSwitch(scene, true, lit);
    ODSE_ASSERT(AddSunSwitch(scene, kSunFlag, kSunStayOn, kSunRoom, false) != NULL, "setup: sun switch");
    ODSE_ASSERT(DepartF10(), "control (e): blob");
    ODSE_ASSERT(SunSceneFlagsAre(&sScratch, lit | other) && SunStaticSet(),
                "control (e): a sun switch with no destroy function writes nothing");
    // (f) no PlayState: nothing is dereferenced or written; the blob carries
    //     whatever the save held (there is no flush either).
    ArmSunSwitch(scene, true, lit);
    ODSE_ASSERT(AddSunSwitch(scene, kSunFlag, kSunStayOn, kSunRoom, true) != NULL, "setup: sun switch");
    OoT_gPlayState = NULL;
    const bool departed = DepartF10();
    OoT_gPlayState = scene->play;
    ODSE_ASSERT(departed, "control (f): a freeze with no PlayState must still record a blob");
    ODSE_ASSERT(memcmp(sScratch.sceneFlags, sSceneBefore, sizeof(sSceneBefore)) == 0 && SunStaticSet() &&
                    (scene->play->actorCtx.flags.swch & lit),
                "control (f): with no PlayState nothing may be written");
    return 0;
}

// ============================================================================
// #807 (2): linkAge between an age-change write and its scene reload
// ============================================================================
//
// OoT_Player_Destroy writes gSaveContext.linkAge = play->linkAgeOnLoad, and
// Play_Destroy, just before it destroys the actors, swaps the age equipment
// when the two differ (Inventory_SwapAgeEquipment keys off the CURRENT age).
// They differ only between a linkAgeOnLoad write (the Temple of Time
// age-change cutscene, SoH's switch-age mod) and the reload it triggers.

const u8 kAdult = LINK_AGE_ADULT;
const u8 kChild = LINK_AGE_CHILD;

void SetBucket(ItemEquips* bucket, u8 sword, u16 swordValue) {
    memset(bucket, 0, sizeof(*bucket));
    memset(bucket->buttonItems, ITEM_NONE, sizeof(bucket->buttonItems));
    memset(bucket->cButtonSlots, SLOT_NONE, sizeof(bucket->cButtonSlots));
    bucket->buttonItems[0] = sword;
    bucket->equipment =
        (u16)((swordValue << (EQUIP_TYPE_SWORD * 4)) | (EQUIP_VALUE_TUNIC_KOKIRI << (EQUIP_TYPE_TUNIC * 4)) |
              (EQUIP_VALUE_BOOTS_KOKIRI << (EQUIP_TYPE_BOOTS * 4)));
}

ItemEquips sEquipsBefore[3];

// A vanilla Temple of Time visit in @p age with linkAgeOnLoad = @p onLoad. The
// live equips are @p age's; the child bucket holds the Kokiri Sword and the
// adult bucket the Biggoron Sword, so every swap direction is visible.
void ArmAge(Scene* scene, u8 age, u8 onLoad, bool player, bool playerHasDestroy) {
    ArmLiveOoTSession();
    ClearActors(scene);
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    scene->play->sceneNum = SCENE_TEMPLE_OF_TIME;
    gSaveContext.linkAge = age;
    scene->play->linkAgeOnLoad = onLoad;
    if (age == kAdult) {
        SetBucket(&gSaveContext.equips, ITEM_SWORD_MASTER, EQUIP_VALUE_SWORD_MASTER);
    } else {
        SetBucket(&gSaveContext.equips, ITEM_SWORD_KOKIRI, EQUIP_VALUE_SWORD_KOKIRI);
    }
    SetBucket(&gSaveContext.childEquips, ITEM_SWORD_KOKIRI, EQUIP_VALUE_SWORD_KOKIRI);
    SetBucket(&gSaveContext.adultEquips, ITEM_SWORD_BGS, EQUIP_VALUE_SWORD_BIGGORON);
    if (player) {
        (void)AddActor(scene, ACTOR_PLAYER, 0, ACTORCAT_PLAYER, playerHasDestroy);
    }
    Snapshot();
    sEquipsBefore[0] = gSaveContext.equips;
    sEquipsBefore[1] = gSaveContext.childEquips;
    sEquipsBefore[2] = gSaveContext.adultEquips;
}

int SwordValue(const ItemEquips* equips) {
    return (equips->equipment >> (EQUIP_TYPE_SWORD * 4)) & 0xF;
}

// The age and every equipment bucket are exactly as armed.
bool AgeUntouched(const SaveContext* save, u8 age) {
    if (save->linkAge != age || memcmp(&save->equips, &sEquipsBefore[0], sizeof(ItemEquips)) != 0 ||
        memcmp(&save->childEquips, &sEquipsBefore[1], sizeof(ItemEquips)) != 0 ||
        memcmp(&save->adultEquips, &sEquipsBefore[2], sizeof(ItemEquips)) != 0) {
        printf("[TEST] linkAge=%d (want %d) B=0x%02X sword=%d\n", (int)save->linkAge, (int)age,
               save->equips.buttonItems[0], SwordValue(&save->equips));
        return false;
    }
    return OnlyFlagMoved(save, -1, true);
}

// Adult -> child (the Master Sword put back): the frozen save is a child
// wearing the child bucket, with the adult's equips archived.
bool BecameChild(const SaveContext* save) {
    if (save->linkAge != kChild || save->equips.buttonItems[0] != ITEM_SWORD_KOKIRI ||
        SwordValue(&save->equips) != EQUIP_VALUE_SWORD_KOKIRI ||
        save->adultEquips.buttonItems[0] != ITEM_SWORD_MASTER ||
        SwordValue(&save->adultEquips) != EQUIP_VALUE_SWORD_MASTER) {
        printf("[TEST] linkAge=%d B=0x%02X sword=%d adultB=0x%02X adultSword=%d\n", (int)save->linkAge,
               save->equips.buttonItems[0], SwordValue(&save->equips), save->adultEquips.buttonItems[0],
               SwordValue(&save->adultEquips));
        return false;
    }
    return OnlyFlagMoved(save, -1, true);
}

int RunLinkAge(Scene* scene) {
    // ---- 1. F10 between the Master Sword cutscene and its reload ----------
    ArmAge(scene, kAdult, kChild, true, true);
    ODSE_ASSERT(gSaveContext.linkAge != scene->play->linkAgeOnLoad,
                "non-vacuity: linkAge and linkAgeOnLoad must differ before the departure");
    ODSE_ASSERT(DepartF10(), "the hot-swap driver must record and read back a fresh OoT blob");
    ODSE_ASSERT(BecameChild(&sScratch),
                "an F10 after the age-change write must freeze linkAge = linkAgeOnLoad (CHILD), as the Player "
                "Destroy leaves it, wearing the child equipment Play_Destroy's swap loads (#807)");
    ODSE_ASSERT(gSaveContext.linkAge == kChild, "the live gSaveContext must carry the new age too");

    // ---- 2. The door driver ---------------------------------------------
    ArmAge(scene, kAdult, kChild, true, true);
    ODSE_ASSERT(DepartDoor(), "the entrance driver must record and read back a fresh OoT blob");
    ODSE_ASSERT(BecameChild(&sScratch), "a cross-game door departure in the same window must freeze it child (#807)");

    // ---- 3. Child -> adult (the Master Sword drawn) -----------------------
    ArmAge(scene, kChild, kAdult, true, true);
    ODSE_ASSERT(DepartF10(), "leg 3: blob");
    ODSE_ASSERT(sScratch.linkAge == kAdult && sScratch.equips.buttonItems[0] == ITEM_SWORD_BGS &&
                    SwordValue(&sScratch.equips) == EQUIP_VALUE_SWORD_BIGGORON &&
                    sScratch.childEquips.buttonItems[0] == ITEM_SWORD_KOKIRI && OnlyFlagMoved(&sScratch, -1, true),
                "child -> adult: the frozen save must be adult wearing the adult bucket, the child's archived (#807)");

    // ---- 4. Controls ------------------------------------------------------
    // (a) no age change pending: nothing moves.
    ArmAge(scene, kAdult, kAdult, true, true);
    ODSE_ASSERT(DepartF10() && AgeUntouched(&sScratch, kAdult),
                "control (a): with linkAge == linkAgeOnLoad the age and every equipment bucket stay as they are");
    // (b) no Player actor live: no Player Destroy, so no age write either.
    ArmAge(scene, kAdult, kChild, false, true);
    ODSE_ASSERT(DepartF10() && AgeUntouched(&sScratch, kAdult), "control (b): with no Player live nothing is written");
    // (c) a Player with no destroy function.
    ArmAge(scene, kAdult, kChild, true, false);
    ODSE_ASSERT(DepartF10() && AgeUntouched(&sScratch, kAdult),
                "control (c): a Player OoT_Actor_Destroy would not call writes nothing");
    // (d) no PlayState.
    ArmAge(scene, kAdult, kChild, true, true);
    OoT_gPlayState = NULL;
    const bool departed = DepartF10();
    OoT_gPlayState = scene->play;
    ODSE_ASSERT(departed && AgeUntouched(&sScratch, kAdult), "control (d): with no PlayState nothing is written");
    return 0;
}

int RunRow(int (*body)(Scene*), const char* name) {
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    // The flag setters dispatch OnFlagSet / OnFlagUnset through
    // GameInteractor::Instance (the same lazy-init idiom production uses).
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const GameId prevGame = Context_GetCurrentGame();
    // Combo_CheckEntranceSwitch resolves the departing game from this tracker,
    // and the GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);

    int rc = 1;
    Scene scene = {};
    scene.play = (PlayState*)calloc(1, sizeof(PlayState));
    if (scene.play == NULL) {
        printf("[TEST] FAIL: could not allocate a PlayState (%s:%d)\n", __FILE__, __LINE__);
    } else if (!ResetEntranceTable()) {
        printf("[TEST] FAIL: the combo entrance table did not reset (%s:%d)\n", __FILE__, __LINE__);
    } else {
        OoT_gPlayState = scene.play;
        rc = body(&scene);
    }

    // Leave global state clean for whatever runs next in this process, on pass
    // or fail: the published PlayState must never outlive this row.
    OoT_gPlayState = NULL;
    if (scene.play != NULL) {
        ClearActors(&scene);
        free(scene.play);
    }
    memset(&gSaveContext, 0, sizeof(SaveContext));
    OoT_ObjLightswitch_SetDestroyStaticsForTest(0);
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    (void)ResetEntranceTable();
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] %s: PASS\n", name);
    }
    return rc;
}

} // namespace

extern "C" int OoT_DepartureWindmillFlag_RunHeadless(void) {
    return RunRow(RunWindmill, "oot-departure-windmill-flag");
}

extern "C" int OoT_DepartureLakeFlag_RunHeadless(void) {
    return RunRow(RunLake, "oot-departure-lake-flag");
}

extern "C" int OoT_DepartureSunSwitchFlag_RunHeadless(void) {
    return RunRow(RunSunSwitch, "oot-departure-sun-switch-flag");
}

extern "C" int OoT_DepartureLinkAge_RunHeadless(void) {
    return RunRow(RunLinkAge, "oot-departure-link-age");
}
