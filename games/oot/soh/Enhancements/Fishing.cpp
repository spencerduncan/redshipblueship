#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "macros.h"
#include "variables.h"

extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
}

void RegisterAllowFishingBlankB() {
    bool shouldRegister = CVarGetInteger(CVAR_ENHANCEMENT("FishingBlankB"), 0) || IS_RANDO;

    COND_VB_SHOULD(VB_PUTAWAY_BECAUSE_DISABLED_ITEM_BUTTONS, shouldRegister, {
        if (OoT_gPlayState->sceneNum == SCENE_FISHING_POND && gSaveContext.equips.buttonItems[0] == ITEM_NONE) {
            *should = false;
        }
    });

    COND_VB_SHOULD(VB_OVERRIDE_BUTTON_ITEM_USED, shouldRegister, {
        int32_t* usedButton = va_arg(args, int32_t*);
        Player* player = va_arg(args, Player*);
        int32_t* item = va_arg(args, int32_t*);

        if (*usedButton == EQUIP_SLOT_B && OoT_gPlayState->sceneNum == SCENE_FISHING_POND && *item == ITEM_NONE &&
            player->heldItemAction == PLAYER_IA_FISHING_POLE) {
            *item = ITEM_FISHING_POLE;
        }
    });
}

static RegisterShipInitFunc initAllowFishingBlankB(RegisterAllowFishingBlankB,
                                                   { CVAR_ENHANCEMENT("FishingBlankB"), "IS_RANDO" });
