/*
 * File: z_oceff_wipe3.c
 * Overlay: ovl_Oceff_Wipe3
 * Description: Unused OoT Saria's Song Ocarina Effect
 */

#include "prevent_bss_reordering.h"
#include "z_oceff_wipe3.h"
#include "BenPort.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA)

void MM_OceffWipe3_Init(Actor* thisx, PlayState* play);
void MM_OceffWipe3_Destroy(Actor* thisx, PlayState* play);
void MM_OceffWipe3_Update(Actor* thisx, PlayState* play);
void MM_OceffWipe3_Draw(Actor* thisx, PlayState* play);

ActorProfile Oceff_Wipe3_Profile = {
    ACTOR_OCEFF_WIPE3,
    ACTORCAT_ITEMACTION,
    FLAGS,
    GAMEPLAY_KEEP,
    sizeof(OceffWipe3),
    (ActorFunc)MM_OceffWipe3_Init,
    (ActorFunc)MM_OceffWipe3_Destroy,
    (ActorFunc)MM_OceffWipe3_Update,
    (ActorFunc)MM_OceffWipe3_Draw,
};

#include "assets/overlays/ovl_Oceff_Wipe3/ovl_Oceff_Wipe3.h"

static s32 sBssPad;

void MM_OceffWipe3_Init(Actor* thisx, PlayState* play) {
    OceffWipe3* this = (OceffWipe3*)thisx;

    MM_Actor_SetScale(&this->actor, 0.1f);
    this->counter = 0;
    this->actor.world.pos = GET_ACTIVE_CAM(play)->eye;
}

void MM_OceffWipe3_Destroy(Actor* thisx, PlayState* play) {
    OceffWipe3* this = (OceffWipe3*)thisx;

    MM_Magic_Reset(play);
    play->msgCtx.ocarinaSongEffectActive = false;
}

void MM_OceffWipe3_Update(Actor* thisx, PlayState* play) {
    OceffWipe3* this = (OceffWipe3*)thisx;

    this->actor.world.pos = GET_ACTIVE_CAM(play)->eye;
    if (this->counter < 100) {
        this->counter++;
    } else {
        MM_Actor_Kill(&this->actor);
    }
}

void MM_OceffWipe3_Draw(Actor* thisx, PlayState* play) {
    u32 scroll = play->state.frames & 0xFFF;
    OceffWipe3* this = (OceffWipe3*)thisx;
    f32 z;
    u8 alpha;
    s32 pad[2];
    Vec3f eye = GET_ACTIVE_CAM(play)->eye;
    Vtx* vtxPtr;
    Vec3f quakeOffset;

    quakeOffset = Camera_GetQuakeOffset(GET_ACTIVE_CAM(play));

    vtxPtr = ResourceMgr_LoadVtxByName(sSariaSongFrustumVtx);

    // #region 2S2H [Widescreen] Ocarina Effects
    f32 effectDistance = 1220.0f; // Vanilla value
    s32 x = OTRGetRectDimensionFromLeftEdge(0) << 2;
    if (x < 0) {
        // Only render if the screen is wider then original
        effectDistance = 1220.0f / (OTRGetAspectRatio() * 0.85f); // Widescreen value
    }
    // #endregion

    if (this->counter < 32) {
        z = MM_Math_SinS(this->counter * 512) * effectDistance;
    } else {
        z = effectDistance;
    }

    if (this->counter >= 80) {
        alpha = 12 * (100 - this->counter);
    } else {
        alpha = 255;
    }

    vtxPtr[1].v.cn[3] = vtxPtr[3].v.cn[3] = vtxPtr[5].v.cn[3] = vtxPtr[7].v.cn[3] = vtxPtr[9].v.cn[3] =
        vtxPtr[11].v.cn[3] = vtxPtr[13].v.cn[3] = vtxPtr[15].v.cn[3] = vtxPtr[17].v.cn[3] = vtxPtr[19].v.cn[3] =
            vtxPtr[21].v.cn[3] = alpha;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MM_Matrix_Translate(eye.x + quakeOffset.x, eye.y + quakeOffset.y, eye.z + quakeOffset.z, MTXMODE_NEW);
    MM_Matrix_Scale(0.1f, 0.1f, 0.1f, MTXMODE_APPLY);
    MM_Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_RotateXS(0x708, MTXMODE_APPLY);
    MM_Matrix_Translate(0.0f, 0.0f, -z, MTXMODE_APPLY);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 170, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 100, 200, 0, 128);
    MM_gSPDisplayList(POLY_XLU_DISP++, &sSariaSongFrustrumMaterialDL);
    MM_gSPDisplayList(POLY_XLU_DISP++, MM_Gfx_TwoTexScroll(play->state.gfxCtx, 0, scroll * 12, scroll * -12, 64, 64, 1,
                                                     scroll * 8, scroll * -8, 64, 64));
    MM_gSPDisplayList(POLY_XLU_DISP++, &sSariaSongFrustumModelDL);

    CLOSE_DISPS(play->state.gfxCtx);
}
