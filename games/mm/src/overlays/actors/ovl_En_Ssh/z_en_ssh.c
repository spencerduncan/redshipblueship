/*
 * File: z_en_ssh.c
 * Overlay: ovl_En_Ssh
 * Description: Cursed Man (Swamp Spider House)
 */

#include "z_en_ssh.h"
#include "objects/object_st/object_st.h"
#include "2s2h/GameInteractor/GameInteractor.h"

#define FLAGS                                                                                 \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_DRAW_CULLING_DISABLED)

void MM_EnSsh_Init(Actor* thisx, PlayState* play);
void MM_EnSsh_Destroy(Actor* thisx, PlayState* play);
void MM_EnSsh_Update(Actor* thisx, PlayState* play);
void MM_EnSsh_Draw(Actor* thisx, PlayState* play);

void MM_EnSsh_Wait(EnSsh* this, PlayState* play);
void MM_EnSsh_Idle(EnSsh* this, PlayState* play);
void MM_EnSsh_Land(EnSsh* this, PlayState* play);
void MM_EnSsh_Drop(EnSsh* this, PlayState* play);
void MM_EnSsh_Return(EnSsh* this, PlayState* play);
void MM_EnSsh_Start(EnSsh* this, PlayState* play);

extern AnimationHeader D_06000304;

ActorProfile En_Ssh_Profile = {
    /**/ ACTOR_EN_SSH,
    /**/ ACTORCAT_NPC,
    /**/ FLAGS,
    /**/ OBJECT_SSH,
    /**/ sizeof(EnSsh),
    /**/ MM_EnSsh_Init,
    /**/ MM_EnSsh_Destroy,
    /**/ MM_EnSsh_Update,
    /**/ MM_EnSsh_Draw,
};

static ColliderCylinderInit MM_sCylinderInit1 = {
    {
        COL_MATERIAL_HIT6,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0x00000000, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_NONE,
    },
    { 32, 50, -24, { 0, 0, 0 } },
};

static CollisionCheckInfoInit2 MM_sColChkInfoInit = { 1, 0, 0, 0, MASS_IMMOVABLE };

static ColliderCylinderInit MM_sCylinderInit2 = {
    {
        COL_MATERIAL_HIT6,
        AT_NONE,
        AC_NONE,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0x00000000, 0x00, 0x00 },
        ATELEM_NONE | ATELEM_SFX_NORMAL,
        ACELEM_NONE,
        OCELEM_ON,
    },
    { 20, 60, -30, { 0, 0, 0 } },
};

static ColliderJntSphElementInit MM_sJntSphElementsInit[1] = {
    {
        {
            ELEM_MATERIAL_UNK0,
            { 0xF7CFFFFF, 0x00, 0x04 },
            { 0x00000000, 0x00, 0x00 },
            ATELEM_ON | ATELEM_SFX_NORMAL,
            ACELEM_NONE,
            OCELEM_ON,
        },
        { 1, { { 0, -240, 0 }, 28 }, 100 },
    },
};

