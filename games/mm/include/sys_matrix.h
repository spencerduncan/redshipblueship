#ifndef SYS_MATRIX_H
#define SYS_MATRIX_H

#include "z64math.h"


struct GraphicsContext;
struct GameState;


typedef enum {
    /* 0 */ MTXMODE_NEW,  // generates a new matrix
    /* 1 */ MTXMODE_APPLY // applies transformation to the current matrix
} MatrixMode;

#define MATRIX_FINALIZE_AND_LOAD(pkt, gfxCtx) \
    gSPMatrix(pkt, Matrix_Finalize(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW)

extern Mtx gIdentityMtx;
extern MtxF gIdentityMtxF;


/* Stack operations */

void MM_Matrix_Init(struct GameState* gameState);
void MM_Matrix_Push(void);
void MM_Matrix_Pop(void);
void MM_Matrix_Get(MtxF* dest);
void MM_Matrix_Put(MtxF* src);
MtxF* MM_Matrix_GetCurrent(void);

/* Basic operations */

void MM_Matrix_Mult(MtxF* mf, MatrixMode mode);
void MM_Matrix_Translate(f32 x, f32 y, f32 z, MatrixMode mode);
void MM_Matrix_Scale(f32 x, f32 y, f32 z, MatrixMode mode);
void Matrix_RotateXS(s16 x, MatrixMode mode);
void Matrix_RotateXF(f32 x, MatrixMode mode);
void Matrix_RotateXFApply(f32 x);
void Matrix_RotateXFNew(f32 x);
void Matrix_RotateYS(s16 y, MatrixMode mode);
void Matrix_RotateYF(f32 y, MatrixMode mode);
void Matrix_RotateZS(s16 z, MatrixMode mode);
void Matrix_RotateZF(f32 z, MatrixMode mode);

/* Compound operations */

void MM_Matrix_RotateZYX(s16 x, s16 y, s16 z, MatrixMode mode);
void MM_Matrix_TranslateRotateZYX(Vec3f* translation, Vec3s* rot);
void MM_Matrix_SetTranslateRotateYXZ(f32 x, f32 y, f32 z, Vec3s* rot);

/* Conversion and allocation operations */

Mtx* MM_Matrix_MtxFToMtx(MtxF* src, Mtx* dest);
Mtx* MM_Matrix_ToMtx(Mtx* dest);
Mtx* Matrix_Finalize(struct GraphicsContext* gfxCtx);
Mtx* MM_Matrix_MtxFToNewMtx(MtxF* src, struct GraphicsContext* gfxCtx);

/* Vector operations */

void MM_Matrix_MultVec3f(Vec3f* src, Vec3f* dest);
void Matrix_MultZero(Vec3f* dest);
void Matrix_MultVecX(f32 x, Vec3f* dest);
void Matrix_MultVecY(f32 y, Vec3f* dest);
void Matrix_MultVecZ(f32 z, Vec3f* dest);
void Matrix_MultVec3fXZ(Vec3f* src, Vec3f* dest);

/* Copy and another conversion */

void MM_Matrix_MtxFCopy(MtxF* dest, MtxF* src);
void MM_Matrix_MtxToMtxF(Mtx* src, MtxF* dest);

/* Miscellaneous */

void MM_Matrix_MultVec3fExt(Vec3f* src, Vec3f* dest, MtxF* mf);
void MM_Matrix_Transpose(MtxF* mf);
void MM_Matrix_ReplaceRotation(MtxF* mf);
void Matrix_MtxFToYXZRot(MtxF* src, Vec3s* dest, s32 nonUniformScale);
void Matrix_MtxFToZYXRot(MtxF* src, Vec3s* dest, s32 nonUniformScale);
void Matrix_RotateAxisF(f32 angle, Vec3f* axis, MatrixMode mode);
void Matrix_RotateAxisS(s16 angle, Vec3f* axis, MatrixMode mode);

#endif
