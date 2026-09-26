/**
 * @file triforce_hunt.h
 * @brief The combo triforce hunt: ONE shared piece count across both worlds
 *        (ADR 0010 answer O10).
 *
 * ============================================================================
 * WHAT O10 DECIDED, AND WHAT THIS FILE IS
 * ============================================================================
 *
 * ADR 0010 D1 lets the combo GOAL be `triforce-hunt`, and answer O10 decided
 * its accounting: "ONE shared triforce piece count across both worlds, carried
 * shared-resource-style — not per-half composition. Both engines' existing piece
 * machinery (OoT `RSK_TRIFORCE_HUNT_PIECES_*`, MM `RO_TRIFORCE_PIECES_*`) is the
 * substrate feeding that one combo-level count."
 *
 * So there are four pieces, and this header owns the three that are
 * game-neutral (the fourth, the engine query, is `triforcePieces` on the
 * ComboLogicEngine vtable in combo_logic.h):
 *
 *   1. THE FROZEN RECORD. The requirement and the split of pieces between the
 *      two pools are decided ONCE, at creation, from each half's own settings
 *      (ComboTriforceRecord in context.h states the rule). After that each
 *      half's settings are inputs that were used, not a truth either game
 *      consults for the hunt.
 *   2. THE COUNT. RSBS_SHARED_RES_TRIFORCE_PIECES, a MONOTONIC shared resource
 *      (shared_resources.h): each game's own counter is a mirror of it. A
 *      collect in either game raises that game's counter; the suspend harvest
 *      max-merges it into the pool; the next arrival raises the other game's
 *      counter to the pool. Because every arrival materializes the WHOLE count
 *      before the next collect adds to it, max-merge is a sum across a switch:
 *      collect k in OoT and m in MM and both counters read k+m.
 *   3. THE WIN. Each port already ends a hunt on the give that reaches its
 *      requirement (OoT `Randomizer_Item_Give`'s RG_TRIFORCE_PIECE arm: the
 *      credits warp under "Win"; MM `Rando::GiveItem`'s RI_TRIFORCE_PIECE arm:
 *      Majora's soul, OnGameCompletion and the ending transition). Paired, the
 *      requirement those arms compare against is the COMBO requirement, so the
 *      win fires in whichever game the reaching collect happens in, through that
 *      game's own ending. Combo_TriforceHuntOnPieceGiven is the one decision
 *      both arms call.
 *
 * ARMING. Everything above is live only when the FROZEN combo goal is
 * triforce-hunt and a valid record is frozen beside it. Every other world —
 * the shipped default is beat-both — sees each port's own hunt exactly as
 * upstream wrote it, stores four zero bytes, grows no shared slot, and so moves
 * no golden.
 *
 * Game-header-free, like foreign_items.c and shared_resources.c. THREADING: game
 * thread only.
 */

#ifndef RSBS_COMMON_TRIFORCE_HUNT_H
#define RSBS_COMMON_TRIFORCE_HUNT_H

#include "context.h" // ComboTriforceRecord, ComboSettingsRecord, gComboCtx, GameId

