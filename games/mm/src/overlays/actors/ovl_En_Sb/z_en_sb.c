/*
 * File: z_en_sb.c
 * Overlay: ovl_En_Sb
 * Description: Shellblade
 */

#include "z_en_sb.h"
#include "overlays/actors/ovl_En_Part/z_en_part.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE)

void MM_EnSb_Init(Actor* thisx, PlayState* play);
void MM_EnSb_Destroy(Actor* thisx, PlayState* play);
void MM_EnSb_Update(Actor* thisx, PlayState* play);
void MM_EnSb_Draw(Actor* thisx, PlayState* play);

void MM_EnSb_SetupWaitClosed(EnSb* this);
void EnSb_Idle(EnSb* this, PlayState* play);
void MM_EnSb_Open(EnSb* this, PlayState* play);
void MM_EnSb_WaitOpen(EnSb* this, PlayState* play);
void MM_EnSb_TurnAround(EnSb* this, PlayState* play);
void MM_EnSb_Lunge(EnSb* this, PlayState* play);
void MM_EnSb_Bounce(EnSb* this, PlayState* play);
void EnSb_ReturnToIdle(EnSb* this, PlayState* play);

ActorProfile En_Sb_Profile = {
    /**/ ACTOR_EN_SB,
    /**/ ACTORCAT_ENEMY,
    /**/ FLAGS,
    /**/ OBJECT_SB,
    /**/ sizeof(EnSb),
    /**/ MM_EnSb_Init,
    /**/ MM_EnSb_Destroy,
    /**/ MM_EnSb_Update,
    /**/ MM_EnSb_Draw,
};

static ColliderCylinderInitType1 MM_sCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x04, 0x08 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 30, 40, 0, { 0, 0, 0 } },
};

static DamageTable MM_sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, 0x0),
    /* Deku Stick     */ DMG_ENTRY(0, 0x0),
    /* Horse trample  */ DMG_ENTRY(0, 0x0),
    /* Explosives     */ DMG_ENTRY(1, 0xF),
    /* Zora boomerang */ DMG_ENTRY(1, 0xF),
    /* Normal arrow   */ DMG_ENTRY(1, 0xF),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, 0x0),
    /* Hookshot       */ DMG_ENTRY(1, 0xF),
    /* Goron punch    */ DMG_ENTRY(0, 0x0),
    /* Sword          */ DMG_ENTRY(0, 0x0),
    /* Goron pound    */ DMG_ENTRY(0, 0x0),
    /* Fire arrow     */ DMG_ENTRY(1, 0xF),
    /* Ice arrow      */ DMG_ENTRY(1, 0xF),
    /* Light arrow    */ DMG_ENTRY(1, 0xF),
    /* Goron spikes   */ DMG_ENTRY(0, 0x0),
    /* Deku spin      */ DMG_ENTRY(0, 0x0),
    /* Deku bubble    */ DMG_ENTRY(1, 0xF),
    /* Deku launch    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, 0x0),
    /* Zora barrier   */ DMG_ENTRY(1, 0xF),
    /* Normal shield  */ DMG_ENTRY(0, 0x0),
    /* Light ray      */ DMG_ENTRY(0, 0x0),
    /* Thrown object  */ DMG_ENTRY(0, 0x0),
    /* Zora punch     */ DMG_ENTRY(1, 0xF),
    /* Spin attack    */ DMG_ENTRY(0, 0x0),
    /* Sword beam     */ DMG_ENTRY(0, 0x0),
    /* Normal Roll    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, 0x0),
    /* Unblockable    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, 0x0),
    /* Powder Keg     */ DMG_ENTRY(1, 0xF),
};

static InitChainEntry MM_sInitChain[] = {
    ICHAIN_S8(hintId, TATL_HINT_ID_SHELLBLADE, ICHAIN_CONTINUE),
    ICHAIN_U8(attentionRangeType, ATTENTION_RANGE_2, ICHAIN_CONTINUE),
    ICHAIN_F32(lockOnArrowOffset, 30, ICHAIN_STOP),
};

static Vec3f MM_sFlamePosOffsets[] = {
    { 5.0f, 0.0f, 0.0f },
    { -5.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 5.0f },
    { 0.0f, 0.0f, -5.0f },
};

