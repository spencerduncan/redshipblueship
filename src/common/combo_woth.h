/**
 * @file combo_woth.h
 * @brief The pair-level WAY OF THE HERO (#1439; epic #575): which hosts of the
 *        paired world the frozen goal cannot do without, and the areas they sit in.
 *
 * ============================================================================
 * WHAT IT IS
 * ============================================================================
 *
 * OoTMM's largest default hint bucket is `woth` (9 hints in SETTINGS_DEFAULT_HINTS,
 * packages/core/src/settings/hints.ts): "They say that <region> is on the Way of
 * the Hero." Its set is analysis.ts's makeRequiredLocs: for each location of the
 * playthrough, one search with that location forbidden; the location is REQUIRED
 * when the goal falls. hints.ts's placePath then hints the region of a required
 * location (isLocationHintable 'path': never a key, a stray fairy, a silver rupee,
 * a token or a dungeon reward).
 *
 * Here the search is the coordinator's round (combo_logic.h) with its FORBIDDEN
 * HOSTS input (ABI 6, #575 H4), over the PAIRED world: a host is on the Way of the
 * Hero when the frozen GOAL falls without it, whichever game hosts it and whichever
 * game's item it holds. An OoT check holding the Hookshot Majora's Mask's goal
 * needs is on it; so is an MM check holding the Lens Ocarina of Time's goal needs.
 *
 * The candidates are the coordinator's own placements ("bag hosts"), the set #575
 * H4 measured (PR #918, ADR 0010's 2026-10-11 entry): every progression row the
 * single bag placed, in either game. A host each game's own restricted pass filled
 * (OoT's songs on song locations, its dungeon rewards and own-dungeon keys) is not
 * a candidate; that is a smaller set than OoTMM's, never a different answer for a
 * host it does test.
 *
 * ============================================================================
 * WHEN IT RUNS, AND WHAT IT COSTS
 * ============================================================================
 *
 * Once per creation, inside the single-bag fill's success path
 * (combo_single_bag.c), where both engines are live over the proved world: one
 * round with nothing forbidden (the baseline, which must prove the goal), then one
 * round per eligible candidate, in the two tables' order interleaved OoT, MM, OoT,
 * ... H4 measured those rounds at 30 to 45 ms each: on the shipped and plentiful
 * profiles every seed fit beside creation (worst 28.6 s against a 30 s budget then),
 * on the armed profile most did not. So the analysis is bounded by a ROUND count,
 * RSBS_COMBO_WOTH_ROUND_CAP, never a clock: a seed answers the same on every
 * machine (#950's rule). A candidate the cap leaves untested is NOT on the path, so
 * the cap can only drop hints, never claim one falsely. Its rounds are not charged
 * to the creation's fill budget (they decide no placement and cannot fail a
 * creation), and they change no placement (H4: the placement digest was the same
 * before and after on every seed).
 *
 * NO LOGIC CLAIMS NOTHING (Combo_ComboLogicClaimsAllowed): under the `none` rung no
 * round runs and the record stays ABSENT, as OoTMM's analysis skips its required
 * set under logic 'none'.
 *
 * ============================================================================
 * WHO READS IT
 * ============================================================================
 *
 *  - OoT's paired remainder (HintAreaOoT.cpp), while it hints the world it was
 *    created with: every OoT host on the path becomes a native WotH candidate, and
 *    the MM areas on the path join the WotH entry's draw (named through the
 *    foreign-area plumbing of #575 H3a). OoT's hints persist in its own hint data.
 *  - Majora's Mask's free gossip stones, at read time, from the frozen AREA record
 *    (gComboCtx.comboWoth, context.h ComboWothRecord), which the creation event
 *    writes and the .redsave and the seed package carry.
 *
 * ADR 0002: this file includes no game header. Hosts are {hostGame, hostCheck} as
 * the coordinator keys them; areas are the shared vocabulary's, through each game's
 * area adapter.
 */

#ifndef RSBS_COMMON_COMBO_WOTH_H
#define RSBS_COMMON_COMBO_WOTH_H

#include "combo_area.h"
#include "combo_logic.h"
#include "context.h" // ComboWothRecord

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The analysis's round bound, baseline included (see the file header). H4's
 *  shipped and plentiful worlds hold 257 and 362 bag hosts, so it binds only on
 *  armed-sized worlds (549). */
#define RSBS_COMBO_WOTH_ROUND_CAP 400

/** Required hosts the analysis can hold: every placement of both tables. */
#define RSBS_COMBO_WOTH_HOST_CAP (2 * RSBS_COMBO_LOGIC_PLACEMENT_CAP)

/* Combo_Woth_Analyze statuses. */
#define RSBS_COMBO_WOTH_OK 0
#define RSBS_COMBO_WOTH_NO_PROOF 1     /* the baseline round did not prove the goal: nothing is claimed */
#define RSBS_COMBO_WOTH_NO_CLAIMS 2    /* the frozen rung proves nothing (No Logic): no round runs */
#define RSBS_COMBO_WOTH_ERR_ROUND (-1) /* a round failed: the set is left empty */

