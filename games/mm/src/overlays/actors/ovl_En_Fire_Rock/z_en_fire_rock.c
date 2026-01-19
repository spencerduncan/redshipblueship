/*
 * File: z_en_fire_rock.c
 * Overlay: ovl_En_Fire_Rock
 * Description: [Empty]
 */

#include "z_en_fire_rock.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void MM_EnFireRock_Init(Actor* thisx, PlayState* play);
void MM_EnFireRock_Destroy(Actor* thisx, PlayState* play);
void MM_EnFireRock_Update(Actor* thisx, PlayState* play);
void MM_EnFireRock_Draw(Actor* thisx, PlayState* play);

ActorProfile En_Fire_Rock_Profile = {
    /**/ ACTOR_EN_FIRE_ROCK,
    /**/ ACTORCAT_ENEMY,
    /**/ FLAGS,
    /**/ OBJECT_EFC_STAR_FIELD,
    /**/ sizeof(EnFireRock),
    /**/ MM_EnFireRock_Init,
    /**/ MM_EnFireRock_Destroy,
    /**/ MM_EnFireRock_Update,
    /**/ MM_EnFireRock_Draw,
};

void MM_EnFireRock_Init(Actor* thisx, PlayState* play) {
}

void MM_EnFireRock_Destroy(Actor* thisx, PlayState* play) {
}

void MM_EnFireRock_Update(Actor* thisx, PlayState* play) {
}

void MM_EnFireRock_Draw(Actor* thisx, PlayState* play) {
}