static ColliderJntSphInit MM_sJntSphInit = {
    {
        COL_MATERIAL_HIT6,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    ARRAY_COUNT(MM_sJntSphElementsInit),
    MM_sJntSphElementsInit,
};

void MM_EnSsh_SetupAction(EnSsh* this, EnSshActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void MM_EnSsh_SpawnShockwave(EnSsh* this, PlayState* play) {
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    Vec3f pos;

    pos.x = this->actor.world.pos.x;
    pos.y = this->actor.floorHeight;
    pos.z = this->actor.world.pos.z;
    MM_EffectSsBlast_SpawnWhiteCustomScale(play, &pos, &zeroVec, &zeroVec, 100, 220, 8);
}

s32 MM_EnSsh_CreateBlureEffect(PlayState* play) {
    EffectBlureInit1 blureInit;
    u8 sP1StartColor[4] = { 255, 255, 255, 75 };
    u8 sP2StartColor[4] = { 255, 255, 255, 75 };
    u8 sP1EndColor[4] = { 255, 255, 255, 0 };
    u8 sP2EndColor[4] = { 255, 255, 255, 0 };
    s32 i;
    s32 blureIdx;

    for (i = 0; i < EFFECT_BLURE_COLOR_COUNT; i++) {
        blureInit.p1StartColor[i] = sP1StartColor[i];
        blureInit.p2StartColor[i] = sP2StartColor[i];
        blureInit.p1EndColor[i] = sP1EndColor[i];
        blureInit.p2EndColor[i] = sP2EndColor[i];
    }
    blureInit.elemDuration = 6;
    blureInit.unkFlag = false;
    blureInit.calcMode = 3;

    MM_Effect_Add(play, &blureIdx, EFFECT_BLURE1, 0, 0, &blureInit);
    return blureIdx;
}

s32 MM_EnSsh_CheckCeilingPos(EnSsh* this, PlayState* play) {
    CollisionPoly* poly;
    s32 bgId;
    Vec3f posB;

    posB.x = this->actor.world.pos.x;
    posB.y = this->actor.world.pos.y + 1000.0f;
    posB.z = this->actor.world.pos.z;
    if (!MM_BgCheck_EntityLineTest1(&play->colCtx, &this->actor.world.pos, &posB, &this->ceilingPos, &poly, false, false,
                                 true, true, &bgId)) {
        return false;
    }
    return true;
}

void MM_EnSsh_AddBlureVertex(EnSsh* this) {
    Vec3f p1Base = { 834.0f, 834.0f, 0.0f };
    Vec3f p2Base = { 834.0f, -584.0f, 0.0f };
    Vec3f p1;
    Vec3f p2;

    p1Base.x *= this->colliderScale;
    p1Base.y *= this->colliderScale;
    p1Base.z *= this->colliderScale;

    p2Base.x *= this->colliderScale;
    p2Base.y *= this->colliderScale;
    p2Base.z *= this->colliderScale;

    MM_Matrix_Push();
    MM_Matrix_MultVec3f(&p1Base, &p1);
    MM_Matrix_MultVec3f(&p2Base, &p2);
    MM_Matrix_Pop();
    MM_EffectBlure_AddVertex(MM_Effect_GetByIndex(this->blureIdx), &p1, &p2);
}

void MM_EnSsh_AddBlureSpace(EnSsh* this) {
    MM_EffectBlure_AddSpace(MM_Effect_GetByIndex(this->blureIdx));
}

void MM_EnSsh_InitColliders(EnSsh* this, PlayState* play) {
    ColliderCylinderInit* cylinders[] = {
        &MM_sCylinderInit1, &MM_sCylinderInit1, &MM_sCylinderInit1, &MM_sCylinderInit2, &MM_sCylinderInit2, &MM_sCylinderInit2,
    };
    s32 i;
    s32 pad;

    for (i = 0; i < ARRAY_COUNT(this->collider1); i++) {
        Collider_InitAndSetCylinder(play, &this->collider1[i], &this->actor, cylinders[i]);
    }

    this->collider1[0].elem.acDmgInfo.dmgFlags = 0x38A9;
    this->collider1[1].elem.acDmgInfo.dmgFlags = ~0x83038A9;
    this->collider1[2].base.colMaterial = COL_MATERIAL_METAL;
    this->collider1[2].elem.acElemFlags = (ACELEM_NO_AT_INFO | ACELEM_HOOKABLE | ACELEM_ON);
    this->collider1[2].elem.elemMaterial = ELEM_MATERIAL_UNK2;
    this->collider1[2].elem.acDmgInfo.dmgFlags = ~0x83038A9;

    MM_CollisionCheck_SetInfo2(&this->actor.colChkInfo, MM_DamageTable_Get(2), &MM_sColChkInfoInit);
    MM_Collider_InitJntSph(play, &this->collider2);
    MM_Collider_SetJntSph(play, &this->collider2, &this->actor, &MM_sJntSphInit, this->collider2Elements);
}

typedef enum EnSshAnimation {
    /* 0x0 */ SSH_ANIM_0, // Unused animation. Possibly being knocked back?
    /* 0x1 */ SSH_ANIM_UP,
    /* 0x2 */ SSH_ANIM_WAIT,
    /* 0x3 */ SSH_ANIM_LAND,
    /* 0x4 */ SSH_ANIM_DROP,
    /* 0x5 */ SSH_ANIM_5, // Slower version of ANIM_DROP
    /* 0x6 */ SSH_ANIM_6, // Faster repeating version of
    /* 0x7 */ SSH_ANIM_MAX
} EnSshAnimation;

f32 EnSsh_ChangeAnim(EnSsh* this, s32 animIndex) {
    AnimationHeader* MM_sAnimations[SSH_ANIM_MAX] = {
        &object_ssh_Anim_006D78, // SSH_ANIM_0
        &object_ssh_Anim_001494, // SSH_ANIM_UP
        &object_ssh_Anim_001494, // SSH_ANIM_WAIT
        &object_ssh_Anim_006788, // SSH_ANIM_LAND
        &object_ssh_Anim_001494, // SSH_ANIM_DROP
        &object_ssh_Anim_001494, // SSH_ANIM_5
        &object_ssh_Anim_006D78, // SSH_ANIM_6
    };
    f32 sPlaySpeeds[SSH_ANIM_MAX] = {
        1.0f, // SSH_ANIM_0
        4.0f, // SSH_ANIM_UP
        1.0f, // SSH_ANIM_WAIT
        1.0f, // SSH_ANIM_LAND
        8.0f, // SSH_ANIM_DROP
        6.0f, // SSH_ANIM_5
        2.0f, // SSH_ANIM_6
    };
    u8 MM_sAnimationModes[SSH_ANIM_MAX] = {
        ANIMMODE_ONCE_INTERP, // SSH_ANIM_0
        ANIMMODE_ONCE_INTERP, // SSH_ANIM_UP
        ANIMMODE_LOOP_INTERP, // SSH_ANIM_WAIT
        ANIMMODE_ONCE_INTERP, // SSH_ANIM_LAND
        ANIMMODE_LOOP_INTERP, // SSH_ANIM_DROP
        ANIMMODE_LOOP_INTERP, // SSH_ANIM_5
        ANIMMODE_LOOP_INTERP, // SSH_ANIM_6
    };
    f32 endFrame = MM_Animation_GetLastFrame(MM_sAnimations[animIndex]);
    s32 pad;

    MM_Animation_Change(&this->skelAnime, MM_sAnimations[animIndex], sPlaySpeeds[animIndex], 0.0f, endFrame,
                     MM_sAnimationModes[animIndex], -6.0f);
    return endFrame;
}

void MM_EnSsh_SetWaitAnimation(EnSsh* this) {
    EnSsh_ChangeAnim(this, SSH_ANIM_WAIT);
}

void MM_EnSsh_SetReturnAnimation(EnSsh* this) {
    Actor_PlaySfx(&this->actor, NA_SE_EN_STALTU_UP);
    EnSsh_ChangeAnim(this, SSH_ANIM_UP);
}

void MM_EnSsh_SetLandAnimation(EnSsh* this) {
    this->actor.world.pos.y = this->floorHeightOffset + this->actor.floorHeight;
    this->animTimer = EnSsh_ChangeAnim(this, SSH_ANIM_LAND);
}

void MM_EnSsh_SetDropAnimation(EnSsh* this) {
    if (this->unkTimer == 0) {
        this->animTimer = EnSsh_ChangeAnim(this, SSH_ANIM_DROP);
    }
    this->actor.velocity.y = -10.0f;
}

void MM_EnSsh_SetStunned(EnSsh* this) {
    if (this->stunTimer == 0) {
        this->stateFlags |= SSH_STATE_ATTACKED;
        this->stunTimer = 120;
        this->actor.colorFilterTimer = 0;
    }
}

void MM_EnSsh_SetColliderScale(EnSsh* this, f32 arg1, f32 arg2) {
    s32 i;
    f32 radius = this->collider2.elements[0].dim.modelSphere.radius;
    f32 height;
    f32 yShift;

    radius *= arg1;
    this->collider2.elements[0].dim.modelSphere.radius = radius;

    for (i = 0; i < ARRAY_COUNT(this->collider1); i++) {
        yShift = this->collider1[i].dim.yShift;
        radius = this->collider1[i].dim.radius;
        height = this->collider1[i].dim.height;

        height *= arg1;
        yShift *= arg1;
        radius *= arg1 * arg2;

        this->collider1[i].dim.yShift = yShift;
        this->collider1[i].dim.radius = radius;
        this->collider1[i].dim.height = height;
    }

    MM_Actor_SetScale(&this->actor, 0.04f * arg1);
    this->floorHeightOffset = 60.0f * arg1;
    this->colliderScale = arg1 * 1.5f;
}

s32 MM_EnSsh_Damaged(EnSsh* this) {
    if ((this->stunTimer == 120) && (this->stateFlags & SSH_STATE_STUNNED)) {
        MM_Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 200, COLORFILTER_BUFFLAG_OPA, this->stunTimer);
    }

    if (DECR(this->stunTimer) != 0) {
        MM_Math_SmoothStepToS(&this->maxTurnRate, 0x2710, 10, 0x3E8, 1);
        return false;
    }

    this->stunTimer = 0;
    this->stateFlags &= ~SSH_STATE_STUNNED;
    this->spinTimer = 0;
    if (this->swayTimer == 0) {
        this->spinTimer = 30;
    }

    Actor_PlaySfx(&this->actor, NA_SE_EN_STALTU_ROLL);
    Actor_PlaySfx(&this->actor, NA_SE_VO_ST_ATTACK);

    return true;
}

void MM_EnSsh_Turn(EnSsh* this, PlayState* play) {
    if (this->hitTimer != 0) {
        this->hitTimer--;
    }

    if (DECR(this->spinTimer) != 0) {
        this->actor.world.rot.y += TRUNCF_BINANG(0x2710 * (this->spinTimer / 30.0f));
    } else if ((this->swayTimer == 0) && (this->stunTimer == 0)) {
        MM_Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 4, 0x2710, 1);
    }
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

void MM_EnSsh_Stunned(EnSsh* this, PlayState* play) {
    if ((this->swayTimer == 0) && (this->stunTimer == 0)) {
        MM_Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer ^ 0x8000, 4, this->maxTurnRate, 1);
    }

    this->actor.shape.rot.y = this->actor.world.rot.y;

    if (this->stunTimer < 30) {
        if (this->stunTimer & 1) {
            this->actor.shape.rot.y += 0x7D0;
        } else {
            this->actor.shape.rot.y -= 0x7D0;
        }
    }
}

