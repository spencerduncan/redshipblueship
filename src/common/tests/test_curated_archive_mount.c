/**
 * @file test_curated_archive_mount.c
 * @brief Mount-and-identity lock for the curated cross-game archives (#577 M1).
 *
 * The 2026-09-30 ruling on #577 splits the curated archive per direction and
 * gives each half the identity of the game that DRAWS it:
 *
 *   redship-oot.o2r   MM-origin content OoT draws    -> an OoT archive
 *   redship-mm.o2r    OoT-origin content MM draws    -> an MM archive, recorded
 *                                                       through MM's
 *                                                       RecordMMArchivePath
 *
 * Identity is not a label here, it is behaviour: RsbsMMArchiveFactoryDispatcher
 * (games/mm/2s2h/GameExports_SingleExe.cpp) picks the Room/Cutscene/Path/Array
 * parser by whether the OWNING archive is recorded as MM's, and
 * ResourceMgr_ListFilesForGame files a path under the game its owner belongs to.
 * The runtime side under test is rsbs/src/main.cpp: Combo_EnsureGameArchivesLoaded
 * (every switch) mounts the ARRIVING game's half right after that game's base
 * archives and before its mods, and the MM half is recorded as MM's.
 *
 * Two legs.
 *
 *  1. ROM-free (runs wherever soh.o2r is staged, CI included). Synthetic
 *     stand-ins named redship-oot.o2r / redship-mm.o2r, each holding paths no
 *     real archive ships, are staged in a private directory that the
 *     production lookup (Ship::Context::LocateFileAcrossAppDirs) probes FIRST:
 *     the process's working directory, which is the app-config directory of a
 *     portable build, plus $SHIP_HOME pointed at it for Linux/macOS, where
 *     libultraship reads the app-config directory from there instead. An MM
 *     "mod" stand-in carrying one of redship-mm.o2r's paths is registered for
 *     MM. Then, driving the PRODUCTION Combo_EnsureGameArchivesLoaded:
 *       - arrival in OoT: OoT's half owns its paths and is not MM's; MM's half
 *         is NOT mounted (nothing in OoT draws OoT-origin content from it);
 *       - arrival in MM: MM's half owns its paths and IS recorded as MM's;
 *         the MM mod still wins the path it shares with MM's half (curated
 *         before mods, so a player's mod can restyle a foreign model); OoT's
 *         half is still not MM's;
 *       - back in OoT: neither half changed identity.
 *
 *  2. ROM-staged (needs the extracted oot.o2r / mm.o2r AND the generated
 *     redship-oot.o2r / redship-mm.o2r beside the binary; SKIPs the leg
 *     otherwise). The property the split exists for, over EVERY path the two
 *     real curated halves carry: after arriving in game G, each of those paths
 *     is owned by an archive of G's identity -- the curated copy while its
 *     host runs, the source game's own base archive once the source game runs.
 *     A foreign model therefore always parses with the running game's readers.
 *     mm.o2r is recorded as MM's first through MM_MountArchiveHeadless, which is
 *     what LoadMMArchives does on MM's first init right after the switch mounts
 *     it; this row never initializes MM itself.
 *
 * RED before the fix: nothing in rsbs/ mounted a curated archive at all
 * (Combo_EnsureGameArchivesLoaded re-added base archives and mods only), so the
 * first ownership assertion of either leg finds no owner.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++): it drives the
 * C++-linkage Ship::Context / ArchiveManager APIs directly. The wrapper
 * (Test_CuratedArchiveMount) does the display-free OoT bring-up and SKIPs the
 * whole row when soh.o2r is not staged (the #562 archive-less control run).
 */

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "../mod_archives.h"

extern "C" bool Combo_ArchivePathIsMM(const char* path);
extern "C" int MM_MountArchiveHeadless(const char* path);

