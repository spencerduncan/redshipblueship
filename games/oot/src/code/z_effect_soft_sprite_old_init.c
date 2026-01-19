#include "global.h"
#include "overlays/effects/ovl_Effect_Ss_Dust/z_eff_ss_dust.h"
#include "overlays/effects/ovl_Effect_Ss_KiraKira/z_eff_ss_kirakira.h"
#include "overlays/effects/ovl_Effect_Ss_Bomb/z_eff_ss_bomb.h"
#include "overlays/effects/ovl_Effect_Ss_Bomb2/z_eff_ss_bomb2.h"
#include "overlays/effects/ovl_Effect_Ss_Blast/z_eff_ss_blast.h"
#include "overlays/effects/ovl_Effect_Ss_G_Spk/z_eff_ss_g_spk.h"
#include "overlays/effects/ovl_Effect_Ss_D_Fire/z_eff_ss_d_fire.h"
#include "overlays/effects/ovl_Effect_Ss_Bubble/z_eff_ss_bubble.h"
#include "overlays/effects/ovl_Effect_Ss_G_Ripple/z_eff_ss_g_ripple.h"
#include "overlays/effects/ovl_Effect_Ss_G_Splash/z_eff_ss_g_splash.h"
#include "overlays/effects/ovl_Effect_Ss_G_Magma/z_eff_ss_g_magma.h"
#include "overlays/effects/ovl_Effect_Ss_G_Fire/z_eff_ss_g_fire.h"
#include "overlays/effects/ovl_Effect_Ss_Lightning/z_eff_ss_lightning.h"
#include "overlays/effects/ovl_Effect_Ss_Dt_Bubble/z_eff_ss_dt_bubble.h"
#include "overlays/effects/ovl_Effect_Ss_Hahen/z_eff_ss_hahen.h"
#include "overlays/effects/ovl_Effect_Ss_Stick/z_eff_ss_stick.h"
#include "overlays/effects/ovl_Effect_Ss_Sibuki/z_eff_ss_sibuki.h"
#include "overlays/effects/ovl_Effect_Ss_Sibuki2/z_eff_ss_sibuki2.h"
#include "overlays/effects/ovl_Effect_Ss_G_Magma2/z_eff_ss_g_magma2.h"
#include "overlays/effects/ovl_Effect_Ss_Stone1/z_eff_ss_stone1.h"
#include "overlays/effects/ovl_Effect_Ss_HitMark/z_eff_ss_hitmark.h"
#include "overlays/effects/ovl_Effect_Ss_Fhg_Flash/z_eff_ss_fhg_flash.h"
#include "overlays/effects/ovl_Effect_Ss_K_Fire/z_eff_ss_k_fire.h"
#include "overlays/effects/ovl_Effect_Ss_Solder_Srch_Ball/z_eff_ss_solder_srch_ball.h"
#include "overlays/effects/ovl_Effect_Ss_Kakera/z_eff_ss_kakera.h"
#include "overlays/effects/ovl_Effect_Ss_Ice_Piece/z_eff_ss_ice_piece.h"
#include "overlays/effects/ovl_Effect_Ss_En_Ice/z_eff_ss_en_ice.h"
#include "overlays/effects/ovl_Effect_Ss_Fire_Tail/z_eff_ss_fire_tail.h"
#include "overlays/effects/ovl_Effect_Ss_En_Fire/z_eff_ss_en_fire.h"
#include "overlays/effects/ovl_Effect_Ss_Extra/z_eff_ss_extra.h"
#include "overlays/effects/ovl_Effect_Ss_Fcircle/z_eff_ss_fcircle.h"
#include "overlays/effects/ovl_Effect_Ss_Dead_Db/z_eff_ss_dead_db.h"
#include "overlays/effects/ovl_Effect_Ss_Dead_Dd/z_eff_ss_dead_dd.h"
#include "overlays/effects/ovl_Effect_Ss_Dead_Ds/z_eff_ss_dead_ds.h"
#include "overlays/effects/ovl_Effect_Ss_Dead_Sound/z_eff_ss_dead_sound.h"
#include "overlays/effects/ovl_Effect_Ss_Ice_Smoke/z_eff_ss_ice_smoke.h"

static Vec3f OoT_sZeroVec = { 0.0f, 0.0f, 0.0f };

// effects that use this draw function are responsible for making sure their regs line up with the usage here

void OoT_EffectSs_DrawGEffect(PlayState* play, EffectSs* this, void* texture) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    f32 scale;
    MtxF mfTrans;
    MtxF mfScale;
    MtxF mfResult;
    MtxF mfTrans11DA0;
    s32 pad1;
    Mtx* mtx;
    void* object = play->objectCtx.status[this->rgObjBankIdx].segment;

    OPEN_DISPS(gfxCtx);

    scale = this->rgScale * 0.0025f;
    OoT_SkinMatrix_SetTranslate(&mfTrans, this->pos.x, this->pos.y, this->pos.z);
    OoT_SkinMatrix_SetScale(&mfScale, scale, scale, scale);
    OoT_SkinMatrix_MtxFMtxFMult(&mfTrans, &play->billboardMtxF, &mfTrans11DA0);
    OoT_SkinMatrix_MtxFMtxFMult(&mfTrans11DA0, &mfScale, &mfResult);
    OoT_gSegments[6] = VIRTUAL_TO_PHYSICAL(object);
    gSPSegment(POLY_XLU_DISP++, 0x06, object);

    mtx = OoT_SkinMatrix_MtxFToNewMtx(gfxCtx, &mfResult);

    if (mtx != NULL) {
        gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(texture));
        Gfx_SetupDL_61Xlu(gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, this->rgPrimColorR, this->rgPrimColorG, this->rgPrimColorB,
                        this->rgPrimColorA);
        gDPSetEnvColor(POLY_XLU_DISP++, this->rgEnvColorR, this->rgEnvColorG, this->rgEnvColorB, this->rgEnvColorA);
        gSPDisplayList(POLY_XLU_DISP++, this->gfx);
    }

    CLOSE_DISPS(gfxCtx);
}

