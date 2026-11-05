/**
 * @file test_rando_presets.c
 * @brief Operator ruling 2026-11-05 ("Should the Standard and Beginner presets
 *        follow the new OoTMM defaults? yes"): the shipped "Rando Seed Settings -
 *        Standard" and "Rando Seed Settings - Beginner" presets pin no option whose
 *        default the D wave moved to OoTMM's, and reset the paired profile. Row
 *        RandoPresetsFollowDefaults (ROM-free, default tier), dispatch
 *        `rando-presets-follow-defaults`.
 *
 * Their worlds are the `rando` rows RandoPresetWorldStandard and
 * RandoPresetWorldBeginner (games/oot/soh/oot_preset_world_test.cpp): each applies
 * one preset as Settings > Presets applies it and creates a paired world from it.
 * The runner reads the file for them with RandoPresets_ReadShipped below, because
 * RSBS_SOURCE_DIR is defined for this translation unit only.
 *
 *   J1 THE FILE. Each preset parses, is built in and names itself, and its Rando
 *      Settings block holds a gRandoSettings object beside a NULL pairedProfile.
 *      The null profile is #1368's reset (combo_preset_profile.h): MM's options,
 *      trap types, tricks and excluded checks and the Cross-Game rules go back to
 *      "never chosen". So every default the D wave moved on the MM and [Both Games]
 *      side applies too (MM's hints, PR #1446; Time Progression, PR #1440; MM shop
 *      and Tingle prices, PR #1436; trap types, PR #1432; [Both Games] Logic, PR
 *      #1392), and two players who apply the same preset get one paired profile.
 *   J2 NO D-WAVE PIN. gRandoSettings holds no key in kRandoPresetDWaveKeys: none
 *      whose default a D-wave lane moved, none that tunes a gate mode the new default
 *      does not select, and neither of the two rows [Both Games] Logic decides.
 *      Applying a preset replaces the whole gRandoSettings block (SoH's "overwrite"
 *      strategy), so every absent key reads the shipped default.
 *   J3 THE BEGINNER CHOICES STAY. Beginner still excludes SoH's four hard checks
 *      (the 40 and 50 Gold Skulltula rewards, the Frogs' Ocarina Game, the 1500-point
 *      horseback archery prize) and starts with the Kokiri Sword. No D-wave lane
 *      touched either.
 *
 * RED HALVES, derived, not run (this lane had no local build): on main before this
 * change, J1 fails on both presets (no pairedProfile), and J2 fails on both for
 * RainbowBridge, ShuffleGanonBossKey, LacsRewardCount, ShuffleMasterSword,
 * ShuffleGerudoToken and ShuffleOcarinas.
 *
 * Touches no CVar: it reads the checkout only, so it needs no bring-up.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++).
 */

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include <nlohmann/json.hpp>

#include "../combo_preset_profile.h" // COMBO_PRESET_PAIRED_PROFILE_KEY

/** The two presets the ruling covers. Advanced and Hell Mode keep their own values. */
static const char* const kRandoPresetsFollowingDefaults[] = { "Standard", "Beginner" };

/**
 * The shipped preset "Rando Seed Settings - <name>", read from the checkout into
 * @p out. False, with the reason in @p why, when it cannot be read.
 */
static bool RandoPresets_ReadShipped(const char* name, std::string* out, std::string* why) {
#ifdef RSBS_SOURCE_DIR
    const std::string path =
        std::string(RSBS_SOURCE_DIR) + "/games/oot/assets/custom/presets/Rando Seed Settings - " + name + ".json";
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        *why = "cannot open " + path;
        return false;
    }
    out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
#else
    (void)name;
    (void)out;
    *why = "RSBS_SOURCE_DIR is not defined for this build, so the shipped presets cannot be read";
    return false;
#endif
}

struct RandoPresetDWaveKey {
    const char* key; // under gRandoSettings
    const char* why; // what the preset must follow instead
};

