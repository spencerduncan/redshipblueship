/**
 * @file ForeignMaskMM.cpp
 * @brief MM wears and draws an Ocarina of Time mask (#977, epic #975 P3).
 *
 * WHERE IT LIVES. In 2ship_rando, which single-exe links WHOLE_ARCHIVE, not
 * beside 2S2H's Enhancements/Masks/PersistentMasks.cpp (its near-complete
 * template): 2ship_enh is a plain static archive and none of
 * Enhancements/Masks/*.cpp reaches redship.map (games/mm/CMakeLists.txt).
 *
 * THE FIELD. `gSaveContext.save.shipSaveInfo.rando.foreignMask` holds the worn
 * mask as a ComboForeignMask (src/common/foreign_masks.h), never currentMask or
 * equippedMask: MM indexes sMaskObjectIds, sMaskItemIds and D_801C0B20 by both
 * with no bound (z_player_lib.c). Every reader goes through
 * Combo_ForeignMask_Wearable: a mask is worn only while the frozen world's
 * item rules hold its extension bit. It is appended to RandoSaveInfo, inside
 * Save, so it crosses with the frozen MM half, rides the .redsave's Tier 3, and
 * stays on through scene changes the way equippedMask does.
 *
 * MM'S MASK RULES, applied to it as MM applies them to its own:
 *  - human form only: transforming takes it off (MM_ForeignMask_ApplyRules),
 *    and it cannot be put on in another form (MM_ForeignMask_Use);
 *  - one mask at a time: a native mask put on takes it off, and putting it on
 *    takes the native mask off (currentMask and equippedMask to NONE).
 *
 * THE PAUSE SLOTS (OoTMM's sharing: Stone/Gerudo/Skull share Stone's slot,
 * Gibdo/Spooky share Gibdo's). MM's inventory keeps the slot's own item, so
 * the C button, the item tables and the save all keep an MM ItemId; the slot's
 * chosen variant is `rando.foreignMaskSlotSel[slot - 1]` (NONE: the slot's own
 * mask). On the mask page, A on a shared slot with more than one variant
 * cycles it, as KaleidoItemPage.cpp's short-circuit does for two-item trade
 * slots, and the page draws the chosen variant's icon (one guarded call in
 * z_kaleido_mask.c). Using the slot's C button while a foreign variant is
 * chosen puts that mask on or takes it off (VB_USE_ITEM_EQUIP_MASK) instead of
 * the slot's own. Ownership is P5's (#980): until it registers a source,
 * nothing is owned, every slot has one variant, and the page is unchanged.
 * Still P5's: an owned OoT mask whose slot's own mask is not owned has no cell
 * to select (the cell is empty), the C button shows the slot's own icon, and
 * the item description names the slot's own mask.
 *
 * THE DRAW. OnPlayerPostLimbDraw on PLAYER_LIMB_HEAD, where MM draws its own
 * masks. Each OoT list loads the head matrix itself from segment 0x0D + 0x1C0
 * (OoT's sMaskDlists); MM's flex skeleton draw binds segment 0x0D to its limb
 * matrices the same way (z_skelanime.c), and both child skeletons put the head
 * at that index. The list is named by its path in redship-mm.o2r, the curated
 * OoT half MM mounts, never through oot.o2r's (#981).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "Rando/Rando.h"
#include "Rando/ForeignMaskMM.h"

#include "context.h"       // src/common: Context_GetCurrentGame
#include "foreign_masks.h" // src/common: the table and the equip rules

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
#include "z64skin_matrix.h"
void func_8082E1F0(Player* player, u16 sfxId);
}

static_assert(sizeof(((RandoSaveInfo*)0)->foreignMaskSlotSel) == COMBO_FOREIGN_MASK_SLOT_COUNT - 1,
              "one persisted variant per shared pause slot");

namespace {

bool PathMounted(const char* path) {
    static const char kOtr[] = "__OTR__";
    if (path == nullptr || std::strncmp(path, kOtr, sizeof(kOtr) - 1) != 0) {
        return false;
    }
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr) {
        return false;
    }
    auto archives = ctx->GetResourceManager()->GetArchiveManager();
    return archives != nullptr && archives->HasFile(std::string(path + sizeof(kOtr) - 1));
}

RandoSaveInfo& RandoSave() {
    return gSaveContext.save.shipSaveInfo.rando;
}

/** The shared slot an MM item heads, or SLOT_NONE. */
uint8_t SlotForItem(int item) {
    switch (item) {
        case ITEM_MASK_STONE:
            return COMBO_FOREIGN_MASK_SLOT_MM_STONE;
        case ITEM_MASK_GIBDO:
            return COMBO_FOREIGN_MASK_SLOT_MM_GIBDO;
        default:
            return COMBO_FOREIGN_MASK_SLOT_NONE;
    }
}

