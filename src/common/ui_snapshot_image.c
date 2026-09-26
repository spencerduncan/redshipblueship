/**
 * @file ui_snapshot_image.c
 * @brief See ui_snapshot_image.h.
 *
 * The PNG writer drives libpng through png_set_write_fn with a writer in this
 * TU, never png_init_io: png_init_io hands libpng a FILE* from THIS module's C
 * runtime, which is only safe when libpng was linked against the same one. The
 * callback makes that a non-question on every triplet.
 */
#include "ui_snapshot_image.h"

#include <png.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_image.h"

int UiImage_Alloc(UiImage* img, int w, int h) {
    if (img == NULL || w <= 0 || h <= 0) {
        return -1;
    }
    img->rgba = (uint8_t*)calloc((size_t)w * (size_t)h, 4);
    if (img->rgba == NULL) {
        img->w = img->h = 0;
        return -1;
    }
    img->w = w;
    img->h = h;
    return 0;
}

void UiImage_Free(UiImage* img) {
    if (img == NULL) {
        return;
    }
    free(img->rgba);
    img->rgba = NULL;
    img->w = img->h = 0;
}

void UiImage_Fill(UiImage* img, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (img == NULL || img->rgba == NULL) {
        return;
    }
    const size_t n = (size_t)img->w * (size_t)img->h;
    for (size_t i = 0; i < n; i++) {
        img->rgba[i * 4 + 0] = r;
        img->rgba[i * 4 + 1] = g;
        img->rgba[i * 4 + 2] = b;
        img->rgba[i * 4 + 3] = a;
    }
}

void UiImage_FlipRows(UiImage* img) {
    if (img == NULL || img->rgba == NULL || img->h < 2) {
        return;
    }
    const size_t stride = (size_t)img->w * 4;
    uint8_t* tmp = (uint8_t*)malloc(stride);
    if (tmp == NULL) {
        return;
    }
    for (int y = 0; y < img->h / 2; y++) {
        uint8_t* a = img->rgba + (size_t)y * stride;
        uint8_t* b = img->rgba + (size_t)(img->h - 1 - y) * stride;
        memcpy(tmp, a, stride);
        memcpy(a, b, stride);
        memcpy(b, tmp, stride);
    }
    free(tmp);
}