void MM_EnSsh_UpdateYaw(EnSsh* this, PlayState* play) {
    if (this->stunTimer != 0) {
        MM_EnSsh_Stunned(this, play);
    } else {
        MM_EnSsh_Turn(this, play);
    }
}

void MM_EnSsh_Bob(EnSsh* this, PlayState* play) {
    f32 bobVel = 0.5f;

    if (play->state.frames & 8) {
        bobVel *= -1.0f;
    }
    MM_Math_SmoothStepToF(&this->actor.velocity.y, bobVel, 0.4f, 1000.0f, 0.0f);
}

s32 MM_EnSsh_IsCloseToLink(EnSsh* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 yDist;

    if (this->stateFlags & SSH_STATE_GROUND_START) {
        return true;
    }

    if (this->unkTimer != 0) {
        return true;
    }

    if (this->swayTimer != 0) {
        return true;
    }

    if (this->animTimer != 0) {
        return true;
    }

    if (this->actor.xzDistToPlayer > 160.0f) {
        return false;
    }

    yDist = this->actor.world.pos.y - player->actor.world.pos.y;
    if ((yDist < 0.0f) || (yDist > 400.0f)) {
        return false;
    }

    if (player->actor.world.pos.y < this->actor.floorHeight) {
        return false;
    }

    return true;
}

