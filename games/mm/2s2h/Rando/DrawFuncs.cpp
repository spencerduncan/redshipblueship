#include "DrawFuncs.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "BenPort.h"

extern "C" {
#include <functions.h>
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_obj_tokeidai/object_obj_tokeidai.h"

// Soul Effects
#include "src/overlays/actors/ovl_Obj_Moon_Stone/z_obj_moon_stone.h"
#include "assets/objects/object_gi_reserve00/object_gi_reserve00.h"

s32 EnMinifrog_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* enMini);
s32 EnRd_ShouldNotDance(PlayState* play);
Gfx* EnKnight_BuildEmptyDL(GraphicsContext* gfxCtx);

// clang-format off
// Boss Includes
/* Goht */      #include "objects/object_boss_hakugin/object_boss_hakugin.h"
/* Gyorg */     #include "objects/object_boss03/object_boss03.h"
/* Odolwa */    #include "objects/object_boss01/object_boss01.h"
/* Twinmold */  #include "objects/object_boss02/object_boss02.h"
/* Majora */    #include "objects/object_boss07/object_boss07.h"

// Enemy Includes
/* Alien */         #include "assets/objects/object_uch/object_uch.h"
/* Armos */         #include "assets/objects/object_am/object_am.h" 
/* Bad Bat */       #include "assets/objects/object_bat/object_bat.h"
/* Beamos */        #include "assets/objects/object_vm/object_vm.h"
/* Boe */           #include "assets/objects/object_mkk/object_mkk.h"
/* Captain Keeta */ #include "assets/objects/object_bsb/object_bsb.h"
/* Chuchu */        #include "assets/objects/object_slime/object_slime.h"
/* Bubble */        #include "assets/objects/object_bb/object_bb.h"
/* Death Armos */   #include "assets/objects/object_famos/object_famos.h"
/* Deep Python */   #include "assets/objects/object_utubo/object_utubo.h"
/* Deku Baba */     #include "assets/objects/object_dekubaba/object_dekubaba.h"
/* Dexihand */      #include "assets/objects/object_wdhand/object_wdhand.h"
/* Dinolfos */      #include "assets/objects/object_dinofos/object_dinofos.h"
/* Dodongo */       #include "assets/objects/object_dodongo/object_dodongo.h"
/* Dragonfly */     #include "assets/objects/object_grasshopper/object_grasshopper.h"
/* Eeno */          #include "assets/objects/object_snowman/object_snowman.h"
/* Eyegore */       #include "assets/objects/object_eg/object_eg.h"
/* Flying Pot */    #include "assets/objects/gameplay_dangeon_keep/gameplay_dangeon_keep.h"
/* Freezard */      #include "assets/objects/object_fz/object_fz.h"
/* Garo */          #include "assets/objects/object_jso/object_jso.h"
/* Gekko */         #include "overlays/actors/ovl_En_Pametfrog/z_en_pametfrog.h"
/* Giant Bee */     #include "assets/objects/object_bee/object_bee.h"
/* Gomess */        #include "assets/objects/object_death/object_death.h"
/* Guay */          #include "assets/objects/object_crow/object_crow.h"
/* Hiploop */       #include "assets/objects/object_pp/object_pp.h"
/* Igos du Ikana */ #include "assets/objects/object_knight/object_knight.h"
/* Iron Knuckle */  #include "assets/objects/object_ik/object_ik.h"
/* Keese */         #include "assets/objects/object_firefly/object_firefly.h"
/* Leever */        #include "assets/objects/object_rb/object_rb.h"
/* Like Like */     #include "assets/objects/object_rr/object_rr.h"
/* Mad Scrub */     #include "assets/objects/object_dekunuts/object_dekunuts.h"
/* Nejiron */       #include "assets/objects/object_gmo/object_gmo.h"
/* Octorok */       #include "assets/objects/object_okuta/object_okuta.h"
/* Peehat */        #include "assets/objects/object_ph/object_ph.h"
/* Pirate */        #include "assets/objects/object_kz/object_kz.h"
/* Poe */           #include "assets/objects/object_po/object_po.h"
/* Poe */           #include "assets/objects/object_bigpo/object_bigpo.h"
/* Real Bombchu */  #include "assets/objects/object_rat/object_rat.h"
/* Redead */        #include "assets/objects/object_rd/object_rd.h"
/* Shellblade */    #include "assets/objects/object_sb/object_sb.h"
/* Skullfish */     #include "assets/objects/object_pr/object_pr.h"
/* Skulltula */     #include "assets/objects/object_st/object_st.h"
/* Snapper */       #include "assets/objects/object_tl/object_tl.h"
/* Stalchild */     #include "assets/objects/object_skb/object_skb.h"
/* Takkuri */       #include "assets/objects/object_thiefbird/object_thiefbird.h"
/* Tektite */       #include "assets/objects/object_tite/object_tite.h"
/* Wallmaster */    #include "assets/objects/object_wallmaster/object_wallmaster.h"
/* Wart */          #include "assets/objects/object_boss04/object_boss04.h"
/* Wizrobe */       #include "assets/objects/object_wiz/object_wiz.h"
/* Wolfos */        #include "assets/objects/object_wf/object_wf.h"

// Other Actor Includes
/* Minifrog */  #include "objects/object_fr/object_fr.h"
/* Clock */     #include "overlays/actors/ovl_Obj_Tokeidai/z_obj_tokeidai.h"
    
// Clock
void ObjTokeidai_RotateOnMinuteChange(ObjTokeidai* thisx, s32 playSfx);
void ObjTokeidai_RotateOnHourChange(ObjTokeidai* thisx, PlayState* play);
// clang-format on
}

#define SETUP_DRAW(LIMB_MAX)           \
    static bool initialized = false;   \
    static SkelAnime skelAnime;        \
    static Vec3s jointTable[LIMB_MAX]; \
    static Vec3s morphTable[LIMB_MAX]; \
    static u32 lastUpdate = 0;         \
    OPEN_DISPS(MM_gPlayState->state.gfxCtx);

