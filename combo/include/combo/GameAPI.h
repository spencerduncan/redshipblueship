/**
 * GameAPI.h - Clean namespace interface for OoT and MM game code
 *
 * After symbol namespacing (issue #29), both games' symbols are prefixed:
 *   - OoT functions/globals: OoT_<name>
 *   - MM functions/globals:  MM_<name>
 *
 * This header provides a clean C++ namespace interface for accessing
 * the renamed game functions. It serves as the primary API for combo
 * code to interact with game-specific functionality.
 *
 * Example usage:
 *   namespace OoT {
 *       Actor_Init(actor, play);  // Calls OoT_Actor_Init
 *   }
 *   namespace MM {
 *       Actor_Init(actor, play);  // Calls MM_Actor_Init
 *   }
 */

#pragma once

#include "combo/ComboContext.h"

// Forward declarations for game types
// These will need to be defined or included from game headers
struct Actor;
struct PlayState;
struct SaveContext;

/**
 * OoT namespace - wrapper for OoT_* prefixed functions
 *
 * After LSC, OoT functions are prefixed with OoT_ to avoid collisions.
 * This namespace provides the original function names for cleaner code.
 *
 * Note: The actual extern declarations will be added as the unified build
 * progresses. For now, this serves as documentation of the API pattern.
 */
namespace OoT {

// Example declarations (uncomment and add proper signatures as needed):
// extern "C" void Actor_Init(Actor* actor, PlayState* play);
// extern "C" void Main_Init(void);
// extern "C" void Main_Run(void);
// extern "C" SaveContext gSaveContext;

} // namespace OoT

/**
 * MM namespace - wrapper for MM_* prefixed functions
 *
 * After LSC, MM functions are prefixed with MM_ to avoid collisions.
 * This namespace provides the original function names for cleaner code.
 */
namespace MM {

// Example declarations (uncomment and add proper signatures as needed):
// extern "C" void Actor_Init(Actor* actor, PlayState* play);
// extern "C" void Main_Init(void);
// extern "C" void Main_Run(void);
// extern "C" SaveContext gSaveContext;

} // namespace MM

/**
 * Macro helpers for calling namespaced functions from C code
 *
 * When C code needs to call game-specific functions, use these macros:
 *
 *   GAME_FUNC(OoT, Actor_Init)(actor, play);
 *   GAME_FUNC(MM, Actor_Init)(actor, play);
 *
 * Which expands to:
 *   OoT_Actor_Init(actor, play);
 *   MM_Actor_Init(actor, play);
 */
#define GAME_FUNC(game, name) game##_##name
#define GAME_VAR(game, name) game##_##name

/**
 * Runtime dispatch helper
 *
 * For cases where the game is determined at runtime:
 *
 *   if (currentGame == Combo::Game::OoT) {
 *       GAME_FUNC(OoT, Actor_Init)(actor, play);
 *   } else {
 *       GAME_FUNC(MM, Actor_Init)(actor, play);
 *   }
 */
