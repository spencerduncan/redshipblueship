/**
 * @file test_mm_extension_rescan.c
 * @brief #618 (#516 Phase 3): a rescan after MM's archives mount must ADD
 *        MM-scoped ExtensionCache entries and remove none of OoT's.
 *
 * CTest row MMExtensionRescan (label "redship"), dispatch "mm-extension-rescan"
 * in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. `ExtensionCache` (games/oot/soh/OTRGlobals.cpp) is the ONE
 * shared map `ResourceMgr_FileExists` / `ResourceMgr_FileAltExists` answer from,
 * and `OTRExtScanner()` fills it exactly once, from OoT's `InitOTRImpl`. MM's
 * archives are not mounted at that moment — `LoadMMArchives()` runs later, inside
 * `MM_Game_Init` — and MM's own copy of the scanner lives in the excluded
 * `2s2h/BenPort.cpp`, so nothing ever rescanned. Every MM path was therefore
 * missing, and the two entries BenPort's `InitOTR` ends with were the complete
 * remainder of PR #616's audit: `OTRExtScanner` and `PlayerCustomFlipbooks_Patch`
 * both read false for everything (MM's HD gfxprint font; the static FD/Deku/Goron
 * eye and mouth flipbooks).
 *
 * WHAT THIS ROW ADDS OVER MMRegistrarCoverage. That row proves MM_Rando_Init
 * REACHED both entries, in order, but it is ROM-free: with no MM archive mounted
 * the scan has nothing to find, so it cannot show the rescan does its job. This
 * row mounts a real MM-side archive and asserts the CONTENT contract:
 *
 *   1. a path that exists only in the MM archive is absent from the cache before
 *      the rescan and present after;
 *   2. the cache size grows by EXACTLY the number of new keys the scan reported,
 *      which is what makes "purely additive" a measurement rather than an
 *      argument — an erase anywhere would break the equality;
 *   3. a sample of OoT-owned paths still resolves afterwards, path by path;
 *   4. a second rescan adds zero and changes nothing (idempotent across the
 *      repeated switches that re-add both games' archives);
 *   5. scoping is real in both directions: a "mm" scan with no MM archive
 *      recorded adds nothing, and an "oot" scan after the MM mount adds nothing
 *      (every OoT-owned path is already keyed).
 *
 * ANTI-VACUITY. The OoT baseline must be non-empty, the OoT sample must be
 * non-empty, and the probe path must be one the cache genuinely lacked — it is
 * CHOSEN by that property, from the live archive listing, rather than hardcoded.
 * If the MM archive contributes no path the cache lacks (it would have to be a
 * subset of soh.o2r), the row SKIPs with that stated instead of passing on an
 * empty comparison.
 *
 * WHY THE MOUNT GOES THROUGH MM. `Combo_ExtensionCache_ScanGame`'s scope keys on
 * MM's own archive-path registry (`Combo_ArchivePathIsMM`, fed by
 * `RecordMMArchivePath`), the same registry the #344 Room/Cutscene factory
 * dispatcher uses. A bare `AddArchive` from here would mount an archive that the
 * "mm" scope could never match, and the row would fail for the wrong reason. It
 * mounts through `MM_MountArchiveHeadless`, which makes exactly the pair of calls
 * `LoadMMArchives()` makes per archive and deliberately does NOT latch
 * `sMMArchivesLoaded` (see its doc comment) — so this row has no ordering
 * coupling to any other row in a shared `--test all` process.
 *
 * SKIPs when 2ship.o2r is not staged, the same policy as the #560/#595 archive
 * rows: the netplay-relay CI job re-runs this label archive-less on purpose.
 * 2ship.o2r rather than mm.o2r on purpose too — mm.o2r is ROM-derived and cannot
 * be distributed, so a row needing it would skip on every CI run.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++): it drives the
 * C++-linkage Ship::Context / ResourceManager APIs directly.
 */

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

extern "C" {
// OoT's boot-time scanner (games/oot/soh/OTRGlobals.cpp) — the production call
// that establishes the OoT-side baseline this row measures against. Run here
// because the display-free shared bring-up mounts soh.o2r but does not scan.
void OTRExtScanner(void);
// The per-game rescan and the size accessor under test
// (games/oot/soh/ResourceManagerHelpers.cpp, #618).
int Combo_ExtensionCache_ScanGame(const char* gameTag);
size_t Combo_ExtensionCache_Size(void);
// The cache's own consumers, declared rather than reached through OoT's
// ResourceManagerHelpers.h (which pulls OoT's z64* umbrella headers).
unsigned char ResourceMgr_FileExists(const char* resName);
char** ResourceMgr_ListFilesForGame(const char* gameTag, const char* searchMask, int* resultSize);
// MM's side (games/mm/2s2h/GameExports_SingleExe.cpp): the archive mount seam
// and the production rescan entry point plus its state accessors.
int MM_MountArchiveHeadless(const char* path);
void MM_ExtensionRescan_AfterArchiveMount(void);
int MM_ExtensionRescan_Scanned(void);
int MM_ExtensionRescan_FlipbooksPatchedAfterScan(void);
int MM_ExtensionRescan_CallCount(void);
int MM_ExtensionRescan_LastEntriesAdded(void);
}

