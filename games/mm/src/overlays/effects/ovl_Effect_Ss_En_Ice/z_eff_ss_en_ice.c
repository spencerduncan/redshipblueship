/*
 * File: z_eff_ss_en_ice.c
 * Overlay: ovl_Effect_Ss_En_Ice
 * Description: Ice clumps
 */

#include "z_eff_ss_en_ice.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define rLifespan regs[0]
#define rYaw regs[1]
#define rPitch regs[2]
#define rRotSpeed regs[3]
#define rPrimColorR regs[4]
#define rPrimColorG regs[5]
#define rPrimColorB regs[6]
#define rPrimColorA regs[7]
#define rEnvColorR regs[8]
#define rEnvColorG regs[9]
#define rEnvColorB regs[10]
#define rAlphaMode regs[11]
#define rScale regs[12]

#define PARAMS ((EffectSsEnIceInitParams*)initParamsx)

u32 MM_EffectSsEnIce_Init(PlayState* play, u32 index, EffectSs* this, void* initParamsx);
void MM_EffectSsEnIce_UpdateFlying(PlayState* play, u32 index, EffectSs* this);
void MM_EffectSsEnIce_Update(PlayState* play, u32 index, EffectSs* this);
void MM_EffectSsEnIce_Draw(PlayState* play, u32 index, EffectSs* this);

EffectSsProfile Effect_Ss_En_Ice_Profile = {
    EFFECT_SS_EN_ICE,
    MM_EffectSsEnIce_Init,
};

u32 MM_EffectSsEnIce_Init(PlayState* play, u32 index, EffectSs* this, void* initParamsx) {
    EffectSsEnIceInitParams* initParams = PARAMS;

    if (initParams->type == ENICE_TYPE_FLYING) {
        MM_Math_Vec3f_Copy(&this->pos, &initParams->pos);

        if (initParams->actor != NULL) {
            MM_Math_Vec3f_Diff(&this->pos, &initParams->actor->world.pos, &this->vec);
        } else {
            MM_Math_Vec3f_Copy(&this->vec, &initParams->pos);
        }

        MM_Math_Vec3f_Copy(&this->velocity, &gZeroVec3f);
        MM_Math_Vec3f_Copy(&this->accel, &gZeroVec3f);
        this->life = 10;
        this->actor = initParams->actor;
        this->draw = MM_EffectSsEnIce_Draw;
        this->update = MM_EffectSsEnIce_UpdateFlying;
        this->rScale = initParams->scale * 100.0f;
        this->rPrimColorR = initParams->primColor.r;
        this->rPrimColorG = initParams->primColor.g;
        this->rPrimColorB = initParams->primColor.b;
        this->rPrimColorA = initParams->primColor.a;
        this->rEnvColorR = initParams->envColor.r;
        this->rEnvColorG = initParams->envColor.g;
        this->rEnvColorB = initParams->envColor.b;
        this->rAlphaMode = 1;
        this->rPitch = MM_Rand_CenteredFloat(0x10000);
    } else if (initParams->type == ENICE_TYPE_NORMAL) {
        MM_Math_Vec3f_Copy(&this->pos, &initParams->pos);
        MM_Math_Vec3f_Copy(&this->vec, &initParams->pos);
        MM_Math_Vec3f_Copy(&this->velocity, &initParams->velocity);
        MM_Math_Vec3f_Copy(&this->accel, &initParams->accel);
        this->life = initParams->life;
        this->draw = MM_EffectSsEnIce_Draw;
        this->update = MM_EffectSsEnIce_Update;
        this->rLifespan = initParams->life;
        this->rScale = initParams->scale * 100.0f;
        this->rYaw = Math_Atan2S_XY(initParams->velocity.z, initParams->velocity.x);
        this->rPitch = 0;
        this->rPrimColorR = initParams->primColor.r;
        this->rPrimColorG = initParams->primColor.g;
        this->rPrimColorB = initParams->primColor.b;
        this->rPrimColorA = initParams->primColor.a;
        this->rEnvColorR = initParams->envColor.r;
        this->rEnvColorG = initParams->envColor.g;
        this->rEnvColorB = initParams->envColor.b;
        this->rAlphaMode = 0;
    } else {
        return 0;
    }

    return 1;
}

void MM_EffectSsEnIce_Draw(PlayState* play, u32 index, EffectSs* this) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 pad;
    f32 scale = this->rScale * 0.01f;
    u32 gameplayFrames = play->gameplayFrames;
    f32 alpha;

    OPEN_DISPS(gfxCtx);

    if (this->rAlphaMode != 0) {
        alpha = this->life * 12.0f;
    } else if ((this->rLifespan > 0) && (this->life < (this->rLifespan >> 1))) {
        alpha = (this->life * 2.0f) / this->rLifespan;
        alpha *= 255.0f;
    } else {
        alpha = 255.0f;
    }

    MM_Matrix_Translate(this->pos.x, this->pos.y, this->pos.z, MTXMODE_NEW);
    MM_Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_RotateYS(this->rYaw, MTXMODE_APPLY);
    Matrix_RotateXS(this->rPitch, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, gfxCtx);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    func_800BCC68(&this->pos, play);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               MM_Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, gameplayFrames & 0xFF, 0x20, 0x10, 1, 0,
                                (gameplayFrames * 2) & 0xFF, 0x40, 0x20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, this->rPrimColorR, this->rPrimColorG, this->rPrimColorB,
                    this->rPrimColorA);
    gDPSetEnvColor(POLY_XLU_DISP++, this->rEnvColorR, this->rEnvColorG, this->rEnvColorB, (u32)alpha);
    gSPDisplayList(POLY_XLU_DISP++, gEffIceFragment2MaterialDL);
    gSPDisplayList(POLY_XLU_DISP++, gEffIceFragment2ModelDL);

    CLOSE_DISPS(gfxCtx);
}

void MM_EffectSsEnIce_UpdateFlying(PlayState* play, u32 index, EffectSs* this) {
    s16 rand;

    if ((this->actor != NULL) && (this->actor->update != NULL)) {
        if ((this->life >= 9) && (this->actor->colorFilterTimer != 0) && !(this->actor->colorFilterParams & 0xC000)) {
            MM_Math_Vec3f_Sum(&this->actor->world.pos, &this->vec, &this->pos);
            this->life++;
        } else if (this->life == 9) {
            this->accel.x = MM_Math_SinS(MM_Math_Vec3f_Yaw(&this->actor->world.pos, &this->pos)) * (MM_Rand_ZeroOne() + 1.0f);
            this->accel.z = MM_Math_CosS(MM_Math_Vec3f_Yaw(&this->actor->world.pos, &this->pos)) * (MM_Rand_ZeroOne() + 1.0f);
            this->accel.y = -1.5f;
            this->velocity.y = 5.0f;
        }
    } else {
        this->actor = NULL;
        if (this->life >= 9) {
            rand = MM_Rand_CenteredFloat(0xFFFF);
            this->accel.x = MM_Math_SinS(rand) * (MM_Rand_ZeroOne() + 1.0f);
            this->accel.z = MM_Math_CosS(rand) * (MM_Rand_ZeroOne() + 1.0f);
            this->life = 8;
            this->accel.y = -1.5f;
            this->velocity.y = 5.0f;
        }
    }
}

void MM_EffectSsEnIce_Update(PlayState* play, u32 index, EffectSs* this) {
    this->rPitch += this->rRotSpeed; // rRotSpeed is not initialized so this does nothing
}
