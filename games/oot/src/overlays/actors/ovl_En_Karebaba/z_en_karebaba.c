/**
 * File: z_en_karebaba.c
 * Overlay: ovl_En_Karebaba
 * Description: Withered Deku Baba
 */

#include "z_en_karebaba.h"
#include "objects/object_dekubaba/object_dekubaba.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/effects/ovl_Effect_Ss_Hahen/z_eff_ss_hahen.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ResourceManagerHelpers.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE)

void OoT_EnKarebaba_Init(Actor* thisx, PlayState* play);
void OoT_EnKarebaba_Destroy(Actor* thisx, PlayState* play);
void OoT_EnKarebaba_Update(Actor* thisx, PlayState* play);
void OoT_EnKarebaba_Draw(Actor* thisx, PlayState* play);

void OoT_EnKarebaba_SetupGrow(EnKarebaba* this);
void OoT_EnKarebaba_SetupIdle(EnKarebaba* this);
void OoT_EnKarebaba_Grow(EnKarebaba* this, PlayState* play);
void OoT_EnKarebaba_Idle(EnKarebaba* this, PlayState* play);
void OoT_EnKarebaba_Awaken(EnKarebaba* this, PlayState* play);
void OoT_EnKarebaba_Spin(EnKarebaba* this, PlayState* play);
void OoT_EnKarebaba_Dying(EnKarebaba* this, PlayState* play);
void OoT_EnKarebaba_DeadItemDrop(EnKarebaba* this, PlayState* play);
void OoT_EnKarebaba_Retract(EnKarebaba* this, PlayState* play);
void OoT_EnKarebaba_Dead(EnKarebaba* this, PlayState* play);
void OoT_EnKarebaba_Regrow(EnKarebaba* this, PlayState* play);
void OoT_EnKarebaba_Upright(EnKarebaba* this, PlayState* play);

const ActorInit En_Karebaba_InitVars = {
    ACTOR_EN_KAREBABA,
    ACTORCAT_ENEMY,
    FLAGS,
    OBJECT_DEKUBABA,
    sizeof(EnKarebaba),
    (ActorFunc)OoT_EnKarebaba_Init,
    (ActorFunc)OoT_EnKarebaba_Destroy,
    (ActorFunc)OoT_EnKarebaba_Update,
    (ActorFunc)OoT_EnKarebaba_Draw,
    NULL,
};

