#include "z_en_okuta.h"
#include "objects/object_okuta/object_okuta.h"
#include "objects/gameplay_field_keep/gameplay_field_keep.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ResourceManagerHelpers.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE)

void OoT_EnOkuta_Init(Actor* thisx, PlayState* play);
void OoT_EnOkuta_Destroy(Actor* thisx, PlayState* play);
void OoT_EnOkuta_Update(Actor* thisx, PlayState* play);
void OoT_EnOkuta_Draw(Actor* thisx, PlayState* play);

void OoT_EnOkuta_SetupWaitToAppear(EnOkuta* this);
void OoT_EnOkuta_WaitToAppear(EnOkuta* this, PlayState* play);
void OoT_EnOkuta_Appear(EnOkuta* this, PlayState* play);
void OoT_EnOkuta_Hide(EnOkuta* this, PlayState* play);
void EnOkuta_WaitToShoot(EnOkuta* this, PlayState* play);
void OoT_EnOkuta_Shoot(EnOkuta* this, PlayState* play);
void EnOkuta_WaitToDie(EnOkuta* this, PlayState* play);
void OoT_EnOkuta_Die(EnOkuta* this, PlayState* play);
void OoT_EnOkuta_Freeze(EnOkuta* this, PlayState* play);
void EnOkuta_ProjectileFly(EnOkuta* this, PlayState* play);

const ActorInit En_Okuta_InitVars = {
    ACTOR_EN_OKUTA,
    ACTORCAT_ENEMY,
    FLAGS,
    OBJECT_OKUTA,
    sizeof(EnOkuta),
    (ActorFunc)OoT_EnOkuta_Init,
    (ActorFunc)OoT_EnOkuta_Destroy,
    (ActorFunc)OoT_EnOkuta_Update,
    (ActorFunc)OoT_EnOkuta_Draw,
    NULL,
};

