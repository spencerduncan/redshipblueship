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
 *   redship               # Run Ocarina of Time (default)
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
#include "rsbs_version.h"
#include "test_runner.h"
#include "integration_test_hooks.h"
#include "archive_check.h"

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

// ============================================================================
// Resource archive hot-swap for cross-game switches (issue #154)
// ============================================================================

static void EnsureGameArchivesLoaded(GameId targetGame) {
    auto ctx = Ship::Context::GetInstance();
    if (!ctx || !ctx->GetResourceManager()) return;
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
    // Whether the shared bring-up found an OoT game archive and ran OoT's
    // asset-dependent init (games/oot/soh/OTRGlobals.cpp, #330).
    int OoT_GameAssetsInitialized(void);
    // In-app mm.o2r generation, offered at the start prompt when MM is
    // selected without a game archive (#317). Defined in
    // games/mm/2s2h/GameExports_SingleExe.cpp. Success is observed by
    // re-checking archive availability, not via a return value.
    void MM_Extract_OfferAndRun(void);
    // Build-version strings baked into each game library
    // (games/*/src/boot/build.c); declarations match each game's own header.
    extern const char OoT_gBuildVersion[];
    extern char MM_gBuildVersion[];
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
    // Integration-test diagnostics only (release behavior unchanged): dump
    // the gameplay phase machine so a crash log is attributable to a repro
    // step even when it fires before libultraship's CrashHandler takes over
    // the signals, and exit 128+sig so CI can tell "crashed" from "test
    // failed". (Best effort — like the fprintf above, not strictly
    // async-signal-safe, but we are exiting anyway.)
    if (TestRunner_IsIntegrationTestMode()) {
        if (IntegrationTest_GetMode() == INT_TEST_GAMEPLAY_ROUNDTRIP) {
            IntegrationTest_LogGameplayState("signal");
        }
        _Exit(128 + signal);
    }
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

bool HasVersionFlag(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            return true;
        }
    }
    return false;
}

void PrintVersion(void) {
    // Base-port versions come from the linked game libraries so they can't
    // drift from what the archive/save compatibility checks actually enforce.
    printf("%s %s (%s)\n", RSBS_APP_NAME, RSBS_VERSION_STRING, RSBS_VERSION_CODENAME);
    printf("Base ports: Ship of Harkinian %s (OoT), 2 Ship 2 Harkinian %s (MM)\n",
           (const char*)OoT_gBuildVersion, (const char*)MM_gBuildVersion);
}

void PrintUsage(const char* progName) {
    printf("%s %s - Unified OoT+MM Executable\n\n", RSBS_APP_NAME, RSBS_VERSION_STRING);
    printf("Usage: %s [OPTIONS]\n\n", progName);
    printf("Options:\n");
    printf("  --game oot            Run Ocarina of Time\n");
    printf("  --game mm             Run Majora's Mask\n");
    printf("  --test <name>         Run unit tests\n");
    printf("  --integration-test <name>  Run integration tests (boots game)\n");
    printf("  --test-entrance       Use test entrance links (Mido's House)\n");
    printf("  --version             Show version information\n");
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

} // anonymous namespace

// ============================================================================
// Main entry point
// ============================================================================

