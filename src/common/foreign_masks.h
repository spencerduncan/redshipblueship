/**
 * @file foreign_masks.h
 * @brief Wearing the OTHER game's masks (#977, epic #975 phase P3): the id space
 *        both games persist, the worn model and pause icon each host draws from
 *        its own curated half, and the host-independent equip rules.
 *
 * WHAT THIS PHASE IS. Groundwork, with no item, option or logic edge. OoTMM lets
 * OoT wear three MM masks (Blast, Stone, Kamaro) and MM wear three OoT masks
 * (Gerudo, Skull, Spooky); P4 (#979) and P5 (#980) add those items. This header
 * is what both of them wear and draw through:
 *
 *  - ONE WORN-MASK FIELD PER GAME, never `currentMask` (the epic's design rule
 *    4). Both engines index unbounded tables by `currentMask` (OoT: sMaskDlists,
 *    the face reactions and the auto-unequip arithmetic; MM: sMaskObjectIds,
 *    sMaskItemIds, D_801C0B20), so a foreign id there would read past them. The
 *    field holds a ComboForeignMask: OoT's is `gSaveContext.ship.foreignMask`,
 *    MM's is `gSaveContext.save.shipSaveInfo.rando.foreignMask`. Both live in
 *    the game's SaveContext, so they cross with the frozen half and persist in
 *    the .redsave's Tier 2 / Tier 3 like every other save byte.
 *  - THE MODELS ARE CURATED, never the other game's base-archive path (#981:
 *    a path both base archives carry is served from whichever game cached it
 *    first). Each worn display list and icon below is carried by its HOST's
 *    half (assets/crossgame/manifest.txt): OoT draws MM's lists from
 *    redship-oot.o2r, MM draws OoT's from redship-mm.o2r. The generator refuses
 *    a curated path any base archive the host mounts also carries, so these
 *    paths resolve to exactly one copy. The ForeignMaskTable row locks that
 *    every path here is curated in its host's direction.
 *  - THE HOST'S RULES. A mask added to the other game is a native item of the
 *    host (design rule 3), so each host applies its own mask rules to it:
 *    OoT's child-only masks come off at a scene change unless SoH's Persistent
 *    Masks is on, and on an age change; MM's work in human form only and come
 *    off when Link transforms. Putting a native mask on takes a foreign one off
 *    and the reverse, in both games.
 *
 * OWNERSHIP is not this phase's. The phase that adds a mask's item registers
 * the host's ownership source (Combo_ForeignMask_SetOwnedSource); until then
 * nothing is owned, so the pause surfaces and the trade cycle are unchanged.
 *
 * Plain C, no game header (ADR 0002). Every value below is persisted: the ids
 * and the slot numbers are APPEND-ONLY.
 */

#ifndef RSBS_COMMON_FOREIGN_MASKS_H
#define RSBS_COMMON_FOREIGN_MASKS_H

#include <stdint.h>

#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The worn-mask field's values. Persisted in both games' saves: APPEND-ONLY. */
typedef enum ComboForeignMask {
    COMBO_FOREIGN_MASK_NONE = 0,
    /* Majora's Mask masks, worn in Ocarina of Time (P4, #979). */
    COMBO_FOREIGN_MASK_MM_BLAST = 1,
    COMBO_FOREIGN_MASK_MM_STONE = 2,
    COMBO_FOREIGN_MASK_MM_KAMARO = 3,
    /* Ocarina of Time masks, worn in Majora's Mask (P5, #980). */
    COMBO_FOREIGN_MASK_OOT_GERUDO = 4,
    COMBO_FOREIGN_MASK_OOT_SKULL = 5,
    COMBO_FOREIGN_MASK_OOT_SPOOKY = 6,
    COMBO_FOREIGN_MASK_COUNT = 7,
} ComboForeignMask;

/**
 * The MM pause slot an OoT mask shares (OoTMM: Stone/Gerudo/Skull share one,
 * Gibdo/Spooky another). MM keeps the slot's own item in the inventory, so
 * every table MM indexes by item stays in range; the slot's selected variant is
 * MM's `rando.foreignMaskSlotSel[slot - 1]`. Persisted: APPEND-ONLY.
 */
