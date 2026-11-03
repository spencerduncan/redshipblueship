/**
 * @file soh_twoship_import_test.cpp
 * @brief ROM-free, display-free lock for #1240 (W6-IMPORT): a returning 2Ship player's MM-only settings reach the
 *        combined game through the one-time 2Ship config import.
 *
 * CTest label "redship", row `TwoShipImportMmKeys` in CMake/SingleExecutable.cmake, dispatch `twoship-import-mm-keys`
 * in src/common/test_runner.cpp (Test_TwoShipImportMmKeys, which runs the display-free shared bring-up first because
 * this row reads and writes the CVar store).
 *
 * WHAT WAS BROKEN
 * ===============
 * SOH::ImportTwoShipConfig walked only RSBS::kConvergedKeys, so it carried only the settings that have a SoH twin.
 * Every MM-only key a 2Ship player set (first-person, gyro and camera inversion, sensitivities, Fast Text, motion
 * blur and the rest of #1240's table) was dropped, and Termina played with 2Ship's defaults, three of which are
 * inverted Y axes. The import also replayed 2Ship's v1 rename gFixes.FixAmmoCountEnvColor -> gFixes.FixButtonEnvColor,
 * which in the single exe moves the value off the spelling z_parameter.c reads.
 *
 * WHAT THIS DRIVES
 * ================
 * SOH::ImportTwoShipConfigFile: the pipeline ImportTwoShipConfig runs (parse, 2Ship version replay, the walks and the
 * conflict rule) on a synthetic 2ship2harkinian.json in this process's own scratch directory, without the run-once
 * stamp and without saving any config. The CVar store is touched in memory only, and every key this row sets it
 * clears again. Keys are named only through src/common/cvar_shared_keys.h (the import list and the do-not-import
 * table), never as literals here: games/oot spells no MM key.
 *
 * LEGS
 * ====
 *  1 Control: a converged key's legacy spelling still lands on its canonical key, so a red leg 2 is about the list
 *    and not a seam that reads nothing.
 *  2 Every RSBS::kTwoShipImportKeys entry, written as its own integer, lands on that key with that integer. Each entry
 *    is the spelling MM reads: CvarClassification's leg 3f-2c holds MM's literal read for every RSBS_CVAR_MM_* define.
 *  3 The same keys written as floats land as floats. MM reads its sensitivities with CVarGetFloat, and an integer
 *    stored over a float key reads back as the default.
 *  4 The ammo-colour key in a version-0 file lands under its live spelling, and 2Ship's v1 rename target stays unset.
 *  5 Every RSBS::kTwoShipDoNotImport key, and one member of every family on it, is untouched.
 *  6 ADR 0003 §6.3: a value already set in the combined config wins over the imported one.
 *  7 #1272 item 3: 2Ship's changed colours land where the single exe reads them. Each MM element with an SoH twin (the
 *    first kTranslatedKeys row of its MM key) lands on SoH's Cosmetics Editor row, .Value and .Changed, and a fallback
 *    row (SoH's C Buttons) stays untouched; each MM-only element lands on its own MM row's keys; an unchanged 2Ship
 *    colour imports nothing; and an SoH row already changed keeps its colour. Driven from the manifest and the MM
 *    rows' view, so no MM key is spelled here.
 *
 * RED without the fix: the importer ignores the list, so legs 2, 3 and 4 fail (and leg 4 also because the replayed
 * rename moves the ammo value onto a key nothing imports). RED without #1272 item 3: leg 7 (no colour imports).
 */

#include "soh/config/TwoShipImport.h"
#include "soh/Enhancements/cosmetics/CosmeticsEditorMm.h" // leg 7: the MM rows' view

#include "cvar_shared_keys.h" // src/common: the import list and the do-not-import table
#include "test_scratch.h"     // src/common: per-process scratch directories

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

constexpr const char* kFamilyProbeLeaf = "RsbsImportProbe";

/// Writes @p value under @p dottedKey in 2Ship's nested CVars object. False on a shape conflict (a key that is both a
/// leaf and the parent of another key in the same file), which the row reports instead of throwing.
bool PutNested(nlohmann::json& cvars, const std::string& dottedKey, const nlohmann::json& value) {
    std::string pointer = "/";
    for (char c : dottedKey) {
        pointer += (c == '.') ? '/' : c;
    }
    try {
        cvars[nlohmann::json::json_pointer(pointer)] = value;
    } catch (const std::exception&) { return false; }
    return true;
}

