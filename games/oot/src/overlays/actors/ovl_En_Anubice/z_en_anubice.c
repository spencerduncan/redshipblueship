/*
 * File: z_en_anubice.c
 * Overlay: ovl_En_Anubice
 * Description: Anubis Body
 */

#include "z_en_anubice.h"
#include "objects/object_anubice/object_anubice.h"
#include "overlays/actors/ovl_En_Anubice_Tag/z_en_anubice_tag.h"
#include "overlays/actors/ovl_Bg_Hidan_Curtain/z_bg_hidan_curtain.h"
#include "vt.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ResourceManagerHelpers.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)

void EnAnubice_Init(Actor* thisx, PlayState* play);
void EnAnubice_Destroy(Actor* thisx, PlayState* play);
void EnAnubice_Update(Actor* thisx, PlayState* play);
void EnAnubice_Draw(Actor* thisx, PlayState* play);

void EnAnubice_FindFlameCircles(EnAnubice* this, PlayState* play);
void EnAnubice_SetupIdle(EnAnubice* this, PlayState* play);
void EnAnubice_Idle(EnAnubice* this, PlayState* play);
void EnAnubice_GoToHome(EnAnubice* this, PlayState* play);
void EnAnubice_SetupShootFireball(EnAnubice* this, PlayState* play);
void EnAnubice_ShootFireball(EnAnubice* this, PlayState* play);
void EnAnubice_Die(EnAnubice* this, PlayState* play);

const ActorInit En_Anubice_InitVars = {
    ACTOR_EN_ANUBICE,
    ACTORCAT_ENEMY,
    FLAGS,
    OBJECT_ANUBICE,
    sizeof(EnAnubice),
    (ActorFunc)EnAnubice_Init,
    (ActorFunc)EnAnubice_Destroy,
    (ActorFunc)EnAnubice_Update,
    (ActorFunc)EnAnubice_Draw,
    NULL,
};

static ColliderCylinderInit OoT_sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 29, 103, 0, { 0, 0, 0 } },
};

typedef enum {
    /* 0x0 */ ANUBICE_DMGEFF_NONE,
    /* 0x2 */ ANUBICE_DMGEFF_FIRE = 2,
    /* 0xF */ ANUBICE_DMGEFF_0xF = 0xF // Treated the same as ANUBICE_DMGEFF_NONE in code
} AnubiceDamageEffect;

