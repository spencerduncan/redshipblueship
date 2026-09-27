/**
 * @file combo_spoiler_view.h
 * @brief Read-only view model over the paired world's cross-game crossings, in
 *        both directions (#496; #755, #757).
 *
 * The paired-world spoiler already records every crossing, but only to a JSON
 * file on disk (the one spoiler's combo.crossingStore section) — the operator
 * has to be told an absolute path to see their own seed. This is the in-game
 * *view*: "which check of one game hosts which item of the other, and has that
 * check been collected".
 *
 * THE SAME ROWS AS THE COMBO TRACKER. Since the single bag (ADR 0010 increment
 * 3) the crossing store is the only record of the crossings, and the combo
 * tracker's crossing rows (combo_tracker_view.h, "Cross-game crossings") already
 * read it together with the legacy pinned tables, name both sides, and derive
 * each row's found state from the host game's own save. This view is those rows,
 * not a second reading of them: a spoiler and a tracker that could disagree
 * about which crossings a world has would each be a bug the other hides. Before
 * #755 this view walked the forward pinned table alone, which the single bag
 * never writes, so it was empty on every single-bag world; and it printed MM
 * checks as hex ids (#757).
 *
 * WHY THIS IS NOT THE GENERATOR. `Rando::Spoiler::GenerateFromSaveContext`
 * reads `gSaveContext` and `RANDO_SAVE_CHECKS` through MM's layout, so calling
 * it while OoT is active would read MM's layout over OoT's bytes. This model
 * reads gComboCtx, the crossing store and the tracker adapters only — game-
 * neutral by construction (ADR 0002) — which is what lets the view render
 * safely under GAME_OOT, GAME_MM and GAME_NONE alike. It touches no
 * `gSaveContext`, no ImGui and no game headers.
 *
 * NOT PAIRED IS NOT THE SAME AS NO CROSSINGS. When
 * `Combo_ForeignPairingActive()` is false the model reports zero rows AND a
 * summary with `paired == false`. A caller that renders an empty list without
 * checking the summary would tell the player "this world has no crossings"
 * when the truth is "these two worlds were never paired".
 *
 * Locked ROM-free by the ComboSpoilerView and ComboCrossingViews CTests
 * (src/common/tests/test_combo_spoiler_view.c, test_combo_crossing_views.c),
 * and over a real single-bag world against the one spoiler's combo section by
 * ComboCrossingViewsWorld.
 */

#ifndef RSBS_COMMON_COMBO_SPOILER_VIEW_H
#define RSBS_COMMON_COMBO_SPOILER_VIEW_H

#include "combo_tracker_view.h" // ComboTrackerForeignRow, the crossing rows
#include "foreign_items.h"      // SharedItem, gComboCtx

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One cross-game crossing, as the view renders it: check `hostCheckId` of
 * `hostGame` hosts the foreign item `(originGame, itemId)`, displayed as
 * `itemName`. The tracker's row type, field for field (see the file comment).
 */
typedef ComboTrackerForeignRow ComboSpoilerRow;

/**
 * Fallback `ComboSpoilerRow.itemName` for a crossing whose item its origin's
 * describer cannot name (an unregistered game, an unknown id). Reaching this
 * string means the world names an item its own game does not know — a bug
 * worth seeing on screen rather than crashing on. The tracker's placeholder,
 * because the rows are the tracker's.
 */
#define RSBS_SPOILER_UNKNOWN_ITEM_NAME RSBS_TRACKER_UNKNOWN_ITEM_NAME

/**
 * Header for the view: the pairing key and how many crossings each game hosts.
 * `paired == false` means the worlds were never paired at all — the seed and
 * hash are then whatever `gComboCtx` holds (typically 0) and must not be shown
 * as a real pairing.
 */
typedef struct {
    bool paired;                      // Combo_ForeignPairingActive()
    uint32_t sharedRandoSeed;         // gComboCtx.sharedRandoSeed
    uint32_t sharedRandoSettingsHash; // gComboCtx.sharedRandoSettingsHash
    int mmHosted;                     // == Combo_SpoilerRowCount(GAME_MM): OoT items in MM checks
    int ootHosted;                    // == Combo_SpoilerRowCount(GAME_OOT): MM items in OoT checks
} ComboSpoilerSummary;

/**
 * Number of rows the view renders for crossings hosted in `hostGame`, or 0 when
 * the worlds are not paired or `hostGame` is not a game.
 */
int Combo_SpoilerRowCount(uint8_t hostGame);

/**
 * Fill `out` with row `index` (0-based) of the crossings hosted in `hostGame`,
 * in the order combo.crossingStore prints them (legacy pinned rows first).
 * @return true on success; false for a NULL `out`, an out-of-range index, or
 *         an unpaired world (in which case `out` is untouched).
 */
bool Combo_SpoilerRowAt(uint8_t hostGame, int index, ComboSpoilerRow* out);

/** Fill `out` with the pairing header. NULL `out` is ignored. */
void Combo_SpoilerPairingSummary(ComboSpoilerSummary* out);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_SPOILER_VIEW_H
