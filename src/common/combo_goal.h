/**
 * @file combo_goal.h
 * @brief The paired game's END: which final-boss defeat plays its game's
 *        ending, under the FROZEN combo goal (OoTMM parity).
 *
 * ============================================================================
 * WHAT OoTMM DOES, AND SO WHAT THIS DOES
 * ============================================================================
 *
 * OoTMM's `goal` setting reads "The game will end when the specified goal is
 * reached" (packages/core/src/settings/data.ts). Its runtime, read at
 * OoTMM/OoTMM 57cc028b (packages/generator/src):
 *
 *   - common/config.c `Config_IsGoal()`: false while CFG_GOAL_GANON is set and
 *     Ganon is not beaten, or CFG_GOAL_MAJORA is set and Majora is not; true
 *     otherwise.
 *   - oot/play/play.c `endGame()`, run when OoT's post-Ganon ending entrance is
 *     taken: sets the durable "Ganon beaten" flag, saves, and plays the ending
 *     only when Config_IsGoal() is true. Otherwise the saved position stands
 *     and the player is put back into OoT (Ganon's Castle exterior, or Ganon's
 *     Tower when entrances are shuffled). No message is shown.
 *   - mm/play/play.c, on the arrival that would start MM's ending (Termina
 *     Field with cutscene 0xfff7): sets the durable "Majora beaten" flag and,
 *     when Config_IsGoal() is false, sets day 0 at 05:59, performs the
 *     new-day save and warps to the start of a new cycle. No message.
 *   - common/triggers.c: under the triforce goals the hunt's last piece calls
 *     comboCreditWarp(), whichever game it is found in. Both final bosses are
 *     locked away under those goals (oot/doors.c, mm/actors/En/En_Js.c).
 *
 * So the ending plays in whichever game makes the goal expression TRUE, and a
 * final-boss defeat that leaves it false records the defeat and sends the
 * player back into that game. This header is that decision for the combo:
 *
 *   1. THE RECORD. Each game's final-boss defeat is one bit in
 *      gComboCtx.sharedFlags (ADR 0002 kept the array for origin-neutral event
 *      bits; this file assigns word 0's first two bits). It is session state:
 *      the creation event's invalidation zeroes it, the .redsave Tier-1 record
 *      carries it, and it is resident across a game switch, so a defeat in one
 *      game is known to the other with no shim.
 *   2. THE PREDICATE. Combo_GoalMet evaluates the FROZEN goal
 *      (gComboCtx.comboSettings.goal, never a CVar) over those two bits: the
 *      same expression the coordinator proves (Combo_Logic_EvaluateGoal), over
 *      the live record instead of logic. Triforce hunt is the shared piece count
 *      against the frozen combo requirement (Combo_Logic_EvaluateTriforceHunt).
 *   3. THE DECISION. Combo_GoalOnFinalBossDefeated records the defeat and says
 *      what the defeating game does: its own ending (not paired: upstream,
 *      untouched), its ending as the end of the paired game (goal met), or no
 *      ending (goal not met: the port puts the player back into its game, the
 *      way OoTMM does).
 *
 * ARMING. Everything here is live only when a combo settings record is frozen
 * (Combo_ComboSettingsFrozen: a paired world created since ADR 0011, or a
 * legacy pair after its first crossing). An unpaired file, a vanilla file and a
 * legacy pair before its first crossing see each game's ending exactly as
 * upstream wrote it, and no bit is written. (So a legacy pair that beat Ganon
 * before its first crossing, which then freezes beat-both, must beat him again:
 * accepted, saves are pre-release; ADR 0010's 2026-09-27 D1 amendment.)
 *
 * TRIFORCE HUNT AND THE BOSSES: OoTMM locks both away. Here a boss is locked
 * only by its own half's hunt when that half's hunt is on; the other half's
 * boss stays fightable and its defeat is withheld, never an ending (the ADR
 * amendment gives the reason).
 *
 * NOT COVERED: a half's OWN triforce hunt in a world whose combo goal is not
 * triforce-hunt still ends that half's game through its own win arm
 * (Combo_TriforceHuntOnPieceGiven's WIN_OWN), as it did before this file.
 *
 * Game-header-free, like triforce_hunt.c. THREADING: game thread only.
 */