/** Every gRandoSettings key a shipped preset must not pin (J2). */
static const RandoPresetDWaveKey kRandoPresetDWaveKeys[] = {
    // PR #1436 (#1428): Rainbow Bridge defaults to Medallions, all six required.
    { "RainbowBridge", "Rainbow Bridge defaults to Medallions (PR #1436, #1428)" },
    { "MedallionCount", "the bridge needs all six medallions by default (PR #1436, #1428)" },
    { "StoneCount", "a count of a bridge mode the Medallions default does not use (PR #1436, #1428)" },
    { "RewardCount", "a count of a bridge mode the Medallions default does not use (PR #1436, #1428)" },
    { "DungeonCount", "a count of a bridge mode the Medallions default does not use (PR #1436, #1428)" },
    { "TokenCount", "a count of a bridge mode the Medallions default does not use (PR #1436, #1428)" },
    { "BridgeRewardOptions", "an option of a bridge mode the Medallions default does not use (PR #1436, #1428)" },
    // PR #1436 (#1428): Ganon's Boss Key defaults to Start With; its LACS modes are not selected.
    { "ShuffleGanonBossKey", "Ganon's Boss Key defaults to Start With (PR #1436, #1428)" },
    { "LacsStoneCount", "a count of a LACS mode the Start With default does not use (PR #1436, #1428)" },
    { "LacsMedallionCount", "a count of a LACS mode the Start With default does not use (PR #1436, #1428)" },
    { "LacsRewardCount", "a count of a LACS mode the Start With default does not use (PR #1436, #1428)" },
    { "LacsDungeonCount", "a count of a LACS mode the Start With default does not use (PR #1436, #1428)" },
    { "LacsTokenCount", "a count of a LACS mode the Start With default does not use (PR #1436, #1428)" },
    { "LacsRewardOptions", "an option of a LACS mode the Start With default does not use (PR #1436, #1428)" },
    // PR #1436 (#1425): OoTMM's three OoT shuffles, on by default.
    { "ShuffleMasterSword", "Shuffle Master Sword is on by default (PR #1436, #1425)" },
    { "ShuffleOcarinas", "Shuffle Ocarinas is on by default (PR #1436, #1425)" },
    { "ShuffleGerudoToken", "Shuffle Gerudo Membership Card is on by default (PR #1436, #1425)" },
    // PR #1446 (#1434): OoTMM's timeTravelSword, on by default.
    { "TimeTravelRequiresMasterSword", "Time Travel Requires Master Sword is on by default (PR #1446, #1434)" },
    // PR #1392 (#1367): [Both Games] Logic (All Locations by default) decides both rows at generation.
    { "LogicRules", "[Both Games] Logic decides it, All Locations by default (PR #1392, #1367)" },
    { "AllLocationsReachable", "[Both Games] Logic decides it, All Locations by default (PR #1392, #1367)" },
};

/** One preset's J1 and J2; the parsed file in @p preset. Returns the failure count. */
static int RandoPresets_CheckFollowsDefaults(const char* name, nlohmann::json* preset) {
    int failures = 0;
    std::string text;
    std::string why;
    if (!RandoPresets_ReadShipped(name, &text, &why)) {
        printf("[TEST] FAIL: J1 (%s): %s\n", name, why.c_str());
        return 1;
    }
    *preset = nlohmann::json::parse(text, nullptr, false);
    if (preset->is_discarded() || !preset->is_object()) {
        printf("[TEST] FAIL: J1 (%s): the shipped preset is not a JSON object\n", name);
        return 1;
    }
    const std::string wantName = std::string("Rando Seed Settings - ") + name;
    const bool named = preset->contains("presetName") && (*preset)["presetName"].is_string() &&
                       (*preset)["presetName"].get<std::string>() == wantName;
    const bool builtIn =
        preset->contains("isBuiltIn") && (*preset)["isBuiltIn"].is_boolean() && (*preset)["isBuiltIn"].get<bool>();
    if (!named || !builtIn) {
        printf("[TEST] FAIL: J1 (%s): named '%s' %d, built in %d\n", name, wantName.c_str(), named ? 1 : 0,
               builtIn ? 1 : 0);
        failures++;
    }
    if (!preset->contains("blocks") || !(*preset)["blocks"].is_object() || !(*preset)["blocks"].contains("rando") ||
        !(*preset)["blocks"]["rando"].is_object()) {
        printf("[TEST] FAIL: J1 (%s): no Rando Settings block (blocks.rando)\n", name);
        return failures + 1;
    }
    const nlohmann::json& rando = (*preset)["blocks"]["rando"];
    const bool settingsObject = rando.contains("gRandoSettings") && rando["gRandoSettings"].is_object();
    const bool profileReset =
        rando.contains(COMBO_PRESET_PAIRED_PROFILE_KEY) && rando[COMBO_PRESET_PAIRED_PROFILE_KEY].is_null();
    printf("[TEST] J1 (%s): gRandoSettings an object %d (%zu keys); %s present and null %d\n", name,
           settingsObject ? 1 : 0, settingsObject ? rando["gRandoSettings"].size() : (size_t)0,
           COMBO_PRESET_PAIRED_PROFILE_KEY, profileReset ? 1 : 0);
    if (!settingsObject) {
        printf("[TEST] FAIL: J1 (%s): the Rando Settings block holds no gRandoSettings object\n", name);
        return failures + 1;
    }
    if (!profileReset) {
        printf("[TEST] FAIL: J1 (%s): the Rando Settings block does not reset the paired profile (\"%s\": null), so "
               "applying it keeps whatever MM options and Cross-Game rules the session held, and the MM and [Both "
               "Games] defaults the D wave moved are not taken\n",
               name, COMBO_PRESET_PAIRED_PROFILE_KEY);
        failures++;
    }

    const nlohmann::json& settings = rando["gRandoSettings"];
    int pinned = 0;
    for (const RandoPresetDWaveKey& dwave : kRandoPresetDWaveKeys) {
        if (settings.contains(dwave.key)) {
            printf("[TEST] FAIL: J2 (%s): gRandoSettings.%s is pinned to %s; %s, so the preset must leave it out\n",
                   name, dwave.key, settings[dwave.key].dump().c_str(), dwave.why);
            pinned++;
        }
    }
    printf("[TEST] J2 (%s): %d of %zu D-wave keys pinned\n", name, pinned,
           sizeof(kRandoPresetDWaveKeys) / sizeof(kRandoPresetDWaveKeys[0]));
    return failures + pinned;
}

