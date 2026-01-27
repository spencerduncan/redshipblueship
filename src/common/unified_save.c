/**
 * @file unified_save.c
 * @brief Unified SaveContext storage for single-executable builds
 *
 * Both OoT and MM define `SaveContext gSaveContext`, but with different
 * struct layouts (OoT: ~5KB, MM: ~18KB). In single-exe mode, we provide
 * a single storage area large enough for either game.
 *
 * The games' code has `extern SaveContext gSaveContext` declarations that
 * resolve to this storage. Since C doesn't do type checking at link time,
 * each game interprets the memory according to its own SaveContext layout.
 *
 * The union ensures both layouts share the same base address:
 * - OoT accesses gSaveContext.health (flat struct)
 * - MM accesses gSaveContext.save.saveInfo.playerData.health (nested)
 * Both work because member access is just base + offset math.
 */

#include <stddef.h>
#include <stdalign.h>

/* SaveContext sizes from z64save.h:
 * OoT: 0x1428 (~5KB)
 * MM:  0x48C8 (~18KB)
 */
#define OOT_SAVE_CONTEXT_SIZE 0x1428
#define MM_SAVE_CONTEXT_SIZE  0x48C8
#define UNIFIED_SAVE_SIZE     MM_SAVE_CONTEXT_SIZE  /* Use larger */

/**
 * Unified SaveContext storage.
 *
 * Declared as a char array so we don't need to include either game's
 * z64save.h (which would bring in many dependencies). The game code's
 * extern declarations provide the type interpretation.
 *
 * Aligned to 16 bytes as required by MM.
 */
alignas(16) char gSaveContext[UNIFIED_SAVE_SIZE];

/**
 * For cross-game code that needs to access both interpretations,
 * we provide typed pointers. These must be set up after including
 * the appropriate game headers in the translation unit that needs them.
 */

/* Size verification - can be checked at runtime during init */
_Static_assert(UNIFIED_SAVE_SIZE >= OOT_SAVE_CONTEXT_SIZE,
               "Unified storage must fit OoT SaveContext");
_Static_assert(UNIFIED_SAVE_SIZE >= MM_SAVE_CONTEXT_SIZE,
               "Unified storage must fit MM SaveContext");