bool WriteSource(const std::filesystem::path& path, uint32_t version, const nlohmann::json& cvars) {
    nlohmann::json root;
    root["ConfigVersion"] = version;
    root["CVars"] = cvars;
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << root.dump(4);
    return out.good();
}

/// The key a do-not-import entry is probed through: the key itself, or one synthetic member of a family.
std::string ProbeKeyFor(const RSBS::TwoShipDoNotImport& entry) {
    return entry.family ? std::string(entry.key) + kFamilyProbeLeaf : std::string(entry.key);
}

/// The first converged row whose value imports unconverted, for the control leg.
const RSBS::ConvergedKey* ControlRow() {
    for (const RSBS::ConvergedKey& row : RSBS::kConvergedKeys) {
        if (!row.scaledPercent) {
            return &row;
        }
    }
    return nullptr;
}

void ClearImportKeys() {
    for (const char* key : RSBS::kTwoShipImportKeys) {
        CVarClear(key);
    }
}

struct Failures {
    std::vector<std::string> lines;
    void Add(const std::string& line) {
        lines.push_back(line);
    }
};

/// One do-not-import probe's state before the import, to compare after.
struct ProbeState {
    std::string key;
    const char* why;
    bool present;
    int32_t value;
};

} // namespace

extern "C" int OoT_TwoShipImport_RunHeadless(void) {
    printf("[TEST] twoship-import-mm-keys: the 2Ship config import carries every MM-only key with a shared-menu row "
           "(#1240)\n");
    Failures failures;

    const RSBS::ConvergedKey* control = ControlRow();
    if (control == nullptr) {
        printf("[TEST] FAIL: RSBS::kConvergedKeys has no plain rename row for the control leg\n");
        return 1;
    }
    if (RSBS::kTwoShipImportKeyCount == 0) {
        printf("[TEST] FAIL: RSBS::kTwoShipImportKeys is empty, so legs 2 and 3 would prove nothing\n");
        return 1;
    }

    const std::filesystem::path dir = rsbs::test::ScratchDir("rsbs_twoship_import_test");
    const std::filesystem::path source = dir / "2ship2harkinian.json";
    const char* ammoKey = RSBS_CVAR_MM_FIXES_AMMO_COUNT_ENV_COLOR;

    // ---- Run 1: integers in a version-0 file (so 2Ship's v1 rows are replayed): legs 1, 2, 4 and 5. ----
    ClearImportKeys();
    CVarClear(control->canonical);

    nlohmann::json cvars = nlohmann::json::object();
    bool shapeOk = PutNested(cvars, control->legacy, 1);
    int32_t ammoWant = 1;
    bool ammoListed = false;
    for (std::size_t i = 0; i < RSBS::kTwoShipImportKeyCount; i++) {
        const char* key = RSBS::kTwoShipImportKeys[i];
        if (!PutNested(cvars, key, static_cast<int32_t>(1000 + i))) {
            failures.Add(std::string("synthetic file: \"") + key +
                         "\" is both a leaf and the parent of another listed key, so 2Ship's nested json cannot hold "
                         "both");
            shapeOk = false;
        }
        if (std::strcmp(key, ammoKey) == 0) {
            ammoListed = true;
            ammoWant = static_cast<int32_t>(1000 + i);
        }
    }
    if (!ammoListed) {
        // Leg 4 holds the ammo key whether or not the list names it: it is #1240's migration hazard.
        CVarClear(ammoKey);
        shapeOk = PutNested(cvars, ammoKey, ammoWant) && shapeOk;
    }
    std::vector<ProbeState> probes;
    for (const RSBS::TwoShipDoNotImport& entry : RSBS::kTwoShipDoNotImport) {
        const std::string key = ProbeKeyFor(entry);
        probes.push_back({ key, entry.why, CVarGet(key.c_str()) != nullptr, CVarGetInteger(key.c_str(), -1) });
        if (!PutNested(cvars, key, 1)) {
            failures.Add("synthetic file: the do-not-import probe \"" + key + "\" collides with a listed key's shape");
            shapeOk = false;
        }
    }

    if (!shapeOk || !WriteSource(source, 0, cvars)) {
        failures.Add("could not build or write the synthetic 2ship2harkinian.json in " + dir.string());
    } else {
        const int adopted = SOH::ImportTwoShipConfigFile(source.string());
        printf("[TEST] run 1 (integers, ConfigVersion 0): %d setting(s) adopted, %zu key(s) listed\n", adopted,
               RSBS::kTwoShipImportKeyCount);

        // Leg 1: the control.
        if (CVarGetInteger(control->canonical, 0) != 1) {
            failures.Add(std::string("leg 1 control: the converged legacy key \"") + control->legacy +
                         "\" did not land on \"" + control->canonical +
                         "\", so the seam imported nothing at all and the other legs would prove nothing");
        }

        // Leg 2: every listed key lands, with its own integer.
        for (std::size_t i = 0; i < RSBS::kTwoShipImportKeyCount; i++) {
            const char* key = RSBS::kTwoShipImportKeys[i];
            const int32_t want = static_cast<int32_t>(1000 + i);
            const int32_t got = CVarGetInteger(key, -1);
            if (got != want) {
                failures.Add(std::string("leg 2: \"") + key + "\" (" + RSBS::kTwoShipImportKeySpellings[i] +
                             ") reads " + std::to_string(got) + " after the import, not the file's " +
                             std::to_string(want) + ": a returning 2Ship player's setting is dropped");
            }
        }

        // Leg 4: the ammo-colour key keeps the spelling z_parameter.c reads (2Ship's v1 rename target is a
        // do-not-import entry, so leg 5 holds that it stays unset).
        if (CVarGetInteger(ammoKey, -1) != ammoWant) {
            failures.Add(std::string("leg 4: \"") + ammoKey +
                         "\" did not import from a version-0 2Ship file under its live spelling (the replayed v1 "
                         "rename moves it onto a key nothing in the single exe reads)");
        }

        // Leg 5: every do-not-import key and family member is untouched.
        for (const ProbeState& st : probes) {
            const bool nowPresent = CVarGet(st.key.c_str()) != nullptr;
            const int32_t nowValue = CVarGetInteger(st.key.c_str(), -1);
            if (nowPresent != st.present || (st.present && nowValue != st.value)) {
                failures.Add("leg 5: the do-not-import key \"" + st.key + "\" changed on import (" + st.why + ")");
            }
        }
    }
    ClearImportKeys();
    CVarClear(control->canonical);
    CVarClear(ammoKey);

    // ---- Run 2: the same keys as floats, in a current (version 1) file: leg 3. ----
    nlohmann::json floats = nlohmann::json::object();
    for (std::size_t i = 0; i < RSBS::kTwoShipImportKeyCount; i++) {
        PutNested(floats, RSBS::kTwoShipImportKeys[i], static_cast<double>(i) + 0.25);
    }
    if (!WriteSource(source, 1, floats)) {
        failures.Add("could not write the float run's synthetic file");
    } else {
        SOH::ImportTwoShipConfigFile(source.string());
        for (std::size_t i = 0; i < RSBS::kTwoShipImportKeyCount; i++) {
            const char* key = RSBS::kTwoShipImportKeys[i];
            const float want = static_cast<float>(static_cast<double>(i) + 0.25);
            const float got = CVarGetFloat(key, -1.0f);
            if (std::fabs(got - want) > 1e-4f) {
                failures.Add(std::string("leg 3: \"") + key + "\" reads " + std::to_string(got) +
                             " as a float after importing " + std::to_string(want) +
                             ": a float setting (a sensitivity) would read back as MM's default");
            }
        }
    }
    ClearImportKeys();

    // ---- Run 3: a value already set in the combined config wins (ADR 0003 section 6.3): leg 6. ----
    const char* presetKey = RSBS::kTwoShipImportKeys[0];
    constexpr int32_t kPresetValue = 4242;
    CVarSetInteger(presetKey, kPresetValue);
    nlohmann::json preset = nlohmann::json::object();
    PutNested(preset, presetKey, 1);
    if (!WriteSource(source, 1, preset)) {
        failures.Add("could not write the conflict run's synthetic file");
    } else {
        SOH::ImportTwoShipConfigFile(source.string());
        if (CVarGetInteger(presetKey, -1) != kPresetValue) {
            failures.Add(std::string("leg 6: \"") + presetKey +
                         "\" was already set in the combined config and the import overwrote it (ADR 0003 "
                         "section 6.3: the value set in RSBS wins)");
        }
    }
    CVarClear(presetKey);

#ifdef RSBS_SINGLE_EXECUTABLE
    // ---- Run 4 (#1272 item 3): 2Ship's changed colours: leg 7. ----
    {
        auto stemOf = [](const char* key, const char* suffix) {
            const std::string k = key;
            const std::size_t n = std::strlen(suffix);
            return (k.size() > n && k.compare(k.size() - n, n, suffix) == 0) ? k.substr(0, k.size() - n)
                                                                             : std::string();
        };
        auto colorJson = [](int r, int g, int b) {
            nlohmann::json c = nlohmann::json::object();
            c["R"] = r;
            c["G"] = g;
            c["B"] = b;
            c["A"] = 255;
            c["Type"] = "RGBA";
            return c;
        };
        struct ColorExpect {
            std::string valueKey;
            std::string changedKey;
            int r;
            int g;
            int b;
        };
        std::vector<ColorExpect> want;      // what must land
        std::vector<std::string> untouched; // .Changed keys that must stay unset
        std::vector<std::string> allKeys;   // every key cleared before and after
        nlohmann::json colors = nlohmann::json::object();
        bool colorShapeOk = true;
        int n = 0;
        std::string presetChanged;
        std::string presetValue;
        for (std::size_t i = 0; i < RSBS::kTranslatedKeyCount; i++) {
            const RSBS::TranslatedKey& key = RSBS::kTranslatedKeys[i];
            if (std::strcmp(key.translator, RSBS::kCosmeticTranslator) != 0) {
                continue;
            }
            const std::string mmStem = stemOf(key.mmKey, ".Color");
            const std::string sohStem = stemOf(key.ootKey, ".Value");
            allKeys.push_back(key.ootKey);
            allKeys.push_back(sohStem + ".Changed");
            bool fallback = false;
            for (std::size_t j = 0; j < i; j++) {
                fallback = fallback || std::strcmp(RSBS::kTranslatedKeys[j].mmKey, key.mmKey) == 0;
            }
            if (fallback) {
                // A fallback row (SoH's C Buttons) is never an import target. It may also be another MM key's
                // first row, which then lands; so only a key that is no first row anywhere must stay untouched.
                bool someonesTwin = false;
                for (std::size_t j = 0; j < RSBS::kTranslatedKeyCount; j++) {
                    bool firstOfItsKey = true;
                    for (std::size_t k = 0; k < j; k++) {
                        firstOfItsKey = firstOfItsKey && std::strcmp(RSBS::kTranslatedKeys[k].mmKey,
                                                                     RSBS::kTranslatedKeys[j].mmKey) != 0;
                    }
                    someonesTwin = someonesTwin ||
                                   (firstOfItsKey &&
                                    std::strcmp(RSBS::kTranslatedKeys[j].translator, RSBS::kCosmeticTranslator) == 0 &&
                                    std::strcmp(RSBS::kTranslatedKeys[j].ootKey, key.ootKey) == 0);
                }
                if (!someonesTwin) {
                    untouched.push_back(sohStem + ".Changed");
                }
                continue;
            }
            n++;
            colorShapeOk = PutNested(colors, mmStem + ".Color", colorJson(n, n + 1, n + 2)) && colorShapeOk;
            colorShapeOk = PutNested(colors, mmStem + ".Changed", 1) && colorShapeOk;
            bool sohRowTaken = false;
            for (const ColorExpect& e : want) {
                sohRowTaken = sohRowTaken || e.valueKey == key.ootKey;
            }
            if (presetChanged.empty()) {
                // The first twin's SoH row is already changed in the combined config: it keeps its colour.
                presetValue = key.ootKey;
                presetChanged = sohStem + ".Changed";
                want.push_back({ presetValue, presetChanged, 7, 8, 9 });
            } else if (!sohRowTaken) {
                want.push_back({ key.ootKey, sohStem + ".Changed", n, n + 1, n + 2 });
            }
        }
        const int rows = CosmeticsEditor_MmRowCount();
        std::string unchangedMm;
        for (int r = 0; r < rows; r++) {
            const char* label = nullptr;
            const char* colorKey = nullptr;
            const char* changedKey = nullptr;
            if (!CosmeticsEditor_MmRowAt(r, &label, &colorKey, &changedKey)) {
                continue;
            }
            const std::string stem = stemOf(colorKey, ".Color");
            allKeys.push_back(colorKey);
            allKeys.push_back(changedKey);
            n++;
            colorShapeOk = PutNested(colors, stem + ".Color", colorJson(n, n + 1, n + 2)) && colorShapeOk;
            if (unchangedMm.empty()) {
                // 2Ship's Reset clears .Changed and may leave .Color: nothing imports.
                unchangedMm = colorKey;
                colorShapeOk = PutNested(colors, stem + ".Changed", 0) && colorShapeOk;
                untouched.push_back(changedKey);
            } else {
                colorShapeOk = PutNested(colors, stem + ".Changed", 1) && colorShapeOk;
                want.push_back({ colorKey, changedKey, n, n + 1, n + 2 });
            }
        }
        for (const std::string& key : allKeys) {
            CVarClear(key.c_str());
        }
        if (want.size() < 3 || presetChanged.empty() || unchangedMm.empty()) {
            failures.Add("leg 7: the manifest or the MM rows' view gave too few colours to drive the leg");
        } else if (!colorShapeOk || !WriteSource(source, 1, colors)) {
            failures.Add("could not build or write the colour run's synthetic file");
        } else {
            CVarSetColor(presetValue.c_str(), Color_RGBA8{ 7, 8, 9, 255 });
            CVarSetInteger(presetChanged.c_str(), 1);
            SOH::ImportTwoShipConfigFile(source.string());
            for (const ColorExpect& e : want) {
                const Color_RGBA8 got = CVarGetColor(e.valueKey.c_str(), Color_RGBA8{ 0, 0, 0, 0 });
                if (CVarGetInteger(e.changedKey.c_str(), 0) != 1 || got.r != e.r || got.g != e.g || got.b != e.b) {
                    failures.Add("leg 7: \"" + e.valueKey + "\" reads " + std::to_string(got.r) + "," +
                                 std::to_string(got.g) + "," + std::to_string(got.b) + " (changed " +
                                 std::to_string(CVarGetInteger(e.changedKey.c_str(), 0)) + "), want " +
                                 std::to_string(e.r) + "," + std::to_string(e.g) + "," + std::to_string(e.b) +
                                 ": a returning 2Ship player's colour is dropped, or overwrote a row set in RSBS");
                }
            }
            for (const std::string& key : untouched) {
                if (CVarGetInteger(key.c_str(), 0) != 0) {
                    failures.Add("leg 7: \"" + key +
                                 "\" was changed by the import: a fallback row or an unchanged 2Ship colour imported");
                }
            }
            printf("[TEST] run 4 (colours): %zu colour(s) checked, %zu key(s) held untouched\n", want.size(),
                   untouched.size());
        }
        for (const std::string& key : allKeys) {
            CVarClear(key.c_str());
        }
    }
#endif

    if (!failures.lines.empty()) {
        const std::size_t shown = failures.lines.size() < 40 ? failures.lines.size() : 40;
        for (std::size_t i = 0; i < shown; i++) {
            printf("[TEST] FAIL: %s\n", failures.lines[i].c_str());
        }
        if (shown < failures.lines.size()) {
            printf("[TEST] FAIL: ... and %zu more\n", failures.lines.size() - shown);
        }
        printf("[TEST] FAIL: %zu failure(s). The importer must walk RSBS::kTwoShipImportKeys beside kConvergedKeys,\n"
               "[TEST]       adopting each value as-is under ADR 0003's conflict rule (TwoShipImport.cpp).\n",
               failures.lines.size());
        return 1;
    }
    printf("[TEST] PASS: %zu listed key(s) import as integers and as floats, the ammo colour keeps its live spelling, "
           "%zu do-not-import entr(ies) stay untouched, and a value set in RSBS wins\n",
           RSBS::kTwoShipImportKeyCount, RSBS::kTwoShipDoNotImportCount);
    return 0;
}
