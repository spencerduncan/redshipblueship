/*
 * File: z_boss_fd2.c
 * Overlay: ovl_Boss_Fd2
 * Description: Volvagia, hole form
 */

#include "z_boss_fd2.h"
#include "objects/object_fd2/object_fd2.h"
#include "overlays/actors/ovl_Boss_Fd/z_boss_fd.h"
#include "overlays/actors/ovl_Door_Warp1/z_door_warp1.h"
#include "vt.h"
#include "soh/frame_interpolation.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS                                                                                 \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_DRAW_CULLING_DISABLED)

typedef enum {
    /* 0 */ DEATH_START,
    /* 1 */ DEATH_RETREAT,
    /* 2 */ DEATH_HANDOFF,
    /* 3 */ DEATH_FD_BODY,
    /* 4 */ DEATH_FD_SKULL,
    /* 5 */ DEATH_FINISH
} BossFd2CutsceneState;

typedef enum {
    /* 0 */ EYE_OPEN,
    /* 1 */ EYE_HALF,
    /* 2 */ EYE_CLOSED
} BossFd2EyeState;

void BossFd2_Init(Actor* thisx, PlayState* play);
void BossFd2_Destroy(Actor* thisx, PlayState* play);
void BossFd2_Update(Actor* thisx, PlayState* play);
void BossFd2_Draw(Actor* thisx, PlayState* play);

void BossFd2_SetupEmerge(BossFd2* this, PlayState* play);
void BossFd2_Emerge(BossFd2* this, PlayState* play);
void BossFd2_SetupIdle(BossFd2* this, PlayState* play);
void BossFd2_Idle(BossFd2* this, PlayState* play);
void BossFd2_Burrow(BossFd2* this, PlayState* play);
void BossFd2_SetupBreatheFire(BossFd2* this, PlayState* play);
void BossFd2_BreatheFire(BossFd2* this, PlayState* play);
void BossFd2_SetupClawSwipe(BossFd2* this, PlayState* play);
void BossFd2_ClawSwipe(BossFd2* this, PlayState* play);
void BossFd2_Vulnerable(BossFd2* this, PlayState* play);
void BossFd2_Damaged(BossFd2* this, PlayState* play);
void BossFd2_Death(BossFd2* this, PlayState* play);
void BossFd2_Wait(BossFd2* this, PlayState* play);

const ActorInit Boss_Fd2_InitVars = {
    ACTOR_BOSS_FD2,
    ACTORCAT_BOSS,
    FLAGS,
    OBJECT_FD2,
    sizeof(BossFd2),
    (ActorFunc)BossFd2_Init,
    (ActorFunc)BossFd2_Destroy,
    (ActorFunc)BossFd2_Update,
    (ActorFunc)BossFd2_Draw,
    NULL,
};

#include "z_boss_fd2_colchk.c"

static Vec3f sHoleLocations[] = {
    { 0.0f, 90.0f, -243.0f },    { 0.0f, 90.0f, 0.0f },    { 0.0f, 90.0f, 243.0f },
    { -243.0f, 90.0f, -243.0f }, { -243.0f, 90.0f, 0.0f }, { -243.0f, 90.0f, 243.0f },
    { 243.0f, 90.0f, -243.0f },  { 243.0f, 90.0f, 0.0f },  { 243.0f, 90.0f, 243.0f },
};

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_U8(targetMode, 5, ICHAIN_CONTINUE),
    ICHAIN_S8(naviEnemyId, 0x21, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, 0, ICHAIN_CONTINUE),
    ICHAIN_F32(targetArrowOffset, 0, ICHAIN_STOP),
};

void BossFd2_SpawnDebris(PlayState* play, BossFdEffect* effect, Vec3f* position, Vec3f* velocity, Vec3f* acceleration,
                         f32 scale) {
    s16 i;

    for (i = 0; i < 180; i++, effect++) {
        if (effect->type == BFD_FX_NONE) {
            effect->type = BFD_FX_DEBRIS;
            effect->pos = *position;
            effect->velocity = *velocity;
            effect->accel = *acceleration;
            effect->scale = scale / 1000.0f;
            effect->vFdFxRotX = OoT_Rand_ZeroFloat(100.0f);
            effect->vFdFxRotY = OoT_Rand_ZeroFloat(100.0f);
            effect->epoch++;
            break;
        }
    }
}

void BossFd2_SpawnFireBreath(PlayState* play, BossFdEffect* effect, Vec3f* position, Vec3f* velocity,
                             Vec3f* acceleration, f32 scale, s16 alpha, s16 kbAngle) {
    s16 i;

    for (i = 0; i < 180; i++, effect++) {
        if (effect->type == BFD_FX_NONE) {
            effect->type = BFD_FX_FIRE_BREATH;
            effect->timer1 = 0;
            effect->pos = *position;
            effect->velocity = *velocity;
            effect->accel = *acceleration;
            effect->pos.x -= effect->velocity.x;
            effect->pos.y -= effect->velocity.y;
            effect->pos.z -= effect->velocity.z;
            effect->vFdFxScaleMod = 0.0f;
            effect->alpha = alpha;
            effect->vFdFxYStop = OoT_Rand_ZeroFloat(10.0f);
            effect->timer2 = 0;
            effect->scale = scale / 400.0f;
            effect->kbAngle = kbAngle;
            effect->epoch++;
            break;
        }
    }
}

void BossFd2_SpawnEmber(PlayState* play, BossFdEffect* effect, Vec3f* position, Vec3f* velocity, Vec3f* acceleration,
                        f32 scale) {
    s16 i;

    for (i = 0; i < 180; i++, effect++) {
        if (effect->type == 0) {
            effect->type = BFD_FX_EMBER;
            effect->pos = *position;
            effect->velocity = *velocity;
            effect->accel = *acceleration;
            effect->scale = scale / 1000.0f;
            effect->alpha = 255;
            effect->timer1 = (s16)OoT_Rand_ZeroFloat(10.0f);
            effect->epoch++;
            break;
        }
    }
}

void BossFd2_SpawnSkullPiece(PlayState* play, BossFdEffect* effect, Vec3f* position, Vec3f* velocity,
                             Vec3f* acceleration, f32 scale) {
    s16 i;

    for (i = 0; i < 180; i++, effect++) {
        if (effect->type == BFD_FX_NONE) {
            effect->type = BFD_FX_SKULL_PIECE;
            effect->pos = *position;
            effect->velocity = *velocity;
            effect->accel = *acceleration;
            effect->scale = scale / 1000.0f;
            effect->vFdFxRotX = OoT_Rand_ZeroFloat(100.0f);
            effect->vFdFxRotY = OoT_Rand_ZeroFloat(100.0f);
            effect->epoch++;
            break;
        }
    }
}

void BossFd2_SpawnDust(BossFdEffect* effect, Vec3f* position, Vec3f* velocity, Vec3f* acceleration, f32 scale) {
    s16 i;

    for (i = 0; i < 180; i++, effect++) {
        if (effect->type == BFD_FX_NONE) {
            effect->type = BFD_FX_DUST;
            effect->pos = *position;
            effect->velocity = *velocity;
            effect->accel = *acceleration;
            effect->timer2 = 0;
            effect->scale = scale / 400.0f;
            effect->epoch++;
            break;
        }
    }
}

