#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
}

#define CVAR_NAME "gEnhancements.Player.UnderwaterOcarina"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterUnderwaterOcarina() {
    COND_VB_SHOULD(VB_DISABLE_ITEM_UNDERWATER, CVAR, {
        // W6-MASKS-SONGS (#1236): this TU is in the single-exe link now and the body reads MM_gPlayState; a missing
        // play state leaves the vanilla behavior untouched.
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
        const auto item = va_arg(args, s32);
        if (item == ITEM_OCARINA_OF_TIME &&
            MM_Player_GetEnvironmentalHazard(MM_gPlayState) == PLAYER_ENV_HAZARD_UNDERWATER_FLOOR) {
            *should = false;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterUnderwaterOcarina, { CVAR_NAME });
