/*
 * File: z_en_kusa.c
 * Overlay: ovl_En_Kusa
 * Description: Grass / Bush
 */

#include "prevent_bss_reordering.h"
#include "z_en_kusa.h"
#include "objects/object_kusa/object_kusa.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/gameplay_field_keep/gameplay_field_keep.h"
#include "overlays/actors/ovl_En_Insect/z_en_insect.h"

#include "2s2h/ShipUtils.h"
#include "GameInteractor/GameInteractor.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_THROW_ONLY)

void MM_EnKusa_Init(Actor* thisx, PlayState* play);
void MM_EnKusa_Destroy(Actor* thisx, PlayState* play);
void MM_EnKusa_Update(Actor* thisx, PlayState* play2);

s32 MM_EnKusa_SnapToFloor(EnKusa* this, PlayState* play, f32 yOffset);
void MM_EnKusa_DropCollectible(EnKusa* this, PlayState* play);
void MM_EnKusa_UpdateVelY(EnKusa* this);
void MM_EnKusa_RandScaleVecToZero(Vec3f* vec, f32 scaleFactor);
void MM_EnKusa_SetScaleSmall(EnKusa* this);
s32 EnKusa_IsUnderwater(EnKusa* this, PlayState* play);
void MM_EnKusa_SetupWaitObject(EnKusa* this);
void MM_EnKusa_WaitObject(EnKusa* this, PlayState* play);
void EnKusa_WaitForInteract(EnKusa* this, PlayState* play);
void MM_EnKusa_SetupLiftedUp(EnKusa* this);
void MM_EnKusa_LiftedUp(EnKusa* this, PlayState* play);
void MM_EnKusa_WaitObject(EnKusa* this, PlayState* play);
void EnKusa_SetupInteract(EnKusa* this);
void MM_EnKusa_SetupFall(EnKusa* this);
void MM_EnKusa_Fall(EnKusa* this, PlayState* play);
void MM_EnKusa_SetupCut(EnKusa* this);
void MM_EnKusa_CutWaitRegrow(EnKusa* this, PlayState* play);
void MM_EnKusa_DoNothing(EnKusa* this, PlayState* play);
void MM_EnKusa_SetupUprootedWaitRegrow(EnKusa* this);
void MM_EnKusa_UprootedWaitRegrow(EnKusa* this, PlayState* play);
void MM_EnKusa_SetupRegrow(EnKusa* this);
void MM_EnKusa_Regrow(EnKusa* this, PlayState* play);
void EnKusa_DrawBush(Actor* thisx, PlayState* play2);
void EnKusa_DrawGrass(Actor* thisx, PlayState* play);

s16 MM_rotSpeedXtarget = 0;
s16 MM_rotSpeedX = 0;
s16 MM_rotSpeedYtarget = 0;
s16 MM_rotSpeedY = 0;
s16 D_809366B0 = 0;
u8 D_809366B4 = true;

u32 kusaGameplayFrames;
MtxF D_80936AD8[8];
s16 D_80936CD8;
s16 D_80936CDA;
s16 D_80936CDC;
s16 D_80936CDE;
s16 D_80936CE0;

ActorProfile En_Kusa_Profile = {
    /**/ ACTOR_EN_KUSA,
    /**/ ACTORCAT_PROP,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(EnKusa),
    /**/ MM_EnKusa_Init,
    /**/ MM_EnKusa_Destroy,
    /**/ MM_EnKusa_Update,
    /**/ NULL,
};

static s16 MM_sObjectIds[] = { GAMEPLAY_FIELD_KEEP, OBJECT_KUSA, OBJECT_KUSA, OBJECT_KUSA };