static ColliderCylinderInit sProjectileColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x08 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_HARD,
        BUMP_ON,
        OCELEM_ON,
    },
    { 13, 20, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sOctorockColliderInit = {
    {
        COLTYPE_HIT0,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK1,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 20, 40, -30, { 0, 0, 0 } },
};

static CollisionCheckInfoInit OoT_sColChkInfoInit = { 1, 15, 60, 100 };

static DamageTable OoT_sDamageTable = {
    /* Deku nut      */ DMG_ENTRY(0, 0x0),
    /* Deku stick    */ DMG_ENTRY(2, 0x0),
    /* Slingshot     */ DMG_ENTRY(1, 0x0),
    /* Explosive     */ DMG_ENTRY(2, 0x0),
    /* Boomerang     */ DMG_ENTRY(1, 0x0),
    /* Normal arrow  */ DMG_ENTRY(2, 0x0),
    /* Hammer swing  */ DMG_ENTRY(2, 0x0),
    /* Hookshot      */ DMG_ENTRY(2, 0x0),
    /* Kokiri sword  */ DMG_ENTRY(1, 0x0),
    /* Master sword  */ DMG_ENTRY(2, 0x0),
    /* Giant's Knife */ DMG_ENTRY(4, 0x0),
    /* Fire arrow    */ DMG_ENTRY(2, 0x0),
    /* Ice arrow     */ DMG_ENTRY(4, 0x3),
    /* Light arrow   */ DMG_ENTRY(2, 0x0),
    /* Unk arrow 1   */ DMG_ENTRY(2, 0x0),
    /* Unk arrow 2   */ DMG_ENTRY(2, 0x0),
    /* Unk arrow 3   */ DMG_ENTRY(2, 0x0),
    /* Fire magic    */ DMG_ENTRY(0, 0x0),
    /* Ice magic     */ DMG_ENTRY(0, 0x0),
    /* Light magic   */ DMG_ENTRY(0, 0x0),
    /* Shield        */ DMG_ENTRY(0, 0x0),
    /* Mirror Ray    */ DMG_ENTRY(0, 0x0),
    /* Kokiri spin   */ DMG_ENTRY(1, 0x0),
    /* Giant spin    */ DMG_ENTRY(4, 0x0),
    /* Master spin   */ DMG_ENTRY(2, 0x0),
    /* Kokiri jump   */ DMG_ENTRY(2, 0x0),
    /* Giant jump    */ DMG_ENTRY(8, 0x0),
    /* Master jump   */ DMG_ENTRY(4, 0x0),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(4, 0x0),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_S8(naviEnemyId, 0x42, ICHAIN_CONTINUE),
    ICHAIN_F32(targetArrowOffset, 6500, ICHAIN_STOP),
};

void OoT_EnOkuta_Init(Actor* thisx, PlayState* play) {
    EnOkuta* this = (EnOkuta*)thisx;
    s32 pad;
    WaterBox* outWaterBox;
    f32 ySurface;
    s32 sp30;

    OoT_Actor_ProcessInitChain(thisx, OoT_sInitChain);
    this->numShots = (thisx->params >> 8) & 0xFF;
    thisx->params &= 0xFF;
    if (thisx->params == 0) {
        OoT_SkelAnime_Init(play, &this->skelAnime, &gOctorokSkel, &gOctorokAppearAnim, this->jointTable, this->morphTable,
                       38);
        OoT_Collider_InitCylinder(play, &this->collider);
        OoT_Collider_SetCylinder(play, &this->collider, thisx, &sOctorockColliderInit);
        OoT_CollisionCheck_SetInfo(&thisx->colChkInfo, &OoT_sDamageTable, &OoT_sColChkInfoInit);
        if ((this->numShots == 0xFF) || (this->numShots == 0)) {
            this->numShots = 1;
        }
        thisx->floorHeight =
            BgCheck_EntityRaycastFloor4(&play->colCtx, &thisx->floorPoly, &sp30, thisx, &thisx->world.pos);
        //! @bug calls OoT_WaterBox_GetSurfaceImpl directly
        if (!OoT_WaterBox_GetSurfaceImpl(play, &play->colCtx, thisx->world.pos.x, thisx->world.pos.z, &ySurface,
                                     &outWaterBox) ||
            (ySurface <= thisx->floorHeight)) {
            OoT_Actor_Kill(thisx);
        } else {
            thisx->home.pos.y = ySurface;
        }
        OoT_EnOkuta_SetupWaitToAppear(this);
    } else {
        OoT_ActorShape_Init(&thisx->shape, 1100.0f, OoT_ActorShadow_DrawCircle, 18.0f);
        thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        thisx->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        OoT_Collider_InitCylinder(play, &this->collider);
        OoT_Collider_SetCylinder(play, &this->collider, thisx, &sProjectileColliderInit);
        OoT_Actor_ChangeCategory(play, &play->actorCtx, thisx, ACTORCAT_PROP);
        this->timer = 30;
        thisx->shape.rot.y = 0;
        this->actionFunc = EnOkuta_ProjectileFly;
        thisx->speedXZ = 10.0f;
    }
}

void OoT_EnOkuta_Destroy(Actor* thisx, PlayState* play) {
    EnOkuta* this = (EnOkuta*)thisx;

    OoT_Collider_DestroyCylinder(play, &this->collider);

    if (thisx->params == 0) {
        ResourceMgr_UnregisterSkeleton(&this->skelAnime);
    }
}

void OoT_EnOkuta_SpawnBubbles(EnOkuta* this, PlayState* play) {
    s32 i;

    for (i = 0; i < 10; i++) {
        OoT_EffectSsBubble_Spawn(play, &this->actor.world.pos, -10.0f, 10.0f, 30.0f, 0.25f);
    }
}

void EnOkuta_SpawnDust(Vec3f* pos, Vec3f* velocity, s16 scaleStep, PlayState* play) {
    static Vec3f accel = { 0.0f, 0.0f, 0.0f };
    static Color_RGBA8 primColor = { 255, 255, 255, 255 };
    static Color_RGBA8 envColor = { 150, 150, 150, 255 };

    func_8002829C(play, pos, velocity, &accel, &primColor, &envColor, 0x190, scaleStep);
}

void OoT_EnOkuta_SpawnSplash(EnOkuta* this, PlayState* play) {
    OoT_EffectSsGSplash_Spawn(play, &this->actor.home.pos, NULL, NULL, 0, 1300);
}

void OoT_EnOkuta_SpawnRipple(EnOkuta* this, PlayState* play) {
    Vec3f pos;

    pos.x = this->actor.world.pos.x;
    pos.y = this->actor.home.pos.y;
    pos.z = this->actor.world.pos.z;
    if ((play->gameplayFrames % 7) == 0 &&
        ((this->actionFunc != OoT_EnOkuta_Shoot) || ((this->actor.world.pos.y - this->actor.home.pos.y) < 50.0f))) {
        OoT_EffectSsGRipple_Spawn(play, &pos, 250, 650, 0);
    }
}

void OoT_EnOkuta_SetupWaitToAppear(EnOkuta* this) {
    this->actor.draw = NULL;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = OoT_EnOkuta_WaitToAppear;
    this->actor.world.pos.y = this->actor.home.pos.y;
}

void OoT_EnOkuta_SetupAppear(EnOkuta* this, PlayState* play) {
    this->actor.draw = OoT_EnOkuta_Draw;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    OoT_Animation_PlayOnce(&this->skelAnime, &gOctorokAppearAnim);
    OoT_EnOkuta_SpawnBubbles(this, play);
    this->actionFunc = OoT_EnOkuta_Appear;
}

void OoT_EnOkuta_SetupHide(EnOkuta* this) {
    OoT_Animation_PlayOnce(&this->skelAnime, &gOctorokHideAnim);
    this->actionFunc = OoT_EnOkuta_Hide;
}

void EnOkuta_SetupWaitToShoot(EnOkuta* this) {
    OoT_Animation_PlayLoop(&this->skelAnime, &gOctorokFloatAnim);
    this->timer = (this->actionFunc == OoT_EnOkuta_Shoot) ? 2 : 0;
    this->actionFunc = EnOkuta_WaitToShoot;
}

void OoT_EnOkuta_SetupShoot(EnOkuta* this, PlayState* play) {
    OoT_Animation_PlayOnce(&this->skelAnime, &gOctorokShootAnim);
    if (this->actionFunc != OoT_EnOkuta_Shoot) {
        this->timer = this->numShots;
    }
    this->jumpHeight = this->actor.yDistToPlayer + 20.0f;
    this->jumpHeight = CLAMP_MIN(this->jumpHeight, 10.0f);
    if (this->jumpHeight > 50.0f) {
        OoT_EnOkuta_SpawnSplash(this, play);
    }
    if (this->jumpHeight > 50.0f) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_OCTAROCK_JUMP);
    }
    this->actionFunc = OoT_EnOkuta_Shoot;
}

