/**
 * @file ForeignModelSingleExe.cpp
 * @brief MM's answer to "which get-item model does this RI_* show in the other
 *        game?" (#577 M2), and MM's host-native mapper for colliding models.
 *
 * Registered into src/common/foreign_model.h at static-init time. OoT will call
 * it (through Combo_GetForeignItemModel) when it draws an MM item on an OoT
 * check; #577 M4 is that consumer.
 *
 * WHICH MODEL. The one MM itself draws: Rando::DrawItem's default branch draws
 * the row of MM_sDrawItemTable that the item's static `Items[ri].drawId` names,
 * and the row's draw function gives its lists their shape. This file never
 * CALLS a draw function (they take MM's PlayState and bind scrolling textures
 * through MM's graph allocator); it re-expresses each draw function as a
 * ComboModel recipe below, one per function, read off games/mm/src/code/
 * z_draw.c. Rows that answer no model, by name: the three empty rows (GID_37,
 * GID_46, GID_4C), the seahorse and the bottled fairy (they multiply in a
 * matrix RESOURCE), the Moon's Tear (an animated material) and the four
 * remains (they bind OBJECT_BSMASK's object slot to segment 6).
 *
 * Rando::DrawItem's special cases:
 *  - small and boss keys, Double Defense, the milk refill and the Gold Skulltula
 *    tokens tint the model of their own `drawId` row (the key tints ride a
 *    runtime-patched copy of the list, which has no archive path): they answer
 *    that row, the vanilla model;
 *  - progressive items answer their static `drawId` (the first tier; Rando::
 *    ConvertItem reads live inventory, which belongs to a suspended game here);
 *  - every other special case (songs, stray fairies, owl statues, clocks, souls,
 *    frogs, the swim ability, the Triforce piece, traps, ocarina buttons) is a
 *    2S2H custom draw and answers no model.
 *
 * WHAT IT NEVER DOES: read the save, a CVar or any live state. The answer is a
 * pure function of static tables (ForeignModel row).
 *
 * THE HOST SIDE (#577 M3), below the source: MM draws an OoT item's model in
 * its own get-item cutscene. CheckQueue's foreign draw asks
 * Rando::Foreign::DrawForeignModelForCheck (ForeignModel.h), which takes OoT's
 * DESCRIPTOR for the item the check hosts and re-expresses its shape with MM's
 * own primitives (setup list, texture scrolls, colours, matrices), naming each
 * display list by its path. A colliding OoT model that MM's host-native table
 * maps (#577 M7) draws MM's OWN row instead, through the same primitives and
 * MM's own recipe for that row, or, for an OoT song MM has no row for, MM's
 * host-side tinted note (#830, DrawSong's shape). Anything else (no placement, no model, a
 * colliding model the table answers "no model" for, a path no mounted archive
 * holds) draws nothing and leaves CheckQueue's model-less stand-in.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <string>
#include <variant>
#include <vector>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/ForeignModel.h"
// C linkage for the frame-interpolation calls OPEN_DISPS / CLOSE_DISPS make.
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
// Test bridges only: the ForeignModel row drives CheckQueue's real foreign draw.
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/MiscBehavior/MiscBehavior.h"

extern "C" {
#include "variables.h" // MM_gPlayState, MM_sMatrixStack, MM_sCurrentMatrix
#include "functions.h"
#include "objects/object_gi_melody/object_gi_melody.h" // gGiSongNoteDL (#830)
// Test bridges only (#800): the shop shelf actor the shelf-draw bridge stands up.
#include "overlays/actors/ovl_En_GirlA/z_en_girla.h"
}

// Test bridges only (#800): the two shop draws the shelf-draw bridge drives
// (ActorBehavior/EnGirlA.cpp, ActorBehavior/EnSob1.cpp).
void EnGirlA_RandoDrawFunc(Actor* actor, PlayState* play);
void EnSob1_DrawCustomItem(Actor* thisx, PlayState* play);

// CustomMessage.cpp's file-scope message (the test bridge restores it).
extern CustomMessage::Entry activeCustomMessage;

// src/common. Outside any extern "C" block: each header manages its own linkage.
#include "context.h"
#include "crossing_store.h"
#include "foreign_items.h"
#include "foreign_model.h"

typedef void (*MMGetItemDrawFn)(PlayState*, s16);

// games/mm/src/code/z_draw.c (the single-exe accessor and the row draw
// functions this file recognises by address; none of them is ever called here).
extern "C" {
s32 MM_GetItem_DrawTableCount(void);
s32 MM_GetItem_DrawTableRow(s16 drawId, MMGetItemDrawFn* drawFunc, void* const** drawResources);
void GetItem_DrawBombchu(PlayState* play, s16 drawId);
void MM_GetItem_DrawPoes(PlayState* play, s16 drawId);
void GetItem_DrawFairyBottle(PlayState* play, s16 drawId);
void MM_GetItem_DrawSkullToken(PlayState* play, s16 drawId);
void MM_GetItem_DrawCompass(PlayState* play, s16 drawId);
void MM_GetItem_DrawPotion(PlayState* play, s16 drawId);
void MM_GetItem_DrawGoronSword(PlayState* play, s16 drawId);
void MM_GetItem_DrawDekuNuts(PlayState* play, s16 drawId);
void MM_GetItem_DrawRecoveryHeart(PlayState* play, s16 drawId);
void MM_GetItem_DrawFish(PlayState* play, s16 drawId);
void MM_GetItem_DrawOpa0(PlayState* play, s16 drawId);
void MM_GetItem_DrawOpa0Xlu1(PlayState* play, s16 drawId);
void GetItem_DrawOpa01(PlayState* play, s16 drawId);
void MM_GetItem_DrawXlu01(PlayState* play, s16 drawId);
void GetItem_DrawSeahorse(PlayState* play, s16 drawId);
void GetItem_DrawFairyContainer(PlayState* play, s16 drawId);
void GetItem_DrawMoonsTear(PlayState* play, s16 drawId);
void MM_GetItem_DrawMagicArrow(PlayState* play, s16 drawId);
void GetItem_DrawUpgrades(PlayState* play, s16 drawId);
void GetItem_DrawRupee(PlayState* play, s16 drawId);
void MM_GetItem_DrawSmallRupee(PlayState* play, s16 drawId);
void MM_GetItem_DrawWallet(PlayState* play, s16 drawId);
void GetItem_DrawRemains(PlayState* play, s16 drawId);
}

namespace {

constexpr uint8_t kOpa = (uint8_t)COMBO_MODEL_LAYER_OPA;
constexpr uint8_t kXlu = (uint8_t)COMBO_MODEL_LAYER_XLU;

struct MMModelStep {
    uint8_t list; // index into the row's drawResources
    uint8_t layer;
    uint8_t billboard;
};

/** One tile-pair scroll as z_draw.c's MM_Gfx_TwoTexScroll arguments give it. */
ComboModelScroll TwoTileScroll(uint8_t segment, uint8_t layer, int16_t x1PerFrame, int16_t y1PerFrame, uint16_t w1,
                               uint16_t h1, int16_t x2PerFrame, int16_t y2PerFrame, uint16_t w2, uint16_t h2) {
    ComboModelScroll s = {};
    s.segment = segment;
    s.layer = layer;
    s.twoTiles = 1;
    s.x1PerFrame = x1PerFrame;
    s.y1PerFrame = y1PerFrame;
    s.w1 = w1;
    s.h1 = h1;
    s.x2PerFrame = x2PerFrame;
    s.y2PerFrame = y2PerFrame;
    s.w2 = w2;
    s.h2 = h2;
    return s;
}

/** Append the steps' lists from the row; 0 if a step names an empty list. */
int AddSteps(ComboModel* out, void* const* res, std::initializer_list<MMModelStep> steps) {
    for (const MMModelStep& step : steps) {
        const char* dl = static_cast<const char*>(res[step.list]);
        if (dl == nullptr || Combo_ModelAddPart(out, dl, step.layer, step.billboard) != 1) {
            return 0;
        }
    }
    return 1;
}

/** Setup lists for the layers the parts use: 25 unless the draw says otherwise. */
void DefaultSetups(ComboModel* out) {
    for (uint8_t i = 0; i < out->partCount; i++) {
        if (out->parts[i].layer == kOpa && out->opaSetupDl == 0) {
            out->opaSetupDl = 25;
        }
        if (out->parts[i].layer == kXlu && out->xluSetupDl == 0) {
            out->xluSetupDl = 25;
        }
    }
}

/**
 * The recipe for one draw-table row: 1 and *out, or 0 and *reason. Each branch
 * mirrors the named z_draw.c function's emission order (OPA and XLU are separate
 * lists, so only the order within a layer is meaningful).
 */
