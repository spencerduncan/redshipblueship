/**
 * @file mm_combo_goal_test.cpp
 * MM's half of the combo-goal-ending lock (src/common/tests/test_combo_goal.c):
 * runs MM's REAL ending site, as Majora's death cutscene leaves it, against
 * whatever world the row has frozen, and reports which way it went.
 *
 * The site is Boss07_Wrath_DeathCutscene's warp in z_boss_07.c: upstream's three
 * assignments (Termina Field with cutscene 0xFFF7, which is the ending and then
 * the credits), then MM_ComboGoal_RedirectEndingIfWithheld. This helper
 * performs exactly those assignments on a zeroed PlayState and calls the same
 * function, so the row observes what the actor would leave behind, including
 * the new-day save's roll-over of the cycle. It runs in the shape a cross-game
 * MM session runs in (fileNum 0xFF, no flash write) with no unified-save slot
 * established, so the save's commit writes nothing; MM's live save context and
 * the slot are put back afterwards.
 */

#include "global.h"

#ifdef RSBS_SINGLE_EXECUTABLE

#include "save.h"

#include <cstring>
#include <memory>
#include <vector>

extern "C" SaveContext gSaveContext;
extern "C" int MM_ComboGoal_RedirectEndingIfWithheld(PlayState* play);
extern "C" int MM_ComboGoal_OnMajoraDefeated(void);

namespace {

// Backing store for the marshaller's saveBuf (the new-day save packs into it).
u8 sGoalSaveBuf[SAVE_BUFFER_SIZE];

} // namespace

/**
 * @return 0 the ending warp stands (Termina Field, 0xFFF7) and no cycle rolled;
 *         1 it was rewritten to South Clock Town at day 0, 05:59, no cutscene,
 *           after the new-day save rolled the cycle over;
 *        -1 anything else (a half-rewritten warp).
 */
extern "C" int MM_ComboGoalTest_EndingSite(void) {
    std::unique_ptr<SaveContext> saved(new SaveContext);
    memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    const int savedSlot = RsbsSave_GetActiveSlot();
    RsbsSave_SetActiveSlot(-1);

    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.save.day = 3;
    gSaveContext.save.time = CLOCK_TIME(5, 0);
    const s32 resetsBefore = gSaveContext.save.saveInfo.playerData.threeDayResetCount;

    std::vector<uint8_t> playMem(sizeof(PlayState), uint8_t(0));
    PlayState* play = reinterpret_cast<PlayState*>(playMem.data());
    play->sramCtx.saveBuf = sGoalSaveBuf;

    // Boss07_Wrath_DeathCutscene's warp, verbatim, then the RSBS call after it.
    play->nextEntrance = ENTRANCE(TERMINA_FIELD, 0);
    gSaveContext.nextCutsceneIndex = 0xFFF7;
    play->transitionTrigger = TRANS_TRIGGER_START;
    MM_ComboGoal_RedirectEndingIfWithheld(play);

    const int entrance = play->nextEntrance;
    const int cutscene = gSaveContext.nextCutsceneIndex;
    const int trigger = play->transitionTrigger;
    const int day = gSaveContext.save.day;
    const int time = gSaveContext.save.time;
    const bool rolled = gSaveContext.save.saveInfo.playerData.threeDayResetCount == resetsBefore + 1;

    memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    RsbsSave_SetActiveSlot(savedSlot);

    if (trigger != TRANS_TRIGGER_START) {
        return -1;
    }
    if (entrance == ENTRANCE(TERMINA_FIELD, 0) && cutscene == 0xFFF7 && day == 3 && !rolled) {
        return 0;
    }
    if (entrance == ENTRANCE(SOUTH_CLOCK_TOWN, 0) && cutscene == 0 && day == 0 &&
        time == (int)(u16)(CLOCK_TIME(6, 0) - 1) && rolled) {
        return 1;
    }
    return -1;
}

/** The final-blow half: 1 when MM may dispatch OnGameCompletion. */
extern "C" int MM_ComboGoalTest_FinalBlow(void) {
    const u16 savedPieces = gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces;
    gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces = 0;
    const int allowed = MM_ComboGoal_OnMajoraDefeated();
    gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces = savedPieces;
    return allowed;
}

#endif // RSBS_SINGLE_EXECUTABLE