// EffectSsDust Spawn Functions

void OoT_EffectSsDust_Spawn(PlayState* play, u16 drawFlags, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                        Color_RGBA8* primColor, Color_RGBA8* envColor, s16 scale, s16 scaleStep, s16 life,
                        u8 updateMode) {
    EffectSsDustInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.primColor = *primColor;
    initParams.envColor = *envColor;
    initParams.drawFlags = drawFlags;
    initParams.scale = scale;
    initParams.scaleStep = scaleStep;
    initParams.life = life;
    initParams.updateMode = updateMode;

    OoT_EffectSs_Spawn(play, EFFECT_SS_DUST, 128, &initParams);
}

void func_8002829C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, s16 scale, s16 scaleStep) {
    OoT_EffectSsDust_Spawn(play, 0, pos, velocity, accel, primColor, envColor, scale, scaleStep, 10, 0);
}

void func_80028304(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, s16 scale, s16 scaleStep) {
    OoT_EffectSsDust_Spawn(play, 1, pos, velocity, accel, primColor, envColor, scale, scaleStep, 10, 0);
}

void func_8002836C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, s16 scale, s16 scaleStep, s16 life) {
    OoT_EffectSsDust_Spawn(play, 0, pos, velocity, accel, primColor, envColor, scale, scaleStep, life, 0);
}

void func_800283D4(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, s16 scale, s16 scaleStep, s16 life) {
    OoT_EffectSsDust_Spawn(play, 1, pos, velocity, accel, primColor, envColor, scale, scaleStep, life, 0);
}

void func_8002843C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, s16 scale, s16 scaleStep, s16 life) {
    OoT_EffectSsDust_Spawn(play, 2, pos, velocity, accel, primColor, envColor, scale, scaleStep, life, 0);
}

// unused
void func_800284A4(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, s16 scale, s16 scaleStep) {
    OoT_EffectSsDust_Spawn(play, 0, pos, velocity, accel, primColor, envColor, scale, scaleStep, 10, 1);
}

// unused
void func_80028510(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, s16 scale, s16 scaleStep) {
    OoT_EffectSsDust_Spawn(play, 1, pos, velocity, accel, primColor, envColor, scale, scaleStep, 10, 1);
}

static Color_RGBA8 OoT_sDustBrownPrim = { 170, 130, 90, 255 };
static Color_RGBA8 OoT_sDustBrownEnv = { 100, 60, 20, 255 };

void func_8002857C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel) {
    OoT_EffectSsDust_Spawn(play, 4, pos, velocity, accel, &OoT_sDustBrownPrim, &OoT_sDustBrownEnv, 100, 5, 10, 0);
}

// unused
void func_800285EC(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel) {
    OoT_EffectSsDust_Spawn(play, 5, pos, velocity, accel, &OoT_sDustBrownPrim, &OoT_sDustBrownEnv, 100, 5, 10, 0);
}

void func_8002865C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, s16 scaleStep) {
    OoT_EffectSsDust_Spawn(play, 4, pos, velocity, accel, &OoT_sDustBrownPrim, &OoT_sDustBrownEnv, scale, scaleStep, 10, 0);
}

void func_800286CC(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, s16 scaleStep) {
    OoT_EffectSsDust_Spawn(play, 5, pos, velocity, accel, &OoT_sDustBrownPrim, &OoT_sDustBrownEnv, scale, scaleStep, 10, 0);
}

void func_8002873C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, s16 scaleStep, s16 life) {
    OoT_EffectSsDust_Spawn(play, 4, pos, velocity, accel, &OoT_sDustBrownPrim, &OoT_sDustBrownEnv, scale, scaleStep, life, 0);
}

void func_800287AC(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, s16 scaleStep, s16 life) {
    OoT_EffectSsDust_Spawn(play, 5, pos, velocity, accel, &OoT_sDustBrownPrim, &OoT_sDustBrownEnv, scale, scaleStep, life, 0);
}

// unused
void func_8002881C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor) {
    func_8002829C(play, pos, velocity, accel, primColor, envColor, 100, 5);
}

// unused
void func_80028858(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor) {
    func_80028304(play, pos, velocity, accel, primColor, envColor, 100, 5);
}

void func_80028894(Vec3f* srcPos, f32 randScale, Vec3f* newPos, Vec3f* velocity, Vec3f* accel) {
    s16 randAngle;
    f32 rand = OoT_Rand_ZeroOne() * randScale;

    randAngle = (OoT_Rand_ZeroOne() * 65536.0f);

    *newPos = *srcPos;

    newPos->x += OoT_Math_SinS(randAngle) * rand;
    newPos->z += OoT_Math_CosS(randAngle) * rand;

    velocity->y = 1.0f;
    velocity->x = OoT_Math_SinS(randAngle);
    velocity->z = OoT_Math_CosS(randAngle);

    accel->x = 0.0f;
    accel->y = 0.0f;
    accel->z = 0.0f;
}

void func_80028990(PlayState* play, f32 randScale, Vec3f* srcPos) {
    s32 i;
    Vec3f pos;
    Vec3f velocity;
    Vec3f accel;

    for (i = 0; i < 20; i++) {
        func_80028894(srcPos, randScale, &pos, &velocity, &accel);
        func_8002873C(play, &pos, &velocity, &accel, 100, 30, 7);
    }
}

void func_80028A54(PlayState* play, f32 randScale, Vec3f* srcPos) {
    s32 i;
    Vec3f pos;
    Vec3f velocity;
    Vec3f accel;

    for (i = 0; i < 20; i++) {
        func_80028894(srcPos, randScale, &pos, &velocity, &accel);
        func_800287AC(play, &pos, &velocity, &accel, 100, 30, 7);
    }
}

