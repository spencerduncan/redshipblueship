/**
 * @file combo_area.c
 * @brief The shared area vocabulary (epic #575, child H1). See combo_area.h.
 *
 * Game-header-free like foreign_items.c: the two halves' names are this TU's
 * own literals, and each game's check-to-area mapping arrives through the
 * adapter that game registers from its own TU. Nothing here knows a RA_*,
 * RC_*, RI_*, scene id or region id.
 */

#include "combo_area.h"
#include "game.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const char* name;   /* English, with its article */
    const char* prepos; /* the preposition a sentence puts before it, or NULL */
} ComboAreaText;

/* The OoT half, index = area (1-based), in SoH's RandomizerArea order. Each name
 * is OoT's own English clear text for that area (3drando/hint_list.cpp), so an OoT
 * hint naming an OoT area reads as it always has; ComboAreaVocabulary compares
 * every entry with OoT's live hint table. OoT's clear text carries no preposition
 * (its sentences supply their own), so neither does this half. */
static const ComboAreaText sOoTAreas[COMBO_AREA_OOT_END] = {
    [0] = { NULL, NULL },
    [COMBO_AREA_OOT_KOKIRI_FOREST] = { "Kokiri Forest", NULL },
    [COMBO_AREA_OOT_THE_LOST_WOODS] = { "the Lost Woods", NULL },
    [COMBO_AREA_OOT_SACRED_FOREST_MEADOW] = { "Sacred Forest Meadow", NULL },
    [COMBO_AREA_OOT_HYRULE_FIELD] = { "Hyrule Field", NULL },
    [COMBO_AREA_OOT_LAKE_HYLIA] = { "Lake Hylia", NULL },
    [COMBO_AREA_OOT_GERUDO_VALLEY] = { "Gerudo Valley", NULL },
    [COMBO_AREA_OOT_GERUDO_FORTRESS] = { "Gerudo's Fortress", NULL },
    [COMBO_AREA_OOT_HAUNTED_WASTELAND] = { "Haunted Wasteland", NULL },
    [COMBO_AREA_OOT_DESERT_COLOSSUS] = { "Desert Colossus", NULL },
    [COMBO_AREA_OOT_THE_MARKET] = { "the Market", NULL },
    [COMBO_AREA_OOT_TEMPLE_OF_TIME] = { "Temple of Time", NULL },
    [COMBO_AREA_OOT_HYRULE_CASTLE] = { "Hyrule Castle", NULL },
    [COMBO_AREA_OOT_OUTSIDE_GANONS_CASTLE] = { "outside Ganon's Castle", NULL },
    [COMBO_AREA_OOT_CASTLE_GROUNDS] = { "the Castle Grounds", NULL },
    [COMBO_AREA_OOT_KAKARIKO_VILLAGE] = { "Kakariko Village", NULL },
    [COMBO_AREA_OOT_THE_GRAVEYARD] = { "the Graveyard", NULL },
    [COMBO_AREA_OOT_DEATH_MOUNTAIN_TRAIL] = { "Death Mountain Trail", NULL },
    [COMBO_AREA_OOT_GORON_CITY] = { "Goron City", NULL },
    [COMBO_AREA_OOT_DEATH_MOUNTAIN_CRATER] = { "Death Mountain Crater", NULL },
    [COMBO_AREA_OOT_ZORAS_RIVER] = { "Zora's River", NULL },
    [COMBO_AREA_OOT_ZORAS_DOMAIN] = { "Zora's Domain", NULL },
    [COMBO_AREA_OOT_ZORAS_FOUNTAIN] = { "Zora's Fountain", NULL },
    [COMBO_AREA_OOT_LON_LON_RANCH] = { "Lon Lon Ranch", NULL },
    [COMBO_AREA_OOT_DEKU_TREE] = { "Deku Tree", NULL },
    [COMBO_AREA_OOT_DODONGOS_CAVERN] = { "Dodongo's Cavern", NULL },
    [COMBO_AREA_OOT_JABU_JABUS_BELLY] = { "Jabu-Jabu's Belly", NULL },
    [COMBO_AREA_OOT_FOREST_TEMPLE] = { "Forest Temple", NULL },
    [COMBO_AREA_OOT_FIRE_TEMPLE] = { "Fire Temple", NULL },
    [COMBO_AREA_OOT_WATER_TEMPLE] = { "Water Temple", NULL },
    [COMBO_AREA_OOT_SPIRIT_TEMPLE] = { "Spirit Temple", NULL },
    [COMBO_AREA_OOT_SHADOW_TEMPLE] = { "Shadow Temple", NULL },
    [COMBO_AREA_OOT_BOTTOM_OF_THE_WELL] = { "Bottom of the Well", NULL },
    [COMBO_AREA_OOT_ICE_CAVERN] = { "Ice Cavern", NULL },
    [COMBO_AREA_OOT_GERUDO_TRAINING_GROUND] = { "Gerudo Training Ground", NULL },
    [COMBO_AREA_OOT_GANONS_CASTLE] = { "Inside Ganon's Castle", NULL },
};

