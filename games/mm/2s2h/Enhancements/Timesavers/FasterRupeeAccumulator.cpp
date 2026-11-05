#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
}

#define CVAR_NAME "gEnhancements.FasterRupeeAccumulator"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void MM_RegisterFasterRupeeAccumulator() {
    COND_HOOK(OnGameStateUpdate, CVAR, []() {
        if (!MM_gPlayState) {
            return;
        }

        if (gSaveContext.rupeeAccumulator == 0) {
            return;
        }

        s16 capacity = (s16)CUR_CAPACITY(UPG_WALLET);
        s16 step = capacity / 50;

        // Gaining rupees - add extra per frame on top of vanilla's 1/frame
        if (gSaveContext.rupeeAccumulator > 0) {
            s16 amount = MIN(step, MIN(gSaveContext.rupeeAccumulator,
                                       (s16)(capacity - gSaveContext.save.saveInfo.playerData.rupees)));
            if (amount > 0) {
                gSaveContext.rupeeAccumulator -= amount;
                gSaveContext.save.saveInfo.playerData.rupees += amount;
            }
            // Losing rupees - add extra per frame on top of vanilla's handling
        } else {
            s16 amount =
                MIN(step, MIN((s16)(-gSaveContext.rupeeAccumulator), gSaveContext.save.saveInfo.playerData.rupees));
            if (amount > 0) {
                gSaveContext.rupeeAccumulator += amount;
                gSaveContext.save.saveInfo.playerData.rupees -= amount;
            }
        }
    });
}

static RegisterShipInitFunc initFunc(MM_RegisterFasterRupeeAccumulator, { CVAR_NAME });
