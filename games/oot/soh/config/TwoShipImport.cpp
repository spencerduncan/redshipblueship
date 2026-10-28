#include "TwoShipImport.h"
#include "ConfigUpdaters.h"

// src/common — the cross-game CVar classification manifest. The importer and
// ConfigVersion7Updater read the SAME table, so they cannot disagree about
// what maps to what (ADR 0003 §6.4).
#include "cvar_shared_keys.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace SOH {

const char* const kTwoShipImportStampKey = "TwoShipImport.Completed";

namespace {

constexpr const char* kTwoShipAppShortName = "2ship";
constexpr const char* kTwoShipConfigFileName = "2ship2harkinian.json";

/**
 * Walk a dotted CVar name through 2Ship's NESTED config json.
 *
 * `gSettings.Audio.MasterVolume` is stored as
 * `CVars > gSettings > Audio > MasterVolume`, so the lookup is a segment walk
 * rather than a flat map hit. Returns nullptr when any segment is missing.
 */
const nlohmann::json* FindNested(const nlohmann::json& cvars, const std::string& dottedKey) {
    const nlohmann::json* node = &cvars;
    std::size_t start = 0;

    while (start <= dottedKey.size()) {
        const std::size_t dot = dottedKey.find('.', start);
        const std::string segment = dottedKey.substr(start, dot == std::string::npos ? std::string::npos : dot - start);

        if (!node->is_object() || !node->contains(segment)) {
            return nullptr;
        }
        node = &(*node)[segment];

        if (dot == std::string::npos) {
            return node;
        }
        start = dot + 1;
    }
    return nullptr;
}

/*
 * 2Ship's own config migrations are NOT replayed (#1240).
 *
 * ADR 0003 §6.4's ordering hazard says a legacy `2ship2harkinian.json` must be
 * brought up to 2Ship's own config version before key mapping, and until #1240
 * this file replayed `Ben::ConfigVersion1Updater`'s one rename against the
 * parsed json. Neither v1 row may be replayed in the single exe:
 *
 *   - `gFixes.FixAmmoCountEnvColor` -> `gFixes.FixButtonEnvColor`: MM's code
 *     never followed the rename (`z_parameter.c` reads the old spelling, as does
 *     2Ship's own menu row), so replaying it moved the player's value onto a key
 *     nothing reads. The old spelling is the live one and imports through
 *     RSBS::kTwoShipImportKeys; the new one is on RSBS::kTwoShipDoNotImport.
 *   - The `gDisplayOverlay` reshape reads `gWindows.DisplayOverlay`, a window
 *     key the import does not carry; a v1 file already holds the reshaped
 *     `gDisplayOverlay.Mode`, which imports through the list.
 *
 * A later 2Ship migration that renames a key the import carries is replayed
 * here, against the parsed json, before the walks below.
 */

/** Is this CVar already set in the LIVE store? See ConfigUpdaters.cpp. */
bool CVarPresent(const char* name) {
    return CVarGet(name) != nullptr;
}

/**
 * Adopt one imported value into the live store under its canonical spelling.
 *
 * Applies ADR 0003 §6.3's conflict rule: OoT's value wins, so an imported
 * value lands only where the canonical key is currently unset. A 2Ship user
 * who has also been playing RSBS keeps what they set in RSBS's own menu.
 */
bool AdoptImportedValue(const RSBS::ConvergedKey& key, const nlohmann::json& value) {
    if (!ShouldAdoptLegacyValue(/*legacyPresent=*/true, CVarPresent(key.canonical))) {
        return false;
    }

    if (key.scaledPercent) {
        // 2Ship stored volume as a float scale; OoT stores integer percent and
        // every reader calls CVarGetInteger. Importing the raw float would
        // produce a key whose readers all fall back to their default.
        if (!value.is_number()) {
            return false;
        }
        CVarSetInteger(key.canonical, VolumePercentFromFloatScale(value.get<float>()));
        return true;
    }

    // Colours serialize as a nested {R,G,B,Type} object, not a scalar.
    if (value.is_object() && value.contains("R") && value.contains("G") && value.contains("B")) {
        Color_RGB8 color;
        color.r = (uint8_t)value["R"].get<uint32_t>();
        color.g = (uint8_t)value["G"].get<uint32_t>();
        color.b = (uint8_t)value["B"].get<uint32_t>();
        CVarSetColor24(key.canonical, color);
        return true;
    }

    if (value.is_number_integer() || value.is_boolean()) {
        CVarSetInteger(key.canonical, value.get<int32_t>());
        return true;
    }
    if (value.is_number_float()) {
        CVarSetFloat(key.canonical, value.get<float>());
        return true;
    }
    if (value.is_string()) {
        CVarSetString(key.canonical, value.get<std::string>().c_str());
        return true;
    }
    return false;
}

/** Where a standalone 2Ship install would have left its config. */
std::vector<std::string> CandidateSourcePaths() {
    std::vector<std::string> paths;
    paths.push_back(Ship::Context::GetPathRelativeToAppDirectory(kTwoShipConfigFileName, kTwoShipAppShortName));
    // Portable installs keep the json beside the executable rather than in the
    // per-app data directory.
    paths.push_back(std::string("./") + kTwoShipConfigFileName);
    return paths;
}

/** How reading one source file went. */
enum class SourceStatus {
    Imported,   ///< Parsed and walked (possibly adopting nothing).
    Unreadable, ///< Could not be opened or is not valid JSON: do not stamp, a repaired file should still import.
    NoCVars,    ///< Valid JSON without a CVars object: nothing to import, ever.
};

struct SourceResult {
    SourceStatus status = SourceStatus::Unreadable;
    uint32_t sourceVersion = 0;
    int imported = 0;
    int skipped = 0;
};

/**
 * The import itself, on one source file: parse, replay 2Ship's own migrations, walk the tables and adopt under the
 * conflict rule. No stamp and no save: ImportTwoShipConfig owns those, and the test seam below must not write any
 * config.
 */
SourceResult ImportFromSource(const std::string& sourcePath) {
    SourceResult result;
    nlohmann::json source;
    try {
        std::ifstream file(sourcePath);
        if (!file.is_open()) {
            SPDLOG_WARN("2Ship config import: cannot open {}, skipping", sourcePath);
            return result;
        }
        file >> source;
    } catch (const std::exception& e) {
        // A malformed source file is the user's problem to fix, not a reason to
        // fail RSBS's boot. Do NOT stamp: a corrupt file they later repair
        // should still get imported.
        SPDLOG_WARN("2Ship config import: {} is not valid JSON ({}), skipping", sourcePath, e.what());
        return result;
    }

    if (!source.contains("CVars") || !source["CVars"].is_object()) {
        result.status = SourceStatus::NoCVars;
        return result;
    }

    nlohmann::json cvars = source["CVars"];
    result.sourceVersion = source.value("ConfigVersion", 0u);

    for (const RSBS::ConvergedKey& key : RSBS::kConvergedKeys) {
        const nlohmann::json* value = FindNested(cvars, key.legacy);
        if (value == nullptr) {
            continue;
        }
        if (AdoptImportedValue(key, *value)) {
            result.imported++;
        } else {
            result.skipped++;
        }
    }

    // #1240: the MM-only keys that have a shared-menu row, and the shared keys
    // safe to carry, under the spelling MM reads, adopted as-is under the same
    // conflict rule. After the converged walk, so a legacy spelling converging
    // onto a listed key (the Ambience volume) is converted first and wins. The
    // list is fixed at compile time: this runs before the menu exists.
    for (const char* key : RSBS::kTwoShipImportKeys) {
        const nlohmann::json* value = FindNested(cvars, key);
        if (value == nullptr) {
            continue;
        }
        const RSBS::ConvergedKey asIs = { key, key, false };
        if (AdoptImportedValue(asIs, *value)) {
            result.imported++;
        } else {
            result.skipped++;
        }
    }

    result.status = SourceStatus::Imported;
    return result;
}

} // namespace

