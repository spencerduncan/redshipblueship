#include "z_en_ny.h"
#include "objects/object_ny/object_ny.h"
#include "soh/frame_interpolation.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE)

void EnNy_Init(Actor* thisx, PlayState* play);
void EnNy_Destroy(Actor* thisx, PlayState* play);
void EnNy_Update(Actor* thisx, PlayState* play);
void EnNy_Draw(Actor* thisx, PlayState* play);

void EnNy_UpdateUnused(Actor* thisx, PlayState* play);
void EnNy_Move(EnNy* this, PlayState* play);
void EnNy_Die(EnNy* this, PlayState* play);
void func_80ABCD40(EnNy* this);
void func_80ABCDBC(EnNy* this);
void EnNy_TurnToStone(EnNy* this, PlayState* play);
void func_80ABD11C(EnNy* this, PlayState* play);
void func_80ABCE50(EnNy* this, PlayState* play);
void func_80ABCE90(EnNy* this, PlayState* play);
void func_80ABCEEC(EnNy* this, PlayState* play);
void EnNy_UpdateDeath(Actor* thisx, PlayState* PlayState);
void EnNy_SetupDie(EnNy* this, PlayState* play);
void EnNy_DrawDeathEffect(Actor* thisx, PlayState* PlayState);
void func_80ABD3B8(EnNy* this, f32, f32);

const ActorInit En_Ny_InitVars = {
    ACTOR_EN_NY,
    ACTORCAT_ENEMY,
    FLAGS,
    OBJECT_NY,
    sizeof(EnNy),
    (ActorFunc)EnNy_Init,
    (ActorFunc)EnNy_Destroy,
    (ActorFunc)EnNy_Update,
    (ActorFunc)EnNy_Draw,
    NULL,
};

static ColliderJntSphElementInit OoT_sJntSphElementsInit[1] = {
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x04, 0x08 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_ON | TOUCH_SFX_NORMAL,
            BUMP_ON,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 15 }, 100 },
    },
};

static ColliderJntSphInit sColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    1,
    OoT_sJntSphElementsInit,
};

static DamageTable OoT_sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, 0x0),
    /* Deku stick    */ DMG_ENTRY(0, 0x0),
    /* Slingshot     */ DMG_ENTRY(0, 0x0),
    /* Explosive     */ DMG_ENTRY(2, 0xF),
    /* Boomerang     */ DMG_ENTRY(0, 0x0),
    /* Normal arrow  */ DMG_ENTRY(2, 0xF),
    /* Hammer swing  */ DMG_ENTRY(2, 0xF),
    /* Hookshot      */ DMG_ENTRY(2, 0x1),
    /* Kokiri sword  */ DMG_ENTRY(0, 0x0),
    /* Master sword  */ DMG_ENTRY(2, 0xF),
    /* Giant's Knife */ DMG_ENTRY(4, 0xF),
    /* Fire arrow    */ DMG_ENTRY(4, 0x2),
    /* Ice arrow     */ DMG_ENTRY(2, 0xF),
    /* Light arrow   */ DMG_ENTRY(2, 0xF),
    /* Unk arrow 1   */ DMG_ENTRY(4, 0xE),
    /* Unk arrow 2   */ DMG_ENTRY(0, 0x0),
    /* Unk arrow 3   */ DMG_ENTRY(0, 0x0),
    /* Fire magic    */ DMG_ENTRY(4, 0x2),
    /* Ice magic     */ DMG_ENTRY(0, 0x0),
    /* Light magic   */ DMG_ENTRY(0, 0x0),
    /* Shield        */ DMG_ENTRY(0, 0x0),
    /* Mirror Ray    */ DMG_ENTRY(0, 0x0),
    /* Kokiri spin   */ DMG_ENTRY(0, 0x0),
    /* Giant spin    */ DMG_ENTRY(4, 0xF),
    /* Master spin   */ DMG_ENTRY(2, 0xF),
    /* Kokiri jump   */ DMG_ENTRY(0, 0x0),
    /* Giant jump    */ DMG_ENTRY(8, 0xF),
    /* Master jump   */ DMG_ENTRY(4, 0xF),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(0, 0x0),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_S8(naviEnemyId, 0x28, ICHAIN_CONTINUE),
    ICHAIN_U8(targetMode, 2, ICHAIN_CONTINUE),
    ICHAIN_F32(targetArrowOffset, 30, ICHAIN_STOP),
};