/** The shared slot an MM mask heads, or SLOT_NONE. */
uint8_t SlotForMask(int maskId) {
    switch (maskId) {
        case PLAYER_MASK_STONE:
            return COMBO_FOREIGN_MASK_SLOT_MM_STONE;
        case PLAYER_MASK_GIBDO:
            return COMBO_FOREIGN_MASK_SLOT_MM_GIBDO;
        default:
            return COMBO_FOREIGN_MASK_SLOT_NONE;
    }
}

int NativeItemOfSlot(uint8_t slot) {
    return slot == COMBO_FOREIGN_MASK_SLOT_MM_STONE ? ITEM_MASK_STONE : ITEM_MASK_GIBDO;
}

bool NativeOwned(uint8_t slot) {
    const int item = NativeItemOfSlot(slot);
    return INV_CONTENT(item) == item;
}

} // namespace

extern "C" uint8_t MM_ForeignMask_Worn(void) {
    const uint8_t stored = RandoSave().foreignMask;
    return Combo_ForeignMask_Wearable(stored, GAME_MM) ? stored : (uint8_t)COMBO_FOREIGN_MASK_NONE;
}

/**
 * Put `mask` on (or take it off when worn): human form only. Putting one on
 * takes the native mask off. 1 when the worn field changed, else 0.
 */
extern "C" int MM_ForeignMask_Use(Player* player, uint8_t mask) {
    if (player == nullptr || player->transformation != PLAYER_FORM_HUMAN ||
        !Combo_ForeignMask_Wearable(mask, GAME_MM)) {
        return 0;
    }
    const uint8_t before = MM_ForeignMask_Worn();
    const uint8_t after = Combo_ForeignMask_Toggle(before, mask, GAME_MM);
    if (after == before) {
        return 0;
    }
    RandoSave().foreignMask = after;
    if (after != COMBO_FOREIGN_MASK_NONE) {
        player->prevMask = player->currentMask;
        player->currentMask = PLAYER_MASK_NONE;
        gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
    }
    return 1;
}

/** MM's mask rules on the worn field, once per player update. */
extern "C" void MM_ForeignMask_ApplyRules(Player* player) {
    const uint8_t stored = RandoSave().foreignMask;
    uint8_t worn = MM_ForeignMask_Worn();
    if (worn != COMBO_FOREIGN_MASK_NONE && player != nullptr &&
        (player->transformation != PLAYER_FORM_HUMAN || player->currentMask != PLAYER_MASK_NONE)) {
        worn = COMBO_FOREIGN_MASK_NONE;
    }
    if (worn != stored) {
        RandoSave().foreignMask = worn;
    }
}

/** A shared slot's chosen variant, sanitized: an owned OoT mask sharing it, or NONE (the slot's own mask). */
extern "C" uint8_t MM_ForeignMask_SlotSelection(uint8_t slot) {
    if (slot == COMBO_FOREIGN_MASK_SLOT_NONE || slot >= COMBO_FOREIGN_MASK_SLOT_COUNT) {
        return COMBO_FOREIGN_MASK_NONE;
    }
    const uint8_t sel = RandoSave().foreignMaskSlotSel[slot - 1];
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(sel);
    if (info == nullptr || info->host != GAME_MM || info->mmSharedSlot != slot ||
        !Combo_ForeignMask_Owned(GAME_MM, sel)) {
        return COMBO_FOREIGN_MASK_NONE;
    }
    return sel;
}

