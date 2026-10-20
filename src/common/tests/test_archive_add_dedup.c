/**
 * @file test_archive_add_dedup.c
 * @brief Lock for ArchiveManager::AddArchive de-duplicating a re-added path (#987).
 *
 * Every game switch re-adds the arriving game's archives (and its curated half
 * and mods) by path. libultraship used to open a second O2rArchive for each
 * call and append it to mArchives and mGameVersions, so every round trip left
 * one more zip handle and one more file list per re-added archive, and the
 * game-version list grew by one entry per versioned archive. The fork's
 * AddArchive now returns the mounted archive for an identical path and
 * re-registers it on top, which is what a fresh add did to the lookup order.
 *
 * ArchiveAddDedup_RunHeadless is the ROM-free `archive-add-dedup` row, included
 * at FILE SCOPE by test_runner.cpp (compiled as C++, it drives the C++-linkage
 * ArchiveManager directly). It builds two temporary O2R zips that carry a
 * version entry and one shared path with different bytes, then asserts:
 *   1. a first add grows the archive count and the game-version list by one;
 *   2. adding the same path again returns the same archive object and grows
 *      neither, and a file only that archive carries still resolves;
 *   3. newest-wins survives the re-add: with A then B mounted, re-adding A
 *      makes the shared path resolve to A again, and re-adding B back to B;
 *   4. repeated alternating re-adds (a long play session of switches) leave the
 *      counts where two archives put them.
 */

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <zip.h>

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

constexpr const char* kAadSharedPath = "rsbs-test/archive-add-dedup/shared";
constexpr const char* kAadOnlyAPath = "rsbs-test/archive-add-dedup/only-a";
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
    AAD_REQUIRE(AadWriteZip(zipA, { { "version", AadVersionEntry(kAadVersionA) },
                                    { kAadSharedPath, bytesA },
                                    { kAadOnlyAPath, onlyA } }),
                "could not write %s", zipA.c_str());
    AAD_REQUIRE(AadWriteZip(zipB, { { "version", AadVersionEntry(kAadVersionB) }, { kAadSharedPath, bytesB } }),
                "could not write %s", zipB.c_str());

    const size_t baseArchives = am->GetArchives()->size();
    const size_t baseVersions = am->GetGameVersions().size();
    auto archives = [&]() { return am->GetArchives()->size() - baseArchives; };
    auto versions = [&]() { return am->GetGameVersions().size() - baseVersions; };
    auto sharedOwner = [&]() {
        auto owner = am->GetArchiveFromFile(kAadSharedPath);
        return owner ? owner->GetPath() : std::string("(none)");
    };

    // 1. A first add mounts one archive and records its version once.
    auto firstA = am->AddArchive(zipA);
    AAD_REQUIRE(firstA != nullptr, "AddArchive refused %s", zipA.c_str());
    AAD_EXPECT(archives() == 1, "after adding A: %zu archives mounted, expected 1", archives());
    AAD_EXPECT(versions() == 1, "after adding A: %zu game versions recorded, expected 1", versions());

    // 2. The same path again: same object, nothing grows, A's own file resolves.
    auto secondA = am->AddArchive(zipA);
    AAD_EXPECT(secondA == firstA, "re-adding A returned %p, not the mounted archive %p", (void*)secondA.get(),
               (void*)firstA.get());
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
    am->AddArchive(zipA);
    AAD_EXPECT(sharedOwner() == zipA, "A was re-added last but %s is served by '%s'", kAadSharedPath,
               sharedOwner().c_str());
    AAD_EXPECT(AadBytes(am->LoadFile(kAadSharedPath)) == bytesA, "shared path read \"%s\" after A was re-added",
               AadBytes(am->LoadFile(kAadSharedPath)).c_str());
    am->AddArchive(zipB);
    AAD_EXPECT(sharedOwner() == zipB, "B was re-added last but %s is served by '%s'", kAadSharedPath,
               sharedOwner().c_str());
    AAD_EXPECT(AadBytes(am->LoadFile(kAadSharedPath)) == bytesB, "shared path read \"%s\" after B was re-added",
               AadBytes(am->LoadFile(kAadSharedPath)).c_str());
    AAD_EXPECT(archives() == 2, "after A, B and both re-adds: %zu archives mounted, expected 2", archives());
    AAD_EXPECT(versions() == 2, "after A, B and both re-adds: %zu game versions recorded, expected 2", versions());

    // 4. Twenty round trips later the counts are still those of two archives, each
    //    version appears once, and the mount order is the last re-add's.
    for (int trip = 0; trip < 20; trip++) {
        AAD_EXPECT(am->AddArchive(zipA) == firstA, "trip %d: A came back as a different object", trip);
        AAD_EXPECT(am->AddArchive(zipB) == firstB, "trip %d: B came back as a different object", trip);
    }
    AAD_EXPECT(archives() == 2, "after 20 round trips: %zu archives mounted, expected 2", archives());
    AAD_EXPECT(versions() == 2, "after 20 round trips: %zu game versions recorded, expected 2", versions());
    const auto allVersions = am->GetGameVersions();
    AAD_EXPECT(AadCountVersion(allVersions, kAadVersionA) == 1, "A's version is listed %zu times, expected 1",
               AadCountVersion(allVersions, kAadVersionA));
    AAD_EXPECT(AadCountVersion(allVersions, kAadVersionB) == 1, "B's version is listed %zu times, expected 1",
               AadCountVersion(allVersions, kAadVersionB));
    auto mounted = am->GetArchives();
    AAD_EXPECT(!mounted->empty() && mounted->back() == firstB,
               "the last re-add (B) is not last in the mount order (back is '%s')",
               mounted->empty() ? "(empty)" : mounted->back()->GetPath().c_str());
    AAD_EXPECT(sharedOwner() == zipB, "after 20 round trips %s is served by '%s', expected B", kAadSharedPath,
               sharedOwner().c_str());
    AAD_EXPECT(AadBytes(am->LoadFile(kAadOnlyAPath)) == onlyA, "A's own file stopped resolving after 20 round trips");

    // Leave the shared ArchiveManager as the row found it.
    am->RemoveArchive(zipA);
    am->RemoveArchive(zipB);

    if (failures != 0) {
        printf("[archive-add-dedup] %d assertion(s) failed\n", failures);
        fflush(stdout);
        return 1;
    }
    printf("[archive-add-dedup] PASS: 2 archives, 2 versions after 22 re-adds; newest-wins held (A->B->A->B)\n");
    fflush(stdout);
    return 0;
}

#undef AAD_EXPECT
#undef AAD_REQUIRE
#undef AAD_REPORT