s32 MM_EnSsh_IsCloseToHome(EnSsh* this) {
    f32 velocityY = this->actor.velocity.y;
    f32 nextY = this->actor.world.pos.y + velocityY * 2.0f;

    if (this->actor.home.pos.y <= nextY) {
        return true;
    }
    return false;
}

s32 MM_EnSsh_IsCloseToGround(EnSsh* this) {
    f32 velocityY = this->actor.velocity.y;
    f32 nextY = this->actor.world.pos.y + velocityY * 2.0f;

    if ((nextY - this->actor.floorHeight) <= this->floorHeightOffset) {
        return true;
    }
    return false;
}

void MM_EnSsh_Sway(EnSsh* this) {
    Vec3f swayVecBase;
    Vec3f swayVec;
    f32 temp_f20;
    s16 swayAngle;

    if (this->swayTimer != 0) {
        this->swayAngle += 1600;
        this->swayTimer--;
        if (this->swayTimer == 0) {
            this->swayAngle = 0;
        }

        temp_f20 = (this->swayTimer * (1.0f / 6));
        swayAngle = (f32)DEG_TO_BINANG_ALT3(temp_f20) * MM_Math_SinS(this->swayAngle);
        temp_f20 = this->actor.world.pos.y - this->ceilingPos.y;

        swayVecBase.x = MM_Math_SinS(swayAngle) * temp_f20;
        swayVecBase.y = MM_Math_CosS(swayAngle) * temp_f20;
        swayVecBase.z = 0.0f;

        MM_Matrix_Push();
        MM_Matrix_Translate(this->ceilingPos.x, this->ceilingPos.y, this->ceilingPos.z, MTXMODE_NEW);
        Matrix_RotateYF(BINANG_TO_RAD(this->actor.world.rot.y), MTXMODE_APPLY);
        MM_Matrix_MultVec3f(&swayVecBase, &swayVec);
        MM_Matrix_Pop();

        this->actor.shape.rot.z = -(swayAngle * 2);
        this->actor.world.pos.x = swayVec.x;
        this->actor.world.pos.z = swayVec.z;
    }
}

void MM_EnSsh_CheckBodyStickHit(EnSsh* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    ColliderElement* elem = &this->collider1[0].elem;

    if (player->unk_B28 != 0) {
        elem->acDmgInfo.dmgFlags |= 2;
        this->collider1[1].elem.acDmgInfo.dmgFlags &= ~2;
        this->collider1[2].elem.acDmgInfo.dmgFlags &= ~2;
    } else {
        elem->acDmgInfo.dmgFlags &= ~2;
        this->collider1[1].elem.acDmgInfo.dmgFlags |= 2;
        this->collider1[2].elem.acDmgInfo.dmgFlags |= 2;
    }
}

