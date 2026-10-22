/**
 * @file test_archive_add_dedup.c
 * @brief Lock for ArchiveManager::AddArchive de-duplicating a re-added path (#987).
 *
 * Every game switch re-adds the arriving game's archives (and its curated half
 * and mods) by path. libultraship used to open a second O2rArchive for each
 * call and append it to mArchives and mGameVersions, so every round trip left
 * one more zip handle and one more file list per re-added archive, and the
 * game-version list grew by one entry per versioned archive. The fork's
 * AddArchive now REPLACES the mounted archive of an identical path with the
 * freshly read one (a re-add must still see a file that changed on disk, which
 * the #705 loose-mod folders rely on) and releases the old object, while the
 * new one registers on top exactly as a first add would.
 *
 * ArchiveAddDedup_RunHeadless is the ROM-free `archive-add-dedup` row, included
 * at FILE SCOPE by test_runner.cpp (compiled as C++, it drives the C++-linkage
 * ArchiveManager directly). It builds two temporary O2R zips that carry a
 * version entry and one shared path with different bytes, then asserts:
 *   1. a first add grows the archive count and the game-version list by one;
 *   2. adding the same path again grows neither, the replaced object is
 *      released (nothing keeps its zip handle and file list), the new one is
 *      the last in the mount order, and a file only that archive carries still
 *      resolves;
 *   3. newest-wins survives the re-add: with A then B mounted, re-adding A
 *      makes the shared path resolve to A again, and re-adding B back to B;
 *   4. repeated alternating re-adds (a long play session of switches) leave the
 *      counts where two archives put them and release every replaced object;
 *      the game-version list also keeps its ORDER through every re-add (the
 *      games read GetGameVersions()[0] via ResourceMgr_GetGameVersion(0) and
 *      friends, so re-adding the first-mounted archive must not turn index 0
 *      over);
 *   5. a file added to a loose folder between two adds is seen by the second;
 *      a file only the old copy carried stops resolving (checked on the file
 *      map, since the file is gone from disk either way); a file an earlier
 *      archive also carries goes to the newest such archive.
 */

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <zip.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../test_scratch.h"

namespace {

// AddGameVersion is protected; a using-declaration in a derived class makes it nameable, and the
// pointer-to-member formed through it has the base's type (the same access ItemAdapterSingleExe
// uses), so it is called on the real ArchiveManager without a cast.
struct AadArchiveVersionAccess : Ship::ArchiveManager {
    using Ship::ArchiveManager::AddGameVersion;
};

constexpr const char* kAadSharedPath = "rsbs-test/archive-add-dedup/shared";
constexpr const char* kAadOnlyAPath = "rsbs-test/archive-add-dedup/only-a";
constexpr const char* kAadHeirPath = "rsbs-test/archive-add-dedup/heir";
constexpr const char* kAadLooseOldPath = "rsbs-test/archive-add-dedup/loose-old";
constexpr const char* kAadLooseNewPath = "rsbs-test/archive-add-dedup/loose-new";
constexpr uint32_t kAadVersionA = 0x41414441u; // "ADAA"
constexpr uint32_t kAadVersionB = 0x42414442u; // "BDAB"

// The "version" entry Archive::Load reads: a byte-order byte (0 = little) and a u32.
std::string AadVersionEntry(uint32_t version) {
    std::string v(5, '\0');
    for (int i = 0; i < 4; i++) {
        v[1 + i] = (char)((version >> (8 * i)) & 0xFF);
    }
    return v;
}

bool AadWriteZip(const std::string& path, const std::vector<std::pair<std::string, std::string>>& entries) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    int err = 0;
    zip_t* za = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (za == nullptr) {
        return false;
    }
    for (const auto& [name, data] : entries) {
        // libzip keeps the buffer by pointer until zip_close, so hand it a copy it frees.
        char* copy = (char*)malloc(data.size());
        memcpy(copy, data.data(), data.size());
        zip_source_t* src = zip_source_buffer(za, copy, data.size(), 1);
        if (src == nullptr || zip_file_add(za, name.c_str(), src, ZIP_FL_ENC_UTF_8) < 0) {
            if (src != nullptr) {
                zip_source_free(src);
            }
            zip_discard(za);
            return false;
        }
    }
    return zip_close(za) == 0;
}

bool AadWriteFile(const std::filesystem::path& path, const std::string& data) {
    FILE* f = fopen(path.string().c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    return fclose(f) == 0 && ok;
}

std::string AadBytes(const std::shared_ptr<Ship::File>& file) {
    if (file == nullptr || file->Buffer == nullptr) {
        return "(not found)";
    }
    return std::string(file->Buffer->begin(), file->Buffer->end());
}

size_t AadCountVersion(const std::vector<uint32_t>& versions, uint32_t version) {
    size_t n = 0;
    for (uint32_t v : versions) {
        if (v == version) {
            n++;
        }
    }
    return n;
}

} // namespace

