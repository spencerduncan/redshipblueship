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
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "2s2h/Rando/Rando.h"

// src/common. Outside any extern "C" block: each header manages its own linkage.
#include "context.h"
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

struct Step {
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
int AddSteps(ComboModel* out, void* const* res, std::initializer_list<Step> steps) {
    for (const Step& step : steps) {
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

// ---- Host-native mapping (#577 M7 fills this) --------------------------------
//
// When OoT hands MM a model whose paths live in a directory both archives carry,
// MM draws its OWN equivalent: a row of MM_sDrawItemTable, keyed by the OoT
// model's first list. The rows are M7's data, so none answers yet and every
// colliding OoT model is "no model".
struct HostNativeRow {
    const char* foreignFirstList; // "__OTR__objects/<dir>/<name>" as OoT answers it
    s16 hostDrawId;               // MM_sDrawItemTable row
};
constexpr std::array<HostNativeRow, 0> kHostNativeRows{};

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
    if (foreign == nullptr || hostKey == nullptr || foreign->partCount == 0 || foreign->parts[0].dl == nullptr) {
        return 0;
    }
    for (const HostNativeRow& row : kHostNativeRows) {
        if (std::strcmp(row.foreignFirstList, foreign->parts[0].dl) == 0 && row.hostDrawId >= 0 &&
            row.hostDrawId < MM_GetItem_DrawTableCount()) {
            *hostKey = (uint16_t)row.hostDrawId;
            return 1;
        }
    }
    return 0;
}

// ---- TEST BRIDGES (ForeignModel row, src/common/tests/test_foreign_model.c) ---

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
