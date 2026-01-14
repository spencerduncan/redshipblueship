#pragma once

#include <cstddef>

/**
 * Game Export Interface
 *
 * Each game (OoT, MM) must implement these functions and export them
 * from their shared library. The combo launcher loads these dynamically.
 *
 * To implement in a game:
 *   #include "combo/GameExports.h"
 *   GAME_EXPORT int Game_Init(int argc, char** argv) { ... }
 */

// Export macro for symbol visibility
#ifdef _WIN32
    #ifdef GAME_BUILDING_DLL
        #define GAME_EXPORT extern "C" __declspec(dllexport)
    #else
        #define GAME_EXPORT extern "C" __declspec(dllimport)
    #endif
#else
    #define GAME_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Function pointer typedefs for dynamic loading
extern "C" {

/**
 * Initialize the game.
 * @param argc Command line argument count
 * @param argv Command line arguments
 * @return 0 on success, non-zero on failure
 */
typedef int (*GameInitFn)(int argc, char** argv);

/**
 * Run the game's main loop.
 * This typically doesn't return until the game exits or is paused.
 */
typedef void (*GameRunFn)(void);

/**
 * Shutdown the game and release resources.
 */
typedef void (*GameShutdownFn)(void);

/**
 * Pause the game loop (for hot-switching).
 * The game should stop processing but retain state.
 */
typedef void (*GamePauseFn)(void);

/**
 * Resume a paused game.
 */
typedef void (*GameResumeFn)(void);

/**
 * Save the game's state to a buffer (for hot-switching).
 * @param outSize Pointer to receive the size of the returned buffer
 * @return Pointer to state data (caller must free), or nullptr on failure
 */
typedef void* (*GameSaveStateFn)(size_t* outSize);

/**
 * Load game state from a buffer.
 * @param data State data from a previous SaveState call
 * @param size Size of the data buffer
 * @return 0 on success, non-zero on failure
 */
typedef int (*GameLoadStateFn)(void* data, size_t size);

/**
 * Get the game's display name (e.g., "Ocarina of Time").
 */
typedef const char* (*GameGetNameFn)(void);

/**
 * Get the game's short identifier (e.g., "oot", "mm").
 */
typedef const char* (*GameGetIdFn)(void);

} // extern "C"

/**
 * Container for all game function pointers.
 * Populated by loading symbols from a game library.
 */
struct GameExports {
    GameInitFn Init = nullptr;
    GameRunFn Run = nullptr;
    GameShutdownFn Shutdown = nullptr;
    GamePauseFn Pause = nullptr;
    GameResumeFn Resume = nullptr;
    GameSaveStateFn SaveState = nullptr;
    GameLoadStateFn LoadState = nullptr;
    GameGetNameFn GetName = nullptr;
    GameGetIdFn GetId = nullptr;

    /**
     * Check if required exports are loaded.
     * Init, Run, Shutdown, GetName, GetId are required.
     * Pause, Resume, SaveState, LoadState are optional (for hot-switching).
     */
    bool HasRequiredExports() const {
        return Init && Run && Shutdown && GetName && GetId;
    }

    /**
     * Check if hot-switching exports are available.
     */
    bool HasHotSwitchExports() const {
        return Pause && Resume;
    }

    /**
     * Check if state serialization exports are available.
     */
    bool HasStateExports() const {
        return SaveState && LoadState;
    }
};

// Symbol names for dynamic loading
namespace GameSymbols {
    constexpr const char* Init = "Game_Init";
    constexpr const char* Run = "Game_Run";
    constexpr const char* Shutdown = "Game_Shutdown";
    constexpr const char* Pause = "Game_Pause";
    constexpr const char* Resume = "Game_Resume";
    constexpr const char* SaveState = "Game_SaveState";
    constexpr const char* LoadState = "Game_LoadState";
    constexpr const char* GetName = "Game_GetName";
    constexpr const char* GetId = "Game_GetId";
}
