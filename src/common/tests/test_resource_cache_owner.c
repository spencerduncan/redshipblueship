/**
 * @file test_resource_cache_owner.c
 * @brief Locks for the per-game resource cache owner (#981).
 *
 * libultraship caches every parsed resource under {path, owner, archive}, and
 * every path-only load takes the owner from ResourceManager's default. With one
 * default for both games, the first game to parse a path fixed the copy both
 * games got for the rest of the process: OoT's 37-limb gOctorokSkel reached
 * MM's 16-entry joint tables. The fix gives each game its own owner at the
 * switch (rsbs/src/main.cpp, ApplyResourceCacheOwner) through the fork's
 * ResourceManager::SetDefaultCacheOwner.
 *
 * Two pieces live here, both included at FILE SCOPE by test_runner.cpp
 * (compiled as C++, they drive the C++-linkage ResourceManager directly):
 *
 *   ResourceCacheOwner_RunHeadless: the ROM-free `resource-cache-owner` row. It
 *     builds two O2R zips with different Blob contents at one path, mounts A
 *     and loads under owner X, mounts B and loads under owner Y, then loads
 *     under X again. Under Y it must get B's bytes; under X, A's bytes in A's
 *     original object, although B now owns the path in the archive layer.
 *
 *   ResourceCacheOwner_CheckArrival: one arrival's checks for the two ROM-staged
 *     rows (IntArchiveHotswapCycle and IntResourceCacheOwner), called from each
 *     game's GameExports_SingleExe.cpp with the skeleton its own loader served.
 */

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/type/Blob.h>

#include <zip.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../game.h"
#include "../test_scratch.h"

namespace {

constexpr const char* kRcoBlobPath = "rsbs-test/resource-cache-owner/blob";
// Neither is 0 (the booted game's owner) nor main.cpp's other-game owner, so the
// row never shares an entry with a live game's.
constexpr uintptr_t kRcoOwnerX = 0x52434F58u; // "RCOX"
constexpr uintptr_t kRcoOwnerY = 0x52434F59u; // "RCOY"

// One binary O2R resource: the 64-byte header ResourceLoader reads
// (ReadResourceInitDataBinary: byte order, custom flag, two unused bytes, type,
// version, id), then the Blob V0 body (u32 size, bytes). Little-endian.
std::string RcoBlobFile(const std::string& payload) {
    std::string f(OTR_HEADER_SIZE, '\0');
    auto put32 = [&f](size_t at, uint32_t v) {
        for (int i = 0; i < 4; i++) {
            f[at + i] = (char)((v >> (8 * i)) & 0xFF);
        }
    };
    f[0] = 0; // Endianness::Little
    put32(4, static_cast<uint32_t>(Ship::ResourceType::Blob));
    put32(8, 0);
    const uint32_t size = (uint32_t)payload.size();
    std::string body(4, '\0');
    for (int i = 0; i < 4; i++) {
        body[i] = (char)((size >> (8 * i)) & 0xFF);
    }
    return f + body + payload;
}

bool RcoWriteZip(const std::string& path, const std::string& entryName, const std::string& data) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    int err = 0;
    zip_t* za = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (za == nullptr) {
        return false;
    }
    zip_source_t* src = zip_source_buffer(za, data.data(), data.size(), 0);
    if (src == nullptr || zip_file_add(za, entryName.c_str(), src, ZIP_FL_ENC_UTF_8) < 0) {
        if (src != nullptr) {
            zip_source_free(src);
        }
        zip_discard(za);
        return false;
    }
    return zip_close(za) == 0;
}

std::string RcoBlobBytes(const std::shared_ptr<Ship::IResource>& res) {
    auto blob = std::dynamic_pointer_cast<Ship::Blob>(res);
    if (blob == nullptr) {
        return "(not a Blob)";
    }
    return std::string(blob->Data.begin(), blob->Data.end());
}

} // namespace