int ImportTwoShipConfigFile(const std::string& sourcePath) {
    const SourceResult result = ImportFromSource(sourcePath);
    return result.status == SourceStatus::Unreadable ? -1 : result.imported;
}

bool ImportTwoShipConfig(Ship::Config* conf) {
    if (conf == nullptr) {
        return false;
    }

    // Run-once. Stamped even when there was nothing to import, so a user
    // without a 2Ship install does not re-probe the filesystem every boot.
    if (conf->GetBool(kTwoShipImportStampKey, false)) {
        return false;
    }

    std::string sourcePath;
    for (const std::string& candidate : CandidateSourcePaths()) {
        std::error_code ec;
        if (!candidate.empty() && std::filesystem::exists(candidate, ec) && !ec) {
            sourcePath = candidate;
            break;
        }
    }

    if (sourcePath.empty()) {
        // Nothing to import. Stamp anyway — see above.
        conf->SetBool(kTwoShipImportStampKey, true);
        conf->Save();
        return false;
    }

    const SourceResult result = ImportFromSource(sourcePath);
    if (result.status == SourceStatus::Unreadable) {
        return false;
    }
    if (result.status == SourceStatus::NoCVars) {
        conf->SetBool(kTwoShipImportStampKey, true);
        conf->Save();
        return false;
    }

    conf->SetBool(kTwoShipImportStampKey, true);
    conf->Save();
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();

    SPDLOG_INFO("2Ship config import from {} (source version {}): {} setting(s) adopted, {} already set in RSBS",
                sourcePath, result.sourceVersion, result.imported, result.skipped);
    return result.imported > 0;
}

} // namespace SOH
