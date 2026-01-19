/**
 * Unified Entry Point for RedShipBlueShip
 *
 * This is the main entry point for the unified build that contains both
 * OoT and MM in a single executable. After symbol namespacing (issue #29),
 * both games can coexist without symbol collision.
 *
 * The launcher can start either game and switch between them at runtime.
 */

#include "combo/ComboContext.h"
#include "combo/GameAPI.h"
#include "combo/SharedGraphics.h"
#include <libultraship/bridge.h>
#include <ship/Context.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// =============================================================================
// Game Function Declarations
// =============================================================================
// These functions are provided by the game object libraries when linked.
// For stub builds (without game code), weak symbols provide default no-ops.

#ifdef __GNUC__
#define WEAK_SYMBOL __attribute__((weak))
#else
#define WEAK_SYMBOL
#endif

// OoT functions (from games/oot)
extern "C" {
    WEAK_SYMBOL void OoT_GameConsole_Init(void) {
        fprintf(stderr, "[STUB] OoT_GameConsole_Init not linked\n");
    }
    WEAK_SYMBOL void OoT_InitOTR(int argc, char* argv[]) {
        fprintf(stderr, "[STUB] OoT_InitOTR not linked\n");
    }
    WEAK_SYMBOL void OoT_DeinitOTR(void) {
        fprintf(stderr, "[STUB] OoT_DeinitOTR not linked\n");
    }
    WEAK_SYMBOL void OoT_Heaps_Alloc(void) {
        fprintf(stderr, "[STUB] OoT_Heaps_Alloc not linked\n");
    }
    WEAK_SYMBOL void OoT_Heaps_Free(void) {
        fprintf(stderr, "[STUB] OoT_Heaps_Free not linked\n");
    }
    WEAK_SYMBOL void OoT_Main(void* arg) {
        fprintf(stderr, "[STUB] OoT_Main not linked - exiting\n");
    }
    WEAK_SYMBOL void OoT_BootCommands_Init(void) {
        fprintf(stderr, "[STUB] OoT_BootCommands_Init not linked\n");
    }
}

// MM functions (from games/mm)
extern "C" {
    WEAK_SYMBOL void MM_InitBen(int argc, char* argv[]) {
        fprintf(stderr, "[STUB] MM_InitBen not linked\n");
    }
    WEAK_SYMBOL void MM_DeinitBen(void) {
        fprintf(stderr, "[STUB] MM_DeinitBen not linked\n");
    }
    WEAK_SYMBOL void MM_Main(void* arg) {
        fprintf(stderr, "[STUB] MM_Main not linked - exiting\n");
    }
}

// Global combo context
Combo::ComboContext gComboCtx;
Combo::Game gCurrentGame = Combo::Game::None;

// Command line state for re-init
static int sArgc = 0;
static char** sArgv = nullptr;

/**
 * Initialize both games' static data structures.
 * This allows fast switching without full re-initialization.
 */
static void InitBothGames(int argc, char** argv) {
    sArgc = argc;
    sArgv = argv;

    fprintf(stderr, "[UNIFIED] Initializing OoT subsystems...\n");
    OoT_GameConsole_Init();
    OoT_InitOTR(argc, argv);
    OoT_BootCommands_Init();
    OoT_Heaps_Alloc();

    // TODO: Initialize MM subsystems
    // MM_InitBen(argc, argv);

    fprintf(stderr, "[UNIFIED] Both games initialized\n");
}

/**
 * Run the main game loop for the current game.
 */
static void RunCurrentGame() {
    if (gCurrentGame == Combo::Game::OoT) {
        fprintf(stderr, "[UNIFIED] Starting OoT main loop\n");
        OoT_Main(nullptr);
    } else if (gCurrentGame == Combo::Game::MM) {
        fprintf(stderr, "[UNIFIED] Starting MM main loop\n");
        MM_Main(nullptr);
    }
}

/**
 * Process a game switch request.
 */
static void ProcessGameSwitch() {
    if (!gComboCtx.switchRequested) {
        return;
    }

    fprintf(stderr, "[UNIFIED] Processing switch: %s -> %s\n",
            gCurrentGame == Combo::Game::OoT ? "OoT" : "MM",
            gComboCtx.targetGame == Combo::Game::OoT ? "OoT" : "MM");

    gCurrentGame = gComboCtx.targetGame;
    gComboCtx.switchRequested = false;
}

/**
 * Parse command line arguments.
 */
static Combo::Game ParseStartGame(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "mm") == 0) {
                return Combo::Game::MM;
            }
        }
        if (strcmp(argv[i], "--mm") == 0) {
            return Combo::Game::MM;
        }
    }
    // Default to OoT
    return Combo::Game::OoT;
}

/**
 * Handle --test flag for automated testing.
 */
static bool HandleTestMode(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0 && i + 1 < argc) {
            const char* testName = argv[i + 1];

            // Set SDL to use dummy drivers for headless operation
            setenv("SDL_AUDIODRIVER", "dummy", 1);
            setenv("SDL_VIDEODRIVER", "dummy", 1);

            // TODO: Implement actual test logic using TestRunner
            fprintf(stderr, "[UNIFIED] Test mode: %s\n", testName);

            if (strcmp(testName, "boot-oot") == 0) {
                gCurrentGame = Combo::Game::OoT;
                return true;
            } else if (strcmp(testName, "boot-mm") == 0) {
                gCurrentGame = Combo::Game::MM;
                return true;
            }
            // Other tests handled by TestRunner
            return true;
        }
    }
    return false;
}

int main(int argc, char** argv) {
    fprintf(stderr, "[UNIFIED] RedShipBlueShip unified build starting\n");

    // Initialize combo context
    gComboCtx = Combo::ComboContext{};

    // Check for test mode
    bool testMode = HandleTestMode(argc, argv);

    // Determine starting game
    if (gCurrentGame == Combo::Game::None) {
        gCurrentGame = ParseStartGame(argc, argv);
    }

    fprintf(stderr, "[UNIFIED] Starting game: %s\n",
            gCurrentGame == Combo::Game::OoT ? "OoT" : "MM");

    // Initialize shared graphics (creates window, GL context)
    // SharedGraphics_Init(argc, argv);

    // Initialize both games' static data
    InitBothGames(argc, argv);

    // Main game loop
    bool running = true;
    while (running) {
        RunCurrentGame();

        // Check for switch request
        if (gComboCtx.switchRequested) {
            ProcessGameSwitch();
        } else {
            // Game exited normally
            running = false;
        }
    }

    // Cleanup
    fprintf(stderr, "[UNIFIED] Shutting down...\n");
    OoT_DeinitOTR();
    OoT_Heaps_Free();
    // MM_DeinitBen();

    return 0;
}