#ifndef RSBS_COMMON_COMBO_GOAL_H
#define RSBS_COMMON_COMBO_GOAL_H

#include "context.h" // gComboCtx, GameId

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- The record: gComboCtx.sharedFlags assignments -------------------------
// Word and bit positions are .redsave format (Tier-1 carries sharedFlags raw):
// append-only, never renumbered. Every other bit of every word stays zero.
#define RSBS_SHARED_FLAGS_WORD_GOAL 0u
#define RSBS_GOAL_FLAG_OOT_FINAL_BOSS (1u << 0) // Ganon defeated in OoT
#define RSBS_GOAL_FLAG_MM_FINAL_BOSS (1u << 1)  // Majora defeated in MM

// ---- The decision -----------------------------------------------------------
/** Not paired (no frozen combo record): the game plays its own ending exactly
 *  as upstream does, and nothing is recorded. Also the FAIL-OPEN answer for a
 *  paired world whose frozen goal cannot be evaluated (see
 *  Combo_GoalOnFinalBossDefeated): the defeat is recorded and the game ends as
 *  its own game, never "withhold forever". */
#define RSBS_GOAL_ENDING_OWN 0
/** Paired, and the frozen goal is met: the game plays its own ending, and that
 *  ending is the end of the paired game. */
#define RSBS_GOAL_ENDING_PLAY 1
/** Paired, and the frozen goal is still unmet: no ending. The defeat is
 *  recorded; the port returns the player to its game (OoTMM's behaviour). */
#define RSBS_GOAL_ENDING_WITHHOLD 2

/** Its name ("own", "play", "withhold"), or "(unknown)". Never NULL. */
const char* Combo_GoalEndingName(int ending);

/**
 * THE PREDICATE, pure. 1 when `goal` is met by the given state, 0 when it is
 * not, -1 when it cannot be evaluated (a goal outside the pinned table, or a
 * triforce hunt with no requirement). Only the terms of `goal`'s own expression
 * are read: `beat-oot` never reads `mmBeaten`, `triforce-hunt` reads neither
 * boss.
 */
int Combo_GoalMet(uint8_t goal, bool ootBeaten, bool mmBeaten, int triforcePieces, uint16_t triforceRequired);

/** Is the goal machinery live for the resident world (a frozen combo record)? */
bool Combo_GoalArmed(void);

/** Has `game`'s final boss been recorded as defeated in the resident world? */
bool Combo_GoalFinalBossRecorded(GameId game);

/**
 * Is the resident world's frozen goal met right now? False when not armed.
 * `liveTriforcePieces` is the calling game's own counter (a mirror of the one
 * combo count, which can be ahead of the shared pool between harvests); pass -1
 * when the caller has none. Only a triforce-hunt goal reads it.
 */
bool Combo_GoalMetNow(int liveTriforcePieces);

/**
 * May a game mark ITSELF complete right now (OoT's gameplay-stat
 * `gameComplete`, which freezes its timers)? True unpaired (upstream), true when
 * the frozen goal is met, true when it cannot be evaluated (fail open, as the
 * decision below); false only while a paired goal is evaluably unmet. Reads,
 * never records: for completion writers that are not a final-boss site (OoT's
 * Time Splits' last split).
 */
bool Combo_GoalAllowsCompletion(int liveTriforcePieces);

/**
 * THE ONE DECISION both ports' ending sites call: at the final-boss defeat, and
 * again where the game would start its ending. Idempotent: it records `game`'s
 * defeat (armed only) and returns RSBS_GOAL_ENDING_*. A second call for the
 * same defeat returns the same answer and changes nothing further.
 *
 * FAIL DIRECTION: a frozen goal that cannot be evaluated (Combo_GoalMet -1: a
 * goal byte outside the pinned table, or triforce-hunt whose record fails its
 * check) answers RSBS_GOAL_ENDING_OWN and logs an ERROR, because "withhold"
 * there would mean a paired world that can never end while its hunt is
 * disarmed. Creation and the .redsave load refuse both states; this is the
 * answer for a damaged record that got past them.
 */
int Combo_GoalOnFinalBossDefeated(GameId game, int liveTriforcePieces);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_GOAL_H