namespace {

#define MER_ASSERT(cond, code, msg)                                                     \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

// Returned by the body when the staged archives cannot support a non-vacuous
// comparison. Test_MMExtensionRescan maps it onto TEST_SKIP, and the CTest row
// declares SKIP_RETURN_CODE 77.
constexpr int kMerSkip = 77;

// How many OoT-owned paths to re-check individually after the MM rescan. The
// size accounting already proves nothing was erased; this is the direct,
// per-path form of the same claim, bounded so the row stays fast.
constexpr size_t kMerOotSampleTarget = 200;

// Collect up to `want` extension-less paths from one game's scope. Extension-less
// on purpose: ExtensionCache keys a dotted path by its stem, so
// ResourceMgr_FileExists(pathWithExtension) would miss by design and the check
// would be testing the wrong thing. Every extracted base resource is
// extension-less, so this is the common case, not a corner.
std::vector<std::string> MerCollectScopedPaths(const char* gameTag, size_t want, int* totalOut) {
    std::vector<std::string> picked;
    int listSize = 0;
    char** files = ResourceMgr_ListFilesForGame(gameTag, "*", &listSize);
    if (totalOut != nullptr) {
        *totalOut = listSize;
    }
    if (files == nullptr) {
        return picked;
    }
    for (int i = 0; i < listSize; i++) {
        if (files[i] != nullptr) {
            if (picked.size() < want && strchr(files[i], '.') == nullptr) {
                picked.emplace_back(files[i]);
            }
            free(files[i]);
        }
    }
    free(files);
    return picked;
}

std::string MerResolveArchive(const char* filename) {
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

} // namespace

/**
 * @param mmArchivePath a staged MM-side archive (2ship.o2r), already resolved by
 *                      the wrapper. Mounted here, through MM's own recorder.
 * @return 0 pass, kMerSkip when the staged archives cannot support a non-vacuous
 *         comparison, any other non-zero on failure.
 */
extern "C" int MMExtensionRescan_RunHeadless(const char* mmArchivePath) {
    printf("[TEST] mm-extension-rescan: a rescan after MM's archives mount adds MM entries and keeps OoT's "
           "(#618)\n");

    auto ctx = Ship::Context::GetInstance();
    MER_ASSERT(ctx != nullptr && ctx->GetResourceManager() != nullptr &&
                   ctx->GetResourceManager()->GetArchiveManager() != nullptr,
               1, "no Ship::Context/ResourceManager/ArchiveManager — run the shared bring-up first");
    MER_ASSERT(mmArchivePath != nullptr && mmArchivePath[0] != '\0', 1, "no MM archive path supplied");

    // ---- OoT baseline: the scan a real OoT boot performs -------------------
    OTRExtScanner();
    const size_t sizeAfterOotScan = Combo_ExtensionCache_Size();
    MER_ASSERT(sizeAfterOotScan > 0, 2,
               "ExtensionCache empty after OTRExtScanner — no OoT archive is mounted, so every 'OoT's entries "
               "survived' assertion below would be vacuous");

    int ootTotal = 0;
    const std::vector<std::string> ootSample = MerCollectScopedPaths("oot", kMerOotSampleTarget, &ootTotal);
    MER_ASSERT(!ootSample.empty(), 2,
               "no extension-less OoT-owned path to sample — the survival check would be vacuous");
    for (const auto& path : ootSample) {
        MER_ASSERT(ResourceMgr_FileExists(path.c_str()) != 0, 2,
                   "an OoT-owned path is missing from the cache right after OTRExtScanner — the baseline is "
                   "not what this row assumes");
    }

    // ---- Scoping control, BEFORE the MM mount ------------------------------
    // With no MM archive recorded the "mm" scope must be empty, and a scan of it
    // must add nothing. If an earlier row in a shared process already mounted MM
    // archives the premise does not hold, so the control is skipped rather than
    // asserted falsely.
    int mmTotalBefore = 0;
    (void)MerCollectScopedPaths("mm", 0, &mmTotalBefore);
    if (mmTotalBefore == 0) {
        const int addedWithNoMmArchive = Combo_ExtensionCache_ScanGame("mm");
        MER_ASSERT(addedWithNoMmArchive == 0, 3,
                   "the 'mm' scope added entries with no MM archive mounted — the archive-ownership filter is "
                   "not scoping anything");
        MER_ASSERT(Combo_ExtensionCache_Size() == sizeAfterOotScan, 3,
                   "the no-op 'mm' scan changed the cache size");
    }

    // ---- Mount MM's archive, the way MM mounts one ------------------------
    MER_ASSERT(MM_MountArchiveHeadless(mmArchivePath) == 0, 4, "could not mount the MM archive");

    int mmTotalAfterMount = 0;
    const std::vector<std::string> mmCandidates = MerCollectScopedPaths("mm", 4096, &mmTotalAfterMount);
    MER_ASSERT(mmTotalAfterMount > 0, 5,
               "the 'mm' scope is still empty after mounting an MM archive — MM_MountArchiveHeadless did not "
               "record it, so the rescan could never see it");

    // The probe path: MM-owned AND genuinely absent from the cache. Chosen from
    // the live listing so this row cannot rot against a renamed asset, and so
    // "present after" is provably a change rather than a pre-existing truth.
    std::string probePath;
    for (const auto& path : mmCandidates) {
        if (ResourceMgr_FileExists(path.c_str()) == 0) {
            probePath = path;
            break;
        }
    }
    if (probePath.empty()) {
        printf("[TEST] SKIP: every MM-owned path is already in the ExtensionCache (the staged MM archive is a "
               "subset of OoT's), so there is nothing for the rescan to add — comparison would be vacuous "
               "(#618)\n");
        return kMerSkip;
    }

    const size_t sizeBefore = Combo_ExtensionCache_Size();
    const int callsBefore = MM_ExtensionRescan_CallCount();

    // ---- The production rescan --------------------------------------------
    MM_ExtensionRescan_AfterArchiveMount();

    MER_ASSERT(MM_ExtensionRescan_CallCount() == callsBefore + 1, 6, "the rescan entry point did not run");
    MER_ASSERT(MM_ExtensionRescan_Scanned() == 1, 6, "the MM-scoped scan did not complete");
    MER_ASSERT(MM_ExtensionRescan_FlipbooksPatchedAfterScan() == 1, 6,
               "PlayerCustomFlipbooks_Patch did not run after the scan — its one-shot latch would fix MM's "
               "faces to vanilla against an unscanned cache (#618)");

    const int added = MM_ExtensionRescan_LastEntriesAdded();
    MER_ASSERT(added > 0, 6,
               "the rescan added no ExtensionCache entries although the MM archive holds paths the cache "
               "lacked — MM custom assets stay invisible to ResourceMgr_FileExists (#618)");

    // Purely additive, by exact accounting: size grew by precisely the number of
    // new keys reported, so nothing was erased or displaced.
    MER_ASSERT(Combo_ExtensionCache_Size() == sizeBefore + (size_t)added, 7,
               "ExtensionCache size does not equal before + newly-added — the rescan erased or displaced "
               "entries instead of only inserting (#618)");

    MER_ASSERT(ResourceMgr_FileExists(probePath.c_str()) != 0, 8,
               "an MM-owned path the cache lacked is STILL absent after the rescan — the scan did not reach "
               "MM's archives (#618)");

    for (const auto& path : ootSample) {
        MER_ASSERT(ResourceMgr_FileExists(path.c_str()) != 0, 9,
                   "an OoT-owned path stopped resolving after the MM rescan — the rescan clobbered OoT's "
                   "entries (#618)");
    }

    // ---- Idempotent across repeated switches ------------------------------
    // Both games' archives stay mounted for the process lifetime (nothing calls
    // RemoveArchive), so every later arrival re-runs this against the same set.
    const size_t sizeAfterFirst = Combo_ExtensionCache_Size();
    MM_ExtensionRescan_AfterArchiveMount();
    MER_ASSERT(MM_ExtensionRescan_LastEntriesAdded() == 0, 10,
               "a second rescan reported new entries — it is not idempotent");
    MER_ASSERT(Combo_ExtensionCache_Size() == sizeAfterFirst, 10, "a second rescan changed the cache size");
    MER_ASSERT(ResourceMgr_FileExists(probePath.c_str()) != 0, 10, "the MM probe path was lost by the second rescan");
    for (const auto& path : ootSample) {
        MER_ASSERT(ResourceMgr_FileExists(path.c_str()) != 0, 10,
                   "an OoT-owned path was lost by the second rescan");
    }

    // ---- Scoping control, the other direction -----------------------------
    // Every OoT-owned path was keyed by the baseline scan, so re-scanning the
    // "oot" scope now must add nothing. A non-zero here would mean the MM mount
    // had displaced OoT keys that the OoT scan then had to restore.
    const int reAddedForOot = Combo_ExtensionCache_ScanGame("oot");
    MER_ASSERT(reAddedForOot == 0, 11,
               "an 'oot' rescan added keys after the MM mount — OoT entries had gone missing (#618)");

    printf("[TEST] mm-extension-rescan: PASS (%d OoT baseline entries, %d MM-owned paths visible, +%d new keys, "
           "probe '%s' now resolves, %zu OoT paths re-verified, second rescan a no-op)\n",
           ootTotal, mmTotalAfterMount, added, probePath.c_str(), ootSample.size());
    return 0;
}
