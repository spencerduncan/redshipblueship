/**
 * @file combo_preset_profile.cpp
 * @brief The paired half of a preset's Rando Settings block (#1368). See
 *        combo_preset_profile.h for the contract.
 *
 * Holds no key of its own: every key it reads or writes comes from a table
 * MM registered (options, trap types, tricks, checks) or from the Cross-Game
 * settings model, and every write goes through that table's writer.
 */

#include "combo_preset_profile.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "combo_mm_locations_view.h" // the excluded-check list and its writers
#include "combo_mm_options_page.h"   // Combo_MMOptionsPages_Init, Combo_RandoPages_FrozenNote
#include "combo_mm_options_view.h"   // the option and trap-type writers, Combo_MMProfileFrozen
#include "combo_mm_tricks_view.h"    // the trick writers
#include "combo_settings_view.h"     // the Cross-Game rules' writers
#include "foreign_items.h"           // Combo_ComboSettingsFrozen, gComboCtx (the fingerprint)

namespace {

using nlohmann::json;

/** Split a dotted CVar name into its path segments. */
std::vector<std::string> CVarPath(const char* cvar) {
    std::vector<std::string> parts;
    std::string part;
    for (const char* c = cvar; *c != '\0'; c++) {
        if (*c == '.') {
            parts.push_back(part);
            part.clear();
        } else {
            part += *c;
        }
    }
    parts.push_back(part);
    return parts;
}

/** The profile's value for @p cvar, or nullptr when it holds none. */
const json* Lookup(const json& profile, const char* cvar) {
    const json* node = &profile;
    for (const std::string& part : CVarPath(cvar)) {
        if (!node->is_object()) {
            return nullptr;
        }
        auto it = node->find(part);
        if (it == node->end()) {
            return nullptr;
        }
        node = &*it;
    }
    return node;
}

void Store(json& profile, const char* cvar, const json& value) {
    json* node = &profile;
    for (const std::string& part : CVarPath(cvar)) {
        node = &(*node)[part];
    }
    *node = value;
}

/** The tables are published by a call, not a registrar (combo_mm_options_page.h);
 *  the menu's bring-up has made it by the time a preset can be saved or applied.
 *  Made here only if it has not, because the registries log a re-registration. */
void EnsureTables() {
    if (Combo_MMOptionCount() == 0 || Combo_MMTrickCount() == 0 || Combo_MMLocationCount() == 0) {
        Combo_MMOptionsPages_Init();
    }
}

/** An integer slot's value: an integer, or a boolean as 0/1 (a hand-written
 *  preset may say true). Anything else is not a value the key can hold. */
bool ReadInt(const json& value, int32_t* out) {
    if (value.is_boolean()) {
        *out = value.get<bool>() ? 1 : 0;
        return true;
    }
    if (value.is_number_integer()) {
        *out = (int32_t)value.get<int64_t>();
        return true;
    }
    return false;
}

/** The page's own test of an option row: PARTIAL and DORMANT rows are drawn
 *  disabled because setting them cannot change the world in this build, and
 *  MM's Logic is drawn read-only because [Both Games] Logic decides it (#1367;
 *  the profile carries that rule as gCombo.Rando.Logic instead). */
bool OptionIsAuthorable(const ComboMMOptionDesc* desc) {
    return desc->liveness != (uint8_t)COMBO_MM_LIVENESS_PARTIAL &&
           desc->liveness != (uint8_t)COMBO_MM_LIVENESS_DORMANT && !Combo_MMOptionsPage_OptionSetByCombo(desc);
}

/** Why the page will not let a player set @p desc (OptionIsAuthorable false). */
const char* OptionRefusalReason(const ComboMMOptionDesc* desc) {
    if (Combo_MMOptionsPage_OptionSetByCombo(desc)) {
        // [Both Games] Logic, or [Both Games] Triforce Hunt under its goal (#1389).
        return Combo_MMOptionsPage_OptionSetByComboReason(desc);
    }
    return desc->disabledReason != NULL ? desc->disabledReason : "not available";
}

void Refuse(ComboPresetProfileReport& report, const char* key, const char* why) {
    std::fprintf(stderr, "[Presets] paired profile: '%s' REFUSED: %s\n", key, why);
    report.refused++;
}

/** Every leaf of @p node, as dotted CVar names. */
void Leaves(const json& node, const std::string& prefix, std::vector<std::string>& out) {
    if (node.is_object() && !node.empty()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            Leaves(it.value(), prefix.empty() ? it.key() : prefix + "." + it.key(), out);
        }
        return;
    }
    if (!prefix.empty()) {
        out.push_back(prefix);
    }
}

/** The excluded-check list's one spelling, ascending ids each followed by a
 *  comma (the writers' form), "" when empty. */
std::string ExcludedChecksString() {
    const int count = Combo_MMLocationsExcluded(NULL, 0);
    if (count <= 0) {
        return "";
    }
    std::vector<uint16_t> ids((size_t)count);
    Combo_MMLocationsExcluded(ids.data(), count);
    std::string s;
    for (uint16_t id : ids) {
        s += std::to_string((unsigned)id);
        s += ',';
    }
    return s;
}