/** Cycle a shared slot's variant (dir > 0 forward). 1 when it changed. */
extern "C" int MM_ForeignMask_CycleSlot(uint8_t slot, int dir) {
    if (slot == COMBO_FOREIGN_MASK_SLOT_NONE || slot >= COMBO_FOREIGN_MASK_SLOT_COUNT) {
        return 0;
    }
    const uint8_t current = MM_ForeignMask_SlotSelection(slot);
    const uint8_t next = Combo_ForeignMask_SlotCycle(slot, current, NativeOwned(slot) ? 1 : 0, dir);
    RandoSave().foreignMaskSlotSel[slot - 1] = next;
    return next != current;
}

/** How many variants a shared slot cycles through (1 or 0: no cycling). */
extern "C" int MM_ForeignMask_SlotVariantCount(uint8_t slot) {
    uint8_t variants[COMBO_FOREIGN_MASK_SLOT_MAX_VARIANTS];
    return Combo_ForeignMask_SlotVariants(slot, NativeOwned(slot) ? 1 : 0, variants,
                                          COMBO_FOREIGN_MASK_SLOT_MAX_VARIANTS);
}

/**
 * VB_USE_ITEM_EQUIP_MASK's question: the player used the C button of native
 * mask `maskId`. When its shared slot has a foreign variant chosen, that mask
 * is used instead and this answers 1 (the vanilla equip must not run); else 0.
 */
extern "C" int MM_ForeignMask_OnEquipNative(Player* player, int maskId) {
    const uint8_t sel = MM_ForeignMask_SlotSelection(SlotForMask(maskId));
    if (sel == COMBO_FOREIGN_MASK_NONE) {
        return 0;
    }
    MM_ForeignMask_Use(player, sel);
    return 1;
}

/**
 * The mask page's icon for cell `maskSlot` (0-based on the page), whose own
 * item's icon is `nativeIcon`: the chosen variant's icon on a shared slot, when
 * redship-mm.o2r holds it; `nativeIcon` otherwise. Called by z_kaleido_mask.c.
 */
extern "C" const char* MM_ForeignMask_PageIcon(int maskSlot, const char* nativeIcon) {
    if (maskSlot < 0 || maskSlot >= MASK_NUM_SLOTS) {
        return nativeIcon;
    }
    const uint8_t slot = SlotForItem(gSaveContext.save.saveInfo.inventory.items[maskSlot + ITEM_NUM_SLOTS]);
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(MM_ForeignMask_SlotSelection(slot));
    if (info == nullptr || !PathMounted(info->icon)) {
        return nativeIcon;
    }
    return info->icon;
}

/** The list MM's head draw would emit for the live worn field, or NULL. */
extern "C" const char* MM_ForeignMask_WornDl(Player* player) {
    const uint8_t worn = MM_ForeignMask_Worn();
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(worn);
    if (info == nullptr || player == nullptr || player->transformation != PLAYER_FORM_HUMAN ||
        player->currentMask != PLAYER_MASK_NONE) {
        return nullptr;
    }
    return info->wornDl;
}

extern "C" uint8_t MM_ForeignMask_StoredRaw(void) {
    return RandoSave().foreignMask;
}

extern "C" void MM_ForeignMask_SetStoredRaw(uint8_t value) {
    RandoSave().foreignMask = value;
}

extern "C" uint8_t MM_ForeignMask_SlotSelRaw(uint8_t slot) {
    return slot == COMBO_FOREIGN_MASK_SLOT_NONE || slot >= COMBO_FOREIGN_MASK_SLOT_COUNT
               ? (uint8_t)COMBO_FOREIGN_MASK_NONE
               : RandoSave().foreignMaskSlotSel[slot - 1];
}

