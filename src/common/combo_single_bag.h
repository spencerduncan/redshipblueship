/**
 * @file combo_single_bag.h
 * @brief The single-bag fill AT THE CREATION EVENT: the production wiring of the
 *        combo-logic coordinator (ADR 0010 increment 3, D3/D5; #645, lane K11).
 *
 * ============================================================================
 * WHAT THIS IS
 * ============================================================================
 *
 * combo_logic.h is the coordinator and its contract; it composes a bag and runs
 * a fill over whatever engines are registered, and it knows nothing about WHEN a
 * world is made. This file is its one production caller: the seam ADR 0010's
 * increment 3 names ("one fill, at the creation event, draws from the union bag
 * and places across both games' shuffled check sets, and its exit condition is
 * the GOAL expression provable under the frozen rung and trick set"). It is
 * deliberately thin: the four things it decides are stated below, and everything
 * else is the coordinator's or an engine's.
 *
 * ============================================================================
 * WHERE IT RUNS, AND WHY THERE
 * ============================================================================
 *
 * The two engines need two different worlds live at once, and exactly one place
 * in the program has both:
 *
 *   - OoT's engine works on `Rando::Context`, which holds OoT's world from the
 *     Generate button (Playthrough_Init -> Fill()) until the file is created.
 *     For a paired world Fill() now STOPS after its restricted passes (shops,
 *     rewards, own-dungeon items, restricted songs, restricted dungeon items,
 *     Link's pocket) and leaves the general pass's hosts empty and its items in
 *     `itemPool` (fill.cpp's RSBS_SINGLE_EXECUTABLE seam). That is audit §4.6's
 *     reading of P14: the union bag is OoT's LAST GENERAL PASS.
 *   - MM's engine works on the LIVE gSaveContext in its file-creation state,
 *     which exists only inside the creation event's snapshot bracket
 *     (OoT_RunPairedCreationEvent -> MM_Rando_GenerateAtCreation ->
 *     OnFileCreate), after GeneratePools and GrantStartingItems.
 *
 * So the fill runs from MM's OnFileCreate paired branch, in place of MM's own
 * fill and the forward crossing pass, under MM's attempt ladder; OoT's leftover
 * hosts are filled back at the OoT seam, after the bracket, by
 * OoT_ComboLogic_FinishGeneralPass. The Generate button's own flow and the game
 * thread's ownership of gSaveContext are both unchanged.
 *
 * ============================================================================
 * WHAT IT DECIDES (the four rules the coordinator leaves to its caller)
 * ============================================================================
 *
 *  1. THE BAG. Combo_Logic_ComposeBag over OoT's general-pass pool
 *     (OoT_ComboLogic_ExportPool source 0) and MM's GeneratePools pool, with
 *     RSBS_COMBO_COMPOSE_ADMIT_CONFINED_HOME: a CONFINED row has no other pass to
 *     fall to under one bag, so it enters as a HOME_ONLY row. Filler, renewables
 *     and traps never enter; each game's own pass places them on its own
 *     leftover hosts, which is how "a trap never crosses" (a caller convention,
 *     combo_logic.h) is kept.
 *     THE SHARED-QUANTITY TRIM (combo_logic.h; lane K13, PR #744) runs inside the
 *     composer: this caller publishes both games' FROZEN starting healths
 *     (OoT_ComboLogic_StartingHealth / MM_ComboLogic_StartingHealth, from the
 *     settings the identity froze) and a trim seed derived from the identity
 *     (Combo_SingleBag_TrimSeed), never KEEP_ALL. A TRIMMED row is not its item
 *     any more: it goes back to its ORIGIN game's own pass as ONE FILLER COPY
 *     (OoT: OoT_ComboLogic_NoteTrimmedRows, one GetJunkItem() each; MM: the
 *     RSBS_SINGLE_BAG_MM_ROW_TRIMMED mark, one RI_JUNK each), dealt after that
 *     game's traps. Handing it back as the item would put the dead heart pickups
 *     the trim removed straight back into the world through the per-game pass.
 *  2. WHICH ORIGINS MAY CROSS. The frozen DIRECTION must arm the origin
 *     (Combo_ComboDirectionArms), and the frozen ITEM-CLASS bitset for that origin
 *     must hold RSBS_ITEMCLASS_PROGRESSION: the one class either pinned pool ever
 *     populated, and the class every bag row is by construction. A row of an
 *     origin that may not cross is HOME_ONLY. Under RSBS_COMBO_DIR_OFF the world
 *     is still ONE fill with ONE proof; nothing crosses.
 *  3. THE SEED. A pure function of the frozen identity (master seed, OoT settings
 *     hash, MM profile digest, combo fingerprint) and the ladder attempt
 *     (Combo_SingleBag_SeedFor). No game RNG. The TRIM seed is the same identity
 *     under its own domain tag and WITHOUT the ladder attempt: which copies of a
 *     shared family survive is a fact of the world's identity, so a ladder retry
 *     re-draws the placements over the same bag.
 *  4. THE BUDGET. The per-attempt wall-clock budget MM's ladder hands its fill
 *     (#582) is checked between rounds by the fill's observer; exceeding it is
 *     RSBS_COMBO_LOGIC_ERR_ABORTED, which the ladder treats as a wall-clock STOP
 *     and never as a rung (#581 §2a). Presentation time is credited back, exactly
 *     as the MM fill's own stop credits it.
 */

