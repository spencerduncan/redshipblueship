#include "z_en_shopnuts.h"
#include "objects/object_shopnuts/object_shopnuts.h"
#include "overlays/actors/ovl_En_Dns/z_en_dns.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE)

void EnShopnuts_Init(Actor* thisx, PlayState* play);
void EnShopnuts_Destroy(Actor* thisx, PlayState* play);
void EnShopnuts_Update(Actor* thisx, PlayState* play);
void EnShopnuts_Draw(Actor* thisx, PlayState* play);

void EnShopnuts_SetupWait(EnShopnuts* this);
void EnShopnuts_Wait(EnShopnuts* this, PlayState* play);
void EnShopnuts_LookAround(EnShopnuts* this, PlayState* play);
void EnShopnuts_Stand(EnShopnuts* this, PlayState* play);
void EnShopnuts_ThrowNut(EnShopnuts* this, PlayState* play);
void EnShopnuts_Burrow(EnShopnuts* this, PlayState* play);
void EnShopnuts_SpawnSalesman(EnShopnuts* this, PlayState* play);

const ActorInit En_Shopnuts_InitVars = {
    ACTOR_EN_SHOPNUTS,
    ACTORCAT_ENEMY,
    FLAGS,
    OBJECT_SHOPNUTS,
    sizeof(EnShopnuts),
    (ActorFunc)EnShopnuts_Init,
    (ActorFunc)EnShopnuts_Destroy,
    (ActorFunc)EnShopnuts_Update,
    (ActorFunc)EnShopnuts_Draw,
    NULL,
};

static ColliderCylinderInit OoT_sCylinderInit = {
    {
        COLTYPE_HIT6,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 20, 40, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit OoT_sColChkInfoInit = { 1, 20, 40, 0xFE };

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_S8(naviEnemyId, 0x4E, ICHAIN_CONTINUE),
    ICHAIN_F32(gravity, -1, ICHAIN_CONTINUE),
    ICHAIN_F32(targetArrowOffset, 2600, ICHAIN_STOP),
};

void EnShopnuts_Init(Actor* thisx, PlayState* play) {
    EnShopnuts* this = (EnShopnuts*)thisx;

    OoT_Actor_ProcessInitChain(&this->actor, OoT_sInitChain);
    OoT_ActorShape_Init(&this->actor.shape, 0.0f, OoT_ActorShadow_DrawCircle, 35.0f);
    OoT_SkelAnime_InitFlex(play, &this->skelAnime, &gBusinessScrubSkel, &gBusinessScrubAnim_4574, this->jointTable,
                       this->morphTable, 18);
    OoT_Collider_InitCylinder(play, &this->collider);
    OoT_Collider_SetCylinder(play, &this->collider, &this->actor, &OoT_sCylinderInit);
    OoT_CollisionCheck_SetInfo(&this->actor.colChkInfo, NULL, &OoT_sColChkInfoInit);
    OoT_Collider_UpdateCylinder(&this->actor, &this->collider);

    if (GameInteractor_Should(
            VB_BUSINESS_SCRUB_DESPAWN,
            ((this->actor.params == DNS_TYPE_HEART_PIECE) &&
             (Flags_GetItemGetInf(ITEMGETINF_DEKU_SCRUB_HEART_PIECE))) ||
                ((this->actor.params == DNS_TYPE_DEKU_STICK_UPGRADE) &&
                 (OoT_Flags_GetInfTable(INFTABLE_BOUGHT_STICK_UPGRADE))) ||
                ((this->actor.params == DNS_TYPE_DEKU_NUT_UPGRADE) && (OoT_Flags_GetInfTable(INFTABLE_BOUGHT_NUT_UPGRADE))),
            this)) {
        OoT_Actor_Kill(&this->actor);
    } else {
        EnShopnuts_SetupWait(this);
    }
}

void EnShopnuts_Destroy(Actor* thisx, PlayState* play) {
    EnShopnuts* this = (EnShopnuts*)thisx;

    OoT_Collider_DestroyCylinder(play, &this->collider);

    ResourceMgr_UnregisterSkeleton(&this->skelAnime);
}

void EnShopnuts_SetupWait(EnShopnuts* this) {
    OoT_Animation_PlayOnceSetSpeed(&this->skelAnime, &gBusinessScrubAnim_139C, 0.0f);
    this->animFlagAndTimer = OoT_Rand_S16Offset(100, 50);
    this->collider.dim.height = 5;
    this->collider.base.acFlags &= ~AC_ON;
    this->actionFunc = EnShopnuts_Wait;
}

void EnShopnuts_SetupLookAround(EnShopnuts* this) {
    OoT_Animation_PlayLoop(&this->skelAnime, &gBusinessScrubLookAroundAnim);
    this->animFlagAndTimer = 2;
    this->actionFunc = EnShopnuts_LookAround;
}

void EnShopnuts_SetupThrowNut(EnShopnuts* this) {
    OoT_Animation_PlayOnce(&this->skelAnime, &gBusinessScrubAnim_1EC);
    this->actionFunc = EnShopnuts_ThrowNut;
}

void EnShopnuts_SetupStand(EnShopnuts* this) {
    OoT_Animation_MorphToLoop(&this->skelAnime, &gBusinessScrubAnim_4574, -3.0f);
    if (this->actionFunc == EnShopnuts_ThrowNut) {
        this->animFlagAndTimer = 2 | 0x1000; // sets timer and flag
    } else {
        this->animFlagAndTimer = 1;
    }
    this->actionFunc = EnShopnuts_Stand;
}

void EnShopnuts_SetupBurrow(EnShopnuts* this) {
    OoT_Animation_MorphToPlayOnce(&this->skelAnime, &gBusinessScrubAnim_39C, -5.0f);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_DOWN);
    this->actionFunc = EnShopnuts_Burrow;
}

