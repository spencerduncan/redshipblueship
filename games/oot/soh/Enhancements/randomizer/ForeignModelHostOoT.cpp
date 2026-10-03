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
 * WHAT IS DRAWN (HostModelForItem, the one answer both consumers draw). A
 * DESCRIPTOR whose every display-list path a mounted archive holds
 * (foreign_model.h, THREE ANSWERS: the answer says nothing about mounts, the
 * consumer checks). MM's archives are added the first time MM is entered in
 * this process (rsbs/src/main.cpp, Combo_EnsureGameArchivesLoaded), and
 * redship-oot.o2r carries the MM content #577 M6 curates for OoT; an MM model
 * none of them holds answers "no model" here. A HOST_NATIVE answer (a colliding
 * MM model OoT's host-native table maps to OoT's own draw row, #577 M7) draws
 * that row, re-expressed by path from OoT's own recipe for it
 * (OoT_ComboModel_DrawRowModel; #832), under the same mount check. In every
 * "no model" case the caller keeps its stand-in.
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
#include "soh/Enhancements/custom-message/CustomMessageTypes.h" // TEXT_RANDOMIZER_CUSTOM_ITEM
#include "soh/Enhancements/randomizer/randomizerTypes.h"        // MOD_RANDOMIZER, RG_BLUE_RUPEE
#include "soh/Enhancements/randomizer/draw.h"                   // Randomizer_DrawRocsFeather (the M13 refusal)
#include "soh/Enhancements/randomizer/static_data.h"            // RetrieveItem (the chest-game prize bridge)
#include "ForeignModelHostOoT.h"

extern "C" {
#include <z64.h>
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "src/overlays/actors/ovl_Item_Etcetera/z_item_etcetera.h" // the chest-game prize bridge
// sys_matrix.c's stack (no header declares it): the test bridge below gives the
// draw a private stack and puts these back.
extern MtxF* OoT_sMatrixStack;
extern MtxF* OoT_sCurrentMatrix;
// OTRGlobals.cpp; OTRGlobals.h declares it for C translation units only.
GetItemEntry GetItemMystery();
}

#include "context.h"       // src/common — GameId
#include "foreign_items.h" // src/common — Combo_GetForeignPlacementForOoTCheck
#include "foreign_model.h" // src/common — the registry

// ForeignModelOoT.cpp: OoT's own draw row for a HOST_NATIVE answer, as a
// descriptor of the lists OoT's own recipe draws for it.
extern "C" int OoT_ComboModel_DrawRowModel(int drawId, ComboModel* out);

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

/** The model OoT draws, as the host, for MM item `item`: MM's DESCRIPTOR, or for
 *  a colliding MM model OoT's host-native table maps (#577 M7), OoT's OWN row as
 *  OoT's own recipe draws it (#832); either way only when a mounted archive holds
 *  every path. 0 and a zeroed *out otherwise (the caller's stand-in). */
bool HostModelForItem(const SharedItem& item, ComboModel* out) {
    Combo_ModelInit(out);
    ComboModelAnswer answer;
    ComboModel model;
    const uint8_t kind = Combo_GetForeignItemModel((uint8_t)GAME_OOT, item, &answer);
    if (kind == COMBO_MODEL_ANSWER_DESCRIPTOR) {
        model = answer.model;
    } else if (kind == COMBO_MODEL_ANSWER_HOST_NATIVE) {
        if (OoT_ComboModel_DrawRowModel((int)answer.hostKey, &model) != 1) {
            return false;
        }
    } else {
        return false;
    }
    if (!HostModelMounted(model)) {
        return false;
    }
    *out = model;
    return true;
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
 * that HostModelForItem can draw (MM's DESCRIPTOR, or OoT's own row for a
 * HOST_NATIVE answer, every path mounted); 0 and a zeroed *out otherwise (the
 * caller's stand-in).
 */
extern "C" int OoT_ForeignModel_ModelForOoTCheck(uint16_t rc, ComboModel* out) {
    ComboModel scratch;
    ComboModel* model = out != nullptr ? out : &scratch;
    Combo_ModelInit(model);
    if (rc == 0) {
        return 0;
    }
    const SharedItem* item = Combo_GetForeignPlacementForOoTCheck(rc);
    return item != nullptr && HostModelForItem(*item, model) ? 1 : 0;
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

// ============================================================================
// #577 M4: OoT'S GET-ITEM CUTSCENE SHOWS THE MM ITEM AND GIVES NOTHING
// ============================================================================
//
// THE COUPLING. OoT's get-item cutscene grants on its first frame what it holds
// up: func_8084DFF4 (z_player.c) calls OoT_Item_Give or Randomizer_Item_Give on
// the very entry it shows. An MM item on an OoT check is MM's to give: the
// RC-queue drain records its crossing (OoT_Rando_Foreign_RecordPickup) and MM
// redeems it on the next arrival in Termina. So until now the drain skipped the
// cutscene and showed a toast (hook_handlers.cpp).
//
// THE DISPLAY-WITHOUT-GRANT PATH: one entry, built once, taken once.
//   1. BUILD (the drain, after the crossing is recorded): a GetItemEntry whose
//      drawFunc is ShowOnlyDraw, armed for one take. It carries a valid object
//      (the get-item DMA still reads OoT_gObjectTable[objectId]), a draw id so
//      the cutscene draws at all, the custom-item text id, the MAJOR category
//      (so "skip junk get-item animations" does not drop it as a collectible)
//      and MOD_RANDOMIZER (so the cutscene plays the item fanfare). Its item id
//      is OoT's foreign junk cover, the item the OoT table holds at every
//      crossing host (ComboLogicEngineOoT.cpp, kOoTForeignJunkCover): it never
//      reaches a give, and if a future give point forgot to ask (2), it would
//      give nothing of the MM item's identity.
//   2. TAKE (the give point, func_8084DFF4): asked before every give. Only the
//      armed entry, recognised by its draw function and its item ids, answers 1;
//      the give is then skipped, the arm is spent, and the drain's queue slot is
//      released (Randomizer_ReleaseQueuedShowOnly), which is what the
//      item-receive hook does for an entry that IS given. Every other entry,
//      custom-drawn or not, answers 0 and is given as before.
//   3. DRAW (the cutscene, OoT_Player_DrawGetItemImpl calls the entry's
//      drawFunc under its own 0.2-scale matrix): what the shelf draws too
//      (HostModelForItem): the MM model (a mounted DESCRIPTOR), or for a
//      colliding MM model OoT's host-native table maps (#577 M7), OoT's OWN row
//      for it, re-expressed by path the same way. Neither: the mystery item,
//      the stand-in the shelf uses too.
//   4. TEXT (Messages/ItemMessages.cpp): "You found <article><name>!" from MM's
//      describer, the name the toast used.
//
// ONE ARMED ENTRY AT A TIME is all the drain can produce: it builds only when
// its queue slot is free and the player is in no item cutscene, and the slot is
// released only by the take.

// hook_handlers.cpp: release the RC queue's slot held by show-only check `rc`.
void Randomizer_ReleaseQueuedShowOnly(uint16_t rc);

static void ShowOnlyDraw(PlayState* play, GetItemEntry* entry);

namespace {

struct ShowOnlyState {
    bool armed = false;    // built, not yet taken
    uint16_t rc = 0;       // the OoT check it was built for
    SharedItem item = {};  // the MM item it shows
    bool hasModel = false; // false: the mystery stand-in
    ComboModel model = {};
    GetItemEntry entry = {};
};
ShowOnlyState sShowOnly;

bool IsShowOnlyEntry(const GetItemEntry* entry) {
    return entry != nullptr && entry->drawFunc == ShowOnlyDraw && entry->modIndex == sShowOnly.entry.modIndex &&
           entry->itemId == sShowOnly.entry.itemId && entry->getItemId == sShowOnly.entry.getItemId;
}

} // namespace

/** The cutscene's draw (see the section header, 3). */
static void ShowOnlyDraw(PlayState* play, GetItemEntry* entry) {
    (void)entry;
    if (play == nullptr) {
        return;
    }
    if (sShowOnly.hasModel) {
        OoTHostModel_Draw(play, sShowOnly.model);
        return;
    }
    GetItemEntry_Draw(play, GetItemMystery());
}

extern "C" int OoT_Rando_Foreign_BuildShowOnlyGetItem(uint16_t rc, GetItemEntry* out) {
    const SharedItem* item = rc != 0 ? Combo_GetForeignPlacementForOoTCheck(rc) : nullptr;
    if (item == nullptr || out == nullptr) {
        return 0;
    }
    GetItemEntry entry = GET_ITEM(RG_BLUE_RUPEE, OBJECT_GI_RUPY, GID_RUPEE_BLUE, TEXT_RANDOMIZER_CUSTOM_ITEM, 0x80,
                                  CHEST_ANIM_LONG, ITEM_CATEGORY_MAJOR, MOD_RANDOMIZER, RG_BLUE_RUPEE);
    entry.drawFunc = ShowOnlyDraw;
    sShowOnly.armed = true;
    sShowOnly.rc = rc;
    sShowOnly.item = *item;
    sShowOnly.hasModel = HostModelForItem(*item, &sShowOnly.model);
    sShowOnly.entry = entry;
    *out = entry;
    return 1;
}

extern "C" int OoT_Rando_Foreign_TakeShowOnlyGetItem(const GetItemEntry* entry) {
    if (!sShowOnly.armed || !IsShowOnlyEntry(entry)) {
        return 0;
    }
    sShowOnly.armed = false;
    Randomizer_ReleaseQueuedShowOnly(sShowOnly.rc);
    const char* name = Combo_GetForeignItemName(sShowOnly.item);
    std::fprintf(stderr, "[OoT] get-item cutscene shows the MM item %s from check %u and gives nothing (#577 M4)\n",
                 name != nullptr ? name : "?", (unsigned)sShowOnly.rc);
    std::fflush(stderr);
    return 1;
}

extern "C" int OoT_Rando_Foreign_ShowOnlyItemText(const GetItemEntry* entry, const char** article, const char** name) {
    if (!IsShowOnlyEntry(entry)) {
        return 0;
    }
    const char* a = Combo_GetForeignItemArticle(sShowOnly.item);
    const char* n = Combo_GetForeignItemName(sShowOnly.item);
    if (article != nullptr) {
        *article = a != nullptr ? a : "";
    }
    if (name != nullptr) {
        *name = n != nullptr ? n : "a Majora's Mask item";
    }
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

// hook_handlers.cpp: the treasure chest game's prize display, seen through the
// Lens of Truth (the drawFunc the actor-init hook gives it).
void ItemEtcetera_DrawRandomizedItemThroughLens(ItemEtcetera* itemEtcetera, PlayState* play);

/**
 * TEST BRIDGE (combo-single-bag leg G, #800 pass 2): the treasure chest game's
 * prize display as EnChanger spawns it above the final chest (ACTOR_ITEM_ETCETERA,
 * params (0x0A << 8) + ITEM_ETC_HEART_PIECE_CHEST_GAME, in SCENE_TREASURE_BOX_SHOP),
 * seen through the Lens of Truth: its REAL randomized draw into a real OoT
 * GraphicsContext, holding the entry the actor-init hook gives a display whose
 * check holds OoT's junk cover (RG_BLUE_RUPEE, what every crossing host holds),
 * with every path mounted (test override). Both layers must emit exactly
 * `want`'s lists. Returns 0 on success; prints the failing observation.
 */
extern "C" int OoT_ForeignModel_TestChestGamePrizeDraw(const ComboModel* want) {
    if (want == nullptr) {
        return 1;
    }
    OoTFakeDrawPlay fake;
    fake.play->sceneNum = SCENE_TREASURE_BOX_SHOP;
    fake.play->actorCtx.lensActive = 1;
    ItemEtcetera* prize = (ItemEtcetera*)std::calloc(1, sizeof(ItemEtcetera));
    prize->actor.id = ACTOR_ITEM_ETCETERA;
    prize->actor.params = (0x0A << 8) + ITEM_ETC_HEART_PIECE_CHEST_GAME;
    prize->sohItemEntry = Rando::StaticData::RetrieveItem(RG_BLUE_RUPEE).GetGIEntry_Copy();
    const int savedMount = sHostMountOverride;
    sHostMountOverride = 1;
    sHostEmitUnresolved = true;
    ItemEtcetera_DrawRandomizedItemThroughLens(prize, fake.play);
    sHostEmitUnresolved = false;
    sHostMountOverride = savedMount;
    std::free(prize);
    if (OoT_sCurrentMatrix != fake.stack) {
        std::printf("[TEST]   chest-game prize: the draw left OoT's matrix stack unbalanced\n");
        return 1;
    }
    int result = 0;
    for (uint8_t layer : { kHostOpa, kHostXlu }) {
        const Gfx* begin = layer == kHostOpa ? fake.opa.data() : fake.xlu.data();
        const Gfx* end = layer == kHostOpa ? fake.gfxCtx->polyOpa.p : fake.gfxCtx->polyXlu.p;
        bool matrixFirst = false;
        const std::vector<const char*> parts = OoTHostReadParts(begin, end, &matrixFirst);
        std::vector<const char*> expected;
        for (uint8_t i = 0; i < want->partCount; i++) {
            if (want->parts[i].layer == layer) {
                expected.push_back(want->parts[i].dl);
            }
        }
        std::printf("[TEST]   chest-game prize %s: %zu model list(s) emitted, %zu expected%s%s\n",
                    layer == kHostOpa ? "OPA" : "XLU", parts.size(), expected.size(), parts.empty() ? "" : ", first ",
                    parts.empty() ? "" : parts[0]);
        if (parts != expected) {
            std::printf("[TEST]   chest-game prize: the Lens display emitted the wrong model lists on this layer\n");
            result = 1;
        }
    }
    return result;
}

// hook_handlers.cpp: the drain's show-only queueing and its queue slot.
extern "C" int OoT_Rando_Foreign_TestQueueShowOnly(uint16_t rc, GetItemEntry* queued);
extern "C" int OoT_Rando_Foreign_TestQueuedCheck(void);

/**
 * TEST BRIDGE (ForeignModel row M13, #577 M4): OoT's show-only get-item path for
 * OoT check `rc`, end to end short of a live Player.
 *   1. The DRAIN's queueing (RandomizerQueueForeignShowOnly, hook_handlers.cpp)
 *      takes the queue slot with the built entry, or, `wantEntry` 0, does not.
 *   2. The entry is one OoT's get-item cutscene shows: the show-only draw, a
 *      valid object, a draw id, the custom-item text, MOD_RANDOMIZER, MAJOR.
 *   3. The TEXT names `wantName`.
 *   4. The GIVE POINT's take refuses an ordinary custom-drawn randomizer entry
 *      (Roc's Feather's draw on the same ids) and the entry with its draw
 *      cleared, claims the entry once, releases the queue slot, and refuses it
 *      a second time.
 *   5. The entry's own draw function, into a real OoT GraphicsContext, emits
 *      exactly `want`'s lists (a matrix first); `want` null: the entry draws the
 *      mystery stand-in (not run here: it loads its resources), checked as "no
 *      model armed".
 * Returns 0 on success; prints the failing observation.
 */
extern "C" int OoT_ForeignModel_TestShowOnlyGetItem(uint16_t rc, int wantEntry, const ComboModel* want,
                                                    const char* wantName) {
    GetItemEntry entry = {};
    const int queued = OoT_Rando_Foreign_TestQueueShowOnly(rc, &entry);
    const int slot = OoT_Rando_Foreign_TestQueuedCheck();
    std::printf("[TEST]   M13 drain: show-only entry %s, queue slot holds check %d\n", queued ? "QUEUED" : "not queued",
                slot);
    if (!wantEntry) {
        if (queued != 0 || slot == (int)rc) {
            std::printf("[TEST]   M13 a check with no MM item queued a show-only entry\n");
            return 1;
        }
        return 0;
    }
    if (queued != 1 || slot != (int)rc) {
        std::printf("[TEST]   M13 the drain queued no show-only entry for an OoT check hosting an MM item (the "
                    "toast-only presentation)\n");
        return 1;
    }
    std::printf("[TEST]   M13 entry: draw %s, object %u, gi %d, text 0x%04X, mod %u, category %u, item %u/%d\n",
                entry.drawFunc == ShowOnlyDraw ? "show-only" : "OTHER", (unsigned)entry.objectId, (int)entry.gi,
                (unsigned)entry.textId, (unsigned)entry.modIndex, (unsigned)entry.getItemCategory,
                (unsigned)entry.itemId, (int)entry.getItemId);
    if (entry.drawFunc != ShowOnlyDraw || entry.objectId == OBJECT_INVALID || entry.objectId >= OBJECT_ID_MAX ||
        entry.gi == 0 || !entry.collectable || entry.textId != TEXT_RANDOMIZER_CUSTOM_ITEM ||
        entry.modIndex != MOD_RANDOMIZER || entry.getItemCategory != ITEM_CATEGORY_MAJOR || entry.getItemId <= 0 ||
        entry.getItemId >= RG_MAX) {
        std::printf("[TEST]   M13 the entry is not one OoT's get-item cutscene shows as a major item\n");
        return 1;
    }

    const char* article = nullptr;
    const char* name = nullptr;
    if (OoT_Rando_Foreign_ShowOnlyItemText(&entry, &article, &name) != 1 || name == nullptr ||
        std::strcmp(name, wantName) != 0) {
        std::printf("[TEST]   M13 the textbox names \"%s\", want \"%s\"\n", name != nullptr ? name : "(none)",
                    wantName);
        return 1;
    }
    std::printf("[TEST]   M13 textbox: You found %s%s!\n", article != nullptr ? article : "", name);

    GetItemEntry feather = entry;
    feather.drawFunc = Randomizer_DrawRocsFeather;
    GetItemEntry plain = entry;
    plain.drawFunc = nullptr;
    const int takeFeather = OoT_Rando_Foreign_TakeShowOnlyGetItem(&feather);
    const int takePlain = OoT_Rando_Foreign_TakeShowOnlyGetItem(&plain);
    const int takeNull = OoT_Rando_Foreign_TakeShowOnlyGetItem(nullptr);
    const int slotBeforeTake = OoT_Rando_Foreign_TestQueuedCheck();
    const int take = OoT_Rando_Foreign_TakeShowOnlyGetItem(&entry);
    const int slotAfterTake = OoT_Rando_Foreign_TestQueuedCheck();
    const int takeAgain = OoT_Rando_Foreign_TakeShowOnlyGetItem(&entry);
    std::printf("[TEST]   M13 give point: feather %d, no draw %d, null %d, the entry %d (slot %d -> %d), again %d\n",
                takeFeather, takePlain, takeNull, take, slotBeforeTake, slotAfterTake, takeAgain);
    if (takeFeather != 0 || takePlain != 0 || takeNull != 0) {
        std::printf("[TEST]   M13 the give point's take claimed an entry that is not the show-only one\n");
        return 1;
    }
    if (take != 1 || slotBeforeTake != (int)rc || slotAfterTake != (int)RC_UNKNOWN_CHECK || takeAgain != 0) {
        std::printf("[TEST]   M13 the take did not claim the entry once and release the drain's slot\n");
        return 1;
    }

    if (want == nullptr) {
        if (sShowOnly.hasModel) {
            std::printf("[TEST]   M13 a model was armed where the stand-in is due\n");
            return 1;
        }
        std::printf("[TEST]   M13 draw: the mystery stand-in\n");
        return 0;
    }
    if (!sShowOnly.hasModel) {
        // Checked before drawing: the stand-in's draw loads its resources.
        std::printf("[TEST]   M13 the entry would draw the mystery stand-in where a model is due\n");
        return 1;
    }
    OoTFakeDrawPlay fake;
    sHostEmitUnresolved = true;
    entry.drawFunc(fake.play, &entry);
    sHostEmitUnresolved = false;
    if (OoT_sCurrentMatrix != fake.stack) {
        std::printf("[TEST]   M13 the draw left OoT's matrix stack unbalanced\n");
        return 1;
    }
    for (uint8_t layer : { kHostOpa, kHostXlu }) {
        const Gfx* begin = layer == kHostOpa ? fake.opa.data() : fake.xlu.data();
        const Gfx* end = layer == kHostOpa ? fake.gfxCtx->polyOpa.p : fake.gfxCtx->polyXlu.p;
        bool matrixFirst = false;
        const std::vector<const char*> parts = OoTHostReadParts(begin, end, &matrixFirst);
        std::vector<const char*> expected;
        for (uint8_t i = 0; i < want->partCount; i++) {
            if (want->parts[i].layer == layer) {
                expected.push_back(want->parts[i].dl);
            }
        }
        std::printf("[TEST]   M13 draw %s: %zu model list(s) emitted, %zu expected%s%s\n",
                    layer == kHostOpa ? "OPA" : "XLU", parts.size(), expected.size(), parts.empty() ? "" : ", first ",
                    parts.empty() ? "" : parts[0]);
        if (parts != expected) {
            std::printf("[TEST]   M13 the get-item cutscene's draw emitted the wrong model lists on this layer\n");
            return 1;
        }
        if (!expected.empty() && !matrixFirst) {
            std::printf("[TEST]   M13 no matrix loaded before the first model list\n");
            return 1;
        }
    }
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
