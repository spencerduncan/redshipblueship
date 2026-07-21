/**
 * @file netplay_items.c
 * @brief Provisional AP item-id mapping (see netplay_items.h).
 */

#include "netplay_items.h"

int64_t NetplayItems_Encode(GameId originGame, uint16_t itemId) {
    if (originGame != GAME_OOT && originGame != GAME_MM) {
        return -1;
    }
    return RSBS_AP_ITEM_BASE + ((int64_t)originGame << 16) + (int64_t)itemId;
}

bool NetplayItems_Decode(int64_t apItemId, GameId* outGame, uint16_t* outItemId) {
    if (outGame == NULL || outItemId == NULL) {
        return false;
    }
    int64_t rel = apItemId - RSBS_AP_ITEM_BASE;
    if (rel < 0) {
        return false;
    }
    int64_t game = rel >> 16;
    if (game != (int64_t)GAME_OOT && game != (int64_t)GAME_MM) {
        return false;
    }
    *outGame = (GameId)game;
    *outItemId = (uint16_t)(rel & 0xFFFF);
    return true;
}