int RecipeForRow(MMGetItemDrawFn fn, void* const* d, ComboModel* out, const char** reason) {
    Combo_ModelInit(out);
    *reason = nullptr;
    int ok = 0;
    if (fn == MM_GetItem_DrawOpa0) {
        ok = AddSteps(out, d, { { 0, kOpa, 0 } });
    } else if (fn == MM_GetItem_DrawOpa0Xlu1) {
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 } });
    } else if (fn == GetItem_DrawOpa01) {
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kOpa, 0 } });
    } else if (fn == MM_GetItem_DrawXlu01) {
        ok = AddSteps(out, d, { { 0, kXlu, 0 }, { 1, kXlu, 0 } });
    } else if (fn == GetItem_DrawBombchu) {
        out->opaSetupDl = 23;
        ok = AddSteps(out, d, { { 0, kOpa, 0 } });
    } else if (fn == MM_GetItem_DrawCompass) {
        out->xluSetupDl = 5;
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 } });
    } else if (fn == MM_GetItem_DrawMagicArrow) {
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 }, { 2, kXlu, 0 } });
    } else if (fn == GetItem_DrawUpgrades) {
        ok = AddSteps(out, d, { { 1, kOpa, 0 }, { 0, kOpa, 0 }, { 2, kOpa, 0 }, { 3, kOpa, 0 } });
    } else if (fn == GetItem_DrawRupee) {
        ok = AddSteps(out, d, { { 1, kOpa, 0 }, { 0, kOpa, 0 }, { 3, kXlu, 0 }, { 2, kXlu, 0 } });
    } else if (fn == MM_GetItem_DrawSmallRupee) {
        out->scale = 0.7f;
        ok = AddSteps(out, d, { { 1, kOpa, 0 }, { 0, kOpa, 0 }, { 3, kXlu, 0 }, { 2, kXlu, 0 } });
    } else if (fn == MM_GetItem_DrawWallet) {
        ok = AddSteps(out, d,
                      { { 1, kOpa, 0 },
                        { 0, kOpa, 0 },
                        { 2, kOpa, 0 },
                        { 3, kOpa, 0 },
                        { 4, kOpa, 0 },
                        { 5, kOpa, 0 },
                        { 6, kOpa, 0 },
                        { 7, kOpa, 0 } });
    } else if (fn == MM_GetItem_DrawPotion) {
        out->scrolls[0] = TwoTileScroll(8, kOpa, -1, 1, 32, 32, -1, 1, 32, 32);
        ok = AddSteps(
            out, d, { { 1, kOpa, 0 }, { 0, kOpa, 0 }, { 2, kOpa, 0 }, { 3, kOpa, 0 }, { 4, kXlu, 0 }, { 5, kXlu, 0 } });
    } else if (fn == MM_GetItem_DrawGoronSword) {
        out->scrolls[0] = TwoTileScroll(8, kOpa, 1, 0, 32, 32, 0, 0, 32, 32);
        ok = AddSteps(out, d, { { 0, kOpa, 0 } });
    } else if (fn == MM_GetItem_DrawDekuNuts) {
        out->scrolls[0] = TwoTileScroll(8, kOpa, 6, 6, 32, 32, 6, 6, 32, 32);
        ok = AddSteps(out, d, { { 0, kOpa, 0 } });
    } else if (fn == MM_GetItem_DrawRecoveryHeart) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 0, -3, 32, 32, 0, -2, 32, 32);
        ok = AddSteps(out, d, { { 0, kXlu, 0 } });
    } else if (fn == MM_GetItem_DrawFish) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 0, 1, 32, 32, 0, 1, 32, 32);
        ok = AddSteps(out, d, { { 0, kXlu, 0 } });
    } else if (fn == MM_GetItem_DrawSkullToken) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 0, -5, 32, 32, 0, 0, 32, 64);
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 } });
    } else if (fn == MM_GetItem_DrawPoes) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 0, 0, 16, 32, 1, -6, 16, 32);
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 }, { 3, kXlu, 1 }, { 2, kXlu, 1 } });
    } else if (fn == GetItem_DrawFairyBottle) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 0, 0, 32, 32, 1, -6, 32, 32);
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 }, { 2, kXlu, 1 } });
    } else if (fn == GetItem_DrawSeahorse) {
        *reason = "seahorse: multiplies in a matrix resource before its billboard";
        return 0;
    } else if (fn == GetItem_DrawFairyContainer) {
        *reason = "bottled fairy: animated material and a matrix resource";
        return 0;
    } else if (fn == GetItem_DrawMoonsTear) {
        *reason = "Moon's Tear: animated material";
        return 0;
    } else if (fn == GetItem_DrawRemains) {
        *reason = "remains: binds OBJECT_BSMASK's object slot to segment 6";
        return 0;
    } else {
        *reason = "draw function without a recipe";
        return 0;
    }
    if (!ok) {
        *reason = "row has no list for its recipe";
        return 0;
    }
    DefaultSetups(out);
    if (!Combo_ModelIsWellFormed(out)) {
        *reason = "recipe produced a malformed model";
        return 0;
    }
    return 1;
}

int ModelForDrawId(int drawId, ComboModel* out, const char** reason) {
    MMGetItemDrawFn fn = nullptr;
    void* const* res = nullptr;
    if (drawId < 0 || drawId > INT16_MAX || MM_GetItem_DrawTableRow((s16)drawId, &fn, &res) != 1) {
        Combo_ModelInit(out);
        *reason = "no such draw row";
        return 0;
    }
    return RecipeForRow(fn, res, out, reason);
}

/** Rando::DrawItem's special cases that are 2S2H custom draws (DrawItem.cpp). */
bool IsCustomDrawn(RandoItemId ri) {
    if (ri >= RI_SOUL_BOSS_GOHT && ri <= RI_SOUL_BOSS_TWINMOLD) {
        return true;
    }
    if (ri >= RI_SOUL_ENEMY_ALIEN && ri <= RI_SOUL_ENEMY_WOLFOS) {
        return true;
    }
    switch (ri) {
        case RI_SONG_TIME:
        case RI_SONG_STORMS:
        case RI_SONG_SUN:
        case RI_SONG_HEALING:
        case RI_SONG_SOARING:
        case RI_SONG_SONATA:
        case RI_SONG_ELEGY:
        case RI_SONG_LULLABY_INTRO:
        case RI_SONG_LULLABY:
        case RI_SONG_OATH:
        case RI_SONG_EPONA:
        case RI_SONG_NOVA:
        case RI_CLOCK_TOWN_STRAY_FAIRY:
        case RI_WOODFALL_STRAY_FAIRY:
        case RI_SNOWHEAD_STRAY_FAIRY:
        case RI_GREAT_BAY_STRAY_FAIRY:
        case RI_STONE_TOWER_STRAY_FAIRY:
        case RI_OWL_CLOCK_TOWN_SOUTH:
        case RI_OWL_GREAT_BAY_COAST:
        case RI_OWL_IKANA_CANYON:
        case RI_OWL_MILK_ROAD:
        case RI_OWL_MOUNTAIN_VILLAGE:
        case RI_OWL_SNOWHEAD:
        case RI_OWL_SOUTHERN_SWAMP:
        case RI_OWL_STONE_TOWER:
        case RI_OWL_WOODFALL:
        case RI_OWL_ZORA_CAPE:
        case RI_TIME_DAY_1:
        case RI_TIME_NIGHT_1:
        case RI_TIME_DAY_2:
        case RI_TIME_NIGHT_2:
        case RI_TIME_DAY_3:
        case RI_TIME_NIGHT_3:
        case RI_TIME_PROGRESSIVE:
        case RI_FROG_BLUE:
        case RI_FROG_CYAN:
        case RI_FROG_PINK:
        case RI_FROG_WHITE:
        case RI_ABILITY_SWIM:
        case RI_TRIFORCE_PIECE_PREVIOUS:
        case RI_TRIFORCE_PIECE:
        case RI_TRAP:
        case RI_MAX_TRAP:
        case RI_OCARINA_BUTTON_A:
        case RI_OCARINA_BUTTON_C_DOWN:
        case RI_OCARINA_BUTTON_C_LEFT:
        case RI_OCARINA_BUTTON_C_RIGHT:
        case RI_OCARINA_BUTTON_C_UP:
            return true;
        default:
            return false;
    }
}

int ModelForItem(uint16_t id, ComboModel* out, const char** reason) {
    Combo_ModelInit(out);
    *reason = nullptr;
    const RandoItemId ri = (RandoItemId)id;
    if (id >= (uint16_t)RI_MAX || ri == RI_NONE || ri == RI_UNKNOWN || ri == RI_JUNK) {
        *reason = "not an item";
        return 0;
    }
    if (IsCustomDrawn(ri)) {
        *reason = "2S2H custom draw that is not re-expressed";
        return 0;
    }
    auto it = Rando::StaticData::Items.find(ri);
    if (it == Rando::StaticData::Items.end()) {
        *reason = "no static item";
        return 0;
    }
    if (it->second.drawId == GID_NONE) {
        *reason = "no draw row";
        return 0;
    }
    return ModelForDrawId((int)it->second.drawId, out, reason);
}

// ---- Host-side tinted song notes (#830) ---------------------------------------
//
// MM's get-item draw table has no row for most of OoT's songs, but MM draws a
// song itself: Rando::DrawItem's DrawSong (DrawItem.cpp) emits setup list 25 on
// XLU, the matrix, an env colour per song, then gGiSongNoteDL. The note list
// sets its own prim colour (255,255,255, LOD 0x80) before any triangle, so the
// env colour is the whole tint. These recipes are that draw, numbered in MM's
// own draw space from kNoteKeyFirst up (far past MM_GetItem_DrawTableCount, so
// a note key never names a draw row): a host-native row answers one of them for
// an OoT song MM has no row for. OoTMM, the reference for player-facing
// choices, draws every song as a tinted note in both games. Where MM has the
// same song (Sun, Time, Storms, Epona) the note takes DrawSong's colour; the
// others take OoT's own tint: GetItem_DrawGenericMusicNote's grayscale colour,
// or for Minuet and Bolero the env colour their colour list sets
// (object_gi_melody gGiMinuetColorDL / gGiBoleroColorDL, the same bytes in both
// games' archives).
enum : int16_t {
    kNoteKeyFirst = 0x4000,
    kNoteGeneric = kNoteKeyFirst,
    kNoteLullaby,
    kNoteEpona,
    kNoteSaria,
    kNoteSun,
    kNoteTime,
    kNoteStorms,
    kNoteMinuet,
    kNoteBolero,
    kNoteKeyEnd,
};