static ColliderCylinderInit sBodyColliderInit = {
    {
        COLTYPE_HARD,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 7, 25, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sHeadColliderInit = {
    {
        COLTYPE_HARD,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_HARD,
        BUMP_NONE,
        OCELEM_ON,
    },
    { 4, 25, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit sColCheckInfoInit = { 1, 15, 80, MASS_HEAVY };

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_F32(targetArrowOffset, 2500, ICHAIN_CONTINUE),
    ICHAIN_U8(targetMode, 1, ICHAIN_CONTINUE),
    ICHAIN_S8(naviEnemyId, 0x09, ICHAIN_STOP),
};

void OoT_EnKarebaba_Init(Actor* thisx, PlayState* play) {
    EnKarebaba* this = (EnKarebaba*)thisx;

    OoT_Actor_ProcessInitChain(&this->actor, OoT_sInitChain);
    OoT_ActorShape_Init(&this->actor.shape, 0.0f, OoT_ActorShadow_DrawCircle, 22.0f);
    OoT_SkelAnime_Init(play, &this->skelAnime, &gDekuBabaSkel, &gDekuBabaFastChompAnim, this->jointTable, this->morphTable,
                   8);
    OoT_Collider_InitCylinder(play, &this->bodyCollider);
    OoT_Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyColliderInit);
    OoT_Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    OoT_Collider_InitCylinder(play, &this->headCollider);
    OoT_Collider_SetCylinder(play, &this->headCollider, &this->actor, &sHeadColliderInit);
    OoT_Collider_UpdateCylinder(&this->actor, &this->headCollider);
    OoT_CollisionCheck_SetInfo(&this->actor.colChkInfo, OoT_DamageTable_Get(1), &sColCheckInfoInit);

    this->boundFloor = NULL;

    if (this->actor.params == 0) {
        OoT_EnKarebaba_SetupGrow(this);
    } else {
        OoT_EnKarebaba_SetupIdle(this);
    }
}

void OoT_EnKarebaba_Destroy(Actor* thisx, PlayState* play) {
    EnKarebaba* this = (EnKarebaba*)thisx;

    OoT_Collider_DestroyCylinder(play, &this->bodyCollider);
    OoT_Collider_DestroyCylinder(play, &this->headCollider);

    ResourceMgr_UnregisterSkeleton(&this->skelAnime);
}

void EnKarebaba_ResetCollider(EnKarebaba* this) {
    this->bodyCollider.dim.radius = 7;
    this->bodyCollider.dim.height = 25;
    this->bodyCollider.base.colType = COLTYPE_HARD;
    this->bodyCollider.base.acFlags |= AC_HARD;
    this->bodyCollider.info.bumper.dmgFlags = ~0x00300000;
    this->headCollider.dim.height = 25;
}

void OoT_EnKarebaba_SetupGrow(EnKarebaba* this) {
    OoT_Actor_SetScale(&this->actor, 0.0f);
    this->actor.shape.rot.x = -0x4000;
    this->actionFunc = OoT_EnKarebaba_Grow;
    this->actor.world.pos.y = this->actor.home.pos.y + 14.0f;
}

void OoT_EnKarebaba_SetupIdle(EnKarebaba* this) {
    OoT_Actor_SetScale(&this->actor, 0.005f);
    this->actor.shape.rot.x = -0x4000;
    this->actionFunc = OoT_EnKarebaba_Idle;
    this->actor.world.pos.y = this->actor.home.pos.y + 14.0f;
}

void OoT_EnKarebaba_SetupAwaken(EnKarebaba* this) {
    OoT_Animation_Change(&this->skelAnime, &gDekuBabaFastChompAnim, 4.0f, 0.0f,
                     OoT_Animation_GetLastFrame(&gDekuBabaFastChompAnim), ANIMMODE_LOOP, -3.0f);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_DUMMY482);
    this->actionFunc = OoT_EnKarebaba_Awaken;
}

void OoT_EnKarebaba_SetupUpright(EnKarebaba* this) {
    if (this->actionFunc != OoT_EnKarebaba_Spin) {
        OoT_Actor_SetScale(&this->actor, 0.01f);
        this->bodyCollider.base.colType = COLTYPE_HIT6;
        this->bodyCollider.base.acFlags &= ~AC_HARD;
        this->bodyCollider.info.bumper.dmgFlags = !LINK_IS_ADULT ? 0x07C00710 : 0x0FC00710;
        this->bodyCollider.dim.radius = 15;
        this->bodyCollider.dim.height = 80;
        this->headCollider.dim.height = 80;
    }

    this->actor.params = 40;
    this->actionFunc = OoT_EnKarebaba_Upright;
}

void OoT_EnKarebaba_SetupSpin(EnKarebaba* this) {
    this->actor.params = 40;
    this->actionFunc = OoT_EnKarebaba_Spin;
}

void OoT_EnKarebaba_SetupDying(EnKarebaba* this) {
    this->actor.params = 0;
    this->actor.gravity = -0.8f;
    this->actor.velocity.y = 4.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x8000;
    this->actor.speedXZ = 3.0f;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_DEKU_JR_DEAD);
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    this->actionFunc = OoT_EnKarebaba_Dying;
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
}