s32 MM_EnSsh_CheckHitPlayer(EnSsh* this, PlayState* play) {
    s32 i;
    s32 hit = false;

    if ((this->hitCount == 0) && (this->spinTimer == 0)) {
        return false;
    }

    for (i = 0; i < ARRAY_COUNT(this->collider1) / 2; i++) {
        if (this->collider1[3 + i].base.ocFlags2 & OC2_HIT_PLAYER) {
            this->collider1[3 + i].base.ocFlags2 &= ~OC2_HIT_PLAYER;
            hit = true;
        }
    }

    if (!hit) {
        return false;
    }

    this->hitTimer = 30;
    if (this->swayTimer == 0) {
        this->spinTimer = this->hitTimer;
    }

    Actor_PlaySfx(&this->actor, NA_SE_EN_STALTU_ROLL);
    Actor_PlaySfx(&this->actor, NA_SE_VO_ST_ATTACK);

    play->damagePlayer(play, -8);

    func_800B8D98(play, &this->actor, 4.0f, this->actor.yawTowardsPlayer, 6.0f);
    this->hitCount--;
    return true;
}

s32 MM_EnSsh_CheckHitFront(EnSsh* this) {
    u32 acFlags;

    if (this->collider1[2].base.acFlags) {}
    acFlags = this->collider1[2].base.acFlags;

    if (!!(acFlags & AC_HIT) == 0) {
        return false;
    }

    this->collider1[2].base.acFlags &= ~AC_HIT;
    this->invincibilityTimer = 8;

    if ((this->swayTimer == 0) && (this->hitTimer == 0) && (this->stunTimer == 0)) {
        this->swayTimer = 60;
    }

    return true;
}

s32 MM_EnSsh_CheckHitBack(EnSsh* this, PlayState* play) {
    ColliderCylinder* collider = &this->collider1[0];
    s32 hit = false;

    if (collider->base.acFlags & AC_HIT) {
        collider->base.acFlags &= ~AC_HIT;
        hit = true;
    }

    collider = &this->collider1[1];
    if (collider->base.acFlags & AC_HIT) {
        collider->base.acFlags &= ~AC_HIT;
        hit = true;
    }

    if (!hit) {
        return false;
    }

    this->invincibilityTimer = 8;
    if (this->hitCount <= 0) {
        this->hitCount++;
    }

    if (this->stunTimer == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
        Actor_PlaySfx(&this->actor, NA_SE_VO_ST_DAMAGE);
    }

    MM_EnSsh_SetStunned(this);
    this->stateFlags |= SSH_STATE_STUNNED;
    return false;
}

s32 MM_EnSsh_CollisionCheck(EnSsh* this, PlayState* play) {
    if (this->stunTimer == 0) {
        MM_EnSsh_CheckHitPlayer(this, play);
    }

    if (MM_EnSsh_CheckHitFront(this)) {
        return false;
    }

    if (play->actorCtx.unk2 != 0) {
        this->invincibilityTimer = 8;
        if (this->stunTimer == 0) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
            Actor_PlaySfx(&this->actor, NA_SE_VO_ST_DAMAGE);
        }
        MM_EnSsh_SetStunned(this);
        this->stateFlags |= SSH_STATE_STUNNED;
        return false;
    }

    return MM_EnSsh_CheckHitBack(this, play);
}

void MM_EnSsh_SetBodyCylinderAC(EnSsh* this, PlayState* play) {
    MM_Collider_UpdateCylinder(&this->actor, &this->collider1[0]);
    MM_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider1[0].base);
}

void MM_EnSsh_SetLegsCylinderAC(EnSsh* this, PlayState* play) {
    s16 angleTowardsLink = ABS_ALT((s16)(this->actor.yawTowardsPlayer - this->actor.shape.rot.y));

    if (angleTowardsLink < (90 * (0x10000 / 360))) {
        MM_Collider_UpdateCylinder(&this->actor, &this->collider1[2]);
        MM_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider1[2].base);
    } else {
        MM_Collider_UpdateCylinder(&this->actor, &this->collider1[1]);
        MM_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider1[1].base);
    }
}

s32 MM_EnSsh_SetCylinderOC(EnSsh* this, PlayState* play) {
    Vec3f colliderOffsets[] = { { 40.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { -40.0f, 0.0f, 0.0f } };
    Vec3f colliderPos;
    s32 i;

    for (i = 0; i < ARRAY_COUNT(this->collider1) / 2; i++) {
        colliderPos = this->actor.world.pos;

        colliderOffsets[i].x *= this->colliderScale;
        colliderOffsets[i].y *= this->colliderScale;
        colliderOffsets[i].z *= this->colliderScale;

        MM_Matrix_Push();
        MM_Matrix_Translate(colliderPos.x, colliderPos.y, colliderPos.z, MTXMODE_NEW);
        Matrix_RotateYF(BINANG_TO_RAD_ALT(this->initialYaw), MTXMODE_APPLY);
        MM_Matrix_MultVec3f(&colliderOffsets[i], &colliderPos);
        MM_Matrix_Pop();

        this->collider1[3 + i].dim.pos.x = colliderPos.x;
        this->collider1[3 + i].dim.pos.y = colliderPos.y;
        this->collider1[3 + i].dim.pos.z = colliderPos.z;

        MM_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider1[3 + i].base);
    }

    return true;
}