#define SETUP_DRAW_TYPE(LIMB_MAX, SKEL_HEADER, ANIM_HEADER, INIT_TYPE, HEADER_TYPE)                               \
    if (!initialized) {                                                                                           \
        initialized = true;                                                                                       \
        INIT_TYPE(MM_gPlayState, &skelAnime, (HEADER_TYPE*)&SKEL_HEADER, (AnimationHeader*)&ANIM_HEADER, jointTable, \
                  morphTable, LIMB_MAX);                                                                          \
    }                                                                                                             \
    if (MM_gPlayState != NULL && lastUpdate != MM_gPlayState->state.frames) {                                           \
        lastUpdate = MM_gPlayState->state.frames;                                                                    \
        MM_SkelAnime_Update(&skelAnime);                                                                             \
    }

#define SETUP_SKEL(LIMB_MAX, SKEL_HEADER, ANIM_HEADER) \
    SETUP_DRAW_TYPE(LIMB_MAX, SKEL_HEADER, ANIM_HEADER, MM_SkelAnime_Init, SkeletonHeader)

#define SETUP_FLEX_SKEL(LIMB_MAX, SKEL_HEADER, ANIM_HEADER) \
    SETUP_DRAW_TYPE(LIMB_MAX, SKEL_HEADER, ANIM_HEADER, MM_SkelAnime_InitFlex, FlexSkeletonHeader)

// Soul Effects
extern void DrawEnLight(Color_RGB8 flameColor, Vec3f flameSize) {
    Gfx* sp68;
    static s8 unk_144 = (s8)(MM_Rand_ZeroOne() * 255.0f);
    static u32 lastUpdate = 0;

    OPEN_DISPS(MM_gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    MM_Matrix_ReplaceRotation(&MM_gPlayState->billboardMtxF);

    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)MM_Gfx_TwoTexScroll(MM_gPlayState->state.gfxCtx, 0, 0, 0, 0x10, 0x20, 1, (unk_144 * 2) & 0x3F,
                                           (unk_144 * -6) & 0x7F, 0x10, 0x20));
    sp68 = (Gfx*)gameplay_keep_DL_01ACF0;
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, flameColor.r, flameColor.g, flameColor.b, 0);
    gDPSetEnvColor(POLY_XLU_DISP++, flameColor.r, flameColor.g, flameColor.b, 0);
    MM_Matrix_Scale(flameSize.x, flameSize.y, flameSize.z, MTXMODE_APPLY);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, MM_gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sp68);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);

    if (MM_gPlayState != NULL && lastUpdate != MM_gPlayState->state.frames) {
        lastUpdate = MM_gPlayState->state.frames;
        unk_144++;
    }
}

// Limb Override Functions
void EnMinifrogPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    if ((limbIndex == FROG_LIMB_RIGHT_EYE) || (limbIndex == FROG_LIMB_LEFT_EYE)) {
        OPEN_DISPS(play->state.gfxCtx);

        MM_Matrix_ReplaceRotation(&play->billboardMtxF);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, *dList);

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void DrawEnFirefly_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* firefly) {
    static Color_RGBA8 auraPrimColor[2] = { { 255, 255, 100, 255 }, { 100, 200, 255, 255 } };
    static Color_RGBA8 auraEnvColor[2] = { { 255, 50, 0, 0 }, { 0, 0, 255, 0 } };
    static uint32_t dustUpdate = 0;
    static bool auraColor = false;
    static Vec3f auraVelocity = { 0, 0.5f, 0 };
    static Vec3f auraAccel = { 0, 0.5f, 0 };
    static Vec3f auraPos;
    if (firefly != NULL) {
        auraPos = firefly->world.pos;
    }

    Matrix_MultZero(&auraPos);
    auraPos.x += MM_Rand_ZeroOne() * 0.5f;
    auraPos.y += MM_Rand_ZeroOne() * 0.5f;
    auraPos.z += MM_Rand_ZeroOne() * 0.5f;

    if (MM_gPlayState != NULL && dustUpdate != MM_gPlayState->state.frames) {
        if (dustUpdate == MM_gPlayState->state.frames - 20) {
            dustUpdate = MM_gPlayState->state.frames;
            auraColor = !auraColor;
        }
    }

    if (limbIndex == FIRE_KEESE_LIMB_HEAD) {
        Gfx* gfx = play->state.gfxCtx->polyXlu.p;
        Scene_SetRenderModeXlu(play, 1, 2);
        MATRIX_FINALIZE_AND_LOAD(gfx++, play->state.gfxCtx);
        gSPDisplayList(gfx++, (Gfx*)&gKeeseRedEyesDL);
    }
    if (limbIndex == FIRE_KEESE_LIMB_LEFT_WING_END || limbIndex == FIRE_KEESE_LIMB_RIGHT_WING_END_ROOT) {
        MM_EffectSsDust_Spawn(MM_gPlayState, 2, &auraPos, &auraVelocity, &auraAccel, &auraPrimColor[auraColor],
                           &auraEnvColor[auraColor], 100, -40, 3, 0);
    }
}

void DrawEnRealBombchu_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* rat) {
    if (limbIndex == REAL_BOMBCHU_LIMB_TAIL_END) {
        OPEN_DISPS(play->state.gfxCtx);
        MM_Matrix_ReplaceRotation(&play->billboardMtxF);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)&gBombCapDL);
        MM_Matrix_RotateZYX(0x4000, 0, 0, MTXMODE_APPLY);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 80, 255);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 10, 0, 40, 255);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)&gBombBodyDL);
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

s32 DrawEnSkb_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    s16 MM_sins;
    if (limbIndex == STALCHILD_LIMB_HEAD) {
        OPEN_DISPS(play->state.gfxCtx);

        MM_sins = fabsf(MM_Math_SinS(play->gameplayFrames * 6000) * 95.0f) + 160.0f;

        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, MM_sins, MM_sins, MM_sins, 255);

        CLOSE_DISPS(play->state.gfxCtx);
    }
    return false;
}

