/**
 * RedShipBlueShip Combo Launcher
 *
 * Unified launcher for OoT and MM that can run either game by loading
 * them as shared libraries. This provides symbol isolation and enables
 * future hot-switching capabilities.
 *
 * Usage:
 *   redship --game oot    # Run Ocarina of Time
 *   redship --game mm     # Run Majora's Mask
 *   redship               # Show game selector menu (TODO)
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <csignal>
#include <cstdlib>

#include "combo/ComboContextBridge.h"
#include "combo/CrossGameEntrance.h"
#include "combo/FrozenState.h"

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

// Windows crash handler to get stack trace
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* exceptionInfo) {
    std::cerr << "\n[CRASH HANDLER] ========================================" << std::endl;
    std::cerr << "[CRASH HANDLER] EXCEPTION CAUGHT!" << std::endl;

    DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;
    std::cerr << "[CRASH HANDLER] Exception Code: 0x" << std::hex << exceptionCode << std::dec << std::endl;

    switch (exceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            std::cerr << "[CRASH HANDLER] Type: ACCESS VIOLATION" << std::endl;
            if (exceptionInfo->ExceptionRecord->NumberParameters >= 2) {
                ULONG_PTR accessType = exceptionInfo->ExceptionRecord->ExceptionInformation[0];
                ULONG_PTR address = exceptionInfo->ExceptionRecord->ExceptionInformation[1];
                std::cerr << "[CRASH HANDLER] " << (accessType == 0 ? "Read" : "Write")
                          << " access violation at address: 0x" << std::hex << address << std::dec << std::endl;
            }
            break;
        case EXCEPTION_STACK_OVERFLOW:
            std::cerr << "[CRASH HANDLER] Type: STACK OVERFLOW" << std::endl;
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            std::cerr << "[CRASH HANDLER] Type: INTEGER DIVIDE BY ZERO" << std::endl;
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            std::cerr << "[CRASH HANDLER] Type: ILLEGAL INSTRUCTION" << std::endl;
            break;
        default:
            std::cerr << "[CRASH HANDLER] Type: OTHER (code 0x" << std::hex << exceptionCode << std::dec << ")" << std::endl;
            break;
    }

    std::cerr << "[CRASH HANDLER] Exception Address: 0x" << std::hex
              << (ULONG_PTR)exceptionInfo->ExceptionRecord->ExceptionAddress << std::dec << std::endl;

    // Try to get stack trace
    std::cerr << "[CRASH HANDLER] Stack trace:" << std::endl;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    SymInitialize(process, NULL, TRUE);

    CONTEXT* context = exceptionInfo->ContextRecord;
    STACKFRAME64 stackFrame = {};
#ifdef _M_X64
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context->Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context->Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context->Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#else
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context->Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context->Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context->Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#endif

    char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbolBuffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    for (int frame = 0; frame < 30; frame++) {
        if (!StackWalk64(machineType, process, thread, &stackFrame, context,
                         NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
            break;
        }

        DWORD64 address = stackFrame.AddrPC.Offset;
        if (address == 0) break;

        std::cerr << "  [" << frame << "] 0x" << std::hex << address << std::dec;

        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, symbol)) {
            std::cerr << " " << symbol->Name << " + 0x" << std::hex << displacement << std::dec;
        }

        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line)) {
            std::cerr << " (" << line.FileName << ":" << line.LineNumber << ")";
        }

        std::cerr << std::endl;
    }

    SymCleanup(process);
    std::cerr << "[CRASH HANDLER] ========================================" << std::endl;
    std::cerr.flush();

    return EXCEPTION_CONTINUE_SEARCH;
}

void AtExitHandler() {
    std::cerr << "\n[ATEXIT] Process is exiting via exit() or return from main" << std::endl;
    std::cerr.flush();
}

void InstallCrashHandler() {
    SetUnhandledExceptionFilter(CrashHandler);
    std::atexit(AtExitHandler);
}
#else
// Unix signal handler
void SignalHandler(int signal) {
    std::cerr << "\n[CRASH HANDLER] Signal received: " << signal << std::endl;
    switch (signal) {
        case SIGSEGV: std::cerr << "[CRASH HANDLER] SIGSEGV (Segmentation fault)" << std::endl; break;
        case SIGABRT: std::cerr << "[CRASH HANDLER] SIGABRT (Abort)" << std::endl; break;
        case SIGFPE:  std::cerr << "[CRASH HANDLER] SIGFPE (Floating point exception)" << std::endl; break;
        case SIGILL:  std::cerr << "[CRASH HANDLER] SIGILL (Illegal instruction)" << std::endl; break;
    }
    std::cerr.flush();
    std::_Exit(1);
}

void InstallCrashHandler() {
    std::signal(SIGSEGV, SignalHandler);
    std::signal(SIGABRT, SignalHandler);
    std::signal(SIGFPE, SignalHandler);
    std::signal(SIGILL, SignalHandler);
}
#endif

#ifdef _WIN32
#define LIB_SUFFIX ".dll"
#elif defined(__APPLE__)
#define LIB_SUFFIX ".dylib"
#else
#define LIB_SUFFIX ".so"
#endif

namespace {

/**
 * Get the path to a game library relative to the executable
 */
