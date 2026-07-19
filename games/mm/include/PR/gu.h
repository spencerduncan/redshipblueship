#ifndef PR_GU_H
#define PR_GU_H

#include <libultraship/libultra/gu.h>

#include "ultratypes.h"
#include "gbi.h"

/**
 * RSBS 2026-07-18: these prototypes were trapped inside a vendoring-era
 * `#if 0` block — every MM caller of the MM_gu* family compiled them as
 * IMPLICIT declarations. In C that promotes every float argument to double
 * (and MSVC x64 additionally mirrors promoted args into integer registers),
 * while the definitions in src/code/stubs.c read the normal float ABI — so
 * every gu-built matrix (projection, look-at, ortho, rotate) was register
 * noise. Symptom: all MM 3D rendered as flashing fullscreen triangles while
 * 2D texrects (no matrices) were perfect. Do NOT let these go dead again:
 * MM builds carry implicit-function-declaration as an error precisely so
 * this class cannot recur silently.
 *
 * (`near`/`far` parameter names avoided — windows headers #define them away
 * in TUs that include windows.h.)
 */

void MM_guOrtho(Mtx* m, f32 l, f32 r, f32 b, f32 t, f32 n, f32 f, f32 scale);
void MM_guOrthoF(float m[4][4], f32 l, f32 r, f32 b, f32 t, f32 n, f32 f, f32 scale);

void MM_guPerspective(Mtx* m, u16* perspNorm, f32 fovy, f32 aspect, f32 zNear, f32 zFar, f32 scale);
void MM_guPerspectiveF(float mf[4][4], u16* perspNorm, f32 fovy, f32 aspect, f32 zNear, f32 zFar, f32 scale);

void MM_guLookAt(Mtx* m, f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp, f32 zUp);
void MM_guLookAtF(float mf[4][4], f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp,
                  f32 zUp);

void MM_guLookAtHilite(Mtx* m, LookAt* l, Hilite* h, f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt,
                       f32 xUp, f32 yUp, f32 zUp, f32 xl1, f32 yl1, f32 zl1, f32 xl2, f32 yl2, f32 zl2,
                       s32 hiliteWidth, s32 hiliteHeight);
void MM_guLookAtHiliteF(float mf[4][4], LookAt* l, Hilite* h, f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt,
                        f32 zAt, f32 xUp, f32 yUp, f32 zUp, f32 xl1, f32 yl1, f32 zl1, f32 xl2, f32 yl2, f32 zl2,
                        s32 hiliteWidth, s32 hiliteHeight);

void MM_guRotate(Mtx* m, f32 a, f32 x, f32 y, f32 z);
void MM_guRotateF(float m[4][4], f32 a, f32 x, f32 y, f32 z);

void MM_guPosition(Mtx* m, f32 rot, f32 pitch, f32 yaw, f32 scale, f32 x, f32 y, f32 z);
void MM_guPositionF(float mf[4][4], f32 rot, f32 pitch, f32 yaw, f32 scale, f32 x, f32 y, f32 z);

// f32 sinf(f32 __x);
// f32 cosf(f32 __x);

s16 MM_sins(u16 x);
s16 MM_coss(u16 x);

// Original N64 used compiler intrinsic for sqrtf. We map to standard sqrtf.
#include <math.h>
#ifndef MM_sqrtf
#define MM_sqrtf sqrtf
#endif


#endif