/* The MM half, index = area (1-based), in OoTMM's region order
 * (data/defs/regions.yml MM_*). Names and prepositions are OoTMM's in-game table
 * (packages/generator/src/common/text/text.c, kRegionNamesMm) without its colour
 * codes. Three names are respelled the way MM's own text spells the place:
 * "Pirates' Fortress" (OoTMM: "Pirate's"), "Ikana Graveyard" (OoTMM: "Ikana's
 * Graveyard") and "Beneath the Well" (OoTMM: "Beneath The Well"). */
static const ComboAreaText sMMAreas[COMBO_AREA_MM_END] = {
    [0] = { NULL, NULL },
    [COMBO_AREA_MM_WOODFALL_TEMPLE] = { "Woodfall Temple", "in" },
    [COMBO_AREA_MM_SNOWHEAD_TEMPLE] = { "Snowhead Temple", "in" },
    [COMBO_AREA_MM_GREAT_BAY_TEMPLE] = { "Great Bay Temple", "in" },
    [COMBO_AREA_MM_STONE_TOWER_TEMPLE] = { "Stone Tower Temple", "in" },
    [COMBO_AREA_MM_CLOCK_TOWN_SOUTH] = { "South Clock Town", "in" },
    [COMBO_AREA_MM_CLOCK_TOWN_NORTH] = { "North Clock Town", "in" },
    [COMBO_AREA_MM_CLOCK_TOWN_EAST] = { "East Clock Town", "in" },
    [COMBO_AREA_MM_CLOCK_TOWN_WEST] = { "West Clock Town", "in" },
    [COMBO_AREA_MM_LAUNDRY_POOL] = { "the Laundry Pool", "in" },
    [COMBO_AREA_MM_GIANT_DREAM] = { "the Giant's Dream", "inside" },
    [COMBO_AREA_MM_CLOCK_TOWER_ROOFTOP] = { "Clock Tower Roof", "on" },
    [COMBO_AREA_MM_STOCK_POT_INN] = { "the Stock Pot Inn", "in" },
    [COMBO_AREA_MM_TERMINA_FIELD] = { "Termina Field", "in" },
    [COMBO_AREA_MM_ROAD_TO_SWAMP] = { "the Road to the Swamp", "on" },
    [COMBO_AREA_MM_SOUTHERN_SWAMP] = { "the Southern Swamp", "in" },
    [COMBO_AREA_MM_DEKU_PALACE] = { "Deku Palace", "in" },
    [COMBO_AREA_MM_WOODFALL] = { "Woodfall", "in" },
    [COMBO_AREA_MM_PATH_TO_MOUNTAIN_VILLAGE] = { "the Path to Mountain Village", "on" },
    [COMBO_AREA_MM_MOUNTAIN_VILLAGE] = { "the Mountain Village", "in" },
    [COMBO_AREA_MM_PATH_TO_SNOWHEAD] = { "the Path to Snowhead", "on" },
    [COMBO_AREA_MM_TWIN_ISLANDS] = { "Twin Islands", "around" },
    [COMBO_AREA_MM_GORON_VILLAGE] = { "Goron Village", "around" },
    [COMBO_AREA_MM_SNOWHEAD] = { "Snowhead", "in" },
    [COMBO_AREA_MM_MILK_ROAD] = { "the Milk Road", "around" },
    [COMBO_AREA_MM_ROMANI_RANCH] = { "Romani Ranch", "around" },
    [COMBO_AREA_MM_GREAT_BAY_COAST] = { "Great Bay Coast", "around" },
    [COMBO_AREA_MM_PIRATE_FORTRESS_EXTERIOR] = { "the Pirates' Fortress Exterior", "in" },
    [COMBO_AREA_MM_PIRATE_FORTRESS_SEWERS] = { "the Pirates' Fortress Sewers", "in" },
    [COMBO_AREA_MM_PIRATE_FORTRESS_INTERIOR] = { "the Pirates' Fortress Interior", "in" },
    [COMBO_AREA_MM_ZORA_CAPE] = { "Zora Cape", "around" },
    [COMBO_AREA_MM_ZORA_HALL] = { "Zora Hall", "in" },
    [COMBO_AREA_MM_PINNACLE_ROCK] = { "Pinnacle Rock", "in" },
    [COMBO_AREA_MM_ROAD_TO_IKANA] = { "the Road to Ikana", "on" },
    [COMBO_AREA_MM_IKANA_GRAVEYARD] = { "Ikana Graveyard", "in" },
    [COMBO_AREA_MM_IKANA_CANYON] = { "Ikana Canyon", "in" },
    [COMBO_AREA_MM_IKANA_CASTLE] = { "the Ancient Castle of Ikana", "in" },
    [COMBO_AREA_MM_BENEATH_THE_WELL] = { "Beneath the Well", "somewhere" },
    [COMBO_AREA_MM_SECRET_SHRINE] = { "a Secret Shrine", "in" },
    [COMBO_AREA_MM_STONE_TOWER] = { "the Stone Tower", "on" },
    [COMBO_AREA_MM_MOON] = { "the Moon", "on" },
    [COMBO_AREA_MM_SPIDER_HOUSE_SWAMP] = { "the Swamp Spider House", "in" },
    [COMBO_AREA_MM_SPIDER_HOUSE_OCEAN] = { "the Ocean Spider House", "in" },
    [COMBO_AREA_MM_TINGLE] = { "Tingle", "from" },
    [COMBO_AREA_MM_STONE_TOWER_TEMPLE_INVERTED] = { "Inverted Stone Tower Temple", "in" },
    [COMBO_AREA_MM_BUTLER_RACE] = { "the Butler Race", "inside" },
    [COMBO_AREA_MM_GORON_RACETRACK] = { "the Goron Racetrack", "inside" },
};

