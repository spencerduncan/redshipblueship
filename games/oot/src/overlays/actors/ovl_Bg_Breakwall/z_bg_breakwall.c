/*
 * File: z_bg_breakwall.c
 * Overlay: Bg_Breakwall
 * Description: Bombable Wall
 */

#include "z_bg_breakwall.h"
#include "scenes/dungeons/ddan/ddan_scene.h"
#include "objects/object_bwall/object_bwall.h"
#include "objects/object_kingdodongo/object_kingdodongo.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED

typedef struct {
    /* 0x00 */ CollisionHeader* colHeader;
    /* 0x04 */ Gfx* dList;
    /* 0x08 */ s8 colType;
} BombableWallInfo;

void OoT_BgBreakwall_Init(Actor* thisx, PlayState* play);
void OoT_BgBreakwall_Destroy(Actor* thisx, PlayState* play);
void OoT_BgBreakwall_Update(Actor* thisx, PlayState* play);
void OoT_BgBreakwall_Draw(Actor* thisx, PlayState* play);

void BgBreakwall_WaitForObject(BgBreakwall* this, PlayState* play);
void BgBreakwall_Wait(BgBreakwall* this, PlayState* play);
void BgBreakwall_LavaCoverMove(BgBreakwall* this, PlayState* play);

const ActorInit Bg_Breakwall_InitVars = {
    ACTOR_BG_BREAKWALL,
    ACTORCAT_BG,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(BgBreakwall),
    (ActorFunc)OoT_BgBreakwall_Init,
    (ActorFunc)OoT_BgBreakwall_Destroy,
    (ActorFunc)OoT_BgBreakwall_Update,
    NULL,
    NULL,
};

static ColliderQuadInit OoT_sQuadInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER | AC_TYPE_OTHER,
        OC1_NONE,
        OC2_TYPE_2,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000048, 0x00, 0x00 },
        { 0x00000048, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

// Replacement quad used for "Blue Fire Arrows" enhancement
static ColliderQuadInit sIceArrowQuadInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER | AC_TYPE_OTHER,
        OC1_NONE,
        OC2_TYPE_2,
        COLSHAPE_QUAD,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000048, 0x00, 0x00 },
        { 0x00001048, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static BombableWallInfo sBombableWallInfo[] = {
    { &object_bwall_Col_000118, object_bwall_DL_000040, 0 },
    { &object_bwall_Col_000118, object_bwall_DL_000040, 0 },
    { &object_kingdodongo_Col_0264A8, object_kingdodongo_DL_025BD0, 1 },
    { &object_kingdodongo_Col_025B64, NULL, -1 },
};

static InitChainEntry OoT_sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneForward, 4000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 400, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 400, ICHAIN_STOP),
};

bool blueFireArrowsEnabledOnMudwallLoad = false;