// EffectSsKiraKira Spawn Functions

void EffectSsKiraKira_SpawnSmallYellow(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel) {
    Color_RGBA8 primColor = { 255, 255, 200, 255 };
    Color_RGBA8 envColor = { 255, 200, 0, 0 };

    EffectSsKiraKira_SpawnDispersed(play, pos, velocity, accel, &primColor, &envColor, 1000, 16);
}

void EffectSsKiraKira_SpawnSmall(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                                 Color_RGBA8* envColor) {
    EffectSsKiraKira_SpawnDispersed(play, pos, velocity, accel, primColor, envColor, 1000, 16);
}

void EffectSsKiraKira_SpawnDispersed(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                                     Color_RGBA8* envColor, s16 scale, s32 life) {
    EffectSsKiraKiraInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    initParams.velocity.y = ((OoT_Rand_ZeroOne() * initParams.velocity.y) + initParams.velocity.y) * 0.5f;
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.accel.y = ((OoT_Rand_ZeroOne() * initParams.accel.y) + initParams.accel.y) * 0.5f;
    initParams.life = life;
    initParams.updateMode = 0;
    initParams.rotSpeed = 0x1518;
    initParams.yaw = OoT_Rand_ZeroOne() * 16384.0f;
    initParams.scale = scale;
    initParams.primColor = *primColor;
    initParams.envColor = *envColor;
    initParams.alphaStep = (-(255.0f / initParams.life)) + (-(255.0f / initParams.life));

    OoT_EffectSs_Spawn(play, EFFECT_SS_KIRAKIRA, 128, &initParams);
}

void EffectSsKiraKira_SpawnFocused(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                                   Color_RGBA8* envColor, s16 scale, s32 life) {
    EffectSsKiraKiraInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.life = life;
    initParams.updateMode = 1;
    initParams.rotSpeed = 0x1518;
    initParams.yaw = OoT_Rand_ZeroOne() * 16384.0f;
    initParams.scale = scale;
    OoT_Color_RGBA8_Copy(&initParams.primColor, primColor);
    OoT_Color_RGBA8_Copy(&initParams.envColor, envColor);
    initParams.alphaStep = (-(255.0f / initParams.life)) + (-(255.0f / initParams.life));

    OoT_EffectSs_Spawn(play, EFFECT_SS_KIRAKIRA, 128, &initParams);
}

// EffectSsBomb Spawn Functions

// unused
void EffectSsBomb_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel) {
    EffectSsBombInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);

    OoT_EffectSs_Spawn(play, EFFECT_SS_BOMB, 128, &initParams);
}

// EffectSsBomb2 Spawn Functions

// unused
void OoT_EffectSsBomb2_SpawnFade(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel) {
    EffectSsBomb2InitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = 100;
    initParams.scaleStep = 0;
    initParams.drawMode = 0;

    OoT_EffectSs_Spawn(play, EFFECT_SS_BOMB2, 10, &initParams);
}

void OoT_EffectSsBomb2_SpawnLayered(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, s16 scaleStep) {
    EffectSsBomb2InitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = scale;
    initParams.scaleStep = scaleStep;
    initParams.drawMode = 1;

    OoT_EffectSs_Spawn(play, EFFECT_SS_BOMB2, 10, &initParams);
}

// EffectSsBlast Spawn Functions

void OoT_EffectSsBlast_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                         Color_RGBA8* envColor, s16 scale, s16 scaleStep, s16 sclaeStepDecay, s16 life) {
    EffectSsBlastParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    OoT_Color_RGBA8_Copy(&initParams.primColor, primColor);
    OoT_Color_RGBA8_Copy(&initParams.envColor, envColor);
    initParams.scale = scale;
    initParams.scaleStep = scaleStep;
    initParams.sclaeStepDecay = sclaeStepDecay;
    initParams.life = life;

    OoT_EffectSs_Spawn(play, EFFECT_SS_BLAST, 128, &initParams);
}

void OoT_EffectSsBlast_SpawnWhiteCustomScale(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale,
                                         s16 scaleStep, s16 life) {
    static Color_RGBA8 primColor = { 255, 255, 255, 255 };
    static Color_RGBA8 envColor = { 200, 200, 200, 0 };

    OoT_EffectSsBlast_Spawn(play, pos, velocity, accel, &primColor, &envColor, scale, scaleStep, 35, life);
}

void OoT_EffectSsBlast_SpawnShockwave(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                                  Color_RGBA8* envColor, s16 life) {
    OoT_EffectSsBlast_Spawn(play, pos, velocity, accel, primColor, envColor, 100, 375, 35, life);
}

void OoT_EffectSsBlast_SpawnWhiteShockwave(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel) {
    static Color_RGBA8 primColor = { 255, 255, 255, 255 };
    static Color_RGBA8 envColor = { 200, 200, 200, 0 };

    OoT_EffectSsBlast_SpawnShockwave(play, pos, velocity, accel, &primColor, &envColor, 10);
}

// EffectSsGSpk Spawn Functions

void OoT_EffectSsGSpk_SpawnAccel(PlayState* play, Actor* actor, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                             Color_RGBA8* primColor, Color_RGBA8* envColor, s16 scale, s16 scaleStep) {
    EffectSsGSpkInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    OoT_Color_RGBA8_Copy(&initParams.primColor, primColor);
    OoT_Color_RGBA8_Copy(&initParams.envColor, envColor);
    initParams.actor = actor;
    initParams.scale = scale;
    initParams.scaleStep = scaleStep;
    initParams.updateMode = 0;

    OoT_EffectSs_Spawn(play, EFFECT_SS_G_SPK, 128, &initParams);
}

