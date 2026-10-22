#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_En_Dnp/z_en_dnp.h"
}

#define CVAR_NAME "gEnhancements.TimeSavers.SkipCutscene.Story"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterSkipPrincessDelivery() {
    COND_VB_SHOULD(VB_START_CUTSCENE, CVAR, {
        // #1018-#1022, per PR #673's criterion: this TU is in the single-exe link now and the body reads MM_gPlayState;
        // a missing play state leaves the vanilla verdict untouched.
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != NULL);
        s16* csId = va_arg(args, s16*);
        if (MM_gPlayState->sceneId == SCENE_DEKU_KING && *csId == 16 &&
            MM_gPlayState->transitionTrigger == TRANS_TRIGGER_OFF) {
            // Set the flag that would normally be set after the dialogue
            SET_WEEKEVENTREG(WEEKEVENTREG_23_20);

            // Set up transition to Deku Palace throne room
            MM_GameEvents_Queue().emplace_back(
                GIEventTransition{ .entrance = ENTRANCE(DEKU_KINGS_CHAMBER, 3),
                                   .cutsceneIndex = 0,
                                   .transitionTrigger = TRANS_TRIGGER_START,
                                   .transitionType = TRANS_TYPE_FADE_BLACK });
            *should = false;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipPrincessDelivery, { CVAR_NAME });