/**
 * @file SeedPackageTestBridgeSingleExe.cpp
 * @brief TEST BRIDGE for the SeedPackageRoundTrip row (#1044): what a seed package
 *        carries beyond the world a load applies, as key=value lines, read either
 *        from the LIVE sources a completed world is built from or from a package
 *        file.
 *
 * The round trip's world report (games/mm/2s2h/mm_rando_gen_test.cpp,
 * SpkWriteWorldReport) covers what the load APPLIES: gComboCtx, OoT's placement and
 * content, the crossings, the give capabilities, MM's half and the spoiler the
 * spoiler window reads. A package also carries what nothing applies: identity.json's
 * self-describing terms (the seed string and name, the hash icons, every OoT setting
 * by name, MM's profile, the build, the combo record and crossing summary),
 * digest.txt and the two split spoilers. This bridge reports those, one line per
 * term, from either side:
 *
 *   live    (pkgPath NULL): Rando::Context, the settings, gComboCtx, the crossing
 *           store, the give capabilities, the completed world Generate holds (its
 *           digest, MM's profile, the goal, the unproved halves, MM's half) and the
 *           spoiler document gGeneral.SpoilerLog names. Read before the package
 *           exists, so a term the writer drops cannot also be missing here.
 *   package (pkgPath set): the entries of the zip, read one by one, and the terms
 *           parsed out of its identity.json.
 *
 * Both sides print every value as the JSON its identity.json term holds (dump()),
 * and every JSON document in canonical form (keys sorted), so the lines compare
 * as text whatever the writer's key order. The row requires the two sides equal:
 * a writer that drops or alters any term, any entry, or either split spoiler makes
 * them differ.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"

#include "context.h"
#include "crossing_store.h"
#include "foreign_items.h"
#include "rsbs_version.h"
#include "seed_package.h"

// ForeignItemsSingleExe.cpp: the completed world Generate holds.
extern "C" int OoT_PairedGen_CompletedWorld(const uint8_t** outHalf, size_t* outHalfSize, const char** outDigest,
                                            const char** outMMProfile, uint32_t* outUnprovedHalves, unsigned* outGoal);

namespace {

// The entries a package can carry, in the order the writer stores them.
const char* const kEntryNames[] = {
    RSBS_SEEDPKG_ENTRY_IDENTITY,   RSBS_SEEDPKG_ENTRY_DIGEST,        RSBS_SEEDPKG_ENTRY_OOT_SPOILER,
    RSBS_SEEDPKG_ENTRY_MM_SPOILER, RSBS_SEEDPKG_ENTRY_COMBO_SPOILER, RSBS_SEEDPKG_ENTRY_OOT_WORLD,
    RSBS_SEEDPKG_ENTRY_MM_HALF,    RSBS_SEEDPKG_ENTRY_COMBO_RECORD,  RSBS_SEEDPKG_ENTRY_CROSSINGS,
};

/**
 * What Generate holds of the completed world besides MM's half: the digest text,
 * MM's frozen profile dump, the unproved halves and the frozen goal.
 * @return 1 when a completed world is held, else 0.
 */
int CompletedExtras(std::string& digest, std::string& mmProfile, uint32_t& unprovedHalves, unsigned& goal) {
    // The completion's own record (ForeignItemsSingleExe.cpp), the one the writer reads.
    const char* digestText = nullptr;
    const char* profileText = nullptr;
    unprovedHalves = 0;
    goal = 0;
    if (!OoT_PairedGen_CompletedWorld(nullptr, nullptr, &digestText, &profileText, &unprovedHalves, &goal)) {
        digest.clear();
        mmProfile.clear();
        return 0;
    }
    digest = digestText != nullptr ? digestText : "";
    mmProfile = profileText != nullptr ? profileText : "";
    return 1;
}

std::string Hex8(uint32_t v) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%08X", (unsigned)v);
    return buf;
}

std::string SizeFnv(const std::string& bytes) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%zu/%08X", bytes.size(), (unsigned)SeedPackage_Fnv1a(bytes.data(), bytes.size()));
    return buf;
}

/** A JSON document in canonical form (keys sorted), as size/FNV-1a. */
std::string CanonFnv(const nlohmann::json& doc) {
    return SizeFnv(doc.dump());
}

/** OoT's settings as name -> value, the way identity.json names them, in canonical form. */
std::string SettingsFnv(const std::map<std::string, std::string>& settings) {
    std::string text;
    for (const auto& [name, value] : settings) {
        text += name + "=" + value + "\n";
    }
    return std::to_string(settings.size()) + "/" + SizeFnv(text);
}

