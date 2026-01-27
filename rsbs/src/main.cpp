/**
 * @file main.cpp
 * @brief RedShip unified executable entry point
 *
 * Single executable architecture for running OoT and MM.
 * Both games are compiled as object libraries with namespaced symbols
 * (OoT_* and MM_*) and linked into this single binary.
 *
 * Usage:
 *   redship --game oot    # Run Ocarina of Time
 *   redship --game mm     # Run Majora's Mask
 *   redship --test <name> # Run integration tests
 *   redship               # Show game selector menu
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>

#include "game.h"
#include "context.h"
#include "entrance.h"
#include "test_runner.h"

// ============================================================================
// Forward declarations for namespaced game functions
// These will be provided by the OoT and MM object libraries
// ============================================================================

extern "C" {
    // OoT namespaced functions (from OoT object library)
    int OoT_Game_Init(int argc, char** argv);
    void OoT_Game_Run(void);
    void OoT_Game_Shutdown(void);
    const char* OoT_Game_GetName(void);
    const char* OoT_Game_GetId(void);

    // MM namespaced functions (from MM object library)
    int MM_Game_Init(int argc, char** argv);
    void MM_Game_Run(void);
    void MM_Game_Shutdown(void);
    const char* MM_Game_GetName(void);
    const char* MM_Game_GetId(void);
}

// ============================================================================
// Signal handler for crash diagnostics
// ============================================================================

#ifndef _WIN32
static void SignalHandler(int signal) {
    fprintf(stderr, "\n[CRASH] Signal received: %d\n", signal);
    switch (signal) {
        case SIGSEGV: fprintf(stderr, "[CRASH] SIGSEGV (Segmentation fault)\n"); break;
        case SIGABRT: fprintf(stderr, "[CRASH] SIGABRT (Abort)\n"); break;
        case SIGFPE:  fprintf(stderr, "[CRASH] SIGFPE (Floating point exception)\n"); break;
        case SIGILL:  fprintf(stderr, "[CRASH] SIGILL (Illegal instruction)\n"); break;
    }
    fflush(stderr);
    _Exit(1);
}

static void InstallCrashHandler(void) {
    signal(SIGSEGV, SignalHandler);
    signal(SIGABRT, SignalHandler);
    signal(SIGFPE, SignalHandler);
    signal(SIGILL, SignalHandler);
}
#else
#include <windows.h>

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* exceptionInfo) {
    fprintf(stderr, "\n[CRASH] Windows exception: 0x%08X\n",
            exceptionInfo->ExceptionRecord->ExceptionCode);
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void InstallCrashHandler(void) {
    SetUnhandledExceptionFilter(CrashHandler);
}
#endif

// ============================================================================
// Command line parsing
// ============================================================================

namespace {

GameId ParseGameArg(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            return Game_FromString(argv[i + 1]);
        }
        if (strncmp(argv[i], "--game=", 7) == 0) {
            return Game_FromString(argv[i] + 7);
        }
    }
    return GAME_NONE;
}

bool HasTestEntranceFlag(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test-entrance") == 0) {
            return true;
        }
    }
    return false;
}

bool HasHelpFlag(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            return true;
        }
    }
    return false;
}

void PrintUsage(const char* progName) {
    printf("RedShip - Unified OoT+MM Executable\n\n");
    printf("Usage: %s [OPTIONS]\n\n", progName);
    printf("Options:\n");
    printf("  --game oot       Run Ocarina of Time\n");
    printf("  --game mm        Run Majora's Mask\n");
    printf("  --test <name>    Run integration tests\n");
    printf("  --test-entrance  Use test entrance links (Mido's House)\n");
    printf("  --help, -h       Show this help message\n\n");
    printf("Test commands:\n");
    printf("  --test boot-oot  Boot OoT, exit on main menu\n");
    printf("  --test boot-mm   Boot MM, exit on main menu\n");
    printf("  --test all       Run all tests\n");
    printf("  --test list      List available tests\n\n");
    printf("Hotkeys:\n");
    printf("  F10              Switch between OoT and MM\n");
}

GameId ShowGameMenu(void) {
    printf("\n=== RedShip - Unified OoT+MM ===\n\n");
    printf("Select a game to play:\n");
    printf("  1) Ocarina of Time\n");
    printf("  2) Majora's Mask\n\n");
    printf("Enter choice (1 or 2): ");
    fflush(stdout);

    char input[32];
    if (fgets(input, sizeof(input), stdin) == nullptr) {
        return GAME_OOT;  // Default
    }

    // Remove newline
    input[strcspn(input, "\n")] = 0;

    if (strcmp(input, "1") == 0 || strcmp(input, "oot") == 0) {
        return GAME_OOT;
    }
    if (strcmp(input, "2") == 0 || strcmp(input, "mm") == 0) {
        return GAME_MM;
    }

    printf("Invalid choice. Defaulting to OoT.\n");
    return GAME_OOT;
}

// ============================================================================
// Game dispatch functions
// ============================================================================

int InitGame(GameId game, int argc, char** argv) {
    switch (game) {
        case GAME_OOT:
            printf("Initializing Ocarina of Time...\n");
            return OoT_Game_Init(argc, argv);
        case GAME_MM:
            printf("Initializing Majora's Mask...\n");
            return MM_Game_Init(argc, argv);
        default:
            fprintf(stderr, "Error: Invalid game ID\n");
            return -1;
    }
}

void RunGame(GameId game) {
    switch (game) {
        case GAME_OOT:
            OoT_Game_Run();
            break;
        case GAME_MM:
            MM_Game_Run();
            break;
        default:
            break;
    }
}

void ShutdownGame(GameId game) {
    switch (game) {
        case GAME_OOT:
            OoT_Game_Shutdown();
            break;
        case GAME_MM:
            MM_Game_Shutdown();
            break;
        default:
            break;
    }
}

const char* GetGameName(GameId game) {
    switch (game) {
        case GAME_OOT: return OoT_Game_GetName();
        case GAME_MM: return MM_Game_GetName();
        default: return "Unknown";
    }
}

} // anonymous namespace

// ============================================================================
// Main entry point
// ============================================================================

int main(int argc, char** argv) {
    // Install crash handler
    InstallCrashHandler();

    // Check for help flag
    if (HasHelpFlag(argc, argv)) {
        PrintUsage(argv[0]);
        return 0;
    }

    // Check for test mode
    const char* testArg = TestRunner_ParseArgs(argc, argv);
    if (testArg != nullptr) {
        return TestRunner_Run(testArg);
    }

    // Initialize combo infrastructure
    Context_InitFrozenStates();
    ComboContext_Init();
    Entrance_Init();

    // Register entrance links
    Entrance_RegisterDefaultLinks();
    // Also register test links (Mido's House) for easy testing
    Entrance_RegisterTestLinks();
    if (HasTestEntranceFlag(argc, argv)) {
        printf("Test entrance links also registered (Mido's House <-> Clock Tower)\n");
    }

    // Determine which game to run
    GameId selectedGame = ParseGameArg(argc, argv);
    if (selectedGame == GAME_NONE) {
        selectedGame = ShowGameMenu();
    }

    // Build filtered argv (remove our flags before passing to game)
    char** gameArgv = (char**)malloc(sizeof(char*) * (argc + 1));
    int gameArgc = 0;
    gameArgv[gameArgc++] = argv[0];

    for (int i = 1; i < argc; i++) {
        // Skip our flags
        if (strcmp(argv[i], "--game") == 0) {
            i++; // Skip value too
            continue;
        }
        if (strncmp(argv[i], "--game=", 7) == 0) {
            continue;
        }
        if (strcmp(argv[i], "--test-entrance") == 0) {
            continue;
        }
        if (strcmp(argv[i], "--test") == 0) {
            i++; // Skip value too
            continue;
        }
        if (strncmp(argv[i], "--test=", 7) == 0) {
            continue;
        }
        gameArgv[gameArgc++] = argv[i];
    }
    gameArgv[gameArgc] = nullptr;

    // Main game loop with hot-swap support
    bool keepRunning = true;

    while (keepRunning) {
        // Clear any previous switch requests
        Combo_ClearGameSwitchRequest();
        Entrance_ClearPendingSwitch();

        // Initialize the game
        int initResult = InitGame(selectedGame, gameArgc, gameArgv);
        if (initResult != 0) {
            fprintf(stderr, "Error: Failed to initialize %s (code %d)\n",
                    GetGameName(selectedGame), initResult);
            free(gameArgv);
            return 1;
        }

        // Run the game
        printf("Starting %s... (Press F10 to switch games)\n", GetGameName(selectedGame));
        RunGame(selectedGame);

        // Check if we need to switch games
        GameId nextGame = GAME_NONE;
        uint16_t targetEntrance = 0;
        bool isEntranceSwitch = false;

        if (Entrance_IsCrossGameSwitch()) {
            // Entrance-based cross-game switch
            nextGame = Entrance_GetSwitchTargetGame();
            targetEntrance = Entrance_GetSwitchTargetEntrance();
            isEntranceSwitch = true;
            printf("\n=== Cross-game entrance detected ===\n");
            printf("Target: %s entrance 0x%04X\n",
                   Game_ToString(nextGame), targetEntrance);
        } else if (Combo_IsGameSwitchRequested()) {
            // F10 hotkey switch
            nextGame = Game_GetOther(selectedGame);
        }

        // Handle the switch
        if (nextGame != GAME_NONE) {
            printf("\n=== Switching to %s ===\n", GetGameName(nextGame));

            // Shutdown current game
            ShutdownGame(selectedGame);

            // Set startup entrance if this is an entrance-based switch
            if (isEntranceSwitch && targetEntrance != 0) {
                Entrance_SetStartupEntrance(targetEntrance);
            }

            // Switch to new game
            selectedGame = nextGame;
        } else {
            // Normal exit
            keepRunning = false;
            ShutdownGame(selectedGame);
        }
    }

    free(gameArgv);
    printf("Game exited normally.\n");
    return 0;
}
