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

/**
 * 2Ship's own config migrations, replayed against an imported json.
 *
 * ADR 0003 §6.4's ordering hazard: a legacy `2ship2harkinian.json` must be
 * brought up to 2Ship's own config version BEFORE key mapping, or a pre-v1
 * file imports mis-keyed. `Ben::ConfigVersion1Updater` cannot be reused for
 * this — it mutates the LIVE CVar store through CVarCopy/CVarClear, which is
 * the wrong target for a file we are only reading — so its key mapping is
 * replayed here against the parsed json instead.
 *
 * None of 2Ship's v1 rows currently intersect the convergence table (v1 renames
 * `gFixes.FixAmmoCountEnvColor` and reshapes `gDisplayOverlay`, neither of which
 * is a converged key), so today this is a no-op for everything we import. It
 * exists so that a future 2Ship version that DOES touch a converged key is
 * handled at the correct point in the pipeline rather than silently mis-keyed.
 */
void ApplyTwoShipVersionUpdates(nlohmann::json& cvars, uint32_t sourceVersion) {
    struct SourceMigration {
        uint32_t introducedIn;
        const char* from;
        const char* to;
    };
    static constexpr SourceMigration kSourceMigrations[] = {
        { 1, "gFixes.FixAmmoCountEnvColor", "gFixes.FixButtonEnvColor" },
    };

    for (const SourceMigration& migration : kSourceMigrations) {
        if (sourceVersion >= migration.introducedIn) {
            continue; // the source file already applied this one
        }
        const nlohmann::json* value = FindNested(cvars, migration.from);
        if (value == nullptr) {
            continue;
        }
        // Write through the flat pointer form so intermediate objects are
        // created as needed, matching how the destination store nests keys.
        std::string pointer = "/";
        for (char c : std::string(migration.to)) {
            pointer += (c == '.') ? '/' : c;
        }
        cvars[nlohmann::json::json_pointer(pointer)] = *value;
    }
}

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

} // namespace

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

    nlohmann::json source;
    try {
        std::ifstream file(sourcePath);
        if (!file.is_open()) {
            SPDLOG_WARN("2Ship config import: cannot open {}, skipping", sourcePath);
            return false;
        }
        file >> source;
    } catch (const std::exception& e) {
        // A malformed source file is the user's problem to fix, not a reason to
        // fail RSBS's boot. Do NOT stamp: a corrupt file they later repair
        // should still get imported.
        SPDLOG_WARN("2Ship config import: {} is not valid JSON ({}), skipping", sourcePath, e.what());
        return false;
    }

    if (!source.contains("CVars") || !source["CVars"].is_object()) {
        conf->SetBool(kTwoShipImportStampKey, true);
        conf->Save();
        return false;
    }

    nlohmann::json cvars = source["CVars"];
    const uint32_t sourceVersion = source.value("ConfigVersion", 0u);
    ApplyTwoShipVersionUpdates(cvars, sourceVersion);

    int imported = 0;
    int skipped = 0;
    for (const RSBS::ConvergedKey& key : RSBS::kConvergedKeys) {
        const nlohmann::json* value = FindNested(cvars, key.legacy);
        if (value == nullptr) {
            continue;
        }
        if (AdoptImportedValue(key, *value)) {
            imported++;
        } else {
            skipped++;
        }
    }

    conf->SetBool(kTwoShipImportStampKey, true);
    conf->Save();
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();

    SPDLOG_INFO("2Ship config import from {} (source version {}): {} setting(s) adopted, {} already set in RSBS",
                sourcePath, sourceVersion, imported, skipped);
    return imported > 0;
}

} // namespace SOH
