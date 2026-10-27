#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "variables.h"
#include "src/overlays/actors/ovl_En_Fu/z_en_fu.h"
void MM_Player_StartTalking(PlayState* play, Actor* actor);
}

void Rando::ActorBehavior::InitEnAob01Behavior() {
    COND_VB_SHOULD(VB_GIVE_ITEM_FROM_OFFER, IS_RANDO, {
        GetItemId* item = va_arg(args, GetItemId*);
        Actor* refActor = va_arg(args, Actor*);
        Player* player = GET_PLAYER(MM_gPlayState);

        if (refActor->id != ACTOR_EN_AOB_01) {
            return;
        }

        /*
         * #1219: vanilla offers the heart piece while WEEKEVENTREG_RECEIVED_DOGGY_RACETRACK_HEART_PIECE is clear and a
         * Red Rupee once it is set, and sets it in the HasParent branch. That flag is the check's own flag
         * (StaticData/Checks.cpp), so on the first win of a cycle the flag set arms the check through OnFlagSet and
         * the hook gives nothing itself. Once the check is spent this cycle, a repeat win gets the vanilla Red Rupee,
         * as OoTMM does. An unshuffled check is never armed by OnFlagSet, so it keeps its vanilla heart piece.
         */
        RandoSaveCheck& randoSaveCheck = RANDO_SAVE_CHECKS[RC_DOGGY_RACETRACK_PIECE_OF_HEART];
        if (!randoSaveCheck.shuffled || randoSaveCheck.cycleObtained) {
            return;
        }
        if (CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_DOGGY_RACETRACK_HEART_PIECE)) {
            // The flag is already set, so the HasParent branch will not set it again: arm the open check here.
            randoSaveCheck.eligible = true;
        }

        *should = false;
        refActor->parent = &player->actor;
        player->talkActor = refActor;
        player->talkActorDistance = refActor->xzDistToPlayer;
        player->exchangeItemAction = PLAYER_IA_MINUS1;
        MM_Player_StartTalking(MM_gPlayState, refActor);
    });
}