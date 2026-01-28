/**
 * @file game_lifecycle.h
 * @brief Game lifecycle management for single-executable architecture
 *
 * Provides the GameOps function table and GameRunner API for managing
 * game initialization, execution, and shutdown across OoT and MM.
 */

#ifndef RSBS_COMMON_GAME_LIFECYCLE_H
#define RSBS_COMMON_GAME_LIFECYCLE_H

#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Function table for a game engine's lifecycle operations.
 */
typedef struct GameOps {
    int  (*init)(int argc, char** argv);
    void (*run)(void);
    void (*suspend)(void);
    void (*resume)(void);
    void (*shutdown)(void);
} GameOps;

/**
 * Get the operations table for a game.
 * Implemented by each game's object library (OoT_GetGameOps / MM_GetGameOps).
 */
const GameOps* OoT_GetGameOps(void);
const GameOps* MM_GetGameOps(void);

/**
 * Get the GameOps for a given GameId.
 */
const GameOps* GameRunner_GetOps(GameId game);

/**
 * Start a game: calls ops->init then ops->run.
 * ops->run is blocking (runs the game loop).
 */
int GameRunner_StartGame(GameId game, int argc, char** argv);

/**
 * Initialize a game without running it (for tests that need the thread split).
 */
int GameRunner_InitGame(GameId game, int argc, char** argv);

/**
 * Run a previously initialized game (blocking).
 */
void GameRunner_RunGame(GameId game);

/**
 * Request a switch to a different game (used by hot-swap / entrance system).
 */
void GameRunner_SwitchTo(GameId game);

/**
 * Shut down all running games cleanly.
 */
void GameRunner_ShutdownAll(void);

/**
 * Shut down a specific game.
 */
void GameRunner_ShutdownGame(GameId game);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_GAME_LIFECYCLE_H
