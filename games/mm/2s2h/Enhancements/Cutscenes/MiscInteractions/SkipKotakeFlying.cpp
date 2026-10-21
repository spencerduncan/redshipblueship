#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
}

#define CVAR_NAME "gEnhancements.TimeSavers.SkipMiscInteractions"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterSkipKotakeFlying() {
    COND_VB_SHOULD(VB_START_CUTSCENE, CVAR, {
        // #1030-#1034, per PR #673's criterion: this TU is in the single-exe link now and the body reads MM_gPlayState;
        // a missing play state leaves the vanilla behavior untouched.
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
        s16* csId = va_arg(args, s16*);
        if (MM_gPlayState->sceneId == SCENE_20SICHITAI) { // poison
            if (*csId == 20 || *csId == 21) {
                *should = false;
            }
        }
        if (MM_gPlayState->sceneId == SCENE_20SICHITAI2) { // cleared
            if (*csId == 19 || *csId == 20) {
                *should = false;
            }
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipKotakeFlying, { CVAR_NAME });