void OoT_EnKarebaba_SetupDeadItemDrop(EnKarebaba* this, PlayState* play) {
    OoT_Actor_SetScale(&this->actor, 0.03f);
    this->actor.shape.rot.x -= 0x4000;
    this->actor.shape.yOffset = 1000.0f;
    this->actor.gravity = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.shape.shadowScale = 3.0f;
    OoT_Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_MISC);
    this->actor.params = 200;
    this->actor.flags &= ~ACTOR_FLAG_DRAW_CULLING_DISABLED;
    this->actionFunc = OoT_EnKarebaba_DeadItemDrop;
}

void OoT_EnKarebaba_SetupRetract(EnKarebaba* this) {
    OoT_Animation_Change(&this->skelAnime, &gDekuBabaFastChompAnim, -3.0f, OoT_Animation_GetLastFrame(&gDekuBabaFastChompAnim),
                     0.0f, ANIMMODE_ONCE, -3.0f);
    EnKarebaba_ResetCollider(this);
    this->actionFunc = OoT_EnKarebaba_Retract;
}

void OoT_EnKarebaba_SetupDead(EnKarebaba* this) {
    OoT_Animation_Change(&this->skelAnime, &gDekuBabaFastChompAnim, 0.0f, 0.0f, 0.0f, ANIMMODE_ONCE, 0.0f);
    EnKarebaba_ResetCollider(this);
    this->actor.shape.rot.x = -0x4000;
    this->actor.params = 200;
    this->actor.parent = NULL;
    this->actor.shape.shadowScale = 0.0f;
    OoT_Math_Vec3f_Copy(&this->actor.world.pos, &this->actor.home.pos);
    this->actionFunc = OoT_EnKarebaba_Dead;
}

void OoT_EnKarebaba_SetupRegrow(EnKarebaba* this) {
    this->actor.shape.yOffset = 0.0f;
    this->actor.shape.shadowScale = 22.0f;
    this->headCollider.dim.radius = sHeadColliderInit.dim.radius;
    OoT_Actor_SetScale(&this->actor, 0.0f);
    this->actionFunc = OoT_EnKarebaba_Regrow;
}

void OoT_EnKarebaba_Grow(EnKarebaba* this, PlayState* play) {
    f32 scale;

    this->actor.params++;
    scale = this->actor.params * 0.05f;
    OoT_Actor_SetScale(&this->actor, 0.005f * scale);
    this->actor.world.pos.y = this->actor.home.pos.y + (14.0f * scale);
    if (this->actor.params == 20) {
        OoT_EnKarebaba_SetupIdle(this);
    }
}

void OoT_EnKarebaba_Idle(EnKarebaba* this, PlayState* play) {
    if (this->actor.xzDistToPlayer < 200.0f && fabsf(this->actor.yDistToPlayer) < 30.0f) {
        OoT_EnKarebaba_SetupAwaken(this);
    }
}

void OoT_EnKarebaba_Awaken(EnKarebaba* this, PlayState* play) {
    OoT_SkelAnime_Update(&this->skelAnime);
    OoT_Math_StepToF(&this->actor.scale.x, 0.01f, 0.0005f);
    this->actor.scale.y = this->actor.scale.z = this->actor.scale.x;
    if (OoT_Math_StepToF(&this->actor.world.pos.y, this->actor.home.pos.y + 60.0f, 5.0f)) {
        OoT_EnKarebaba_SetupUpright(this);
    }
    this->actor.shape.rot.y += 0x1999;
    OoT_EffectSsHahen_SpawnBurst(play, &this->actor.home.pos, 3.0f, 0, 12, 5, 1, HAHEN_OBJECT_DEFAULT, 10, NULL);
}