// unused
void OoT_EffectSsGSpk_SpawnNoAccel(PlayState* play, Actor* actor, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                               Color_RGBA8* primColor, Color_RGBA8* envColor, s16 scale, s16 scaleStep) {
    EffectSsGSpkInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    OoT_Color_RGBA8_Copy(&initParams.primColor, primColor);
    OoT_Color_RGBA8_Copy(&initParams.envColor, envColor);
    initParams.actor = actor;
    initParams.scale = scale;
    initParams.scaleStep = scaleStep;
    initParams.updateMode = 1;

    OoT_EffectSs_Spawn(play, EFFECT_SS_G_SPK, 128, &initParams);
}

void OoT_EffectSsGSpk_SpawnFuse(PlayState* play, Actor* actor, Vec3f* pos, Vec3f* velocity, Vec3f* accel) {
    Color_RGBA8 primColor = { 255, 255, 150, 255 };
    Color_RGBA8 envColor = { 255, 0, 0, 0 };

    OoT_EffectSsGSpk_SpawnSmall(play, actor, pos, velocity, accel, &primColor, &envColor);
}

// unused
void OoT_EffectSsGSpk_SpawnRandColor(PlayState* play, Actor* actor, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale,
                                 s16 scaleStep) {
    Color_RGBA8 primColor = { 255, 255, 150, 255 };
    Color_RGBA8 envColor = { 255, 0, 0, 0 };
    s32 randOffset = (OoT_Rand_ZeroOne() * 20.0f) - 10.0f;

    primColor.r += randOffset;
    primColor.g += randOffset;
    primColor.b += randOffset;
    primColor.a += randOffset;
    envColor.r += randOffset;
    envColor.g += randOffset;
    envColor.b += randOffset;
    envColor.a += randOffset;

    OoT_EffectSsGSpk_SpawnAccel(play, actor, pos, velocity, accel, &primColor, &envColor, scale, scaleStep);
}

void OoT_EffectSsGSpk_SpawnSmall(PlayState* play, Actor* actor, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                             Color_RGBA8* primColor, Color_RGBA8* envColor) {
    OoT_EffectSsGSpk_SpawnAccel(play, actor, pos, velocity, accel, primColor, envColor, 100, 5);
}

// EffectSsDFire Spawn Functions

void OoT_EffectSsDFire_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, s16 scaleStep,
                         s16 alpha, s16 fadeDelay, s32 life) {
    EffectSsDFireInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = scale;
    initParams.scaleStep = scaleStep;
    initParams.alpha = alpha;
    initParams.fadeDelay = fadeDelay;
    initParams.life = life;

    OoT_EffectSs_Spawn(play, EFFECT_SS_D_FIRE, 128, &initParams);
}

void EffectSsDFire_SpawnFixedScale(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 alpha,
                                   s16 fadeDelay) {
    OoT_EffectSsDFire_Spawn(play, pos, velocity, accel, 100, 35, alpha, fadeDelay, 8);
}

// EffectSsBubble Spawn Functions

void OoT_EffectSsBubble_Spawn(PlayState* play, Vec3f* pos, f32 yPosOffset, f32 yPosRandScale, f32 xzPosRandScale,
                          f32 scale) {
    EffectSsBubbleInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    initParams.yPosOffset = yPosOffset;
    initParams.yPosRandScale = yPosRandScale;
    initParams.xzPosRandScale = xzPosRandScale;
    initParams.scale = scale;

    OoT_EffectSs_Spawn(play, EFFECT_SS_BUBBLE, 128, &initParams);
}

// EffectSsGRipple Spawn Functions

void OoT_EffectSsGRipple_Spawn(PlayState* play, Vec3f* pos, s16 radius, s16 radiusMax, s16 life) {
    EffectSsGRippleInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    initParams.radius = radius;
    initParams.radiusMax = radiusMax;
    initParams.life = life;

    OoT_EffectSs_Spawn(play, EFFECT_SS_G_RIPPLE, 128, &initParams);
}

// EffectSsGSplash Spawn Functions

void OoT_EffectSsGSplash_Spawn(PlayState* play, Vec3f* pos, Color_RGBA8* primColor, Color_RGBA8* envColor, s16 type,
                           s16 scale) {
    EffectSsGSplashInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    initParams.type = type;
    initParams.scale = scale;

    if (primColor != NULL) {
        initParams.primColor = *primColor;
        initParams.envColor = *envColor;
        initParams.customColor = true;
    } else {
        initParams.customColor = false;
    }

    OoT_EffectSs_Spawn(play, EFFECT_SS_G_SPLASH, 128, &initParams);
}

// EffectSsGMagma Spawn Functions

void EffectSsGMagma_Spawn(PlayState* play, Vec3f* pos) {
    EffectSsGMagmaInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);

    OoT_EffectSs_Spawn(play, EFFECT_SS_G_MAGMA, 128, &initParams);
}

// EffectSsGFire Spawn Functions

void OoT_EffectSsGFire_Spawn(PlayState* play, Vec3f* pos) {
    EffectSsGFireInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);

    OoT_EffectSs_Spawn(play, EFFECT_SS_G_FIRE, 128, &initParams);
}

// EffectSsLightning Spawn Functions

void OoT_EffectSsLightning_Spawn(PlayState* play, Vec3f* pos, Color_RGBA8* primColor, Color_RGBA8* envColor, s16 scale,
                             s16 yaw, s16 life, s16 numBolts) {
    EffectSsLightningInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Color_RGBA8_Copy(&initParams.primColor, primColor);
    OoT_Color_RGBA8_Copy(&initParams.envColor, envColor);
    initParams.scale = scale;
    initParams.yaw = yaw;
    initParams.life = life;
    initParams.numBolts = numBolts;

    OoT_EffectSs_Spawn(play, EFFECT_SS_LIGHTNING, 128, &initParams);
}

// EffectSsDtBubble Spawn Functions

