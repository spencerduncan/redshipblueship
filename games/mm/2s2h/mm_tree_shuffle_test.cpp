/**
 * ROM-free locks for Majora's Mask's tree shuffle ("Shuffle Tree Drops", #1121;
 * upstream 2Ship's Tree Shuffle, minus the Gorman Track trees it later removed).
 * CTest label "redship". Two rows, two entry points:
 *
 * ============================================================================
 * mm-tree-shuffle (MMTreeShuffle) -- the option, the page row and the pool
 * ============================================================================
 *
 *  (1) The option exists under its upstream key (RO_SHUFFLE_TREE_DROPS, CVar
 *      gRando.Options.RO_SHUFFLE_TREE_DROPS) and defaults OFF.
 *  (2) The MM Randomizer page draws it: a descriptor row labelled "Shuffle Tree
 *      Drops" with the upstream tooltip, a live checkbox under "Shuffle Items",
 *      immediately after "Shuffle Grass Drops" (the shape upstream's menu uses).
 *  (3) The check table holds exactly the 53 trees (upstream's 78 minus the 25
 *      Gorman Track trees "Remove Gorman Track trees from shuffle" took out), all
 *      of one check type that no other check carries, every one listed by some
 *      logic region, none a cross-game host class (an Ocarina of Time item never
 *      lands on a tree, as it never lands on a pot, crate or grass), and every
 *      one a row of Randomizer > MM Locations.
 *  (4) Pool generation: with the option off no tree enters the check pool and
 *      the pool is exactly the pool with it on minus the trees, in the same
 *      order; with it on every tree enters it.
 *
 * Every id is looked up BY NAME (GetOptionIdFromName / GetCheckIdFromName), so
 * the row compiles at a tree that has none of them, and fails there on (1).
 *
 * ============================================================================
 * mm-tree-shuffle-hooks (MMTreeShuffleHooks) -- the provider arms and runs
 * ============================================================================
 *
 * The tree behaviour (Rando/ActorBehavior/ObjTree.cpp) registers from
 * Rando::ActorBehavior::OnFileLoad, the production path a rando save load runs
 * (Rando.cpp OnSaveLoadHandler). This row drives THAT function, never a symbol
 * of ObjTree.cpp itself (a reference from here would pull the object into the
 * link on its own and hide an elided provider, the #516/#1071 class):
 *
 *  - Registry legs: with the option on, OnActorInit for each of the four tree
 *    actors (En_Wood02, Obj_Tree, Obj_Yasi, En_Snowwd) and ShouldActorInit for
 *    En_Scopecoin each gain EXACTLY one registrant over the option-off drive,
 *    and VB_APPLY_BONK_TO_ACTOR / VB_TREE_DROP_COLLECTIBLE gain one each.
 *  - Behaviour leg: VB_TREE_DROP_COLLECTIBLE, dispatched the way z_obj_yasi.c and
 *    z_en_snowwd.c dispatch it (GameInteractor_Should, which GameInteractor.h
 *    binds to MM_GameHooks_ExecuteVBShould), suppresses the vanilla drop of a
 *    tree that carries a tree check, and leaves a tree with no check alone.
 *    Disarmed, the same dispatch keeps the vanilla drop (non-vacuity).
 *  - Guard leg: OnActorInit for a tree with no play state does not crash and
 *    attaches no check.
 *
 * Restores the SaveContext image, the play state and the option-off drive on
 * the way out, so it has no ordering constraints.
 */

#ifndef RSBS_SINGLE_EXECUTABLE
extern "C" int MM_TreeShuffle_RunHeadless(void) {
    return 0;
}
extern "C" int MM_TreeShuffleHooks_RunHeadless(void) {
    return 0;
}
#else

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ObjectExtension/ObjectExtension.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"
#include "2s2h/Rando/Foreign.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"
#include "mm_game_hooks.h"

// src/common: the page's descriptor table and Randomizer > MM Locations.
#include "combo_mm_locations_view.h"
#include "combo_mm_options_view.h"