/** The world fields of @p live in an otherwise zeroed record: what a package's
 *  placement/combo-context.bin must hold (no session state an earlier file left). */
std::string WorldOnlyRecord(const ComboContext& live) {
    auto rec = std::make_unique<ComboContext>();
    memset(rec.get(), 0, sizeof(ComboContext));
    memcpy(rec->magic, live.magic, sizeof(rec->magic));
    memcpy(&rec->version, &live.version, sizeof(rec->version));
    memcpy(&rec->sourceIsRando, &live.sourceIsRando, sizeof(rec->sourceIsRando));
    memcpy(&rec->sharedRandoSeed, &live.sharedRandoSeed, sizeof(rec->sharedRandoSeed));
    memcpy(&rec->sharedRandoSettingsHash, &live.sharedRandoSettingsHash, sizeof(rec->sharedRandoSettingsHash));
    memcpy(rec->foreignPlacements, live.foreignPlacements, sizeof(rec->foreignPlacements));
    memcpy(rec->foreignPlacementsOoT, live.foreignPlacementsOoT, sizeof(rec->foreignPlacementsOoT));
    memcpy(&rec->mmProfileDigest, &live.mmProfileDigest, sizeof(rec->mmProfileDigest));
    memcpy(&rec->mmPairedAttempt, &live.mmPairedAttempt, sizeof(rec->mmPairedAttempt));
    memcpy(&rec->comboSettingsHash, &live.comboSettingsHash, sizeof(rec->comboSettingsHash));
    memcpy(&rec->comboSettings, &live.comboSettings, sizeof(rec->comboSettings));
    memcpy(&rec->comboTriforce, &live.comboTriforce, sizeof(rec->comboTriforce));
    memcpy(&rec->comboItemRules, &live.comboItemRules, sizeof(rec->comboItemRules));
    return std::string(reinterpret_cast<const char*>(rec.get()), sizeof(ComboContext));
}

std::string ReadFileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/** The spoiler document gGeneral.SpoilerLog names, as an absolute path ("" when none). */
std::string SpoilerPathFromCVar() {
    std::string rel = CVarGetString(CVAR_GENERAL("SpoilerLog"), "");
    if (rel.empty()) {
        return std::string();
    }
    if (rel.rfind("./", 0) == 0) {
        rel = rel.substr(2);
    }
    return Ship::Context::GetPathRelativeToAppDirectory(rel.c_str());
}

/** The identity terms the report names, in its order, as JSON (null when absent). */
struct Terms {
    std::vector<std::pair<std::string, std::string>> lines;
    void Add(const std::string& key, const std::string& value) {
        lines.emplace_back(key, value);
    }
    void AddJson(const std::string& key, const nlohmann::json& value) {
        Add(key, value.dump());
    }
};

/** @p doc at the '/'-separated @p path, or null. */
nlohmann::json At(const nlohmann::json& doc, const char* path) {
    const nlohmann::json* cur = &doc;
    std::string rest = path;
    while (!rest.empty()) {
        const size_t slash = rest.find('/');
        const std::string key = rest.substr(0, slash);
        rest = slash == std::string::npos ? std::string() : rest.substr(slash + 1);
        if (!cur->is_object() || !cur->contains(key)) {
            return nlohmann::json();
        }
        cur = &(*cur)[key];
    }
    return *cur;
}

// The identity terms, by their identity.json path. Both sides walk this list.
const char* const kComboTerms[] = {
    "combo/masterSeed",
    "combo/comboSettingsHash",
    "combo/record/formatVersion",
    "combo/record/direction",
    "combo/record/poolSizeOoT",
    "combo/record/poolSizeMM",
    "combo/record/itemClassOoT",
    "combo/record/itemClassMM",
    "combo/record/goal",
    "combo/record/logicRung",
    "combo/record/comboFlags",
    "combo/itemRules/crossItemFlags",
    "combo/itemRules/blastMaskCooldown",
    "combo/triforce/totalOoT",
    "combo/triforce/requiredOoT",
    "combo/triforce/totalMM",
    "combo/triforce/requiredMM",
    "combo/mmPairedAttempt",
    "combo/goal",
    "combo/unprovedHalves",
    "combo/giveCapsMM",
    "combo/giveCapsMMPublished",
    "crossings/ootHosted",
    "crossings/mmHosted",
    "crossings/digest",
};

