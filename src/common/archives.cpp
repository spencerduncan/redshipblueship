/**
 * @file archives.cpp
 * @brief Archive hot-swap implementation for game switching
 */

#include "archives.h"
#include "combo/GameArchives.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <cstdio>

/**
 * Register the current game's archives with the GameArchiveManager.
 * Reads archives from Ship::Context's ArchiveManager and tags them
 * as belonging to the specified game.
 */
void Archives_OnGameInit(GameId game) {
    auto context = Ship::Context::GetInstance();
    if (!context) {
        fprintf(stderr, "[ARCHIVES] OnGameInit(%s): No Ship::Context\n", Game_ToString(game));
        return;
    }

    auto resourceMgr = context->GetResourceManager();
    if (!resourceMgr) {
        fprintf(stderr, "[ARCHIVES] OnGameInit(%s): No ResourceManager\n", Game_ToString(game));
        return;
    }

    auto archiveMgr = resourceMgr->GetArchiveManager();
    if (!archiveMgr) {
        fprintf(stderr, "[ARCHIVES] OnGameInit(%s): No ArchiveManager\n", Game_ToString(game));
        return;
    }

    Combo::Game comboGame = Combo::FromGameId(game);
    auto& gam = Combo::GameArchiveManager::Instance();

    // Clear any stale entries for this game first
    if (gam.IsGameLoaded(comboGame)) {
        gam.UnloadGame(comboGame);
    }

    // Register all currently loaded archives as belonging to this game
    auto archives = archiveMgr->GetArchives();
    if (archives) {
        for (const auto& archive : *archives) {
            gam.RegisterArchive(comboGame, archive);
        }
        fprintf(stderr, "[ARCHIVES] OnGameInit(%s): Registered %zu archives\n",
                Game_ToString(game), archives->size());
    }
}

/**
 * Clean up the outgoing game's archive entries from GameArchiveManager.
 */
void Archives_OnGameShutdown(GameId game) {
    Combo::Game comboGame = Combo::FromGameId(game);
    auto& gam = Combo::GameArchiveManager::Instance();

    if (gam.IsGameLoaded(comboGame)) {
        // Don't pass ArchiveManager here — Ship::Context handles its own
        // archive cleanup during shutdown. We just clean our tracking.
        gam.UnloadGame(comboGame);
        fprintf(stderr, "[ARCHIVES] OnGameShutdown(%s): Unloaded game archives\n",
                Game_ToString(game));
    }
}

/**
 * Handle the transition between games.
 * Ensures the GameArchiveManager is clean for the incoming game.
 */
void Archives_OnGameSwitch(GameId fromGame, GameId toGame) {
    fprintf(stderr, "[ARCHIVES] Game switch: %s -> %s\n",
            Game_ToString(fromGame), Game_ToString(toGame));

    // The outgoing game's archives will be cleaned up by Archives_OnGameShutdown
    // (called before Game_Shutdown). The incoming game will register its archives
    // via Archives_OnGameInit (called after Game_Init).
    //
    // Clear all tracked archives to ensure a clean slate.
    // This handles edge cases where shutdown didn't clean up properly.
    Combo::GameArchiveManager::Instance().Clear();

    fprintf(stderr, "[ARCHIVES] GameArchiveManager cleared for fresh start\n");
}
