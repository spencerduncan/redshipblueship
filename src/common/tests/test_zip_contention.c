/**
 * @file test_zip_contention.c
 * @brief Deterministic concurrent-load contention lock over the shared archive
 *        handle (#560).
 *
 * The #560 field crash: libultraship's archive layer reads ONE shared zip_t per
 * o2r with no synchronization anywhere (O2rArchive::LoadFile does
 * zip_name_locate / zip_stat_index / zip_fopen_index / zip_fread / zip_fclose
 * on it; ResourceManager::mMutex guards only the resource cache map). During
 * menu-triggered seed generation, a resource-pool worker (scene sub-load in the
 * fill) and the render thread (direct LoadResourceProcess for a cold glyph
 * texture) zip_fread the same handle concurrently; libzip documents zip_t as
 * not thread-safe, both buffers come back zeroed, ReadResourceInitDataBinary
 * sees ByteOrder/Type/Version = 0, and both loads return nullptr — one
 * unchecked consumer then AVs. The root fix is a per-archive-object mutex in
 * the libultraship fork (spencerduncan/libultraship); this lock is the
 * regression tripwire for it, made possible by #562 mounting soh.o2r where the
 * ctest rows look.
 *
 * Shape — deliberately deterministic rather than probabilistic:
 *
 *   1. CALIBRATION (single-threaded): enumerate soh.o2r entries (textures/ and
 *      objects/ first — the largest, widest-window entries), keep every entry
 *      whose RAW archive read succeeds at >= 64 bytes, and record its size and
 *      FNV-1a64 content hash. Each kept entry is also probed once through
 *      LoadResourceProcess: entries a registered factory parses cleanly are
 *      flagged `resLoadable` and additionally exercise the factory pipeline
 *      below. (Most soh.o2r entries are RAW files — .png/.json/shader text —
 *      with no OTR header, so the RAW read is the universal detector; the
 *      factory leg mirrors the field crash's exact null/type-0 signature on
 *      the subset that parses. An entry that cannot load single-threaded is
 *      excluded from the corresponding assertion, so a contention failure can
 *      only mean concurrency.)
 *
 *   2. CONTENTION: kZcLoaderThreads threads each own a DISJOINT slice of the
 *      cold entries and tight-loop { raw load + byte-exact size/hash compare
 *      against calibration; for resLoadable entries also resource load +
 *      null/type-0 check + unload so the next pass is cold }. One more thread
 *      re-reads a small HOT set of resLoadable entries through the resource
 *      cache throughout — the glyph-side access pattern, exercising the cache
 *      map concurrently with the writers.
 *
 * Determinism note: the loader threads call LoadFileProcess /
 * LoadResourceProcess DIRECTLY (the render thread's call shape in
 * gfx_set_timg_otr_filepath_handler_custom), so N-way concurrency over the one
 * zip_t is guaranteed by this test's own threads on every machine — it does
 * not depend on the resource pool's sizing, which collapses to a single worker
 * on small CI runners. Through-pool loads execute the identical
 * LoadResourceProcess body, so the code under contention is the same either
 * way.
 *
 * On the UNFIXED libultraship this fails (or crashes — the harness's headless
 * crash handler converts that into a fast failure) within the first few
 * hundred concurrent loads; the pass count is sized well past ten times that
 * horizon. Stop-on-first-failure keeps the red run short and reports a crisp
 * loads-to-failure number.
 *
 * Env knob: RSBS_ZIP_CONTENTION_PASSES overrides the per-thread pass count
 * (used for calibration/soak runs; the default is the shipped lock).
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++): it drives the
 * C++-linkage Ship::Context / ResourceManager / ArchiveManager APIs. The
 * caller (Test_ZipContention) performs the display-free shared bring-up, the
 * soh.o2r resolvability check, and the factory registration first; with no
 * archive this body is never reached.
 */

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/Resource.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kZcLoaderThreads = 4;
constexpr int kZcMaxHotEntries = 4;
constexpr int kZcMaxColdEntries = 48;
// Minimum RAW-verified entries to stage real contention: at least two cold
// entries per loader thread. Below that the lock FAILS (loudly): a contention
// lock that silently degrades to fewer threads than it claims is the vacuity
// #560 documents.
constexpr int kZcMinColdEntries = kZcLoaderThreads * 2;
// Skip near-empty entries: a 4-byte read has no meaningful race window.
constexpr size_t kZcMinEntryBytes = 64;
// Per-thread passes over its cold slice. Sized from the measured unfixed
// failure horizon (see the pointer-bump PR's counterfactual evidence): the
// unfixed archive layer fails within the first few hundred concurrent cold
// loads, and this default drives well over ten times that many.
constexpr int kZcDefaultPasses = 400;

