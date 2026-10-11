/**
 * @file HintAreaOoT.cpp
 * @brief OoT's half of the shared area vocabulary (epic #575, child H1).
 *
 * The OoT half of src/common's vocabulary (combo_area.h) mirrors SoH's
 * RandomizerArea 1:1. This TU maps RandomizerArea into it and registers OoT's
 * area adapter, whose answer for a check is that location's first area, as
 * OoT's own hints name it (ItemLocation::GetFirstArea). ADR 0002: OoT's ids are
 * read only here; what crosses into src/common is the {GAME_OOT, area} pair.
 *
 * RA_TERMINA is not an OoT area. It is the hint layer's "this target is in the
 * MM half" marker (PR #743), so it maps to no OoT area, like RA_NONE (a region
 * the graph could not place).
 *
 * Locations get their areas from SetAreas() over the region graph, which only
 * generation (Fill) and the spoiler loader run. A cold load of a saved file
 * builds a fresh Rando::Context whose locations have no areas, so this TU runs
 * the spoiler loader's sequence once at the end of the randomizer section's
 * load (SaveManager::LoadRandomizer), never lazily from an adapter call.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/entrance.h"
#include "soh/Enhancements/randomizer/3drando/fill.hpp"

#include "combo_area.h" // src/common: the shared vocabulary and the registrar
#include "game.h"       // src/common: GAME_OOT

namespace {

struct OoTAreaPair {
    RandomizerArea ra;
    uint8_t area;
};

// RandomizerArea -> the OoT half. Every OoT area, in RandomizerArea order.
const OoTAreaPair kOoTAreaPairs[] = {
    { RA_LINKS_POCKET, COMBO_AREA_POCKET },
    { RA_KOKIRI_FOREST, COMBO_AREA_OOT_KOKIRI_FOREST },
    { RA_THE_LOST_WOODS, COMBO_AREA_OOT_THE_LOST_WOODS },
    { RA_SACRED_FOREST_MEADOW, COMBO_AREA_OOT_SACRED_FOREST_MEADOW },
    { RA_HYRULE_FIELD, COMBO_AREA_OOT_HYRULE_FIELD },
    { RA_LAKE_HYLIA, COMBO_AREA_OOT_LAKE_HYLIA },
    { RA_GERUDO_VALLEY, COMBO_AREA_OOT_GERUDO_VALLEY },
    { RA_GERUDO_FORTRESS, COMBO_AREA_OOT_GERUDO_FORTRESS },
    { RA_HAUNTED_WASTELAND, COMBO_AREA_OOT_HAUNTED_WASTELAND },
    { RA_DESERT_COLOSSUS, COMBO_AREA_OOT_DESERT_COLOSSUS },
    { RA_THE_MARKET, COMBO_AREA_OOT_THE_MARKET },
    { RA_TEMPLE_OF_TIME, COMBO_AREA_OOT_TEMPLE_OF_TIME },
    { RA_HYRULE_CASTLE, COMBO_AREA_OOT_HYRULE_CASTLE },
    { RA_OUTSIDE_GANONS_CASTLE, COMBO_AREA_OOT_OUTSIDE_GANONS_CASTLE },
    { RA_CASTLE_GROUNDS, COMBO_AREA_OOT_CASTLE_GROUNDS },
    { RA_KAKARIKO_VILLAGE, COMBO_AREA_OOT_KAKARIKO_VILLAGE },
    { RA_THE_GRAVEYARD, COMBO_AREA_OOT_THE_GRAVEYARD },
    { RA_DEATH_MOUNTAIN_TRAIL, COMBO_AREA_OOT_DEATH_MOUNTAIN_TRAIL },
    { RA_GORON_CITY, COMBO_AREA_OOT_GORON_CITY },
    { RA_DEATH_MOUNTAIN_CRATER, COMBO_AREA_OOT_DEATH_MOUNTAIN_CRATER },
    { RA_ZORAS_RIVER, COMBO_AREA_OOT_ZORAS_RIVER },
    { RA_ZORAS_DOMAIN, COMBO_AREA_OOT_ZORAS_DOMAIN },
    { RA_ZORAS_FOUNTAIN, COMBO_AREA_OOT_ZORAS_FOUNTAIN },
    { RA_LON_LON_RANCH, COMBO_AREA_OOT_LON_LON_RANCH },
    { RA_DEKU_TREE, COMBO_AREA_OOT_DEKU_TREE },
    { RA_DODONGOS_CAVERN, COMBO_AREA_OOT_DODONGOS_CAVERN },
    { RA_JABU_JABUS_BELLY, COMBO_AREA_OOT_JABU_JABUS_BELLY },
    { RA_FOREST_TEMPLE, COMBO_AREA_OOT_FOREST_TEMPLE },
    { RA_FIRE_TEMPLE, COMBO_AREA_OOT_FIRE_TEMPLE },
    { RA_WATER_TEMPLE, COMBO_AREA_OOT_WATER_TEMPLE },
    { RA_SPIRIT_TEMPLE, COMBO_AREA_OOT_SPIRIT_TEMPLE },
    { RA_SHADOW_TEMPLE, COMBO_AREA_OOT_SHADOW_TEMPLE },
    { RA_BOTTOM_OF_THE_WELL, COMBO_AREA_OOT_BOTTOM_OF_THE_WELL },
    { RA_ICE_CAVERN, COMBO_AREA_OOT_ICE_CAVERN },
    { RA_GERUDO_TRAINING_GROUND, COMBO_AREA_OOT_GERUDO_TRAINING_GROUND },
    { RA_GANONS_CASTLE, COMBO_AREA_OOT_GANONS_CASTLE },
};

static_assert(RA_GANONS_CASTLE - RA_KOKIRI_FOREST + 1 == COMBO_AREA_OOT_END - 1,
              "the OoT half mirrors RA_KOKIRI_FOREST..RA_GANONS_CASTLE one to one");

/** RA_NONE, RA_TERMINA and anything unlisted map to no OoT area. */
uint8_t OoTComboAreaOf(RandomizerArea ra) {
    for (const OoTAreaPair& p : kOoTAreaPairs) {
        if (p.ra == ra) {
            return p.area;
        }
    }
    return COMBO_AREA_NONE;
}