std::string GetLibPath(const char* baseName) {
    // For now, just look in the current directory or a games subdirectory
    // In the future, this could search multiple locations
    std::string name = std::string(baseName) + LIB_SUFFIX;

    // Try games directory first (deployment layout)
    std::string gamesPath = std::string("games/") + baseName + "/" + name;

    // Fall back to same directory
    return name;
}

/**
 * Parse command line arguments to determine which game to run
 */
Combo::Game ParseGameArg(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            return Combo::IdToGame(argv[i + 1]);
        }
        // Also support --game=oot syntax
        if (std::strncmp(argv[i], "--game=", 7) == 0) {
            return Combo::IdToGame(argv[i] + 7);
        }
    }
    return Combo::Game::None;
}

/**
 * Show simple game selection menu
 * TODO: Replace with ImGui selector in Phase 2
 */
Combo::Game ShowGameMenu() {
    std::cout << "\n";
    std::cout << "=== RedShipBlueShip Combo Launcher ===" << std::endl;
    std::cout << "\n";
    std::cout << "Select a game to play:" << std::endl;
    std::cout << "  1) Ocarina of Time" << std::endl;
    std::cout << "  2) Majora's Mask" << std::endl;
    std::cout << "\n";
    std::cout << "Enter choice (1 or 2): ";

    std::string input;
    std::getline(std::cin, input);

    if (input == "1" || input == "oot") {
        return Combo::Game::OoT;
    }
    if (input == "2" || input == "mm") {
        return Combo::Game::MM;
    }

    std::cerr << "Invalid choice. Defaulting to OoT." << std::endl;
    return Combo::Game::OoT;
}

void PrintUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [OPTIONS]" << std::endl;
    std::cout << "\n";
    std::cout << "Options:" << std::endl;
    std::cout << "  --game oot      Run Ocarina of Time" << std::endl;
    std::cout << "  --game mm       Run Majora's Mask" << std::endl;
    std::cout << "  --test-entrance Use Mido's House instead of Happy Mask Shop" << std::endl;
    std::cout << "  --help          Show this help message" << std::endl;
    std::cout << "\n";
    std::cout << "If no game is specified, a menu will be displayed." << std::endl;
    std::cout << "\n";
    std::cout << "Hotkeys:" << std::endl;
    std::cout << "  F10             Switch between OoT and MM" << std::endl;
}

/**
 * Check if --test-entrance flag is present
 */
bool HasTestEntranceFlag(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--test-entrance") == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Get the other game (for switching)
 */
Combo::Game GetOtherGame(Combo::Game current) {
    switch (current) {
        case Combo::Game::OoT: return Combo::Game::MM;
        case Combo::Game::MM: return Combo::Game::OoT;
        default: return Combo::Game::None;
    }
}

} // namespace