int main(int argc, char** argv) {
    // Install crash handler
    InstallCrashHandler();

    // Check for version flag
    if (HasVersionFlag(argc, argv)) {
        PrintVersion();
        return 0;
    }

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
    // Create Ship::Context singleton up front — before either game's Init
    // runs (issue #184, #271).
    //
    // The singleton is created UNINITIALIZED; the first game to boot runs
    // the shared subsystem bring-up on it (issues #329/#330): OoT through
    // OoT_Game_Init -> InitOTR (OTRGlobals adopts the singleton and
    // initializes it), MM through MM_Game_Init -> InitOTRForMMFirstBoot.
    // Creating the placeholder here means neither game depends on the
    // *other* having booted first — i.e. there is no "OoT must run before
    // MM" implicit ordering. The user can launch either `redship --game oot`
    // or `redship --game mm` cold, and the selected game's Init layers its
    // own factories/heaps onto the shared context.
    //
    // The display name (first argument) is RSBS's own identity so the window
    // title, crash dialog, and log name attribute tester reports to this
    // build instead of upstream SoH (issue #319). The app short name ("soh")
    // and config file ("shipofharkinian.json") are kept stable on purpose:
    // they pin the config and savestate directory to one location across
    // game switches. Re-keying these on the selected game would split a
    // single user's saves into two app-data folders the first time they
    // switched games. Asymmetric branding != asymmetric bootstrap; the
    // bootstrap itself is now game-agnostic.
    // ========================================================================
    fprintf(stderr, "[RSBS] About to create Ship::Context singleton...\n");
    fflush(stderr);
    auto shipContext = Ship::Context::CreateUninitializedInstance(
        RSBS_WINDOW_TITLE, "soh", "shipofharkinian.json");
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

    // Integration tests override the game selection. Each integration test
    // boots its target game directly — no implicit OoT-first dance for MM
    // tests (#271). The harness has already created Ship::Context above, so
    // either game can be the first to initialize libultraship factories
    // and resources on top of it.
    if (TestRunner_IsIntegrationTestMode()) {
        selectedGame = TestRunner_GetIntegrationTestGame();
        printf("[INT-TEST] Using game from integration test: %s\n",
               Game_ToString(selectedGame));
    } else {
        selectedGame = ParseGameArg(argc, argv);
        if (selectedGame == GAME_NONE) {
            // No --game argument: boot straight into Ocarina of Time (landing
            // on its title/file-select menu) instead of prompting the user to
            // pick a game. MM is still reachable via `--game mm` or by
            // switching in-game (Happy Mask Shop <-> Clock Tower, or F10).
            selectedGame = GAME_OOT;
        }
    }

    // ========================================================================
    // Pre-flight archive check (#317): verify the selected game's ROM-derived
    // archives exist before any init. OoT can self-heal during its own init —
    // its in-app extractor (RunExtract) generates oot.o2r from a ROM. MM's
    // init has no equivalent, so the harness offers MM's extractor here
    // instead: it runs before any window exists (parentless SDL dialogs plus
    // a native file picker), generates mm.o2r from the user's ROM via the
    // bundled ZAPD_MM, and lets the boot continue. Integration tests stay
    // non-interactive: there a missing archive remains a hard failure.
    // Without this gate, a missing mm.o2r surfaces as a bare "Failed to
    // initialize game" on the console, which a GUI-launched user never sees.
    // ========================================================================
    if (selectedGame == GAME_MM && !ArchiveCheck_GameAvailable(GAME_MM)) {
        bool interactive = !TestRunner_IsIntegrationTestMode();
        bool available = false;
        if (interactive) {
            MM_Extract_OfferAndRun();
            available = ArchiveCheck_GameAvailable(GAME_MM);
        }
        if (!available) {
            ArchiveCheck_ReportMissing(GAME_MM, /*showDialog=*/interactive);
            return 1;
        }
    }

    // MM also needs its port-asset archive (2ship.o2r), which ships with the
    // package and cannot be generated from a ROM. Without it MM's early boot
    // dereferences null resources (fonts, boot-logo textures), so refuse up
    // front with an "incomplete install" message instead of crashing later.
    if (selectedGame == GAME_MM && !ArchiveCheck_PortArchiveAvailable(GAME_MM)) {
        ArchiveCheck_ReportMissingPortArchive(GAME_MM,
                                              /*showDialog=*/!TestRunner_IsIntegrationTestMode());
        return 1;
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
    // Keep context's view of the current game in sync with the runner so that
    // cross-game entrance hooks dispatch against the right game (issue #170).
    Context_SetCurrentGame(selectedGame);

    // Note: the previous MM-test detour (init OoT then SwitchTo MM) was
    // removed with #271 — MM integration tests now boot MM directly above.

    while (keepRunning) {
        // Run the active game
        ops = GameRunner_GetOps(&runner, GameRunner_GetActive(&runner));
        printf("Starting %s... (Press F10 to switch games)\n", ops ? ops->name : "Unknown");
        if (ops && ops->run) {
            ops->run();
        }

        // Integration test exit takes priority over game switching
        if (TestRunner_IsIntegrationTestMode() && IntegrationTest_ExitRequested()) {
            keepRunning = false;
            break;
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
            // Refuse the switch up front if the target game's ROM archives are
            // missing (#317). GameRunner_SwitchTo suspends the current game
            // before initializing the target, so a late init failure would
            // take down the whole app. Checking here keeps the current game
            // running: clear the pending switch, tell the user what's missing,
            // and loop back into the active game's run loop.
            if (!ArchiveCheck_GameAvailable(nextGame)) {
                ArchiveCheck_ReportMissing(nextGame, /*showDialog=*/!TestRunner_IsIntegrationTestMode());
                Combo_ClearGameSwitchRequest();
                Entrance_ClearPendingSwitch();
                printf("Switch to %s refused (missing game archive) — continuing %s.\n",
                       Game_ToString(nextGame), Game_ToString(GameRunner_GetActive(&runner)));
                continue;
            }

            // Same up-front refusal for the target game's port-asset archive
            // (2ship.o2r for MM): a switch into MM without it would fail in
            // MM_Game_Init and take down the whole app, or worse, boot into
            // null-resource crashes. Keep the current game running instead.
            if (!ArchiveCheck_PortArchiveAvailable(nextGame)) {
                ArchiveCheck_ReportMissingPortArchive(nextGame,
                                                      /*showDialog=*/!TestRunner_IsIntegrationTestMode());
                Combo_ClearGameSwitchRequest();
                Entrance_ClearPendingSwitch();
                printf("Switch to %s refused (missing port-asset archive) — continuing %s.\n",
                       Game_ToString(nextGame), Game_ToString(GameRunner_GetActive(&runner)));
                continue;
            }

            // The shared bring-up skips OoT's asset-dependent init (GUI, message
            // tables, item icons) when no OoT game archive was loaded at boot
            // (#330). If oot.o2r appeared on disk afterwards, the archive check
            // above passes but OoT would enter half-initialized — refuse and
            // ask for a restart instead.
            if (nextGame == GAME_OOT && !OoT_GameAssetsInitialized()) {
                Combo_ClearGameSwitchRequest();
                Entrance_ClearPendingSwitch();
                printf("Switch to OoT refused: oot.o2r was not present at startup — "
                       "restart RedShipBlueShip to play Ocarina of Time.\n");
                continue;
            }

            GameOps* nextOps = GameRunner_GetOps(&runner, nextGame);
            printf("\n=== Switching to %s ===\n",
                   nextOps ? nextOps->name : "Unknown");

            // Clear switch state before transitioning
            Combo_ClearGameSwitchRequest();
            Entrance_ClearPendingSwitch();

            // Set startup entrance if this is an entrance-based switch. Tag it
            // with the destination game so only that game can consume it — this
            // prevents an MM entrance (e.g. 0xC010) from leaking into OoT's
            // entranceIndex and reading gEntranceTable out of bounds (crash).
            if (isEntranceSwitch && targetEntrance != 0) {
                Entrance_SetStartupEntrance(targetEntrance, nextGame);
            }

            // Hot-swap resource archives before game init/resume
            EnsureGameArchivesLoaded(nextGame);

            // GameRunner handles suspend/resume/init lifecycle
            int switchResult = GameRunner_SwitchTo(&runner, nextGame, gameArgc, gameArgv);
            if (switchResult != 0) {
                fprintf(stderr, "Error: Failed to switch to game (code %d)\n", switchResult);
                keepRunning = false;
            } else {
                // Track the new active game so cross-game entrance hooks
                // resolve to the correct side on the next round-trip (#170).
                Context_SetCurrentGame(nextGame);
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
