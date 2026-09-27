/**
 * @file ComboGoalEndingMM.cpp
 * @brief MM's two ending sites under the frozen combo goal (see
 *        src/common/combo_goal.h for the decision and the OoTMM code it follows).
 *
 * MM decides its ending in two places, both for Majora (Boss07, Majora's Wrath):
 *
 *   1. THE FINAL BLOW. Boss07_Wrath_SetupDeathCutscene dispatches
 *      OnGameCompletion, which stamps the file's completion time
 *      (SavingEnhancements.cpp). MM_ComboGoal_OnMajoraDefeated records the
 *      defeat and says whether that dispatch may happen: not while the paired
 *      goal is unmet, because the game goes on.
 *   2. THE ENDING WARP. At the end of the death cutscene Boss07 warps to Termina
 *      Field with cutscene 0xFFF7, which is the ending and then the credits.
 *      MM_ComboGoal_RedirectEndingIfWithheld runs right after those assignments
 *      and, when the paired goal is unmet, rewrites them the way OoTMM does
 *      (mm/play/play.c): day 0 at 05:59, the new-day save (MM's own
 *      Sram_SaveSpecialNewDay, the save the ending itself makes, which rolls the
 *      cycle over and commits the unified save), and a warp to South Clock Town,
 *      where a new cycle starts. The Dawn of the First Day then plays as after
 *      the Song of Time. SkipSoTCutscenes lands a skipped Song of Time at the
 *      same entrance with the same day and time.
 *
 * Unpaired (no frozen combo record) both are no-ops and MM ends as upstream.
 */

#include "global.h"

#ifdef RSBS_SINGLE_EXECUTABLE

#include "combo_goal.h"

extern "C" SaveContext gSaveContext;

namespace {

int LiveTriforcePieces() {
    return (int)gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces;
}

} // namespace

extern "C" int MM_ComboGoal_OnMajoraDefeated(void) {
    return Combo_GoalOnFinalBossDefeated(GAME_MM, LiveTriforcePieces()) != RSBS_GOAL_ENDING_WITHHOLD ? 1 : 0;
}

extern "C" int MM_ComboGoal_RedirectEndingIfWithheld(PlayState* play) {
    if (play == NULL || Combo_GoalOnFinalBossDefeated(GAME_MM, LiveTriforcePieces()) != RSBS_GOAL_ENDING_WITHHOLD) {
        return 0;
    }
    gSaveContext.save.day = 0;
    gSaveContext.save.time = CLOCK_TIME(6, 0) - 1;
    Sram_SaveSpecialNewDay(play);
    play->nextEntrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    gSaveContext.nextCutsceneIndex = 0;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    return 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
