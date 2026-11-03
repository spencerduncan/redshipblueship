/**
 * @file oot_goal_gate_defaults_test.cpp
 * @brief #1428 (operator ruling 2026-11-03, "Yes, match OoTMM"): Ocarina of
 *        Time's two goal gates ship at OoTMM's defaults. Row RandoGoalGateDefaults
 *        (CTest label `rando`), dispatch `rando-goal-gate-defaults` in
 *        src/common/test_runner.cpp.
 *
 * OoTMM (packages/core/src/settings/data.ts): rainbowBridge defaults to
 * "medallions" ("Opens when you have all Medallions") and ganonBossKey to
 * "removed" ("Ganon Boss Key is removed, and so is the lock on the door leading
 * to Ganondorf"), which its `cond` hides under the triforce goals. Ship of
 * Harkinian has no lock-less value; "Start With" is the one whose result in play
 * is the same, the boss door opening. The Light Arrow Cutscene follows Ganon's
 * Boss Key in Ship of Harkinian and stays Vanilla, OoTMM's `lacs` default.
 *
 * One paired creation on the shipped defaults (the TrackerOoTPanelList seed and
 * bring-up), then every leg below. Every check runs and reports; the entry point
 * returns the failure count. Every key, option and save field a leg changes is
 * put back (in memory: nothing here writes a config file).
 *
 *   G1 THE SHIPPED DEFAULTS, with every key cleared: Rainbow Bridge Medallions,
 *      its medallion count 6, Ganon's Boss Key Start With.
 *   G2 THE MENU READS THE SAME DEFAULTS with no key stored: the bridge row's
 *      callback shows the medallion count and the reward options and hides the
 *      stone count; Ganon's Boss Key's hides every LACS row; neither stores a
 *      key. The Locations exclusion list offers the Tower Boss Key Chest, and
 *      hides it once a stored Vanilla says the chest holds the key (the control).
 *   G3 THE CREATED WORLD: Ganon & Majora at All Locations (the shipped goal and
 *      rung), created with no half left unproved; the world carries Medallions,
 *      6 and Start With; the Light Arrow Cutscene is Vanilla; logic starts with
 *      Ganon's Boss Key exactly once and no location holds it; the Tower Boss Key
 *      Chest holds a placed item; the live save holds the key, so the door opens.
 *   G4 LOGIC: five medallions do not build the bridge, the sixth does, and under
 *      the old Vanilla value the six alone did not (the gate moved).
 *   G5 THE CHECK TRACKER lists the Tower Boss Key Chest on that file
 *      (showGanonBossKey), so it is counted.
 *   G6 TRIFORCE HUNT STILL DECIDES GANON'S BOSS KEY (OoTMM hides ganonBossKey
 *      under its triforce goals and opens the tower door only at the hunt's win;
 *      SoH greys the row and says the hunt gives the key): with Start With stored,
 *      "Ganon's Boss Key" mode puts the key in neither logic's start nor the
 *      creation's starting items; "Win" credits it in logic only (upstream: the
 *      lair is post-game); with the hunt off Start With grants both (the control).
 *   G7 A STORED WORLD KEEPS ITS OWN GATES: the OoT world a save and a seed package
 *      carry (SaveManager's randomizer section), rewritten to the OLD values
 *      (Vanilla bridge, a medallion count of 4, Vanilla Ganon's Boss Key), loads
 *      them unchanged with every key cleared, and SoH's tracker then hides the
 *      tower chest, as it did for such a world.
 *
 * RED HALVES, derived, not run (this lane had no local build): with main's
 * defaults G1 fails on all three values and G3 on the world's settings, its
 * starting inventory and the live save; with main's bridge-callback fallback
 * (Vanilla) G2's medallion row stays hidden; with main's exclusion-list fallback
 * G2's offer fails; with main's starting_inventory.cpp and savefile.cpp G6's
 * "Ganon's Boss Key" legs fail (the key is credited and granted at the start).
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include <libultraship/bridge.h>
#include <nlohmann/json.hpp>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/randomizer_check_objects.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"
#include "soh/Enhancements/randomizer/3drando/starting_inventory.hpp"

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"

#include "combo_logic.h"      // src/common: RSBS_COMBO_HALF_*
#include "cvar_shared_keys.h" // src/common: RSBS_CVAR_COMBO_RANDO_GOAL, RSBS_CVAR_COMBO_RANDO_LOGIC
#include "foreign_items.h"    // src/common: gComboCtx, RSBS_COMBO_GOAL_*, RSBS_COMBO_RUNG_*

extern "C" {
// 3drando/menu.cpp: the headless generation bridge the RandoGen rows use.
int Rando_HeadlessSeedTest(const char* seedStr);
// ItemAdapterSingleExe.cpp: the new file Ship of Harkinian's own init path
// authors (variant 0: Save_InitFile(false), what OoT_Sram_InitNewSave runs).
int OoT_ItemAdapter_TestAuthorNewFile(void* buf, size_t size, int variant, int* outNewfMarked);
// ForeignItemsSingleExe.cpp: the creation event, then Randomizer_InitSaveFile; the
// #1055 bridge that removes a stale spoiler of this seed first; the halves the
// last creation carried no proof for.
int OoT_Creation_AuthorRandoFile(int slot);
int OoT_Creation_TestClearPairedSpoiler(void);
uint32_t OoT_Creation_TestLastUnprovedHalves(void);
// randomizer_check_tracker.cpp: CheckTrackerLoadGame, as its OnLoadGame hook runs it (#1126).
void RandoTest_CheckTrackerLoadGame(int32_t fileNum);
}

// SaveManager.cpp (#1044): the OoT world a seed package carries, which is SoH's own
// randomizer save section, written and read back by the functions a save uses.
std::string OoT_SeedPackage_SerializeOoTWorld();
bool OoT_SeedPackage_LoadOoTWorld(const std::string& sectionJson);
// savefile.cpp: the creation's starting items (Randomizer_InitSaveFile runs it).
void SetStartingItems();

static_assert(RO_BRIDGE_SHIPPED_DEFAULT == RO_BRIDGE_MEDALLIONS,
              "#1428: the shipped Rainbow Bridge is OoTMM's 'medallions'");
static_assert(RO_GANON_BOSS_KEY_SHIPPED_DEFAULT == RO_GANON_BOSS_KEY_STARTWITH,
              "#1428: the shipped Ganon's Boss Key is Start With, OoTMM's 'removed' in play");

namespace {

int sFailures = 0;

#define GGD_CHECK(cond, msg)                                              \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                  \
        }                                                                 \
    } while (0)

/** A precondition: without it every leg after it would prove nothing. */
#define GGD_REQUIRE(cond, msg)                                            \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                  \
            return false;                                                 \
        }                                                                 \
    } while (0)

