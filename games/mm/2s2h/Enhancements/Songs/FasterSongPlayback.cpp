#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
extern u8 MM_sPlaybackState;
#include "overlays/actors/ovl_En_Torch2/z_en_torch2.h"
}

#define CVAR_NAME "gEnhancements.Songs.FasterSongPlayback"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

#define NOT_OCARINA_ACTION_BALAD_WIND_FISH                                       \
    (MM_gPlayState->msgCtx.ocarinaAction < OCARINA_ACTION_PROMPT_WIND_FISH_HUMAN || \
     MM_gPlayState->msgCtx.ocarinaAction > OCARINA_ACTION_PROMPT_WIND_FISH_DEKU)

void RegisterFasterSongPlayback() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, CVAR, [](Actor* actor) {
        if (MM_gPlayState->msgCtx.msgMode >= MSGMODE_SONG_PLAYED && MM_gPlayState->msgCtx.msgMode <= MSGMODE_17 &&
            !MM_gPlayState->csCtx.state && NOT_OCARINA_ACTION_BALAD_WIND_FISH) {
            if (MM_gPlayState->msgCtx.stateTimer > 1) {
                MM_gPlayState->msgCtx.stateTimer = 1;
            }
            MM_gPlayState->msgCtx.ocarinaSongEffectActive = 0;
            MM_sPlaybackState = 0;
        }
    });

    COND_ID_HOOK(ShouldActorInit, ACTOR_EFF_CHANGE, CVAR, [](Actor* actor, bool* should) {
        *should = false;
        Player* player = GET_PLAYER(MM_gPlayState);

        if (player->av2.actionVar2 < 10) {
            player->av2.actionVar2 = 10;
        } else if (player->av2.actionVar2 < 90) {
            EnTorch2* torch2 = MM_gPlayState->actorCtx.elegyShells[player->transformation];
            if (torch2 != NULL) {
                torch2->framesUntilNextState = 1;
            }

            player->av2.actionVar2 = 90;
        }
    });

    COND_VB_SHOULD(VB_ELEGY_STATUE_FADE_IN_OUT, CVAR, {
        EnTorch2* torch2 = va_arg(args, EnTorch2*);
        u16* targetAlpha = va_arg(args, u16*);

        MM_Math_StepToS(&torch2->alpha, *targetAlpha, 24);
    });
}

static RegisterShipInitFunc initFunc(RegisterFasterSongPlayback, { CVAR_NAME });
