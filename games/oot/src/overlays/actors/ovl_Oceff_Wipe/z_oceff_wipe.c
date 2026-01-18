/*
 * File: z_oceff_wipe.c
 * Overlay: ovl_Oceff_Wipe
 * Description: Zelda's Lullaby and Song of Time Ocarina Effect
 */

#include "z_oceff_wipe.h"
#include "vt.h"
#include "soh/ResourceManagerHelpers.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA)

void OoT_OceffWipe_Init(Actor* thisx, PlayState* play);
void OoT_OceffWipe_Destroy(Actor* thisx, PlayState* play);
void OoT_OceffWipe_Update(Actor* thisx, PlayState* play);
void OoT_OceffWipe_Draw(Actor* thisx, PlayState* play);

const ActorInit Oceff_Wipe_InitVars = {
    ACTOR_OCEFF_WIPE,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(OceffWipe),
    (ActorFunc)OoT_OceffWipe_Init,
    (ActorFunc)OoT_OceffWipe_Destroy,
    (ActorFunc)OoT_OceffWipe_Update,
    (ActorFunc)OoT_OceffWipe_Draw,
    NULL,
};

void OoT_OceffWipe_Init(Actor* thisx, PlayState* play) {
    OceffWipe* this = (OceffWipe*)thisx;

    OoT_Actor_SetScale(&this->actor, 0.1f);
    this->timer = 0;
    this->actor.world.pos = GET_ACTIVE_CAM(play)->eye;
    osSyncPrintf(VT_FGCOL(CYAN) " WIPE arg_data = %d\n" VT_RST, this->actor.params);
}

void OoT_OceffWipe_Destroy(Actor* thisx, PlayState* play) {
    OceffWipe* this = (OceffWipe*)thisx;
    Player* player = GET_PLAYER(play);

    OoT_Magic_Reset(play);
    if (gSaveContext.nayrusLoveTimer != 0) {
        player->stateFlags3 |= PLAYER_STATE3_RESTORE_NAYRUS_LOVE;
    }
}

void OoT_OceffWipe_Update(Actor* thisx, PlayState* play) {
    OceffWipe* this = (OceffWipe*)thisx;

    this->actor.world.pos = GET_ACTIVE_CAM(play)->eye;
    if (this->timer < 100) {
        this->timer++;
    } else {
        OoT_Actor_Kill(&this->actor);
    }
}

#include "overlays/ovl_Oceff_Wipe/ovl_Oceff_Wipe.h"

static u8 OoT_sAlphaIndices[] = {
    0x01, 0x10, 0x22, 0x01, 0x20, 0x12, 0x01, 0x20, 0x12, 0x01,
    0x10, 0x22, 0x01, 0x20, 0x12, 0x01, 0x12, 0x21, 0x01, 0x02,
};

void OoT_OceffWipe_Draw(Actor* thisx, PlayState* play) {
    u32 scroll = play->state.frames & 0xFF;
    OceffWipe* this = (OceffWipe*)thisx;
    f32 z;
    s32 pad;
    u8 alphaTable[3];
    s32 i;
    Vec3f eye;
    Vtx* vtxPtr;
    Vec3f vec;

    eye = GET_ACTIVE_CAM(play)->eye;
    Camera_GetSkyboxOffset(&vec, GET_ACTIVE_CAM(play));

    OPEN_DISPS(play->state.gfxCtx);

    int fastOcarinaPlayback = (CVarGetInteger(CVAR_ENHANCEMENT("FastOcarinaPlayback"), 0) != 0);
    if (this->timer < 32) {
        z = OoT_Math_SinS(this->timer << 9) * (fastOcarinaPlayback ? 1200.0f : 1400.0f);
    } else {
        z = fastOcarinaPlayback ? 1200.0f : 1400.0f;
    }

    if (this->timer >= 80) {
        alphaTable[0] = 0;
        alphaTable[1] = (0x64 - this->timer) * 8;
        alphaTable[2] = (0x64 - this->timer) * 12;
    } else {
        alphaTable[0] = 0;
        alphaTable[1] = 0xA0;
        alphaTable[2] = 0xFF;
    }

    for (i = 0; i < 20; i++) {
        vtxPtr = ResourceMgr_LoadVtxByName(sFrustumVtx);
        vtxPtr[i * 2 + 0].v.cn[3] = alphaTable[(OoT_sAlphaIndices[i] & 0xF0) >> 4];
        vtxPtr[i * 2 + 1].v.cn[3] = alphaTable[OoT_sAlphaIndices[i] & 0xF];
    }

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    OoT_Matrix_Translate(eye.x + vec.x, eye.y + vec.y, eye.z + vec.z, MTXMODE_NEW);
    OoT_Matrix_Scale(0.1f, 0.1f, 0.1f, MTXMODE_APPLY);
    OoT_Matrix_ReplaceRotation(&play->billboardMtxF);
    OoT_Matrix_Translate(0.0f, 0.0f, -z, MTXMODE_APPLY);

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    if (this->actor.params != OCEFF_WIPE_ZL) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 170, 255, 255, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 150, 255, 128);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 200, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 100, 0, 255, 128);
    }

    gSPDisplayList(POLY_XLU_DISP++, sMaterialDL);
    gSPDisplayList(POLY_XLU_DISP++, OoT_Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0 - scroll, scroll * (-2), 32, 32, 1,
                                                     0 - scroll, scroll * (-2), 32, 32));
    gSPDisplayList(POLY_XLU_DISP++, sFrustumDL);

    CLOSE_DISPS(play->state.gfxCtx);
}
