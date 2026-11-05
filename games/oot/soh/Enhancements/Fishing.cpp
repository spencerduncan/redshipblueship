#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "macros.h"
#include "variables.h"

extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
}

// Vanilla bug: Not possible to fish with blank B because blank B item value 0xFF is saved
// as temp B = disabled B -> fishing pole is unequipped.
// Fix: If fishing, disregard disabled B and on B press set used item to fishing pole.
void RegisterAllowFishingBlankB() {
    COND_VB_SHOULD(VB_PUTAWAY_BECAUSE_DISABLED_ITEM_BUTTONS,
                   (IS_RANDO || CVarGetInteger(CVAR_ENHANCEMENT("FishingBlankB"), IS_RANDO)), {
                       if (OoT_gPlayState->interfaceCtx.unk_260 != 0 &&
                           gSaveContext.equips.buttonItems[0] == ITEM_FISHING_POLE) {
                           *should = false;
                       }
                   });

    COND_VB_SHOULD(
        VB_OVERRIDE_BUTTON_ITEM_USED, (IS_RANDO || CVarGetInteger(CVAR_ENHANCEMENT("FishingBlankB"), IS_RANDO)), {
            s32* i = va_arg(args, s32*);
            Player* player = va_arg(args, Player*);
            s32* item = va_arg(args, s32*);
            if (OoT_gPlayState->interfaceCtx.unk_260 != 0 && *i == 0 && player->itemAction == PLAYER_IA_FISHING_POLE) {
                *item = ITEM_FISHING_POLE;
            }
        });
}

static RegisterShipInitFunc initAllowFishingBlankB(RegisterAllowFishingBlankB,
                                                   { CVAR_ENHANCEMENT("FishingBlankB"), "IS_RANDO" });