void OoT_BgBreakwall_SetupAction(BgBreakwall* this, BgBreakwallActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void OoT_BgBreakwall_Init(Actor* thisx, PlayState* play) {
    BgBreakwall* this = (BgBreakwall*)thisx;
    s32 pad;
    s32 wallType = ((this->dyna.actor.params >> 13) & 3) & 0xFF;

    // Initialize this with the mud wall, so it can't be affected by toggling while the actor is loaded
    blueFireArrowsEnabledOnMudwallLoad = CVarGetInteger(CVAR_ENHANCEMENT("BlueFireArrows"), 0) ||
                                         (IS_RANDO && Randomizer_GetSettingValue(RSK_BLUE_FIRE_ARROWS));

    OoT_Actor_ProcessInitChain(&this->dyna.actor, OoT_sInitChain);
    OoT_DynaPolyActor_Init(&this->dyna, DPM_UNK);
    this->bombableWallDList = sBombableWallInfo[wallType].dList;
    this->colType = sBombableWallInfo[wallType].colType;

    if (this->colType == 1) {
        this->dyna.actor.world.rot.x = 0x4000;
    }

    if (this->bombableWallDList != NULL) {
        if (OoT_Flags_GetSwitch(play, this->dyna.actor.params & 0x3F)) {
            OoT_Actor_Kill(&this->dyna.actor);
            return;
        }

        OoT_ActorShape_Init(&this->dyna.actor.shape, 0.0f, NULL, 0.0f);

        // If "Blue Fire Arrows" are enabled, set up this collider for them
        if (blueFireArrowsEnabledOnMudwallLoad) {
            OoT_Collider_InitQuad(play, &this->collider);
            OoT_Collider_SetQuad(play, &this->collider, &this->dyna.actor, &sIceArrowQuadInit);
        } else {
            OoT_Collider_InitQuad(play, &this->collider);
            OoT_Collider_SetQuad(play, &this->collider, &this->dyna.actor, &OoT_sQuadInit);
        }
    } else {
        this->dyna.actor.world.pos.y -= 40.0f;
    }

    this->bankIndex = (wallType >= BWALL_KD_FLOOR) ? Object_GetIndex(&play->objectCtx, OBJECT_KINGDODONGO)
                                                   : Object_GetIndex(&play->objectCtx, OBJECT_BWALL);

    if (this->bankIndex < 0) {
        OoT_Actor_Kill(&this->dyna.actor);
    } else {
        OoT_BgBreakwall_SetupAction(this, BgBreakwall_WaitForObject);
    }
}

void OoT_BgBreakwall_Destroy(Actor* thisx, PlayState* play) {
    BgBreakwall* this = (BgBreakwall*)thisx;

    OoT_DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

/**
 * Spawns fragments using ACTOR_EN_A_OBJ whenever the wall or floor is exploded.
 * Returns the last spawned actor
 */
Actor* BgBreakwall_SpawnFragments(PlayState* play, BgBreakwall* this, Vec3f* pos, f32 velocity, f32 scaleY, f32 scaleX,
                                  s32 count, f32 accel) {
    Actor* actor;
    Vec3f actorPos;
    s32 k;
    s32 j;
    s32 i;
    s16 angle1;
    s16 angle2 = 0;
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f }; // unused
    Vec3s actorRotList[] = { { 0, 0, 0 }, { 0, 0, 0x4000 }, { 0, 0, -0x4000 }, { 0, 0, 0 } };
    Vec3f actorScaleList[] = {
        { 0.004f, 0.004f, 0.004f },
        { 0.004f, 0.004f, 0.004f },
        { 0.004f, 0.004f, 0.004f },
        { 0.004f, 0.004f, 0.004f },
    };
    Vec3f actorPosList[][4] = {
        { { 40.0f, 15.0f, 0.0f }, { 30.0f, 57.0f, 0.0f }, { 50.0f, 57.0f, 0.0f }, { 40.0f, 70.0f, 0.0f } },
        { { 55.0f, -15.0f, 0.0f }, { 30.0f, -32.0f, 0.0f }, { 50.0f, -32.0f, 0.0f }, { 20.0f, -10.0f, 0.0f } },
        { { -40.0f, 14.0f, 0.0f }, { -50.0f, 57.0f, 0.0f }, { -30.0f, 57.0f, 0.0f }, { -40.0f, 70.0f, 0.0f } },
        { { -55.0f, -15.0f, 0.0f }, { -55.0f, -32.0f, 0.0f }, { -30.0f, -32.0f, 0.0f }, { -20.0f, -10.0f, 0.0f } },
    };
    s32 pad;

    for (k = 3; k >= 0; k--) {
        if ((k == 0) || (k == 3)) {
            actorScaleList[k].x *= scaleX;
            actorScaleList[k].y *= scaleY;
            actorScaleList[k].z *= scaleY;
        } else {
            actorScaleList[k].x *= scaleY;
            actorScaleList[k].y *= scaleX;
            actorScaleList[k].z *= scaleX;
        }
    }

    for (i = 0; i < count; angle2 += 0x4000, i++) {
        angle1 = ABS(this->dyna.actor.world.rot.y) + angle2;
        OoT_Matrix_Translate(this->dyna.actor.world.pos.x, this->dyna.actor.world.pos.y, this->dyna.actor.world.pos.z,
                         MTXMODE_NEW);
        OoT_Matrix_RotateZYX(this->dyna.actor.world.rot.x, this->dyna.actor.world.rot.y, this->dyna.actor.world.rot.z,
                         MTXMODE_APPLY);
        OoT_Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_APPLY);

        for (j = 3; j >= 0; j--) {
            for (k = 3; k >= 0; k--) {
                OoT_Matrix_MultVec3f(&actorPosList[j][k], &actorPos);
                actor = OoT_Actor_Spawn(&play->actorCtx, play, ACTOR_EN_A_OBJ, OoT_Rand_CenteredFloat(20.0f) + actorPos.x,
                                    OoT_Rand_CenteredFloat(20.0f) + actorPos.y, OoT_Rand_CenteredFloat(20.0f) + actorPos.z,
                                    actorRotList[k].x, actorRotList[k].y + angle1, actorRotList[k].z, 0x000B, true);

                if ((j & 1) == 0) {
                    func_80033480(play, &actorPos, velocity * 200.0f, 1, 650, 150, 1);
                }

                if (actor != NULL) {
                    actor->speedXZ = OoT_Rand_ZeroOne() + (accel * 0.6f);
                    actor->velocity.y = OoT_Rand_ZeroOne() + (accel * 0.6f);
                    actor->world.rot.y += (s16)((OoT_Rand_ZeroOne() - 0.5f) * 3000.0f);
                    actor->world.rot.x = (s16)(OoT_Rand_ZeroOne() * 3500.0f) + 2000;
                    actor->world.rot.z = (s16)(OoT_Rand_ZeroOne() * 3500.0f) + 2000;
                    actor->parent = &this->dyna.actor;
                    actor->scale.x = actorScaleList[k].x + OoT_Rand_CenteredFloat(0.001f);
                    actor->scale.y = actorScaleList[k].y + OoT_Rand_CenteredFloat(0.001f);
                    actor->scale.z = actorScaleList[k].z + OoT_Rand_CenteredFloat(0.001f);
                }
            }
        }
    }

    return actor;
}

