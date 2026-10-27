/**
 * @file SeedPackageSingleExe.cpp
 * @brief OoT's side of the seed package (#1044, src/common/seed_package.h): build
 *        a completed paired world into a package, write and stage it, load and
 *        validate one, and hand file creation what it binds.
 *
 * ONE GAME, FROZEN AT GENERATE. The paired world is completed when the player
 * presses Generate (OoT_PairedGen_CompleteWorld, ForeignItemsSingleExe.cpp): the
 * file select's per-frame tick runs it on the game thread the frame the Generate
 * worker ends. The world is then written to Randomizer/<seed name>-<fingerprint>
 * .rsbs-seed.zip and STAGED. Randomizer > General's "Load seed package" stages a
 * package from that folder the same way, through the same apply. File creation
 * binds the staged package (OoT_RunPairedCreationEvent): it re-applies the frozen
 * identity, records, crossings and give capabilities, stamps the file's name into
 * MM's half and arms it. Nothing at file select reads a CVar that shapes the world.
 *
 * WHAT A STAGE IS. A validated package held in memory (its entries, so an export
 * or a bind never rereads a file a player may have moved) plus the live state it
 * applied: OoT's world in Rando::Context (reloaded from placement/oot-world.json
 * for a loaded package; already live for the one Generate just completed), the
 * world fields of gComboCtx, the crossing store and MM's give capabilities.
 *
 * VALIDATION IS WHOLE BEFORE ANYTHING IS APPLIED, so a refusal changes nothing:
 * identity.json parses and names a package format this build reads and THIS
 * build's generator version; every required entry is present; every entry's size
 * and FNV-1a match what identity.json records; MM's half and the combo record have
 * this build's layout; the crossing block validates; the combo record names the
 * identity identity.json names, and its fingerprint is the one this generator's
 * formula gives (Combo_ComboSettingsHashGeneratorVersionEx).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "soh/OTRGlobals.h"
#include "soh/Notification/Notification.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/randomizer.h" // JoinRandoGenerationThread

#include "context.h"
#include "foreign_items.h"
#include "crossing_store.h"
#include "combo_single_bag.h"      // OoT_ComboLogic_GeneralPassDeferred
#include "pairing_refusal_toast.h" // RSBS_REFUSAL_TOAST_PREFIX
#include "rsbs_version.h"
#include "seed_package.h"

extern "C" {
// ForeignItemsSingleExe.cpp: the completion and the completed world.
int OoT_PairedGen_CompleteWorld(void);
int OoT_PairedGen_CompletedWorld(const uint8_t** outHalf, size_t* outHalfSize, const char** outDigest,
                                 const char** outMMProfile, uint32_t* outUnprovedHalves, unsigned* outGoal);
void OoT_PairedGen_RetractWorld(void);
void OoT_Creation_MarkFreshStagedWorld(void);
// ComboLogicEngineOoT.cpp.
void OoT_ComboLogic_SetGeneralPassDeferred(int deferred);
// games/mm/2s2h/GameExports_SingleExe.cpp: the bytes of MM's half.
size_t MM_SeedPackage_HalfSize(void);
// OTRGlobals.cpp: the seed of the OoT world the process holds.
uint32_t Randomizer_GetCurrentWorldSeed(void);
}
// SaveManager.cpp: OoT's world as a created file saves it.
std::string OoT_SeedPackage_SerializeOoTWorld();
bool OoT_SeedPackage_LoadOoTWorld(const std::string& sectionJson);

namespace {

// The order a package's entries are written in (and the order an export writes).
const char* const kEntryOrder[] = {
    RSBS_SEEDPKG_ENTRY_IDENTITY,   RSBS_SEEDPKG_ENTRY_DIGEST,        RSBS_SEEDPKG_ENTRY_OOT_SPOILER,
    RSBS_SEEDPKG_ENTRY_MM_SPOILER, RSBS_SEEDPKG_ENTRY_COMBO_SPOILER, RSBS_SEEDPKG_ENTRY_OOT_WORLD,
    RSBS_SEEDPKG_ENTRY_MM_HALF,    RSBS_SEEDPKG_ENTRY_COMBO_RECORD,  RSBS_SEEDPKG_ENTRY_CROSSINGS,
};
// What a package must carry to be loaded. The spoilers and the digest are carried
// when the world had them (a spoiler can be refused a name, #1055), and checked
// whenever they are.
const char* const kRequiredEntries[] = {
    RSBS_SEEDPKG_ENTRY_OOT_WORLD,
    RSBS_SEEDPKG_ENTRY_MM_HALF,
    RSBS_SEEDPKG_ENTRY_COMBO_RECORD,
    RSBS_SEEDPKG_ENTRY_CROSSINGS,
};

struct StagedPackage {
    std::string path; // "" when the package exists only in memory (its write failed)
    rsbs::seedpkg::Entries entries;
    uint32_t generatorVersion = 0;
    uint32_t ootFinalSeed = 0;
    uint32_t unprovedHalves = 0;
    uint32_t giveCaps = 0;
    bool giveCapsPublished = false;
    std::string seedString;
    std::string seedName;
    uint32_t fingerprint = 0;
};

// The staged package. Read and written on the game thread; unstaged from the
// Generate worker too (Playthrough_Init), hence the lock.
std::mutex sStageMutex;
std::unique_ptr<StagedPackage> sStaged;
std::string sStagedPathText; // StagedPath's storage; only the game thread writes it
std::atomic<int> sLastStatus{ RSBS_SEEDPKG_OK };
std::atomic<int> sWorkerFinished{ 0 };

std::string Hex8(uint32_t v) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%08X", (unsigned)v);
    return buf;
}

bool ParseHex8(const nlohmann::json& j, uint32_t& out) {
    if (!j.is_string()) {
        return false;
    }
    const std::string s = j.get<std::string>();
    if (s.size() != 8 || s.find_first_not_of("0123456789ABCDEFabcdef") != std::string::npos) {
        return false;
    }
    out = (uint32_t)strtoul(s.c_str(), nullptr, 16);
    return true;
}

bool ParseU32(const nlohmann::json& j, uint32_t& out) {
    if (!j.is_number_unsigned()) {
        return false;
    }
    out = j.get<uint32_t>();
    return true;
}

std::string RandomizerDir() {
    return Ship::Context::GetPathRelativeToAppDirectory("Randomizer");
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

std::string ReadFileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string SeedNameOf(const Rando::Context& ctx) {
    std::string name;
    for (size_t i = 0; i < ctx.hashIconIndexes.size(); i++) {
        char part[8];
        snprintf(part, sizeof(part), i == 0 ? "%02u" : "-%02u", (unsigned)ctx.hashIconIndexes[i]);
        name += part;
    }
    return name;
}

// ----------------------------------------------------------------------------
// Building a package from the completed world
// ----------------------------------------------------------------------------

/**
 * placement/combo-context.bin: the WORLD fields of @p live (the ones ApplyComboWorld
 * applies, plus the layout's magic and version) in an otherwise zeroed record. At
 * Generate the session fields (the switch request, shared flags and items, the
 * slot, the grant cursors, the shared pools, the commit count, the family bits)
 * can still hold what an earlier opened file left, and none of it is the world:
 * copied raw, a package's bytes, and identity.json's digest of them, would depend
 * on the session's history.
 */