extern "C" void MM_ForeignMask_SetSlotSelRaw(uint8_t slot, uint8_t value) {
    if (slot != COMBO_FOREIGN_MASK_SLOT_NONE && slot < COMBO_FOREIGN_MASK_SLOT_COUNT) {
        RandoSave().foreignMaskSlotSel[slot - 1] = value;
    }
}

extern "C" size_t MM_ForeignMask_FieldOffset(void) {
    return offsetof(SaveContext, save) + offsetof(Save, shipSaveInfo) + offsetof(ShipSaveInfo, rando) +
           offsetof(RandoSaveInfo, foreignMask);
}

// Outside the anonymous namespace: OPEN_DISPS block-declares the
// FrameInterpolation_Record* pair, and inside one GCC binds them to
// (anonymous namespace) symbols that never link.
static void DrawOnHead(Player* player, s32 limbIndex) {
    (void)limbIndex;
    if (Context_GetCurrentGame() != GAME_MM || MM_gPlayState == nullptr || player == nullptr ||
        player != GET_PLAYER(MM_gPlayState)) {
        return;
    }
    const char* dl = MM_ForeignMask_WornDl(player);
    if (dl == nullptr || !PathMounted(dl)) {
        return;
    }
    // MM's own first-person test for drawing masks (z_player.c, Player_Draw):
    // with the camera inside the head, no mask.
    if (player->stateFlags1 & PLAYER_STATE1_100000) {
        Vec3f projected;
        MM_SkinMatrix_Vec3fMtxFMultXYZ(&MM_gPlayState->viewProjectionMtxF, &player->actor.focus.pos, &projected);
        if (projected.z < -4.0f) {
            return;
        }
    }

    OPEN_DISPS(MM_gPlayState->state.gfxCtx);
    // The head list's geometry mode, as PersistentMasks.cpp restores it before
    // a mask list that does not set its own.
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_FOG | G_LIGHTING | G_SHADING_SMOOTH);
    MM_gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);
    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
}

namespace {

void RegisterForeignMasksMM() {
    COND_ID_HOOK(OnPlayerPostLimbDraw, PLAYER_LIMB_HEAD, true, DrawOnHead);

    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, true, [](Actor* actor) {
        if (Context_GetCurrentGame() == GAME_MM) {
            MM_ForeignMask_ApplyRules((Player*)actor);
        }
    });

    COND_VB_SHOULD(VB_USE_ITEM_EQUIP_MASK, true, {
        PlayerMask* maskId = va_arg(args, PlayerMask*);
        if (MM_gPlayState == nullptr || maskId == nullptr) {
            return;
        }
        Player* player = GET_PLAYER(MM_gPlayState);
        const uint8_t before = MM_ForeignMask_Worn();
        if (MM_ForeignMask_OnEquipNative(player, (int)*maskId)) {
            *should = false;
            func_8082E1F0(player, MM_ForeignMask_Worn() != before && MM_ForeignMask_Worn() != COMBO_FOREIGN_MASK_NONE
                                      ? NA_SE_PL_CHANGE_ARMS
                                      : NA_SE_PL_TAKE_OUT_SHIELD);
        }
    });

    // A on a shared slot with a choice cycles it instead of showing the text.
    COND_VB_SHOULD(VB_KALEIDO_DISPLAY_ITEM_TEXT, true, {
        u16* cursorItem = va_arg(args, u16*);
        if (cursorItem == nullptr) {
            return;
        }
        const uint8_t slot = SlotForItem(*cursorItem);
        if (slot == COMBO_FOREIGN_MASK_SLOT_NONE || MM_ForeignMask_SlotVariantCount(slot) < 2) {
            return;
        }
        *should = false;
        MM_ForeignMask_CycleSlot(slot, 1);
        Audio_PlaySfx(NA_SE_SY_CURSOR);
    });
}

} // namespace

// Always registered: the hooks act only on a worn field or a slot choice,
// which nothing but the items P5 adds (or a test) writes.
static RegisterShipInitFunc initFunc(RegisterForeignMasksMM, {});

#endif // RSBS_SINGLE_EXECUTABLE
