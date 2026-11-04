/**
 * @file critical_renewables.h
 * @brief OoTMM's CRITICAL-RENEWABLES rule under All Locations: the FIRST copy of
 *        each critical renewable lands where the player can get it again.
 *
 * ============================================================================
 * WHAT OoTMM DOES
 * ============================================================================
 *
 * packages/logic/src/items/groups.ts:535 `CRITICAL_RENEWABLES` names twelve items:
 * MM's Hero's Shield, 10 Bombchus, Magic Bean, Red Potion, Blue Potion and Milk;
 * OoT's Deku and Hylian Shields; and the SHARED_* forms of the potions and the
 * shields. packages/logic/src/solver/solve.ts places them under one rule, in both
 * of its fills (forwardFill :1101, randomAssumed :1149):
 *
 *     if (settings.logic === 'allLocations' && isItemCriticalRenewable(item)
 *         && !state.criticalRenewables.has(item))
 *       unplacedLocs = unplacedLocs.filter(x => isLocationRenewable(x));
 *
 * and `place` (:1249) adds the item to `criticalRenewables` the moment ANY copy of
 * it lands on a renewable location, fixed placements included (vanilla shops are
 * fixed through `place` too). So: only under All Locations, the first copy of each
 * critical renewable must land on a renewable location, and every later copy may
 * land anywhere. Beatable Only and No Logic are untouched. Why it exists: OoTMM's
 * logic reads these items through `renewable(X)` (data/macros/macros_*.yml,
 * `has_shield`, `bombchu_source`, `has_beans`, `has_red_potion`, `has_milk`), and
 * All Locations promises every location, including the ones that consume them.
 *
 * `isLocationRenewable` (packages/logic/src/locations.ts:90-109) is a list: MM's
 * three business scrubs, its milk merchants and its Tingle maps, OoT's potion
 * merchant, milk and carpet merchants, and every check of type shop, cow, scrub,
 * fairy, fish, fairy-spot, gossip or gossip-big, minus three one-time shop checks
 * (MM's two Bomb Shop bomb bags and the All-Night Mask).
 *
 * ============================================================================
 * WHAT THIS DOES, AND WHERE OUR RUNTIME DIFFERS
 * ============================================================================
 *
 * OoTMM's list is a list of places ITS runtime re-gives. Ours re-gives less, so
 * each game's predicate here (`hostRenews`) is OoTMM's list INTERSECTED WITH what
 * this port actually does when the player comes back, and the PR that added this
 * file cites the runtime line for every exclusion:
 *
 *   OoT. A shuffled shop shelf sells its item ONCE (z_en_girla.c: the slot's
 *        RandomizerInf turns it SI_SOLD_OUT), cows, scrubs and merchants give a
 *        shuffled item once (RAND_INF_COWS_MILKED_*, VB_BUSINESS_SCRUB_DESPAWN,
 *        the merchants' sold-out hooks). What renews is a shelf that keeps its
 *        VANILLA stock (an ITEMTYPE_SHOP `RG_BUY_*` row): SoH caps the shop count
 *        at seven (settings.cpp RSK_SHOPSANITY_COUNT 0..7, shops.cpp's minimum
 *        list puts both shields first), so both shields are always on a vanilla
 *        shelf. OoT's two kinds are therefore renewed before any fill runs, which
 *        is exactly OoTMM with vanilla shops (fixed placements through `place`).
 *   MM.  A shuffled shop shelf restocks whatever is still obtainable
 *        (EnGirlA_RandoRestock, ShopShelfSlotSold), a counter slot (milk bar,
 *        Gorman) and a cow give their item again every cycle (cycleObtained),
 *        and Tingle sells a map again while it is obtainable (TingleMapSlotSold).
 *        Those are MM's renewable hosts: shop (minus OoTMM's three one-time
 *        checks), Tingle map, cow. A shuffled business scrub sells its shuffled
 *        item ONCE (EnAkindonuts: RANDO_INF_PURCHASED_*), so it is not a host;
 *        but afterwards it sells its vanilla goods again, as an unshuffled one
 *        does, so its VANILLA item still counts as renewed (beans, blue potion),
 *        and so do a cow's milk and every unshuffled renewable check's vanilla
 *        item.
 *   A FOREIGN item is delivered once by any host (the crossing store), so a
 *   critical renewable's first copy may only land on its OWN game's hosts.
 *   SHARED SHIELDS (#1068): the Hylian and Hero's Shields are ONE item, so a
 *   renewed Hylian Shield renews the Hero's Shield and back.
 *
 * WHERE IT RUNS. Three places fill a paired world under All Locations, and the
 * rule is in all three (ADR 0010's bag model, shape 4):
 *   1. the single-bag coordinator (combo_logic.c), for the rows the bag takes
 *      (MM's Hero's Shield and Magic Bean are progression to MM's fill);
 *   2. MM's own pass over its leftover hosts (Rando::Foreign::RunPairedSingleBagFill),
 *      for MM's renewable-class rows (bombchus, potions, milk);
 *   3. OoT's remainder (RsbsFinishPairedGeneralPass), for OoT's shields.
 * The remainders share Combo_CriticalRenewablesPlanFirstCopies. None of the three
 * draws a random number for the rule: when no first copy is pending the fills are
 * the fills they were, value for value.
 *
 * Game-header-free. Each game registers ONE source from its combo-logic engine TU.
 */