struct MMNoteRecipe {
    int16_t key;
    uint8_t env[3];
};

const MMNoteRecipe kNoteRecipes[] = {
    { kNoteGeneric, { 255, 255, 255 } }, // OoT's generic note tint
    { kNoteLullaby, { 109, 73, 143 } },  // OoT's Zelda's Lullaby tint (MM's Lullaby is Goron)
    { kNoteEpona, { 146, 87, 49 } },     // DrawSong, RI_SONG_EPONA
    { kNoteSaria, { 62, 109, 23 } },     // OoT's Saria's Song tint
    { kNoteSun, { 237, 231, 62 } },      // DrawSong, RI_SONG_SUN
    { kNoteTime, { 98, 177, 211 } },     // DrawSong, RI_SONG_TIME
    { kNoteStorms, { 146, 146, 146 } },  // DrawSong, RI_SONG_STORMS
    { kNoteMinuet, { 0, 200, 0 } },      // gGiMinuetColorDL's gsDPSetEnvColor
    { kNoteBolero, { 255, 50, 0 } },     // gGiBoleroColorDL's gsDPSetEnvColor
};
static_assert(sizeof(kNoteRecipes) / sizeof(kNoteRecipes[0]) == (size_t)(kNoteKeyEnd - kNoteKeyFirst),
              "one recipe per note key");

/** A note key's tinted note: 1 and *out, or 0 and *reason. */
int ModelForNoteKey(int key, ComboModel* out, const char** reason) {
    Combo_ModelInit(out);
    *reason = nullptr;
    if (key < kNoteKeyFirst || key >= kNoteKeyEnd || kNoteRecipes[key - kNoteKeyFirst].key != key) {
        *reason = "no such note recipe";
        return 0;
    }
    const MMNoteRecipe& recipe = kNoteRecipes[key - kNoteKeyFirst];
    if (Combo_ModelAddPart(out, gGiSongNoteDL, kXlu, 0) != 1) {
        *reason = "note list path malformed";
        return 0;
    }
    out->xluSetupDl = 25;
    out->xluColor.set = 1;
    out->xluColor.primLodFrac = 0x80; // what gGiSongNoteDL sets itself
    out->xluColor.prim[0] = out->xluColor.prim[1] = out->xluColor.prim[2] = 255;
    std::memcpy(out->xluColor.env, recipe.env, sizeof(recipe.env));
    if (!Combo_ModelIsWellFormed(out)) {
        *reason = "recipe produced a malformed model";
        return 0;
    }
    return 1;
}

/** Any key of MM's draw space: a draw row, or a host-side note recipe. */
int ModelForHostKey(int key, ComboModel* out, const char** reason) {
    if (key >= kNoteKeyFirst) {
        return ModelForNoteKey(key, out, reason);
    }
    return ModelForDrawId(key, out, reason);
}

