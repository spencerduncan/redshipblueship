#include "global.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

typedef struct {
    /* 0x00 */ s16 drawType;  // indicates which draw function to use when displaying the object
    /* 0x04 */ void* drawArg; // segment address (display list or texture) passed to the draw function when called
} DebugDispObjectInfo;        // size = 0x8

typedef void (*DebugDispObject_DrawFunc)(DebugDispObject*, void*, PlayState*);

void OoT_DebugDisplay_DrawSpriteI8(DebugDispObject* dispObj, void* texture, PlayState* play);
void OoT_DebugDisplay_DrawPolygon(DebugDispObject* dispObj, void* dlist, PlayState* play);

static DebugDispObject_DrawFunc OoT_sDebugObjectDrawFuncTable[] = {
    OoT_DebugDisplay_DrawSpriteI8,
    OoT_DebugDisplay_DrawPolygon,
};

static DebugDispObjectInfo OoT_sDebugObjectInfoTable[] = {
    { 0, gDebugCircleTex }, { 0, gDebugCrossTex }, { 0, gDebugBallTex },
    { 0, gDebugCursorTex }, { 1, gDebugArrowDL },  { 1, gDebugCameraDL },
};

static Lights1 sDebugObjectLights = gdSPDefLights1(0x80, 0x80, 0x80, 0xFF, 0xFF, 0xFF, 0x49, 0x49, 0x49);

static DebugDispObject* OoT_sDebugObjectListHead;

void OoT_DebugDisplay_Init(void) {
    OoT_sDebugObjectListHead = NULL;
}

DebugDispObject* OoT_DebugDisplay_AddObject(f32 posX, f32 posY, f32 posZ, s16 rotX, s16 rotY, s16 rotZ, f32 scaleX,
                                        f32 scaleY, f32 scaleZ, u8 red, u8 green, u8 blue, u8 alpha, s16 type,
                                        GraphicsContext* gfxCtx) {
    DebugDispObject* prevHead = OoT_sDebugObjectListHead;

    if (GameInteractor_NoUIActive()) {
        return OoT_sDebugObjectListHead;
    }

    OoT_sDebugObjectListHead = Graph_Alloc(gfxCtx, sizeof(DebugDispObject));

    OoT_sDebugObjectListHead->pos.x = posX;
    OoT_sDebugObjectListHead->pos.y = posY;
    OoT_sDebugObjectListHead->pos.z = posZ;
    OoT_sDebugObjectListHead->rot.x = rotX;
    OoT_sDebugObjectListHead->rot.y = rotY;
    OoT_sDebugObjectListHead->rot.z = rotZ;
    OoT_sDebugObjectListHead->scale.x = scaleX;
    OoT_sDebugObjectListHead->scale.y = scaleY;
    OoT_sDebugObjectListHead->scale.z = scaleZ;
    OoT_sDebugObjectListHead->color.r = red;
    OoT_sDebugObjectListHead->color.g = green;
    OoT_sDebugObjectListHead->color.b = blue;
    OoT_sDebugObjectListHead->color.a = alpha;
    OoT_sDebugObjectListHead->type = type;
    OoT_sDebugObjectListHead->next = prevHead;

    return OoT_sDebugObjectListHead;
}

void OoT_DebugDisplay_DrawObjects(PlayState* play) {
    DebugDispObject* dispObj = OoT_sDebugObjectListHead;
    DebugDispObjectInfo* objInfo;

    while (dispObj != NULL) {
        objInfo = &OoT_sDebugObjectInfoTable[dispObj->type];
        OoT_sDebugObjectDrawFuncTable[objInfo->drawType](dispObj, objInfo->drawArg, play);
        dispObj = dispObj->next;
    }
}

void OoT_DebugDisplay_DrawSpriteI8(DebugDispObject* dispObj, void* texture, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_47Xlu(play->state.gfxCtx);

    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, dispObj->color.r, dispObj->color.g, dispObj->color.b, dispObj->color.a);

    OoT_Matrix_Translate(dispObj->pos.x, dispObj->pos.y, dispObj->pos.z, MTXMODE_NEW);
    OoT_Matrix_Scale(dispObj->scale.x, dispObj->scale.y, dispObj->scale.z, MTXMODE_APPLY);
    OoT_Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
    OoT_Matrix_RotateZYX(dispObj->rot.x, dispObj->rot.y, dispObj->rot.z, MTXMODE_APPLY);

    gDPLoadTextureBlock(POLY_XLU_DISP++, texture, G_IM_FMT_I, G_IM_SIZ_8b, 16, 16, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, gDebugSpriteDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void OoT_DebugDisplay_DrawPolygon(DebugDispObject* dispObj, void* dlist, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_4Xlu(play->state.gfxCtx);

    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, dispObj->color.r, dispObj->color.g, dispObj->color.b, dispObj->color.a);

    gSPSetLights1(POLY_XLU_DISP++, sDebugObjectLights);

    OoT_Matrix_SetTranslateRotateYXZ(dispObj->pos.x, dispObj->pos.y, dispObj->pos.z, &dispObj->rot);
    OoT_Matrix_Scale(dispObj->scale.x, dispObj->scale.y, dispObj->scale.z, MTXMODE_APPLY);

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, dlist);

    CLOSE_DISPS(play->state.gfxCtx);
}