#ifndef RSBS_COMMON_CRITICAL_RENEWABLES_H
#define RSBS_COMMON_CRITICAL_RENEWABLES_H

#include "context.h"       // SharedItem
#include "foreign_items.h" // RSBS_FOREIGN_POOL_ORIGIN_COUNT, RSBS_COMBO_RUNG_*

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The source ABI. Pinned; a source with any other value is refused. */
#define RSBS_CRITICAL_RENEWABLE_SOURCE_ABI 1u

/** OoT's critical renewables (OoTMM OOT_SHIELD_DEKU, OOT_SHIELD_HYLIAN). The kind
 *  is a bit index into one game's renewed mask. Pinned values, append-only. */
#define RSBS_CRIT_OOT_DEKU_SHIELD 0
#define RSBS_CRIT_OOT_HYLIAN_SHIELD 1
#define RSBS_CRIT_OOT_KIND_COUNT 2

/** MM's critical renewables (OoTMM MM_SHIELD_HERO, MM_BOMBCHU_10, MM_MAGIC_BEAN,
 *  MM_POTION_RED, MM_POTION_BLUE, MM_MILK). Pinned values, append-only. */
#define RSBS_CRIT_MM_HERO_SHIELD 0
#define RSBS_CRIT_MM_BOMBCHU_10 1
#define RSBS_CRIT_MM_MAGIC_BEAN 2
#define RSBS_CRIT_MM_RED_POTION 3
#define RSBS_CRIT_MM_BLUE_POTION 4
#define RSBS_CRIT_MM_MILK 5
#define RSBS_CRIT_MM_KIND_COUNT 6

/** No game has more kinds than this (one bit each of a uint32_t mask). */
#define RSBS_CRIT_KIND_CAP 16

/**
 * What one game answers, in its own id spaces.
 *
 *   kindOf           the critical-renewable kind of item @p id (this game's item
 *                    ids), or -1 when it is not one. An item that IS the same
 *                    thing in play (OoT's RG_BUY_DEKU_SHIELD shelf row and its
 *                    RG_DEKU_SHIELD) answers the same kind.
 *   hostRenews       1 when check @p hostCheck, holding one of THIS game's items,
 *                    gives it again when the player comes back; 0 otherwise. For
 *                    a host whose answer depends on what it holds (an OoT shelf
 *                    renews only its vanilla stock), the answer is about what it
 *                    holds NOW, and an empty host answers for a shuffled item.
 *   kindsRenewedNow  the kinds (bit k = kind k) this game ALREADY renews from
 *                    what stands in its world: vanilla renewable checks the fill
 *                    does not own, vanilla goods a host sells again after its
 *                    shuffled item, and placements this game's engine holds on
 *                    renewable hosts. Read-only; draws no random number.
 */