void EnShopnuts_SetupSpawnSalesman(EnShopnuts* this) {
    OoT_Animation_MorphToPlayOnce(&this->skelAnime, &gBusinessScrubRotateAnim, -3.0f);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_DAMAGE);
    this->collider.base.acFlags &= ~AC_ON;
    this->actionFunc = EnShopnuts_SpawnSalesman;
}

void EnShopnuts_Wait(EnShopnuts* this, PlayState* play) {
    s32 hasSlowPlaybackSpeed = false;

    if (this->skelAnime.playSpeed < 0.5f) {
        hasSlowPlaybackSpeed = true;
    }
    if (hasSlowPlaybackSpeed && (this->animFlagAndTimer != 0)) {
        this->animFlagAndTimer--;
    }
    if (OoT_Animation_OnFrame(&this->skelAnime, 9.0f)) {
        this->collider.base.acFlags |= AC_ON;
    } else if (OoT_Animation_OnFrame(&this->skelAnime, 8.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_UP);
    }

    this->collider.dim.height = ((CLAMP(this->skelAnime.curFrame, 9.0f, 13.0f) - 9.0f) * 9.0f) + 5.0f;
    if (!hasSlowPlaybackSpeed && (this->actor.xzDistToPlayer < 120.0f)) {
        EnShopnuts_SetupBurrow(this);
    } else if (OoT_SkelAnime_Update(&this->skelAnime)) {
        if (this->actor.xzDistToPlayer < 120.0f) {
            EnShopnuts_SetupBurrow(this);
        } else if ((this->animFlagAndTimer == 0) && (this->actor.xzDistToPlayer > 320.0f)) {
            EnShopnuts_SetupLookAround(this);
        } else {
            EnShopnuts_SetupStand(this);
        }
    }
    if (hasSlowPlaybackSpeed &&
        ((this->actor.xzDistToPlayer > 160.0f) && (fabsf(this->actor.yDistToPlayer) < 120.0f)) &&
        ((this->animFlagAndTimer == 0) || (this->actor.xzDistToPlayer < 480.0f))) {
        this->skelAnime.playSpeed = 1.0f;
    }
}

void EnShopnuts_LookAround(EnShopnuts* this, PlayState* play) {
    OoT_SkelAnime_Update(&this->skelAnime);
    if (OoT_Animation_OnFrame(&this->skelAnime, 0.0f) && (this->animFlagAndTimer != 0)) {
        this->animFlagAndTimer--;
    }
    if ((this->actor.xzDistToPlayer < 120.0f) || (this->animFlagAndTimer == 0)) {
        EnShopnuts_SetupBurrow(this);
    }
}

void EnShopnuts_Stand(EnShopnuts* this, PlayState* play) {
    OoT_SkelAnime_Update(&this->skelAnime);
    if (OoT_Animation_OnFrame(&this->skelAnime, 0.0f) && (this->animFlagAndTimer != 0)) {
        this->animFlagAndTimer--;
    }
    if (!(this->animFlagAndTimer & 0x1000)) {
        OoT_Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 2, 0xE38);
    }
    if ((this->actor.xzDistToPlayer < 120.0f) || (this->animFlagAndTimer == 0x1000)) {
        EnShopnuts_SetupBurrow(this);
    } else if (this->animFlagAndTimer == 0) {
        EnShopnuts_SetupThrowNut(this);
    }
}