static DamageTable OoT_sDamageTable[] = {
    /* Deku nut      */ DMG_ENTRY(0, ANUBICE_DMGEFF_NONE),
    /* Deku stick    */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Slingshot     */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Explosive     */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Boomerang     */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Normal arrow  */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Hammer swing  */ DMG_ENTRY(1, ANUBICE_DMGEFF_0xF),
    /* Hookshot      */ DMG_ENTRY(2, ANUBICE_DMGEFF_0xF),
    /* Kokiri sword  */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Master sword  */ DMG_ENTRY(2, ANUBICE_DMGEFF_0xF),
    /* Giant's Knife */ DMG_ENTRY(6, ANUBICE_DMGEFF_0xF),
    /* Fire arrow    */ DMG_ENTRY(2, ANUBICE_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Light arrow   */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Unk arrow 1   */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Unk arrow 2   */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Unk arrow 3   */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Fire magic    */ DMG_ENTRY(3, ANUBICE_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(0, ANUBICE_DMGEFF_NONE),
    /* Light magic   */ DMG_ENTRY(0, ANUBICE_DMGEFF_NONE),
    /* Shield        */ DMG_ENTRY(0, ANUBICE_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, ANUBICE_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Giant spin    */ DMG_ENTRY(6, ANUBICE_DMGEFF_0xF),
    /* Master spin   */ DMG_ENTRY(2, ANUBICE_DMGEFF_0xF),
    /* Kokiri jump   */ DMG_ENTRY(0, ANUBICE_DMGEFF_0xF),
    /* Giant jump    */ DMG_ENTRY(12, ANUBICE_DMGEFF_0xF),
    /* Master jump   */ DMG_ENTRY(4, ANUBICE_DMGEFF_0xF),
    /* Unknown 1     */ DMG_ENTRY(0, ANUBICE_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, ANUBICE_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(0, ANUBICE_DMGEFF_NONE),
    /* Unknown 2     */ DMG_ENTRY(0, ANUBICE_DMGEFF_NONE),
};

void EnAnubice_Hover(EnAnubice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->hoverVelocityTimer += 1500.0f;
    this->targetHeight = player->actor.world.pos.y + this->playerHeightOffset;
    OoT_Math_ApproachF(&this->actor.world.pos.y, this->targetHeight, 0.1f, 10.0f);
    OoT_Math_ApproachF(&this->playerHeightOffset, 10.0f, 0.1f, 0.5f);
    this->actor.velocity.y = OoT_Math_SinS(this->hoverVelocityTimer);
}

void EnAnubice_SetFireballRot(EnAnubice* this, PlayState* play) {
    f32 xzdist;
    f32 x;
    f32 y;
    f32 z;
    Player* player = GET_PLAYER(play);

    x = player->actor.world.pos.x - this->fireballPos.x;
    y = player->actor.world.pos.y + 10.0f - this->fireballPos.y;
    z = player->actor.world.pos.z - this->fireballPos.z;
    xzdist = OoT_sqrtf(SQ(x) + SQ(z));

    this->fireballRot.x = -RADF_TO_BINANG(OoT_Math_FAtan2F(y, xzdist));
    this->fireballRot.y = RADF_TO_BINANG(OoT_Math_FAtan2F(x, z));
}

void EnAnubice_Init(Actor* thisx, PlayState* play) {
    EnAnubice* this = (EnAnubice*)thisx;

    OoT_ActorShape_Init(&this->actor.shape, 0.0f, OoT_ActorShadow_DrawCircle, 20.0f);
    OoT_SkelAnime_Init(play, &this->skelAnime, &gAnubiceSkel, &gAnubiceIdleAnim, this->jointTable, this->morphTable,
                   ANUBICE_LIMB_MAX);

    osSyncPrintf("\n\n");
    // "☆☆☆☆☆ Anubis occurence ☆☆☆☆☆"
    osSyncPrintf(VT_FGCOL(YELLOW) "☆☆☆☆☆ アヌビス発生 ☆☆☆☆☆ \n" VT_RST);

    this->actor.naviEnemyId = 0x3A;

    OoT_Collider_InitCylinder(play, &this->collider);
    OoT_Collider_SetCylinder(play, &this->collider, &this->actor, &OoT_sCylinderInit);

    OoT_Actor_SetScale(&this->actor, 0.015f);

    this->actor.colChkInfo.damageTable = OoT_sDamageTable;
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.shape.yOffset = -4230.0f;
    this->focusHeightOffset = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->home = this->actor.world.pos;
    this->actor.targetMode = 3;
    this->actionFunc = EnAnubice_FindFlameCircles;
}

void EnAnubice_Destroy(Actor* thisx, PlayState* play) {
    EnAnubice* this = (EnAnubice*)thisx;
    EnAnubiceTag* tag;

    OoT_Collider_DestroyCylinder(play, &this->collider);

    if (this->actor.params != 0) {
        if (this->actor.parent) {}

        tag = (EnAnubiceTag*)this->actor.parent;
        if (tag != NULL && tag->actor.update != NULL) {
            tag->anubis = NULL;
        }
    }

    ResourceMgr_UnregisterSkeleton(&this->skelAnime);
}

void EnAnubice_FindFlameCircles(EnAnubice* this, PlayState* play) {
    Actor* currentProp;
    s32 flameCirclesFound;

    if (this->isMirroringLink) {
        if (!this->hasSearchedForFlameCircles) {
            flameCirclesFound = 0;
            currentProp = play->actorCtx.actorLists[ACTORCAT_PROP].head;
            while (currentProp != NULL) {
                if (currentProp->id != ACTOR_BG_HIDAN_CURTAIN) {
                    currentProp = currentProp->next;
                } else {
                    this->flameCircles[flameCirclesFound] = (BgHidanCurtain*)currentProp;
                    // "☆☆☆☆☆ How many fires? ☆☆☆☆☆"
                    osSyncPrintf(VT_FGCOL(GREEN) "☆☆☆☆☆ 火は幾つ？ ☆☆☆☆☆ %d\n" VT_RST, flameCirclesFound);
                    osSyncPrintf(VT_FGCOL(YELLOW) "☆☆☆☆☆ 火は幾つ？ ☆☆☆☆☆ %x\n" VT_RST,
                                 this->flameCircles[flameCirclesFound]);
                    if (flameCirclesFound < ARRAY_COUNT(this->flameCircles) - 1) {
                        flameCirclesFound++;
                    }
                    currentProp = currentProp->next;
                }
            }
            this->hasSearchedForFlameCircles = true;
        }
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        this->actionFunc = EnAnubice_SetupIdle;
    }
}

void EnAnubice_SetupIdle(EnAnubice* this, PlayState* play) {
    f32 lastFrame = OoT_Animation_GetLastFrame(&gAnubiceIdleAnim);

    OoT_Animation_Change(&this->skelAnime, &gAnubiceIdleAnim, 1.0f, 0.0f, (s16)lastFrame, ANIMMODE_LOOP, -10.0f);

    this->actionFunc = EnAnubice_Idle;
    this->actor.velocity.x = this->actor.velocity.z = this->actor.gravity = 0.0f;
}

void EnAnubice_Idle(EnAnubice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    OoT_SkelAnime_Update(&this->skelAnime);
    OoT_Math_ApproachZeroF(&this->actor.shape.yOffset, 0.5f, 300.0f);
    OoT_Math_ApproachF(&this->focusHeightOffset, 70.0f, 0.5f, 5.0f);

    if (!this->isKnockedback) {
        OoT_Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, 3000, 0);
    }

    if (this->actor.shape.yOffset > -2.0f) {
        this->actor.shape.yOffset = 0.0f;

        if (player->meleeWeaponState != 0) {
            this->actionFunc = EnAnubice_SetupShootFireball;
        } else if (this->isLinkOutOfRange) {
            this->actor.velocity.y = 0.0f;
            this->actor.gravity = -1.0f;
            this->actionFunc = EnAnubice_GoToHome;
        }
    }
}

void EnAnubice_GoToHome(EnAnubice* this, PlayState* play) {
    f32 xzdist;
    f32 xRatio;
    f32 zRatio;
    f32 x;
    f32 z;

    OoT_SkelAnime_Update(&this->skelAnime);
    OoT_Math_ApproachF(&this->actor.shape.yOffset, -4230.0f, 0.5f, 300.0f);
    OoT_Math_ApproachZeroF(&this->focusHeightOffset, 0.5f, 5.0f);

    if (!this->isKnockedback) {
        OoT_Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, 3000, 0);
    }

    if ((fabsf(this->home.x - this->actor.world.pos.x) > 3.0f) &&
        (fabsf(this->home.z - this->actor.world.pos.z) > 3.0f)) {
        x = this->home.x - this->actor.world.pos.x;
        z = this->home.z - this->actor.world.pos.z;
        xzdist = OoT_sqrtf(SQ(x) + SQ(z));
        xRatio = x / xzdist;
        zRatio = z / xzdist;
        this->actor.world.pos.x += xRatio * 8;
        this->actor.world.pos.z += zRatio * 8.0f;
    } else if (this->actor.shape.yOffset < -4220.0f) {
        this->actor.shape.yOffset = -4230.0f;
        this->isMirroringLink = this->isLinkOutOfRange = false;
        this->actionFunc = EnAnubice_FindFlameCircles;
        this->actor.gravity = 0.0f;
    }
}

void EnAnubice_SetupShootFireball(EnAnubice* this, PlayState* play) {
    f32 lastFrame = OoT_Animation_GetLastFrame(&gAnubiceAttackingAnim);

    this->animLastFrame = lastFrame;
    OoT_Animation_Change(&this->skelAnime, &gAnubiceAttackingAnim, 1.0f, 0.0f, lastFrame, ANIMMODE_ONCE, -10.0f);
    this->actionFunc = EnAnubice_ShootFireball;
    this->actor.velocity.x = this->actor.velocity.z = 0.0f;
}

void EnAnubice_ShootFireball(EnAnubice* this, PlayState* play) {
    f32 curFrame = this->skelAnime.curFrame;

    OoT_SkelAnime_Update(&this->skelAnime);

    if (!this->isKnockedback) {
        OoT_Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, 3000, 0);
    }

    EnAnubice_SetFireballRot(this, play);

    if (curFrame == 12.0f) {
        OoT_Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ANUBICE_FIRE, this->fireballPos.x, this->fireballPos.y + 15.0f,
                    this->fireballPos.z, this->fireballRot.x, this->fireballRot.y, 0, 0, true);
    }

    if (this->animLastFrame <= curFrame) {
        this->actionFunc = EnAnubice_SetupIdle;
    }
}