void EnNy_Init(Actor* thisx, PlayState* play) {
    EnNy* this = (EnNy*)thisx;

    this->epoch++;
    OoT_Actor_ProcessInitChain(&this->actor, OoT_sInitChain);
    this->actor.colChkInfo.damageTable = &OoT_sDamageTable;
    this->actor.colChkInfo.health = 2;
    OoT_Collider_InitJntSph(play, &this->collider);
    OoT_Collider_SetJntSph(play, &this->collider, &this->actor, &sColliderInit, this->elements);
    OoT_ActorShape_Init(&this->actor.shape, 0.0f, OoT_ActorShadow_DrawCircle, 20.0f);
    this->unk_1CA = 0;
    this->unk_1D0 = 0;
    OoT_Actor_SetScale(&this->actor, 0.01f);
    this->actor.speedXZ = 0.0f;
    this->actor.shape.rot.y = 0;
    this->actor.gravity = -0.4f;
    this->hitPlayer = 0;
    this->unk_1CE = 2;
    this->actor.velocity.y = 0.0f;
    this->unk_1D4 = 0xFF;
    this->unk_1D8 = 0;
    this->unk_1E8 = 0.0f;
    this->unk_1E0 = 0.25f;
    if (this->actor.params == 0) {
        // "New initials"
        osSyncPrintf("ニュウ イニシャル[ %d ] ！！\n", this->actor.params);
        this->actor.colChkInfo.mass = 0;
        this->unk_1D4 = 0;
        this->unk_1D8 = 0xFF;
        this->unk_1E0 = 1.0f;
        func_80ABCDBC(this);
    } else {
        // This mode is unused in the final game
        // "Dummy new initials"
        osSyncPrintf("ダミーニュウ イニシャル[ %d ] ！！\n", this->actor.params);
        osSyncPrintf("En_Ny_actor_move2[ %x ] ！！\n", EnNy_UpdateUnused);
        this->actor.colChkInfo.mass = 0xFF;
        this->collider.base.colType = COLTYPE_METAL;
        this->actor.update = EnNy_UpdateUnused;
    }
}

void EnNy_Destroy(Actor* thisx, PlayState* play) {
    EnNy* this = (EnNy*)thisx;
    OoT_Collider_DestroyJntSph(play, &this->collider);
}

void func_80ABCD40(EnNy* this) {
    f32 temp;

    temp = (this->actor.yDistToWater > 0.0f) ? 0.7f : 1.0f;
    this->unk_1E8 = 2.8f * temp;
}

void func_80ABCD84(EnNy* this) {
    this->actionFunc = func_80ABCE50;
}

void func_80ABCD94(EnNy* this) {
    this->stoneTimer = 0x14;
    this->actionFunc = func_80ABCE90;
}

void func_80ABCDAC(EnNy* this) {
    this->actionFunc = func_80ABCEEC;
}

void func_80ABCDBC(EnNy* this) {
    this->unk_1F4 = 0.0f;
    func_80ABCD40(this);
    this->stoneTimer = 180;
    this->actionFunc = EnNy_Move;
}

void EnNy_SetupTurnToStone(EnNy* this) {
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_NYU_HIT_STOP);
    this->actionFunc = EnNy_TurnToStone;
    this->unk_1E8 = 0.0f;
}

void func_80ABCE38(EnNy* this) {
    this->stoneTimer = 0x3C;
    this->actionFunc = func_80ABD11C;
}

void func_80ABCE50(EnNy* this, PlayState* play) {
    if (this->actor.xyzDistToPlayerSq <= 25600.0f) {
        func_80ABCD94(this);
    }
}

void func_80ABCE90(EnNy* this, PlayState* play) {
    s32 phi_v1;
    s32 phi_v0;

    phi_v1 = this->unk_1D4 - 0x40;
    phi_v0 = this->unk_1D8 + 0x40;
    if (phi_v0 >= 0xFF) {
        phi_v1 = 0;
        phi_v0 = 0xFF;
        func_80ABCDAC(this);
    }
    this->unk_1D4 = phi_v1;
    this->unk_1D8 = phi_v0;
}

