/*
 * File: z_eff_ss_hahen.c
 * Overlay: ovl_Effect_Ss_Hahen
 * Description: Fragments
 */

#include "z_eff_ss_hahen.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define rPitch regs[0]
#define rYaw regs[1]
#define rFlags regs[2]
#define rScale regs[3]
#define rObjectId regs[4]
#define rObjectSlot regs[5]
#define rMinLife regs[6]

#define PARAMS ((EffectSsHahenInitParams*)initParamsx)

u32 MM_EffectSsHahen_Init(PlayState* play, u32 index, EffectSs* this, void* initParamsx);
void MM_EffectSsHahen_Update(PlayState* play, u32 index, EffectSs* this);
void MM_EffectSsHahen_Draw(PlayState* play, u32 index, EffectSs* this);

EffectSsProfile Effect_Ss_Hahen_Profile = {
    EFFECT_SS_HAHEN,
    MM_EffectSsHahen_Init,
};

void MM_EffectSsHahen_CheckForObject(EffectSs* this, PlayState* play) {
    if (((this->rObjectSlot = Object_GetSlot(&play->objectCtx, this->rObjectId)) <= OBJECT_SLOT_NONE) ||
        !MM_Object_IsLoaded(&play->objectCtx, this->rObjectSlot)) {
        this->life = -1;
        this->draw = NULL;
    }
}

u32 MM_EffectSsHahen_Init(PlayState* play, u32 index, EffectSs* this, void* initParamsx) {
    EffectSsHahenInitParams* initParams = PARAMS;

    this->pos = initParams->pos;
    this->velocity = initParams->velocity;
    this->accel = initParams->accel;
    this->life = 200;

    if (initParams->dList != NULL) {
        this->gfx = initParams->dList;
        this->rObjectId = initParams->objectId;
        MM_EffectSsHahen_CheckForObject(this, play);
    } else {
        this->gfx = gEffFragments1DL;
        this->rObjectId = HAHEN_OBJECT_DEFAULT;
    }

    this->draw = MM_EffectSsHahen_Draw;
    this->update = MM_EffectSsHahen_Update;
    this->rFlags = initParams->flags;
    this->rScale = initParams->scale;
    this->rPitch = MM_Rand_ZeroOne() * 314.0f;
    this->rYaw = MM_Rand_ZeroOne() * 314.0f;
    this->rMinLife = 200 - initParams->life;

    return 1;
}

void EffectSsHahen_DrawOpa(PlayState* play, EffectSs* this) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 pad;

    OPEN_DISPS(gfxCtx);

    if (this->rObjectId != HAHEN_OBJECT_DEFAULT) {
        gSPSegment(POLY_OPA_DISP++, 0x06, play->objectCtx.slots[this->rObjectSlot].segment);
    }
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, this->gfx);

    CLOSE_DISPS(gfxCtx);
}

void EffectSsHahen_DrawXlu(PlayState* play, EffectSs* this) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 pad;

    OPEN_DISPS(gfxCtx);

    if (this->rObjectId != -1) {
        gSPSegment(POLY_XLU_DISP++, 0x06, play->objectCtx.slots[this->rObjectSlot].segment);
    }
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, this->gfx);

    CLOSE_DISPS(gfxCtx);
}

void MM_EffectSsHahen_Draw(PlayState* play, u32 index, EffectSs* this) {
    f32 scale;

    if (this->rFlags & HAHEN_SMALL) {
        scale = this->rScale * (0.001f * 0.1f);
    } else {
        scale = this->rScale * 0.001f;
    }

    MM_Matrix_Translate(this->pos.x, this->pos.y, this->pos.z, MTXMODE_NEW);
    Matrix_RotateYF(this->rYaw * 0.01f, MTXMODE_APPLY);
    Matrix_RotateXFApply(this->rPitch * 0.01f);
    MM_Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);

    if (this->rFlags & HAHEN_XLU) {
        EffectSsHahen_DrawXlu(play, this);
    } else {
        EffectSsHahen_DrawOpa(play, this);
    }
}

void MM_EffectSsHahen_Update(PlayState* play, u32 index, EffectSs* this) {
    Player* player = GET_PLAYER(play);

    this->rPitch += 0x37;
    this->rYaw += 0xA;

    if ((this->pos.y <= player->actor.floorHeight) && (this->life < this->rMinLife)) {
        this->life = 0;
    }

    if (this->rObjectId != HAHEN_OBJECT_DEFAULT) {
        MM_EffectSsHahen_CheckForObject(this, play);
    }
}
