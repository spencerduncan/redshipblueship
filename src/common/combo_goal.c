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

bool Combo_GoalMetNow(int liveTriforcePieces) {
    if (!Combo_GoalArmed()) {
        return false;
    }
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
                         required) == 1;
}

int Combo_GoalOnFinalBossDefeated(GameId game, int liveTriforcePieces) {
    const uint32_t flag = GoalFlagFor(game);
    if (flag == 0u || !Combo_GoalArmed()) {
        return RSBS_GOAL_ENDING_OWN;
    }
    const bool firstRecord = (gComboCtx.sharedFlags[RSBS_SHARED_FLAGS_WORD_GOAL] & flag) == 0u;
    gComboCtx.sharedFlags[RSBS_SHARED_FLAGS_WORD_GOAL] |= flag;
    const int ending = Combo_GoalMetNow(liveTriforcePieces) ? RSBS_GOAL_ENDING_PLAY : RSBS_GOAL_ENDING_WITHHOLD;
    if (firstRecord) {
        fprintf(stderr, "[Combo] goal: %s final boss defeated under goal %u -> %s\n", game == GAME_OOT ? "OoT" : "MM",
                (unsigned)gComboCtx.comboSettings.goal, Combo_GoalEndingName(ending));
    }
    return ending;
}