static ColliderCylinderInit MM_sCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER | OC1_TYPE_2,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00400000, 0x00, 0x02 },
        { 0x0580C71C, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NONE,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 6, 44, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit MM_sColChkInfoInit = { 0, 12, 30, MASS_IMMOVABLE };

static Vec3f MM_sUnitDirections[] = {
    { 0.0f, 0.7071f, 0.7071f },
    { 0.7071f, 0.7071f, 0.0f },
    { 0.0f, 0.7071f, -0.7071f },
    { -0.7071f, 0.7071f, 0.0f },
};

static s16 MM_sFragmentScales[] = { 108, 102, 96, 84, 66, 55, 42, 38 };

static InitChainEntry MM_sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 400, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, -3200, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(terminalVelocity, -17000, ICHAIN_CONTINUE),
    ICHAIN_F32(cullingVolumeDistance, 1200, ICHAIN_CONTINUE),
    ICHAIN_F32(cullingVolumeScale, 100, ICHAIN_CONTINUE),
    ICHAIN_F32(cullingVolumeDownward, 200, ICHAIN_STOP),
};

/**
 * @brief Applies a "swaying" motion to the provided matrix
 *
 */
void EnKusa_ApplySway(MtxF* matrix) {
    MtxF* mtxState = MM_Matrix_GetCurrent();
    f32* tmp = &mtxState->mf[0][0];
    f32* tmp2 = &matrix->mf[0][0];
    s32 i;

    for (i = 0; i < 16; i++) {
        *tmp++ += *tmp2++;
    }
}

/**
 * @brief Updates the matrix controlling movement of the leaves of grass to simulate a swaying motion from the wind
 * blowing.
 */
void EnKusa_Sway(void) {
    s32 i;
    s32 pad;
    f32 sin_6CDA;
    f32* ptr;
    f32 sin_6CDE;
    f32 sin_6CE0;
    f32 tempf1;
    f32 tempf2;
    f32 tempf3;
    f32 tempf4;
    f32 tempf5;
    f32 sp7C[8];
    f32 cos_6CE0;
    f32 cos_6CDA;
    f32 cos_6CDC;
    f32 cos_6CDE;
    f32 cos_6CD8;
    f32 sin_6CD8;
    f32 sin_6CDC;

    D_80936CD8 += 0x46;
    D_80936CDA += 0x12C;
    D_80936CDC += 0x2BC;
    D_80936CDE += 0x514;
    D_80936CE0 += 0x22C4;

    sin_6CD8 = MM_Math_SinS(D_80936CD8);
    sin_6CDA = MM_Math_SinS(D_80936CDA);
    sin_6CDC = MM_Math_SinS(D_80936CDC);
    sin_6CDE = MM_Math_SinS(D_80936CDE) * 1.2f;
    sin_6CE0 = MM_Math_SinS(D_80936CE0) * 1.5f;

    cos_6CD8 = MM_Math_CosS(D_80936CD8);
    cos_6CDA = MM_Math_CosS(D_80936CDA);
    cos_6CDC = MM_Math_CosS(D_80936CDC);
    cos_6CDE = MM_Math_CosS(D_80936CDE) * 1.3f;
    cos_6CE0 = MM_Math_CosS(D_80936CE0) * 1.7f;

    sp7C[0] = (sin_6CD8 - cos_6CDA) * sin_6CDC * cos_6CD8 * sin_6CD8 * 0.0015f;
    sp7C[1] = (sin_6CDA - cos_6CDC) * sin_6CDE * cos_6CDA * sin_6CD8 * 0.0015f;
    sp7C[2] = (sin_6CDC - cos_6CDE) * cos_6CDC * sin_6CD8 * cos_6CD8 * 0.0015f;
    sp7C[3] = (sin_6CDE - cos_6CDA) * cos_6CDE * sin_6CDA * cos_6CD8 * 0.0015f;
    sp7C[4] = (sin_6CD8 - cos_6CDC) * sin_6CD8 * sin_6CDA * sin_6CE0 * 0.0015f;
    sp7C[5] = (sin_6CDA - cos_6CDE) * sin_6CDC * sin_6CDE * sin_6CE0 * 0.0015f;
    sp7C[6] = (sin_6CDC - cos_6CD8) * cos_6CD8 * cos_6CDA * cos_6CE0 * 0.0015f;
    sp7C[7] = (sin_6CDE - cos_6CDA) * cos_6CDC * cos_6CDE * cos_6CE0 * 0.0015f;

    for (i = 0; i < ARRAY_COUNT(D_80936AD8); i++) {
        ptr = &D_80936AD8[i].mf[0][0];

        tempf1 = sp7C[i & 7];
        tempf2 = sp7C[(i + 1) & 7];
        tempf3 = sp7C[(i + 2) & 7];
        tempf4 = sp7C[(i + 3) & 7];
        tempf5 = sp7C[(i + 4) & 7];

        ptr[0] = sp7C[1] * 0.2f;
        ptr[1] = tempf1;
        ptr[2] = tempf2;
        ptr[3] = 0.0f;

        ptr[4] = tempf3;
        ptr[5] = sp7C[0];
        ptr[6] = tempf3;
        ptr[7] = 0.0f;

        ptr[8] = tempf4;
        ptr[9] = tempf5;
        ptr[10] = sp7C[3] * 0.2f;
        ptr[11] = 0.0f;

        ptr[12] = 0.0f;
        ptr[13] = 0.0f;
        ptr[14] = 0.0f;
        ptr[15] = 0.0f;
    }
}

