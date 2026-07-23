/**
 * @file combo_spoiler_view.h
 * @brief Read-only view model over the paired world's cross-game placements
 *        (#496, Lane C1 follow-up to #392).
 *
 * The paired-world spoiler already records every crossing, but only to a JSON
 * file on disk (`randomizer-mm/RSBSPAIR<masterSeed>.json`) — the operator has
 * to be told an absolute path to see their own seed. This is the in-game
 * *view*: "which MM check hosts which OoT item, and has it been collected".
 *
 * WHY THIS IS NOT THE GENERATOR. `Rando::Spoiler::GenerateFromSaveContext`
 * reads `gSaveContext` and `RANDO_SAVE_CHECKS` through MM's layout, so calling
 * it while OoT is active would read MM's layout over OoT's bytes. This model
 * reads `gComboCtx` only — game-neutral by construction (ADR 0002) — which is
 * what lets the view render safely under GAME_OOT, GAME_MM and GAME_NONE
 * alike. It touches no `gSaveContext`, no ImGui and no game headers.
 *
 * NOT PAIRED IS NOT THE SAME AS NO CROSSINGS. When
 * `Combo_ForeignPairingActive()` is false the model reports zero rows AND a
 * summary with `paired == false`. A caller that renders an empty list without
 * checking the summary would tell the player "this world has no crossings"
 * when the truth is "these two worlds were never paired".
 *
 * MM CHECK IDS ARE IDS, NOT NAMES. `ComboSpoilerRow.mmCheckId` is the raw
 * `RandoCheckId`. Common code has no MM check-name table and must not acquire
 * one by including an MM header; resolving ids to names needs the MM adapter
 * (#458) and is deliberately out of scope. Label them as ids.
 *
 * Locked ROM-free by the ComboSpoilerView CTest
 * (src/common/tests/test_combo_spoiler_view.c).
 */

#ifndef RSBS_COMMON_COMBO_SPOILER_VIEW_H
#define RSBS_COMMON_COMBO_SPOILER_VIEW_H

#include "foreign_items.h" // SharedItem, gComboCtx, the placement-table accessors

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One cross-game crossing, as the view renders it: MM check `mmCheckId` hosts
 * the foreign item `(originGame, itemId)`, displayed as `itemName`.
 */
typedef struct {
    uint16_t mmCheckId;    // MM RandoCheckId hosting the foreign item (never 0)
    uint8_t originGame;    // GameId owning itemId's id-space (GAME_OOT today)
    uint16_t itemId;       // RG_* when originGame == GAME_OOT
    const char* itemName;  // pinned-pool display name; never NULL (see below)
    bool redeemed;         // the origin game has already awarded this crossing
} ComboSpoilerRow;

/**
 * Fallback `ComboSpoilerRow.itemName` for a placement whose item is not in the
 * pinned pool. `Combo_GetForeignItemName` returns NULL there, and a view that
 * propagated the NULL would hand a printf-family "%s" an invalid pointer.
 * Reaching this string means the placement table and the pinned pool have
 * diverged — a bug worth seeing on screen rather than crashing on.
 */
#define RSBS_SPOILER_UNKNOWN_ITEM_NAME "Unknown Foreign Item"

/**
 * Header for the view: the pairing key and how many crossings exist.
 * `paired == false` means the worlds were never paired at all — the seed and
 * hash are then whatever `gComboCtx` holds (typically 0) and must not be shown
 * as a real pairing.
 */
typedef struct {
    bool paired;                      // Combo_ForeignPairingActive()
    uint32_t sharedRandoSeed;         // gComboCtx.sharedRandoSeed
    uint32_t sharedRandoSettingsHash; // gComboCtx.sharedRandoSettingsHash
    int placementCount;               // == Combo_SpoilerRowCount()
} ComboSpoilerSummary;

/**
 * Number of rows the view should render: the occupied placement slots, or 0
 * when the worlds are not paired.
 */
int Combo_SpoilerRowCount(void);

/**
 * Fill `out` with row `index` (0-based, in placement-SLOT order so the view is
 * stable across calls and matches the serialized table).
 * @return true on success; false for a NULL `out`, an out-of-range index, or
 *         an unpaired world (in which case `out` is untouched).
 */
bool Combo_SpoilerRowAt(int index, ComboSpoilerRow* out);

/** Fill `out` with the pairing header. NULL `out` is ignored. */
void Combo_SpoilerPairingSummary(ComboSpoilerSummary* out);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_SPOILER_VIEW_H
