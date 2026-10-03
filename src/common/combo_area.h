/**
 * @file combo_area.h
 * @brief The one shared AREA vocabulary both games' hints speak (epic #575,
 *        child H1; operator ruling 2026-10-01: "one shared vocabulary").
 *
 * ============================================================================
 * WHAT IT IS
 * ============================================================================
 *
 * A common-owned, origin-tagged area id space with English display names. An
 * area is `{game, area}`: `game` is GAME_OOT or GAME_MM and `area` indexes that
 * game's HALF (1..Combo_Area_Count(game)). The two halves never share a value
 * in meaning: no OoT area equals an MM area, and no table maps one game's area
 * to the other's (ADR 0002 amendment of 2026-10-02).
 *
 *  - The OoT half mirrors SoH's RandomizerArea 1:1 (RA_KOKIRI_FOREST ..
 *    RA_GANONS_CASTLE, 35 areas), and each name is OoT's own English clear text,
 *    so an OoT hint that names an OoT area reads exactly as it always has.
 *  - The MM half is OoTMM's 46 Majora's Mask regions, in OoTMM's order
 *    (data/defs/regions.yml), named from OoTMM's in-game table with its article
 *    and preposition (packages/generator/src/common/text/text.c), spelled the way
 *    MM's own text spells them ("Pirates' Fortress", "Ikana Graveyard").
 *  - Two specials are valid in either half: COMBO_AREA_NONE ("nowhere", never
 *    hinted) and COMBO_AREA_POCKET (an item the player starts with).
 *
 * ============================================================================
 * WHY THIS SATISFIES ADR 0002
 * ============================================================================
 *
 * The ids are src/common's own and every one carries its game. No RA_*, RC_*,
 * RI_*, scene id or region id crosses: each game maps ITS OWN checks into ITS
 * OWN half from its own TU, through the adapter it registers here (the
 * describer precedent in foreign_items.h: a file-scope registrar that stores a
 * pointer and calls nothing). Combo_Area_OfCheck is keyed (hostGame, check)
 * exactly as Combo_DescribeCheckName is. This header includes no game header.
 *
 * Area values are APPEND-ONLY once anything persists them, and anything that
 * persists an area does so BY NAME (Combo_Area_Name / Combo_Area_FromName),
 * never by value.
 */

#ifndef RSBS_COMMON_COMBO_AREA_H
#define RSBS_COMMON_COMBO_AREA_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One area of one game's half. `game` is a GameId (GAME_OOT / GAME_MM). */
typedef struct {
    uint8_t game;
    uint8_t area;
} ComboArea;

/** "nowhere": a check with no area (MM's enemy drops, a disconnected OoT region). Never hinted. */
#define COMBO_AREA_NONE 0u
/** Link's Pocket: an item the player starts with. */
#define COMBO_AREA_POCKET 0xFFu

/* ---- The OoT half: SoH's RandomizerArea order, RA_KOKIRI_FOREST = 1 ------- */
enum {
    COMBO_AREA_OOT_KOKIRI_FOREST = 1,
    COMBO_AREA_OOT_THE_LOST_WOODS,
    COMBO_AREA_OOT_SACRED_FOREST_MEADOW,
    COMBO_AREA_OOT_HYRULE_FIELD,
    COMBO_AREA_OOT_LAKE_HYLIA,
    COMBO_AREA_OOT_GERUDO_VALLEY,
    COMBO_AREA_OOT_GERUDO_FORTRESS,
    COMBO_AREA_OOT_HAUNTED_WASTELAND,
    COMBO_AREA_OOT_DESERT_COLOSSUS,
    COMBO_AREA_OOT_THE_MARKET,
    COMBO_AREA_OOT_TEMPLE_OF_TIME,
    COMBO_AREA_OOT_HYRULE_CASTLE,
    COMBO_AREA_OOT_OUTSIDE_GANONS_CASTLE,
    COMBO_AREA_OOT_CASTLE_GROUNDS,
    COMBO_AREA_OOT_KAKARIKO_VILLAGE,
    COMBO_AREA_OOT_THE_GRAVEYARD,
    COMBO_AREA_OOT_DEATH_MOUNTAIN_TRAIL,
    COMBO_AREA_OOT_GORON_CITY,
    COMBO_AREA_OOT_DEATH_MOUNTAIN_CRATER,
    COMBO_AREA_OOT_ZORAS_RIVER,
    COMBO_AREA_OOT_ZORAS_DOMAIN,
    COMBO_AREA_OOT_ZORAS_FOUNTAIN,
    COMBO_AREA_OOT_LON_LON_RANCH,
    COMBO_AREA_OOT_DEKU_TREE,
    COMBO_AREA_OOT_DODONGOS_CAVERN,
    COMBO_AREA_OOT_JABU_JABUS_BELLY,
    COMBO_AREA_OOT_FOREST_TEMPLE,
    COMBO_AREA_OOT_FIRE_TEMPLE,
    COMBO_AREA_OOT_WATER_TEMPLE,
    COMBO_AREA_OOT_SPIRIT_TEMPLE,
    COMBO_AREA_OOT_SHADOW_TEMPLE,
    COMBO_AREA_OOT_BOTTOM_OF_THE_WELL,
    COMBO_AREA_OOT_ICE_CAVERN,
    COMBO_AREA_OOT_GERUDO_TRAINING_GROUND,
    COMBO_AREA_OOT_GANONS_CASTLE,
    COMBO_AREA_OOT_END /* one past the last OoT area */
};