void EnAnubice_SetupDie(EnAnubice* this, PlayState* play) {
    f32 lastFrame = OoT_Animation_GetLastFrame(&gAnubiceFallDownAnim);

    this->animLastFrame = lastFrame;
    OoT_Animation_Change(&this->skelAnime, &gAnubiceFallDownAnim, 1.0f, 0.0f, lastFrame, ANIMMODE_ONCE, -20.0f);

    this->isFallingOver = false;
    this->fallTargetPitch = 0;
    this->deathTimer = 20;
    this->actor.velocity.x = this->actor.velocity.z = 0.0f;
    this->actor.gravity = -1.0f;

    if (OoT_BgCheck_SphVsFirstPoly(&play->colCtx, &this->fireballPos, 70.0f)) {
        this->isFallingOver = true;
        this->fallTargetPitch = this->actor.shape.rot.x - 0x7F00;
    }

    this->actionFunc = EnAnubice_Die;
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
}

void EnAnubice_Die(EnAnubice* this, PlayState* play) {
    f32 curFrame;
    f32 rotX;
    Vec3f fireEffectInitialPos = { 0.0f, 0.0f, 0.0f };
    Vec3f fireEffectPos = { 0.0f, 0.0f, 0.0f };
    s32 pad;

    OoT_SkelAnime_Update(&this->skelAnime);
    OoT_Math_ApproachZeroF(&this->actor.shape.shadowScale, 0.4f, 0.25f);

    if (this->isFallingOver) {
        OoT_Math_SmoothStepToS(&this->actor.shape.rot.y, this->fallTargetPitch, 1, 10000, 0);
        if (fabsf(this->actor.shape.rot.y - this->fallTargetPitch) < 100.0f) {
            this->isFallingOver = false;
        }
    }

    curFrame = this->skelAnime.curFrame;
    rotX = curFrame * -3000.0f;
    rotX = CLAMP_MIN(rotX, -11000.0f);

    Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_NEW);
    Matrix_RotateX(BINANG_TO_RAD(rotX), MTXMODE_APPLY);
    fireEffectInitialPos.y = OoT_Rand_CenteredFloat(10.0f) + 30.0f;
    OoT_Matrix_MultVec3f(&fireEffectInitialPos, &fireEffectPos);
    fireEffectPos.x += this->actor.world.pos.x + OoT_Rand_CenteredFloat(40.0f);
    fireEffectPos.y += this->actor.world.pos.y + OoT_Rand_CenteredFloat(40.0f);
    fireEffectPos.z += this->actor.world.pos.z + OoT_Rand_CenteredFloat(30.0f);
    OoT_Actor_SetColorFilter(&this->actor, 0x4000, 128, 0, 8);
    OoT_EffectSsEnFire_SpawnVec3f(play, &this->actor, &fireEffectPos, 100, 0, 0, -1);

    if ((this->animLastFrame <= curFrame) && (this->actor.bgCheckFlags & 1)) {
        OoT_Math_ApproachF(&this->actor.shape.yOffset, -4230.0f, 0.5f, 300.0f);
        if (this->actor.shape.yOffset < -2000.0f) {
            OoT_Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xC0);
            OoT_Actor_Kill(&this->actor);
        }
    }
}

