/**
 * @file test_otr_readd_releases_handle.cpp
 * @brief Lock for a re-added .otr archive releasing the copy it replaced (#1190).
 *
 * Every boot re-adds each mod archive after the curated half is mounted
 * (Combo_MountCuratedArchive -> ReapplyModArchives, the #577 M1 order), and
 * every game switch re-adds the arriving game's mods. ArchiveManager::AddArchive
 * replaces the mounted archive of an identical path with the freshly read one
 * (#987) and drops the old object without Unload()ing it. ~O2rArchive closes its
 * zip; ~OtrArchive did not close its MPQ, so every re-added .otr leaked its
 * StormLib handle, the OS file handle under it and its tables (#1190 measured
 * 409-410 process handles with the boot double mount against 403 without).
 *
 * OtrReaddReleasesHandle_RunHeadless is the ROM-free `otr-readd-releases-handle`
 * row. It is its own translation unit, not a file-scope include of
 * test_runner.cpp, because StormLib's header brings in <windows.h>. It writes a
 * one-file MPQ (plus StormLib's listfile, which OtrArchive::Open needs) into a
 * per-process scratch directory, adds it once and re-adds it once to settle,
 * records the process's open handle count (GetProcessHandleCount on Windows,
 * the /proc/self/fd entries on Linux, /dev/fd elsewhere), re-adds the same path
 * eight more times and requires the count to have grown by less than eight: a
 * leak costs at least one OS file handle per re-add.
 */

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// The value StormPort.h gives it, so its own unguarded define is not a redefinition.
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#endif

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#ifdef INCLUDE_MPQ_SUPPORT
// CMake links StormLib (through libultraship); keep its header from naming a library by pragma.
#ifndef STORMLIB_NO_AUTO_LINK
#define STORMLIB_NO_AUTO_LINK
#endif
#include <StormLib.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "../test_scratch.h"

namespace {

// AddGameVersion is protected; the same using-declaration access test_archive_add_dedup.c uses to
// put back a version RemoveArchive's file-system rebuild dropped.
struct OrhArchiveVersionAccess : Ship::ArchiveManager {
    using Ship::ArchiveManager::AddGameVersion;
};

constexpr const char* kOrhProbePath = "rsbs-test/otr-readd-releases-handle/probe";
constexpr const char* kOrhProbeBytes = "otr-readd-releases-handle: the one file in the probe MPQ";
constexpr int kOrhReadds = 8;

// The process's open handle (Windows) or descriptor (POSIX) count, or -1 when it cannot be read.
long OrhOpenHandleCount() {
#if defined(_WIN32)
    DWORD count = 0;
    if (!GetProcessHandleCount(GetCurrentProcess(), &count)) {
        return -1;
    }
    return static_cast<long>(count);
#else
    // The iterator's own descriptor is counted in every sample alike.
    std::error_code ec;
    const char* dir = std::filesystem::exists("/proc/self/fd", ec) ? "/proc/self/fd" : "/dev/fd";
    long count = 0;
    for (auto it = std::filesystem::directory_iterator(dir, ec); !ec && it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        count++;
    }
    return ec ? -1 : count;
#endif
}

#ifdef INCLUDE_MPQ_SUPPORT
bool OrhWriteMpq(const std::string& path, const std::string& bytes) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    HANDLE mpq = nullptr;
    if (!SFileCreateArchive(path.c_str(), MPQ_CREATE_LISTFILE | MPQ_CREATE_ARCHIVE_V1, 16, &mpq)) {
        return false;
    }
    HANDLE file = nullptr;
    bool ok =
        SFileCreateFile(mpq, kOrhProbePath, 0, static_cast<DWORD>(bytes.size()), 0, MPQ_FILE_REPLACEEXISTING, &file);
    if (ok) {
        ok = SFileWriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), 0);
        ok = SFileFinishFile(file) && ok;
    }
    // Closing writes the listfile and the tables.
    return SFileCloseArchive(mpq) && ok;
}
#endif

std::string OrhBytes(const std::shared_ptr<Ship::File>& file) {
    if (file == nullptr || file->Buffer == nullptr) {
        return "(not found)";
    }
    return std::string(file->Buffer->begin(), file->Buffer->end());
}

} // namespace