void OoT_EnKarebaba_Upright(EnKarebaba* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    OoT_SkelAnime_Update(&this->skelAnime);

    if (this->actor.params != 0) {
        this->actor.params--;
    }

    if (OoT_Animation_OnFrame(&this->skelAnime, 0.0f) || OoT_Animation_OnFrame(&this->skelAnime, 12.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_DEKU_JR_MOUTH);
    }

    if (this->bodyCollider.base.acFlags & AC_HIT) {
        OoT_EnKarebaba_SetupDying(this);
        OoT_Enemy_StartFinishingBlow(play, &this->actor);
    } else if (OoT_Math_Vec3f_DistXZ(&this->actor.home.pos, &player->actor.world.pos) > 240.0f) {
        OoT_EnKarebaba_SetupRetract(this);
    } else if (this->actor.params == 0) {
        OoT_EnKarebaba_SetupSpin(this);
    }
}

void OoT_EnKarebaba_Spin(EnKarebaba* this, PlayState* play) {
    s32 value;
    f32 cos60;

    if (this->actor.params != 0) {
        this->actor.params--;
    }

    OoT_SkelAnime_Update(&this->skelAnime);

    if (OoT_Animation_OnFrame(&this->skelAnime, 0.0f) || OoT_Animation_OnFrame(&this->skelAnime, 12.0f)) {

        Audio_PlayActorSound2(&this->actor, NA_SE_EN_DEKU_JR_MOUTH);
    }

    value = 20 - this->actor.params;
    value = 20 - ABS(value);

    if (value > 10) {
        value = 10;
    }

    this->headCollider.dim.radius = sHeadColliderInit.dim.radius + (value * 2);
    this->actor.shape.rot.x = 0xC000 - (value * 0x100);
    this->actor.shape.rot.y += value * 0x2C0;
    this->actor.world.pos.y = (OoT_Math_SinS(this->actor.shape.rot.x) * -60.0f) + this->actor.home.pos.y;

    cos60 = OoT_Math_CosS(this->actor.shape.rot.x) * 60.0f;

    this->actor.world.pos.x = (OoT_Math_SinS(this->actor.shape.rot.y) * cos60) + this->actor.home.pos.x;
    this->actor.world.pos.z = (OoT_Math_CosS(this->actor.shape.rot.y) * cos60) + this->actor.home.pos.z;

    if (this->bodyCollider.base.acFlags & AC_HIT) {
        OoT_EnKarebaba_SetupDying(this);
        OoT_Enemy_StartFinishingBlow(play, &this->actor);
    } else if (this->actor.params == 0) {
        OoT_EnKarebaba_SetupUpright(this);
    }
}

void OoT_EnKarebaba_Dying(EnKarebaba* this, PlayState* play) {
    static Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    s32 i;
    Vec3f position;
    Vec3f rotation;

    OoT_Math_StepToF(&this->actor.speedXZ, 0.0f, 0.1f);

    if (this->actor.params == 0) {
        OoT_Math_ScaledStepToS(&this->actor.shape.rot.x, 0x4800, 0x71C);
        OoT_EffectSsHahen_SpawnBurst(play, &this->actor.world.pos, 3.0f, 0, 12, 5, 1, HAHEN_OBJECT_DEFAULT, 10, NULL);

        if (this->actor.scale.x > 0.005f && ((this->actor.bgCheckFlags & 2) || (this->actor.bgCheckFlags & 8))) {
            this->actor.scale.x = this->actor.scale.y = this->actor.scale.z = 0.0f;
            this->actor.speedXZ = 0.0f;
            this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
            OoT_EffectSsHahen_SpawnBurst(play, &this->actor.world.pos, 3.0f, 0, 12, 5, 15, HAHEN_OBJECT_DEFAULT, 10, NULL);
        }

        if (this->actor.bgCheckFlags & 2) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_DODO_M_GND);
            this->actor.params = 1;
        }
    } else if (this->actor.params == 1) {
        OoT_Math_Vec3f_Copy(&position, &this->actor.world.pos);
        rotation.z = OoT_Math_SinS(this->actor.shape.rot.x) * 20.0f;
        rotation.x = -20.0f * OoT_Math_CosS(this->actor.shape.rot.x) * OoT_Math_SinS(this->actor.shape.rot.y);
        rotation.y = -20.0f * OoT_Math_CosS(this->actor.shape.rot.x) * OoT_Math_CosS(this->actor.shape.rot.y);

        for (i = 0; i < 4; i++) {
            func_800286CC(play, &position, &zeroVec, &zeroVec, 500, 50);
            position.x += rotation.x;
            position.y += rotation.z;
            position.z += rotation.y;
        }

        func_800286CC(play, &this->actor.home.pos, &zeroVec, &zeroVec, 500, 100);
        OoT_EnKarebaba_SetupDeadItemDrop(this, play);
    }
}

