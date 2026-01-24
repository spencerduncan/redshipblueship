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

// SaveContext sizes from the game headers
// OoT: z64save.h line 354: size = 0x1428
// MM:  z64save.h line 527: size = 0x48C8
#define OOT_SAVE_CONTEXT_SIZE 0x1428  // 5160 bytes
#define MM_SAVE_CONTEXT_SIZE  0x48C8  // 18632 bytes

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
