/*
 * File: z_en_hs2.c
 * Overlay: ovl_En_Hs2
 * Description: Near-empty actor. Does nothing, but can be targeted.
 */

#include "z_en_hs2.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY)

void MM_EnHs2_Init(Actor* thisx, PlayState* play);
void MM_EnHs2_Destroy(Actor* thisx, PlayState* play);
void MM_EnHs2_Update(Actor* thisx, PlayState* play);
void MM_EnHs2_Draw(Actor* thisx, PlayState* play);

void EnHs2_DoNothing(EnHs2* this, PlayState* play);

ActorProfile En_Hs2_Profile = {
    /**/ ACTOR_EN_HS2,
    /**/ ACTORCAT_NPC,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(EnHs2),
    /**/ MM_EnHs2_Init,
    /**/ MM_EnHs2_Destroy,
    /**/ MM_EnHs2_Update,
    /**/ MM_EnHs2_Draw,
};

void MM_EnHs2_Init(Actor* thisx, PlayState* play) {
    EnHs2* this = (EnHs2*)thisx;

    MM_Actor_SetScale(&this->actor, 1.0f);
    this->actionFunc = EnHs2_DoNothing;
}

void MM_EnHs2_Destroy(Actor* thisx, PlayState* play) {
}

void EnHs2_DoNothing(EnHs2* this, PlayState* play) {
}

void MM_EnHs2_Update(Actor* thisx, PlayState* play) {
    EnHs2* this = (EnHs2*)thisx;

    this->actionFunc(this, play);
}

void MM_EnHs2_Draw(Actor* thisx, PlayState* play) {
}
