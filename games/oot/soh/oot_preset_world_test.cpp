/**
 * @file oot_preset_world_test.cpp
 * @brief Operator ruling 2026-11-05 ("Should the Standard and Beginner presets
 *        follow the new OoTMM defaults? yes"): a paired world made from the shipped
 *        Standard or Beginner preset takes every default the D wave moved to OoTMM's
 *        and is proved under All Locations. Rows RandoPresetWorldStandard and
 *        RandoPresetWorldBeginner (CTest label `rando`), dispatch
 *        `rando-preset-world-standard` / `rando-preset-world-beginner` in
 *        src/common/test_runner.cpp, which reads the preset from the checkout
 *        (RandoPresets_ReadShipped, src/common/tests/test_rando_presets.c) and hands
 *        its text here. The file's own shape is the ROM-free row
 *        RandoPresetsFollowDefaults.
 *
 *  S THE SESSION BEFORE: the preset's OLD pins (Shuffle Master Sword and Shuffle
 *    Gerudo Membership Card off, Rainbow Bridge Greg, Ganon's Boss Key LACS-Rewards),
 *    [Both Games] Logic Beatable Only, an MM hint row off and an MM trap type on,
 *    so the apply below is not vacuous. No world is decided yet.
 *  A THE APPLY, as Presets.cpp's applyPreset applies a Rando Settings block under
 *    Ship of Harkinian's default "overwrite" strategy, in memory: gRandoSettings
 *    REPLACES the block (every option key it does not hold reads its default), the
 *    null pairedProfile resets the paired profile through ComboPresetProfile_Apply
 *    (applyPreset's own call), and the options' callbacks run (UpdateAllOptions).
 *    Not Config::SetBlock, which saves the config file (the gate-contamination
 *    class); applyPreset's CVarSave is left out for the same reason.
 *  V EVERY VALUE IS VALID: each gRandoSettings key names a registered option of
 *    Ship of Harkinian's randomizer, with one of that option's values (or is the
 *    excluded-locations list, whose ids are checks; or is in kInertKeys, with why).
 *  D THE D WAVE'S DEFAULTS ARE TAKEN, read from the options: Shuffle Master Sword,
 *    Shuffle Ocarinas and Shuffle Gerudo Membership Card on (PR #1436, #1425);
 *    Rainbow Bridge Medallions with all six, Ganon's Boss Key Start With (PR #1436,
 *    #1428); Time Travel Requires Master Sword on (PR #1446, #1434). [Both Games]
 *    Logic resolves to All Locations (PR #1392) and no Cross-Game rule is chosen; no
 *    MM option, trap type, trick or excluded check is chosen, so MM's hints (PR
 *    #1446), Time Progression (PR #1440) and shop and Tingle prices (PR #1436) read
 *    their new defaults.
 *  W THE WORLD, one paired creation on the file select's production path (the
 *    preset's excluded locations reach the generation through the Generate
 *    button's own parse, see SetGoldenExcludes): created with no half left
 *    unproved, under All Locations and Ganon & Majora; the world holds leg D's OoT
 *    values; every other value the preset holds reaches the world, except
 *    kDecidedByGeneration's (each says why); each excluded location is excluded.
 *  F ONE GAME: once the world is created the paired profile is decided, so the same
 *    preset's block is refused whole (ComboPresetProfile_RefusalNote) and its reset
 *    writes nothing.
 *
 * RED HALVES, derived, not run (this lane had no local build): with the presets as
 * main shipped them, A's reset is skipped (no pairedProfile), D fails on Shuffle
 * Master Sword (both), Shuffle Gerudo Membership Card (Beginner), the bridge and
 * Ganon's Boss Key (both), [Both Games] Logic (left at S's Beatable Only), the MM
 * hint row and the trap type; W fails on the same OoT values and on the rung.
 *
 * Every CVar and environment variable the row touches is put back in memory at the
 * end; nothing here writes a config file.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <libultraship/bridge.h>
#include <nlohmann/json.hpp>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"

#include "combo_mm_locations_view.h" // src/common: MM's excluded checks
#include "combo_mm_options_page.h"   // src/common: Combo_MMOptionsPages_Init
#include "combo_mm_options_view.h"   // src/common: MM's options and trap types
#include "combo_mm_tricks_view.h"    // src/common: MM's tricks
#include "combo_preset_profile.h"    // src/common: the paired half of the Rando Settings block (#1368)
#include "combo_settings_view.h"     // src/common: the Cross-Game rules
#include "context.h"                 // src/common: Context_ClearFrozenState
#include "foreign_items.h"           // src/common: gComboCtx, RSBS_COMBO_GOAL_*, RSBS_COMBO_RUNG_*

extern "C" {
// 3drando/menu.cpp: the headless generation bridge the RandoGen rows use.
int Rando_HeadlessSeedTest(const char* seedStr);
// ItemAdapterSingleExe.cpp: the new file Ship of Harkinian's own init path authors.
int OoT_ItemAdapter_TestAuthorNewFile(void* buf, size_t size, int variant, int* outNewfMarked);
// ForeignItemsSingleExe.cpp: the creation event, then Randomizer_InitSaveFile; the
// #1055 bridge that removes a stale spoiler of this seed first; the halves the last
// creation carried no proof for.
int OoT_Creation_AuthorRandoFile(int slot);
int OoT_Creation_TestClearPairedSpoiler(void);
uint32_t OoT_Creation_TestLastUnprovedHalves(void);
}

static_assert(RO_BRIDGE_SHIPPED_DEFAULT == RO_BRIDGE_MEDALLIONS,
              "#1428: the shipped Rainbow Bridge is OoTMM's 'medallions', which leg D reads");
static_assert(RO_GANON_BOSS_KEY_SHIPPED_DEFAULT == RO_GANON_BOSS_KEY_STARTWITH,
              "#1428: the shipped Ganon's Boss Key is Start With, which leg D reads");

namespace {

int sFailures = 0;
const char* sPreset = "";

void PwFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] FAIL: rando-preset-world (%s): ", sPreset);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define PW_CHECK(cond, ...)      \
    do {                         \
        if (!(cond)) {           \
            PwFail(__VA_ARGS__); \
        }                        \
    } while (0)

constexpr const char* kRandoPrefix = "gRandoSettings.";
constexpr const char* kExcludedLocationsKey = "ExcludedLocations";
// The menu's list keys beside the options (Rando_ReadMenuExcludesAndTricks reads the first two).
const char* const kListKeys[] = { "ExcludedLocations", "EnabledTricks", "EnabledGlitches" };
// The environment the headless bridge reads a golden profile's sets from (#982).
constexpr const char* kGoldenExcludesEnv = "RSBS_GOLDEN_EXCLUDES";

struct PwKeyReason {
    const char* key;
    const char* why;
};

/** gRandoSettings keys a shipped preset holds that no option reads (leg V). */
const PwKeyReason kInertKeys[] = {
    { "CuccosToReturn", "Ship of Harkinian's own preset key: Cuccos to Return is an Enhancements row here "
                        "(gEnhancements.CuccosToReturn), so no randomizer option reads it" },
};