const char* const kBridgeCVar = CVAR_RANDOMIZER_SETTING("RainbowBridge");
const char* const kMedallionCountCVar = CVAR_RANDOMIZER_SETTING("MedallionCount");
const char* const kGanonBossKeyCVar = CVAR_RANDOMIZER_SETTING("ShuffleGanonBossKey");
const char* const kTriforceHuntCVar = CVAR_RANDOMIZER_SETTING("TriforceHunt");

/** Every key whose stored value would put this row off the shipped defaults. */
const char* const kRowKeys[] = {
    kBridgeCVar,       kMedallionCountCVar,        kGanonBossKeyCVar,
    kTriforceHuntCVar, RSBS_CVAR_COMBO_RANDO_GOAL, RSBS_CVAR_COMBO_RANDO_LOGIC,
};
constexpr int kRowKeyCount = (int)(sizeof(kRowKeys) / sizeof(kRowKeys[0]));
bool sKeyExisted[kRowKeyCount];
int32_t sKeyValue[kRowKeyCount];

/** The row starts from the shipped defaults whatever the build directory's config holds. */
void ClearRowKeys() {
    for (int i = 0; i < kRowKeyCount; i++) {
        sKeyExisted[i] = CVarExists(kRowKeys[i]);
        sKeyValue[i] = CVarGetInteger(kRowKeys[i], 0);
        CVarClear(kRowKeys[i]);
    }
}

/** Puts back what ClearRowKeys found, in memory only. */
void RestoreRowKeys() {
    for (int i = 0; i < kRowKeyCount; i++) {
        if (sKeyExisted[i]) {
            CVarSetInteger(kRowKeys[i], sKeyValue[i]);
        } else {
            CVarClear(kRowKeys[i]);
        }
    }
}