extern "C" {
#include "variables.h"
#include "functions.h"
void MM_Rando_Init(void);
}

namespace {

int Fail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] FAIL(%d): ", code);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    return code;
}

constexpr const char* kOptionName = "RO_SHUFFLE_TREE_DROPS";
constexpr const char* kOptionCVar = "gRando.Options.RO_SHUFFLE_TREE_DROPS";
constexpr const char* kRowLabel = "Shuffle Tree Drops";
constexpr const char* kRowTooltip =
    "Bonking into each tree that shakes is a check. Trees that don't shake have no drops.";

// Upstream's tree checks after "Remove Gorman Track trees from shuffle".
const char* const kTreeChecks[] = {
    "RC_BENEATH_THE_WELL_TREE",
    "RC_CLOCK_TOWN_NORTH_TREE_01",
    "RC_CUCCO_SHACK_TREE",
    "RC_GORON_RACETRACK_TREE_01",
    "RC_GORON_RACETRACK_TREE_02",
    "RC_GORON_RACETRACK_TREE_03",
    "RC_GORON_RACETRACK_TREE_04",
    "RC_GORON_RACETRACK_TREE_05",
    "RC_GORON_RACETRACK_TREE_06",
    "RC_GORON_RACETRACK_TREE_07",
    "RC_GORON_RACETRACK_TREE_08",
    "RC_GORON_RACETRACK_TREE_09",
    "RC_GORON_RACETRACK_TREE_10",
    "RC_GREAT_BAY_COAST_TREE_01",
    "RC_GREAT_BAY_COAST_TREE_02",
    "RC_GREAT_BAY_COAST_TREE_03",
    "RC_GREAT_BAY_COAST_TREE_04",
    "RC_PATH_TO_MOUNTAIN_VILLAGE_TREE_01",
    "RC_PATH_TO_MOUNTAIN_VILLAGE_TREE_02",
    "RC_PATH_TO_MOUNTAIN_VILLAGE_TREE_03",
    "RC_PATH_TO_MOUNTAIN_VILLAGE_TREE_04",
    "RC_PATH_TO_SNOWHEAD_TREE_01",
    "RC_PATH_TO_SNOWHEAD_TREE_02",
    "RC_PATH_TO_SNOWHEAD_TREE_03",
    "RC_PATH_TO_SNOWHEAD_TREE_04",
    "RC_ROAD_TO_SOUTHERN_SWAMP_TREE_01",
    "RC_ROAD_TO_SOUTHERN_SWAMP_TREE_02",
    "RC_ROAD_TO_SOUTHERN_SWAMP_TREE_03",
    "RC_ROAD_TO_SOUTHERN_SWAMP_TREE_04",
    "RC_ROAD_TO_SOUTHERN_SWAMP_TREE_05",
    "RC_ROAD_TO_SOUTHERN_SWAMP_TREE_06",
    "RC_ROMANI_RANCH_TREE_01",
    "RC_ROMANI_RANCH_TREE_02",
    "RC_ROMANI_RANCH_TREE_03",
    "RC_ROMANI_RANCH_TREE_04",
    "RC_ROMANI_RANCH_TREE_05",
    "RC_ROMANI_RANCH_TREE_06",
    "RC_ROMANI_RANCH_TREE_07",
    "RC_TERMINA_FIELD_MAN_IN_THE_TREE_RUPEE_01",
    "RC_TERMINA_FIELD_MAN_IN_THE_TREE_RUPEE_02",
    "RC_TERMINA_FIELD_TREE_01",
    "RC_TERMINA_FIELD_TREE_02",
    "RC_TWIN_ISLANDS_SPRING_TREE_01",
    "RC_TWIN_ISLANDS_SPRING_TREE_02",
    "RC_TWIN_ISLANDS_SPRING_TREE_03",
    "RC_TWIN_ISLANDS_TREE_01",
    "RC_TWIN_ISLANDS_TREE_02",
    "RC_TWIN_ISLANDS_TREE_03",
    "RC_ZORA_CAPE_TREE_01",
    "RC_ZORA_CAPE_TREE_02",
    "RC_ZORA_CAPE_TREE_03",
    "RC_ZORA_CAPE_TREE_04",
    "RC_ZORA_CAPE_TREE_05",
};
constexpr int kTreeCheckCount = (int)(sizeof(kTreeChecks) / sizeof(kTreeChecks[0]));

