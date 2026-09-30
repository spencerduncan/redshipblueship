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
 *  - some FALL BACK to their entry's `gid` row, the vanilla model, as the
 *    textbox icon does for the same items. Two kinds, one rule:
 *     - recolours of that row's model (small and boss keys, maps, compasses,
 *       Double Defense, the Power Bracelet, the bronze scale);
 *     - SUBSTITUTES that draw a different model (the key ring's gKeyring* lists
 *       or five small keys, the overworld key's gHouseKeyDL, the bombchu bag's
 *       gBombchuBag* lists): the vanilla row is a stand-in for them, not the
 *       model OoT shows. #577 M7 keeps the stand-in: those rows (small key,
 *       bombchu) live in colliding directories, so MM draws its own small key
 *       or bombchu for them through its host-native table. That is a
 *       DIFFERENT item's model (a key ring or house key shows a small key, a
 *       bombchu bag a bombchu), an open exception to CheckQueue.cpp's
 *       "never a different item's model" rule awaiting the operator's call
 *       (PR #829); answering no model here restores the stand-in;
 *  - the Master Sword and Roc's Feather are re-expressed from their draw
 *    functions (one list each);
 *  - every other custom draw (boss and bean souls, ocarina buttons, jabber nuts,
 *    the action-shuffle abilities, the Triforce piece, the fishing pole, the
 *    skeleton key, the mystery item) answers no model: its `gid` is a
 *    placeholder that would draw the wrong thing.
 * The ice trap has no custom draw, but OoT_Player_DrawGetItemImpl special-cases
 * it (a growing ice fragment, not its entry's gold-rupee row), so it answers no
 * model, as MM's source declines RI_TRAP.
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

/** SoH custom draws that fall back to their entry's `gid` row: the recolours
 *  of that row's model, and the substitutes (key ring, overworld key, bombchu
 *  bag) that draw a different model the row stands in for (#577 M7). */
bool CustomDrawFallsBackToGidRow(CustomDrawFunc fn) {
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
    if (rg == RG_ICE_TRAP) {
        // OoT_Player_DrawGetItemImpl draws it by getItemId (Player_DrawGetItemIceTrap:
        // a growing ice fragment), never through its entry's GID_RUPEE_GOLD row.
        *reason = "ice trap: OoT special-cases its draw, not its entry's row";
        return 0;
    }
    const GetItemEntry* entry = Rando::StaticData::RetrieveItem(rg).GetStaticGIEntry();
    if (entry == nullptr) {
        *reason = "no static get-item entry";
        return 0;
    }
    const CustomDrawFunc custom = entry->drawFunc;
    if (custom != nullptr && !CustomDrawFallsBackToGidRow(custom)) {
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

// ---- Host-native mapping (#577 M7) --------------------------------------------
//
// When MM hands OoT a model whose paths live in a directory both archives carry,
// OoT draws its OWN equivalent: a row of OoT_sDrawItemTable, keyed by the MM
// model's whole part list (foreign_model.h, ComboHostNativeRow).
//
// Each MM get-item model whose lists live in a colliding directory has a row:
// OoT's own model for the same item (MM's hookshot draws OoT's hookshot, MM's
// Hero's Shield OoT's Hylian Shield, MM's huge rupee OoT's gold rupee), or
// "no model" with its reason where OoT has no such item. The ForeignModel row
// (M11) walks every MM draw row and item against this table, so a colliding MM
// model without a row, and a row no MM model reaches, both fail.
const ComboHostNativeRow kHostNativeRows[] = {
    // MM GID_BOTTLE
    { { "object_gi_bottle/gGiEmptyBottleCorkDL", "object_gi_bottle/gGiEmptyBottleGlassDL" }, GID_BOTTLE, nullptr },
    // MM GID_KEY_SMALL
    { { "object_gi_key/gGiSmallKeyDL" }, GID_KEY_SMALL, nullptr },
    // MM GID_04
    { { "object_gi_melody/gGiSerenadeColorDL", "object_gi_melody/gGiSongNoteDL" }, GID_SONG_SERENADE, nullptr },
    // MM GID_05
    { { "object_gi_melody/gGiRequiemColorDL", "object_gi_melody/gGiSongNoteDL" }, GID_SONG_REQUIEM, nullptr },
    // MM GID_06
    { { "object_gi_melody/gGiNocturneColorDL", "object_gi_melody/gGiSongNoteDL" }, GID_SONG_NOCTURNE, nullptr },
    // MM GID_07
    { { "object_gi_melody/gGiPreludeColorDL", "object_gi_melody/gGiSongNoteDL" }, GID_SONG_PRELUDE, nullptr },
    // MM GID_RECOVERY_HEART
    { { "object_gi_heart/gGiRecoveryHeartDL" }, GID_HEART, nullptr },
    // MM GID_KEY_BOSS
    { { "object_gi_bosskey/gGiBossKeyDL", "object_gi_bosskey/gGiBossKeyGemDL" }, GID_KEY_BOSS, nullptr },
    // MM GID_COMPASS
    { { "object_gi_compass/gGiCompassDL", "object_gi_compass/gGiCompassGlassDL" }, GID_COMPASS, nullptr },
    // MM GID_DEKU_NUTS
    { { "object_gi_nuts/gGiNutDL" }, GID_NUTS, nullptr },
    // MM GID_HEART_CONTAINER
    { { "object_gi_hearts/gGiHeartBorderDL", "object_gi_hearts/gGiHeartContainerDL" }, GID_HEART_CONTAINER, nullptr },
    // MM GID_HEART_PIECE
    { { "object_gi_hearts/gGiHeartBorderDL", "object_gi_hearts/gGiHeartPieceDL" }, GID_HEART_PIECE, nullptr },
    // MM GID_QUIVER_30
    { { "object_gi_arrowcase/gGiQuiver30InnerColorDL", "object_gi_arrowcase/gGiQuiverInnerDL",
        "object_gi_arrowcase/gGiQuiver30OuterColorDL", "object_gi_arrowcase/gGiQuiverOuterDL" },
      GID_QUIVER_30,
      nullptr },
    // MM GID_QUIVER_40
    { { "object_gi_arrowcase/gGiQuiver40InnerColorDL", "object_gi_arrowcase/gGiQuiverInnerDL",
        "object_gi_arrowcase/gGiQuiver40OuterColorDL", "object_gi_arrowcase/gGiQuiverOuterDL" },
      GID_QUIVER_40,
      nullptr },
    // MM GID_QUIVER_50
    { { "object_gi_arrowcase/gGiQuiver50InnerColorDL", "object_gi_arrowcase/gGiQuiverInnerDL",
        "object_gi_arrowcase/gGiQuiver50OuterColorDL", "object_gi_arrowcase/gGiQuiverOuterDL" },
      GID_QUIVER_50,
      nullptr },
    // MM GID_BOMB_BAG_20
    { { "object_gi_bombpouch/gGiBombBag20BagColorDL", "object_gi_bombpouch/gGiBombBagDL",
        "object_gi_bombpouch/gGiBombBag20RingColorDL", "object_gi_bombpouch/gGiBombBagRingDL" },
      GID_BOMB_BAG_20,
      nullptr },
    // MM GID_BOMB_BAG_30
    { { "object_gi_bombpouch/gGiBombBag30BagColorDL", "object_gi_bombpouch/gGiBombBagDL",
        "object_gi_bombpouch/gGiBombBag30RingColorDL", "object_gi_bombpouch/gGiBombBagRingDL" },
      GID_BOMB_BAG_30,
      nullptr },
    // MM GID_BOMB_BAG_40
    { { "object_gi_bombpouch/gGiBombBag40BagColorDL", "object_gi_bombpouch/gGiBombBagDL",
        "object_gi_bombpouch/gGiBombBag40RingColorDL", "object_gi_bombpouch/gGiBombBagRingDL" },
      GID_BOMB_BAG_40,
      nullptr },
    // MM GID_DEKU_STICK
    { { "object_gi_stick/gGiStickDL" }, GID_STICK, nullptr },
    // MM GID_DUNGEON_MAP
    { { "object_gi_map/gGiDungeonMapDL" }, GID_DUNGEON_MAP, nullptr },
    // MM GID_MAGIC_JAR_SMALL
    { { "object_gi_magicpot/gGiMagicJarSmallDL" }, GID_MAGIC_SMALL, nullptr },
    // MM GID_MAGIC_JAR_BIG
    { { "object_gi_magicpot/gGiMagicJarLargeDL" }, GID_MAGIC_LARGE, nullptr },
    // MM GID_BOMB
    { { "object_gi_bomb_1/gGiBombDL" }, GID_BOMB, nullptr },
    // MM GID_STONE_OF_AGONY
    { { "object_gi_map/gGiStoneOfAgonyDL" }, GID_STONE_OF_AGONY, nullptr },
    // MM GID_WALLET_ADULT
    { { "object_gi_purse/gGiAdultWalletColorDL", "object_gi_purse/gGiWalletDL",
        "object_gi_purse/gGiAdultWalletRupeeOuterColorDL", "object_gi_purse/gGiWalletRupeeOuterDL",
        "object_gi_purse/gGiAdultWalletStringColorDL", "object_gi_purse/gGiWalletStringDL",
        "object_gi_purse/gGiAdultWalletRupeeInnerColorDL", "object_gi_purse/gGiWalletRupeeInnerDL" },
      GID_WALLET_ADULT,
      nullptr },
    // MM GID_WALLET_GIANT
    { { "object_gi_purse/gGiGiantsWalletColorDL", "object_gi_purse/gGiWalletDL",
        "object_gi_purse/gGiGiantsWalletRupeeOuterColorDL", "object_gi_purse/gGiWalletRupeeOuterDL",
        "object_gi_purse/gGiGiantsWalletStringColorDL", "object_gi_purse/gGiWalletStringDL",
        "object_gi_purse/gGiGiantsWalletRupeeInnerColorDL", "object_gi_purse/gGiWalletRupeeInnerDL" },
      GID_WALLET_GIANT,
      nullptr },
    // MM GID_ARROWS_SMALL
    { { "object_gi_arrow/gGiArrowSmallDL" }, GID_ARROWS_SMALL, nullptr },
    // MM GID_ARROWS_MEDIUM
    { { "object_gi_arrow/gGiArrowMediumDL" }, GID_ARROWS_MEDIUM, nullptr },
    // MM GID_ARROWS_LARGE
    { { "object_gi_arrow/gGiArrowLargeDL" }, GID_ARROWS_LARGE, nullptr },
    // MM GID_BOMBCHU
    { { "object_gi_bomb_2/gGiBombchuDL" }, GID_BOMBCHU, nullptr },
    // MM GID_SHIELD_HERO
    { { "object_gi_shield_2/gGiHerosShieldEmblemDL", "object_gi_shield_2/gGiHerosShieldDL" },
      GID_SHIELD_HYLIAN,
      nullptr },
    // MM GID_HOOKSHOT (and GID_29, the same lists)
    { { "object_gi_hookshot/gGiHookshotEmptyDL", "object_gi_hookshot/gGiHookshotDL" }, GID_HOOKSHOT, nullptr },
    // MM GID_OCARINA
    { { "object_gi_ocarina/gGiOcarinaOfTimeDL", "object_gi_ocarina/gGiOcarinaOfTimeHolesDL" },
      GID_OCARINA_TIME,
      nullptr },
    // MM GID_MILK
    { { "object_gi_milk/gGiMilkBottleContentsDL", "object_gi_milk/gGiMilkBottleGlassDL" }, GID_MILK, nullptr },
    // MM GID_MASK_KEATON
    { { "object_gi_ki_tan_mask/gGiKeatonMaskDL", "object_gi_ki_tan_mask/gGiKeatonMaskEyesDL" },
      GID_MASK_KEATON,
      nullptr },
    // MM GID_BOW
    { { "object_gi_bow/gGiBowHandleDL", "object_gi_bow/gGiBowStringDL" }, GID_BOW, nullptr },
    // MM GID_LENS
    { { "object_gi_glasses/gGiLensDL", "object_gi_glasses/gGiLensGlassDL" }, GID_LENS, nullptr },
    // MM GID_POTION_GREEN
    { { "object_gi_liquid/gGiPotionContainerGreenPotColorDL", "object_gi_liquid/gGiPotionContainerPotDL",
        "object_gi_liquid/gGiPotionContainerGreenLiquidColorDL", "object_gi_liquid/gGiPotionContainerLiquidDL",
        "object_gi_liquid/gGiPotionContainerGreenPatternColorDL", "object_gi_liquid/gGiPotionContainerPatternDL" },
      GID_POTION_GREEN,
      nullptr },
    // MM GID_POTION_RED
    { { "object_gi_liquid/gGiPotionContainerRedPotColorDL", "object_gi_liquid/gGiPotionContainerPotDL",
        "object_gi_liquid/gGiPotionContainerRedLiquidColorDL", "object_gi_liquid/gGiPotionContainerLiquidDL",
        "object_gi_liquid/gGiPotionContainerRedPatternColorDL", "object_gi_liquid/gGiPotionContainerPatternDL" },
      GID_POTION_RED,
      nullptr },
    // MM GID_POTION_BLUE
    { { "object_gi_liquid/gGiPotionContainerBluePotColorDL", "object_gi_liquid/gGiPotionContainerPotDL",
        "object_gi_liquid/gGiPotionContainerBlueLiquidColorDL", "object_gi_liquid/gGiPotionContainerLiquidDL",
        "object_gi_liquid/gGiPotionContainerBluePatternColorDL", "object_gi_liquid/gGiPotionContainerPatternDL" },
      GID_POTION_BLUE,
      nullptr },
    // MM GID_SHIELD_MIRROR
    { { "object_gi_shield_3/gGiMirrorShieldEmptyDL", "object_gi_shield_3/gGiMirrorShieldDL" },
      GID_SHIELD_MIRROR,
      nullptr },
    // MM GID_MAGIC_BEANS
    { { "object_gi_bean/gGiBeanDL" }, GID_BEAN, nullptr },
    // MM GID_FISH
    { { "object_gi_fish/gGiFishContainerDL" }, GID_FISH, nullptr },
    // MM GID_SWORD_BGS
    { { "object_gi_longsword/gGiBiggoronSwordDL" }, GID_SWORD_BGS, nullptr },
    // MM GID_MASK_BUNNY
    { { "object_gi_rabit_mask/gGiBunnyHoodDL", "object_gi_rabit_mask/gGiBunnyHoodEyesDL" }, GID_MASK_BUNNY, nullptr },
    // MM GID_MASK_TRUTH
    { { "object_gi_truth_mask/gGiMaskOfTruthDL", "object_gi_truth_mask/gGiMaskOfTruthAccentsDL" },
      GID_MASK_TRUTH,
      nullptr },
    // MM GID_RUPEE_HUGE
    { { "object_gi_rupy/gGiGoldRupeeInnerColorDL", "object_gi_rupy/gGiRupeeInnerDL",
        "object_gi_rupy/gGiGoldRupeeOuterColorDL", "object_gi_rupy/gGiRupeeOuterDL" },
      GID_RUPEE_GOLD,
      nullptr },
    // MM GID_MASK_GORON
    { { "object_gi_golonmask/gGiGoronMaskEmptyDL", "object_gi_golonmask/gGiGoronMaskDL" }, GID_MASK_GORON, nullptr },
    // MM GID_MASK_ZORA
    { { "object_gi_zoramask/gGiZoraMaskEmptyDL", "object_gi_zoramask/gGiZoraMaskDL" }, GID_MASK_ZORA, nullptr },
    // MM GID_ARROW_FIRE
    { { "object_gi_m_arrow/gGiMagicArrowAmmoDL", "object_gi_m_arrow/gGiMagicArrowFireColorDL",
        "object_gi_m_arrow/gGiMagicArrowGlowDL" },
      GID_ARROW_FIRE,
      nullptr },
    // MM GID_ARROW_ICE
    { { "object_gi_m_arrow/gGiMagicArrowAmmoDL", "object_gi_m_arrow/gGiMagicArrowIceColorDL",
        "object_gi_m_arrow/gGiMagicArrowGlowDL" },
      GID_ARROW_ICE,
      nullptr },
    // MM GID_ARROW_LIGHT
    { { "object_gi_m_arrow/gGiMagicArrowAmmoDL", "object_gi_m_arrow/gGiMagicArrowLightColorDL",
        "object_gi_m_arrow/gGiMagicArrowGlowDL" },
      GID_ARROW_LIGHT,
      nullptr },
    // MM GID_SKULL_TOKEN
    { { "object_gi_sutaru/gGiSkulltulaTokenDL", "object_gi_sutaru/gGiSkulltulaTokenFlameDL" },
      GID_SKULL_TOKEN,
      nullptr },
    // MM GID_BUG
    { { "object_gi_insect/gGiBugContainerContentsDL", "object_gi_insect/gGiBugContainerGlassDL" }, GID_BUG, nullptr },
    // MM GID_POE
    { { "object_gi_ghost/gGiPoeContainerLidDL", "object_gi_ghost/gGiPoeContainerGlassDL",
        "object_gi_ghost/gGiPoeContainerPoeColorDL", "object_gi_ghost/gGiPoeContainerContentsDL" },
      GID_POE,
      nullptr },
    // MM GID_FAIRY_2
    { { "object_gi_soul/gGiFairyContainerBaseCapDL", "object_gi_soul/gGiFairyContainerGlassDL",
        "object_gi_soul/gGiFairyContainerContentsDL" },
      GID_FAIRY,
      nullptr },
    // MM GID_RUPEE_GREEN
    { { "object_gi_rupy/gGiGreenRupeeInnerColorDL", "object_gi_rupy/gGiRupeeInnerDL",
        "object_gi_rupy/gGiGreenRupeeOuterColorDL", "object_gi_rupy/gGiRupeeOuterDL" },
      GID_RUPEE_GREEN,
      nullptr },
    // MM GID_RUPEE_BLUE
    { { "object_gi_rupy/gGiBlueRupeeInnerColorDL", "object_gi_rupy/gGiRupeeInnerDL",
        "object_gi_rupy/gGiBlueRupeeOuterColorDL", "object_gi_rupy/gGiRupeeOuterDL" },
      GID_RUPEE_BLUE,
      nullptr },
    // MM GID_RUPEE_RED
    { { "object_gi_rupy/gGiRedRupeeInnerColorDL", "object_gi_rupy/gGiRupeeInnerDL",
        "object_gi_rupy/gGiRedRupeeOuterColorDL", "object_gi_rupy/gGiRupeeOuterDL" },
      GID_RUPEE_RED,
      nullptr },
    // MM GID_BIG_POE
    { { "object_gi_ghost/gGiPoeContainerLidDL", "object_gi_ghost/gGiPoeContainerGlassDL",
        "object_gi_ghost/gGiPoeContainerBigPoeColorDL", "object_gi_ghost/gGiPoeContainerContentsDL" },
      GID_BIG_POE,
      nullptr },
    // MM GID_RUPEE_PURPLE
    { { "object_gi_rupy/gGiPurpleRupeeInnerColorDL", "object_gi_rupy/gGiRupeeInnerDL",
        "object_gi_rupy/gGiPurpleRupeeOuterColorDL", "object_gi_rupy/gGiRupeeOuterDL" },
      GID_RUPEE_PURPLE,
      nullptr },
    // MM GID_RUPEE_SILVER
    { { "object_gi_rupy/gGiSilverRupeeInnerColorDL", "object_gi_rupy/gGiRupeeInnerDL",
        "object_gi_rupy/gGiSilverRupeeOuterColorDL", "object_gi_rupy/gGiRupeeOuterDL" },
      -1,
      "silver rupee: OoT's get-item table has no silver rupee row" },
    // MM GID_SWORD_KOKIRI
    { { "object_gi_sword_1/gGiKokiriSwordBladeHiltDL", "object_gi_sword_1/gGiKokiriSwordGuardDL" },
      GID_SWORD_KOKIRI,
      nullptr },
    // MM GID_SKULL_TOKEN_2
    { { "object_st/gSkulltulaTokenDL", "object_st/gSkulltulaTokenFlameDL" }, GID_SKULL_TOKEN_2, nullptr },
};
constexpr int kHostNativeRowCount = (int)(sizeof(kHostNativeRows) / sizeof(kHostNativeRows[0]));

/** 1 and *hostKey: OoT's own row. 0 and *reason (NULL: no row at all). */
int MapHostNative(const ComboModel* foreign, uint16_t* hostKey, const char** reason, int* tableRow) {
    *reason = nullptr;
    *tableRow = Combo_HostNativeFind(kHostNativeRows, kHostNativeRowCount, foreign);
    if (*tableRow < 0) {
        return 0;
    }
    const ComboHostNativeRow& row = kHostNativeRows[*tableRow];
    if (row.hostDrawId < 0 || row.hostDrawId >= OoT_GetItem_DrawTableCount()) {
        *reason = row.noModelReason;
        return 0;
    }
    *hostKey = (uint16_t)row.hostDrawId;
    return 1;
}

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
    if (foreign == nullptr || hostKey == nullptr) {
        return 0;
    }
    const char* reason = nullptr;
    int tableRow = -1;
    return MapHostNative(foreign, hostKey, &reason, &tableRow);
}

// ---- TEST BRIDGES (ForeignModel row, src/common/tests/test_foreign_model.c) ---

/** OoT's host-native answer for a foreign model: 1 and *hostKey, or 0 and
 *  *reason (NULL when no row names the model). *tableRow: the row, or -1. */
extern "C" int OoT_ComboModelHostNative_TestAnswer(const ComboModel* foreign, uint16_t* hostKey, const char** reason,
                                                   int* tableRow) {
    return MapHostNative(foreign, hostKey, reason, tableRow);
}

extern "C" int OoT_ComboModelHostNative_TestRowCount(void) {
    return kHostNativeRowCount;
}

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