bool AnyGateKeyStored() {
    return CVarExists(kBridgeCVar) || CVarExists(kMedallionCountCVar) || CVarExists(kGanonBossKeyCVar);
}

int StartingBossKeyCount() {
    int count = 0;
    for (const RandomizerGet rg : StartingInventory) {
        count += rg == RG_GANONS_CASTLE_BOSS_KEY ? 1 : 0;
    }
    return count;
}

bool LiveSaveHoldsGanonsBossKey() {
    return (gSaveContext.inventory.dungeonItems[SCENE_GANONS_TOWER] & 1) != 0;
}

/** SoH's Check Tracker loads for the live save, as its OnLoadGame hook loads it.
 *  The ROM-free tier never registers its tracker-data section, so the load's
 *  saves would land on the base section: 0xFE is a slot SaveSection refuses. */
void LoadCheckTracker() {
    const int32_t fileNum = gSaveContext.fileNum;
    gSaveContext.fileNum = 0xFE;
    RandoTest_CheckTrackerLoadGame(0);
    gSaveContext.fileNum = fileNum;
}

// ---- G3: one paired creation on the shipped defaults -------------------------

/**
 * A paired file as the file select creates one (oot_tracker_reachability_test.cpp's
 * TrrCreatePairedFile): a headless generation, the new file Ship of Harkinian
 * authors, the quest id OoT_Sram_InitSave sets, then the creation event and
 * Randomizer_InitSaveFile in their production order.
 */