std::string WorldRecordBytes(const ComboContext& live) {
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

int BuildEntries(std::vector<std::pair<std::string, std::string>>& out, std::string& outFileName) {
    out.clear();
    const uint8_t* half = nullptr;
    size_t halfSize = 0;
    const char* digest = nullptr;
    const char* mmProfile = nullptr;
    uint32_t unproved = 0;
    unsigned goal = 0;
    if (!OoT_PairedGen_CompletedWorld(&half, &halfSize, &digest, &mmProfile, &unproved, &goal) || half == nullptr ||
        halfSize == 0) {
        return RSBS_SEEDPKG_ERR_NO_WORLD;
    }
    auto ctx = Rando::Context::GetInstance();
    if (!ctx) {
        return RSBS_SEEDPKG_ERR_NO_WORLD;
    }
    const std::string seedName = SeedNameOf(*ctx);

    // The spoilers: today's document as written (OoT's with MM's half and the
    // crossings joined in as "combo"), and its two halves apart.
    std::string ootSpoiler;
    std::string mmSpoiler;
    std::string comboSpoiler;
    const std::string spoilerPath = SpoilerPathFromCVar();
    if (!spoilerPath.empty()) {
        ootSpoiler = ReadFileBytes(spoilerPath);
    }
    if (!ootSpoiler.empty()) {
        try {
            nlohmann::ordered_json doc = nlohmann::ordered_json::parse(ootSpoiler);
            if (doc.contains("combo") && doc["combo"].is_object()) {
                nlohmann::ordered_json combo = doc["combo"];
                if (combo.contains("mm")) {
                    mmSpoiler = combo["mm"].dump(4) + "\n";
                    combo.erase("mm");
                }
                comboSpoiler = combo.dump(4) + "\n";
            }
        } catch (...) {
            fprintf(stderr, "[OoT] seed package: the spoiler '%s' is not JSON; its halves are not split out\n",
                    spoilerPath.c_str());
        }
    }

    // The placements.
    const std::string ootWorld = OoT_SeedPackage_SerializeOoTWorld();
    const std::string mmHalf(reinterpret_cast<const char*>(half), halfSize);
    const std::string comboRecord = WorldRecordBytes(gComboCtx);
    std::string crossings(Combo_Crossings_SerializedSize(), '\0');
    if (Combo_Crossings_Serialize(reinterpret_cast<uint8_t*>(crossings.data()), crossings.size()) != crossings.size()) {
        return RSBS_SEEDPKG_ERR_NO_WORLD;
    }

    std::vector<std::pair<std::string, std::string>> body;
    if (digest != nullptr && digest[0] != '\0') {
        body.emplace_back(RSBS_SEEDPKG_ENTRY_DIGEST, digest);
    }
    if (!ootSpoiler.empty()) {
        body.emplace_back(RSBS_SEEDPKG_ENTRY_OOT_SPOILER, ootSpoiler);
    }
    if (!mmSpoiler.empty()) {
        body.emplace_back(RSBS_SEEDPKG_ENTRY_MM_SPOILER, mmSpoiler);
    }
    if (!comboSpoiler.empty()) {
        body.emplace_back(RSBS_SEEDPKG_ENTRY_COMBO_SPOILER, comboSpoiler);
    }
    body.emplace_back(RSBS_SEEDPKG_ENTRY_OOT_WORLD, ootWorld);
    body.emplace_back(RSBS_SEEDPKG_ENTRY_MM_HALF, mmHalf);
    body.emplace_back(RSBS_SEEDPKG_ENTRY_COMBO_RECORD, comboRecord);
    body.emplace_back(RSBS_SEEDPKG_ENTRY_CROSSINGS, crossings);

    // THE IDENTITY. Self-describing: what a player who receives the package reads
    // before creating a file (the seed, the fingerprint, both games' settings and
    // the generator), and what the load checks.
    nlohmann::ordered_json id = nlohmann::ordered_json::object();
    id["packageFormat"] = RSBS_SEED_PACKAGE_FORMAT;
    id["generatorVersion"] = RSBS_GENERATOR_VERSION;
    id["build"] = RsbsVersion_Display();
    id["seedString"] = ctx->GetSeedString();
    id["seedName"] = seedName;
    nlohmann::ordered_json icons = nlohmann::ordered_json::array();
    for (uint8_t icon : ctx->hashIconIndexes) {
        icons.push_back(icon);
    }
    id["hashIcons"] = icons;
    id["ootFinalSeed"] = ctx->GetSeed();
    {
        nlohmann::ordered_json settings = nlohmann::ordered_json::object();
        for (int key = 0; key < RSK_MAX; key++) {
            const std::string& name = Rando::Settings::GetInstance()->GetOption((RandomizerSettingKey)key).GetName();
            settings[name.empty() ? ("RSK " + std::to_string(key)) : name] =
                ctx->GetOption((RandomizerSettingKey)key).Get();
        }
        id["oot"] = { { "settingsHash", Hex8(gComboCtx.sharedRandoSettingsHash) }, { "settings", settings } };
    }
    {
        nlohmann::ordered_json profile = nlohmann::ordered_json::object();
        try {
            profile = nlohmann::ordered_json::parse(mmProfile != nullptr ? mmProfile : "{}");
        } catch (...) {}
        id["mm"] = { { "profileDigest", Hex8(gComboCtx.mmProfileDigest) }, { "profile", profile } };
    }
    {
        const ComboSettingsRecord& r = gComboCtx.comboSettings;
        nlohmann::ordered_json combo = nlohmann::ordered_json::object();
        combo["masterSeed"] = gComboCtx.sharedRandoSeed;
        combo["comboSettingsHash"] = Hex8(gComboCtx.comboSettingsHash);
        combo["record"] = {
            { "formatVersion", r.formatVersion },
            { "direction", r.direction },
            { "poolSizeOoT", r.poolSizeOoT },
            { "poolSizeMM", r.poolSizeMM },
            { "itemClassOoT", r.itemClassOoT },
            { "itemClassMM", r.itemClassMM },
            { "goal", r.goal },
            { "logicRung", r.logicRung },
            { "comboFlags", r.comboFlags },
        };
        combo["itemRules"] = { { "crossItemFlags", gComboCtx.comboItemRules.crossItemFlags },
                               { "blastMaskCooldown", gComboCtx.comboItemRules.blastMaskCooldown } };
        combo["triforce"] = { { "totalOoT", gComboCtx.comboTriforce.totalOoT },
                              { "requiredOoT", gComboCtx.comboTriforce.requiredOoT },
                              { "totalMM", gComboCtx.comboTriforce.totalMM },
                              { "requiredMM", gComboCtx.comboTriforce.requiredMM } };
        combo["mmPairedAttempt"] = gComboCtx.mmPairedAttempt;
        combo["goal"] = goal;
        combo["unprovedHalves"] = unproved;
        combo["giveCapsMM"] = Combo_ForeignGiveCaps((uint8_t)GAME_MM);
        combo["giveCapsMMPublished"] = Combo_ForeignGiveCapsPublished((uint8_t)GAME_MM);
        id["combo"] = combo;
    }
    id["crossings"] = { { "ootHosted", Combo_Crossings_Count(GAME_OOT) },
                        { "mmHosted", Combo_Crossings_Count(GAME_MM) },
                        { "digest", Hex8(Combo_Crossings_Digest()) } };
    nlohmann::ordered_json entries = nlohmann::ordered_json::object();
    for (const auto& [name, bytes] : body) {
        entries[name] = { { "size", bytes.size() }, { "fnv1a", Hex8(SeedPackage_Fnv1a(bytes.data(), bytes.size())) } };
    }
    id["entries"] = entries;

    out.emplace_back(RSBS_SEEDPKG_ENTRY_IDENTITY, id.dump(2) + "\n");
    for (auto& entry : body) {
        out.push_back(std::move(entry));
    }

    char fileName[256];
    if (SeedPackage_FileName(seedName.c_str(), gComboCtx.comboSettingsHash, fileName, sizeof(fileName)) == 0) {
        return RSBS_SEEDPKG_ERR_IO;
    }
    outFileName = fileName;
    return RSBS_SEEDPKG_OK;
}

// ----------------------------------------------------------------------------
// Validation (nothing is applied until all of it passes)
// ----------------------------------------------------------------------------

int Validate(const rsbs::seedpkg::Entries& entries, nlohmann::json& id, std::string& why) {
    auto identityIt = entries.find(RSBS_SEEDPKG_ENTRY_IDENTITY);
    if (identityIt == entries.end()) {
        why = "the package has no identity.json";
        return RSBS_SEEDPKG_ERR_MISSING_ENTRY;
    }
    try {
        id = nlohmann::json::parse(identityIt->second);
    } catch (...) {
        why = "identity.json is not JSON";
        return RSBS_SEEDPKG_ERR_IDENTITY;
    }
    if (!id.is_object()) {
        why = "identity.json is not an object";
        return RSBS_SEEDPKG_ERR_IDENTITY;
    }
    uint32_t format = 0;
    if (!id.contains("packageFormat") || !ParseU32(id["packageFormat"], format) || format == 0) {
        why = "identity.json names no package format";
        return RSBS_SEEDPKG_ERR_IDENTITY;
    }
    if (format > RSBS_SEED_PACKAGE_FORMAT) {
        why = "the package format " + std::to_string(format) + " is newer than this build reads (" +
              std::to_string(RSBS_SEED_PACKAGE_FORMAT) + ")";
        return RSBS_SEEDPKG_ERR_FORMAT;
    }
    // THE GENERATOR (#1003). A package carries a finished world; a build whose
    // generator differs would play that world under rules it does not share. The
    // first check, so a package from another generator is refused for that, before
    // anything about its contents is read.
    uint32_t generator = 0;
    if (!id.contains("generatorVersion") || !ParseU32(id["generatorVersion"], generator)) {
        why = "identity.json names no generator version";
        return RSBS_SEEDPKG_ERR_IDENTITY;
    }
    if (SeedPackage_CheckGeneratorVersion(generator) != RSBS_SEEDPKG_OK) {
        why = "the package was generated by generator " + std::to_string(generator) + "; this build is generator " +
              std::to_string(RSBS_GENERATOR_VERSION);
        return RSBS_SEEDPKG_ERR_GENERATOR_VERSION;
    }
    for (const char* name : kRequiredEntries) {
        if (entries.find(name) == entries.end()) {
            why = std::string("the package has no ") + name;
            return RSBS_SEEDPKG_ERR_MISSING_ENTRY;
        }
    }
    if (!id.contains("entries") || !id["entries"].is_object()) {
        why = "identity.json records no entry digests";
        return RSBS_SEEDPKG_ERR_IDENTITY;
    }
    for (const auto& [name, bytes] : entries) {
        if (name == RSBS_SEEDPKG_ENTRY_IDENTITY) {
            continue;
        }
        const nlohmann::json& rec = id["entries"].contains(name) ? id["entries"][name] : nlohmann::json();
        uint32_t size = 0;
        uint32_t fnv = 0;
        if (!rec.is_object() || !rec.contains("size") || !ParseU32(rec["size"], size) || !rec.contains("fnv1a") ||
            !ParseHex8(rec["fnv1a"], fnv)) {
            why = "identity.json records nothing for " + name;
            return RSBS_SEEDPKG_ERR_INTEGRITY;
        }
        if (size != bytes.size() || fnv != SeedPackage_Fnv1a(bytes.data(), bytes.size())) {
            why = name + " is not the bytes identity.json records";
            return RSBS_SEEDPKG_ERR_INTEGRITY;
        }
    }
    for (const auto& [name, rec] : id["entries"].items()) {
        (void)rec;
        if (entries.find(name) == entries.end()) {
            why = "identity.json records " + name + ", which the package does not carry";
            return RSBS_SEEDPKG_ERR_MISSING_ENTRY;
        }
    }

    // The layouts: MM's half and the combo record are this build's structures.
    if (entries.at(RSBS_SEEDPKG_ENTRY_MM_HALF).size() != MM_SeedPackage_HalfSize()) {
        why = "MM's half is " + std::to_string(entries.at(RSBS_SEEDPKG_ENTRY_MM_HALF).size()) +
              " bytes; this build's is " + std::to_string(MM_SeedPackage_HalfSize());
        return RSBS_SEEDPKG_ERR_LAYOUT;
    }
    const std::string& comboBytes = entries.at(RSBS_SEEDPKG_ENTRY_COMBO_RECORD);
    if (comboBytes.size() != sizeof(ComboContext)) {
        why = "the combo record is " + std::to_string(comboBytes.size()) + " bytes; this build's is " +
              std::to_string(sizeof(ComboContext));
        return RSBS_SEEDPKG_ERR_LAYOUT;
    }
    const std::string& crossingBytes = entries.at(RSBS_SEEDPKG_ENTRY_CROSSINGS);
    if (Combo_Crossings_ValidateBlock(reinterpret_cast<const uint8_t*>(crossingBytes.data()), crossingBytes.size()) !=
        RSBS_CROSSING_OK) {
        why = "the crossing block does not validate";
        return RSBS_SEEDPKG_ERR_INTEGRITY;
    }

    // THE IDENTITY, TWICE OVER: the combo record names the pair identity.json
    // names, and its fingerprint is this generator's formula over it.
    ComboContext rec;
    memcpy(&rec, comboBytes.data(), sizeof(ComboContext));
    uint32_t masterSeed = 0;
    uint32_t settingsHash = 0;
    uint32_t profileDigest = 0;
    uint32_t fingerprint = 0;
    uint32_t ootFinalSeed = 0;
    if (!id.contains("combo") || !id["combo"].is_object() || !id["combo"].contains("masterSeed") ||
        !ParseU32(id["combo"]["masterSeed"], masterSeed) || !id["combo"].contains("comboSettingsHash") ||
        !ParseHex8(id["combo"]["comboSettingsHash"], fingerprint) || !id.contains("oot") ||
        !id["oot"].contains("settingsHash") || !ParseHex8(id["oot"]["settingsHash"], settingsHash) ||
        !id.contains("mm") || !id["mm"].contains("profileDigest") ||
        !ParseHex8(id["mm"]["profileDigest"], profileDigest) || !id.contains("ootFinalSeed") ||
        !ParseU32(id["ootFinalSeed"], ootFinalSeed)) {
        why = "identity.json omits a term of the pair's identity";
        return RSBS_SEEDPKG_ERR_IDENTITY;
    }
    if (!rec.sourceIsRando || rec.sharedRandoSeed == 0 || rec.sharedRandoSeed != masterSeed ||
        rec.sharedRandoSettingsHash != settingsHash || rec.mmProfileDigest != profileDigest ||
        rec.comboSettingsHash != fingerprint || rec.comboSettings.formatVersion == 0) {
        why = "the combo record does not name the identity identity.json names";
        return RSBS_SEEDPKG_ERR_INTEGRITY;
    }
    const int stampedBy =
        Combo_ComboSettingsHashGeneratorVersionEx(&rec.comboSettings, &rec.comboItemRules, rec.sharedRandoSettingsHash,
                                                  rec.mmProfileDigest, rec.comboSettingsHash);
    if (stampedBy != (int)RSBS_GENERATOR_VERSION) {
        why =
            "the pair's fingerprint " + Hex8(rec.comboSettingsHash) + " is not this generator's (" +
            (stampedBy < 0 ? std::string("no generator this build knows") : "generator " + std::to_string(stampedBy)) +
            ")";
        return RSBS_SEEDPKG_ERR_GENERATOR_VERSION;
    }
    return RSBS_SEEDPKG_OK;
}

// ----------------------------------------------------------------------------
// Applying a validated package
// ----------------------------------------------------------------------------

/** The world fields of the frozen combo record, and nothing of the session's. */
void ApplyComboWorld(const ComboContext& rec) {
    gComboCtx.sourceIsRando = rec.sourceIsRando;
    gComboCtx.sharedRandoSeed = rec.sharedRandoSeed;
    gComboCtx.sharedRandoSettingsHash = rec.sharedRandoSettingsHash;
    gComboCtx.mmProfileDigest = rec.mmProfileDigest;
    gComboCtx.mmPairedAttempt = rec.mmPairedAttempt;
    memcpy(gComboCtx.foreignPlacements, rec.foreignPlacements, sizeof(gComboCtx.foreignPlacements));
    memcpy(gComboCtx.foreignPlacementsOoT, rec.foreignPlacementsOoT, sizeof(gComboCtx.foreignPlacementsOoT));
    ComboCreationRecords records;
    records.comboSettings = rec.comboSettings;
    records.comboSettingsHash = rec.comboSettingsHash;
    records.comboTriforce = rec.comboTriforce;
    records.comboItemRules = rec.comboItemRules;
    Combo_RestoreCreationRecords(&records);
}

/** The combo world, the crossings and the give capabilities of @p p. */
int ApplyComboState(const StagedPackage& p) {
    ComboContext rec;
    memcpy(&rec, p.entries.at(RSBS_SEEDPKG_ENTRY_COMBO_RECORD).data(), sizeof(ComboContext));
    ApplyComboWorld(rec);
    const std::string& crossings = p.entries.at(RSBS_SEEDPKG_ENTRY_CROSSINGS);
    if (Combo_Crossings_LoadBlock(reinterpret_cast<const uint8_t*>(crossings.data()), crossings.size()) !=
        RSBS_CROSSING_OK) {
        return RSBS_SEEDPKG_ERR_APPLY;
    }
    Combo_ClearForeignGiveCaps();
    if (p.giveCapsPublished) {
        Combo_PublishForeignGiveCaps((uint8_t)GAME_MM, p.giveCaps);
    }
    return RSBS_SEEDPKG_OK;
}

/** OoT's world from the package, into a fresh context, marked as a staged generation. */
bool LoadOoTWorld(const StagedPackage& p) {
    if (!OoT_SeedPackage_LoadOoTWorld(p.entries.at(RSBS_SEEDPKG_ENTRY_OOT_WORLD))) {
        return false;
    }
    auto ctx = Rando::Context::GetInstance();
    return ctx && ctx->GetSeed() == p.ootFinalSeed;
}

/** The package's spoiler as the document the spoiler windows read: written beside
 *  the package as <package stem>.json unless gGeneral.SpoilerLog already names
 *  those bytes. */
void StageSpoiler(const StagedPackage& p) {
    auto it = p.entries.find(RSBS_SEEDPKG_ENTRY_OOT_SPOILER);
    if (it == p.entries.end()) {
        return;
    }
    const std::string current = SpoilerPathFromCVar();
    if (!current.empty() && ReadFileBytes(current) == it->second) {
        return;
    }
    std::string stem = std::filesystem::path(p.path).filename().string();
    const std::string ext = RSBS_SEED_PACKAGE_EXTENSION;
    if (stem.size() > ext.size() && stem.compare(stem.size() - ext.size(), ext.size(), ext) == 0) {
        stem = stem.substr(0, stem.size() - ext.size());
    }
    if (stem.empty()) {
        stem = p.seedName + "-" + Hex8(p.fingerprint);
    }
    std::error_code ec;
    std::filesystem::create_directories(RandomizerDir(), ec);
    const std::string target = RandomizerDir() + "/" + stem + ".json";
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        fprintf(stderr, "[OoT] seed package: could not write its spoiler to '%s'\n", target.c_str());
        return;
    }
    out << it->second;
    out.close();
    CVarSetString(CVAR_GENERAL("SpoilerLog"), ("./Randomizer/" + stem + ".json").c_str());
}