void DrawEnIk_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    if (limbIndex == IRON_KNUCKLE_LIMB_HELMET_ARMOR) {
        OPEN_DISPS(play->state.gfxCtx);

        Gfx* xlu = POLY_XLU_DISP;

        MATRIX_FINALIZE_AND_LOAD(&xlu[0], play->state.gfxCtx);
        gSPDisplayList(&xlu[1], (Gfx*)gIronKnuckleHelmetMarkingDL);
        POLY_XLU_DISP = &xlu[2];

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void EnKaizoku_TransformLimbDraw(PlayState* play, s32 limbIndex, Actor* thisx) {
    // Even if this does nothing, it must exist, as TransformLimbDrawOpa is not null checked before invocation.
}

// Enemy Soul Draw Functions
extern void DrawAlien() {
    SETUP_DRAW(ALIEN_LIMB_MAX);
    static uintptr_t eyeTexture = (uintptr_t)Lib_SegmentedToVirtual((TexturePtr)gAlienEyeTex);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.007f, 0.007f, 0.007f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(ALIEN_LIMB_MAX, gAlienSkel, gAlienFloatAnim);

    gSPSegment(POLY_OPA_DISP++, 0x08, eyeTexture);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 10, 138, 46 }, { 30.0f, 30.0f, 30.0f });
}

extern void DrawArmos() {
    SETUP_DRAW(OBJECT_AM_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -3100, 0, MTXMODE_APPLY);
    SETUP_SKEL(OBJECT_AM_LIMB_MAX, object_am_Skel_005948, gArmosHopAnim);

    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawBat() {
    static u32 lastUpdate = 0;
    static u32 wingAnim = 0;

    OPEN_DISPS(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);

    static Gfx* sWingsDLs[] = {
        (Gfx*)&gBadBatWingsFrame0DL, (Gfx*)&gBadBatWingsFrame1DL, (Gfx*)&gBadBatWingsFrame2DL,
        (Gfx*)&gBadBatWingsFrame3DL, (Gfx*)&gBadBatWingsFrame4DL, (Gfx*)&gBadBatWingsFrame5DL,
        (Gfx*)&gBadBatWingsFrame6DL, (Gfx*)&gBadBatWingsFrame7DL, (Gfx*)&gBadBatWingsFrame8DL,
    };

    Gfx* gfx = POLY_OPA_DISP;

    if (MM_gPlayState != NULL && lastUpdate != MM_gPlayState->state.frames) {
        lastUpdate = MM_gPlayState->state.frames;
        if (wingAnim == 8) {
            wingAnim = 0;
        } else {
            wingAnim++;
        }
    }

    gSPDisplayList(&gfx[0], gSetupDLs[SETUPDL_25]);
    MATRIX_FINALIZE_AND_LOAD(&gfx[1], MM_gPlayState->state.gfxCtx);
    gSPDisplayList(&gfx[2], (Gfx*)&gBadBatSetupDL);
    gSPDisplayList(&gfx[3], (Gfx*)&gBadBatBodyDL);
    gSPDisplayList(&gfx[4], sWingsDLs[wingAnim]);

    POLY_OPA_DISP = &gfx[5];

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 6.0f, 6.0f, 6.0f });
}

extern void DrawBeamos() {
    SETUP_DRAW(BEAMOS_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -3200, 0, MTXMODE_APPLY);
    SETUP_SKEL(BEAMOS_LIMB_MAX, gBeamosSkel, gBeamosAnim);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawBoe() {
    static Color_RGBA8 D_80A4F7C4 = { 0, 0, 0, 255 };

    OPEN_DISPS(MM_gPlayState->state.gfxCtx);

    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -1200, 0, MTXMODE_APPLY);

    gSPDisplayList(POLY_OPA_DISP++, gSetupDLs[SETUPDL_25]);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0xFF, 0, 0, 0, 255);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)D_801AEFA0);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, MM_gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBlackBoeEndDL);

    gSPDisplayList(POLY_XLU_DISP++, gSetupDLs[SETUPDL_25]);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBlackBoeBodyMaterialDL);
    MM_Matrix_ReplaceRotation(&MM_gPlayState->billboardMtxF);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, MM_gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBlackBoeBodyModelDL);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0xFF, 245, 97, 0, 255);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBlackBoeEyesDL);
    MM_Matrix_Scale(0.009f, 0.009f, 0.009f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, MM_gPlayState->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0xFF, 245, 214, 0, 255);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gBlackBoeEyesDL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 1000.0f, 1000.0f, 1000.0f });
}

extern void DrawBubble() {
    SETUP_DRAW(BUBBLE_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SETUP_SKEL(BUBBLE_LIMB_MAX, gBubbleSkel, gBubbleFlyingAnim);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawCaptainKeeta() {
    SETUP_DRAW(OBJECT_BSB_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -3500.0f, 0, MTXMODE_APPLY);
    SETUP_SKEL(OBJECT_BSB_LIMB_MAX, object_bsb_Skel_00C3E0, object_bsb_Anim_004894);

    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 255, 192, 0 }, { 5.0f, 10.0f, 5.0f });
}

extern void DrawChuchu() {
    static int16_t timer = 25;
    f32 timerFactor = MM_sqrtf(timer) * 0.2f;
    static AnimatedMaterial* sSlimeTexAnim = (AnimatedMaterial*)Lib_SegmentedToVirtual((void*)gChuchuSlimeFlowTexAnim);

    OPEN_DISPS(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(
        0.01f,
        ((((MM_coss(RAD_TO_BINANG(timer * (2.0f * M_PI / 5.0f))) * SHT_MINV) * (0.07f * timerFactor)) + 1.0f) * 0.01f),
        0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -2700.0f, 0, MTXMODE_APPLY);

    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    AnimatedMat_Draw(MM_gPlayState, sSlimeTexAnim);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 100, 255, 255, 200, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 180, 0, 255);

    if (timer == 0) {
        timer = 25;
    }

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, MM_gPlayState->state.gfxCtx);
    Scene_SetRenderModeXlu(MM_gPlayState, 1, 2);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gChuchuBodyDL);
    gSPSegment(POLY_XLU_DISP++, 9, (uintptr_t)gChuchuEyeOpenTex);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gChuchuEyesDL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
    timer--;
}

