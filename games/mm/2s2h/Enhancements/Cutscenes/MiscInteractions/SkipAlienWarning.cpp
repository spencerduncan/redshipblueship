#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "functions.h"
#include "variables.h"
}

#define CVAR_NAME "gEnhancements.TimeSavers.SkipMiscInteractions"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterSkipAlienWarning() {
    COND_ID_HOOK(ShouldActorInit, ACTOR_EN_MA4, CVAR, [](Actor* actor, bool* should) {
        // #1030-#1034, per PR #673's criterion: this TU is in the single-exe link now and the body reads MM_gPlayState;
        // a missing play state leaves the vanilla behavior untouched.
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
        if (MM_gPlayState->sceneId != SCENE_F01) { // Romani Ranch
            return;
        }

        SET_WEEKEVENTREG(WEEKEVENTREG_21_40);
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipAlienWarning, { CVAR_NAME });