void EnAnubice_Update(Actor* thisx, PlayState* play) {
    f32 OoT_zero;
    BgHidanCurtain* flameCircle;
    s32 i;
    Vec3f sp48;
    Vec3f sp3C;
    EnAnubice* this = (EnAnubice*)thisx;

    if ((this->actionFunc != EnAnubice_SetupDie) && (this->actionFunc != EnAnubice_Die) &&
        (this->actor.shape.yOffset == 0.0f)) {
        EnAnubice_Hover(this, play);
        for (i = 0; i < ARRAY_COUNT(this->flameCircles); i++) {
            flameCircle = this->flameCircles[i];

            if ((flameCircle != NULL) && (fabsf(flameCircle->actor.world.pos.x - this->actor.world.pos.x) < 60.0f) &&
                (fabsf(this->flameCircles[i]->actor.world.pos.z - this->actor.world.pos.z) < 60.0f) &&
                (flameCircle->timer != 0)) {
                OoT_Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_PROP);
                this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
                OoT_Enemy_StartFinishingBlow(play, &this->actor);
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_ANUBIS_DEAD);
                this->actionFunc = EnAnubice_SetupDie;
                return;
            }
        }

        if (this->collider.base.acFlags & AC_HIT) {
            this->collider.base.acFlags &= ~AC_HIT;
            if (this->actor.colChkInfo.damageEffect == ANUBICE_DMGEFF_FIRE) {
                OoT_Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_PROP);
                this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
                OoT_Enemy_StartFinishingBlow(play, &this->actor);
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_ANUBIS_DEAD);
                this->actionFunc = EnAnubice_SetupDie;
                return;
            }

            if (!this->isKnockedback) {
                this->knockbackTimer = 10;
                this->isKnockedback = true;

                sp48.x = 0.0f;
                sp48.y = 0.0f;
                sp48.z = -10.0f;
                sp3C.x = 0.0f;
                sp3C.y = 0.0f;
                sp3C.z = 0.0f;

                Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_NEW);
                OoT_Matrix_MultVec3f(&sp48, &sp3C);

                this->actor.velocity.x = sp3C.x;
                this->actor.velocity.z = sp3C.z;
                this->knockbackRecoveryVelocity.x = -sp3C.x;
                this->knockbackRecoveryVelocity.z = -sp3C.z;

                Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_CUTBODY);
            }
        }

        if (this->isKnockedback) {
            this->actor.shape.rot.y += 6500;
            OoT_Math_ApproachF(&this->actor.velocity.x, this->knockbackRecoveryVelocity.x, 0.3f, 1.0f);
            OoT_Math_ApproachF(&this->actor.velocity.z, this->knockbackRecoveryVelocity.z, 0.3f, 1.0f);

            OoT_zero = 0.0f;
            if (OoT_zero) {}

            if (this->knockbackTimer == 0) {
                this->actor.velocity.x = this->actor.velocity.z = 0.0f;
                this->knockbackRecoveryVelocity.x = this->knockbackRecoveryVelocity.z = 0.0f;
                this->isKnockedback = false;
            }
        }
    }

    this->timeAlive++;

    if (this->knockbackTimer != 0) {
        this->knockbackTimer--;
    }

    if (this->deathTimer != 0) {
        this->deathTimer--;
    }

    this->actionFunc(this, play);

    this->actor.velocity.y += this->actor.gravity;
    OoT_Actor_UpdatePos(&this->actor);

    if (!this->isLinkOutOfRange) {
        OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 5.0f, 5.0f, 10.0f, 0x1D);
    } else {
        OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 5.0f, 5.0f, 10.0f, 0x1C);
    }

    if ((this->actionFunc != EnAnubice_SetupDie) && (this->actionFunc != EnAnubice_Die)) {
        OoT_Actor_SetFocus(&this->actor, this->focusHeightOffset);
        OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
        OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);

        if (!this->isKnockedback && (this->actor.shape.yOffset == 0.0f)) {
            OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }
    }
}

s32 EnAnubice_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnAnubice* this = (EnAnubice*)thisx;

    if (limbIndex == ANUBICE_LIMB_HEAD) {
        rot->z += this->headRot;
    }

    return false;
}

void EnAnubice_PostLimbDraw(struct PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    EnAnubice* this = (EnAnubice*)thisx;
    Vec3f pos = { 0.0f, 0.0f, 0.0f };

    if (limbIndex == ANUBICE_LIMB_HEAD) {
        OPEN_DISPS(play->state.gfxCtx);

        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gAnubiceEyesDL);
        OoT_Matrix_MultVec3f(&pos, &this->fireballPos);

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void EnAnubice_Draw(Actor* thisx, PlayState* play) {
    EnAnubice* this = (EnAnubice*)thisx;

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, EnAnubice_OverrideLimbDraw, EnAnubice_PostLimbDraw, this);
}
