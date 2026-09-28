/**
 * @file combo_goal.c
 * @brief The paired game's end under the frozen combo goal. See combo_goal.h
 *        for the contract and the OoTMM code it follows.
 *
 * Game-header-free: both ports reach it through their ending sites, and the
 * ROM-free lock (combo-goal-ending) drives it directly.
 */

#include "combo_goal.h"

#include "combo_logic.h"      // Combo_Logic_EvaluateGoal, Combo_Logic_EvaluateTriforceHunt
#include "foreign_items.h"    // RSBS_COMBO_GOAL_*, Combo_ComboSettingsFrozen
#include "shared_resources.h" // Combo_GetSharedResource
#include "triforce_hunt.h"    // Combo_TriforceHuntRequired

#include <stdio.h>

const char* Combo_GoalEndingName(int ending) {
    switch (ending) {
        case RSBS_GOAL_ENDING_OWN:
            return "own";
        case RSBS_GOAL_ENDING_PLAY:
            return "play";
        case RSBS_GOAL_ENDING_WITHHOLD:
            return "withhold";
        default:
            return "(unknown)";
    }
}

int Combo_GoalMet(uint8_t goal, bool ootBeaten, bool mmBeaten, int triforcePieces, uint16_t triforceRequired) {
    if (goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) {
        // One shared count against the frozen combo requirement (ADR 0010 O10).
        // Neither boss is a term: in OoTMM both are locked away under this goal,
        // and here a boss defeated before the last piece ends nothing.
        return Combo_Logic_EvaluateTriforceHunt(triforcePieces, triforceRequired);
    }
    // The coordinator's own boolean expression, so the runtime end and the
    // creation's proof cannot disagree about what a goal means.
    return Combo_Logic_EvaluateGoal(goal, ootBeaten ? 1 : 0, mmBeaten ? 1 : 0);
}

bool Combo_GoalArmed(void) {
    return Combo_ComboSettingsFrozen();
}

static uint32_t GoalFlagFor(GameId game) {
    if (game == GAME_OOT) {
        return RSBS_GOAL_FLAG_OOT_FINAL_BOSS;
    }
    if (game == GAME_MM) {
        return RSBS_GOAL_FLAG_MM_FINAL_BOSS;
    }
    return 0u;
}

bool Combo_GoalFinalBossRecorded(GameId game) {
    const uint32_t flag = GoalFlagFor(game);
    return flag != 0u && (gComboCtx.sharedFlags[RSBS_SHARED_FLAGS_WORD_GOAL] & flag) != 0u;
}

/** The frozen goal's state over the live record: 1 met, 0 not met, -1 cannot
 *  be evaluated. Armed callers only. */
static int GoalStateNow(int liveTriforcePieces) {
    int pieces = -1;
    uint16_t required = 0u;
    const uint8_t goal = gComboCtx.comboSettings.goal;
    if (goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) {
        // The pool holds every collect harvested so far; the calling game's own
        // counter can be ahead of it by collects since its arrival. Both are
        // mirrors of one MONOTONIC count, so the larger is the count.
        uint16_t pool = 0u;
        pieces = Combo_GetSharedResource((uint8_t)RSBS_SHARED_RES_TRIFORCE_PIECES, &pool) ? (int)pool : 0;
        if (liveTriforcePieces > pieces) {
            pieces = liveTriforcePieces;
        }
        required = Combo_TriforceHuntRequired();
    }
    return Combo_GoalMet(goal, Combo_GoalFinalBossRecorded(GAME_OOT), Combo_GoalFinalBossRecorded(GAME_MM), pieces,
                         required);
}

bool Combo_GoalMetNow(int liveTriforcePieces) {
    return Combo_GoalArmed() && GoalStateNow(liveTriforcePieces) == 1;
}

bool Combo_GoalAllowsCompletion(int liveTriforcePieces) {
    // Unpaired: upstream. Unevaluable: fail open, as the decision does.
    return !Combo_GoalArmed() || GoalStateNow(liveTriforcePieces) != 0;
}