void func_80ABCEEC(EnNy* this, PlayState* play) {
    f32 phi_f0;

    phi_f0 = this->unk_1E0;
    phi_f0 += 2.0f;
    if (phi_f0 >= 1.0f) {
        phi_f0 = 1.0f;
        func_80ABCDBC(this);
    }
    this->unk_1E0 = phi_f0;
}

void EnNy_Move(EnNy* this, PlayState* play) {
    f32 yawDiff;
    s32 stoneTimer;

    if (!(this->unk_1F0 < this->actor.yDistToWater)) {
        func_8002F974(&this->actor, NA_SE_EN_NYU_MOVE - SFX_FLAG);
    }
    func_80ABCD40(this);
    stoneTimer = this->stoneTimer;
    this->stoneTimer--;
    if ((stoneTimer <= 0) || (this->hitPlayer != false)) {
        EnNy_SetupTurnToStone(this);
    } else {
        OoT_Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, this->unk_1F4, 0);
        OoT_Math_ApproachF(&this->unk_1F4, 2000.0f, 1.0f, 100.0f);
        this->actor.world.rot.y = this->actor.shape.rot.y;
        yawDiff = OoT_Math_FAtan2F(this->actor.yDistToPlayer, this->actor.xzDistToPlayer);
        this->actor.speedXZ = fabsf(cosf(yawDiff) * this->unk_1E8);
        if (this->unk_1F0 < this->actor.yDistToWater) {
            this->unk_1EC = sinf(yawDiff) * this->unk_1E8;
        }
    }
}

void EnNy_TurnToStone(EnNy* this, PlayState* play) {
    f32 phi_f0;

    phi_f0 = this->unk_1E0;
    phi_f0 -= 2.0f;
    if (phi_f0 <= 0.25f) {
        phi_f0 = 0.25f;
        if (this->actor.bgCheckFlags & 2) {
            if (!(this->unk_1F0 < this->actor.yDistToWater)) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_DODO_M_GND);
            }
            this->actor.bgCheckFlags &= ~2;
            this->actor.speedXZ = 0.0f;
            this->actor.world.rot.y = this->actor.shape.rot.y;
            func_80ABCE38(this);
        }
    }
    this->unk_1E0 = phi_f0;
}

void func_80ABD11C(EnNy* this, PlayState* play) {
    s32 phi_v0;
    s32 phi_v1;

    phi_v0 = this->unk_1D4;
    phi_v0 += 0x40;
    phi_v1 = this->unk_1D8;
    phi_v1 -= 0x40;
    if (phi_v0 >= 0xFF) {
        phi_v0 = 0xFF;
        phi_v1 = 0;
        if (this->stoneTimer != 0) {
            this->stoneTimer--;
        } else {
            func_80ABCD84(this);
        }
    }
    this->unk_1D4 = phi_v0;
    this->unk_1D8 = phi_v1;
}

s32 EnNy_CollisionCheck(EnNy* this, PlayState* play) {
    u8 sp3F;
    Vec3f effectPos;

    sp3F = 0;
    this->hitPlayer = 0;
    if (this->collider.base.atFlags & 4) {
        this->collider.base.atFlags &= ~4;
        this->hitPlayer = 1;
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        this->actor.speedXZ = -4.0f;
        return 0;
    }
    if (this->collider.base.atFlags & 2) {
        this->collider.base.atFlags &= ~2;
        this->hitPlayer = 1;
        return 0;
    } else {
        if (this->collider.base.acFlags & 2) {
            this->collider.base.acFlags &= ~2;
            effectPos.x = this->collider.elements[0].info.bumper.hitPos.x;
            effectPos.y = this->collider.elements[0].info.bumper.hitPos.y;
            effectPos.z = this->collider.elements[0].info.bumper.hitPos.z;
            if ((this->unk_1E0 == 0.25f) && (this->unk_1D4 == 0xFF)) {
                switch (this->actor.colChkInfo.damageEffect) {
                    case 0xE:
                        sp3F = 1;
                    case 0xF:
                        OoT_Actor_ApplyDamage(&this->actor);
                        OoT_Actor_SetColorFilter(&this->actor, 0x4000, 0xFF, 0x2000, 0x50);
                        break;
                    case 1:
                        OoT_Actor_ApplyDamage(&this->actor);
                        OoT_Actor_SetColorFilter(&this->actor, 0x4000, 0xFF, 0x2000, 0x50);
                        break;
                    case 2:
                        this->unk_1CA = 4;
                        OoT_Actor_ApplyDamage(&this->actor);
                        OoT_Actor_SetColorFilter(&this->actor, 0x4000, 0xFF, 0x2000, 0x50);
                        break;
                }
            }
            this->stoneTimer = 0;
            if (this->actor.colChkInfo.health == 0) {
                this->actor.shape.shadowAlpha = 0;
                this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
                this->unk_1D0 = sp3F;
                OoT_Enemy_StartFinishingBlow(play, &this->actor);
                return 1;
            }
            EffectSsHitMark_SpawnFixedScale(play, 0, &effectPos);
            return 0;
        }
    }
    return 0;
}

