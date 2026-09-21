/**
 * @file test_mm_mods_mount.c
 * @brief #670: MM mounts its mods/ folder in single-exe, the shared mods/ tree is
 *        partitioned between the two games, and an MM mod override survives a
 *        cross-game switch without ever shadowing an OoT path once OoT is active.
 *
 * CTest row MMModsMount (label "redship"), dispatch "mm-mods-mount" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. MM's whole mod-mount sequence lives in
 * games/mm/2s2h/BenPort.cpp's InitOTR, and games/mm/CMakeLists.txt:244 EXCLUDES
 * that file from the single exe. Nothing replaced it: LoadMMArchives() mounted
 * mm.o2r and 2ship.o2r and stopped. So Combo_GetModArchiveCount(GAME_MM) was
 * structurally always 0 — #593's switch-time re-apply loop iterated it and did
 * nothing, for MM, forever — and every MM-side custom-asset consumer
 * (MM_GfxPrint_HasArchiveTexture, the split PlayerCustomFlipbooks frame names)
 * could only read false, because the paths they want are supplied by mods.
 *
 * WHY A PARTITION IS PART OF THE FIX. Both ports resolve their mods folder with
 * LocateFileAcrossAppDirs("mods", <appShortName>), and in a portable build
 * (NON_PORTABLE=OFF — what this project configures)
 * Context::GetAppDirectoryPath IGNORES appName and returns "."
 * (libultraship/src/ship/Context.cpp), so "soh" and "2s2h" resolve to the SAME
 * ./mods. Re-homing BenPort's glob verbatim would mount every OoT mod again under
 * GAME_MM, and the #593 re-apply would then stack OoT's mods over MM's base
 * archives on every MM arrival — shadowing that SURVIVES the switch. MM therefore
 * claims mods/mm only, OoT claims the complement, and one predicate
 * (Combo_ModPathIsForGame) defines the split for both globs.
 *
 * WHAT THIS ROW ASSERTS, against the production functions:
 *
 *   1. The partition is total and disjoint, over path spellings that actually
 *      occur: "./mods" vs "mods", '\' separators, "MM" in any case, the
 *      near-misses "mmx"/"xmm", a nested archive, and a path outside the root.
 *   2. MM's real glob (MM_MountModArchivesHeadless -> MountMMModArchives) mounts
 *      the two staged mods/mm archives and NOT the root-level one, recurses into
 *      mods/mm/sub, ignores a non-archive file, sorts by file-name stem, and
 *      records each mount in BOTH registries — Combo_ArchivePathIsMM (the #344
 *      dispatcher and the #618 "mm" extension scope key on it) and
 *      Combo_GetModArchiveCount(GAME_MM) (the exact invariant #593's loop needs
 *      and nothing checked before).
 *   3. The MM-side override applies: the contested path is owned by MM's base
 *      archive BEFORE the mod mount and by the last-sorting mod AFTER it.
 *   4. An OoT path is not shadowed: after the production
 *      Combo_EnsureGameArchivesLoaded(GAME_OOT), the contested path is owned by
 *      the real soh.o2r again, even though the MM mod that carries it is still
 *      mounted — i.e. an MM mod cannot shadow an OoT path once OoT is the active
 *      game. OoT's mod registry is EMPTY for this leg, so the only thing that can
 *      reclaim the path is the base re-add.
 *   5. The override survives the switch back:
 *      Combo_EnsureGameArchivesLoaded(GAME_MM) hands the path to the mod again,
 *      via the registry MM's mount fed in (2).
 *   6. A second mount is idempotent: neither registry grows and mod precedence
 *      does not move.
 *
 * ANTI-VACUITY. Every "the mod owns it" assertion is preceded by the matching
 * "the base owns it" precondition on the SAME path, so each leg is a measured
 * change of ownership rather than a standing truth. The contested path is
 * `portVersion`, which the exporter writes into every curated archive, so it is
 * genuinely carried by both the base archives and the stand-in mods; the row
 * asserts it is resolvable at all before using it.
 *
 * WHY COPIES OF soh.o2r / 2ship.o2r STAND IN FOR MODS. A mod archive has to be a
 * real loadable o2r, and nothing in the tree can author one at runtime
 * (ArchiveManager::AddArchive on a non-existent path fails at Archive::Load). A
 * byte copy of a staged archive is a real one whose paths deliberately overlap
 * the base archives', and its FILE NAME is what makes "which archive won"
 * observable. soh.o2r for the MM mod that must lose to OoT on leg (4) is the
 * point, not an accident: it is the strongest available form of "an MM mod that
 * ships OoT-owned paths".
 *
 * mm.o2r is never used: it is ROM-derived and undistributable, so a row needing
 * it would skip on every CI run. SKIPs when soh.o2r or 2ship.o2r is unstaged, the
 * same policy as the #593/#595/#618 rows — the netplay-relay CI job re-runs this
 * label archive-less on purpose (#562).
 *
 * The staged tree lives in a private directory under the CWD and is deleted
 * afterwards; the row never touches the player's ./mods, and it restores the
 * shared ArchiveManager's archive list from a snapshot so no later row in
 * `--test all` inherits a re-ordered virtual file system (the #593 row's
 * discipline, for the same reason).
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++): it drives the
 * C++-linkage Ship::Context / ArchiveManager APIs directly.
 */

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../mod_archives.h"