extern void DrawDeathArmos() {
    SETUP_DRAW(FAMOS_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    AnimatedMat_Draw(MM_gPlayState, (AnimatedMaterial*)gFamosNormalGlowingEmblemTexAnim);
    MM_Matrix_Scale(0.008f, 0.008f, 0.008f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -4100, 0, MTXMODE_APPLY);
    SETUP_SKEL(FAMOS_LIMB_MAX, gFamosSkel, gFamosIdleAnim);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawDeepPython() {
    SETUP_DRAW(DEEP_PYTHON_LIMB_MAX);
    MM_Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(DEEP_PYTHON_LIMB_MAX, gDeepPythonSkel, gDeepPythonUnusedSideSwayAnim);

    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawDekuBaba() {
    SETUP_DRAW(DEKUBABA_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SETUP_SKEL(DEKUBABA_LIMB_MAX, gDekuBabaSkel, gDekuBabaFastChompAnim);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 6.0f, 6.0f, 6.0f });
}

extern void DrawDexihand() {
    SETUP_DRAW(DEXIHAND_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(DEXIHAND_LIMB_MAX, gDexihandSkel, gDexihandIdleAnim);

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 70 }, { 6.0f, 6.0f, 6.0f });
}

extern void DrawDinolfos() {
    static uintptr_t eyeTexture = (uintptr_t)Lib_SegmentedToVirtual((TexturePtr)gDinolfosEyeOpenTex);
    SETUP_DRAW(DINOLFOS_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.014f, 0.014f, 0.014f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -2200.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(DINOLFOS_LIMB_MAX, gDinolfosSkel, gDinolfosIdleAnim);

    Scene_SetRenderModeXlu(MM_gPlayState, 0, 1);
    gSPSegment(POLY_OPA_DISP++, 0x08, eyeTexture);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawDodongo() {
    SETUP_DRAW(OBJECT_DODONGO_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -1500.0f, 0, MTXMODE_APPLY);
    SETUP_SKEL(OBJECT_DODONGO_LIMB_MAX, object_dodongo_Skel_008318, object_dodongo_Anim_004C20);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawDragonfly() {
    SETUP_DRAW(DRAGONFLY_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -700.0f, 0, MTXMODE_APPLY);
    SETUP_SKEL(DRAGONFLY_LIMB_MAX, gDragonflySkel, gDragonflyFlyAnim);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawEeno() {
    SETUP_DRAW(EENO_LIMB_MAX);
    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -3000.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(EENO_LIMB_MAX, gEenoSkel, gEenoIdleAnim);

    Scene_SetRenderModeXlu(MM_gPlayState, 0, 1);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 35 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawEyegore() {
    static AnimatedMaterial* sEyegoreEyeLaserTexAnim =
        (AnimatedMaterial*)Lib_SegmentedToVirtual((void*)gEyegoreEyeLaserTexAnim);
    SETUP_DRAW(EYEGORE_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -4000.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(EYEGORE_LIMB_MAX, gEyegoreSkel, gEyegoreUnusedWalkAnim);

    AnimatedMat_Draw(MM_gPlayState, sEyegoreEyeLaserTexAnim);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);
    POLY_OPA_DISP = MM_Play_SetFog(MM_gPlayState, POLY_OPA_DISP);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 192, 192, 64 }, { 20.0f, 20.0f, 20.0f });
}

extern void DrawFlyingPot() {
    OPEN_DISPS(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.3f, 0.3f, 0.3f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -100.0f, 0, MTXMODE_APPLY);

    MM_Gfx_DrawDListOpa(MM_gPlayState, (Gfx*)gameplay_dangeon_keep_DL_017EA0);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 0.4f, 0.4f, 0.4f });
}

extern void DrawFreezard() {
    OPEN_DISPS(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -4100.0f, 0, MTXMODE_APPLY);
    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);

    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)MM_Gfx_TwoTexScroll(MM_gPlayState->state.gfxCtx, 0, 0, MM_gPlayState->state.frames % 128, 0x20, 0x20,
                                           1, 0, (MM_gPlayState->state.frames * 2) % 128, 0x20, 0x20));
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, MM_gPlayState->state.gfxCtx);
    gDPSetCombineLERP(POLY_XLU_DISP++, TEXEL1, PRIMITIVE, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIMITIVE, TEXEL0,
                      PRIMITIVE, ENVIRONMENT, COMBINED, ENVIRONMENT, COMBINED, 0, ENVIRONMENT, 0);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 155, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 200, 200, 200, 255);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)object_fz_DL_001130);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 20.0f, 20.0f, 20.0f });
}

extern void DrawGaro() {
    SETUP_DRAW(GARO_LIMB_MAX);
    MM_Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    SETUP_FLEX_SKEL(GARO_LIMB_MAX, gGaroSkel, gGaroIdleAnim);

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 150, 255, 150 }, { 8.0f, 8.0f, 8.0f });
}

extern void DrawGekko() {
    SETUP_DRAW(GEKKO_LIMB_MAX);
    MM_Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -4100.0f, 0, MTXMODE_APPLY);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    SETUP_FLEX_SKEL(GEKKO_LIMB_MAX, gGekkoSkel, gGekkoBoxingStanceAnim);

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 150, 100, 255 }, { 20.0f, 20.0f, 20.0f });
}