/** Which placements are candidates. NULL = every placement. */
typedef bool (*ComboWothEligibleFn)(uint8_t hostGame, const ComboLogicPlacement* placement);

/** Called before each candidate round, so a caller can keep a progress display
 *  alive. Optional. */
typedef void (*ComboWothProgressFn)(void* ctx, int tested, int candidates);

typedef struct {
    int status;     /* RSBS_COMBO_WOTH_* */
    int placements; /* placements in both tables */
    int candidates; /* those `eligible` accepted */
    int tested;     /* candidates a round ran for */
    int required;   /* tested candidates the goal could not do without */
    int rounds;     /* rounds run, the baseline included */
    bool capped;    /* the round cap left candidates untested */
} ComboWothReport;

/**
 * THE ANALYSIS: the baseline round, then one round per eligible placement with
 * that host forbidden, up to `roundCap` rounds in all. Replaces the resident set.
 * Reads the coordinator's tables and runs rounds; writes nothing else.
 * @return the status (also in `out->status` when `out` is non-NULL).
 */
int Combo_Woth_Analyze(uint8_t goal, uint16_t triforceRequired, int roundCap, ComboWothEligibleFn eligible,
                       ComboWothProgressFn progress, void* progressCtx, ComboWothReport* out);

/**
 * The production candidate rule (OoTMM isLocationHintable 'path', in the shared
 * vocabulary): the host names an area a hint can say (not COMBO_AREA_NONE or
 * COMBO_AREA_POCKET), and its item is not a key, a token or a stray fairy (the
 * item's chest grade, shared_items.h, which each game publishes for its own ids).
 */
bool Combo_Woth_DefaultEligible(uint8_t hostGame, const ComboLogicPlacement* placement);

/** Forget the resident set (the next fill's analysis replaces it). */
void Combo_Woth_Clear(void);

/** Required hosts resident, in the order the analysis found them. */
int Combo_Woth_Count(void);

/** The `index`-th required host. */
bool Combo_Woth_At(int index, ComboLogicHostRef* out);

/** Whether host `hostCheck` of `hostGame` is in the resident set. */
bool Combo_Woth_IsRequired(uint8_t hostGame, uint16_t hostCheck);

/** The last analysis's report (zeroed by Combo_Woth_Clear). */
const ComboWothReport* Combo_Woth_LastReport(void);

/* ---- The area record (context.h ComboWothRecord) ------------------------- */

/** All eleven bytes zero: ABSENT. */
void Combo_WothRecord_Clear(ComboWothRecord* rec);

/** Set the PRESENT tag (an analysis ran, even if it found nothing). */
void Combo_WothRecord_MarkPresent(ComboWothRecord* rec);

bool Combo_WothRecord_Present(const ComboWothRecord* rec);

/** Add area `a`; false (and nothing changed) for a special, an area outside its
 *  half, or a non-game. */
bool Combo_WothRecord_Add(ComboWothRecord* rec, ComboArea a);

/** Whether `a` is on the path (false for an absent record, a special or a non-game). */
bool Combo_WothRecord_Has(const ComboWothRecord* rec, ComboArea a);

/** How many of `game`'s areas are on the path (0 for an absent record). */
int Combo_WothRecord_Count(const ComboWothRecord* rec, uint8_t game);

/** `game`'s areas on the path, ascending, into out[0..cap); returns how many
 *  there are (which may exceed `cap`). */
int Combo_WothRecord_Areas(const ComboWothRecord* rec, uint8_t game, uint8_t* out, int cap);

/** Where a host is, for Combo_Woth_BuildRecord. NULL = Combo_Area_OfCheck. */
typedef ComboArea (*ComboWothAreaFn)(uint8_t hostGame, uint16_t hostCheck);

/**
 * The record of the resident set: PRESENT, with the area of every required host
 * (`areaOf`, NULL = each game's area adapter). A host with no area adds none.
 * @return the number of areas on the path.
 */
int Combo_Woth_BuildRecord(ComboWothRecord* out, ComboWothAreaFn areaOf);

/**
 * THE CREATION'S CALL (combo_single_bag.c, after a fill succeeded): under a rung
 * that claims anything, run the analysis with the production candidate rule and
 * the production cap, and write the record into gComboCtx.comboWoth; otherwise
 * clear both. Logs one line.
 * @return the analysis status, or RSBS_COMBO_WOTH_NO_CLAIMS.
 */
int Combo_Woth_RunAtCreation(uint8_t goal, uint16_t triforceRequired, ComboWothProgressFn progress, void* progressCtx);

/** The world's frozen record (gComboCtx.comboWoth), for the stones that read at
 *  play time. Never NULL. */
const ComboWothRecord* Combo_Woth_FrozenRecord(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_WOTH_H
