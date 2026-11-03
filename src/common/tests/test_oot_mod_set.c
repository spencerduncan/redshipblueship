/**
 * @file test_oot_mod_set.c
 * @brief #1196: OoT's mod menu names an archive by its path under mods/, so two
 *        archives with the same file name in different folders are two mods, and an
 *        enabled list saved by file name migrates to paths keeping its mods.
 *
 * CTest row OoTModSet ("oot-mod-set", label "redship"). ROM-free and archive-free: it
 * writes its own small O2R zips (no `version` entry, like most mods), so it needs only
 * the display-free shared bring-up and never skips.
 *
 * WHAT WAS BROKEN. games/oot/soh/Enhancements/mod_menu.cpp keyed `filePaths` by the
 * file-name STEM and `filePaths.emplace` keeps the first, so of two archives called
 * `same.o2r` in two folders only the first was ever mounted. The second still arrived
 * as "new", which pushed the same stem onto the enabled list a second time, so the
 * first archive was mounted twice and the list saved `same|same`.
 *
 * WHAT IT ASSERTS, against OoT_UpdateModFilesHeadless, which runs UpdateModFilesAt:
 * everything UpdateModFiles(true) does at GUI init once the two roots are resolved
 * (the enabled-mods CVar read, the walk, ResolveOoTModSet, the mount through
 * MountOoTModLayers, the CVar write-back). The staged tree is `a/same.o2r`,
 * `b/same.o2r`, `root.o2r`, `x/y/deep.o2r` and `mm/same.o2r` (MM's half of the shared
 * tree). Each archive carries a marker path only it ships, and the two OoT
 * `same.o2r` also carry one contested path with different bytes, so "mounted" is
 * measured on the ArchiveManager, not inferred from a count.
 *
 *   A. A fresh config: all four OoT archives mount and register, BOTH `same.o2r`
 *      included, each serving its own marker; the list is saved by path, new archives
 *      in path order; the later `same.o2r` wins the contested path; MM's `same.o2r`
 *      is neither mounted nor registered.
 *   B. The switch (#1319's eviction, #593's re-apply, both reading the registry):
 *      Combo_EnsureGameArchivesLoaded(GAME_MM) unmounts all four, both same-named
 *      archives included, and (GAME_OOT) mounts all four again in their order.
 *   C. The migration: the pre-#1196 list `gone|root|deep|same||same` (the doubled
 *      stem is what upstream saved for two new same-named archives) is saved back as
 *      `root.o2r|x/y/deep.o2r|S1|S2`, S1 being the `same.o2r` that comes first in the
 *      walk, i.e. the one the stem-keyed map mounted; the missing stem and the empty
 *      entry are dropped; the mount order is that list's, so S2 wins the contested
 *      path; MM's `same.o2r` is still nobody's.
 *   D. Resolving the saved list again changes nothing: same list, same mount order.
 *   E. No mods folder: a stem list is left exactly as saved, nothing is mounted.
 *
 * The row leaves the shared ArchiveManager, OoT's registry and the enabled-mods CVar
 * as it found them (a stored CVar would reach the build tree's config: clear, never
 * store). Included at FILE SCOPE by test_runner.cpp (compiled as C++).
 */

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <zip.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "../mod_archives.h"
#include "../test_scratch.h"

extern "C" {
// The init leg of OoT's UpdateModFiles over explicit roots
// (games/oot/soh/Enhancements/mod_menu.cpp, #1196).
int OoT_UpdateModFilesHeadless(const char* modsRoot, const char* mmModsRoot);
}

