/*
 * File: z_obj_kibako.c
 * Overlay: ovl_Obj_Kibako
 * Description: Small grabbable crate
 */

#include "z_obj_kibako.h"
#include "objects/gameplay_dangeon_keep/gameplay_dangeon_keep.h"
#include "objects/object_kibako/object_kibako.h"
#include "GameInteractor/GameInteractor.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_CAN_PRESS_SWITCHES)

void MM_ObjKibako_Init(Actor* thisx, PlayState* play2);
void MM_ObjKibako_Destroy(Actor* thisx, PlayState* play2);
void MM_ObjKibako_Update(Actor* thisx, PlayState* play);

void MM_ObjKibako_Draw(Actor* thisx, PlayState* play);
void MM_ObjKibako_SpawnCollectible(ObjKibako* this, PlayState* play);
void func_809262BC(ObjKibako* this);
void func_80926318(ObjKibako* this, PlayState* play);
void MM_ObjKibako_AirBreak(ObjKibako* this, PlayState* play);
void MM_ObjKibako_WaterBreak(ObjKibako* this, PlayState* play);
void func_80926B40(ObjKibako* this);
void func_80926B54(ObjKibako* this, PlayState* play);
void MM_ObjKibako_SetupIdle(ObjKibako* this);
void MM_ObjKibako_Idle(ObjKibako* this, PlayState* play);
void MM_ObjKibako_SetupHeld(ObjKibako* this);
void MM_ObjKibako_Held(ObjKibako* this, PlayState* play);
void MM_ObjKibako_SetupThrown(ObjKibako* this);
void MM_ObjKibako_Thrown(ObjKibako* this, PlayState* play);

static s16 D_80927380 = 0;
static s16 D_80927384 = 0;
static s16 D_80927388 = 0;
static s16 D_8092738C = 0;

ActorProfile Obj_Kibako_Profile = {
    /**/ ACTOR_OBJ_KIBAKO,
    /**/ ACTORCAT_PROP,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(ObjKibako),
    /**/ MM_ObjKibako_Init,
    /**/ MM_ObjKibako_Destroy,
    /**/ MM_ObjKibako_Update,
    /**/ NULL,
};

static ColliderCylinderInit MM_sCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00400000, 0x00, 0x02 },
        { 0x058BC79C, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NONE,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 15, 30, 0, { 0, 0, 0 } },
};

static s16 MM_sObjectIds[] = { GAMEPLAY_DANGEON_KEEP, OBJECT_KIBAKO };

static Gfx* sKakeraDisplayLists[] = { gameplay_dangeon_keep_DL_007980, gSmallCrateFragmentDL };

static Gfx* MM_sDisplayLists[] = { gameplay_dangeon_keep_DL_007890, gSmallCrateDL };

