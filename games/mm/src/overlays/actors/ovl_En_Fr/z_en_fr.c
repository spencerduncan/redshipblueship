/*
 * File: z_en_fr.c
 * Overlay: ovl_En_Fr
 * Description: Invisible spot that triggers a slow camera drift towards the spot while player is moving.
 *              Unused in game.
 */

#include "z_en_fr.h"

#define FLAGS (ACTOR_FLAG_CAMERA_DRIFT_ENABLED)

void MM_EnFr_Init(Actor* thisx, PlayState* play);
void MM_EnFr_Destroy(Actor* thisx, PlayState* play);
void MM_EnFr_Update(Actor* thisx, PlayState* play);

ActorProfile En_Fr_Profile = {
    /**/ ACTOR_EN_FR,
    /**/ ACTORCAT_ITEMACTION,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(EnFr),
    /**/ MM_EnFr_Init,
    /**/ MM_EnFr_Destroy,
    /**/ MM_EnFr_Update,
    /**/ NULL,
};

void MM_EnFr_Init(Actor* thisx, PlayState* play) {
    EnFr* this = (EnFr*)thisx;

    if (MM_Flags_GetSwitch(play, ENFR_GET_SWITCH_FLAG(&this->actor))) {
        MM_Actor_Kill(&this->actor);
        return;
    }

    this->actor.attentionRangeType = ENFR_GET_ATTENTION_RANGE_TYPE(&this->actor);
}

void MM_EnFr_Destroy(Actor* thisx, PlayState* play) {
}

void MM_EnFr_Update(Actor* thisx, PlayState* play) {
    EnFr* this = (EnFr*)thisx;

    if (MM_Flags_GetSwitch(play, ENFR_GET_SWITCH_FLAG(&this->actor))) {
        MM_Actor_Kill(&this->actor);
        return;
    }

    if (this->actor.xyzDistToPlayerSq < SQ(IREG(29))) {
        this->actor.flags &= ~ACTOR_FLAG_CAMERA_DRIFT_ENABLED;
    } else {
        this->actor.flags |= ACTOR_FLAG_CAMERA_DRIFT_ENABLED;
    }
}