namespace {

// CVAR_SETTING("EnabledMods") in games/oot/soh/Enhancements/mod_menu.cpp
// (CMake/soh-cvars.cmake's `gSettings` prefix). Leg C would go red on a wrong name:
// the migration is read back from it.
constexpr const char* kOmsEnabledMods = "gSettings.EnabledMods";
constexpr const char* kOmsContested = "rsbs-test/oot-mod-set/contested";

#define OMS_REQUIRE(cond, code, ...)                  \
    do {                                              \
        if (!(cond)) {                                \
            printf("[oot-mod-set] FAIL(%d): ", code); \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            fflush(stdout);                           \
            return (code);                            \
        }                                             \
    } while (0)

/** One staged archive: its key under the mods root and the marker only it ships. */
struct OmsArchive {
    const char* key;
    const char* marker;
};

const OmsArchive kOmsA = { "a/same.o2r", "rsbs-test/oot-mod-set/a-same" };
const OmsArchive kOmsB = { "b/same.o2r", "rsbs-test/oot-mod-set/b-same" };
const OmsArchive kOmsRoot = { "root.o2r", "rsbs-test/oot-mod-set/root" };
const OmsArchive kOmsDeep = { "x/y/deep.o2r", "rsbs-test/oot-mod-set/deep" };
const OmsArchive kOmsMm = { "mm/same.o2r", "rsbs-test/oot-mod-set/mm-same" };

bool OmsWriteZip(const std::filesystem::path& path, const std::vector<std::pair<std::string, std::string>>& entries) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::filesystem::remove(path, ec);
    int err = 0;
    zip_t* za = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (za == nullptr) {
        return false;
    }
    for (const auto& [name, data] : entries) {
        // libzip keeps the buffer by pointer until zip_close, so hand it a copy it frees.
        char* copy = (char*)malloc(data.size());
        if (copy == nullptr) {
            zip_discard(za);
            return false;
        }
        memcpy(copy, data.data(), data.size());
        zip_source_t* src = zip_source_buffer(za, copy, data.size(), 1);
        if (src == nullptr || zip_file_add(za, name.c_str(), src, ZIP_FL_ENC_UTF_8) < 0) {
            if (src != nullptr) {
                zip_source_free(src);
            } else {
                free(copy);
            }
            zip_discard(za);
            return false;
        }
    }
    return zip_close(za) == 0 && std::filesystem::is_regular_file(path, ec);
}

std::string OmsNormal(const std::string& path) {
    return std::filesystem::path(path).lexically_normal().generic_string();
}

/** The archive the shared manager serves @p resource from, or "(none)". */
std::string OmsOwner(Ship::ArchiveManager& am, const char* resource) {
    auto owner = am.GetArchiveFromFile(resource);
    return owner ? OmsNormal(owner->GetPath()) : std::string("(none)");
}

std::string OmsCVar() {
    const char* v = CVarGetString(kOmsEnabledMods, "");
    return v != nullptr ? std::string(v) : std::string();
}

/** OoT's registry (what #593's re-apply and #1319's eviction read), as registered. */
std::vector<std::string> OmsRegistryRaw() {
    std::vector<std::string> out;
    for (int i = 0; i < Combo_GetModArchiveCount(GAME_OOT); i++) {
        const char* p = Combo_GetModArchive(GAME_OOT, i);
        out.push_back(p != nullptr ? std::string(p) : std::string());
    }
    return out;
}

/** The same, normalised for comparison. */
std::vector<std::string> OmsRegistry() {
    std::vector<std::string> out;
    for (const std::string& p : OmsRegistryRaw()) {
        out.push_back(OmsNormal(p));
    }
    return out;
}

std::string OmsJoin(const std::vector<std::string>& v) {
    std::string out;
    for (const std::string& s : v) {
        out += (out.empty() ? "" : " | ") + s;
    }
    return out;
}

/** The OoT `same.o2r` keys in the order OoT's walk meets them: the order the old
 *  stem-keyed map saw them in, so the first is the one it mounted. Same options and
 *  same partition predicate as CollectOoTModFiles. */
std::vector<std::string> OmsSameNamedInWalkOrder(const std::string& root) {
    std::vector<std::string> keys;
    const std::filesystem::path base = std::filesystem::path(root).lexically_normal();
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, Rsbs::kModsWalkOptions, ec), end; it != end && !ec;
         it.increment(ec)) {
        std::error_code entryEc;
        if (it->is_directory(entryEc)) {
            continue;
        }
        const std::string generic = it->path().generic_string();
        if (it->path().filename() == "same.o2r" && Combo_ModPathIsForGame(GAME_OOT, root.c_str(), generic.c_str())) {
            keys.push_back(it->path().lexically_normal().lexically_relative(base).generic_string());
        }
    }
    return keys;
}

/** The legs. Setup and teardown are the caller's, so every early return still
 *  restores the manager, the registry and the CVar. */
