#pragma once
/**
 * @file ForeignModelHostOoT.h
 * @brief OoT's get-item cutscene shows a Majora's Mask item and gives nothing
 *        (#577 M4). The definitions and the reasoning are in
 *        ForeignModelHostOoT.cpp; this header is what the RC-queue drain
 *        (hook_handlers.cpp), the give point (z_player.c) and the item textbox
 *        (Messages/ItemMessages.cpp) call. C-compatible: z_player.c includes it.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdint.h>

#include "soh/Enhancements/item-tables/ItemTableTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Build the show-only get-item entry for OoT check `rc`, which must host an MM
 * item (the placement table, and behind it the crossing store), and arm it for
 * one take. 1 and *out when built; 0 (nothing armed) when the check hosts no MM
 * item or `out` is null.
 */
int OoT_Rando_Foreign_BuildShowOnlyGetItem(uint16_t rc, GetItemEntry* out);

/**
 * The give point's question (func_8084DFF4, z_player.c): 1 when `entry` is the
 * armed show-only entry, which spends the arm and releases the RC queue's slot
 * (what the item-receive hook does for an item that is given); the caller then
 * gives nothing. 0 for every other entry, which the caller gives as before.
 */
int OoT_Rando_Foreign_TakeShowOnlyGetItem(const GetItemEntry* entry);

/**
 * The item textbox's words for a show-only entry: 1 with the MM item's article
 * ("the ", "a ", ... or "") and name; 0 when `entry` is not the show-only entry
 * (the caller builds OoT's own message).
 */
int OoT_Rando_Foreign_ShowOnlyItemText(const GetItemEntry* entry, const char** article, const char** name);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
