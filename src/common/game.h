/**
 * @file game.h
 * @brief Common game definitions for single-executable architecture
 *
 * This header defines the Game enum and common types shared between
 * OoT and MM in the unified single-executable build.
 */

#ifndef RSBS_COMMON_GAME_H
#define RSBS_COMMON_GAME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Game identifiers for the unified executable
 */
typedef enum {
    GAME_NONE = 0,
    GAME_OOT = 1,   // Ocarina of Time
    GAME_MM = 2     // Majora's Mask
} GameId;

// SaveContext blob capacities — the SINGLE source of truth for every buffer
// that holds a full runtime SaveContext image (context.cpp shadow copies,
// unified_save.c storage, the .redsave tiers, and the headless tests).
//
// These are CAPACITIES, not exact struct sizes. Each port's runtime
// SaveContext is much larger than the original N64 struct the "// size ="
// comments in z64save.h describe:
//   OoT: N64 struct 0x1428, but SoH appends ShipSaveContextData (SohStats
//        with sceneTimestamps[8191], ...) — sizeof(SaveContext) ~= 0x21C30.
//   MM:  N64 struct 0x48C8, but 2S2H appends ShipSaveInfo (rando check
//        tables, ...) and ShipSaveContext — sizeof(SaveContext) ~= 0xC000.
// Those extension sections grow as the upstream ports evolve, so the exact
// sizeof cannot be hardcoded here (this header must stay free of game
// includes). Instead each capacity carries headroom, and a static_assert in a
// TU that CAN see the real struct (games/oot/soh/GameExports_SingleExe.cpp,
// games/mm/2s2h/GameExports_SingleExe.cpp) verifies
// sizeof(SaveContext) <= *_SAVE_CONTEXT_SIZE so drift fails the build loudly
// instead of silently truncating cross-game save state (issue: freeze/restore
// used to clamp OoT to 0x1428, losing all SoH ship.* state per switch).
#define OOT_SAVE_CONTEXT_SIZE 0x22000  // ~136KB capacity (SoH runtime struct ~0x21C30)
#define MM_SAVE_CONTEXT_SIZE  0x10000  // 64KB capacity (2S2H runtime struct ~0xC000)

/**
 * Convert game ID string to enum
 * @param id "oot" or "mm"
 * @return GameId enum value, GAME_NONE if invalid
 */
GameId Game_FromString(const char* id);

/**
 * Convert game enum to ID string
 * @param game Game enum value
 * @return "oot", "mm", or NULL if invalid
 */
const char* Game_ToString(GameId game);

/**
 * Get the other game (for switching)
 */
GameId Game_GetOther(GameId game);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_GAME_H