void BossFd2_Init(Actor* thisx, PlayState* play) {
    s32 pad;
    BossFd2* this = (BossFd2*)thisx;

    OoT_Actor_ProcessInitChain(&this->actor, OoT_sInitChain);
    OoT_Actor_SetScale(&this->actor, 0.0069999993f);
    this->actor.world.pos.y = -850.0f;
    OoT_ActorShape_Init(&this->actor.shape, -580.0f / this->actor.scale.y, NULL, 0.0f);
    OoT_SkelAnime_InitFlex(play, &this->skelAnime, &gHoleVolvagiaSkel, &gHoleVolvagiaIdleAnim, NULL, NULL, 0);
    if (this->actor.params == BFD_CS_NONE) {
        BossFd2_SetupEmerge(this, play);
    } else {
        this->actionFunc = BossFd2_Wait;
    }
    OoT_Collider_InitJntSph(play, &this->collider);
    OoT_Collider_SetJntSph(play, &this->collider, &this->actor, &OoT_sJntSphInit, this->elements);
}

void BossFd2_Destroy(Actor* thisx, PlayState* play) {
    s32 pad;
    BossFd2* this = (BossFd2*)thisx;

    OoT_SkelAnime_Free(&this->skelAnime, play);
    OoT_Collider_DestroyJntSph(play, &this->collider);
}

void BossFd2_SetupEmerge(BossFd2* this, PlayState* play) {
    BossFd* bossFd = (BossFd*)this->actor.parent;
    s16 temp_rand;
    s8 health;

    osSyncPrintf("UP INIT 1\n");
    OoT_Animation_PlayOnce(&this->skelAnime, &gHoleVolvagiaEmergeAnim);
    this->actionFunc = BossFd2_Emerge;
    this->skelAnime.playSpeed = 0.0f;
    temp_rand = OoT_Rand_ZeroFloat(8.9f);
    this->actor.world.pos.x = sHoleLocations[temp_rand].x;
    this->actor.world.pos.z = sHoleLocations[temp_rand].z;
    this->work[FD2_ACTION_STATE] = 0;
    osSyncPrintf("UP INIT 2\n");
    this->timers[0] = 10;
    if (bossFd != NULL) {
        health = bossFd->actor.colChkInfo.health;
        if (health >= 18) {
            this->work[FD2_FAKEOUT_COUNT] = 0;
        } else if (health >= 12) {
            this->work[FD2_FAKEOUT_COUNT] = 1;
        } else if (health >= 6) {
            this->work[FD2_FAKEOUT_COUNT] = 2;
        } else {
            this->work[FD2_FAKEOUT_COUNT] = 3;
        }
    }
}

void BossFd2_Emerge(BossFd2* this, PlayState* play) {
    s8 health;
    BossFd* bossFd = (BossFd*)this->actor.parent;
    Player* player = GET_PLAYER(play);
    s16 i;
    s16 holeTime;

    osSyncPrintf("UP 1    mode %d\n", this->work[FD2_ACTION_STATE]);
    OoT_SkelAnime_Update(&this->skelAnime);
    osSyncPrintf("UP 1.5 \n");
    switch (this->work[FD2_ACTION_STATE]) {
        case 0:
            osSyncPrintf("UP time %d \n", this->timers[0]);
            osSyncPrintf("PL time %x \n", player);
            osSyncPrintf("MT time %x \n", bossFd);
            if ((this->timers[0] == 0) && (player->actor.world.pos.y > 70.0f)) {
                osSyncPrintf("UP 1.6 \n");
                bossFd->faceExposed = 0;
                bossFd->holePosition.x = this->actor.world.pos.x;
                bossFd->holePosition.z = this->actor.world.pos.z;
                func_80033E1C(play, 1, 0x32, 0x5000);
                this->work[FD2_ACTION_STATE] = 1;
                this->work[FD2_HOLE_COUNTER]++;
                this->actor.world.pos.y = -200.0f;
                health = bossFd->actor.colChkInfo.health;
                if (health == 24) {
                    holeTime = 30;
                } else if (health >= 18) {
                    holeTime = 25;
                } else if (health >= 12) {
                    holeTime = 20;
                } else if (health >= 6) {
                    holeTime = 10;
                } else {
                    holeTime = 5;
                }
                this->timers[0] = holeTime;
                bossFd->timers[4] = this->timers[0] + 10;
                osSyncPrintf("UP 1.7 \n");
            }
            break;
        case 1:
            if (this->timers[0] == 0) {
                if (this->work[FD2_FAKEOUT_COUNT] != 0) {
                    this->work[FD2_FAKEOUT_COUNT]--;
                    i = OoT_Rand_ZeroFloat(8.9f);
                    this->actor.world.pos.x = sHoleLocations[i].x;
                    this->actor.world.pos.z = sHoleLocations[i].z;
                    this->work[FD2_ACTION_STATE] = 0;
                    this->timers[0] = 10;
                } else {
                    this->skelAnime.playSpeed = 1.0f;
                    this->fwork[FD2_END_FRAME] = OoT_Animation_GetLastFrame(&gHoleVolvagiaEmergeAnim);
                    this->work[FD2_ACTION_STATE] = 2;
                    Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_ROAR);
                    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
                    this->timers[0] = 15;
                    this->actor.world.pos.y = 150.0f;
                    for (i = 0; i < 10; i++) {
                        this->rightMane.pos[i].x += OoT_Rand_CenteredFloat(100.0f);
                        this->rightMane.pos[i].z += OoT_Rand_CenteredFloat(100.0f);
                        this->leftMane.pos[i].x += OoT_Rand_CenteredFloat(100.0f);
                        this->leftMane.pos[i].z += OoT_Rand_CenteredFloat(100.0f);
                    }
                    bossFd->work[BFD_SPLASH_TIMER] = 5;
                }
            }
            break;
        case 2:
            OoT_Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 0x7D0);
            if ((this->timers[0] == 1) && (this->actor.xzDistToPlayer < 120.0f)) {
                func_8002F6D4(play, &this->actor, 3.0f, this->actor.yawTowardsPlayer, 2.0f, 0x20);
                Audio_PlayActorSound2(&player->actor, NA_SE_PL_BODY_HIT);
            }
            if (OoT_Animation_OnFrame(&this->skelAnime, this->fwork[FD2_END_FRAME])) {
                BossFd2_SetupIdle(this, play);
            }
            break;
    }
    osSyncPrintf("UP 2\n");
}

void BossFd2_SetupIdle(BossFd2* this, PlayState* play) {
    BossFd* bossFd = (BossFd*)this->actor.parent;
    s8 health;
    s16 idleTime;

    osSyncPrintf("UP INIT 1\n");
    OoT_Animation_PlayLoop(&this->skelAnime, &gHoleVolvagiaTurnAnim);
    this->actionFunc = BossFd2_Idle;
    health = bossFd->actor.colChkInfo.health;
    if (health == 24) {
        idleTime = 50;
    } else if (health >= 18) {
        idleTime = 40;
    } else if (health >= 12) {
        idleTime = 40;
    } else if (health >= 6) {
        idleTime = 30;
    } else {
        idleTime = 20;
    }
    this->timers[0] = idleTime;
}