/** Preset values the paired generation decides instead (leg W). */
const PwKeyReason kDecidedByGeneration[] = {
    { "CompleteMaskQuest", "#973: a paired world is generated with Mask Quest on Shuffle, because the Happy Mask "
                           "Shop door leads to Termina (3drando/playthrough.cpp); the row's callback clears the key" },
    { "MalonHint", "Ship of Harkinian's FinalizeSettings turns Malon's hint off while cows are not shuffled, and "
                   "neither preset shuffles them" },
};

const char* ReasonFor(const PwKeyReason* table, size_t count, const std::string& key) {
    for (size_t i = 0; i < count; i++) {
        if (key == table[i].key) {
            return table[i].why;
        }
    }
    return nullptr;
}

/** The OoT randomizer option whose CVar is @p cvar, or -1. */
int OptionForCVar(const std::string& cvar) {
    auto settings = Rando::Settings::GetInstance();
    for (int i = 0; i < RSK_MAX; i++) {
        if (settings->GetOption(static_cast<RandomizerSettingKey>(i)).GetCVarName() == cvar) {
            return i;
        }
    }
    return -1;
}

/** The ids of a comma-separated check list ("147,148,"). False on a token that is not a number. */
bool ParseCheckIds(const std::string& list, std::vector<long>* ids) {
    size_t start = 0;
    while (start < list.size()) {
        size_t comma = list.find(',', start);
        if (comma == std::string::npos) {
            comma = list.size();
        }
        const std::string token = list.substr(start, comma - start);
        if (!token.empty()) {
            char* end = nullptr;
            const long id = std::strtol(token.c_str(), &end, 10);
            if (end == token.c_str() || *end != '\0') {
                return false;
            }
            ids->push_back(id);
        }
        start = comma + 1;
    }
    return true;
}

