/**
 * @file switch.cpp
 * @brief Game switching logic for cross-game transitions
 *
 * Coordinates the process of switching between OoT and MM:
 * 1. Freeze current game state (save context + return entrance)
 * 2. Update gCurrentGame to target
 * 3. Resume target game from frozen state (or fresh start if first switch)
 *
 * The freeze/resume functions are implemented in each game's port layer:
 * - OoT: games/oot/soh/OTRGlobals.cpp
 * - MM:  games/mm/2s2h/BenPort.cpp
 */

#include "context.h"
#include "entrance.h"
#include <cstdio>

// ============================================================================
// Game-specific freeze/resume functions (implemented in game port layers)
// ============================================================================

extern "C" {

/**
 * Freeze OoT game state before switching away
 * Saves the current SaveContext and any other necessary state
 */
void OoT_FreezeState(ComboContext* ctx);

/**
 * Resume OoT from a frozen state or start fresh
 * Called when switching back to OoT (or starting OoT from MM)
 */
void OoT_ResumeFromContext(ComboContext* ctx);

/**
 * Freeze MM game state before switching away
 * Saves the current SaveContext and any other necessary state
 */
void MM_FreezeState(ComboContext* ctx);

/**
 * Resume MM from a frozen state or start fresh
 * Called when switching back to MM (or starting MM from OoT)
 */
void MM_ResumeFromContext(ComboContext* ctx);

} // extern "C"

// ============================================================================
// Internal state
// ============================================================================

namespace {

/**
 * Track whether a switch is currently in progress
 * Prevents re-entrancy during the switch process
 */
bool gSwitchInProgress = false;

/**
 * Freeze the current game's state
 */
void FreezeCurrentGame(void) {
    if (gCurrentGame == GAME_NONE) {
        return;
    }

    // Record source game info in combo context
    gComboCtx.sourceGame = gCurrentGame;
    gComboCtx.sourceEntrance = Entrance_GetSwitchReturnEntrance();

    // Call game-specific freeze function
    if (gCurrentGame == GAME_OOT) {
        OoT_FreezeState(&gComboCtx);
    } else if (gCurrentGame == GAME_MM) {
        MM_FreezeState(&gComboCtx);
    }

    fprintf(stderr, "[Switch] Froze %s state, return entrance: 0x%04X\n",
            Game_ToString(gCurrentGame), gComboCtx.sourceEntrance);
}

/**
 * Resume the target game
 */
void ResumeTargetGame(GameId target) {
    fprintf(stderr, "[Switch] Resuming %s at entrance 0x%04X\n",
            Game_ToString(target), gComboCtx.targetEntrance);

    // Set the startup entrance for the target game
    Entrance_SetStartupEntrance(gComboCtx.targetEntrance);

    // Call game-specific resume function
    if (target == GAME_OOT) {
        OoT_ResumeFromContext(&gComboCtx);
    } else if (target == GAME_MM) {
        MM_ResumeFromContext(&gComboCtx);
    }

    // Update current game
    gCurrentGame = target;
}

} // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

extern "C" {

void Context_ProcessSwitch(void) {
    // Check if a switch was requested
    if (!Context_HasPendingSwitch()) {
        return;
    }

    // Prevent re-entrancy
    if (gSwitchInProgress) {
        fprintf(stderr, "[Switch] Warning: Switch already in progress\n");
        return;
    }
    gSwitchInProgress = true;

    GameId target = gComboCtx.targetGame;
    uint16_t targetEntrance = gComboCtx.targetEntrance;

    fprintf(stderr, "[Switch] Processing switch: %s -> %s (entrance 0x%04X)\n",
            Game_ToString(gCurrentGame),
            Game_ToString(target),
            targetEntrance);

    // Validate target
    if (target == GAME_NONE || target == gCurrentGame) {
        fprintf(stderr, "[Switch] Invalid target game, aborting\n");
        ComboContext_ClearSwitch();
        gSwitchInProgress = false;
        return;
    }

    // Step 1: Freeze current game state
    FreezeCurrentGame();

    // Step 2: Resume target game
    ResumeTargetGame(target);

    // Step 3: Clear the switch request
    ComboContext_ClearSwitch();
    Entrance_ClearPendingSwitch();

    fprintf(stderr, "[Switch] Switch complete, now in %s\n",
            Game_ToString(gCurrentGame));

    gSwitchInProgress = false;
}

/**
 * Get whether a switch is currently in progress
 */
bool Context_IsSwitchInProgress(void) {
    return gSwitchInProgress;
}

/**
 * Set the current game (used during initialization)
 */
void Context_SetCurrentGame(GameId game) {
    gCurrentGame = game;
    fprintf(stderr, "[Switch] Current game set to %s\n", Game_ToString(game));
}

/**
 * Get the current game
 */
GameId Context_GetCurrentGame(void) {
    return gCurrentGame;
}

} // extern "C"