uint64_t Ui_Fnv1a64(const void* data, size_t len, uint64_t seed) {
    const uint8_t* p = (const uint8_t*)data;
    uint64_t h = seed;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

uint64_t UiImage_Fnv1a64(const UiImage* img) {
    if (img == NULL || img->rgba == NULL) {
        return 0;
    }
    uint8_t dims[8];
    for (int i = 0; i < 4; i++) {
        dims[i] = (uint8_t)(((uint32_t)img->w >> (8 * i)) & 0xFF);
        dims[4 + i] = (uint8_t)(((uint32_t)img->h >> (8 * i)) & 0xFF);
    }
    uint64_t h = Ui_Fnv1a64(dims, sizeof(dims), UI_FNV1A64_OFFSET);
    return Ui_Fnv1a64(img->rgba, (size_t)img->w * (size_t)img->h * 4, h);
}

/* An open-addressed set of 32-bit colours with counts, for the modal colour and
 * the distinct count. Sized for the cap; saturates rather than growing. */
typedef struct {
    uint32_t key;
    uint32_t count;
    int used;
} UiColorSlot;

static uint32_t UiImage_PixelKey(const uint8_t* px) {
    return (uint32_t)px[0] | ((uint32_t)px[1] << 8) | ((uint32_t)px[2] << 16) | ((uint32_t)px[3] << 24);
}

int UiImage_Stats(const UiImage* img, int distinctCap, uint32_t* modalRgba, double* nonModalRatio,
                  int* distinctColors) {
    if (img == NULL || img->rgba == NULL || distinctCap <= 0) {
        return -1;
    }
    size_t cap = 1;
    while (cap < (size_t)distinctCap * 2u) {
        cap <<= 1;
    }
    UiColorSlot* slots = (UiColorSlot*)calloc(cap, sizeof(UiColorSlot));
    if (slots == NULL) {
        return -1;
    }
    int distinct = 0;
    int saturated = 0;
    const size_t n = (size_t)img->w * (size_t)img->h;
    for (size_t i = 0; i < n; i++) {
        const uint32_t key = UiImage_PixelKey(img->rgba + i * 4);
        size_t idx = (size_t)((key * 2654435761u) & (uint32_t)(cap - 1));
        for (;;) {
            if (!slots[idx].used) {
                if (distinct >= distinctCap) {
                    saturated = 1;
                    break;
                }
                slots[idx].used = 1;
                slots[idx].key = key;
                slots[idx].count = 1;
                distinct++;
                break;
            }
            if (slots[idx].key == key) {
                slots[idx].count++;
                break;
            }
            idx = (idx + 1) & (cap - 1);
        }
    }
    uint32_t bestKey = 0;
    uint32_t bestCount = 0;
    for (size_t i = 0; i < cap; i++) {
        if (slots[i].used && slots[i].count > bestCount) {
            bestCount = slots[i].count;
            bestKey = slots[i].key;
        }
    }
    free(slots);
    if (modalRgba != NULL) {
        *modalRgba = bestKey;
    }
    if (nonModalRatio != NULL) {
        /* Recount exactly against the modal colour: a saturated set undercounts
         * the tail, and this keeps the ratio exact regardless. */
        size_t same = 0;
        for (size_t i = 0; i < n; i++) {
            if (UiImage_PixelKey(img->rgba + i * 4) == bestKey) {
                same++;
            }
        }
        *nonModalRatio = (n > 0) ? (double)(n - same) / (double)n : 0.0;
    }
    if (distinctColors != NULL) {
        *distinctColors = saturated ? distinctCap : distinct;
    }
    return 0;
}

/* ---- PNG writer (libpng) --------------------------------------------------- */

typedef struct {
    FILE* fp;
    int failed;
} UiPngSink;

static void UiPng_Write(png_structp png, png_bytep data, png_size_t len) {
    UiPngSink* sink = (UiPngSink*)png_get_io_ptr(png);
    if (sink != NULL && !sink->failed && fwrite(data, 1, len, sink->fp) != len) {
        sink->failed = 1;
    }
}

static void UiPng_Flush(png_structp png) {
    UiPngSink* sink = (UiPngSink*)png_get_io_ptr(png);
    if (sink != NULL) {
        fflush(sink->fp);
    }
}

/* The row table is allocated BEFORE the setjmp so the longjmp target never has
 * to reason about a local that changed after it was armed. */
int UiImage_WritePng(const UiImage* img, const char* path) {
    if (img == NULL || img->rgba == NULL || path == NULL) {
        return -1;
    }
    png_bytep* rows = (png_bytep*)malloc(sizeof(png_bytep) * (size_t)img->h);
    if (rows == NULL) {
        return -1;
    }
    for (int y = 0; y < img->h; y++) {
        rows[y] = (png_bytep)(img->rgba + (size_t)y * (size_t)img->w * 4);
    }
    UiPngSink sink;
    sink.fp = fopen(path, "wb");
    sink.failed = 0;
    if (sink.fp == NULL) {
        free(rows);
        return -1;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = (png != NULL) ? png_create_info_struct(png) : NULL;
    if (png == NULL || info == NULL) {
        png_destroy_write_struct(&png, info != NULL ? &info : NULL);
        fclose(sink.fp);
        free(rows);
        return -1;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(sink.fp);
        free(rows);
        return -1;
    }
    png_set_write_fn(png, &sink, UiPng_Write, UiPng_Flush);
    png_set_IHDR(png, info, (png_uint_32)img->w, (png_uint_32)img->h, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    /* A fast level: these are working images, written dozens per run. */
    png_set_compression_level(png, 3);
    png_write_info(png, info);
    png_write_image(png, rows);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    free(rows);
    const int failed = sink.failed;
    if (fclose(sink.fp) != 0) {
        return -1;
    }
    return failed ? -1 : 0;
}

int UiImage_ReadPng(UiImage* out, const char* path) {
    if (out == NULL || path == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    int w = 0;
    int h = 0;
    int comp = 0;
    unsigned char* data = stbi_load(path, &w, &h, &comp, 4);
    if (data == NULL) {
        return -1;
    }
    if (UiImage_Alloc(out, w, h) != 0) {
        stbi_image_free(data);
        return -1;
    }
    memcpy(out->rgba, data, (size_t)w * (size_t)h * 4);
    stbi_image_free(data);
    return 0;
}

/* ---- Composites ------------------------------------------------------------ */

int UiImage_Crop(const UiImage* src, int x, int y, int w, int h, UiImage* out) {
    if (src == NULL || src->rgba == NULL || out == NULL) {
        return -1;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > src->w) {
        w = src->w - x;
    }
    if (y + h > src->h) {
        h = src->h - y;
    }
    if (w <= 0 || h <= 0) {
        return -1;
    }
    if (UiImage_Alloc(out, w, h) != 0) {
        return -1;
    }
    for (int row = 0; row < h; row++) {
        memcpy(out->rgba + (size_t)row * (size_t)w * 4,
               src->rgba + ((size_t)(y + row) * (size_t)src->w + (size_t)x) * 4, (size_t)w * 4);
    }
    return 0;
}

static void UiImage_Blit(UiImage* dst, const UiImage* src, int dx, int dy) {
    for (int row = 0; row < src->h; row++) {
        const int y = dy + row;
        if (y < 0 || y >= dst->h) {
            continue;
        }
        int w = src->w;
        if (dx + w > dst->w) {
            w = dst->w - dx;
        }
        if (w <= 0) {
            continue;
        }
        memcpy(dst->rgba + ((size_t)y * (size_t)dst->w + (size_t)dx) * 4,
               src->rgba + (size_t)row * (size_t)src->w * 4, (size_t)w * 4);
    }
}

int UiImage_StackVertical(const UiImage* top, const UiImage* bottom, int separatorPx, UiImage* out) {
    if (top == NULL || bottom == NULL || top->rgba == NULL || bottom->rgba == NULL || out == NULL ||
        separatorPx < 0) {
        return -1;
    }
    const int w = (top->w > bottom->w) ? top->w : bottom->w;
    const int h = top->h + separatorPx + bottom->h;
    if (UiImage_Alloc(out, w, h) != 0) {
        return -1;
    }
    UiImage_Fill(out, 0, 0, 0, 255);
    UiImage_Blit(out, top, 0, 0);
    for (int y = top->h; y < top->h + separatorPx; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t* px = out->rgba + ((size_t)y * (size_t)w + (size_t)x) * 4;
            px[0] = px[1] = px[2] = 128;
            px[3] = 255;
        }
    }
    UiImage_Blit(out, bottom, 0, top->h + separatorPx);
    return 0;
}

static int UiImage_Half(const UiImage* src, UiImage* out) {
    const int w = (src->w + 1) / 2;
    const int h = (src->h + 1) / 2;
    if (UiImage_Alloc(out, w, h) != 0) {
        return -1;
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned sum[4] = { 0, 0, 0, 0 };
            unsigned n = 0;
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    const int sx = 2 * x + dx;
                    const int sy = 2 * y + dy;
                    if (sx >= src->w || sy >= src->h) {
                        continue;
                    }
                    const uint8_t* p = src->rgba + ((size_t)sy * (size_t)src->w + (size_t)sx) * 4;
                    for (int c = 0; c < 4; c++) {
                        sum[c] += p[c];
                    }
                    n++;
                }
            }
            uint8_t* d = out->rgba + ((size_t)y * (size_t)w + (size_t)x) * 4;
            for (int c = 0; c < 4; c++) {
                d[c] = (uint8_t)((sum[c] + n / 2) / (n ? n : 1));
            }
        }
    }
    return 0;
}

