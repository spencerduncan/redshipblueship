#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "CustomItem/CustomItem.h"

extern "C" {
#include "variables.h"
#include "src/overlays/actors/ovl_En_Fish2/z_en_fish2.h"
void MM_Player_StartTalking(PlayState* play, Actor* actor);
}

/*
 * #1220: the check is FLAG_NONE and its pickup arms it on touch, and CheckQueue gives any eligible check, so a fish fed
 * again after a lab re-entry would give the check's item a second time. Offer it only while it is still open this
 * cycle. A repeat gives nothing: the vanilla En_Col_Man stays blocked too, because the rando never sets
 * WEEKEVENTREG_RECEIVED_MARINE_RESEARCH_LAB_FISH_HEART_PIECE and Col_Man would offer a vanilla heart piece.
 */
bool Rando::ActorBehavior::Fish2ShouldOfferCheck() {
    return !RANDO_SAVE_CHECKS[RC_GREAT_BAY_COAST_MARINE_LAB_FISH_PIECE_OF_HEART].cycleObtained;
}

void Rando::ActorBehavior::InitEnFish2Behavior() {
    COND_VB_SHOULD(VB_FISH2_SPAWN_HEART_PIECE, IS_RANDO, {
        EnFish2* refActor = va_arg(args, EnFish2*);
        if (refActor->actor.id != ACTOR_EN_FISH2) {
            return;
        }

        RandoCheckId randoCheckId =
            Rando::StaticData::Checks[RC_GREAT_BAY_COAST_MARINE_LAB_FISH_PIECE_OF_HEART].randoCheckId;

        if (Rando::ActorBehavior::Fish2ShouldOfferCheck()) {
            CustomItem::Spawn(
                221.624f, 100.0f, -182.550f, 0, CustomItem::KILL_ON_TOUCH, randoCheckId,
                [](Actor* actor, PlayState* play) {
                    RandoSaveCheck& randoSaveCheck = RANDO_SAVE_CHECKS[CUSTOM_ITEM_PARAM];
                    randoSaveCheck.eligible = true;
                },
                [](Actor* actor, PlayState* play) {
                    auto& randoSaveCheck = RANDO_SAVE_CHECKS[CUSTOM_ITEM_PARAM];
                    RandoItemId randoItemId = Rando::ConvertItem(randoSaveCheck.randoItemId);
                    MM_Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
                    Rando::DrawItem(Rando::ConvertItem(randoSaveCheck.randoItemId, (RandoCheckId)CUSTOM_ITEM_PARAM),
                                    actor);
                });
        }

        CLEAR_WEEKEVENTREG(WEEKEVENTREG_81_10);
        CLEAR_WEEKEVENTREG(WEEKEVENTREG_81_20);
        CLEAR_WEEKEVENTREG(WEEKEVENTREG_81_40);
        CLEAR_WEEKEVENTREG(WEEKEVENTREG_81_80);
        CLEAR_WEEKEVENTREG(WEEKEVENTREG_82_01);
        CLEAR_WEEKEVENTREG(WEEKEVENTREG_82_02);

        *should = false;
    });
}