/* ---- The MM half: OoTMM's 46 MM regions, in OoTMM's order ----------------- */
enum {
    COMBO_AREA_MM_WOODFALL_TEMPLE = 1,
    COMBO_AREA_MM_SNOWHEAD_TEMPLE,
    COMBO_AREA_MM_GREAT_BAY_TEMPLE,
    COMBO_AREA_MM_STONE_TOWER_TEMPLE,
    COMBO_AREA_MM_CLOCK_TOWN_SOUTH,
    COMBO_AREA_MM_CLOCK_TOWN_NORTH,
    COMBO_AREA_MM_CLOCK_TOWN_EAST,
    COMBO_AREA_MM_CLOCK_TOWN_WEST,
    COMBO_AREA_MM_LAUNDRY_POOL,
    COMBO_AREA_MM_GIANT_DREAM,
    COMBO_AREA_MM_CLOCK_TOWER_ROOFTOP,
    COMBO_AREA_MM_STOCK_POT_INN,
    COMBO_AREA_MM_TERMINA_FIELD,
    COMBO_AREA_MM_ROAD_TO_SWAMP,
    COMBO_AREA_MM_SOUTHERN_SWAMP,
    COMBO_AREA_MM_DEKU_PALACE,
    COMBO_AREA_MM_WOODFALL,
    COMBO_AREA_MM_PATH_TO_MOUNTAIN_VILLAGE,
    COMBO_AREA_MM_MOUNTAIN_VILLAGE,
    COMBO_AREA_MM_PATH_TO_SNOWHEAD,
    COMBO_AREA_MM_TWIN_ISLANDS,
    COMBO_AREA_MM_GORON_VILLAGE,
    COMBO_AREA_MM_SNOWHEAD,
    COMBO_AREA_MM_MILK_ROAD,
    COMBO_AREA_MM_ROMANI_RANCH,
    COMBO_AREA_MM_GREAT_BAY_COAST,
    COMBO_AREA_MM_PIRATE_FORTRESS_EXTERIOR,
    COMBO_AREA_MM_PIRATE_FORTRESS_SEWERS,
    COMBO_AREA_MM_PIRATE_FORTRESS_INTERIOR,
    COMBO_AREA_MM_ZORA_CAPE,
    COMBO_AREA_MM_ZORA_HALL,
    COMBO_AREA_MM_PINNACLE_ROCK,
    COMBO_AREA_MM_ROAD_TO_IKANA,
    COMBO_AREA_MM_IKANA_GRAVEYARD,
    COMBO_AREA_MM_IKANA_CANYON,
    COMBO_AREA_MM_IKANA_CASTLE,
    COMBO_AREA_MM_BENEATH_THE_WELL,
    COMBO_AREA_MM_SECRET_SHRINE,
    COMBO_AREA_MM_STONE_TOWER,
    COMBO_AREA_MM_MOON,
    COMBO_AREA_MM_SPIDER_HOUSE_SWAMP,
    COMBO_AREA_MM_SPIDER_HOUSE_OCEAN,
    COMBO_AREA_MM_TINGLE,
    COMBO_AREA_MM_STONE_TOWER_TEMPLE_INVERTED,
    COMBO_AREA_MM_BUTLER_RACE,
    COMBO_AREA_MM_GORON_RACETRACK,
    COMBO_AREA_MM_END /* one past the last MM area */
};

/** English display name with its article ("the Southern Swamp", "Kokiri Forest"),
 *  or NULL for a non-game or an area outside that game's half. The specials
 *  name "nowhere" and "Link's Pocket" in either half. */
const char* Combo_Area_Name(ComboArea a);

/** The preposition a sentence puts before Combo_Area_Name ("in", "on",
 *  "around", "from", ...), or NULL. The MM half carries OoTMM's; the OoT half
 *  carries none, because OoT's own clear text already reads in place ("outside
 *  Ganon's Castle", "Inside Ganon's Castle"); COMBO_AREA_NONE carries none. */
const char* Combo_Area_Prepos(ComboArea a);

/** The exact inverse of Combo_Area_Name within `game`'s half (specials
 *  included). Returns false, leaving *out untouched, for an unknown name. */
bool Combo_Area_FromName(uint8_t game, const char* name, ComboArea* out);

/** Number of areas in `game`'s half (35 for OoT, 46 for MM), 0 for a non-game. */
int Combo_Area_Count(uint8_t game);

/** Whether `a` is one of its game's dungeons: OoT's twelve RandomizerArea
 *  dungeons (Deku Tree .. Inside Ganon's Castle), MM's four temples plus the
 *  inverted Stone Tower Temple. Specials are never dungeons. */
bool Combo_Area_IsDungeon(ComboArea a);

/** Whether `a` names an area of its game's half or one of the two specials. */
bool Combo_Area_IsValid(ComboArea a);

/** One game's mapping of ITS OWN check ids into ITS OWN half. */
typedef struct {
    /** This game's check id-space -> this game's half (1..Count), COMBO_AREA_POCKET,
     *  or COMBO_AREA_NONE (no area, or an id this game does not know). */
    uint8_t (*areaOfCheck)(uint16_t check);
} ComboAreaAdapter;

/** Register (or, with NULL, un-register) `game`'s adapter. A non-game is
 *  ignored. File-scope registrar: stores the pointer, calls nothing. */
void Combo_RegisterAreaAdapter(uint8_t game, const ComboAreaAdapter* adapter);

/** Whether `game` has registered an adapter. */
bool Combo_Area_HasAdapter(uint8_t game);

/** The area of `check` in `hostGame`'s check id-space, routed to that game's
 *  adapter. {hostGame, COMBO_AREA_NONE} for an unregistered game or a check
 *  without an area; {GAME_NONE, COMBO_AREA_NONE} for a non-game. An adapter
 *  answer outside the half is reported as NONE rather than passed on. */
ComboArea Combo_Area_OfCheck(uint8_t hostGame, uint16_t check);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_AREA_H