int Combo_GoalOnFinalBossDefeated(GameId game, int liveTriforcePieces) {
    const uint32_t flag = GoalFlagFor(game);
    if (flag == 0u || !Combo_GoalArmed()) {
        return RSBS_GOAL_ENDING_OWN;
    }
    const bool firstRecord = (gComboCtx.sharedFlags[RSBS_SHARED_FLAGS_WORD_GOAL] & flag) == 0u;
    gComboCtx.sharedFlags[RSBS_SHARED_FLAGS_WORD_GOAL] |= flag;
    const int state = GoalStateNow(liveTriforcePieces);
    // FAIL OPEN. A goal that cannot be evaluated (a goal byte outside the pinned
    // table, or triforce-hunt whose record fails its check, so the requirement
    // reads 0) would otherwise withhold every ending forever while the hunt
    // itself is disarmed: a world that can never end. Creation and the .redsave
    // load refuse both states, so this is a damaged record; the defeating game
    // then ends as its own game, as upstream does, and the log says why.
    const int ending =
        state == 1 ? RSBS_GOAL_ENDING_PLAY : (state == 0 ? RSBS_GOAL_ENDING_WITHHOLD : RSBS_GOAL_ENDING_OWN);
    if (firstRecord) {
        if (state < 0) {
            fprintf(stderr,
                    "[Combo] goal: ERROR: frozen goal %u cannot be evaluated (triforce requirement %u); %s's final "
                    "boss ends its own game instead of never ending the paired game\n",
                    (unsigned)gComboCtx.comboSettings.goal, (unsigned)Combo_TriforceHuntRequired(),
                    game == GAME_OOT ? "OoT" : "MM");
        }
        fprintf(stderr, "[Combo] goal: %s final boss defeated under goal %u -> %s\n", game == GAME_OOT ? "OoT" : "MM",
                (unsigned)gComboCtx.comboSettings.goal, Combo_GoalEndingName(ending));
    }
    return ending;
}

bool Combo_GoalKeepsOwnHuntWin(uint8_t goal) {
    if (goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) {
        return true;
    }
    // A boss goal the coordinator can evaluate: a half's own hunt is no term of
    // it. Anything else is a record the creation refuses on its own.
    return Combo_Logic_EvaluateGoal(goal, 0, 0) < 0;
}

int Combo_GoalOnTriforceHuntCompleted(GameId game, int liveTriforcePieces) {
    if (GoalFlagFor(game) == 0u || !Combo_GoalArmed()) {
        return RSBS_GOAL_ENDING_OWN;
    }
    const int state = GoalStateNow(liveTriforcePieces);
    int ending;
    if (state < 0) {
        // FAIL OPEN, as the final-boss decision does: a damaged record must not
        // turn a hunt that used to end its game into one that never can.
        ending = RSBS_GOAL_ENDING_OWN;
    } else if (Combo_TriforceHuntArmed()) {
        // The combo hunt: the reaching give meets the goal (the arms only get
        // here on the combo requirement).
        ending = state == 1 ? RSBS_GOAL_ENDING_PLAY : RSBS_GOAL_ENDING_WITHHOLD;
    } else {
        // A boss goal: the half's own hunt is not a term of it (OoTMM has no
        // per-game hunt). Its completion unlocks the final boss and ends nothing.
        ending = RSBS_GOAL_ENDING_WITHHOLD;
    }
    if (state < 0) {
        fprintf(stderr,
                "[Combo] goal: ERROR: frozen goal %u cannot be evaluated (triforce requirement %u); %s's own triforce "
                "hunt ends its own game instead of never ending the paired game\n",
                (unsigned)gComboCtx.comboSettings.goal, (unsigned)Combo_TriforceHuntRequired(),
                game == GAME_OOT ? "OoT" : "MM");
    }
    fprintf(stderr, "[Combo] goal: %s triforce hunt completed under goal %u -> %s\n", game == GAME_OOT ? "OoT" : "MM",
            (unsigned)gComboCtx.comboSettings.goal, Combo_GoalEndingName(ending));
    return ending;
}
