/**
 * @file foreign_items.h
 * @brief Cross-game (foreign) item placements and the pinned foreign-item pool
 *        (Phase 3.0 Lane C1, #392; ADR 0002).
 *
 * The MVP contract ships ONE direction, ONE item class: a pinned set of OoT
 * progression items placeable into MM checks. Two data surfaces live behind
 * this header:
 *
 *  1. The PLACEMENT TABLE — `gComboCtx.foreignPlacements[]` (context.h), the
 *     serialized record of "MM check X hosts foreign item Y". Written once by
 *     MM's generation pass (Rando::Foreign::PlaceForeignItems at OnFileCreate)
 *     when a paired world generates; read by MM's give path and both spoiler
 *     surfaces. The MM save's own check table keeps a legal MM item (RI_JUNK)
 *     at hosting checks — a raw RG_* never enters an MM table (ADR 0002).
 *
 *  2. The PINNED POOL — the fixed foreign-item class. Its table is DEFINED on
 *     the OoT side (games/oot/soh/Enhancements/randomizer/
 *     ForeignItemsSingleExe.cpp), the only place the real RG_* enumerators are
 *     in scope, and served here as origin-tagged SharedItems plus stable
 *     display names. MM and the tests consume it through these C entry points
 *     and never see an OoT header.
 *
 * The give-time flow (who calls what):
 *   MM CheckQueue foreign branch -> Combo_GetForeignPlacementForCheck ->
 *   Combo_RecordSharedItem (shared_items.h; durable immediately) -> presented
 *   with Combo_GetForeignItemName. On the next arrival in OoT, A1's consumer
 *   (Combo_RedeemSharedItemsForGame) awards it via OoT_ForeignItem_Give and
 *   marks it RSBS_SHARED_ITEM_REDEEMED — single-use per crossing.
 */

#ifndef RSBS_COMMON_FOREIGN_ITEMS_H
#define RSBS_COMMON_FOREIGN_ITEMS_H

#include "context.h" // SharedItem, ComboForeignPlacement, gComboCtx, RSBS_FOREIGN_PLACEMENT_CAP

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One pinned foreign-item class member: the origin-tagged item plus its
 * stable, human-readable name (used by the MM textbox presentation and by
 * both spoiler surfaces — the name is deliberately game-neutral text so the
 * spoiler stays meaningful without OoT's enum in scope).
 */
typedef struct {
    SharedItem item;  // originGame == GAME_OOT, flags == 0, id == the RG_* value
    const char* name; // e.g. "Progressive Hookshot"
} ComboForeignItemDef;

/**
 * The pinned foreign-item pool (defined OoT-side, see the file header).
 * @param outPool receives a pointer to the static pool table (never NULL on a
 *                nonzero return)
 * @return the number of pool entries
 */
int Combo_GetForeignItemPool(const ComboForeignItemDef** outPool);

/**
 * Display/spoiler name for a foreign item, or NULL if (originGame, id) is not
 * in the pinned pool. Flags are ignored on purpose: a redeemed entry keeps its
 * name.
 */
const char* Combo_GetForeignItemName(SharedItem item);

/**
 * The inverse of Combo_GetForeignItemName: resolve a pinned-pool display name
 * back to its origin-tagged SharedItem. Used by the spoiler-LOAD path
 * (Rando::Spoiler::ReconstructForeignPlacements) to rebuild
 * gComboCtx.foreignPlacements from the spoiler's "foreign" section without an
 * OoT header in scope — the SharedItem is copied straight out of the pinned
 * pool, so a raw RG_* is never reconstructed on the MM side (ADR 0002).
 * @param name    the display name to look up (NULL is rejected)
 * @param outItem receives the tagged SharedItem on a match (may be NULL)
 * @return true if a pool entry has this name, false otherwise.
 */
bool Combo_GetForeignItemByName(const char* name, SharedItem* outItem);

// ============================================================================
// Placement table accessors (gComboCtx.foreignPlacements)
// ============================================================================

/**
 * True when the paired-world keying is satisfied: an OoT rando world was
 * generated in the live path (gComboCtx.sourceIsRando) AND it recorded its
 * settings profile digest (sharedRandoSettingsHash != 0 — Lane B's carrier
 * contract: 0 means "no profile recorded" and the worlds must not pair).
 */
bool Combo_ForeignPairingActive(void);

/**
 * Record "MM check `mmCheckId` hosts `item`". Rejects (returning -1) an unset
 * item tag, an mmCheckId of 0 (MM's RC_UNKNOWN), a full table, or a duplicate
 * mmCheckId — one check hosts at most one foreign item.
 * @return the slot index used (>= 0), or -1.
 */
int Combo_SetForeignPlacement(uint16_t mmCheckId, SharedItem item);

/**
 * The foreign item hosted by MM check `mmCheckId`, or NULL if that check hosts
 * none. The returned pointer aliases gComboCtx (read-only use).
 */
const SharedItem* Combo_GetForeignPlacementForCheck(uint16_t mmCheckId);

/** Number of occupied placement slots. */
int Combo_CountForeignPlacements(void);

/**
 * Wipe the placement table. Called by MM's placement pass before re-placing
 * (a re-generated MM world must not inherit a previous world's placements).
 */
void Combo_ClearForeignPlacements(void);

/**
 * OoT-side redemption give (defined in ForeignItemsSingleExe.cpp): resolve the
 * RG_* id to its GetItemEntry — progressive items resolve against the LIVE
 * save, the same path SoH's own in-game gives take — and hand it to the
 * matching give routine (OoT_Item_Give for vanilla-equivalent entries,
 * Randomizer_Item_Give for MOD_RANDOMIZER entries).
 * @return 1 if the item was given, 0 if the give path was unavailable (logged;
 *         the redemption bit is set by the caller's consumer either way).
 */
int OoT_ForeignItem_Give(uint16_t rgId);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_FOREIGN_ITEMS_H