TestResult Test_RandoPresetsFollowDefaults(void) {
    printf("[TEST] rando-presets-follow-defaults: the Standard and Beginner presets follow the D wave's OoTMM "
           "defaults and reset the paired profile (operator ruling 2026-11-05)\n");
    int failures = 0;
    nlohmann::json beginner;
    for (const char* name : kRandoPresetsFollowingDefaults) {
        nlohmann::json preset;
        failures += RandoPresets_CheckFollowsDefaults(name, &preset);
        if (std::string(name) == "Beginner") {
            beginner = preset;
        }
    }

    // J3: what Beginner keeps of Ship of Harkinian's own Beginner preset.
    const nlohmann::json* settings = nullptr;
    if (beginner.is_object() && beginner.contains("blocks") && beginner["blocks"].contains("rando") &&
        beginner["blocks"]["rando"].contains("gRandoSettings") &&
        beginner["blocks"]["rando"]["gRandoSettings"].is_object()) {
        settings = &beginner["blocks"]["rando"]["gRandoSettings"];
    }
    const bool excludes = settings != nullptr && settings->contains("ExcludedLocations") &&
                          (*settings)["ExcludedLocations"].is_string() &&
                          (*settings)["ExcludedLocations"].get<std::string>() == "147,148,233,323,";
    const bool kokiriSword = settings != nullptr && settings->contains("StartingKokiriSword") &&
                             (*settings)["StartingKokiriSword"].is_number_integer() &&
                             (*settings)["StartingKokiriSword"].get<int>() == 1;
    printf("[TEST] J3 (Beginner): excludes the four hard checks %d; starts with the Kokiri Sword %d\n",
           excludes ? 1 : 0, kokiriSword ? 1 : 0);
    if (!excludes) {
        printf("[TEST] FAIL: J3: Beginner no longer excludes RC 147, 148, 233 and 323 (the 40 and 50 Gold "
               "Skulltula rewards, the Frogs' Ocarina Game, the 1500-point horseback archery prize), a choice of "
               "Ship of Harkinian's own Beginner preset that no D-wave lane touched\n");
        failures++;
    }
    if (!kokiriSword) {
        printf("[TEST] FAIL: J3: Beginner no longer starts with the Kokiri Sword, a choice of Ship of Harkinian's "
               "own Beginner preset that no D-wave lane touched\n");
        failures++;
    }

    if (failures != 0) {
        printf("[TEST] FAIL: rando-presets-follow-defaults: %d check(s) failed\n", failures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: rando-presets-follow-defaults\n");
    return TEST_PASS;
}