void BossFd2_Idle(BossFd2* this, PlayState* play) {
    s16 prevToLink;

    OoT_SkelAnime_Update(&this->skelAnime);
    prevToLink = this->work[FD2_TURN_TO_LINK];
    this->work[FD2_TURN_TO_LINK] =
        OoT_Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 0x7D0, 0);
    osSyncPrintf("SW1 = %d\n", prevToLink);
    osSyncPrintf("SW2 = %d\n", this->work[FD2_TURN_TO_LINK]);
    if ((fabsf(prevToLink) <= 1000.0f) && (1000.0f < fabsf(this->work[FD2_TURN_TO_LINK]))) {
        OoT_Animation_MorphToLoop(&this->skelAnime, &gHoleVolvagiaTurnAnim, -5.0f);
    }
    if ((1000.0f < fabsf(prevToLink)) && (fabsf(this->work[FD2_TURN_TO_LINK]) <= 1000.0f)) {
        OoT_Animation_MorphToLoop(&this->skelAnime, &gHoleVolvagiaIdleAnim, -5.0f);
    }
    if (this->timers[0] == 0) {
        if (this->actor.xzDistToPlayer < 200.0f) {
            BossFd2_SetupClawSwipe(this, play);
        } else {
            BossFd2_SetupBreatheFire(this, play);
        }
    }
}

void BossFd2_SetupBurrow(BossFd2* this, PlayState* play) {
    BossFd* bossFd = (BossFd*)this->actor.parent;

    OoT_Animation_MorphToPlayOnce(&this->skelAnime, &gHoleVolvagiaBurrowAnim, -5.0f);
    this->actionFunc = BossFd2_Burrow;
    this->fwork[FD2_END_FRAME] = OoT_Animation_GetLastFrame(&gHoleVolvagiaBurrowAnim);
    bossFd->timers[4] = 30;
    this->work[FD2_ACTION_STATE] = 0;
}

void BossFd2_Burrow(BossFd2* this, PlayState* play) {
    BossFd* bossFd = (BossFd*)this->actor.parent;

    if (this->work[FD2_ACTION_STATE] == 0) {
        OoT_SkelAnime_Update(&this->skelAnime);
        if (OoT_Animation_OnFrame(&this->skelAnime, this->fwork[FD2_END_FRAME])) {
            this->work[FD2_ACTION_STATE] = 1;
            this->timers[0] = 25;
        }
    } else {
        OoT_Math_ApproachF(&this->actor.world.pos.y, -100.0f, 1.0f, 10.0f);
        if (this->timers[0] == 0) {
            if ((this->work[FD2_HOLE_COUNTER] >= 3) && ((s8)bossFd->actor.colChkInfo.health < 24)) {
                this->work[FD2_HOLE_COUNTER] = 0;
                this->actionFunc = BossFd2_Wait;
                bossFd->handoffSignal = FD2_SIGNAL_FLY;
            } else {
                BossFd2_SetupEmerge(this, play);
            }
        }
    }
}

void BossFd2_SetupBreatheFire(BossFd2* this, PlayState* play) {
    OoT_Animation_MorphToPlayOnce(&this->skelAnime, &gHoleVolvagiaBreatheFireAnim, -5.0f);
    this->actionFunc = BossFd2_BreatheFire;
    this->fwork[FD2_END_FRAME] = OoT_Animation_GetLastFrame(&gHoleVolvagiaBreatheFireAnim);
    this->work[FD2_ACTION_STATE] = 0;
}

static Vec3f sUnkVec = { 0.0f, 0.0f, 50.0f }; // Unused? BossFd uses a similar array for its fire breath sfx.

void BossFd2_BreatheFire(BossFd2* this, PlayState* play) {
    s16 i;
    Vec3f toLink;
    s16 angleX;
    s16 angleY;
    s16 breathOpacity = 0;
    BossFd* bossFd = (BossFd*)this->actor.parent;
    Player* player = GET_PLAYER(play);
    f32 tempX;
    f32 tempY;

    OoT_SkelAnime_Update(&this->skelAnime);
    if (OoT_Animation_OnFrame(&this->skelAnime, this->fwork[FD2_END_FRAME])) {
        BossFd2_SetupBurrow(this, play);
    }
    if ((25.0f <= this->skelAnime.curFrame) && (this->skelAnime.curFrame < 70.0f)) {
        if (this->skelAnime.curFrame == 25.0f) {
            play->envCtx.unk_D8 = 0.0f;
        }
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_FIRE - SFX_FLAG);
        if (this->skelAnime.curFrame > 50) {
            breathOpacity = (70.0f - this->skelAnime.curFrame) * 12.0f;
        } else {
            breathOpacity = 255;
        }
        toLink.x = player->actor.world.pos.x - this->headPos.x;
        toLink.y = player->actor.world.pos.y - this->headPos.y;
        toLink.z = player->actor.world.pos.z - this->headPos.z;
        angleY = OoT_Math_Atan2S(toLink.z, toLink.x);
        angleX = -OoT_Math_Atan2S(OoT_sqrtf(SQ(toLink.x) + SQ(toLink.z)), toLink.y);
        angleY -= this->actor.shape.rot.y;
        if (angleY > 0x1F40) {
            angleY = 0x1F40;
        }
        if (angleY < -0x1F40) {
            angleY = -0x1F40;
        }
        angleX += (-0x1B58);
        if (angleX > 0x3E8) {
            angleX = 0x3E8;
        }
        if (angleX < -0xFA0) {
            angleX = -0xFA0;
        }
        OoT_Math_ApproachS(&this->headRot.y, angleY, 5, 0x7D0);
        OoT_Math_ApproachS(&this->headRot.x, angleX, 5, 0x7D0);
    } else {
        OoT_Math_ApproachS(&this->headRot.y, 0, 5, 0x7D0);
        OoT_Math_ApproachS(&this->headRot.x, 0, 5, 0x7D0);
    }
    if (breathOpacity != 0) {
        f32 breathScale;
        Vec3f spawnSpeed = { 0.0f, 0.0f, 0.0f };
        Vec3f spawnVel;
        Vec3f spawnAccel = { 0.0f, 0.0f, 0.0f };
        Vec3f spawnPos;

        bossFd->fogMode = 2;
        spawnSpeed.z = 30.0f;
        spawnPos = this->headPos;

        tempY = ((this->actor.shape.rot.y + this->headRot.y) / (f32)0x8000) * M_PI;
        tempX = ((this->headRot.x / (f32)0x8000) * M_PI) + 1.0f / 2;
        Matrix_RotateY(tempY, MTXMODE_NEW);
        Matrix_RotateX(tempX, MTXMODE_APPLY);
        OoT_Matrix_MultVec3f(&spawnSpeed, &spawnVel);

        breathScale = 300.0f + 50.0f * OoT_Math_SinS(this->work[FD2_VAR_TIMER] * 0x2000);
        BossFd2_SpawnFireBreath(play, bossFd->effects, &spawnPos, &spawnVel, &spawnAccel, breathScale, breathOpacity,
                                this->actor.shape.rot.y + this->headRot.y);

        spawnPos.x += spawnVel.x * 0.5f;
        spawnPos.y += spawnVel.y * 0.5f;
        spawnPos.z += spawnVel.z * 0.5f;

        breathScale = 300.0f + 50.0f * OoT_Math_SinS(this->work[FD2_VAR_TIMER] * 0x2000);
        BossFd2_SpawnFireBreath(play, bossFd->effects, &spawnPos, &spawnVel, &spawnAccel, breathScale, breathOpacity,
                                this->actor.shape.rot.y + this->headRot.y);

        spawnSpeed.x = 0.0f;
        spawnSpeed.y = 17.0f;
        spawnSpeed.z = 0.0f;

        for (i = 0; i < 6; i++) {
            tempY = OoT_Rand_ZeroFloat(2.0f * M_PI);
            tempX = OoT_Rand_ZeroFloat(2.0f * M_PI);
            Matrix_RotateY(tempY, MTXMODE_NEW);
            Matrix_RotateX(tempX, MTXMODE_APPLY);
            OoT_Matrix_MultVec3f(&spawnSpeed, &spawnVel);

            spawnAccel.x = (spawnVel.x * -10.0f) / 100.0f;
            spawnAccel.y = (spawnVel.y * -10.0f) / 100.0f;
            spawnAccel.z = (spawnVel.z * -10.0f) / 100.0f;

            BossFd2_SpawnEmber(play, bossFd->effects, &this->headPos, &spawnVel, &spawnAccel,
                               (s16)OoT_Rand_ZeroFloat(2.0f) + 8);
        }
    }
}

