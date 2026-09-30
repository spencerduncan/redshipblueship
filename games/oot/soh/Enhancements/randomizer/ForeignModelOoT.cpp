/**
 * @file ForeignModelOoT.cpp
 * @brief OoT's answer to "which get-item model does this RG_* show in the other
 *        game?" (#577 M2), and OoT's host-native mapper for colliding models.
 *
 * Registered into src/common/foreign_model.h at static-init time, the same shape
 * as ForeignTextboxIconOoT.cpp. MM will call it (through Combo_GetForeignItemModel)
 * when it draws an OoT item on an MM check; #577 M3 is the first consumer.
 *
 * WHICH MODEL. The one OoT itself draws in its get-item cutscene: the item's
 * static GetItemEntry names a row of OoT_sDrawItemTable by `gid`, and the row's
 * draw function gives that row's display lists their shape. This file never
 * CALLS a draw function (they take OoT's PlayState and bind scrolling textures
 * through OoT's graph allocator); it re-expresses each draw function as a
 * ComboModel recipe below, one per function, read off games/oot/src/code/
 * z_draw.c. Two rows answer no model, by name: the Triforce piece (it reads the
 * live piece count) and the fishing pole (rod, float and four hooks under
 * separate matrices).
 *
 * Randomizer items whose entry carries a CUSTOM draw function (SoH's draw.cpp):
 *  - the ones that draw the same model as their entry's `gid` row with SoH's
 *    cosmetic recolours (keys, key rings, maps, compasses, Double Defense, the
 *    Power Bracelet, the bronze scale, the bombchu bag) answer that row, the
 *    vanilla model, as the textbox icon does for the same items;
 *  - the Master Sword and Roc's Feather are re-expressed from their draw
 *    functions (one list each);
 *  - every other custom draw (boss and bean souls, ocarina buttons, jabber nuts,
 *    the action-shuffle abilities, the Triforce piece, the fishing pole, the
 *    skeleton key, the mystery item) answers no model: its `gid` is a
 *    placeholder that would draw the wrong thing.
 * Progressive rows have no static entry; they show their first tier, as the
 * textbox icon does.
 *
 * WHAT IT NEVER DOES: read the save, the rando Context, a CVar or any live
 * state. The answer is a pure function of static tables (ForeignModel row).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/draw.h"

#include "assets/soh_assets.h" // gGiRocsFeatherDL

#include "context.h"       // src/common — GameId
#include "foreign_model.h" // src/common — the registry this file feeds

extern "C" {
#include <z64.h>
#include "objects/object_toki_objects/object_toki_objects.h" // the Master Sword's list
}

typedef void (*OoTGetItemDrawFn)(PlayState*, s16);

// games/oot/src/code/z_draw.c (the single-exe accessor and the row draw
// functions this file recognises by address; none of them is ever called here).
extern "C" {
s32 OoT_GetItem_DrawTableCount(void);
s32 OoT_GetItem_DrawTableRow(s16 drawId, OoTGetItemDrawFn* drawFunc, Gfx* const** dlists);
void GetItem_DrawMaskOrBombchu(PlayState* play, s16 drawId);
void GetItem_DrawSoldOut(PlayState* play, s16 drawId);
void GetItem_DrawBlueFire(PlayState* play, s16 drawId);
void OoT_GetItem_DrawPoes(PlayState* play, s16 drawId);
void GetItem_DrawFairy(PlayState* play, s16 drawId);
void GetItem_DrawMirrorShield(PlayState* play, s16 drawId);
void OoT_GetItem_DrawSkullToken(PlayState* play, s16 drawId);
void GetItem_DrawEggOrMedallion(PlayState* play, s16 drawId);
void OoT_GetItem_DrawCompass(PlayState* play, s16 drawId);
void OoT_GetItem_DrawPotion(PlayState* play, s16 drawId);
void OoT_GetItem_DrawGoronSword(PlayState* play, s16 drawId);
void OoT_GetItem_DrawDekuNuts(PlayState* play, s16 drawId);
void OoT_GetItem_DrawRecoveryHeart(PlayState* play, s16 drawId);
void OoT_GetItem_DrawFish(PlayState* play, s16 drawId);
void OoT_GetItem_DrawOpa0(PlayState* play, s16 drawId);
void OoT_GetItem_DrawOpa0Xlu1(PlayState* play, s16 drawId);
void OoT_GetItem_DrawXlu01(PlayState* play, s16 drawId);
void GetItem_DrawOpa10Xlu2(PlayState* play, s16 drawId);
void OoT_GetItem_DrawMagicArrow(PlayState* play, s16 drawId);
void GetItem_DrawMagicSpell(PlayState* play, s16 drawId);
void GetItem_DrawOpa1023(PlayState* play, s16 drawId);
void GetItem_DrawOpa10Xlu32(PlayState* play, s16 drawId);
void OoT_GetItem_DrawSmallRupee(PlayState* play, s16 drawId);
void GetItem_DrawScale(PlayState* play, s16 drawId);
void GetItem_DrawBulletBag(PlayState* play, s16 drawId);
void OoT_GetItem_DrawWallet(PlayState* play, s16 drawId);
void GetItem_DrawJewelKokiri(PlayState* play, s16 drawId);
void GetItem_DrawJewelGoron(PlayState* play, s16 drawId);
void GetItem_DrawJewelZora(PlayState* play, s16 drawId);
void GetItem_DrawGenericMusicNote(PlayState* play, s16 drawId);
void GetItem_DrawTriforcePiece(PlayState* play, s16 drawId);
void GetItem_DrawFishingPole(PlayState* play, s16 drawId);
}

// GetItem_DrawGenericMusicNote picks its tint as colors[drawId - 120].
static_assert(GID_SONG_GENERIC == 120, "OoT's generic-note tint slot is drawId - 120 (z_draw.c)");

namespace {

constexpr uint8_t kOpa = (uint8_t)COMBO_MODEL_LAYER_OPA;
constexpr uint8_t kXlu = (uint8_t)COMBO_MODEL_LAYER_XLU;

struct Step {
    uint8_t list; // index into the row's dlists
    uint8_t layer;
    uint8_t billboard;
};

/** One tile-pair scroll as z_draw.c's OoT_Gfx_TwoTexScrollEx arguments give it
 *  (base, per-frame rate, size per tile; the trailing interpolation deltas are
 *  the rates again and are not carried). */
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