void OoT_EnKarebaba_DeadItemDrop(EnKarebaba* this, PlayState* play) {
    if (this->actor.params != 0) {
        this->actor.params--;
    }

    if (OoT_Actor_HasParent(&this->actor, play) || this->actor.params == 0) {
        OoT_EnKarebaba_SetupDead(this);
    } else {
        OoT_Actor_OfferGetItemNearby(&this->actor, play, GI_STICKS_1);
    }
}

void OoT_EnKarebaba_Retract(EnKarebaba* this, PlayState* play) {
    OoT_SkelAnime_Update(&this->skelAnime);
    OoT_Math_StepToF(&this->actor.scale.x, 0.005f, 0.0005f);
    this->actor.scale.y = this->actor.scale.z = this->actor.scale.x;

    if (OoT_Math_StepToF(&this->actor.world.pos.y, this->actor.home.pos.y + 14.0f, 5.0f)) {
        OoT_EnKarebaba_SetupIdle(this);
    }

    this->actor.shape.rot.y += 0x1999;
    OoT_EffectSsHahen_SpawnBurst(play, &this->actor.home.pos, 3.0f, 0, 12, 5, 1, HAHEN_OBJECT_DEFAULT, 10, NULL);
}

void OoT_EnKarebaba_Dead(EnKarebaba* this, PlayState* play) {
    OoT_SkelAnime_Update(&this->skelAnime);

    if (this->actor.params != 0) {
        this->actor.params--;
    }
    if (this->actor.params == 0) {
        OoT_EnKarebaba_SetupRegrow(this);
    }
}

void OoT_EnKarebaba_Regrow(EnKarebaba* this, PlayState* play) {
    f32 scaleFactor;

    this->actor.params++;
    scaleFactor = this->actor.params * 0.05f;
    OoT_Actor_SetScale(&this->actor, 0.005f * scaleFactor);
    this->actor.world.pos.y = this->actor.home.pos.y + (14.0f * scaleFactor);

    if (this->actor.params == 20) {
        this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE;
        OoT_Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_ENEMY);
        OoT_EnKarebaba_SetupIdle(this);
    }
}

void OoT_EnKarebaba_Update(Actor* thisx, PlayState* play) {
    s32 pad;
    EnKarebaba* this = (EnKarebaba*)thisx;
    f32 height;

    this->actionFunc(this, play);

    if (this->actionFunc != OoT_EnKarebaba_Dead) {
        if (this->actionFunc == OoT_EnKarebaba_Dying) {
            Actor_MoveXZGravity(&this->actor);
            OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 10.0f, 15.0f, 10.0f, 5);
        } else {
            OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 0.0f, 0.0f, 4);
            if (this->boundFloor == NULL) {
                this->boundFloor = this->actor.floorPoly;
            }
        }
        if (this->actionFunc != OoT_EnKarebaba_Dying && this->actionFunc != OoT_EnKarebaba_DeadItemDrop) {
            if (this->actionFunc != OoT_EnKarebaba_Regrow && this->actionFunc != OoT_EnKarebaba_Grow) {
                OoT_CollisionCheck_SetAT(play, &play->colChkCtx, &this->headCollider.base);
                OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
            }
            OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->headCollider.base);
            OoT_Actor_SetFocus(&this->actor, (this->actor.scale.x * 10.0f) / 0.01f);
            height = this->actor.home.pos.y + 40.0f;
            this->actor.focus.pos.x = this->actor.home.pos.x;
            this->actor.focus.pos.y = CLAMP_MAX(this->actor.focus.pos.y, height);
            this->actor.focus.pos.z = this->actor.home.pos.z;
        }
    }
}

