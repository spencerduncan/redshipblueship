/**
 * @file ComboGoalEndingOoT.cpp
 * @brief OoT's two ending sites under the frozen combo goal (see
 *        src/common/combo_goal.h for the decision and the OoTMM code it follows).
 *
 * OoT decides its ending in two places, both for Ganon (BossGanon2):
 *
 *   1. THE FINAL BLOW. BossGanon2's death cutscene dispatches OnBossDefeat;
 *      BossDefeatTimestamps.cpp then marks the game complete, which freezes
 *      OoT's gameplay-stat timers. OoT_ComboGoal_OnGanonDefeated records the
 *      defeat and says whether that mark may be made: not while the paired goal
 *      is unmet, because the game goes on.
 *   2. THE ENDING WARP. At the end of that cutscene BossGanon2 warps to the
 *      Chamber of the Sages with cutscene 0xFFF2, which is the ending and then
 *      the credits. OoT_ComboGoal_RedirectEndingIfWithheld runs right after
 *      those assignments and, when the paired goal is unmet, rewrites them: the
 *      player goes to Ganon's Tower as an adult instead, and the file is saved
 *      once. That is OoTMM's endGame() without the ending: it saves and puts the
 *      player back at Ganon's Castle (Ganon's Tower when entrances are shuffled).
 *      Ganon's Tower is used here in every case because it is where OoT's own
 *      load puts a save made in Ganon's arena (z_sram.c maps SCENE_GANON_BOSS to
 *      ENTR_GANONS_TOWER_0), so the live warp and a reload of that save agree.
 *
 * Unpaired (no frozen combo record) both are no-ops and OoT ends as upstream.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "combo_goal.h"

// z64.h includes <memory> under __cplusplus. Included first here, outside the
// extern "C" block below: libstdc++ refuses templates with C linkage, so a
// first sight of <memory> inside that block fails to compile with GCC 11 on
// Linux (MSVC accepts it, which is how it passed the Windows build).
#include <memory>

extern "C" {
#include <z64.h>
#include "variables.h"
#include "functions.h"
}

namespace {

int LiveTriforcePieces() {
    return (int)gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;
}

} // namespace

extern "C" int OoT_ComboGoal_OnGanonDefeated(void) {
    return Combo_GoalOnFinalBossDefeated(GAME_OOT, LiveTriforcePieces()) != RSBS_GOAL_ENDING_WITHHOLD ? 1 : 0;
}

extern "C" int OoT_ComboGoal_RedirectEndingIfWithheld(PlayState* play) {
    if (play == NULL || Combo_GoalOnFinalBossDefeated(GAME_OOT, LiveTriforcePieces()) != RSBS_GOAL_ENDING_WITHHOLD) {
        return 0;
    }
    // BossGanon2 re-issues its warp every frame until the transition starts, so
    // this runs more than once. The save is made on the first frame only: the
    // next Play_Update moves transitionMode off TRANS_MODE_OFF.
    if (play->transitionMode == TRANS_MODE_OFF) {
        Play_PerformSave(play);
    }
    play->nextEntranceIndex = ENTR_GANONS_TOWER_0;
    gSaveContext.nextCutsceneIndex = 0;
    play->linkAgeOnLoad = LINK_AGE_ADULT;
    return 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