void MM_EnSb_Init(Actor* thisx, PlayState* play) {
    EnSb* this = (EnSb*)thisx;

    MM_Actor_ProcessInitChain(&this->actor, MM_sInitChain);
    this->actor.colChkInfo.damageTable = &MM_sDamageTable;
    this->actor.colChkInfo.mass = 10;
    this->actor.colChkInfo.health = 2;
    MM_SkelAnime_InitFlex(play, &this->skelAnime, &object_sb_Skel_002BF0, &object_sb_Anim_000194, this->jointTable,
                       this->morphTable, OBJECT_SB_LIMB_MAX);
    MM_Collider_InitCylinder(play, &this->collider);
    MM_Collider_SetCylinderType1(play, &this->collider, &this->actor, &MM_sCylinderInit);
    this->isDead = false;
    this->actor.colChkInfo.mass = 90;
    this->actor.shape.rot.y = 0;
    this->actor.speed = 0.0f;
    this->actor.gravity = -0.35f;
    this->fireCount = 0;
    this->unk_252 = 0;
    this->actor.velocity.y = -1.0f;
    MM_Actor_SetScale(&this->actor, 0.006f);
    MM_EnSb_SetupWaitClosed(this);
}

void MM_EnSb_Destroy(Actor* thisx, PlayState* play) {
    EnSb* this = (EnSb*)thisx;

    MM_Collider_DestroyCylinder(play, &this->collider);
}

void MM_EnSb_SpawnBubbles(PlayState* play, EnSb* this) {
    s32 bubbleCount;

    if (this->actor.depthInWater > 0.0f) {
        for (bubbleCount = 0; bubbleCount < 10; bubbleCount++) {
            MM_EffectSsBubble_Spawn(play, &this->actor.world.pos, 10.0f, 10.0f, 30.0f, 0.25f);
        }
    }
}

void MM_EnSb_SetupWaitClosed(EnSb* this) {
    MM_Animation_Change(&this->skelAnime, &object_sb_Anim_00004C, 1.0f, 0, MM_Animation_GetLastFrame(&object_sb_Anim_00004C),
                     ANIMMODE_ONCE, 0.0f);
    this->state = SHELLBLADE_WAIT_CLOSED;
    this->actionFunc = EnSb_Idle;
}

void MM_EnSb_SetupOpen(EnSb* this) {
    MM_Animation_Change(&this->skelAnime, &object_sb_Anim_000194, 1.0f, 0, MM_Animation_GetLastFrame(&object_sb_Anim_000194),
                     ANIMMODE_ONCE, 0.0f);
    this->state = SHELLBLADE_OPEN;
    this->actionFunc = MM_EnSb_Open;
    //! @bug Incorrect sfx
    //! In OoT, NA_SE_EN_SHELL_MOUTH is the value 0x3849
    //! But in MM, certain sfxIds got reordered this was not updated:
    //! In MM, NA_SE_EN_KUSAMUSHI_VIBE is the old value 0x3849
    //! In MM, NA_SE_EN_SHELL_MOUTH does not exist
    Actor_PlaySfx(&this->actor, NA_SE_EN_KUSAMUSHI_VIBE);
}

void MM_EnSb_SetupWaitOpen(EnSb* this) {
    MM_Animation_Change(&this->skelAnime, &object_sb_Anim_002C8C, 1.0f, 0, MM_Animation_GetLastFrame(&object_sb_Anim_002C8C),
                     ANIMMODE_LOOP, 0.0f);
    this->state = SHELLBLADE_WAIT_OPEN;
    this->actionFunc = MM_EnSb_WaitOpen;
}

void MM_EnSb_SetupLunge(EnSb* this) {
    f32 endFrame = MM_Animation_GetLastFrame(&object_sb_Anim_000124);
    f32 playbackSpeed = this->actor.depthInWater > 0.0f ? 1.0f : 0.0f;

    MM_Animation_Change(&this->skelAnime, &object_sb_Anim_000124, playbackSpeed, 0.0f, endFrame, ANIMMODE_ONCE, 0);
    this->state = SHELLBLADE_LUNGE;
    this->actionFunc = MM_EnSb_Lunge;
    //! @bug Incorrect sfx
    //! In OoT, NA_SE_EN_SHELL_MOUTH is the value 0x3849
    //! But in MM, certain sfxIds got reordered this was not updated:
    //! In MM, NA_SE_EN_KUSAMUSHI_VIBE is the old value 0x3849
    //! In MM, NA_SE_EN_SHELL_MOUTH does not exist
    Actor_PlaySfx(&this->actor, NA_SE_EN_KUSAMUSHI_VIBE);
}

void MM_EnSb_SetupBounce(EnSb* this) {
    MM_Animation_Change(&this->skelAnime, &object_sb_Anim_0000B4, 1.0f, 0, MM_Animation_GetLastFrame(&object_sb_Anim_0000B4),
                     ANIMMODE_ONCE, 0.0f);
    this->state = SHELLBLADE_BOUNCE;
    this->actionFunc = MM_EnSb_Bounce;
}