/**
 * STAGE @p p: OoT's world (reloaded from the package when @p reloadOoTWorld; the
 * world Generate just completed is already live), the combo state, the spoiler,
 * and the file select's view of a staged generation. On an apply failure the
 * half-applied world is retracted and nothing is staged.
 */
int Stage(std::unique_ptr<StagedPackage> p, bool reloadOoTWorld) {
    if (reloadOoTWorld && !LoadOoTWorld(*p)) {
        fprintf(stderr, "[OoT] seed package: OoT's world in '%s' could not be loaded\n", p->path.c_str());
        OoT_PairedGen_RetractWorld();
        return RSBS_SEEDPKG_ERR_APPLY;
    }
    if (ApplyComboState(*p) != RSBS_SEEDPKG_OK) {
        OoT_PairedGen_RetractWorld();
        return RSBS_SEEDPKG_ERR_APPLY;
    }
    if (reloadOoTWorld) {
        StageSpoiler(*p);
    }
    auto ctx = Rando::Context::GetInstance();
    ctx->SetSeedGenerated(true);
    ctx->SetSpoilerLoaded(false);
    OoT_ComboLogic_SetGeneralPassDeferred(0);
    OoT_Creation_MarkFreshStagedWorld();
    fprintf(stderr, "[OoT] seed package: STAGED %s (seed %s, fingerprint %s, generator %u) — file creation binds it\n",
            p->path.empty() ? "(in memory)" : p->path.c_str(), p->seedName.c_str(), Hex8(p->fingerprint).c_str(),
            p->generatorVersion);
    fflush(stderr);
    {
        std::lock_guard<std::mutex> lock(sStageMutex);
        sStagedPathText = p->path;
        sStaged = std::move(p);
    }
    return RSBS_SEEDPKG_OK;
}

