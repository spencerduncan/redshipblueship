/**
 * @file oot_settings_fold_test.cpp
 * #702 lock: the per-area exclude-location option groups reach OoT's settings
 * fingerprint at their shipped sizes, whichever archives are mounted.
 *
 * THE DEFECT. `Playthrough_Init` re-seeds the fill with Hash(seed + settingsStr),
 * where settingsStr folds every Setting-category option of every non-subgroup
 * option group, including the 32 `RSG_EXCLUDES_*` groups (one per check area).
 * Those groups are built in `Settings::CreateOptions()` as COPIES of
 * `mExcludeLocationsOptionsAreas[area]`, which `Context::AddExcludedOptions()`
 * fills from the static location table. With `oot.o2r` mounted the SoH menu is
 * set up (`SohGui::SetupMenuElements`, gated on a game archive), its randomizer
 * page calls `CreateOptions()` BEFORE `InitOTRImpl` reaches
 * `AddExcludedOptions()`, and every exclude group copies an empty vector. Without
 * the archive the menu is skipped, `AddExcludedOptions()` runs first, and the
 * harness's own `CreateOptions()` copies full vectors. Same binary, same seed,
 * same CVars: 638 folded option lines against 3087, a different settingsHash and
 * a different OoT world.
 *
 * WHAT THIS ROW ASSERTS, after a real generation in whatever environment the
 * row runs in (the build directory: ROM-staged in the operator's tree,
 * archive-free on hosted CI):
 *   1. every `RSG_EXCLUDES_*` group holds exactly the exclude options of its area,
 *      counted here INDEPENDENTLY from the static location table (same filter and
 *      the same one-option-per-name rule `AddExcludedOptions` applies), and none
 *      of the 32 counts is zero;
 *   2. the fold that generation ran contributed exactly that many lines per group
 *      (`OoT_Rando_LastSettingsFold`, recorded inside the fold itself), so the
 *      groups being right is not enough: the fingerprint has to have read them.
 *
 * RED HALF, observed: on a binary without the fix, in a ROM-staged build
 * directory, check 1 fails on the first area (Kokiri Forest: the group holds 0
 * of its options) and every group folds 0 lines. On hosted CI this row is green
 * with and without the fix, because CI can never mount a ROM-derived archive; the
 * cross-environment half of the lock is the golden rows running in both
 * environments (CMake/CheckGoldenDigest.cmake, docs/determinism-goldens.md).
 */

#include <cstdint>
#include <cstdio>
#include <set>
#include <string>

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/location.h"

#include "context.h" // src/common — gComboCtx, for the settings fingerprint the fold stamped

// games/oot/soh/Enhancements/randomizer/3drando/menu.cpp: the headless generation
// bridge every rando row uses (creates the real options, then generates).
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
// games/oot/soh/Enhancements/randomizer/3drando/playthrough.cpp: what the last
// settings fold folded.
extern "C" int OoT_Rando_LastSettingsFold(uint32_t* outTotalLines, uint32_t* outExcludeLines, int groupCap);

