/**
 * MM Game Adapter for Unified Build
 *
 * Provides the prefixed game interface (MM_Game_*) for the unified
 * single-executable build. This adapter bridges the main entry point
 * to MM's internal initialization and game loop.
 *
 * The unified build compiles both OoT and MM as OBJECT libraries,
 * requiring symbol namespacing to avoid collisions.
 */

#include <cstdio>
#include <cstdlib>

// =============================================================================
// External declarations for MM internal functions
// These functions come from the MM decomp code compiled as mm_decomp OBJECT
// Note: MM uses different init paths than OoT - it uses InitOTR() (no args)
// =============================================================================
extern "C" {
    // Initialization functions from 2s2h/
    void InitOTR(void);
    void DeinitOTR(void);

    // Heap management from src/buffers/heaps.c (already prefixed)
    void MM_Heaps_Alloc(void);
    void MM_Heaps_Free(void);

    // Game loop from src/code/graph.c (already prefixed)
    void MM_Graph_ThreadEntry(void* arg);

    // Low-level init from src/boot/
    void Nmi_Init(void);
    void MM_Fault_Init(void);
    void Check_RegionIsSupported(void);
    void Check_ExpansionPak(void);
    void Regs_Init(void);

    // System heap from src/code/
    void MM_SystemHeap_Init(void* base, size_t size);

    // Screen dimensions
    extern int MM_gScreenWidth;
    extern int MM_gScreenHeight;
    extern unsigned long MM_gSystemHeap;

    // Crash handler from 2s2h/
    void CrashHandler_PrintExt(char* buffer, size_t* pos);
    void CrashHandlerRegisterCallback(void (*callback)(char*, size_t*));
}

// Constants from MM's headers
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define SYSTEM_HEAP_SIZE 0x29000

// =============================================================================
// Prefixed game interface for unified executable
// These are the entry points called by pc/common/main.cpp
// =============================================================================
extern "C" {

/**
 * Initialize MM game subsystems.
 * Matches the initialization sequence from MM's original main().
 */
int MM_Game_Init(int argc, char** argv) {
    (void)argc;
    (void)argv;

    fprintf(stderr, "[MM] Initializing...\n");

    // Core initialization from 2s2h
    InitOTR();
    CrashHandlerRegisterCallback(CrashHandler_PrintExt);
    MM_Heaps_Alloc();

    // Set screen dimensions
    MM_gScreenWidth = SCREEN_WIDTH;
    MM_gScreenHeight = SCREEN_HEIGHT;

    // Low-level initialization
    Nmi_Init();
    MM_Fault_Init();
    Check_RegionIsSupported();
    Check_ExpansionPak();
    MM_SystemHeap_Init((void*)MM_gSystemHeap, SYSTEM_HEAP_SIZE);
    Regs_Init();

    fprintf(stderr, "[MM] Initialization complete.\n");
    return 0;
}

/**
 * Run MM's main game loop.
 * This function typically doesn't return until the game exits.
 */
void MM_Game_Run(void) {
    MM_Graph_ThreadEntry(nullptr);
}

/**
 * Shutdown MM and release resources.
 */
void MM_Game_Shutdown(void) {
    fprintf(stderr, "[MM] Shutting down...\n");
    DeinitOTR();
    MM_Heaps_Free();
    fprintf(stderr, "[MM] Shutdown complete.\n");
}

/**
 * Get the display name for MM.
 */
const char* MM_Game_GetName(void) {
    return "Majora's Mask";
}

/**
 * Get the short identifier for MM.
 */
const char* MM_Game_GetId(void) {
    return "mm";
}

} // extern "C"
