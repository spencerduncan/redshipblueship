#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include "2s2h/Rando/Logic/Logic.h"

using namespace Rando::Logic;

// clang-format off
static RegisterShipInitFunc initFunc([]() {
    // Rightside Temple.
    Regions[RR_STONE_TOWER_TEMPLE_ENTRANCE] = RandoRegion{ .name = "Entrace", .sceneId = SCENE_INISIE_N,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_ENTRANCE_CHEST, HAS_ITEM(ITEM_BOW)),
            CHECK(RC_STONE_TOWER_TEMPLE_ENTRANCE_SMALL_CRATE_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_ENTRANCE_SMALL_CRATE_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_ENTRANCE_SWITCH_CHEST, RANDO_EVENTS[RE_STONE_TOWER_TEMPLE_ENTRANCE_SWITCH_CHEST]),
            CHECK(RC_STONE_TOWER_TEMPLE_ENTRANCE_POT_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_ENTRANCE_POT_02, true),
            CHECK(RC_ENEMY_DROP_DRAGONFLY, CanKillEnemy(ACTOR_EN_GRASSHOPPER)),
        },
        .exits = { //     TO                                         FROM
            EXIT(ENTRANCE(STONE_TOWER, 2),                  ENTRANCE(STONE_TOWER_TEMPLE, 0), true),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_SWITCH_ROOM, true),
            CONNECTION(RR_STONE_TOWER_TEMPLE_DEEP_POOL_ROOM, (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR) || CAN_USE_MAGIC_ARROW(LIGHT)),
            CONNECTION(RR_STONE_TOWER_TEMPLE_BRIDGE, false) //This is one way, not sure how to go about doing this with a connection.
        },
        .events = {
            EVENT(RE_STONE_TOWER_STRAY_FAIRY_INVERTED_ENTRANCE, CAN_USE_MAGIC_ARROW(LIGHT)),
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_SWITCH_ROOM] = RandoRegion{ .name = "Switch Room", .sceneId = SCENE_INISIE_N,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_LARGE_CRATE_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_LARGE_CRATE_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_LARGE_CRATE_03, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_LARGE_CRATE_04, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_LARGE_CRATE_05, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_SMALL_CRATE_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SWITCH_ROOM_SMALL_CRATE_02, true),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_ENTRANCE, true),
            CONNECTION(RR_STONE_TOWER_TEMPLE_OUTSIDE_SWITCH_ROOM, CAN_BE_GORON && CAN_PLAY_SONG(ELEGY) && CAN_USE_EXPLOSIVE)
        },
    };
    Regions[RR_STONE_TOWER_TEMPLE_OUTSIDE_SWITCH_ROOM] = RandoRegion{ .name = "Outside of Switch Room",  .sceneId = SCENE_INISIE_N,
        .checks = {
            CHECK(RC_ENEMY_DROP_DRAGONFLY, CanKillEnemy(ACTOR_EN_GRASSHOPPER)),
            CHECK(RC_ENEMY_DROP_GUAY, CanKillEnemy(ACTOR_EN_CROW)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_SWITCH_ROOM, false),
            CONNECTION(RR_STONE_TOWER_TEMPLE_ARMOS_ROOM, true),
            CONNECTION(RR_STONE_TOWER_TEMPLE_SHALLOW_POOL_ROOM, KEY_COUNT(STONE_TOWER_TEMPLE) >= 1),
        },
    };
    Regions[RR_STONE_TOWER_TEMPLE_ARMOS_ROOM] = RandoRegion{ .name = "Armos Room", .sceneId = SCENE_INISIE_N,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_UNDER_WEST_GARDEN_LEDGE_CHEST, HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_STONE_TOWER_TEMPLE_UNDER_WEST_GARDEN_LAVA_CHEST, CAN_BE_GORON && ((CAN_USE_EXPLOSIVE && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR)) || CAN_USE_MAGIC_ARROW(LIGHT))),
            CHECK(RC_STONE_TOWER_TEMPLE_LAVA_ROOM_POT_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_LAVA_ROOM_POT_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_LAVA_ROOM_POT_03, true),
            CHECK(RC_STONE_TOWER_TEMPLE_LAVA_ROOM_POT_04, true),
            CHECK(RC_STONE_TOWER_TEMPLE_MAP_CHEST, CAN_BE_GORON && ((CAN_USE_EXPLOSIVE && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR)) || CAN_USE_MAGIC_ARROW(LIGHT))),
            CHECK(RC_STONE_TOWER_TEMPLE_LAVA_ROOM_AFTER_BLOCK_POT_01, CAN_BE_GORON && ((CAN_USE_EXPLOSIVE && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR)) || CAN_USE_MAGIC_ARROW(LIGHT))),
            CHECK(RC_STONE_TOWER_TEMPLE_LAVA_ROOM_AFTER_BLOCK_POT_02, CAN_BE_GORON && ((CAN_USE_EXPLOSIVE && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR)) || CAN_USE_MAGIC_ARROW(LIGHT))),
            CHECK(RC_STONE_TOWER_TEMPLE_LAVA_ROOM_AFTER_BLOCK_POT_03, CAN_BE_GORON && ((CAN_USE_EXPLOSIVE && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR)) || CAN_USE_MAGIC_ARROW(LIGHT))),
            CHECK(RC_STONE_TOWER_TEMPLE_LAVA_ROOM_AFTER_BLOCK_POT_04, CAN_BE_GORON && ((CAN_USE_EXPLOSIVE && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR)) || CAN_USE_MAGIC_ARROW(LIGHT))),
            CHECK(RC_ENEMY_DROP_ARMOS, CanKillEnemy(ACTOR_EN_AM))
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_OUTSIDE_SWITCH_ROOM, true)
        },
    };
    Regions[RR_STONE_TOWER_TEMPLE_SHALLOW_POOL_ROOM] = RandoRegion{ .name = "Shallow Pool Room", .sceneId = SCENE_INISIE_N,
        .checks = {
            // IF you get this check you must be Zora to leave, but you could in theory get this as Human too without the Zora mask...kinda iffy to me
            CHECK(RC_STONE_TOWER_TEMPLE_CENTER_ACROSS_WATER_CHEST, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_STONE_TOWER_TEMPLE_CENTER_SUN_BLOCK_CHEST, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM) && CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_STONE_TOWER_TEMPLE_CENTER_FREESTANDING_RUPEE_01, CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_STONE_TOWER_TEMPLE_CENTER_FREESTANDING_RUPEE_02, CAN_USE_MAGIC_ARROW(LIGHT) && CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_STONE_TOWER_TEMPLE_CENTER_SMALL_CRATE_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_CENTER_SMALL_CRATE_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_CENTER_SMALL_CRATE_03, true),
            CHECK(RC_ENEMY_DROP_BEAMOS, CanKillEnemy(ACTOR_EN_VM)),
            CHECK(RC_ENEMY_DROP_DEXIHAND, CanKillEnemy(ACTOR_EN_WDHAND)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_OUTSIDE_SWITCH_ROOM, KEY_COUNT(STONE_TOWER_TEMPLE) >= 1),
            CONNECTION(RR_STONE_TOWER_TEMPLE_DEEP_POOL_ROOM_UNDERWATER, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM))
        },
    };
    Regions[RR_STONE_TOWER_TEMPLE_DEEP_POOL_ROOM] = RandoRegion{ .name = "Deep Pool Room", .sceneId = SCENE_INISIE_N,
        .checks = {
            // TODO : Go back and add stay fairy chest that spawns from inverted.
            CHECK(RC_STONE_TOWER_TEMPLE_COMPASS_CHEST, (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR) || CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_STONE_TOWER_TEMPLE_WATER_ROOM_BRIDGE_POT_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_WATER_ROOM_BRIDGE_POT_02, true),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_DEEP_POOL_ROOM_UNDERWATER, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CONNECTION(RR_STONE_TOWER_TEMPLE_ENTRANCE, (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR) || CAN_USE_MAGIC_ARROW(LIGHT)),
            CONNECTION(RR_STONE_TOWER_TEMPLE_MIRROR_PILLAR_ROOM, KEY_COUNT(STONE_TOWER_TEMPLE) >= 2),
        },
        .events = {
            // Can only be shot at an angle from the water, and can only use arrows there if standing on ice platform
            EVENT(RE_STONE_TOWER_TEMPLE_DEEP_POOL_SUN_SWITCH, CAN_USE_MAGIC_ARROW(LIGHT) && CAN_USE_MAGIC_ARROW(ICE)),
        },
    };
    Regions[RR_STONE_TOWER_TEMPLE_DEEP_POOL_ROOM_UNDERWATER] = RandoRegion{ .name = "Deep Pool Room Underwater", .sceneId = SCENE_INISIE_N,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_WATER_ROOM_UNDERWATER_LOWER_POT_01, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_STONE_TOWER_TEMPLE_WATER_ROOM_UNDERWATER_LOWER_POT_02, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_STONE_TOWER_TEMPLE_WATER_ROOM_UNDERWATER_LOWER_POT_03, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_STONE_TOWER_TEMPLE_WATER_ROOM_UNDERWATER_UPPER_POT_01, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_STONE_TOWER_TEMPLE_WATER_ROOM_UNDERWATER_UPPER_POT_02, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_STONE_TOWER_TEMPLE_WATER_SUN_SWITCH_CHEST, RANDO_EVENTS[RE_STONE_TOWER_TEMPLE_DEEP_POOL_SUN_SWITCH] && CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_ENEMY_DROP_BIO_DEKU_BABA, CanKillEnemy(ACTOR_BOSS_05)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_SHALLOW_POOL_ROOM, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CONNECTION(RR_STONE_TOWER_TEMPLE_DEEP_POOL_ROOM, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
        },
    };
    Regions[RR_STONE_TOWER_TEMPLE_MIRROR_PILLAR_ROOM] = RandoRegion{ .name = "Mirror Pillar Room", .sceneId = SCENE_INISIE_N,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_MIRRORS_ROOM_CENTER_CHEST, (CAN_BE_GORON && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR)) || CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_STONE_TOWER_TEMPLE_MIRRORS_ROOM_RIGHT_CHEST, (CAN_BE_GORON && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR)) || CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_STONE_TOWER_TEMPLE_MIRROR_ROOM_POT_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_MIRROR_ROOM_POT_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_MIRRORS_ROOM_LARGE_CRATE_01, (CAN_BE_GORON && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR)) || CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_STONE_TOWER_TEMPLE_MIRRORS_ROOM_LARGE_CRATE_02, (CAN_BE_GORON && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR)) || CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_ENEMY_DROP_NEJIRON, CanKillEnemy(ACTOR_EN_BAGUO)),
            CHECK(RC_ENEMY_DROP_BOE, CanKillEnemy(ACTOR_EN_MKK)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_DEEP_POOL_ROOM, KEY_COUNT(STONE_TOWER_TEMPLE) >= 2),
            CONNECTION(RR_STONE_TOWER_TEMPLE_LAVA_WIND_ROOM, (CAN_BE_GORON && (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MIRROR)) || CAN_USE_MAGIC_ARROW(LIGHT))
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_LAVA_WIND_ROOM] = RandoRegion{ .name = "Lava Wind Room", .sceneId = SCENE_INISIE_N,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_LEDGE_CHEST, CAN_BE_DEKU),
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_JAIL_CHEST, CAN_BE_GORON && (CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(LIGHT))),
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_FREESTANDING_RUPEE_01, CAN_BE_DEKU),
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_FREESTANDING_RUPEE_02, CAN_BE_DEKU),
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_FREESTANDING_RUPEE_03, CAN_BE_DEKU),
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_FREESTANDING_RUPEE_04, CAN_BE_DEKU),
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_FREESTANDING_RUPEE_05, CAN_BE_DEKU),
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_FREESTANDING_RUPEE_06, CAN_BE_DEKU),
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_POT_01, CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_POT_02, CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_POT_03, CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_STONE_TOWER_TEMPLE_WIND_ROOM_POT_04, CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(LIGHT)),
        },
        .connections = {
            // The end of this is a one way exit, so logically you should be able to return without issue.
            CONNECTION(RR_STONE_TOWER_TEMPLE_MIRROR_PILLAR_ROOM, true),
            // Literally only just now found the hidden sun switch to trigger the ladder spawn, wild. Look above the fire switch in the lava that spawns the chest.
            CONNECTION(RR_STONE_TOWER_TEMPLE_GARO_MASTER_ROOM, CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(LIGHT))
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_GARO_MASTER_ROOM] = RandoRegion{ .name = "Garo Master Room", .sceneId = SCENE_INISIE_N,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_LIGHT_ARROW_CHEST, CanKillEnemy(ACTOR_EN_JSO2)),
            CHECK(RC_ENEMY_DROP_GARO_MASTER, CanKillEnemy(ACTOR_EN_JSO2)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_LAVA_WIND_ROOM, CanKillEnemy(ACTOR_EN_JSO2)),
            CONNECTION(RR_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_UPPER, CanKillEnemy(ACTOR_EN_JSO2))
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_LOWER] = RandoRegion{ .name = "Spiked Bar Room Lower", .sceneId = SCENE_INISIE_N,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_FREESTANDING_RUPEE_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_FREESTANDING_RUPEE_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_FREESTANDING_RUPEE_03, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_FREESTANDING_RUPEE_04, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_FREESTANDING_RUPEE_05, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_FREESTANDING_RUPEE_06, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_FREESTANDING_RUPEE_07, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_FREESTANDING_RUPEE_08, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_POT_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_POT_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_POT_03, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_POT_04, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_POT_05, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_POT_06, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_POT_07, true),
            CHECK(RC_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_POT_08, true),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_UPPER, HAS_ITEM(ITEM_HOOKSHOT)),
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_UPPER] = RandoRegion{ .name = "Spiked Bar Room Upper", .sceneId = SCENE_INISIE_N,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_BEFORE_WATER_BRIDGE_CHEST, CAN_USE_EXPLOSIVE),
            CHECK(RC_ENEMY_DROP_HIPLOOP, CanKillEnemy(ACTOR_EN_PP)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_GARO_MASTER_ROOM, HAS_ITEM(ITEM_HOOKSHOT)),
            CONNECTION(RR_STONE_TOWER_TEMPLE_BRIDGE, HAS_ITEM(ITEM_HOOKSHOT)),
            CONNECTION(RR_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_LOWER, true)
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_BRIDGE] = RandoRegion{ .sceneId = SCENE_INISIE_N,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_WATER_BRIDGE_CHEST, CanKillEnemy(ACTOR_EN_EGOL)),
            CHECK(RC_ENEMY_DROP_EYEGORE, CanKillEnemy(ACTOR_EN_EGOL)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_SPIKED_BAR_ROOM_UPPER, true),
            CONNECTION(RR_STONE_TOWER_TEMPLE_ENTRANCE, CanKillEnemy(ACTOR_EN_EGOL))
        }
    };

    // Inverted Temple
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE] = RandoRegion{ .name = "Entrace", .sceneId = SCENE_INISIE_R,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE_CHEST, RANDO_EVENTS[RE_STONE_TOWER_STRAY_FAIRY_INVERTED_ENTRANCE]),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE_SMALL_CRATE_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE_SMALL_CRATE_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE_SMALL_CRATE_03, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE_SMALL_CRATE_04, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE_SMALL_CRATE_05, true),
        },
        .exits = { //     TO                                         FROM
            EXIT(ENTRANCE(STONE_TOWER_INVERTED, 1),         ENTRANCE(STONE_TOWER_TEMPLE_INVERTED, 0), true),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM, CAN_USE_MAGIC_ARROW(LIGHT)),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE_TOP, HAS_ITEM(ITEM_HOOKSHOT) && RANDO_EVENTS[RE_STONE_TOWER_TEMPLE_ENTRANCE_SWITCH_CHEST])
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM] = RandoRegion{ .name = "Wind Room", .sceneId = SCENE_INISIE_R,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_EAST_UPPER_CHEST, CAN_BE_DEKU && CAN_PLAY_SONG(ELEGY)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_EAST_MIDDLE_CHEST, CAN_BE_DEKU),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_EAST_LOWER_CHEST, CAN_BE_DEKU && CAN_USE_MAGIC_ARROW(FIRE)),
            // #578 part 3 (second pass) — MMRT_ST_UPDRAFTS ("Stone Tower Updrafts without Deku Mask —
            // This room can be traversed using Recoil Flips or Goron Mask instead of Deku Mask"),
            // DEFAULT OFF, on the six checks whose own RC names say UPDRAFTS.
            //
            // WHY HERE AND NOT THE OUTDOOR STONE TOWER, since the key's prefix is `ST_` and our area
            // column says MMRTA_STONE_TOWER: by elimination. `Logic/Regions/East.cpp`'s outdoor
            // RR_STONE_TOWER_* regions gate on HOOKSHOT / ELEGY / GORON / ZORA / CAN_GROW_BEAN_PLANT and
            // carry no Deku updraft term at all, so the only updraft vocabulary in the graph is this
            // room's. A binding needs a term the tooltip's "instead of Deku Mask" can name, and these
            // are the only ones.
            //
            // The substitute is CAN_BE_GORON or a plain BOMB, NOT CAN_USE_EXPLOSIVE: a recoil flip is
            // done off a bomb you stand beside, and CAN_USE_EXPLOSIVE also admits Bombchus, the Blast
            // Mask and (under MMRT_KEG_EXPLOSIVES) a Powder Keg, which are not that trick.
            //
            // WHAT IN THIS ROOM IS AND IS NOT WIDENED — exhaustive, because the first pass's note was
            // not. It named ONE un-widened sibling and left three unmentioned, which is worse than
            // saying nothing at all: a reader trusts a note that presents itself as complete. The line
            // drawn, stated as a rule so it can be checked: the tooltip licenses TRAVERSING this room,
            // so the widening covers the checks whose own RC names sit on the updraft structures
            // (UPDRAFTS_BRIDGE_POT_01/02 and UPDRAFTS_LEDGE_POT_01..04, below) and the room's outbound
            // traversal to RR_STONE_TOWER_TEMPLE_INVERTED_LAVA_FLIP_ROOM (widened in .connections below,
            // with its own evidence).
            //
            // NOT widened, all three of them, and recorded on #697 as judgements still owed:
            //   RC_STONE_TOWER_TEMPLE_INVERTED_EAST_UPPER_CHEST  (CAN_BE_DEKU && CAN_PLAY_SONG(ELEGY))
            //   RC_STONE_TOWER_TEMPLE_INVERTED_EAST_MIDDLE_CHEST (bare CAN_BE_DEKU)
            //   RC_STONE_TOWER_TEMPLE_INVERTED_EAST_LOWER_CHEST  (CAN_BE_DEKU && CAN_USE_MAGIC_ARROW(FIRE))
            // — the three rows immediately above. They are DESTINATIONS at particular heights on the
            // east wall rather than the crossing, and nothing in this file ties their Deku term to the
            // updrafts specifically rather than to what a Deku flower does once you are up there (hold
            // a position, hover, aim). That the middle one is textually identical to a widened pot is a
            // coincidence of spelling, not evidence about the route; widening a destination the trick's
            // own text does not name is the over-widening that hands a fill a route a player cannot
            // take, so these three wait for someone who can say which it is.
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_BRIDGE_POT_01, CAN_BE_DEKU || (MM_TRICK(MMRT_ST_UPDRAFTS) && (CAN_BE_GORON || HAS_ITEM(ITEM_BOMB)))),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_BRIDGE_POT_02, CAN_BE_DEKU || (MM_TRICK(MMRT_ST_UPDRAFTS) && (CAN_BE_GORON || HAS_ITEM(ITEM_BOMB)))),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_LEDGE_POT_01, CAN_BE_DEKU || (MM_TRICK(MMRT_ST_UPDRAFTS) && (CAN_BE_GORON || HAS_ITEM(ITEM_BOMB)))),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_LEDGE_POT_02, CAN_BE_DEKU || (MM_TRICK(MMRT_ST_UPDRAFTS) && (CAN_BE_GORON || HAS_ITEM(ITEM_BOMB)))),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_LEDGE_POT_03, CAN_BE_DEKU || (MM_TRICK(MMRT_ST_UPDRAFTS) && (CAN_BE_GORON || HAS_ITEM(ITEM_BOMB)))),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_LEDGE_POT_04, CAN_BE_DEKU || (MM_TRICK(MMRT_ST_UPDRAFTS) && (CAN_BE_GORON || HAS_ITEM(ITEM_BOMB)))),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM_FREESTANDING_RUPEE_01, CAN_USE_MAGIC_ARROW(LIGHT) && HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM_FREESTANDING_RUPEE_02, CAN_USE_MAGIC_ARROW(LIGHT) && HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM_FREESTANDING_RUPEE_03, CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM_FREESTANDING_RUPEE_04, CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM_FREESTANDING_RUPEE_05, CAN_USE_MAGIC_ARROW(LIGHT)),
            CHECK(RC_ENEMY_DROP_HIPLOOP, CanKillEnemy(ACTOR_EN_PP)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE, true),
            // #578 part 3 (second pass) — MMRT_ST_UPDRAFTS again, on this room's outbound TRAVERSAL.
            //
            // THE FIRST PASS SAID THE FILE WAS SILENT ABOUT WHETHER THIS DEKU TERM IS THE UPDRAFT
            // CROSSING OR THE DOOR BEYOND IT. It is not silent, and a reviewer was right to point
            // twelve lines down: the MIRROR of this edge, inside RR_STONE_TOWER_TEMPLE_INVERTED_LAVA_
            // FLIP_ROOM, is `CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM,
            // KEY_COUNT(STONE_TOWER_TEMPLE) >= 3)` with NO Deku term at all. A locked door is symmetric
            // and this graph DOES carry the small-key count symmetrically across the pair; the Deku term
            // is carried in one direction only. A one-way form requirement between two rooms whose door
            // is already modelled by the key count is the climb INSIDE this room — the updraft crossing
            // to the door — which is exactly what the tooltip's "this room can be traversed" relaxes.
            //
            // KEY_COUNT survives OUTSIDE the trick's parentheses, so this is the one binding of this
            // pass that needs, and has, a leg-(f) survivor arm in mm_trick_bindings_test.cpp: Deku
            // satisfied and zero keys must stay shut with the trick both off and on.
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_LAVA_FLIP_ROOM, KEY_COUNT(STONE_TOWER_TEMPLE) >= 3 && (CAN_BE_DEKU || (MM_TRICK(MMRT_ST_UPDRAFTS) && (CAN_BE_GORON || HAS_ITEM(ITEM_BOMB))))),
        },
        .events = {
            EVENT(RE_STONE_TOWER_TEMPLE_DEEP_POOL_SUN_SWITCH, CAN_USE_MAGIC_ARROW(LIGHT)),
        },
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_LAVA_FLIP_ROOM] = RandoRegion{ .name = "Flipped Lava Room", .sceneId = SCENE_INISIE_R,
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM, KEY_COUNT(STONE_TOWER_TEMPLE) >= 3),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_BLOCK_FLIP_ROOM, CAN_BE_GORON && CAN_USE_MAGIC_ARROW(LIGHT))
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_BLOCK_FLIP_ROOM] = RandoRegion{ .name = "Flipped Block Room", .sceneId = SCENE_INISIE_R,
        .checks = {
            CHECK(RC_ENEMY_DROP_CHUCHU, CanKillEnemy(ACTOR_EN_SLIME)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_LAVA_FLIP_ROOM, CAN_USE_MAGIC_ARROW(LIGHT)),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_WIZZROBE_ROOM, CAN_USE_MAGIC_ARROW(LIGHT))
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_WIZZROBE_ROOM] = RandoRegion{ .name = "Wizrobe Room", .sceneId = SCENE_INISIE_R,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_WIZZROBE_CHEST, CanKillEnemy(ACTOR_EN_WIZ) && HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_WIZZROBE_POT_01, HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_WIZZROBE_POT_02, HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_WIZZROBE_POT_03, HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_WIZZROBE_POT_04, HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_WIZZROBE_POT_05, HAS_ITEM(ITEM_HOOKSHOT)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_BLOCK_FLIP_ROOM, CanKillEnemy(ACTOR_EN_WIZ)),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_POE_ROOM, CanKillEnemy(ACTOR_EN_WIZ) && HAS_ITEM(ITEM_HOOKSHOT))
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_POE_ROOM] = RandoRegion{ .name = "Poe Room", .sceneId = SCENE_INISIE_R,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_POE_WIZZROBE_SIDE_POT_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_POE_WIZZROBE_SIDE_POT_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_POE_MAZE_SIDE_POT_01, CAN_BE_DEKU),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_POE_MAZE_SIDE_POT_02, CAN_BE_DEKU),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_DEATH_ARMOS_CHEST, CAN_BE_DEKU && CAN_PLAY_SONG(ELEGY)),
            CHECK(RC_ENEMY_DROP_DEATH_ARMOS, CanKillEnemy(ACTOR_EN_FAMOS)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_WIZZROBE_ROOM, true),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_UNDER_BRIDGE, CAN_BE_DEKU)
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_UNDER_BRIDGE] = RandoRegion{ .name = "Under Bridge", .sceneId = SCENE_INISIE_R,
        .checks = {
            CHECK(RC_ENEMY_DROP_FLYING_POT, CanKillEnemy(ACTOR_EN_TUBO_TRAP)),
            // There is a Dexihand here, but there's no reasonable way to kill it and collect its drop.
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_POE_ROOM, true),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_PATH_TO_GOMESS, CAN_BE_DEKU && (HAS_ITEM(ITEM_HOOKSHOT) || HAS_ITEM(ITEM_BOW) || CAN_BE_ZORA)), // Deku bubbles can work with TIGHT aim, prob better to not consider that in logic
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_SIDE_OF_ENTRANCE, true)
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_PATH_TO_GOMESS] = RandoRegion{ .name = "Path to Gomess", .sceneId = SCENE_INISIE_R,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PATH_TO_GOMESS_SMALL_CRATE_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PATH_TO_GOMESS_SMALL_CRATE_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PATH_TO_GOMESS_SMALL_CRATE_03, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PATH_TO_GOMESS_SMALL_CRATE_04, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PATH_TO_GOMESS_SMALL_CRATE_05, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PATH_TO_GOMESS_SMALL_CRATE_06, true),
            CHECK(RC_ENEMY_DROP_BLUE_BUBBLE, CanKillEnemy(ACTOR_EN_BB)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_UNDER_BRIDGE, true),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_GOMESS_ROOM, true)
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_GOMESS_ROOM] = RandoRegion{ .name = "Gomess Room", .sceneId = SCENE_INISIE_R,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_BOSS_KEY, CAN_USE_SWORD || HAS_ITEM(ITEM_BOW) || HAS_ITEM(ITEM_HOOKSHOT) || CAN_BE_ZORA || CAN_BE_GORON || (CAN_BE_DEKU && HAS_MAGIC)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_GOMESS_POT_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_GOMESS_POT_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_GOMESS_POT_03, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_GOMESS_POT_04, true),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_PATH_TO_GOMESS, true),
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_SIDE_OF_ENTRANCE] = RandoRegion{ .name = "Side of Entrance", .sceneId = SCENE_INISIE_R,
        .checks = {
            CHECK(RC_ENEMY_DROP_DEATH_ARMOS, CanKillEnemy(ACTOR_EN_FAMOS)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_UNDER_BRIDGE, true),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE, true),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE_TOP, HAS_ITEM(ITEM_HOOKSHOT) && RANDO_EVENTS[RE_STONE_TOWER_TEMPLE_ENTRANCE_SWITCH_CHEST])
        },
        .events = {
            EVENT(RE_STONE_TOWER_TEMPLE_ENTRANCE_SWITCH_CHEST, true),
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE_TOP] = RandoRegion{ .name = "Top of Entrance", .sceneId = SCENE_INISIE_R,
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE, true),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_SIDE_OF_ENTRANCE, true),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_BRIDGE, KEY_COUNT(STONE_TOWER_TEMPLE) >= 4),
        },
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_BRIDGE] = RandoRegion{ .name = "Bridge", .sceneId = SCENE_INISIE_R,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_GIANT_MASK, CanKillEnemy(ACTOR_EN_EGOL)),
            CHECK(RC_ENEMY_DROP_EYEGORE, CanKillEnemy(ACTOR_EN_EGOL)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_ENTRANCE_TOP, KEY_COUNT(STONE_TOWER_TEMPLE) >= 4),
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_UPPER, CanKillEnemy(ACTOR_EN_EGOL))
        },
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_UPPER] = RandoRegion{ .name = "Spiked Bar Room Upper", .sceneId = SCENE_INISIE_R,
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_LOWER, HAS_ITEM(ITEM_HOOKSHOT))
        },
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_LOWER] = RandoRegion{ .name = "Spiked Bar Room Lower", .sceneId = SCENE_INISIE_R,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_03, true),
            // #578 part 3 — MMRT_ISTT_RUPEES_GORON ("Collect the Floating Rupees in ISTT as Goron — In
            // the room before Twinmold, Goron can collect the rupees by rolling on the platform over
            // them."), DEFAULT OFF, on every Zora-gated rupee in this room (01-03 are already free).
            // The trick names the Goron mask, so CAN_BE_GORON is the disjunct's own conjunct.
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_04, CAN_BE_ZORA || (MM_TRICK(MMRT_ISTT_RUPEES_GORON) && CAN_BE_GORON)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_05, CAN_BE_ZORA || (MM_TRICK(MMRT_ISTT_RUPEES_GORON) && CAN_BE_GORON)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_06, CAN_BE_ZORA || (MM_TRICK(MMRT_ISTT_RUPEES_GORON) && CAN_BE_GORON)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_07, CAN_BE_ZORA || (MM_TRICK(MMRT_ISTT_RUPEES_GORON) && CAN_BE_GORON)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_08, CAN_BE_ZORA || (MM_TRICK(MMRT_ISTT_RUPEES_GORON) && CAN_BE_GORON)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_09, CAN_BE_ZORA || (MM_TRICK(MMRT_ISTT_RUPEES_GORON) && CAN_BE_GORON)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_10, CAN_BE_ZORA || (MM_TRICK(MMRT_ISTT_RUPEES_GORON) && CAN_BE_GORON)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_11, CAN_BE_ZORA || (MM_TRICK(MMRT_ISTT_RUPEES_GORON) && CAN_BE_GORON)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_POT_01, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_POT_02, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_POT_03, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_POT_04, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_POT_05, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_POT_06, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_POT_07, true),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_POT_08, true),
        },
        .exits = { //     TO                                         FROM
            EXIT(ENTRANCE(STONE_TOWER_TEMPLE_INVERTED, 1),           ONE_WAY_EXIT, CHECK_DUNGEON_ITEM(DUNGEON_BOSS_KEY, DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE)),
        },
        .connections = {
            CONNECTION(RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_UPPER, HAS_ITEM(ITEM_HOOKSHOT)),
        },
    };
    Regions[RR_STONE_TOWER_TEMPLE_INVERTED_TWINMOLD_BOSS_ENTRANCE] = RandoRegion{ .name = "Twinmold Boss Entrance", .sceneId = SCENE_INISIE_R,
        .exits = { //     TO                                         FROM
            EXIT(ENTRANCE(TWINMOLDS_LAIR, 0),                        ONE_WAY_EXIT, true),
        },
        .oneWayEntrances = {
            ENTRANCE(STONE_TOWER_TEMPLE_INVERTED, 1), // Hole to before boss arena
        }
    };
    Regions[RR_STONE_TOWER_TEMPLE_BOSS_ROOM] = RandoRegion{ .name = "Twinmold Boss Lair", .sceneId = SCENE_INISIE_BS,
        .checks = {
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_BOSS_HEART_CONTAINER, CanKillEnemy(ACTOR_BOSS_02)),
            CHECK(RC_STONE_TOWER_TEMPLE_INVERTED_BOSS_WARP, CanKillEnemy(ACTOR_BOSS_02)),
            CHECK(RC_GIANTS_CHAMBER_OATH_TO_ORDER, CanKillEnemy(ACTOR_BOSS_02)),
        },
        .exits = { //     TO                                         FROM
            EXIT(ENTRANCE(IKANA_CANYON, 15),                        ONE_WAY_EXIT, true),
        },
        .oneWayEntrances = {
            ENTRANCE(TWINMOLDS_LAIR, 0), // Blue warp exit
        }
    };
}, {});
// clang-format on