/**
 * @brief Detects if a bush is able to snap to the floor. MM_BgCheck_EntityRaycastFloor5 will give the intersect point
 *        if no poit is found, a false value is returned.
 *
 * @param this
 * @param play
 * @param yOffset offset of Y coordinate, can be positive or negative.
 * @return true/false if the bush is able to snap to the floor and is above BGCHECK_Y_MIN
 */
s32 MM_EnKusa_SnapToFloor(EnKusa* this, PlayState* play, f32 yOffset) {
    s32 pad;
    CollisionPoly* poly;
    Vec3f pos;
    s32 bgId;
    f32 floorY;

    pos.x = this->actor.world.pos.x;
    pos.y = this->actor.world.pos.y + 30.0f;
    pos.z = this->actor.world.pos.z;

    floorY = MM_BgCheck_EntityRaycastFloor5(&play->colCtx, &poly, &bgId, &this->actor, &pos);
    if (floorY > BGCHECK_Y_MIN) {
        this->actor.world.pos.y = floorY + yOffset;
        MM_Math_Vec3f_Copy(&this->actor.home.pos, &this->actor.world.pos);
        return true;
    } else {
        return false;
    }
}

void MM_EnKusa_DropCollectible(EnKusa* this, PlayState* play) {
    s32 collectible;
    s32 collectableParams;

    if ((KUSA_GET_TYPE(&this->actor) == ENKUSA_TYPE_GRASS) || (KUSA_GET_TYPE(&this->actor) == ENKUSA_TYPE_BUSH)) {
        if (!KUSA_GET_PARAM_0C(&this->actor)) {
            if (GameInteractor_Should(VB_GRASS_DROP_COLLECTIBLE, true, ACTOR_EN_KUSA, this)) {
                MM_Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos,
                                           KUSA_GET_RAND_COLLECTIBLE_ID(&this->actor) * 0x10);
            }
        }
    } else if (KUSA_GET_TYPE(&this->actor) == ENKUSA_TYPE_REGROWING_GRASS) {
        MM_Item_DropCollectible(play, &this->actor.world.pos, 3);
    } else { // ENKUSA_TYPE_GRASS_2
        collectible = func_800A8150(KUSA_GET_PARAM_FC(&this->actor));
        if (collectible >= 0) {
            collectableParams = KUSA_GET_COLLECTIBLE_ID(&this->actor);
            MM_Item_DropCollectible(play, &this->actor.world.pos, (collectableParams << 8) | collectible);
        }
    }
}

void MM_EnKusa_UpdateVelY(EnKusa* this) {
    this->actor.velocity.y += this->actor.gravity;
    if (this->actor.velocity.y < this->actor.terminalVelocity) {
        this->actor.velocity.y = this->actor.terminalVelocity;
    }
}

/**
 * @brief Scales a vector down by provided scale factor
 *
 * @param vec vector to be scaled
 * @param scaleFactor scale factor to be applied to vector
 */
void MM_EnKusa_RandScaleVecToZero(Vec3f* vec, f32 scaleFactor) {
    scaleFactor += ((MM_Rand_ZeroOne() * 0.2f) - 0.1f) * scaleFactor;
    vec->x -= vec->x * scaleFactor;
    vec->y -= vec->y * scaleFactor;
    vec->z -= vec->z * scaleFactor;
}

