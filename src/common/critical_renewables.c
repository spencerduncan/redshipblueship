/**
 * @file critical_renewables.c
 * @brief OoTMM's critical-renewables rule under All Locations. See
 *        critical_renewables.h for the rule, the OoTMM lines it ports and the
 *        runtime facts each game's predicate rests on.
 *
 * Game-header-free: each game answers through its registered source; the
 * coordinator (combo_logic.c) and both games' remainder passes ask here.
 */

#include "critical_renewables.h"

#include <stdio.h>

static const ComboCriticalRenewableSource* sCritSources[RSBS_FOREIGN_POOL_ORIGIN_COUNT];

static bool CritIsGame(uint8_t game) {
    return game == (uint8_t)GAME_OOT || game == (uint8_t)GAME_MM;
}

bool Combo_RegisterCriticalRenewableSource(uint8_t game, const ComboCriticalRenewableSource* source) {
    if (!CritIsGame(game)) {
        fprintf(stderr, "[CriticalRenewables] registration REFUSED: %u is not a real game\n", (unsigned)game);
        return false;
    }
    if (source == NULL || source->abiVersion != RSBS_CRITICAL_RENEWABLE_SOURCE_ABI || source->kindOf == NULL ||
        source->hostRenews == NULL || source->kindsRenewedNow == NULL) {
        fprintf(stderr, "[CriticalRenewables] registration REFUSED for %s: a NULL or malformed source\n",
                Game_ToString((GameId)game));
        return false;
    }
    if (sCritSources[game] != NULL) {
        fprintf(stderr, "[CriticalRenewables] registration REFUSED for %s: a source is already registered\n",
                Game_ToString((GameId)game));
        return false;
    }
    sCritSources[game] = source;
    return true;
}

const ComboCriticalRenewableSource* Combo_GetCriticalRenewableSource(uint8_t game) {
    return CritIsGame(game) ? sCritSources[game] : NULL;
}

const ComboCriticalRenewableSource* Combo_TestSwapCriticalRenewableSource(uint8_t game,
                                                                          const ComboCriticalRenewableSource* source) {
    if (!CritIsGame(game)) {
        return NULL;
    }
    const ComboCriticalRenewableSource* prior = sCritSources[game];
    sCritSources[game] = source;
    fprintf(stderr, "[CriticalRenewables] TEST: %s's source swapped (%s -> %s)\n", Game_ToString((GameId)game),
            prior != NULL ? "set" : "none", source != NULL ? "set" : "none");
    return prior;
}

bool Combo_CriticalRenewablesArmed(uint8_t logicRung) {
    return logicRung == (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE;
}

int Combo_CriticalRenewableKind(SharedItem item) {
    const ComboCriticalRenewableSource* src = Combo_GetCriticalRenewableSource(item.originGame);
    if (src == NULL) {
        return -1;
    }
    const int kind = src->kindOf(item.id);
    return (kind >= 0 && kind < RSBS_CRIT_KIND_CAP) ? kind : -1;
}

bool Combo_CriticalRenewableHostRenews(uint8_t hostGame, uint16_t host) {
    const ComboCriticalRenewableSource* src = Combo_GetCriticalRenewableSource(hostGame);
    return src != NULL && src->hostRenews(host) != 0;
}

void Combo_CriticalRenewablesCouple(uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT]) {
    if (!Combo_ComboCrossItemRule(RSBS_XITEM_SHARED_SHIELDS)) {
        return;
    }
    const uint32_t hylian = 1u << RSBS_CRIT_OOT_HYLIAN_SHIELD;
    const uint32_t hero = 1u << RSBS_CRIT_MM_HERO_SHIELD;
    if ((renewed[GAME_OOT] & hylian) != 0u || (renewed[GAME_MM] & hero) != 0u) {
        renewed[GAME_OOT] |= hylian;
        renewed[GAME_MM] |= hero;
    }
}

void Combo_CriticalRenewablesRenewedNow(uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT]) {
    for (int g = 0; g < (int)RSBS_FOREIGN_POOL_ORIGIN_COUNT; ++g) {
        const ComboCriticalRenewableSource* src = Combo_GetCriticalRenewableSource((uint8_t)g);
        renewed[g] = (src != NULL) ? src->kindsRenewedNow() : 0u;
    }
    Combo_CriticalRenewablesCouple(renewed);
}

