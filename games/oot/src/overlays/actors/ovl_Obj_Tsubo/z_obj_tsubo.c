/*
 * File: z_obj_tsubo.c
 * Overlay: ovl_Obj_Tsubo
 * Description: Breakable pot
 */

#include "z_obj_tsubo.h"
#include "overlays/effects/ovl_Effect_Ss_Kakera/z_eff_ss_kakera.h"
#include "objects/gameplay_dangeon_keep/gameplay_dangeon_keep.h"
#include "objects/object_tsubo/object_tsubo.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_THROW_ONLY)

void OoT_ObjTsubo_Init(Actor* thisx, PlayState* play);
void OoT_ObjTsubo_Destroy(Actor* thisx, PlayState* play);
void OoT_ObjTsubo_Update(Actor* thisx, PlayState* play);
void OoT_ObjTsubo_Draw(Actor* thisx, PlayState* play);

void ObjTsubo_SpawnCollectible(ObjTsubo* this, PlayState* play);
void ObjTsubo_ApplyGravity(ObjTsubo* this);
s32 ObjTsubo_SnapToFloor(ObjTsubo* this, PlayState* play);
void ObjTsubo_InitCollider(Actor* thisx, PlayState* play);
void ObjTsubo_AirBreak(ObjTsubo* this, PlayState* play);
void ObjTsubo_WaterBreak(ObjTsubo* this, PlayState* play);
void ObjTsubo_SetupWaitForObject(ObjTsubo* this);
void ObjTsubo_WaitForObject(ObjTsubo* this, PlayState* play);
void ObjTsubo_SetupIdle(ObjTsubo* this);
void ObjTsubo_Idle(ObjTsubo* this, PlayState* play);
void ObjTsubo_SetupLiftedUp(ObjTsubo* this);
void ObjTsubo_LiftedUp(ObjTsubo* this, PlayState* play);
void ObjTsubo_SetupThrown(ObjTsubo* this);
void ObjTsubo_Thrown(ObjTsubo* this, PlayState* play);

static s16 D_80BA1B50 = 0;
static s16 D_80BA1B54 = 0;
static s16 D_80BA1B58 = 0;
static s16 D_80BA1B5C = 0;

const ActorInit Obj_Tsubo_InitVars = {
    ACTOR_OBJ_TSUBO,
    ACTORCAT_PROP,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(ObjTsubo),
    (ActorFunc)OoT_ObjTsubo_Init,
    (ActorFunc)OoT_ObjTsubo_Destroy,
    (ActorFunc)OoT_ObjTsubo_Update,
    NULL,
    NULL,
};

static s16 OoT_sObjectIds[] = { OBJECT_GAMEPLAY_DANGEON_KEEP, OBJECT_TSUBO };

static Gfx* D_80BA1B84[] = { gPotDL, object_tsubo_DL_0017C0 };

static Gfx* D_80BA1B8C[] = { gPotFragmentDL, object_tsubo_DL_001960 };

