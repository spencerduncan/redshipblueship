/**
 * @file game_stubs.cpp
 * @brief Stub implementations for game entry points
 *
 * These stubs provide the OoT_* and MM_* namespaced functions that the
 * single executable main.cpp expects. They will be replaced by the actual
 * game code when the full symbol namespacing is integrated.
 *
 * For now, these allow the infrastructure to be tested.
 */

#include <cstdio>

// ============================================================================
// OoT Stub Implementations
// ============================================================================

extern "C" {

int OoT_Game_Init(int argc, char** argv) {
    printf("[OoT STUB] Game_Init called (argc=%d)\n", argc);
    printf("[OoT STUB] This is a stub implementation for testing.\n");
    printf("[OoT STUB] Replace with actual OoT code when symbol namespacing is complete.\n");
    return 0;
}

void OoT_Game_Run(void) {
    printf("[OoT STUB] Game_Run called\n");
    printf("[OoT STUB] Game would run here. Press Ctrl+C to exit.\n");
    // In a real implementation, this would run the game loop
    // For testing, we just return immediately
}

void OoT_Game_Shutdown(void) {
    printf("[OoT STUB] Game_Shutdown called\n");
}

const char* OoT_Game_GetName(void) {
    return "Ocarina of Time (Stub)";
}

const char* OoT_Game_GetId(void) {
    return "oot";
}

// ============================================================================
// MM Stub Implementations
// ============================================================================

int MM_Game_Init(int argc, char** argv) {
    printf("[MM STUB] Game_Init called (argc=%d)\n", argc);
    printf("[MM STUB] This is a stub implementation for testing.\n");
    printf("[MM STUB] Replace with actual MM code when symbol namespacing is complete.\n");
    return 0;
}

void MM_Game_Run(void) {
    printf("[MM STUB] Game_Run called\n");
    printf("[MM STUB] Game would run here. Press Ctrl+C to exit.\n");
    // In a real implementation, this would run the game loop
    // For testing, we just return immediately
}

void MM_Game_Shutdown(void) {
    printf("[MM STUB] Game_Shutdown called\n");
}

const char* MM_Game_GetName(void) {
    return "Majora's Mask (Stub)";
}

const char* MM_Game_GetId(void) {
    return "mm";
}

} // extern "C"