int main(int argc, char** argv) {
    // Install crash handler for stack traces
    InstallCrashHandler();

    // Check for help
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    Combo::ComboContextBridge bridge;

    // Determine library paths - try current directory first, then games subdirectory
    std::string ootLibPath = "soh" LIB_SUFFIX;
    std::string mmLibPath = "2ship" LIB_SUFFIX;

    // Initialize combo infrastructure
    Combo_InitFrozenStates();

    // Register entrance links (test or production)
    if (HasTestEntranceFlag(argc, argv)) {
        std::cout << "Using TEST entrance: Mido's House ↔ Clock Tower" << std::endl;
        Combo::gCrossGameEntrances.RegisterTestLinks();
    } else {
        Combo::gCrossGameEntrances.RegisterDefaultLinks();
    }

    // Try to load both games
    bool ootLoaded = bridge.LoadGame(Combo::Game::OoT, ootLibPath);
    bool mmLoaded = bridge.LoadGame(Combo::Game::MM, mmLibPath);

    if (!ootLoaded && !mmLoaded) {
        std::cerr << "Error: Could not load any game libraries." << std::endl;
        std::cerr << "Make sure soh" LIB_SUFFIX " or 2ship" LIB_SUFFIX
                  << " are in the current directory." << std::endl;
        return 1;
    }

    // Build gameArgv early - needed for pre-init
    // Remove --game argument from argv before passing to game
    std::vector<char*> gameArgv;
    gameArgv.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--game") == 0) {
            i++; // Skip the value too
            continue;
        }
        if (std::strncmp(argv[i], "--game=", 7) == 0) {
            continue;
        }
        // Filter out combo-specific flags that games don't understand
        if (std::strcmp(argv[i], "--test-entrance") == 0) {
            continue;
        }
        gameArgv.push_back(argv[i]);
    }

    // =========================================================================
    // Pre-initialize both games so they share the SDL window
    // First game creates window, second game reuses it via SharedGraphics
    // This enables instant hot-switching without window recreation
    // =========================================================================
    if (ootLoaded) {
        std::cout << "Pre-initializing OoT..." << std::endl;
        bridge.SwitchGame(Combo::Game::OoT);
        int ootInit = bridge.Init(static_cast<int>(gameArgv.size()), gameArgv.data());
        if (ootInit != 0) {
            std::cerr << "Warning: OoT pre-init failed with code " << ootInit << std::endl;
            ootLoaded = false;
        }
    }

    if (mmLoaded) {
        std::cout << "Pre-initializing MM..." << std::endl;
        bridge.SwitchGame(Combo::Game::MM);
        int mmInit = bridge.Init(static_cast<int>(gameArgv.size()), gameArgv.data());
        if (mmInit != 0) {
            std::cerr << "Warning: MM pre-init failed with code " << mmInit << std::endl;
            mmLoaded = false;
        }
    }

    std::cout << "Pre-initialization complete." << std::endl;

    // Parse --game argument or show menu
    Combo::Game selected = ParseGameArg(argc, argv);

    if (selected == Combo::Game::None) {
        // No game specified, show menu
        selected = ShowGameMenu();
    }

    // Validate selection
    if (selected == Combo::Game::OoT && !ootLoaded) {
        std::cerr << "Error: OoT was selected but could not be loaded." << std::endl;
        if (mmLoaded) {
            std::cerr << "Falling back to MM." << std::endl;
            selected = Combo::Game::MM;
        } else {
            return 1;
        }
    }
    if (selected == Combo::Game::MM && !mmLoaded) {
        std::cerr << "Error: MM was selected but could not be loaded." << std::endl;
        if (ootLoaded) {
            std::cerr << "Falling back to OoT." << std::endl;
            selected = Combo::Game::OoT;
        } else {
            return 1;
        }
    }

    // Switch to selected game (already pre-initialized above)
    if (!bridge.SwitchGame(selected)) {
        std::cerr << "Error: Failed to switch to selected game." << std::endl;
        return 1;
    }

    // Games are already initialized from pre-init above
    // Init() will return 0 immediately if already initialized
    if (!bridge.IsGameInitialized(selected)) {
        std::cerr << "Error: Selected game was not pre-initialized." << std::endl;
        return 1;
    }

    // Game loop with hot-swap and entrance-based switch support
    bool keepRunning = true;

    while (keepRunning) {
        // Clear any previous switch requests
        Combo_ClearGameSwitchRequest();
        Combo_ClearPendingSwitch();

        // Run the game
        std::cout << "Starting " << bridge.GetGameName(selected).value_or("game")
                  << "... (Press F10 to switch games)" << std::endl;
        bridge.Run();

        // Determine next game: entrance-based switch takes priority
        Combo::Game nextGame = Combo::Game::None;
        uint16_t targetEntrance = 0;
        bool isEntranceSwitch = false;

        if (Combo_IsCrossGameSwitch()) {
            // Entrance-based cross-game switch
            const char* targetId = Combo_GetSwitchTargetGameId();
            if (targetId) {
                nextGame = Combo::IdToGame(targetId);
                targetEntrance = Combo_GetSwitchTargetEntrance();
                isEntranceSwitch = true;
                std::cout << "\n=== Cross-game entrance detected ===" << std::endl;
                std::cout << "Target: " << (nextGame == Combo::Game::OoT ? "OoT" : "MM")
                          << " entrance 0x" << std::hex << targetEntrance
                          << std::dec << std::endl;
            }
        } else if (Combo_IsGameSwitchRequested()) {
            // F10 hotkey switch (just toggle games)
            nextGame = GetOtherGame(selected);
        }

        // Handle the switch if requested
        if (nextGame != Combo::Game::None && bridge.IsGameLoaded(nextGame)) {
            std::cout << "\n=== Switching to "
                      << bridge.GetGameName(nextGame).value_or("other game")
                      << " ===" << std::endl;
            std::cout.flush();

            // Check if target game is pre-initialized (instant hot-switch)
            bool targetPreInitialized = bridge.IsGameInitialized(nextGame);

            if (targetPreInitialized) {
                // INSTANT HOT-SWITCH: Target game is pre-initialized
                // Skip Shutdown/Init - just switch the active game pointer
                std::cerr << "[HOT-SWITCH] Target game is pre-initialized, performing instant switch" << std::endl;
                std::cerr << "[HOT-SWITCH] Target game: " << Combo::GameToId(nextGame) << std::endl;

                // Try to restore frozen state for the target game
                const Combo::GameExports* exports = bridge.GetGameExports(nextGame);
                if (exports && exports->LoadState) {
                    std::cerr << "[HOT-SWITCH] Attempting to restore frozen state..." << std::endl;
                    int loadResult = exports->LoadState(nullptr, 0);
                    std::cerr << "[HOT-SWITCH] LoadState returned: " << loadResult
                              << " (0=success, -1=no state)" << std::endl;
                } else {
                    std::cerr << "[HOT-SWITCH] No LoadState function available" << std::endl;
                }

                // For entrance-based switches, set up the target entrance
                if (isEntranceSwitch && targetEntrance != 0) {
                    std::cerr << "[HOT-SWITCH] Setting startup entrance to 0x"
                              << std::hex << targetEntrance << std::dec << std::endl;
                    Combo_SetStartupEntrance(targetEntrance);
                    std::cerr << "[HOT-SWITCH] Startup entrance set, verifying: 0x"
                              << std::hex << Combo_GetStartupEntrance() << std::dec << std::endl;
                }

                // Just switch - no shutdown, no init needed
                selected = nextGame;
                bridge.SwitchGame(selected);
                std::cerr << "[HOT-SWITCH] Switch complete, about to run "
                          << bridge.GetGameName(selected).value_or("game") << std::endl;
                // Loop continues, will run the new game
            } else {
                // FALLBACK: Target game not pre-initialized (shouldn't happen normally)
                // Use the old shutdown/init path
                std::cerr << "[SWITCH DEBUG] Target not pre-initialized, using shutdown/init path" << std::endl;

                // Shutdown current game
                bridge.Shutdown();

                // For entrance-based switches, set up the target entrance
                if (isEntranceSwitch && targetEntrance != 0) {
                    Combo_SetStartupEntrance(targetEntrance);
                }

                // Switch to the other game
                selected = nextGame;
                bridge.SwitchGame(selected);

                // Initialize the new game
                int switchInitResult = bridge.Init(
                    static_cast<int>(gameArgv.size()), gameArgv.data());
                if (switchInitResult != 0) {
                    std::cerr << "Error: Failed to initialize "
                              << Combo::GameToId(selected)
                              << " (code " << switchInitResult << ")" << std::endl;
                    keepRunning = false;
                }
                // Loop continues, will run the new game
            }
        } else if (nextGame != Combo::Game::None) {
            std::cerr << "Cannot switch: target game not loaded" << std::endl;
            keepRunning = false;
        } else {
            // Normal exit (no switch requested)
            keepRunning = false;
        }
    }

    // Cleanup
    bridge.Shutdown();

    std::cout << "Game exited normally." << std::endl;
    return 0;
}
