/**
 * @file test_curated_archive_order.c
 * @brief Archive-layer locks for the two curated custom-asset archives (#595)
 *        and for user mod archives surviving a game switch (#593).
 *
 * Both defects come from the same property of libultraship's archive layer:
 * `ArchiveManager::AddArchive` walks the archive's file list and overwrites
 * `mFileToArchive[hash]` unconditionally, so resolution is LAST-ADDED-WINS
 * globally, with no priority field
 * (libultraship/src/ship/resource/archive/ArchiveManager.cpp). In a
 * single-executable build both games' archives live in ONE flat manager, so
 * "who wins" is a function of mount order — and mount order is a function of
 * which game booted first and whether a switch has happened.
 *
 * ---------------------------------------------------------------------------
 * Test_CuratedArchiveOrder (#595)
 * ---------------------------------------------------------------------------
 * soh.o2r and 2ship.o2r are the two archives WE generate from in-tree custom
 * assets (`--norom --custom-assets-path`, CMakeLists.txt GenerateSohOtr /
 * Generate2ShipOtr). They collided on 595 paths, 21 of which differed in
 * content: the fourteen chest-appearance corner/lock textures under
 * objects/object_box/, three accessibility text banks, and Fast3D's four
 * default shaders. Because both are mounted into the one manager, which copy
 * either game got depended on switch order.
 *
 * The lock states the invariant directly rather than enumerating the 21: mount
 * both curated archives BOTH WAYS ROUND and require every path to resolve to
 * identical bytes either way. That is exactly "mount order cannot change what
 * any resource is", it needs no allowlist to maintain, and it catches a
 * twenty-second colliding path the moment someone adds one.
 *
 * Two anti-vacuity guards, because "no differences" is the pass condition and
 * an empty comparison would also produce it:
 *   - each archive alone must expose a plausible number of paths, and
 *   - the two must actually SHARE paths (the collision set must be non-empty).
 * Without the second guard the test would go green if a future change simply
 * stopped generating one of the archives.
 *
 * Counterfactual (run): with `mm/objects/object_box/*` moved back to
 * `objects/object_box/*` and MM's stale shader/accessibility copies restored,
 * the row reports 21 order-dependent paths and fails.
 *
 * ---------------------------------------------------------------------------
 * Test_ModArchiveSurvivesSwitch (#593)
 * ---------------------------------------------------------------------------
 * A user mod overrides a base asset ONLY by being mounted after it. Both ports
 * mount `mods/` last for that reason. `EnsureGameArchivesLoaded` re-adds the
 * destination game's base archives on every cross-game switch (the #154 fix),
 * which put them back on top and silently revoked every override — the mods
 * stayed listed as enabled and simply stopped applying.
 *
 * This row drives the REAL production function (`Combo_EnsureGameArchivesLoaded`
 * in rsbs/src/main.cpp) against the real shared ArchiveManager, using a copy of
 * a staged archive as a stand-in mod, and asserts the mod owns the contested
 * path afterwards. Its own negative control runs the identical sequence with an
 * EMPTY registry and asserts the base archive wins there — so the positive leg
 * is provably detecting the replay and not some incidental ordering.
 *
 * Both rows SKIP (not fail) when the archives they need are not staged, the
 * same policy as the #560 zip-contention row: the netplay-relay CI job re-runs
 * this label archive-less on purpose.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++): it drives the
 * C++-linkage Ship::Context / ArchiveManager APIs directly.
 */

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "../mod_archives.h"