#ifndef RSBS_COMMON_COMBO_SINGLE_BAG_H
#define RSBS_COMMON_COMBO_SINGLE_BAG_H

#include "combo_logic.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** `Combo_SingleBag_Run`'s `outMmInBag` values, one per MM pool row. */
#define RSBS_SINGLE_BAG_MM_ROW_OWN_PASS 0 /**< left for MM's own pass, as itself */
#define RSBS_SINGLE_BAG_MM_ROW_IN_BAG 1   /**< a bag row: the coordinator placed (or dropped) it */
#define RSBS_SINGLE_BAG_MM_ROW_TRIMMED 2  /**< trimmed: MM's own pass deals one junk copy instead */

/** What the last single-bag fill did. Diagnostics, not format. */
typedef struct {
    int status;        // RSBS_COMBO_LOGIC_*
    int ladderAttempt; // 0-based MM ladder attempt this fill ran under
    uint32_t seed;     // the coordinator seed (Combo_SingleBag_SeedFor)
    uint8_t goal;      // the frozen GOAL
    uint8_t rung;      // the frozen logic rung
    uint32_t budgetMs; // the per-attempt wall-clock budget; 0 = none
    uint32_t wallMs;   // this fill's wall time, bag build included
    int ootRows;       // OoT general-pass pool rows exported
    int mmRows;        // MM pool rows handed in
    ComboLogicComposeResult compose;
    uint32_t trimSeed;          // THE SHARED-QUANTITY TRIM's seed (Combo_SingleBag_TrimSeed)
    uint16_t startingHealthOoT; // the frozen starting healths the trim was handed (0x10 per heart)
    uint16_t startingHealthMM;
    int trimmedOoT; // OoT pool rows the trim removed: each one OoT filler copy
    int trimmedMM;  // ... and MM's
    int bagCount;
    int homeOnlyRows; // bag rows kept in their own game (direction / class / confinement)
    ComboLogicFillResult fill;
    int crossingsIntoMM;  // OoT-origin rows placed on MM hosts
    int crossingsIntoOoT; // MM-origin rows placed on OoT hosts
} ComboSingleBagReport;

/**
 * THE FILL. Compose the bag from OoT's general-pass pool and `mmItems`, gate each
 * origin's crossings on the frozen direction and class bitset, and run the
 * coordinator under the frozen GOAL and rung.
 *
 * PRECONDITIONS (the caller's, per combo_logic.h's `beginQuery` contract): OoT's
 * world is at its general-pass point (OoT_ComboLogic_GeneralPassDeferred() != 0),
 * MM's live save is in its file-creation state with GeneratePools' check pool
 * handed to MM's engine (MM_ComboLogic_SetHostPool), and the pairing identity and
 * the combo record are frozen.
 *
 * @param mmItems     MM's pool, one row per copy, in GeneratePools' order
 * @param mmFlags     each row's RSBS_COMBO_POOL_* flags (MM_ComboLogic_MarkPoolRows)
 * @param budgetMs    the per-attempt wall-clock budget; 0 = none
 * @param outMmInBag  optional, `mmCount` bytes, one RSBS_SINGLE_BAG_MM_ROW_* per
 *                    MM row: IN_BAG where it entered the bag (the coordinator
 *                    placed or dropped it), TRIMMED where the shared-quantity trim
 *                    removed it (MM's own pass deals ONE JUNK COPY for it, never
 *                    the item), OWN_PASS where it is left for MM's own pass as
 *                    itself. Written only on success.
 * @return RSBS_COMBO_LOGIC_OK, or the refusal / failure status. ON ANY FAILURE
 *         both engines and both tables are rolled back to the state they held on
 *         entry (Combo_Logic_ResetPlacements), so a ladder retry starts clean.
 *         ON SUCCESS OoT's engine is told which of its pool rows the bag took and
 *         which the trim removed, so OoT_ComboLogic_FinishGeneralPass fills OoT's
 *         leftovers from the rest, with one junk copy per trimmed row.
 */