void SetColor(ComboModelColor* c, uint8_t lodFrac, uint8_t pr, uint8_t pg, uint8_t pb, uint8_t er, uint8_t eg,
              uint8_t eb) {
    c->set = 1;
    c->primLodFrac = lodFrac;
    c->prim[0] = pr;
    c->prim[1] = pg;
    c->prim[2] = pb;
    c->env[0] = er;
    c->env[1] = eg;
    c->env[2] = eb;
}

/** Append the steps' lists from the row; 0 if a step names an empty list. */
int AddSteps(ComboModel* out, Gfx* const* dlists, std::initializer_list<Step> steps) {
    for (const Step& step : steps) {
        const char* dl = reinterpret_cast<const char*>(dlists[step.list]);
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

void JewelColors(OoTGetItemDrawFn fn, ComboModel* out) {
    // GetItem_DrawJewel{Kokiri,Goron,Zora}: the XLU gem and the OPA setting.
    if (fn == GetItem_DrawJewelKokiri) {
        SetColor(&out->xluColor, 128, 255, 255, 160, 0, 255, 0);
    } else if (fn == GetItem_DrawJewelGoron) {
        SetColor(&out->xluColor, 128, 255, 170, 255, 255, 0, 100);
    } else {
        SetColor(&out->xluColor, 128, 50, 255, 255, 50, 0, 150);
    }
    SetColor(&out->opaColor, 128, 255, 255, 170, 150, 120, 0);
}

/**
 * The recipe for one draw-table row: 1 and *out, or 0 and *reason. Each branch
 * mirrors the named z_draw.c function's emission order (OPA and XLU are separate
 * lists, so only the order within a layer is meaningful).
 */
int RecipeForRow(s16 drawId, OoTGetItemDrawFn fn, Gfx* const* d, ComboModel* out, const char** reason) {
    Combo_ModelInit(out);
    *reason = nullptr;
    int ok = 0;
    if (fn == OoT_GetItem_DrawOpa0) {
        ok = AddSteps(out, d, { { 0, kOpa, 0 } });
    } else if (fn == OoT_GetItem_DrawOpa0Xlu1) {
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 } });
    } else if (fn == OoT_GetItem_DrawXlu01) {
        ok = AddSteps(out, d, { { 0, kXlu, 0 }, { 1, kXlu, 0 } });
    } else if (fn == GetItem_DrawOpa1023) {
        ok = AddSteps(out, d, { { 1, kOpa, 0 }, { 0, kOpa, 0 }, { 2, kOpa, 0 }, { 3, kOpa, 0 } });
    } else if (fn == GetItem_DrawOpa10Xlu2) {
        ok = AddSteps(out, d, { { 1, kOpa, 0 }, { 0, kOpa, 0 }, { 2, kXlu, 0 } });
    } else if (fn == GetItem_DrawOpa10Xlu32) {
        ok = AddSteps(out, d, { { 1, kOpa, 0 }, { 0, kOpa, 0 }, { 3, kXlu, 0 }, { 2, kXlu, 0 } });
    } else if (fn == OoT_GetItem_DrawMagicArrow) {
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 }, { 2, kXlu, 0 } });
    } else if (fn == OoT_GetItem_DrawSmallRupee) {
        out->scale = 0.7f;
        ok = AddSteps(out, d, { { 1, kOpa, 0 }, { 0, kOpa, 0 }, { 3, kXlu, 0 }, { 2, kXlu, 0 } });
    } else if (fn == OoT_GetItem_DrawWallet) {
        ok = AddSteps(out, d,
                      { { 1, kOpa, 0 },
                        { 0, kOpa, 0 },
                        { 2, kOpa, 0 },
                        { 3, kOpa, 0 },
                        { 4, kOpa, 0 },
                        { 5, kOpa, 0 },
                        { 6, kOpa, 0 },
                        { 7, kOpa, 0 } });
    } else if (fn == GetItem_DrawBulletBag) {
        ok = AddSteps(out, d, { { 1, kOpa, 0 }, { 0, kOpa, 0 }, { 2, kXlu, 0 }, { 3, kXlu, 0 }, { 4, kXlu, 0 } });
    } else if (fn == GetItem_DrawEggOrMedallion) {
        out->opaSetupDl = 26;
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kOpa, 0 } });
    } else if (fn == GetItem_DrawMaskOrBombchu) {
        out->opaSetupDl = 26;
        ok = AddSteps(out, d, { { 0, kOpa, 0 } });
    } else if (fn == OoT_GetItem_DrawCompass) {
        out->xluSetupDl = 5;
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 } });
    } else if (fn == GetItem_DrawSoldOut) {
        out->xluSetupDl = 5;
        ok = AddSteps(out, d, { { 0, kXlu, 0 } });
    } else if (fn == OoT_GetItem_DrawDekuNuts) {
        out->scrolls[0] = TwoTileScroll(8, kOpa, 6, 6, 32, 32, 6, 6, 32, 32);
        ok = AddSteps(out, d, { { 0, kOpa, 0 } });
    } else if (fn == OoT_GetItem_DrawGoronSword) {
        out->scrolls[0] = TwoTileScroll(8, kOpa, 1, 0, 32, 32, 0, 0, 32, 32);
        ok = AddSteps(out, d, { { 0, kOpa, 0 } });
    } else if (fn == OoT_GetItem_DrawRecoveryHeart) {
        // The cosmetic heart-colour CVar tint is not carried (static tables only).
        out->scrolls[0] = TwoTileScroll(8, kXlu, 0, -3, 32, 32, 0, -2, 32, 32);
        ok = AddSteps(out, d, { { 0, kXlu, 0 } });
    } else if (fn == OoT_GetItem_DrawFish) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 0, 1, 32, 32, 0, 1, 32, 32);
        ok = AddSteps(out, d, { { 0, kXlu, 0 } });
    } else if (fn == GetItem_DrawScale) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 2, -2, 64, 64, 4, -4, 32, 32);
        ok = AddSteps(out, d, { { 2, kXlu, 0 }, { 3, kXlu, 0 }, { 1, kXlu, 0 }, { 0, kXlu, 0 } });
    } else if (fn == OoT_GetItem_DrawPotion) {
        out->scrolls[0] = TwoTileScroll(8, kOpa, -1, 1, 32, 32, -1, 1, 32, 32);
        ok = AddSteps(
            out, d, { { 1, kOpa, 0 }, { 0, kOpa, 0 }, { 2, kOpa, 0 }, { 3, kOpa, 0 }, { 4, kXlu, 0 }, { 5, kXlu, 0 } });
    } else if (fn == GetItem_DrawMirrorShield) {
        // z_draw.c wraps the coordinates % 256 and % 128: one texture period of
        // the 64- and 32-texel tiles, so the plain rates draw the same scroll.
        out->scrolls[0] = TwoTileScroll(8, kOpa, 0, 2, 64, 64, 0, 1, 32, 32);
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 } });
    } else if (fn == OoT_GetItem_DrawSkullToken) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 0, -5, 32, 32, 0, 0, 32, 64);
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 } });
    } else if (fn == GetItem_DrawMagicSpell) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 2, -6, 32, 32, 1, -2, 32, 32);
        ok = AddSteps(out, d, { { 0, kXlu, 0 }, { 1, kXlu, 0 }, { 2, kXlu, 0 } });
    } else if (fn == GetItem_DrawBlueFire) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 0, 0, 16, 32, 1, -8, 16, 32);
        out->billboardOffset[0] = -8.0f;
        out->billboardOffset[1] = -2.0f;
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 1 } });
    } else if (fn == OoT_GetItem_DrawPoes) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 0, 0, 16, 32, 1, -6, 16, 32);
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 }, { 3, kXlu, 1 }, { 2, kXlu, 1 } });
    } else if (fn == GetItem_DrawFairy) {
        out->scrolls[0] = TwoTileScroll(8, kXlu, 0, 0, 32, 32, 1, -6, 32, 32);
        ok = AddSteps(out, d, { { 0, kOpa, 0 }, { 1, kXlu, 0 }, { 2, kXlu, 1 } });
    } else if (fn == GetItem_DrawJewelKokiri || fn == GetItem_DrawJewelGoron || fn == GetItem_DrawJewelZora) {
        // GetItem_DrawJewel: segment 9 (XLU) and 8 (OPA) get fixed scroll
        // offsets, the stone stands upright, and each layer gets its colours.
        ComboModelScroll gem = TwoTileScroll(9, kXlu, 0, 0, 64, 64, 0, 0, 16, 16);
        gem.y1 = 255;
        gem.y2 = 255;
        out->scrolls[0] = gem;
        ComboModelScroll setting = {};
        setting.segment = 8;
        setting.layer = kOpa;
        setting.w1 = 16;
        setting.h1 = 16;
        out->scrolls[1] = setting;
        out->rotation[1] = -0x4000;
        out->rotation[2] = 0x4000;
        JewelColors(fn, out);
        ok = AddSteps(out, d, { { 0, kXlu, 0 }, { 1, kOpa, 0 } });
    } else if (fn == GetItem_DrawGenericMusicNote) {
        static const uint8_t kNoteTints[7][3] = {
            { 255, 255, 255 }, // generic
            { 109, 73, 143 },  // Zelda's Lullaby
            { 217, 110, 48 },  // Epona's Song
            { 62, 109, 23 },   // Saria's Song
            { 237, 231, 62 },  // Sun's Song
            { 98, 177, 211 },  // Song of Time
            { 146, 146, 146 }, // Song of Storms
        };
        const int slot = drawId - GID_SONG_GENERIC;
        if (slot < 0 || slot >= 7) {
            *reason = "generic song note outside its tint table";
            return 0;
        }
        out->grayscale = 1;
        std::memcpy(out->grayscaleRgb, kNoteTints[slot], 3);
        ok = AddSteps(out, d, { { 0, kXlu, 0 } });
    } else if (fn == GetItem_DrawTriforcePiece) {
        *reason = "Triforce piece: the shard drawn depends on the live piece count";
        return 0;
    } else if (fn == GetItem_DrawFishingPole) {
        *reason = "fishing pole: rod, float and hooks under separate matrices";
        return 0;
    } else {
        *reason = "draw function without a recipe";
        return 0;
    }
    if (!ok) {
        *reason = "row list missing for its recipe";
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
    OoTGetItemDrawFn fn = nullptr;
    Gfx* const* dlists = nullptr;
    if (drawId < 0 || drawId > INT16_MAX || OoT_GetItem_DrawTableRow((s16)drawId, &fn, &dlists) != 1) {
        *reason = "no such draw row";
        return 0;
    }
    return RecipeForRow((s16)drawId, fn, dlists, out, reason);
}

