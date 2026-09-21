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
            // #578 part 2 — NOT BOUND, and the TODO stays. Answering it "yes" would move CAN_BE_ZORA
            // out of the shipped Glitchless rung, i.e. TIGHTEN tricks-off logic. Part 2's contract is
            // widenings only (every edge it touches evaluates identically with tricks off), and the
            // one tightening in this epic so far — the Great Bay Temple boss-key edge — took an
            // explicit operator ruling because it changes what worlds the fill can produce. There is
            // also no declared key for it: OoTMM has no Snowhead block-room entry, so this needs a
            // key of ours. Both halves are part 3's, along with the mirror at
            // RR_SNOWHEAD_TEMPLE_COMPASS_ROOM below.
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
            // #578 part 2 — NOT BOUND, and the TODO stays. This one IS a widening, so the contract is
            // not the blocker: the missing piece is a KEY. OoTMM has no Snowhead bomb-jump entry (its
            // MMRT_GORON_BOMB_JUMP is specifically "Bomb Jump Fences as Goron", and this is a gap, not
            // a fence), so the binding needs a key of ours, which part 1's table does not declare and
            // which appending here would grow MMRT_MAX and therefore the frozen save array and the
            // profile identity string. Part 3 adds the key and the disjunct together.
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
            CONNECTION(RR_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER,         CAN_USE_MAGIC_ARROW(FIRE)),
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
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_LEDGE_CHEST, CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_POT_01, true),
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_POT_02, true),
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_POT_03, true),
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_POT_04, true),
            CHECK(RC_SNOWHEAD_TEMPLE_COMPASS_ROOM_POT_05, true),
            // #578 part 2 — NOT BOUND, and the TODO stays, for the same reason as the bridge-room
            // bomb jump above: a widening with no declared key to hang it on (OoTMM has no Snowhead
            // compass-room entry). Part 3 adds the key and the disjunct together.
            CHECK(RC_SNOWHEAD_TEMPLE_SF_COMPASS_ROOM_CRATE, (CAN_USE_EXPLOSIVE && HAS_ITEM(ITEM_MASK_GREAT_FAIRY))), // TODO : Zora Mask can be used from the upper ledge to reach this after breaking the crate. Implement as a trick?
            CHECK(RC_ENEMY_DROP_WOLFOS, CanKillEnemy(ACTOR_EN_WF)),
        },
        .connections = {
            CONNECTION(RR_SNOWHEAD_TEMPLE_ENTRANCE_AFTER_BLOCK,     KEY_COUNT(SNOWHEAD_TEMPLE) >= 1),
            // #578 part 2 — NOT BOUND; the mirror of the block-room seam at the top of this file,
            // which carries the reasoning (gating it would tighten tricks-off logic, and there is no
            // declared key). Part 3 owns both, together, because gating one and not the other would
            // make the pair inconsistent.
            CONNECTION(RR_SNOWHEAD_TEMPLE_BLOCK_ROOM_UPPER,   CAN_BE_ZORA || HAS_ITEM(ITEM_HOOKSHOT) || CAN_USE_MAGIC_ARROW(FIRE)), // TODO : Should using Zora for this be considered a trick?
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
          CONNECTION(RR_SNOWHEAD_TEMPLE_CENTRAL_ROOM_FIRST_FLOOR,   CAN_USE_MAGIC_ARROW(FIRE)),
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