/** The live sources, as the identity terms and entries a package of this world must carry. */
int LiveTerms(Terms& t, std::string& why) {
    auto ctx = Rando::Context::GetInstance();
    if (!ctx) {
        why = "no Rando::Context";
        return 1;
    }
    std::string digest;
    std::string mmProfile;
    uint32_t unproved = 0;
    unsigned goal = 0;
    if (!CompletedExtras(digest, mmProfile, unproved, goal)) {
        why = "no completed paired world is held";
        return 2;
    }
    size_t halfSize = 0;
    const uint8_t* half = OoT_SeedPackage_TestFinishedMMHalf(&halfSize);
    if (half == nullptr || halfSize == 0) {
        why = "the completed world has no MM half";
        return 3;
    }
    const std::string spoilerPath = SpoilerPathFromCVar();
    const std::string spoiler = spoilerPath.empty() ? std::string() : ReadFileBytes(spoilerPath);
    nlohmann::json spoilerDoc;
    try {
        spoilerDoc = nlohmann::json::parse(spoiler);
    } catch (...) {
        why = "the live spoiler '" + spoilerPath + "' is absent or not JSON";
        return 4;
    }
    const nlohmann::json comboSpoiler = At(spoilerDoc, "combo");
    const nlohmann::json mmSpoiler = At(spoilerDoc, "combo/mm");
    // Anti-vacuity: every optional entry exists for this world, so dropping one is seen.
    if (digest.empty() || mmProfile.empty() || !comboSpoiler.is_object() || mmSpoiler.is_null()) {
        why = "the completed world lacks a digest, MM's profile or a split spoiler half, so their entries would "
              "prove nothing";
        return 5;
    }
    nlohmann::json comboRest = comboSpoiler;
    comboRest.erase("mm");
    nlohmann::json profile;
    try {
        profile = nlohmann::json::parse(mmProfile);
    } catch (...) {
        why = "MM's frozen profile is not JSON";
        return 6;
    }

    std::map<std::string, std::string> settings;
    for (int key = 0; key < RSK_MAX; key++) {
        const std::string& name = Rando::Settings::GetInstance()->GetOption((RandomizerSettingKey)key).GetName();
        const nlohmann::json value = ctx->GetOption((RandomizerSettingKey)key).Get();
        settings[name.empty() ? ("RSK " + std::to_string(key)) : name] = value.dump();
    }
    if (settings.empty()) {
        why = "no OoT setting is named";
        return 7;
    }
    std::string seedName;
    nlohmann::json icons = nlohmann::json::array();
    for (size_t i = 0; i < ctx->hashIconIndexes.size(); i++) {
        char part[8];
        snprintf(part, sizeof(part), i == 0 ? "%02u" : "-%02u", (unsigned)ctx->hashIconIndexes[i]);
        seedName += part;
        icons.push_back(ctx->hashIconIndexes[i]);
    }
    if (ctx->GetSeedString().empty() || seedName.empty()) {
        why = "the live world has no seed string or hash icons";
        return 8;
    }

    // Combo terms, by the same paths the package side reads.
    const ComboSettingsRecord& r = gComboCtx.comboSettings;
    nlohmann::json id = nlohmann::json::object();
    id["combo"]["masterSeed"] = gComboCtx.sharedRandoSeed;
    id["combo"]["comboSettingsHash"] = Hex8(gComboCtx.comboSettingsHash);
    id["combo"]["record"]["formatVersion"] = r.formatVersion;
    id["combo"]["record"]["direction"] = r.direction;
    id["combo"]["record"]["poolSizeOoT"] = r.poolSizeOoT;
    id["combo"]["record"]["poolSizeMM"] = r.poolSizeMM;
    id["combo"]["record"]["itemClassOoT"] = r.itemClassOoT;
    id["combo"]["record"]["itemClassMM"] = r.itemClassMM;
    id["combo"]["record"]["goal"] = r.goal;
    id["combo"]["record"]["logicRung"] = r.logicRung;
    id["combo"]["record"]["comboFlags"] = r.comboFlags;
    id["combo"]["itemRules"]["crossItemFlags"] = gComboCtx.comboItemRules.crossItemFlags;
    id["combo"]["itemRules"]["blastMaskCooldown"] = gComboCtx.comboItemRules.blastMaskCooldown;
    id["combo"]["triforce"]["totalOoT"] = gComboCtx.comboTriforce.totalOoT;
    id["combo"]["triforce"]["requiredOoT"] = gComboCtx.comboTriforce.requiredOoT;
    id["combo"]["triforce"]["totalMM"] = gComboCtx.comboTriforce.totalMM;
    id["combo"]["triforce"]["requiredMM"] = gComboCtx.comboTriforce.requiredMM;
    id["combo"]["mmPairedAttempt"] = gComboCtx.mmPairedAttempt;
    id["combo"]["goal"] = goal;
    id["combo"]["unprovedHalves"] = unproved;
    id["combo"]["giveCapsMM"] = Combo_ForeignGiveCaps((uint8_t)GAME_MM);
    id["combo"]["giveCapsMMPublished"] = Combo_ForeignGiveCapsPublished((uint8_t)GAME_MM);
    id["crossings"]["ootHosted"] = Combo_Crossings_Count(GAME_OOT);
    id["crossings"]["mmHosted"] = Combo_Crossings_Count(GAME_MM);
    id["crossings"]["digest"] = Hex8(Combo_Crossings_Digest());

    t.AddJson("id.packageFormat", RSBS_SEED_PACKAGE_FORMAT);
    t.AddJson("id.generatorVersion", RSBS_GENERATOR_VERSION);
    t.AddJson("id.build", RsbsVersion_Display());
    t.AddJson("id.seedString", ctx->GetSeedString());
    t.AddJson("id.seedName", seedName);
    t.AddJson("id.hashIcons", icons);
    t.AddJson("id.ootFinalSeed", ctx->GetSeed());
    t.AddJson("id.oot.settingsHash", Hex8(gComboCtx.sharedRandoSettingsHash));
    t.Add("id.oot.settings", SettingsFnv(settings));
    t.AddJson("id.mm.profileDigest", Hex8(gComboCtx.mmProfileDigest));
    t.Add("id.mm.profile", std::to_string(profile.size()) + "/" + CanonFnv(profile));
    for (const char* path : kComboTerms) {
        t.AddJson(std::string("id.") + path, At(id, path));
    }

    // The entries: which ones, and the bytes of each the live state can name.
    std::vector<std::string> expected = { RSBS_SEEDPKG_ENTRY_IDENTITY,      RSBS_SEEDPKG_ENTRY_DIGEST,
                                          RSBS_SEEDPKG_ENTRY_OOT_SPOILER,   RSBS_SEEDPKG_ENTRY_MM_SPOILER,
                                          RSBS_SEEDPKG_ENTRY_COMBO_SPOILER, RSBS_SEEDPKG_ENTRY_OOT_WORLD,
                                          RSBS_SEEDPKG_ENTRY_MM_HALF,       RSBS_SEEDPKG_ENTRY_COMBO_RECORD,
                                          RSBS_SEEDPKG_ENTRY_CROSSINGS };
    std::string names;
    std::string recorded;
    for (const std::string& n : expected) {
        names += (names.empty() ? "" : ",") + n;
        if (n != RSBS_SEEDPKG_ENTRY_IDENTITY) {
            recorded += (recorded.empty() ? "" : ",") + n;
        }
    }
    t.Add("pkg.entries", names);
    t.Add("id.entries", recorded);
    std::string crossings(Combo_Crossings_SerializedSize(), '\0');
    if (Combo_Crossings_Serialize(reinterpret_cast<uint8_t*>(crossings.data()), crossings.size()) != crossings.size()) {
        why = "the crossing store does not serialize";
        return 9;
    }
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_DIGEST, SizeFnv(digest));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_OOT_SPOILER, SizeFnv(spoiler));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_MM_SPOILER, CanonFnv(mmSpoiler));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_COMBO_SPOILER, CanonFnv(comboRest));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_MM_HALF,
          SizeFnv(std::string(reinterpret_cast<const char*>(half), halfSize)));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_COMBO_RECORD, SizeFnv(WorldOnlyRecord(gComboCtx)));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_CROSSINGS, SizeFnv(crossings));
    return 0;
}