// RCO_REQUIRE stops the row (what follows needs it); RCO_EXPECT reports the
// failure and carries on, so one run shows every assertion that fails.
#define RCO_REPORT(...)                          \
    do {                                         \
        printf("[resource-cache-owner] FAIL: "); \
        printf(__VA_ARGS__);                     \
        printf("\n");                            \
        fflush(stdout);                          \
    } while (0)
#define RCO_REQUIRE(cond, ...)                    \
    do {                                          \
        if (!(cond)) {                            \
            RCO_REPORT(__VA_ARGS__);              \
            rm->SetDefaultCacheOwner(savedOwner); \
            return 1;                             \
        }                                         \
    } while (0)
#define RCO_EXPECT(cond, ...)        \
    do {                             \
        if (!(cond)) {               \
            RCO_REPORT(__VA_ARGS__); \
            failures++;              \
        }                            \
    } while (0)

// The ROM-free row's body. The caller has done the display-free shared bring-up
// and registered the Blob factory. Loads go through LoadResourceProcess (the
// path-only overload Fast3D's interpreter calls) because the resource pool is
// paused in a bring-up with no archive; the C bridge's ResourceGetDataByName is
// then checked against the same objects, which it reaches as cache hits.
int ResourceCacheOwner_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    auto rm = ctx ? ctx->GetResourceManager() : nullptr;
    if (rm == nullptr || rm->GetArchiveManager() == nullptr) {
        printf("[resource-cache-owner] FAIL: no ResourceManager/ArchiveManager after the bring-up\n");
        return 1;
    }
    auto am = rm->GetArchiveManager();
    const uintptr_t savedOwner = rm->GetDefaultCacheOwner();
    int failures = 0;

    const std::filesystem::path dir = rsbs::test::ScratchDir("resource-cache-owner");
    const std::string zipA = (dir / "rco-a.o2r").string();
    const std::string zipB = (dir / "rco-b.o2r").string();
    const std::string bytesA = "resource-cache-owner: archive A's blob";
    const std::string bytesB = "resource-cache-owner: archive B's blob, a different length";
    RCO_REQUIRE(RcoWriteZip(zipA, kRcoBlobPath, RcoBlobFile(bytesA)), "could not write %s", zipA.c_str());
    RCO_REQUIRE(RcoWriteZip(zipB, kRcoBlobPath, RcoBlobFile(bytesB)), "could not write %s", zipB.c_str());

    // 1. The setter is what the getter reports.
    rm->SetDefaultCacheOwner(kRcoOwnerX);
    RCO_EXPECT(rm->GetDefaultCacheOwner() == kRcoOwnerX, "GetDefaultCacheOwner() = 0x%llx after setting 0x%llx",
               (unsigned long long)rm->GetDefaultCacheOwner(), (unsigned long long)kRcoOwnerX);

    // 2. Owner X parses A.
    RCO_REQUIRE(am->AddArchive(zipA) != nullptr, "AddArchive refused %s", zipA.c_str());
    auto resA = rm->LoadResourceProcess(kRcoBlobPath);
    RCO_REQUIRE(resA != nullptr, "owner X: no resource at %s from archive A", kRcoBlobPath);
    RCO_REQUIRE(RcoBlobBytes(resA) == bytesA, "owner X: got \"%s\", expected archive A's \"%s\"",
                RcoBlobBytes(resA).c_str(), bytesA.c_str());
    void* const rawA = resA->GetRawPointer();

    // 3. B now owns the path in the archive layer. Owner Y must get B's bytes,
    //    in an object of its own: the cache key carries the owner.
    RCO_REQUIRE(am->AddArchive(zipB) != nullptr, "AddArchive refused %s", zipB.c_str());
    auto owningArchive = am->GetArchiveFromFile(kRcoBlobPath);
    RCO_REQUIRE(owningArchive != nullptr && owningArchive->GetPath() == zipB,
                "archive B does not own %s after it was added (owner '%s')", kRcoBlobPath,
                owningArchive ? owningArchive->GetPath().c_str() : "(none)");
    rm->SetDefaultCacheOwner(kRcoOwnerY);
    auto resB = rm->LoadResourceProcess(kRcoBlobPath);
    RCO_REQUIRE(resB != nullptr, "owner Y: no resource at %s", kRcoBlobPath);
    RCO_EXPECT(RcoBlobBytes(resB) == bytesB, "owner Y: got \"%s\", expected archive B's \"%s\"",
               RcoBlobBytes(resB).c_str(), bytesB.c_str());
    RCO_EXPECT(resB.get() != resA.get(), "owner Y was served owner X's object %p", (void*)resA.get());

    // 4. Back under X: A's original object and bytes, not a reparse from B and
    //    not a fresh copy. Setting the owner freed and dirtied nothing.
    rm->SetDefaultCacheOwner(kRcoOwnerX);
    auto resA2 = rm->LoadResourceProcess(kRcoBlobPath);
    RCO_EXPECT(resA2.get() == resA.get(), "owner X: served %p, not its original object %p", (void*)resA2.get(),
               (void*)resA.get());
    RCO_EXPECT(RcoBlobBytes(resA2) == bytesA, "owner X: got \"%s\" after the switch, expected archive A's \"%s\"",
               RcoBlobBytes(resA2).c_str(), bytesA.c_str());
    RCO_EXPECT(!resA->IsDirty(), "owner X's object was dirtied by the owner switches");

    // 5. The C bridge (ResourceGetDataByName -> GetResourceRawPointer ->
    //    LoadResource(path)) keys by the same owner.
    void* bridgeX = rm->GetResourceRawPointer(kRcoBlobPath);
    RCO_EXPECT(bridgeX == rawA, "bridge under owner X: %p, expected A's data %p", bridgeX, rawA);
    rm->SetDefaultCacheOwner(kRcoOwnerY);
    void* bridgeY = rm->GetResourceRawPointer(kRcoBlobPath);
    RCO_EXPECT(bridgeY == resB->GetRawPointer(), "bridge under owner Y: %p, expected B's data %p", bridgeY,
               resB->GetRawPointer());

    rm->SetDefaultCacheOwner(savedOwner);
    if (failures != 0) {
        printf("[resource-cache-owner] %d assertion(s) failed\n", failures);
        fflush(stdout);
        return 1;
    }
    printf("[resource-cache-owner] PASS: owner X kept archive A's object %p (\"%s\"), owner Y got archive B's %p "
           "(\"%s\"), bridge agrees\n",
           (void*)resA.get(), bytesA.c_str(), (void*)resB.get(), bytesB.c_str());
    fflush(stdout);
    return 0;
}