namespace {

constexpr int kGroups = RSG_EXCLUDES_GANONS_CASTLE - RSG_EXCLUDES_KOKIRI_FOREST + 1;

// The group -> area pairing `Settings::CreateOptions()` writes, one line per group
// in RSG_EXCLUDES_* order. Spelled out rather than derived: the two enums are in
// different orders (RandomizerCheckArea lists overworld areas first).
struct GroupArea {
    RandomizerSettingGroupKey group;
    RandomizerCheckArea area;
    const char* name;
};
constexpr GroupArea kGroupAreas[kGroups] = {
    { RSG_EXCLUDES_KOKIRI_FOREST, RCAREA_KOKIRI_FOREST, "Kokiri Forest" },
    { RSG_EXCLUDES_LOST_WOODS, RCAREA_LOST_WOODS, "Lost Woods" },
    { RSG_EXCLUDES_SACRED_FOREST_MEADOW, RCAREA_SACRED_FOREST_MEADOW, "Sacred Forest Meadow" },
    { RSG_EXCLUDES_DEKU_TREE, RCAREA_DEKU_TREE, "Deku Tree" },
    { RSG_EXCLUDES_FOREST_TEMPLE, RCAREA_FOREST_TEMPLE, "Forest Temple" },
    { RSG_EXCLUDES_KAKARIKO_VILLAGE, RCAREA_KAKARIKO_VILLAGE, "Kakariko Village" },
    { RSG_EXCLUDES_GRAVEYARD, RCAREA_GRAVEYARD, "Graveyard" },
    { RSG_EXCLUDES_BOTTOM_OF_THE_WELL, RCAREA_BOTTOM_OF_THE_WELL, "Bottom of the Well" },
    { RSG_EXCLUDES_SHADOW_TEMPLE, RCAREA_SHADOW_TEMPLE, "Shadow Temple" },
    { RSG_EXCLUDES_DEATH_MOUNTAIN_TRAIL, RCAREA_DEATH_MOUNTAIN_TRAIL, "Death Mountain Trail" },
    { RSG_EXCLUDES_DEATH_MOUNTAIN_CRATER, RCAREA_DEATH_MOUNTAIN_CRATER, "Death Mountain Crater" },
    { RSG_EXCLUDES_GORON_CITY, RCAREA_GORON_CITY, "Goron City" },
    { RSG_EXCLUDES_DODONGOS_CAVERN, RCAREA_DODONGOS_CAVERN, "Dodongo's Cavern" },
    { RSG_EXCLUDES_FIRE_TEMPLE, RCAREA_FIRE_TEMPLE, "Fire Temple" },
    { RSG_EXCLUDES_ZORAS_RIVER, RCAREA_ZORAS_RIVER, "Zora's River" },
    { RSG_EXCLUDES_ZORAS_DOMAIN, RCAREA_ZORAS_DOMAIN, "Zora's Domain" },
    { RSG_EXCLUDES_ZORAS_FOUNTAIN, RCAREA_ZORAS_FOUNTAIN, "Zora's Fountain" },
    { RSG_EXCLUDES_JABU_JABU, RCAREA_JABU_JABUS_BELLY, "Jabu Jabu's Belly" },
    { RSG_EXCLUDES_ICE_CAVERN, RCAREA_ICE_CAVERN, "Ice Cavern" },
    { RSG_EXCLUDES_HYRULE_FIELD, RCAREA_HYRULE_FIELD, "Hyrule Field" },
    { RSG_EXCLUDES_LON_LON_RANCH, RCAREA_LON_LON_RANCH, "Lon Lon Ranch" },
    { RSG_EXCLUDES_LAKE_HYLIA, RCAREA_LAKE_HYLIA, "Lake Hylia" },
    { RSG_EXCLUDES_WATER_TEMPLE, RCAREA_WATER_TEMPLE, "Water Temple" },
    { RSG_EXCLUDES_GERUDO_VALLEY, RCAREA_GERUDO_VALLEY, "Gerudo Valley" },
    { RSG_EXCLUDES_GERUDO_FORTRESS, RCAREA_GERUDO_FORTRESS, "Gerudo Fortress" },
    { RSG_EXCLUDES_HAUNTED_WASTELAND, RCAREA_WASTELAND, "Haunted Wasteland" },
    { RSG_EXCLUDES_DESERT_COLOSSUS, RCAREA_DESERT_COLOSSUS, "Desert Colossus" },
    { RSG_EXCLUDES_GERUDO_TRAINING_GROUND, RCAREA_GERUDO_TRAINING_GROUND, "Gerudo Training Ground" },
    { RSG_EXCLUDES_SPIRIT_TEMPLE, RCAREA_SPIRIT_TEMPLE, "Spirit Temple" },
    { RSG_EXCLUDES_HYRULE_CASTLE, RCAREA_HYRULE_CASTLE, "Hyrule Castle" },
    { RSG_EXCLUDES_MARKET, RCAREA_MARKET, "Market" },
    { RSG_EXCLUDES_GANONS_CASTLE, RCAREA_GANONS_CASTLE, "Ganon's Castle" },
};

// The shipped size of each area's exclude group, from the static location table
// alone: the checks `Context::AddExcludedOptions()` gives an exclude option (it
// skips the five types below, which hold no item), one option per distinct name.
void CountExpected(size_t (&expected)[RCAREA_INVALID]) {
    std::set<std::string> seen[RCAREA_INVALID];
    for (auto& loc : Rando::StaticData::GetLocationTable()) {
        if (loc.GetRandomizerCheck() == RC_UNKNOWN_CHECK || loc.GetRandomizerCheck() == RC_TRIFORCE_COMPLETED ||
            loc.GetRCType() == RCTYPE_CHEST_GAME || loc.GetRCType() == RCTYPE_STATIC_HINT ||
            loc.GetRCType() == RCTYPE_GOSSIP_STONE) {
            continue;
        }
        const RandomizerCheckArea area = loc.GetArea();
        if (area < 0 || area >= RCAREA_INVALID) {
            continue;
        }
        seen[area].insert(loc.GetExcludedOption()->GetName());
    }
    for (int a = 0; a < RCAREA_INVALID; a++) {
        expected[a] = seen[a].size();
    }
}

} // namespace