/** A progressive row's first tier (ForeignTextboxIconOoT.cpp's rule). */
RandomizerGet FirstTier(RandomizerGet rg) {
    switch (rg) {
        case RG_PROGRESSIVE_STICK_UPGRADE:
            return RG_DEKU_STICK_CAPACITY_20;
        case RG_PROGRESSIVE_NUT_UPGRADE:
            return RG_DEKU_NUT_CAPACITY_30;
        case RG_PROGRESSIVE_BOMB_BAG:
            return RG_BOMB_BAG;
        case RG_PROGRESSIVE_BOW:
            return RG_FAIRY_BOW;
        case RG_PROGRESSIVE_SLINGSHOT:
            return RG_FAIRY_SLINGSHOT;
        case RG_PROGRESSIVE_OCARINA:
            return RG_FAIRY_OCARINA;
        case RG_PROGRESSIVE_HOOKSHOT:
            return RG_HOOKSHOT;
        case RG_PROGRESSIVE_STRENGTH:
            return RG_GORONS_BRACELET;
        case RG_PROGRESSIVE_WALLET:
            return RG_ADULT_WALLET;
        case RG_PROGRESSIVE_SCALE:
            return RG_SILVER_SCALE;
        case RG_PROGRESSIVE_MAGIC_METER:
            return RG_MAGIC_SINGLE;
        case RG_PROGRESSIVE_GORONSWORD:
            return RG_BIGGORON_SWORD;
        case RG_PROGRESSIVE_BOMBCHU_BAG:
            return RG_BOMBCHU_10;
        default:
            return rg;
    }
}

