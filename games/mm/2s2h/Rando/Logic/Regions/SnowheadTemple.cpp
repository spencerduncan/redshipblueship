#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include "2s2h/Rando/Logic/Logic.h"

using namespace Rando::Logic;

// clang-format off
static RegisterShipInitFunc initFunc([]() {
    Regions[RR_SNOWHEAD_TEMPLE_BLOCK_ROOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_BLOCK_ROOM_HIDDEN_CHEST, true),
            CHECK(RC_SNOWHEAD_TEMPLE_BLOCK_ROOM_POT_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_BLOCK_ROOM_POT_02, true),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR,  true),
            // #697 — the TODO's question is answered NO, by the reference project: OoTMM's
            // snowhead_temple.yml climbs from its "Block Room" to "Block Room Upper" with
            // `can_hookshot_short || (event(SNOWHEAD_PUSH_BLOCK) && is_tall)`, and `is_tall` is the
            // Zora Mask — plain logic, no trick key. So no key is appended and nothing is tightened; the
            // Zora term stays in the shipped rung. (OoTMM also requires the block pushed first; this graph
            // does not model the block, which is a logic-parity difference, not a trick.)
            CONNECTION(RR_SNOWHEAD_TEMPLE_BLOCK_ROOM_UPPER, HAS_ITEM(ITEM_HOOKSHOT) || CAN_BE_ZORA), // TODO : Should using Zora for this be considered a trick?
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_BLOCK_ROOM_UPPER] = RandoRegion { .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_BLOCK_ROOM_LEDGE_CHEST, true),
            CHECK(RC_ENEMY_DROP_FLYING_POT, CanKillEnemy(ACTOR_EN_TUBO_TRAP)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_BLOCK_ROOM, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_COMPASS_ROOM, true),
        }
    };
    Regions[RR_SNOWHEAD_TEMPLE_BOSS_ROOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN_BS,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_HEART_CONTAINER,  CanKillEnemy(ACTOR_BOSS_HAKUGIN)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_WARP,             CanKillEnemy(ACTOR_BOSS_HAKUGIN)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_POT_01,           CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_POT_10,           CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_POT_02,           CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_POT_03,           CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_POT_04,           CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_POT_05,           CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_POT_06,           CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_POT_07,           CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_POT_08,           CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_POT_09,           CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_EARLY_POT_01,     true),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_EARLY_POT_02,     true),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_EARLY_POT_03,     true),
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_EARLY_POT_04,     true),
            CHECK(RC_GIANTS_CHAMBER_OATH_TO_ORDER,          CanKillEnemy(ACTOR_BOSS_HAKUGIN)),
        },
        .exits = { //     TO                                         FROM
            EXIT(ENTRANCE(MOUNTAIN_VILLAGE_SPRING, 7),               ONE_WAY_EXIT, true),
        },
        .events = {
            EVENT(RE_CLEARED_SNOWHEAD_TEMPLE, CanKillEnemy(ACTOR_BOSS_HAKUGIN)),
        },
        .oneWayEntrances = {
            ENTRANCE(GOHTS_LAIR, 0), // Snowhead Temple Boss Room
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_BRIDGE_ROOM_AFTER] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_BRIDGE_ROOM_CHEST, (CAN_BE_ZORA || CAN_USE_MAGIC_ARROW(FIRE) || HAS_ITEM(ITEM_HOOKSHOT))),
            CHECK(RC_SNOWHEAD_TEMPLE_BRIDGE_ROOM_LARGE_CRATE, true),
            CHECK(RC_SNOWHEAD_TEMPLE_BRIDGE_ROOM_AFTER_POT_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_BRIDGE_ROOM_AFTER_POT_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_SF_BRIDGE_PILLAR, CAN_USE_PROJECTILE && HAS_ITEM(ITEM_MASK_GREAT_FAIRY)), // Accessible from both sides
            CHECK(RC_SNOWHEAD_TEMPLE_SF_BRIDGE_UNDER_PLATFORM, CAN_USE_PROJECTILE && HAS_ITEM(ITEM_MASK_GREAT_FAIRY)), // Accessible from both sides
            CHECK(RC_ENEMY_DROP_FREEZARD, CanKillEnemy(ACTOR_EN_FZ)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_BRIDGE_ROOM_BEFORE, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_MAP_ROOM_LOWER, true),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_BRIDGE_ROOM_BEFORE] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_BRIDGE_ROOM_POT_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_BRIDGE_ROOM_POT_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_BRIDGE_ROOM_POT_03, true),
            CHECK(RC_SNOWHEAD_TEMPLE_BRIDGE_ROOM_POT_04, true),
            CHECK(RC_SNOWHEAD_TEMPLE_BRIDGE_ROOM_POT_05, true),
            CHECK(RC_SNOWHEAD_TEMPLE_SF_BRIDGE_PILLAR, CAN_USE_PROJECTILE && HAS_ITEM(ITEM_MASK_GREAT_FAIRY)), // Accessible from both sides
            CHECK(RC_SNOWHEAD_TEMPLE_SF_BRIDGE_UNDER_PLATFORM, CAN_USE_PROJECTILE && HAS_ITEM(ITEM_MASK_GREAT_FAIRY)), // Accessible from both sides
            CHECK(RC_SNOWHEAD_TEMPLE_BRIDGE_ROOM_SMALL_SNOWBALL_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_BRIDGE_ROOM_SMALL_SNOWBALL_02, true),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_ENTRANCE_AFTER_BLOCK, true),
            // #697 — NOT BOUND, and the TODO stays. OoTMM's own graph has no bomb jump on this bridge
            // (its "Bridge Front" -> "Bridge Back" is `goron_fast_roll || can_hookshot || ...`), so it
            // would need a key of our own; #697 appends a key only where OoTMM has the trick, and it does
            // not have this one. (OoTMM DOES use MMRT_GORON_BOMB_JUMP twice in this temple — the compass
            // room's climb to the block room's upper floor and the compass-room crate — and both are
            // bound below.)
            CONNECTION(RR_SNOWHEAD_TEMPLE_BRIDGE_ROOM_AFTER, (CAN_BE_GORON && HAS_MAGIC) || (HAS_ITEM(ITEM_HOOKSHOT) && CAN_BE_ZORA)) // TODO : Add bomb jump trick here.
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BEFORE_UPPER_WIZZROBE_ROOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_NEAR_BOSS_KEY_POT_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_NEAR_BOSS_KEY_POT_02, true),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_DINOLFOS_ROOM, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_UPPER_WIZZROBE_ROOM, true),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_CHEST, CAN_BE_GORON),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_POT_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM_POT_02, true),
            CHECK(RC_ENEMY_DROP_RED_BUBBLE, CanKillEnemy(ACTOR_EN_BBFALL))
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER,       true),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        // TODO : Think of the best way to handle these central rooms in logic
        // Flags_GetSceneSwitch(SCENE_HAKUGIN, 0x35) will block access to some of these rooms.
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_BLOCK_ROOM,           true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_BRIDGE_ROOM_AFTER,    true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM,  true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_ENTRANCE_AFTER_BLOCK, HAS_ITEM(ITEM_BOW)),
            // #697 — MMRT_SHT_HOT_WATER, DEFAULT OFF: OoTMM's "Snowhead Temple Center Level 1" ->
            // "Pillars Room Upper" is `can_use_fire_short_range || trick_sht_hot_water`.
            CONNECTION(RR_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER,         CAN_USE_MAGIC_ARROW(FIRE) || CAN_CARRY_HOT_WATER_TO_SNOWHEAD),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_SCARECROW_FLOOR, CAN_HOOK_SCARECROW)
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR_SWITCH_ROOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER, CAN_BE_DEKU && CAN_USE_MAGIC_ARROW(FIRE)),
        }
    };
    Regions[RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_SCARECROW_FLOOR] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_SCARECROW_POT_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_SCARECROW_POT_02, true),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_SECOND_FLOOR, HAS_ITEM(ITEM_HOOKSHOT)),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM, true)
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_SECOND_FLOOR] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        // TODO : Thinking of the best way to handle this room is a total headache, gonna leave it as if for now and get back to it later.
        // Flags_GetSceneSwitch(SCENE_HAKUGIN, 0x35) will block access to some of these rooms.
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_2_POT_01, CAN_BE_GORON || HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_2_POT_02, CAN_BE_GORON || HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_2_SMALL_SNOWBALL_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_2_SMALL_SNOWBALL_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_2_SMALL_SNOWBALL_03, true),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_2_SMALL_SNOWBALL_04, CAN_BE_GORON && HAS_MAGIC),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_2_SMALL_SNOWBALL_05, CAN_BE_GORON && HAS_MAGIC),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_2_SMALL_SNOWBALL_06, CAN_BE_GORON && HAS_MAGIC),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_DUAL_SWITCHES_ROOM, (CAN_BE_GORON || CAN_USE_MAGIC_ARROW(FIRE))),
            CONNECTION(RR_SNOWHEAD_TEMPLE_LOWER_WIZZROBE_ROOM, CAN_BE_GORON && HAS_MAGIC),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_THIRD_FLOOR, CAN_USE_MAGIC_ARROW(FIRE)),
            CONNECTION(RR_SNOWHEAD_TEMPLE_MAP_ROOM_UPPER, CAN_BE_GORON || HAS_ITEM(ITEM_HOOKSHOT)),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_SCARECROW_FLOOR, true)
        }
    };
    Regions[RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_THIRD_FLOOR] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        // This region is being treated the same as the upper part that you can access using the completed pillar puzzle...its probably fine like this.
        .checks = {
            // #578 part 2 — MMRT_LENS ("Fewer Lens Requirements (MM)"), default off. North.cpp's
            // header note carries the rationale and names the two sites the trick excludes.
            // The Lens term here has no HAS_MAGIC conjunct, unlike every other site in this file;
            // that asymmetry is left exactly as it was, because changing it would change tricks-off
            // logic. The disjunct is parenthesized around the Lens term ONLY, so the Hookshot stays
            // required: `(DEKU && GORON) || ((trick || LENS) && HOOKSHOT)`.
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_ALCOVE_CHEST, ((CAN_BE_DEKU && CAN_BE_GORON) || (MM_TRICK(MMRT_LENS) || HAS_ITEM(ITEM_LENS_OF_TRUTH)) && HAS_ITEM(ITEM_HOOKSHOT))),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_3_LARGE_SNOWBALL_01, CanKillEnemy(ACTOR_OBJ_SNOWBALL)),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_3_LARGE_SNOWBALL_02, CanKillEnemy(ACTOR_OBJ_SNOWBALL)),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_3_LARGE_SNOWBALL_03, CanKillEnemy(ACTOR_OBJ_SNOWBALL)),
            CHECK(RC_SNOWHEAD_TEMPLE_CENTRAL_ROOM_LEVEL_3_LARGE_SNOWBALL_04, CanKillEnemy(ACTOR_OBJ_SNOWBALL)),
        },
        .exits = {
            EXIT(ENTRANCE(GOHTS_LAIR, 0),           ONE_WAY_EXIT, CHECK_DUNGEON_ITEM(DUNGEON_BOSS_KEY, DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_SECOND_FLOOR, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_SNOW_ROOM, (CAN_BE_GORON || HAS_ITEM(ITEM_HOOKSHOT)) && KEY_COUNT(SNOWHEAD_TEMPLE) >= 3),
        }
    };
    Regions[RR_SNOWHEAD_TEMPLE_COMPASS_ROOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_CHEST, true),
            // #697 — MMRT_SHT_HOT_WATER, DEFAULT OFF: OoTMM's "Snowhead Temple Compass Room Ledge" is
            // `can_use_fire_short_range || trick_sht_hot_water`. CAN_CARRY_HOT_WATER_TO_SNOWHEAD
            // (Logic/Logic.h) is that trick's MM-only leg.
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_LEDGE_CHEST, CAN_USE_MAGIC_ARROW(FIRE) || CAN_CARRY_HOT_WATER_TO_SNOWHEAD),
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_POT_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_POT_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_POT_03, true),
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_POT_04, true),
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_POT_05, true),
            // #697 — MMRT_GORON_BOMB_JUMP, DEFAULT OFF: OoTMM's "Snowhead Temple SF Compass Room Crate"
            // ends `... || can_goron_bomb_jump`. The TODO's Zora question is answered NO by the same
            // line: OoTMM reaches the crate from the ledge with `is_tall` (Zora) and an explosive or
            // Goron as PLAIN logic, not behind any trick — so there is no key to bind it to, and
            // admitting it here would be a tricks-off widening, which is not a trick binding's job.
            CHECK(RC_SNOWHEAD_TEMPLE_SF_COMPASS_ROOM_CRATE, (CAN_USE_EXPLOSIVE && HAS_ITEM(ITEM_MASK_GREAT_FAIRY)) || CAN_GORON_BOMB_JUMP), // TODO : Zora Mask can be used from the upper ledge to reach this after breaking the crate. Implement as a trick?
            CHECK(RC_ENEMY_DROP_WOLFOS, CanKillEnemy(ACTOR_EN_WF)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_ENTRANCE_AFTER_BLOCK,     KEY_COUNT(SNOWHEAD_TEMPLE) >= 1),
            // #697 — MMRT_SHT_HOT_WATER and MMRT_GORON_BOMB_JUMP, both DEFAULT OFF: OoTMM's "Snowhead
            // Temple Compass Room" -> "Block Room Upper" is `can_use_fire_short_range || trick_sht_hot_water
            // || can_hookshot_short || can_goron_bomb_jump`. The TODO's Zora question: OoTMM does not admit
            // Zora from THIS side at all and admits it from the block-room side as plain logic (see the top
            // of this file), so it is not a trick either way; the Zora term is left exactly as it was.
            CONNECTION(RR_SNOWHEAD_TEMPLE_BLOCK_ROOM_UPPER,   CAN_BE_ZORA || HAS_ITEM(ITEM_HOOKSHOT) || CAN_USE_MAGIC_ARROW(FIRE) || CAN_CARRY_HOT_WATER_TO_SNOWHEAD || CAN_GORON_BOMB_JUMP), // TODO : Should using Zora for this be considered a trick?
            CONNECTION(RR_SNOWHEAD_TEMPLE_ICICLE_ROOM,  CAN_USE_EXPLOSIVE),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_DINOLFOS_ROOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_SF_DINOLFOS_01, CanKillEnemy(ACTOR_EN_DINOFOS)),
            CHECK(RC_SNOWHEAD_TEMPLE_SF_DINOLFOS_02, CanKillEnemy(ACTOR_EN_DINOFOS)),
            CHECK(RC_ENEMY_DROP_DINOLFOS, CanKillEnemy(ACTOR_EN_DINOFOS)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_SNOW_ROOM, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BEFORE_UPPER_WIZZROBE_ROOM, true),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_DUAL_SWITCHES_ROOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_DUAL_SWITCHES_ROOM_LARGE_CRATE_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_DUAL_SWITCHES_ROOM_LARGE_CRATE_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_DUAL_SWITCHES_POT_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_DUAL_SWITCHES_POT_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_SF_DUAL_SWITCHES, (((MM_TRICK(MMRT_LENS) || (HAS_ITEM(ITEM_LENS_OF_TRUTH) && HAS_MAGIC)) && HAS_ITEM(ITEM_MASK_GREAT_FAIRY)) && ((HAS_ITEM(ITEM_BOW) || HAS_ITEM(ITEM_HOOKSHOT)) || CAN_BE_DEKU))), // #578 part 2 — MMRT_LENS
            CHECK(RC_ENEMY_DROP_BOE, CanKillEnemy(ACTOR_EN_MKK)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_SECOND_FLOOR, CAN_BE_GORON || CAN_USE_MAGIC_ARROW(FIRE)),
            CONNECTION(RR_SNOWHEAD_TEMPLE_ICICLE_ROOM, KEY_COUNT(SNOWHEAD_TEMPLE) >= 2),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_ENTRANCE_AFTER_BLOCK] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
      .checks = {
          CHECK(RC_SNOWHEAD_TEMPLE_ENTRANCE_POT_01, CAN_BE_GORON),
          CHECK(RC_SNOWHEAD_TEMPLE_ENTRANCE_POT_02, CAN_BE_GORON),
          CHECK(RC_ENEMY_DROP_WOLFOS, CanKillEnemy(ACTOR_EN_WF)),
      },
      .connections = {
          // #697 — MMRT_SHT_HOT_WATER, DEFAULT OFF: OoTMM's "Snowhead Temple Main" -> "Center Level 1"
          // is `can_use_fire_short_range || trick_sht_hot_water`.
          CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR,   CAN_USE_MAGIC_ARROW(FIRE) || CAN_CARRY_HOT_WATER_TO_SNOWHEAD),
          CONNECTION(RR_SNOWHEAD_TEMPLE_BRIDGE_ROOM_BEFORE,  true),
          CONNECTION(RR_SNOWHEAD_TEMPLE_ENTRANCE_BEFORE_BLOCK, true),
          CONNECTION(RR_SNOWHEAD_TEMPLE_COMPASS_ROOM, KEY_COUNT(SNOWHEAD_TEMPLE) >= 1),
      },
    };
    Regions[RR_SNOWHEAD_TEMPLE_ENTRANCE_BEFORE_BLOCK] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
      .checks = {
            CHECK(RC_ENEMY_DROP_BOE, CanKillEnemy(ACTOR_EN_MKK)),
        },
      .exits = { //     TO                                         FROM
          EXIT(ENTRANCE(SNOWHEAD, 1),                     ENTRANCE(SNOWHEAD_TEMPLE, 0), true),
      },
      .connections = {
          CONNECTION(RR_SNOWHEAD_TEMPLE_ENTRANCE_AFTER_BLOCK,   CAN_BE_GORON),
      },
    };
    Regions[RR_SNOWHEAD_TEMPLE_ICICLE_ROOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_ICICLE_ROOM_ALCOVE_CHEST, (MM_TRICK(MMRT_LENS) || (HAS_ITEM(ITEM_LENS_OF_TRUTH) && HAS_MAGIC))), // #578 part 2 — MMRT_LENS
            CHECK(RC_SNOWHEAD_TEMPLE_ICICLE_ROOM_CHEST, ((CAN_USE_EXPLOSIVE && HAS_ITEM(ITEM_HOOKSHOT)) || CAN_BE_GORON)),
            CHECK(RC_SNOWHEAD_TEMPLE_ICICLE_ROOM_FREESTANDING_RUPEE_01, CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_ICICLE_ROOM_FREESTANDING_RUPEE_02, CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_ICICLE_ROOM_FREESTANDING_RUPEE_03, CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_ICICLE_ROOM_LARGE_SNOWBALL_01, CanKillEnemy(ACTOR_OBJ_SNOWBALL)),
            CHECK(RC_SNOWHEAD_TEMPLE_ICICLE_ROOM_SMALL_SNOWBALL_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_ICICLE_ROOM_SMALL_SNOWBALL_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_ICICLE_ROOM_SMALL_SNOWBALL_03, true),
            CHECK(RC_SNOWHEAD_TEMPLE_ICICLE_ROOM_SMALL_SNOWBALL_04, true),
            CHECK(RC_SNOWHEAD_TEMPLE_ICICLE_ROOM_SMALL_SNOWBALL_05, true),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_COMPASS_ROOM,             CAN_USE_EXPLOSIVE),
            CONNECTION(RR_SNOWHEAD_TEMPLE_DUAL_SWITCHES_ROOM,       KEY_COUNT(SNOWHEAD_TEMPLE) >= 2),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_LOWER_WIZZROBE_ROOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_FIRE_ARROW_CHEST, CanKillEnemy(ACTOR_EN_WIZ)),
            CHECK(RC_ENEMY_DROP_WIZROBE, CanKillEnemy(ACTOR_EN_WIZ)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_SECOND_FLOOR, CanKillEnemy(ACTOR_EN_WIZ)),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_MAP_ROOM_LOWER] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_MAP_CHEST, true),
            CHECK(RC_SNOWHEAD_TEMPLE_SF_MAP_ROOM, true),
            CHECK(RC_SNOWHEAD_TEMPLE_MAP_ROOM_LARGE_CRATE_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_MAP_ROOM_LARGE_CRATE_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_MAP_ROOM_LARGE_CRATE_03, true),
            CHECK(RC_SNOWHEAD_TEMPLE_MAP_ROOM_LARGE_CRATE_04, true),
            CHECK(RC_SNOWHEAD_TEMPLE_MAP_ROOM_LARGE_CRATE_05, true),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_BRIDGE_ROOM_AFTER, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_MAP_ROOM_UPPER, CAN_USE_MAGIC_ARROW(FIRE)),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_MAP_ROOM_UPPER] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_MAP_ALCOVE_CHEST, ((MM_TRICK(MMRT_LENS) || (HAS_ITEM(ITEM_LENS_OF_TRUTH) && HAS_MAGIC)) && HAS_ITEM(ITEM_BOW) && HAS_ITEM(ITEM_ARROW_FIRE))), // #578 part 2 — MMRT_LENS
            CHECK(RC_ENEMY_DROP_FREEZARD, CanKillEnemy(ACTOR_EN_FZ)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_MAP_ROOM_LOWER, true),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_SECOND_FLOOR, true),
        },
    };
    // #578 part 3 — MMRT_SHT_PILLAR_ROOM_HOOKSHOT is bound on this region's one connection up; see the
    // note there.
    Regions[RR_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER_POT_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER_POT_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER_POT_03, true),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER_POT_04, true),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER_POT_05, true),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER_POT_06, true),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER_POT_07, true),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_BOTTOM, true),
            // #578 part 3 — MMRT_SHT_PILLAR_ROOM_HOOKSHOT ("From the ground floor of pillar room, use a
            // Hookshot to kill the freezards and then climb using the chest that spawns."), DEFAULT OFF.
            // "From the ground floor" is why only THIS region's connection up is gated: the two
            // CAN_USE_MAGIC_ARROW(FIRE) connections into the upper pillar room from the central rooms
            // start somewhere else and the trick says nothing about them. Item term: the Hookshot.
            CONNECTION(RR_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER, (CAN_BE_DEKU && CAN_USE_MAGIC_ARROW(FIRE)) || (MM_TRICK(MMRT_SHT_PILLAR_ROOM_HOOKSHOT) && HAS_ITEM(ITEM_HOOKSHOT))),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_CHEST,       (CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(FIRE))),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER_POT_01, (CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(FIRE))),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER_POT_02, (CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(FIRE))),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER_POT_03, (CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(FIRE))),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER_POT_04, (CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(FIRE))),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER_POT_05, (CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(FIRE))),
            CHECK(RC_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER_POT_06, (CAN_BE_DEKU || CAN_USE_MAGIC_ARROW(FIRE))),
            CHECK(RC_ENEMY_DROP_FREEZARD, CanKillEnemy(ACTOR_EN_FZ)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR_SWITCH_ROOM, CAN_USE_MAGIC_ARROW(FIRE)),
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR, CAN_USE_MAGIC_ARROW(FIRE)),
            CONNECTION(RR_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER, true),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_SNOW_ROOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_SF_SNOW_ROOM, (MM_TRICK(MMRT_LENS) || (HAS_ITEM(ITEM_LENS_OF_TRUTH) && HAS_MAGIC)) && HAS_ITEM(ITEM_MASK_GREAT_FAIRY) && CAN_USE_PROJECTILE), // #578 part 2 — MMRT_LENS
            CHECK(RC_SNOWHEAD_TEMPLE_SNOW_ROOM_SMALL_SNOWBALL_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_SNOW_ROOM_SMALL_SNOWBALL_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_SNOW_ROOM_SMALL_SNOWBALL_03, true),
            CHECK(RC_SNOWHEAD_TEMPLE_SNOW_ROOM_SMALL_SNOWBALL_04, true),
            CHECK(RC_SNOWHEAD_TEMPLE_SNOW_ROOM_SMALL_SNOWBALL_05, true),
            CHECK(RC_SNOWHEAD_TEMPLE_SNOW_ROOM_SMALL_SNOWBALL_06, true),
            CHECK(RC_SNOWHEAD_TEMPLE_SNOW_ROOM_SMALL_SNOWBALL_07, true),
            CHECK(RC_SNOWHEAD_TEMPLE_SNOW_ROOM_SMALL_SNOWBALL_08, true),
            CHECK(RC_ENEMY_DROP_EENO, CanKillEnemy(ACTOR_EN_SNOWMAN)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_THIRD_FLOOR, KEY_COUNT(SNOWHEAD_TEMPLE) >= 3),
            CONNECTION(RR_SNOWHEAD_TEMPLE_DINOLFOS_ROOM, CAN_USE_MAGIC_ARROW(FIRE)),
        },
    };
    Regions[RR_SNOWHEAD_TEMPLE_UPPER_WIZZROBE_ROOM] = RandoRegion{ .sceneId = SCENE_HAKUGIN,
        .checks = {
            CHECK(RC_SNOWHEAD_TEMPLE_BOSS_KEY, CanKillEnemy(ACTOR_EN_WIZ)),
            CHECK(RC_SNOWHEAD_TEMPLE_WIZZROBE_POT_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_WIZZROBE_POT_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_WIZZROBE_POT_03, true),
            CHECK(RC_SNOWHEAD_TEMPLE_WIZZROBE_POT_04, true),
            CHECK(RC_SNOWHEAD_TEMPLE_WIZZROBE_POT_05, true),
            CHECK(RC_ENEMY_DROP_WIZROBE, CanKillEnemy(ACTOR_EN_WIZ)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_THIRD_FLOOR, CanKillEnemy(ACTOR_EN_WIZ)),
        },
    };
}, {});
// clang-format on