// ---- Host-native mapping (#577 M7) --------------------------------------------
//
// When OoT hands MM a model whose paths live in a directory both archives carry,
// MM draws its OWN equivalent: a row of MM_sDrawItemTable, keyed by the OoT
// model's whole part list (foreign_model.h, ComboHostNativeRow).
//
// Each OoT get-item model whose lists live in a colliding directory has a row:
// MM's own model for the same item (OoT's hookshot and longshot draw MM's
// hookshot, OoT's Hylian Shield MM's Hero's Shield, OoT's gold rupee MM's huge
// rupee), a host-side tinted note for a song (#830, keyed by its tint as well),
// or "no model" with its reason where MM has no such item. The
// ForeignModel row (M11) walks every OoT draw row and item against this table,
// so a colliding OoT model without a row, and a row no OoT model reaches, both
// fail.
const ComboHostNativeRow kHostNativeRows[] = {
    // OoT GID_BOTTLE
    { { "object_gi_bottle/gGiBottleStopperDL", "object_gi_bottle/gGiBottleDL" }, GID_BOTTLE, nullptr },
    // OoT GID_KEY_SMALL
    { { "object_gi_key/gGiSmallKeyDL" }, GID_KEY_SMALL, nullptr },
    // OoT GID_SONG_MINUET: MM's get-item table has no Minuet note row, so its
    // host-side tinted note in OoT's colour (#830).
    { { "object_gi_melody/gGiMinuetColorDL", "object_gi_melody/gGiSongNoteDL" }, kNoteMinuet, nullptr },
    // OoT GID_SONG_BOLERO: likewise.
    { { "object_gi_melody/gGiBoleroColorDL", "object_gi_melody/gGiSongNoteDL" }, kNoteBolero, nullptr },
    // OoT GID_SONG_SERENADE
    { { "object_gi_melody/gGiSerenadeColorDL", "object_gi_melody/gGiSongNoteDL" }, GID_04, nullptr },
    // OoT GID_SONG_REQUIEM
    { { "object_gi_melody/gGiRequiemColorDL", "object_gi_melody/gGiSongNoteDL" }, GID_05, nullptr },
    // OoT GID_SONG_NOCTURNE
    { { "object_gi_melody/gGiNocturneColorDL", "object_gi_melody/gGiSongNoteDL" }, GID_06, nullptr },
    // OoT GID_SONG_PRELUDE
    { { "object_gi_melody/gGiPreludeColorDL", "object_gi_melody/gGiSongNoteDL" }, GID_07, nullptr },
    // OoT GID_HEART
    { { "object_gi_heart/gGiRecoveryHeartDL" }, GID_RECOVERY_HEART, nullptr },
    // OoT GID_KEY_BOSS
    { { "object_gi_bosskey/gGiBossKeyDL", "object_gi_bosskey/gGiBossKeyGemDL" }, GID_KEY_BOSS, nullptr },
    // OoT GID_COMPASS
    { { "object_gi_compass/gGiCompassDL", "object_gi_compass/gGiCompassGlassDL" }, GID_COMPASS, nullptr },
    // OoT GID_NUTS
    { { "object_gi_nuts/gGiNutDL" }, GID_DEKU_NUTS, nullptr },
    // OoT GID_HEART_CONTAINER
    { { "object_gi_hearts/gGiHeartBorderDL", "object_gi_hearts/gGiHeartContainerDL" }, GID_HEART_CONTAINER, nullptr },
    // OoT GID_HEART_PIECE
    { { "object_gi_hearts/gGiHeartBorderDL", "object_gi_hearts/gGiHeartPieceDL" }, GID_HEART_PIECE, nullptr },
    // OoT GID_QUIVER_30
    { { "object_gi_arrowcase/gGiQuiver30InnerColorDL", "object_gi_arrowcase/gGiQuiverInnerDL",
        "object_gi_arrowcase/gGiQuiver30OuterColorDL", "object_gi_arrowcase/gGiQuiverOuterDL" },
      GID_QUIVER_30,
      nullptr },
    // OoT GID_QUIVER_40
    { { "object_gi_arrowcase/gGiQuiver40InnerColorDL", "object_gi_arrowcase/gGiQuiverInnerDL",
        "object_gi_arrowcase/gGiQuiver40OuterColorDL", "object_gi_arrowcase/gGiQuiverOuterDL" },
      GID_QUIVER_40,
      nullptr },
    // OoT GID_QUIVER_50
    { { "object_gi_arrowcase/gGiQuiver50InnerColorDL", "object_gi_arrowcase/gGiQuiverInnerDL",
        "object_gi_arrowcase/gGiQuiver50OuterColorDL", "object_gi_arrowcase/gGiQuiverOuterDL" },
      GID_QUIVER_50,
      nullptr },
    // OoT GID_BOMB_BAG_20
    { { "object_gi_bombpouch/gGiBombBag20BagColorDL", "object_gi_bombpouch/gGiBombBagDL",
        "object_gi_bombpouch/gGiBombBag20RingColorDL", "object_gi_bombpouch/gGiBombBagRingDL" },
      GID_BOMB_BAG_20,
      nullptr },
    // OoT GID_BOMB_BAG_30
    { { "object_gi_bombpouch/gGiBombBag30BagColorDL", "object_gi_bombpouch/gGiBombBagDL",
        "object_gi_bombpouch/gGiBombBag30RingColorDL", "object_gi_bombpouch/gGiBombBagRingDL" },
      GID_BOMB_BAG_30,
      nullptr },
    // OoT GID_BOMB_BAG_40
    { { "object_gi_bombpouch/gGiBombBag40BagColorDL", "object_gi_bombpouch/gGiBombBagDL",
        "object_gi_bombpouch/gGiBombBag40RingColorDL", "object_gi_bombpouch/gGiBombBagRingDL" },
      GID_BOMB_BAG_40,
      nullptr },
    // OoT GID_STICK
    { { "object_gi_stick/gGiStickDL" }, GID_DEKU_STICK, nullptr },
    // OoT GID_DUNGEON_MAP
    { { "object_gi_map/gGiDungeonMapDL" }, GID_DUNGEON_MAP, nullptr },
    // OoT GID_MAGIC_SMALL
    { { "object_gi_magicpot/gGiMagicJarSmallDL" }, GID_MAGIC_JAR_SMALL, nullptr },
    // OoT GID_MAGIC_LARGE
    { { "object_gi_magicpot/gGiMagicJarLargeDL" }, GID_MAGIC_JAR_BIG, nullptr },
    // OoT GID_BOMB
    { { "object_gi_bomb_1/gGiBombDL" }, GID_BOMB, nullptr },
    // OoT GID_STONE_OF_AGONY
    { { "object_gi_map/gGiStoneOfAgonyDL" }, GID_STONE_OF_AGONY, nullptr },
    // OoT GID_WALLET_ADULT
    { { "object_gi_purse/gGiAdultWalletColorDL", "object_gi_purse/gGiWalletDL",
        "object_gi_purse/gGiAdultWalletRupeeOuterColorDL", "object_gi_purse/gGiWalletRupeeOuterDL",
        "object_gi_purse/gGiAdultWalletStringColorDL", "object_gi_purse/gGiWalletStringDL",
        "object_gi_purse/gGiAdultWalletRupeeInnerColorDL", "object_gi_purse/gGiWalletRupeeInnerDL" },
      GID_WALLET_ADULT,
      nullptr },
    // OoT GID_WALLET_GIANT
    { { "object_gi_purse/gGiGiantsWalletColorDL", "object_gi_purse/gGiWalletDL",
        "object_gi_purse/gGiGiantsWalletRupeeOuterColorDL", "object_gi_purse/gGiWalletRupeeOuterDL",
        "object_gi_purse/gGiGiantsWalletStringColorDL", "object_gi_purse/gGiWalletStringDL",
        "object_gi_purse/gGiGiantsWalletRupeeInnerColorDL", "object_gi_purse/gGiWalletRupeeInnerDL" },
      GID_WALLET_GIANT,
      nullptr },
    // OoT GID_ARROWS_SMALL
    { { "object_gi_arrow/gGiArrowSmallDL" }, GID_ARROWS_SMALL, nullptr },
    // OoT GID_ARROWS_MEDIUM
    { { "object_gi_arrow/gGiArrowMediumDL" }, GID_ARROWS_MEDIUM, nullptr },
    // OoT GID_ARROWS_LARGE
    { { "object_gi_arrow/gGiArrowLargeDL" }, GID_ARROWS_LARGE, nullptr },
    // OoT GID_BOMBCHU
    { { "object_gi_bomb_2/gGiBombchuDL" }, GID_BOMBCHU, nullptr },
    // OoT GID_SHIELD_HYLIAN
    { { "object_gi_shield_2/gGiHylianShieldDL" }, GID_SHIELD_HERO, nullptr },
    // OoT GID_HOOKSHOT
    { { "object_gi_hookshot/gGiHookshotDL" }, GID_HOOKSHOT, nullptr },
    // OoT GID_LONGSHOT: MM has one hookshot
    { { "object_gi_hookshot/gGiLongshotDL" }, GID_HOOKSHOT, nullptr },
    // OoT GID_OCARINA_TIME
    { { "object_gi_ocarina/gGiOcarinaTimeDL", "object_gi_ocarina/gGiOcarinaTimeHolesDL" }, GID_OCARINA, nullptr },
    // OoT GID_MILK
    { { "object_gi_milk/gGiMilkBottleContentsDL", "object_gi_milk/gGiMilkBottleDL" }, GID_MILK, nullptr },
    // OoT GID_MASK_KEATON
    { { "object_gi_ki_tan_mask/gGiKeatonMaskDL", "object_gi_ki_tan_mask/gGiKeatonMaskEyesDL" },
      GID_MASK_KEATON,
      nullptr },
    // OoT GID_BOW
    { { "object_gi_bow/gGiBowDL" }, GID_BOW, nullptr },
    // OoT GID_LENS
    { { "object_gi_glasses/gGiLensDL", "object_gi_glasses/gGiLensGlassDL" }, GID_LENS, nullptr },
    // OoT GID_POTION_GREEN
    { { "object_gi_liquid/gGiGreenPotColorDL", "object_gi_liquid/gGiPotionPotDL",
        "object_gi_liquid/gGiGreenLiquidColorDL", "object_gi_liquid/gGiPotionLiquidDL",
        "object_gi_liquid/gGiGreenPatternColorDL", "object_gi_liquid/gGiPotionPatternDL" },
      GID_POTION_GREEN,
      nullptr },
    // OoT GID_POTION_RED
    { { "object_gi_liquid/gGiRedPotColorDL", "object_gi_liquid/gGiPotionPotDL", "object_gi_liquid/gGiRedLiquidColorDL",
        "object_gi_liquid/gGiPotionLiquidDL", "object_gi_liquid/gGiRedPatternColorDL",
        "object_gi_liquid/gGiPotionPatternDL" },
      GID_POTION_RED,
      nullptr },
    // OoT GID_POTION_BLUE
    { { "object_gi_liquid/gGiBluePotColorDL", "object_gi_liquid/gGiPotionPotDL",
        "object_gi_liquid/gGiBlueLiquidColorDL", "object_gi_liquid/gGiPotionLiquidDL",
        "object_gi_liquid/gGiBluePatternColorDL", "object_gi_liquid/gGiPotionPatternDL" },
      GID_POTION_BLUE,
      nullptr },
    // OoT GID_SHIELD_MIRROR
    { { "object_gi_shield_3/gGiMirrorShieldDL", "object_gi_shield_3/gGiMirrorShieldSymbolDL" },
      GID_SHIELD_MIRROR,
      nullptr },
    // OoT GID_BEAN
    { { "object_gi_bean/gGiBeanDL" }, GID_MAGIC_BEANS, nullptr },
    // OoT GID_FISH
    { { "object_gi_fish/gGiFishDL" }, GID_FISH, nullptr },
    // OoT GID_SWORD_BGS
    { { "object_gi_longsword/gGiBiggoronSwordDL" }, GID_SWORD_BGS, nullptr },
    // OoT GID_MASK_BUNNY
    { { "object_gi_rabit_mask/gGiBunnyHoodDL", "object_gi_rabit_mask/gGiBunnyHoodEyesDL" }, GID_MASK_BUNNY, nullptr },
    // OoT GID_MASK_TRUTH
    { { "object_gi_truth_mask/gGiMaskOfTruthDL", "object_gi_truth_mask/gGiMaskOfTruthAccentsDL" },
      GID_MASK_TRUTH,
      nullptr },
    // OoT GID_SOLDOUT
    { { "object_gi_soldout/gGiSoldOutDL" }, -1, "sold-out sign: MM's get-item table has no sold-out row" },
    // OoT GID_MASK_GORON
    { { "object_gi_golonmask/gGiGoronMaskDL" }, GID_MASK_GORON, nullptr },
    // OoT GID_MASK_ZORA
    { { "object_gi_zoramask/gGiZoraMaskDL" }, GID_MASK_ZORA, nullptr },
    // OoT GID_ARROW_FIRE
    { { "object_gi_m_arrow/gGiMagicArrowDL", "object_gi_m_arrow/gGiFireArrowColorDL",
        "object_gi_m_arrow/gGiArrowMagicDL" },
      GID_ARROW_FIRE,
      nullptr },
    // OoT GID_ARROW_ICE
    { { "object_gi_m_arrow/gGiMagicArrowDL", "object_gi_m_arrow/gGiIceArrowColorDL",
        "object_gi_m_arrow/gGiArrowMagicDL" },
      GID_ARROW_ICE,
      nullptr },
    // OoT GID_ARROW_LIGHT
    { { "object_gi_m_arrow/gGiMagicArrowDL", "object_gi_m_arrow/gGiLightArrowColorDL",
        "object_gi_m_arrow/gGiArrowMagicDL" },
      GID_ARROW_LIGHT,
      nullptr },
    // OoT GID_SKULL_TOKEN
    { { "object_gi_sutaru/gGiSkulltulaTokenDL", "object_gi_sutaru/gGiSkulltulaTokenFlameDL" },
      GID_SKULL_TOKEN,
      nullptr },
    // OoT GID_BUG
    { { "object_gi_insect/gGiBugsContainerDL", "object_gi_insect/gGiBugsGlassDL" }, GID_BUG, nullptr },
    // OoT GID_POE
    { { "object_gi_ghost/gGiGhostContainerLidDL", "object_gi_ghost/gGiGhostContainerGlassDL",
        "object_gi_ghost/gGiPoeColorDL", "object_gi_ghost/gGiGhostContainerContentsDL" },
      GID_POE,
      nullptr },
    // OoT GID_FAIRY
    { { "object_gi_soul/gGiFairyContainerBaseCapDL", "object_gi_soul/gGiFairyContainerGlassDL",
        "object_gi_soul/gGiFairyContainerContentsDL" },
      GID_FAIRY_2,
      nullptr },
    // OoT GID_RUPEE_GREEN
    { { "object_gi_rupy/gGiGreenRupeeInnerColorDL", "object_gi_rupy/gGiRupeeInnerDL",
        "object_gi_rupy/gGiGreenRupeeOuterColorDL", "object_gi_rupy/gGiRupeeOuterDL" },
      GID_RUPEE_GREEN,
      nullptr },
    // OoT GID_RUPEE_BLUE
    { { "object_gi_rupy/gGiBlueRupeeInnerColorDL", "object_gi_rupy/gGiRupeeInnerDL",
        "object_gi_rupy/gGiBlueRupeeOuterColorDL", "object_gi_rupy/gGiRupeeOuterDL" },
      GID_RUPEE_BLUE,
      nullptr },
    // OoT GID_RUPEE_RED
    { { "object_gi_rupy/gGiRedRupeeInnerColorDL", "object_gi_rupy/gGiRupeeInnerDL",
        "object_gi_rupy/gGiRedRupeeOuterColorDL", "object_gi_rupy/gGiRupeeOuterDL" },
      GID_RUPEE_RED,
      nullptr },
    // OoT GID_BIG_POE
    { { "object_gi_ghost/gGiGhostContainerLidDL", "object_gi_ghost/gGiGhostContainerGlassDL",
        "object_gi_ghost/gGiBigPoeColorDL", "object_gi_ghost/gGiGhostContainerContentsDL" },
      GID_BIG_POE,
      nullptr },
    // OoT GID_RUPEE_PURPLE
    { { "object_gi_rupy/gGiPurpleRupeeInnerColorDL", "object_gi_rupy/gGiRupeeInnerDL",
        "object_gi_rupy/gGiPurpleRupeeOuterColorDL", "object_gi_rupy/gGiRupeeOuterDL" },
      GID_RUPEE_PURPLE,
      nullptr },
    // OoT GID_RUPEE_GOLD
    { { "object_gi_rupy/gGiGoldRupeeInnerColorDL", "object_gi_rupy/gGiRupeeInnerDL",
        "object_gi_rupy/gGiGoldRupeeOuterColorDL", "object_gi_rupy/gGiRupeeOuterDL" },
      GID_RUPEE_HUGE,
      nullptr },
    // OoT GID_SWORD_KOKIRI
    { { "object_gi_sword_1/gGiKokiriSwordDL" }, GID_SWORD_KOKIRI, nullptr },
    // OoT GID_SKULL_TOKEN_2
    { { "object_st/gSkulltulaTokenDL", "object_st/gSkulltulaTokenFlameDL" }, GID_SKULL_TOKEN_2, nullptr },
    // OoT GID_SONG_GENERIC .. GID_SONG_STORM: one list under seven grayscale
    // tints, so these rows key the tint too (#830), each answering MM's
    // host-side tinted note (DrawSong's colour where MM has the song).
    { { "object_gi_melody/gGiSongNoteDL" }, kNoteGeneric, nullptr, 1, { 255, 255, 255 } },
    { { "object_gi_melody/gGiSongNoteDL" }, kNoteLullaby, nullptr, 1, { 109, 73, 143 } },
    { { "object_gi_melody/gGiSongNoteDL" }, kNoteEpona, nullptr, 1, { 217, 110, 48 } },
    { { "object_gi_melody/gGiSongNoteDL" }, kNoteSaria, nullptr, 1, { 62, 109, 23 } },
    { { "object_gi_melody/gGiSongNoteDL" }, kNoteSun, nullptr, 1, { 237, 231, 62 } },
    { { "object_gi_melody/gGiSongNoteDL" }, kNoteTime, nullptr, 1, { 98, 177, 211 } },
    { { "object_gi_melody/gGiSongNoteDL" }, kNoteStorms, nullptr, 1, { 146, 146, 146 } },
};
constexpr int kHostNativeRowCount = (int)(sizeof(kHostNativeRows) / sizeof(kHostNativeRows[0]));

