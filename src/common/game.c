/**
 * @file game.c
 * @brief Common game definitions for single-executable architecture
 */

#include "game.h"
#include <string.h>

GameId Game_FromString(const char* id) {
    if (id == NULL) {
        return GAME_NONE;
    }
    if (strcmp(id, "oot") == 0) {
        return GAME_OOT;
    }
    if (strcmp(id, "mm") == 0) {
        return GAME_MM;
    }
    return GAME_NONE;
}

const char* Game_ToString(GameId game) {
    switch (game) {
        case GAME_OOT: return "oot";
        case GAME_MM: return "mm";
        default: return NULL;
    }
}

GameId Game_GetOther(GameId game) {
    switch (game) {
        case GAME_OOT: return GAME_MM;
        case GAME_MM: return GAME_OOT;
        default: return GAME_NONE;
    }
}