void MM_EnKusa_SetScaleSmall(EnKusa* this) {
    this->actor.scale.y = 160.0f * 0.001f;
    this->actor.scale.x = 120.0f * 0.001f;
    this->actor.scale.z = 120.0f * 0.001f;
}

void MM_EnKusa_SpawnFragments(EnKusa* this, PlayState* play) {
    Vec3f velocity;
    Vec3f pos;
    s32 i;
    s32 scaleIndex;
    Vec3f* directon;
    s32 pad;

    for (i = 0; i < ARRAY_COUNT(MM_sUnitDirections); i++) {
        directon = &MM_sUnitDirections[i];

        pos.x = this->actor.world.pos.x + (directon->x * this->actor.scale.x * 20.0f);
        pos.y = this->actor.world.pos.y + (directon->y * this->actor.scale.y * 20.0f) + 10.0f;
        pos.z = this->actor.world.pos.z + (directon->z * this->actor.scale.z * 20.0f);
        velocity.x = (MM_Rand_ZeroOne() - 0.5f) * 8.0f;
        velocity.y = MM_Rand_ZeroOne() * 10.0f;
        velocity.z = (MM_Rand_ZeroOne() - 0.5f) * 8.0f;

        scaleIndex = (s32)(MM_Rand_ZeroOne() * 111.1f) & 7;

        MM_EffectSsKakera_Spawn(play, &pos, &velocity, &pos, -100, 64, 40, 3, 0, MM_sFragmentScales[scaleIndex], 0, 0, 0x50,
                             -1, 1, gKakeraLeafMiddleDL);

        pos.x = this->actor.world.pos.x + (directon->x * this->actor.scale.x * 40.0f);
        pos.y = this->actor.world.pos.y + (directon->y * this->actor.scale.y * 40.0f) + 10.0f;
        pos.z = this->actor.world.pos.z + (directon->z * this->actor.scale.z * 40.0f);
        velocity.x = (MM_Rand_ZeroOne() - 0.5f) * 6.0f;
        velocity.y = MM_Rand_ZeroOne() * 10.0f;
        velocity.z = (MM_Rand_ZeroOne() - 0.5f) * 6.0f;

        scaleIndex = (s32)(MM_Rand_ZeroOne() * 111.1f) % 7;

        MM_EffectSsKakera_Spawn(play, &pos, &velocity, &pos, -100, 64, 40, 3, 0, MM_sFragmentScales[scaleIndex], 0, 0, 0x50,
                             -1, 1, gKakeraLeafTipDL);
    }
}

void MM_EnKusa_SpawnBugs(EnKusa* this, PlayState* play) {
    u32 numBugs;

    for (numBugs = 0; numBugs < 3; numBugs++) {
        Actor* bug = Actor_SpawnAsChildAndCutscene(
            &play->actorCtx, play, ACTOR_EN_INSECT, this->actor.world.pos.x, this->actor.world.pos.y,
            this->actor.world.pos.z, 0, 0, 0, ENINSECT_PARAMS(true), this->actor.csId, this->actor.halfDaysBits, 0);

        if (bug == NULL) {
            break;
        }
    }
}

s32 EnKusa_IsUnderwater(EnKusa* this, PlayState* play) {
    s32 pad;
    WaterBox* waterBox;
    f32 waterSurface;
    s32 bgId;

    if (MM_WaterBox_GetSurfaceImpl(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &waterSurface,
                                &waterBox, &bgId) &&
        (this->actor.world.pos.y < waterSurface)) {
        return true;
    }
    return false;
}

void MM_EnKusa_InitCollider(Actor* thisx, PlayState* play) {
    EnKusa* this = (EnKusa*)thisx;

    MM_Collider_InitCylinder(play, &this->collider);
    MM_Collider_SetCylinder(play, &this->collider, &this->actor, &MM_sCylinderInit);
    MM_Collider_UpdateCylinder(thisx, &this->collider);
}

