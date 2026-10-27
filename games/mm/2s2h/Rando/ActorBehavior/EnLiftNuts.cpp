#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "variables.h"
void MM_Player_StartTalking(PlayState* play, Actor* actor);
}

void Rando::ActorBehavior::InitEnLiftNutsBehavior() {
    COND_VB_SHOULD(VB_GIVE_ITEM_FROM_OFFER, IS_RANDO, {
        GetItemId* item = va_arg(args, GetItemId*);
        Actor* refActor = va_arg(args, Actor*);
        Player* player = GET_PLAYER(MM_gPlayState);

        if (refActor->id != ACTOR_EN_LIFT_NUTS) {
            return;
        }

        /*
         * The day-3 all-days heart piece stands in for RC_DEKU_PLAYGROUND_ALL_DAYS: the HasParent branch sets its flag
         * and OnFlagSet arms it. Any other win stands in for RC_DEKU_PLAYGROUND_ANY_DAY, a FLAG_NONE check this hook
         * arms itself, but only while it is still open this cycle: CheckQueue gives any eligible check, so a second
         * win would give its item again (#1220). A repeat win gets the vanilla Purple Rupee, as OoTMM does.
         */
        if (*item != GI_HEART_PIECE) {
            if (!((CURRENT_DAY == 3 && (!CHECK_WEEKEVENTREG(WEEKEVENTREG_WON_DEKU_PLAYGROUND_DAY_1) ||
                                        !CHECK_WEEKEVENTREG(WEEKEVENTREG_WON_DEKU_PLAYGROUND_DAY_2))) ||
                  (CURRENT_DAY == 2 || CURRENT_DAY == 1)) ||
                RANDO_SAVE_CHECKS[RC_DEKU_PLAYGROUND_ANY_DAY].cycleObtained) {
                return;
            }
            RANDO_SAVE_CHECKS[RC_DEKU_PLAYGROUND_ANY_DAY].eligible = true;
        }

        *should = false;
        refActor->parent = &player->actor;
        player->talkActor = refActor;
        player->talkActorDistance = refActor->xzDistToPlayer;
        player->exchangeItemAction = PLAYER_IA_MINUS1;
        MM_Player_StartTalking(MM_gPlayState, refActor);
    });
}