extern "C" {
// MM's production mod glob + mount + both registrations, over an explicitly
// supplied mods root (games/mm/2s2h/GameExports_SingleExe.cpp, #670).
int MM_MountModArchivesHeadless(const char* modsRoot);
// MM's archive-origin registry, fed by RecordMMArchivePath — what the #344
// Room/Cutscene factory dispatcher and the #618 "mm" extension scope key on.
bool Combo_ArchivePathIsMM(const char* path);
}

namespace {

#define MMM_ASSERT(cond, code, msg)                                                     \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

// A path both the base archives and the stand-in mods carry: the exporter writes
// portVersion into every curated archive, so ownership of it is genuinely
// contested and "who won" is a real measurement.
constexpr const char kMmmContestedPath[] = "portVersion";

std::string MmmOwnerOf(const std::shared_ptr<Ship::ArchiveManager>& mgr, const char* path) {
    auto archive = mgr->GetArchiveFromFile(path);
    return archive != nullptr ? archive->GetPath() : std::string();
}

bool MmmPathContains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

/**
 * @param sohArchive a staged soh.o2r (an OoT-side archive, and the source for the
 *                   MM mod that deliberately carries OoT-owned paths).
 * @param mmArchive  a staged 2ship.o2r (MM's port-asset archive: the base the
 *                   MM-side override has to beat).
 * @return 0 pass, non-zero failure code.
 */
extern "C" int MMModsMount_RunHeadless(const char* sohArchive, const char* mmArchive) {
    printf("[TEST] mm-mods-mount: MM mounts mods/mm, the tree is partitioned, and the override survives a switch "
           "(#670)\n");

    auto ctx = Ship::Context::GetInstance();
    MMM_ASSERT(ctx != nullptr && ctx->GetResourceManager() != nullptr &&
                   ctx->GetResourceManager()->GetArchiveManager() != nullptr,
               1, "no Ship::Context/ResourceManager/ArchiveManager — run the shared bring-up first");
    MMM_ASSERT(sohArchive != nullptr && sohArchive[0] != '\0' && mmArchive != nullptr && mmArchive[0] != '\0', 1,
               "no staged archives supplied");
    auto archiveMgr = ctx->GetResourceManager()->GetArchiveManager();

    // ---- Part 1: the partition, as pure path logic -------------------------
    // No filesystem involved, so these run even when the archives are unstaged
    // and they cover the spellings that actually reach the two globs: modsRoot
    // arrives from LocateFileAcrossAppDirs (can be "./mods"), while mod_menu
    // hands over a lexically_normal()'d path ("mods/mm/x.o2r") and Windows
    // iteration yields '\' separators.
    struct PartitionCase {
        const char* root;
        const char* path;
        bool isMm;
        const char* why;
    };
    static const PartitionCase kCases[] = {
        { "./mods", "./mods/mm/10-mod.o2r", true, "the plain MM case" },
        { "./mods", "mods/mm/10-mod.o2r", true, "a lexically_normal()'d path against a './' root" },
        { "mods", "./mods/mm/10-mod.o2r", true, "a './' path against a bare root" },
        { "./mods", "./mods/mm/sub/20-mod.o2r", true, "nested under mods/mm" },
#if defined(_WIN32)
        // Windows only, and deliberately so. '\' is a path separator on Windows,
        // which std::filesystem::path splits on, so the predicate classifies this
        // correctly there. On POSIX '\' is a LEGAL FILENAME CHARACTER — a file
        // really can be called `mm\10-mod.o2r` — and the predicate must NOT
        // reinterpret it as a separator, or such a file sitting in the mods root
        // would be handed to MM. Asserting the Windows answer on Linux was this
        // row's first CI failure, and the predicate is right: the difference is
        // real.
        //
        // Neither glob relies on this either way: both pass generic_string()
        // (forward slashes) on both platforms — mod_menu's
        // `p.path().generic_string()` and MountMMModArchives's `generic`, with
        // roots built by LocateFileAcrossAppDirs, which concatenates with '/'.
        { "./mods", ".\\mods\\mm\\10-mod.o2r", true, "native Windows separators" },
#endif
        { "./mods", "./mods/MM/10-mod.o2r", true, "the reserved name is case-insensitive" },
        { "./mods", "./mods/10-mod.o2r", false, "the root belongs to OoT" },
        { "./mods", "./mods/sub/10-mod.o2r", false, "any other subfolder belongs to OoT" },
        { "./mods", "./mods/mmx/10-mod.o2r", false, "'mmx' is not 'mm' (prefix near-miss)" },
        { "./mods", "./mods/xmm/10-mod.o2r", false, "'xmm' is not 'mm' (suffix near-miss)" },
        { "./mods", "./mods/mm-extra/10-mod.o2r", false, "'mm-extra' is not 'mm'" },
        { "./mods", "./elsewhere/mm/10-mod.o2r", false, "outside the root is not MM's" },
    };
    int mmClaims = 0;
    int ootClaims = 0;
    for (const auto& c : kCases) {
        const bool mm = Combo_ModPathIsForGame(GAME_MM, c.root, c.path);
        const bool oot = Combo_ModPathIsForGame(GAME_OOT, c.root, c.path);
        if (mm != c.isMm) {
            printf("[TEST] FAIL(2): partition: MM %s '%s' under root '%s' (%s)\n",
                   mm ? "claimed" : "did not claim", c.path, c.root, c.why);
            return 2;
        }
        // Total AND disjoint: exactly one game claims every path. If this ever
        // held for neither, a mod would silently be mounted by nobody; if for
        // both, by everybody.
        if (oot == mm) {
            printf("[TEST] FAIL(2): partition is not a partition: both games answered %s for '%s' (%s)\n",
                   mm ? "true" : "false", c.path, c.why);
            return 2;
        }
        mm ? mmClaims++ : ootClaims++;
    }
    MMM_ASSERT(mmClaims > 0 && ootClaims > 0, 2,
               "the partition table exercised only one side — the disjointness check would be one-sided");

    // Degenerate arguments claim nothing, for either game: a NULL root must not
    // make every path MM's by accident, nor make every path OoT's.
    for (GameId g : { GAME_OOT, GAME_MM }) {
        MMM_ASSERT(!Combo_ModPathIsForGame(g, nullptr, "./mods/mm/x.o2r"), 3, "a NULL mods root claimed a path");
        MMM_ASSERT(!Combo_ModPathIsForGame(g, "./mods", nullptr), 3, "a NULL path was claimed");
        MMM_ASSERT(!Combo_ModPathIsForGame(g, "", "./mods/mm/x.o2r"), 3, "an empty mods root claimed a path");
        MMM_ASSERT(!Combo_ModPathIsForGame(g, "./mods", ""), 3, "an empty path was claimed");
    }
    MMM_ASSERT(!Combo_ModPathIsForGame(GAME_NONE, "./mods", "./mods/mm/x.o2r"), 3, "GAME_NONE claimed a path");
    MMM_ASSERT(std::string(Combo_ModsSubdirForGame(GAME_MM)) == "mm", 3, "MM's mods subdir is not 'mm'");
    MMM_ASSERT(std::string(Combo_ModsSubdirForGame(GAME_OOT)).empty(), 3, "OoT's mods subdir is not the root");

    // ---- Stage a private mods tree ----------------------------------------
    //   <root>/root-level.o2r      OoT's (the root) — MM must NOT mount it
    //   <root>/mm/10-mm.o2r        MM's, a copy of 2ship.o2r
    //   <root>/mm/sub/20-soh.o2r   MM's, nested, a copy of soh.o2r: an MM mod
    //                              that ships OoT-owned paths, which is the
    //                              strongest form of the shadowing question
    //   <root>/mm/notes.txt        not an archive — must be ignored
    // Stems sort "10-mm" < "20-soh", so 20-soh mounts last and wins.
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::current_path(ec) / "rsbs_test_mods_670";
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "mm" / "sub", ec);
    if (ec) {
        printf("[TEST] FAIL(4): could not stage the mods tree at %s (%s)\n", root.generic_string().c_str(),
               ec.message().c_str());
        return 4;
    }
    const std::string rootMod = (root / "root-level.o2r").generic_string();
    const std::string mmModFirst = (root / "mm" / "10-mm.o2r").generic_string();
    const std::string mmModLast = (root / "mm" / "sub" / "20-soh.o2r").generic_string();
    const auto copyOpts = std::filesystem::copy_options::overwrite_existing;
    std::filesystem::copy_file(mmArchive, rootMod, copyOpts, ec);
    std::filesystem::copy_file(mmArchive, mmModFirst, copyOpts, ec);
    std::filesystem::copy_file(sohArchive, mmModLast, copyOpts, ec);
    if (ec) {
        printf("[TEST] FAIL(4): could not stage the stand-in mod archives (%s)\n", ec.message().c_str());
        std::filesystem::remove_all(root, ec);
        return 4;
    }
    { std::FILE* f = std::fopen((root / "mm" / "notes.txt").string().c_str(), "wb");
      if (f != nullptr) { std::fputs("not an archive\n", f); std::fclose(f); } }

