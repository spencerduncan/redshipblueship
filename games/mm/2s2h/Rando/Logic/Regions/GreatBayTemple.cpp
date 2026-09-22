#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include "2s2h/Rando/Logic/Logic.h"

using namespace Rando::Logic;

// clang-format off
static RegisterShipInitFunc initFunc([]() {
    Regions[RR_GREAT_BAY_TEMPLE_BABA_CHEST_ROOM] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_BABA_CHEST, CAN_BE_ZORA || CAN_USE_PROJECTILE || HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_ENEMY_DROP_BIO_DEKU_BABA, CanKillEnemy(ACTOR_BOSS_05)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_COMPASS_ROOM,    true),
            CONNECTION(RR_GREAT_BAY_TEMPLE_MAP_ROOM,        true),
        },
    };
    Regions[RR_GREAT_BAY_TEMPLE_BEFORE_WART] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_01, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_02, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_03, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_04, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_05, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_06, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_07, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_08, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_09, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_10, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_11, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BEFORE_WART_POT_12, true),
            CHECK(RC_ENEMY_DROP_CHUCHU,                   CanKillEnemy(ACTOR_EN_SLIME)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_RED_PIPE_BEFORE_WART,  KEY_COUNT(GREAT_BAY_TEMPLE) >= 1),
            CONNECTION(RR_GREAT_BAY_TEMPLE_WART,                  true),
        }
    };
    Regions[RR_GREAT_BAY_TEMPLE_BOSS_ROOM] = RandoRegion{ .sceneId = SCENE_SEA_BS,
        .checks = {
            // TODO: CAN_KILL_BOSS(Gyorg)?
            CHECK(RC_GREAT_BAY_TEMPLE_BOSS_HEART_CONTAINER, CanKillEnemy(ACTOR_BOSS_03)),
            CHECK(RC_GREAT_BAY_TEMPLE_BOSS_WARP, CanKillEnemy(ACTOR_BOSS_03)),
            CHECK(RC_GREAT_BAY_TEMPLE_BOSS_POT_01, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BOSS_POT_02, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BOSS_POT_03, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BOSS_POT_04, true),
            CHECK(RC_GREAT_BAY_TEMPLE_BOSS_UNDERWATER_POT_01, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_BOSS_UNDERWATER_POT_02, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_BOSS_UNDERWATER_POT_03, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_BOSS_UNDERWATER_POT_04, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GIANTS_CHAMBER_OATH_TO_ORDER, CanKillEnemy(ACTOR_BOSS_03)),
        },
        .exits = { //     TO                                         FROM
            EXIT(ENTRANCE(ZORA_CAPE, 9),                             ONE_WAY_EXIT, true),
        },
        .events = {
            EVENT(RE_CLEARED_GREAT_BAY_TEMPLE, CanKillEnemy(ACTOR_BOSS_03)),
        },
        .oneWayEntrances = {
            ENTRANCE(GYORGS_LAIR, 0), // Great Bay Temple Pre Boss Room
        },
    };
    Regions[RR_GREAT_BAY_TEMPLE_CENTRAL_ROOM] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_SF_CENTRAL_ROOM_BARREL,           true),
            CHECK(RC_GREAT_BAY_TEMPLE_CENTRAL_ROOM_POT_01,              true),
            CHECK(RC_GREAT_BAY_TEMPLE_CENTRAL_ROOM_POT_02,              true),
            CHECK(RC_GREAT_BAY_TEMPLE_SF_CENTRAL_ROOM_UNDERWATER_POT,   CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_COMPASS_ROOM_TUNNEL,     CAN_BE_ZORA && GBT_CAN_REVERSE_WATER_FLOW && CAN_USE_ABILITY(SWIM)),
            CONNECTION(RR_GREAT_BAY_TEMPLE_GREEN_PIPE_1,            CAN_USE_MAGIC_ARROW(ICE)),
            CONNECTION(RR_GREAT_BAY_TEMPLE_MAP_ROOM,                CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CONNECTION(RR_GREAT_BAY_TEMPLE_PRE_BOSS_ROOM,           CAN_BE_ZORA && GBT_CAN_REVERSE_WATER_FLOW && CAN_USE_ABILITY(SWIM)),
            CONNECTION(RR_GREAT_BAY_TEMPLE_RED_PIPE_BEFORE_WART,    CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CONNECTION(RR_GREAT_BAY_TEMPLE_WATER_WHEEL_ROOM,        true)
        },
    };
    Regions[RR_GREAT_BAY_TEMPLE_COMPASS_ROOM_WITH_BOSS_KEY_CHEST] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_BOSS_KEY, true),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_COMPASS_ROOM,    true),
            CONNECTION(RR_GREAT_BAY_TEMPLE_GEKKO,           CAN_USE_MAGIC_ARROW(ICE)),
        },
    };
    Regions[RR_GREAT_BAY_TEMPLE_COMPASS_ROOM_TUNNEL] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            // TODO: Think about the best way to handle these checks with waterflow in mind.
            CHECK(RC_GREAT_BAY_TEMPLE_SF_COMPASS_ROOM_TUNNEL_POT,                CAN_BE_ZORA || (CAN_USE_PROJECTILE && HAS_ITEM(ITEM_MASK_GREAT_FAIRY))),
            CHECK(RC_GREAT_BAY_TEMPLE_COMPASS_ROOM_TUNNEL_FREESTANDING_RUPEE_01, CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_COMPASS_ROOM_TUNNEL_FREESTANDING_RUPEE_02, CAN_BE_ZORA),
            CHECK(RC_ENEMY_DROP_DEXIHAND,                                        CanKillEnemy(ACTOR_EN_WDHAND)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_CENTRAL_ROOM,              CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CONNECTION(RR_GREAT_BAY_TEMPLE_GREEN_PIPE_2,              GBT_CAN_REVERSE_WATER_FLOW && CAN_USE_ABILITY(SWIM)),
        },
    };
    Regions[RR_GREAT_BAY_TEMPLE_COMPASS_ROOM] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_COMPASS_CHEST,               true),
            CHECK(RC_GREAT_BAY_TEMPLE_COMPASS_ROOM_UNDERWATER,     CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_COMPASS_ROOM_SURFACE_POT_01, true),
            CHECK(RC_GREAT_BAY_TEMPLE_COMPASS_ROOM_SURFACE_POT_02, true),
            CHECK(RC_GREAT_BAY_TEMPLE_COMPASS_ROOM_SURFACE_POT_03, true),
            CHECK(RC_GREAT_BAY_TEMPLE_COMPASS_ROOM_SURFACE_POT_04, true),
            CHECK(RC_GREAT_BAY_TEMPLE_COMPASS_ROOM_WATER_POT_01,   CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_COMPASS_ROOM_WATER_POT_02,   CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_COMPASS_ROOM_WATER_POT_03,   CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_ENEMY_DROP_BIO_DEKU_BABA,                     CanKillEnemy(ACTOR_BOSS_05)),
            CHECK(RC_ENEMY_DROP_DEXIHAND,                          CanKillEnemy(ACTOR_EN_WDHAND)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_BABA_CHEST_ROOM,                   true),
            // #578 finding (b) — DECIDED (operator ruling 2026-09-17: sync and gate).
            // Upstream 2Ship commented this exact connection out in its PR #1661: "Boss Key
            // connection in GBT was left over when it was intended to be done as a trick(my bad)
            // Commented it out for now until trick menu is implemented." Our games/mm snapshot
            // predates that, so redship's Glitchless assumed a trick upstream's does not — an
            // accident, not a bug. Now it is a trick-gated disjunct under MMRT_GBT_BOSS_KEY_ICE
            // (ours; no OoTMM equivalent), DEFAULT OFF: tricks-off logic matches upstream's
            // current shape, and the route is still available to anyone who asks for it instead
            // of being deleted. The TODO is resolved, so it is gone.
            CONNECTION(RR_GREAT_BAY_TEMPLE_COMPASS_ROOM_WITH_BOSS_KEY_CHEST,  MM_TRICK(MMRT_GBT_BOSS_KEY_ICE) && CAN_BE_ZORA && CAN_USE_MAGIC_ARROW(ICE)),
            CONNECTION(RR_GREAT_BAY_TEMPLE_GEKKO,                             CAN_USE_MAGIC_ARROW(ICE) && CAN_USE_MAGIC_ARROW(FIRE)),
            CONNECTION(RR_GREAT_BAY_TEMPLE_COMPASS_ROOM_TUNNEL, CAN_USE_ABILITY(SWIM))
        },
    };
    Regions[RR_GREAT_BAY_TEMPLE_ENTRANCE] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_ENTRANCE_BARREL_01, true),
            CHECK(RC_GREAT_BAY_TEMPLE_ENTRANCE_BARREL_02, true),
            CHECK(RC_GREAT_BAY_TEMPLE_ENTRANCE_BARREL_03, true),
            CHECK(RC_GREAT_BAY_TEMPLE_ENTRANCE_BARREL_04, true),
            CHECK(RC_GREAT_BAY_TEMPLE_ENTRANCE_BARREL_05, true),
            CHECK(RC_GREAT_BAY_TEMPLE_ENTRANCE_BARREL_06, true),
            CHECK(RC_GREAT_BAY_TEMPLE_ENTRANCE_BARREL_07, true),
            CHECK(RC_GREAT_BAY_TEMPLE_ENTRANCE_BARREL_08, true),
            // #578 part 3 — MMRT_GBT_ENTRANCE_BOW ("Great Bay Temple Entrance Chest using only Bow —
            // Light the four torches using somewhat precise arrow shots"), DEFAULT OFF. Vanilla wants
            // CAN_LIGHT_TORCH_NEAR_ANOTHER, i.e. a Deku Stick or Fire Arrows (Logic.h); the trick is
            // carrying the flame on a PLAIN arrow, so its own item term is the Bow alone and that term
            // is a conjunct of the disjunct rather than a replacement for it.
            CHECK(RC_GREAT_BAY_TEMPLE_ENTRANCE_CHEST,     CAN_LIGHT_TORCH_NEAR_ANOTHER || (MM_TRICK(MMRT_GBT_ENTRANCE_BOW) && HAS_ITEM(ITEM_BOW))),
        },
        .exits = { //     TO                                         FROM
            EXIT(ENTRANCE(ZORA_CAPE, 7),                    ENTRANCE(GREAT_BAY_TEMPLE, 0), HAS_ITEM(ITEM_HOOKSHOT)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_WATER_WHEEL_ROOM,  true),
        },
    };
    Regions[RR_GREAT_BAY_TEMPLE_GEKKO] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_FROG, CanKillEnemy(ACTOR_EN_BIGSLIME) && HAS_ITEM(ITEM_MASK_DON_GERO)),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_01, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_02, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_03, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_04, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_05, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_06, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_07, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_08, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_09, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_10, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_11, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_12, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_13, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_14, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_15, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GEKKO_SMALL_CRATE_16, true),
            CHECK(RC_ENEMY_DROP_GEKKO, CanKillEnemy(ACTOR_EN_BIGSLIME)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_COMPASS_ROOM_WITH_BOSS_KEY_CHEST,    CanKillEnemy(ACTOR_EN_BIGSLIME)),
        },
    };
    Regions[RR_GREAT_BAY_TEMPLE_GREEN_PIPE_1] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_1_CHEST,                 HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_1_POT_01,                CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_1_POT_02,                CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_1_POT_03,                CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_1_POT_04,                CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_1_FREESTANDING_RUPEE_01, HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_1_FREESTANDING_RUPEE_02, HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_1_BARREL_01,             HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_1_BARREL_02,             HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_1_BARREL_03,             HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_ENEMY_DROP_TEKTITE,                                  CanKillEnemy(ACTOR_EN_TITE)),
            CHECK(RC_ENEMY_DROP_DESBREKO,                                 CanKillEnemy(ACTOR_EN_PR)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_CENTRAL_ROOM,  CAN_USE_ABILITY(SWIM)),
        },
        .events = {
            EVENT(RE_GREAT_BAY_GREEN_SWITCH_1, CAN_USE_MAGIC_ARROW(ICE)),
        }
    };
    Regions[RR_GREAT_BAY_TEMPLE_GREEN_PIPE_2] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_BARREL_01,   true),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_BARREL_02,   true),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_LOWER_CHEST, HAS_ITEM(ITEM_HOOKSHOT)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_UPPER_CHEST, (HAS_ITEM(ITEM_HOOKSHOT) && CAN_USE_MAGIC_ARROW(ICE))),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_POT_01,      CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_POT_02,      CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_POT_03,      CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_POT_04,      CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_POT_05,      CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_POT_06,      CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_POT_07,      CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_2_POT_08,      CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_ENEMY_DROP_DEXIHAND,                       CanKillEnemy(ACTOR_EN_WDHAND)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_COMPASS_ROOM,  CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CONNECTION(RR_GREAT_BAY_TEMPLE_GREEN_PIPE_3,  CAN_USE_MAGIC_ARROW(ICE)),
        },
    };
    Regions[RR_GREAT_BAY_TEMPLE_GREEN_PIPE_3] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_3_CHEST,          CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_3_LOWER_POT,      true),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_3_UPPER_POT_01,   CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_3_UPPER_POT_02,   CAN_USE_MAGIC_ARROW(FIRE)),
            CHECK(RC_GREAT_BAY_TEMPLE_SF_GREEN_PIPE_3_BARREL,      CAN_BE_ZORA && CAN_USE_MAGIC_ARROW(FIRE) && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_3_LARGE_CRATE_01, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_3_LARGE_CRATE_02, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_3_LARGE_CRATE_03, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_3_LARGE_CRATE_04, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_3_LARGE_CRATE_05, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_3_LARGE_CRATE_06, true),
            CHECK(RC_GREAT_BAY_TEMPLE_GREEN_PIPE_3_LARGE_CRATE_07, true),
            CHECK(RC_ENEMY_DROP_CHUCHU,                            CanKillEnemy(ACTOR_EN_SLIME)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_GREEN_PIPE_2,  true),
            CONNECTION(RR_GREAT_BAY_TEMPLE_MAP_ROOM,      CAN_USE_MAGIC_ARROW(FIRE)),
        },
        .events = {
            EVENT(RE_GREAT_BAY_GREEN_SWITCH_2, CAN_USE_MAGIC_ARROW(FIRE)),
        }
    };
    Regions[RR_GREAT_BAY_TEMPLE_MAP_ROOM] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_CHEST,                HAS_ITEM(ITEM_HOOKSHOT) || CAN_USE_MAGIC_ARROW(ICE)),
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_ROOM_SURFACE_POT_01,  true),
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_ROOM_SURFACE_POT_02,  true),
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_ROOM_SURFACE_POT_03,  CAN_BE_ZORA || CAN_USE_MAGIC_ARROW(ICE)),
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_ROOM_WATER_POT_01,    CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_ROOM_WATER_POT_02,    CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_ROOM_WATER_POT_03,    CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_ROOM_WATER_POT_04,    CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_ROOM_WATER_POT_05,    CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_ROOM_WATER_POT_06,    CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_ROOM_WATER_POT_07,    CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_MAP_ROOM_WATER_POT_08,    CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_SF_MAP_ROOM_POT,          CAN_BE_ZORA || CAN_USE_MAGIC_ARROW(ICE)),
            CHECK(RC_ENEMY_DROP_SKULLFISH, CanKillEnemy(ACTOR_EN_PR2)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_BABA_CHEST_ROOM,         CAN_BE_ZORA),
            CONNECTION(RR_GREAT_BAY_TEMPLE_CENTRAL_ROOM,            CAN_BE_ZORA && GBT_CAN_REVERSE_WATER_FLOW),
            CONNECTION(RR_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM,    CAN_USE_MAGIC_ARROW(ICE)),
        },
    };
    Regions[RR_GREAT_BAY_TEMPLE_PRE_BOSS_ROOM] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_PRE_BOSS_POT_01,         CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_PRE_BOSS_POT_02,         CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_PRE_BOSS_POT_03,         CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_PRE_BOSS_POT_04,         CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_PRE_BOSS_POT_05,         CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_PRE_BOSS_POT_06,         CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_PRE_BOSS_POT_07,         CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_PRE_BOSS_POT_08,         CAN_BE_ZORA),
            CHECK(RC_GREAT_BAY_TEMPLE_SF_PRE_BOSS_ABOVE_WATER, CAN_USE_PROJECTILE && HAS_ITEM(ITEM_MASK_GREAT_FAIRY)),
            CHECK(RC_GREAT_BAY_TEMPLE_SF_PRE_BOSS_UNDERWATER,  CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
        },
        .exits = { //     TO                                         FROM
            EXIT(ENTRANCE(GYORGS_LAIR, 0),                           ONE_WAY_EXIT, CHECK_DUNGEON_ITEM(DUNGEON_BOSS_KEY, DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE) && GBT_GREEN_SWITCH_FLOW),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_CENTRAL_ROOM, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
        },
        .events = {
            EVENT(RE_GREAT_BAY_GREEN_SWITCH_3, true),
        }
    };
    Regions[RR_GREAT_BAY_TEMPLE_RED_PIPE_BEFORE_WART] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_BEFORE_WART_POT_01, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_BEFORE_WART_POT_02, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_BEFORE_WART_POT_03, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_BEFORE_WART_POT_04, CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CHECK(RC_ENEMY_DROP_SHELLBLADE, CanKillEnemy(ACTOR_EN_SB)),
            CHECK(RC_ENEMY_DROP_OCTOROK, CanKillEnemy(ACTOR_EN_OKUTA)),
            CHECK(RC_ENEMY_DROP_SKULLFISH, CanKillEnemy(ACTOR_EN_PR2)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_CENTRAL_ROOM,    CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)),
            CONNECTION(RR_GREAT_BAY_TEMPLE_BEFORE_WART,     KEY_COUNT(GREAT_BAY_TEMPLE) >= 1),
        },
        .events = {
           EVENT(RE_GREAT_BAY_RED_SWITCH_1, CAN_USE_MAGIC_ARROW(ICE) && Flags_GetRandoInf(RANDO_INF_OBTAINED_SOUL_OF_ENEMY_OCTOROKS)),
        }
    };
    Regions[RR_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM_BARREL_01,      true),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM_BARREL_02,      true),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM_BARREL_03,      true),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM_BARREL_04,      true),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM_BARREL_05,      true),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM_LARGE_CRATE_01, true),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM_LARGE_CRATE_02, true),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM_LARGE_CRATE_03, true),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM_LARGE_CRATE_04, true),
            CHECK(RC_GREAT_BAY_TEMPLE_RED_PIPE_SWITCH_ROOM_LARGE_CRATE_05, true),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_MAP_ROOM, true),
        },
        .events = {
           EVENT(RE_GREAT_BAY_RED_SWITCH_2, CAN_USE_MAGIC_ARROW(ICE)),
        }
    };
    Regions[RR_GREAT_BAY_TEMPLE_WART] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_ICE_ARROW_CHEST,   CanKillEnemy(ACTOR_BOSS_04)),
            CHECK(RC_GREAT_BAY_TEMPLE_WART_POT_01,       true),
            CHECK(RC_GREAT_BAY_TEMPLE_WART_POT_02,       true),
            CHECK(RC_GREAT_BAY_TEMPLE_WART_POT_03,       true),
            CHECK(RC_GREAT_BAY_TEMPLE_WART_POT_04,       true),
            CHECK(RC_GREAT_BAY_TEMPLE_WART_POT_05,       true),
            CHECK(RC_GREAT_BAY_TEMPLE_WART_POT_06,       true),
            CHECK(RC_GREAT_BAY_TEMPLE_WART_POT_07,       true),
            CHECK(RC_GREAT_BAY_TEMPLE_WART_POT_08,       true),
            CHECK(RC_ENEMY_DROP_WART,                    CanKillEnemy(ACTOR_BOSS_04)),
        },
        .connections = {
            CONNECTION(RR_GREAT_BAY_TEMPLE_BEFORE_WART, CanKillEnemy(ACTOR_BOSS_04)),
        },
    };
    Regions[RR_GREAT_BAY_TEMPLE_WATER_WHEEL_ROOM] = RandoRegion{ .sceneId = SCENE_SEA,
        .checks = {
            CHECK(RC_GREAT_BAY_TEMPLE_SF_WATER_WHEEL_PLATFORM,           (CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)) || (HAS_ITEM(ITEM_BOW) && HAS_ITEM(ITEM_MASK_GREAT_FAIRY))),
            CHECK(RC_GREAT_BAY_TEMPLE_SF_WATER_WHEEL_SKULLTULA,          true),
            CHECK(RC_GREAT_BAY_TEMPLE_WATER_WHEEL_FREESTANDING_RUPEE_01, true),
            CHECK(RC_GREAT_BAY_TEMPLE_WATER_WHEEL_FREESTANDING_RUPEE_02, true),
            CHECK(RC_GREAT_BAY_TEMPLE_WATER_WHEEL_FREESTANDING_RUPEE_03, true),
            CHECK(RC_GREAT_BAY_TEMPLE_WATER_WHEEL_FREESTANDING_RUPEE_04, true),
            CHECK(RC_GREAT_BAY_TEMPLE_WATER_WHEEL_FREESTANDING_RUPEE_05, true),
            CHECK(RC_ENEMY_DROP_SKULLTULA, CanKillEnemy(ACTOR_EN_ST)),
        },
        .connections = {
            // #578 part 3 (second pass) — MMRT_GBT_WATERWHEEL_GORON ("Cross GBT Waterwheel Room as
            // Goron — Skip the first yellow turnkey with a precise Goron Roll"), DEFAULT OFF. The
            // TARGET region is not guessed from the display name: the RESERVED sibling
            // MMRT_GBT_WATERWHEEL_HOVERS says "Similar to using Goron, Hover Boots can be used to get
            // on the water wheel and then reach the central room", which names this connection. Goron
            // rolls the wheel rather than swimming, so CAN_BE_GORON is the disjunct's own term and the
            // vanilla Zora-and-swim pair is kept verbatim as the other disjunct.
            CONNECTION(RR_GREAT_BAY_TEMPLE_CENTRAL_ROOM,  (CAN_BE_ZORA && CAN_USE_ABILITY(SWIM)) || (MM_TRICK(MMRT_GBT_WATERWHEEL_GORON) && CAN_BE_GORON)),
            CONNECTION(RR_GREAT_BAY_TEMPLE_ENTRANCE,      true),
        }
    };
}, {});
// clang-format on