/** SoH custom draws that recolour the model their entry's `gid` row draws. */
bool CustomDrawIsGidRecolour(CustomDrawFunc fn) {
    return fn == Randomizer_DrawSmallKey || fn == Randomizer_DrawKeyRing || fn == Randomizer_DrawOverworldKey ||
           fn == Randomizer_DrawBossKey || fn == Randomizer_DrawMap || fn == Randomizer_DrawCompass ||
           fn == Randomizer_DrawDoubleDefense || fn == Randomizer_DrawPowerBracelet ||
           fn == Randomizer_DrawBronzeScale || fn == Randomizer_DrawBombchuBag ||
           fn == Randomizer_DrawBombchuBagInLogic;
}

int ModelForItem(uint16_t id, ComboModel* out, const char** reason) {
    Combo_ModelInit(out);
    *reason = nullptr;
    if (id == (uint16_t)RG_NONE || id >= (uint16_t)RG_MAX) {
        *reason = "unknown id";
        return 0;
    }
    const RandomizerGet rg = FirstTier((RandomizerGet)id);
    const GetItemEntry* entry = Rando::StaticData::RetrieveItem(rg).GetStaticGIEntry();
    if (entry == nullptr) {
        *reason = "no static get-item entry";
        return 0;
    }
    const CustomDrawFunc custom = entry->drawFunc;
    if (custom != nullptr && !CustomDrawIsGidRecolour(custom)) {
        if (custom == Randomizer_DrawMasterSword) {
            // Randomizer_DrawMasterSword: the pedestal sword, scrolled, at 0.05
            // and turned 2.1 rad about Z.
            out->scale = 0.05f;
            out->rotation[2] = (int16_t)(2.1f * 32768.0f / 3.14159265f);
            out->scrolls[0] = TwoTileScroll(8, kOpa, 1, 0, 32, 32, 0, 0, 32, 32);
            Combo_ModelAddPart(out, object_toki_objects_DL_001BD0, kOpa, 0);
        } else if (custom == Randomizer_DrawRocsFeather) {
            Combo_ModelAddPart(out, gGiRocsFeatherDL, kXlu, 0);
        } else {
            *reason = "SoH custom draw that is not re-expressed";
            return 0;
        }
        DefaultSetups(out);
        if (!Combo_ModelIsWellFormed(out)) {
            *reason = "custom recipe produced a malformed model";
            return 0;
        }
        return 1;
    }
    return ModelForDrawId((int)entry->gid, out, reason);
}

