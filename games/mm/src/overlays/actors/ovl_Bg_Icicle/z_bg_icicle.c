/*
 * File: z_bg_icicle.c
 * Overlay: ovl_Bg_Icicle
 * Description: Icicles
 */

#include "z_bg_icicle.h"
#include "objects/object_icicle/object_icicle.h"

#define FLAGS 0x00000000

void BgIcicle_Init(Actor* thisx, PlayState* play);
void BgIcicle_Destroy(Actor* thisx, PlayState* play);
void BgIcicle_Update(Actor* thisx, PlayState* play);
void BgIcicle_Draw(Actor* thisx, PlayState* play);

void BgIcicle_DoNothing(BgIcicle* this, PlayState* play);
void BgIcicle_Wait(BgIcicle* this, PlayState* play);
void BgIcicle_Shiver(BgIcicle* this, PlayState* play);
void BgIcicle_Fall(BgIcicle* this, PlayState* play);
void BgIcicle_Regrow(BgIcicle* this, PlayState* play);

static ColliderCylinderInit MM_sCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x04 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_NONE,
    },
    { 13, 120, 0, { 0, 0, 0 } },
};

ActorProfile Bg_Icicle_Profile = {
    /**/ ACTOR_BG_ICICLE,
    /**/ ACTORCAT_PROP,
    /**/ FLAGS,
    /**/ OBJECT_ICICLE,
    /**/ sizeof(BgIcicle),
    /**/ BgIcicle_Init,
    /**/ BgIcicle_Destroy,
    /**/ BgIcicle_Update,
    /**/ BgIcicle_Draw,
};

static InitChainEntry MM_sInitChain[] = {
    ICHAIN_F32(cullingVolumeScale, 1500, ICHAIN_CONTINUE),
    ICHAIN_F32(gravity, -3, ICHAIN_CONTINUE),
    ICHAIN_F32(terminalVelocity, -30, ICHAIN_CONTINUE),
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_STOP),
};

void BgIcicle_Init(Actor* thisx, PlayState* play) {
    s32 pad;
    BgIcicle* this = (BgIcicle*)thisx;
    s32 paramsHigh;
    s32 paramsMid;

    MM_Actor_ProcessInitChain(thisx, MM_sInitChain);
    MM_DynaPolyActor_Init(&this->dyna, 0);
    DynaPolyActor_LoadMesh(play, &this->dyna, &gIcicleCol);

    Collider_InitAndSetCylinder(play, &this->collider, thisx, &MM_sCylinderInit);
    MM_Collider_UpdateCylinder(thisx, &this->collider);

    paramsHigh = (thisx->params >> 8) & 0xFF;
    paramsMid = (thisx->params >> 2) & 0x3F;
    this->unk_161 = (thisx->params >> 8) & 0xFF;
    thisx->params &= 3;

    if (thisx->params == ICICLE_STALAGMITE_RANDOM_DROP || thisx->params == ICICLE_STALAGMITE_FIXED_DROP) {
        this->unk_160 = ((thisx->params == ICICLE_STALAGMITE_RANDOM_DROP) ? paramsHigh : paramsMid);
        this->actionFunc = BgIcicle_DoNothing;
    } else {
        this->dyna.actor.shape.rot.x = -0x8000;
        this->dyna.actor.shape.yOffset = 1200.0f;
        this->actionFunc = BgIcicle_Wait;
    }
}

void BgIcicle_Destroy(Actor* thisx, PlayState* play) {
    BgIcicle* this = (BgIcicle*)thisx;

    MM_DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    MM_Collider_DestroyCylinder(play, &this->collider);
}

void BgIcicle_Break(BgIcicle* this, PlayState* play, f32 arg2) {
    static Vec3f MM_sAccel = { 0.0f, -1.0f, 0.0f };
    static Color_RGBA8 sPrimColor = { 170, 255, 255, 255 };
    static Color_RGBA8 sEnvColor = { 0, 50, 100, 255 };
    Vec3f velocity;
    Vec3f pos;
    s32 j;
    s32 i;

    MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->dyna.actor.world.pos, 30, NA_SE_EV_ICE_BROKEN);

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 10; j++) {
            pos.x = this->dyna.actor.world.pos.x + MM_Rand_CenteredFloat(8.0f);
            pos.y = this->dyna.actor.world.pos.y + (MM_Rand_ZeroOne() * arg2) + (i * arg2);
            pos.z = this->dyna.actor.world.pos.z + MM_Rand_CenteredFloat(8.0f);

            velocity.x = MM_Rand_CenteredFloat(7.0f);
            velocity.z = MM_Rand_CenteredFloat(7.0f);
            velocity.y = (MM_Rand_ZeroOne() * 4.0f) + 8.0f;

            MM_EffectSsEnIce_Spawn(play, &pos, (MM_Rand_ZeroOne() * 0.2f) + 0.1f, &velocity, &MM_sAccel, &sPrimColor, &sEnvColor,
                                30);
        }
    }
}

void BgIcicle_DoNothing(BgIcicle* this, PlayState* play) {
}