extern "C" int OtrReaddReleasesHandle_RunHeadless(void) {
#ifndef INCLUDE_MPQ_SUPPORT
    printf("[otr-readd-releases-handle] FAIL: this build has no MPQ support, so the row cannot lock anything\n");
    return 1;
#else
    auto ctx = Ship::Context::GetInstance();
    auto rm = ctx ? ctx->GetResourceManager() : nullptr;
    if (rm == nullptr || rm->GetArchiveManager() == nullptr) {
        printf("[otr-readd-releases-handle] FAIL: no ResourceManager/ArchiveManager after the bring-up\n");
        return 1;
    }
    auto am = rm->GetArchiveManager();

    const std::filesystem::path dir = rsbs::test::ScratchDir("otr-readd-releases-handle");
    const std::string mpq = (dir / "orh-probe.otr").string();
    if (!OrhWriteMpq(mpq, kOrhProbeBytes)) {
        printf("[otr-readd-releases-handle] FAIL: StormLib could not write %s\n", mpq.c_str());
        return 1;
    }
    const std::vector<uint32_t> versionsAtStart = am->GetGameVersions();
    int failures = 0;

    // A first add and one settling re-add: anything the first re-add allocates once (a log line's
    // buffers, a lazily created handle) is in the baseline, not in the growth.
    if (am->AddArchive(mpq) == nullptr || am->AddArchive(mpq) == nullptr) {
        printf("[otr-readd-releases-handle] FAIL: AddArchive refused %s\n", mpq.c_str());
        am->RemoveArchive(mpq);
        return 1;
    }
    if (OrhBytes(am->LoadFile(kOrhProbePath)) != kOrhProbeBytes) {
        printf("[otr-readd-releases-handle] FAIL: the probe file read \"%s\" from the mounted MPQ\n",
               OrhBytes(am->LoadFile(kOrhProbePath)).c_str());
        failures++;
    }

    const long before = OrhOpenHandleCount();
    for (int i = 0; i < kOrhReadds; i++) {
        if (am->AddArchive(mpq) == nullptr) {
            printf("[otr-readd-releases-handle] FAIL: re-add %d of %s was refused\n", i + 1, mpq.c_str());
            failures++;
        }
    }
    const long after = OrhOpenHandleCount();
    printf("[otr-readd-releases-handle] open handles: %ld before %d re-adds, %ld after (growth %ld)\n", before,
           kOrhReadds, after, after - before);
    if (before < 0 || after < 0) {
        printf("[otr-readd-releases-handle] FAIL: could not read the process's open handle count\n");
        failures++;
    } else if (after - before >= kOrhReadds) {
        printf("[otr-readd-releases-handle] FAIL: %d re-adds of one .otr grew the open handles by %ld: every "
               "replaced archive kept its MPQ open\n",
               kOrhReadds, after - before);
        failures++;
    }
    if (OrhBytes(am->LoadFile(kOrhProbePath)) != kOrhProbeBytes) {
        printf("[otr-readd-releases-handle] FAIL: after the re-adds the probe file read \"%s\"\n",
               OrhBytes(am->LoadFile(kOrhProbePath)).c_str());
        failures++;
    }

    // Leave the shared ArchiveManager as the row found it (RemoveArchive rebuilds the file system,
    // which re-derives mGameVersions from the mounted archives only).
    am->RemoveArchive(mpq);
    {
        auto have = am->GetGameVersions();
        for (uint32_t v : versionsAtStart) {
            auto it = std::find(have.begin(), have.end(), v);
            if (it != have.end()) {
                have.erase(it);
            } else {
                void (Ship::ArchiveManager::*add)(uint32_t) = &OrhArchiveVersionAccess::AddGameVersion;
                ((*am).*add)(v);
            }
        }
    }

    if (failures != 0) {
        printf("[otr-readd-releases-handle] %d assertion(s) failed\n", failures);
        fflush(stdout);
        return 1;
    }
    printf("[otr-readd-releases-handle] PASS: %d re-adds of one .otr left the open handle count where it was\n",
           kOrhReadds);
    fflush(stdout);
    return 0;
#endif
}
