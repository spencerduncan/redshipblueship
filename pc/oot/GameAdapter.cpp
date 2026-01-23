/**
 * OoT Game Adapter for Unified Build
 *
 * Provides the prefixed game interface (OoT_Game_*) for the unified
 * single-executable build. This adapter bridges the main entry point
 * to OoT's internal initialization and game loop.
 *
 * The unified build compiles both OoT and MM as OBJECT libraries,
 * requiring symbol namespacing to avoid collisions.
 */

#include <cstdio>
#include <cstdlib>

// =============================================================================
// External declarations for OoT internal functions
// These functions come from the OoT decomp code compiled as oot_decomp OBJECT
// =============================================================================
extern "C" {
    // Initialization functions from soh/
    void GameConsole_Init(void);
    void InitOTR(int argc, char* argv[]);
    void DeinitOTR(void);
    void BootCommands_Init(void);

    // Game loop from src/code/main.c
    void Main(void* arg);

    // Heap management from src/buffers/heaps.c
    void OoT_Heaps_Alloc(void);
    void OoT_Heaps_Free(void);

    // Crash handler from soh/
    void CrashHandler_PrintSohData(char* buffer, size_t* pos);
    void CrashHandlerRegisterCallback(void (*callback)(char*, size_t*));
}

// =============================================================================
// Prefixed game interface for unified executable
// These are the entry points called by pc/common/main.cpp
// =============================================================================
extern "C" {

/**
 * Initialize OoT game subsystems.
 * Matches the initialization sequence from soh's original main().
 */
int OoT_Game_Init(int argc, char** argv) {
    fprintf(stderr, "[OoT] Initializing...\n");

    GameConsole_Init();
    InitOTR(argc, argv);
    CrashHandlerRegisterCallback(CrashHandler_PrintSohData);
    BootCommands_Init();
    OoT_Heaps_Alloc();

    fprintf(stderr, "[OoT] Initialization complete.\n");
    return 0;
}

/**
 * Run OoT's main game loop.
 * This function typically doesn't return until the game exits.
 */
void OoT_Game_Run(void) {
    Main(nullptr);
}

/**
 * Shutdown OoT and release resources.
 */
void OoT_Game_Shutdown(void) {
    fprintf(stderr, "[OoT] Shutting down...\n");
    DeinitOTR();
    OoT_Heaps_Free();
    fprintf(stderr, "[OoT] Shutdown complete.\n");
}

/**
 * Get the display name for OoT.
 */
const char* OoT_Game_GetName(void) {
    return "Ocarina of Time";
}

/**
 * Get the short identifier for OoT.
 */
const char* OoT_Game_GetId(void) {
    return "oot";
}

} // extern "C"
