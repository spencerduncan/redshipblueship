/**
 * @file game_lifecycle.c
 * @brief GameRunner implementation
 */

#include "game_lifecycle.h"
#include <stdio.h>
#include <string.h>

/* Convert GameId to array index (GameId is 1-based, array is 0-based) */
static int GameIdToIndex(GameId id) {
    if (id < GAME_OOT || id > GAME_MM) return -1;
    return (int)id - 1;
}

void GameRunner_Init(GameRunner* runner) {
    memset(runner, 0, sizeof(*runner));
    runner->activeGame = GAME_NONE;
    for (int i = 0; i < GAME_RUNNER_MAX_GAMES; i++) {
        runner->states[i] = GAME_LIFECYCLE_STATE_NONE;
    }
}

int GameRunner_RegisterGame(GameRunner* runner, GameId id, GameOps* ops) {
    int idx = GameIdToIndex(id);
    if (idx < 0 || !ops) return -1;
    if (runner->games[idx] != NULL) return -1; /* Already registered */

    runner->games[idx] = ops;
    runner->states[idx] = GAME_LIFECYCLE_STATE_READY;
    return 0;
}

int GameRunner_StartGame(GameRunner* runner, GameId id, int argc, char** argv) {
    int idx = GameIdToIndex(id);
    if (idx < 0 || !runner->games[idx]) return -1;
    if (runner->activeGame != GAME_NONE) return -1; /* Already have an active game */

    GameOps* ops = runner->games[idx];
    int rc = 0;
    if (ops->init) {
        rc = ops->init(argc, argv);
        if (rc != 0) return rc;
    }

    runner->states[idx] = GAME_LIFECYCLE_STATE_RUNNING;
    runner->activeGame = id;
    return 0;
}

int GameRunner_SwitchTo(GameRunner* runner, GameId target, int argc, char** argv) {
    int targetIdx = GameIdToIndex(target);
    if (targetIdx < 0 || !runner->games[targetIdx]) return -1;

    /* If switching to the same game, no-op */
    if (target == runner->activeGame) return 0;

    /* Suspend the currently active game */
    if (runner->activeGame != GAME_NONE) {
        int activeIdx = GameIdToIndex(runner->activeGame);
        GameOps* activeOps = runner->games[activeIdx];
        if (activeOps && activeOps->suspend) {
            activeOps->suspend();
        }
        runner->states[activeIdx] = GAME_LIFECYCLE_STATE_SUSPENDED;
    }

    /* Start or resume the target game */
    GameOps* targetOps = runner->games[targetIdx];
    int rc = 0;

    if (runner->states[targetIdx] == GAME_LIFECYCLE_STATE_SUSPENDED) {
        /* Resume */
        if (targetOps->resume) {
            targetOps->resume();
        }
    } else {
        /* First time — init */
        if (targetOps->init) {
            rc = targetOps->init(argc, argv);
            if (rc != 0) return rc;
        }
    }

    runner->states[targetIdx] = GAME_LIFECYCLE_STATE_RUNNING;
    runner->activeGame = target;
    return 0;
}

void GameRunner_ShutdownAll(GameRunner* runner) {
    for (int i = 0; i < GAME_RUNNER_MAX_GAMES; i++) {
        if (runner->games[i] && runner->states[i] != GAME_LIFECYCLE_STATE_NONE
                              && runner->states[i] != GAME_LIFECYCLE_STATE_STOPPED) {
            if (runner->games[i]->shutdown) {
                runner->games[i]->shutdown();
            }
            runner->states[i] = GAME_LIFECYCLE_STATE_STOPPED;
        }
    }
    runner->activeGame = GAME_NONE;
}

GameId GameRunner_GetActive(const GameRunner* runner) {
    return runner->activeGame;
}

bool GameRunner_IsSuspended(const GameRunner* runner, GameId id) {
    int idx = GameIdToIndex(id);
    if (idx < 0) return false;
    return runner->states[idx] == GAME_LIFECYCLE_STATE_SUSPENDED;
}

GameOps* GameRunner_GetOps(const GameRunner* runner, GameId id) {
    int idx = GameIdToIndex(id);
    if (idx < 0) return NULL;
    return runner->games[idx];
}

GameLifecycleState GameRunner_GetState(const GameRunner* runner, GameId id) {
    int idx = GameIdToIndex(id);
    if (idx < 0) return GAME_LIFECYCLE_STATE_NONE;
    return runner->states[idx];
}