/* The two specials, the same in either half (OoTMM's kRegionNowhere and
 * kRegionLinkPocket). */
static const ComboAreaText sNone = { "nowhere", NULL };
static const ComboAreaText sPocket = { "Link's Pocket", "inside" };

static const ComboAreaAdapter* sAdapters[3]; /* indexed by GameId; [GAME_NONE] stays NULL */
static const ComboFoolishAdapter* sFoolishAdapters[3]; /* #575 H5a; same indexing */

static bool IsGame(uint8_t game) {
    return game == (uint8_t)GAME_OOT || game == (uint8_t)GAME_MM;
}

int Combo_Area_Count(uint8_t game) {
    if (game == (uint8_t)GAME_OOT) {
        return COMBO_AREA_OOT_END - 1;
    }
    if (game == (uint8_t)GAME_MM) {
        return COMBO_AREA_MM_END - 1;
    }
    return 0;
}

static const ComboAreaText* TextOf(ComboArea a) {
    if (!IsGame(a.game)) {
        return NULL;
    }
    if (a.area == COMBO_AREA_NONE) {
        return &sNone;
    }
    if (a.area == COMBO_AREA_POCKET) {
        return &sPocket;
    }
    if ((int)a.area > Combo_Area_Count(a.game)) {
        return NULL;
    }
    return a.game == (uint8_t)GAME_OOT ? &sOoTAreas[a.area] : &sMMAreas[a.area];
}

