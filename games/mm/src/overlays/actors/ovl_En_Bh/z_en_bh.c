/*
 * File: z_en_bh.c
 * Overlay: ovl_En_Bh
 * Description: Brown Bird (on the Moon)
 */

#include "z_en_bh.h"

#define FLAGS 0x00000000

void EnBh_Init(Actor* thisx, PlayState* play);
void EnBh_Destroy(Actor* thisx, PlayState* play);
void EnBh_Update(Actor* thisx, PlayState* play);
void EnBh_Draw(Actor* thisx, PlayState* play);

void func_80C22DEC(EnBh* this, PlayState* play);

ActorProfile En_Bh_Profile = {
    /**/ ACTOR_EN_BH,
    /**/ ACTORCAT_ITEMACTION,
    /**/ FLAGS,
    /**/ OBJECT_BH,
    /**/ sizeof(EnBh),
    /**/ EnBh_Init,
    /**/ EnBh_Destroy,
    /**/ EnBh_Update,
    /**/ EnBh_Draw,
};

void EnBh_Init(Actor* thisx, PlayState* play) {
    EnBh* this = (EnBh*)thisx;

    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    MM_Actor_SetScale(&this->actor, 0.01f);
    MM_SkelAnime_InitFlex(play, &this->skelAnime, &gBhSkel, &gBhFlyingAnim, this->jointTable, this->morphTable,
                       OBJECT_BH_LIMB_MAX);
    MM_Animation_PlayLoop(&this->skelAnime, &gBhFlyingAnim);
    this->actionFunc = func_80C22DEC;
}

void EnBh_Destroy(Actor* thisx, PlayState* play) {
}

void func_80C22DEC(EnBh* this, PlayState* play) {
    f32 xDiff;
    f32 yDiff;
    f32 zDiff;
    f32 xzDist;
    s16 xRot;
    s16 yRot;
    s16 zRot;

    this->actor.speed = 3.0f;
    xDiff = this->pos.x - this->actor.world.pos.x;
    yDiff = this->pos.y - this->actor.world.pos.y;
    zDiff = this->pos.z - this->actor.world.pos.z;
    xzDist = sqrtf(SQ(xDiff) + SQ(zDiff));

    if ((this->timer2 == 0) || (xzDist < 100.0f)) {
        this->pos.x = MM_Rand_CenteredFloat(300.0f) + this->actor.home.pos.x;
        this->pos.y = MM_Rand_CenteredFloat(100.0f) + this->actor.home.pos.y;
        this->pos.z = MM_Rand_CenteredFloat(300.0f) + this->actor.home.pos.z;
        this->timer2 = MM_Rand_ZeroFloat(50.0f) + 30.0f;
        this->step = 0;
    }

    yRot = MM_Math_Atan2S(xDiff, zDiff);
    xRot = MM_Math_Atan2S(yDiff, xzDist);
    zRot = MM_Math_SmoothStepToS(&this->actor.world.rot.y, yRot, 0xA, this->step, 0);

    if (zRot > 0x1000) {
        zRot = 0x1000;
    } else if (zRot < -0x1000) {
        zRot = -0x1000;
    }

    MM_Math_ApproachS(&this->actor.world.rot.x, xRot, 0xA, this->step);
    MM_Math_ApproachS(&this->actor.world.rot.z, -zRot, 0xA, this->step);
    MM_Math_ApproachS(&this->step, 0x200, 1, 0x10);

    if ((s32)this->skelAnime.playSpeed == 0) {
        if (this->timer == 0) {
            this->skelAnime.playSpeed = 1.0f;
            this->timer = MM_Rand_ZeroFloat(70.0f) + 50.0f;
        } else if (((this->timer & 7) == 7) && (MM_Rand_ZeroOne() < 0.5f)) {
            this->unk1E4 = MM_Rand_CenteredFloat(3000.0f);
        }
    } else {
        MM_SkelAnime_Update(&this->skelAnime);
        if ((this->timer == 0) && MM_Animation_OnFrame(&this->skelAnime, 6.0f)) {
            this->skelAnime.playSpeed = 0.0f;
            this->timer = MM_Rand_ZeroFloat(50.0f) + 50.0f;
        }
    }

    this->actor.shape.rot.x = -this->actor.world.rot.x;
    this->actor.shape.rot.y = this->actor.world.rot.y;
    this->actor.shape.rot.z = this->actor.world.rot.z;
    MM_Math_ApproachS(&this->unk1E2, this->unk1E4, 3, 0x3E8);
}

void EnBh_Update(Actor* thisx, PlayState* play) {
    EnBh* this = (EnBh*)thisx;

    Actor_MoveWithoutGravity(&this->actor);
    DECR(this->timer2);
    DECR(this->timer);
    this->actionFunc(this, play);
    MM_Math_Vec3f_Copy(&this->actor.focus.pos, &this->actor.world.pos);
}

void EnBh_Draw(Actor* thisx, PlayState* play) {
    EnBh* this = (EnBh*)thisx;

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_RotateZS(this->unk1E2, MTXMODE_APPLY);
    MM_SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          NULL, &this->actor);
}