/** The package at @p pkgPath, as the same terms and entries. */
int PackageTerms(const char* pkgPath, Terms& t, std::string& why) {
    std::map<std::string, std::string> entries;
    std::string names;
    for (const char* name : kEntryNames) {
        void* data = nullptr;
        size_t size = 0;
        const int rc = SeedPackage_ReadEntry(pkgPath, name, &data, &size);
        if (rc == RSBS_SEEDPKG_OK) {
            entries[name] = std::string(static_cast<const char*>(data), size);
            names += (names.empty() ? "" : ",") + std::string(name);
        } else if (rc != RSBS_SEEDPKG_ERR_MISSING_ENTRY) {
            free(data);
            why = std::string("reading ") + name + " failed (" + SeedPackage_StatusName(rc) + ")";
            return 1;
        }
        free(data);
    }
    nlohmann::json id;
    try {
        id = nlohmann::json::parse(entries[RSBS_SEEDPKG_ENTRY_IDENTITY]);
    } catch (...) {
        why = "identity.json is absent or not JSON";
        return 2;
    }

    t.AddJson("id.packageFormat", At(id, "packageFormat"));
    t.AddJson("id.generatorVersion", At(id, "generatorVersion"));
    t.AddJson("id.build", At(id, "build"));
    t.AddJson("id.seedString", At(id, "seedString"));
    t.AddJson("id.seedName", At(id, "seedName"));
    t.AddJson("id.hashIcons", At(id, "hashIcons"));
    t.AddJson("id.ootFinalSeed", At(id, "ootFinalSeed"));
    t.AddJson("id.oot.settingsHash", At(id, "oot/settingsHash"));
    {
        std::map<std::string, std::string> settings;
        const nlohmann::json s = At(id, "oot/settings");
        if (s.is_object()) {
            for (const auto& [name, value] : s.items()) {
                settings[name] = value.dump();
            }
        }
        t.Add("id.oot.settings", SettingsFnv(settings));
    }
    t.AddJson("id.mm.profileDigest", At(id, "mm/profileDigest"));
    {
        const nlohmann::json profile = At(id, "mm/profile");
        t.Add("id.mm.profile", std::to_string(profile.size()) + "/" + CanonFnv(profile));
    }
    for (const char* path : kComboTerms) {
        t.AddJson(std::string("id.") + path, At(id, path));
    }

    std::string recorded;
    const nlohmann::json rec = At(id, "entries");
    if (rec.is_object()) {
        // In the writer's order where it is a known entry; anything else after.
        for (const char* name : kEntryNames) {
            if (rec.contains(name)) {
                recorded += (recorded.empty() ? "" : ",") + std::string(name);
            }
        }
        for (const auto& [name, value] : rec.items()) {
            (void)value;
            bool known = false;
            for (const char* k : kEntryNames) {
                known = known || name == k;
            }
            if (!known) {
                recorded += (recorded.empty() ? "" : ",") + name;
            }
        }
    }
    t.Add("pkg.entries", names);
    t.Add("id.entries", recorded);

    auto bytesOf = [&](const char* name) {
        auto it = entries.find(name);
        return it == entries.end() ? std::string("(absent)") : SizeFnv(it->second);
    };
    auto canonOf = [&](const char* name) {
        auto it = entries.find(name);
        if (it == entries.end()) {
            return std::string("(absent)");
        }
        try {
            return CanonFnv(nlohmann::json::parse(it->second));
        } catch (...) { return std::string("(not JSON)"); }
    };
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_DIGEST, bytesOf(RSBS_SEEDPKG_ENTRY_DIGEST));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_OOT_SPOILER, bytesOf(RSBS_SEEDPKG_ENTRY_OOT_SPOILER));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_MM_SPOILER, canonOf(RSBS_SEEDPKG_ENTRY_MM_SPOILER));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_COMBO_SPOILER, canonOf(RSBS_SEEDPKG_ENTRY_COMBO_SPOILER));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_MM_HALF, bytesOf(RSBS_SEEDPKG_ENTRY_MM_HALF));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_COMBO_RECORD, bytesOf(RSBS_SEEDPKG_ENTRY_COMBO_RECORD));
    t.Add(std::string("pkg.") + RSBS_SEEDPKG_ENTRY_CROSSINGS, bytesOf(RSBS_SEEDPKG_ENTRY_CROSSINGS));
    return 0;
}

} // namespace

/**
 * TEST BRIDGE (#1044, SeedPackageRoundTrip): append to @p reportPath one line per
 * identity term and package entry, read from the live completed world when
 * @p pkgPath is NULL or "" (process 1, before the package is written) or from the
 * package at @p pkgPath (process 2). The same keys in the same order either way.
 * @return 0, or nonzero (with the cause on stderr) when the side cannot be read.
 */
extern "C" int OoT_SeedPackage_TestReportTerms(const char* pkgPath, const char* reportPath) {
    Terms t;
    std::string why;
    const bool live = pkgPath == nullptr || pkgPath[0] == '\0';
    const int rc = live ? LiveTerms(t, why) : PackageTerms(pkgPath, t, why);
    if (rc != 0) {
        fprintf(stderr, "[SEED-PACKAGE] the %s terms cannot be reported (%d): %s\n", live ? "live" : "package", rc,
                why.c_str());
        return rc;
    }
    FILE* out = fopen(reportPath, "a");
    if (out == nullptr) {
        return 100;
    }
    for (const auto& [key, value] : t.lines) {
        fprintf(out, "%s=%s\n", key.c_str(), value.c_str());
    }
    fclose(out);
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