// #1040 R4: the ROM-free `resource-cache-owner-unload-filter` row. A ResourceFilter
// carries its own owner, and DirtyResources honours it; UnloadResourcesProcess
// once unloaded under mDefaultCacheOwner instead. That is the path an
// UnloadResourcesAsync queued before a game switch and run after it takes: the
// owner was captured into the filter at queue time, and the default has since
// become the arriving game's. Here owner X and owner Y each cache their own
// copy of one path; with Y the default, an unload filtered to X must drop X's
// entry and leave Y's. Same bring-up as the row above.
int ResourceCacheOwner_RunUnloadFilterHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    auto rm = ctx ? ctx->GetResourceManager() : nullptr;
    if (rm == nullptr || rm->GetArchiveManager() == nullptr) {
        printf("[resource-cache-owner] FAIL: no ResourceManager/ArchiveManager after the bring-up\n");
        return 1;
    }
    auto am = rm->GetArchiveManager();
    const uintptr_t savedOwner = rm->GetDefaultCacheOwner();
    int failures = 0;

    const std::filesystem::path dir = rsbs::test::ScratchDir("resource-cache-owner-unload-filter");
    const std::string zip = (dir / "rco-unload.o2r").string();
    const std::string bytes = "resource-cache-owner: the unload-filter row's blob";
    RCO_REQUIRE(RcoWriteZip(zip, kRcoBlobPath, RcoBlobFile(bytes)), "could not write %s", zip.c_str());
    RCO_REQUIRE(am->AddArchive(zip) != nullptr, "AddArchive refused %s", zip.c_str());

    // 1. Each owner caches its own copy.
    rm->SetDefaultCacheOwner(kRcoOwnerX);
    auto resX = rm->LoadResourceProcess(kRcoBlobPath);
    RCO_REQUIRE(resX != nullptr, "owner X: no resource at %s", kRcoBlobPath);
    rm->SetDefaultCacheOwner(kRcoOwnerY);
    auto resY = rm->LoadResourceProcess(kRcoBlobPath);
    RCO_REQUIRE(resY != nullptr, "owner Y: no resource at %s", kRcoBlobPath);
    RCO_REQUIRE(resY.get() != resX.get(), "owner Y was served owner X's object %p", (void*)resX.get());
    // Anti-vacuity: a plain reload is a cache hit for either owner.
    RCO_REQUIRE(rm->LoadResourceProcess(kRcoBlobPath).get() == resY.get(),
                "owner Y: a second load reparsed instead of hitting the cache");

    // 2. With Y the default, unload X's entry through a filter naming X.
    rm->UnloadResources(Ship::ResourceFilter({ kRcoBlobPath }, {}, kRcoOwnerX, nullptr));

    // 3. Y keeps its entry: still the very object it had.
    auto resY2 = rm->LoadResourceProcess(kRcoBlobPath);
    RCO_EXPECT(resY2.get() == resY.get(),
               "owner Y: served %p after an unload filtered to owner X, not its cached %p (the unload took the "
               "default owner instead of the filter's)",
               (void*)resY2.get(), (void*)resY.get());

    // 4. X lost its entry: a load under X parses a new object. resX still holds
    //    the old one, so a new object cannot reuse its address.
    rm->SetDefaultCacheOwner(kRcoOwnerX);
    auto resX2 = rm->LoadResourceProcess(kRcoBlobPath);
    RCO_EXPECT(resX2 != nullptr && resX2.get() != resX.get(),
               "owner X: still served its cached %p after an unload filtered to owner X", (void*)resX.get());
    RCO_EXPECT(RcoBlobBytes(resX2) == bytes, "owner X: reparsed \"%s\", expected \"%s\"", RcoBlobBytes(resX2).c_str(),
               bytes.c_str());

    rm->SetDefaultCacheOwner(savedOwner);
    if (failures != 0) {
        printf("[resource-cache-owner] %d assertion(s) failed\n", failures);
        fflush(stdout);
        return 1;
    }
    printf("[resource-cache-owner] PASS: an unload filtered to owner X dropped X's %p and kept owner Y's %p\n",
           (void*)resX.get(), (void*)resY.get());
    fflush(stdout);
    return 0;
}