void EnSb_SetupIdle(EnSb* this, s32 changeSpeed) {
    f32 endFrame = MM_Animation_GetLastFrame(&object_sb_Anim_00004C);

    if (this->state != SHELLBLADE_WAIT_CLOSED) {
        MM_Animation_Change(&this->skelAnime, &object_sb_Anim_00004C, 1.0f, 0, endFrame, ANIMMODE_ONCE, 0.0f);
    }
    this->state = SHELLBLADE_WAIT_CLOSED;
    if (changeSpeed) {
        if (this->actor.depthInWater > 0.0f) {
            this->actor.speed = -5.0f;
            if (this->actor.velocity.y < 0.0f) {
                this->actor.velocity.y = 2.1f;
            }
        } else {
            this->actor.speed = -6.0f;
            if (this->actor.velocity.y < 0.0f) {
                this->actor.velocity.y = 1.4f;
            }
        }
    }
    this->attackTimer = 60;
    this->actionFunc = EnSb_ReturnToIdle;
}

void EnSb_Idle(EnSb* this, PlayState* play) {
    MM_Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0x7D0, 0);
    if ((this->actor.xzDistToPlayer <= 240.0f) && (this->actor.xzDistToPlayer > 0.0f)) {
        MM_EnSb_SetupOpen(this);
    }
}

void MM_EnSb_Open(EnSb* this, PlayState* play) {
    f32 curFrame = this->skelAnime.curFrame;
    f32 endFrame = MM_Animation_GetLastFrame(&object_sb_Anim_000194);

    if (curFrame >= endFrame) {
        this->vulnerableTimer = 20;
        MM_EnSb_SetupWaitOpen(this);
    } else {
        MM_Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0x7D0, 0);
        if ((this->actor.xzDistToPlayer > 240.0f) || (this->actor.xzDistToPlayer <= 40.0f)) {
            this->vulnerableTimer = 0;
            MM_EnSb_SetupWaitClosed(this);
        }
    }
}

void MM_EnSb_WaitOpen(EnSb* this, PlayState* play) {

    MM_Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, 0x7D0, 0);
    if ((this->actor.xzDistToPlayer > 240.0f) || (this->actor.xzDistToPlayer <= 40.0f)) {
        this->vulnerableTimer = 0;
        MM_EnSb_SetupWaitClosed(this);
    }
    if (this->vulnerableTimer > 0) {
        this->vulnerableTimer--;
    } else {
        this->vulnerableTimer = 0;
        this->attackTimer = 0;
        this->yawAngle = this->actor.yawTowardsPlayer;
        this->actionFunc = MM_EnSb_TurnAround;
    }
}

void MM_EnSb_TurnAround(EnSb* this, PlayState* play) {
    s16 invertedYaw = BINANG_ROT180(this->yawAngle);

    MM_Math_SmoothStepToS(&this->actor.shape.rot.y, invertedYaw, 1, 0x1F40, 0xA);
    if (this->actor.shape.rot.y == invertedYaw) {
        this->actor.world.rot.y = this->yawAngle;
        if (this->actor.depthInWater > 0.0f) {
            this->actor.velocity.y = 3.0f;
            this->actor.speed = 5.0f;
            this->actor.gravity = -0.35f;
        } else {
            this->actor.velocity.y = 2.0f;
            this->actor.speed = 6.0f;
            this->actor.gravity = -2.0f;
        }
        MM_EnSb_SpawnBubbles(play, this);
        this->bounceCounter = 3;
        MM_EnSb_SetupLunge(this);
    }
}

void MM_EnSb_Lunge(EnSb* this, PlayState* play) {
    MM_Math_StepToF(&this->actor.speed, 0.0f, 0.2f);
    if ((this->actor.velocity.y <= -0.1f) || (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH)) {
        if (!(this->actor.depthInWater > 0.0f)) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_EYEGOLE_ATTACK);
        }
        this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND_TOUCH;
        MM_EnSb_SetupBounce(this);
    }
}

void MM_EnSb_Bounce(EnSb* this, PlayState* play) {
    s32 pad;
    f32 curFrame = this->skelAnime.curFrame;
    f32 endFrame = MM_Animation_GetLastFrame(&object_sb_Anim_0000B4);

    MM_Math_StepToF(&this->actor.speed, 0.0f, 0.2f);
    if (curFrame == endFrame) {
        if (this->bounceCounter != 0) {
            this->bounceCounter--;
            this->attackTimer = 1;
            if (this->actor.depthInWater > 0.0f) {
                this->actor.velocity.y = 3.0f;
                this->actor.speed = 5.0f;
                this->actor.gravity = -0.35f;
            } else {
                this->actor.velocity.y = 2.0f;
                this->actor.speed = 6.0f;
                this->actor.gravity = -2.0f;
            }
            MM_EnSb_SpawnBubbles(play, this);
            MM_EnSb_SetupLunge(this);
        } else if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
            this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND_TOUCH;
            this->actor.speed = 0.0f;
            this->attackTimer = 1;
            MM_EnSb_SetupWaitClosed(this);
        }
    }
}

