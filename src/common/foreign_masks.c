/**
 * @file foreign_masks.c
 * @brief The cross-game worn-mask table and equip rules (#977). See foreign_masks.h.
 */

#include "foreign_masks.h"

#include <stddef.h>

// Paths are the extracted resources' own names, verbatim (the curated halves
// copy paths unchanged; see scripts/make_redship_otr.py constraint 1). Each one
// is listed in assets/crossgame/manifest.txt in its host's direction, which the
// ForeignMaskTable row checks against the manifest compiled into the binary.
static const ComboForeignMaskInfo sForeignMasks[COMBO_FOREIGN_MASK_COUNT] = {
    { COMBO_FOREIGN_MASK_NONE, GAME_NONE, GAME_NONE, COMBO_FOREIGN_MASK_SLOT_NONE, 0, NULL, NULL, NULL },
    // MM's worn lists are the ones MM's own D_801C0B20 names
    // (games/mm/src/code/z_player_lib.c), drawn in the head limb's space.
    { COMBO_FOREIGN_MASK_MM_BLAST, GAME_MM, GAME_OOT, COMBO_FOREIGN_MASK_SLOT_NONE, 1, "Blast Mask",
      "__OTR__objects/object_mask_bakuretu/object_mask_bakuretu_DL_0005C0",
      "__OTR__icon_item_static_yar/gItemIconBlastMaskTex" },
    { COMBO_FOREIGN_MASK_MM_STONE, GAME_MM, GAME_OOT, COMBO_FOREIGN_MASK_SLOT_NONE, 0, "Stone Mask",
      "__OTR__objects/object_mask_stone/object_mask_stone_DL_000820",
      "__OTR__icon_item_static_yar/gItemIconStoneMaskTex" },
    { COMBO_FOREIGN_MASK_MM_KAMARO, GAME_MM, GAME_OOT, COMBO_FOREIGN_MASK_SLOT_NONE, 0, "Kamaro's Mask",
      "__OTR__objects/object_mask_dancer/object_mask_dancer_DL_000EF0",
      "__OTR__icon_item_static_yar/gItemIconKamaroMaskTex" },
    // OoT's worn lists are the ones OoT's own sMaskDlists names
    // (ovl_player_actor/z_player.c); each loads the head matrix itself from
    // segment 0x0D + 0x1C0, which MM's flex skeleton draw binds the same way.
    { COMBO_FOREIGN_MASK_OOT_GERUDO, GAME_OOT, GAME_MM, COMBO_FOREIGN_MASK_SLOT_MM_STONE, 0, "Gerudo Mask",
      "__OTR__objects/object_link_child/gLinkChildGerudoMaskDL",
      "__OTR__textures/icon_item_static/gItemIconMaskGerudoTex" },
    { COMBO_FOREIGN_MASK_OOT_SKULL, GAME_OOT, GAME_MM, COMBO_FOREIGN_MASK_SLOT_MM_STONE, 0, "Skull Mask",
      "__OTR__objects/object_link_child/gLinkChildSkullMaskDL",
      "__OTR__textures/icon_item_static/gItemIconMaskSkullTex" },
    { COMBO_FOREIGN_MASK_OOT_SPOOKY, GAME_OOT, GAME_MM, COMBO_FOREIGN_MASK_SLOT_MM_GIBDO, 0, "Spooky Mask",
      "__OTR__objects/object_link_child/gLinkChildSpookyMaskDL",
      "__OTR__textures/icon_item_static/gItemIconMaskSpookyTex" },
};

// Indexed by GameId; GAME_NONE's slot is never written.
static ComboForeignMaskOwnedFn sOwnedSources[3];

static int HostValid(GameId host) {
    return host == GAME_OOT || host == GAME_MM;
}

const ComboForeignMaskInfo* Combo_ForeignMask_Info(uint8_t id) {
    if (id == COMBO_FOREIGN_MASK_NONE || id >= COMBO_FOREIGN_MASK_COUNT) {
        return NULL;
    }
    return &sForeignMasks[id];
}

int Combo_ForeignMask_IsWornIn(uint8_t id, GameId host) {
    const ComboForeignMaskInfo* info = Combo_ForeignMask_Info(id);
    return info != NULL && HostValid(host) && info->host == (uint8_t)host;
}

uint8_t Combo_ForeignMask_Sanitize(uint8_t value, GameId host) {
    return Combo_ForeignMask_IsWornIn(value, host) ? value : (uint8_t)COMBO_FOREIGN_MASK_NONE;
}

uint8_t Combo_ForeignMask_Toggle(uint8_t worn, uint8_t requested, GameId host) {
    if (!Combo_ForeignMask_IsWornIn(requested, host)) {
        return worn;
    }
    return Combo_ForeignMask_Sanitize(worn, host) == requested ? (uint8_t)COMBO_FOREIGN_MASK_NONE : requested;
}

void Combo_ForeignMask_SetOwnedSource(GameId host, ComboForeignMaskOwnedFn fn) {
    if (HostValid(host)) {
        sOwnedSources[host] = fn;
    }
}

int Combo_ForeignMask_Owned(GameId host, uint8_t id) {
    if (!Combo_ForeignMask_IsWornIn(id, host) || sOwnedSources[host] == NULL) {
        return 0;
    }
    return sOwnedSources[host](id) ? 1 : 0;
}

int Combo_ForeignMask_SlotVariants(uint8_t slot, int nativeOwned, uint8_t* out, int cap) {
    int count = 0;
    if (slot == COMBO_FOREIGN_MASK_SLOT_NONE || slot >= COMBO_FOREIGN_MASK_SLOT_COUNT || out == NULL || cap <= 0) {
        return 0;
    }
    if (nativeOwned && count < cap) {
        out[count++] = (uint8_t)COMBO_FOREIGN_MASK_NONE;
    }
    for (uint8_t id = 1; id < COMBO_FOREIGN_MASK_COUNT && count < cap; id++) {
        if (sForeignMasks[id].host == (uint8_t)GAME_MM && sForeignMasks[id].mmSharedSlot == slot &&
            Combo_ForeignMask_Owned(GAME_MM, id)) {
            out[count++] = id;
        }
    }
    return count;
}

uint8_t Combo_ForeignMask_SlotCycle(uint8_t slot, uint8_t current, int nativeOwned, int dir) {
    uint8_t variants[COMBO_FOREIGN_MASK_SLOT_MAX_VARIANTS];
    const int n = Combo_ForeignMask_SlotVariants(slot, nativeOwned, variants, COMBO_FOREIGN_MASK_SLOT_MAX_VARIANTS);
    if (n == 0) {
        return (uint8_t)COMBO_FOREIGN_MASK_NONE;
    }
    for (int i = 0; i < n; i++) {
        if (variants[i] == current) {
            const int step = dir < 0 ? n - 1 : (dir > 0 ? 1 : 0);
            return variants[(i + step) % n];
        }
    }
    return variants[0];
}

int Combo_ListCycle_Step(const uint8_t* owned, int n, int currentIndex, int dir) {
    if (owned == NULL || n <= 0) {
        return -1;
    }
    int start = currentIndex % n;
    if (start < 0) {
        start += n;
    }
    for (int i = 1; i <= n; i++) {
        const int idx = dir < 0 ? (start - i + n * 2) % n : (start + i) % n;
        if (owned[idx]) {
            return idx;
        }
    }
    return -1;
}
