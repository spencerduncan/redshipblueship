/*
 * File: z_en_elfbub.c
 * Overlay: ovl_En_Elfbub
 * Description: Stray fairy in bubble
 */

#include "z_en_elfbub.h"
#include "overlays/actors/ovl_En_Elforg/z_en_elforg.h"
#include "objects/object_bubble/object_bubble.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED)

void EnElfbub_Init(Actor* thisx, PlayState* play);
void EnElfbub_Destroy(Actor* thisx, PlayState* play);
void EnElfbub_Update(Actor* thisx, PlayState* play);
void EnElfbub_Draw(Actor* thisx, PlayState* play2);

void EnElfbub_Pop(EnElfbub* this, PlayState* play);
void EnElfbub_Idle(EnElfbub* this, PlayState* play);

ActorProfile En_Elfbub_Profile = {
    /**/ ACTOR_EN_ELFBUB,
    /**/ ACTORCAT_MISC,
    /**/ FLAGS,
    /**/ OBJECT_BUBBLE,
    /**/ sizeof(EnElfbub),
    /**/ EnElfbub_Init,
    /**/ EnElfbub_Destroy,
    /**/ EnElfbub_Update,
    /**/ EnElfbub_Draw,
};

static ColliderCylinderInit MM_sCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_NONE | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 16, 32, 0, { 0, 0, 0 } },
};

void EnElfbub_Init(Actor* thisx, PlayState* play) {
    EnElfbub* this = (EnElfbub*)thisx;
    Actor* childActor;

    if (MM_Flags_GetSwitch(play, ENELFBUB_GET_SWITCH_FLAG(&this->actor))) {
        MM_Actor_Kill(&this->actor);
        return;
    }

    MM_ActorShape_Init(&this->actor.shape, 16.0f, MM_ActorShadow_DrawCircle, 0.2f);
    //! @bug: hint Id not correctly migrated from OoT `NAVI_ENEMY_SHABOM`
    this->actor.hintId = TATL_HINT_ID_IGOS_DU_IKANA;
    MM_Actor_SetScale(&this->actor, 1.25f);

    this->actionFunc = EnElfbub_Idle;
    this->zRot = MM_Rand_CenteredFloat(0x10000);
    this->zRotDelta = 1000;
    this->xScale = 0.08f;

    Collider_InitAndSetCylinder(play, &this->collider, &this->actor, &MM_sCylinderInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;

    childActor = MM_Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_ELFORG, this->actor.world.pos.x,
                                    this->actor.world.pos.y + 12.0f, this->actor.world.pos.z, this->actor.world.rot.x,
                                    this->actor.world.rot.y, this->actor.world.rot.z,
                                    STRAY_FAIRY_PARAMS(ENELFBUB_GET_SWITCH_FLAG(&this->actor),
                                                       STRAY_FAIRY_AREA_CLOCK_TOWN, STRAY_FAIRY_TYPE_BUBBLE));
    if (childActor != NULL) {
        childActor->parent = &this->actor;
    }

    this->oscillationAngle = 0;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
}

void EnElfbub_Destroy(Actor* thisx, PlayState* play) {
    EnElfbub* this = (EnElfbub*)thisx;
    MM_Collider_DestroyCylinder(play, &this->collider);
}

void EnElfbub_Pop(EnElfbub* this, PlayState* play) {
    static Color_RGBA8 sPrimColor = { 255, 255, 255, 255 };
    static Color_RGBA8 sEnvColor = { 150, 150, 150, 0 };
    static Vec3f MM_sAccel = { 0.0f, -0.5f, 0.0f };
    s32 effectCounter;
    Vec3f velocity;
    Vec3f pos;

    MM_Math_SmoothStepToF(&this->xyScale, 3.0f, 0.1f, 1000.0f, 0.0f);
    MM_Math_SmoothStepToF(&this->xScale, 0.2f, 0.1f, 1000.0f, 0.0f);
    this->zRotDelta += 1000;
    this->zRot += this->zRotDelta;
    this->popTimer--;
    if (this->popTimer <= 0) {
        pos.x = this->actor.world.pos.x;
        pos.y = this->actor.world.pos.y;
        pos.z = this->actor.world.pos.z;

        for (effectCounter = 0; effectCounter < 20; effectCounter++) {
            velocity.x = (MM_Rand_ZeroOne() - 0.5f) * 7.0f;
            velocity.y = MM_Rand_ZeroOne() * 7.0f;
            velocity.z = (MM_Rand_ZeroOne() - 0.5f) * 7.0f;
            MM_EffectSsDtBubble_SpawnCustomColor(play, &pos, &velocity, &MM_sAccel, &sPrimColor, &sEnvColor,
                                              MM_Rand_S16Offset(100, 50), 25, false);
        }

        MM_SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 60, NA_SE_EN_AWA_BREAK);
        MM_Actor_Kill(&this->actor);
    }
}

void EnElfbub_Idle(EnElfbub* this, PlayState* play) {
    s32 pad;

    this->zRot += this->zRotDelta;
    this->actor.world.pos.y += MM_Math_SinS(this->oscillationAngle);
    this->oscillationAngle += 0x200;

    if (this->collider.base.acFlags & AC_HIT || this->collider.base.ocFlags1 & OC1_HIT) {
        this->actionFunc = EnElfbub_Pop;
        this->popTimer = 6;
        return;
    }

    MM_CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    MM_CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

void EnElfbub_Update(Actor* thisx, PlayState* play) {
    EnElfbub* this = (EnElfbub*)thisx;

    MM_Collider_UpdateCylinder(&this->actor, &this->collider);
    this->actionFunc(this, play);
    MM_Actor_SetFocus(&this->actor, this->actor.shape.yOffset);
}

void EnElfbub_Draw(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    EnElfbub* this = (EnElfbub*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);

    MM_Matrix_Translate(0.0f, 0.0f, 1.0f, MTXMODE_APPLY);
    MM_Matrix_ReplaceRotation(&play->billboardMtxF);
    MM_Matrix_Scale(this->xyScale + 1.0f, this->xyScale + 1.0f, 1.0f, MTXMODE_APPLY);
    Matrix_RotateZS(this->zRot, MTXMODE_APPLY);
    MM_Matrix_Scale(this->xScale + 1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
    Matrix_RotateZS(this->zRot * -1, MTXMODE_APPLY);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, gBubbleDL);

    CLOSE_DISPS(play->state.gfxCtx);
}