/**
 * Sets up the collision model as well is the object dependency and action function to use.
 */
void BgBreakwall_WaitForObject(BgBreakwall* this, PlayState* play) {
    if (OoT_Object_IsLoaded(&play->objectCtx, this->bankIndex)) {
        CollisionHeader* colHeader = NULL;
        s32 wallType = ((this->dyna.actor.params >> 13) & 3) & 0xFF;

        this->dyna.actor.objBankIndex = this->bankIndex;
        OoT_Actor_SetObjectDependency(play, &this->dyna.actor);
        this->dyna.actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        this->dyna.actor.draw = OoT_BgBreakwall_Draw;
        OoT_CollisionHeader_GetVirtual(sBombableWallInfo[wallType].colHeader, &colHeader);
        this->dyna.bgId = OoT_DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);

        if (wallType == BWALL_KD_LAVA_COVER) {
            OoT_BgBreakwall_SetupAction(this, BgBreakwall_LavaCoverMove);
        } else {
            OoT_BgBreakwall_SetupAction(this, BgBreakwall_Wait);
        }
    }
}

/**
 * Checks for an explosion using quad collision. If the wall or floor is exploded then it will spawn fragments and
 * despawn itself.
 */
void BgBreakwall_Wait(BgBreakwall* this, PlayState* play) {
    bool blueFireArrowHit = false;
    // If "Blue Fire Arrows" enabled, check this collider for a hit
    if (blueFireArrowsEnabledOnMudwallLoad) {
        if (this->collider.base.acFlags & AC_HIT) {
            if ((this->collider.base.ac != NULL) && (this->collider.base.ac->id == ACTOR_EN_ARROW)) {

                if (this->collider.base.ac->child != NULL && this->collider.base.ac->child->id == ACTOR_ARROW_ICE) {
                    blueFireArrowHit = true;
                }
            }
        }
    }

    if (GameInteractor_Should(VB_BG_BREAKWALL_BREAK, this->collider.base.acFlags & 2 || blueFireArrowHit)) {
        Vec3f effectPos;
        s32 wallType = ((this->dyna.actor.params >> 13) & 3) & 0xFF;

        OoT_DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
        effectPos.y = effectPos.z = effectPos.x = 0.0f;

        if (this->dyna.actor.world.rot.x == 0) {
            effectPos.y = 55.0f;
        } else {
            effectPos.z = 25.0f;
            effectPos.y = -10.0f;
        }

        BgBreakwall_SpawnFragments(play, this, &effectPos, 0.0f, 6.4f, 5.0f, 1, 2.0f);
        OoT_Flags_SetSwitch(play, this->dyna.actor.params & 0x3F);

        if (wallType == BWALL_KD_FLOOR) {
            Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_EXPLOSION);
        } else {
            Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_WALL_BROKEN);
        }

        if ((wallType == BWALL_DC_ENTRANCE) && (!OoT_Flags_GetEventChkInf(EVENTCHKINF_ENTERED_DODONGOS_CAVERN))) {
            OoT_Flags_SetEventChkInf(EVENTCHKINF_ENTERED_DODONGOS_CAVERN);
            if (GameInteractor_Should(VB_PLAY_ENTRANCE_CS, true, EVENTCHKINF_ENTERED_DODONGOS_CAVERN,
                                      gSaveContext.entranceIndex)) {
                Cutscene_SetSegment(play, gDcOpeningCs);
                gSaveContext.cutsceneTrigger = 1;
                OoT_Player_SetCsActionWithHaltedActors(play, NULL, 0x31);
            }
            Audio_PlaySoundGeneral(NA_SE_SY_CORRECT_CHIME, &OoT_gSfxDefaultPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                                   &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
        }

        if (this->dyna.actor.params < 0) {
            Audio_PlaySoundGeneral(NA_SE_SY_TRE_BOX_APPEAR, &OoT_gSfxDefaultPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                                   &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
        }

        OoT_Actor_Kill(&this->dyna.actor);
    }
}