/** 1 and *hostKey: MM's own row. 0 and *reason (NULL: no row at all). */
int MapHostNative(const ComboModel* foreign, uint16_t* hostKey, const char** reason, int* tableRow) {
    *reason = nullptr;
    *tableRow = Combo_HostNativeFind(kHostNativeRows, kHostNativeRowCount, foreign);
    if (*tableRow < 0) {
        return 0;
    }
    const ComboHostNativeRow& row = kHostNativeRows[*tableRow];
    const bool drawRow = row.hostDrawId >= 0 && row.hostDrawId < MM_GetItem_DrawTableCount();
    const bool noteRecipe = row.hostDrawId >= kNoteKeyFirst && row.hostDrawId < kNoteKeyEnd;
    if (!drawRow && !noteRecipe) {
        *reason = row.noModelReason;
        return 0;
    }
    *hostKey = (uint16_t)row.hostDrawId;
    return 1;
}

} // namespace

/** THE SOURCE. C linkage so the test row can prove the registered pointer is
 *  this function (a registrar the linker dropped would leave MM unregistered). */
extern "C" int MM_ComboModel(uint16_t id, ComboModel* out) {
    ComboModel model;
    const char* reason = nullptr;
    const int answered = ModelForItem(id, &model, &reason);
    if (out != nullptr) {
        if (answered == 1) {
            *out = model;
        } else {
            Combo_ModelInit(out);
        }
    }
    return answered == 1 && out != nullptr ? 1 : 0;
}

/** THE HOST-NATIVE MAPPER MM registers for itself as a host. */
extern "C" int MM_ComboModelHostNative(const ComboModel* foreign, uint16_t* hostKey) {
    if (foreign == nullptr || hostKey == nullptr) {
        return 0;
    }
    const char* reason = nullptr;
    int tableRow = -1;
    return MapHostNative(foreign, hostKey, &reason, &tableRow);
}

// ---- TEST BRIDGES (ForeignModel row, src/common/tests/test_foreign_model.c) ---

/** MM's host-native answer for a foreign model: 1 and *hostKey, or 0 and
 *  *reason (NULL when no row names the model). *tableRow: the row, or -1. */
extern "C" int MM_ComboModelHostNative_TestAnswer(const ComboModel* foreign, uint16_t* hostKey, const char** reason,
                                                  int* tableRow) {
    return MapHostNative(foreign, hostKey, reason, tableRow);
}

extern "C" int MM_ComboModelHostNative_TestRowCount(void) {
    return kHostNativeRowCount;
}

extern "C" int MM_ComboModel_TestIdSpace(void) {
    return (int)RI_MAX;
}

extern "C" int MM_ComboModel_TestDrawRowCount(void) {
    return (int)MM_GetItem_DrawTableCount();
}

/** A draw row's answer: 1 and *out, or 0 and *reason. */
extern "C" int MM_ComboModel_TestForDrawRow(int drawId, ComboModel* out, const char** reason) {
    const char* why = nullptr;
    const int answered = ModelForDrawId(drawId, out, &why);
    if (reason != nullptr) {
        *reason = why;
    }
    return answered;
}

/** The model MM draws, as a host, for one of its own host keys (a draw row, or a
 *  host-side note recipe, #830): 1 and *out, or 0 and *reason. */
extern "C" int MM_ComboModel_TestForHostKey(int hostKey, ComboModel* out, const char** reason) {
    const char* why = nullptr;
    const int answered = ModelForHostKey(hostKey, out, &why);
    if (reason != nullptr) {
        *reason = why;
    }
    return answered;
}

/** A draw row's lists (NULL where the row has none): 1, or 0 past the table. */
extern "C" int MM_ComboModel_TestDrawRowLists(int drawId, const char* lists[COMBO_MODEL_MAX_PARTS]) {
    MMGetItemDrawFn fn = nullptr;
    void* const* res = nullptr;
    if (drawId < 0 || drawId > INT16_MAX || MM_GetItem_DrawTableRow((s16)drawId, &fn, &res) != 1) {
        return 0;
    }
    for (int i = 0; i < COMBO_MODEL_MAX_PARTS; i++) {
        lists[i] = static_cast<const char*>(res[i]);
    }
    return 1;
}

/** An item's reason for answering no model (NULL when it answers one). */
extern "C" const char* MM_ComboModel_TestItemReason(uint16_t id) {
    ComboModel model;
    const char* reason = nullptr;
    return ModelForItem(id, &model, &reason) == 1 ? nullptr : reason;
}

// ============================================================================
// THE HOST SIDE (#577 M3): an OoT item's model in MM's get-item cutscene
// ============================================================================

