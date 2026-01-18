/*
 * File: z_en_m_fire1.c
 * Overlay: ovl_En_M_Fire1
 * Description: Deku Nut Hitbox
 */

#include "z_en_m_fire1.h"

#define FLAGS 0

void OoT_EnMFire1_Init(Actor* thisx, PlayState* play);
void OoT_EnMFire1_Destroy(Actor* thisx, PlayState* play);
void OoT_EnMFire1_Update(Actor* thisx, PlayState* play);

const ActorInit En_M_Fire1_InitVars = {
    ACTOR_EN_M_FIRE1,
    ACTORCAT_MISC,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(EnMFire1),
    (ActorFunc)OoT_EnMFire1_Init,
    (ActorFunc)OoT_EnMFire1_Destroy,
    (ActorFunc)OoT_EnMFire1_Update,
    NULL,
    NULL,
};

static ColliderCylinderInit OoT_sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000001, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NONE,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 200, 200, 0, { 0 } },
};

void OoT_EnMFire1_Init(Actor* thisx, PlayState* play) {
    EnMFire1* this = (EnMFire1*)thisx;
    s32 pad;

    if (this->actor.params < 0) {
        OoT_Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_ITEMACTION);
    }

    OoT_Collider_InitCylinder(play, &this->collider);
    OoT_Collider_SetCylinder(play, &this->collider, &this->actor, &OoT_sCylinderInit);
}

void OoT_EnMFire1_Destroy(Actor* thisx, PlayState* play) {
    EnMFire1* this = (EnMFire1*)thisx;

    OoT_Collider_DestroyCylinder(play, &this->collider);
}

void OoT_EnMFire1_Update(Actor* thisx, PlayState* play) {
    EnMFire1* this = (EnMFire1*)thisx;
    s32 pad;

    if (OoT_Math_StepToF(&this->timer, 1.0f, 0.2f)) {
        OoT_Actor_Kill(&this->actor);
    } else {
        OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
        OoT_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}