void BossFd2_SetupClawSwipe(BossFd2* this, PlayState* play) {
    OoT_Animation_MorphToPlayOnce(&this->skelAnime, &gHoleVolvagiaClawSwipeAnim, -5.0f);
    this->actionFunc = BossFd2_ClawSwipe;
    this->fwork[FD2_END_FRAME] = OoT_Animation_GetLastFrame(&gHoleVolvagiaClawSwipeAnim);
}

void BossFd2_ClawSwipe(BossFd2* this, PlayState* play) {
    OoT_SkelAnime_Update(&this->skelAnime);
    if (OoT_Animation_OnFrame(&this->skelAnime, 5.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_ROAR);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_SW_NAIL);
    }
    if (OoT_Animation_OnFrame(&this->skelAnime, this->fwork[FD2_END_FRAME])) {
        BossFd2_SetupBurrow(this, play);
    }
}

void BossFd2_SetupVulnerable(BossFd2* this, PlayState* play) {
    OoT_Animation_PlayOnce(&this->skelAnime, &gHoleVolvagiaKnockoutAnim);
    this->fwork[FD2_END_FRAME] = OoT_Animation_GetLastFrame(&gHoleVolvagiaKnockoutAnim);
    this->actionFunc = BossFd2_Vulnerable;
    this->work[FD2_ACTION_STATE] = 0;
}

void BossFd2_Vulnerable(BossFd2* this, PlayState* play) {
    BossFd* bossFd = (BossFd*)this->actor.parent;
    s16 i;

    this->disableAT = true;
    this->actor.flags |= ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    OoT_SkelAnime_Update(&this->skelAnime);
    switch (this->work[FD2_ACTION_STATE]) {
        case 0:
            if (OoT_Animation_OnFrame(&this->skelAnime, 13.0f)) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_MAHI2);
            }
            if (OoT_Animation_OnFrame(&this->skelAnime, this->fwork[FD2_END_FRAME] - 3.0f)) {
                for (i = 0; i < 25; i++) {
                    Vec3f spawnVel;
                    Vec3f spawnAccel = { 0.0f, 0.0f, 0.0f };
                    Vec3f spawnPos;

                    spawnVel.x = OoT_Rand_CenteredFloat(8.0f);
                    spawnVel.y = OoT_Rand_ZeroFloat(1.0f);
                    spawnVel.z = OoT_Rand_CenteredFloat(8.0f);

                    spawnAccel.y = 0.5f;

                    spawnPos.x = OoT_Rand_CenteredFloat(10.0f) + this->actor.focus.pos.x;
                    spawnPos.y = OoT_Rand_CenteredFloat(10.0f) + this->actor.focus.pos.y;
                    spawnPos.z = OoT_Rand_CenteredFloat(10.0f) + this->actor.focus.pos.z;

                    BossFd2_SpawnDust(bossFd->effects, &spawnPos, &spawnVel, &spawnAccel,
                                      OoT_Rand_ZeroFloat(100.0f) + 300.0f);
                }
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_LAND);
            }
            if (OoT_Animation_OnFrame(&this->skelAnime, this->fwork[FD2_END_FRAME])) {
                OoT_Animation_MorphToLoop(&this->skelAnime, &gHoleVolvagiaVulnerableAnim, -5.0f);
                this->work[FD2_ACTION_STATE] = 1;
                this->timers[0] = 60;
            }
            break;
        case 1:
            if ((this->work[FD2_VAR_TIMER] & 0xF) == 0xF) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_KNOCKOUT);
            }
            if (this->timers[0] == 0) {
                BossFd2_SetupBurrow(this, play);
            }
            break;
    }
}

void BossFd2_SetupDamaged(BossFd2* this, PlayState* play) {
    OoT_Animation_PlayOnce(&this->skelAnime, &gHoleVolvagiaHitAnim);
    this->fwork[FD2_END_FRAME] = OoT_Animation_GetLastFrame(&gHoleVolvagiaHitAnim);
    this->actionFunc = BossFd2_Damaged;
    this->work[FD2_ACTION_STATE] = 0;
}

void BossFd2_Damaged(BossFd2* this, PlayState* play) {
    BossFd* bossFd = (BossFd*)this->actor.parent;

    OoT_SkelAnime_Update(&this->skelAnime);
    this->disableAT = true;
    if (this->work[FD2_ACTION_STATE] == 0) {
        if (OoT_Animation_OnFrame(&this->skelAnime, this->fwork[FD2_END_FRAME])) {
            OoT_Animation_PlayOnce(&this->skelAnime, &gHoleVolvagiaDamagedAnim);
            this->fwork[FD2_END_FRAME] = OoT_Animation_GetLastFrame(&gHoleVolvagiaDamagedAnim);
            this->work[FD2_ACTION_STATE] = 1;
        }
    } else if (this->work[FD2_ACTION_STATE] == 1) {
        if (OoT_Animation_OnFrame(&this->skelAnime, 6.0f)) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_DAMAGE2);
        }
        if (OoT_Animation_OnFrame(&this->skelAnime, 20.0f)) {
            bossFd->timers[4] = 30;
        }
        if (OoT_Animation_OnFrame(&this->skelAnime, this->fwork[FD2_END_FRAME])) {
            this->work[FD2_ACTION_STATE] = 2;
            this->timers[0] = 25;
        }
    } else {
        OoT_Math_ApproachF(&this->actor.world.pos.y, -100.0f, 1.0f, 10.0f);
        if (this->timers[0] == 0) {
            this->actionFunc = BossFd2_Wait;
            bossFd->handoffSignal = FD2_SIGNAL_FLY;
        }
    }
}

void BossFd2_SetupDeath(BossFd2* this, PlayState* play) {
    this->fwork[FD2_END_FRAME] = OoT_Animation_GetLastFrame(&gHoleVolvagiaDamagedAnim);
    OoT_Animation_Change(&this->skelAnime, &gHoleVolvagiaDamagedAnim, 1.0f, 0.0f, this->fwork[FD2_END_FRAME],
                     ANIMMODE_ONCE_INTERP, -3.0f);
    this->actionFunc = BossFd2_Death;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->deathState = DEATH_START;
}