extern "C" int RandoTest_SettingsFoldExcludes(void) {
    printf("[TEST] rando-settings-fold-excludes: every exclude-location group reaches the settings fingerprint at "
           "its shipped size (#702)\n");

    const int rc = Rando_HeadlessSeedTest("RSBSUNIFIED1");
    if (rc != 0) {
        printf("[TEST] FAIL: headless generation failed rc=%d, so no fold ran to inspect\n", rc);
        return 1;
    }

    uint32_t foldTotal = 0;
    uint32_t foldPerGroup[kGroups] = {};
    const int folds = OoT_Rando_LastSettingsFold(&foldTotal, foldPerGroup, kGroups);
    if (folds <= 0) {
        printf("[TEST] FAIL: the generation reported success but no settings fold was recorded\n");
        return 1;
    }

    size_t expected[RCAREA_INVALID] = {};
    CountExpected(expected);

    auto settings = Rando::Settings::GetInstance();
    int failures = 0;
    size_t expectedTotal = 0;
    size_t groupTotal = 0;
    uint32_t foldExcludeTotal = 0;
    for (int g = 0; g < kGroups; g++) {
        const GroupArea& ga = kGroupAreas[g];
        const size_t want = expected[ga.area];
        const size_t held = settings->GetOptionGroup(ga.group).GetOptions().size();
        const uint32_t folded = foldPerGroup[ga.group - RSG_EXCLUDES_KOKIRI_FOREST];
        expectedTotal += want;
        groupTotal += held;
        foldExcludeTotal += folded;
        if (want == 0) {
            printf("[TEST] FAIL: %s: the static location table gives this area no exclude option at all, so this "
                   "row could not tell an empty group from a correct one\n",
                   ga.name);
            failures++;
            continue;
        }
        if (held != want) {
            printf("[TEST] FAIL: %s: its exclude group holds %zu option(s), the area has %zu — the group was built "
                   "before the area's exclude options existed\n",
                   ga.name, held, want);
            failures++;
        }
        if (folded != want) {
            printf("[TEST] FAIL: %s: the settings fold read %u line(s) from its group, expected %zu\n", ga.name,
                   (unsigned)folded, want);
            failures++;
        }
    }

    printf("[TEST] rando-settings-fold-excludes: areas=%d expected=%zu held=%zu folded=%u foldTotal=%u "
           "settingsHash=%08X\n",
           kGroups, expectedTotal, groupTotal, (unsigned)foldExcludeTotal, (unsigned)foldTotal,
           (unsigned)gComboCtx.sharedRandoSettingsHash);
    if (failures != 0) {
        printf("[TEST] FAIL: %d exclude-group assertion(s) failed\n", failures);
        return 1;
    }
    return 0;
}
