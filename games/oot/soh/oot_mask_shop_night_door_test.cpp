/**
 * @file oot_mask_shop_night_door_test.cpp
 * ROM-free, display-free lock for #948: in a paired world the night Market's
 * Happy Mask Shop door is a scene-exit door, so the OoT -> MM crossing works at
 * any hour. CTest label "redship", row OoTMaskShopNightDoor in
 * CMake/SingleExecutable.cmake, dispatch "oot-mask-shop-night-door" in
 * src/common/test_runner.cpp. The fix is games/oot/soh/Enhancements/
 * MaskShopNightDoor.cpp.
 *
 * WHAT IT DRIVES. The production order of a door's spawn (z_actor.c,
 * OoT_Actor_Init): the REAL ShouldActorInit dispatch
 * (GameInteractor_ShouldActorInit, ID hooks included), then the door's REAL
 * EnDoor_SetupType, which decides the type, arms the sign text and picks the
 * action. The hook is registered the way boot registers it: by running its
 * entry in ShipInit's "*" map, found by its registration site, so a registrar
 * the linker dropped fails the row instead of passing vacuously.
 *
 * THE DOORS are the real ones, read from oot.o2r's scene data: the spawned
 * params are (transition index << 10) + the scene's params.
 *   - night Mask Shop:   SCENE_MARKET_NIGHT actor 3, 0x0290 -> 0x0E90
 *                        (DOOR_CHECKABLE, sign text 0x0210)
 *   - night Potion Shop: SCENE_MARKET_NIGHT actor 1, 0x028E -> 0x068E
 *                        (DOOR_CHECKABLE, sign text 0x020E)
 *   - day Mask Shop:     SCENE_MARKET_DAY actor 3,   0x01BF -> 0x0DBF
 *                        (DOOR_SCENEEXIT)
 *
 * THE LEGS.
 *   (a) a paired file open: the night Mask Shop door comes out of setup as the
 *       day door, byte for byte (0x0DBF): DOOR_SCENEEXIT, no text id, the idle
 *       action, none of the talk flags.
 *   (b) no paired file: the sign. Three ways a session can fall short of
 *       Context_ComboSessionResident: nothing at all, a slot with no identity
 *       (a vanilla file), an identity with no slot (a staged generation).
 *   (c) a paired file open: the night Potion Shop door keeps its sign.
 *   (d) a paired file open: the day Mask Shop door is untouched.
 *   (e) a paired file open: a door with the same params in another scene keeps
 *       its sign (the rewrite is keyed on SCENE_MARKET_NIGHT).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>

#include "context.h"
#include "save.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include "src/overlays/actors/ovl_En_Door/z_en_door.h"
// games/oot/src/code/z_play.c: the hook reads the scene from it.
extern PlayState* OoT_gPlayState;
// z_en_door.c's two actions the legs tell apart (external linkage there).
void OoT_EnDoor_Idle(EnDoor* thisx, PlayState* play);
void EnDoor_WaitForCheck(EnDoor* thisx, PlayState* play);
}

namespace {

#define MSND_ASSERT(cond, msg)                                            \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

constexpr s16 kNightMaskShopDoor = 0x0E90;
constexpr s16 kNightPotionShopDoor = 0x068E;
constexpr s16 kDayMaskShopDoor = 0x0DBF;
constexpr u16 kMaskShopSignTextId = 0x0210;
constexpr u16 kPotionShopSignTextId = 0x020E;
constexpr u32 kTalkFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_LOCK_ON_DISABLED;

constexpr int kSlot = 0;
constexpr uint32_t kSeed = 0x00948948u;
constexpr uint32_t kSettingsHash = 0x5EED0948u;

// EnDoor carries SoH's randomizerInf; static so every leg starts zeroed.
EnDoor sDoor;

struct DoorOutcome {
    bool initAllowed;
    u16 params;
    u16 textId;
    EnDoorActionFunc actionFunc;
    u32 flags;
};

int DoorType(u16 params) {
    return (params >> 7) & 7;
}

// One door's spawn in @p sceneNum, in production order.
DoorOutcome SpawnDoor(PlayState* play, s16 sceneNum, s16 params) {
    memset(&sDoor, 0, sizeof(sDoor));
    sDoor.actor.id = ACTOR_EN_DOOR;
    sDoor.actor.params = params;
    sDoor.requiredObjBankIndex = 0; // bank 0 is marked loaded by the row
    play->sceneNum = sceneNum;

    DoorOutcome out = {};
    out.initAllowed = GameInteractor_ShouldActorInit(&sDoor.actor);
    EnDoor_SetupType(&sDoor, play);
    out.params = (u16)sDoor.actor.params;
    out.textId = sDoor.actor.textId;
    out.actionFunc = sDoor.actionFunc;
    out.flags = sDoor.actor.flags;
    return out;
}

void CloseSession(void) {
    ComboContext_Init();
    RsbsSave_SetActiveSlot(-1);
}

// A paired file is open: the identity is stamped and the slot is active
// (the two facts Context_ComboSessionResident reads).
void OpenPairedFile(void) {
    CloseSession();
    gComboCtx.sourceIsRando = 1;
    gComboCtx.sharedRandoSeed = kSeed;
    gComboCtx.sharedRandoSettingsHash = kSettingsHash;
    RsbsSave_SetActiveSlot(kSlot);
}

// Runs the hook's own boot registrar out of ShipInit's "*" map. Exactly one
// entry must come from MaskShopNightDoor.cpp.
bool RunBootRegistrar(void) {
    int found = 0;
    for (const auto& entry : ShipInit::GetAll()["*"]) {
        const std::string file = entry.registeredAt.file_name();
        const std::string want = "MaskShopNightDoor.cpp";
        if (file.size() >= want.size() && file.compare(file.size() - want.size(), want.size(), want) == 0) {
            entry.initFunc();
            found++;
        }
    }
    printf("[TEST] MaskShopNightDoor.cpp boot registrars found: %d\n", found);
    return found == 1;
}

int CheckSign(const DoorOutcome& door, s16 params, u16 textId, const char* leg) {
    if (!door.initAllowed || door.params != (u16)params || DoorType(door.params) != DOOR_CHECKABLE ||
        door.textId != textId || door.actionFunc != EnDoor_WaitForCheck || (door.flags & kTalkFlags) != kTalkFlags) {
        printf("[TEST] %s: params 0x%04X type %d text 0x%04X wait-for-check %d talk flags 0x%08X\n", leg, door.params,
               DoorType(door.params), door.textId, door.actionFunc == EnDoor_WaitForCheck ? 1 : 0,
               door.flags & kTalkFlags);
        return 1;
    }
    return 0;
}

int RunLegs(PlayState* play) {
    MSND_ASSERT(RunBootRegistrar(), "the hook's boot registrar is not in ShipInit's map exactly once");

    // (a) a paired file open: the night door is the day door.
    OpenPairedFile();
    MSND_ASSERT(Context_ComboSessionResident(), "setup: the paired file does not read as resident");
    DoorOutcome a = SpawnDoor(play, SCENE_MARKET_NIGHT, kNightMaskShopDoor);
    printf("[TEST] (a) paired night Mask Shop: params 0x%04X type %d text 0x%04X idle %d talk flags 0x%08X\n", a.params,
           DoorType(a.params), a.textId, a.actionFunc == OoT_EnDoor_Idle ? 1 : 0, a.flags & kTalkFlags);
    MSND_ASSERT(a.initAllowed, "(a): the hook refused the door's init");
    MSND_ASSERT(DoorType(a.params) == DOOR_SCENEEXIT, "(a): the night Mask Shop door is not a scene-exit door");
    MSND_ASSERT(a.params == (u16)kDayMaskShopDoor, "(a): the night door's params are not the day door's");
    MSND_ASSERT((a.params >> 10) == 3, "(a): the transition index moved");
    MSND_ASSERT(a.textId == 0, "(a): a sign text id is armed");
    MSND_ASSERT(a.actionFunc == OoT_EnDoor_Idle, "(a): the door does not idle like an openable door");
    MSND_ASSERT((a.flags & kTalkFlags) == 0, "(a): the door is still something to talk to");

    // (b) no paired file: the sign, three ways.
    CloseSession();
    MSND_ASSERT(CheckSign(SpawnDoor(play, SCENE_MARKET_NIGHT, kNightMaskShopDoor), kNightMaskShopDoor,
                          kMaskShopSignTextId, "(b1) no session") == 0,
                "(b1): with no session the night Mask Shop door is not the sign");
    CloseSession();
    RsbsSave_SetActiveSlot(kSlot);
    MSND_ASSERT(!Context_ComboSessionResident(), "setup (b2): a slot with no identity reads as resident");
    MSND_ASSERT(CheckSign(SpawnDoor(play, SCENE_MARKET_NIGHT, kNightMaskShopDoor), kNightMaskShopDoor,
                          kMaskShopSignTextId, "(b2) vanilla file") == 0,
                "(b2): on a vanilla file the night Mask Shop door is not the sign");
    OpenPairedFile();
    RsbsSave_SetActiveSlot(-1);
    MSND_ASSERT(!Context_ComboSessionResident(), "setup (b3): an identity with no slot reads as resident");
    MSND_ASSERT(CheckSign(SpawnDoor(play, SCENE_MARKET_NIGHT, kNightMaskShopDoor), kNightMaskShopDoor,
                          kMaskShopSignTextId, "(b3) staged generation") == 0,
                "(b3): with a staged generation the night Mask Shop door is not the sign");

    // (c) a paired file open: the other night shops stay closed.
    OpenPairedFile();
    MSND_ASSERT(CheckSign(SpawnDoor(play, SCENE_MARKET_NIGHT, kNightPotionShopDoor), kNightPotionShopDoor,
                          kPotionShopSignTextId, "(c) paired night Potion Shop") == 0,
                "(c): the night Potion Shop door lost its sign in a paired world");

    // (d) a paired file open: the day door is untouched.
    DoorOutcome d = SpawnDoor(play, SCENE_MARKET_DAY, kDayMaskShopDoor);
    MSND_ASSERT(d.initAllowed && d.params == (u16)kDayMaskShopDoor && d.textId == 0 && d.actionFunc == OoT_EnDoor_Idle,
                "(d): the day Mask Shop door changed in a paired world");

    // (e) a paired file open: the same params in another night scene keep the sign.
    MSND_ASSERT(CheckSign(SpawnDoor(play, SCENE_BACK_ALLEY_NIGHT, kNightMaskShopDoor), kNightMaskShopDoor,
                          kMaskShopSignTextId, "(e) paired other scene") == 0,
                "(e): a sign door outside SCENE_MARKET_NIGHT was opened");
    return 0;
}

} // namespace

extern "C" int OoT_MaskShopNightDoor_RunHeadless(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const GameId prevGame = Context_GetCurrentGame();
    const int prevSlot = RsbsSave_GetActiveSlot();
    PlayState* const prevPlay = OoT_gPlayState;
    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);

    int rc = 1;
    PlayState* play = (PlayState*)calloc(1, sizeof(PlayState));
    if (play == NULL) {
        printf("[TEST] FAIL: could not allocate a PlayState (%s:%d)\n", __FILE__, __LINE__);
    } else {
        // OoT_Object_IsLoaded reads status[bank].id > 0; the door's object is
        // gameplay_keep, which is always loaded in play.
        play->objectCtx.status[0].id = OBJECT_GAMEPLAY_KEEP;
        OoT_gPlayState = play;
        rc = RunLegs(play);
    }

    // Leave global state as it was, on pass or fail.
    OoT_gPlayState = prevPlay;
    free(play);
    ComboContext_Init();
    RsbsSave_SetActiveSlot(prevSlot);
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] oot-mask-shop-night-door: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