void MM_EnSsh_SetColliders(EnSsh* this, PlayState* play) {
    if (this->actor.colChkInfo.health == 0) {
        MM_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider2.base);
        MM_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider2.base);
        return;
    }

    if (this->hitTimer == 0) {
        MM_EnSsh_SetCylinderOC(this, play);
    }

    if (DECR(this->invincibilityTimer) == 0) {
        MM_EnSsh_SetBodyCylinderAC(this, play);
        MM_EnSsh_SetLegsCylinderAC(this, play);
    }
}

void MM_EnSsh_Init(Actor* thisx, PlayState* play) {
    //! @bug: object_st_Anim_000304 is similar if not idential to object_ssh_Anim_001494.
    //! They also shared the same offset into their respective object files in OoT.
    //! However since object_ssh is the one loaded, this ends up reading garbage data from within object_ssh_Tex_000190.
    // 2S2H [Port] - Due to the nature of the port, this ends up reading the correct data anyway
    f32 endFrame = MM_Animation_GetLastFrame(&object_st_Anim_000304);
    s32 pad;
    EnSsh* this = (EnSsh*)thisx;

    MM_ActorShape_Init(&this->actor.shape, 0.0f, MM_ActorShadow_DrawCircle, 30.0f);
    MM_SkelAnime_Init(play, &this->skelAnime, &object_ssh_Skel_006470, NULL, this->jointTable, this->morphTable,
                   OBJECT_SSH_LIMB_MAX);
    MM_Animation_Change(&this->skelAnime, &object_ssh_Anim_001494, 1.0f, 0.0f, endFrame, ANIMMODE_LOOP_INTERP, 0.0f);
    this->blureIdx = MM_EnSsh_CreateBlureEffect(play);
    MM_EnSsh_InitColliders(this, play);
    this->stateFlags = 0;
    this->hitCount = 0;
    MM_EnSsh_CheckCeilingPos(this, play);

    if (!ENSSH_IS_CHILD(&this->actor)) {
        this->stateFlags |= SSH_STATE_FATHER;
    }

    if (!(this->stateFlags & SSH_STATE_FATHER)) {
        MM_EnSsh_SetColliderScale(this, 0.5f, 1.0f);
    } else {
        MM_EnSsh_SetColliderScale(this, 0.75f, 1.0f);
    }

    this->actor.gravity = 0.0f;
    this->initialYaw = this->actor.world.rot.y;
    MM_EnSsh_SetupAction(this, MM_EnSsh_Start);
    if (GameInteractor_Should(VB_HAVE_ALL_SKULLTULA_TOKENS,
                              Inventory_GetSkullTokenCount(play->sceneId) >= SPIDER_HOUSE_TOKENS_REQUIRED)) {
        MM_Actor_Kill(&this->actor);
    }
}

void MM_EnSsh_Destroy(Actor* thisx, PlayState* play) {
    EnSsh* this = (EnSsh*)thisx;
    s32 i;

    Effect_Destroy(play, this->blureIdx);

    for (i = 0; i < ARRAY_COUNT(this->collider1); i++) {
        MM_Collider_DestroyCylinder(play, &this->collider1[i]);
    }

    MM_Collider_DestroyJntSph(play, &this->collider2);
}

void MM_EnSsh_Wait(EnSsh* this, PlayState* play) {
    if (MM_EnSsh_IsCloseToLink(this, play)) {
        MM_EnSsh_SetDropAnimation(this);
        MM_EnSsh_SetupAction(this, MM_EnSsh_Drop);
    } else {
        MM_EnSsh_Bob(this, play);
    }
}

