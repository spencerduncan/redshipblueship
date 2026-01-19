/*
 * File: z_eff_ss_g_ripple.c
 * Overlay: ovl_Effect_Ss_G_Ripple
 * Description: Water Ripple
 */

#include "z_eff_ss_g_ripple.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define rWaterBoxNum regs[0]
#define rRadius regs[1]
#define rRadiusMax regs[2]
#define rPrimColorR regs[3]
#define rPrimColorG regs[4]
#define rPrimColorB regs[5]
#define rPrimColorA regs[6]
#define rEnvColorR regs[7]
#define rEnvColorG regs[8]
#define rEnvColorB regs[9]
#define rEnvColorA regs[10]
#define rLifespan regs[11]
#define rBgId regs[12]

#define PARAMS ((EffectSsGRippleInitParams*)initParamsx)

u32 MM_EffectSsGRipple_Init(PlayState* play, u32 index, EffectSs* this, void* initParamsx);
void MM_EffectSsGRipple_Update(PlayState* play, u32 index, EffectSs* this);
void MM_EffectSsGRipple_Draw(PlayState* play, u32 index, EffectSs* this);

EffectSsProfile Effect_Ss_G_Ripple_Profile = {
    EFFECT_SS_G_RIPPLE,
    MM_EffectSsGRipple_Init,
};

u32 MM_EffectSsGRipple_Init(PlayState* play, u32 index, EffectSs* this, void* initParamsx) {
    EffectSsGRippleInitParams* initParams = PARAMS;
    WaterBox* waterBox = NULL;
    s32 pad;
    s32 bgId;

    MM_Math_Vec3f_Copy(&this->velocity, &gZeroVec3f);
    MM_Math_Vec3f_Copy(&this->accel, &gZeroVec3f);
    MM_Math_Vec3f_Copy(&this->pos, &initParams->pos);
    this->gfx = gEffWaterRippleDL;
    this->life = initParams->life + 20;
    this->flags = 0;
    this->draw = MM_EffectSsGRipple_Draw;
    this->update = MM_EffectSsGRipple_Update;
    this->rRadius = initParams->radius;
    this->rRadiusMax = initParams->radiusMax;
    this->rLifespan = initParams->life;
    this->rPrimColorR = 255;
    this->rPrimColorG = 255;
    this->rPrimColorB = 255;
    this->rPrimColorA = 255;
    this->rEnvColorR = 255;
    this->rEnvColorG = 255;
    this->rEnvColorB = 255;
    this->rEnvColorA = 255;
    this->rWaterBoxNum = MM_WaterBox_GetSurface2(play, &play->colCtx, &initParams->pos, 3.0f, &waterBox, &bgId);
    this->rBgId = bgId;

    return 1;
}

void MM_EffectSsGRipple_DrawRipple(PlayState* play2, EffectSs* this, TexturePtr tex) {
    PlayState* play = play2;
    f32 radius;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    MtxF mfTrans;
    MtxF mfScale;
    MtxF mfResult;
    Mtx* mtx;
    f32 yPos;
    CollisionHeader* colHeader;

    OPEN_DISPS(gfxCtx);

    radius = this->rRadius * 0.0025f;
    colHeader = MM_BgCheck_GetCollisionHeader(&play->colCtx, this->rBgId);

    if ((this->rWaterBoxNum != -1) && (colHeader != NULL) && (this->rWaterBoxNum < colHeader->numWaterBoxes)) {
        yPos = func_800CA568(&play->colCtx, this->rWaterBoxNum, this->rBgId);
    } else {
        yPos = this->pos.y;
    }

    MM_SkinMatrix_SetTranslate(&mfTrans, this->pos.x, yPos, this->pos.z);
    MM_SkinMatrix_SetScale(&mfScale, radius, radius, radius);
    MM_SkinMatrix_MtxFMtxFMult(&mfTrans, &mfScale, &mfResult);

    mtx = MM_SkinMatrix_MtxFToNewMtx(gfxCtx, &mfResult);

    if (mtx != NULL) {
        gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        Gfx_SetupDL60_XluNoCD(gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, this->rPrimColorR, this->rPrimColorG, this->rPrimColorB,
                        this->rPrimColorA);
        gDPSetEnvColor(POLY_XLU_DISP++, this->rEnvColorR, this->rEnvColorG, this->rEnvColorB, this->rEnvColorA);
        gDPSetAlphaDither(POLY_XLU_DISP++, G_AD_NOISE);
        gDPSetColorDither(POLY_XLU_DISP++, G_CD_NOISE);
        gSPDisplayList(POLY_XLU_DISP++, this->gfx);
    }

    CLOSE_DISPS(gfxCtx);
}

void MM_EffectSsGRipple_Draw(PlayState* play, u32 index, EffectSs* this) {
    if (this->rLifespan == 0) {
        MM_EffectSsGRipple_DrawRipple(play, this, gEffWaterRippleTex);
    }
}

void MM_EffectSsGRipple_Update(PlayState* play, u32 index, EffectSs* this) {
    f32 radius;
    f32 primAlpha;
    f32 envAlpha;
    WaterBox* waterBox;
    s32 bgId;

    this->rWaterBoxNum = MM_WaterBox_GetSurface2(play, &play->colCtx, &this->pos, 3.0f, &waterBox, &bgId);
    this->rBgId = bgId;

    if (DECR(this->rLifespan) == 0) {
        radius = this->rRadius;
        MM_Math_SmoothStepToF(&radius, this->rRadiusMax, 0.2f, 30.0f, 1.0f);
        this->rRadius = radius;

        primAlpha = this->rPrimColorA;
        envAlpha = this->rEnvColorA;

        MM_Math_SmoothStepToF(&primAlpha, 0.0f, 0.2f, 15.0f, 7.0f);
        MM_Math_SmoothStepToF(&envAlpha, 0.0f, 0.2f, 15.0f, 7.0f);

        this->rPrimColorA = primAlpha;
        this->rEnvColorA = envAlpha;
    }
}