void BossFd2_UpdateCamera(BossFd2* this, PlayState* play) {
    if (this->deathCamera != SUBCAM_FREE) {
        OoT_Math_ApproachF(&this->camData.eye.x, this->camData.nextEye.x, this->camData.eyeMaxVel.x,
                       this->camData.eyeVel.x * this->camData.speedMod);
        OoT_Math_ApproachF(&this->camData.eye.y, this->camData.nextEye.y, this->camData.eyeMaxVel.y,
                       this->camData.eyeVel.y * this->camData.speedMod);
        OoT_Math_ApproachF(&this->camData.eye.z, this->camData.nextEye.z, this->camData.eyeMaxVel.z,
                       this->camData.eyeVel.z * this->camData.speedMod);
        OoT_Math_ApproachF(&this->camData.at.x, this->camData.nextAt.x, this->camData.atMaxVel.x,
                       this->camData.atVel.x * this->camData.speedMod);
        OoT_Math_ApproachF(&this->camData.at.y, this->camData.nextAt.y, this->camData.atMaxVel.y,
                       this->camData.atVel.y * this->camData.speedMod);
        OoT_Math_ApproachF(&this->camData.at.z, this->camData.nextAt.z, this->camData.atMaxVel.z,
                       this->camData.atVel.z * this->camData.speedMod);
        OoT_Math_ApproachF(&this->camData.speedMod, 1.0f, 1.0f, this->camData.accel);
        this->camData.at.y += this->camData.yMod;
        Play_CameraSetAtEye(play, this->deathCamera, &this->camData.at, &this->camData.eye);
        OoT_Math_ApproachF(&this->camData.yMod, 0.0f, 1.0f, 0.1f);
    }
}

void BossFd2_Death(BossFd2* this, PlayState* play) {
    f32 retreatSpeed;
    Vec3f sp70;
    Vec3f sp64;
    BossFd* bossFd = (BossFd*)this->actor.parent;
    Camera* mainCam = OoT_Play_GetCamera(play, MAIN_CAM);
    f32 pad3;
    f32 pad2;
    f32 pad1;
    f32 cameraShake;
    SkelAnime* skelAnime = &this->skelAnime;

    OoT_SkelAnime_Update(skelAnime);
    switch (this->deathState) {
        case DEATH_START:
            this->deathState = DEATH_RETREAT;
            func_80064520(play, &play->csCtx);
            OoT_Player_SetCsActionWithHaltedActors(play, &this->actor, 1);
            this->deathCamera = OoT_Play_CreateSubCamera(play);
            OoT_Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_WAIT);
            OoT_Play_ChangeCameraStatus(play, this->deathCamera, CAM_STAT_ACTIVE);
            this->camData.eye = mainCam->eye;
            this->camData.at = mainCam->at;
            this->camData.eyeVel.x = 100.0f;
            this->camData.eyeVel.y = 100.0f;
            this->camData.eyeVel.z = 100.0f;
            this->camData.atVel.x = 100.0f;
            this->camData.atVel.y = 100.0f;
            this->camData.atVel.z = 100.0f;
            this->camData.accel = 0.02f;
            this->timers[0] = 0;
            this->work[FD2_HOLE_COUNTER] = 0;
            this->camData.eyeMaxVel.x = 0.1f;
            this->camData.eyeMaxVel.y = 0.1f;
            this->camData.eyeMaxVel.z = 0.1f;
            this->camData.atMaxVel.x = 0.1f;
            this->camData.atMaxVel.y = 0.1f;
            this->camData.atMaxVel.z = 0.1f;
        case DEATH_RETREAT:
            this->work[FD2_HOLE_COUNTER]++;
            if (this->work[FD2_HOLE_COUNTER] < 15) {
                retreatSpeed = 1.0f;
            } else if (this->work[FD2_HOLE_COUNTER] < 20) {
                retreatSpeed = 0.5f;
            } else {
                retreatSpeed = 0.25f;
            }
            if ((this->work[FD2_HOLE_COUNTER] == 1) || (this->work[FD2_HOLE_COUNTER] == 40)) {
                this->work[FD2_SCREAM_TIMER] = 20;
                if (this->work[FD2_HOLE_COUNTER] == 40) {
                    Audio_StopSfxById(NA_SE_EN_VALVAISA_DEAD);
                }

                Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_DAMAGE2);
            }
            OoT_Math_ApproachF(&this->skelAnime.playSpeed, retreatSpeed, 1.0f, 1.0f);
            Matrix_RotateY(((this->actor.yawTowardsPlayer / (f32)0x8000) * M_PI) + 0.2f, MTXMODE_NEW);
            sp70.x = 0.0f;
            sp70.y = 0.0f;
            sp70.z = 250.0f;
            OoT_Matrix_MultVec3f(&sp70, &sp64);
            this->camData.nextEye.x = this->actor.world.pos.x + sp64.x;
            this->camData.nextEye.y = 140.0f;
            this->camData.nextEye.z = this->actor.world.pos.z + sp64.z;
            if (this->actor.focus.pos.y >= 90.0f) {
                this->camData.nextAt.y = this->actor.focus.pos.y;
                this->camData.nextAt.x = this->actor.focus.pos.x;
                this->camData.nextAt.z = this->actor.focus.pos.z;
            }
            if (this->timers[0] == 0) {
                if (OoT_Animation_OnFrame(skelAnime, 20.0f)) {
                    bossFd->timers[4] = 60;
                }
                if (this->work[FD2_HOLE_COUNTER] >= 100) {
                    this->deathState = DEATH_HANDOFF;
                    this->timers[0] = 50;
                }
            } else if (OoT_Animation_OnFrame(skelAnime, 15.0f)) {
                OoT_Animation_MorphToPlayOnce(skelAnime, &gHoleVolvagiaDamagedAnim, -10.0f);
            }
            break;
        case DEATH_HANDOFF:
            if (this->timers[0] == 0) {
                this->actor.draw = NULL;
                this->deathState = DEATH_FD_BODY;
                bossFd->handoffSignal = FD2_SIGNAL_DEATH;
                this->work[FD2_ACTION_STATE] = 0;
                this->camData.speedMod = 0.0f;
            } else {
                OoT_Math_ApproachF(&this->actor.world.pos.y, -100.0f, 1.0f, 5.0f);
            }
            break;
        case DEATH_FD_BODY:
            if (bossFd->actor.world.pos.y < 80.0f) {
                if (bossFd->actor.world.rot.x > 0x3000) {
                    this->camData.nextAt = bossFd->actor.world.pos;
                    this->camData.nextAt.y = 80.0f;
                    this->camData.nextEye.x = bossFd->actor.world.pos.x;
                    this->camData.nextEye.y = 150.0f;
                    this->camData.nextEye.z = bossFd->actor.world.pos.z + 300.0f;
                }
            } else {
                this->camData.nextAt = bossFd->actor.world.pos;
                this->camData.nextEye.x = this->actor.world.pos.x;
                OoT_Math_ApproachF(&this->camData.nextEye.y, 200.0f, 1.0f, 2.0f);
                OoT_Math_ApproachF(&this->camData.nextEye.z, bossFd->actor.world.pos.z + 200.0f, 1.0f, 3.0f);
                if (this->work[FD2_ACTION_STATE] == 0) {
                    this->work[FD2_ACTION_STATE]++;
                    this->camData.speedMod = 0.0f;
                    this->camData.accel = 0.02f;
                    OoT_Player_SetCsActionWithHaltedActors(play, &bossFd->actor, 1);
                }
            }
            if ((bossFd->work[BFD_ACTION_STATE] == BOSSFD_BONES_FALL) && (bossFd->timers[0] == 5)) {
                this->deathState = DEATH_FD_SKULL;
                this->camData.speedMod = 0.0f;
                this->camData.accel = 0.02f;
                this->camData.nextEye.y = 150.0f;
                this->camData.nextEye.z = bossFd->actor.world.pos.z + 300.0f;
            }
            break;
        case DEATH_FD_SKULL:
            OoT_Math_ApproachF(&this->camData.nextAt.y, 100.0, 1.0f, 100.0f);
            this->camData.nextAt.x = 0.0f;
            this->camData.nextAt.z = 0.0f;
            this->camData.nextEye.x = 0.0f;
            this->camData.nextEye.y = 140.0f;
            OoT_Math_ApproachF(&this->camData.nextEye.z, 220.0f, 0.5f, 1.15f);
            if (bossFd->work[BFD_CAM_SHAKE_TIMER] != 0) {
                bossFd->work[BFD_CAM_SHAKE_TIMER]--;
                cameraShake = bossFd->work[BFD_CAM_SHAKE_TIMER] / 0.5f;
                if (cameraShake >= 20.0f) {
                    cameraShake = 20.0f;
                }
                this->camData.yMod = (bossFd->work[BFD_CAM_SHAKE_TIMER] & 1) ? cameraShake : -cameraShake;
            }
            if (bossFd->work[BFD_ACTION_STATE] == BOSSFD_SKULL_BURN) {
                this->deathState = DEATH_FINISH;
                mainCam->eye = this->camData.eye;
                mainCam->eyeNext = this->camData.eye;
                mainCam->at = this->camData.at;
                func_800C08AC(play, this->deathCamera, 0);
                this->deathCamera = 0;
                func_80064534(play, &play->csCtx);
                OoT_Player_SetCsActionWithHaltedActors(play, &this->actor, 7);
                if (GameInteractor_Should(VB_SPAWN_BLUE_WARP, true, this)) {
                    OoT_Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_DOOR_WARP1, 0.0f, 100.0f, 0.0f, 0, 0,
                                       0, WARP_DUNGEON_ADULT);
                }
                OoT_Flags_SetClear(play, play->roomCtx.curRoom.num);
            }
            break;
        case DEATH_FINISH:
            break;
    }
    BossFd2_UpdateCamera(this, play);
}