int Combo_SingleBag_Run(const uint16_t* mmItems, const uint16_t* mmFlags, int mmCount, int ladderAttempt,
                        uint32_t budgetMs, uint8_t* outMmInBag, ComboSingleBagReport* out);

/** The most recent Combo_SingleBag_Run's report. Never NULL (zeroed before any run). */
const ComboSingleBagReport* Combo_SingleBag_LastReport(void);

/** The coordinator seed for ladder attempt `ladderAttempt` under the frozen identity. */
uint32_t Combo_SingleBag_SeedFor(int ladderAttempt);

/** THE SHARED-QUANTITY TRIM's seed (ComboLogicComposeRequest.trimSeed) under the
 *  frozen identity: its own domain tag, no ladder attempt (see rule 3 above). */
uint32_t Combo_SingleBag_TrimSeed(void);

/**
 * COMMIT: the world is decided, so drop every record of HOW it was decided without
 * undoing it: the coordinator's tables (Combo_Logic_HydrateTables with no rows,
 * which calls no engine) and both engines' own placement records. Those records
 * exist to ROLL BACK, and a later fill's reset would otherwise roll this finished
 * world back: OoT would write RG_NONE into its hosts, MM would write the priors
 * into whatever save is live by then. Call AFTER the crossing store has captured
 * the crossings. Idempotent.
 */
void Combo_SingleBag_Forget(void);

/**
 * Is `status` (a Combo_SingleBag_Run return) a DETERMINISTIC WORLD DEAD END, the
 * only kind of failure the attempt ladder may climb a rung on (PR #743 review)?
 * True for RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE, _GOAL_UNPROVABLE and
 * _NOT_ALL_REACHED: facts about the world this attempt's seed drew, which another
 * seed can change. False for OK, for ERR_ABORTED (a wall-clock stop, never a rung,
 * #581 section 2a) and for every refusal and engine defect (BAD_REQUEST,
 * NO_ENGINE, UNSUPPORTED_GOAL, NON_MONOTONE, NO_FIXPOINT, CAPACITY,
 * ENGINE_REFUSED): none of those depends on the seed, so re-seeding only repeats
 * it and then misreports the failure as an exhausted ladder.
 */
bool Combo_SingleBag_StatusIsWorldDeadEnd(int status);

// ----------------------------------------------------------------------------
// The two ports' halves (implemented in each game's engine TU; declared here so
// src/common names no game header, ADR 0002).
// ----------------------------------------------------------------------------

/** OoT (ComboLogicEngineOoT.cpp): its pool rows; see the definition. */
int OoT_ComboLogic_ExportPool(int source, uint16_t* outItems, uint16_t* outHosts, uint16_t* outFlags, int cap);
/** OoT: the confinement bits its frozen settings arm. */
uint32_t OoT_ComboLogic_ConfinementArmed(void);
/** OoT: nonzero while OoT's world waits at its general-pass point for this fill. */
int OoT_ComboLogic_GeneralPassDeferred(void);
/** OoT: the export rows (source 0, export order) the bag took. */
void OoT_ComboLogic_NoteBagRows(const int* exportRows, int count);
/** OoT: the export rows (source 0, export order) THE SHARED-QUANTITY TRIM removed;
 *  OoT's remainder places one junk copy for each instead of the item. */
void OoT_ComboLogic_NoteTrimmedRows(const int* exportRows, int count);
/** Each game's FROZEN starting health in health units (0x10 per heart), for the
 *  trim's health budget (lane K13's exports; 0 = not published). */
uint16_t OoT_ComboLogic_StartingHealth(void);
uint16_t MM_ComboLogic_StartingHealth(void);
/** OoT: drop the engine's placement record without restoring (see Forget). */
void OoT_ComboLogic_ForgetPlacements(void);
/** MM (ComboLogicEngineSingleExe.cpp): the same for MM's engine, and its host pool. */
void MM_ComboLogic_ForgetPlacements(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_SINGLE_BAG_H