void func_80ABD3B8(EnNy* this, f32 arg1, f32 arg2) {
    if (this->unk_1E8 == 0.0f) {
        this->actor.gravity = -0.4f;
    } else if (!(arg1 < this->actor.yDistToWater)) {
        this->actor.gravity = -0.4f;
    } else if (arg2 < this->actor.yDistToWater) {
        this->actor.gravity = 0.0;
        if (this->unk_1EC < this->actor.velocity.y) {
            this->actor.velocity.y -= 0.4f;
            if (this->actor.velocity.y < this->unk_1EC) {
                this->actor.velocity.y = this->unk_1EC;
            }
        } else if (this->actor.velocity.y < this->unk_1EC) {
            this->actor.velocity.y += 0.4f;
            if (this->unk_1EC < this->actor.velocity.y) {
                this->actor.velocity.y = this->unk_1EC;
            }
        }
    }
}

void EnNy_Update(Actor* thisx, PlayState* play) {
    EnNy* this = (EnNy*)thisx;
    f32 temp_f20;
    f32 temp_f22;
    s32 i;

    this->timer++;
    temp_f20 = this->unk_1E0 - 0.25f;
    if (this->unk_1CA != 0) {
        this->unk_1CA--;
    }
    OoT_Actor_SetFocus(&this->actor, 0.0f);
    OoT_Actor_SetScale(&this->actor, 0.01f);
    this->collider.elements[0].dim.scale = 1.33f * temp_f20 + 1.0f;
    temp_f22 = (24.0f * temp_f20) + 12.0f;
    this->actor.shape.rot.x += (s16)(this->unk_1E8 * 1000.0f);
    func_80ABD3B8(this, temp_f22 + 10.0f, temp_f22 - 10.0f);
    Actor_MoveXZGravity(&this->actor);
    OoT_Math_StepToF(&this->unk_1E4, this->unk_1E8, 0.1f);
    this->actionFunc(this, play);
    this->actor.prevPos.y -= temp_f22;
    this->actor.world.pos.y -= temp_f22;
    OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 60.0f, 7);
    this->unk_1F0 = temp_f22;
    this->actor.world.pos.y += temp_f22;
    if (EnNy_CollisionCheck(this, play) != 0) {
        for (i = 0; i < 8; i++) {
            this->unk_1F8[i].x = (OoT_Rand_CenteredFloat(20.0f) + this->actor.world.pos.x);
            this->unk_1F8[i].y = (OoT_Rand_CenteredFloat(20.0f) + this->actor.world.pos.y);
            this->unk_1F8[i].z = (OoT_Rand_CenteredFloat(20.0f) + this->actor.world.pos.z);
        }
        this->timer = 0;
        this->actor.update = EnNy_UpdateDeath;
        this->actor.draw = EnNy_DrawDeathEffect;
        this->actionFunc = EnNy_SetupDie;
        return;
    }
    if (this->unk_1E0 > 0.25f) {
        OoT_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

void EnNy_SetupDie(EnNy* this, PlayState* play) {
    s32 effectScale;
    s32 i;
    Vec3f effectPos;
    Vec3f effectVelocity = { 0.0f, 0.0f, 0.0f };
    Vec3f effectAccel = { 0.0f, 0.1f, 0.0f };

    if (this->timer >= 2) {
        if (this->actor.yDistToWater > 0.0f) {
            for (i = 0; i < 10; i++) {
                effectPos.x = OoT_Rand_CenteredFloat(30.0f) + this->actor.world.pos.x;
                effectPos.y = OoT_Rand_CenteredFloat(30.0f) + this->actor.world.pos.y;
                effectPos.z = OoT_Rand_CenteredFloat(30.0f) + this->actor.world.pos.z;
                effectScale = OoT_Rand_S16Offset(0x50, 0x64);
                OoT_EffectSsDtBubble_SpawnColorProfile(play, &effectPos, &effectVelocity, &effectAccel, effectScale, 25, 0,
                                                   1);
            }
            for (i = 0; i < 0x14; i++) {
                effectPos.x = OoT_Rand_CenteredFloat(30.0f) + this->actor.world.pos.x;
                effectPos.y = OoT_Rand_CenteredFloat(30.0f) + this->actor.world.pos.y;
                effectPos.z = OoT_Rand_CenteredFloat(30.0f) + this->actor.world.pos.z;
                OoT_EffectSsBubble_Spawn(play, &effectPos, 10.0f, 10.0f, 30.0f, 0.25f);
            }
        }
        for (i = 0; i < 8; i++) {
            this->unk_1F8[i + 8].x = OoT_Rand_CenteredFloat(10.0f);
            this->unk_1F8[i + 8].z = OoT_Rand_CenteredFloat(10.0f);
            this->unk_1F8[i + 8].y = OoT_Rand_ZeroFloat(4.0f) + 4.0f;
        }
        this->timer = 0;
        if (this->unk_1D0 == 0) {
            OoT_Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xA0);
        } else {
            OoT_Item_DropCollectible(play, &this->actor.world.pos, 8);
        }
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_NYU_DEAD);
        this->actionFunc = EnNy_Die;
        GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
    }
}

