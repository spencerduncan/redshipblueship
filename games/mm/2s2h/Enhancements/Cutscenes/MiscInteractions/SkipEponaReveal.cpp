#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "overlays/actors/ovl_Bg_Umajump/z_bg_umajump.h"
}

#define CVAR_NAME "gEnhancements.TimeSavers.SkipMiscInteractions"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterSkipEponaReveal() {
    COND_VB_SHOULD(VB_QUEUE_CUTSCENE, CVAR, {
        // #1030-#1034, per PR #673's criterion: this TU is in the single-exe link now and the body reads MM_gPlayState;
        // a missing play state leaves the vanilla behavior untouched.
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
        s16* csId = va_arg(args, s16*);

        if (MM_gPlayState->sceneId != SCENE_F01) { // Romani Ranch
            return;
        }

        BgUmajump* bgUmajump = (BgUmajump*)MM_Actor_FindNearby(MM_gPlayState, &GET_PLAYER(MM_gPlayState)->actor,
                                                            ACTOR_BG_UMAJUMP, ACTORCAT_PROP, 99999.9f);
        if (!bgUmajump) {
            return;
        }

        if (*csId != 20) { // Epona Reveal
            return;
        }

        *should = false;
        SET_WEEKEVENTREG(WEEKEVENTREG_89_20);
        bgUmajump->dyna.actor.update = MM_Actor_Noop;
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipEponaReveal, { CVAR_NAME });