/** The option's id, or RO_MAX when this tree has no such option. */
RandoOptionId TreeOptionId() {
    return Rando::StaticData::GetOptionIdFromName(kOptionName);
}

/** One pool generation over a fresh save info, every option at its default but
 *  the tree option, which is `treesOn`. Seeded, so the run is deterministic. */
void GenerateWithTrees(RandoOptionId treeOption, bool treesOn, std::vector<RandoCheckId>& checkPool,
                       std::vector<RandoItemId>& itemPool) {
    auto info = std::make_unique<RandoSaveInfo>();
    memset(info.get(), 0, sizeof(RandoSaveInfo));
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        info->randoSaveOptions[randoOptionId] = row.defaultValue;
    }
    info->randoSaveOptions[RO_LOGIC] = RO_LOGIC_GLITCHLESS;
    info->randoSaveOptions[treeOption] = treesOn ? RO_GENERIC_YES : RO_GENERIC_NO;
    checkPool.clear();
    itemPool.clear();
    Ship_Random_Seed(0x7EE5u);
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
}

int RunOptionAndPool() {
    // ---- (1) the option -----------------------------------------------------
    const RandoOptionId treeOption = TreeOptionId();
    if (treeOption >= RO_MAX) {
        return Fail(1, "MM has no %s option: tree shuffle is missing from MM (#1121)", kOptionName);
    }
    const auto& optionRow = Rando::StaticData::Options.at(treeOption);
    if (optionRow.defaultValue != RO_GENERIC_OFF) {
        return Fail(2, "%s defaults to %u, expected off", kOptionName, (unsigned)optionRow.defaultValue);
    }
    if (strcmp(optionRow.cvar, kOptionCVar) != 0) {
        return Fail(3, "%s binds '%s', expected '%s'", kOptionName, optionRow.cvar, kOptionCVar);
    }

    // ---- (2) the page row ---------------------------------------------------
    MM_RandoOptionsUi_Register();
    const ComboMMOptionDesc* row = Combo_MMOptionById((uint16_t)treeOption);
    if (row == NULL) {
        return Fail(4, "the MM Randomizer page has no row for %s", kOptionName);
    }
    if (row->label == NULL || strcmp(row->label, kRowLabel) != 0 || row->tooltip == NULL ||
        strcmp(row->tooltip, kRowTooltip) != 0) {
        return Fail(5, "the %s row reads '%s' / '%s', expected '%s' / '%s'", kOptionName,
                    row->label ? row->label : "(null)", row->tooltip ? row->tooltip : "(null)", kRowLabel, kRowTooltip);
    }
    if (row->widget != (uint8_t)COMBO_MM_WIDGET_CHECKBOX || row->defaultValue != 0 ||
        row->liveness != (uint8_t)COMBO_MM_LIVENESS_LIVE ||
        strcmp(Combo_MMOptionGroupName(row->group), "Shuffle Items") != 0) {
        return Fail(6, "the %s row is not a live, default-off checkbox under Shuffle Items", kOptionName);
    }
    int grassAt = -1;
    int treeAt = -1;
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
        if (desc->id == (uint16_t)RO_SHUFFLE_GRASS_DROPS) {
            grassAt = i;
        } else if (desc->id == (uint16_t)treeOption) {
            treeAt = i;
        }
    }
    if (grassAt < 0 || treeAt != grassAt + 1) {
        return Fail(7,
                    "Shuffle Tree Drops is row %d, Shuffle Grass Drops row %d: the tree row belongs right after "
                    "the grass row",
                    treeAt, grassAt);
    }

    // ---- (3) the check table ------------------------------------------------
    std::set<RandoCheckId> trees;
    for (const char* name : kTreeChecks) {
        const RandoCheckId id = Rando::StaticData::GetCheckIdFromName(name);
        if (id == RC_UNKNOWN) {
            return Fail(8, "MM's check table has no %s", name);
        }
        trees.insert(id);
    }
    const RandoCheckType treeType = Rando::StaticData::Checks.at(*trees.begin()).randoCheckType;
    for (const auto& [randoCheckId, check] : Rando::StaticData::Checks) {
        const bool isTree = trees.count(randoCheckId) != 0;
        if (isTree != (check.randoCheckType == treeType)) {
            return Fail(9, "%s %s the tree check type: the tree type must hold exactly the %d trees", check.name,
                        isTree ? "lacks" : "carries", kTreeCheckCount);
        }
    }
    MM_Rando_Init(); // fills Rando::Logic::Regions (irreversible; ShipInit::InitAll)
    std::set<RandoCheckId> listed;
    for (const auto& [regionId, region] : Rando::Logic::Regions) {
        for (const auto& [checkId, condition] : region.checks) {
            listed.insert(checkId);
        }
    }
    for (RandoCheckId id : trees) {
        if (listed.count(id) == 0) {
            return Fail(10, "%s is in no logic region, so no seed can ever place on it",
                        Rando::StaticData::Checks.at(id).name);
        }
        if (Rando::Foreign::IsForeignHostClass(id)) {
            return Fail(11, "%s is a cross-game host class: an Ocarina of Time item would wait on a tree drop",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    MM_RandoLocationsUi_Register();
    std::set<RandoCheckId> located;
    for (int i = 0; i < Combo_MMLocationCount(); i++) {
        located.insert((RandoCheckId)Combo_MMLocationAt(i)->id);
    }
    for (RandoCheckId id : trees) {
        if (located.count(id) == 0) {
            return Fail(12, "%s is not a row of Randomizer > MM Locations", Rando::StaticData::Checks.at(id).name);
        }
    }

    // ---- (4) pool generation ------------------------------------------------
    const std::string savedExcluded = CVarGetString("gRando.ExcludedChecks", "");
    CVarClear("gRando.ExcludedChecks");
    std::vector<RandoCheckId> offChecks, onChecks;
    std::vector<RandoItemId> offItems, onItems;
    GenerateWithTrees(treeOption, false, offChecks, offItems);
    GenerateWithTrees(treeOption, true, onChecks, onItems);
    if (!savedExcluded.empty()) {
        CVarSetString("gRando.ExcludedChecks", savedExcluded.c_str());
    }
    for (RandoCheckId id : offChecks) {
        if (trees.count(id) != 0) {
            return Fail(13, "with Shuffle Tree Drops off, %s entered the check pool",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    for (RandoCheckId id : trees) {
        if (std::find(onChecks.begin(), onChecks.end(), id) == onChecks.end()) {
            return Fail(14, "with Shuffle Tree Drops on, %s did not enter the check pool",
                        Rando::StaticData::Checks.at(id).name);
        }
    }
    std::vector<RandoCheckId> onWithoutTrees;
    for (RandoCheckId id : onChecks) {
        if (trees.count(id) == 0) {
            onWithoutTrees.push_back(id);
        }
    }
    if (onWithoutTrees != offChecks) {
        return Fail(15,
                    "the option-off check pool (%zu) is not the option-on pool minus the trees (%zu): the option "
                    "moves checks it does not own",
                    offChecks.size(), onWithoutTrees.size());
    }
    if (onItems.size() != offItems.size() + trees.size()) {
        return Fail(16, "the option-on item pool holds %zu items, expected %zu (one per tree)", onItems.size(),
                    offItems.size() + trees.size());
    }
    printf("[TEST] mm-tree-shuffle: %d trees, option off -> %zu checks, option on -> %zu checks\n", kTreeCheckCount,
           offChecks.size(), onChecks.size());
    return 0;
}

// ---------------------------------------------------------------------------
// The provider row.
// ---------------------------------------------------------------------------

// Settled counts: S2H::GameHooks unregistration is deferred until the next
// Execute of that hook type, and the COND_* macros unregister-then-register on
// every drive, so the raw maps can credit a hook already slated for removal.
template <typename H> size_t SettledCountForId(int32_t forId) {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    auto& buckets = S2H::GameHooks::Registry<H>::functionsForID;
    auto it = buckets.find(forId);
    return it == buckets.end() ? 0u : it->second.size();
}

struct RegistryProbe {
    const char* what;
    size_t (*count)();
    size_t before;
};

size_t WoodCount() {
    return SettledCountForId<GameInteractor::OnActorInit>(ACTOR_EN_WOOD02);
}
size_t ObjTreeCount() {
    return SettledCountForId<GameInteractor::OnActorInit>(ACTOR_OBJ_TREE);
}
size_t YasiCount() {
    return SettledCountForId<GameInteractor::OnActorInit>(ACTOR_OBJ_YASI);
}
size_t SnowwdCount() {
    return SettledCountForId<GameInteractor::OnActorInit>(ACTOR_EN_SNOWWD);
}
size_t ScopecoinCount() {
    return SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EN_SCOPECOIN);
}
size_t BonkCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_APPLY_BONK_TO_ACTOR);
}
size_t TreeDropCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_TREE_DROP_COLLECTIBLE);
}

