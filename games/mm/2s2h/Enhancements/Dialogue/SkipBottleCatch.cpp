#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/BenGui/Notification.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
}

#define CVAR_NAME "gEnhancements.FastBottles"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterSkipBottleCatchCutscene() {
    COND_VB_SHOULD(VB_PLAY_BOTTLE_CATCH_TEXT, CVAR, {
        if (*should) {
            if (!MM_gPlayState) {
                return;
            }

            Player* player = va_arg(args, Player*);
            Actor* interactRangeActor = va_arg(args, Actor*);
            ItemId itemId = (ItemId)va_arg(args, int);
            PlayerItemAction itemAction = (PlayerItemAction)va_arg(args, int);

            interactRangeActor->parent = &player->actor;
            MM_Player_UpdateBottleHeld(MM_gPlayState, player, itemId, itemAction);
            MM_Audio_PlayFanfare(NA_BGM_GET_ITEM);

            if (itemAction == PLAYER_IA_BOTTLE_DEKU_PRINCESS) {
                // Kill Deku Princess actor immediately upon capture
                MM_Actor_Kill(interactRangeActor);
            }

            Notification::MM_Notify_Emit({
                .itemIcon = (const char*)MM_gItemIcons[itemId],
                .message = "Caught!",
            });

            *should = false;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipBottleCatchCutscene, { CVAR_NAME });