bool Combo_CriticalRenewablePending(SharedItem item, const uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT]) {
    const int kind = Combo_CriticalRenewableKind(item);
    return kind >= 0 && (renewed[item.originGame] & (1u << kind)) == 0u;
}

bool Combo_CriticalRenewableServes(uint8_t hostGame, uint16_t host, SharedItem item) {
    // A foreign item is delivered ONCE by any host (the crossing store), so only
    // the item's own game can renew it.
    return CritIsGame(hostGame) && item.originGame == hostGame && Combo_CriticalRenewableKind(item) >= 0 &&
           Combo_CriticalRenewableHostRenews(hostGame, host);
}

void Combo_CriticalRenewablesNote(uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT], uint8_t hostGame, uint16_t host,
                                  SharedItem item) {
    if (!Combo_CriticalRenewableServes(hostGame, host, item)) {
        return;
    }
    renewed[hostGame] |= 1u << Combo_CriticalRenewableKind(item);
    Combo_CriticalRenewablesCouple(renewed);
}

int Combo_CriticalRenewablesPlanFirstCopies(uint8_t game, const uint16_t* items, int itemCount, const uint16_t* hosts,
                                            int hostCount, uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT],
                                            int* outItemIdx, int* outHostIdx, int cap, uint32_t* outUnhosted) {
    *outUnhosted = 0u;
    if (!CritIsGame(game) || items == NULL || itemCount <= 0) {
        return 0;
    }
    // Hosts already paired, one bit per host index. A pass's leftover hosts are a
    // game's check pool at most (MM 2258, OoT 2528 checks), so a fixed bitset
    // sized past both keeps this allocation-free.
    enum { kHostBits = 4096 };
    uint32_t used[kHostBits / 32] = { 0 };
    int pairs = 0;
    for (int i = 0; i < itemCount && pairs < cap; ++i) {
        SharedItem row;
        row.originGame = game;
        row.flags = 0u;
        row.id = items[i];
        if (!Combo_CriticalRenewablePending(row, renewed)) {
            continue;
        }
        const int kind = Combo_CriticalRenewableKind(row);
        if ((*outUnhosted & (1u << kind)) != 0u) {
            continue; // no host renewed this kind's first copy; none will its next
        }
        int chosen = -1;
        for (int h = 0; h < hostCount && h < kHostBits && chosen < 0; ++h) {
            if ((used[h / 32] & (1u << (h % 32))) == 0u && Combo_CriticalRenewableServes(game, hosts[h], row)) {
                chosen = h;
            }
        }
        if (chosen < 0) {
            *outUnhosted |= 1u << kind;
            continue;
        }
        used[chosen / 32] |= 1u << (chosen % 32);
        outItemIdx[pairs] = i;
        outHostIdx[pairs] = chosen;
        pairs++;
        Combo_CriticalRenewablesNote(renewed, game, hosts[chosen], row);
    }
    return pairs;
}

const char* Combo_CriticalRenewableKindName(uint8_t game, int kind) {
    if (game == (uint8_t)GAME_OOT) {
        switch (kind) {
            case RSBS_CRIT_OOT_DEKU_SHIELD:
                return "Deku Shield";
            case RSBS_CRIT_OOT_HYLIAN_SHIELD:
                return "Hylian Shield";
            default:
                break;
        }
    } else if (game == (uint8_t)GAME_MM) {
        switch (kind) {
            case RSBS_CRIT_MM_HERO_SHIELD:
                return "Hero's Shield";
            case RSBS_CRIT_MM_BOMBCHU_10:
                return "10 Bombchus";
            case RSBS_CRIT_MM_MAGIC_BEAN:
                return "Magic Bean";
            case RSBS_CRIT_MM_RED_POTION:
                return "Red Potion";
            case RSBS_CRIT_MM_BLUE_POTION:
                return "Blue Potion";
            case RSBS_CRIT_MM_MILK:
                return "Milk";
            default:
                break;
        }
    }
    return "(unknown)";
}