void OoT_EffectSsDtBubble_SpawnColorProfile(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, s16 life,
                                        s16 colorProfile, s16 randXZ) {
    EffectSsDtBubbleInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.customColor = false;
    initParams.colorProfile = colorProfile;
    initParams.scale = scale;
    initParams.life = life;
    initParams.randXZ = randXZ;

    OoT_EffectSs_Spawn(play, EFFECT_SS_DT_BUBBLE, 128, &initParams);
}

void OoT_EffectSsDtBubble_SpawnCustomColor(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                                       Color_RGBA8* primColor, Color_RGBA8* envColor, s16 scale, s16 life, s16 randXZ) {
    EffectSsDtBubbleInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    OoT_Color_RGBA8_Copy(&initParams.primColor, primColor);
    OoT_Color_RGBA8_Copy(&initParams.envColor, envColor);
    initParams.scale = scale;
    initParams.life = life;
    initParams.randXZ = randXZ;
    initParams.customColor = true;

    OoT_EffectSs_Spawn(play, EFFECT_SS_DT_BUBBLE, 128, &initParams);
}

// EffectSsHahen Spawn Functions

/**
 * Spawn a single fragment
 *
 * Notes:
 *     - if a display list is not provided, D_0400C0D0 (wilted deku fragment) will be used as default
 *     - the unused arg does not do anything, any value can be passed here
 *     - due to how life is implemented it is capped at 200. Any value over 200 is accepted, but the fragment will
 *       only live for 200 frames
 */
void OoT_EffectSsHahen_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 unused, s16 scale, s16 objId,
                         s16 life, Gfx* dList) {
    EffectSsHahenInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.dList = dList;
    initParams.unused = unused;
    initParams.scale = scale;
    initParams.objId = objId;
    initParams.life = life;

    OoT_EffectSs_Spawn(play, EFFECT_SS_HAHEN, 128, &initParams);
}

/**
 * Spawn a burst of fragments, with the amount of fragments specifed by count and burst speed set by <arg2>
 *
 * Notes:
 *     - if a display list is not provided, D_0400C0D0 (wilted deku fragment) will be used as default
 *     - the unused arg does not do anything, any value can be passed here
 *     - due to how life is implemented it is capped at 200. Any value over 200 is accepted, but the fragment will
 *       only live for 200 frames
 */
void OoT_EffectSsHahen_SpawnBurst(PlayState* play, Vec3f* pos, f32 burstScale, s16 unused, s16 scale, s16 randScaleRange,
                              s16 count, s16 objId, s16 life, Gfx* dList) {
    s32 i;
    Vec3f velocity;
    Vec3f accel;

    accel.y = -0.07f * burstScale;
    accel.x = accel.z = 0.0f;

    for (i = 0; i < count; i++) {
        velocity.x = (OoT_Rand_ZeroOne() - 0.5f) * burstScale;
        velocity.z = (OoT_Rand_ZeroOne() - 0.5f) * burstScale;
        velocity.y = ((OoT_Rand_ZeroOne() * 0.5f) + 0.5f) * burstScale;

        OoT_EffectSsHahen_Spawn(play, pos, &velocity, &accel, unused, OoT_Rand_S16Offset(scale, randScaleRange), objId, life,
                            dList);
    }
}

// EffectSsStick Spawn Functions

/**
 * As child, spawn a broken stick fragment
 * As adult, spawn a broken sword fragment
 */
void OoT_EffectSsStick_Spawn(PlayState* play, Vec3f* pos, s16 yaw) {
    EffectSsStickInitParams initParams;

    initParams.pos = *pos;
    initParams.yaw = yaw;

    OoT_EffectSs_Spawn(play, EFFECT_SS_STICK, 128, &initParams);
}

// EffectSsSibuki Spawn Functions

void OoT_EffectSsSibuki_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 moveDelay, s16 direction,
                          s16 scale) {
    EffectSsSibukiInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.moveDelay = moveDelay;
    initParams.direction = direction;
    initParams.scale = scale;

    OoT_EffectSs_Spawn(play, EFFECT_SS_SIBUKI, 128, &initParams);
}

void OoT_EffectSsSibuki_SpawnBurst(PlayState* play, Vec3f* pos) {
    s16 i;
    Vec3f unusedZeroVec1 = { 0.0f, 0.0f, 0.0f };
    Vec3f unusedZeroVec2 = { 0.0f, 0.0f, 0.0f };
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    s16 randDirection = OoT_Rand_ZeroOne() * 1.99f;

    for (i = 0; i < KREG(19) + 30; i++) {
        OoT_EffectSsSibuki_Spawn(play, pos, &zeroVec, &zeroVec, i / (KREG(27) + 6), randDirection, KREG(18) + 40);
    }
}

// EffectSsSibuki2 Spawn Functions

// unused
void EffectSsSibuki2_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale) {
    EffectSsSibuki2InitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = scale;
    OoT_EffectSs_Spawn(play, EFFECT_SS_SIBUKI2, 128, &initParams);
}

// EffectSsGMagma2 Spawn Functions

void EffectSsGMagma2_Spawn(PlayState* play, Vec3f* pos, Color_RGBA8* primColor, Color_RGBA8* envColor, s16 updateRate,
                           s16 drawMode, s16 scale) {
    EffectSsGMagma2InitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Color_RGBA8_Copy(&initParams.primColor, primColor);
    OoT_Color_RGBA8_Copy(&initParams.envColor, envColor);
    initParams.updateRate = updateRate;
    initParams.drawMode = drawMode;
    initParams.scale = scale;

    OoT_EffectSs_Spawn(play, EFFECT_SS_G_MAGMA2, 128, &initParams);
}

// EffectSsStone1 Spawn Functions

void OoT_EffectSsStone1_Spawn(PlayState* play, Vec3f* pos, s32 arg2) {
    EffectSsStone1InitParams initParams;

    initParams.pos = *pos;
    initParams.unk_C = arg2;

    OoT_EffectSs_Spawn(play, EFFECT_SS_STONE1, 128, &initParams);
}