void EnSb_ReturnToIdle(EnSb* this, PlayState* play) {
    if (this->attackTimer != 0) {
        this->attackTimer--;
        if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
            this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
            this->actor.speed = 0.0f;
        }
    } else if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        this->actionFunc = EnSb_Idle;
        this->actor.speed = 0.0f;
    }
}

void MM_EnSb_UpdateDamage(EnSb* this, PlayState* play) {
    Vec3f hitPoint;

    if (this->collider.base.acFlags & AC_HIT) {
        s32 hitPlayer = 0;
        this->collider.base.acFlags &= ~AC_HIT;
        if (this->actor.colChkInfo.damageEffect == 0xF) {
            hitPlayer = 0;
            if (this->vulnerableTimer != 0) {
                MM_Actor_ApplyDamage(&this->actor);
                MM_Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_XLU, 80);
                hitPlayer = 1;
            }
        }
        if (hitPlayer) {
            this->unk_252 = 0;
            if ((this->actor.draw != NULL) && !this->isDrawn) {
                this->isDrawn = true;
            }
            this->isDead = true;
            MM_Enemy_StartFinishingBlow(play, &this->actor);
            //! @bug Incorrect sfx
            //! In OoT, NA_SE_EN_SHELL_DEAD is the value 0x384A
            //! But in MM, certain sfxIds got reordered this was not updated:
            //! In MM, NA_SE_EN_BEE_FLY is the old value 0x384A
            //! In MM, NA_SE_EN_SHELL_DEAD does not exist
            MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EN_BEE_FLY);
            return;
        }
        hitPoint.x = this->collider.elem.acDmgInfo.hitPos.x;
        hitPoint.y = this->collider.elem.acDmgInfo.hitPos.y;
        hitPoint.z = this->collider.elem.acDmgInfo.hitPos.z;
        MM_CollisionCheck_SpawnShieldParticlesMetal2(play, &hitPoint);
        return;
    }
    if (this->collider.base.atFlags & AT_HIT) {
        EnSb_SetupIdle(this, 1);
    }
}

void MM_EnSb_Update(Actor* thisx, PlayState* play) {
    s32 pad;
    EnSb* this = (EnSb*)thisx;
    Player* player = GET_PLAYER(play);

    if (this->isDead) {
        if (this->actor.depthInWater > 0.0f) {
            this->actor.params = 4;
        } else {
            this->actor.params = 1;
        }
        MM_Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x80);
        MM_Actor_Kill(&this->actor);
        return;
    }

    MM_Actor_SetFocus(&this->actor, 20.0f);
    Actor_MoveWithGravity(&this->actor);
    this->actionFunc(this, play);
    MM_Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 25.0f, 20.0f, UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4);
    MM_EnSb_UpdateDamage(this, play);
    if (player->stateFlags1 & PLAYER_STATE1_8000000) {
        MM_Collider_UpdateCylinder(&this->actor, &this->collider);
        if (this->vulnerableTimer == 0) {
            MM_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        }
        MM_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    MM_SkelAnime_Update(&this->skelAnime);
}

void MM_EnSb_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    s8 partParams;
    EnSb* this = (EnSb*)thisx;

    if (this->isDrawn) {
        if (limbIndex <= OBJECT_SB_LIMB_06) {
            partParams = (this->actor.depthInWater > 0) ? ENPART_PARAMS(ENPART_TYPE_4) : ENPART_PARAMS(ENPART_TYPE_1);
            Actor_SpawnBodyParts(thisx, play, partParams, dList);
        }
        if (limbIndex == OBJECT_SB_LIMB_06) {
            this->isDrawn = false;
            this->actor.draw = NULL;
        }
    }
}

void MM_EnSb_Draw(Actor* thisx, PlayState* play) {
    EnSb* this = (EnSb*)thisx;
    Vec3f flamePos;
    Vec3f* offset;
    s16 fireDecr;

    func_800B8050(&this->actor, play, 1);
    MM_SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          MM_EnSb_PostLimbDraw, &this->actor);
    if (this->fireCount != 0) {
        this->actor.colorFilterTimer++;
        fireDecr = this->fireCount - 1;
        if (!(fireDecr & 1)) {
            offset = &MM_sFlamePosOffsets[fireDecr & 3];
            flamePos.x = MM_Rand_CenteredFloat(5.0f) + (this->actor.world.pos.x + offset->x);
            flamePos.y = MM_Rand_CenteredFloat(5.0f) + (this->actor.world.pos.y + offset->y);
            flamePos.z = MM_Rand_CenteredFloat(5.0f) + (this->actor.world.pos.z + offset->z);
            MM_EffectSsEnFire_SpawnVec3f(play, &this->actor, &flamePos, 100, 0, 0, -1);
        }
    }
}