extern void DrawGiantBee() {
    SETUP_DRAW(OBJECT_BEE_LIMB_MAX);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    SETUP_SKEL(OBJECT_BEE_LIMB_MAX, gBeeSkel, gBeeFlyingAnim);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawGomess() {
    static AnimatedMaterial* bodyMatAnim = (AnimatedMaterial*)Lib_SegmentedToVirtual((void*)&gGomessBodyMatAnim);
    static AnimatedMaterial* coreMatAnim = (AnimatedMaterial*)Lib_SegmentedToVirtual((void*)&gGomessCoreMatAnim);
    SETUP_DRAW(GOMESS_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(GOMESS_LIMB_MAX, gGomessSkel, gGomessFloatAnim);

    AnimatedMat_DrawStepOpa(MM_gPlayState, bodyMatAnim, 23);
    AnimatedMat_DrawOpa(MM_gPlayState, coreMatAnim);
    Scene_SetRenderModeXlu(MM_gPlayState, 0, 1);
    gDPSetEnvColor(POLY_OPA_DISP++, 30, 30, 0, 255);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 0, 0 }, { 15.0f, 15.0f, 15.0f });
}

extern void DrawGuay() {
    SETUP_DRAW(OBJECT_CROW_LIMB_MAX);
    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(OBJECT_CROW_LIMB_MAX, gGuaySkel, gGuayFlyAnim);

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 6.0f, 6.0f, 6.0f });
}

extern void DrawHiploop() {
    SETUP_DRAW(HIPLOOP_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -1400.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(HIPLOOP_LIMB_MAX, gHiploopSkel, gHiploopChargeAnim);

    Scene_SetRenderModeXlu(MM_gPlayState, 0, 1);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawIgosDuIkana() {
    SETUP_DRAW(IGOS_LIMB_MAX);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)EnKnight_BuildEmptyDL(MM_gPlayState->state.gfxCtx));
    gSPSegment(POLY_XLU_DISP++, 0x0A, (uintptr_t)EnKnight_BuildEmptyDL(MM_gPlayState->state.gfxCtx));
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)EnKnight_BuildEmptyDL(MM_gPlayState->state.gfxCtx));
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -2000.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(IGOS_LIMB_MAX, gIgosSkel, gKnightIdleAnim);

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);
    POLY_OPA_DISP = MM_Play_SetFog(MM_gPlayState, POLY_OPA_DISP);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 0, 0, 0 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawIronKnuckle() {
    SETUP_DRAW(IRON_KNUCKLE_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -2900.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(IRON_KNUCKLE_LIMB_MAX, gIronKnuckleSkel, gIronKnuckleWalkAnim);

    Gfx* gfx = POLY_XLU_DISP;
    gSPDisplayList(&gfx[0], gSetupDLs[SETUPDL_25]);
    POLY_XLU_DISP = &gfx[1];
    gfx = POLY_OPA_DISP;
    gSPDisplayList(&gfx[0], gSetupDLs[SETUPDL_25]);
    gSPSegment(&gfx[1], 0x08, (uintptr_t)gIronKnuckleBlackArmorMaterialDL);
    gSPSegment(&gfx[2], 0x09, (uintptr_t)gIronKnuckleBrownArmorMaterialDL);
    gSPSegment(&gfx[3], 0x0A, (uintptr_t)gIronKnuckleBrownArmorMaterialDL);
    POLY_OPA_DISP = &gfx[4];

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL,
                          DrawEnIk_PostLimbDraw, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 12.0f, 12.0f, 12.0f });
}

extern void DrawKeese() {
    SETUP_DRAW(FIRE_KEESE_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -700.0f, 0, MTXMODE_APPLY);
    SETUP_SKEL(FIRE_KEESE_LIMB_MAX, gFireKeeseSkel, gFireKeeseFlyAnim);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, DrawEnFirefly_PostLimbDraw, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawLeever() {
    SETUP_DRAW(LEEVER_LIMB_MAX);
    // The Leever animation already spins in the same direction as the Get Item animation, which looks really fast.
    // Reverse the animation so that it spins more slowly.
    skelAnime.playSpeed = -1.0f;
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -700.0f, 0, MTXMODE_APPLY);
    SETUP_SKEL(LEEVER_LIMB_MAX, gLeeverSkel, gLeeverSpinAnim);

    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x01, 255, 255, 255, 255);
    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 3.0f, 3.0f, 3.0f });
}

