/**
 * @file mm_gameover_revive_test.cpp
 * MM-side helpers for the ComboGameOverRevive row (#664), whose driver is
 * games/oot/soh/oot_gameover_revive_test.cpp. That row drives BOTH directions of
 * the F10-during-game-over health handover, and each direction ends in the OTHER
 * game's arrival apply, so the OoT TU needs a way to arm, read and freeze MM's
 * layout of the unified gSaveContext storage (src/common/unified_save.c). Only
 * an MM TU can name MM's SaveContext fields; these are those names, nothing more.
 * No assertions live here -- the OoT TU owns the row's invariants.
 */

#include "global.h"

#include "context.h"

#include <cstring>

extern "C" SaveContext gSaveContext;

namespace {
// Frozen-blob readback target. File-static: MM's runtime SaveContext carries the
// 2s2h extensions and is tens of KB.
SaveContext sMMScratch;
} // namespace

/** MM's live save as a cross-game MM session runs it: the 0xFF "no flash slot"
 *  sentinel and GAMEMODE_NORMAL (the gate MM_SaveIsLiveFile reads), with the
 *  given current health and health capacity. Everything else zero. */
extern "C" void MM_GameOverReviveTest_ArmLive(int16_t health, int16_t capacity) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.saveInfo.playerData.health = health;
    gSaveContext.save.saveInfo.playerData.healthCapacity = capacity;
}

extern "C" void MM_GameOverReviveTest_SetHealth(int16_t health) {
    gSaveContext.save.saveInfo.playerData.health = health;
}

extern "C" int MM_GameOverReviveTest_Health(void) {
    return (int)gSaveContext.save.saveInfo.playerData.health;
}

/** Current health of the frozen MM half, or -1 if there is none. Reads through
 *  Context_RestoreState, which copies without retiring the blob. */
extern "C" int MM_GameOverReviveTest_FrozenHealth(void) {
    memset(&sMMScratch, 0, sizeof(SaveContext));
    if (Context_RestoreState(GAME_MM, &sMMScratch, sizeof(SaveContext)) == 0) {
        return -1;
    }
    return (int)sMMScratch.save.saveInfo.playerData.health;
}
