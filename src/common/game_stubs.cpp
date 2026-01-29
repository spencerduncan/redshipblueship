/**
 * @file game_stubs.cpp
 * @brief Stub implementations for game entry points (TEST MODE ONLY)
 *
 * These stubs are ONLY used when the actual game object libraries are not linked.
 * They exist to allow infrastructure tests (--test mode) to run without requiring
 * the full game code.
 *
 * In normal builds, the real implementations are in:
 * - games/oot/soh/GameExports_SingleExe.cpp
 * - games/mm/2s2h/GameExports_SingleExe.cpp
 *
 * The linker prefers the real implementations (defined in the game object libraries)
 * over these weak symbols.
 */

#include <cstdio>

// Mark stubs as weak so the real implementations take priority
#ifdef __GNUC__
#define WEAK_SYMBOL __attribute__((weak))
#else
#define WEAK_SYMBOL
#endif

extern "C" {

WEAK_SYMBOL int OoT_Game_Init(int argc, char** argv) {
    printf("[OoT STUB] Game_Init called (argc=%d)\n", argc);
    printf("[OoT STUB] This is a stub - real implementation not linked.\n");
    return 0;
}

WEAK_SYMBOL void OoT_Game_Run(void) {
    printf("[OoT STUB] Game_Run called - stub implementation\n");
}

WEAK_SYMBOL void OoT_Game_Shutdown(void) {
    printf("[OoT STUB] Game_Shutdown called\n");
}

WEAK_SYMBOL const char* OoT_Game_GetName(void) {
    return "Ocarina of Time (Stub)";
}

WEAK_SYMBOL const char* OoT_Game_GetId(void) {
    return "oot";
}

WEAK_SYMBOL int MM_Game_Init(int argc, char** argv) {
    printf("[MM STUB] Game_Init called (argc=%d)\n", argc);
    printf("[MM STUB] This is a stub - real implementation not linked.\n");
    return 0;
}

WEAK_SYMBOL void MM_Game_Run(void) {
    printf("[MM STUB] Game_Run called - stub implementation\n");
}

WEAK_SYMBOL void MM_Game_Shutdown(void) {
    printf("[MM STUB] Game_Shutdown called\n");
}

WEAK_SYMBOL const char* MM_Game_GetName(void) {
    return "Majora's Mask (Stub)";
}

WEAK_SYMBOL const char* MM_Game_GetId(void) {
    return "mm";
}

} // extern "C"