void BgIcicle_Wait(BgIcicle* this, PlayState* play) {
    if (this->dyna.actor.xzDistToPlayer < 60.0f) {
        this->shiverTimer = 10;
        this->actionFunc = BgIcicle_Shiver;
    }
}

void BgIcicle_Shiver(BgIcicle* this, PlayState* play) {
    s32 randSign;
    f32 rand;

    if (this->shiverTimer != 0) {
        this->shiverTimer--;
    }

    if (!(this->shiverTimer % 4)) {
        Actor_PlaySfx(&this->dyna.actor, NA_SE_EV_ICE_SWING);
    }

    if (this->shiverTimer == 0) {
        this->dyna.actor.world.pos.x = this->dyna.actor.home.pos.x;
        this->dyna.actor.world.pos.z = this->dyna.actor.home.pos.z;

        MM_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        DynaPoly_DisableCollision(play, &play->colCtx.dyna, this->dyna.bgId);
        this->actionFunc = BgIcicle_Fall;
    } else {
        rand = MM_Rand_ZeroOne();
        randSign = (MM_Rand_ZeroOne() < 0.5f ? -1 : 1);
        this->dyna.actor.world.pos.x = (randSign * ((0.5f * rand) + 0.5f)) + this->dyna.actor.home.pos.x;
        rand = MM_Rand_ZeroOne();
        randSign = (MM_Rand_ZeroOne() < 0.5f ? -1 : 1);
        this->dyna.actor.world.pos.z = (randSign * ((0.5f * rand) + 0.5f)) + this->dyna.actor.home.pos.z;
    }
}

void BgIcicle_Fall(BgIcicle* this, PlayState* play) {
    if ((this->collider.base.atFlags & AT_HIT) || (this->dyna.actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        this->collider.base.atFlags &= ~AT_HIT;
        this->dyna.actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;

        if (this->dyna.actor.world.pos.y < this->dyna.actor.floorHeight) {
            this->dyna.actor.world.pos.y = this->dyna.actor.floorHeight;
        }

        BgIcicle_Break(this, play, 40.0f);

        if (this->dyna.actor.params == ICICLE_STALACTITE_REGROW) {
            this->dyna.actor.world.pos.y = this->dyna.actor.home.pos.y + 120.0f;
            DynaPoly_EnableCollision(play, &play->colCtx.dyna, this->dyna.bgId);
            this->actionFunc = BgIcicle_Regrow;
        } else {
            MM_Actor_Kill(&this->dyna.actor);
        }
    } else {
        Actor_MoveWithGravity(&this->dyna.actor);
        this->dyna.actor.world.pos.y += 40.0f;
        MM_Actor_UpdateBgCheckInfo(play, &this->dyna.actor, 0.0f, 0.0f, 0.0f, UPDBGCHECKINFO_FLAG_4);
        this->dyna.actor.world.pos.y -= 40.0f;
        MM_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}

void BgIcicle_Regrow(BgIcicle* this, PlayState* play) {
    if (MM_Math_StepToF(&this->dyna.actor.world.pos.y, this->dyna.actor.home.pos.y, 1.0f)) {
        this->actionFunc = BgIcicle_Wait;
        this->dyna.actor.velocity.y = 0.0f;
    }
}

void BgIcicle_UpdateAttacked(BgIcicle* this, PlayState* play) {
    s32 dropItem00Id;

    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;

        if (this->dyna.actor.params == ICICLE_STALAGMITE_RANDOM_DROP) {
            BgIcicle_Break(this, play, 50.0f);

            if (this->unk_160 != 0xFF) {
                MM_Item_DropCollectibleRandom(play, NULL, &this->dyna.actor.world.pos, this->unk_160 << 4);
            }
        } else if (this->dyna.actor.params == ICICLE_STALAGMITE_FIXED_DROP) {
            dropItem00Id = func_800A8150(this->unk_160);
            BgIcicle_Break(this, play, 50.0f);
            MM_Item_DropCollectible(play, &this->dyna.actor.world.pos, (this->unk_161 << 8) | dropItem00Id);
        } else {
            if (this->dyna.actor.params == ICICLE_STALACTITE_REGROW) {
                BgIcicle_Break(this, play, 40.0f);
                this->dyna.actor.world.pos.y = this->dyna.actor.home.pos.y + 120.0f;
                DynaPoly_EnableCollision(play, &play->colCtx.dyna, this->dyna.bgId);
                this->actionFunc = BgIcicle_Regrow;
                return;
            }

            BgIcicle_Break(this, play, 40.0f);
        }

        MM_Actor_Kill(&this->dyna.actor);
    }
}

void BgIcicle_Update(Actor* thisx, PlayState* play) {
    s32 pad;
    BgIcicle* this = (BgIcicle*)thisx;

    BgIcicle_UpdateAttacked(this, play);
    this->actionFunc(this, play);

    if (this->actionFunc != BgIcicle_Regrow) {
        MM_Collider_UpdateCylinder(&this->dyna.actor, &this->collider);
        MM_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

void BgIcicle_Draw(Actor* thisx, PlayState* play) {
    MM_Gfx_DrawDListOpa(play, gIcicleDL);
}
