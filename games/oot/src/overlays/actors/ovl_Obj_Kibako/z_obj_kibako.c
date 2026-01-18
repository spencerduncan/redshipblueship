/*
 * File: z_obj_kibako.c
 * Overlay: ovl_Obj_Kibako
 * Description: Small wooden box
 */

#include "z_obj_kibako.h"
#include "objects/gameplay_dangeon_keep/gameplay_dangeon_keep.h"
#include "overlays/effects/ovl_Effect_Ss_Kakera/z_eff_ss_kakera.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_CAN_PRESS_SWITCHES)

void OoT_ObjKibako_Init(Actor* thisx, PlayState* play);
void OoT_ObjKibako_Destroy(Actor* thisx, PlayState* play);
void OoT_ObjKibako_Update(Actor* thisx, PlayState* play);
void OoT_ObjKibako_Draw(Actor* thisx, PlayState* play);

void OoT_ObjKibako_SetupIdle(ObjKibako* this);
void OoT_ObjKibako_Idle(ObjKibako* this, PlayState* play);
void OoT_ObjKibako_SetupHeld(ObjKibako* this);
void OoT_ObjKibako_Held(ObjKibako* this, PlayState* play);
void OoT_ObjKibako_SetupThrown(ObjKibako* this);
void OoT_ObjKibako_Thrown(ObjKibako* this, PlayState* play);

const ActorInit Obj_Kibako_InitVars = {
    ACTOR_OBJ_KIBAKO,
    ACTORCAT_PROP,
    FLAGS,
    OBJECT_GAMEPLAY_DANGEON_KEEP,
    sizeof(ObjKibako),
    (ActorFunc)OoT_ObjKibako_Init,
    (ActorFunc)OoT_ObjKibako_Destroy,
    (ActorFunc)OoT_ObjKibako_Update,
    (ActorFunc)OoT_ObjKibako_Draw,
    NULL,
};

static ColliderCylinderInit OoT_sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000002, 0x00, 0x01 },
        { 0x4FC00748, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_ON,
    },
    { 12, 27, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit sCCInfoInit = { 0, 12, 60, MASS_HEAVY };

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneForward, 1000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 60, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 1000, ICHAIN_STOP),
};

void OoT_ObjKibako_SpawnCollectible(ObjKibako* this, PlayState* play) {
    s16 collectible;

    collectible = this->actor.params & 0x1F;
    if (GameInteractor_Should(VB_SMALL_CRATE_DROP_ITEM, (collectible >= 0) && (collectible <= 0x19), this)) {
        OoT_Item_DropCollectible(play, &this->actor.world.pos, collectible | (((this->actor.params >> 8) & 0x3F) << 8));
    }
}

void ObjKibako_ApplyGravity(ObjKibako* this) {
    this->actor.velocity.y += this->actor.gravity;
    if (this->actor.velocity.y < this->actor.minVelocityY) {
        this->actor.velocity.y = this->actor.minVelocityY;
    }
}

void ObjKibako_InitCollider(Actor* thisx, PlayState* play) {
    ObjKibako* this = (ObjKibako*)thisx;

    OoT_Collider_InitCylinder(play, &this->collider);
    OoT_Collider_SetCylinder(play, &this->collider, &this->actor, &OoT_sCylinderInit);
    OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
}

void OoT_ObjKibako_Init(Actor* thisx, PlayState* play) {
    s32 pad;
    ObjKibako* this = (ObjKibako*)thisx;

    OoT_Actor_ProcessInitChain(&this->actor, OoT_sInitChain);
    this->actor.gravity = -1.2f;
    this->actor.minVelocityY = -13.0f;
    ObjKibako_InitCollider(&this->actor, play);
    OoT_CollisionCheck_SetInfo(&this->actor.colChkInfo, NULL, &sCCInfoInit);
    OoT_ObjKibako_SetupIdle(this);
    // "wooden box"
    osSyncPrintf("(dungeon keep 木箱)(arg_data 0x%04x)\n", this->actor.params);
}

void OoT_ObjKibako_Destroy(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    ObjKibako* this = (ObjKibako*)thisx;

    OoT_Collider_DestroyCylinder(play, &this->collider);
}