#ifdef __cplusplus
extern "C" {
#endif

/** The largest combo total the pair can carry: OoT mirrors the count in its
 *  8-bit `triforcePiecesCollected`, so a larger total would wrap OoT's counter
 *  (the #726 class). MM's counter is 16-bit and is not the bound. */
#define RSBS_TRIFORCE_COMBO_MAX 255u

/** One half's own settings, as that half's resolver reads them. `total == 0`
 *  means that half's own hunt is off (and then `required` must be 0 too). RAM
 *  only; the stored form is ComboTriforceRecord. 16-bit because MM's own
 *  slider reaches 1000: the resolver must SEE an out-of-range half to refuse
 *  it, not receive it already truncated. */
typedef struct {
    uint16_t total;
    uint16_t required;
} ComboTriforceHalf;

// ---- Resolution status (diagnostics, not format) ---------------------------
#define RSBS_TRIFORCE_OK 0
/** Neither half's own hunt is on: a triforce-hunt world with no pieces in
 *  either pool can never be won. */
#define RSBS_TRIFORCE_ERR_NO_PIECES 1
/** A half is incoherent: its hunt is on with a zero requirement, it requires
 *  more than its own pool holds, or its hunt is off with a nonzero requirement. */
#define RSBS_TRIFORCE_ERR_BAD_HALF 2
/** The combo total exceeds RSBS_TRIFORCE_COMBO_MAX. */
#define RSBS_TRIFORCE_ERR_OVER_CAP 3
/** A NULL where a pointer is required. */
#define RSBS_TRIFORCE_ERR_BAD_REQUEST 4

/** Its name ("ok", "no-pieces", ...), or "(unknown)". Never NULL. */
const char* Combo_TriforceStatusName(int status);

/**
 * THE RULE (ADR 0010 answer O10): from each half's own settings, the frozen
 * record — each half's pieces stay in its own pool, the combo total is their
 * sum, the combo requirement is the sum of the two halves' requirements.
 *
 * @return RSBS_TRIFORCE_OK and `*out` written, or an error and `*out` zeroed.
 */
int Combo_TriforceResolve(const ComboTriforceHalf* oot, const ComboTriforceHalf* mm, ComboTriforceRecord* out);

/** Re-validate STORED bytes by the same rule Combo_TriforceResolve applies, so
 *  a record read back from a `.redsave` is held to what creation could write.
 *  An all-zero record is NO_PIECES (it describes no hunt). */
int Combo_TriforceRecordCheck(const ComboTriforceRecord* rec);

/** Occupancy: a record describes a hunt iff its total is nonzero. */
bool Combo_TriforceRecordPresent(const ComboTriforceRecord* rec);

/** The combo requirement / total a record describes (the sums). 0 for NULL. */
uint16_t Combo_TriforceRecordRequired(const ComboTriforceRecord* rec);
uint16_t Combo_TriforceRecordTotal(const ComboTriforceRecord* rec);

/**
 * THE CREATION FREEZE. Called by the creation event right after the combo
 * settings record is frozen (the goal it reads must already be decided):
 *
 *   - frozen goal is NOT triforce-hunt: the record is ZEROED (no hunt) and the
 *     call succeeds — this is every shipped-default world, and it stores
 *     exactly what a world from before this carve stored;
 *   - frozen goal IS triforce-hunt: the record is resolved from the two halves
 *     and stored; a resolution error stores nothing (zero) and is returned, and
 *     the creation must refuse — a triforce-hunt world the rule cannot describe
 *     is not a world to generate.
 *
 * The creation event is the ONLY writer. Like Combo_FreezeComboSettings it
 * overwrites: a second creation in one process authors a new world. Arrivals
 * and loads never write it; they compare (Combo_TriforceRecordDivergence,
 * Combo_TriforceHalfDiverges).
 */
int Combo_TriforceFreezeAtCreation(const ComboTriforceHalf* oot, const ComboTriforceHalf* mm);

/**
 * Is a stored triforce record consistent with the stored combo record beside it?
 * RSBS_COMBO_DIVERGE_TRIFORCE (foreign_items.h) when not, 0 when it is:
 *
 *   - goal triforce-hunt: the record must be present and pass
 *     Combo_TriforceRecordCheck;
 *   - any other goal, or no combo record at all (the legacy pair): the record
 *     must be all zero.
 *
 * Takes both records explicitly so the `.redsave` load can run it over the bytes
 * it just read, before they replace the resident context.
 */
uint32_t Combo_TriforceRecordDivergence(const ComboSettingsRecord* settings, const ComboTriforceRecord* rec);

/**
 * Does `live` — `game`'s half re-derived from that game's OWN frozen settings —
 * differ from what the record froze for that half? A present record only; an
 * absent one diverges from nothing. Used by MM's arrival gate, where MM's
 * resolved profile is in hand.
 */
bool Combo_TriforceHalfDiverges(const ComboTriforceRecord* rec, GameId game, const ComboTriforceHalf* live);

/**
 * Is the combo hunt ARMED for the resident world: frozen combo record, frozen
 * goal triforce-hunt, and a triforce record that passes its check? This is the
 * one gate the shared count, both apply caps and both win arms consult.
 */
bool Combo_TriforceHuntArmed(void);

/** The resident world's combo requirement / total; 0 when not armed. */
uint16_t Combo_TriforceHuntRequired(void);
uint16_t Combo_TriforceHuntTotal(void);

// ---- The win trigger ---------------------------------------------------------
/** The give did not reach a requirement. */
#define RSBS_TRIFORCE_WIN_NONE 0
/** NOT armed: the game's OWN requirement was reached; the port does exactly
 *  what upstream does (OoT's mode decides Win vs Ganon's Boss Key). */
#define RSBS_TRIFORCE_WIN_OWN 1
/** Armed: the COMBO requirement was reached by this give. This game ends the
 *  combo through its own ending (OoT always takes its "Win" branch here: the
 *  combo goal is the hunt, so reaching it is the win whichever mode OoT's own
 *  setting named). */
#define RSBS_TRIFORCE_WIN_COMBO 2

/**
 * THE ONE DECISION both ports' piece-give arms call, with the game's counter
 * AFTER this give's increment and the game's OWN requirement.
 *
 * Equality, not >=, exactly as both ports test it: the win fires once, on the
 * give that reaches the requirement. A count raised past it by an arrival's
 * apply did not come from a give in this game — the game that made the reaching
 * collect already fired.
 */
int Combo_TriforceHuntOnPieceGiven(GameId game, uint16_t countAfterGive, uint16_t ownRequired);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_TRIFORCE_HUNT_H