void EnOkuta_SetupWaitToDie(EnOkuta* this) {
    OoT_Animation_MorphToPlayOnce(&this->skelAnime, &gOctorokHitAnim, -5.0f);
    OoT_Actor_SetColorFilter(&this->actor, 0x4000, 0xFF, 0, 0xB);
    this->collider.base.acFlags &= ~AC_HIT;
    OoT_Actor_SetScale(&this->actor, 0.01f);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_OCTAROCK_DEAD1);
    this->actionFunc = EnOkuta_WaitToDie;
}

void OoT_EnOkuta_SetupDie(EnOkuta* this) {
    OoT_Animation_MorphToPlayOnce(&this->skelAnime, &gOctorokDieAnim, -3.0f);
    this->timer = 0;
    this->actionFunc = OoT_EnOkuta_Die;
    GameInteractor_ExecuteOnEnemyDefeat(&this->actor);
}

void EnOkuta_SetupFreeze(EnOkuta* this) {
    this->timer = 80;
    OoT_Actor_SetColorFilter(&this->actor, 0, 0xFF, 0, 0x50);
    this->actionFunc = OoT_EnOkuta_Freeze;
}

void OoT_EnOkuta_SpawnProjectile(EnOkuta* this, PlayState* play) {
    Vec3f pos;
    Vec3f velocity;
    f32 sin = OoT_Math_SinS(this->actor.shape.rot.y);
    f32 cos = OoT_Math_CosS(this->actor.shape.rot.y);

    pos.x = this->actor.world.pos.x + (25.0f * sin);
    pos.y = this->actor.world.pos.y - 6.0f;
    pos.z = this->actor.world.pos.z + (25.0f * cos);
    if (OoT_Actor_Spawn(&play->actorCtx, play, ACTOR_EN_OKUTA, pos.x, pos.y, pos.z, this->actor.shape.rot.x,
                    this->actor.shape.rot.y, this->actor.shape.rot.z, 0x10, true) != NULL) {
        pos.x = this->actor.world.pos.x + (40.0f * sin);
        pos.z = this->actor.world.pos.z + (40.0f * cos);
        pos.y = this->actor.world.pos.y;
        velocity.x = 1.5f * sin;
        velocity.y = 0.0f;
        velocity.z = 1.5f * cos;
        EnOkuta_SpawnDust(&pos, &velocity, 20, play);
    }
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_THROW);
}

void OoT_EnOkuta_WaitToAppear(EnOkuta* this, PlayState* play) {
    this->actor.world.pos.y = this->actor.home.pos.y;
    if ((this->actor.xzDistToPlayer < 480.0f) && (this->actor.xzDistToPlayer > 200.0f)) {
        OoT_EnOkuta_SetupAppear(this, play);
    }
}