static InitChainEntry MM_sInitChain[] = {
    ICHAIN_F32_DIV1000(gravity, -1500, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(terminalVelocity, -18000, ICHAIN_CONTINUE),
    ICHAIN_F32(cullingVolumeScale, 60, ICHAIN_CONTINUE),
    ICHAIN_F32(cullingVolumeDownward, 60, ICHAIN_STOP),
};

void MM_ObjKibako_SpawnCollectible(ObjKibako* this, PlayState* play) {
    if (!GameInteractor_Should(VB_BARREL_OR_CRATE_DROP_COLLECTIBLE, true, this)) {
        return;
    }

    s32 dropItem00Id;

    if (this->isDropCollected == 0) {
        dropItem00Id = func_800A8150(KIBAKO_COLLECTIBLE_ID(&this->actor));
        if (dropItem00Id > ITEM00_NO_DROP) {
            MM_Item_DropCollectible(play, &this->actor.world.pos,
                                 dropItem00Id | KIBAKO_COLLECTIBLE_FLAG(&this->actor) << 8);
            this->isDropCollected = 1;
        }
    }
}

void ObjKibako_SetShadow(ObjKibako* this) {
    if ((this->actor.projectedPos.z < 370.0f) && (this->actor.projectedPos.z > -10.0f)) {
        this->actor.shape.shadowDraw = ActorShadow_DrawSquare;
        this->actor.shape.shadowScale = 1.4f;
        this->actor.shape.shadowAlpha =
            (this->actor.projectedPos.z < 200.0f) ? 100 : (400 - ((s32)this->actor.projectedPos.z)) >> 1;
    } else {
        this->actor.shape.shadowDraw = NULL;
    }
}

void func_809262BC(ObjKibako* this) {
    s16 angle = this->actor.world.rot.y;

    if ((angle & 0x3FFF) != 0) {
        angle = MM_Math_ScaledStepToS(&this->actor.world.rot.y, (s16)(angle + 0x2000) & 0xC000, 0x640);
        this->actor.shape.rot.y = this->actor.world.rot.y;
    }
}

void func_80926318(ObjKibako* this, PlayState* play) {
    s16 angle;
    s32 pad;

    if (this->actor.xzDistToPlayer < 100.0f) {
        angle = this->actor.yawTowardsPlayer - GET_PLAYER(play)->actor.world.rot.y;
        if (ABS_ALT(angle) > 0x5555) {
            MM_Actor_OfferGetItem(&this->actor, play, GI_NONE, 36.0f, 30.0f);
        }
    }
}

void func_80926394(ObjKibako* this, PlayState* play) {
    if ((this->isDropCollected == 0) && (play->roomCtx.curRoom.num != this->unk199)) {
        this->isDropCollected = 1;
    }
}

void MM_ObjKibako_Init(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    ObjKibako* this = (ObjKibako*)thisx;
    s32 objectIndex;

    objectIndex = KIBAKO_BANK_INDEX(thisx);
    MM_Actor_ProcessInitChain(&this->actor, MM_sInitChain);
    MM_Actor_SetScale(&this->actor, 0.15f);
    if (objectIndex == 0) {
        this->actor.cullingVolumeDistance = 4000.0f;
    } else {
        this->actor.cullingVolumeDistance = 800.0f;
    }
    MM_Collider_InitCylinder(play, &this->collider);
    MM_Collider_SetCylinder(play, &this->collider, &this->actor, &MM_sCylinderInit);
    MM_Collider_UpdateCylinder(&this->actor, &this->collider);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->objectSlot = Object_GetSlot(&play->objectCtx, MM_sObjectIds[objectIndex]);
    if (this->objectSlot <= OBJECT_SLOT_NONE) {
        MM_Actor_Kill(&this->actor);
        return;
    }
    this->unk199 = this->actor.room;
    func_80926B40(this);
}

void MM_ObjKibako_Destroy(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    ObjKibako* this = (ObjKibako*)thisx;

    MM_Collider_DestroyCylinder(play, &this->collider);
}

void MM_ObjKibako_AirBreak(ObjKibako* this, PlayState* play) {
    s16 angle;
    s32 i;
    Vec3f* worldPos = &this->actor.world.pos;
    Vec3f pos;
    Vec3f velocity;

    for (i = 0, angle = 0; i < 12; i++, angle += 0x4E20) {
        f32 sn = MM_Math_SinS(angle);
        f32 cs = MM_Math_CosS(angle);
        f32 temp_rand;
        s16 phi_s0;

        pos.x = sn * 16.0f;
        pos.y = (MM_Rand_ZeroOne() * 5.0f) + 2.0f;
        pos.z = cs * 16.0f;
        velocity.x = pos.x * 0.2f;
        velocity.y = (MM_Rand_ZeroOne() * 6.0f) + 2.0f;
        velocity.z = pos.z * 0.2f;
        pos.x += worldPos->x;
        pos.y += worldPos->y;
        pos.z += worldPos->z;
        temp_rand = MM_Rand_ZeroOne();
        if (temp_rand < 0.1f) {
            phi_s0 = 0x60;
        } else if (temp_rand < 0.7f) {
            phi_s0 = 0x40;
        } else {
            phi_s0 = 0x20;
        }

        MM_EffectSsKakera_Spawn(play, &pos, &velocity, worldPos, -200, phi_s0, 20, 0, 0, (MM_Rand_ZeroOne() * 38.0f) + 10.0f,
                             0, 0, 60, -1, MM_sObjectIds[KIBAKO_BANK_INDEX(&this->actor)],
                             sKakeraDisplayLists[KIBAKO_BANK_INDEX(&this->actor)]);
    }

    func_800BBFB0(play, worldPos, 40.0f, 3, 0x32, 0x8C, 1);
    func_800BBFB0(play, worldPos, 40.0f, 2, 0x14, 0x50, 1);
}

void MM_ObjKibako_WaterBreak(ObjKibako* this, PlayState* play) {
    s16 angle;
    s32 i;
    Vec3f* worldPos = &this->actor.world.pos;
    Vec3f pos;
    Vec3f velocity;

    pos.y = worldPos->y + this->actor.depthInWater;
    for (angle = 0, i = 0; i < 5; i++, angle += 0x3333) {
        pos.x = worldPos->x + (MM_Math_SinS(((s32)(MM_Rand_ZeroOne() * 6000.0f)) + angle) * 15.0f);
        pos.z = worldPos->z + (MM_Math_CosS(((s32)(MM_Rand_ZeroOne() * 6000.0f)) + angle) * 15.0f);
        MM_EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, 350);
    }
    pos.x = worldPos->x;
    pos.z = worldPos->z;
    MM_EffectSsGRipple_Spawn(play, &pos, 200, 600, 0);

    for (i = 0, angle = 0; i < 12; i++, angle += 0x4E20) {
        f32 sn = MM_Math_SinS(angle);
        f32 cs = MM_Math_CosS(angle);
        f32 temp_rand;
        s16 phi_s0;

        pos.x = sn * 16.0f;
        pos.y = (MM_Rand_ZeroOne() * 5.0f) + 2.0f;
        pos.z = cs * 16.0f;
        velocity.x = pos.x * 0.18f;
        velocity.y = (MM_Rand_ZeroOne() * 4.0f) + 2.0f;
        velocity.z = pos.z * 0.18f;
        pos.x += worldPos->x;
        pos.y += worldPos->y;
        pos.z += worldPos->z;
        temp_rand = MM_Rand_ZeroOne();
        phi_s0 = (temp_rand < 0.2f) ? 0x40 : 0x20;

        MM_EffectSsKakera_Spawn(play, &pos, &velocity, worldPos, -180, phi_s0, 50, 5, 0, (MM_Rand_ZeroOne() * 35.0f) + 10.0f,
                             0, 0, 70, -1, MM_sObjectIds[KIBAKO_BANK_INDEX(&this->actor)],
                             sKakeraDisplayLists[KIBAKO_BANK_INDEX(&this->actor)]);
    }
}