int UiImage_FitLongSide(const UiImage* src, int maxLongSide, UiImage* out) {
    if (src == NULL || src->rgba == NULL || out == NULL || maxLongSide <= 0) {
        return -1;
    }
    UiImage cur;
    if (UiImage_Crop(src, 0, 0, src->w, src->h, &cur) != 0) {
        return -1;
    }
    while ((cur.w > cur.h ? cur.w : cur.h) > maxLongSide) {
        UiImage half;
        if (UiImage_Half(&cur, &half) != 0) {
            UiImage_Free(&cur);
            return -1;
        }
        UiImage_Free(&cur);
        cur = half;
    }
    *out = cur;
    return 0;
}

int UiImage_AbsDiffX4(const UiImage* a, const UiImage* b, UiImage* out, uint64_t* changedPixels) {
    if (a == NULL || b == NULL || a->rgba == NULL || b->rgba == NULL || out == NULL) {
        return -1;
    }
    const int w = (a->w > b->w) ? a->w : b->w;
    const int h = (a->h > b->h) ? a->h : b->h;
    if (UiImage_Alloc(out, w, h) != 0) {
        return -1;
    }
    uint64_t changed = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t* d = out->rgba + ((size_t)y * (size_t)w + (size_t)x) * 4;
            int v;
            if (x >= a->w || y >= a->h || x >= b->w || y >= b->h) {
                v = 255;
                changed++;
            } else {
                const uint8_t* pa = a->rgba + ((size_t)y * (size_t)a->w + (size_t)x) * 4;
                const uint8_t* pb = b->rgba + ((size_t)y * (size_t)b->w + (size_t)x) * 4;
                int m = 0;
                for (int c = 0; c < 3; c++) {
                    const int dd = abs((int)pa[c] - (int)pb[c]);
                    if (dd > m) {
                        m = dd;
                    }
                }
                if (m != 0 || pa[3] != pb[3]) {
                    changed++;
                }
                v = m * 4;
                if (v > 255) {
                    v = 255;
                }
            }
            d[0] = d[1] = d[2] = (uint8_t)v;
            d[3] = 255;
        }
    }
    if (changedPixels != NULL) {
        *changedPixels = changed;
    }
    return 0;
}
