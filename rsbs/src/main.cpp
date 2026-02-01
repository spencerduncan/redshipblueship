/**
 * @file main.cpp
 * @brief RedShip unified executable entry point
 *
 * Single executable architecture for running OoT and MM.
 * Both games are compiled as object libraries with namespaced symbols
 * (OoT_* and MM_*) and linked into this single binary.
 *
 * Uses GameRunner (composable lifecycle) to manage game transitions.
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
#include <filesystem>
#include <string>

#include "game.h"
#include "game_lifecycle.h"
#include "context.h"
#include "entrance.h"
#include "test_runner.h"
#include "integration_test_hooks.h"

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

// ============================================================================
// Resource archive hot-swap for cross-game switches (issue #154)
// ============================================================================

static void EnsureGameArchivesLoaded(GameId targetGame) {
    auto ctx = Ship::Context::GetInstance();
    if (!ctx) return;
    auto archiveManager = ctx->GetResourceManager()->GetArchiveManager();
    if (!archiveManager) return;

    struct ArchiveEntry { const char* filename; bool useLocate; };
    const char* appName = nullptr;
    std::vector<ArchiveEntry> entries;

    switch (targetGame) {
        case GAME_OOT:
            appName = "soh";
            entries = {{"oot.o2r", true}, {"oot-mq.o2r", true}, {"soh.o2r", false}};
            break;
        case GAME_MM:
            appName = "2s2h";
            entries = {{"mm.o2r", true}, {"2ship.o2r", false}};
            break;
        default:
            return;
    }

    for (const auto& entry : entries) {
        std::string path = entry.useLocate
            ? Ship::Context::LocateFileAcrossAppDirs(entry.filename, appName)
            : Ship::Context::GetPathRelativeToAppBundle(entry.filename);

        if (path.empty() || !std::filesystem::exists(path)) continue;

        auto archive = archiveManager->AddArchive(path);
        if (archive) {
            printf("[RSBS] Archive ready: %s\n", path.c_str());
        } else {
            fprintf(stderr, "[RSBS] Warning: Failed to load archive: %s\n", path.c_str());
        }
    }
}

// ============================================================================
// Forward declarations for game ops providers
// ============================================================================

extern "C" {
    GameOps* OoT_GetGameOps(void);
    GameOps* MM_GetGameOps(void);
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
    printf("  --game oot            Run Ocarina of Time\n");
    printf("  --game mm             Run Majora's Mask\n");
    printf("  --test <name>         Run unit tests\n");
    printf("  --integration-test <name>  Run integration tests (boots game)\n");
    printf("  --test-entrance       Use test entrance links (Mido's House)\n");
    printf("  --help, -h            Show this help message\n\n");
    printf("Unit test commands (--test):\n");
    printf("  boot-oot        Infrastructure test for OoT boot\n");
    printf("  boot-mm         Infrastructure test for MM boot\n");
    printf("  switch-oot-mm   Test OoT -> MM entrance switching\n");
    printf("  switch-mm-oot   Test MM -> OoT entrance switching\n");
    printf("  all             Run all unit tests\n");
    printf("  list            List all available tests\n\n");
    printf("Integration test commands (--integration-test):\n");
    printf("  int-boot-oot    Actually boot OoT and verify title screen\n");
    printf("  int-boot-mm     Actually boot MM and verify title screen\n\n");
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

    // Check for unit test mode (--test)
    const char* testArg = TestRunner_ParseArgs(argc, argv);
    if (testArg != nullptr) {
        return TestRunner_Run(testArg);
    }

    // Check for integration test mode (--integration-test)
    const char* integrationTestArg = TestRunner_ParseIntegrationArgs(argc, argv);
    if (integrationTestArg != nullptr) {
        if (!TestRunner_SetupIntegrationTest(integrationTestArg)) {
            return 1; // Test not found
        }
        // Continue to game initialization - integration tests actually run the game
        printf("[INT-TEST] Integration test mode - will boot game with hooks\n");
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

    // ========================================================================
    // Create Ship::Context singleton early - before any game init (issue #184)
    // This ensures the singleton exists before OTRGlobals can interfere.
    // Games will detect the existing context and reuse it.
    // ========================================================================
    fprintf(stderr, "[RSBS] About to create Ship::Context singleton...\n");
    fflush(stderr);
    auto shipContext = Ship::Context::CreateUninitializedInstance(
        "RedShip", "redship", "redship.json");
    fprintf(stderr, "[RSBS] CreateUninitializedInstance returned: %p\n", (void*)shipContext.get());
    fflush(stderr);
    if (!shipContext) {
        fprintf(stderr, "[RSBS] FATAL: Failed to create Ship::Context singleton\n");
        return 1;
    }
    fprintf(stderr, "[RSBS] Ship::Context singleton created successfully at %p\n", (void*)shipContext.get());
    fflush(stderr);

    // ========================================================================
    // Set up GameRunner with composable lifecycle
    // ========================================================================

    fprintf(stderr, "[RSBS] Creating GameRunner...\n");
    fflush(stderr);
    GameRunner runner;
    GameRunner_Init(&runner);
    fprintf(stderr, "[RSBS] Registering OoT...\n");
    fflush(stderr);
    GameRunner_RegisterGame(&runner, GAME_OOT, OoT_GetGameOps());
    fprintf(stderr, "[RSBS] Registering MM...\n");
    fflush(stderr);
    GameRunner_RegisterGame(&runner, GAME_MM, MM_GetGameOps());
    fprintf(stderr, "[RSBS] Games registered\n");
    fflush(stderr);

    // Determine which game to run
    GameId selectedGame = GAME_NONE;

    // Integration tests override the game selection
    if (TestRunner_IsIntegrationTestMode()) {
        selectedGame = TestRunner_GetIntegrationTestGame();
        printf("[INT-TEST] Using game from integration test: %s\n",
               Game_ToString(selectedGame));
    } else {
        selectedGame = ParseGameArg(argc, argv);
        if (selectedGame == GAME_NONE) {
            selectedGame = ShowGameMenu();
        }
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
        if (strcmp(argv[i], "--integration-test") == 0) {
            i++; // Skip value too
            continue;
        }
        if (strncmp(argv[i], "--integration-test=", 19) == 0) {
            continue;
        }
        gameArgv[gameArgc++] = argv[i];
    }
    gameArgv[gameArgc] = nullptr;

    // ========================================================================
    // Main game loop with hot-swap support via GameRunner
    // ========================================================================

    bool keepRunning = true;

    // Start the first game
    Combo_ClearGameSwitchRequest();
    Entrance_ClearPendingSwitch();

    GameOps* ops = GameRunner_GetOps(&runner, selectedGame);
    printf("Initializing %s...\n", ops ? ops->name : "Unknown");

    int initResult = GameRunner_StartGame(&runner, selectedGame, gameArgc, gameArgv);
    if (initResult != 0) {
        fprintf(stderr, "Error: Failed to initialize game (code %d)\n", initResult);
        free(gameArgv);
        return 1;
    }

    // Register integration test hooks after game is initialized
    if (TestRunner_IsIntegrationTestMode()) {
        IntegrationTest_RegisterHooks(selectedGame);
    }

    while (keepRunning) {
        // Run the active game
        ops = GameRunner_GetOps(&runner, GameRunner_GetActive(&runner));
        printf("Starting %s... (Press F10 to switch games)\n", ops ? ops->name : "Unknown");
        if (ops && ops->run) {
            ops->run();
        }

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
            nextGame = Game_GetOther(GameRunner_GetActive(&runner));
        }

        // Handle the switch
        if (nextGame != GAME_NONE) {
            GameOps* nextOps = GameRunner_GetOps(&runner, nextGame);
            printf("\n=== Switching to %s ===\n",
                   nextOps ? nextOps->name : "Unknown");

            // Clear switch state before transitioning
            Combo_ClearGameSwitchRequest();
            Entrance_ClearPendingSwitch();

            // Set startup entrance if this is an entrance-based switch
            if (isEntranceSwitch && targetEntrance != 0) {
                Entrance_SetStartupEntrance(targetEntrance);
            }

            // Hot-swap resource archives before game init/resume
            EnsureGameArchivesLoaded(nextGame);

            // GameRunner handles suspend/resume/init lifecycle
            int switchResult = GameRunner_SwitchTo(&runner, nextGame, gameArgc, gameArgv);
            if (switchResult != 0) {
                fprintf(stderr, "Error: Failed to switch to game (code %d)\n", switchResult);
                keepRunning = false;
            }
        } else {
            // Normal exit
            keepRunning = false;
        }
    }

    // Final cleanup — shutdown all games
    GameRunner_ShutdownAll(&runner);

    free(gameArgv);

    // Return integration test result if in integration test mode
    if (TestRunner_IsIntegrationTestMode()) {
        return TestRunner_GetIntegrationTestResult();
    }

    printf("Game exited normally.\n");
    return 0;
}