void func_80926B40(ObjKibako* this) {
    this->actionFunc = func_80926B54;
}

void func_80926B54(ObjKibako* this, PlayState* play) {
    Actor_MoveWithGravity(&this->actor);
    MM_Actor_UpdateBgCheckInfo(play, &this->actor, 18.0f, 15.0f, 0.0f,
                            UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_40);
    if (MM_Object_IsLoaded(&play->objectCtx, this->objectSlot)) {
        if (GameInteractor_Should(VB_CRATE_DRAW_BE_OVERRIDDEN, true, this)) {
            this->actor.draw = MM_ObjKibako_Draw;
        }
        this->actor.objectSlot = this->objectSlot;
        MM_ObjKibako_SetupIdle(this);
    }
}

void MM_ObjKibako_SetupIdle(ObjKibako* this) {
    this->actionFunc = MM_ObjKibako_Idle;
}

void MM_ObjKibako_Idle(ObjKibako* this, PlayState* play) {
    s32 pad;
    s32 pad1;

    if (MM_Actor_HasParent(&this->actor, play)) {
        MM_ObjKibako_SetupHeld(this);
        this->actor.room = -1;
        this->actor.colChkInfo.mass = 120;
        if (func_800A817C(KIBAKO_COLLECTIBLE_ID(&this->actor))) {
            MM_ObjKibako_SpawnCollectible(this, play);
        }

        //! @bug: This function should only pass Player*: it uses *(this + 0x153), which is meant to be
        //! player->currentMask, but in this case is garbage in the collider
        MM_Player_PlaySfx((Player*)&this->actor, NA_SE_PL_PULL_UP_WOODBOX);
    } else if ((this->actor.bgCheckFlags & BGCHECKFLAG_WATER) && (this->actor.depthInWater > 19.0f)) {
        MM_ObjKibako_WaterBreak(this, play);
        MM_ObjKibako_SpawnCollectible(this, play);
        MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
        MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_DIVE_INTO_WATER_L);
        MM_Actor_Kill(&this->actor);
    } else if (this->collider.base.acFlags & AC_HIT) {
        MM_ObjKibako_AirBreak(this, play);
        MM_ObjKibako_SpawnCollectible(this, play);
        MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
        MM_Actor_Kill(&this->actor);
    } else {
        Actor_MoveWithGravity(&this->actor);
        func_809262BC(this);
        MM_Actor_UpdateBgCheckInfo(play, &this->actor, 18.0f, 15.0f, 0.0f,
                                UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_40);

        if (!(this->collider.base.ocFlags1 & OC1_TYPE_PLAYER) && (this->actor.xzDistToPlayer > 28.0f)) {
            this->collider.base.ocFlags1 |= OC1_TYPE_PLAYER;
        }

        if ((this->actor.colChkInfo.mass != MASS_IMMOVABLE) &&
            (MM_Math3D_Vec3fDistSq(&this->actor.world.pos, &this->actor.prevPos) < 0.01f)) {
            this->actor.colChkInfo.mass = MASS_IMMOVABLE;
        }

        this->collider.base.acFlags &= ~AC_HIT;

        if (KIBAKO_BOMBER_CAN_HIDE_IN_BOX(&this->actor)) {
            MM_Collider_UpdateCylinder(&this->actor, &this->collider);
            MM_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);

            if (this->actor.xzDistToPlayer < 800.0f) {
                MM_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
                func_80926318(this, play);
            }
        } else {
            if (this->actor.xzDistToPlayer < 800.0f) {
                MM_Collider_UpdateCylinder(&this->actor, &this->collider);
                MM_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);

                if (this->actor.xzDistToPlayer < 180.0f) {
                    MM_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
                    func_80926318(this, play);
                }
            }
        }
    }
}