void OoT_ObjKibako_AirBreak(ObjKibako* this, PlayState* play) {
    s16 angle;
    s32 i;
    Vec3f* breakPos = &this->actor.world.pos;
    Vec3f pos;
    Vec3f velocity;

    for (i = 0, angle = 0; i < 12; i++, angle += 0x4E20) {
        f32 sn = OoT_Math_SinS(angle);
        f32 cs = OoT_Math_CosS(angle);
        f32 temp_rand;
        s16 phi_s0;

        pos.x = sn * 16.0f;
        pos.y = (OoT_Rand_ZeroOne() * 5.0f) + 2.0f;
        pos.z = cs * 16.0f;
        velocity.x = pos.x * 0.2f;
        velocity.y = (OoT_Rand_ZeroOne() * 6.0f) + 2.0f;
        velocity.z = pos.z * 0.2f;
        pos.x += breakPos->x;
        pos.y += breakPos->y;
        pos.z += breakPos->z;
        temp_rand = OoT_Rand_ZeroOne();
        if (temp_rand < 0.1f) {
            phi_s0 = 0x60;
        } else if (temp_rand < 0.7f) {
            phi_s0 = 0x40;
        } else {
            phi_s0 = 0x20;
        }
        OoT_EffectSsKakera_Spawn(play, &pos, &velocity, breakPos, -200, phi_s0, 10, 10, 0, (OoT_Rand_ZeroOne() * 30.0f) + 10.0f,
                             0, 32, 60, KAKERA_COLOR_NONE, OBJECT_GAMEPLAY_DANGEON_KEEP, gSmallWoodenBoxFragmentDL);
    }
    func_80033480(play, &this->actor.world.pos, 40.0f, 3, 50, 140, 1);
}

void OoT_ObjKibako_WaterBreak(ObjKibako* this, PlayState* play) {
    s16 angle;
    s32 i;
    Vec3f* breakPos = &this->actor.world.pos;
    Vec3f pos;
    Vec3f velocity;

    pos = *breakPos;
    pos.y += this->actor.yDistToWater;
    OoT_EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, 500);

    for (i = 0, angle = 0; i < 12; i++, angle += 0x4E20) {
        f32 sn = OoT_Math_SinS(angle);
        f32 cs = OoT_Math_CosS(angle);
        f32 temp_rand;
        s16 phi_s0;

        pos.x = sn * 16.0f;
        pos.y = (OoT_Rand_ZeroOne() * 5.0f) + 2.0f;
        pos.z = cs * 16.0f;
        velocity.x = pos.x * 0.18f;
        velocity.y = (OoT_Rand_ZeroOne() * 4.0f) + 2.0f;
        velocity.z = pos.z * 0.18f;
        pos.x += breakPos->x;
        pos.y += breakPos->y;
        pos.z += breakPos->z;
        temp_rand = OoT_Rand_ZeroOne();
        phi_s0 = (temp_rand < 0.2f) ? 0x40 : 0x20;
        OoT_EffectSsKakera_Spawn(play, &pos, &velocity, breakPos, -180, phi_s0, 30, 30, 0, (OoT_Rand_ZeroOne() * 30.0f) + 10.0f,
                             0, 32, 70, KAKERA_COLOR_NONE, OBJECT_GAMEPLAY_DANGEON_KEEP, gSmallWoodenBoxFragmentDL);
    }
}

void OoT_ObjKibako_SetupIdle(ObjKibako* this) {
    this->actionFunc = OoT_ObjKibako_Idle;
    this->actor.colChkInfo.mass = MASS_HEAVY;
}