void MM_EnSsh_Talk(EnSsh* this, PlayState* play) {
    MM_EnSsh_Bob(this, play);

    if ((MM_Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && MM_Message_ShouldAdvance(play)) {
        switch (play->msgCtx.currentTextId) {
            case 0x904: // (does not exist)
            case 0x905: // (does not exist)
            case 0x906: // (does not exist)
            case 0x908: // (does not exist)
            case 0x910: // Help me! I am not a monster, I was cursed this way
            case 0x911: // Find all in here and defeat them
            case 0x912: // Don't forget to collect their token
            case 0x914: // In here, cursed spiders, defeat them to make me normal
                MM_Message_ContinueTextbox(play, play->msgCtx.currentTextId + 1);
                break;

            default: // intended case 0x915 from above (914+1)
                MM_Message_CloseTextbox(play);
                this->actionFunc = MM_EnSsh_Idle;
                break;
        }
    }
}

void func_809756D0(EnSsh* this, PlayState* play) {
    u16 nextTextId;

    if (CHECK_WEEKEVENTREG(WEEKEVENTREG_TALKED_SWAMP_SPIDER_HOUSE_MAN)) {
        nextTextId = 0x914; // In here, cursed spiders, defeat them to make me normal
    } else {
        nextTextId = 0x910; // Help me! I am not a monster, I was cursed this way
        SET_WEEKEVENTREG(WEEKEVENTREG_TALKED_SWAMP_SPIDER_HOUSE_MAN);
    }
    MM_Message_StartTextbox(play, nextTextId, &this->actor);
}

void MM_EnSsh_Idle(EnSsh* this, PlayState* play) {
    if (Actor_TalkOfferAccepted(&this->actor, &play->state)) {
        this->actionFunc = MM_EnSsh_Talk;
        func_809756D0(this, play);
        return;
    }

    if ((this->unkTimer != 0) && (DECR(this->unkTimer) == 0)) {
        EnSsh_ChangeAnim(this, SSH_ANIM_WAIT);
    }

    if ((this->animTimer != 0) && (DECR(this->animTimer) == 0)) {
        EnSsh_ChangeAnim(this, SSH_ANIM_WAIT);
    }

    if (!MM_EnSsh_IsCloseToLink(this, play)) {
        MM_EnSsh_SetReturnAnimation(this);
        MM_EnSsh_SetupAction(this, MM_EnSsh_Return);
        return;
    }

    if (DECR(this->sfxTimer) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_STALTU_LAUGH);
        this->sfxTimer = 64;
    }

    MM_EnSsh_Bob(this, play);

    if ((this->unkTimer == 0) && (this->animTimer == 0) && (this->actor.xzDistToPlayer < 100.0f) &&
        MM_Player_IsFacingActor(&this->actor, 0x3000, play)) {
        Actor_OfferTalk(&this->actor, play, 100.0f);
    }
}

void MM_EnSsh_Land(EnSsh* this, PlayState* play) {
    if ((this->unkTimer != 0) && (DECR(this->unkTimer) == 0)) {
        EnSsh_ChangeAnim(this, SSH_ANIM_WAIT);
    }

    if ((this->animTimer != 0) && (DECR(this->animTimer) == 0)) {
        EnSsh_ChangeAnim(this, SSH_ANIM_WAIT);
    }

    if ((this->actor.floorHeight + this->floorHeightOffset) <= this->actor.world.pos.y) {
        MM_EnSsh_SetupAction(this, MM_EnSsh_Idle);
    } else {
        MM_Math_SmoothStepToF(&this->actor.velocity.y, 2.0f, 0.6f, 1000.0f, 0.0f);
    }
}

void MM_EnSsh_Drop(EnSsh* this, PlayState* play) {
    if ((this->unkTimer != 0) && (DECR(this->unkTimer) == 0)) {
        EnSsh_ChangeAnim(this, SSH_ANIM_DROP);
    }

    if (!MM_EnSsh_IsCloseToLink(this, play)) {
        MM_EnSsh_SetReturnAnimation(this);
        MM_EnSsh_SetupAction(this, MM_EnSsh_Return);
    } else if (MM_EnSsh_IsCloseToGround(this)) {
        MM_EnSsh_SpawnShockwave(this, play);
        MM_EnSsh_SetLandAnimation(this);
        MM_EnSsh_SetupAction(this, MM_EnSsh_Land);
    } else if (DECR(this->sfxTimer) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_STALTU_DOWN);
        this->sfxTimer = 3;
    }
}

void MM_EnSsh_Return(EnSsh* this, PlayState* play) {
    f32 frameRatio = this->skelAnime.curFrame / (this->skelAnime.animLength - 1.0f);

    if (frameRatio == 1.0f) {
        MM_EnSsh_SetReturnAnimation(this);
    }

    if (MM_EnSsh_IsCloseToLink(this, play)) {
        MM_EnSsh_SetDropAnimation(this);
        MM_EnSsh_SetupAction(this, MM_EnSsh_Drop);
    } else if (MM_EnSsh_IsCloseToHome(this)) {
        MM_EnSsh_SetWaitAnimation(this);
        MM_EnSsh_SetupAction(this, MM_EnSsh_Wait);
    } else {
        this->actor.velocity.y = 4.0f * frameRatio;
    }
}