void MM_EnKusa_Init(Actor* thisx, PlayState* play) {
    EnKusa* this = (EnKusa*)thisx;
    s32 pad;
    s32 kusaType = KUSA_GET_TYPE(&this->actor);

    MM_Actor_ProcessInitChain(&this->actor, MM_sInitChain);

    if (play->csCtx.state != CS_STATE_IDLE) {
        this->actor.cullingVolumeDistance += 1000.0f;
    }
    MM_EnKusa_InitCollider(&this->actor, play);
    MM_CollisionCheck_SetInfo(&this->actor.colChkInfo, NULL, &MM_sColChkInfoInit);

    if (kusaType == ENKUSA_TYPE_BUSH) {
        this->actor.shape.shadowScale = 1.0f;
        this->actor.shape.shadowAlpha = 60;
    } else {
        this->actor.shape.shadowScale = 0.9f;
        this->actor.shape.shadowAlpha = 70;
    }

    if (this->actor.shape.rot.y == 0) {
        this->actor.shape.rot.y = (MM_Rand_Next() >> 0x10);
        this->actor.home.rot.y = this->actor.shape.rot.y;
        this->actor.world.rot.y = this->actor.shape.rot.y;
    }
    if (!MM_EnKusa_SnapToFloor(this, play, 0.0f)) {
        MM_Actor_Kill(&this->actor);
        return;
    }
    if (EnKusa_IsUnderwater(this, play)) {
        this->isInWater |= 1;
    }

    this->objectSlot = Object_GetSlot(&play->objectCtx, MM_sObjectIds[(KUSA_GET_TYPE(&this->actor))]);
    if (this->objectSlot <= OBJECT_SLOT_NONE) {
        MM_Actor_Kill(&this->actor);
        return;
    }

    MM_EnKusa_SetupWaitObject(this);
    if (D_809366B4) {
        D_80936CD8 = MM_Rand_Next() >> 0x10;
        D_80936CDA = MM_Rand_Next() >> 0x10;
        D_80936CDC = MM_Rand_Next() >> 0x10;
        D_80936CDE = MM_Rand_Next() >> 0x10;
        D_80936CE0 = MM_Rand_Next() >> 0x10;
        D_809366B4 = false;
        EnKusa_Sway();
        kusaGameplayFrames = play->gameplayFrames;
    }
    this->kusaMtxIdx = D_809366B0 & 7;
    D_809366B0++;
}

void MM_EnKusa_Destroy(Actor* thisx, PlayState* play) {
    PlayState* play2 = play;
    EnKusa* this = (EnKusa*)thisx;

    MM_Collider_DestroyCylinder(play, &this->collider);
}

void MM_EnKusa_SetupWaitObject(EnKusa* this) {
    this->actionFunc = MM_EnKusa_WaitObject;
}

void MM_EnKusa_WaitObject(EnKusa* this, PlayState* play) {
    s32 pad;

    if (MM_Object_IsLoaded(&play->objectCtx, this->objectSlot)) {
        s32 kusaType = KUSA_GET_TYPE(&this->actor);

        if (this->isCut) {
            MM_EnKusa_SetupCut(this);
        } else {
            EnKusa_SetupInteract(this);
        }
        if (kusaType == ENKUSA_TYPE_BUSH) {
            if (GameInteractor_Should(VB_KUSA_BUSH_DRAW_BE_OVERRIDDEN, true, this)) {
                this->actor.draw = EnKusa_DrawBush;
            }
        } else {
            this->actor.draw = EnKusa_DrawGrass;
        }
        this->actor.objectSlot = this->objectSlot;
        this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    }
}

void EnKusa_SetupInteract(EnKusa* this) {
    this->actionFunc = EnKusa_WaitForInteract;
    this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
}

