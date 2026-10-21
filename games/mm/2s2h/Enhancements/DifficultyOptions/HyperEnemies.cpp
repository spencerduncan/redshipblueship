#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
}

#define CVAR_NAME "gEnhancements.HyperEnemies"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterHyperEnemies() {
    COND_HOOK(OnActorUpdate, CVAR, [](Actor* actor) {
        // #1017: this TU is in the single-exe link now (2ship_enh_registrars); GET_PLAYER and the frame-advance
        // toggle below dereference the play state, and OnActorUpdate is pumped for every MM actor update.
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL && GET_PLAYER(MM_gPlayState) != NULL);
        if (actor->category != ACTORCAT_ENEMY || actor->id == ACTOR_EN_DG) {
            return;
        }

        if (MM_Player_InBlockingCsMode(MM_gPlayState, GET_PLAYER(MM_gPlayState))) {
            return;
        }

        if (actor->update != NULL) {
            // Everywhere we need to disable the collision checks already checks if frameAdvance is enabled, so we abuse
            // that temporarily. This doesn't have any _known_ side effects :)
            bool prevFrameAdv = MM_gPlayState->frameAdvCtx.enabled;
            MM_gPlayState->frameAdvCtx.enabled = true;
            actor->update(actor, MM_gPlayState);
            MM_gPlayState->frameAdvCtx.enabled = prevFrameAdv;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterHyperEnemies, { CVAR_NAME });
