/*
 * File: z_en_m_fire1.c
 * Overlay: ovl_En_M_Fire1
 * Description: Deku Nut Effect
 */

#include "z_en_m_fire1.h"

#define FLAGS 0x00000000

void MM_EnMFire1_Init(Actor* thisx, PlayState* play);
void MM_EnMFire1_Destroy(Actor* thisx, PlayState* play);
void MM_EnMFire1_Update(Actor* thisx, PlayState* play);

ActorProfile En_M_Fire1_Profile = {
    /**/ ACTOR_EN_M_FIRE1,
    /**/ ACTORCAT_MISC,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(EnMFire1),
    /**/ MM_EnMFire1_Init,
    /**/ MM_EnMFire1_Destroy,
    /**/ MM_EnMFire1_Update,
    /**/ NULL,
};

static ColliderCylinderInit MM_sCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK2,
        { 0x00000001, 0x00, 0x01 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NONE,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { 100, 100, 0, { 0, 0, 0 } },
};

void MM_EnMFire1_Init(Actor* thisx, PlayState* play) {
    EnMFire1* this = (EnMFire1*)thisx;
    s32 pad;

    MM_Collider_InitCylinder(play, &this->collider);
    MM_Collider_SetCylinder(play, &this->collider, &this->actor, &MM_sCylinderInit);
    if (this->actor.params != 0) {
        this->collider.elem.atDmgInfo.dmgFlags = 0x40000;
    }
}

void MM_EnMFire1_Destroy(Actor* thisx, PlayState* play) {
    EnMFire1* this = (EnMFire1*)thisx;

    MM_Collider_DestroyCylinder(play, &this->collider);
}

void MM_EnMFire1_Update(Actor* thisx, PlayState* play) {
    EnMFire1* this = (EnMFire1*)thisx;
    s32 pad;

    if (MM_Math_StepToF(&this->timer, 1.0f, 0.2f)) {
        MM_Actor_Kill(&this->actor);
        return;
    }

    MM_Collider_UpdateCylinder(&this->actor, &this->collider);
    MM_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
}