void EnNy_Die(EnNy* this, PlayState* play) {
    s32 i;

    if (this->actor.yDistToWater > 0.0f) {
        for (i = 0; i < 8; i += 1) {
            this->unk_1F8[i].x += this->unk_1F8[i + 8].x;
            this->unk_1F8[i].y += this->unk_1F8[i + 8].y;
            this->unk_1F8[i].z += this->unk_1F8[i + 8].z;
            OoT_Math_StepToF(&this->unk_1F8[i + 8].x, 0.0f, 0.1f);
            OoT_Math_StepToF(&this->unk_1F8[i + 8].y, -1.0f, 0.4f);
            OoT_Math_StepToF(&this->unk_1F8[i + 8].z, 0.0f, 0.1f);
        }
        if (this->timer >= 0x1F) {
            OoT_Actor_Kill(&this->actor);
            return;
        }
    } else {
        for (i = 0; i < 8; i += 1) {
            this->unk_1F8[i].x += this->unk_1F8[i + 8].x;
            this->unk_1F8[i].y += this->unk_1F8[i + 8].y;
            this->unk_1F8[i].z += this->unk_1F8[i + 8].z;
            OoT_Math_StepToF(&this->unk_1F8[i + 8].x, 0.0f, 0.15f);
            OoT_Math_StepToF(&this->unk_1F8[i + 8].y, -1.0f, 0.6f);
            OoT_Math_StepToF(&this->unk_1F8[i + 8].z, 0.0f, 0.15f);
        }
        if (this->timer >= 0x10) {
            OoT_Actor_Kill(&this->actor);
            return;
        }
    }
}

void EnNy_UpdateDeath(Actor* thisx, PlayState* play) {
    EnNy* this = (EnNy*)thisx;

    this->timer++;
    if (this->unk_1CA != 0) {
        this->unk_1CA--;
    }
    this->actionFunc(this, play);
}