void EnKarebaba_DrawBaseShadow(EnKarebaba* this, PlayState* play) {
    MtxF mf;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_44Xlu(play->state.gfxCtx);

    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 0, 0, 0, 255);
    func_80038A28(this->boundFloor, this->actor.home.pos.x, this->actor.home.pos.y, this->actor.home.pos.z, &mf);
    OoT_Matrix_Mult(&mf, MTXMODE_NEW);
    OoT_Matrix_Scale(0.15f, 1.0f, 0.15f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, gCircleShadowDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void OoT_EnKarebaba_Draw(Actor* thisx, PlayState* play) {
    static Color_RGBA8 black = { 0, 0, 0, 0 };
    static Gfx* stemDLists[] = { gDekuBabaStemTopDL, gDekuBabaStemMiddleDL, gDekuBabaStemBaseDL };
    static Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    EnKarebaba* this = (EnKarebaba*)thisx;
    s32 i;
    s32 stemSections;
    f32 scale;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    if (this->actionFunc == OoT_EnKarebaba_DeadItemDrop) {
        if (this->actor.params > 40 || (this->actor.params & 1)) {
            OoT_Matrix_Translate(0.0f, 0.0f, 200.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, gDekuBabaStickDropDL);
        }
    } else if (this->actionFunc != OoT_EnKarebaba_Dead) {
        func_80026230(play, &black, 1, 2);
        SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, NULL, NULL, NULL);
        OoT_Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);

        if ((this->actionFunc == OoT_EnKarebaba_Regrow) || (this->actionFunc == OoT_EnKarebaba_Grow)) {
            scale = this->actor.params * 0.0005f;
        } else {
            scale = 0.01f;
        }

        OoT_Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        OoT_Matrix_RotateZYX(this->actor.shape.rot.x, this->actor.shape.rot.y, 0, MTXMODE_APPLY);

        if (this->actionFunc == OoT_EnKarebaba_Dying) {
            stemSections = 2;
        } else {
            stemSections = 3;
        }

        for (i = 0; i < stemSections; i++) {
            OoT_Matrix_Translate(0.0f, 0.0f, -2000.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_LOAD | G_MTX_NOPUSH | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, stemDLists[i]);

            if (i == 0 && this->actionFunc == OoT_EnKarebaba_Dying) {
                OoT_Matrix_MultVec3f(&zeroVec, &this->actor.focus.pos);
            }
        }

        func_80026608(play);
    }

    func_80026230(play, &black, 1, 2);
    OoT_Matrix_Translate(this->actor.home.pos.x, this->actor.home.pos.y, this->actor.home.pos.z, MTXMODE_NEW);

    if (this->actionFunc != OoT_EnKarebaba_Grow) {
        scale = 0.01f;
    }

    OoT_Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_RotateY(this->actor.home.rot.y * (M_PI / 0x8000), MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_LOAD | G_MTX_NOPUSH | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, gDekuBabaBaseLeavesDL);

    if (this->actionFunc == OoT_EnKarebaba_Dying) {
        OoT_Matrix_RotateZYX(-0x4000, (s16)(this->actor.shape.rot.y - this->actor.home.rot.y), 0, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_LOAD | G_MTX_NOPUSH | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, gDekuBabaStemBaseDL);
    }

    func_80026608(play);

    CLOSE_DISPS(play->state.gfxCtx);

    if (this->boundFloor != NULL) {
        EnKarebaba_DrawBaseShadow(this, play);
    }
}