namespace {

// TEST ONLY: -1 = ask the archive manager; 0 / 1 = answer that.
int sModelMountOverride = -1;
// TEST ONLY: emit each part's path as the display-list pointer without loading
// it, so a ROM-free row can read the emitted lists back.
bool sModelEmitUnresolved = false;

bool ModelPathMounted(const char* dl) {
    if (sModelMountOverride >= 0) {
        return sModelMountOverride == 1;
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
    // oot.o2r is mounted from OoT's first boot on (rsbs/src/main.cpp,
    // Combo_EnsureGameArchivesLoaded), and redship-mm.o2r carries every
    // OoT-exclusive get-item directory from MM's first arrival on (#577 M6).
    // An MM-first session that never entered OoT therefore answers yes for
    // those, and no for any other OoT path, keeping the stand-in rather than
    // loading a path nothing resolves.
    return archives != nullptr && archives->HasFile(std::string(dl + sizeof(kOtr) - 1));
}

bool ModelMounted(const ComboModel& model) {
    for (uint8_t i = 0; i < model.partCount; i++) {
        if (!ModelPathMounted(model.parts[i].dl)) {
            return false;
        }
    }
    return model.partCount > 0;
}

void EmitPart(Gfx* pkt, const char* dl) {
    Gfx* list = reinterpret_cast<Gfx*>(const_cast<char*>(dl));
    if (sModelEmitUnresolved) {
        __gSPDisplayList(pkt, list);
    } else {
        MM_gSPDisplayList(pkt, list);
    }
}

} // namespace

// At global scope, not in the anonymous namespace: OPEN_DISPS / CLOSE_DISPS
// re-declare the frame-interpolation calls at block scope, and inside a
// namespace that declaration names a C++-linkage function of that namespace
// instead of the extern "C" one FrameInterpolation.h declares.

/**
 * One layer of the descriptor, in the order both games' z_draw.c emit it: the
 * setup list, the layer's scrolling segments, its colours (and the grayscale
 * tint), the matrix, the plain parts, then the camera-facing parts under their
 * offset and the billboard rotation.
 */
static void ForeignModel_DrawLayer(PlayState* play, const ComboModel& model, uint8_t layer) {
    const uint8_t setup = layer == kOpa ? model.opaSetupDl : model.xluSetupDl;
    if (setup == 0) {
        return; // no part on this layer (Combo_ModelIsWellFormed)
    }
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    const s32 frames = (s32)play->state.frames;
    const ComboModelColor& color = layer == kOpa ? model.opaColor : model.xluColor;

    OPEN_DISPS(gfxCtx);
    Gfx*& disp = layer == kOpa ? POLY_OPA_DISP : POLY_XLU_DISP;

    disp = MM_Gfx_SetupDL(disp, setup);
    for (const ComboModelScroll& s : model.scrolls) {
        if (s.segment == 0 || s.layer != layer) {
            continue;
        }
        Gfx* tex = s.twoTiles ? MM_Gfx_TwoTexScroll(gfxCtx, G_TX_RENDERTILE, (u32)(s.x1 + s.x1PerFrame * frames),
                                                    (u32)(s.y1 + s.y1PerFrame * frames), s.w1, s.h1, 1,
                                                    (u32)(s.x2 + s.x2PerFrame * frames),
                                                    (u32)(s.y2 + s.y2PerFrame * frames), s.w2, s.h2)
                              : MM_Gfx_TexScroll(gfxCtx, (u32)(s.x1 + s.x1PerFrame * frames),
                                                 (u32)(s.y1 + s.y1PerFrame * frames), s.w1, s.h1);
        MM_gSPSegment(disp++, s.segment, (uintptr_t)tex);
    }
    if (color.set) {
        gDPSetPrimColor(disp++, 0, color.primLodFrac, color.prim[0], color.prim[1], color.prim[2], 255);
        gDPSetEnvColor(disp++, color.env[0], color.env[1], color.env[2], 255);
    }
    if (model.grayscale) {
        gDPSetGrayscaleColor(disp++, model.grayscaleRgb[0], model.grayscaleRgb[1], model.grayscaleRgb[2], 255);
        gSPGrayscale(disp++, true);
    }

    MATRIX_FINALIZE_AND_LOAD(disp++, gfxCtx);
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
        EmitPart(disp++, part.dl);
    }
    if (anyBillboard) {
        MM_Matrix_Push();
        MM_Matrix_Translate(model.billboardOffset[0], model.billboardOffset[1], model.billboardOffset[2],
                            MTXMODE_APPLY);
        MM_Matrix_ReplaceRotation(&play->billboardMtxF);
        MATRIX_FINALIZE_AND_LOAD(disp++, gfxCtx);
        for (uint8_t i = 0; i < model.partCount; i++) {
            const ComboModelPart& part = model.parts[i];
            if (part.layer == layer && part.billboard) {
                EmitPart(disp++, part.dl);
            }
        }
        MM_Matrix_Pop();
    }
    if (model.grayscale) {
        gSPGrayscale(disp++, false);
    }

    CLOSE_DISPS(gfxCtx);
}

/** The descriptor under the current matrix: its own scale, then its rotation
 *  (foreign_model.h), then both layers. */
static void ForeignModel_Draw(PlayState* play, const ComboModel& model) {
    MM_Matrix_Push();
    if (model.scale != 1.0f) {
        MM_Matrix_Scale(model.scale, model.scale, model.scale, MTXMODE_APPLY);
    }
    if (model.rotation[0] != 0 || model.rotation[1] != 0 || model.rotation[2] != 0) {
        MM_Matrix_RotateZYX(model.rotation[0], model.rotation[1], model.rotation[2], MTXMODE_APPLY);
    }
    ForeignModel_DrawLayer(play, model, kOpa);
    ForeignModel_DrawLayer(play, model, kXlu);
    MM_Matrix_Pop();
}

namespace {

/** The drawable model of the foreign item `checkId` hosts: OoT's DESCRIPTOR, or,
 *  for a colliding OoT model MM's host-native table maps (#577 M7), MM's OWN
 *  row as MM's own recipe draws it; either way only when a mounted archive
 *  holds every path. `kindOut` (optional) receives which of the two it is
 *  (COMBO_MODEL_ANSWER_DESCRIPTOR or _HOST_NATIVE) when it returns true. */
bool ForeignModelForCheck(RandoCheckId checkId, ComboModel* out, uint8_t* kindOut = nullptr) {
    Combo_ModelInit(out);
    if (checkId == RC_UNKNOWN) {
        return false;
    }
    const SharedItem* item = Combo_GetForeignPlacementForCheck((uint16_t)checkId);
    if (item == nullptr) {
        return false;
    }
    ComboModelAnswer answer;
    ComboModel model;
    const uint8_t kind = Combo_GetForeignItemModel((uint8_t)GAME_MM, *item, &answer);
    if (kind == COMBO_MODEL_ANSWER_DESCRIPTOR) {
        model = answer.model;
    } else if (kind == COMBO_MODEL_ANSWER_HOST_NATIVE) {
        const char* reason = nullptr;
        if (ModelForHostKey((int)answer.hostKey, &model, &reason) != 1) {
            return false;
        }
    } else {
        return false;
    }
    if (!ModelMounted(model)) {
        return false;
    }
    *out = model;
    if (kindOut != nullptr) {
        *kindOut = kind;
    }
    return true;
}

} // namespace

namespace Rando {
namespace Foreign {

bool DrawForeignModelForCheck(RandoCheckId randoCheckId, PlayState* play) {
    ComboModel model;
    if (play == nullptr || !ForeignModelForCheck(randoCheckId, &model)) {
        return false;
    }
    ForeignModel_Draw(play, model);
    return true;
}

} // namespace Foreign
} // namespace Rando

// ---- TEST BRIDGES (ForeignModel row M10, src/common/tests/test_foreign_model.c)

extern "C" void MM_ForeignModel_TestSetMountOverride(int value) {
    sModelMountOverride = value;
}

/** ModelPathMounted with the override cleared: the PRODUCTION branch. */
extern "C" int MM_ForeignModel_TestPathMountedReal(const char* dl) {
    const int saved = sModelMountOverride;
    sModelMountOverride = -1;
    const bool mounted = ModelPathMounted(dl);
    sModelMountOverride = saved;
    return mounted ? 1 : 0;
}