// ---- Putting the session back ---------------------------------------------------

struct PwSavedCVar {
    std::string name;
    bool existed = false;
    Ship::ConsoleVariableType type = Ship::ConsoleVariableType::Integer;
    int32_t integer = 0;
    float real = 0.0f;
    std::string text;
    Color_RGBA8 color = {};
    Color_RGB8 color24 = {};
};

PwSavedCVar SaveCVar(const std::string& name) {
    PwSavedCVar saved;
    saved.name = name;
    const std::shared_ptr<Ship::CVar> cvar = CVarGet(name.c_str());
    if (cvar == nullptr) {
        return saved;
    }
    saved.existed = true;
    saved.type = cvar->Type;
    switch (cvar->Type) {
        case Ship::ConsoleVariableType::Integer:
            saved.integer = cvar->Integer;
            break;
        case Ship::ConsoleVariableType::Float:
            saved.real = cvar->Float;
            break;
        case Ship::ConsoleVariableType::String:
            saved.text = cvar->String != nullptr ? cvar->String : "";
            break;
        case Ship::ConsoleVariableType::Color:
            saved.color = cvar->Color;
            break;
        case Ship::ConsoleVariableType::Color24:
            saved.color24 = cvar->Color24;
            break;
    }
    return saved;
}

void RestoreCVar(const PwSavedCVar& saved) {
    if (!saved.existed) {
        CVarClear(saved.name.c_str());
        return;
    }
    switch (saved.type) {
        case Ship::ConsoleVariableType::Integer:
            CVarSetInteger(saved.name.c_str(), saved.integer);
            break;
        case Ship::ConsoleVariableType::Float:
            CVarSetFloat(saved.name.c_str(), saved.real);
            break;
        case Ship::ConsoleVariableType::String:
            CVarSetString(saved.name.c_str(), saved.text.c_str());
            break;
        case Ship::ConsoleVariableType::Color:
            CVarSetColor(saved.name.c_str(), saved.color);
            break;
        case Ship::ConsoleVariableType::Color24:
            CVarSetColor24(saved.name.c_str(), saved.color24);
            break;
    }
}

/**
 * Every key the row may write: each OoT randomizer option's CVar and the menu's
 * list keys (what SetBlock would replace), every key the preset holds, and every
 * key of the paired profile (what ComboPresetProfile_Apply writes).
 */
std::vector<std::string> RowKeys(const nlohmann::json& randoSettings) {
    std::set<std::string> keys;
    auto settings = Rando::Settings::GetInstance();
    for (int i = 0; i < RSK_MAX; i++) {
        const std::string& cvar = settings->GetOption(static_cast<RandomizerSettingKey>(i)).GetCVarName();
        if (!cvar.empty()) {
            keys.insert(cvar);
        }
    }
    for (const char* list : kListKeys) {
        keys.insert(std::string(kRandoPrefix) + list);
    }
    for (const auto& item : randoSettings.items()) {
        keys.insert(std::string(kRandoPrefix) + item.key());
    }
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        if (const ComboMMOptionDesc* desc = Combo_MMOptionAt(i); desc != nullptr) {
            keys.insert(desc->cvar);
        }
    }
    for (int i = 0; i < Combo_MMTrapTypeCount(); i++) {
        if (const ComboMMTrapTypeDesc* desc = Combo_MMTrapTypeAt(i); desc != nullptr) {
            keys.insert(desc->cvar);
        }
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        if (const ComboMMTrickDesc* desc = Combo_MMTrickAt(i); desc != nullptr) {
            keys.insert(desc->cvar);
        }
    }
    keys.insert(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    for (int id = 0; id < (int)COMBO_SETTING_COUNT; id++) {
        keys.insert(Combo_ComboSettingKey((ComboSettingId)id));
    }
    return std::vector<std::string>(keys.begin(), keys.end());
}