std::unique_ptr<StagedPackage> DescribeValidated(const std::string& path, rsbs::seedpkg::Entries&& entries,
                                                 const nlohmann::json& id) {
    auto p = std::make_unique<StagedPackage>();
    p->path = path;
    p->entries = std::move(entries);
    ParseU32(id["generatorVersion"], p->generatorVersion);
    ParseU32(id["ootFinalSeed"], p->ootFinalSeed);
    ParseHex8(id["combo"]["comboSettingsHash"], p->fingerprint);
    if (id["combo"].contains("unprovedHalves")) {
        ParseU32(id["combo"]["unprovedHalves"], p->unprovedHalves);
    }
    if (id["combo"].contains("giveCapsMM")) {
        ParseU32(id["combo"]["giveCapsMM"], p->giveCaps);
    }
    p->giveCapsPublished = id["combo"].contains("giveCapsMMPublished") &&
                           id["combo"]["giveCapsMMPublished"].is_boolean() &&
                           id["combo"]["giveCapsMMPublished"].get<bool>();
    if (id.contains("seedString") && id["seedString"].is_string()) {
        p->seedString = id["seedString"].get<std::string>();
    }
    if (id.contains("seedName") && id["seedName"].is_string()) {
        p->seedName = id["seedName"].get<std::string>();
    }
    return p;
}