void OoT_EnOkuta_Appear(EnOkuta* this, PlayState* play) {
    s32 pad;

    if (OoT_SkelAnime_Update(&this->skelAnime)) {
        if (this->actor.xzDistToPlayer < 160.0f) {
            OoT_EnOkuta_SetupHide(this);
        } else {
            EnOkuta_SetupWaitToShoot(this);
        }
    } else if (this->skelAnime.curFrame <= 4.0f) {
        OoT_Actor_SetScale(&this->actor, this->skelAnime.curFrame * 0.25f * 0.01f);
    } else if (OoT_Animation_OnFrame(&this->skelAnime, 5.0f)) {
        OoT_Actor_SetScale(&this->actor, 0.01f);
    }
    if (OoT_Animation_OnFrame(&this->skelAnime, 2.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_OCTAROCK_JUMP);
    }
    if (OoT_Animation_OnFrame(&this->skelAnime, 12.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_OCTAROCK_LAND);
    }
    if (OoT_Animation_OnFrame(&this->skelAnime, 3.0f) || OoT_Animation_OnFrame(&this->skelAnime, 15.0f)) {
        OoT_EnOkuta_SpawnSplash(this, play);
    }
}

void OoT_EnOkuta_Hide(EnOkuta* this, PlayState* play) {
    s32 pad;

    OoT_Math_ApproachF(&this->actor.world.pos.y, this->actor.home.pos.y, 0.5f, 30.0f);
    if (OoT_SkelAnime_Update(&this->skelAnime)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_OCTAROCK_BUBLE);
        OoT_EnOkuta_SpawnBubbles(this, play);
        OoT_EnOkuta_SetupWaitToAppear(this);
    } else if (this->skelAnime.curFrame >= 4.0f) {
        OoT_Actor_SetScale(&this->actor, (6.0f - this->skelAnime.curFrame) * 0.5f * 0.01f);
    }
    if (OoT_Animation_OnFrame(&this->skelAnime, 2.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_OCTAROCK_SINK);
    }
    if (OoT_Animation_OnFrame(&this->skelAnime, 4.0f)) {
        OoT_EnOkuta_SpawnSplash(this, play);
    }
}

void EnOkuta_WaitToShoot(EnOkuta* this, PlayState* play) {
    s16 temp_v0_2;
    s32 phi_v1;

    this->actor.world.pos.y = this->actor.home.pos.y;
    OoT_SkelAnime_Update(&this->skelAnime);
    if (OoT_Animation_OnFrame(&this->skelAnime, 0.0f)) {
        if (this->timer != 0) {
            this->timer--;
        }
    }
    if (OoT_Animation_OnFrame(&this->skelAnime, 0.5f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_OCTAROCK_FLOAT);
    }
    if (this->actor.xzDistToPlayer < 160.0f || this->actor.xzDistToPlayer > 560.0f) {
        OoT_EnOkuta_SetupHide(this);
    } else {
        temp_v0_2 = OoT_Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 0x71C, 0x38E);
        phi_v1 = ABS(temp_v0_2);
        if ((phi_v1 < 0x38E) && (this->timer == 0) && (this->actor.yDistToPlayer < 200.0f)) {
            OoT_EnOkuta_SetupShoot(this, play);
        }
    }
}

void OoT_EnOkuta_Shoot(EnOkuta* this, PlayState* play) {
    OoT_Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 0x71C);
    if (OoT_SkelAnime_Update(&this->skelAnime)) {
        if (this->timer != 0) {
            this->timer--;
        }
        if (this->timer == 0) {
            EnOkuta_SetupWaitToShoot(this);
        } else {
            OoT_EnOkuta_SetupShoot(this, play);
        }
    } else {
        f32 curFrame = this->skelAnime.curFrame;

        if (curFrame < 13.0f) {
            this->actor.world.pos.y = (sinf((0.08333f * M_PI) * curFrame) * this->jumpHeight) + this->actor.home.pos.y;
        }
        if (OoT_Animation_OnFrame(&this->skelAnime, 6.0f)) {
            OoT_EnOkuta_SpawnProjectile(this, play);
        }
        if ((this->jumpHeight > 50.0f) && OoT_Animation_OnFrame(&this->skelAnime, 13.0f)) {
            OoT_EnOkuta_SpawnSplash(this, play);
        }
        if ((this->jumpHeight > 50.0f) && OoT_Animation_OnFrame(&this->skelAnime, 13.0f)) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_OCTAROCK_LAND);
        }
    }
    if (this->actor.xzDistToPlayer < 160.0f) {
        OoT_EnOkuta_SetupHide(this);
    }
}

void EnOkuta_WaitToDie(EnOkuta* this, PlayState* play) {
    if (OoT_SkelAnime_Update(&this->skelAnime)) {
        OoT_EnOkuta_SetupDie(this);
    }
    OoT_Math_ApproachF(&this->actor.world.pos.y, this->actor.home.pos.y, 0.5f, 5.0f);
}

