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

#include "combo/ComboContextBridge.h"

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
    std::cout << "  --help          Show this help message" << std::endl;
    std::cout << "\n";
    std::cout << "If no game is specified, a menu will be displayed." << std::endl;
}

} // namespace

int main(int argc, char** argv) {
    // Check for help
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    Combo::ComboContextBridge bridge;

    // Determine library paths
    // TODO: Make these configurable or auto-detect
    std::string ootLibPath = "games/oot/soh" LIB_SUFFIX;
    std::string mmLibPath = "games/mm/2ship" LIB_SUFFIX;

    // Try to load both games
    bool ootLoaded = bridge.LoadGame(Combo::Game::OoT, ootLibPath);
    bool mmLoaded = bridge.LoadGame(Combo::Game::MM, mmLibPath);

    if (!ootLoaded && !mmLoaded) {
        std::cerr << "Error: Could not load any game libraries." << std::endl;
        std::cerr << "Make sure soh" LIB_SUFFIX " or 2ship" LIB_SUFFIX
                  << " are in the current directory." << std::endl;
        return 1;
    }

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

    // Switch to selected game
    if (!bridge.SwitchGame(selected)) {
        std::cerr << "Error: Failed to switch to selected game." << std::endl;
        return 1;
    }

    // Initialize the game
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
        gameArgv.push_back(argv[i]);
    }

    int initResult = bridge.Init(static_cast<int>(gameArgv.size()), gameArgv.data());
    if (initResult != 0) {
        std::cerr << "Error: Game initialization failed with code "
                  << initResult << std::endl;
        return initResult;
    }

    // Run the game
    std::cout << "Starting " << bridge.GetGameName(selected).value_or("game")
              << "..." << std::endl;
    bridge.Run();

    // Cleanup
    bridge.Shutdown();

    std::cout << "Game exited normally." << std::endl;
    return 0;
}