void EnKusa_WaitForInteract(EnKusa* this, PlayState* play) {
    s32 pad;

    if (MM_Actor_HasParent(&this->actor, play)) {
        MM_EnKusa_SetupLiftedUp(this);
        MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_PL_PULL_UP_PLANT);
        this->actor.shape.shadowDraw = MM_ActorShadow_DrawCircle;

    } else if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        MM_EnKusa_SpawnFragments(this, play);
        MM_EnKusa_DropCollectible(this, play);
        MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_PLANT_BROKEN);

        if (KUSA_SHOULD_SPAWN_BUGS(&this->actor)) {
            if (KUSA_GET_TYPE(&this->actor) != ENKUSA_TYPE_GRASS_2) {
                MM_EnKusa_SpawnBugs(this, play);
            }
        }
        if (KUSA_GET_TYPE(&this->actor) == ENKUSA_TYPE_BUSH) {
            MM_Actor_Kill(&this->actor);
            return;
        }

        MM_EnKusa_SetupCut(this);
        this->isCut = true;
    } else {
        if (!(this->collider.base.ocFlags1 & OC1_TYPE_PLAYER) && (this->actor.xzDistToPlayer > 12.0f)) {
            this->collider.base.ocFlags1 |= OC1_TYPE_PLAYER;
        }

        if (this->actor.xzDistToPlayer < 600.0f) {
            MM_Collider_UpdateCylinder(&this->actor, &this->collider);
            MM_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);

            if (this->actor.xzDistToPlayer < 400.0f) {
                MM_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
                if (this->actor.xzDistToPlayer < 100.0f) {
                    if (KUSA_GET_TYPE(&this->actor) != ENKUSA_TYPE_GRASS_2) {
                        MM_Actor_OfferCarry(&this->actor, play);
                    }
                }
            }
        }
    }
}

void MM_EnKusa_SetupLiftedUp(EnKusa* this) {
    this->actionFunc = MM_EnKusa_LiftedUp;
    this->actor.room = -1;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
}

void MM_EnKusa_LiftedUp(EnKusa* this, PlayState* play) {
    s32 pad;
    Vec3f pos;
    s32 bgId;

    if (MM_Actor_HasNoParent(&this->actor, play)) {
        this->actor.room = play->roomCtx.curRoom.num;
        MM_EnKusa_SetupFall(this);
        this->actor.velocity.x = this->actor.speed * MM_Math_SinS(this->actor.world.rot.y);
        this->actor.velocity.z = this->actor.speed * MM_Math_CosS(this->actor.world.rot.y);
        this->actor.colChkInfo.mass = 80;
        this->actor.gravity = -0.1f;
        MM_EnKusa_UpdateVelY(this);
        MM_EnKusa_RandScaleVecToZero(&this->actor.velocity, 0.005f);
        MM_Actor_UpdatePos(&this->actor);
        MM_Actor_UpdateBgCheckInfo(play, &this->actor, 7.5f, 35.0f, 0.0f,
                                UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_40 |
                                    UPDBGCHECKINFO_FLAG_80);
        this->actor.gravity = -3.2f;
    } else {
        pos.x = this->actor.world.pos.x;
        pos.y = this->actor.world.pos.y + 20.0f;
        pos.z = this->actor.world.pos.z;
        this->actor.floorHeight =
            MM_BgCheck_EntityRaycastFloor5(&play->colCtx, &this->actor.floorPoly, &bgId, &this->actor, &pos);
    }
}

void MM_EnKusa_SetupFall(EnKusa* this) {
    this->actionFunc = MM_EnKusa_Fall;
    MM_rotSpeedXtarget = -0xBB8;
    MM_rotSpeedYtarget = (MM_Rand_ZeroOne() - 0.5f) * 1600.0f;
    MM_rotSpeedX = 0;
    MM_rotSpeedY = 0;
    this->timer = 0;
}