typedef struct ComboCriticalRenewableSource {
    uint32_t abiVersion; // RSBS_CRITICAL_RENEWABLE_SOURCE_ABI
    int (*kindOf)(uint16_t id);
    int (*hostRenews)(uint16_t hostCheck);
    uint32_t (*kindsRenewedNow)(void);
} ComboCriticalRenewableSource;

/** Register @p game's source, once. Refused (false, logged) for a non-game, a
 *  NULL or malformed source, or a game that already has one. */
bool Combo_RegisterCriticalRenewableSource(uint8_t game, const ComboCriticalRenewableSource* source);

/** The registered source for @p game, or NULL. */
const ComboCriticalRenewableSource* Combo_GetCriticalRenewableSource(uint8_t game);

/** TEST ONLY: put @p source (NULL clears) in @p game's slot and return what was
 *  there, so a lock can install a stub and restore the real source after. Logged. */
const ComboCriticalRenewableSource* Combo_TestSwapCriticalRenewableSource(uint8_t game,
                                                                          const ComboCriticalRenewableSource* source);

/** Is the rule armed for a fill at @p logicRung? Only All Locations
 *  (RSBS_COMBO_RUNG_ALL_REACHABLE), as OoTMM's `settings.logic === 'allLocations'`. */
bool Combo_CriticalRenewablesArmed(uint8_t logicRung);

/** @p item's kind in its own game, or -1 (not critical, or no source). */
int Combo_CriticalRenewableKind(SharedItem item);

/** Does @p hostGame's check @p host give one of its own game's items again? */
bool Combo_CriticalRenewableHostRenews(uint8_t hostGame, uint16_t host);

/** Fill @p renewed (indexed by GameId) with each game's kindsRenewedNow, then
 *  apply the Shared Shields coupling. Zero for a game with no source. */
void Combo_CriticalRenewablesRenewedNow(uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT]);

/** The Shared Shields coupling (#1068): while the frozen rule is on, OoT's Hylian
 *  Shield and MM's Hero's Shield are one item, so each renews the other. */
void Combo_CriticalRenewablesCouple(uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT]);

/** Is @p item a critical renewable whose kind is NOT yet renewed in its game? Such
 *  a copy may only land on a host where Combo_CriticalRenewableServes holds. */
bool Combo_CriticalRenewablePending(SharedItem item, const uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT]);

/** Would @p item on @p hostGame's check @p host renew its kind? Its own game's
 *  host, a critical item, and a host that gives it again. */
bool Combo_CriticalRenewableServes(uint8_t hostGame, uint16_t host, SharedItem item);

/** Record @p item on @p hostGame's check @p host: when it serves, its kind (and
 *  its Shared Shields twin) is renewed from now on. */
void Combo_CriticalRenewablesNote(uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT], uint8_t hostGame, uint16_t host,
                                  SharedItem item);

/**
 * THE REMAINDERS' HALF. Given one game's own pass — its rows @p items (that
 * game's ids, in the pass's own order) and its empty hosts @p hosts (in the
 * pass's own order) — choose where each pending first copy goes: walking the rows
 * in order, the first copy of each pending kind is paired with the first unused
 * host that renews it. Draws no random number; the pass's own shuffles already
 * decided both orders. @p renewed is updated as pairs are made.
 *
 * @param outItemIdx, outHostIdx  the pairs (an index into @p items, one into
 *        @p hosts), at most @p cap; the pass deals these first.
 * @param outUnhosted  the kinds with a pending copy and no renewing host left
 *        (bit k = kind k). Never NULL.
 * @return the number of pairs.
 */
int Combo_CriticalRenewablesPlanFirstCopies(uint8_t game, const uint16_t* items, int itemCount, const uint16_t* hosts,
                                            int hostCount, uint32_t renewed[RSBS_FOREIGN_POOL_ORIGIN_COUNT],
                                            int* outItemIdx, int* outHostIdx, int cap, uint32_t* outUnhosted);

/** A kind's name for logs ("Hero's Shield"), or "(unknown)". Never NULL. */
const char* Combo_CriticalRenewableKindName(uint8_t game, int kind);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_CRITICAL_RENEWABLES_H