namespace {

// Below this, an "archive" is not a plausibly-generated curated archive and
// the comparison would be near-vacuous. soh.o2r ships ~1280 paths and
// 2ship.o2r ~840; 100 is a floor, not a target.
constexpr size_t kCaoMinPathsPerArchive = 100;

// How many differing paths to print before truncating the report. All of them
// are counted either way; this only bounds the log.
constexpr int kCaoMaxReported = 40;

std::string CaoResolveArchive(const char* filename) {
    std::string path = Ship::Context::LocateFileAcrossAppDirs(filename);
    if (!path.empty() && std::filesystem::exists(path)) {
        return path;
    }
    path = Ship::Context::GetPathRelativeToAppBundle(filename);
    if (!path.empty() && std::filesystem::exists(path)) {
        return path;
    }
    return "";
}

// A manager holding exactly the given archives, added in the given order.
// Returns nullptr if any AddArchive refused (a corrupt/unreadable archive).
std::unique_ptr<Ship::ArchiveManager> CaoMakeManager(const std::vector<std::string>& archivesInOrder) {
    auto mgr = std::make_unique<Ship::ArchiveManager>();
    for (const auto& archivePath : archivesInOrder) {
        if (mgr->AddArchive(archivePath) == nullptr) {
            fprintf(stderr, "[curated-archive-order] AddArchive refused: %s\n", archivePath.c_str());
            return nullptr;
        }
    }
    return mgr;
}

// Read one path through the manager's OWN winner for it.
//
// Deliberately not ArchiveManager::LoadFile: that resolves the archive and then
// calls Archive::LoadFile(hash), whose first act is
// `*Context::GetInstance()->GetResourceManager()->GetArchiveManager()->HashToString(hash)`
// — the GLOBAL manager, not the one that owns the archive
// (libultraship O2rArchive.cpp:16-19). Off the singleton that dereferences
// null. GetArchiveFromFile is a pure map lookup on the manager we hand it, and
// Archive::LoadFile(const std::string&) reads the zip by name with no globals
// at all, which is exactly the pair this row needs.
std::shared_ptr<Ship::File> CaoLoadThroughWinner(Ship::ArchiveManager& mgr, const std::string& path) {
    auto archive = mgr.GetArchiveFromFile(path);
    if (archive == nullptr) {
        return nullptr;
    }
    return archive->LoadFile(path);
}

// Byte-compare one path as resolved by two managers. Absent from both counts
// as equal (neither manager can serve it, so order cannot matter).
bool CaoResolvesIdentically(Ship::ArchiveManager& a, Ship::ArchiveManager& b, const std::string& path,
                            size_t* outSizeA, size_t* outSizeB) {
    auto fileA = CaoLoadThroughWinner(a, path);
    auto fileB = CaoLoadThroughWinner(b, path);

    const bool haveA = fileA != nullptr && fileA->Buffer != nullptr;
    const bool haveB = fileB != nullptr && fileB->Buffer != nullptr;
    *outSizeA = haveA ? fileA->Buffer->size() : 0;
    *outSizeB = haveB ? fileB->Buffer->size() : 0;

    if (haveA != haveB) {
        return false;
    }
    if (!haveA) {
        return true;
    }
    if (*outSizeA != *outSizeB) {
        return false;
    }
    return memcmp(fileA->Buffer->data(), fileB->Buffer->data(), *outSizeA) == 0;
}

} // namespace

// ============================================================================
// #595 — mount order must not change what any resource is
// ============================================================================