/** Parse a list in any spelling the key accepts: comma-separated ids. */
std::vector<long> ParseIds(const std::string& raw) {
    std::vector<long> ids;
    const char* p = raw.c_str();
    while (*p != '\0') {
        char* end = nullptr;
        const long id = std::strtol(p, &end, 10);
        if (end != p) {
            ids.push_back(id);
        }
        while (*p != '\0' && *p != ',') {
            p++;
        }
        if (*p == ',') {
            p++;
        }
    }
    return ids;
}

} // namespace

json ComboPresetProfile_Capture(void) {
    EnsureTables();
    json profile = json::object();

    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
        // Only what the player chose: an unchosen key must come back unchosen.
        // MM's Logic is never carried (#1367): [Both Games] Logic decides it and
        // the page shows it read-only. Its key matters only to a world made
        // before that rule, and the load restore, not a preset, puts that
        // world's value back.
        if (desc != NULL && OptionIsAuthorable(desc) && Combo_MMOptionIsExplicit(desc)) {
            Store(profile, desc->cvar, Combo_MMOptionGetValue(desc));
        }
    }
    for (int i = 0; i < Combo_MMTrapTypeCount(); i++) {
        const ComboMMTrapTypeDesc* desc = Combo_MMTrapTypeAt(i);
        if (desc != NULL && Combo_CVarIsExplicitInt(desc->cvar)) {
            Store(profile, desc->cvar, Combo_MMTrapTypeGetValue(desc) ? 1 : 0);
        }
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
        // The reader agrees with the writer: a reserved or unbound key reads off
        // whatever its CVar holds, so a stale one is never carried.
        if (desc != NULL && Combo_MMTrickGetValue(desc)) {
            Store(profile, desc->cvar, 1);
        }
    }
    if (!Combo_MMLocationsBlockedByLogic()) {
        const std::string excluded = ExcludedChecksString();
        if (!excluded.empty()) {
            Store(profile, COMBO_MM_EXCLUDED_CHECKS_CVAR, excluded);
        }
    }
    for (int id = 0; id < (int)COMBO_SETTING_COUNT; id++) {
        int32_t value = 0;
        if (Combo_ComboSettingReadStore((ComboSettingId)id, &value) &&
            Combo_ComboSettingValueValid((ComboSettingId)id, value)) {
            Store(profile, Combo_ComboSettingKey((ComboSettingId)id), value);
        }
    }
    return profile;
}

std::string ComboPresetProfile_RefusalNote(void) {
    if (!Combo_MMProfileFrozen() && !Combo_ComboSettingsFrozen()) {
        return "";
    }
    char note[192];
    Combo_RandoPages_FrozenNote(gComboCtx.comboSettingsHash, note, sizeof(note));
    return note;
}