int Finish(int status) {
    sLastStatus = status;
    return status;
}

/**
 * Complete the waiting paired world, write its package to the Randomizer folder
 * and stage it. @p fromGenerate raises the failure toast (the creation's fallback
 * raises its own).
 */
int CompleteAndStage(bool fromGenerate) {
    const int completed = OoT_PairedGen_CompleteWorld();
    if (completed == RSBS_SEEDPKG_ERR_NO_WORLD) {
        return 0;
    }
    auto ctx = Rando::Context::GetInstance();
    if (completed != 0) {
        ctx->SetSeedGenerated(false);
        if (fromGenerate) {
            Notification::Emit(
                { .prefix = "Not generated:", .message = "try a new seed or other options.", .mute = true });
        }
        return Finish(RSBS_SEEDPKG_ERR_GENERATION);
    }

    std::vector<std::pair<std::string, std::string>> ordered;
    std::string fileName;
    int rc = BuildEntries(ordered, fileName);
    if (rc != RSBS_SEEDPKG_OK) {
        fprintf(stderr, "[OoT] seed package: the completed world could not be packaged (%s)\n",
                SeedPackage_StatusName(rc));
        OoT_PairedGen_RetractWorld();
        ctx->SetSeedGenerated(false);
        return Finish(rc);
    }
    std::string path = RandomizerDir() + "/" + fileName;
    const int written = rsbs::seedpkg::WriteAll(path, ordered);
    if (written != RSBS_SEEDPKG_OK) {
        // The world is complete and the creation binds it from memory; only sharing
        // it needs the file. Loud, and the status says so.
        fprintf(stderr, "[OoT] seed package: could not write '%s' (%s); it is staged in memory only\n", path.c_str(),
                SeedPackage_StatusName(written));
        path.clear();
    }
    rsbs::seedpkg::Entries entries;
    for (auto& [name, bytes] : ordered) {
        entries[name] = std::move(bytes);
    }
    // The package this build just wrote must pass the load's own checks: one
    // validator for both directions.
    nlohmann::json id;
    std::string why;
    rc = Validate(entries, id, why);
    if (rc != RSBS_SEEDPKG_OK) {
        fprintf(stderr, "[OoT] seed package: the package written for this world fails its own validation (%s: %s)\n",
                SeedPackage_StatusName(rc), why.c_str());
        OoT_PairedGen_RetractWorld();
        ctx->SetSeedGenerated(false);
        return Finish(rc);
    }
    rc = Stage(DescribeValidated(path, std::move(entries), id), /*reloadOoTWorld=*/false);
    if (rc != RSBS_SEEDPKG_OK) {
        ctx->SetSeedGenerated(false);
        return Finish(rc);
    }
    Finish(written == RSBS_SEEDPKG_OK ? RSBS_SEEDPKG_OK : written);
    return 1;
}

} // namespace