extern void DrawLikeLike() {
    static bool initialized = false;
    static u32 lastUpdate = 0;
    static s16 textureScroll = 0;
    static struct {
        f32 unk_08;
        f32 unk_10;
        f32 unk_00;
        Vec3s unk_1A;
    } segments[5];

    if (!initialized) {
        initialized = true;
        for (int i = 0; i < 5; i++) {
            segments[i].unk_08 = 0.8f;
            segments[i].unk_10 = 0.0f;
            segments[i].unk_00 = 0.0f;
            segments[i].unk_1A.x = 0;
            segments[i].unk_1A.y = 0;
            segments[i].unk_1A.z = 0;
        }
    }

    if (MM_gPlayState != NULL && lastUpdate != MM_gPlayState->state.frames) {
        lastUpdate = MM_gPlayState->state.frames;
        textureScroll++;

        f32 phase = MM_gPlayState->state.frames * (2500.0f * (2.0f * M_PI / 65536.0f));

        for (int j = 0; j < 5; j++) {
            f32 segmentPhase = phase + (j * 0x4000) * (2.0f * M_PI / 65536.0f);
            segments[j].unk_10 = cosf(segmentPhase) * 0.15f;
            segments[j].unk_00 = 0.0f;
        }

        for (int j = 1; j < 5; j++) {
            segments[j].unk_1A.x = (s16)(cosf(phase + (j * 0x3000) * (2.0f * M_PI / 65536.0f)) * 2048.0f);
            segments[j].unk_1A.z = (s16)(sinf(phase + (j * 0x1000) * (2.0f * M_PI / 65536.0f)) * 2048.0f);
        }
    }

    Mtx* mtx = (Mtx*)GRAPH_ALLOC(MM_gPlayState->state.gfxCtx, 4 * sizeof(Mtx));
    s32 i;
    f32 temp_f20;

    OPEN_DISPS(MM_gPlayState->state.gfxCtx);

    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -3000.0f, 0, MTXMODE_APPLY);
    MM_Matrix_Push();

    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)mtx);
    gSPSegment(POLY_OPA_DISP++, 0x08,
               (uintptr_t)MM_Gfx_TwoTexScroll(MM_gPlayState->state.gfxCtx, 0, 0, 0, 0x20, 0x10, 1, (textureScroll * 0) & 0x3F,
                                           (textureScroll * -6) & 0x7F, 0x20, 0x10));

    MM_Matrix_Push();
    MM_Matrix_Scale((1.0f + segments[0].unk_10) * segments[0].unk_08, 1.0f,
                 (1.0f + segments[0].unk_10) * segments[0].unk_08, MTXMODE_APPLY);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, MM_gPlayState->state.gfxCtx);

    MM_Matrix_Pop();

    for (i = 1; i < 5; i++) {
        temp_f20 = segments[i].unk_08 * (segments[i].unk_10 + 1.0f);

        MM_Matrix_Translate(0.0f, segments[i].unk_00 + 1000.0f, 0.0f, MTXMODE_APPLY);
        MM_Matrix_RotateZYX(segments[i].unk_1A.x, segments[i].unk_1A.y, segments[i].unk_1A.z, MTXMODE_APPLY);
        MM_Matrix_Push();
        MM_Matrix_Scale(temp_f20, 1.0f, temp_f20, MTXMODE_APPLY);
        MM_Matrix_ToMtx(mtx);
        MM_Matrix_Pop();
        mtx++;
    }

    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gLikeLikeDL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Pop();
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawMadScrub() {
    SETUP_DRAW(DEKU_SCRUB_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -2300, 0, MTXMODE_APPLY);
    SETUP_SKEL(DEKU_SCRUB_LIMB_MAX, gDekuScrubSkel, gDekuScrubLookAroundAnim);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawNejiron() {
    SETUP_DRAW(NEJIRON_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
    SETUP_SKEL(NEJIRON_LIMB_MAX, gNejironSkel, gNejironIdleAnim);

    gSPSegment(POLY_OPA_DISP++, 8, (uintptr_t)gNejironEyeOpenTex);
    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 13.0f, 13.0f, 13.0f });
}