static ColliderCylinderInit OoT_sCylinderInit = {
    {
        COLTYPE_HARD,
        AT_ON | AT_TYPE_PLAYER,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000002, 0x00, 0x01 },
        { 0x4FC1FFFE, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_ON,
    },
    { 9, 26, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit OoT_sColChkInfoInit[] = { 0, 12, 60, MASS_IMMOVABLE };

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_F32_DIV1000(gravity, -1200, ICHAIN_CONTINUE), ICHAIN_F32_DIV1000(minVelocityY, -20000, ICHAIN_CONTINUE),
    ICHAIN_VEC3F_DIV1000(scale, 150, ICHAIN_CONTINUE),   ICHAIN_F32(uncullZoneForward, 900, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 100, ICHAIN_CONTINUE),   ICHAIN_F32(uncullZoneDownward, 800, ICHAIN_STOP),
};

void ObjTsubo_SpawnCollectible(ObjTsubo* this, PlayState* play) {
    s16 dropParams = this->actor.params & 0x1F;

    if (GameInteractor_Should(VB_POT_DROP_ITEM,
                              (dropParams >= ITEM00_RUPEE_GREEN) && (dropParams <= ITEM00_BOMBS_SPECIAL), this)) {
        OoT_Item_DropCollectible(play, &this->actor.world.pos, (dropParams | (((this->actor.params >> 9) & 0x3F) << 8)));
    }
}

void ObjTsubo_ApplyGravity(ObjTsubo* this) {
    this->actor.velocity.y += this->actor.gravity;
    if (this->actor.velocity.y < this->actor.minVelocityY) {
        this->actor.velocity.y = this->actor.minVelocityY;
    }
}

s32 ObjTsubo_SnapToFloor(ObjTsubo* this, PlayState* play) {
    CollisionPoly* floorPoly;
    Vec3f pos;
    s32 bgID;
    f32 floorY;

    pos.x = this->actor.world.pos.x;
    pos.y = this->actor.world.pos.y + 20.0f;
    pos.z = this->actor.world.pos.z;
    floorY = BgCheck_EntityRaycastFloor4(&play->colCtx, &floorPoly, &bgID, &this->actor, &pos);
    if (floorY > BGCHECK_Y_MIN) {
        this->actor.world.pos.y = floorY;
        OoT_Math_Vec3f_Copy(&this->actor.home.pos, &this->actor.world.pos);
        return true;
    } else {
        osSyncPrintf("地面に付着失敗\n");
        return false;
    }
}

void ObjTsubo_InitCollider(Actor* thisx, PlayState* play) {
    ObjTsubo* this = (ObjTsubo*)thisx;

    OoT_Collider_InitCylinder(play, &this->collider);
    OoT_Collider_SetCylinder(play, &this->collider, &this->actor, &OoT_sCylinderInit);
    OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
}

void OoT_ObjTsubo_Init(Actor* thisx, PlayState* play) {
    ObjTsubo* this = (ObjTsubo*)thisx;

    OoT_Actor_ProcessInitChain(&this->actor, OoT_sInitChain);
    ObjTsubo_InitCollider(&this->actor, play);
    OoT_CollisionCheck_SetInfo(&this->actor.colChkInfo, NULL, OoT_sColChkInfoInit);
    if (!ObjTsubo_SnapToFloor(this, play)) {
        OoT_Actor_Kill(&this->actor);
        return;
    }
    this->objTsuboBankIndex = Object_GetIndex(&play->objectCtx, OoT_sObjectIds[(this->actor.params >> 8) & 1]);
    if (this->objTsuboBankIndex < 0) {
        osSyncPrintf("Error : バンク危険！ (arg_data 0x%04x)(%s %d)\n", this->actor.params, __FILE__, __LINE__);
        OoT_Actor_Kill(&this->actor);
    } else {
        ObjTsubo_SetupWaitForObject(this);
        osSyncPrintf("(dungeon keep 壷)(arg_data 0x%04x)\n", this->actor.params);
    }
}

void OoT_ObjTsubo_Destroy(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    ObjTsubo* this = (ObjTsubo*)thisx;

    OoT_Collider_DestroyCylinder(play, &this->collider);
}

void ObjTsubo_AirBreak(ObjTsubo* this, PlayState* play) {
    s32 pad;
    f32 rand;
    s16 angle;
    Vec3f pos;
    Vec3f velocity;
    f32 OoT_sins;
    f32 OoT_coss;
    s32 arg5;
    s32 i;

    for (i = 0, angle = 0; i < 15; i++, angle += 0x4E20) {
        OoT_sins = OoT_Math_SinS(angle);
        OoT_coss = OoT_Math_CosS(angle);
        pos.x = OoT_sins * 8.0f;
        pos.y = (OoT_Rand_ZeroOne() * 5.0f) + 2.0f;
        pos.z = OoT_coss * 8.0f;
        velocity.x = pos.x * 0.23f;
        velocity.y = (OoT_Rand_ZeroOne() * 5.0f) + 2.0f;
        velocity.z = pos.z * 0.23f;
        OoT_Math_Vec3f_Sum(&pos, &this->actor.world.pos, &pos);
        rand = OoT_Rand_ZeroOne();
        if (rand < 0.2f) {
            arg5 = 96;
        } else if (rand < 0.6f) {
            arg5 = 64;
        } else {
            arg5 = 32;
        }
        OoT_EffectSsKakera_Spawn(play, &pos, &velocity, &this->actor.world.pos, -240, arg5, 10, 10, 0,
                             (OoT_Rand_ZeroOne() * 95.0f) + 15.0f, 0, 32, 60, KAKERA_COLOR_NONE,
                             OoT_sObjectIds[(this->actor.params >> 8) & 1], D_80BA1B8C[(this->actor.params >> 8) & 1]);
    }
    func_80033480(play, &this->actor.world.pos, 30.0f, 4, 20, 50, 1);
    gSaveContext.ship.stats.count[COUNT_POTS_BROKEN]++;
}

void ObjTsubo_WaterBreak(ObjTsubo* this, PlayState* play) {
    s32 pad[2];
    s16 angle;
    Vec3f pos = this->actor.world.pos;
    Vec3f velocity;
    s32 phi_s0;
    s32 i;

    pos.y += this->actor.yDistToWater;
    OoT_EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, 400);
    for (i = 0, angle = 0; i < 15; i++, angle += 0x4E20) {
        f32 OoT_sins = OoT_Math_SinS(angle);
        f32 OoT_coss = OoT_Math_CosS(angle);

        pos.x = OoT_sins * 8.0f;
        pos.y = (OoT_Rand_ZeroOne() * 5.0f) + 2.0f;
        pos.z = OoT_coss * 8.0f;
        velocity.x = pos.x * 0.2f;
        velocity.y = (OoT_Rand_ZeroOne() * 4.0f) + 2.0f;
        velocity.z = pos.z * 0.2f;
        OoT_Math_Vec3f_Sum(&pos, &this->actor.world.pos, &pos);
        phi_s0 = (OoT_Rand_ZeroOne() < .2f) ? 64 : 32;
        OoT_EffectSsKakera_Spawn(play, &pos, &velocity, &this->actor.world.pos, -180, phi_s0, 30, 30, 0,
                             (OoT_Rand_ZeroOne() * 95.0f) + 15.0f, 0, 32, 70, KAKERA_COLOR_NONE,
                             OoT_sObjectIds[(this->actor.params >> 8) & 1], D_80BA1B8C[(this->actor.params >> 8) & 1]);
    }
    gSaveContext.ship.stats.count[COUNT_POTS_BROKEN]++;
}