// ----------------------------------------------------------------------------
// The entry points (seed_package.h)
// ----------------------------------------------------------------------------

extern "C" void OoT_SeedPackage_MarkWorkerFinished(void) {
    sWorkerFinished = 1;
}

extern "C" int OoT_SeedPackage_CompletePendingGeneration(void) {
    // Generate's world, and only Generate's: the context must still hold the world
    // the identity names (a slot opened since then replaced it).
    auto ctx = Rando::Context::GetInstance();
    if (!ctx || !ctx->IsSeedGenerated() || !Combo_ForeignPairingActive() || OoT_ComboLogic_GeneralPassDeferred() == 0 ||
        Randomizer_GetCurrentWorldSeed() != gComboCtx.sharedRandoSeed) {
        return 0;
    }
    return CompleteAndStage(/*fromGenerate=*/true);
}

/** ForeignItemsSingleExe.cpp's creation fallback, in a test process only (a headless
 *  generation no file-select frame completed): the same completion, no Generate toast. */
extern "C" int OoT_SeedPackage_CompleteForCreation(void) {
    if (!Combo_ForeignPairingActive() || OoT_ComboLogic_GeneralPassDeferred() == 0) {
        return 0;
    }
    return CompleteAndStage(/*fromGenerate=*/false);
}