void MM_EnKusa_Fall(EnKusa* this, PlayState* play) {
    s32 pad;
    s32 wasHit;
    Vec3f contactPos;
    s32 i;
    s16 angleOffset;

    wasHit = (this->collider.base.atFlags & AT_HIT) != 0;

    if (wasHit) {
        this->collider.base.atFlags &= ~AT_HIT;
    }
    this->timer++;
    if ((this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH | BGCHECKFLAG_WALL)) || wasHit ||
        (this->timer >= 100)) {
        if (!(this->actor.bgCheckFlags & BGCHECKFLAG_WATER)) {
            MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_PLANT_BROKEN);
        }
        MM_EnKusa_SpawnFragments(this, play);
        MM_EnKusa_DropCollectible(this, play);
        switch (KUSA_GET_TYPE(&this->actor)) {
            case ENKUSA_TYPE_BUSH:
            case ENKUSA_TYPE_GRASS:
                MM_Actor_Kill(&this->actor);
                break;

            case ENKUSA_TYPE_REGROWING_GRASS:
                MM_EnKusa_SetupUprootedWaitRegrow(this);
                this->actor.shape.shadowDraw = NULL;
                break;

            default:
                break;
        }

    } else {
        if (this->actor.bgCheckFlags & BGCHECKFLAG_WATER_TOUCH) {
            contactPos.y = this->actor.world.pos.y + this->actor.depthInWater;
            for (angleOffset = 0, i = 0; i < 4; i++, angleOffset += 0x4000) {
                contactPos.x =
                    this->actor.world.pos.x + (MM_Math_SinS((s32)(MM_Rand_ZeroOne() * 7200.0f) + angleOffset) * 15.0f);
                contactPos.z =
                    this->actor.world.pos.z + (MM_Math_CosS((s32)(MM_Rand_ZeroOne() * 7200.0f) + angleOffset) * 15.0f);
                MM_EffectSsGSplash_Spawn(play, &contactPos, NULL, NULL, 0, 190);
            }
            contactPos.x = this->actor.world.pos.x;
            contactPos.z = this->actor.world.pos.z;
            MM_EffectSsGSplash_Spawn(play, &contactPos, NULL, NULL, 0, 280);
            MM_EffectSsGRipple_Spawn(play, &contactPos, 300, 700, 0);
            this->actor.terminalVelocity = -3.0f;
            this->actor.velocity.x *= 0.1f;
            this->actor.velocity.y *= 0.4f;
            this->actor.velocity.z *= 0.1f;
            this->actor.gravity *= 0.5f;
            MM_rotSpeedX >>= 1;
            MM_rotSpeedXtarget >>= 1;
            MM_rotSpeedY >>= 1;
            MM_rotSpeedYtarget >>= 1;
            this->actor.bgCheckFlags &= ~BGCHECKFLAG_WATER_TOUCH;
            MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_DIVE_INTO_WATER_L);
        }
        MM_EnKusa_UpdateVelY(this);
        MM_Math_StepToS(&MM_rotSpeedX, MM_rotSpeedXtarget, 500);
        MM_Math_StepToS(&MM_rotSpeedY, MM_rotSpeedYtarget, 170);
        this->actor.shape.rot.x += MM_rotSpeedX;
        this->actor.shape.rot.y += MM_rotSpeedY;
        MM_EnKusa_RandScaleVecToZero(&this->actor.velocity, 0.05f);
        MM_Actor_UpdatePos(&this->actor);
        MM_Actor_UpdateBgCheckInfo(play, &this->actor, 7.5f, 35.0f, 0.0f,
                                UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_40 |
                                    UPDBGCHECKINFO_FLAG_80);
        MM_Collider_UpdateCylinder(&this->actor, &this->collider);
        MM_CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        MM_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
}

void MM_EnKusa_SetupCut(EnKusa* this) {
    switch (KUSA_GET_TYPE(&this->actor)) {
        case ENKUSA_TYPE_GRASS:
        case ENKUSA_TYPE_GRASS_2:
            this->actionFunc = MM_EnKusa_DoNothing;
            break;

        case ENKUSA_TYPE_REGROWING_GRASS:
            this->actionFunc = MM_EnKusa_CutWaitRegrow;
            break;

        default:
            break;
    }
    this->timer = 0;
}

void MM_EnKusa_CutWaitRegrow(EnKusa* this, PlayState* play) {
    this->timer++;
    if (this->timer >= 120) {
        MM_EnKusa_SetupRegrow(this);
    }
}

void MM_EnKusa_DoNothing(EnKusa* this, PlayState* play) {
}