void EnNy_UpdateUnused(Actor* thisx, PlayState* play2) {
    EnNy* this = (EnNy*)thisx;
    PlayState* play = play2;
    f32 sp3C;
    f32 temp_f0;

    sp3C = this->unk_1E0 - 0.25f;
    this->timer++;
    OoT_Actor_SetFocus(&this->actor, 0.0f);
    OoT_Actor_SetScale(&this->actor, 0.01f);
    temp_f0 = (24.0f * sp3C) + 12.0f;
    this->actor.prevPos.y -= temp_f0;
    this->actor.world.pos.y -= temp_f0;

    OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 60.0f, 7);
    this->unk_1F0 = temp_f0;
    this->actor.world.pos.y += temp_f0;

    OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    Actor_MoveXZGravity(&this->actor);
    OoT_Math_StepToF(&this->unk_1E4, this->unk_1E8, 0.1f);
}
static Vec3f sFireOffsets[] = {
    { 5.0f, 0.0f, 0.0f },
    { -5.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 5.0f },
    { 0.0f, 0.0f, -5.0f },
};

void EnNy_Draw(Actor* thisx, PlayState* play) {
    s32 pad;
    EnNy* this = (EnNy*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    OoT_Collider_UpdateSpheres(0, &this->collider);
    func_8002ED80(&this->actor, play, 1);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->unk_1D8);
    gSPDisplayList(POLY_XLU_DISP++, gEnNyMetalBodyDL);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->unk_1D4);
    gSPDisplayList(POLY_XLU_DISP++, gEnNyRockBodyDL);
    if (this->unk_1E0 > 0.25f) {
        OoT_Matrix_Scale(this->unk_1E0, this->unk_1E0, this->unk_1E0, MTXMODE_APPLY);
        func_8002EBCC(&this->actor, play, 1);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, gEnNySpikeDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    if (this->unk_1CA != 0) {
        Vec3f tempVec;
        Vec3f* fireOffset;
        s16 temp;

        temp = this->unk_1CA - 1;
        this->actor.colorFilterTimer++;
        if (temp == 0) {
            fireOffset = &sFireOffsets[temp & 3];
            tempVec.x = OoT_Rand_CenteredFloat(5.0f) + (this->actor.world.pos.x + fireOffset->x);
            tempVec.y = OoT_Rand_CenteredFloat(5.0f) + (this->actor.world.pos.y + fireOffset->y);
            tempVec.z = OoT_Rand_CenteredFloat(5.0f) + (this->actor.world.pos.z + fireOffset->z);
            OoT_EffectSsEnFire_SpawnVec3f(play, &this->actor, &tempVec, 100, 0, 0, -1);
        }
    }
}

void EnNy_DrawDeathEffect(Actor* thisx, PlayState* play) {
    EnNy* this = (EnNy*)thisx;
    Vec3f* temp;
    f32 scale;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, 0x00, 0x00, 0x00, 0xFF);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_SURF2);
    gDPPipeSync(POLY_OPA_DISP++);
    for (i = 0; i < 8; i++) {
        if (this->timer < (i + 22)) {
            FrameInterpolation_RecordOpenChild(this, this->epoch + i * 25);
            temp = &this->unk_1F8[i];
            OoT_Matrix_Translate(temp->x, temp->y, temp->z, MTXMODE_NEW);
            scale = this->actor.scale.x * 0.4f * (1.0f + (i * 0.04f));
            OoT_Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, gEnNyRockBodyDL);
            FrameInterpolation_RecordCloseChild();
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
    if (this->unk_1CA != 0) {
        Vec3f tempVec;
        Vec3f* fireOffset;
        s16 fireOffsetIndex;

        fireOffsetIndex = this->unk_1CA - 1;
        this->actor.colorFilterTimer++;
        if ((fireOffsetIndex & 1) == 0) {
            fireOffset = &sFireOffsets[fireOffsetIndex & 3];
            tempVec.x = OoT_Rand_CenteredFloat(5.0f) + (this->actor.world.pos.x + fireOffset->x);
            tempVec.y = OoT_Rand_CenteredFloat(5.0f) + (this->actor.world.pos.y + fireOffset->y);
            tempVec.z = OoT_Rand_CenteredFloat(5.0f) + (this->actor.world.pos.z + fireOffset->z);
            OoT_EffectSsEnFire_SpawnVec3f(play, &this->actor, &tempVec, 100, 0, 0, -1);
        }
    }
}