// ---- Host-native mapping (#577 M7 fills this) --------------------------------
//
// When MM hands OoT a model whose paths live in a directory both archives carry,
// OoT draws its OWN equivalent: a row of OoT_sDrawItemTable, keyed by the MM
// model's first list. The operator ruled host-native mapping first; the rows are
// M7's data, so none answers yet and every colliding MM model is "no model".
struct HostNativeRow {
    const char* foreignFirstList; // "__OTR__objects/<dir>/<name>" as MM answers it
    s16 hostDrawId;               // OoT_sDrawItemTable row
};
constexpr std::array<HostNativeRow, 0> kHostNativeRows{};

} // namespace

/** THE SOURCE. C linkage so the test row can prove the registered pointer is
 *  this function (a registrar the linker dropped would leave OoT unregistered). */
extern "C" int OoT_ComboModel(uint16_t id, ComboModel* out) {
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

/** THE HOST-NATIVE MAPPER OoT registers for itself as a host. */
extern "C" int OoT_ComboModelHostNative(const ComboModel* foreign, uint16_t* hostKey) {
    if (foreign == nullptr || hostKey == nullptr || foreign->partCount == 0 || foreign->parts[0].dl == nullptr) {
        return 0;
    }
    for (const HostNativeRow& row : kHostNativeRows) {
        if (std::strcmp(row.foreignFirstList, foreign->parts[0].dl) == 0 && row.hostDrawId >= 0 &&
            row.hostDrawId < OoT_GetItem_DrawTableCount()) {
            *hostKey = (uint16_t)row.hostDrawId;
            return 1;
        }
    }
    return 0;
}

// ---- TEST BRIDGES (ForeignModel row, src/common/tests/test_foreign_model.c) ---

extern "C" int OoT_ComboModel_TestIdSpace(void) {
    return (int)RG_MAX;
}

extern "C" int OoT_ComboModel_TestDrawRowCount(void) {
    return (int)OoT_GetItem_DrawTableCount();
}

/** A draw row's answer: 1 and *out, or 0 and *reason. */
extern "C" int OoT_ComboModel_TestForDrawRow(int drawId, ComboModel* out, const char** reason) {
    const char* why = nullptr;
    const int answered = ModelForDrawId(drawId, out, &why);
    if (reason != nullptr) {
        *reason = why;
    }
    return answered;
}

/** A draw row's lists (NULL where the row has none): 1, or 0 past the table. */
extern "C" int OoT_ComboModel_TestDrawRowLists(int drawId, const char* lists[COMBO_MODEL_MAX_PARTS]) {
    OoTGetItemDrawFn fn = nullptr;
    Gfx* const* dlists = nullptr;
    if (drawId < 0 || drawId > INT16_MAX || OoT_GetItem_DrawTableRow((s16)drawId, &fn, &dlists) != 1) {
        return 0;
    }
    for (int i = 0; i < COMBO_MODEL_MAX_PARTS; i++) {
        lists[i] = reinterpret_cast<const char*>(dlists[i]);
    }
    return 1;
}

/** An item's reason for answering no model (NULL when it answers one). */
extern "C" const char* OoT_ComboModel_TestItemReason(uint16_t id) {
    ComboModel model;
    const char* reason = nullptr;
    return ModelForItem(id, &model, &reason) == 1 ? nullptr : reason;
}

namespace {
struct OoTModelRegistrar {
    OoTModelRegistrar() {
        Combo_RegisterModelSource((uint8_t)GAME_OOT, OoT_ComboModel);
        Combo_RegisterHostNativeModel((uint8_t)GAME_OOT, OoT_ComboModelHostNative);
    }
};
const OoTModelRegistrar gOoTModelRegistrar;
} // namespace

#endif // RSBS_SINGLE_EXECUTABLE
