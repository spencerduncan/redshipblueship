/**
 * @file netplay_items.h
 * @brief Provisional Archipelago item-id <-> origin-tagged item mapping.
 *
 * Archipelago item ids are a flat int64 namespace defined by a world's data
 * package; RSBS items are per-game enums carried as origin-tagged structs
 * (ADR 0002). Until an actual RSBS apworld exists (deferred — see ADR 0006
 * §Deferred), the transport and the loopback harness agree on this explicit
 * bijection:
 *
 *     apId = RSBS_AP_ITEM_BASE + (game << 16) + localId
 *
 * where game is the GameId enum value (GAME_OOT=1 / GAME_MM=2) and localId
 * is the RG_* / RI_* value. This is a WIRE encoding, not a storage format:
 * decode returns the ADR-0002 tagged struct members and validates strictly,
 * so an untagged integer still cannot cross into the save (the #356 lesson
 * — the tag is packed only inside an id namespace that never touches
 * ComboContext).
 *
 * RSBS_AP_ITEM_BASE is 0x52530000 ("RS" << 16): far outside every id range
 * shipped by existing Archipelago worlds (which allocate in the low
 * millions), so a real AP room's foreign item ids can never alias ours by
 * accident — they fail decode and are skipped with a counter.
 */

#ifndef RSBS_NETPLAY_NETPLAY_ITEMS_H
#define RSBS_NETPLAY_NETPLAY_ITEMS_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h" // GameId

#ifdef __cplusplus
extern "C" {
#endif

#define RSBS_AP_ITEM_BASE 0x52530000ll

/**
 * Encode an origin-tagged item as an Archipelago item id.
 * Returns -1 if originGame is not a real game.
 */
int64_t NetplayItems_Encode(GameId originGame, uint16_t itemId);

/**
 * Decode an Archipelago item id. Returns true and fills the out params only
 * for a well-formed RSBS id (base matches, game tag is GAME_OOT/GAME_MM);
 * anything else returns false and leaves the out params untouched.
 */
bool NetplayItems_Decode(int64_t apItemId, GameId* outGame, uint16_t* outItemId);

#ifdef __cplusplus
}
#endif

#endif // RSBS_NETPLAY_NETPLAY_ITEMS_H
