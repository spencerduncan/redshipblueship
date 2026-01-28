/**
 * @file game_lifecycle.cpp
 * @brief GameOps / GameRunner implementation
 */

#include "game_lifecycle.h"
#include <cstdio>

// Namespaced game functions provided by the OoT and MM object libraries
extern "C" {
    int  OoT_Game_Init(int argc, char** argv);
    void OoT_Game_Run(void);
    void OoT_Game_Shutdown(void);
    void OoT_Game_Pause(void);
    void OoT_Game_Resume(void);

    int  MM_Game_Init(int argc, char** argv);
    void MM_Game_Run(void);
    void MM_Game_Shutdown(void);
    void MM_Game_Pause(void);
    void MM_Game_Resume(void);
}

// ============================================================================
// Static GameOps tables
// ============================================================================

static const GameOps sOoTOps = {
    OoT_Game_Init,
    OoT_Game_Run,
    OoT_Game_Pause,
    OoT_Game_Resume,
    OoT_Game_Shutdown,
};

static const GameOps sMMOps = {
    MM_Game_Init,
    MM_Game_Run,
    MM_Game_Pause,
    MM_Game_Resume,
    MM_Game_Shutdown,
};

// ============================================================================
// GameOps accessors
// ============================================================================

const GameOps* OoT_GetGameOps(void) { return &sOoTOps; }
const GameOps* MM_GetGameOps(void)  { return &sMMOps;  }

const GameOps* GameRunner_GetOps(GameId game) {
    switch (game) {
        case GAME_OOT: return &sOoTOps;
        case GAME_MM:  return &sMMOps;
        default:       return nullptr;
    }
}

// ============================================================================
// GameRunner API
// ============================================================================

static GameId sActiveGame = GAME_NONE;

int GameRunner_InitGame(GameId game, int argc, char** argv) {
    const GameOps* ops = GameRunner_GetOps(game);
    if (!ops) {
        fprintf(stderr, "Error: Invalid game ID %d\n", game);
        return -1;
    }
    printf("Initializing %s...\n", (game == GAME_OOT) ? "Ocarina of Time" : "Majora's Mask");
    int rc = ops->init(argc, argv);
    if (rc == 0) {
        sActiveGame = game;
    }
    return rc;
}

void GameRunner_RunGame(GameId game) {
    const GameOps* ops = GameRunner_GetOps(game);
    if (ops) {
        ops->run();
    }
}

int GameRunner_StartGame(GameId game, int argc, char** argv) {
    int rc = GameRunner_InitGame(game, argc, argv);
    if (rc != 0) return rc;
    GameRunner_RunGame(game);
    return 0;
}

void GameRunner_SwitchTo(GameId game) {
    // Shut down current game if different
    if (sActiveGame != GAME_NONE && sActiveGame != game) {
        GameRunner_ShutdownGame(sActiveGame);
    }
    sActiveGame = game;
}

void GameRunner_ShutdownGame(GameId game) {
    const GameOps* ops = GameRunner_GetOps(game);
    if (ops) {
        ops->shutdown();
    }
    if (sActiveGame == game) {
        sActiveGame = GAME_NONE;
    }
}

void GameRunner_ShutdownAll(void) {
    if (sActiveGame != GAME_NONE) {
        GameRunner_ShutdownGame(sActiveGame);
    }
}
