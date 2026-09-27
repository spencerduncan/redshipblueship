/**
 * @file test_ui_snapshot_image.c
 * @brief Display-free, ROM-free lock for the UI snapshot harness's pixel half
 *        (src/common/ui_snapshot_image.{h,c}).
 *
 * The window-bound `UiSnapshot` row runs on its own `ui` CI step and on the
 * workstation; everything it writes goes through the functions below. This row
 * pins them on every PR in the default `redship` tier, each with a red half:
 *
 *  1. The content hash sees every byte and the dimensions: one changed channel,
 *     or the same bytes at another width, is a different hash.
 *  2. A PNG written by libpng decodes back through stb_image to the SAME hash --
 *     the harness's assert 2, and the reason a capture can be re-read by the
 *     before/after composites.
 *  3. The blank-page oracle: a flat image is 0% non-modal with 1 colour (red); a
 *     drawn one is above both floors the harness uses (0.5%, 16 colours).
 *  4. Row flip is an involution; crop, the vertical stack (with its grey
 *     separator), the long-side fit and the x4 difference have the shapes and
 *     values the composites rely on.
 *
 * FILE SCOPE, compiled as C++ by test_runner.cpp (it uses std::filesystem for a
 * scratch directory).
 */

#include "ui_snapshot_image.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

int gUiImgFailures = 0;

#define UIIMG_CHECK(cond, ...)                                             \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            printf("[TEST]       ");                                       \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
            gUiImgFailures++;                                              \
        }                                                                  \
    } while (0)

void UiImgPaintSynthetic(UiImage* img) {
    // A gradient background with a solid rectangle: many colours, one of them
    // clearly modal, and no symmetry that a flip or crop bug could hide behind.
    for (int y = 0; y < img->h; y++) {
        for (int x = 0; x < img->w; x++) {
            uint8_t* p = img->rgba + ((size_t)y * (size_t)img->w + (size_t)x) * 4;
            p[0] = (uint8_t)(x * 2);
            p[1] = (uint8_t)(y * 3);
            p[2] = (uint8_t)((x + y) & 0xFF);
            p[3] = 255;
        }
    }
    for (int y = 10; y < 40; y++) {
        for (int x = 20; x < 90; x++) {
            uint8_t* p = img->rgba + ((size_t)y * (size_t)img->w + (size_t)x) * 4;
            p[0] = 30;
            p[1] = 144;
            p[2] = 255;
            p[3] = 255;
        }
    }
}

const uint8_t* UiImgPx(const UiImage* img, int x, int y) {
    return img->rgba + ((size_t)y * (size_t)img->w + (size_t)x) * 4;
}

} // namespace