void ObjTsubo_SetupWaitForObject(ObjTsubo* this) {
    this->actionFunc = ObjTsubo_WaitForObject;
}

void ObjTsubo_WaitForObject(ObjTsubo* this, PlayState* play) {
    if (OoT_Object_IsLoaded(&play->objectCtx, this->objTsuboBankIndex)) {
        if (GameInteractor_Should(VB_POT_SETUP_DRAW, true, this)) {
            this->actor.draw = OoT_ObjTsubo_Draw;
        }
        this->actor.objBankIndex = this->objTsuboBankIndex;
        ObjTsubo_SetupIdle(this);
        this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    }
}

void ObjTsubo_SetupIdle(ObjTsubo* this) {
    this->actionFunc = ObjTsubo_Idle;
}

void ObjTsubo_Idle(ObjTsubo* this, PlayState* play) {
    s32 pad;
    s16 temp_v0;
    s32 phi_v1;

    if (OoT_Actor_HasParent(&this->actor, play)) {
        ObjTsubo_SetupLiftedUp(this);
    } else if ((this->actor.bgCheckFlags & 0x20) && (this->actor.yDistToWater > 15.0f)) {
        ObjTsubo_WaterBreak(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_POT_BROKEN);
        ObjTsubo_SpawnCollectible(this, play);
        OoT_Actor_Kill(&this->actor);
    } else if ((this->collider.base.acFlags & AC_HIT) &&
               (this->collider.info.acHitInfo->toucher.dmgFlags & 0x4FC1FFFC)) {
        ObjTsubo_AirBreak(this, play);
        ObjTsubo_SpawnCollectible(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_POT_BROKEN);
        OoT_Actor_Kill(&this->actor);
    } else {
        if (this->actor.xzDistToPlayer < 600.0f) {
            OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
            this->collider.base.acFlags &= ~AC_HIT;
            OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
            if (this->actor.xzDistToPlayer < 150.0f) {
                OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
            }
        }
        if (this->actor.xzDistToPlayer < 100.0f) {
            temp_v0 = this->actor.yawTowardsPlayer - GET_PLAYER(play)->actor.world.rot.y;
            phi_v1 = ABS(temp_v0);
            if (phi_v1 >= 0x5556) {
                // GI_NONE in this case allows the player to lift the actor
                OoT_Actor_OfferGetItem(&this->actor, play, GI_NONE, 30.0f, 30.0f);
            }
        }
    }
}