extern "C" int CuratedArchiveOrder_RunHeadless(const char* sohArchive, const char* mmArchive) {
    // Each archive alone: used both to enumerate the collision set and to name
    // the side that differs in the failure report.
    auto sohOnly = CaoMakeManager({ sohArchive });
    auto mmOnly = CaoMakeManager({ mmArchive });
    if (sohOnly == nullptr || mmOnly == nullptr) {
        fprintf(stderr, "[curated-archive-order] FAIL: could not mount one of the curated archives\n");
        return 1;
    }

    auto sohPaths = sohOnly->ListFiles();
    auto mmPaths = mmOnly->ListFiles();
    printf("[curated-archive-order] soh.o2r: %zu paths, 2ship.o2r: %zu paths\n", sohPaths->size(), mmPaths->size());

    if (sohPaths->size() < kCaoMinPathsPerArchive || mmPaths->size() < kCaoMinPathsPerArchive) {
        fprintf(stderr,
                "[curated-archive-order] FAIL: a curated archive is implausibly small (<%zu paths) — the "
                "comparison below would be vacuous\n",
                kCaoMinPathsPerArchive);
        return 1;
    }

    std::unordered_set<std::string> sohSet(sohPaths->begin(), sohPaths->end());
    std::vector<std::string> shared;
    for (const auto& path : *mmPaths) {
        if (sohSet.count(path) > 0) {
            shared.push_back(path);
        }
    }
    printf("[curated-archive-order] shared paths: %zu\n", shared.size());

    if (shared.empty()) {
        // Not a pass: the two archives are known to overlap (portVersion at a
        // minimum, plus every LUS-owned path both must carry). An empty
        // intersection means one of them stopped being generated, and the
        // both-orders comparison below would then prove nothing.
        fprintf(stderr, "[curated-archive-order] FAIL: the two curated archives share NO path — the order "
                        "comparison would be vacuous. Check that both archives are the real generated ones.\n");
        return 1;
    }

    // The actual claim: same two archives, both mount orders, every path.
    auto sohFirst = CaoMakeManager({ sohArchive, mmArchive });
    auto mmFirst = CaoMakeManager({ mmArchive, sohArchive });
    if (sohFirst == nullptr || mmFirst == nullptr) {
        fprintf(stderr, "[curated-archive-order] FAIL: could not mount both curated archives together\n");
        return 1;
    }

    // Every path either archive contributes. Non-shared paths resolve to the
    // same archive in both managers and are near-free to check; including them
    // means the row does not depend on the intersection computed above being
    // right.
    std::vector<std::string> allPaths(sohPaths->begin(), sohPaths->end());
    for (const auto& path : *mmPaths) {
        if (sohSet.count(path) == 0) {
            allPaths.push_back(path);
        }
    }
    std::sort(allPaths.begin(), allPaths.end());

    int differing = 0;
    for (const auto& path : allPaths) {
        size_t sizeA = 0;
        size_t sizeB = 0;
        if (CaoResolvesIdentically(*sohFirst, *mmFirst, path, &sizeA, &sizeB)) {
            continue;
        }
        differing++;
        if (differing <= kCaoMaxReported) {
            fprintf(stderr,
                    "[curated-archive-order] ORDER-DEPENDENT: %s (soh-first %zu bytes, 2ship-first %zu bytes)\n",
                    path.c_str(), sizeA, sizeB);
        }
    }

    if (differing > 0) {
        fprintf(stderr,
                "[curated-archive-order] FAIL: %d of %zu paths resolve differently depending on which curated "
                "archive was mounted first (#595). Give the divergent MM-side assets a game-scoped path (the "
                "\"mm/\" subtree of 2ship.o2r), or make both archives ship identical bytes for LUS-owned paths.\n",
                differing, allPaths.size());
        return 1;
    }

    printf("[curated-archive-order] PASS: %zu paths (%zu shared) resolve identically in both mount orders\n",
           allPaths.size(), shared.size());
    return 0;
}

// ============================================================================
// #593 — a game switch must not revoke the destination game's mod archives
// ============================================================================

