/**
 * MaskShopNightDoor.cpp: in a paired world the Happy Mask Shop door opens at
 * night too (#948).
 *
 * The door is the OoT -> MM crossing: walking through it takes entrance 0x0530
 * (src/common/entrance.h, OOT_ENTR_HAPPY_MASK_SHOP), and nothing else does. At
 * night the Market is its own scene, SCENE_MARKET_NIGHT, and that scene places
 * the door (transition actor 3, at (320, 0, -579)) as DOOR_CHECKABLE with sign
 * text 0x0210, "Happy Mask Shop / Now hiring part-time / Apply during the day".
 * EnDoor_SetupType parks such a door in EnDoor_WaitForCheck, so it never
 * opens and the crossing waited for morning. SCENE_MARKET_DAY places a
 * DOOR_SCENEEXIT door in the same slot (raw params 0x01BF), and both scenes put
 * the same exit polygon behind it: exit index 10 of an identical exit list,
 * which is 0x0530. A scene-exit door there is therefore all the crossing needs.
 *
 * THE FIX. Before the door's init runs (ShouldActorInit fires ahead of
 * actor->init on both of z_actor.c's init paths), a paired world's night door
 * gets the day door's params: the type becomes DOOR_SCENEEXIT and the low bits
 * 0x3F, and the transition index and double-door bit are kept. The result is
 * byte-identical to the day door (0x0DBF), so EnDoor_SetupType never arms the
 * sign text, and Lock Overworld Doors keys it as the day door (see the
 * RSBS (#948) row in randomizer/LockOverworldDoors.cpp). OoTMM's "Open Mask
 * Shop at night" does the same rewrite at spawn; here it is always on in a
 * paired world, because the door doubles as the only OoT -> MM portal.
 *
 * SCOPE. Only that one door (checkable, sign text 0x10) in SCENE_MARKET_NIGHT,
 * and only while Context_ComboSessionResident() says a paired file is open. A
 * vanilla file still reads the sign, the other night shops stay closed, and the
 * daytime door is untouched. OoT's logic still asks for daytime
 * (location_access/overworld/market.cpp), which only makes it conservative.
 *
 * Locked by the redship row OoTMaskShopNightDoor
 * (games/oot/soh/oot_mask_shop_night_door_test.cpp).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"

#include "context.h" // src/common: Context_ComboSessionResident

extern "C" {
extern PlayState* OoT_gPlayState;
#include "src/overlays/actors/ovl_En_Door/z_en_door.h"
}

namespace {

// The night door's low six bits: its sign text id minus 0x0200 (z_en_door.h).
constexpr u16 kMaskShopSignText = 0x10;
// The day door's low six bits in the same slot (SCENE_MARKET_DAY, 0x01BF).
constexpr u16 kDayDoorLowBits = 0x3F;
// What the rewrite keeps: the transition index (set by the actor engine at
// spawn) and the double-door bit.
constexpr u16 kKeptBits = 0xFC40;

void OnEnDoorShouldInit(void* actorRef, bool* should) {
    (void)should;
    EnDoor* door = static_cast<EnDoor*>(actorRef);
    const u16 params = (u16)door->actor.params;
    if (OoT_gPlayState == nullptr || OoT_gPlayState->sceneNum != SCENE_MARKET_NIGHT) {
        return;
    }
    if (((params >> 7) & 7) != DOOR_CHECKABLE || (params & 0x3F) != kMaskShopSignText) {
        return;
    }
    if (!Context_ComboSessionResident()) {
        return;
    }
    door->actor.params = (s16)((params & kKeptBits) | (DOOR_SCENEEXIT << 7) | kDayDoorLowBits);
}

} // namespace

// Always registered: whether a paired file is open changes at runtime with no
// CVar path to re-arm on, so the hook asks at each door's init instead.
static void RegisterMaskShopNightDoor() {
    COND_ID_HOOK(ShouldActorInit, ACTOR_EN_DOOR, true, OnEnDoorShouldInit);
}

static RegisterShipInitFunc initFunc(RegisterMaskShopNightDoor);

#endif // RSBS_SINGLE_EXECUTABLE