extern void DrawOctorok() {
    SETUP_DRAW(OCTOROK_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.007f, 0.007f, 0.007f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -700.0f, 0, MTXMODE_APPLY);
    SETUP_SKEL(OCTOROK_LIMB_MAX, gOctorokSkel, gOctorokFloatAnim);

    Gfx* gfxPtr = POLY_OPA_DISP;
    gSPDisplayList(&gfxPtr[0], gSetupDLs[SETUPDL_25]);
    gSPSegment(&gfxPtr[1], 0x08, (uintptr_t)D_801AEFA0);
    POLY_OPA_DISP = &gfxPtr[2];
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x01, 255, 255, 255, 255);
    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawPeahat() {
    SETUP_DRAW(OBJECT_PH_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    SETUP_SKEL(OBJECT_PH_LIMB_MAX, object_ph_Skel_001C80, object_ph_Anim_0009C4);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawPirate() {
    static uintptr_t eyeTexture = (uintptr_t)Lib_SegmentedToVirtual((TexturePtr)gFighterPirateEyeOpenTex);
    SETUP_DRAW(KAIZOKU_LIMB_MAX);
    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -2000.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(KAIZOKU_LIMB_MAX, gFighterPirateSkel, gFighterPirateFightingIdleAnim);

    gSPSegment(POLY_OPA_DISP++, 0x08, eyeTexture);
    SkelAnime_DrawTransformFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL,
                                   NULL, EnKaizoku_TransformLimbDraw, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawPoe() {
    SETUP_DRAW(POE_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.0075f, 0.0075f, 0.0075f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -5000.0f, 0, MTXMODE_APPLY);
    SETUP_SKEL(POE_LIMB_MAX, gPoeSkel, gPoeFloatAnim);

    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)D_801AEFA0);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawRealBombchu() {
    SETUP_DRAW(REAL_BOMBCHU_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL60_XluNoCD(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -1500.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(REAL_BOMBCHU_LIMB_MAX, gRealBombchuSkel, gRealBombchuRunAnim);

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL,
                          DrawEnRealBombchu_PostLimbDraw, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 6.0f, 6.0f, 6.0f });
}

extern void DrawRedead() {
    static u32 animUpdate = 0;
    static uint32_t rdAnimID = 0;
    static AnimationHeader* currentAnim = (AnimationHeader*)gGibdoRedeadIdleAnim;
    static std::vector<AnimationHeader*> rdAnims = {
        (AnimationHeader*)gGibdoRedeadSquattingDanceAnim,
        (AnimationHeader*)gGibdoRedeadClappingDanceAnim,
        (AnimationHeader*)gGibdoRedeadPirouetteAnim,
    };
    SETUP_DRAW(REDEAD_LIMB_MAX);

    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL60_XluNoCD(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -2900.0f, 0, MTXMODE_APPLY);

    if (!initialized) {
        initialized = true;
        MM_SkelAnime_InitFlex(MM_gPlayState, &skelAnime, (FlexSkeletonHeader*)&gRedeadSkel,
                           (AnimationHeader*)gGibdoRedeadPirouetteAnim, jointTable, morphTable, REDEAD_LIMB_MAX);
    }

    if (MM_gPlayState != NULL && lastUpdate != MM_gPlayState->state.frames) {
        if (EnRd_ShouldNotDance(MM_gPlayState)) {
            currentAnim = (AnimationHeader*)gGibdoRedeadIdleAnim;
            MM_Animation_MorphToLoop(&skelAnime, currentAnim, -6.0f);
        } else if (animUpdate != MM_gPlayState->state.frames && animUpdate <= MM_gPlayState->state.frames - 35) {
            animUpdate = MM_gPlayState->state.frames;
            currentAnim = rdAnims[rdAnimID];
            if (rdAnimID >= rdAnims.size() - 1) {
                rdAnimID = 0;
            } else {
                rdAnimID++;
            }
            MM_Animation_MorphToLoop(&skelAnime, currentAnim, -6.0f);
        }
        lastUpdate = MM_gPlayState->state.frames;
        MM_SkelAnime_Update(&skelAnime);
    }

    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)D_801AEFA0);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawShellBlade() {
    SETUP_DRAW(OBJECT_SB_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.007f, 0.007f, 0.007f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -3500.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(OBJECT_SB_LIMB_MAX, object_sb_Skel_002BF0, object_sb_Anim_000194);

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawSkullfish() {
    SETUP_DRAW(OBJECT_PR_2_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(OBJECT_PR_2_LIMB_MAX, object_pr_Skel_004188, object_pr_Anim_004340);

    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    Scene_SetRenderModeXlu(MM_gPlayState, 0, 1);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 5.0f, 5.0f, 5.0f });
}

extern void DrawSkulltula() {
    SETUP_DRAW(OBJECT_ST_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
    SETUP_SKEL(OBJECT_ST_LIMB_MAX, object_st_Skel_005298, object_st_Anim_000304);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 5.0f, 5.0f, 5.0f });
}

extern void DrawSnapper() {
    SETUP_DRAW(SNAPPER_LIMB_MAX);
    static uintptr_t eyeTexture = (uintptr_t)Lib_SegmentedToVirtual((TexturePtr)gSnapperEyeOpenTex);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -3100.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(SNAPPER_LIMB_MAX, gSnapperSkel, gSnapperIdleAnim);

    gSPSegment(POLY_OPA_DISP++, 0x08, eyeTexture);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawStalchild() {
    SETUP_DRAW(STALCHILD_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -3200, 0, MTXMODE_APPLY);
    SETUP_SKEL(STALCHILD_LIMB_MAX, gStalchildSkel, gStalchildIdleAnim);

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, DrawEnSkb_OverrideLimbDraw, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawTakkuri() {
    SETUP_DRAW(TAKKURI_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(TAKKURI_LIMB_MAX, gTakkuriSkel, gTakkuriFlyAnim);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);
    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawTektite() {
    static TexturePtr D_80896B24[2][3] = {
        { (TexturePtr*)&object_tite_Tex_001300, (TexturePtr*)&object_tite_Tex_001700,
          (TexturePtr*)&object_tite_Tex_001900 },
        { (TexturePtr*)&object_tite_Tex_001B00, (TexturePtr*)&object_tite_Tex_001F00,
          (TexturePtr*)&object_tite_Tex_002100 },
    };
    SETUP_DRAW(OBJECT_TITE_LIMB_MAX);

    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -2900.0f, 0, MTXMODE_APPLY);
    SETUP_SKEL(OBJECT_TITE_LIMB_MAX, object_tite_Skel_003A20, object_tite_Anim_0012E4);

    Gfx* gfx = POLY_OPA_DISP;

    gSPDisplayList(&gfx[0], gSetupDLs[SETUPDL_25]);

    gSPSegment(&gfx[1], 0x08, (uintptr_t)D_80896B24[0][0]);
    gSPSegment(&gfx[2], 0x09, (uintptr_t)D_80896B24[0][1]);
    gSPSegment(&gfx[3], 0x0A, (uintptr_t)D_80896B24[0][2]);

    POLY_OPA_DISP = &gfx[4];

    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawWallmaster() {
    SETUP_DRAW(WALLMASTER_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -3500.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(WALLMASTER_LIMB_MAX, gWallmasterSkel, gWallmasterIdleAnim);

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawWart() {
    SETUP_DRAW(WART_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(WART_LIMB_MAX, gWartSkel, gWartIdleAnim);

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

extern void DrawWizrobe() {
    static uintptr_t eyeTexture = (uintptr_t)Lib_SegmentedToVirtual((TexturePtr)gWizrobeEyeTex);
    SETUP_DRAW(WIZROBE_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    MM_Matrix_Scale(0.006f, 0.006f, 0.006f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(WIZROBE_LIMB_MAX, gWizrobeSkel, gWizrobeIdleAnim);

    Scene_SetRenderModeXlu(MM_gPlayState, 0, 1);
    gSPSegment(POLY_OPA_DISP++, 0x08, eyeTexture);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 15.0f, 15.0f, 15.0f });
}

extern void DrawWolfos() {
    SETUP_DRAW(WOLFOS_NORMAL_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    MM_Matrix_Translate(0, -3000.0f, 0, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(WOLFOS_NORMAL_LIMB_MAX, gWolfosNormalSkel, gWolfosWaitAnim);

    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)&gWolfosNormalEyeOpenTex);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 155, 155, 155 }, { 10.0f, 10.0f, 10.0f });
}

// Boss Souls
extern void DrawGoht() {
    SETUP_DRAW(GOHT_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    MM_Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(GOHT_LIMB_MAX, gGohtSkel, gGohtRunAnim);

    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gGohtMetalPlateWithCirclePatternTex);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 10, 138, 46 }, { 30.0f, 30.0f, 30.0f });
}

extern void DrawGyorg() {
    SETUP_DRAW(GYORG_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    MM_Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(GYORG_LIMB_MAX, gGyorgSkel, gGyorgGentleSwimmingAnim);

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 19, 99, 165 }, { 3.0f, 3.0f, 3.0f });
}

extern void DrawOdolwa() {
    SETUP_DRAW(ODOLWA_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    MM_Matrix_Scale(0.005f, 0.005f, 0.005f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(ODOLWA_LIMB_MAX, gOdolwaSkel, gOdolwaReadyAnim);

    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 145, 20, 133 }, { 25.0f, 25.0f, 25.0f });
}

extern void DrawTwinmold() {
    SETUP_DRAW(TWINMOLD_HEAD_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.06f, 0.06f, 0.06f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(TWINMOLD_HEAD_LIMB_MAX, gTwinmoldHeadSkel, gTwinmoldHeadFlyAnim);

    Mtx* mtxHead = (Mtx*)GRAPH_ALLOC(MM_gPlayState->state.gfxCtx, 23 * sizeof(Mtx));
    gSPSegment(POLY_OPA_DISP++, 0x0D, (uintptr_t)mtxHead);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gTwinmoldBlueSkinTex);
    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 168, 180, 20 }, { 3.0f, 3.0f, 3.0f });
}

extern void DrawMajora() {
    SETUP_DRAW(MAJORAS_MASK_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
    MM_Matrix_ReplaceRotation(&MM_gPlayState->billboardMtxF);
    SETUP_SKEL(MAJORAS_MASK_LIMB_MAX, gMajorasMaskSkel, gMajorasMaskFloatingAnim);

    gSPSegment(POLY_OPA_DISP++, 8, (uintptr_t)gMajorasMaskWithNormalEyesTex);
    MM_SkelAnime_DrawOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, NULL, NULL, NULL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gMajorasMaskTentacleMaterialDL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
    DrawEnLight({ 232, 128, 21 }, { 3.0f, 3.0f, 3.0f });
}

// Other Actors
extern void DrawMinifrog(RandoItemId randoItemId, Actor* actor) {
    SETUP_DRAW(FROG_LIMB_MAX);
    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Translate(0.0f, -20.0f, 0.0f, MTXMODE_APPLY);
    MM_Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
    SETUP_FLEX_SKEL(FROG_LIMB_MAX, gFrogSkel, gFrogIdleAnim);
    Color_RGBA8 envColor = { 200, 170, 0, 255 }; // FROG_YELLOW

    switch (randoItemId) {
        case RI_FROG_BLUE:
            envColor = { 120, 130, 230, 255 };
            break;
        case RI_FROG_CYAN:
            envColor = { 0, 170, 200, 255 };
            break;
        case RI_FROG_PINK:
            envColor = { 210, 120, 100, 255 };
            break;
        case RI_FROG_WHITE:
            envColor = { 190, 190, 190, 255 };
            break;
    }

    gDPSetEnvColor(POLY_OPA_DISP++, envColor.r, envColor.g, envColor.b, envColor.a);

    Mtx* mtxHead = (Mtx*)GRAPH_ALLOC(MM_gPlayState->state.gfxCtx, 23 * sizeof(Mtx));
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)gFrogIrisOpenTex);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)gFrogIrisOpenTex);
    MM_SkelAnime_DrawFlexOpa(MM_gPlayState, skelAnime.skeleton, skelAnime.jointTable, FROG_LIMB_MAX,
                          EnMinifrog_OverrideLimbDraw, EnMinifrogPostLimbDraw, actor);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
}