#define AAD_REPORT(...)                       \
    do {                                      \
        printf("[archive-add-dedup] FAIL: "); \
        printf(__VA_ARGS__);                  \
        printf("\n");                         \
        fflush(stdout);                       \
    } while (0)
#define AAD_REQUIRE(cond, ...)       \
    do {                             \
        if (!(cond)) {               \
            AAD_REPORT(__VA_ARGS__); \
            return 1;                \
        }                            \
    } while (0)
#define AAD_EXPECT(cond, ...)        \
    do {                             \
        if (!(cond)) {               \
            AAD_REPORT(__VA_ARGS__); \
            failures++;              \
        }                            \
    } while (0)

int ArchiveAddDedup_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    auto rm = ctx ? ctx->GetResourceManager() : nullptr;
    if (rm == nullptr || rm->GetArchiveManager() == nullptr) {
        printf("[archive-add-dedup] FAIL: no ResourceManager/ArchiveManager after the bring-up\n");
        return 1;
    }
    auto am = rm->GetArchiveManager();
    int failures = 0;

    const std::filesystem::path dir = rsbs::test::ScratchDir("archive-add-dedup");
    const std::string zipA = (dir / "aad-a.o2r").string();
    const std::string zipB = (dir / "aad-b.o2r").string();
    const std::string bytesA = "archive-add-dedup: archive A's shared bytes";
    const std::string bytesB = "archive-add-dedup: archive B's shared bytes, a different length";
    const std::string onlyA = "archive-add-dedup: only archive A carries this";
    const std::string heirB = "archive-add-dedup: heir bytes from archive B";
    AAD_REQUIRE(AadWriteZip(zipA, { { "version", AadVersionEntry(kAadVersionA) },
                                    { kAadSharedPath, bytesA },
                                    { kAadOnlyAPath, onlyA },
                                    { kAadHeirPath, "archive-add-dedup: heir bytes from archive A" } }),
                "could not write %s", zipA.c_str());
    AAD_REQUIRE(AadWriteZip(zipB, { { "version", AadVersionEntry(kAadVersionB) },
                                    { kAadSharedPath, bytesB },
                                    { kAadHeirPath, heirB } }),
                "could not write %s", zipB.c_str());

    const size_t baseArchives = am->GetArchives()->size();
    const std::vector<uint32_t> versionsAtStart = am->GetGameVersions();
    const size_t baseVersions = versionsAtStart.size();
    auto archives = [&]() { return am->GetArchives()->size() - baseArchives; };
    auto versions = [&]() { return am->GetGameVersions().size() - baseVersions; };
    // The versions this row added, in list order. ResourceMgr_GetGameVersion(0) and friends are
    // index-addressed by the games, so a re-add must keep every entry where it was.
    auto versionOrderIs = [&](uint32_t first, uint32_t second) {
        const auto all = am->GetGameVersions();
        if (all.size() != baseVersions + 2) {
            return false;
        }
        return all[baseVersions] == first && all[baseVersions + 1] == second;
    };
    auto sharedOwner = [&]() {
        auto owner = am->GetArchiveFromFile(kAadSharedPath);
        return owner ? owner->GetPath() : std::string("(none)");
    };

    // 1. A first add mounts one archive and records its version once.
    auto firstA = am->AddArchive(zipA);
    AAD_REQUIRE(firstA != nullptr, "AddArchive refused %s", zipA.c_str());
    AAD_EXPECT(archives() == 1, "after adding A: %zu archives mounted, expected 1", archives());
    AAD_EXPECT(versions() == 1, "after adding A: %zu game versions recorded, expected 1", versions());

    // 2. The same path again: nothing grows, the replaced object is released, A's own file resolves.
    std::weak_ptr<Ship::Archive> weakFirstA = firstA;
    firstA.reset();
    auto secondA = am->AddArchive(zipA);
    AAD_REQUIRE(secondA != nullptr, "re-adding A was refused");
    AAD_EXPECT(weakFirstA.expired(), "the replaced archive object for A is still alive after the re-add");
    AAD_EXPECT(am->GetArchives()->back() == secondA, "the re-added A is not last in the mount order");
    AAD_EXPECT(archives() == 1, "after re-adding A: %zu archives mounted, expected 1", archives());
    AAD_EXPECT(versions() == 1, "after re-adding A: %zu game versions recorded, expected 1", versions());
    AAD_EXPECT(AadBytes(am->LoadFile(kAadOnlyAPath)) == onlyA, "A's own file resolves to \"%s\" after the re-add",
               AadBytes(am->LoadFile(kAadOnlyAPath)).c_str());

    // 3. Newest wins. B owns the shared path once added; re-adding A must hand it
    //    back to A (a switch re-adds the base archive and expects it on top), and
    //    re-adding B must hand it to B again.
    auto firstB = am->AddArchive(zipB);
    AAD_REQUIRE(firstB != nullptr, "AddArchive refused %s", zipB.c_str());
    AAD_EXPECT(sharedOwner() == zipB, "B is newest but %s is served by '%s'", kAadSharedPath, sharedOwner().c_str());
    AAD_EXPECT(AadBytes(am->LoadFile(kAadSharedPath)) == bytesB, "shared path read \"%s\" with B on top",
               AadBytes(am->LoadFile(kAadSharedPath)).c_str());
    AAD_EXPECT(versionOrderIs(kAadVersionA, kAadVersionB), "after adding A then B the version list is not [A, B]");
    firstB.reset();
    secondA.reset();
    auto latestA = am->AddArchive(zipA);
    AAD_EXPECT(versionOrderIs(kAadVersionA, kAadVersionB),
               "re-adding A (mounted first) moved its version: the list is not [A, B] (index 0 is the first "
               "mounted game's version)");
    AAD_EXPECT(am->GetArchives()->back() == latestA, "with B mounted, the re-added A is not last in the mount order");
    AAD_EXPECT(sharedOwner() == zipA, "A was re-added last but %s is served by '%s'", kAadSharedPath,
               sharedOwner().c_str());
    AAD_EXPECT(AadBytes(am->LoadFile(kAadSharedPath)) == bytesA, "shared path read \"%s\" after A was re-added",
               AadBytes(am->LoadFile(kAadSharedPath)).c_str());
    auto latestB = am->AddArchive(zipB);
    AAD_EXPECT(versionOrderIs(kAadVersionA, kAadVersionB), "re-adding B changed the version order, expected [A, B]");
    AAD_EXPECT(sharedOwner() == zipB, "B was re-added last but %s is served by '%s'", kAadSharedPath,
               sharedOwner().c_str());
    AAD_EXPECT(AadBytes(am->LoadFile(kAadSharedPath)) == bytesB, "shared path read \"%s\" after B was re-added",
               AadBytes(am->LoadFile(kAadSharedPath)).c_str());
    AAD_EXPECT(archives() == 2, "after A, B and both re-adds: %zu archives mounted, expected 2", archives());
    AAD_EXPECT(versions() == 2, "after A, B and both re-adds: %zu game versions recorded, expected 2", versions());

    // 4. Twenty round trips later the counts are still those of two archives, every
    //    replaced object is gone, each version appears once, and the mount order
    //    is the last re-add's.
    for (int trip = 0; trip < 20; trip++) {
        std::weak_ptr<Ship::Archive> oldA = latestA;
        std::weak_ptr<Ship::Archive> oldB = latestB;
        latestA.reset();
        latestB.reset();
        latestA = am->AddArchive(zipA);
        latestB = am->AddArchive(zipB);
        AAD_REQUIRE(latestA != nullptr && latestB != nullptr, "trip %d: a re-add was refused", trip);
        AAD_EXPECT(oldA.expired(), "trip %d: the replaced A object is still alive", trip);
        AAD_EXPECT(oldB.expired(), "trip %d: the replaced B object is still alive", trip);
    }
    AAD_EXPECT(archives() == 2, "after 20 round trips: %zu archives mounted, expected 2", archives());
    AAD_EXPECT(versions() == 2, "after 20 round trips: %zu game versions recorded, expected 2", versions());
    const auto allVersions = am->GetGameVersions();
    AAD_EXPECT(AadCountVersion(allVersions, kAadVersionA) == 1, "A's version is listed %zu times, expected 1",
               AadCountVersion(allVersions, kAadVersionA));
    AAD_EXPECT(AadCountVersion(allVersions, kAadVersionB) == 1, "B's version is listed %zu times, expected 1",
               AadCountVersion(allVersions, kAadVersionB));
    AAD_EXPECT(versionOrderIs(kAadVersionA, kAadVersionB), "after 20 round trips the version order is not [A, B]");
    AAD_EXPECT(sharedOwner() == zipB, "after 20 round trips %s is served by '%s', expected B", kAadSharedPath,
               sharedOwner().c_str());
    AAD_EXPECT(AadBytes(am->LoadFile(kAadOnlyAPath)) == onlyA, "A's own file stopped resolving after 20 round trips");

    // 5. A re-add re-reads what is on disk. A loose folder (the shape #705 mounts for a
    //    mod) gains one file and loses two between two adds: the second add must
    //    resolve the new file; a file only the old copy carried must stop resolving
    //    (asserted on the file map, not on a read, since the file is gone from disk
    //    either way); a file an earlier archive also carries must go to that archive
    //    (the newest remaining one, B); and the replaced copy must be released.
    const std::filesystem::path loose = dir / "aad-loose";
    const std::string loosePath = loose.generic_string();
    const std::string looseBytes = "archive-add-dedup: written between two adds";
    const std::filesystem::path looseDir = loose / "rsbs-test" / "archive-add-dedup";
    std::error_code ec;
    std::filesystem::create_directories(looseDir, ec);
    AAD_REQUIRE(AadWriteFile(looseDir / "loose-old", "old") && AadWriteFile(looseDir / "heir", "heir from the folder"),
                "could not write the loose folder");
    auto firstLoose = am->AddArchive(loosePath);
    AAD_REQUIRE(firstLoose != nullptr, "AddArchive refused the loose folder %s", loosePath.c_str());
    AAD_EXPECT(AadBytes(am->LoadFile(kAadLooseOldPath)) == "old", "loose-old did not resolve after the first add");
    AAD_EXPECT(am->GetArchiveFromFile(kAadHeirPath) == firstLoose,
               "the folder is newest but the heir path is not served by it");
    AAD_EXPECT(archives() == 3, "with the loose folder: %zu archives mounted, expected 3", archives());
    std::weak_ptr<Ship::Archive> weakFirstLoose = firstLoose;
    firstLoose.reset();
    std::filesystem::remove(looseDir / "loose-old", ec);
    std::filesystem::remove(looseDir / "heir", ec);
    AAD_REQUIRE(AadWriteFile(looseDir / "loose-new", looseBytes), "could not rewrite the loose folder");
    auto secondLoose = am->AddArchive(loosePath);
    AAD_REQUIRE(secondLoose != nullptr, "re-adding the loose folder was refused");
    AAD_EXPECT(AadBytes(am->LoadFile(kAadLooseNewPath)) == looseBytes, "the file written between two adds read \"%s\"",
               AadBytes(am->LoadFile(kAadLooseNewPath)).c_str());
    auto looseOldOwner = am->GetArchiveFromFile(kAadLooseOldPath);
    AAD_EXPECT(looseOldOwner == nullptr, "a file the new copy of the folder no longer carries still resolves into '%s'",
               looseOldOwner ? looseOldOwner->GetPath().c_str() : "(none)");
    looseOldOwner.reset();
    auto heirOwner = am->GetArchiveFromFile(kAadHeirPath);
    AAD_EXPECT(heirOwner != nullptr && heirOwner->GetPath() == zipB,
               "the heir path the folder dropped is served by '%s', expected B (the newest remaining archive "
               "that carries it)",
               heirOwner ? heirOwner->GetPath().c_str() : "(none)");
    heirOwner.reset();
    AAD_EXPECT(AadBytes(am->LoadFile(kAadHeirPath)) == heirB, "the heir path read \"%s\", expected B's bytes",
               AadBytes(am->LoadFile(kAadHeirPath)).c_str());
    AAD_EXPECT(weakFirstLoose.expired(), "the replaced loose-folder object is still alive after the re-add");
    AAD_EXPECT(archives() == 3, "after re-adding the loose folder: %zu archives mounted, expected 3", archives());

    // Leave the shared ArchiveManager as the row found it. RemoveArchive rebuilds the virtual file
    // system, which clears mGameVersions and re-adds only what the mounted archives carry, so a
    // version another part of the process recorded by hand (AddGameVersion) would be lost: put
    // those back.
    secondLoose.reset();
    latestA.reset();
    latestB.reset();
    am->RemoveArchive(zipA);
    am->RemoveArchive(zipB);
    am->RemoveArchive(loosePath);
    {
        auto have = am->GetGameVersions();
        for (uint32_t v : versionsAtStart) {
            auto it = std::find(have.begin(), have.end(), v);
            if (it != have.end()) {
                have.erase(it);
            } else {
                void (Ship::ArchiveManager::*add)(uint32_t) = &AadArchiveVersionAccess::AddGameVersion;
                ((*am).*add)(v);
            }
        }
    }

    if (failures != 0) {
        printf("[archive-add-dedup] %d assertion(s) failed\n", failures);
        fflush(stdout);
        return 1;
    }
    printf("[archive-add-dedup] PASS: 2 archives, 2 versions after 22 re-adds; replaced objects released; newest-wins "
           "held (A->B->A->B); a changed loose folder is re-read\n");
    fflush(stdout);
    return 0;
}

#undef AAD_EXPECT
#undef AAD_REQUIRE
#undef AAD_REPORT