bool CreatePairedFile(const char* seed) {
    const int genRc = Rando_HeadlessSeedTest(seed);
    GGD_REQUIRE(genRc == 0 && Rando::Context::GetInstance() != nullptr, "the headless paired generation failed");
    OoT_Creation_TestClearPairedSpoiler();
    auto newFile = std::make_unique<SaveContext>();
    GGD_REQUIRE(OoT_ItemAdapter_TestAuthorNewFile(newFile.get(), sizeof(SaveContext), 0, nullptr) == 1,
                "Ship of Harkinian's new-file init could not author a save (missing OoT bring-up)");
    memcpy(&gSaveContext, newFile.get(), sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    Context_ClearFrozenState(GAME_MM);
    GGD_REQUIRE(OoT_Creation_AuthorRandoFile(0) == 1,
                "the production creation event refused a paired world on the shipped defaults: the All Locations "
                "proof no longer holds with the bridge at all six medallions");
    GGD_REQUIRE(Combo_ForeignPairingActive(), "the creation left no paired identity");
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    return true;
}

void CheckCreatedWorld() {
    auto ctx = Rando::Context::GetInstance();
    const uint32_t unproved = OoT_Creation_TestLastUnprovedHalves();
    const int bridge = (int)ctx->GetOption(RSK_RAINBOW_BRIDGE).Get();
    const int medallions = (int)ctx->GetOption(RSK_RAINBOW_BRIDGE_MEDALLION_COUNT).Get();
    const int bossKey = (int)ctx->GetOption(RSK_GANONS_BOSS_KEY).Get();
    const int lacs = (int)ctx->LACSCondition();
    int keysPlaced = 0;
    for (const RandomizerCheck rc : ctx->allLocations) {
        keysPlaced += ctx->GetItemLocation(rc)->GetPlacedRandomizerGet() == RG_GANONS_CASTLE_BOSS_KEY ? 1 : 0;
    }
    const RandomizerGet atTowerChest = ctx->GetItemLocation(RC_GANONS_TOWER_BOSS_KEY_CHEST)->GetPlacedRandomizerGet();
    printf("[TEST] G3: goal %u rung %u, unproved halves 0x%X; bridge %d, medallion count %d, Ganon's Boss Key %d, "
           "LACS %d; logic starts with the key %d time(s), %d location(s) hold it, the tower chest holds RG %d; the "
           "live save holds the key %d\n",
           (unsigned)gComboCtx.comboSettings.goal, (unsigned)gComboCtx.comboSettings.logicRung, (unsigned)unproved,
           bridge, medallions, bossKey, lacs, StartingBossKeyCount(), keysPlaced, (int)atTowerChest,
           LiveSaveHoldsGanonsBossKey() ? 1 : 0);
    GGD_CHECK(gComboCtx.comboSettings.goal == (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH &&
                  gComboCtx.comboSettings.logicRung == (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE,
              "G3: the creation did not freeze the shipped goal and rung (Ganon & Majora, All Locations), so this "
              "row does not prove the shipped world");
    GGD_CHECK(unproved == 0, "G3: the creation left a half unproved: Ganon's Castle behind all six medallions broke "
                             "the goal proof");
    GGD_CHECK(bridge == RO_BRIDGE_MEDALLIONS, "G3: the world's Rainbow Bridge is not Medallions (OoTMM's default)");
    GGD_CHECK(medallions == 6, "G3: the world's bridge does not need all six medallions");
    GGD_CHECK(bossKey == RO_GANON_BOSS_KEY_STARTWITH, "G3: the world's Ganon's Boss Key is not Start With");
    GGD_CHECK(lacs == RO_LACS_VANILLA, "G3: the Light Arrow Cutscene moved off Vanilla (OoTMM's lacs default)");
    GGD_CHECK(StartingBossKeyCount() == 1, "G3: logic does not start with Ganon's Boss Key exactly once");
    GGD_CHECK(keysPlaced == 0, "G3: a location holds Ganon's Boss Key although the world starts with it");
    GGD_CHECK(atTowerChest != RG_NONE && atTowerChest != RG_GANONS_CASTLE_BOSS_KEY,
              "G3: the Tower Boss Key Chest does not hold a placed item of the world's own");
    GGD_CHECK(LiveSaveHoldsGanonsBossKey(), "G3: the created save does not hold Ganon's Boss Key, so the tower door "
                                            "stays locked");
}

// ---- G1, G2: the options and the menu ------------------------------------------

void CheckShippedDefaults() {
    auto settings = Rando::Settings::GetInstance();
    const int bridge = settings->GetOption(RSK_RAINBOW_BRIDGE).GetOptionIndex();
    const int medallions = settings->GetOption(RSK_RAINBOW_BRIDGE_MEDALLION_COUNT).GetOptionIndex();
    const int bossKey = settings->GetOption(RSK_GANONS_BOSS_KEY).GetOptionIndex();
    printf("[TEST] G1 (no key stored): Rainbow Bridge %d (Medallions %d), medallion count %d, Ganon's Boss Key %d "
           "(Start With %d)\n",
           bridge, (int)RO_BRIDGE_MEDALLIONS, medallions, bossKey, (int)RO_GANON_BOSS_KEY_STARTWITH);
    GGD_CHECK(!AnyGateKeyStored(), "G1: a gate key is stored, so the defaults below are not what is read");
    GGD_CHECK(bridge == RO_BRIDGE_MEDALLIONS, "G1: the shipped Rainbow Bridge is not Medallions (OoTMM's default)");
    GGD_CHECK(medallions == 6, "G1: the shipped medallion count is not six");
    GGD_CHECK(bossKey == RO_GANON_BOSS_KEY_STARTWITH,
              "G1: the shipped Ganon's Boss Key is not Start With (OoTMM's 'removed' in play)");
}

void CheckMenuDefaults() {
    auto settings = Rando::Settings::GetInstance();
    settings->GetOption(RSK_RAINBOW_BRIDGE).RunCallback();
    const bool medallionShown = !settings->GetOption(RSK_RAINBOW_BRIDGE_MEDALLION_COUNT).IsHidden();
    const bool rewardOptionsShown = !settings->GetOption(RSK_BRIDGE_OPTIONS).IsHidden();
    const bool stoneHidden = settings->GetOption(RSK_RAINBOW_BRIDGE_STONE_COUNT).IsHidden();
    settings->GetOption(RSK_GANONS_BOSS_KEY).RunCallback();
    bool lacsHidden = true;
    for (const RandomizerSettingKey key : { RSK_LACS_OPTIONS, RSK_LACS_STONE_COUNT, RSK_LACS_MEDALLION_COUNT,
                                            RSK_LACS_REWARD_COUNT, RSK_LACS_DUNGEON_COUNT, RSK_LACS_TOKEN_COUNT }) {
        lacsHidden = lacsHidden && settings->GetOption(key).IsHidden();
    }
    const bool stored = AnyGateKeyStored();
    printf("[TEST] G2 (no key stored): medallion count shown %d, reward options shown %d, stone count hidden %d; "
           "every LACS row hidden %d; a callback stored a key %d\n",
           medallionShown ? 1 : 0, rewardOptionsShown ? 1 : 0, stoneHidden ? 1 : 0, lacsHidden ? 1 : 0, stored ? 1 : 0);
    GGD_CHECK(medallionShown && rewardOptionsShown && stoneHidden,
              "G2: the Rainbow Bridge row's callback does not read Medallions when no key is stored");
    GGD_CHECK(lacsHidden, "G2: a LACS row shows under the shipped Ganon's Boss Key");
    GGD_CHECK(!stored, "G2: a menu callback stored a randomizer setting the player never set");

    // The Locations exclusion list (RandomizerCheckObjects): the tower chest is a
    // check at the shipped default, and holds the key at a stored Vanilla.
    auto ctx = Rando::Context::GetInstance();
    RandomizerCheckObjects::UpdateImGuiVisibility();
    const bool offered = ctx->GetItemLocation(RC_GANONS_TOWER_BOSS_KEY_CHEST)->IsVisible();
    CVarSetInteger(kGanonBossKeyCVar, RO_GANON_BOSS_KEY_VANILLA);
    RandomizerCheckObjects::UpdateImGuiVisibility();
    const bool offeredAtVanilla = ctx->GetItemLocation(RC_GANONS_TOWER_BOSS_KEY_CHEST)->IsVisible();
    CVarClear(kGanonBossKeyCVar);
    RandomizerCheckObjects::UpdateImGuiVisibility();
    printf("[TEST] G2 (exclusion list): tower chest offered with no key stored %d, with Vanilla stored %d\n",
           offered ? 1 : 0, offeredAtVanilla ? 1 : 0);
    GGD_CHECK(offered, "G2: the Locations exclusion list does not offer the Tower Boss Key Chest under the shipped "
                       "Ganon's Boss Key, although it is a check");
    GGD_CHECK(!offeredAtVanilla, "G2: the exclusion list offers the tower chest at a stored Vanilla, where it holds "
                                 "the key (the control)");
}

// ---- G4: logic ------------------------------------------------------------------

void CheckBridgeLogic() {
    auto ctx = Rando::Context::GetInstance();
    auto gateLogic = ctx->GetLogic();
    // A fresh simulated save (Reset allocates one: Randomizer_InitSaveFile pointed
    // the logic at the live save), five medallions, then the sixth.
    gateLogic->Reset(true);
    const bool savedCalculating = gateLogic->CalculatingAvailableChecks;
    gateLogic->CalculatingAvailableChecks = false; // Item::ApplyEffect grants nothing while it is set
    const RandomizerGet kFive[] = { RG_FOREST_MEDALLION, RG_FIRE_MEDALLION, RG_WATER_MEDALLION, RG_SPIRIT_MEDALLION,
                                    RG_SHADOW_MEDALLION };
    for (const RandomizerGet rg : kFive) {
        Rando::StaticData::RetrieveItem(rg).ApplyEffect();
    }
    const bool five = gateLogic->CanBuildRainbowBridge();
    Rando::StaticData::RetrieveItem(RG_LIGHT_MEDALLION).ApplyEffect();
    const bool six = gateLogic->CanBuildRainbowBridge();
    ctx->GetOption(RSK_RAINBOW_BRIDGE).Set(RO_BRIDGE_VANILLA);
    const bool sixAtVanilla = gateLogic->CanBuildRainbowBridge();
    ctx->GetOption(RSK_RAINBOW_BRIDGE).Set(RO_BRIDGE_MEDALLIONS);
    // Back to the live save, where Randomizer_InitSaveFile left it.
    gateLogic->CalculatingAvailableChecks = savedCalculating;
    gateLogic->SetSaveContext(&gSaveContext);
    printf("[TEST] G4: bridge with five medallions %d, with six %d; six under the old Vanilla value %d\n", five ? 1 : 0,
           six ? 1 : 0, sixAtVanilla ? 1 : 0);
    GGD_CHECK(!five, "G4: five medallions build the bridge; OoTMM's default needs all six");
    GGD_CHECK(six, "G4: all six medallions do not build the bridge");
    GGD_CHECK(!sixAtVanilla, "G4: six medallions without the Light Arrows built the Vanilla bridge, so the leg cannot "
                             "tell the two gates apart");
}

// ---- G5: the Check Tracker -------------------------------------------------------

void CheckTrackerListsTowerChest() {
    LoadCheckTracker();
    const bool shuffled = CheckTracker::IsCheckShuffled(RC_GANONS_TOWER_BOSS_KEY_CHEST);
    const bool listed = CheckTracker::IsVisibleInCheckTracker(RC_GANONS_TOWER_BOSS_KEY_CHEST);
    printf("[TEST] G5: SoH's Check Tracker total %d; the Tower Boss Key Chest shuffled %d, listed %d\n",
           (int)CheckTracker::GetTotalChecks(), shuffled ? 1 : 0, listed ? 1 : 0);
    GGD_CHECK(shuffled && listed, "G5: the Check Tracker does not list the Tower Boss Key Chest, which holds a "
                                  "random item under Start With, so it is not counted");
    CheckTracker::Teardown();
}

// ---- G6: Triforce Hunt decides Ganon's Boss Key ------------------------------------

void CheckHuntOverride() {
    auto ctx = Rando::Context::GetInstance();
    const uint8_t savedHunt = ctx->GetOption(RSK_TRIFORCE_HUNT).Get();
    const uint8_t savedBits = gSaveContext.inventory.dungeonItems[SCENE_GANONS_TOWER];
    GGD_CHECK(ctx->GetOption(RSK_GANONS_BOSS_KEY).Is(RO_GANON_BOSS_KEY_STARTWITH),
              "G6: the world's Ganon's Boss Key is not Start With, so the legs below are vacuous");
    struct Mode {
        uint8_t hunt;
        const char* name;
        int logicKeys; // the starting inventory's copies of the key
        bool granted;  // the creation's starting items hand it out
    };
    const Mode kModes[] = {
        { RO_TRIFORCE_HUNT_OFF, "off", 1, true },
        { RO_TRIFORCE_HUNT_WIN, "Win", 1, false },
        { RO_TRIFORCE_HUNT_GBK, "Ganon's Boss Key", 0, false },
    };
    for (const Mode& mode : kModes) {
        ctx->GetOption(RSK_TRIFORCE_HUNT).Set(mode.hunt);
        GenerateStartingInventory();
        const int logicKeys = StartingBossKeyCount();
        gSaveContext.inventory.dungeonItems[SCENE_GANONS_TOWER] &= (uint8_t)~1u;
        SetStartingItems();
        const bool granted = LiveSaveHoldsGanonsBossKey();
        printf("[TEST] G6: Start With stored, Triforce Hunt %s: logic starts with the key %d time(s) (want %d), the "
               "creation grants it %d (want %d)\n",
               mode.name, logicKeys, mode.logicKeys, granted ? 1 : 0, mode.granted ? 1 : 0);
        if (logicKeys != mode.logicKeys || granted != mode.granted) {
            printf("[TEST] FAIL: G6: under Triforce Hunt %s, Start With was %s (%s:%d)\n", mode.name,
                   mode.hunt == RO_TRIFORCE_HUNT_OFF ? "not honoured (the control)"
                                                     : "not overridden: the hunt must decide Ganon's Boss Key",
                   __FILE__, __LINE__);
            sFailures++;
        }
    }
    ctx->GetOption(RSK_TRIFORCE_HUNT).Set(savedHunt);
    GenerateStartingInventory();
    gSaveContext.inventory.dungeonItems[SCENE_GANONS_TOWER] = savedBits;
}

// ---- G7: a stored world keeps its own gates ------------------------------------------

void CheckStoredWorldRestores(const std::string& createdWorld) {
    nlohmann::json section;
    try {
        section = nlohmann::json::parse(createdWorld);
    } catch (...) {
        GGD_CHECK(false, "G7: the serialized OoT world is not JSON");
        return;
    }
    if (!section.contains("randoSettings") || !section["randoSettings"].is_array() ||
        section["randoSettings"].size() <= (size_t)RSK_GANONS_BOSS_KEY) {
        GGD_CHECK(false, "G7: the serialized OoT world carries no randoSettings array");
        return;
    }
    nlohmann::json& stored = section["randoSettings"];
    const int fileBridge = stored[(size_t)RSK_RAINBOW_BRIDGE].get<int>();
    const int fileBossKey = stored[(size_t)RSK_GANONS_BOSS_KEY].get<int>();
    printf("[TEST] G7: the created world's file carries bridge %d, Ganon's Boss Key %d\n", fileBridge, fileBossKey);
    GGD_CHECK(fileBridge == RO_BRIDGE_MEDALLIONS && fileBossKey == RO_GANON_BOSS_KEY_STARTWITH,
              "G7: the file does not carry the world's gates at their indices, so the rewrite below would edit "
              "something else");

    // A world made before #1428: the old defaults, and a count the new default is not.
    const int kOldCount = 4;
    stored[(size_t)RSK_RAINBOW_BRIDGE] = (int)RO_BRIDGE_VANILLA;
    stored[(size_t)RSK_RAINBOW_BRIDGE_MEDALLION_COUNT] = kOldCount;
    stored[(size_t)RSK_GANONS_BOSS_KEY] = (int)RO_GANON_BOSS_KEY_VANILLA;
    GGD_CHECK(!AnyGateKeyStored(), "G7: a gate key is stored, so a load that read the menu would not read defaults");
    const bool loaded = OoT_SeedPackage_LoadOoTWorld(section.dump());
    auto ctx = Rando::Context::GetInstance();
    GGD_CHECK(loaded && ctx != nullptr, "G7: the rewritten OoT world did not load");
    if (!loaded || ctx == nullptr) {
        return;
    }
    const int bridge = (int)ctx->GetOption(RSK_RAINBOW_BRIDGE).Get();
    const int medallions = (int)ctx->GetOption(RSK_RAINBOW_BRIDGE_MEDALLION_COUNT).Get();
    const int bossKey = (int)ctx->GetOption(RSK_GANONS_BOSS_KEY).Get();
    LoadCheckTracker();
    const bool listed = CheckTracker::IsVisibleInCheckTracker(RC_GANONS_TOWER_BOSS_KEY_CHEST);
    CheckTracker::Teardown();
    printf("[TEST] G7: loaded with no key stored -> bridge %d (want %d), medallion count %d (want %d), Ganon's Boss "
           "Key %d (want %d); the tracker lists the tower chest %d (want 0)\n",
           bridge, (int)RO_BRIDGE_VANILLA, medallions, kOldCount, bossKey, (int)RO_GANON_BOSS_KEY_VANILLA,
           listed ? 1 : 0);
    GGD_CHECK(bridge == RO_BRIDGE_VANILLA && medallions == kOldCount && bossKey == RO_GANON_BOSS_KEY_VANILLA,
              "G7: a stored world's gates did not restore unchanged: a load read the new defaults instead");
    GGD_CHECK(!listed, "G7: the tracker lists the tower chest of a stored Vanilla world, where it holds the key");
}

} // namespace

extern "C" int OoT_GoalGateDefaults_Run(void) {
    printf("[TEST] rando-goal-gate-defaults: OoT's Rainbow Bridge and Ganon's Boss Key ship at OoTMM's defaults "
           "(Medallions, all six; Start With), Triforce Hunt still decides the key, and a stored world keeps its own "
           "gates (#1428)\n");
    sFailures = 0;
    ClearRowKeys();
    if (CreatePairedFile("RSBSSINGLEBAG1")) {
        CheckCreatedWorld();
        CheckShippedDefaults();
        CheckMenuDefaults();
        CheckBridgeLogic();
        CheckTrackerListsTowerChest();
        // The world as a save and a seed package carry it, before G6 touches the context.
        const std::string createdWorld = OoT_SeedPackage_SerializeOoTWorld();
        CheckHuntOverride();
        CheckStoredWorldRestores(createdWorld);
    }
    RestoreRowKeys();
    if (sFailures != 0) {
        printf("[TEST] FAIL: rando-goal-gate-defaults: %d check(s) failed\n", sFailures);
    } else {
        printf("[TEST] PASS: rando-goal-gate-defaults\n");
    }
    return sFailures;
}