void EnShopnuts_ThrowNut(EnShopnuts* this, PlayState* play) {
    Vec3f spawnPos;

    OoT_Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 2, 0xE38);
    if (this->actor.xzDistToPlayer < 120.0f) {
        EnShopnuts_SetupBurrow(this);
    } else if (OoT_SkelAnime_Update(&this->skelAnime)) {
        EnShopnuts_SetupStand(this);
    } else if (OoT_Animation_OnFrame(&this->skelAnime, 6.0f)) {
        spawnPos.x = this->actor.world.pos.x + (OoT_Math_SinS(this->actor.shape.rot.y) * 23.0f);
        spawnPos.y = this->actor.world.pos.y + 12.0f;
        spawnPos.z = this->actor.world.pos.z + (OoT_Math_CosS(this->actor.shape.rot.y) * 23.0f);
        if (OoT_Actor_Spawn(&play->actorCtx, play, ACTOR_EN_NUTSBALL, spawnPos.x, spawnPos.y, spawnPos.z,
                        this->actor.shape.rot.x, this->actor.shape.rot.y, this->actor.shape.rot.z, 2, true) != NULL) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_THROW);
        }
    }
}

void EnShopnuts_Burrow(EnShopnuts* this, PlayState* play) {
    if (OoT_SkelAnime_Update(&this->skelAnime)) {
        EnShopnuts_SetupWait(this);
    } else {
        this->collider.dim.height = ((4.0f - CLAMP_MAX(this->skelAnime.curFrame, 4.0f)) * 10.0f) + 5.0f;
    }
    if (OoT_Animation_OnFrame(&this->skelAnime, 4.0f)) {
        this->collider.base.acFlags &= ~AC_ON;
    }
}

void EnShopnuts_SpawnSalesman(EnShopnuts* this, PlayState* play) {
    if (OoT_SkelAnime_Update(&this->skelAnime)) {
        OoT_Actor_Spawn(&play->actorCtx, play, ACTOR_EN_DNS, this->actor.world.pos.x, this->actor.world.pos.y,
                    this->actor.world.pos.z, this->actor.shape.rot.x, this->actor.shape.rot.y, this->actor.shape.rot.z,
                    this->actor.params, true);
        OoT_Actor_Kill(&this->actor);
    } else {
        OoT_Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 2, 0xE38);
    }
}

void EnShopnuts_ColliderCheck(EnShopnuts* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        OoT_Actor_SetDropFlag(&this->actor, &this->collider.info, 1);
        EnShopnuts_SetupSpawnSalesman(this);
    } else if (play->actorCtx.unk_02 != 0) {
        EnShopnuts_SetupSpawnSalesman(this);
    }
}

void EnShopnuts_Update(Actor* thisx, PlayState* play) {
    EnShopnuts* this = (EnShopnuts*)thisx;

    EnShopnuts_ColliderCheck(this, play);
    this->actionFunc(this, play);
    OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, this->collider.dim.radius, this->collider.dim.height, 4);
    if (this->collider.base.acFlags & AC_ON) {
        OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    if (this->actionFunc == EnShopnuts_Wait) {
        OoT_Actor_SetFocus(&this->actor, this->skelAnime.curFrame);
    } else if (this->actionFunc == EnShopnuts_Burrow) {
        OoT_Actor_SetFocus(&this->actor,
                       20.0f - ((this->skelAnime.curFrame * 20.0f) / OoT_Animation_GetLastFrame(&gBusinessScrubAnim_39C)));
    } else {
        OoT_Actor_SetFocus(&this->actor, 20.0f);
    }
}

s32 EnShopnuts_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnShopnuts* this = (EnShopnuts*)thisx;

    if ((limbIndex == 9) && (this->actionFunc == EnShopnuts_ThrowNut)) {
        *dList = NULL;
    }
    return 0;
}

void EnShopnuts_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnShopnuts* this = (EnShopnuts*)thisx;

    f32 curFrame;
    f32 x;
    f32 y;
    f32 z;

    if ((limbIndex == 9) && (this->actionFunc == EnShopnuts_ThrowNut)) {
        OPEN_DISPS(play->state.gfxCtx);
        curFrame = this->skelAnime.curFrame;
        if (curFrame <= 6.0f) {
            y = 1.0f - (curFrame * 0.0833f);
            x = z = (curFrame * 0.1167f) + 1.0f;
        } else if (curFrame <= 7.0f) {
            curFrame -= 6.0f;
            y = 0.5f + curFrame;
            x = z = 1.7f - (curFrame * 0.7f);
        } else if (curFrame <= 10.0f) {
            y = 1.5f - ((curFrame - 7.0f) * 0.1667f);
            x = z = 1.0f;
        } else {
            x = y = z = 1.0f;
        }

        OoT_Matrix_Scale(x, y, z, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, gBusinessScrubNoseDL);
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void EnShopnuts_Draw(Actor* thisx, PlayState* play) {
    EnShopnuts* this = (EnShopnuts*)thisx;

    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, EnShopnuts_OverrideLimbDraw, EnShopnuts_PostLimbDraw, this);
}
