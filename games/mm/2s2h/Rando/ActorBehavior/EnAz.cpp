#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "variables.h"
void MM_Player_StartTalking(PlayState* play, Actor* actor);
}

bool Rando::ActorBehavior::EnAzOfferStandsInForCheck(GetItemId getItemId) {
    if (getItemId > GI_RUPEE_PURPLE) { // Beaver Bro non-repeat reward
        return true;
    }
    /*
     * #1216: race 1's reward is picked at text 0x10F1 right after it sets WEEKEVENTREG_93_01, which is
     * RC_WATERFALL_RAPIDS_BEAVER_RACE_01's own flag (StaticData/Checks.cpp) and is cleared every cycle, so OnFlagSet
     * arms that check on every race-1 win. The persistent WEEKEVENTREG_RECEIVED_BEAVER_RACE_BOTTLE turns the bottle
     * into a Red Rupee from the second cycle on: that Red Rupee stands in for the check too. GI_RUPEE_RED is offered
     * by no other branch. OnFlagSet arms only a shuffled check, so an unshuffled one keeps its rupee.
     */
    return getItemId == GI_RUPEE_RED && RANDO_SAVE_CHECKS[RC_WATERFALL_RAPIDS_BEAVER_RACE_01].shuffled;
}

void Rando::ActorBehavior::InitEnAzBehavior() {
    COND_VB_SHOULD(VB_GIVE_ITEM_FROM_OFFER, IS_RANDO, {
        GetItemId* getItemId = va_arg(args, GetItemId*);
        Actor* actor = va_arg(args, Actor*);
        if (actor->id == ACTOR_EN_AZ && Rando::ActorBehavior::EnAzOfferStandsInForCheck(*getItemId)) {
            Player* player = GET_PLAYER(MM_gPlayState);
            *should = false;
            // Force advance textbox to the line vanilla shows after this reward (func_80A973B4): the one after the
            // offer's for the bottle and the heart piece, 0x1109 for race 1's Red Rupee
            actor->textId = (*getItemId == GI_RUPEE_RED) ? 0x1109 : actor->textId + 1;
            MM_Message_StartTextbox(MM_gPlayState, actor->textId, actor);
            actor->parent = &player->actor;
            player->talkActor = actor;
            player->talkActorDistance = actor->xzDistToPlayer;
            player->exchangeItemAction = PLAYER_IA_MINUS1;
            MM_Player_StartTalking(MM_gPlayState, actor);
        }
    });
}