void OoT_EnOkuta_Die(EnOkuta* this, PlayState* play) {
    static Vec3f accel = { 0.0f, -0.5f, 0.0f };
    static Color_RGBA8 primColor = { 255, 255, 255, 255 };
    static Color_RGBA8 envColor = { 150, 150, 150, 0 };
    Vec3f velocity;
    Vec3f pos;
    s32 i;

    if (OoT_SkelAnime_Update(&this->skelAnime)) {
        this->timer++;
    }
    OoT_Math_ApproachF(&this->actor.world.pos.y, this->actor.home.pos.y, 0.5f, 5.0f);
    if (this->timer == 5) {
        pos.x = this->actor.world.pos.x;
        pos.y = this->actor.world.pos.y + 40.0f;
        pos.z = this->actor.world.pos.z;
        velocity.x = 0.0f;
        velocity.y = -0.5f;
        velocity.z = 0.0f;
        EnOkuta_SpawnDust(&pos, &velocity, -0x14, play);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_OCTAROCK_DEAD2);
    }
    if (OoT_Animation_OnFrame(&this->skelAnime, 15.0f)) {
        OoT_EnOkuta_SpawnSplash(this, play);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_OCTAROCK_LAND);
    }
    if (this->timer < 3) {
        OoT_Actor_SetScale(&this->actor, ((this->timer * 0.25f) + 1.0f) * 0.01f);
    } else if (this->timer < 6) {
        OoT_Actor_SetScale(&this->actor, (1.5f - ((this->timer - 2) * 0.2333f)) * 0.01f);
    } else if (this->timer < 11) {
        OoT_Actor_SetScale(&this->actor, (((this->timer - 5) * 0.04f) + 0.8f) * 0.01f);
    } else {
        if (OoT_Math_StepToF(&this->actor.scale.x, 0.0f, 0.0005f)) {
            OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 30, NA_SE_EN_OCTAROCK_BUBLE);
            OoT_Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x70);
            for (i = 0; i < 20; i++) {
                velocity.x = (OoT_Rand_ZeroOne() - 0.5f) * 7.0f;
                velocity.y = OoT_Rand_ZeroOne() * 7.0f;
                velocity.z = (OoT_Rand_ZeroOne() - 0.5f) * 7.0f;
                OoT_EffectSsDtBubble_SpawnCustomColor(play, &this->actor.world.pos, &velocity, &accel, &primColor,
                                                  &envColor, OoT_Rand_S16Offset(100, 50), 25, 0);
            }
            OoT_Actor_Kill(&this->actor);
        }
        this->actor.scale.y = this->actor.scale.z = this->actor.scale.x;
    }
}

void OoT_EnOkuta_Freeze(EnOkuta* this, PlayState* play) {
    Vec3f pos;
    s16 temp_v1;

    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 0) {
        OoT_EnOkuta_SetupDie(this);
    }
    if ((this->timer >= 64) && (this->timer & 1)) {
        temp_v1 = (this->timer - 64) >> 1;
        pos.y = (this->actor.world.pos.y - 32.0f) + (8.0f * (8 - temp_v1));
        pos.x = this->actor.world.pos.x + ((temp_v1 & 2) ? 10.0f : -10.0f);
        pos.z = this->actor.world.pos.z + ((temp_v1 & 1) ? 10.0f : -10.0f);
        EffectSsEnIce_SpawnFlyingVec3f(play, &this->actor, &pos, 150, 150, 150, 250, 235, 245, 255,
                                       (OoT_Rand_ZeroOne() * 0.2f) + 1.9f);
    }
    OoT_Math_ApproachF(&this->actor.world.pos.y, this->actor.home.pos.y, 0.5f, 5.0f);
}