extern void DrawClock(RandoItemId randoItemId, Actor* actor) {
    OPEN_DISPS(MM_gPlayState->state.gfxCtx);

    ObjTokeidai* clockActor = (ObjTokeidai*)actor;
    static u32 lastUpdate = 0;
    static f32 yTranslation = 0;
    static f32 xRotation = 0;
    static int16_t minuteRingOrExteriorGearRotation = 0;
    static f32 clockFaceZTranslation = 0;
    static int16_t clockFaceRotation = 0;
    static int16_t sunMoonPanelRotation = 0;

    switch (randoItemId) {
        case RI_TIME_DAY_1:
        case RI_TIME_DAY_2:
        case RI_TIME_DAY_3:
            clockFaceRotation = 0xC000;
            sunMoonPanelRotation = 0;
            break;
        case RI_TIME_NIGHT_1:
        case RI_TIME_NIGHT_2:
        case RI_TIME_NIGHT_3:
            clockFaceRotation = 0;
            sunMoonPanelRotation = 0x8000;
            break;
        case RI_TIME_PROGRESSIVE:
            clockFaceRotation = gSaveContext.save.isNight ? 0 : 0xC000;
            sunMoonPanelRotation = gSaveContext.save.isNight ? 0x8000 : 0;
            break;
    }

    if (clockActor != nullptr && clockActor->actor.id == ACTOR_OBJ_TOKEIDAI) {
        clockActor->clockTime = gSaveContext.save.time;

        if (MM_gPlayState != NULL && lastUpdate != MM_gPlayState->state.frames) {
            lastUpdate = MM_gPlayState->state.frames;
            ObjTokeidai_RotateOnMinuteChange(clockActor, true);
            ObjTokeidai_RotateOnHourChange(clockActor, MM_gPlayState);
            yTranslation = clockActor->yTranslation;
            xRotation = clockActor->xRotation;
            minuteRingOrExteriorGearRotation = clockActor->minuteRingOrExteriorGearRotation;
            clockFaceZTranslation = clockActor->clockFaceZTranslation;
            clockFaceRotation = clockActor->clockFaceRotation;
            sunMoonPanelRotation = clockActor->sunMoonPanelRotation;
        }
    }

    Gfx_SetupDL25_Opa(MM_gPlayState->state.gfxCtx);
    MM_Matrix_Translate(0.0f, yTranslation, 0.0f, MTXMODE_APPLY);
    MM_Matrix_Scale(0.015f, 0.015f, 0.015f, MTXMODE_APPLY);
    MM_Matrix_Translate(0.0f, 0.0f, -1791.0f, MTXMODE_APPLY);
    Matrix_RotateXS(-xRotation, MTXMODE_APPLY);
    MM_Matrix_Translate(0.0f, 0.0f, 1791.0f, MTXMODE_APPLY);

    MM_Matrix_Push();
    Matrix_RotateZS(-minuteRingOrExteriorGearRotation, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, MM_gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gClockTowerMinuteRingDL);
    MM_Matrix_Pop();

    MM_Matrix_Translate(0.0f, 0.0f, clockFaceZTranslation, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, MM_gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gClockTowerClockCenterAndHandDL);

    Matrix_RotateZS(-clockFaceRotation * 2, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, MM_gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gClockTowerClockFaceDL);

    MM_Matrix_Translate(0.0f, -1112.0f, -19.6f, MTXMODE_APPLY);
    Matrix_RotateYS(sunMoonPanelRotation, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, MM_gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gClockTowerSunAndMoonPanelDL);

    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
}