/**
 * Moves the actor's y position to cover the lava floor in King Dodongo's lair after he is defeated so the player is no
 * longer hurt by the lava.
 */
void BgBreakwall_LavaCoverMove(BgBreakwall* this, PlayState* play) {
    OoT_Math_StepToF(&this->dyna.actor.world.pos.y, KREG(80) + this->dyna.actor.home.pos.y, 1.0f);
}

void OoT_BgBreakwall_Update(Actor* thisx, PlayState* play) {
    BgBreakwall* this = (BgBreakwall*)thisx;

    this->actionFunc(this, play);
}

/**
 * These are the quads used for the wall and floor collision. These are used for the detecting when a bomb explosion has
 * collided with a wall, and can be adjusted for different wall or floor sizes.
 */
static Vec3f sColQuadList[][4] = {
    { { 800.0f, 1600.0f, 100.0f }, { -800.0f, 1600.0f, 100.0f }, { 800.0f, 0.0f, 100.0f }, { -800.0f, 0.0f, 100.0f } },
    { { 10.0f, 0.0f, 10.0f }, { -10.0f, 0.0f, 10.0f }, { 10.0f, 0.0f, -10.0f }, { -10.0f, 0.0f, -10.0f } },
};

void OoT_BgBreakwall_Draw(Actor* thisx, PlayState* play) {
    s32 pad;
    BgBreakwall* this = (BgBreakwall*)thisx;

    if (this->bombableWallDList != NULL) {
        OPEN_DISPS(play->state.gfxCtx);

        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, this->bombableWallDList);

        if (this->colType >= 0) {
            Vec3f colQuad[4];
            Vec3f* src = &sColQuadList[this->colType][0];
            Vec3f* dst = &colQuad[0];
            s32 i;

            for (i = 0; i < 4; i++) {
                OoT_Matrix_MultVec3f(src++, dst++);
            }

            OoT_Collider_SetQuadVertices(&this->collider, &colQuad[0], &colQuad[1], &colQuad[2], &colQuad[3]);
            OoT_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }

        CLOSE_DISPS(play->state.gfxCtx);
    }
}