struct ZcEntry {
    std::string path;
    size_t size;
    uint64_t hash;
    bool resLoadable;
};

uint64_t ZcFnv1a64(const char* data, size_t len) {
    uint64_t hash = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)data[i];
        hash *= 0x100000001B3ull;
    }
    return hash;
}

int ZcPassCount(void) {
    const char* env = getenv("RSBS_ZIP_CONTENTION_PASSES");
    if (env != nullptr && env[0] != '\0') {
        long parsed = strtol(env, nullptr, 10);
        if (parsed > 0) {
            return (int)parsed;
        }
    }
    return kZcDefaultPasses;
}

} // namespace

extern "C" int ZipContention_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr ||
        ctx->GetResourceManager()->GetArchiveManager() == nullptr) {
        fprintf(stderr, "[zip-contention] FAIL: resource manager not initialized (caller must run the shared "
                        "bring-up first)\n");
        return 1;
    }
    auto rm = ctx->GetResourceManager();
    auto am = rm->GetArchiveManager();
    if (!am->IsLoaded()) {
        fprintf(stderr, "[zip-contention] FAIL: no archive mounted; the caller's soh.o2r resolvability check "
                        "should have skipped this body (#562 staging)\n");
        return 1;
    }

    // ---- Calibration (single-threaded) ------------------------------------
    // Candidate order: the big binary namespaces first (widest zip_fread
    // windows), then everything else, alphabetical within each group so the
    // verified set is deterministic for a given archive.
    std::vector<std::string> candidates;
    for (const char* mask : { "textures/*", "objects/*", "*" }) {
        auto names = am->ListFiles(mask);
        if (names == nullptr) {
            continue;
        }
        std::vector<std::string> group(names->begin(), names->end());
        std::sort(group.begin(), group.end());
        for (auto& name : group) {
            if (std::find(candidates.begin(), candidates.end(), name) == candidates.end()) {
                candidates.push_back(name);
            }
        }
    }

    const int targetEntries = kZcMaxHotEntries + kZcMaxColdEntries;
    std::vector<ZcEntry> verified;
    int resLoadableCount = 0;
    for (const auto& name : candidates) {
        if ((int)verified.size() >= targetEntries) {
            break;
        }
        auto file = rm->LoadFileProcess(name);
        if (file == nullptr || file->Buffer == nullptr || file->Buffer->size() < kZcMinEntryBytes) {
            continue;
        }
        // Factory probe. Most soh.o2r entries are raw files with no OTR
        // header, so a null here is expected and only excludes the entry from
        // the FACTORY leg — the raw-read leg still hammers and checks it.
        auto res = rm->LoadResourceProcess(name, /*loadExact*/ true, nullptr);
        bool resLoadable = res != nullptr && res->GetInitData() != nullptr && res->GetInitData()->Type != 0;
        rm->UnloadResource(name);
        if (resLoadable) {
            resLoadableCount++;
        }
        verified.push_back(
            { name, file->Buffer->size(), ZcFnv1a64(file->Buffer->data(), file->Buffer->size()), resLoadable });
    }

    // Hot set: the first kZcMaxHotEntries factory-loadable entries, read
    // through the resource cache by the hot thread. The factory leg going
    // completely vacuous is a loud failure, not a quiet degradation — it means
    // the harness's factory surface changed and the null/type-0 arm of this
    // lock no longer runs.
    if (resLoadableCount < 1) {
        fprintf(stderr,
                "[zip-contention] FAIL: no factory-loadable entries at all (%d raw-verified candidates). The "
                "factory arm of this lock is vacuous — re-check the factory registrations in "
                "Test_ZipContention.\n",
                (int)verified.size());
        return 1;
    }

    std::vector<ZcEntry> hot;
    std::vector<ZcEntry> cold;
    for (const auto& e : verified) {
        if ((int)hot.size() < kZcMaxHotEntries && e.resLoadable) {
            hot.push_back(e);
        } else {
            cold.push_back(e);
        }
    }

    if ((int)cold.size() < kZcMinColdEntries) {
        fprintf(stderr,
                "[zip-contention] FAIL: only %d cold entries survived calibration (need >= %d to stage %d "
                "loader threads). The lock refuses to run degraded — if the archive shrank this much, "
                "re-inventory the candidate masks.\n",
                (int)cold.size(), kZcMinColdEntries, kZcLoaderThreads);
        return 1;
    }

    printf("[zip-contention] calibration: %d candidates, %d raw-verified (%d factory-loadable), %d hot + %d "
           "cold staged\n",
           (int)candidates.size(), (int)verified.size(), resLoadableCount, (int)hot.size(), (int)cold.size());

    // Warm the hot set into the resource cache.
    for (const auto& e : hot) {
        auto res = rm->LoadResourceProcess(e.path, /*loadExact*/ true, nullptr);
        if (res == nullptr) {
            fprintf(stderr, "[zip-contention] FAIL: hot warm-up load of '%s' failed single-threaded\n",
                    e.path.c_str());
            return 1;
        }
    }

    const int passes = ZcPassCount();

    std::atomic<bool> go{ false };
    std::atomic<bool> stop{ false };
    std::atomic<long long> coldLoads{ 0 };
    std::atomic<long long> hotReads{ 0 };
    std::atomic<long long> failures{ 0 };
    std::mutex failMutex;
    std::string firstFailure;

    auto recordFailure = [&](long long loadOrdinal, int pass, const std::string& path, const char* what) {
        failures.fetch_add(1);
        {
            const std::lock_guard<std::mutex> lock(failMutex);
            if (firstFailure.empty()) {
                char buf[512];
                snprintf(buf, sizeof(buf), "cold load #%lld (pass %d, entry '%s'): %s", loadOrdinal, pass,
                         path.c_str(), what);
                firstFailure = buf;
            }
        }
        stop.store(true);
    };

    auto loaderBody = [&](int tid) {
        while (!go.load()) {
            std::this_thread::yield();
        }
        for (int pass = 0; pass < passes && !stop.load(); pass++) {
            for (size_t i = tid; i < cold.size() && !stop.load(); i += kZcLoaderThreads) {
                const ZcEntry& e = cold[i];
                long long ordinal = coldLoads.fetch_add(1) + 1;

                // Leg (a): the raw archive read — byte-exactness against the
                // single-threaded calibration hash catches silent corruption,
                // not just the null/type-0 signature.
                auto file = rm->LoadFileProcess(e.path);
                if (file == nullptr || file->Buffer == nullptr) {
                    recordFailure(ordinal, pass, e.path, "raw file load returned null");
                    return;
                }
                if (file->Buffer->size() != e.size) {
                    recordFailure(ordinal, pass, e.path, "raw file size differs from single-threaded read");
                    return;
                }
                if (ZcFnv1a64(file->Buffer->data(), file->Buffer->size()) != e.hash) {
                    recordFailure(ordinal, pass, e.path, "raw file bytes corrupted vs single-threaded read");
                    return;
                }

                // Leg (b): the factory pipeline — the exact consumer path the
                // field crash took (null resource / init data type 0). Only for
                // entries a registered factory parses cleanly single-threaded.
                if (e.resLoadable) {
                    auto res = rm->LoadResourceProcess(e.path, /*loadExact*/ true, nullptr);
                    if (res == nullptr) {
                        recordFailure(ordinal, pass, e.path, "resource load returned null (the #560 signature)");
                        return;
                    }
                    if (res->GetInitData() == nullptr || res->GetInitData()->Type == 0) {
                        recordFailure(ordinal, pass, e.path,
                                      "resource init data read as type 0 (the #560 signature)");
                        return;
                    }
                    rm->UnloadResource(e.path); // Cold again for the next pass.
                }
            }
        }
    };

    auto hotBody = [&]() {
        while (!go.load()) {
            std::this_thread::yield();
        }
        size_t idx = 0;
        while (!stop.load()) {
            const ZcEntry& e = hot[idx % hot.size()];
            idx++;
            auto res = rm->LoadResourceProcess(e.path, /*loadExact*/ true, nullptr);
            hotReads.fetch_add(1);
            if (res == nullptr || res->GetInitData() == nullptr || res->GetInitData()->Type == 0) {
                recordFailure(-1, -1, e.path, "HOT cached entry read back null/type-0");
                return;
            }
        }
    };

    std::vector<std::thread> loaders;
    loaders.reserve(kZcLoaderThreads);
    for (int t = 0; t < kZcLoaderThreads; t++) {
        loaders.emplace_back(loaderBody, t);
    }
    std::thread hotThread(hotBody);

    go.store(true);
    for (auto& th : loaders) {
        th.join();
    }
    stop.store(true); // Cold work complete (or failed): release the hot reader.
    hotThread.join();

    printf("[zip-contention] %d loader threads x %d passes over %d cold entries (+%d hot): %lld cold loads, "
           "%lld hot reads, %lld failure(s)\n",
           kZcLoaderThreads, passes, (int)cold.size(), (int)hot.size(), coldLoads.load(), hotReads.load(),
           failures.load());
    if (failures.load() != 0) {
        const std::lock_guard<std::mutex> lock(failMutex);
        fprintf(stderr, "[zip-contention] FAIL: first failure at %s\n", firstFailure.c_str());
        return 1;
    }
    return 0;
}