void EnOkuta_ProjectileFly(EnOkuta* this, PlayState* play) {
    Vec3f pos;
    Player* player = GET_PLAYER(play);
    Vec3s sp40;

    this->timer--;
    if (this->timer == 0) {
        this->actor.gravity = -1.0f;
    }
    this->actor.home.rot.z += 0x1554;
    if (this->actor.bgCheckFlags & 0x20) {
        this->actor.gravity = -1.0f;
        this->actor.speedXZ -= 0.1f;
        this->actor.speedXZ = CLAMP_MIN(this->actor.speedXZ, 1.0f);
    }
    if ((this->actor.bgCheckFlags & 8) || (this->actor.bgCheckFlags & 1) || (this->collider.base.atFlags & AT_HIT) ||
        this->collider.base.acFlags & AC_HIT || this->collider.base.ocFlags1 & OC1_HIT ||
        this->actor.floorHeight == BGCHECK_Y_MIN) {
        if ((player->currentShield == PLAYER_SHIELD_DEKU ||
             (player->currentShield == PLAYER_SHIELD_HYLIAN && LINK_IS_ADULT)) &&
            this->collider.base.atFlags & AT_HIT && this->collider.base.atFlags & AT_TYPE_ENEMY &&
            this->collider.base.atFlags & AT_BOUNCED) {
            this->collider.base.atFlags &= ~(AT_HIT | AT_BOUNCED | AT_TYPE_ENEMY);
            this->collider.base.atFlags |= AT_TYPE_PLAYER;
            this->collider.info.toucher.dmgFlags = 2;
            Matrix_MtxFToYXZRotS(&player->shieldMf, &sp40, 0);
            this->actor.world.rot.y = sp40.y + 0x8000;
            this->timer = 30;
        } else {
            pos.x = this->actor.world.pos.x;
            pos.y = this->actor.world.pos.y + 11.0f;
            pos.z = this->actor.world.pos.z;
            if (CVarGetInteger(CVAR_ENHANCEMENT("NewDrops"), 0) != 0) {
                static s16 sEffectScales[] = {
                    145, 135, 115, 85, 75, 53, 45, 40, 35,
                };
                s32 pad;
                Vec3f velocity;
                Vec3f pos;
                s16 phi_s0 = 500;
                s16 gravity;
                s16 phi_v0;
                f32 temp_f20;
                f32 temp_f22;
                s32 i;
                for (s16 i = 0; i < ARRAY_COUNT(sEffectScales); i++) {
                    phi_s0 += 10000;

                    temp_f20 = OoT_Rand_ZeroOne() * 5.0f;
                    pos.x = (OoT_Math_SinS(phi_s0) * temp_f20) + this->actor.world.pos.x;
                    pos.y = (OoT_Rand_ZeroOne() * 40.0f) + this->actor.world.pos.y + 5.0f;
                    pos.z = (OoT_Math_CosS(phi_s0) * temp_f20) + this->actor.world.pos.z;

                    temp_f20 = (OoT_Rand_ZeroOne() * 5.0f) + 2.0f;
                    velocity.x = OoT_Math_SinS(phi_s0) * temp_f20;
                    temp_f22 = OoT_Rand_ZeroOne();
                    velocity.y = (OoT_Rand_ZeroOne() * i * 2.5f) + (temp_f22 * 5.0f);
                    velocity.z = OoT_Math_CosS(phi_s0) * temp_f20;

                    if (i == 0) {
                        phi_v0 = 41;
                        gravity = -450;
                    } else if (i < 4) {
                        phi_v0 = 37;
                        gravity = -380;
                    } else {
                        phi_v0 = 69;
                        gravity = -320;
                    }
                    OoT_EffectSsKakera_Spawn(play, &pos, &velocity, &this->actor.world.pos, gravity, phi_v0, 30, 5, 0,
                                         sEffectScales[i] / 5, 3, 0, 70, 1, OBJECT_GAMEPLAY_FIELD_KEEP,
                                         gSilverRockFragmentsDL);
                }
            } else {
                OoT_EffectSsHahen_SpawnBurst(play, &pos, 6.0f, 0, 1, 2, 15, 7, 10, gOctorokProjectileDL);
            }

            OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EN_OCTAROCK_ROCK);
            OoT_Actor_Kill(&this->actor);
        }
    } else if (this->timer == -300) {
        OoT_Actor_Kill(&this->actor);
    }
}