uint8_t OoTAreaOfCheck(uint16_t check) {
    if (check == RC_UNKNOWN_CHECK || check >= RC_MAX) {
        return COMBO_AREA_NONE;
    }
    const auto ctx = Rando::Context::GetInstance();
    if (!ctx) {
        return COMBO_AREA_NONE;
    }
    const std::set<RandomizerArea> areas = ctx->GetItemLocation((RandomizerCheck)check)->GetAreas();
    // GetFirstArea() without its empty-set assert: a location with no area has none here.
    return areas.empty() ? COMBO_AREA_NONE : OoTComboAreaOf(*areas.begin());
}

const ComboAreaAdapter kOoTAreaAdapter = { OoTAreaOfCheck };

/** The describer's registrar shape (ComboLogicEngineOoT.cpp): stores a pointer,
 *  calls nothing. soh_rando links WHOLE_ARCHIVE, so it survives. */
struct OoTAreaAdapterRegistrar {
    OoTAreaAdapterRegistrar() {
        Combo_RegisterAreaAdapter((uint8_t)GAME_OOT, &kOoTAreaAdapter);
    }
};
const OoTAreaAdapterRegistrar gOoTAreaAdapterRegistrar;

} // namespace

/**
 * Give every location of the just-loaded world its areas. Called once at the
 * end of SaveManager::LoadRandomizer, after the entrance overrides are loaded:
 * the spoiler loader's own sequence (EntranceShuffler::ParseJson's tail,
 * entrance.cpp), which rebuilds the region graph the check tracker rebuilds at
 * the same load anyway. SetAreas() only adds areas, so a second run on the same
 * world changes nothing.
 */
extern "C" void OoT_HintArea_OnRandomizerLoaded(void) {
    const auto ctx = Rando::Context::GetInstance();
    if (!ctx) {
        return;
    }
    RegionTable_Init();
    ctx->GetEntranceShuffler()->ApplyEntranceOverrides();
    SetAreas();
}

// ============================================================================
// TEST BRIDGES (src/common/tests/test_combo_area.c; RandoHintReload)
// ============================================================================

/** Make OoT's hint text table readable without a ROM: the first step of
 *  Context::InitStaticData, without creating a Rando::Context (OoT's Logic holds
 *  the context it is given, so a context made here would outlive this test and
 *  change what later rows of `--test all` see, e.g. combo-tracker-view). */
extern "C" int OoT_HintArea_TestInitStaticData(void) {
    Rando::StaticData::HintTable_Init();
    return 0;
}

/** The RandomizerArea range the OoT half mirrors. */
extern "C" void OoT_HintArea_TestAreaRange(int* outFirst, int* outLast) {
    *outFirst = (int)RA_KOKIRI_FOREST;
    *outLast = (int)RA_GANONS_CASTLE;
}