/** An environment variable as the row found it: whether it was set, and to what. */
struct PwSavedEnv {
    bool existed = false;
    std::string value;
};

PwSavedEnv SaveEnv(const char* name) {
    PwSavedEnv saved;
    if (const char* value = std::getenv(name); value != nullptr) {
        saved.existed = true;
        saved.value = value;
    }
    return saved;
}

void SetEnv(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value != nullptr ? value : ""); // "" removes it on Windows
#else
    if (value != nullptr) {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
#endif
}

const ComboMMOptionDesc* MMOptionByCVar(const char* cvar) {
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
        if (desc != nullptr && std::strcmp(desc->cvar, cvar) == 0) {
            return desc;
        }
    }
    return nullptr;
}

// ---- S: the session before -------------------------------------------------------

constexpr const char* kMMGossipHints = "gRando.Options.RO_HINTS_GOSSIP_STONES";

bool ScrambleSession() {
    // The old pins of the two presets, as a session that applied one before this change holds them.
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("ShuffleMasterSword"), RO_GENERIC_OFF);
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("ShuffleGerudoToken"), RO_GENERIC_OFF);
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("RainbowBridge"), RO_BRIDGE_GREG);
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("ShuffleGanonBossKey"), RO_GANON_BOSS_KEY_LACS_REWARDS);
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("LacsRewardCount"), 6);
    // The paired profile: [Both Games] Logic Beatable Only, an MM hint row off, an MM trap type on.
    const bool rung = Combo_ComboSettingSet(COMBO_SETTING_LOGIC, (int32_t)RSBS_COMBO_RUNG_BEATABLE) == 1;
    const ComboMMOptionDesc* hints = MMOptionByCVar(kMMGossipHints);
    if (hints != nullptr) {
        Combo_MMOptionSetValue(hints, RO_GENERIC_OFF);
    }
    const ComboMMTrapTypeDesc* trap = Combo_MMTrapTypeCount() > 0 ? Combo_MMTrapTypeAt(0) : nullptr;
    const bool trapSet = trap != nullptr && Combo_MMTrapTypeSetValue(trap, true);
    const bool hintsOff = hints != nullptr && Combo_MMOptionIsExplicit(hints) && Combo_MMOptionGetValue(hints) == 0;
    printf("[TEST] S (%s): old pins stored; [Both Games] Logic Beatable Only %d, MM gossip-stone hints off %d, MM "
           "trap type '%s' on %d\n",
           sPreset, rung ? 1 : 0, hintsOff ? 1 : 0, trap != nullptr ? trap->cvar : "(none)", trapSet ? 1 : 0);
    PW_CHECK(rung && hintsOff && trapSet,
             "S: the session could not be scrambled, so leg A's reset would prove nothing");
    return rung && hintsOff && trapSet;
}

// ---- A: the apply ------------------------------------------------------------------

bool ApplyPresetBlock(const nlohmann::json& rando, const std::vector<std::string>& keys) {
    const nlohmann::json& block = rando.at("gRandoSettings");
    // Config::SetBlock("CVars.gRandoSettings", block): the block is replaced whole.
    for (const std::string& key : keys) {
        if (key.rfind(kRandoPrefix, 0) == 0) {
            CVarClear(key.c_str());
        }
    }
    for (const auto& item : block.items()) {
        const std::string cvar = std::string(kRandoPrefix) + item.key();
        if (item.value().is_number_integer()) {
            CVarSetInteger(cvar.c_str(), item.value().get<int32_t>());
        } else if (item.value().is_string()) {
            CVarSetString(cvar.c_str(), item.value().get<std::string>().c_str());
        } else {
            PwFail("A: %s holds a %s, which no randomizer setting reads", cvar.c_str(), item.value().type_name());
        }
    }
    // applyPreset's own call for the paired half, under the same strategy.
    const ComboPresetProfileReport report = ComboPresetProfile_Apply(rando.at(COMBO_PRESET_PAIRED_PROFILE_KEY), false);
    printf("[TEST] A (%s): gRandoSettings replaced (%zu keys); paired profile applied %d: %d written, %d cleared, %d "
           "refused, %d ignored\n",
           sPreset, block.size(), report.applied ? 1 : 0, report.written, report.cleared, report.refused,
           report.ignored);
    PW_CHECK(report.applied, "A: the paired profile was refused although no world is decided");
    PW_CHECK(report.written == 0 && report.refused == 0 && report.ignored == 0 && report.cleared > 0,
             "A: the preset's paired profile is not a reset (a null profile clears every key and writes none)");
    Rando::Settings::GetInstance()->UpdateAllOptions();
    return report.applied;
}