void OoT_ObjKibako_Idle(ObjKibako* this, PlayState* play) {
    s32 pad;

    if (OoT_Actor_HasParent(&this->actor, play)) {
        OoT_ObjKibako_SetupHeld(this);
    } else if ((this->actor.bgCheckFlags & 0x20) && (this->actor.yDistToWater > 19.0f)) {
        OoT_ObjKibako_WaterBreak(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
        OoT_ObjKibako_SpawnCollectible(this, play);
        OoT_Actor_Kill(&this->actor);
    } else if (this->collider.base.acFlags & AC_HIT) {
        OoT_ObjKibako_AirBreak(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
        OoT_ObjKibako_SpawnCollectible(this, play);
        OoT_Actor_Kill(&this->actor);
    } else {
        Actor_MoveXZGravity(&this->actor);
        OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 19.0f, 20.0f, 0.0f, 5);
        if (!(this->collider.base.ocFlags1 & OC1_TYPE_PLAYER) && (this->actor.xzDistToPlayer > 28.0f)) {
            this->collider.base.ocFlags1 |= OC1_TYPE_PLAYER;
        }
        if (this->actor.xzDistToPlayer < 600.0f) {
            OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
            OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
            if (this->actor.xzDistToPlayer < 180.0f) {
                OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
            }
        }
        if (this->actor.xzDistToPlayer < 100.0f) {
            OoT_Actor_OfferCarry(&this->actor, play);
        }
    }
}

void OoT_ObjKibako_SetupHeld(ObjKibako* this) {
    this->actionFunc = OoT_ObjKibako_Held;
    this->actor.room = -1;
    OoT_Player_PlaySfx(&this->actor, NA_SE_PL_PULL_UP_WOODBOX);
}

void OoT_ObjKibako_Held(ObjKibako* this, PlayState* play) {
    if (OoT_Actor_HasNoParent(&this->actor, play)) {
        this->actor.room = play->roomCtx.curRoom.num;
        if (fabsf(this->actor.speedXZ) < 0.1f) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_PUT_DOWN_WOODBOX);
            OoT_ObjKibako_SetupIdle(this);
            this->collider.base.ocFlags1 &= ~OC1_TYPE_PLAYER;
        } else {
            OoT_ObjKibako_SetupThrown(this);
            ObjKibako_ApplyGravity(this);
            OoT_Actor_UpdatePos(&this->actor);
        }
        OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 19.0f, 20.0f, 0.0f, 5);
    }
}

void OoT_ObjKibako_SetupThrown(ObjKibako* this) {
    this->actor.velocity.x = OoT_Math_SinS(this->actor.world.rot.y) * this->actor.speedXZ;
    this->actor.velocity.z = OoT_Math_CosS(this->actor.world.rot.y) * this->actor.speedXZ;
    this->actor.colChkInfo.mass = 240;
    this->actionFunc = OoT_ObjKibako_Thrown;
}

void OoT_ObjKibako_Thrown(ObjKibako* this, PlayState* play) {
    s32 pad;
    s32 pad2;

    if ((this->actor.bgCheckFlags & 0xB) || (this->collider.base.atFlags & AT_HIT)) {
        OoT_ObjKibako_AirBreak(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
        OoT_ObjKibako_SpawnCollectible(this, play);
        OoT_Actor_Kill(&this->actor);
    } else if (this->actor.bgCheckFlags & 0x40) {
        OoT_ObjKibako_WaterBreak(this, play);
        OoT_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
        OoT_ObjKibako_SpawnCollectible(this, play);
        OoT_Actor_Kill(&this->actor);
    } else {
        ObjKibako_ApplyGravity(this);
        OoT_Actor_UpdatePos(&this->actor);
        OoT_Actor_UpdateBgCheckInfo(play, &this->actor, 19.0f, 20.0f, 0.0f, 5);
        OoT_Collider_UpdateCylinder(&this->actor, &this->collider);
        OoT_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
        OoT_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}

void OoT_ObjKibako_Update(Actor* thisx, PlayState* play) {
    s32 pad;
    ObjKibako* this = (ObjKibako*)thisx;

    this->actionFunc(this, play);
}

void OoT_ObjKibako_Draw(Actor* thisx, PlayState* play) {
    s32 pad;
    ObjKibako* this = (ObjKibako*)thisx;

    if (!GameInteractor_Should(VB_SMALL_CRATE_SETUP_DRAW, true, thisx)) {
        return;
    }

    OoT_Gfx_DrawDListOpa(play, gSmallWoodenBoxDL);
}
