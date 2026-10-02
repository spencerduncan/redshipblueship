/**
 * @file ForeignModelHostOoT.cpp
 * @brief OoT as the HOST of a foreign model: an MM item's model drawn in OoT,
 *        re-expressed from MM's descriptor with OoT's own primitives (#800 S1,
 *        on #577 M2's registry).
 *
 * The first OoT consumer is the plain shop shelf (#800 pass 1): an OoT shop slot
 * that hosts an MM item draws that item's model on the shelf (z_en_girla.c,
 * OoT_EnGirlA_Draw). The twin of MM's host side (ForeignModelSingleExe.cpp,
 * #577 M3) and written to be reused by OoT's get-item cutscene (#577 M4).
 *
 * WHAT IS DRAWN. Only a DESCRIPTOR whose every display-list path a mounted
 * archive holds (foreign_model.h, THREE ANSWERS: the answer says nothing about
 * mounts, the consumer checks). MM's archives are added the first time MM is
 * entered in this process (rsbs/src/main.cpp, Combo_EnsureGameArchivesLoaded),
 * and redship-oot.o2r carries the MM content #577 M6 curates for OoT; an MM
 * model none of them holds answers "no model" here. A HOST_NATIVE answer (a
 * colliding MM model OoT's host-native table maps to OoT's own draw row, #577
 * M7) draws nothing here yet: drawing that row on the shelf is a follow-up (#832). In
 * every "no model" case, and for HOST_NATIVE, the caller keeps its stand-in.
 *
 * HOW. Each layer, in the order both games' z_draw.c emit it: the setup list,
 * the layer's scrolling segments, its colours (and the grayscale tint), the
 * matrix, the plain parts, then the camera-facing parts under their offset and
 * the billboard rotation. The descriptor's own scale and rotation go on top of
 * the caller's matrix. Every display list is named by its "__OTR__" path, which
 * OoT's gSPDisplayList resolves through the shared resource manager (GbiWrap.cpp).
 *
 * WHAT IT NEVER DOES: call an MM draw function, or read MM's live state.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "soh/OTRGlobals.h"
#include "soh/frame_interpolation.h"

extern "C" {
#include <z64.h>
#include "macros.h"
#include "functions.h"
#include "variables.h"
// sys_matrix.c's stack (no header declares it): the test bridge below gives the
// draw a private stack and puts these back.
extern MtxF* OoT_sMatrixStack;
extern MtxF* OoT_sCurrentMatrix;
}

#include "context.h"       // src/common — GameId
#include "foreign_items.h" // src/common — Combo_GetForeignPlacementForOoTCheck
#include "foreign_model.h" // src/common — the registry

namespace {

constexpr uint8_t kHostOpa = (uint8_t)COMBO_MODEL_LAYER_OPA;
constexpr uint8_t kHostXlu = (uint8_t)COMBO_MODEL_LAYER_XLU;

// TEST ONLY: -1 = ask the archive manager; 0 / 1 = answer that.
int sHostMountOverride = -1;
// TEST ONLY: emit each part's path as the display-list pointer without loading
// it, so a ROM-free row can read the emitted lists back.
bool sHostEmitUnresolved = false;

bool HostPathMounted(const char* dl) {
    if (sHostMountOverride >= 0) {
        return sHostMountOverride == 1;
    }
    static const char kOtr[] = "__OTR__";
    if (dl == nullptr || std::strncmp(dl, kOtr, sizeof(kOtr) - 1) != 0) {
        return false;
    }
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr) {
        return false;
    }
    auto archives = ctx->GetResourceManager()->GetArchiveManager();
    return archives != nullptr && archives->HasFile(std::string(dl + sizeof(kOtr) - 1));
}

bool HostModelMounted(const ComboModel& model) {
    for (uint8_t i = 0; i < model.partCount; i++) {
        if (!HostPathMounted(model.parts[i].dl)) {
            return false;
        }
    }
    return model.partCount > 0;
}

void HostEmitPart(Gfx* pkt, const char* dl) {
    Gfx* list = reinterpret_cast<Gfx*>(const_cast<char*>(dl));
    if (sHostEmitUnresolved) {
        __gSPDisplayList(pkt, list);
    } else {
        gSPDisplayList(pkt, list);
    }
}

} // namespace

// At global scope, not in the anonymous namespace: OPEN_DISPS / CLOSE_DISPS
// re-declare the frame-interpolation calls at block scope, and inside a
// namespace that declaration would name a C++-linkage function of that
// namespace instead of the extern "C" one frame_interpolation.h declares.

/** One layer of the descriptor (see the file header for the order). */
static void OoTHostModel_DrawLayer(PlayState* play, const ComboModel& model, uint8_t layer) {
    const uint8_t setup = layer == kHostOpa ? model.opaSetupDl : model.xluSetupDl;
    if (setup == 0) {
        return; // no part on this layer (Combo_ModelIsWellFormed)
    }
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    const s32 frames = (s32)play->state.frames;
    const ComboModelColor& color = layer == kHostOpa ? model.opaColor : model.xluColor;

    OPEN_DISPS(gfxCtx);
    Gfx*& disp = layer == kHostOpa ? POLY_OPA_DISP : POLY_XLU_DISP;

    disp = OoT_Gfx_SetupDL(disp, setup);
    for (const ComboModelScroll& s : model.scrolls) {
        if (s.segment == 0 || s.layer != layer) {
            continue;
        }
        Gfx* tex = s.twoTiles ? OoT_Gfx_TwoTexScroll(gfxCtx, G_TX_RENDERTILE, (u32)(s.x1 + s.x1PerFrame * frames),
                                                     (u32)(s.y1 + s.y1PerFrame * frames), s.w1, s.h1, 1,
                                                     (u32)(s.x2 + s.x2PerFrame * frames),
                                                     (u32)(s.y2 + s.y2PerFrame * frames), s.w2, s.h2)
                              : OoT_Gfx_TexScroll(gfxCtx, (u32)(s.x1 + s.x1PerFrame * frames),
                                                  (u32)(s.y1 + s.y1PerFrame * frames), s.w1, s.h1);
        gSPSegment(disp++, s.segment, (uintptr_t)tex);
    }
    if (color.set) {
        gDPSetPrimColor(disp++, 0, color.primLodFrac, color.prim[0], color.prim[1], color.prim[2], 255);
        gDPSetEnvColor(disp++, color.env[0], color.env[1], color.env[2], 255);
    }
    if (model.grayscale) {
        gDPSetGrayscaleColor(disp++, model.grayscaleRgb[0], model.grayscaleRgb[1], model.grayscaleRgb[2], 255);
        gSPGrayscale(disp++, true);
    }

    gSPMatrix(disp++, MATRIX_NEWMTX(gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    bool anyBillboard = false;
    for (uint8_t i = 0; i < model.partCount; i++) {
        const ComboModelPart& part = model.parts[i];
        if (part.layer != layer) {
            continue;
        }
        if (part.billboard) {
            anyBillboard = true;
            continue;
        }
        HostEmitPart(disp++, part.dl);
    }
    if (anyBillboard) {
        OoT_Matrix_Push();
        OoT_Matrix_Translate(model.billboardOffset[0], model.billboardOffset[1], model.billboardOffset[2],
                             MTXMODE_APPLY);
        OoT_Matrix_ReplaceRotation(&play->billboardMtxF);
        gSPMatrix(disp++, MATRIX_NEWMTX(gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
        for (uint8_t i = 0; i < model.partCount; i++) {
            const ComboModelPart& part = model.parts[i];
            if (part.layer == layer && part.billboard) {
                HostEmitPart(disp++, part.dl);
            }
        }
        OoT_Matrix_Pop();
    }
    if (model.grayscale) {
        gSPGrayscale(disp++, false);
    }

    CLOSE_DISPS(gfxCtx);
}

/** The descriptor under the current matrix: its own scale, then its rotation
 *  (foreign_model.h), then both layers. */
static void OoTHostModel_Draw(PlayState* play, const ComboModel& model) {
    OoT_Matrix_Push();
    if (model.scale != 1.0f) {
        OoT_Matrix_Scale(model.scale, model.scale, model.scale, MTXMODE_APPLY);
    }
    if (model.rotation[0] != 0 || model.rotation[1] != 0 || model.rotation[2] != 0) {
        OoT_Matrix_RotateZYX(model.rotation[0], model.rotation[1], model.rotation[2], MTXMODE_APPLY);
    }
    OoTHostModel_DrawLayer(play, model, kHostOpa);
    OoTHostModel_DrawLayer(play, model, kHostXlu);
    OoT_Matrix_Pop();
}

/**
 * The drawable model of the MM item OoT check `rc` hosts: 1 and *out when the
 * check hosts an MM item (the placement table, and behind it the crossing store)
 * whose origin answers a DESCRIPTOR for OoT and whose every path a mounted
 * archive holds; 0 and a zeroed *out otherwise (the caller's stand-in).
 */
extern "C" int OoT_ForeignModel_ModelForOoTCheck(uint16_t rc, ComboModel* out) {
    ComboModel scratch;
    ComboModel* model = out != nullptr ? out : &scratch;
    Combo_ModelInit(model);
    if (rc == 0) {
        return 0;
    }
    const SharedItem* item = Combo_GetForeignPlacementForOoTCheck(rc);
    ComboModelAnswer answer;
    if (item == nullptr ||
        Combo_GetForeignItemModel((uint8_t)GAME_OOT, *item, &answer) != COMBO_MODEL_ANSWER_DESCRIPTOR ||
        !HostModelMounted(answer.model)) {
        return 0;
    }
    *model = answer.model;
    return 1;
}

/**
 * Draw the MM model OoT check `rc` hosts under the current matrix: 1 when it was
 * drawn, 0 when there is none to draw (OoT_ForeignModel_ModelForOoTCheck) and
 * the caller keeps its stand-in. Called by the shop shelf (OoT_EnGirlA_Draw).
 */
extern "C" int OoT_ForeignModel_DrawForOoTCheck(PlayState* play, uint16_t rc) {
    ComboModel model;
    if (play == nullptr || OoT_ForeignModel_ModelForOoTCheck(rc, &model) != 1) {
        return 0;
    }
    OoTHostModel_Draw(play, model);
    return 1;
}

// ---- TEST BRIDGES (ForeignModel row M12, src/common/tests/test_foreign_model.c)

extern "C" void OoT_ForeignModel_TestSetMountOverride(int value) {
    sHostMountOverride = value;
}

/** HostPathMounted with the override cleared: the PRODUCTION branch. */
extern "C" int OoT_ForeignModel_TestPathMountedReal(const char* dl) {
    const int saved = sHostMountOverride;
    sHostMountOverride = -1;
    const bool mounted = HostPathMounted(dl);
    sHostMountOverride = saved;
    return mounted ? 1 : 0;
}

namespace {

/** A PlayState with a real GraphicsContext (three arenas) and a real matrix
 *  stack, so OoT's own draw primitives run unmodified; everything it replaces
 *  is restored on destruction. */
struct OoTFakeDrawPlay {
    PlayState* play = nullptr;
    GraphicsContext* gfxCtx = nullptr;
    std::vector<Gfx> opa = std::vector<Gfx>(4096);
    std::vector<Gfx> xlu = std::vector<Gfx>(1024);
    std::vector<Gfx> overlay = std::vector<Gfx>(64);
    MtxF stack[20];
    MtxF* savedStack = nullptr;
    MtxF* savedCurrent = nullptr;
    // Graph_Alloc reads HREG(59) through gGameInfo, which only OoT's own boot
    // allocates; a ROM-free row gets a zeroed one for the draw's duration.
    GameInfo* savedGameInfo = nullptr;
    GameInfo* ownGameInfo = nullptr;

    static void InitArena(TwoHeadGfxArena* arena, std::vector<Gfx>& buf) {
        arena->size = buf.size() * sizeof(Gfx);
        arena->bufp = buf.data();
        arena->p = buf.data();
        arena->d = buf.data() + buf.size();
    }
    static void Identity(MtxF* m) {
        std::memset(m, 0, sizeof(*m));
        for (int i = 0; i < 4; i++) {
            m->mf[i][i] = 1.0f;
        }
    }

    OoTFakeDrawPlay() {
        play = (PlayState*)std::calloc(1, sizeof(PlayState));
        gfxCtx = (GraphicsContext*)std::calloc(1, sizeof(GraphicsContext));
        InitArena(&gfxCtx->polyOpa, opa);
        InitArena(&gfxCtx->polyXlu, xlu);
        InitArena(&gfxCtx->overlay, overlay);
        play->state.gfxCtx = gfxCtx;
        play->state.frames = 7;
        Identity(&play->billboardMtxF);
        savedStack = OoT_sMatrixStack;
        savedCurrent = OoT_sCurrentMatrix;
        OoT_sMatrixStack = stack;
        OoT_sCurrentMatrix = stack;
        Identity(&stack[0]);
        savedGameInfo = gGameInfo;
        if (gGameInfo == nullptr) {
            ownGameInfo = (GameInfo*)std::calloc(1, sizeof(GameInfo));
            gGameInfo = ownGameInfo;
        }
    }
    ~OoTFakeDrawPlay() {
        gGameInfo = savedGameInfo;
        std::free(ownGameInfo);
        OoT_sMatrixStack = savedStack;
        OoT_sCurrentMatrix = savedCurrent;
        std::free(gfxCtx);
        std::free(play);
    }
};

/** The "__OTR__" display lists one layer's command list emits, in order, and
 *  whether a matrix was loaded before the first of them. */
std::vector<const char*> OoTHostReadParts(const Gfx* begin, const Gfx* end, bool* matrixFirst) {
    std::vector<const char*> parts;
    bool matrixSeen = false;
    *matrixFirst = false;
    for (const Gfx* g = begin; g < end; g++) {
        const uint32_t w0 = (uint32_t)g->words.w0;
        const uintptr_t w1 = (uintptr_t)g->words.w1;
        const uint32_t op = (w0 >> 24) & 0xFF;
        if (op == (uint32_t)(uint8_t)G_DL && w1 != 0 && std::memcmp((const void*)w1, "__OTR__", 7) == 0) {
            if (parts.empty()) {
                *matrixFirst = matrixSeen;
            }
            parts.push_back((const char*)w1);
        } else if (op == (uint32_t)(uint8_t)G_MTX) {
            matrixSeen = true;
        }
    }
    return parts;
}

} // namespace

/**
 * TEST BRIDGE (ForeignModel row M12): the shelf's REAL draw call
 * (OoT_ForeignModel_DrawForOoTCheck) for OoT check `rc` into a real OoT
 * GraphicsContext, both layers read back. `want` null: nothing drawn and no
 * model list emitted (the caller's stand-in). Returns 0 on success; prints the
 * failing observation.
 */
extern "C" int OoT_ForeignModel_TestShelfDraw(uint16_t rc, const ComboModel* want) {
    OoTFakeDrawPlay fake;
    sHostEmitUnresolved = true;
    const int drew = OoT_ForeignModel_DrawForOoTCheck(fake.play, rc);
    sHostEmitUnresolved = false;
    if (OoT_sCurrentMatrix != fake.stack) {
        std::printf("[TEST]   M12 the draw left OoT's matrix stack unbalanced\n");
        return 1;
    }
    for (uint8_t layer : { kHostOpa, kHostXlu }) {
        const Gfx* begin = layer == kHostOpa ? fake.opa.data() : fake.xlu.data();
        const Gfx* end = layer == kHostOpa ? fake.gfxCtx->polyOpa.p : fake.gfxCtx->polyXlu.p;
        bool matrixFirst = false;
        const std::vector<const char*> parts = OoTHostReadParts(begin, end, &matrixFirst);
        std::vector<const char*> expected;
        if (want != nullptr) {
            for (uint8_t i = 0; i < want->partCount; i++) {
                if (want->parts[i].layer == layer) {
                    expected.push_back(want->parts[i].dl);
                }
            }
        }
        std::printf("[TEST]   M12 %s: %zu model list(s) emitted, %zu expected%s%s\n", layer == kHostOpa ? "OPA" : "XLU",
                    parts.size(), expected.size(), parts.empty() ? "" : ", first ", parts.empty() ? "" : parts[0]);
        if (parts != expected) {
            std::printf("[TEST]   M12 the shelf draw emitted the wrong model lists on this layer\n");
            return 1;
        }
        if (!expected.empty() && !matrixFirst) {
            std::printf("[TEST]   M12 no matrix loaded before the first model list\n");
            return 1;
        }
    }
    if ((drew == 1) != (want != nullptr)) {
        std::printf("[TEST]   M12 the draw answered %d, want %d\n", drew, want != nullptr ? 1 : 0);
        return 1;
    }
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