int OmsLegs(Ship::ArchiveManager& am, const std::shared_ptr<std::vector<std::shared_ptr<Ship::Archive>>>& snapshot,
            const std::string& root, const std::string& absentRoot) {
    const OmsArchive* const kOoT[] = { &kOmsA, &kOmsB, &kOmsRoot, &kOmsDeep };
    auto pathOf = [&](const OmsArchive& a) { return OmsNormal(root + "/" + a.key); };
    auto pathsOf = [&](const std::vector<std::string>& keys) {
        std::vector<std::string> out;
        for (const std::string& k : keys) {
            out.push_back(OmsNormal(root + "/" + k));
        }
        return out;
    };
    // Back to the manager the row found and an empty OoT registry, between legs.
    auto unmountAll = [&]() {
        am.SetArchives(snapshot);
        Combo_ClearModArchives(GAME_OOT);
    };

    // Anti-vacuity: before any leg, no marker resolves, so every "served by" below is a
    // change the leg made.
    for (const OmsArchive* a : { &kOmsA, &kOmsB, &kOmsRoot, &kOmsDeep, &kOmsMm }) {
        OMS_REQUIRE(!am.HasFile(a->marker), 1, "marker %s resolves before anything was mounted (served by %s)",
                    a->marker, OmsOwner(am, a->marker).c_str());
    }

    // A. A fresh config: nothing saved.
    CVarClear(kOmsEnabledMods);
    int mounted = OoT_UpdateModFilesHeadless(root.c_str(), root.c_str());
    OMS_REQUIRE(mounted == 4, 2, "a fresh config mounted %d archive(s), expected the 4 OoT archives", mounted);
    for (const OmsArchive* a : kOoT) {
        OMS_REQUIRE(OmsOwner(am, a->marker) == pathOf(*a), 3,
                    "%s is served by %s, expected %s: that archive did not mount (two same-named archives must "
                    "both mount)",
                    a->marker, OmsOwner(am, a->marker).c_str(), pathOf(*a).c_str());
    }
    OMS_REQUIRE(OmsCVar() == "a/same.o2r|b/same.o2r|root.o2r|x/y/deep.o2r", 4,
                "a fresh config saved '%s', expected every OoT archive by path, in path order", OmsCVar().c_str());
    {
        const std::vector<std::string> expected = pathsOf({ "a/same.o2r", "b/same.o2r", "root.o2r", "x/y/deep.o2r" });
        OMS_REQUIRE(OmsRegistry() == expected, 5, "OoT's registry is [%s], expected [%s]",
                    OmsJoin(OmsRegistry()).c_str(), OmsJoin(expected).c_str());
    }
    OMS_REQUIRE(OmsOwner(am, kOmsContested) == pathOf(kOmsB), 6,
                "the contested path is served by %s, expected b/same.o2r (mounted last)",
                OmsOwner(am, kOmsContested).c_str());
    OMS_REQUIRE(!am.HasFile(kOmsMm.marker), 7, "MM's mods/mm/same.o2r was mounted by OoT (served by %s)",
                OmsOwner(am, kOmsMm.marker).c_str());

    // B. The switch. Eviction and re-apply both read the registry, which now holds two
    //    archives with one file name; both must leave and both must come back.
    Combo_EnsureGameArchivesLoaded(GAME_MM);
    for (const OmsArchive* a : kOoT) {
        OMS_REQUIRE(!am.HasFile(a->marker), 8, "after the switch to MM, OoT's %s is still served by %s (#1319)",
                    a->marker, OmsOwner(am, a->marker).c_str());
    }
    Combo_EnsureGameArchivesLoaded(GAME_OOT);
    for (const OmsArchive* a : kOoT) {
        OMS_REQUIRE(OmsOwner(am, a->marker) == pathOf(*a), 9,
                    "after the switch back to OoT, %s is served by %s, expected %s (#593)", a->marker,
                    OmsOwner(am, a->marker).c_str(), pathOf(*a).c_str());
    }
    OMS_REQUIRE(OmsOwner(am, kOmsContested) == pathOf(kOmsB), 9,
                "after the round trip the contested path is served by %s, expected b/same.o2r",
                OmsOwner(am, kOmsContested).c_str());

    // C. The migration from a list saved before #1196.
    unmountAll();
    for (const OmsArchive* a : kOoT) {
        OMS_REQUIRE(!am.HasFile(a->marker), 10, "%s still resolves after the manager was restored", a->marker);
    }
    const std::vector<std::string> same = OmsSameNamedInWalkOrder(root);
    OMS_REQUIRE(same.size() == 2, 10, "the walk found %zu OoT same.o2r, expected 2", same.size());
    CVarSetString(kOmsEnabledMods, "gone|root|deep|same||same");
    mounted = OoT_UpdateModFilesHeadless(root.c_str(), root.c_str());
    OMS_REQUIRE(mounted == 4, 11, "the migrated list mounted %d archive(s), expected 4", mounted);
    const std::vector<std::string> migratedKeys = { "root.o2r", "x/y/deep.o2r", same[0], same[1] };
    const std::string migrated = "root.o2r|x/y/deep.o2r|" + same[0] + "|" + same[1];
    OMS_REQUIRE(OmsCVar() == migrated, 12,
                "the pre-#1196 list was saved back as '%s', expected '%s' (each stem to the first untaken archive "
                "in walk order, the missing stem and the empty entry dropped)",
                OmsCVar().c_str(), migrated.c_str());
    OMS_REQUIRE(OmsRegistry() == pathsOf(migratedKeys), 13, "OoT's registry is [%s], expected [%s]",
                OmsJoin(OmsRegistry()).c_str(), OmsJoin(pathsOf(migratedKeys)).c_str());
    for (const OmsArchive* a : kOoT) {
        OMS_REQUIRE(OmsOwner(am, a->marker) == pathOf(*a), 14, "after the migration %s is served by %s, expected %s",
                    a->marker, OmsOwner(am, a->marker).c_str(), pathOf(*a).c_str());
    }
    OMS_REQUIRE(OmsOwner(am, kOmsContested) == OmsNormal(root + "/" + same[1]), 15,
                "after the migration the contested path is served by %s, expected %s (last in the saved order)",
                OmsOwner(am, kOmsContested).c_str(), same[1].c_str());
    OMS_REQUIRE(!am.HasFile(kOmsMm.marker), 16, "the stem 'same' was resolved to MM's mods/mm/same.o2r (served by %s)",
                OmsOwner(am, kOmsMm.marker).c_str());

    // D. The saved list resolves to itself.
    unmountAll();
    mounted = OoT_UpdateModFilesHeadless(root.c_str(), root.c_str());
    OMS_REQUIRE(mounted == 4 && OmsCVar() == migrated, 17,
                "resolving the saved list again mounted %d and saved '%s', expected 4 and '%s'", mounted,
                OmsCVar().c_str(), migrated.c_str());
    OMS_REQUIRE(OmsRegistry() == pathsOf(migratedKeys), 17, "the second resolve registered [%s], expected [%s]",
                OmsJoin(OmsRegistry()).c_str(), OmsJoin(pathsOf(migratedKeys)).c_str());

    // E. No mods folder: no walk, so no migration and no write.
    unmountAll();
    CVarSetString(kOmsEnabledMods, "root|same");
    mounted = OoT_UpdateModFilesHeadless(absentRoot.c_str(), absentRoot.c_str());
    OMS_REQUIRE(mounted == 0 && Combo_GetModArchiveCount(GAME_OOT) == 0, 18,
                "a missing mods folder mounted %d and registered %d archive(s), expected none", mounted,
                Combo_GetModArchiveCount(GAME_OOT));
    OMS_REQUIRE(OmsCVar() == "root|same", 19, "a missing mods folder rewrote the saved list to '%s'",
                OmsCVar().c_str());
    return 0;
}

} // namespace