extern "C" int OoT_SeedPackage_FileSelectTick(void) {
    if (sWorkerFinished.exchange(0) == 0) {
        return 0;
    }
    // The worker has finished its last statement; reap it before running on.
    JoinRandoGenerationThread();
    return OoT_SeedPackage_CompletePendingGeneration();
}

extern "C" int OoT_SeedPackage_LoadFile(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return Finish(RSBS_SEEDPKG_ERR_IO);
    }
    rsbs::seedpkg::Entries entries;
    int rc = rsbs::seedpkg::ReadAll(path, entries);
    if (rc != RSBS_SEEDPKG_OK) {
        fprintf(stderr, "[OoT] seed package: '%s' REFUSED — it could not be read (%s)\n", path,
                SeedPackage_StatusName(rc));
        return Finish(rc);
    }
    nlohmann::json id;
    std::string why;
    rc = Validate(entries, id, why);
    if (rc != RSBS_SEEDPKG_OK) {
        fprintf(stderr, "[OoT] seed package: '%s' REFUSED (%s) — %s; nothing was staged\n", path,
                SeedPackage_StatusName(rc), why.c_str());
        fflush(stderr);
        return Finish(rc);
    }
    rc = Stage(DescribeValidated(path, std::move(entries), id), /*reloadOoTWorld=*/true);
    if (rc != RSBS_SEEDPKG_OK) {
        Rando::Context::GetInstance()->SetSeedGenerated(false);
    }
    return Finish(rc);
}

extern "C" int OoT_SeedPackage_BindForCreation(const uint8_t** outHalf, size_t* outSize, uint32_t* outUnprovedHalves) {
    std::lock_guard<std::mutex> lock(sStageMutex);
    if (!sStaged) {
        return RSBS_SEEDPKG_ERR_NO_WORLD;
    }
    // THE DIVERGENCE CHECK at the bind: the package's generator against the build's
    // (a package staged under another build cannot reach here, and this keeps it so).
    if (SeedPackage_CheckGeneratorVersion(sStaged->generatorVersion) != RSBS_SEEDPKG_OK) {
        return RSBS_SEEDPKG_ERR_GENERATOR_VERSION;
    }
    // The live OoT world must be the package's; a context replaced since the stage
    // is put back from the package rather than bound under the wrong world.
    if (Randomizer_GetCurrentWorldSeed() != sStaged->ootFinalSeed) {
        fprintf(stderr, "[OoT] seed package: the live OoT world is not the staged package's; reloading it\n");
        if (!LoadOoTWorld(*sStaged)) {
            return RSBS_SEEDPKG_ERR_APPLY;
        }
        Rando::Context::GetInstance()->SetSeedGenerated(true);
    }
    // The file-create invalidation may have dropped part of the identity (a DROP
    // for a seed it could not match, the generation-authored attempt), and a
    // package outlives the session that loaded it: the bind re-applies it all.
    const int applied = ApplyComboState(*sStaged);
    if (applied != RSBS_SEEDPKG_OK) {
        return applied;
    }
    const std::string& half = sStaged->entries.at(RSBS_SEEDPKG_ENTRY_MM_HALF);
    if (outHalf != nullptr) {
        *outHalf = reinterpret_cast<const uint8_t*>(half.data());
    }
    if (outSize != nullptr) {
        *outSize = half.size();
    }
    if (outUnprovedHalves != nullptr) {
        *outUnprovedHalves = sStaged->unprovedHalves;
    }
    return RSBS_SEEDPKG_OK;
}