    // Value snapshot of the shared manager's archive list (GetArchives already
    // hands back a freshly built vector, so this does not follow our own
    // AddArchive calls). Restored at the end through SetArchives, which replays
    // it via ResetVirtualFileSystem, so mArchives/mHashes/mFileToArchive all come
    // back to what they were. Restoring only "our" archives would leave the base
    // re-adds this row performs on top, and for the 151 paths oot.o2r and mm.o2r
    // both carry that would silently hand a later MM row OoT's copy.
    auto archiveSnapshot = archiveMgr->GetArchives();

    int rc = 0;
    Combo_ClearModArchives(GAME_OOT);
    Combo_ClearModArchives(GAME_MM);

    do {
        // ---- Part 2: MM's base archive owns the contested path -------------
        // Mount 2ship.o2r explicitly so the "before" state is the one
        // LoadMMArchives creates just before it mounts mods: base on top.
        if (archiveMgr->AddArchive(std::string(mmArchive)) == nullptr) {
            printf("[TEST] FAIL(5): could not mount the MM base archive %s\n", mmArchive);
            rc = 5;
            break;
        }
        const std::string ownerBefore = MmmOwnerOf(archiveMgr, kMmmContestedPath);
        if (ownerBefore.empty()) {
            printf("[TEST] FAIL(5): '%s' resolves to no archive at all — every ownership assertion below would be "
                   "vacuous\n",
                   kMmmContestedPath);
            rc = 5;
            break;
        }
        if (MmmPathContains(ownerBefore, "rsbs_test_mods_670")) {
            printf("[TEST] FAIL(5): precondition — a staged mod already owns '%s' before any mod was mounted\n",
                   kMmmContestedPath);
            rc = 5;
            break;
        }

        // ---- Part 3: the production glob + mount ---------------------------
        const int mounted = MM_MountModArchivesHeadless(root.generic_string().c_str());
        if (mounted != 2) {
            printf("[TEST] FAIL(6): MM mounted %d mod archive(s) from the staged tree, expected exactly 2 (the two "
                   "under mods/mm; the root-level one is OoT's and notes.txt is not an archive)\n",
                   mounted);
            rc = 6;
            break;
        }

        // Both registries, in mount order. Combo_GetModArchiveCount(GAME_MM) is
        // the exact invariant #593's re-apply loop reads and that nothing
        // checked before this row: it was structurally 0 forever.
        if (Combo_GetModArchiveCount(GAME_MM) != 2) {
            printf("[TEST] FAIL(7): Combo_GetModArchiveCount(GAME_MM) is %d, expected 2 — #593's re-apply loop stays "
                   "a no-op for MM\n",
                   Combo_GetModArchiveCount(GAME_MM));
            rc = 7;
            break;
        }
        if (Combo_GetModArchiveCount(GAME_OOT) != 0) {
            printf("[TEST] FAIL(7): an MM mod registration leaked into OoT's list — the switch-time re-apply would "
                   "stack MM's mods over OoT's base archives\n");
            rc = 7;
            break;
        }
        const char* reg0 = Combo_GetModArchive(GAME_MM, 0);
        const char* reg1 = Combo_GetModArchive(GAME_MM, 1);
        if (reg0 == nullptr || reg1 == nullptr || !MmmPathContains(reg0, "10-mm.o2r") ||
            !MmmPathContains(reg1, "20-soh.o2r")) {
            printf("[TEST] FAIL(7): mod precedence order is wrong: [0]='%s' [1]='%s', expected 10-mm then 20-soh "
                   "(sorted by file-name stem)\n",
                   reg0 != nullptr ? reg0 : "(null)", reg1 != nullptr ? reg1 : "(null)");
            rc = 7;
            break;
        }
        // The root-level archive is OoT's and must not be anywhere in MM's list.
        for (int i = 0; i < Combo_GetModArchiveCount(GAME_MM); i++) {
            const char* p = Combo_GetModArchive(GAME_MM, i);
            if (p != nullptr && MmmPathContains(p, "root-level.o2r")) {
                printf("[TEST] FAIL(8): MM registered the root-level mod archive, which is OoT's (the shared mods/ "
                       "tree partition is not being applied)\n");
                rc = 8;
                break;
            }
        }
        if (rc != 0) {
            break;
        }
        // Recorded as MM-owned, which is what the #344 dispatcher and the #618
        // "mm" extension scope key on. Without this a mod's scenes would be
        // parsed with OoT's scene command set and its paths would be invisible to
        // the rescan.
        if (!Combo_ArchivePathIsMM(mmModFirst.c_str()) || !Combo_ArchivePathIsMM(mmModLast.c_str())) {
            printf("[TEST] FAIL(8): a mounted MM mod is not in MM's archive-origin registry — the #344 factory "
                   "dispatcher and the #618 'mm' extension scope cannot see it\n");
            rc = 8;
            break;
        }

        // ---- Part 4: the MM-side override applies -------------------------
        const std::string ownerAfterMods = MmmOwnerOf(archiveMgr, kMmmContestedPath);
        printf("[mm-mods-mount] '%s' owner: base=%s -> after mods/mm=%s\n", kMmmContestedPath, ownerBefore.c_str(),
               ownerAfterMods.c_str());
        if (!MmmPathContains(ownerAfterMods, "20-soh.o2r")) {
            printf("[TEST] FAIL(9): after mounting mods/mm the contested path is still owned by '%s', so MM's mods "
                   "do not override its base archives (#670)\n",
                   ownerAfterMods.c_str());
            rc = 9;
            break;
        }

        // ---- Part 5: an OoT path is NOT shadowed by an MM mod -------------
        // The production switch-time mount, unmodified, with an EMPTY OoT mod
        // registry — so the only thing that can reclaim the path is the base
        // re-add. 20-soh.o2r (a copy of soh.o2r) stays mounted throughout: this
        // is an MM mod that genuinely carries OoT-owned paths, and it must lose
        // them the moment OoT is the active game.
        Combo_EnsureGameArchivesLoaded(GAME_OOT);
        const std::string ownerInOot = MmmOwnerOf(archiveMgr, kMmmContestedPath);
        printf("[mm-mods-mount] after EnsureGameArchivesLoaded(GAME_OOT): '%s' owner=%s\n", kMmmContestedPath,
               ownerInOot.c_str());
        if (MmmPathContains(ownerInOot, "rsbs_test_mods_670")) {
            printf("[TEST] FAIL(10): an MM mod still owns an OoT-owned path after the switch to OoT — an MM mod "
                   "shadows OoT (#670)\n");
            rc = 10;
            break;
        }
        if (!MmmPathContains(ownerInOot, "soh.o2r")) {
            printf("[TEST] FAIL(10): after the switch to OoT the contested path is owned by '%s', not by soh.o2r — "
                   "the base re-add did not reclaim it and this leg proves nothing\n",
                   ownerInOot.c_str());
            rc = 10;
            break;
        }

        // ---- Part 6: the override survives the switch back to MM ----------
        Combo_EnsureGameArchivesLoaded(GAME_MM);
        const std::string ownerBackInMm = MmmOwnerOf(archiveMgr, kMmmContestedPath);
        printf("[mm-mods-mount] after EnsureGameArchivesLoaded(GAME_MM): '%s' owner=%s\n", kMmmContestedPath,
               ownerBackInMm.c_str());
        if (!MmmPathContains(ownerBackInMm, "20-soh.o2r")) {
            printf("[TEST] FAIL(11): the MM mod override was not re-applied on the switch back to MM ('%s' owns the "
                   "path) — the mount did not feed the #593 registry usefully (#670)\n",
                   ownerBackInMm.c_str());
            rc = 11;
            break;
        }

        // ---- Part 7: mounting twice changes nothing ------------------------
        // LoadMMArchives can run again after a game switch. A second pass must
        // not grow either registry or move mod precedence.
        const int mountedAgain = MM_MountModArchivesHeadless(root.generic_string().c_str());
        if (mountedAgain != 2 || Combo_GetModArchiveCount(GAME_MM) != 2) {
            printf("[TEST] FAIL(12): a second mount reported %d and left %d registered entries, expected 2 and 2\n",
                   mountedAgain, Combo_GetModArchiveCount(GAME_MM));
            rc = 12;
            break;
        }
        const char* again0 = Combo_GetModArchive(GAME_MM, 0);
        if (again0 == nullptr || !MmmPathContains(again0, "10-mm.o2r")) {
            printf("[TEST] FAIL(12): a second mount moved mod precedence ([0]='%s')\n",
                   again0 != nullptr ? again0 : "(null)");
            rc = 12;
            break;
        }

        // An absent mods root is the normal case for most installs, and must be
        // a quiet zero rather than a failure.
        const std::string missing = (root / "does-not-exist").generic_string();
        if (MM_MountModArchivesHeadless(missing.c_str()) != 0) {
            printf("[TEST] FAIL(13): mounting a non-existent mods root did not report 0\n");
            rc = 13;
            break;
        }
        if (MM_MountModArchivesHeadless(nullptr) != -1 || MM_MountModArchivesHeadless("") != -1) {
            printf("[TEST] FAIL(13): a NULL/empty mods root did not report -1\n");
            rc = 13;
            break;
        }
    } while (false);

    // Leave the shared manager and both registries EXACTLY as we found them.
    archiveMgr->SetArchives(archiveSnapshot);
    Combo_ClearModArchives(GAME_OOT);
    Combo_ClearModArchives(GAME_MM);
    std::filesystem::remove_all(root, ec);

    if (rc == 0) {
        printf("[mm-mods-mount] PASS: 2 of 4 staged files mounted (mods/mm only, nested included, sorted by stem), "
               "both registries fed, the override beats MM's base archive, an OoT path is reclaimed by soh.o2r on "
               "the switch to OoT and the override returns on the switch back, and a second mount is a no-op\n");
    }
    return rc;
}