/** One RandomizerArea: the adapter's area for it, and OoT's own English clear
 *  text for it (the string OoT's hints print and areaNameToEnum reads back). */
extern "C" void OoT_HintArea_TestArea(int ra, uint8_t* outArea, char* outEnglish, int cap) {
    *outArea = OoTComboAreaOf((RandomizerArea)ra);
    const std::string english =
        Rando::StaticData::hintTextTable[Rando::StaticData::areaNames[(uint32_t)ra]].GetClear().GetEnglish(MF_CLEAN);
    snprintf(outEnglish, (size_t)cap, "%s", english.c_str());
}

/** The adapter's answer for RA_NONE, RA_LINKS_POCKET and RA_TERMINA. */
extern "C" void OoT_HintArea_TestSpecials(uint8_t* outNone, uint8_t* outPocket, uint8_t* outTermina) {
    *outNone = OoTComboAreaOf(RA_NONE);
    *outPocket = OoTComboAreaOf(RA_LINKS_POCKET);
    *outTermina = OoTComboAreaOf(RA_TERMINA);
}

/** #575 H3c (MmHintCrossedArea): give OoT check `rc` of the live heap context
 *  the area `ra`, as SetAreas would, so MM's hints can be locked naming the OoT
 *  area of a check that hosts an MM item without generating a world. Returns 0,
 *  or -1 with no heap context or an out-of-range id. */
extern "C" int OoT_HintArea_TestSetArea(uint16_t rc, int ra) {
    const auto ctx = Rando::Context::GetInstance();
    if (!ctx || rc == RC_UNKNOWN_CHECK || rc >= RC_MAX) {
        return -1;
    }
    ctx->GetItemLocation((RandomizerCheck)rc)->MergeAreas({ (RandomizerArea)ra });
    return 0;
}

namespace {
std::vector<uint8_t> sAreaSnapshot;
}

/**
 * RandoHintReload: record the adapter's area for every OoT location of the live
 * (just generated) world. Returns the number of locations with an area, and
 * names one chest among them; 0 means the lock would be vacuous.
 */
extern "C" int OoT_HintArea_TestSnapshot(void) {
    sAreaSnapshot.assign(RC_MAX, COMBO_AREA_NONE);
    int withArea = 0;
    int sampleChest = -1;
    for (int rc = RC_UNKNOWN_CHECK + 1; rc < RC_MAX; rc++) {
        const ComboArea a = Combo_Area_OfCheck((uint8_t)GAME_OOT, (uint16_t)rc);
        sAreaSnapshot[rc] = a.area;
        if (a.area == COMBO_AREA_NONE) {
            continue;
        }
        withArea++;
        if (sampleChest < 0 && Rando::StaticData::GetLocation((RandomizerCheck)rc)->GetActorID() == ACTOR_EN_BOX) {
            sampleChest = rc;
        }
    }
    if (sampleChest < 0) {
        fprintf(stderr, "[hint-reload] no chest has an area before the reload\n");
        return 0;
    }
    const ComboArea chest = { (uint8_t)GAME_OOT, sAreaSnapshot[sampleChest] };
    fprintf(stderr, "[hint-reload] areas before reload: %d locations; chest %s is in \"%s\"\n", withArea,
            Rando::StaticData::GetLocation((RandomizerCheck)sampleChest)->GetName().c_str(), Combo_Area_Name(chest));
    return withArea;
}

/** After the reload: the number of locations whose adapter area changed. */
extern "C" int OoT_HintArea_TestCompareSnapshot(void) {
    int changed = 0;
    for (int rc = RC_UNKNOWN_CHECK + 1; rc < RC_MAX && (size_t)rc < sAreaSnapshot.size(); rc++) {
        const ComboArea after = Combo_Area_OfCheck((uint8_t)GAME_OOT, (uint16_t)rc);
        if (after.area == sAreaSnapshot[rc]) {
            continue;
        }
        if (changed < 5) {
            const ComboArea before = { (uint8_t)GAME_OOT, sAreaSnapshot[rc] };
            fprintf(stderr, "[hint-reload] FAIL area of %s: before \"%s\", after reload \"%s\"\n",
                    Rando::StaticData::GetLocation((RandomizerCheck)rc)->GetName().c_str(), Combo_Area_Name(before),
                    Combo_Area_Name(after));
        }
        changed++;
    }
    return changed;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