void MM_ObjKibako_SetupHeld(ObjKibako* this) {
    this->actionFunc = MM_ObjKibako_Held;
}

void MM_ObjKibako_Held(ObjKibako* this, PlayState* play) {
    s32 pad;
    Vec3f pos;
    s32 bgId;

    func_80926394(this, play);
    if (MM_Actor_HasNoParent(&this->actor, play)) {
        this->actor.room = play->roomCtx.curRoom.num;
        if (fabsf(this->actor.speed) < 0.1f) {
            MM_ObjKibako_SetupIdle(this);
            this->collider.base.ocFlags1 &= ~OC1_TYPE_PLAYER;
            Actor_PlaySfx(&this->actor, NA_SE_EV_PUT_DOWN_WOODBOX);
        } else {
            Actor_MoveWithGravity(&this->actor);
            MM_ObjKibako_SetupThrown(this);
            this->actor.flags &= ~ACTOR_FLAG_CAN_PRESS_SWITCHES;
        }
        MM_Actor_UpdateBgCheckInfo(play, &this->actor, 18.0f, 15.0f, 0.0f,
                                UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_40);
    } else {
        pos.x = this->actor.world.pos.x;
        pos.y = this->actor.world.pos.y + 20.0f;
        pos.z = this->actor.world.pos.z;
        this->actor.floorHeight =
            MM_BgCheck_EntityRaycastFloor5(&play->colCtx, &this->actor.floorPoly, &bgId, &this->actor, &pos);
    }
}

void MM_ObjKibako_SetupThrown(ObjKibako* this) {
    f32 temp;

    D_80927380 = 0;
    temp = (MM_Rand_ZeroOne() - 0.5f) * 1000.0f;
    D_80927388 = temp;
    D_80927384 = (MM_Rand_ZeroOne() - 2.0f) * 1500.0f;
    D_8092738C = temp * 3.0f;
    this->timer = 80;
    this->actionFunc = MM_ObjKibako_Thrown;
}

void MM_ObjKibako_Thrown(ObjKibako* this, PlayState* play) {
    void* pad;
    void* pad2;
    s32 atHit;

    atHit = (this->collider.base.atFlags & AT_HIT) != 0;
    if (atHit) {
        this->collider.base.atFlags &= ~AT_HIT;
    }
    func_80926394(this, play);
    if (this->timer > 0) {
        this->timer--;
    }
    if ((this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH | BGCHECKFLAG_WALL)) || atHit ||
        (this->timer <= 0)) {
        MM_ObjKibako_AirBreak(this, play);
        MM_ObjKibako_SpawnCollectible(this, play);
        MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
        MM_Actor_Kill(&this->actor);
    } else {
        if (this->actor.bgCheckFlags & BGCHECKFLAG_WATER_TOUCH) {
            MM_ObjKibako_WaterBreak(this, play);
            MM_ObjKibako_SpawnCollectible(this, play);
            MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
            MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_DIVE_INTO_WATER_L);
            MM_Actor_Kill(&this->actor);
        } else {
            if (this->actor.velocity.y < -0.05f) {
                this->actor.gravity = -2.3f;
            }
            Actor_MoveWithGravity(&this->actor);
            MM_Math_StepToS(&D_80927384, D_80927380, 0xA0);
            MM_Math_StepToS(&D_8092738C, D_80927388, 0xA0);
            this->actor.shape.rot.x = (s16)(this->actor.shape.rot.x + D_80927384);
            this->actor.shape.rot.y = (s16)(this->actor.shape.rot.y + D_8092738C);
            MM_Actor_UpdateBgCheckInfo(play, &this->actor, 18.0f, 15.0f, 0.0f,
                                    UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_40);
            MM_Collider_UpdateCylinder(&this->actor, &this->collider);
            MM_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
            MM_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        }
    }
}

void MM_ObjKibako_Update(Actor* thisx, PlayState* play) {
    ObjKibako* this = (ObjKibako*)thisx;

    this->actionFunc(this, play);
    ObjKibako_SetShadow(this);
}

void MM_ObjKibako_Draw(Actor* thisx, PlayState* play) {
    MM_Gfx_DrawDListOpa(play, MM_sDisplayLists[KIBAKO_BANK_INDEX(thisx)]);
}