void BossFd2_Wait(BossFd2* this, PlayState* play) {
    BossFd* bossFd = (BossFd*)this->actor.parent;

    if (bossFd->handoffSignal == FD2_SIGNAL_GROUND) {
        bossFd->handoffSignal = FD2_SIGNAL_NONE;
        BossFd2_SetupEmerge(this, play);
        this->timers[0] = 20;
        this->work[FD2_HOLE_COUNTER] = 0;
    }
}

void BossFd2_CollisionCheck(BossFd2* this, PlayState* play) {
    s16 i;
    ColliderInfo* hurtbox;
    BossFd* bossFd = (BossFd*)this->actor.parent;

    if (this->actionFunc == BossFd2_ClawSwipe) {
        Player* player = GET_PLAYER(play);

        for (i = 0; i < ARRAY_COUNT(this->elements); i++) {
            if (this->collider.elements[i].info.toucherFlags & TOUCH_HIT) {
                this->collider.elements[i].info.toucherFlags &= ~TOUCH_HIT;
                Audio_PlayActorSound2(&player->actor, NA_SE_PL_BODY_HIT);
            }
        }
    }
    if (!bossFd->faceExposed) {
        this->collider.elements[0].info.elemType = ELEMTYPE_UNK2;
        this->collider.base.colType = COLTYPE_METAL;
    } else {
        this->collider.elements[0].info.elemType = ELEMTYPE_UNK3;
        this->collider.base.colType = COLTYPE_HIT3;
    }

    if (this->collider.elements[0].info.bumperFlags & BUMP_HIT) {
        this->collider.elements[0].info.bumperFlags &= ~BUMP_HIT;

        hurtbox = this->collider.elements[0].info.acHitInfo;
        if (!bossFd->faceExposed) {
            if (hurtbox->toucher.dmgFlags & 0x40000040) {
                bossFd->actor.colChkInfo.health -= 2;
                if ((s8)bossFd->actor.colChkInfo.health <= 2) {
                    bossFd->actor.colChkInfo.health = 1;
                }
                bossFd->faceExposed = true;
                BossFd2_SetupVulnerable(this, play);
                this->work[FD2_INVINC_TIMER] = 30;
                this->work[FD2_DAMAGE_FLASH_TIMER] = 5;
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_MAHI1);
                for (i = 0; i < 30; i++) {
                    Vec3f debrisVel = { 0.0f, 0.0f, 0.0f };
                    Vec3f debrisAccel = { 0.0f, -1.0f, 0.0f };
                    Vec3f debrisPos;

                    debrisVel.x = OoT_Rand_CenteredFloat(10.0f);
                    debrisVel.y = OoT_Rand_ZeroFloat(5.0f) + 8.0f;
                    debrisVel.z = OoT_Rand_CenteredFloat(10.0f);

                    debrisPos.x = this->actor.focus.pos.x;
                    debrisPos.y = this->actor.focus.pos.y;
                    debrisPos.z = this->actor.focus.pos.z;

                    BossFd2_SpawnDebris(play, bossFd->effects, &debrisPos, &debrisVel, &debrisAccel,
                                        (s16)OoT_Rand_ZeroFloat(10.0) + 10);
                }
            }
        } else {
            u8 canKill = false;
            u8 damage;

            if ((damage = CollisionCheck_GetSwordDamage(hurtbox->toucher.dmgFlags, play)) == 0) {
                damage = (hurtbox->toucher.dmgFlags & 0x00001000) ? 4 : 2;
            } else {
                canKill = true;
            }
            if (hurtbox->toucher.dmgFlags & 0x80) {
                damage = 0;
            }
            if (((s8)bossFd->actor.colChkInfo.health > 2) || canKill) {
                bossFd->actor.colChkInfo.health -= damage;
                osSyncPrintf(VT_FGCOL(GREEN));
                osSyncPrintf("damage   %d\n", damage);
            }
            osSyncPrintf(VT_RST);
            osSyncPrintf("hp %d\n", bossFd->actor.colChkInfo.health);

            if ((s8)bossFd->actor.colChkInfo.health <= 0) {
                bossFd->actor.colChkInfo.health = 0;
                BossFd2_SetupDeath(this, play);
                this->work[FD2_DAMAGE_FLASH_TIMER] = 10;
                this->work[FD2_INVINC_TIMER] = 30000;
                Audio_QueueSeqCmd(0x1 << 28 | SEQ_PLAYER_BGM_MAIN << 24 | 0x100FF);
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_DEAD);
                OoT_Enemy_StartFinishingBlow(play, &this->actor);
                GameInteractor_ExecuteOnBossDefeat(&this->actor);
            } else if (damage) {
                BossFd2_SetupDamaged(this, play);
                this->work[FD2_DAMAGE_FLASH_TIMER] = 10;
                this->work[FD2_INVINC_TIMER] = 100;
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_VALVAISA_DAMAGE1);
            }
            if (damage) {
                for (i = 0; i < 30; i++) {
                    Vec3f pieceVel = { 0.0f, 0.0f, 0.0f };
                    Vec3f pieceAccel = { 0.0f, -1.0f, 0.0f };
                    Vec3f piecePos;

                    pieceVel.x = OoT_Rand_CenteredFloat(6.0f);
                    pieceVel.y = OoT_Rand_ZeroFloat(4.0f) + 6.0f;
                    pieceVel.z = OoT_Rand_CenteredFloat(6.0f);

                    piecePos.x = this->actor.focus.pos.x;
                    piecePos.y = this->actor.focus.pos.y;
                    piecePos.z = this->actor.focus.pos.z;

                    BossFd2_SpawnSkullPiece(play, bossFd->effects, &piecePos, &pieceVel, &pieceAccel,
                                            (s16)OoT_Rand_ZeroFloat(6.0f) + 10);
                }
            }
        }
    }
}

