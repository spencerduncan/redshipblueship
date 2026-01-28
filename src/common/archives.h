/**
 * @file archives.h
 * @brief Archive hot-swap infrastructure for game switching
 *
 * Manages per-game archive loading and unloading during game switches.
 * When switching from OoT to MM (or vice versa), the outgoing game's
 * archives are unloaded from the GameArchiveManager and the incoming
 * game registers its archives during initialization.
 *
 * The Ship::Context (and its ResourceManager/ArchiveManager) is fully
 * recreated on each game init, so resource caches are naturally fresh.
 * This module handles the GameArchiveManager singleton which persists
 * across switches.
 */

#ifndef RSBS_COMMON_ARCHIVES_H
#define RSBS_COMMON_ARCHIVES_H

#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Notify the archive system that a game is being initialized.
 * Called by each game's Game_Init after loading archives into Ship::Context.
 * Registers the game's archives with the GameArchiveManager.
 *
 * @param game Which game is initializing
 */
void Archives_OnGameInit(GameId game);

/**
 * Notify the archive system that a game is being shut down.
 * Called before Game_Shutdown to clean up the game's entries
 * in the GameArchiveManager.
 *
 * @param game Which game is shutting down
 */
void Archives_OnGameShutdown(GameId game);

/**
 * Notify the archive system of a game switch.
 * Called by main loop between shutting down the old game and
 * initializing the new one. Ensures clean archive state.
 *
 * @param fromGame The game being switched away from
 * @param toGame The game being switched to
 */
void Archives_OnGameSwitch(GameId fromGame, GameId toGame);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_ARCHIVES_H