ComboPresetProfileReport ComboPresetProfile_Apply(const json& profile, bool merge) {
    ComboPresetProfileReport report;
    const std::string refusal = ComboPresetProfile_RefusalNote();
    if (!refusal.empty()) {
        // The writers would refuse key by key; refusing here keeps the profile
        // whole: none of it, rather than whichever keys a gate let through.
        std::fprintf(stderr, "[Presets] paired profile REFUSED: %s\n", refusal.c_str());
        return report;
    }
    if (!profile.is_null() && !profile.is_object()) {
        std::fprintf(stderr, "[Presets] paired profile REFUSED: not an object (a %s)\n", profile.type_name());
        return report;
    }
    if (!Combo_ComboSettingStoreAvailable()) {
        std::fprintf(stderr, "[Presets] paired profile REFUSED: no CVar store in this process\n");
        return report;
    }
    EnsureTables();
    report.applied = true;

    const bool reset = profile.is_null();
    const json empty = json::object();
    const json& source = reset ? empty : profile;
    // A reset clears every key whatever the strategy, as SoH's null block does.
    const bool clearAbsent = reset || !merge;
    std::set<std::string> known;

    // ---- MM options. #1367: MM's Logic is refused here, because the
    //      Cross-Game rules below carry it as [Both Games] Logic, and the
    //      excluded-check writer reads the logic that rule derives (never
    //      Vanilla while a preset can apply), not MM's own key.
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
        if (desc == NULL) {
            continue;
        }
        known.insert(desc->cvar);
        const json* value = Lookup(source, desc->cvar);
        int32_t v = 0;
        if (value != nullptr && !ReadInt(*value, &v)) {
            Refuse(report, desc->cvar, "not an integer");
            value = nullptr;
        } else if (value != nullptr && !OptionIsAuthorable(desc)) {
            Refuse(report, desc->cvar, OptionRefusalReason(desc));
            value = nullptr;
        }
        if (value != nullptr) {
            Combo_MMOptionSetValue(desc, v);
            report.written++;
        } else if (clearAbsent) {
            Combo_MMOptionClear(desc);
            report.cleared++;
        }
    }

    // ---- MM trap types --------------------------------------------------------
    for (int i = 0; i < Combo_MMTrapTypeCount(); i++) {
        const ComboMMTrapTypeDesc* desc = Combo_MMTrapTypeAt(i);
        if (desc == NULL) {
            continue;
        }
        known.insert(desc->cvar);
        const json* value = Lookup(source, desc->cvar);
        int32_t v = 0;
        if (value != nullptr && !ReadInt(*value, &v)) {
            Refuse(report, desc->cvar, "not an integer");
            value = nullptr;
        }
        if (value != nullptr && Combo_MMTrapTypeSetValue(desc, v != 0)) {
            report.written++;
        } else if (clearAbsent) {
            Combo_MMTrapTypeClear(desc);
            report.cleared++;
        }
    }

    // ---- MM tricks -------------------------------------------------------------
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
        if (desc == NULL) {
            continue;
        }
        known.insert(desc->cvar);
        const json* value = Lookup(source, desc->cvar);
        int32_t v = 0;
        if (value != nullptr && !ReadInt(*value, &v)) {
            Refuse(report, desc->cvar, "not an integer");
            value = nullptr;
        }
        const bool on = value != nullptr && v != 0;
        if (on && (desc->reserved || !desc->bound)) {
            // Combo_MMTrickSetValue refuses these too (and logs its own line);
            // asked here so the report counts it. Under overwrite the key still
            // goes back to "never chosen" below: off is the only value it can
            // honestly hold.
            Refuse(report, desc->cvar,
                   desc->disabledReason != NULL && desc->disabledReason[0] != '\0' ? desc->disabledReason
                                                                                   : "not settable in this build");
            if (clearAbsent) {
                Combo_MMTrickClear(desc);
                report.cleared++;
            }
            continue;
        }
        if (on) {
            Combo_MMTrickSetValue(desc, true);
            report.written++;
        } else if (value != nullptr || clearAbsent) {
            // Off is "never chosen" for a trick: both resolve off.
            Combo_MMTrickClear(desc);
            report.cleared++;
        }
    }

    // ---- MM excluded checks ----------------------------------------------------
    known.insert(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    if (const json* value = Lookup(source, COMBO_MM_EXCLUDED_CHECKS_CVAR); value != nullptr) {
        if (!value->is_string()) {
            Refuse(report, COMBO_MM_EXCLUDED_CHECKS_CVAR, "not a list of check ids");
        } else {
            // The preset's list REPLACES the authored one.
            Combo_MMLocationsClearExcluded();
            std::vector<uint16_t> ids;
            for (long id : ParseIds(value->get<std::string>())) {
                if (id >= 0 && id <= 0xFFFF && Combo_MMLocationById((uint16_t)id) != NULL) {
                    ids.push_back((uint16_t)id);
                } else {
                    char why[64];
                    std::snprintf(why, sizeof(why), "%ld is not a Majora's Mask check", id);
                    Refuse(report, COMBO_MM_EXCLUDED_CHECKS_CVAR, why);
                }
            }
            if (ids.empty()) {
                report.cleared++;
            } else if (Combo_MMLocationsSetExcluded(ids.data(), (int)ids.size(), true)) {
                report.written++;
            } else {
                Refuse(report, COMBO_MM_EXCLUDED_CHECKS_CVAR, COMBO_MM_LOCATIONS_VANILLA_REASON);
            }
        }
    } else if (clearAbsent) {
        Combo_MMLocationsClearExcluded();
        report.cleared++;
    }

    // ---- Cross-Game rules ------------------------------------------------------
    for (int id = 0; id < (int)COMBO_SETTING_COUNT; id++) {
        const char* key = Combo_ComboSettingKey((ComboSettingId)id);
        known.insert(key);
        const json* value = Lookup(source, key);
        int32_t v = 0;
        if (value != nullptr && !ReadInt(*value, &v)) {
            Refuse(report, key, "not an integer");
            value = nullptr;
        }
        if (value != nullptr) {
            // Combo_ComboSettingSet refuses an out-of-space value (and logs it).
            if (Combo_ComboSettingSet((ComboSettingId)id, v)) {
                report.written++;
            } else {
                report.refused++;
                if (clearAbsent) {
                    Combo_ComboSettingClear((ComboSettingId)id);
                    report.cleared++;
                }
            }
        } else if (clearAbsent) {
            Combo_ComboSettingClear((ComboSettingId)id);
            report.cleared++;
        }
    }

    // ---- Anything else the profile holds is not ours to write -------------------
    std::vector<std::string> leaves;
    Leaves(source, "", leaves);
    for (const std::string& leaf : leaves) {
        if (known.find(leaf) == known.end()) {
            std::fprintf(stderr, "[Presets] paired profile: '%s' ignored: not a paired-profile setting\n",
                         leaf.c_str());
            report.ignored++;
        }
    }

    std::fprintf(stderr, "[Presets] paired profile applied (%s): %d written, %d cleared, %d refused, %d ignored\n",
                 reset ? "reset" : (merge ? "merge" : "overwrite"), report.written, report.cleared, report.refused,
                 report.ignored);
    return report;
}