void BossFd2_UpdateFace(BossFd2* this, PlayState* play) {
    f32 maxOpen;
    f32 openRate;
    s16 eyeStates[5] = { EYE_OPEN, EYE_HALF, EYE_CLOSED, EYE_CLOSED, EYE_HALF };

    if (((this->work[FD2_VAR_TIMER] % 8) == 0) && (OoT_Rand_ZeroOne() < 0.3f)) {
        this->work[FD2_BLINK_TIMER] = 4;
    }
    if ((this->actionFunc == BossFd2_Vulnerable) || (this->actionFunc == BossFd2_Damaged)) {
        if (this->work[FD2_VAR_TIMER] & 0x10) {
            this->eyeState = EYE_HALF;
        } else {
            this->eyeState = EYE_CLOSED;
        }
    } else {
        this->eyeState = eyeStates[this->work[FD2_BLINK_TIMER]];
    }

    if (this->work[FD2_BLINK_TIMER] != 0) {
        this->work[FD2_BLINK_TIMER]--;
    }

    if (this->work[FD2_SCREAM_TIMER] != 0) {
        maxOpen = 6000.0f;
        openRate = 1300.0f;
    } else {
        maxOpen = (this->work[FD2_VAR_TIMER] & 0x10) ? 1000.0f : 0.0f;
        openRate = 700.0f;
    }
    OoT_Math_ApproachF(&this->jawOpening, maxOpen, 0.3f, openRate);

    if (this->work[FD2_SCREAM_TIMER] != 0) {
        this->work[FD2_SCREAM_TIMER]--;
    }
}

void BossFd2_Update(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    BossFd2* this = (BossFd2*)thisx;
    s16 i;

    osSyncPrintf("FD2 move start \n");
    this->disableAT = false;
    this->actor.flags &= ~ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    this->work[FD2_VAR_TIMER]++;
    this->work[FD2_UNK_TIMER]++;

    this->actionFunc(this, play);

    for (i = 0; i < ARRAY_COUNT(this->timers); i++) {
        if (this->timers[i] != 0) {
            this->timers[i]--;
        }
    }
    if (this->work[FD2_DAMAGE_FLASH_TIMER] != 0) {
        this->work[FD2_DAMAGE_FLASH_TIMER]--;
    }
    if (this->work[FD2_INVINC_TIMER] != 0) {
        this->work[FD2_INVINC_TIMER]--;
    }

    if (this->deathState == DEATH_START) {
        if (this->work[FD2_INVINC_TIMER] == 0) {
            BossFd2_CollisionCheck(this, play);
        }
        OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
        if (!this->disableAT) {
            OoT_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        }
    }

    BossFd2_UpdateFace(this, play);
    this->fwork[FD2_TEX1_SCROLL_X] += 4.0f;
    this->fwork[FD2_TEX1_SCROLL_Y] = 120.0f;
    this->fwork[FD2_TEX2_SCROLL_X] += 3.0f;
    this->fwork[FD2_TEX2_SCROLL_Y] -= 2.0f;
    if (this->actor.focus.pos.y < 90.0f) {
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    } else {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    }
}

s32 BossFd2_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    BossFd2* this = (BossFd2*)thisx;
    BossFd* bossFd = (BossFd*)this->actor.parent;

    if (limbIndex == 31) {
        rot->y -= (f32)this->headRot.y;
        rot->z += (f32)this->headRot.x;
    }
    switch (limbIndex) {
        case 35:
        case 36:
            rot->z -= this->jawOpening * 0.1f;
            break;
        case 32:
            rot->z += this->jawOpening;
            break;
    }
    if ((bossFd->faceExposed == 1) && (limbIndex == 35)) {
        *dList = gHoleVolvagiaBrokenFaceDL;
    }

    if ((limbIndex == 32) || (limbIndex == 35) || (limbIndex == 36)) {
        OPEN_DISPS(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, (s8)bossFd->fwork[BFD_HEAD_TEX2_ALPHA]);
        CLOSE_DISPS(play->state.gfxCtx);
    } else {
        OPEN_DISPS(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, (s8)bossFd->fwork[BFD_BODY_TEX2_ALPHA]);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    if ((0 < limbIndex) && (limbIndex < 16)) {
        *dList = NULL;
    }
    return false;
}

void BossFd2_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f targetMod = { 4500.0f, 0.0f, 0.0f };
    static Vec3f headMod = { 4000.0f, 0.0f, 0.0f };
    static Vec3f centerManeMod = { 4000.0f, -2900.0, 2000.0f };
    static Vec3f rightManeMod = { 4000.0f, -1600.0, 0.0f };
    static Vec3f leftManeMod = { 4000.0f, -1600.0, -2000.0f };
    BossFd2* this = (BossFd2*)thisx;

    if (limbIndex == 35) {
        OoT_Matrix_MultVec3f(&targetMod, &this->actor.focus.pos);
        OoT_Matrix_MultVec3f(&headMod, &this->headPos);
        OoT_Matrix_MultVec3f(&centerManeMod, &this->centerMane.head);
        OoT_Matrix_MultVec3f(&rightManeMod, &this->rightMane.head);
        OoT_Matrix_MultVec3f(&leftManeMod, &this->leftMane.head);
    }
    OoT_Collider_UpdateSpheres(limbIndex, &this->collider);
}