void OoT_EnOkuta_UpdateHeadScale(EnOkuta* this) {
    f32 curFrame = this->skelAnime.curFrame;

    if (this->actionFunc == OoT_EnOkuta_Appear) {
        if (curFrame < 8.0f) {
            this->headScale.x = this->headScale.y = this->headScale.z = 1.0f;
        } else if (curFrame < 10.0f) {
            this->headScale.x = this->headScale.z = 1.0f;
            this->headScale.y = ((curFrame - 7.0f) * 0.4f) + 1.0f;
        } else if (curFrame < 14.0f) {
            this->headScale.x = this->headScale.z = ((curFrame - 9.0f) * 0.075f) + 1.0f;
            this->headScale.y = 1.8f - ((curFrame - 9.0f) * 0.25f);
        } else {
            this->headScale.x = this->headScale.z = 1.3f - ((curFrame - 13.0f) * 0.05f);
            this->headScale.y = ((curFrame - 13.0f) * 0.0333f) + 0.8f;
        }
    } else if (this->actionFunc == OoT_EnOkuta_Hide) {
        if (curFrame < 3.0f) {
            this->headScale.y = 1.0f;
        } else if (curFrame < 4.0f) {
            this->headScale.y = (curFrame - 2.0f) + 1.0f;
        } else {
            this->headScale.y = 2.0f - ((curFrame - 3.0f) * 0.333f);
        }
        this->headScale.x = this->headScale.z = 1.0f;
    } else if (this->actionFunc == OoT_EnOkuta_Shoot) {
        if (curFrame < 5.0f) {
            this->headScale.x = this->headScale.y = this->headScale.z = (curFrame * 0.125f) + 1.0f;
        } else if (curFrame < 7.0f) {
            this->headScale.x = this->headScale.y = this->headScale.z = 1.5f - ((curFrame - 4.0f) * 0.35f);
        } else if (curFrame < 17.0f) {
            this->headScale.x = this->headScale.z = ((curFrame - 6.0f) * 0.05f) + 0.8f;
            this->headScale.y = 0.8f;
        } else {
            this->headScale.x = this->headScale.z = 1.3f - ((curFrame - 16.0f) * 0.1f);
            this->headScale.y = ((curFrame - 16.0f) * 0.0666f) + 0.8f;
        }
    } else if (this->actionFunc == EnOkuta_WaitToShoot) {
        this->headScale.x = this->headScale.z = 1.0f;
        this->headScale.y = (sinf((M_PI / 16) * curFrame) * 0.2f) + 1.0f;
    } else {
        this->headScale.x = this->headScale.y = this->headScale.z = 1.0f;
    }
}

void EnOkuta_ColliderCheck(EnOkuta* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        OoT_Actor_SetDropFlag(&this->actor, &this->collider.info, 1);
        if ((this->actor.colChkInfo.damageEffect != 0) || (this->actor.colChkInfo.damage != 0)) {
            OoT_Enemy_StartFinishingBlow(play, &this->actor);
            this->actor.colChkInfo.health = 0;
            this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
            if (this->actor.colChkInfo.damageEffect == 3) {
                EnOkuta_SetupFreeze(this);
            } else {
                EnOkuta_SetupWaitToDie(this);
            }
        }
    }
}

void OoT_EnOkuta_Update(Actor* thisx, PlayState* play2) {
    EnOkuta* this = (EnOkuta*)thisx;
    PlayState* play = play2;
    Player* player = GET_PLAYER(play);
    WaterBox* outWaterBox;
    f32 ySurface;
    Vec3f sp38;
    s32 sp34;

    if (!(player->stateFlags1 &
          (PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE))) {
        if (this->actor.params == 0) {
            EnOkuta_ColliderCheck(this, play);
            if (!OoT_WaterBox_GetSurfaceImpl(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z,
                                         &ySurface, &outWaterBox) ||
                (ySurface < this->actor.floorHeight)) {
                if (this->actor.colChkInfo.health != 0) {
                    OoT_Actor_Kill(&this->actor);
                    return;
                }
            } else {
                this->actor.home.pos.y = ySurface;
            }
        }
        this->actionFunc(this, play);
        if (this->actor.params == 0) {
            OoT_EnOkuta_UpdateHeadScale(this);
            this->collider.dim.height =
                (((sOctorockColliderInit.dim.height * this->headScale.y) - this->collider.dim.yShift) *
                 this->actor.scale.y * 100.0f);
        } else {
            sp34 = false;
            Actor_MoveXZGravity(&this->actor);
            OoT_Math_Vec3f_Copy(&sp38, &this->actor.world.pos);
            OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 10.0f, 15.0f, 30.0f, 5);
            if ((this->actor.bgCheckFlags & 8) &&
                OoT_SurfaceType_IsIgnoredByProjectiles(&play->colCtx, this->actor.wallPoly, this->actor.wallBgId)) {
                sp34 = true;
                this->actor.bgCheckFlags &= ~8;
            }
            if ((this->actor.bgCheckFlags & 1) &&
                OoT_SurfaceType_IsIgnoredByProjectiles(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId)) {
                sp34 = true;
                this->actor.bgCheckFlags &= ~1;
            }
            if (sp34 && !(this->actor.bgCheckFlags & 9)) {
                OoT_Math_Vec3f_Copy(&this->actor.world.pos, &sp38);
            }
        }
        OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
        if ((this->actionFunc == OoT_EnOkuta_Appear) || (this->actionFunc == OoT_EnOkuta_Hide)) {
            this->collider.dim.pos.y = this->actor.world.pos.y + (this->skelAnime.jointTable->y * this->actor.scale.y);
            this->collider.dim.radius = sOctorockColliderInit.dim.radius * this->actor.scale.x * 100.0f;
        }
        if (this->actor.params == 0x10) {
            this->actor.flags |= ACTOR_FLAG_SFX_FOR_PLAYER_BODY_HIT;
            OoT_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        }
        if (this->actionFunc != OoT_EnOkuta_WaitToAppear) {
            if ((this->actionFunc != OoT_EnOkuta_Die) && (this->actionFunc != EnOkuta_WaitToDie) &&
                (this->actionFunc != OoT_EnOkuta_Freeze)) {
                OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
            }
            OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
        }
        OoT_Actor_SetFocus(&this->actor, 15.0f);
        if ((this->actor.params == 0) && (this->actor.draw != NULL)) {
            OoT_EnOkuta_SpawnRipple(this, play);
        }
    }
}