bool Combo_Area_IsValid(ComboArea a) {
    return TextOf(a) != NULL;
}

const char* Combo_Area_Name(ComboArea a) {
    const ComboAreaText* t = TextOf(a);
    return t != NULL ? t->name : NULL;
}

const char* Combo_Area_Prepos(ComboArea a) {
    const ComboAreaText* t = TextOf(a);
    return t != NULL ? t->prepos : NULL;
}

bool Combo_Area_FromName(uint8_t game, const char* name, ComboArea* out) {
    if (!IsGame(game) || name == NULL || out == NULL) {
        return false;
    }
    const int count = Combo_Area_Count(game);
    for (int i = 1; i <= count; i++) {
        const ComboArea a = { game, (uint8_t)i };
        if (strcmp(Combo_Area_Name(a), name) == 0) {
            *out = a;
            return true;
        }
    }
    if (strcmp(sNone.name, name) == 0) {
        out->game = game;
        out->area = COMBO_AREA_NONE;
        return true;
    }
    if (strcmp(sPocket.name, name) == 0) {
        out->game = game;
        out->area = COMBO_AREA_POCKET;
        return true;
    }
    return false;
}

bool Combo_Area_IsDungeon(ComboArea a) {
    if (a.area == COMBO_AREA_NONE || a.area == COMBO_AREA_POCKET || !Combo_Area_IsValid(a)) {
        return false;
    }
    if (a.game == (uint8_t)GAME_OOT) {
        return a.area >= COMBO_AREA_OOT_DEKU_TREE && a.area <= COMBO_AREA_OOT_GANONS_CASTLE;
    }
    switch (a.area) {
        case COMBO_AREA_MM_WOODFALL_TEMPLE:
        case COMBO_AREA_MM_SNOWHEAD_TEMPLE:
        case COMBO_AREA_MM_GREAT_BAY_TEMPLE:
        case COMBO_AREA_MM_STONE_TOWER_TEMPLE:
        case COMBO_AREA_MM_STONE_TOWER_TEMPLE_INVERTED:
            return true;
        default:
            return false;
    }
}

void Combo_RegisterAreaAdapter(uint8_t game, const ComboAreaAdapter* adapter) {
    if (!IsGame(game)) {
        return;
    }
    sAdapters[game] = adapter;
}

bool Combo_Area_HasAdapter(uint8_t game) {
    return IsGame(game) && sAdapters[game] != NULL && sAdapters[game]->areaOfCheck != NULL;
}

ComboArea Combo_Area_OfCheck(uint8_t hostGame, uint16_t check) {
    ComboArea result = { (uint8_t)GAME_NONE, COMBO_AREA_NONE };
    if (!IsGame(hostGame)) {
        return result;
    }
    result.game = hostGame;
    if (!Combo_Area_HasAdapter(hostGame)) {
        return result;
    }
    const ComboArea answered = { hostGame, sAdapters[hostGame]->areaOfCheck(check) };
    if (Combo_Area_IsValid(answered)) {
        result.area = answered.area;
    }
    return result;
}

/* ---- #575 H5a: the foolish rule ------------------------------------------- */

void Combo_RegisterFoolishAdapter(uint8_t game, const ComboFoolishAdapter* adapter) {
    if (!IsGame(game)) {
        return;
    }
    sFoolishAdapters[game] = adapter;
}

bool Combo_Area_HasFoolishAdapter(uint8_t game) {
    return IsGame(game) && sFoolishAdapters[game] != NULL && sFoolishAdapters[game]->hostStanding != NULL;
}