TestResult Test_UiSnapshotImage(void) {
    printf("[TEST] ui-snapshot-image: the snapshot harness's PNG writer, decoder, hash and composites round-trip "
           "a synthetic image (display-free)\n");
    gUiImgFailures = 0;

    UiImage img = { 0, 0, nullptr };
    if (UiImage_Alloc(&img, 130, 70) != 0) {
        printf("[TEST] FAIL: could not allocate the synthetic image\n");
        return TEST_FAIL;
    }
    UiImgPaintSynthetic(&img);
    const uint64_t h0 = UiImage_Fnv1a64(&img);

    // ---- 1: the hash sees bytes and dimensions --------------------------------
    img.rgba[5 * 4 + 1] ^= 1;
    UIIMG_CHECK(UiImage_Fnv1a64(&img) != h0, "flipping one channel bit did not change the content hash");
    img.rgba[5 * 4 + 1] ^= 1;
    UIIMG_CHECK(UiImage_Fnv1a64(&img) == h0, "restoring the bit did not restore the hash");
    {
        UiImage reshaped = { 70, 130, img.rgba };
        UIIMG_CHECK(UiImage_Fnv1a64(&reshaped) != h0, "the same bytes at another shape hashed the same");
    }

    // ---- 2: PNG round trip -------------------------------------------------------
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir = "ui-snapshot-image-test";
    fs::create_directories(dir, ec);
    const std::string pngPath = (dir / "synthetic.png").string();
    UIIMG_CHECK(UiImage_WritePng(&img, pngPath.c_str()) == 0, "UiImage_WritePng failed on %s", pngPath.c_str());
    {
        UiImage back = { 0, 0, nullptr };
        UIIMG_CHECK(UiImage_ReadPng(&back, pngPath.c_str()) == 0, "UiImage_ReadPng failed on %s", pngPath.c_str());
        UIIMG_CHECK(back.w == img.w && back.h == img.h, "decoded %dx%d, wrote %dx%d", back.w, back.h, img.w, img.h);
        UIIMG_CHECK(UiImage_Fnv1a64(&back) == h0, "the decoded PNG does not hash to the written pixels");
        UiImage_Free(&back);
        UiImage none = { 0, 0, nullptr };
        UIIMG_CHECK(UiImage_ReadPng(&none, (dir / "does-not-exist.png").string().c_str()) != 0,
                    "reading a missing file reported success");
    }

    // ---- 3: the blank-page oracle, both halves --------------------------------
    {
        uint32_t modal = 0;
        double ratio = 0.0;
        int distinct = 0;
        UIIMG_CHECK(UiImage_Stats(&img, 4096, &modal, &ratio, &distinct) == 0, "stats failed");
        UIIMG_CHECK(modal == (30u | (144u << 8) | (255u << 16) | (255u << 24)),
                    "the modal colour is %08X, expected the solid rectangle's", (unsigned)modal);
        UIIMG_CHECK(ratio >= 0.005, "a drawn image read as %.4f non-modal (floor 0.005)", ratio);
        UIIMG_CHECK(distinct >= 16, "a drawn image has %d colours (floor 16)", distinct);

        UiImage flat = { 0, 0, nullptr };
        UiImage_Alloc(&flat, 64, 64);
        UiImage_Fill(&flat, 0, 0, 0, 255);
        UiImage_Stats(&flat, 4096, &modal, &ratio, &distinct);
        UIIMG_CHECK(ratio == 0.0 && distinct == 1, "a flat image read as %.4f non-modal with %d colours", ratio,
                    distinct);
        UiImage_Free(&flat);

        UiImage_Stats(&img, 8, &modal, &ratio, &distinct);
        UIIMG_CHECK(distinct == 8, "the distinct count did not saturate at its cap (got %d)", distinct);
    }

    // ---- 4: composites ------------------------------------------------------------
    UiImage_FlipRows(&img);
    UIIMG_CHECK(UiImage_Fnv1a64(&img) != h0, "a row flip left the hash unchanged");
    UiImage_FlipRows(&img);
    UIIMG_CHECK(UiImage_Fnv1a64(&img) == h0, "flipping twice is not the identity");

    UiImage crop = { 0, 0, nullptr };
    UIIMG_CHECK(UiImage_Crop(&img, 20, 10, 70, 30, &crop) == 0 && crop.w == 70 && crop.h == 30,
                "crop produced %dx%d", crop.w, crop.h);
    if (crop.rgba != nullptr) {
        UIIMG_CHECK(memcmp(UiImgPx(&crop, 0, 0), UiImgPx(&img, 20, 10), 4) == 0, "crop origin is off");
        UIIMG_CHECK(memcmp(UiImgPx(&crop, 69, 29), UiImgPx(&img, 89, 39), 4) == 0, "crop extent is off");
    }
    {
        UiImage clamped = { 0, 0, nullptr };
        UIIMG_CHECK(UiImage_Crop(&img, -5, -5, 20, 20, &clamped) == 0 && clamped.w == 15 && clamped.h == 15,
                    "a crop past the top-left edge was not clamped (got %dx%d)", clamped.w, clamped.h);
        UiImage_Free(&clamped);
    }

    UiImage stacked = { 0, 0, nullptr };
    UIIMG_CHECK(UiImage_StackVertical(&img, &crop, 8, &stacked) == 0 && stacked.w == 130 && stacked.h == 70 + 8 + 30,
                "stack produced %dx%d", stacked.w, stacked.h);
    if (stacked.rgba != nullptr) {
        UIIMG_CHECK(memcmp(UiImgPx(&stacked, 3, 4), UiImgPx(&img, 3, 4), 4) == 0, "the top image is not on top");
        UIIMG_CHECK(UiImgPx(&stacked, 50, 72)[0] == 128, "the separator band is not mid-grey");
        UIIMG_CHECK(memcmp(UiImgPx(&stacked, 0, 78), UiImgPx(&crop, 0, 0), 4) == 0, "the bottom image is misplaced");
        UIIMG_CHECK(UiImgPx(&stacked, 100, 90)[0] == 0 && UiImgPx(&stacked, 100, 90)[3] == 255,
                    "the canvas beside the narrower image is not opaque black");
    }

    {
        UiImage wide = { 0, 0, nullptr };
        UiImage fitted = { 0, 0, nullptr };
        // 3200 -> 1600 (still over) -> 800: two halvings, the height rounding up.
        UiImage_Alloc(&wide, 3200, 10);
        UIIMG_CHECK(UiImage_FitLongSide(&wide, 1568, &fitted) == 0 && fitted.w == 800 && fitted.h == 3,
                    "fitting 3200x10 into 1568 produced %dx%d, expected two halvings to 800x3", fitted.w, fitted.h);
        UiImage_Free(&fitted);
        UIIMG_CHECK(UiImage_FitLongSide(&img, 1568, &fitted) == 0 && UiImage_Fnv1a64(&fitted) == h0,
                    "fitting an already small image was not an exact copy");
        UiImage_Free(&fitted);
        UiImage_Free(&wide);
    }

    {
        UiImage other = { 0, 0, nullptr };
        UiImage diff = { 0, 0, nullptr };
        uint64_t changed = 99;
        UiImage_Crop(&img, 0, 0, img.w, img.h, &other);
        UIIMG_CHECK(UiImage_AbsDiffX4(&img, &other, &diff, &changed) == 0 && changed == 0,
                    "identical images differ in %llu pixels", (unsigned long long)changed);
        UiImage_Free(&diff);
        other.rgba[(7 * other.w + 9) * 4 + 2] = (uint8_t)(other.rgba[(7 * other.w + 9) * 4 + 2] ^ 0x10);
        UIIMG_CHECK(UiImage_AbsDiffX4(&img, &other, &diff, &changed) == 0 && changed == 1,
                    "one changed pixel counted as %llu", (unsigned long long)changed);
        if (diff.rgba != nullptr) {
            UIIMG_CHECK(UiImgPx(&diff, 9, 7)[0] == 64, "a 16-level difference is drawn as %d, expected 16 x 4",
                        UiImgPx(&diff, 9, 7)[0]);
            UIIMG_CHECK(UiImgPx(&diff, 8, 7)[0] == 0, "an unchanged pixel is drawn nonzero");
        }
        UiImage_Free(&diff);
        UiImage_Free(&other);
    }

    UiImage_Free(&stacked);
    UiImage_Free(&crop);
    UiImage_Free(&img);
    fs::remove_all(dir, ec);

    if (gUiImgFailures != 0) {
        printf("[TEST] FAIL: ui-snapshot-image: %d check(s) failed\n", gUiImgFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: ui-snapshot-image: hash, PNG round trip, blank oracle and composites behave\n");
    return TEST_PASS;
}
