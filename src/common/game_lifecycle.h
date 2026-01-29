/**
 * @file game_lifecycle.h
 * @brief Composable game lifecycle management
 *
 * GameOps: Function table each game provides (init/run/suspend/resume/shutdown).
 * GameRunner: Orchestrator that manages transitions between games.
 *
 * Design: Composition via function pointers, no inheritance.
 * All GameRunner functions are pure (operate on struct pointer) for easy unit testing.
 */

#ifndef RSBS_COMMON_GAME_LIFECYCLE_H
#define RSBS_COMMON_GAME_LIFECYCLE_H

#include "game.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_RUNNER_MAX_GAMES 2

/**
 * Function table for a game's lifecycle callbacks.
 * Each game (OoT, MM) provides one of these.
 */
typedef struct GameOps {
    const char* id;       /* "oot" or "mm" */
    const char* name;     /* "Ocarina of Time" or "Majora's Mask" */

    int  (*init)(int argc, char** argv);
    void (*run)(void);
    void (*suspend)(void);    /* Pause for game switch (keep shared state alive) */
    void (*resume)(void);     /* Resume after being suspended */
    void (*shutdown)(void);   /* Full cleanup (final exit only) */
} GameOps;

/**
 * Game state as tracked by the runner.
 */
typedef enum {
    GAME_LIFECYCLE_STATE_NONE = 0,      /* Not registered or not started */
    GAME_LIFECYCLE_STATE_READY,         /* Registered but not yet initialized */
    GAME_LIFECYCLE_STATE_RUNNING,       /* Currently active */
    GAME_LIFECYCLE_STATE_SUSPENDED,     /* Paused for a game switch */
    GAME_LIFECYCLE_STATE_STOPPED        /* Shut down */
} GameLifecycleState;

/**
 * Orchestrator that manages game lifecycle transitions.
 */
typedef struct GameRunner {
    GameOps*   games[GAME_RUNNER_MAX_GAMES];    /* Indexed by (GameId - 1) */
    GameLifecycleState  states[GAME_RUNNER_MAX_GAMES];
    GameId     activeGame;
} GameRunner;

/* ---------- GameRunner API ---------- */

/** Initialize a runner to empty state. */
void GameRunner_Init(GameRunner* runner);

/** Register a game's ops. Returns 0 on success, -1 on invalid id or duplicate. */
int GameRunner_RegisterGame(GameRunner* runner, GameId id, GameOps* ops);

/** Start a game (calls init). Returns init's return code. */
int GameRunner_StartGame(GameRunner* runner, GameId id, int argc, char** argv);

/**
 * Switch from the active game to target.
 * - Suspends the current game (calls suspend callback).
 * - If target was previously suspended, calls resume; otherwise calls init.
 * - Calls run on the new game.
 * Returns 0 on success, -1 on error.
 */
int GameRunner_SwitchTo(GameRunner* runner, GameId target, int argc, char** argv);

/** Shutdown all registered games. Calls shutdown on each that isn't already stopped. */
void GameRunner_ShutdownAll(GameRunner* runner);

/** Get the currently active game. */
GameId GameRunner_GetActive(const GameRunner* runner);

/** Check if a specific game is in suspended state. */
bool GameRunner_IsSuspended(const GameRunner* runner, GameId id);

/** Get the GameOps for a registered game (or NULL). */
GameOps* GameRunner_GetOps(const GameRunner* runner, GameId id);

/** Get the state of a registered game. */
GameLifecycleState GameRunner_GetState(const GameRunner* runner, GameId id);

#ifdef __cplusplus
}
#endif

#endif /* RSBS_COMMON_GAME_LIFECYCLE_H */
