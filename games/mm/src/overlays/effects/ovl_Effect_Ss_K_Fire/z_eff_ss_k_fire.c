/*
 * File: z_eff_ss_k_fire.c
 * Overlay: ovl_Effect_Ss_K_Fire
 * Description:
 */

#include "z_eff_ss_k_fire.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define rAlpha regs[0]
#define rScroll regs[2]
#define rType regs[3]
#define rYScale regs[4]
#define rXZScale regs[5]
#define rScaleMax regs[6]

#define PARAMS ((EffectSsKFireInitParams*)initParamsx)

u32 MM_EffectSsKFire_Init(PlayState* play, u32 index, EffectSs* this, void* initParamsx);
void MM_EffectSsKFire_Update(PlayState* play, u32 index, EffectSs* this);
void MM_EffectSsKFire_Draw(PlayState* play, u32 index, EffectSs* this);

EffectSsProfile Effect_Ss_K_Fire_Profile = {
    EFFECT_SS_K_FIRE,
    MM_EffectSsKFire_Init,
};

u32 MM_EffectSsKFire_Init(PlayState* play, u32 index, EffectSs* this, void* initParamsx) {
    EffectSsKFireInitParams* initParams = PARAMS;

    this->pos = initParams->pos;
    this->velocity = initParams->velocity;
    this->accel = initParams->accel;
    this->life = 100;
    this->rScaleMax = initParams->scaleMax;
    this->rAlpha = 255;
    this->rScroll = MM_Rand_ZeroFloat(5.0f) - 25;
    this->rType = initParams->type;
    this->draw = MM_EffectSsKFire_Draw;
    this->update = MM_EffectSsKFire_Update;

    return 1;
}

void MM_EffectSsKFire_Draw(PlayState* play, u32 index, EffectSs* this) {
    s32 pad;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    f32 xzScale;
    f32 yScale;

    xzScale = this->rXZScale / 10000.0f;
    yScale = this->rYScale / 10000.0f;

    OPEN_DISPS(gfxCtx);

    MM_Matrix_Translate(this->pos.x, this->pos.y, this->pos.z, MTXMODE_NEW);
    MM_Matrix_Scale(xzScale, yScale, xzScale, MTXMODE_APPLY);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MM_gSPSegment(POLY_XLU_DISP++, 0x08,
               MM_Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0, play->state.frames * this->rScroll, 0x20,
                                0x80));

    if (this->rType >= 100) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 0, this->rAlpha);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 10, 0, 0);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 255, this->rAlpha);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 255, 255, 0);
    }

    gDPPipeSync(POLY_XLU_DISP++);
    MM_Matrix_ReplaceRotation(&play->billboardMtxF);

    if ((index % 2) != 0) {
        Matrix_RotateYF(M_PIf, MTXMODE_APPLY);
    }

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    MM_gSPDisplayList(POLY_XLU_DISP++, gEffFire1DL);

    CLOSE_DISPS(gfxCtx);
}

void MM_EffectSsKFire_Update(PlayState* play, u32 index, EffectSs* this) {
    if (this->rXZScale < this->rScaleMax) {
        this->rXZScale += 4;
        this->rYScale += 4;

        if (this->rXZScale > this->rScaleMax) {
            this->rXZScale = this->rScaleMax;

            if (this->rType != 3) {
                this->rYScale = this->rScaleMax;
            }
        }
    } else if (this->rAlpha > 0) {
        this->rAlpha -= 10;
        if (this->rAlpha <= 0) {
            this->rAlpha = 0;
            this->life = 0;
        }
    }

    if (this->rType == 3) {
        this->rYScale++;
    }
}