int OoTModSet_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    auto rm = ctx ? ctx->GetResourceManager() : nullptr;
    if (rm == nullptr || rm->GetArchiveManager() == nullptr || ctx->GetConsoleVariables() == nullptr) {
        printf("[oot-mod-set] FAIL: no ArchiveManager or console variables after the bring-up\n");
        return 1;
    }
    auto am = rm->GetArchiveManager();

    const std::filesystem::path scratch = rsbs::test::ScratchDir("oot-mod-set");
    const std::string root = (scratch / "mods").generic_string();
    const std::string absentRoot = (scratch / "no-mods-here").generic_string();
    const std::vector<std::pair<const OmsArchive*, std::string>> staged = {
        { &kOmsA, "a" }, { &kOmsB, "b" }, { &kOmsRoot, "" }, { &kOmsDeep, "" }, { &kOmsMm, "mm" },
    };
    for (const auto& [a, contested] : staged) {
        std::vector<std::pair<std::string, std::string>> entries = { { a->marker,
                                                                       std::string("oot-mod-set: ") + a->key } };
        if (!contested.empty()) {
            entries.push_back({ kOmsContested, "oot-mod-set: contested, from " + contested });
        }
        const std::filesystem::path file = std::filesystem::path(root) / a->key;
        if (!OmsWriteZip(file, entries)) {
            printf("[oot-mod-set] FAIL: could not write %s\n", file.generic_string().c_str());
            return 1;
        }
    }

    // What the row must put back.
    auto snapshot = am->GetArchives();
    const std::vector<std::string> registry = OmsRegistryRaw();
    const std::string savedList = OmsCVar();
    Combo_ClearModArchives(GAME_OOT);

    const int rc = OmsLegs(*am, snapshot, root, absentRoot);

    am->SetArchives(snapshot);
    Combo_ClearModArchives(GAME_OOT);
    for (const std::string& path : registry) {
        Combo_RegisterModArchive(GAME_OOT, path.c_str());
    }
    if (savedList.empty()) {
        CVarClear(kOmsEnabledMods);
    } else {
        CVarSetString(kOmsEnabledMods, savedList.c_str());
    }

    if (rc == 0) {
        printf("[oot-mod-set] PASS: two same-named archives in different folders both mount, are saved by path and "
               "survive a switch; a list saved by file name migrates to paths keeping its mods and order; a re-resolve "
               "changes nothing; a missing folder leaves the list alone\n");
        fflush(stdout);
    }
    return rc;
}

#undef OMS_REQUIRE