namespace {

#define FMD_EXPECT(cond, ...)                                         \
    do {                                                              \
        if (!(cond)) {                                                \
            std::printf("[TEST] FAIL (%s:%d): ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                                 \
            std::printf("\n");                                        \
            return false;                                             \
        }                                                             \
    } while (0)

/** A PlayState with a real GraphicsContext (three arenas) and a real matrix
 *  stack, so MM's own draw primitives run unmodified; everything it replaces
 *  is restored on destruction. */
struct FakeDrawPlay {
    PlayState* play = nullptr;
    GraphicsContext* gfxCtx = nullptr;
    std::vector<Gfx> opa = std::vector<Gfx>(4096);
    std::vector<Gfx> xlu = std::vector<Gfx>(1024);
    std::vector<Gfx> overlay = std::vector<Gfx>(64);
    MtxF stack[20];
    PlayState* savedPlay = nullptr;
    MtxF* savedStack = nullptr;
    MtxF* savedCurrent = nullptr;

    static void InitArena(TwoHeadGfxArena* arena, std::vector<Gfx>& buf) {
        arena->size = buf.size() * sizeof(Gfx);
        arena->start = buf.data();
        arena->p = buf.data();
        arena->d = buf.data() + buf.size();
    }
    static void Identity(MtxF* m) {
        std::memset(m, 0, sizeof(*m));
        for (int i = 0; i < 4; i++) {
            m->mf[i][i] = 1.0f;
        }
    }

    FakeDrawPlay() {
        play = (PlayState*)std::calloc(1, sizeof(PlayState));
        gfxCtx = (GraphicsContext*)std::calloc(1, sizeof(GraphicsContext));
        InitArena(&gfxCtx->polyOpa, opa);
        InitArena(&gfxCtx->polyXlu, xlu);
        InitArena(&gfxCtx->overlay, overlay);
        play->state.gfxCtx = gfxCtx;
        play->state.frames = 7;
        Identity(&play->billboardMtxF);
        savedPlay = MM_gPlayState;
        MM_gPlayState = play;
        savedStack = MM_sMatrixStack;
        savedCurrent = MM_sCurrentMatrix;
        MM_sMatrixStack = stack;
        MM_sCurrentMatrix = stack;
        Identity(&stack[0]);
    }
    ~FakeDrawPlay() {
        MM_sMatrixStack = savedStack;
        MM_sCurrentMatrix = savedCurrent;
        MM_gPlayState = savedPlay;
        std::free(gfxCtx);
        std::free(play);
    }
};

/** What one layer's command list carries, read back word by word. */
struct LayerRead {
    std::vector<const char*> parts; // G_DL targets that are "__OTR__" paths, in order
    bool setupSeen = false;         // the layer's setup list
    bool matrixBeforeFirstPart = false;
    std::vector<int> segments;      // G_MW_SEGMENT indices
    std::vector<uint32_t> primRgba; // G_SETPRIMCOLOR colour words
    std::vector<uint32_t> primLod;  // ... and their LOD fractions
    std::vector<uint32_t> envRgba;  // G_SETENVCOLOR colour words
};

LayerRead ReadLayer(const Gfx* begin, const Gfx* end, uint8_t setupDl) {
    LayerRead read;
    uintptr_t setupTarget = 0;
    if (setupDl != 0) {
        Gfx ref[8] = {};
        MM_Gfx_SetupDL(ref, setupDl);
        setupTarget = (uintptr_t)ref[0].words.w1;
    }
    bool matrixSeen = false;
    for (const Gfx* g = begin; g < end; g++) {
        const uint32_t w0 = (uint32_t)g->words.w0;
        const uintptr_t w1 = (uintptr_t)g->words.w1;
        const uint32_t op = (w0 >> 24) & 0xFF;
        if (op == (uint32_t)(uint8_t)G_DL && w1 != 0) {
            if (setupTarget != 0 && w1 == setupTarget) {
                read.setupSeen = true;
            } else if (std::memcmp((const void*)w1, "__OTR__", 7) == 0) {
                if (read.parts.empty()) {
                    read.matrixBeforeFirstPart = matrixSeen;
                }
                read.parts.push_back((const char*)w1);
            }
        } else if (op == (uint32_t)(uint8_t)G_MTX) {
            matrixSeen = true;
        } else if (op == (uint32_t)(uint8_t)G_MOVEWORD && ((w0 >> 16) & 0xFF) == G_MW_SEGMENT) {
            read.segments.push_back((int)((w0 & 0xFFFF) / 4));
        } else if (op == (uint32_t)(uint8_t)G_SETPRIMCOLOR) {
            read.primRgba.push_back((uint32_t)w1);
            read.primLod.push_back(w0 & 0xFF);
        } else if (op == (uint32_t)(uint8_t)G_SETENVCOLOR) {
            read.envRgba.push_back((uint32_t)w1);
        }
    }
    return read;
}

uint32_t Rgba(const uint8_t rgb[3]) {
    return ((uint32_t)rgb[0] << 24) | ((uint32_t)rgb[1] << 16) | ((uint32_t)rgb[2] << 8) | 0xFF;
}

bool LayerMatches(const char* name, const LayerRead& read, const ComboModel* want, uint8_t layer) {
    std::vector<const char*> expected;
    if (want != nullptr) {
        for (uint8_t i = 0; i < want->partCount; i++) {
            if (want->parts[i].layer == layer) {
                expected.push_back(want->parts[i].dl);
            }
        }
    }
    std::printf("[TEST]   %s: %zu model list(s) emitted, %zu expected%s%s\n", name, read.parts.size(), expected.size(),
                read.parts.empty() ? "" : ", first ", read.parts.empty() ? "" : read.parts[0]);
    FMD_EXPECT(read.parts == expected, "Q3 %s: the draw emitted %zu model list(s), want %zu (in the model's order)",
               name, read.parts.size(), expected.size());
    if (expected.empty()) {
        return true;
    }
    const uint8_t setup = layer == kOpa ? want->opaSetupDl : want->xluSetupDl;
    FMD_EXPECT(read.setupSeen, "Q3 %s: setup list %d not emitted", name, (int)setup);
    FMD_EXPECT(read.matrixBeforeFirstPart, "Q3 %s: no matrix loaded before the first model list", name);
    for (const ComboModelScroll& s : want->scrolls) {
        if (s.segment != 0 && s.layer == layer) {
            FMD_EXPECT(std::find(read.segments.begin(), read.segments.end(), (int)s.segment) != read.segments.end(),
                       "Q3 %s: the scroll on segment %d is not bound", name, (int)s.segment);
        }
    }
    const ComboModelColor& c = layer == kOpa ? want->opaColor : want->xluColor;
    if (c.set) {
        bool prim = false;
        for (size_t i = 0; i < read.primRgba.size(); i++) {
            prim |= read.primRgba[i] == Rgba(c.prim) && read.primLod[i] == c.primLodFrac;
        }
        FMD_EXPECT(prim, "Q3 %s: prim colour %d,%d,%d (lod %d) not set", name, c.prim[0], c.prim[1], c.prim[2],
                   c.primLodFrac);
        FMD_EXPECT(std::find(read.envRgba.begin(), read.envRgba.end(), Rgba(c.env)) != read.envRgba.end(),
                   "Q3 %s: env colour %d,%d,%d not set", name, c.env[0], c.env[1], c.env[2]);
    }
    return true;
}

/**
 * CheckQueue's REAL foreign give-and-draw. Marks `mmCheckId` eligible, lets
 * Rando::MiscBehavior::CheckQueue() queue its GIEventGiveItem, runs the event's
 * giveItem against a fake item00 actor exactly as CustomItem does (the give
 * fires before the item is shown), then the event's drawItem into a real MM
 * GraphicsContext, and reads both layers back. `want` null: the stand-in, which
 * emits no model list at all.
 */
bool RunCheckQueueDraw(uint16_t mmCheckId, const ComboModel* want) {
    struct Restore {
        std::vector<RandoSaveCheck> checks;
        std::vector<GIEvent> queue;
        GIEvent current;
        CustomMessage::Entry active;
        Restore()
            : checks(std::begin(RANDO_SAVE_CHECKS), std::end(RANDO_SAVE_CHECKS)), queue(MM_GameEvents_Queue()),
              current(MM_GameEvents_Current()), active(activeCustomMessage) {
        }
        ~Restore() {
            Rando::MiscBehavior::CheckQueueReset();
            std::copy(checks.begin(), checks.end(), std::begin(RANDO_SAVE_CHECKS));
            MM_GameEvents_Queue() = queue;
            MM_GameEvents_Current() = current;
            activeCustomMessage = active;
            sModelEmitUnresolved = false;
        }
    } restore;

    for (RandoSaveCheck& check : RANDO_SAVE_CHECKS) {
        check.eligible = false;
    }
    RANDO_SAVE_CHECKS[mmCheckId].eligible = true;
    // Pre-set on the host so RecordForeignPickup authors no durable record.
    RANDO_SAVE_CHECKS[mmCheckId].obtained = true;
    Rando::MiscBehavior::CheckQueueReset();

    Rando::MiscBehavior::CheckQueue();
    std::vector<GIEvent>& queue = MM_GameEvents_Queue();
    FMD_EXPECT(queue.size() == 1, "Q1 CheckQueue queued %zu events for the eligible foreign host, want 1",
               queue.size());
    GIEventGiveItem* give = std::get_if<GIEventGiveItem>(&queue.back());
    FMD_EXPECT(give != nullptr && give->giveItem != nullptr && give->drawItem != nullptr,
               "Q1 the queued event is not a GIEventGiveItem with a give and a draw");
    FMD_EXPECT(give->param == (s16)mmCheckId && give->showGetItemCutscene,
               "Q1 the event is not the foreign branch's (param %d, cutscene %d)", (int)give->param,
               (int)give->showGetItemCutscene);

    FakeDrawPlay fake;
    Actor item00;
    std::memset(&item00, 0, sizeof(item00));
    {
        Actor* actor = &item00; // the CUSTOM_ITEM_* macros name `actor`
        CUSTOM_ITEM_PARAM = (s16)mmCheckId;
        CUSTOM_ITEM_FLAGS = CustomItem::GIVE_ITEM_CUTSCENE;
    }
    give->giveItem(&item00, fake.play);
    {
        Actor* actor = &item00;
        CUSTOM_ITEM_FLAGS |= CustomItem::CALLED_ACTION; // what CustomItem00_Update sets after the give
    }

    sModelEmitUnresolved = true;
    give->drawItem(&item00, fake.play);
    sModelEmitUnresolved = false;

    FMD_EXPECT(MM_sCurrentMatrix == fake.stack, "Q2 the draw left the matrix stack unbalanced");
    const LayerRead opa = ReadLayer(fake.opa.data(), fake.gfxCtx->polyOpa.p, want != nullptr ? want->opaSetupDl : 0);
    const LayerRead xlu = ReadLayer(fake.xlu.data(), fake.gfxCtx->polyXlu.p, want != nullptr ? want->xluSetupDl : 0);
    if (!LayerMatches("OPA", opa, want, kOpa) || !LayerMatches("XLU", xlu, want, kXlu)) {
        return false;
    }
    return true;
}

/**
 * A SHOP'S REAL SHELF DRAW (#800 pass 1): EnGirlA_RandoDrawFunc for a shelf item
 * standing on `mmCheckId`, or, with `hand`, EnSob1_DrawCustomItem (the Bomb Shop
 * owner's hand item, always RC_BOMB_SHOP_ITEM_01), into a real MM GraphicsContext,
 * both layers read back. The slot holds MM's junk cover, as the paired fill leaves
 * a crossing host. `want` null: the stand-in, which emits no model list at all
 * (never the cover's model). The frame counter is even, so the stand-in's sparkle
 * (DrawSparkles, odd frames only) spawns no effect into the fake play.
 */
bool RunShopDraw(uint16_t mmCheckId, bool hand, const ComboModel* want) {
    struct Restore {
        std::vector<RandoSaveCheck> checks;
        GameState* gameState;
        Restore() : checks(std::begin(RANDO_SAVE_CHECKS), std::end(RANDO_SAVE_CHECKS)), gameState(MM_gGameState) {
        }
        ~Restore() {
            std::copy(checks.begin(), checks.end(), std::begin(RANDO_SAVE_CHECKS));
            MM_gGameState = gameState;
            sModelEmitUnresolved = false;
        }
    } restore;

    RANDO_SAVE_CHECKS[mmCheckId].randoItemId = RI_JUNK;
    RANDO_SAVE_CHECKS[mmCheckId].shuffled = true;

    FakeDrawPlay fake;
    fake.play->state.frames = 8;
    MM_gGameState = &fake.play->state;
    EnGirlA* shelf = (EnGirlA*)std::calloc(1, sizeof(EnGirlA));
    shelf->actor.world.rot.z = (s16)mmCheckId;

    sModelEmitUnresolved = true;
    if (hand) {
        EnSob1_DrawCustomItem(&shelf->actor, fake.play);
    } else {
        EnGirlA_RandoDrawFunc(&shelf->actor, fake.play);
    }
    sModelEmitUnresolved = false;
    std::free(shelf);

    const LayerRead opa = ReadLayer(fake.opa.data(), fake.gfxCtx->polyOpa.p, want != nullptr ? want->opaSetupDl : 0);
    const LayerRead xlu = ReadLayer(fake.xlu.data(), fake.gfxCtx->polyXlu.p, want != nullptr ? want->xluSetupDl : 0);
    return LayerMatches("OPA", opa, want, kOpa) && LayerMatches("XLU", xlu, want, kXlu);
}

#undef FMD_EXPECT

} // namespace

/**
 * The #577 M3 playtest drive (GameExports_SingleExe.cpp, gameplay round-trip,
 * RSBS_GP_MM_FOREIGN_MODEL=1, 100 live frames into the MM play window): the
 * first OoT item the paired world's crossing
 * store placed on an MM check, not yet obtained, whose REAL OoT model MM can
 * draw right now (a DESCRIPTOR answer: a colliding item that #577 M7 maps to
 * MM's own row is skipped, so the capture always shows an OoT model). Its check
 * is marked eligible, exactly as walking up to it would, so CheckQueue queues
 * the real foreign give and the get-item cutscene follows. Returns the check
 * id, or 0 when the world has no such crossing.
 *
 * #830: RSBS_GP_MM_FOREIGN_MODEL=song arms the first OoT SONG instead (any
 * drawable answer whose model draws the song note: MM's own note row or its
 * host-side tinted note), so the capture shows an OoT song's note in MM. When
 * no crossing of the world hosts a song, the drive places OoT's Song of Time on
 * the first unobtained crossing's host check (a pinned placement, which answers
 * before the crossing store) and arms that: the issue's playtest step "place an
 * OoT song on an MM check". Test-only; nothing in a shipping path sets the
 * variable.
 */
static int ArmFirst(bool wantSong, int count);

extern "C" int MM_ForeignModel_PlaytestArmGive(void) {
    const char* mode = std::getenv("RSBS_GP_MM_FOREIGN_MODEL");
    const bool wantSong = mode != nullptr && std::strcmp(mode, "song") == 0;
    const int count = Combo_Crossings_Count(GAME_MM);
    for (int pass = 0; pass < (wantSong ? 2 : 1); pass++) {
        if (pass == 1) {
            SharedItem song;
            ComboCrossing first;
            bool placed = false;
            for (int i = 0; !placed && i < count; i++) {
                placed = Combo_Crossings_At(GAME_MM, i, &first) && first.hostCheck < RC_MAX &&
                         !RANDO_SAVE_CHECKS[first.hostCheck].obtained &&
                         Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, "Song of Time", &song) &&
                         Combo_SetForeignPlacement(first.hostCheck, song) >= 0;
            }
            std::fprintf(stderr, "[M3-PLAYTEST] no crossing hosts an OoT song: %s\n",
                         placed ? "placed OoT's Song of Time on the first unobtained crossing's MM check"
                                : "could not place one");
            std::fflush(stderr);
            if (!placed) {
                break;
            }
        }
        if (int armed = ArmFirst(wantSong, count); armed != 0) {
            return armed;
        }
    }
    std::fprintf(stderr, "[M3-PLAYTEST] no MM-hosted crossing of %d has a drawable OoT %s\n", count,
                 wantSong ? "song" : "model");
    std::fflush(stderr);
    return 0;
}

// GameExports_SingleExe.cpp: the window (game and ImGui) as a PNG, through the
// shared in-process capture (#843, src/common/frame_capture.h).
extern "C" void MM_Playtest_DumpGameFramebuffer(const char* tag);

/**
 * The drive's per-frame half (#830, same opt-in), every live MM play frame.
 * Nobody is at the keyboard: a textbox up for 150 frames waits for a press that
 * will not come (the arrival's own gives queue ahead of the armed one), so the
 * drive closes it. From frame 160 to 1000 it captures the window every 60
 * frames (mm-foreign-model-<frame>.png in RSBS_CAPTURE_OUT, else
 * RSBS_GP_SHOT_DIR; a no-op without a capture knob), logging the open
 * textbox's id beside each capture.
 */
extern "C" void MM_ForeignModel_PlaytestFrame(int playFrames) {
    static int sTextboxFrames = 0;
    PlayState* play = MM_gPlayState;
    if (play == nullptr) {
        return;
    }
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        if (++sTextboxFrames >= 150) {
            std::fprintf(stderr, "[M3-PLAYTEST] frame %d: closing textbox 0x%04X, up for %d frames\n", playFrames,
                         (unsigned)play->msgCtx.currentTextId, sTextboxFrames);
            std::fflush(stderr);
            MM_Message_CloseTextbox(play);
            sTextboxFrames = 0;
        }
    } else {
        sTextboxFrames = 0;
    }
    if (playFrames >= 160 && playFrames <= 1000 && playFrames % 60 == 40) {
        char tag[48];
        std::snprintf(tag, sizeof(tag), "mm-foreign-model-%04d", playFrames);
        std::fprintf(stderr, "[M3-PLAYTEST] frame %d: capture %s (textbox 0x%04X, msgMode %d)\n", playFrames, tag,
                     (unsigned)play->msgCtx.currentTextId, (int)play->msgCtx.msgMode);
        std::fflush(stderr);
        MM_Playtest_DumpGameFramebuffer(tag);
    }
}

/** The arming loop of MM_ForeignModel_PlaytestArmGive: the check id, or 0. */
static int ArmFirst(bool wantSong, int count) {
    for (int i = 0; i < count; i++) {
        ComboCrossing crossing;
        ComboModel model;
        uint8_t kind = COMBO_MODEL_ANSWER_NONE;
        if (!Combo_Crossings_At(GAME_MM, i, &crossing) || crossing.hostCheck >= RC_MAX ||
            RANDO_SAVE_CHECKS[crossing.hostCheck].obtained ||
            !ForeignModelForCheck((RandoCheckId)crossing.hostCheck, &model, &kind)) {
            continue;
        }
        bool drawsNote = false;
        for (uint8_t p = 0; p < model.partCount; p++) {
            drawsNote |= std::strcmp(model.parts[p].dl, gGiSongNoteDL) == 0;
        }
        if (wantSong ? !drawsNote : kind != COMBO_MODEL_ANSWER_DESCRIPTOR) {
            continue;
        }
        RANDO_SAVE_CHECKS[crossing.hostCheck].eligible = true;
        const SharedItem hosted = *Combo_GetForeignPlacementForCheck(crossing.hostCheck);
        const char* name = Combo_GetForeignItemName(hosted);
        std::fprintf(stderr,
                     "[M3-PLAYTEST] armed MM check %u (%s) hosting OoT item %u (%s, %s): %u part(s), first %s, XLU env "
                     "%d,%d,%d (crossing %d of %d)\n",
                     (unsigned)crossing.hostCheck, Rando::StaticData::CheckNames[crossing.hostCheck].c_str(),
                     (unsigned)hosted.id, name != nullptr ? name : "?",
                     kind == COMBO_MODEL_ANSWER_DESCRIPTOR ? "DESCRIPTOR" : "HOST_NATIVE", (unsigned)model.partCount,
                     model.parts[0].dl, model.xluColor.set ? model.xluColor.env[0] : -1,
                     model.xluColor.set ? model.xluColor.env[1] : -1, model.xluColor.set ? model.xluColor.env[2] : -1,
                     i + 1, count);
        std::fflush(stderr);
        return crossing.hostCheck;
    }
    return 0;
}

/** CheckQueue's real foreign give and draw, end to end (RunCheckQueueDraw): 0 on
 *  success. */
extern "C" int MM_ForeignModel_TestCheckQueueDraw(uint16_t mmCheckId, const ComboModel* want) {
    return RunCheckQueueDraw(mmCheckId, want) ? 0 : 1;
}

/** A shop's real shelf draw (`hand` 0) or the Bomb Shop owner's hand item (`hand`
 *  1), end to end (RunShopDraw): 0 on success. */
extern "C" int MM_ForeignModel_TestShopDraw(uint16_t mmCheckId, int hand, const ComboModel* want) {
    return RunShopDraw(mmCheckId, hand != 0, want) ? 0 : 1;
}

/** The check the Bomb Shop owner holds in his hand (EnSob1.cpp). */
extern "C" uint16_t MM_ForeignModel_TestBombShopHandCheck(void) {
    return (uint16_t)RC_BOMB_SHOP_ITEM_01;
}

namespace {
struct MMModelRegistrar {
    MMModelRegistrar() {
        Combo_RegisterModelSource((uint8_t)GAME_MM, MM_ComboModel);
        Combo_RegisterHostNativeModel((uint8_t)GAME_MM, MM_ComboModelHostNative);
    }
};
const MMModelRegistrar gMMModelRegistrar;
} // namespace

#endif // RSBS_SINGLE_EXECUTABLE