// ---- V: every value is valid ---------------------------------------------------------

void CheckValuesValid(const nlohmann::json& block) {
    auto settings = Rando::Settings::GetInstance();
    int options = 0;
    for (const auto& item : block.items()) {
        const std::string& key = item.key();
        const std::string cvar = std::string(kRandoPrefix) + key;
        if (key == kExcludedLocationsKey) {
            std::vector<long> ids;
            const bool parsed = item.value().is_string() && ParseCheckIds(item.value().get<std::string>(), &ids);
            PW_CHECK(parsed && !ids.empty(), "V: %s is not a list of check ids", cvar.c_str());
            for (const long id : ids) {
                PW_CHECK(id > (long)RC_UNKNOWN_CHECK && id < (long)RC_MAX, "V: %s names %ld, which is not a check",
                         cvar.c_str(), id);
            }
            printf("[TEST] V (%s): %s lists %zu check(s)\n", sPreset, cvar.c_str(), ids.size());
            continue;
        }
        if (const char* why = ReasonFor(kInertKeys, sizeof(kInertKeys) / sizeof(kInertKeys[0]), key)) {
            printf("[TEST] V (%s): %s is inert: %s\n", sPreset, cvar.c_str(), why);
            continue;
        }
        const int rsk = OptionForCVar(cvar);
        if (rsk < 0) {
            PwFail("V: %s names no option of Ship of Harkinian's randomizer", cvar.c_str());
            continue;
        }
        if (!item.value().is_number_integer()) {
            PwFail("V: %s holds a %s, not one of its option's values", cvar.c_str(), item.value().type_name());
            continue;
        }
        const int64_t value = item.value().get<int64_t>();
        Rando::Option& option = settings->GetOption(static_cast<RandomizerSettingKey>(rsk));
        PW_CHECK(value >= 0 && (size_t)value < option.GetOptionCount(), "V: %s = %lld is not one of %s's %zu values",
                 cvar.c_str(), (long long)value, option.GetName().c_str(), option.GetOptionCount());
        options++;
    }
    printf("[TEST] V (%s): %d option value(s) checked against their options\n", sPreset, options);
}

// ---- D: the D wave's defaults are taken ---------------------------------------------

struct PwDWaveOption {
    RandomizerSettingKey key;
    int want;
    const char* what;
};

const PwDWaveOption kDWaveOoT[] = {
    { RSK_SHUFFLE_MASTER_SWORD, RO_GENERIC_ON, "Shuffle Master Sword on (PR #1436, #1425)" },
    { RSK_SHUFFLE_OCARINA, RO_GENERIC_ON, "Shuffle Ocarinas on (PR #1436, #1425)" },
    { RSK_SHUFFLE_GERUDO_MEMBERSHIP_CARD, RO_GENERIC_ON, "Shuffle Gerudo Membership Card on (PR #1436, #1425)" },
    { RSK_RAINBOW_BRIDGE, RO_BRIDGE_MEDALLIONS, "Rainbow Bridge Medallions (PR #1436, #1428)" },
    { RSK_RAINBOW_BRIDGE_MEDALLION_COUNT, 6, "the bridge needs all six medallions (PR #1436, #1428)" },
    { RSK_GANONS_BOSS_KEY, RO_GANON_BOSS_KEY_STARTWITH, "Ganon's Boss Key Start With (PR #1436, #1428)" },
    { RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD, RO_GENERIC_ON, "Time Travel Requires Master Sword on (PR #1446, #1434)" },
};

/** MM's rows whose default the D wave moved; every one reads its default after the reset. */
const char* const kDWaveMM[] = {
    "gRando.Options.RO_HINTS_GOSSIP_STONES", "gRando.Options.RO_HINTS_BOSS_REMAINS",
    "gRando.Options.RO_HINTS_HOOKSHOT",      "gRando.Options.RO_HINTS_OATH_TO_ORDER",
    "gRando.Options.RO_HINTS_SPIDER_HOUSES", "gRando.Options.RO_CLOCK_SHUFFLE_PROGRESSIVE",
    "gRando.Options.RO_SHOP_PRICES",         "gRando.Options.RO_TINGLE_PRICES",
};

