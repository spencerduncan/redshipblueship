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
#include <io.h>
#include <ship/debug/CrashHandler.h>

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* exceptionInfo) {
    fprintf(stderr, "\n[CRASH] Windows exception: 0x%08X\n",
            exceptionInfo->ExceptionRecord->ExceptionCode);
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void InstallCrashHandler(void) {
    SetUnhandledExceptionFilter(CrashHandler);
}

// Map the fatal Windows exception codes to the POSIX signal numbers the
// integration tier documents (docs/ci-gameplay-repro-postmortem.md §7:
// exit >= 128 means "crashed", signal = code - 128), so CI and the agent
// loop read Windows crashes the same way they read Linux ones.
static int MapExceptionToSignal(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_STACK_OVERFLOW:
            return 11; // SIGSEGV
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_PRIV_INSTRUCTION:
            return 4; // SIGILL
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_OVERFLOW:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_INVALID_OPERATION:
        case EXCEPTION_FLT_OVERFLOW:
        case EXCEPTION_FLT_UNDERFLOW:
        case EXCEPTION_FLT_INEXACT_RESULT:
        case EXCEPTION_FLT_DENORMAL_OPERAND:
        case EXCEPTION_FLT_STACK_CHECK:
            return 8; // SIGFPE
        default:
            return 6; // SIGABRT-equivalent bucket
    }
}

/**
 * Integration-mode unhandled-exception filter (Windows).
 *
 * libultraship's CrashHandler constructor takes over the process filter
 * during shared bring-up, and its seh_filter ends in a MODAL MessageBoxA —
 * on an unattended runner a crashed test hangs until the CTest timeout,
 * and the register/backtrace dump is lost if nobody clicks. In integration
 * mode we take the filter back after game init and do what the POSIX side
 * documents:
 *   1. raw _write of the exception code to stderr (stdio can be unusable
 *      in a crash context — this breadcrumb must always escape),
 *   2. best-effort gameplay phase dump,
 *   3. libultraship's own PrintStack (identical registers + traceback dump
 *      into the rotating log, including the flush) minus the MessageBox,
 *   4. TerminateProcess(128 + signal).
 * Release (non-test) behavior is unchanged: this filter is only installed
 * when TestRunner_IsIntegrationTestMode().
 */
static LONG WINAPI IntegrationCrashFilter(EXCEPTION_POINTERS* exceptionInfo) {
    DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "\n[CRASH] Windows exception: 0x%08lX (integration mode)\n",
                     (unsigned long)code);
    if (n > 0) {
        _write(2, buf, (unsigned int)n);
    }

    // ASLR-independent fault location: exe-relative offset survives across
    // runs and resolves to a function via the linker map (redship.map), even
    // without PDBs. For access violations also say read-vs-write and the
    // faulting address — a small faulting address is a near-NULL field deref
    // and its value is the field offset.
    {
        uintptr_t rip = (uintptr_t)exceptionInfo->ExceptionRecord->ExceptionAddress;
        uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
        n = snprintf(buf, sizeof(buf), "[CRASH] addr=%p exe_base=%p exe_offset=0x%zX\n", (void*)rip, (void*)base,
                     (size_t)(rip >= base ? rip - base : rip));
        if (n > 0) {
            _write(2, buf, (unsigned int)n);
        }
        if (code == EXCEPTION_ACCESS_VIOLATION && exceptionInfo->ExceptionRecord->NumberParameters >= 2) {
            const ULONG_PTR* info = exceptionInfo->ExceptionRecord->ExceptionInformation;
            n = snprintf(buf, sizeof(buf), "[CRASH] access violation: %s at 0x%zX\n",
                         info[0] == 1 ? "WRITE" : (info[0] == 8 ? "EXEC" : "READ"), (size_t)info[1]);
            if (n > 0) {
                _write(2, buf, (unsigned int)n);
            }
        }
    }

    if (IntegrationTest_GetMode() == INT_TEST_GAMEPLAY_ROUNDTRIP) {
        IntegrationTest_LogGameplayState("signal");
    }

    // Same dump path libultraship's own filter uses (registers + traceback
    // appended as a CRITICAL record to the log, then flushed) — just without
    // the modal MessageBox that would hang an unattended run.
    auto ctx = Ship::Context::GetInstance();
    if (ctx != nullptr && ctx->GetCrashHandler() != nullptr) {
        ctx->GetCrashHandler()->PrintStack(exceptionInfo->ContextRecord);
    }

    TerminateProcess(GetCurrentProcess(), (UINT)(128 + MapExceptionToSignal(code)));
    return EXCEPTION_EXECUTE_HANDLER; // unreachable
}
#endif

/**
 * Re-take the crash handling after game init. libultraship's CrashHandler
 * constructor (shared Ship::Context bring-up) replaces whatever filter main()
 * installed at startup; in integration mode we need the unattended-safe
 * filter above to be the active one. No-op outside integration mode and on
 * POSIX (there libultraship's sigaction handlers already write the dump and
 * exit non-zero without human interaction, which is all the tier needs).
 */
static void ReinstallCrashHandlerForIntegration(void) {
#ifdef _WIN32
    if (TestRunner_IsIntegrationTestMode()) {
        SetUnhandledExceptionFilter(IntegrationCrashFilter);
    }
#endif
}

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
        // Fast-exit with the verdict: the chimera process heap-corrupts during
        // normal CRT teardown (0xC0000374 in the onexit static-destructor
        // chain — reproducible with just `redship --version` on Windows),
        // which would clobber the test's exit code AFTER the result was
        // decided. _Exit skips teardown so the verdict survives. This does
        // NOT hide the corruption: release runs still exit normally, and the
        // fault is tracked separately (teardown heap corruption, fault A in
        // docs/ci-gameplay-repro-postmortem.md follow-ups).
        int testResult = TestRunner_Run(testArg);
        fflush(stdout);
        fflush(stderr);
        _Exit(testResult);
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

    // Game init created the shared Ship::Context, whose CrashHandler stole
    // the process exception filter (and on Windows ends crashes in a modal
    // MessageBox). In integration mode, take it back so a crashed test dumps
    // and exits instead of hanging an unattended runner.
    ReinstallCrashHandlerForIntegration();

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

    // Return integration test result if in integration test mode.
    // Fast-exit for the same reason as the --test path above: normal CRT
    // teardown heap-corrupts (0xC0000374) and would overwrite the verdict
    // this run just produced. Games were shut down cleanly above; only the
    // static-destructor chain is skipped.
    if (TestRunner_IsIntegrationTestMode()) {
        int integrationResult = TestRunner_GetIntegrationTestResult();
        fflush(stdout);
        fflush(stderr);
        _Exit(integrationResult);
    }

    printf("Game exited normally.\n");
    return 0;
}