namespace {

constexpr const char* kCamOoTHalf = "redship-oot.o2r";
constexpr const char* kCamMMHalf = "redship-mm.o2r";

// Paths no real archive ships, so ownership of them is decided by the
// stand-ins alone -- in `--test all` too, where every other row shares the
// ArchiveManager.
constexpr const char* kCamOoTHalfPath = "rsbs_test_577_m1/oot_half_only";
constexpr const char* kCamMMHalfPath = "rsbs_test_577_m1/mm_half_only";
constexpr const char* kCamModContestedPath = "rsbs_test_577_m1/mm_half_and_mm_mod";

// ---------------------------------------------------------------------------
// A minimal STORED zip writer. libzip is not on this target's include path, and
// the stand-ins need nothing but a valid central directory: the row asks the
// ArchiveManager who OWNS a path, it never reads a payload.
// ---------------------------------------------------------------------------

uint32_t CamCrc32(const std::string& data) {
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned char byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void CamPut16(std::string& out, uint16_t v) {
    out.push_back((char)(v & 0xFF));
    out.push_back((char)((v >> 8) & 0xFF));
}

void CamPut32(std::string& out, uint32_t v) {
    CamPut16(out, (uint16_t)(v & 0xFFFF));
    CamPut16(out, (uint16_t)((v >> 16) & 0xFFFF));
}

bool CamWriteZip(const std::string& zipPath, const std::vector<std::string>& names) {
    std::string body;
    std::string central;
    for (const std::string& name : names) {
        const std::string payload = "rsbs #577 M1 stand-in: " + name;
        const uint32_t crc = CamCrc32(payload);
        const uint32_t offset = (uint32_t)body.size();

        CamPut32(body, 0x04034b50u); // local file header
        CamPut16(body, 20);          // version needed
        CamPut16(body, 0);           // flags
        CamPut16(body, 0);           // method: stored
        CamPut16(body, 0);           // mod time
        CamPut16(body, 0x21);        // mod date: 1980-01-01
        CamPut32(body, crc);
        CamPut32(body, (uint32_t)payload.size());
        CamPut32(body, (uint32_t)payload.size());
        CamPut16(body, (uint16_t)name.size());
        CamPut16(body, 0); // extra length
        body += name;
        body += payload;

        CamPut32(central, 0x02014b50u); // central directory header
        CamPut16(central, 20);          // version made by
        CamPut16(central, 20);          // version needed
        CamPut16(central, 0);
        CamPut16(central, 0);
        CamPut16(central, 0);
        CamPut16(central, 0x21);
        CamPut32(central, crc);
        CamPut32(central, (uint32_t)payload.size());
        CamPut32(central, (uint32_t)payload.size());
        CamPut16(central, (uint16_t)name.size());
        CamPut16(central, 0); // extra
        CamPut16(central, 0); // comment
        CamPut16(central, 0); // disk
        CamPut16(central, 0); // internal attributes
        CamPut32(central, 0); // external attributes
        CamPut32(central, offset);
        central += name;
    }

    std::string eocd;
    CamPut32(eocd, 0x06054b50u);
    CamPut16(eocd, 0);
    CamPut16(eocd, 0);
    CamPut16(eocd, (uint16_t)names.size());
    CamPut16(eocd, (uint16_t)names.size());
    CamPut32(eocd, (uint32_t)central.size());
    CamPut32(eocd, (uint32_t)body.size());
    CamPut16(eocd, 0);

    std::ofstream out(zipPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << body << central << eocd;
    out.close();
    return !out.fail();
}

// ---------------------------------------------------------------------------
// Environment: point the production lookup's FIRST probe at a private directory
// for the duration of a leg, and put everything back afterwards whatever
// happens (RAII), so no later row inherits either the cwd or $SHIP_HOME.
// ---------------------------------------------------------------------------

void CamSetShipHome(const char* value) {
#ifdef _WIN32
    _putenv_s("SHIP_HOME", value != nullptr ? value : "");
#else
    if (value != nullptr) {
        setenv("SHIP_HOME", value, 1);
    } else {
        unsetenv("SHIP_HOME");
    }
#endif
}

class CamAppDirOverride {
  public:
    explicit CamAppDirOverride(const std::filesystem::path& dir) {
        std::error_code ec;
        mOldCwd = std::filesystem::current_path(ec);
        const char* home = std::getenv("SHIP_HOME");
        mHadHome = home != nullptr;
        if (mHadHome) {
            mOldHome = home;
        }
        CamSetShipHome(dir.generic_string().c_str());
        std::filesystem::current_path(dir, ec);
        mOk = !ec;
    }
    ~CamAppDirOverride() {
        std::error_code ec;
        std::filesystem::current_path(mOldCwd, ec);
        CamSetShipHome(mHadHome ? mOldHome.c_str() : nullptr);
    }
    bool Ok() const {
        return mOk;
    }

  private:
    std::filesystem::path mOldCwd;
    std::string mOldHome;
    bool mHadHome = false;
    bool mOk = false;
};

// Both games' mod registries are process-wide; the leg needs a known MM set and
// every later row in `--test all` needs the set it had before. Snapshot, clear,
// and restore in registration order on the way out.
class CamModRegistrySnapshot {
  public:
    CamModRegistrySnapshot() {
        for (GameId game : { GAME_OOT, GAME_MM }) {
            std::vector<std::string>& saved = game == GAME_OOT ? mOoT : mMM;
            const int count = Combo_GetModArchiveCount(game);
            for (int i = 0; i < count; i++) {
                const char* path = Combo_GetModArchive(game, i);
                if (path != nullptr) {
                    saved.push_back(path);
                }
            }
            Combo_ClearModArchives(game);
        }
    }
    ~CamModRegistrySnapshot() {
        for (GameId game : { GAME_OOT, GAME_MM }) {
            Combo_ClearModArchives(game);
            for (const std::string& path : game == GAME_OOT ? mOoT : mMM) {
                Combo_RegisterModArchive(game, path.c_str());
            }
        }
    }

  private:
    std::vector<std::string> mOoT;
    std::vector<std::string> mMM;
};

// ---------------------------------------------------------------------------
// Ownership probes.
// ---------------------------------------------------------------------------

std::string CamFileName(const std::string& archivePath) {
    return std::filesystem::path(archivePath).filename().string();
}

struct CamOwner {
    bool present = false;
    std::string path;
    bool isMM = false;
};

CamOwner CamOwnerOf(Ship::ArchiveManager& am, const std::string& resourcePath) {
    CamOwner owner;
    auto archive = am.GetArchiveFromFile(resourcePath);
    if (archive != nullptr) {
        owner.present = true;
        owner.path = archive->GetPath();
        owner.isMM = Combo_ArchivePathIsMM(owner.path.c_str());
    }
    return owner;
}

// `resourcePath` must be owned by an archive whose FILE NAME is `expectedName`,
// with MM identity == `expectMM`.
bool CamExpectOwner(Ship::ArchiveManager& am, const char* step, const char* resourcePath, const char* expectedName,
                    bool expectMM) {
    const CamOwner owner = CamOwnerOf(am, resourcePath);
    if (!owner.present) {
        fprintf(stderr, "[curated-archive-mount] FAIL (%s): '%s' is in no mounted archive -- expected %s (%s)\n",
                step, resourcePath, expectedName, expectMM ? "MM identity" : "OoT identity");
        return false;
    }
    if (CamFileName(owner.path) != expectedName || owner.isMM != expectMM) {
        fprintf(stderr,
                "[curated-archive-mount] FAIL (%s): '%s' is owned by '%s' (%s identity) -- expected %s with %s "
                "identity\n",
                step, resourcePath, owner.path.c_str(), owner.isMM ? "MM" : "OoT", expectedName,
                expectMM ? "MM" : "OoT");
        return false;
    }
    printf("[curated-archive-mount] %s: '%s' -> '%s' (%s identity)\n", step, resourcePath, owner.path.c_str(),
           owner.isMM ? "MM" : "OoT");
    return true;
}

bool CamExpectUnowned(Ship::ArchiveManager& am, const char* step, const char* resourcePath, const char* why) {
    const CamOwner owner = CamOwnerOf(am, resourcePath);
    if (owner.present) {
        fprintf(stderr, "[curated-archive-mount] FAIL (%s): '%s' is owned by '%s' -- %s\n", step, resourcePath,
                owner.path.c_str(), why);
        return false;
    }
    printf("[curated-archive-mount] %s: '%s' unmounted, as required\n", step, resourcePath);
    return true;
}

// ---------------------------------------------------------------------------
// Leg 1: synthetic stand-ins, production switch path.
// ---------------------------------------------------------------------------

int CamSwitchLeg(Ship::ArchiveManager& am) {
    std::error_code ec;
    const std::filesystem::path stage = std::filesystem::absolute("rsbs_test_577_m1_mount", ec);
    // Best effort: a previous process's stand-ins are overwritten below either
    // way. (This process cannot delete them afterwards on Windows -- archives
    // are never unmounted, so their handles stay open.)
    std::filesystem::remove_all(stage, ec);
    std::filesystem::create_directories(stage, ec);
    const std::string modPath = (stage / "mm_mod_standin.o2r").generic_string();
    if (!CamWriteZip((stage / kCamOoTHalf).string(), { kCamOoTHalfPath }) ||
        !CamWriteZip((stage / kCamMMHalf).string(), { kCamMMHalfPath, kCamModContestedPath }) ||
        !CamWriteZip(modPath, { kCamModContestedPath })) {
        fprintf(stderr, "[curated-archive-mount] FAIL: could not stage the stand-in archives in %s\n",
                stage.generic_string().c_str());
        return 1;
    }

    CamModRegistrySnapshot registry;
    Combo_RegisterModArchive(GAME_MM, modPath.c_str());

    CamAppDirOverride appDir(stage);
    if (!appDir.Ok()) {
        fprintf(stderr, "[curated-archive-mount] FAIL: could not enter %s\n", stage.generic_string().c_str());
        return 1;
    }
    // Anti-vacuity: the production lookup must actually see the stand-ins,
    // or every assertion below is about some other file.
    for (const char* name : { kCamOoTHalf, kCamMMHalf }) {
        const std::string located = Ship::Context::LocateFileAcrossAppDirs(name);
        if (!std::filesystem::exists(located) ||
            !std::filesystem::equivalent(located, stage / name, ec)) {
            fprintf(stderr,
                    "[curated-archive-mount] FAIL: LocateFileAcrossAppDirs(\"%s\") resolved '%s', not the stand-in "
                    "in %s -- the leg would test the wrong file\n",
                    name, located.c_str(), stage.generic_string().c_str());
            return 1;
        }
    }

    int failures = 0;

    // Arrival in OoT.
    Combo_EnsureGameArchivesLoaded(GAME_OOT);
    failures += !CamExpectOwner(am, "switch: arrive OoT", kCamOoTHalfPath, kCamOoTHalf, false);
    failures += !CamExpectUnowned(am, "switch: arrive OoT", kCamMMHalfPath,
                                  "MM's half must not be mounted by an arrival in OoT");

    // Arrival in MM.
    Combo_EnsureGameArchivesLoaded(GAME_MM);
    failures += !CamExpectOwner(am, "switch: arrive MM", kCamMMHalfPath, kCamMMHalf, true);
    failures += !CamExpectOwner(am, "switch: arrive MM (mods after curated)", kCamModContestedPath,
                                "mm_mod_standin.o2r", false);
    failures += !CamExpectOwner(am, "switch: arrive MM", kCamOoTHalfPath, kCamOoTHalf, false);

    // Back in OoT: neither half changes identity.
    Combo_EnsureGameArchivesLoaded(GAME_OOT);
    failures += !CamExpectOwner(am, "switch: back in OoT", kCamOoTHalfPath, kCamOoTHalf, false);
    failures += !CamExpectOwner(am, "switch: back in OoT", kCamMMHalfPath, kCamMMHalf, true);
    return failures;
}

// ---------------------------------------------------------------------------
// Leg 2: the real generated halves over the real base archives.
// ---------------------------------------------------------------------------

std::string CamResolve(const char* filename, const char* appName) {
    std::string path = Ship::Context::LocateFileAcrossAppDirs(filename, appName);
    if (!path.empty() && std::filesystem::exists(path)) {
        return path;
    }
    return "";
}

// Every path an archive file carries, read through a PRIVATE manager so listing
// it does not mount it into the shared one.
std::vector<std::string> CamListArchive(const std::string& archivePath) {
    std::vector<std::string> names;
    auto mgr = std::make_unique<Ship::ArchiveManager>();
    auto archive = mgr->AddArchive(archivePath);
    if (archive == nullptr) {
        return names;
    }
    for (const auto& [hash, name] : *archive->ListFiles()) {
        names.push_back(name);
    }
    return names;
}

int CamIdentityLeg(Ship::ArchiveManager& am) {
    // Same lookups Combo_EnsureGameArchivesLoaded makes, so the strings match
    // the archives it mounts (identity is recorded by path string).
    const std::string ootBase = CamResolve("oot.o2r", "soh");
    const std::string mmBase = CamResolve("mm.o2r", "2s2h");
    const std::string ootHalf = CamResolve(kCamOoTHalf, "");
    const std::string mmHalf = CamResolve(kCamMMHalf, "");
    if (ootBase.empty() || mmBase.empty() || ootHalf.empty() || mmHalf.empty()) {
        printf("[curated-archive-mount] identity leg SKIPPED: needs oot.o2r '%s', mm.o2r '%s', %s '%s', %s '%s' "
               "(run ExtractAssets, ExtractMMAssets and GenerateRedshipOtr to arm it)\n",
               ootBase.c_str(), mmBase.c_str(), kCamOoTHalf, ootHalf.c_str(), kCamMMHalf, mmHalf.c_str());
        return 0;
    }

    const std::vector<std::string> ootHalfPaths = CamListArchive(ootHalf);
    const std::vector<std::string> mmHalfPaths = CamListArchive(mmHalf);
    if (ootHalfPaths.empty() || mmHalfPaths.empty()) {
        fprintf(stderr, "[curated-archive-mount] FAIL: a real curated half lists no paths (%s: %zu, %s: %zu)\n",
                kCamOoTHalf, ootHalfPaths.size(), kCamMMHalf, mmHalfPaths.size());
        return 1;
    }

    // What LoadMMArchives does on MM's first init, right after the switch
    // mounts mm.o2r: record it as MM's. This row never initializes MM.
    if (MM_MountArchiveHeadless(mmBase.c_str()) != 0) {
        fprintf(stderr, "[curated-archive-mount] FAIL: could not mount and record %s as MM's\n", mmBase.c_str());
        return 1;
    }

    int failures = 0;
    auto checkAll = [&](const char* step, bool expectMM) {
        int wrong = 0;
        int servedByCurated = 0;
        for (const auto* list : { &ootHalfPaths, &mmHalfPaths }) {
            for (const std::string& path : *list) {
                const CamOwner owner = CamOwnerOf(am, path);
                if (owner.present &&
                    (CamFileName(owner.path) == kCamOoTHalf || CamFileName(owner.path) == kCamMMHalf)) {
                    servedByCurated++;
                }
                if (!owner.present || owner.isMM != expectMM) {
                    if (wrong < 10) {
                        fprintf(stderr,
                                "[curated-archive-mount] FAIL (%s): '%s' is owned by '%s' (%s identity) -- a "
                                "foreign model would parse with the %s game's readers\n",
                                step, path.c_str(), owner.present ? owner.path.c_str() : "<nothing>",
                                !owner.present ? "no" : (owner.isMM ? "MM" : "OoT"),
                                !owner.present ? "missing" : "wrong");
                    }
                    wrong++;
                }
            }
        }
        printf("[curated-archive-mount] %s: %zu curated path(s), %d owned by the wrong identity, %d served by a "
               "curated half\n",
               step, ootHalfPaths.size() + mmHalfPaths.size(), wrong, servedByCurated);
        // Anti-vacuity: the running game's curated half must be doing the
        // serving for its own paths -- a pass on base archives alone would
        // say nothing about the halves.
        if (servedByCurated == 0) {
            fprintf(stderr, "[curated-archive-mount] FAIL (%s): no curated path is served by a curated half\n",
                    step);
            wrong++;
        }
        failures += wrong > 0 ? 1 : 0;
    };

    Combo_EnsureGameArchivesLoaded(GAME_OOT);
    checkAll("identity: arrive OoT", false);
    Combo_EnsureGameArchivesLoaded(GAME_MM);
    checkAll("identity: arrive MM", true);
    Combo_EnsureGameArchivesLoaded(GAME_OOT);
    checkAll("identity: back in OoT", false);
    return failures;
}

} // namespace

extern "C" int CuratedArchiveMount_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr ||
        ctx->GetResourceManager()->GetArchiveManager() == nullptr) {
        fprintf(stderr, "[curated-archive-mount] FAIL: resource manager not initialized (caller must run the "
                        "shared bring-up first)\n");
        return 1;
    }
    auto& am = *ctx->GetResourceManager()->GetArchiveManager();

    int failures = 0;
    failures += CamSwitchLeg(am);
    failures += CamIdentityLeg(am);

    if (failures == 0) {
        printf("[curated-archive-mount] PASS: each game's curated half is mounted on arrival with that game's "
               "identity, under the game's mods\n");
    }
    return failures == 0 ? 0 : 1;
}
