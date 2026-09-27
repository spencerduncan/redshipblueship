/**
 * @file oot_combo_goal_test.cpp
 * OoT's half of the combo-goal-ending lock (src/common/tests/test_combo_goal.c):
 * runs OoT's REAL ending site, as BossGanon2's death cutscene leaves it, against
 * whatever world the row has frozen, and reports which way it went.
 *
 * The site is BossGanon2's `case 20` in z_boss_ganon2.c: upstream's four
 * assignments (the Chamber of the Sages with cutscene 0xFFF2 and a child Link,
 * which is the ending and then the credits), then
 * OoT_ComboGoal_RedirectEndingIfWithheld. This helper performs exactly those
 * assignments on a zeroed PlayState and calls the same function, so the row
 * observes what the actor would leave behind. gSaveContext.fileNum is the 0xFF
 * "no file" value, so Play_PerformSave (the withheld path's one save) returns
 * without writing; the row does not test saving.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstring>
#include <memory>
#include <vector>

extern "C" {
#include <z64.h>
#include "variables.h"
#include "functions.h"
int OoT_ComboGoal_RedirectEndingIfWithheld(PlayState* play);
int OoT_ComboGoal_OnGanonDefeated(void);
}

/**
 * @return 0 the ending warp stands (Chamber of the Sages, 0xFFF2, child);
 *         1 it was rewritten to Ganon's Tower as an adult with no cutscene;
 *        -1 anything else (a half-rewritten warp).
 */
extern "C" int OoT_ComboGoalTest_EndingSite(void) {
    std::unique_ptr<SaveContext> saved(new SaveContext);
    memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    // The row decides triforce-hunt worlds through the count it passes itself;
    // OoT's own counter must not carry a count from an earlier row.
    gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected = 0;

    std::vector<uint8_t> playMem(sizeof(PlayState), uint8_t(0));
    PlayState* play = reinterpret_cast<PlayState*>(playMem.data());
    play->transitionMode = TRANS_MODE_OFF;

    // BossGanon2's case 20, verbatim, then the RSBS call that follows it.
    play->nextEntranceIndex = ENTR_CHAMBER_OF_THE_SAGES_0;
    gSaveContext.nextCutsceneIndex = 0xFFF2;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_WHITE;
    play->linkAgeOnLoad = 1;
    OoT_ComboGoal_RedirectEndingIfWithheld(play);

    const int entrance = play->nextEntranceIndex;
    const int cutscene = gSaveContext.nextCutsceneIndex;
    const int age = play->linkAgeOnLoad;
    const int trigger = play->transitionTrigger;
    memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));

    if (trigger != TRANS_TRIGGER_START) {
        return -1;
    }
    if (entrance == ENTR_CHAMBER_OF_THE_SAGES_0 && cutscene == 0xFFF2 && age == 1) {
        return 0;
    }
    if (entrance == ENTR_GANONS_TOWER_0 && cutscene == 0 && age == LINK_AGE_ADULT) {
        return 1;
    }
    return -1;
}

/** The final-blow half: 1 when OoT's stats may mark the game complete. */
extern "C" int OoT_ComboGoalTest_FinalBlow(void) {
    const u8 savedPieces = gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;
    gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected = 0;
    const int allowed = OoT_ComboGoal_OnGanonDefeated();
    gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected = savedPieces;
    return allowed;
}

#endif // RSBS_SINGLE_EXECUTABLE
