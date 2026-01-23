/**
 * RedShipBlueShip Unified Executable Entry Point
 *
 * Single executable containing both OoT and MM compiled as OBJECT libraries.
 * This replaces the dynamic library loading approach with direct linking.
 *
 * Usage:
 *   redship --game oot    # Run Ocarina of Time
 *   redship --game mm     # Run Majora's Mask
 *   redship --test boot-oot   # Test OoT boot
 *   redship --test boot-mm    # Test MM boot
 */

#include <iostream>
#include <string>
#include <cstring>

// Forward declarations for namespaced game functions
// OoT functions (prefixed OoT_)
extern "C" {
    int OoT_Game_Init(int argc, char** argv);
    void OoT_Game_Run(void);
    void OoT_Game_Shutdown(void);
    const char* OoT_Game_GetName(void);
}

// MM functions (prefixed MM_)
extern "C" {
    int MM_Game_Init(int argc, char** argv);
    void MM_Game_Run(void);
    void MM_Game_Shutdown(void);
    const char* MM_Game_GetName(void);
}

enum class Game { None, OoT, MM };

namespace {

void PrintUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --game oot      Run Ocarina of Time\n"
              << "  --game mm       Run Majora's Mask\n"
              << "  --test boot-oot Test OoT boot sequence\n"
              << "  --test boot-mm  Test MM boot sequence\n"
              << "  --help          Show this help message\n\n"
              << "If no game is specified, a menu will be displayed.\n";
}

Game ParseGameArg(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            const char* game = argv[i + 1];
            if (std::strcmp(game, "oot") == 0) return Game::OoT;
            if (std::strcmp(game, "mm") == 0) return Game::MM;
        }
        if (std::strncmp(argv[i], "--game=", 7) == 0) {
            const char* game = argv[i] + 7;
            if (std::strcmp(game, "oot") == 0) return Game::OoT;
            if (std::strcmp(game, "mm") == 0) return Game::MM;
        }
    }
    return Game::None;
}

std::string ParseTestArg(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--test") == 0 && i + 1 < argc) {
            return argv[i + 1];
        }
        if (std::strncmp(argv[i], "--test=", 7) == 0) {
            return argv[i] + 7;
        }
    }
    return "";
}

Game ShowGameMenu() {
    std::cout << "\n=== RedShipBlueShip ===\n\n"
              << "Select a game:\n"
              << "  1) Ocarina of Time\n"
              << "  2) Majora's Mask\n\n"
              << "Enter choice (1 or 2): ";

    std::string input;
    std::getline(std::cin, input);

    if (input == "1" || input == "oot") return Game::OoT;
    if (input == "2" || input == "mm") return Game::MM;

    std::cerr << "Invalid choice. Defaulting to OoT.\n";
    return Game::OoT;
}

int RunGame(Game game, int argc, char** argv) {
    int result = 0;

    switch (game) {
        case Game::OoT:
            std::cout << "Starting Ocarina of Time...\n";
            result = OoT_Game_Init(argc, argv);
            if (result != 0) {
                std::cerr << "OoT initialization failed with code " << result << "\n";
                return result;
            }
            OoT_Game_Run();
            OoT_Game_Shutdown();
            break;

        case Game::MM:
            std::cout << "Starting Majora's Mask...\n";
            result = MM_Game_Init(argc, argv);
            if (result != 0) {
                std::cerr << "MM initialization failed with code " << result << "\n";
                return result;
            }
            MM_Game_Run();
            MM_Game_Shutdown();
            break;

        default:
            std::cerr << "No game selected.\n";
            return 1;
    }

    return 0;
}

int RunTest(const std::string& testName, int argc, char** argv) {
    if (testName == "boot-oot") {
        std::cout << "Test: Boot OoT\n";
        // Initialize OoT but don't run the full loop
        int result = OoT_Game_Init(argc, argv);
        if (result == 0) {
            std::cout << "OoT boot test PASSED\n";
            OoT_Game_Shutdown();
        }
        return result;
    }

    if (testName == "boot-mm") {
        std::cout << "Test: Boot MM\n";
        // Initialize MM but don't run the full loop
        int result = MM_Game_Init(argc, argv);
        if (result == 0) {
            std::cout << "MM boot test PASSED\n";
            MM_Game_Shutdown();
        }
        return result;
    }

    if (testName == "list") {
        std::cout << "Available tests:\n"
                  << "  boot-oot    Boot OoT to initialization\n"
                  << "  boot-mm     Boot MM to initialization\n";
        return 0;
    }

    std::cerr << "Unknown test: " << testName << "\n";
    return 1;
}

} // anonymous namespace

int main(int argc, char** argv) {
    // Check for help
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    // Check for test mode
    std::string testArg = ParseTestArg(argc, argv);
    if (!testArg.empty()) {
        return RunTest(testArg, argc, argv);
    }

    // Parse game argument
    Game selected = ParseGameArg(argc, argv);
    if (selected == Game::None) {
        selected = ShowGameMenu();
    }

    // Run the selected game
    return RunGame(selected, argc, argv);
}