void ObjTsubo_SetupLiftedUp(ObjTsubo* this) {
    this->actionFunc = ObjTsubo_LiftedUp;
    this->actor.room = -1;
    OoT_Player_PlaySfx(&this->actor, NA_SE_PL_PULL_UP_POT);
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
}

void ObjTsubo_LiftedUp(ObjTsubo* this, PlayState* play) {
    if (OoT_Actor_HasNoParent(&this->actor, play)) {
        this->actor.room = play->roomCtx.curRoom.num;
        ObjTsubo_SetupThrown(this);
        ObjTsubo_ApplyGravity(this);
        OoT_Actor_UpdatePos(&this->actor);
        OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 5.0f, 15.0f, 0.0f, 0x85);
    }
}

void ObjTsubo_SetupThrown(ObjTsubo* this) {
    this->actor.velocity.x = OoT_Math_SinS(this->actor.world.rot.y) * this->actor.speedXZ;
    this->actor.velocity.z = OoT_Math_CosS(this->actor.world.rot.y) * this->actor.speedXZ;
    this->actor.colChkInfo.mass = 240;
    D_80BA1B50 = (OoT_Rand_ZeroOne() - 0.7f) * 2800.0f;
    D_80BA1B58 = (OoT_Rand_ZeroOne() - 0.5f) * 2000.0f;
    D_80BA1B54 = 0;
    D_80BA1B5C = 0;
    this->actionFunc = ObjTsubo_Thrown;
}

void ObjTsubo_Thrown(ObjTsubo* this, PlayState* play) {
    s32 pad[2];

    if ((this->actor.bgCheckFlags & 0xB) || (this->collider.base.atFlags & AT_HIT)) {
        ObjTsubo_AirBreak(this, play);
        ObjTsubo_SpawnCollectible(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_POT_BROKEN);
        OoT_Actor_Kill(&this->actor);
    } else if (this->actor.bgCheckFlags & 0x40) {
        ObjTsubo_WaterBreak(this, play);
        ObjTsubo_SpawnCollectible(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_POT_BROKEN);
        OoT_Actor_Kill(&this->actor);
    } else {
        ObjTsubo_ApplyGravity(this);
        OoT_Actor_UpdatePos(&this->actor);
        OoT_Math_StepToS(&D_80BA1B54, D_80BA1B50, 0x64);
        OoT_Math_StepToS(&D_80BA1B5C, D_80BA1B58, 0x64);
        this->actor.shape.rot.x += D_80BA1B54;
        this->actor.shape.rot.y += D_80BA1B5C;
        OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 5.0f, 15.0f, 0.0f, 0x85);
        OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
        OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
        OoT_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}

void OoT_ObjTsubo_Update(Actor* thisx, PlayState* play) {
    ObjTsubo* this = (ObjTsubo*)thisx;

    this->actionFunc(this, play);
}

void OoT_ObjTsubo_Draw(Actor* thisx, PlayState* play) {
    OoT_Gfx_DrawDListOpa(play, D_80BA1B84[(thisx->params >> 8) & 1]);
}