extern "C" int OoT_SeedPackage_Staged(void) {
    std::lock_guard<std::mutex> lock(sStageMutex);
    return sStaged ? 1 : 0;
}

extern "C" const char* OoT_SeedPackage_StagedPath(void) {
    std::lock_guard<std::mutex> lock(sStageMutex);
    return sStaged ? sStagedPathText.c_str() : "";
}

extern "C" int OoT_SeedPackage_StagedDigest(char* out, size_t cap) {
    std::lock_guard<std::mutex> lock(sStageMutex);
    if (out == nullptr || cap == 0) {
        return -1;
    }
    out[0] = '\0';
    if (!sStaged) {
        return -1;
    }
    auto it = sStaged->entries.find(RSBS_SEEDPKG_ENTRY_DIGEST);
    const std::string digest = it != sStaged->entries.end() ? it->second : std::string();
    if (digest.size() + 1 > cap) {
        return -1;
    }
    memcpy(out, digest.c_str(), digest.size() + 1);
    return (int)digest.size();
}

extern "C" int OoT_SeedPackage_StagedSummary(char* out, size_t cap) {
    std::lock_guard<std::mutex> lock(sStageMutex);
    if (out == nullptr || cap == 0) {
        return -1;
    }
    out[0] = '\0';
    if (!sStaged) {
        return -1;
    }
    const std::string file =
        sStaged->path.empty() ? std::string("(not written)") : std::filesystem::path(sStaged->path).filename().string();
    const int n = snprintf(out, cap, "%s  Seed: %s  Fingerprint: %s  Generator: %u", file.c_str(),
                           sStaged->seedString.c_str(), Hex8(sStaged->fingerprint).c_str(), sStaged->generatorVersion);
    return n < 0 ? -1 : n;
}

extern "C" int OoT_SeedPackage_ExportStaged(const char* destinationPath) {
    if (destinationPath == nullptr || destinationPath[0] == '\0') {
        return Finish(RSBS_SEEDPKG_ERR_IO);
    }
    std::vector<std::pair<std::string, std::string>> ordered;
    {
        std::lock_guard<std::mutex> lock(sStageMutex);
        if (!sStaged) {
            return Finish(RSBS_SEEDPKG_ERR_NO_WORLD);
        }
        for (const char* name : kEntryOrder) {
            auto it = sStaged->entries.find(name);
            if (it != sStaged->entries.end()) {
                ordered.emplace_back(it->first, it->second);
            }
        }
    }
    const int rc = rsbs::seedpkg::WriteAll(destinationPath, ordered);
    if (rc == RSBS_SEEDPKG_OK) {
        fprintf(stderr, "[OoT] seed package: exported the staged package to '%s'\n", destinationPath);
    }
    return Finish(rc);
}

extern "C" void OoT_SeedPackage_Unstage(void) {
    std::lock_guard<std::mutex> lock(sStageMutex);
    sStaged.reset();
}

extern "C" int OoT_SeedPackage_LastStatus(void) {
    return sLastStatus.load();
}

extern "C" int OoT_SeedPackage_TestCompleteWorld(void) {
    return OoT_PairedGen_CompleteWorld();
}

extern "C" int OoT_SeedPackage_TestWritePackage(const char* path) {
    std::vector<std::pair<std::string, std::string>> ordered;
    std::string fileName;
    const int rc = BuildEntries(ordered, fileName);
    if (rc != RSBS_SEEDPKG_OK) {
        return rc;
    }
    return rsbs::seedpkg::WriteAll(path != nullptr ? path : "", ordered);
}

extern "C" const uint8_t* OoT_SeedPackage_TestFinishedMMHalf(size_t* outSize) {
    const uint8_t* half = nullptr;
    size_t size = 0;
    if (!OoT_PairedGen_CompletedWorld(&half, &size, nullptr, nullptr, nullptr, nullptr)) {
        half = nullptr;
        size = 0;
    }
    if (outSize != nullptr) {
        *outSize = size;
    }
    return half;
}

extern "C" const uint8_t* OoT_SeedPackage_TestStagedMMHalf(size_t* outSize) {
    std::lock_guard<std::mutex> lock(sStageMutex);
    if (!sStaged) {
        if (outSize != nullptr) {
            *outSize = 0;
        }
        return nullptr;
    }
    const std::string& half = sStaged->entries.at(RSBS_SEEDPKG_ENTRY_MM_HALF);
    if (outSize != nullptr) {
        *outSize = half.size();
    }
    return reinterpret_cast<const uint8_t*>(half.data());
}

extern "C" void OoT_SeedPackage_TestReapplyWorldRecord(void) {
    const std::string bytes = WorldRecordBytes(gComboCtx);
    auto rec = std::make_unique<ComboContext>();
    memcpy(rec.get(), bytes.data(), sizeof(ComboContext));
    ComboContext_Init();
    ApplyComboWorld(*rec);
}

#endif // RSBS_SINGLE_EXECUTABLE
