/**
 * Game Export Interface for OoT (Ship of Harkinian)
 *
 * This file implements the standard game interface that allows the combo
 * launcher to load and run OoT as a shared library.
 */

#include "combo/GameExports.h"
#include "OTRGlobals.h"
#include <libultraship/bridge.h>
#include "soh/CrashHandlerExt.h"

// External declarations from main.c and other C sources
extern "C" {
    void GameConsole_Init(void);
    void InitOTR(int argc, char* argv[]);
    void DeinitOTR(void);
    void Heaps_Alloc(void);
    void Heaps_Free(void);
    void Main(void* arg);
    void BootCommands_Init(void);
}

// Game state for pause/resume
static bool sGamePaused = false;
static int sArgc = 0;
static char** sArgv = nullptr;

GAME_EXPORT int Game_Init(int argc, char** argv) {
    // Store args for potential restart
    sArgc = argc;
    sArgv = argv;

    // Initialize OoT subsystems (matching what main() does)
    GameConsole_Init();
    InitOTR(argc, argv);
    CrashHandlerRegisterCallback(CrashHandler_PrintSohData);
    BootCommands_Init();
    Heaps_Alloc();

    return 0;
}

GAME_EXPORT void Game_Run(void) {
    // Run the main game loop
    Main(nullptr);
}

GAME_EXPORT void Game_Shutdown(void) {
    DeinitOTR();
    Heaps_Free();
}

GAME_EXPORT void Game_Pause(void) {
    // TODO: Implement pause logic
    // For now, this is a placeholder. Full implementation requires
    // modifying the game loop to check sGamePaused flag.
    sGamePaused = true;
}

GAME_EXPORT void Game_Resume(void) {
    sGamePaused = false;
}

GAME_EXPORT void* Game_SaveState(size_t* outSize) {
    // TODO: Implement state serialization for hot-switching
    // This is complex and will be implemented in Phase 2
    *outSize = 0;
    return nullptr;
}

GAME_EXPORT int Game_LoadState(void* data, size_t size) {
    // TODO: Implement state deserialization
    (void)data;
    (void)size;
    return -1;  // Not implemented
}

GAME_EXPORT const char* Game_GetName(void) {
    return "Ocarina of Time";
}

GAME_EXPORT const char* Game_GetId(void) {
    return "oot";
}