/** The production arm path: a rando save with the option at `treesOn`, then the
 *  behaviour registrars a save load runs (Rando.cpp OnSaveLoadHandler). */
void DriveTreeArmState(RandoOptionId treeOption, bool rando, bool treesOn) {
    gSaveContext.save.shipSaveInfo.saveType = rando ? SAVETYPE_RANDO : SAVETYPE_VANILLA;
    if (treeOption < RO_MAX) {
        RANDO_SAVE_OPTIONS[treeOption] = treesOn ? RO_GENERIC_ON : RO_GENERIC_OFF;
    }
    Rando::ActorBehavior::OnFileLoad();
}

bool TreeDropVerdict(Actor* tree) {
    return GameInteractor_Should(VB_TREE_DROP_COLLECTIBLE, true, tree);
}

int RunHookLegs(RandoOptionId treeOption, PlayState* play, Actor* tree, Actor* bareTree) {
    RegistryProbe probes[] = {
        { "OnActorInit[ACTOR_EN_WOOD02]", WoodCount, 0 },
        { "OnActorInit[ACTOR_OBJ_TREE]", ObjTreeCount, 0 },
        { "OnActorInit[ACTOR_OBJ_YASI]", YasiCount, 0 },
        { "OnActorInit[ACTOR_EN_SNOWWD]", SnowwdCount, 0 },
        { "ShouldActorInit[ACTOR_EN_SCOPECOIN]", ScopecoinCount, 0 },
        { "ShouldVanillaBehavior[VB_APPLY_BONK_TO_ACTOR]", BonkCount, 0 },
        { "ShouldVanillaBehavior[VB_TREE_DROP_COLLECTIBLE]", TreeDropCount, 0 },
    };

    if (treeOption >= RO_MAX) {
        return Fail(20, "MM has no %s option, so no tree behaviour can arm (#1121)", kOptionName);
    }
    const RandoCheckId treeCheck = Rando::StaticData::GetCheckIdFromName("RC_ROMANI_RANCH_TREE_01");
    if (treeCheck == RC_UNKNOWN) {
        return Fail(20, "MM's check table has no RC_ROMANI_RANCH_TREE_01 (#1121)");
    }
    Rando::ActorBehavior::SetObjectRandoCheckId(tree, treeCheck);

    // ---- Leg 1: rando save, option off (non-vacuity) -----------------------
    MM_gPlayState = play;
    DriveTreeArmState(treeOption, true, false);
    for (auto& probe : probes) {
        probe.before = probe.count();
    }
    if (!TreeDropVerdict(tree)) {
        return Fail(21, "with Shuffle Tree Drops off a tree's vanilla drop is already suppressed: the armed probe "
                        "below would be vacuous");
    }

    // ---- Leg 2: option on, through the production path ---------------------
    DriveTreeArmState(treeOption, true, true);
    for (const auto& probe : probes) {
        const size_t after = probe.count();
        if (after != probe.before + 1) {
            return Fail(22,
                        "%s went %zu -> %zu when Shuffle Tree Drops armed, expected exactly +1: the tree behaviour "
                        "(Rando/ActorBehavior/ObjTree.cpp) is not in the link, is not reached from "
                        "ActorBehavior::OnFileLoad, or a second registrant made the probe unattributable",
                        probe.what, probe.before, after);
        }
    }
    if (TreeDropVerdict(tree)) {
        return Fail(23, "with Shuffle Tree Drops on, a tree carrying a tree check still drops its vanilla "
                        "collectible (VB_TREE_DROP_COLLECTIBLE)");
    }
    if (!TreeDropVerdict(bareTree)) {
        return Fail(24, "with Shuffle Tree Drops on, a tree with no check lost its vanilla drop");
    }

    // ---- Leg 3: no play state ----------------------------------------------
    // Only the ID-keyed OnActorInit registrants for Obj_Tree run: the unkeyed
    // ones belong to other behaviours and are not this row's to exercise.
    MM_gPlayState = nullptr;
    S2H::GameHooks::FlushPendingUnregistrations<GameInteractor::OnActorInit>();
    const auto bucket = S2H::GameHooks::Registry<GameInteractor::OnActorInit>::functionsForID.find(ACTOR_OBJ_TREE);
    if (bucket != S2H::GameHooks::Registry<GameInteractor::OnActorInit>::functionsForID.end()) {
        const auto hooks = bucket->second;
        for (const auto& [hookId, hook] : hooks) {
            hook(bareTree);
        }
    }
    if (Rando::ActorBehavior::GetObjectRandoCheckId(bareTree) != RC_UNKNOWN) {
        return Fail(25, "a tree initialised with no play state was given a check");
    }
    return 0;
}

} // namespace