#undef RCO_EXPECT
#undef RCO_REQUIRE
#undef RCO_REPORT

// ---------------------------------------------------------------------------
// One arrival's checks for the ROM-staged rows
// ---------------------------------------------------------------------------

namespace {

constexpr const char* kRcoBoxVtxPath = "objects/object_box/object_boxVtx_0004B0";

struct RcoGameRecord {
    const void* firstSkel = nullptr;
    int firstArrival = 0;
    std::string servedBoxVtx;
};
RcoGameRecord sRcoRecords[3]; // indexed by GameId

std::string RcoResourceBytes(const std::shared_ptr<Ship::IResource>& res) {
    if (res == nullptr || res->GetRawPointer() == nullptr) {
        return "";
    }
    const char* p = (const char*)res->GetRawPointer();
    return std::string(p, p + res->GetPointerSize());
}

bool RcoEndsWith(const std::string& s, const char* suffix) {
    const size_t n = strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

// Appends one failure to @p out ("; "-separated), so a check reports every
// assertion that failed and not just the first.
void RcoAppend(std::string& out, const char* fmt, ...) {
    char line[384];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (!out.empty()) {
        out += "; ";
    }
    out += line;
}

bool sRcoMMLegPassed = false;

} // namespace

extern "C" {

// IntResourceCacheOwner's hand-back: MM sets it when its leg passed, just before
// it walks back through the Clock Tower door; OoT's return check waits for it.
void ResourceCacheOwner_SetMMLegPassed(void) {
    sRcoMMLegPassed = true;
}

int ResourceCacheOwner_MMLegPassed(void) {
    return sRcoMMLegPassed ? 1 : 0;
}

/**
 * The arriving game's view of two colliding paths, ~10 frames into arrival
 * @p arrival. @p skel and @p limbCount are what the arriving game's own loader
 * (ResourceMgr_LoadSkeletonByName) served for objects/object_okuta/gOctorokSkel:
 *   - its header's limb count is the arriving game's (OoT 37, MM 15);
 *   - a later arrival of the same game gets the very object its first arrival
 *     got (the switch freed nothing and parsed nothing again);
 *   - the served objects/object_box/object_boxVtx_0004B0 vertices equal a fresh
 *     parse of the file in the arriving game's own archive (oot.o2r / mm.o2r),
 *     read raw through GetArchiveFromFile/LoadFile, and differ from what the
 *     other game was served (so the comparison can tell the two copies apart).
 * Every assertion is evaluated; @p msg gets all that failed, or a summary.
 * @return 1 on pass, 0 on any failure.
 */
int ResourceCacheOwner_CheckArrival(GameId game, int arrival, const void* skel, int limbCount, char* msg, size_t cap) {
    if (game != GAME_OOT && game != GAME_MM) {
        snprintf(msg, cap, "arrival #%d of no game", arrival);
        return 0;
    }
    std::string fail;
    const char* name = game == GAME_OOT ? "OoT" : "MM";
    const int wantLimbs = game == GAME_OOT ? 37 : 15;
    if (skel == nullptr || limbCount != wantLimbs) {
        RcoAppend(fail, "gOctorokSkel %p has %d limbs, expected %s's %d", skel, limbCount, name, wantLimbs);
    }
    RcoGameRecord& rec = sRcoRecords[game];
    if (rec.firstSkel == nullptr) {
        rec.firstSkel = skel;
        rec.firstArrival = arrival;
    } else if (rec.firstSkel != skel) {
        RcoAppend(fail, "gOctorokSkel is %p, not the %p %s's arrival #%d was served", skel, rec.firstSkel, name,
                  rec.firstArrival);
    }

    auto rm = Ship::Context::GetInstance()->GetResourceManager();
    auto archive = rm->GetArchiveManager()->GetArchiveFromFile(kRcoBoxVtxPath);
    const char* wantArchive = game == GAME_OOT ? "oot.o2r" : "mm.o2r";
    if (archive == nullptr || !RcoEndsWith(archive->GetPath(), wantArchive)) {
        RcoAppend(fail, "%s is owned by '%s', not %s", kRcoBoxVtxPath, archive ? archive->GetPath().c_str() : "(none)",
                  wantArchive);
    } else {
        auto file = archive->LoadFile(std::string(kRcoBoxVtxPath));
        auto fresh = (file != nullptr) ? rm->GetResourceLoader()->LoadResource(kRcoBoxVtxPath, file) : nullptr;
        const std::string expected = RcoResourceBytes(fresh);
        const std::string served = RcoResourceBytes(rm->LoadResource(kRcoBoxVtxPath));
        if (expected.empty() || served != expected) {
            RcoAppend(fail, "served %s (%zu bytes) differs from %s's own (%zu bytes)", kRcoBoxVtxPath, served.size(),
                      wantArchive, expected.size());
        }
        rec.servedBoxVtx = served;
        const RcoGameRecord& other = sRcoRecords[game == GAME_OOT ? GAME_MM : GAME_OOT];
        if (!other.servedBoxVtx.empty() && other.servedBoxVtx == served) {
            RcoAppend(fail, "both games were served byte-identical %s", kRcoBoxVtxPath);
        }
    }

    if (!fail.empty()) {
        snprintf(msg, cap, "%s arrival #%d: %s", name, arrival, fail.c_str());
        return 0;
    }
    snprintf(msg, cap, "%s arrival #%d: gOctorokSkel %p with %d limbs (first served at arrival #%d), %s matches %s",
             name, arrival, skel, limbCount, rec.firstArrival, kRcoBoxVtxPath, wantArchive);
    return 1;
}

} // extern "C"
