#include "StaticData.h"

extern "C" {
#include "overlays/actors/ovl_En_Sth/z_en_sth.h"
}

namespace Rando {

namespace StaticData {

#define RO(id, defaultValue)                             \
    {                                                    \
        id, {                                            \
            id, #id, "gRando.Options." #id, defaultValue \
        }                                                \
    }

// clang-format off
std::map<RandoOptionId, RandoStaticOption> Options = {
    RO(RO_ACCESS_DUNGEONS,             RO_ACCESS_DUNGEONS_FORM_AND_SONG),
    RO(RO_ACCESS_MAJORA_MASKS_COUNT,   0),
    RO(RO_ACCESS_MAJORA_REMAINS_COUNT, 0),
    // 47 ids, 46 rows: RO_ACCESS_MAJORA_REMAINS was declared (Types.h) and never
    // given a row, so the snapshot loop skipped its RANDO_SAVE_OPTIONS slot, the
    // spoiler never recorded it, and every iteration over the id space was
    // partial. #499 step 5 required settling that BEFORE a table-driven surface
    // existed, and the two candidates were "add the row" and "delete the
    // enumerator". Deleting it renumbers the 44 enumerators after it, and
    // RANDO_SAVE_OPTIONS is indexed BY that number in every already-written MM
    // rando save — a silent value shift across a save format, to tidy dead
    // surface. Adding the row costs one always-zero slot and makes the id space
    // total.
    //
    // RETIRED (operator-accepted, ADR 0010 answer O1, 2026-07-31): this option
    // never gains a consumer and never becomes a working control. The row is a
    // TOMBSTONE, kept — not deleted — for THREE stability reasons: (1) the
    // enumerator-renumbering save break above; (2) removing only this map row
    // would shift every paired world's seed derivation (MMOptionsString hashes
    // the table in row order — Foreign.cpp) and (3) every profile identity
    // digest (ProfileIdentityString iterates the same rows), refusing shipped
    // pairs at arrival over a tidy-up. The combo pane keeps drawing it
    // disabled-with-reason (ADR 0004 §5), with the reason now naming the
    // retirement rather than a pending implementation.
    RO(RO_ACCESS_MAJORA_REMAINS,       RO_GENERIC_OFF),
    RO(RO_ACCESS_MOON_MASKS_COUNT,     0),
    RO(RO_ACCESS_MOON_REMAINS_COUNT,   4),
    RO(RO_ACCESS_TRIALS,               RO_ACCESS_TRIALS_20_MASKS),
#ifdef RSBS_SINGLE_EXECUTABLE
    // RedShipBlueShip (#1439): Majora's Mask's hints are on, as OoTMM's are (its
    // gossip stones and its static hints for the remains, Oath to Order and the
    // spider houses; 2Ship's Hookshot hint, another static hint, follows them),
    // closing #575's decision 5. Purchaseable Gossip Hints stay off: a rupee sink
    // OoTMM does not have (packages/core/src/settings/data.ts).
    RO(RO_HINTS_BOSS_REMAINS,          RO_GENERIC_ON),
    RO(RO_HINTS_GOSSIP_STONES,         RO_GENERIC_ON),
    RO(RO_HINTS_HOOKSHOT,              RO_GENERIC_ON),
    RO(RO_HINTS_OATH_TO_ORDER,         RO_GENERIC_ON),
    RO(RO_HINTS_PURCHASEABLE,          RO_GENERIC_OFF),
    RO(RO_HINTS_SPIDER_HOUSES,         RO_GENERIC_ON),
#else
    RO(RO_HINTS_BOSS_REMAINS,          RO_GENERIC_OFF),
    RO(RO_HINTS_GOSSIP_STONES,         RO_GENERIC_OFF),
    RO(RO_HINTS_HOOKSHOT,              RO_GENERIC_OFF),
    RO(RO_HINTS_OATH_TO_ORDER,         RO_GENERIC_OFF),
    RO(RO_HINTS_PURCHASEABLE,          RO_GENERIC_OFF),
    RO(RO_HINTS_SPIDER_HOUSES,         RO_GENERIC_OFF),
#endif
    RO(RO_TRAP_AMOUNT,                 5),
    RO(RO_LOGIC,                       RO_LOGIC_GLITCHLESS),
    RO(RO_MINIMUM_SKULLTULA_TOKENS,    SPIDER_HOUSE_TOKENS_REQUIRED),
    RO(RO_MINIMUM_STRAY_FAIRIES,       STRAY_FAIRY_SCATTERED_TOTAL),
    RO(RO_PLENTIFUL_ITEMS,             RO_GENERIC_OFF),
    RO(RO_SHUFFLE_BARREL_DROPS,        RO_GENERIC_OFF),
    RO(RO_SHUFFLE_BOSS_REMAINS,        RO_GENERIC_OFF),
    RO(RO_SHUFFLE_BOSS_SOULS,          RO_GENERIC_OFF),
    RO(RO_SHUFFLE_COWS,                RO_GENERIC_OFF),
    RO(RO_SHUFFLE_CRATE_DROPS,         RO_GENERIC_OFF),
    RO(RO_SHUFFLE_ENEMY_DROPS,         RO_GENERIC_OFF),
    RO(RO_SHUFFLE_ENEMY_SOULS,         RO_GENERIC_OFF),
    RO(RO_SHUFFLE_FREESTANDING_ITEMS,  RO_GENERIC_OFF),
    RO(RO_SHUFFLE_FROGS,               RO_GENERIC_OFF),
    RO(RO_SHUFFLE_GOLD_SKULLTULAS,     RO_GENERIC_OFF),
    RO(RO_SHUFFLE_GRASS_DROPS,         RO_GENERIC_OFF),
    RO(RO_SHUFFLE_TRAPS,               RO_GENERIC_OFF),
    RO(RO_SHUFFLE_OWL_STATUES,         RO_GENERIC_OFF),
    RO(RO_SHUFFLE_OCARINA_BUTTONS,     RO_GENERIC_OFF),
    RO(RO_SHUFFLE_POT_DROPS,           RO_GENERIC_OFF),
    RO(RO_SHUFFLE_SHOPS,               RO_GENERIC_OFF),
	RO(RO_SHUFFLE_SNOWBALL_DROPS,      RO_GENERIC_OFF),
    RO(RO_SHUFFLE_SWIM,                RO_GENERIC_OFF),
    RO(RO_SHUFFLE_TINGLE_SHOPS,        RO_GENERIC_OFF),
    RO(RO_SHUFFLE_TRIFORCE_PIECES,     RO_GENERIC_OFF),
    RO(RO_STARTING_CONSUMABLES,        RO_GENERIC_OFF),
    RO(RO_STARTING_HEALTH,             3),
    RO(RO_STARTING_MAPS_AND_COMPASSES, RO_GENERIC_OFF),
    RO(RO_STARTING_RUPEES,             RO_GENERIC_OFF),
    RO(RO_TRIFORCE_PIECES_MAX,         DEFAULT_TRIFORCE_PIECES_MAX),
    RO(RO_TRIFORCE_PIECES_REQUIRED,    DEFAULT_TRIFORCE_PIECES_MAX),
    RO(RO_CLOCK_SHUFFLE,               RO_GENERIC_OFF),
#ifdef RSBS_SINGLE_EXECUTABLE
    // RedShipBlueShip #1437: OoTMM's progressiveClocks default (data.ts). While
    // Shuffle Time is off the mode is inert and an unset key resolves to Random,
    // the value every world before this froze (Foreign.cpp, ResolveProfileValues).
    RO(RO_CLOCK_SHUFFLE_PROGRESSIVE,   RO_CLOCK_SHUFFLE_ASCENDING),
#else
    RO(RO_CLOCK_SHUFFLE_PROGRESSIVE,   RO_CLOCK_SHUFFLE_RANDOM),
#endif
    RO(RO_CLOCK_TERMINAL_TIME,         0), // Default: 00:00 (midnight)
    RO(RO_SHUFFLE_TREE_DROPS,          RO_GENERIC_OFF),
    RO(RO_SHOP_PRICES,                 RO_PRICES_VANILLA), // OoTMM priceMmShops' default (#1427)
    RO(RO_TINGLE_PRICES,               RO_PRICES_VANILLA), // OoTMM priceMmTingle's default (#1427)
    RO(RO_PLACEMENT_BOSS_KEYS,         RO_DUNGEON_ITEM_ANYWHERE),
    RO(RO_PLACEMENT_SMALL_KEYS,        RO_DUNGEON_ITEM_ANYWHERE),
    RO(RO_PLACEMENT_STRAY_FAIRIES,     RO_DUNGEON_ITEM_ANYWHERE),
    RO(RO_SHUFFLE_SONGS,               RO_SONG_SHUFFLE_ANYWHERE),
};
// clang-format on

RandoOptionId GetOptionIdFromName(const char* name) {
    for (auto& [randoOptionId, randoStaticOption] : Options) {
        if (strcmp(name, randoStaticOption.name) == 0) {
            return randoOptionId;
        }
    }
    return RO_MAX;
}

} // namespace StaticData

} // namespace Rando