extern "C" int MM_TreeShuffle_RunHeadless(void) {
    printf("[TEST] mm-tree-shuffle: Shuffle Tree Drops is an MM option, a page row, 53 checks and a pool switch "
           "(#1121)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return Fail(1, "Ship::Context or its ConsoleVariables are missing; run the shared bring-up first");
    }
    const int rc = RunOptionAndPool();
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-tree-shuffle: PASS\n");
    return 0;
}

extern "C" int MM_TreeShuffleHooks_RunHeadless(void) {
    printf("[TEST] mm-tree-shuffle-hooks: the tree behaviour arms from ActorBehavior::OnFileLoad and runs (#1121)\n");
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return Fail(1, "Ship::Context or its ConsoleVariables are missing; run the shared bring-up first");
    }

    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;

    PlayState* play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    Actor* tree = static_cast<Actor*>(calloc(1, sizeof(Actor)));
    Actor* bareTree = static_cast<Actor*>(calloc(1, sizeof(Actor)));
    if (play == nullptr || tree == nullptr || bareTree == nullptr) {
        free(play);
        free(tree);
        free(bareTree);
        return Fail(1, "could not allocate the fixtures");
    }
    play->sceneId = SCENE_F01;
    tree->id = ACTOR_OBJ_YASI;
    bareTree->id = ACTOR_OBJ_TREE;

    const RandoOptionId treeOption = TreeOptionId();
    const int rc = RunHookLegs(treeOption, play, tree, bareTree);

    // ---- Cleanup (pass and fail) -------------------------------------------
    ObjectExtension::GetInstance().Free(tree);
    ObjectExtension::GetInstance().Free(bareTree);
    MM_gPlayState = savedPlay;
    memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));
    Rando::ActorBehavior::OnFileLoad();
    free(play);
    free(tree);
    free(bareTree);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-tree-shuffle-hooks: PASS (seven single-registrant hooks armed, the vanilla drop suppressed "
           "for a tree check only, no play state tolerated)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
