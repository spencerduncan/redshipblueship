#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "src/overlays/actors/ovl_En_Go2/z_en_go2.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"

extern PlayState* OoT_gPlayState;
}

void RegisterAlwaysOnFixes() {
    // This prevents a crash when using items that change the camera (such as din's fire), voiding out or dying on
    // scenes with prerendered backgrounds.
    COND_VB_SHOULD(VB_SHOULD_LOAD_BG_IMAGE, true, {
        s16* camId = va_arg(args, s16*);
        if (*camId == -1) {
            *should = false;
        }
    });

    // Back Face Culling explicitly sets data on segment 0x0C, we then aren't consistently restoring it when other code
    // sets values directly on the segment table
    COND_VB_SHOULD(VB_ITEMSHIELD_DRAW, true, {
        OPEN_DISPS(OoT_gPlayState->state.gfxCtx);

        gSPSegment(POLY_OPA_DISP++, 0x0C, SEGMENTED_TO_VIRTUAL(OoT_gCullBackDList));

        CLOSE_DISPS(OoT_gPlayState->state.gfxCtx);
    });

    // Prevents rolling while starting up the hookshot, holding the actor without using it
    // Softlocks context: https://github.com/HarbourMasters/Shipwright/pull/5578
    COND_VB_SHOULD(VB_INIT_HOOKSHOT_IA, true, {
        Player* player = va_arg(args, Player*);
        if (player->heldActor == NULL) {
            OoT_Player_UseItem(OoT_gPlayState, player, 0xFF);
        }
    });

    // Prevents the player from sliding everywhere if holding an actor that isn't the hookshot while moving & holding
    // out the hookshot
    // Softlocks context: https://github.com/HarbourMasters/Shipwright/pull/5578
    COND_VB_SHOULD(VB_PREVENT_HOOKSHOT_PARENT_SOFTLOCK, true, {
        s16* parentId = va_arg(args, s16*);
        if (*parentId != ACTOR_ARMS_HOOK) {
            *should = false;
        }
    });

    // Prevents softlocking when Goron Link starts rolling just as the player talks to him, keeping him out of range for
    // conversation
    COND_VB_SHOULD(VB_PREVENT_GORON_LINK_SOFTLOCK, true, {
        EnGo2* GoronLink = va_arg(args, EnGo2*);
        if (GoronLink->interactInfo.talkState == NPC_TALK_STATE_TALKING) {
            *should = true;
        }
    });
}

static RegisterShipInitFunc initAlwaysOnFixes(RegisterAlwaysOnFixes, { "" });