void BossFd2_UpdateMane(BossFd2* this, PlayState* play, Vec3f* head, Vec3f* pos, Vec3f* rot, Vec3f* pull, f32* scale) {
    f32 sp138[10] = { 0.0f, 100.0f, 50.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    f32 sp110[10] = { 0.0f, 5.0f, -10.0f, 500.0f, 500.0f, 500.0f, 500.0f, 500.0f, 500.0f, 500.0f };
    f32 spE8[10] = { 0.4f, 0.6f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    s16 i;
    Vec3f temp_vec;
    f32 temp_f2;
    f32 phi_f0;
    f32 temp_angleX;
    f32 temp_angleY;
    Vec3f spBC;
    Vec3f spB0;
    f32 xyScale;

    OPEN_DISPS(play->state.gfxCtx);
    OoT_Matrix_Push();
    gDPPipeSync(POLY_OPA_DISP++);

    for (i = 0; i < 10; i++) {
        if (i == 0) {
            (pos + i)->x = head->x;
            (pos + i)->y = head->y;
            (pos + i)->z = head->z;
        } else {
            OoT_Math_ApproachF(&(pull + i)->x, 0.0f, 1.0f, 1.0f);
            OoT_Math_ApproachF(&(pull + i)->y, 0.0f, 1.0f, 1.0f);
            OoT_Math_ApproachF(&(pull + i)->z, 0.0f, 1.0f, 1.0f);
        }
    }

    for (i = 1; i < 10; i++) {
        temp_vec.x = (pos + i)->x + (pull + i)->x - (pos + i - 1)->x;

        phi_f0 = (pos + i)->y + (pull + i)->y - 2.0f + sp138[i];
        temp_f2 = (pos + i - 1)->y + sp110[i];
        if (phi_f0 > temp_f2) {
            phi_f0 = temp_f2;
        }
        if ((head->y >= -910.0f) && (phi_f0 < 110.0f)) {
            phi_f0 = 110.0f;
        }
        temp_vec.y = phi_f0 - (pos + i - 1)->y;

        temp_vec.z = (pos + i)->z + (pull + i)->z - (pos + i - 1)->z;
        temp_angleY = OoT_Math_Atan2F(temp_vec.z, temp_vec.x);
        temp_angleX = -OoT_Math_Atan2F(OoT_sqrtf(SQ(temp_vec.x) + SQ(temp_vec.z)), temp_vec.y);
        (rot + i - 1)->y = temp_angleY;
        (rot + i - 1)->x = temp_angleX;
        spBC.x = 0.0f;
        spBC.y = 0.0f;
        spBC.z = spE8[i] * 25.0f;
        Matrix_RotateY(temp_angleY, MTXMODE_NEW);
        Matrix_RotateX(temp_angleX, MTXMODE_APPLY);
        OoT_Matrix_MultVec3f(&spBC, &spB0);
        temp_vec.x = (pos + i)->x;
        temp_vec.y = (pos + i)->y;
        temp_vec.z = (pos + i)->z;
        (pos + i)->x = (pos + i - 1)->x + spB0.x;
        (pos + i)->y = (pos + i - 1)->y + spB0.y;
        (pos + i)->z = (pos + i - 1)->z + spB0.z;
        (pull + i)->x = (((pos + i)->x - temp_vec.x) * 88.0f) / 100.0f;
        (pull + i)->y = (((pos + i)->y - temp_vec.y) * 88.0f) / 100.0f;
        (pull + i)->z = (((pos + i)->z - temp_vec.z) * 88.0f) / 100.0f;
        if ((pull + i)->x > 30.0f) {
            (pull + i)->x = 30.0f;
        }
        if ((pull + i)->x < -30.0f) {
            (pull + i)->x = -30.0f;
        }
        if ((pull + i)->y > 30.0f) {
            (pull + i)->y = 30.0f;
        }
        if ((pull + i)->y < -30.0f) {
            (pull + i)->y = -30.0f;
        }
        if ((pull + i)->z > 30.0f) {
            (pull + i)->z = 30.0f;
        }
        if ((pull + i)->z < -30.0f) {
            (pull + i)->z = -30.0f;
        }
    }

    for (i = 0; i < 9; i++) {
        FrameInterpolation_RecordOpenChild(this, this->epoch + i * 25);

        OoT_Matrix_Translate((pos + i)->x, (pos + i)->y, (pos + i)->z, MTXMODE_NEW);
        Matrix_RotateY((rot + i)->y, MTXMODE_APPLY);
        Matrix_RotateX((rot + i)->x, MTXMODE_APPLY);
        xyScale = (0.01f - (i * 0.0009f)) * spE8[i] * scale[i];
        OoT_Matrix_Scale(xyScale, xyScale, 0.01f * spE8[i], MTXMODE_APPLY);
        Matrix_RotateX(M_PI / 2.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gHoleVolvagiaManeModelDL);

        FrameInterpolation_RecordCloseChild();
    }
    OoT_Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

void BossFd2_DrawMane(BossFd2* this, PlayState* play) {
    s32 pad;
    BossFd* bossFd = (BossFd*)this->actor.parent;
    s16 i;

    OPEN_DISPS(play->state.gfxCtx);
    for (i = 0; i < 10; i++) {
        this->centerMane.scale[i] = 1.5f + 0.3f * OoT_Math_SinS(5596.0f * this->work[FD2_VAR_TIMER] + i * 0x3200);
        this->rightMane.scale[i] = 1.5f + 0.3f * OoT_Math_SinS(5496.0f * this->work[FD2_VAR_TIMER] + i * 0x3200);
        this->leftMane.scale[i] = 1.5f + 0.3f * OoT_Math_CosS(5696.0f * this->work[FD2_VAR_TIMER] + i * 0x3200);
    }

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPDisplayList(POLY_XLU_DISP++, gHoleVolvagiaManeMaterialDL);

    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, bossFd->fwork[BFD_MANE_COLOR_CENTER], 0, 255);
    BossFd2_UpdateMane(this, play, &this->centerMane.head, this->centerMane.pos, this->centerMane.rot,
                       this->centerMane.pull, this->centerMane.scale);

    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, bossFd->fwork[BFD_MANE_COLOR_RIGHT], 0, 255);
    BossFd2_UpdateMane(this, play, &this->rightMane.head, this->rightMane.pos, this->rightMane.rot,
                       this->rightMane.pull, this->rightMane.scale);

    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, bossFd->fwork[BFD_MANE_COLOR_LEFT], 0, 255);
    BossFd2_UpdateMane(this, play, &this->leftMane.head, this->leftMane.pos, this->leftMane.rot, this->leftMane.pull,
                       this->leftMane.scale);

    CLOSE_DISPS(play->state.gfxCtx);
}

void BossFd2_Draw(Actor* thisx, PlayState* play) {
    static void* eyeTextures[] = { gHoleVolvagiaEyeOpenTex, gHoleVolvagiaEyeHalfTex, gHoleVolvagiaEyeClosedTex };
    s32 pad;
    BossFd2* this = (BossFd2*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    osSyncPrintf("FD2 draw start \n");
    if (this->actionFunc != BossFd2_Wait) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        if (this->work[FD2_DAMAGE_FLASH_TIMER] & 2) {
            POLY_OPA_DISP = OoT_Gfx_SetFog(POLY_OPA_DISP, 255, 255, 255, 0, 900, 1099);
        }
        gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(eyeTextures[this->eyeState]));

        gSPSegment(POLY_OPA_DISP++, 0x08,
                   OoT_Gfx_TwoTexScroll(play->state.gfxCtx, 0, (s16)this->fwork[FD2_TEX1_SCROLL_X],
                                    (s16)this->fwork[FD2_TEX1_SCROLL_Y], 0x20, 0x20, 1,
                                    (s16)this->fwork[FD2_TEX2_SCROLL_X], (s16)this->fwork[FD2_TEX2_SCROLL_Y], 0x20,
                                    0x20));
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 128);

        SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, BossFd2_OverrideLimbDraw, BossFd2_PostLimbDraw, &this->actor);
        BossFd2_DrawMane(this, play);
        POLY_OPA_DISP = OoT_Play_SetFog(play, POLY_OPA_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