extern "C" int ModArchiveSurvivesSwitch_RunHeadless(const char* baseArchive, const char* modSourceArchive) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr ||
        ctx->GetResourceManager()->GetArchiveManager() == nullptr) {
        fprintf(stderr, "[mod-survives-switch] FAIL: resource manager not initialized\n");
        return 1;
    }
    auto archiveManager = ctx->GetResourceManager()->GetArchiveManager();

    // Stand-in mod: a byte copy of a staged archive, so it is a real loadable
    // o2r whose paths overlap the base archive's. Its own file name is what
    // makes "which archive won" observable.
    const std::string modPath = (std::filesystem::current_path() / "rsbs_test_mod_593.o2r").generic_string();
    std::error_code ec;
    std::filesystem::remove(modPath, ec);
    std::filesystem::copy_file(modSourceArchive, modPath, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        fprintf(stderr, "[mod-survives-switch] FAIL: could not stage the test mod archive at %s (%s)\n",
                modPath.c_str(), ec.message().c_str());
        return 1;
    }

    int rc = 0;
    Combo_ClearModArchives(GAME_OOT);
    Combo_ClearModArchives(GAME_MM);

    // A path both archives carry, so ownership is genuinely contested.
    // portVersion is written into every curated archive by the exporter.
    const std::string contested = "portVersion";

    auto ownerOf = [&](const std::string& path) -> std::string {
        auto archive = archiveManager->GetArchiveFromFile(path);
        return archive != nullptr ? archive->GetPath() : std::string();
    };
    auto isModPath = [&](const std::string& owner) {
        return owner.find("rsbs_test_mod_593.o2r") != std::string::npos;
    };

    do {
        // ---- Registry semantics -------------------------------------------
        Combo_RegisterModArchive(GAME_OOT, modPath.c_str());
        Combo_RegisterModArchive(GAME_OOT, modPath.c_str()); // duplicate: ignored
        Combo_RegisterModArchive(GAME_OOT, nullptr);
        Combo_RegisterModArchive(GAME_OOT, "");
        Combo_RegisterModArchive(GAME_NONE, modPath.c_str());
        if (Combo_GetModArchiveCount(GAME_OOT) != 1) {
            fprintf(stderr, "[mod-survives-switch] FAIL: registry recorded %d entries, expected 1\n",
                    Combo_GetModArchiveCount(GAME_OOT));
            rc = 1;
            break;
        }
        if (Combo_GetModArchiveCount(GAME_MM) != 0) {
            fprintf(stderr, "[mod-survives-switch] FAIL: an OoT registration leaked into MM's list\n");
            rc = 1;
            break;
        }
        if (Combo_GetModArchive(GAME_OOT, 1) != nullptr || Combo_GetModArchive(GAME_OOT, -1) != nullptr) {
            fprintf(stderr, "[mod-survives-switch] FAIL: out-of-range index did not return NULL\n");
            rc = 1;
            break;
        }

        // ---- Positive leg: the switch must leave the mod on top ------------
        // Put the base archive on top first, exactly as the pre-switch state
        // would NOT be -- this is the strictest starting point: if the switch
        // path did nothing at all, the base would still own the path.
        if (archiveManager->AddArchive(std::string(baseArchive)) == nullptr) {
            fprintf(stderr, "[mod-survives-switch] FAIL: could not mount the base archive %s\n", baseArchive);
            rc = 1;
            break;
        }
        if (isModPath(ownerOf(contested))) {
            fprintf(stderr, "[mod-survives-switch] FAIL: precondition — base archive did not take ownership of %s\n",
                    contested.c_str());
            rc = 1;
            break;
        }

        // The production switch-time mount, unmodified.
        Combo_EnsureGameArchivesLoaded(GAME_OOT);

        const std::string ownerAfterSwitch = ownerOf(contested);
        printf("[mod-survives-switch] with 1 registered mod, %s is owned by: %s\n", contested.c_str(),
               ownerAfterSwitch.c_str());
        if (!isModPath(ownerAfterSwitch)) {
            fprintf(stderr,
                    "[mod-survives-switch] FAIL: after the switch the base archives own %s, so every user mod "
                    "override was silently revoked (#593)\n",
                    contested.c_str());
            rc = 1;
            break;
        }

        // ---- Negative control: no registered mods, base must win -----------
        // Same sequence, empty registry. If this ALSO reported the mod as
        // owner, the assertion above would be measuring something incidental
        // rather than the replay.
        Combo_ClearModArchives(GAME_OOT);
        if (archiveManager->AddArchive(std::string(baseArchive)) == nullptr) {
            fprintf(stderr, "[mod-survives-switch] FAIL: could not re-mount the base archive for the control\n");
            rc = 1;
            break;
        }
        Combo_EnsureGameArchivesLoaded(GAME_OOT);

        const std::string ownerControl = ownerOf(contested);
        printf("[mod-survives-switch] with 0 registered mods, %s is owned by: %s\n", contested.c_str(),
               ownerControl.c_str());
        if (isModPath(ownerControl)) {
            fprintf(stderr,
                    "[mod-survives-switch] FAIL: control — the mod archive won with an EMPTY registry, so the "
                    "positive leg above proves nothing\n");
            rc = 1;
            break;
        }
    } while (false);

    // Leave the shared manager as we found it: `--test all` runs every row in
    // one process, and a stray archive mounted on top of soh.o2r would follow
    // later rows around.
    archiveManager->RemoveArchive(modPath);
    Combo_ClearModArchives(GAME_OOT);
    Combo_ClearModArchives(GAME_MM);
    std::filesystem::remove(modPath, ec);

    if (rc == 0) {
        printf("[mod-survives-switch] PASS: a registered mod archive still wins after the switch-time base "
               "re-add, and does not win when unregistered\n");
    }
    return rc;
}
