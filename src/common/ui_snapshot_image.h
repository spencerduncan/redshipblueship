/**
 * @file ui_snapshot_image.h
 * @brief The pixel half of the UI snapshot harness (`redship --test ui-snapshot`):
 *        an RGBA8 buffer, a PNG writer and reader, a content hash, and the
 *        composites an agent Reads to judge a page.
 *
 * WHY THIS IS ITS OWN GAME-HEADER-FREE C FILE. Everything here is plain pixels:
 * nothing reads ImGui, a game, or the renderer. The capture half
 * (games/oot/soh/soh_ui_snapshot.cpp) owns the window and hands this file a
 * buffer. Two consequences are load-bearing:
 *
 *  1. The libpng writer is C, so libpng's setjmp error path never unwinds
 *     through a C++ frame with live destructors.
 *  2. It needs no display and no archive, so the `UiSnapshotImage` row in the
 *     default `redship` tier round-trips a synthetic image through every
 *     function below on every PR, including the ones whose only other caller is
 *     the window-bound `ui` row that CI runs on its own step.
 *
 * WHAT IT NEVER DOES: compare a capture against a stored golden image. The
 * harness asserts STRUCTURE (the page drew, it is not blank, it converged); how
 * a page LOOKS is judged by a person or an agent reading the composites.
 */
#ifndef RSBS_UI_SNAPSHOT_IMAGE_H
#define RSBS_UI_SNAPSHOT_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A tightly packed RGBA8 image, rows top to bottom. `rgba` is owned. */
typedef struct UiImage {
    int w;
    int h;
    uint8_t* rgba;
} UiImage;

/** Allocate a zeroed @p w x @p h image. Returns 0 on success. */
int UiImage_Alloc(UiImage* img, int w, int h);
/** Free and zero @p img. Safe on a zeroed image. */
void UiImage_Free(UiImage* img);
/** Fill every pixel with one colour. */
void UiImage_Fill(UiImage* img, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
/** Reverse the row order in place (GL reads bottom-up). */
void UiImage_FlipRows(UiImage* img);

/** FNV-1a 64 over @p len bytes, continuing from @p seed (pass UI_FNV1A64_OFFSET to start). */
#define UI_FNV1A64_OFFSET 0xcbf29ce484222325ull
uint64_t Ui_Fnv1a64(const void* data, size_t len, uint64_t seed);
/**
 * The content hash the manifest calls `rgbaFnv1a64`: FNV-1a 64 over the width and
 * height (4 little-endian bytes each) and then every RGBA byte. Two captures with
 * equal hashes are the same picture; the settle loop stops on two in a row.
 */
uint64_t UiImage_Fnv1a64(const UiImage* img);

/**
 * Structural "did anything draw" statistics: the modal (most common) colour, the
 * fraction of pixels that differ from it, and the number of distinct colours
 * (counted exactly up to @p distinctCap, then saturated at the cap). Returns 0 on
 * success.
 */
int UiImage_Stats(const UiImage* img, int distinctCap, uint32_t* modalRgba, double* nonModalRatio,
                  int* distinctColors);

/** Write @p img as an 8-bit RGBA PNG through libpng. Returns 0 on success. */
int UiImage_WritePng(const UiImage* img, const char* path);
/** Read any PNG through stb_image into RGBA8. Returns 0 on success. */
int UiImage_ReadPng(UiImage* out, const char* path);

/** Copy the rectangle (@p x, @p y, @p w, @p h), clamped to @p src, into @p out. */
int UiImage_Crop(const UiImage* src, int x, int y, int w, int h, UiImage* out);
/**
 * @p top over an @p separatorPx-high mid-grey band over @p bottom, left-aligned,
 * the canvas as wide as the wider input and filled opaque black. The composite
 * shape the harness writes for "reference over ours" and "before over after".
 */
int UiImage_StackVertical(const UiImage* top, const UiImage* bottom, int separatorPx, UiImage* out);
/**
 * Box-downscale by 2 until the long side is <= @p maxLongSide. The Read tool
 * shrinks large images anyway; doing it here keeps the result predictable. A
 * plain copy when already small enough.
 */
int UiImage_FitLongSide(const UiImage* src, int maxLongSide, UiImage* out);
/**
 * Per-pixel absolute difference, max over R/G/B, times 4, as opaque grey, over
 * the union of @p a and @p b (a pixel outside the overlap counts as changed and
 * is drawn white). @p changedPixels receives the number of pixels that differ in
 * any channel.
 */
int UiImage_AbsDiffX4(const UiImage* a, const UiImage* b, UiImage* out, uint64_t* changedPixels);

#ifdef __cplusplus
}
#endif

#endif // RSBS_UI_SNAPSHOT_IMAGE_H