typedef enum ComboForeignMaskSlot {
    COMBO_FOREIGN_MASK_SLOT_NONE = 0,
    COMBO_FOREIGN_MASK_SLOT_MM_STONE = 1,
    COMBO_FOREIGN_MASK_SLOT_MM_GIBDO = 2,
    COMBO_FOREIGN_MASK_SLOT_COUNT = 3,
} ComboForeignMaskSlot;

/** A shared slot's variant list holds at most the native item plus two masks. */
#define COMBO_FOREIGN_MASK_SLOT_MAX_VARIANTS 3

typedef struct ComboForeignMaskInfo {
    /** ComboForeignMask. */
    uint8_t id;
    /** GameId the mask comes from. */
    uint8_t origin;
    /** GameId that wears and draws it: always the other game. */
    uint8_t host;
    /** ComboForeignMaskSlot: the MM pause slot it shares (MM host only). */
    uint8_t mmSharedSlot;
    /** 1: the worn list calls segment 0x09 (MM's Blast Mask glow pass). The host
     *  binds it to a no-glow list before drawing, as MM's own Player_DrawBlastMask
     *  does while the cooldown timer is 0. */
    uint8_t bindsSegment9;
    /** The mask's English name. */
    const char* name;
    /** The worn display list, "__OTR__<path>", in the HOST's curated half. */
    const char* wornDl;
    /** The pause icon, "__OTR__<path>", in the HOST's curated half. */
    const char* icon;
} ComboForeignMaskInfo;

/** The row for a mask, or NULL for NONE and for any value out of range. */
const ComboForeignMaskInfo* Combo_ForeignMask_Info(uint8_t id);

/** 1 when `id` is a mask `host` wears, else 0 (NONE, out of range, or the
 *  other game's). */
int Combo_ForeignMask_IsWornIn(uint8_t id, GameId host);

/**
 * The value a host may act on: `value` when it is a mask `host` wears, else
 * NONE. Every reader of a worn-mask field goes through this, so a corrupt or
 * foreign byte can never select a table row.
 */
uint8_t Combo_ForeignMask_Sanitize(uint8_t value, GameId host);

/**
 * The equip rule: what the worn field becomes when the player uses `requested`.
 * The worn mask again takes it off (NONE); another mask the host wears is put
 * on; anything else (NONE, out of range, the other game's mask) leaves `worn`
 * as it was. The host clears its native mask itself when this returns a mask.
 */
uint8_t Combo_ForeignMask_Toggle(uint8_t worn, uint8_t requested, GameId host);

/** Ownership of a host's foreign masks: 1 when owned. Pure of the caller. */
typedef int (*ComboForeignMaskOwnedFn)(uint8_t id);

/** Register (or with NULL clear) a host's ownership source. The phase that adds
 *  the item registers it (P4 for OoT, P5 for MM). */
void Combo_ForeignMask_SetOwnedSource(GameId host, ComboForeignMaskOwnedFn fn);

/** 1 when `host` owns mask `id` by its registered source; 0 with none. */
int Combo_ForeignMask_Owned(GameId host, uint8_t id);

/**
 * An MM shared slot's variants, in cycle order: the native item first (written
 * as NONE) when `nativeOwned`, then each owned OoT mask sharing the slot, in id
 * order. Returns the count (0 for an unknown slot or nothing owned).
 */
int Combo_ForeignMask_SlotVariants(uint8_t slot, int nativeOwned, uint8_t* out, int cap);

/**
 * The variant after (dir > 0) or before (dir < 0) `current` in the slot's
 * variant list; `current` when it is the only one. A `current` the list does
 * not hold (no longer owned) yields the list's first entry. NONE means the
 * native item.
 */
uint8_t Combo_ForeignMask_SlotCycle(uint8_t slot, uint8_t current, int nativeOwned, int dir);

/**
 * One step of a list-based item cycle (OoT's child trade slot, #977): the index
 * of the next (dir > 0) or previous (dir < 0) entry after `currentIndex` whose
 * `owned` flag is set, wrapping, or -1 when none is. `currentIndex` may be any
 * value: it is taken modulo `n` first, which is what OoT's original contiguous
 * cycle did with a slot value outside its range.
 */
int Combo_ListCycle_Step(const uint8_t* owned, int n, int currentIndex, int dir);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_FOREIGN_MASKS_H