// EffectSsHitMark Spawn Functions

void EffectSsHitMark_Spawn(PlayState* play, s32 type, s16 scale, Vec3f* pos) {
    EffectSsHitMarkInitParams initParams;

    initParams.type = type;
    initParams.scale = scale;
    OoT_Math_Vec3f_Copy(&initParams.pos, pos);

    OoT_EffectSs_Spawn(play, EFFECT_SS_HITMARK, 128, &initParams);
}

void EffectSsHitMark_SpawnFixedScale(PlayState* play, s32 type, Vec3f* pos) {
    EffectSsHitMark_Spawn(play, type, 300, pos);
}

void EffectSsHitMark_SpawnCustomScale(PlayState* play, s32 type, s16 scale, Vec3f* pos) {
    EffectSsHitMark_Spawn(play, type, scale, pos);
}

// EffectSsFhgFlash Spawn Functions

/**
 * Spawn a light ball effect
 *
 * param changes the color of the ball. Refer to FhgFlashLightBallParam for the options.
 * Note: this type requires OBJECT_FHG to be loaded
 */
void EffectSsFhgFlash_SpawnLightBall(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, u8 param) {
    EffectSsFhgFlashInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = scale;
    initParams.param = param;
    initParams.type = FHGFLASH_LIGHTBALL;

    OoT_EffectSs_Spawn(play, EFFECT_SS_FHG_FLASH, 128, &initParams);
}

/**
 * Spawn a shock effect
 *
 * param determines where the ligntning should go
 * 0: dont attach to any actor. spawns at the position specified by pos
 * 1: spawn at one of Player's body parts, chosen at random
 * 2: spawn at one of Phantom Ganon's body parts, chosen at random
 */
void OoT_EffectSsFhgFlash_SpawnShock(PlayState* play, Actor* actor, Vec3f* pos, s16 scale, u8 param) {
    EffectSsFhgFlashInitParams initParams;

    initParams.actor = actor;
    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    initParams.scale = scale;
    initParams.param = param;
    initParams.type = FHGFLASH_SHOCK;

    OoT_EffectSs_Spawn(play, EFFECT_SS_FHG_FLASH, 128, &initParams);
}

// EffectSsKFire Spawn Functions

void OoT_EffectSsKFire_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scaleMax, u8 type) {
    EffectSsKFireInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scaleMax = scaleMax;
    initParams.type = type;

    OoT_EffectSs_Spawn(play, EFFECT_SS_K_FIRE, 128, &initParams);
}

// EffectSsSolderSrchBall Spawn Functions

void OoT_EffectSsSolderSrchBall_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 unused,
                                  s16* linkDetected) {
    EffectSsSolderSrchBallInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.unused = unused;
    initParams.linkDetected = linkDetected;

    OoT_EffectSs_Spawn(play, EFFECT_SS_SOLDER_SRCH_BALL, 128, &initParams);
}

// EffectSsKakera Spawn Functions

void OoT_EffectSsKakera_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* arg3, s16 gravity, s16 arg5, s16 arg6,
                          s16 arg7, s16 arg8, s16 scale, s16 arg10, s16 arg11, s32 life, s16 colorIdx, s16 objId,
                          Gfx* dList) {
    EffectSsKakeraInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.unk_18, arg3);
    initParams.gravity = gravity;
    initParams.unk_26 = arg5;
    initParams.unk_28 = arg6;
    initParams.unk_2A = arg7;
    initParams.unk_2C = arg8;
    initParams.scale = scale;
    initParams.unk_30 = arg10;
    initParams.unk_32 = arg11;
    initParams.life = life;
    initParams.colorIdx = colorIdx;
    initParams.objId = objId;
    initParams.dList = dList;

    OoT_EffectSs_Spawn(play, EFFECT_SS_KAKERA, 101, &initParams);
}

// EffectSsIcePiece Spawn Functions

void OoT_EffectSsIcePiece_Spawn(PlayState* play, Vec3f* pos, f32 scale, Vec3f* velocity, Vec3f* accel, s32 life) {
    EffectSsIcePieceInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = scale;
    initParams.life = life;
    OoT_EffectSs_Spawn(play, EFFECT_SS_ICE_PIECE, 128, &initParams);
}

void OoT_EffectSsIcePiece_SpawnBurst(PlayState* play, Vec3f* refPos, f32 scale) {
    static Vec3f accel = { 0.0f, 0.0f, 0.0f };
    static Vec3f vecScales[] = {
        { 0.0f, 70.0f, 0.0f },
        { 0.0f, 45.0f, 20.0f },
        { 17.320474f, 45.0f, 9.999695f },
        { 17.320474f, 45.0f, -9.999695f },
        { 0.0f, 45.0f, -20.0f },
        { -17.320474f, 45.0f, -9.999695f },
        { -17.320474f, 45.0f, 9.999695f },
        { 0.0f, 20.0f, 20.0f },
        { 17.320474f, 20.0f, -9.999695f },
        { -17.320474f, 20.0f, -9.999695f },
    }; // 17.320474 is approximately 10 * sqrt(3)
    s32 i;
    Vec3f velocity;
    Vec3f pos;
    f32 velocityScale;

    accel.y = -0.2f;

    for (i = 0; i < ARRAY_COUNT(vecScales); i++) {
        pos = *refPos;
        velocityScale = OoT_Rand_ZeroFloat(1.0f) + 0.5f;
        velocity.x = (vecScales[i].x * 0.18f) * velocityScale;
        velocity.y = (vecScales[i].y * 0.18f) * velocityScale;
        velocity.z = (vecScales[i].z * 0.18f) * velocityScale;
        pos.x += vecScales[i].x;
        pos.y += vecScales[i].y;
        pos.z += vecScales[i].z;

        OoT_EffectSsIcePiece_Spawn(play, &pos, (OoT_Rand_ZeroFloat(1.0f) + 0.5f) * ((scale * 1.3f) * 100.0f), &velocity, &accel,
                               25);
    }
}