void MM_EnSsh_UpdateColliderScale(EnSsh* this) {
    if (this->stateFlags & SSH_STATE_SPIN) {
        if (this->spinTimer == 0) {
            this->stateFlags &= ~SSH_STATE_SPIN;
            if (!(this->stateFlags & SSH_STATE_FATHER)) {
                MM_EnSsh_SetColliderScale(this, 0.5f, 1.0f);
            } else {
                MM_EnSsh_SetColliderScale(this, 0.75f, 1.0f);
            }
        }
    } else if (this->spinTimer != 0) {
        this->stateFlags |= SSH_STATE_SPIN;
        if (!(this->stateFlags & SSH_STATE_FATHER)) {
            MM_EnSsh_SetColliderScale(this, 0.5f, 2.0f);
        } else {
            MM_EnSsh_SetColliderScale(this, 0.75f, 2.0f);
        }
    }
}

void MM_EnSsh_Start(EnSsh* this, PlayState* play) {
    if (!MM_EnSsh_IsCloseToGround(this)) {
        MM_EnSsh_SetupAction(this, MM_EnSsh_Wait);
        MM_EnSsh_Wait(this, play);
    } else {
        MM_EnSsh_SetLandAnimation(this);
        this->stateFlags |= SSH_STATE_GROUND_START;
        MM_EnSsh_SetupAction(this, MM_EnSsh_Land);
        MM_EnSsh_Land(this, play);
    }
}

void MM_EnSsh_Update(Actor* thisx, PlayState* play) {
    EnSsh* this = (EnSsh*)thisx;

    MM_EnSsh_UpdateColliderScale(this);

    if (MM_EnSsh_CollisionCheck(this, play)) {
        return;
    }

    if (this->stunTimer != 0) {
        MM_EnSsh_Damaged(this);
    } else {
        MM_SkelAnime_Update(&this->skelAnime);
        MM_Actor_UpdatePos(&this->actor);
        MM_Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 0.0f, 0.0f, UPDBGCHECKINFO_FLAG_4);
        this->actionFunc(this, play);
    }

    MM_EnSsh_UpdateYaw(this, play);

    if (DECR(this->blinkTimer) == 0) {
        this->blinkTimer = MM_Rand_S16Offset(60, 60);
    }

    this->blinkState = this->blinkTimer;
    if (this->blinkState >= 3) {
        this->blinkState = 0;
    }

    MM_EnSsh_SetColliders(this, play);
    MM_Actor_SetFocus(&this->actor, 0.0f);
}

s32 MM_EnSsh_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    EnSsh* this = (EnSsh*)thisx;

    switch (limbIndex) {
        case OBJECT_SSH_LIMB_01:
            if ((this->spinTimer != 0) && (this->swayTimer == 0)) {
                if (this->spinTimer >= 2) {
                    MM_EnSsh_AddBlureVertex(this);
                } else {
                    MM_EnSsh_AddBlureSpace(this);
                }
            }
            break;

        case OBJECT_SSH_LIMB_04:
            if (this->stateFlags & SSH_STATE_FATHER) {
                *dList = object_ssh_DL_005850;
            }
            break;

        case OBJECT_SSH_LIMB_05:
            if (this->stateFlags & SSH_STATE_FATHER) {
                *dList = object_ssh_DL_005210;
            }
            break;

        case OBJECT_SSH_LIMB_08:
            if (this->stateFlags & SSH_STATE_FATHER) {
                *dList = object_ssh_DL_005F78;
            }
            break;

        default:
            break;
    }
    return false;
}

void MM_EnSsh_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    EnSsh* this = (EnSsh*)thisx;

    if ((limbIndex == OBJECT_SSH_LIMB_05) && (this->stateFlags & SSH_STATE_FATHER)) {
        OPEN_DISPS(play->state.gfxCtx);

        gSPDisplayList(POLY_OPA_DISP++, object_ssh_DL_0000D8);

        CLOSE_DISPS(play->state.gfxCtx);
    }
    MM_Collider_UpdateSpheres(limbIndex, &this->collider2);
}

void MM_EnSsh_Draw(Actor* thisx, PlayState* play) {
    static TexturePtr D_80976178[] = { object_ssh_Tex_001970, object_ssh_Tex_001DF0, object_ssh_Tex_0021F0 };
    s32 pad;
    EnSsh* this = (EnSsh*)thisx;

    MM_EnSsh_CheckBodyStickHit(this, play);
    MM_EnSsh_Sway(this);

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Opa(play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_K0(D_80976178[this->blinkState]));

    MM_SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, MM_EnSsh_OverrideLimbDraw,
                      MM_EnSsh_PostLimbDraw, &this->actor);

    CLOSE_DISPS(play->state.gfxCtx);
}