void CheckDefaultsTaken() {
    auto settings = Rando::Settings::GetInstance();
    for (const PwDWaveOption& d : kDWaveOoT) {
        const int got = (int)settings->GetOption(d.key).GetOptionIndex();
        printf("[TEST] D (%s): %s: reads %d (want %d)\n", sPreset, d.what, got, d.want);
        PW_CHECK(got == d.want, "D: after the apply the option does not read %s", d.what);
    }

    int chosenRules = 0;
    for (int id = 0; id < (int)COMBO_SETTING_COUNT; id++) {
        if (Combo_ComboSettingIsExplicit((ComboSettingId)id)) {
            PwFail("D: the Cross-Game rule '%s' is still chosen after the reset",
                   Combo_ComboSettingKey((ComboSettingId)id));
            chosenRules++;
        }
    }
    const int32_t rung = Combo_ComboSettingResolved(COMBO_SETTING_LOGIC);
    printf("[TEST] D (%s): %d Cross-Game rule(s) chosen; [Both Games] Logic resolves to %d (want %u, All "
           "Locations)\n",
           sPreset, chosenRules, (int)rung, (unsigned)RSBS_COMBO_RUNG_ALL_REACHABLE);
    PW_CHECK(rung == (int32_t)RSBS_COMBO_RUNG_ALL_REACHABLE,
             "D: [Both Games] Logic does not resolve to All Locations (PR #1392, #1367)");

    int chosenMM = 0;
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
        if (desc != nullptr && Combo_MMOptionIsExplicit(desc)) {
            PwFail("D: the MM option '%s' is still chosen after the reset", desc->cvar);
            chosenMM++;
        }
    }
    for (int i = 0; i < Combo_MMTrapTypeCount(); i++) {
        const ComboMMTrapTypeDesc* desc = Combo_MMTrapTypeAt(i);
        if (desc != nullptr && Combo_CVarIsExplicitInt(desc->cvar)) {
            PwFail("D: the MM trap type '%s' is still chosen after the reset", desc->cvar);
            chosenMM++;
        }
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
        if (desc != nullptr && Combo_CVarIsExplicitInt(desc->cvar)) {
            PwFail("D: the MM trick '%s' is still chosen after the reset", desc->cvar);
            chosenMM++;
        }
    }
    const int excludedMM = Combo_MMLocationsExcluded(nullptr, 0);
    PW_CHECK(excludedMM == 0, "D: %d MM check(s) still excluded after the reset", excludedMM);
    for (const char* cvar : kDWaveMM) {
        const ComboMMOptionDesc* desc = MMOptionByCVar(cvar);
        if (desc == nullptr) {
            PwFail("D: MM's option table has no row %s", cvar);
            continue;
        }
        printf("[TEST] D (%s): %s reads %d (its default %d)\n", sPreset, cvar, (int)Combo_MMOptionGetValue(desc),
               (int)desc->defaultValue);
        PW_CHECK(!Combo_MMOptionIsExplicit(desc) && Combo_MMOptionGetValue(desc) == desc->defaultValue,
                 "D: %s does not read its default", cvar);
        if (std::strncmp(cvar, "gRando.Options.RO_HINTS_", 24) == 0) {
            PW_CHECK(desc->defaultValue == RO_GENERIC_ON, "D: %s's default is not on (PR #1446, #1439)", cvar);
        }
    }
    printf("[TEST] D (%s): %d MM option(s), trap type(s) or trick(s) chosen, %d MM check(s) excluded\n", sPreset,
           chosenMM, excludedMM);
}

// ---- W: the world ----------------------------------------------------------------------