s32 OoT_EnOkuta_GetSnoutScale(EnOkuta* this, f32 curFrame, Vec3f* scale) {
    if (this->actionFunc == EnOkuta_WaitToShoot) {
        scale->x = scale->z = 1.0f;
        scale->y = (sinf((M_PI / 16) * curFrame) * 0.4f) + 1.0f;
    } else if (this->actionFunc == OoT_EnOkuta_Shoot) {
        if (curFrame < 5.0f) {
            scale->x = 1.0f;
            scale->y = scale->z = (curFrame * 0.25f) + 1.0f;
        } else if (curFrame < 7.0f) {
            scale->x = (curFrame - 4.0f) * 0.5f + 1.0f;
            scale->y = scale->z = 2.0f - (curFrame - 4.0f) * 0.5f;
        } else {
            scale->x = 2.0f - ((curFrame - 6.0f) * 0.0769f);
            scale->y = scale->z = 1.0f;
        }
    } else if (this->actionFunc == OoT_EnOkuta_Die) {
        if (curFrame >= 35.0f || curFrame < 25.0f) {
            return false;
        }
        if (curFrame < 27.0f) {
            scale->x = 1.0f;
            scale->y = scale->z = ((curFrame - 24.0f) * 0.5f) + 1.0f;
        } else if (curFrame < 30.0f) {
            scale->x = (curFrame - 26.0f) * 0.333f + 1.0f;
            scale->y = scale->z = 2.0f - (curFrame - 26.0f) * 0.333f;
        } else {
            scale->x = 2.0f - ((curFrame - 29.0f) * 0.2f);
            scale->y = scale->z = 1.0f;
        }
    } else {
        return false;
    }

    return true;
}

s32 OoT_EnOkuta_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnOkuta* this = (EnOkuta*)thisx;
    f32 curFrame = this->skelAnime.curFrame;
    Vec3f scale;
    s32 doScale = false;

    if (this->actionFunc == OoT_EnOkuta_Die) {
        curFrame += this->timer;
    }
    if (limbIndex == 5) {
        if ((this->headScale.x != 1.0f) || (this->headScale.y != 1.0f) || (this->headScale.z != 1.0f)) {
            scale = this->headScale;
            doScale = true;
        }
    } else if (limbIndex == 8) {
        doScale = OoT_EnOkuta_GetSnoutScale(this, curFrame, &scale);
    }
    if (doScale) {
        OoT_Matrix_Scale(scale.x, scale.y, scale.z, MTXMODE_APPLY);
    }
    return false;
}

void OoT_EnOkuta_Draw(Actor* thisx, PlayState* play) {
    EnOkuta* this = (EnOkuta*)thisx;
    s32 pad;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    if (this->actor.params == 0) {
        SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, OoT_EnOkuta_OverrideLimbDraw, NULL, this);
    } else {
        OPEN_DISPS(play->state.gfxCtx);

        if (CVarGetInteger(CVAR_ENHANCEMENT("NewDrops"), 0) != 0) {
            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            gSPSegment(POLY_OPA_DISP++, 0x08,
                       OoT_Gfx_TwoTexScroll(play->state.gfxCtx, 0, 1 * (play->state.frames * 6),
                                        1 * (play->state.frames * 6), 32, 32, 1, 1 * (play->state.frames * 6),
                                        1 * (play->state.frames * 6), 32, 32));
            OoT_Matrix_Scale(7.0f, 7.0f, 7.0f, MTXMODE_APPLY);
            Matrix_RotateX(thisx->home.rot.z * (M_PI / 0x8000), MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPDisplayList(POLY_OPA_DISP++, gSilverRockDL);
        } else {
            OoT_Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
            Matrix_RotateZ(this->actor.home.rot.z * (M_PI / 0x8000), MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, gOctorokProjectileDL);
        }

        CLOSE_DISPS(play->state.gfxCtx);
    }
}