// EffectSsEnIce Spawn Functions

void EffectSsEnIce_SpawnFlyingVec3f(PlayState* play, Actor* actor, Vec3f* pos, s16 primR, s16 primG, s16 primB,
                                    s16 primA, s16 envR, s16 envG, s16 envB, f32 scale) {

    EffectSsEnIceInitParams initParams;

    initParams.actor = actor;
    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    initParams.type = 0;
    initParams.primColor.r = primR;
    initParams.primColor.g = primG;
    initParams.primColor.b = primB;
    initParams.primColor.a = primA;
    initParams.envColor.r = envR;
    initParams.envColor.g = envG;
    initParams.envColor.b = envB;
    initParams.scale = scale;

    if (actor != NULL) {
        Audio_PlayActorSound2(actor, NA_SE_PL_FREEZE_S);
    }

    OoT_EffectSs_Spawn(play, EFFECT_SS_EN_ICE, 80, &initParams);
}

void EffectSsEnIce_SpawnFlyingVec3s(PlayState* play, Actor* actor, Vec3s* pos, s16 primR, s16 primG, s16 primB,
                                    s16 primA, s16 envR, s16 envG, s16 envB, f32 scale) {

    EffectSsEnIceInitParams initParams;

    initParams.actor = actor;
    initParams.pos.x = pos->x;
    initParams.pos.y = pos->y;
    initParams.pos.z = pos->z;
    initParams.primColor.r = primR;
    initParams.primColor.g = primG;
    initParams.primColor.b = primB;
    initParams.primColor.a = primA;
    initParams.envColor.r = envR;
    initParams.envColor.g = envG;
    initParams.envColor.b = envB;
    initParams.type = 0;
    initParams.scale = scale;

    if (actor != NULL) {
        Audio_PlayActorSound2(actor, NA_SE_PL_FREEZE_S);
    }

    OoT_EffectSs_Spawn(play, EFFECT_SS_EN_ICE, 80, &initParams);
}

void OoT_EffectSsEnIce_Spawn(PlayState* play, Vec3f* pos, f32 scale, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                         Color_RGBA8* envColor, s32 life) {
    EffectSsEnIceInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    OoT_Color_RGBA8_Copy(&initParams.primColor, primColor);
    OoT_Color_RGBA8_Copy(&initParams.envColor, envColor);
    initParams.scale = scale;
    initParams.life = life;
    initParams.type = 1;

    OoT_EffectSs_Spawn(play, EFFECT_SS_EN_ICE, 128, &initParams);
}

// EffectSsFireTail Spawn Functions

void OoT_EffectSsFireTail_Spawn(PlayState* play, Actor* actor, Vec3f* pos, f32 scale, Vec3f* arg4, s16 arg5,
                            Color_RGBA8* primColor, Color_RGBA8* envColor, s16 type, s16 bodyPart, s32 life) {
    EffectSsFireTailInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.unk_14, arg4);
    OoT_Color_RGBA8_Copy(&initParams.primColor, primColor);
    OoT_Color_RGBA8_Copy(&initParams.envColor, envColor);
    initParams.unk_20 = arg5;
    initParams.actor = actor;
    initParams.scale = scale;
    initParams.type = type;
    initParams.bodyPart = bodyPart;
    initParams.life = life;

    OoT_EffectSs_Spawn(play, EFFECT_SS_FIRE_TAIL, 128, &initParams);
}

void OoT_EffectSsFireTail_SpawnFlame(PlayState* play, Actor* actor, Vec3f* pos, f32 arg3, s16 bodyPart,
                                 f32 colorIntensity) {
    static Color_RGBA8 primColor = { 255, 255, 0, 255 };
    static Color_RGBA8 envColor = { 255, 0, 0, 255 };

    primColor.g = (s32)(255.0f * colorIntensity);
    primColor.b = 0;

    envColor.g = 0;
    envColor.b = 0;
    primColor.r = envColor.r = (s32)(255.0f * colorIntensity);

    OoT_EffectSsFireTail_Spawn(play, actor, pos, arg3, &actor->velocity, 15, &primColor, &envColor,
                           (colorIntensity == 1.0f) ? 0 : 1, bodyPart, 1);
}

void OoT_EffectSsFireTail_SpawnFlameOnPlayer(PlayState* play, f32 scale, s16 bodyPart, f32 colorIntensity) {
    Player* player = GET_PLAYER(play);

    OoT_EffectSsFireTail_SpawnFlame(play, &player->actor, &player->bodyPartsPos[bodyPart], scale, bodyPart, colorIntensity);
}

// EffectSsEnFire Spawn Functions

// note: if bodyPart is greater than -1 the actor MUST have a table of Vec3f positions at offset 0x14C in the instance
void OoT_EffectSsEnFire_SpawnVec3f(PlayState* play, Actor* actor, Vec3f* pos, s16 scale, s16 arg4, s16 flags,
                               s16 bodyPart) {
    EffectSsEnFireInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    initParams.actor = actor;
    initParams.scale = scale;
    initParams.unk_12 = arg4;
    initParams.flags = flags;
    initParams.bodyPart = bodyPart;

    if (actor != NULL) {
        Audio_PlayActorSound2(actor, NA_SE_EV_FLAME_IGNITION);
    }

    OoT_EffectSs_Spawn(play, EFFECT_SS_EN_FIRE, 128, &initParams);
}

