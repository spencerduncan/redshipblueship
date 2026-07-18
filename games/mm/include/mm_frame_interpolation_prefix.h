#ifndef MM_FRAME_INTERPOLATION_PREFIX_H
#define MM_FRAME_INTERPOLATION_PREFIX_H

/**
 * Single-exe symbol-prefix shim for MM's frame-interpolation API.
 *
 * Both ports define an identically-named extern "C" FrameInterpolation_* API
 * (SoH: soh/Enhancements/FrameInterpolation/frame_interpolation.cpp; 2S2H:
 * 2s2h/Enhancements/FrameInterpolation/FrameInterpolation.cpp) — and the C++
 * FrameInterpolation_Interpolate even mangles identically because both games
 * name their matrix types Mtx/MtxF. Under /FORCE:MULTIPLE (and first-wins
 * archive resolution generally) SoH's implementation won the entire family,
 * so every MM draw recorded its matrices into OoT's interpolation engine and
 * replayed OoT's state — MM's 3D world rendered as flashing garbage
 * triangles while 2D texrects (which bypass the matrix path) stayed intact.
 * Found 2026-07-18, the first time human eyes saw MM 3D frames in single-exe
 * (docs/ci-gameplay-repro-postmortem.md section 8).
 *
 * The rename must be visible in EVERY MM TU that touches the API, including
 * the hundreds that only expand OPEN_DISPS/CLOSE_DISPS — those macros
 * (include/gfx.h) locally declare and call RecordOpenChild/RecordCloseChild
 * without including FrameInterpolation.h. gfx.h and FrameInterpolation.h
 * both include this shim, which covers all of: direct callers (they include
 * FrameInterpolation.h), macro expanders (they include gfx.h to get the
 * macros), and the implementation itself (FrameInterpolation.cpp includes
 * its own header).
 *
 * The soh side keeps the unprefixed names — only MM is renamed, mirroring
 * the repo-wide MM_ prefix convention for cross-game symbol collisions.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#define FrameInterpolation_Interpolate MM_FrameInterpolation_Interpolate
#define FrameInterpolation_ShouldInterpolateFrame MM_FrameInterpolation_ShouldInterpolateFrame
#define FrameInterpolation_StartRecord MM_FrameInterpolation_StartRecord
#define FrameInterpolation_StopRecord MM_FrameInterpolation_StopRecord
#define FrameInterpolation_RecordOpenChild MM_FrameInterpolation_RecordOpenChild
#define FrameInterpolation_RecordCloseChild MM_FrameInterpolation_RecordCloseChild
#define FrameInterpolation_DontInterpolateCamera MM_FrameInterpolation_DontInterpolateCamera
#define FrameInterpolation_GetCameraEpoch MM_FrameInterpolation_GetCameraEpoch
#define FrameInterpolation_IgnoreActorMtx MM_FrameInterpolation_IgnoreActorMtx
#define FrameInterpolation_InterpolateWiderAngles MM_FrameInterpolation_InterpolateWiderAngles
#define FrameInterpolation_RecordActorPosRotMatrix MM_FrameInterpolation_RecordActorPosRotMatrix
#define FrameInterpolation_RecordMatrixPush MM_FrameInterpolation_RecordMatrixPush
#define FrameInterpolation_RecordMatrixPop MM_FrameInterpolation_RecordMatrixPop
#define FrameInterpolation_RecordMatrixPut MM_FrameInterpolation_RecordMatrixPut
#define FrameInterpolation_RecordMatrixMult MM_FrameInterpolation_RecordMatrixMult
#define FrameInterpolation_RecordMatrixTranslate MM_FrameInterpolation_RecordMatrixTranslate
#define FrameInterpolation_RecordMatrixScale MM_FrameInterpolation_RecordMatrixScale
#define FrameInterpolation_RecordMatrixRotate1Coord MM_FrameInterpolation_RecordMatrixRotate1Coord
#define FrameInterpolation_RecordMatrixRotateZYX MM_FrameInterpolation_RecordMatrixRotateZYX
#define FrameInterpolation_RecordMatrixTranslateRotateZYX MM_FrameInterpolation_RecordMatrixTranslateRotateZYX
#define FrameInterpolation_RecordMatrixSetTranslateRotateYXZ MM_FrameInterpolation_RecordMatrixSetTranslateRotateYXZ
#define FrameInterpolation_RecordMatrixMtxFToMtx MM_FrameInterpolation_RecordMatrixMtxFToMtx
#define FrameInterpolation_RecordMatrixToMtx MM_FrameInterpolation_RecordMatrixToMtx
#define FrameInterpolation_RecordMatrixReplaceRotation MM_FrameInterpolation_RecordMatrixReplaceRotation
#define FrameInterpolation_RecordMatrixRotateAxis MM_FrameInterpolation_RecordMatrixRotateAxis
#define FrameInterpolation_RecordSkinMatrixMtxFToMtx MM_FrameInterpolation_RecordSkinMatrixMtxFToMtx

#endif // RSBS_SINGLE_EXECUTABLE

#endif // MM_FRAME_INTERPOLATION_PREFIX_H