/** A paired file as the file select creates one (oot_goal_gate_defaults_test.cpp's CreatePairedFile). */
bool CreatePairedFile(const char* seed) {
    const int genRc = Rando_HeadlessSeedTest(seed);
    if (genRc != 0 || Rando::Context::GetInstance() == nullptr) {
        PwFail("W: the headless paired generation failed (rc %d)", genRc);
        return false;
    }
    OoT_Creation_TestClearPairedSpoiler();
    auto newFile = std::make_unique<SaveContext>();
    if (OoT_ItemAdapter_TestAuthorNewFile(newFile.get(), sizeof(SaveContext), 0, nullptr) != 1) {
        PwFail("W: Ship of Harkinian's new-file init could not author a save (missing OoT bring-up)");
        return false;
    }
    memcpy(&gSaveContext, newFile.get(), sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    Context_ClearFrozenState(GAME_MM);
    if (OoT_Creation_AuthorRandoFile(0) != 1) {
        PwFail("W: the production creation event refused the preset's paired world");
        return false;
    }
    if (!Combo_ForeignPairingActive()) {
        PwFail("W: the creation left no paired identity");
        return false;
    }
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    return true;
}

void CheckWorld(const nlohmann::json& block) {
    auto ctx = Rando::Context::GetInstance();
    const uint32_t unproved = OoT_Creation_TestLastUnprovedHalves();
    printf("[TEST] W (%s): goal %u, rung %u, unproved halves 0x%X\n", sPreset, (unsigned)gComboCtx.comboSettings.goal,
           (unsigned)gComboCtx.comboSettings.logicRung, (unsigned)unproved);
    PW_CHECK(gComboCtx.comboSettings.logicRung == (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE,
             "W: the world was not created under All Locations");
    PW_CHECK(gComboCtx.comboSettings.goal == (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH,
             "W: the world's goal is not Ganon & Majora, the shipped goal");
    PW_CHECK(unproved == 0, "W: the creation left a half unproved: the All Locations proof did not accept the world");

    for (const PwDWaveOption& d : kDWaveOoT) {
        const int got = (int)ctx->GetOption(d.key).Get();
        PW_CHECK(got == d.want, "W: the world does not hold %s (it holds %d)", d.what, got);
    }

    int reached = 0;
    for (const auto& item : block.items()) {
        const std::string& key = item.key();
        if (key == kExcludedLocationsKey || !item.value().is_number_integer() ||
            ReasonFor(kInertKeys, sizeof(kInertKeys) / sizeof(kInertKeys[0]), key) != nullptr) {
            continue;
        }
        const int rsk = OptionForCVar(std::string(kRandoPrefix) + key);
        if (rsk < 0) {
            continue; // leg V reported it
        }
        const int want = item.value().get<int>();
        const int got = (int)ctx->GetOption(static_cast<RandomizerSettingKey>(rsk)).Get();
        if (const char* why =
                ReasonFor(kDecidedByGeneration, sizeof(kDecidedByGeneration) / sizeof(kDecidedByGeneration[0]), key)) {
            printf("[TEST] W (%s): %s: preset %d, world %d, decided by the generation: %s\n", sPreset, key.c_str(),
                   want, got, why);
            continue;
        }
        PW_CHECK(got == want, "W: gRandoSettings.%s = %d did not reach the world (it holds %d)", key.c_str(), want,
                 got);
        reached++;
    }
    printf("[TEST] W (%s): %d of the preset's option values compared against the world\n", sPreset, reached);

    if (block.contains(kExcludedLocationsKey) && block.at(kExcludedLocationsKey).is_string()) {
        std::vector<long> ids;
        if (ParseCheckIds(block.at(kExcludedLocationsKey).get<std::string>(), &ids)) {
            for (const long id : ids) {
                if (id <= (long)RC_UNKNOWN_CHECK || id >= (long)RC_MAX) {
                    continue; // leg V reported it
                }
                const bool excluded = ctx->GetItemLocation(static_cast<RandomizerCheck>(id))->IsExcluded();
                printf("[TEST] W (%s): check %ld (%s) excluded %d\n", sPreset, id,
                       Rando::StaticData::GetLocation(static_cast<RandomizerCheck>(id))->GetName().c_str(),
                       excluded ? 1 : 0);
                PW_CHECK(excluded, "W: the preset excludes check %ld, but the world does not", id);
            }
        }
    }
}

// ---- F: one game -------------------------------------------------------------------------

void CheckDecided(const nlohmann::json& rando) {
    const std::string note = ComboPresetProfile_RefusalNote();
    const ComboPresetProfileReport report = ComboPresetProfile_Apply(rando.at(COMBO_PRESET_PAIRED_PROFILE_KEY), false);
    printf("[TEST] F (%s): after the creation the preset is refused %d (\"%s\"); its reset applied %d, cleared %d\n",
           sPreset, note.empty() ? 0 : 1, note.c_str(), report.applied ? 1 : 0, report.cleared);
    PW_CHECK(!note.empty() && !report.applied && report.cleared == 0 && report.written == 0,
             "F: the created world's paired profile is not decided: the same preset could still change it");
}

} // namespace

extern "C" int OoT_PresetWorld_Run(const char* presetName, const char* presetJson, const char* seed) {
    sPreset = presetName != nullptr ? presetName : "?";
    sFailures = 0;
    printf("[TEST] rando-preset-world (%s): a paired world from the shipped preset takes the D wave's OoTMM defaults "
           "and is proved under All Locations (operator ruling 2026-11-05)\n",
           sPreset);

    const nlohmann::json preset = nlohmann::json::parse(presetJson != nullptr ? presetJson : "", nullptr, false);
    if (preset.is_discarded() || !preset.is_object() || !preset.contains("blocks") ||
        !preset["blocks"].contains("rando") || !preset["blocks"]["rando"].contains("gRandoSettings") ||
        !preset["blocks"]["rando"]["gRandoSettings"].is_object() ||
        !preset["blocks"]["rando"].contains(COMBO_PRESET_PAIRED_PROFILE_KEY)) {
        PwFail("the preset has no Rando Settings block with gRandoSettings and %s (RandoPresetsFollowDefaults says "
               "which)",
               COMBO_PRESET_PAIRED_PROFILE_KEY);
        return sFailures;
    }
    const nlohmann::json& rando = preset["blocks"]["rando"];
    const nlohmann::json& block = rando["gRandoSettings"];

    // The tables the keys come from: the options (CreateOptions is idempotent, and what
    // Rando_HeadlessSeedTest calls first) and MM's descriptor tables (EnsureTables'
    // condition in combo_preset_profile.cpp).
    Rando::Settings::GetInstance()->CreateOptions();
    if (Combo_MMOptionCount() == 0 || Combo_MMTrickCount() == 0 || Combo_MMLocationCount() == 0) {
        Combo_MMOptionsPages_Init();
    }
    const std::string refusal = ComboPresetProfile_RefusalNote();
    if (!refusal.empty()) {
        PwFail("a paired world is already decided in this process, so a preset cannot apply: %s", refusal.c_str());
        return sFailures;
    }

    const std::vector<std::string> keys = RowKeys(block);
    std::vector<PwSavedCVar> saved;
    saved.reserve(keys.size());
    for (const std::string& key : keys) {
        saved.push_back(SaveCVar(key));
    }
    const PwSavedEnv savedExcludes = SaveEnv(kGoldenExcludesEnv);

    if (ScrambleSession() && ApplyPresetBlock(rando, keys)) {
        CheckValuesValid(block);
        CheckDefaultsTaken();
        // The headless bridge takes OoT's excluded locations from the environment only
        // (ReadGoldenProfileSets, #982), writes them to the same key and parses them
        // with the Generate button's own code, refusing the run if its visibility
        // filter drops one; without the variable the set is empty.
        const bool hasExcludes = block.contains(kExcludedLocationsKey) && block.at(kExcludedLocationsKey).is_string();
        SetEnv(kGoldenExcludesEnv, hasExcludes ? block.at(kExcludedLocationsKey).get<std::string>().c_str() : nullptr);
        if (CreatePairedFile(seed != nullptr ? seed : "RSBSPRESET")) {
            CheckWorld(block);
            CheckDecided(rando);
        }
    }

    SetEnv(kGoldenExcludesEnv, savedExcludes.existed ? savedExcludes.value.c_str() : nullptr);
    for (const PwSavedCVar& cvar : saved) {
        RestoreCVar(cvar);
    }

    if (sFailures != 0) {
        printf("[TEST] FAIL: rando-preset-world (%s): %d check(s) failed\n", sPreset, sFailures);
    } else {
        printf("[TEST] PASS: rando-preset-world (%s)\n", sPreset);
    }
    return sFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