void MM_EnKusa_SetupUprootedWaitRegrow(EnKusa* this) {
    this->actor.world.pos.x = this->actor.home.pos.x;
    this->actor.world.pos.y = this->actor.home.pos.y - 9.0f;
    this->actor.world.pos.z = this->actor.home.pos.z;
    MM_EnKusa_SetScaleSmall(this);
    this->timer = 0;
    this->actor.shape.rot = this->actor.home.rot;
    this->actionFunc = MM_EnKusa_UprootedWaitRegrow;
}

void MM_EnKusa_UprootedWaitRegrow(EnKusa* this, PlayState* play) {
    this->timer++;
    if (this->timer > 120) {
        if (MM_Math_StepToF(&this->actor.world.pos.y, this->actor.home.pos.y, 0.6f) && (this->timer >= 170)) {
            MM_EnKusa_SetupRegrow(this);
        }
    }
}

void MM_EnKusa_SetupRegrow(EnKusa* this) {
    this->actionFunc = MM_EnKusa_Regrow;
    MM_EnKusa_SetScaleSmall(this);
    this->isCut = false;
    this->actor.shape.rot = this->actor.home.rot;
}

void MM_EnKusa_Regrow(EnKusa* this, PlayState* play) {
    s32 isFullyGrown = 1;

    isFullyGrown &= MM_Math_StepToF(&this->actor.scale.y, 0.4f, 0.014f);
    isFullyGrown &= MM_Math_StepToF(&this->actor.scale.x, 0.4f, 0.011f);
    this->actor.scale.z = this->actor.scale.x;
    if (isFullyGrown) {
        MM_Actor_SetScale(&this->actor, 0.4f);
        EnKusa_SetupInteract(this);
        this->collider.base.ocFlags1 &= ~OC1_TYPE_PLAYER;
    }
}

void MM_EnKusa_Update(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    EnKusa* this = (EnKusa*)thisx;

    this->actionFunc(this, play);

    if (this->isCut) {
        this->actor.shape.yOffset = -6.25f;
    } else {
        this->actor.shape.yOffset = 0.0f;
    }
    if ((kusaGameplayFrames != play->gameplayFrames) && (play->roomCtx.curRoom.type == ROOM_TYPE_NORMAL)) {
        EnKusa_Sway();
        kusaGameplayFrames = play->gameplayFrames;
    }
}

void EnKusa_DrawBush(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    EnKusa* this = (EnKusa*)thisx;

    Ship_ExtendedCullingActorAdjustProjectedZ(&this->actor);

    if ((this->actor.projectedPos.z <= 1200.0f) || ((this->isInWater & 1) && (this->actor.projectedPos.z < 1300.0f))) {

        if ((play->roomCtx.curRoom.type == ROOM_TYPE_NORMAL) && (this->actionFunc == EnKusa_WaitForInteract) &&
            (this->actor.projectedPos.z > -150.0f) && (this->actor.projectedPos.z < 400.0f)) {
            EnKusa_ApplySway(&D_80936AD8[this->kusaMtxIdx]);
        }

        MM_Gfx_DrawDListOpa(play, gKusaBushType1DL);

    } else if (this->actor.projectedPos.z < 1300.0f) {
        s32 alpha;

        OPEN_DISPS(play->state.gfxCtx);

        alpha = (1300.0f - this->actor.projectedPos.z) * 2.55f;
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);

        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, alpha);
        MM_gSPDisplayList(POLY_XLU_DISP++, gKusaBushType2DL);

        CLOSE_DISPS(play->state.gfxCtx);
    }

    Ship_ExtendedCullingActorRestoreProjectedPos(play, &this->actor);
}

void EnKusa_DrawGrass(Actor* thisx, PlayState* play) {
    EnKusa* this = (EnKusa*)thisx;

    if (this->isCut) {
        MM_Gfx_DrawDListOpa(play, gKusaStumpDL);
    } else {
        if ((play->roomCtx.curRoom.type == ROOM_TYPE_NORMAL) && (this->actionFunc == EnKusa_WaitForInteract)) {
            if ((this->actor.projectedPos.z > -150.0f) && (this->actor.projectedPos.z < 400.0f)) {
                EnKusa_ApplySway(&D_80936AD8[this->kusaMtxIdx]);
            }
        }
        MM_Gfx_DrawDListOpa(play, gKusaSproutDL);
    }
}