// note: if bodyPart is greater than -1 the actor MUST have a table of Vec3s positions at offset 0x14C in the instance
void OoT_EffectSsEnFire_SpawnVec3s(PlayState* play, Actor* actor, Vec3s* pos, s16 scale, s16 arg4, s16 flags,
                               s16 bodyPart) {
    EffectSsEnFireInitParams initParams;

    initParams.pos.x = pos->x;
    initParams.pos.y = pos->y;
    initParams.pos.z = pos->z;
    initParams.actor = actor;
    initParams.scale = scale;
    initParams.unk_12 = arg4;
    initParams.flags = flags | 0x8000;
    initParams.bodyPart = bodyPart;

    if (actor != NULL) {
        Audio_PlayActorSound2(actor, NA_SE_EV_FLAME_IGNITION);
    }

    OoT_EffectSs_Spawn(play, EFFECT_SS_EN_FIRE, 128, &initParams);
}

// EffectSsExtra Spawn Functions

void OoT_EffectSsExtra_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, s16 scoreIdx) {
    EffectSsExtraInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = scale;
    initParams.scoreIdx = scoreIdx;

    OoT_EffectSs_Spawn(play, EFFECT_SS_EXTRA, 100, &initParams);
}

// EffectSsFCircle Spawn Functions

void EffectSsFCircle_Spawn(PlayState* play, Actor* actor, Vec3f* pos, s16 radius, s16 height) {
    EffectSsFcircleInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    initParams.actor = actor;
    initParams.radius = radius;
    initParams.height = height;

    OoT_EffectSs_Spawn(play, EFFECT_SS_FCIRCLE, 128, &initParams);
}

// EffectSsDeadDb Spawn Functions

void OoT_EffectSsDeadDb_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, s16 scaleStep,
                          s16 primR, s16 primG, s16 primB, s16 primA, s16 envR, s16 envG, s16 envB, s16 unused,
                          s32 arg14, s16 playSound) {
    EffectSsDeadDbInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = scale;
    initParams.scaleStep = scaleStep;
    initParams.primColor.r = primR;
    initParams.primColor.g = primG;
    initParams.primColor.b = primB;
    initParams.primColor.a = primA;
    initParams.envColor.r = envR;
    initParams.envColor.g = envG;
    initParams.envColor.b = envB;
    initParams.unused = unused;
    initParams.unk_34 = arg14;
    initParams.playSound = playSound;

    OoT_EffectSs_Spawn(play, EFFECT_SS_DEAD_DB, 120, &initParams);
}

// EffectSsDeadDd Spawn Functions

void OoT_EffectSsDeadDd_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, s16 scaleStep,
                          s16 primR, s16 primG, s16 primB, s16 alpha, s16 envR, s16 envG, s16 envB, s16 alphaStep,
                          s32 life) {
    EffectSsDeadDdInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = scale;
    initParams.type = 0;
    initParams.scaleStep = scaleStep;
    initParams.primColor.r = primR;
    initParams.primColor.g = primG;
    initParams.primColor.b = primB;
    initParams.alpha = alpha;
    initParams.envColor.r = envR;
    initParams.envColor.g = envG;
    initParams.envColor.b = envB;
    initParams.alphaStep = alphaStep;
    initParams.life = life;

    OoT_EffectSs_Spawn(play, EFFECT_SS_DEAD_DD, 120, &initParams);
}

// unused
void EffectSsDeadDd_SpawnRandYellow(PlayState* play, Vec3f* pos, s16 scale, s16 scaleStep, f32 randPosScale,
                                    s32 randIter, s32 life) {
    EffectSsDeadDdInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    initParams.scale = scale;
    initParams.scaleStep = scaleStep;
    initParams.randPosScale = randPosScale;
    initParams.randIter = randIter;
    initParams.life = life;
    initParams.type = 1;

    OoT_EffectSs_Spawn(play, EFFECT_SS_DEAD_DD, 120, &initParams);
}

// EffectSsDeadDs Spawn Functions

void OoT_EffectSsDeadDs_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, s16 scaleStep,
                          s16 alpha, s32 life) {
    EffectSsDeadDsInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = scale;
    initParams.scaleStep = scaleStep;
    initParams.alpha = alpha;
    initParams.life = life;
    OoT_EffectSs_Spawn(play, EFFECT_SS_DEAD_DS, 100, &initParams);
}

void EffectSsDeadDs_SpawnStationary(PlayState* play, Vec3f* pos, s16 scale, s16 scaleStep, s16 alpha, s32 life) {
    OoT_EffectSsDeadDs_Spawn(play, pos, &OoT_sZeroVec, &OoT_sZeroVec, scale, scaleStep, alpha, life);
}

// EffectSsDeadSound Spawn Functions

void EffectSsDeadSound_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, u16 sfxId, s16 lowerPriority,
                             s16 repeatMode, s32 life) {
    EffectSsDeadSoundInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.sfxId = sfxId;
    initParams.lowerPriority = lowerPriority;
    initParams.repeatMode = repeatMode;
    initParams.life = life;

    if (!lowerPriority) {
        OoT_EffectSs_Spawn(play, EFFECT_SS_DEAD_SOUND, 100, &initParams);
    } else {
        OoT_EffectSs_Spawn(play, EFFECT_SS_DEAD_SOUND, 127, &initParams);
    }
}

void EffectSsDeadSound_SpawnStationary(PlayState* play, Vec3f* pos, u16 sfxId, s16 lowerPriority, s16 repeatMode,
                                       s32 life) {
    EffectSsDeadSound_Spawn(play, pos, &OoT_sZeroVec, &OoT_sZeroVec, sfxId, lowerPriority, repeatMode, life);
}

// EffectSsIceSmoke Spawn Functions

/**
 * Spawn an Ice Smoke effect
 *
 * Note: this effect requires OBJECT_FZ to be loaded
 */
void OoT_EffectSsIceSmoke_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale) {
    EffectSsIceSmokeInitParams initParams;

    OoT_Math_Vec3f_Copy(&initParams.pos, pos);
    OoT_Math_Vec3f_Copy(&initParams.velocity, velocity);
    OoT_Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = scale;

    OoT_EffectSs_Spawn(play, EFFECT_SS_ICE_SMOKE, 128, &initParams);
}