/* Every area of `check` that names an area of `game`'s half, into
 * out[0..COMBO_FOOLISH_AREAS_CAP). A special (nowhere, Link's Pocket) or an
 * answer outside the half is dropped and sets *outSpecial. */
#define COMBO_FOOLISH_AREAS_CAP 64
static int FoolishAreasOf(uint8_t game, uint16_t check, uint8_t* out, bool* outSpecial) {
    const ComboFoolishAdapter* fa = sFoolishAdapters[game];
    uint8_t raw[COMBO_FOOLISH_AREAS_CAP];
    int n;
    if (fa->areasOfCheck != NULL) {
        n = fa->areasOfCheck(check, raw, COMBO_FOOLISH_AREAS_CAP);
        if (n > COMBO_FOOLISH_AREAS_CAP) {
            n = COMBO_FOOLISH_AREAS_CAP;
        }
    } else {
        raw[0] = Combo_Area_OfCheck(game, check).area;
        n = 1;
    }
    int kept = 0;
    *outSpecial = false;
    for (int i = 0; i < n; i++) {
        const ComboArea a = { game, raw[i] };
        if (a.area != COMBO_AREA_NONE && a.area != COMBO_AREA_POCKET && Combo_Area_IsValid(a)) {
            out[kept++] = a.area;
        } else {
            *outSpecial = true;
        }
    }
    return kept;
}

int Combo_Area_ComputeFoolish(uint8_t game, ComboFoolishAreas* out) {
    if (out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->game = game;
    if (!Combo_Area_HasFoolishAdapter(game) || !Combo_Area_HasAdapter(game)) {
        return -1;
    }
    bool hintable[0x100];
    bool important[0x100];
    memset(hintable, 0, sizeof(hintable));
    memset(important, 0, sizeof(important));
    const ComboFoolishAdapter* fa = sFoolishAdapters[game];
    for (uint32_t check = 0; check < fa->checkEnd; check++) {
        const uint8_t standing = fa->hostStanding((uint16_t)check);
        if ((standing & (COMBO_HOST_HINTABLE | COMBO_HOST_IMPORTANT)) == 0) {
            continue;
        }
        uint8_t areas[COMBO_FOOLISH_AREAS_CAP];
        bool special = false;
        const int n = FoolishAreasOf(game, (uint16_t)check, areas, &special);
        for (int i = 0; i < n; i++) {
            hintable[areas[i]] = hintable[areas[i]] || (standing & COMBO_HOST_HINTABLE) != 0;
            important[areas[i]] = important[areas[i]] || (standing & COMBO_HOST_IMPORTANT) != 0;
        }
    }
    const int count = Combo_Area_Count(game);
    for (int a = 1; a <= count; a++) {
        if (hintable[a] && !important[a]) {
            out->foolish[a] = true;
            out->count++;
        }
    }
    return out->count;
}

bool Combo_Area_IsFoolish(const ComboFoolishAreas* set, ComboArea a) {
    return set != NULL && a.game == set->game && a.area != COMBO_AREA_NONE && a.area != COMBO_AREA_POCKET &&
           set->foolish[a.area];
}

bool Combo_Area_CheckIsFoolish(const ComboFoolishAreas* set, uint16_t check) {
    if (set == NULL || !Combo_Area_HasFoolishAdapter(set->game) || !Combo_Area_HasAdapter(set->game)) {
        return false;
    }
    // A check that is also "nowhere" or in Link's Pocket is never foolish, as
    // OoT's native rule never calls RA_NONE / RA_LINKS_POCKET barren.
    uint8_t areas[COMBO_FOOLISH_AREAS_CAP];
    bool special = false;
    const int n = FoolishAreasOf(set->game, check, areas, &special);
    if (n == 0 || special) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        if (!set->foolish[areas[i]]) {
            return false;
        }
    }
    return true;
}
