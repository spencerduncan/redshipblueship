/**
 * @file test_loose_mods_mount.c
 * @brief #705: a loose (unpacked) asset folder in each game's half of the shared
 *        mods/ tree mounts as an archive, overrides like a packed mod, and does not
 *        cross games.
 *
 * TWO CTest rows (label "redship"), both dispatched from src/common/test_runner.cpp:
 *
 *   LooseModsDiscovery ("loose-mods-discovery") — which folders are each game's
 *     loose layer (Rsbs::FindLooseModDirs), over a staged directory tree. Needs no
 *     archive and no Ship::Context, so it NEVER skips (it runs in the archive-less
 *     netplay-relay job too, #562).
 *   LooseModsMount ("loose-mods-mount") — the real mounts: both ports' production
 *     mod paths (OoT_MountModArchivesHeadless, MM_MountModArchivesHeadless) over a
 *     staged mods tree holding a loose file under EACH partition, then the
 *     production switch-time Combo_EnsureGameArchivesLoaded in both directions.
 *     SKIPs when soh.o2r or 2ship.o2r is unstaged, the #670 row's policy.
 *
 * WHAT WAS MISSING. libultraship can serve a directory as an archive — FolderArchive,
 * constructed by ArchiveManager::AddArchive for a path with no extension — but no
 * caller in this tree ever passed it one: OoT's mod walk and MM's (#670) both hand it
 * archive FILES, and libultraship's own default `mods` path is only used when a port
 * passes no archive list, which neither does. So an unpacked texture could not be
 * applied by either game.
 *
 * WHAT LooseModsMount ASSERTS, each as a measured change of state:
 *
 *   1. OoT's production path (walk + the same mount/register pair its init leg calls
 *      + the loose layer after them) claims its packed mod AND `<mods>/loose`, in that
 *      order, and nothing from `<mods>/mm`.
 *   2. The OoT loose file OVERRIDES: `portVersion`, which soh.o2r owns beforehand, is
 *      now served by the loose folder, with the loose file's own bytes — and a
 *      packed OoT mod carrying the same path was mounted before it and lost, so the
 *      loose layer is last. A nested loose path resolves too, which is what pins the
 *      separator handling (FolderArchive derives names from '/' listings).
 *   3. Entering MM does not carry OoT's loose file across: after the production
 *      Combo_EnsureGameArchivesLoaded(GAME_MM) with an EMPTY MM registry, a base MM
 *      archive owns `portVersion` again.
 *   4. MM's production path claims its packed mod AND `<mods>/mm/loose`, records the
 *      loose folder as an MM archive (Combo_ArchivePathIsMM — the #344 dispatcher and
 *      the #618 "mm" scope key on it) and not OoT's, and leaves OoT's registry alone.
 *      The MM loose file then overrides `portVersion` with its own bytes.
 *   5. The switch back to OoT serves OoT's loose bytes again (MM's loose file does
 *      not survive into OoT), and the switch to MM serves MM's again (both via the
 *      #593 registry the two mounts fed).
 *   6. Only the loose folders are mounted, never their parents: a `portVersion` file
 *      sitting directly in the mods root and in `mods/mm` is never served, and the
 *      paths `loose/portVersion` / `mm/loose/portVersion` (which exist only if a
 *      parent were mounted as a folder) are never resolvable.
 *   7. A file added to a loose folder after it was mounted is picked up on the next
 *      arrival in that game (the re-apply re-walks the folder), as docs/MODDING.md
 *      states.
 *   8. A second MM mount is idempotent, and with MM's root a DIFFERENT directory
 *      (the non-portable case) OoT's loose layer is still only `<mods>/loose`.
 *
 * Content, not just ownership: every override assertion reads the resolved file's
 * BYTES through ArchiveManager::LoadFile and compares them with the marker the row
 * wrote, so "the loose folder owns the path" is also "the loose file is what a load
 * returns".
 *
 * The staged tree lives in a private directory under the CWD and is deleted
 * afterwards; the shared ArchiveManager is restored from a snapshot and both mod
 * registries are cleared, the #670 row's discipline. (MM's archive-origin registry,
 * RecordMMArchivePath, is append-only by design and keeps the staged path, exactly
 * as it keeps the #670 row's.)
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++).
 */

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../mod_archives.h"

extern "C" {
int MM_MountModArchivesHeadless(const char* modsRoot);
int OoT_MountModArchivesHeadless(const char* modsRoot, const char* mmModsRoot);
bool Combo_ArchivePathIsMM(const char* path);
}

namespace {

#define LMM_ASSERT(cond, code, msg)                                                     \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

// A path every curated base archive carries (the exporter writes it), so ownership
// of it is genuinely contested; the loose files below carry it too.
constexpr const char kLmmContested[] = "portVersion";
constexpr const char kLmmOotNested[] = "rsbs705/nested/oot-probe";
constexpr const char kLmmMmNested[] = "rsbs705/nested/mm-probe";
constexpr const char kLmmLate[] = "rsbs705/late-probe";

constexpr const char kLmmOotLooseBytes[] = "RSBS705:OOT-LOOSE";
constexpr const char kLmmMmLooseBytes[] = "RSBS705:MM-LOOSE";
constexpr const char kLmmOotNestedBytes[] = "RSBS705:OOT-NESTED";
constexpr const char kLmmMmNestedBytes[] = "RSBS705:MM-NESTED";
constexpr const char kLmmRootBytes[] = "RSBS705:MODS-ROOT-IS-NOT-LOOSE";
constexpr const char kLmmMmRootBytes[] = "RSBS705:MODS-MM-IS-NOT-LOOSE";

bool LmmWrite(const std::filesystem::path& p, const char* bytes) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    if (ec) {
        return false;
    }
    std::FILE* f = std::fopen(p.string().c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const size_t len = std::strlen(bytes);
    const bool ok = std::fwrite(bytes, 1, len, f) == len;
    return std::fclose(f) == 0 && ok;
}

std::string LmmOwnerOf(const std::shared_ptr<Ship::ArchiveManager>& mgr, const char* path) {
    auto archive = mgr->GetArchiveFromFile(path);
    return archive != nullptr ? archive->GetPath() : std::string();
}

// The bytes a load of @p path returns through the shared manager, or "" when it
// resolves to nothing.
std::string LmmBytesOf(const std::shared_ptr<Ship::ArchiveManager>& mgr, const char* path) {
    auto file = mgr->LoadFile(std::string(path));
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        return std::string();
    }
    return std::string(file->Buffer->data(), file->Buffer->size());
}

bool LmmEndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool LmmContains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

/**
 * Which folders are each game's loose layer. Disk-shaped (it lists two directory
 * levels) but needs no archive and no Ship::Context, so it never skips.
 *
 * @return 0 pass, non-zero failure code.
 */
extern "C" int LooseModsDiscovery_RunHeadless(void) {
    printf("[TEST] loose-mods-discovery: each game's loose asset folder is found only in its own half of the shared "
           "mods/ tree (#705)\n");

    LMM_ASSERT(std::string(Combo_LooseModsDirName()) == "loose", 2, "the loose folder name is not 'loose'");

    // Staged tree. Case is varied on purpose so the match is measured as
    // case-insensitive on every platform without needing two spellings of one name
    // (which a case-insensitive filesystem would merge):
    //   Loose/             OoT's loose layer
    //   Mm/LOOSE/          MM's loose layer
    //   my-pack/loose/     neither: `loose` counts only as the FIRST folder of a half
    //   mmx/loose/         neither: `mmx` is not `mm`, and for OoT this is second-level
    //   Mm/sub/loose/      neither: second-level inside MM's half
    //   Mm/loosely/        neither: a near-miss name
    //   notes-loose        a FILE, never a folder to mount
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::current_path(ec) / "rsbs_test_loose_705_discovery";
    std::filesystem::remove_all(root, ec);
    const std::filesystem::path kDirs[] = {
        root / "Loose",         root / "Mm" / "LOOSE",         root / "my-pack" / "loose",
        root / "mmx" / "loose", root / "Mm" / "sub" / "loose", root / "Mm" / "loosely",
    };
    for (const auto& d : kDirs) {
        std::error_code mkEc;
        std::filesystem::create_directories(d, mkEc);
        if (mkEc) {
            printf("[TEST] FAIL(3): could not stage %s (%s)\n", d.generic_string().c_str(), mkEc.message().c_str());
            std::filesystem::remove_all(root, ec);
            return 3;
        }
    }
    if (!LmmWrite(root / "notes-loose", "a file, not a folder")) {
        printf("[TEST] FAIL(3): could not stage the file control\n");
        std::filesystem::remove_all(root, ec);
        return 3;
    }

    int rc = 0;
    do {
        const std::string rootStr = root.generic_string();
        const auto oot = Rsbs::FindLooseModDirs(GAME_OOT, rootStr);
        const auto mm = Rsbs::FindLooseModDirs(GAME_MM, rootStr);
        if (oot.size() != 1 || !LmmEndsWith(oot[0], "/Loose")) {
            printf("[TEST] FAIL(4): OoT's loose folders under the staged tree: %d (first '%s'), expected exactly "
                   "<root>/Loose\n",
                   (int)oot.size(), oot.empty() ? "" : oot[0].c_str());
            rc = 4;
            break;
        }
        if (mm.size() != 1 || !LmmEndsWith(mm[0], "/Mm/LOOSE")) {
            printf("[TEST] FAIL(5): MM's loose folders under the staged tree: %d (first '%s'), expected exactly "
                   "<root>/Mm/LOOSE\n",
                   (int)mm.size(), mm.empty() ? "" : mm[0].c_str());
            rc = 5;
            break;
        }
        // Each found folder belongs to its own game by the shared partition predicate
        // and NOT to the other: the loose layer cannot cross games.
        if (!Combo_ModPathIsForGame(GAME_OOT, rootStr.c_str(), oot[0].c_str()) ||
            Combo_ModPathIsForGame(GAME_MM, rootStr.c_str(), oot[0].c_str()) ||
            !Combo_ModPathIsForGame(GAME_MM, rootStr.c_str(), mm[0].c_str()) ||
            Combo_ModPathIsForGame(GAME_OOT, rootStr.c_str(), mm[0].c_str())) {
            printf("[TEST] FAIL(6): a found loose folder is not claimed by exactly its own game under the partition\n");
            rc = 6;
            break;
        }
        // Degenerate arguments find nothing.
        if (!Rsbs::FindLooseModDirs(GAME_NONE, rootStr).empty() || !Rsbs::FindLooseModDirs(GAME_OOT, "").empty() ||
            !Rsbs::FindLooseModDirs(GAME_MM, (root / "does-not-exist").generic_string()).empty() ||
            !Rsbs::FindLooseModDirs(GAME_OOT, (root / "notes-loose").generic_string()).empty()) {
            printf("[TEST] FAIL(7): an unknown game, an empty root, a missing root or a file root found a loose "
                   "folder\n");
            rc = 7;
            break;
        }
    } while (false);

    std::filesystem::remove_all(root, ec);
    if (rc == 0) {
        printf("[loose-mods-discovery] PASS: OoT finds only <mods>/Loose, MM only <mods>/Mm/LOOSE (case-insensitive, "
               "first folder of each half only), each claimed by its own game alone; near-misses, deeper folders and "
               "degenerate arguments find nothing\n");
    }
    return rc;
}

/**
 * @param sohArchive a staged soh.o2r (OoT's port archive, the base the OoT loose file
 *                   has to beat, and the byte source for the MM packed stand-in).
 * @param mmArchive  a staged 2ship.o2r (MM's port archive, and the byte source for the
 *                   OoT packed stand-in).
 * @return 0 pass, non-zero failure code.
 */
extern "C" int LooseModsMount_RunHeadless(const char* sohArchive, const char* mmArchive) {
    printf("[TEST] loose-mods-mount: a loose file under each game's mods partition overrides like a packed mod and "
           "does not cross games (#705)\n");

    auto ctx = Ship::Context::GetInstance();
    LMM_ASSERT(ctx != nullptr && ctx->GetResourceManager() != nullptr &&
                   ctx->GetResourceManager()->GetArchiveManager() != nullptr,
               1, "no Ship::Context/ResourceManager/ArchiveManager — run the shared bring-up first");
    LMM_ASSERT(sohArchive != nullptr && sohArchive[0] != '\0' && mmArchive != nullptr && mmArchive[0] != '\0', 1,
               "no staged archives supplied");
    auto archiveMgr = ctx->GetResourceManager()->GetArchiveManager();

    // ---- Stage a private mods tree ------------------------------------------
    //   <root>/root-level.o2r                  OoT packed mod (a 2ship.o2r copy: carries portVersion)
    //   <root>/portVersion                     negative control: the mods ROOT is not a loose folder
    //   <root>/loose/portVersion               OoT loose override
    //   <root>/loose/rsbs705/nested/oot-probe  OoT loose, nested
    //   <root>/mm/10-mm.o2r                    MM packed mod (a soh.o2r copy: carries portVersion)
    //   <root>/mm/portVersion                  negative control: mods/mm is not a loose folder
    //   <root>/mm/loose/portVersion            MM loose override
    //   <root>/mm/loose/rsbs705/nested/mm-probe MM loose, nested
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::current_path(ec) / "rsbs_test_loose_705";
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "mm", ec);
    if (ec) {
        printf("[TEST] FAIL(2): could not create %s (%s)\n", root.generic_string().c_str(), ec.message().c_str());
        return 2;
    }
    const auto copyOpts = std::filesystem::copy_options::overwrite_existing;
    struct StagedCopy {
        const char* from;
        std::filesystem::path to;
    };
    const StagedCopy kCopies[] = {
        { mmArchive, root / "root-level.o2r" },
        { sohArchive, root / "mm" / "10-mm.o2r" },
    };
    for (const auto& c : kCopies) {
        std::error_code copyEc;
        std::filesystem::copy_file(c.from, c.to, copyOpts, copyEc);
        if (copyEc) {
            printf("[TEST] FAIL(2): could not stage %s -> %s (%s)\n", c.from, c.to.generic_string().c_str(),
                   copyEc.message().c_str());
            std::filesystem::remove_all(root, ec);
            return 2;
        }
    }
    struct StagedFile {
        std::filesystem::path to;
        const char* bytes;
    };
    const StagedFile kFiles[] = {
        { root / "portVersion", kLmmRootBytes },
        { root / "loose" / "portVersion", kLmmOotLooseBytes },
        { root / "loose" / "rsbs705" / "nested" / "oot-probe", kLmmOotNestedBytes },
        { root / "mm" / "portVersion", kLmmMmRootBytes },
        { root / "mm" / "loose" / "portVersion", kLmmMmLooseBytes },
        { root / "mm" / "loose" / "rsbs705" / "nested" / "mm-probe", kLmmMmNestedBytes },
    };
    for (const auto& f : kFiles) {
        if (!LmmWrite(f.to, f.bytes)) {
            printf("[TEST] FAIL(2): could not stage %s\n", f.to.generic_string().c_str());
            std::filesystem::remove_all(root, ec);
            return 2;
        }
    }

    auto archiveSnapshot = archiveMgr->GetArchives();
    Combo_ClearModArchives(GAME_OOT);
    Combo_ClearModArchives(GAME_MM);
    const std::string rootStr = root.generic_string();
    // What the helper registers: absolute, normal, '/'-separated.
    const std::string ootLooseAbs = std::filesystem::absolute(root / "loose").lexically_normal().generic_string();
    const std::string mmLooseAbs = std::filesystem::absolute(root / "mm" / "loose").lexically_normal().generic_string();

    // The negative controls of part 6, checked after every state change below.
    auto parentsNeverMounted = [&]() -> bool {
        const std::string bytes = LmmBytesOf(archiveMgr, kLmmContested);
        return bytes != kLmmRootBytes && bytes != kLmmMmRootBytes && !archiveMgr->HasFile("loose/portVersion") &&
               !archiveMgr->HasFile("mm/loose/portVersion");
    };

    int rc = 0;
    do {
        // ---- Precondition: OoT's base owns the contested path ---------------
        Combo_EnsureGameArchivesLoaded(GAME_OOT);
        const std::string ownerBase = LmmOwnerOf(archiveMgr, kLmmContested);
        const std::string bytesBase = LmmBytesOf(archiveMgr, kLmmContested);
        printf("[loose-mods-mount] '%s' owner before any mod: %s\n", kLmmContested, ownerBase.c_str());
        if (!LmmContains(ownerBase, "soh.o2r") || LmmContains(ownerBase, "rsbs_test_loose_705") || bytesBase.empty()) {
            printf("[TEST] FAIL(3): precondition — '%s' should be served by soh.o2r before any mod is mounted, got "
                   "'%s' (%d bytes); every override below would be vacuous\n",
                   kLmmContested, ownerBase.c_str(), (int)bytesBase.size());
            rc = 3;
            break;
        }
        if (archiveMgr->HasFile(kLmmOotNested) || archiveMgr->HasFile(kLmmMmNested)) {
            printf("[TEST] FAIL(3): precondition — a probe path resolves before any loose folder is mounted\n");
            rc = 3;
            break;
        }

        // ---- Part 1: OoT's production path ----------------------------------
        const int ootClaimed = OoT_MountModArchivesHeadless(rootStr.c_str(), rootStr.c_str());
        const char* oot0 = Combo_GetModArchive(GAME_OOT, 0);
        const char* oot1 = Combo_GetModArchive(GAME_OOT, 1);
        if (ootClaimed != 2 || Combo_GetModArchiveCount(GAME_OOT) != 2 || oot0 == nullptr || oot1 == nullptr ||
            !LmmContains(oot0, "root-level.o2r") || ootLooseAbs != oot1) {
            printf("[TEST] FAIL(4): OoT claimed %d and registered %d ([0]='%s' [1]='%s'); expected 2: root-level.o2r "
                   "then '%s' (the loose layer LAST)\n",
                   ootClaimed, Combo_GetModArchiveCount(GAME_OOT), oot0 != nullptr ? oot0 : "(null)",
                   oot1 != nullptr ? oot1 : "(null)", ootLooseAbs.c_str());
            rc = 4;
            break;
        }
        if (Combo_GetModArchiveCount(GAME_MM) != 0) {
            printf("[TEST] FAIL(4): OoT's mount registered something under GAME_MM\n");
            rc = 4;
            break;
        }

        // ---- Part 2: the OoT loose file overrides base AND packed -----------
        const std::string ownerOot = LmmOwnerOf(archiveMgr, kLmmContested);
        const std::string bytesOot = LmmBytesOf(archiveMgr, kLmmContested);
        printf("[loose-mods-mount] after OoT's mods: '%s' owner=%s bytes='%s'\n", kLmmContested, ownerOot.c_str(),
               bytesOot.c_str());
        if (ownerOot != ootLooseAbs || bytesOot != kLmmOotLooseBytes) {
            printf("[TEST] FAIL(5): the OoT loose file does not override '%s' (owner '%s'); soh.o2r and the packed "
                   "root-level.o2r both carry it, so the loose folder must be mounted last and serve its own bytes\n",
                   kLmmContested, ownerOot.c_str());
            rc = 5;
            break;
        }
        if (LmmBytesOf(archiveMgr, kLmmOotNested) != kLmmOotNestedBytes) {
            printf("[TEST] FAIL(5): the nested OoT loose file '%s' does not resolve to its bytes — the loose "
                   "folder's resource names are not derived from '/'-separated relative paths\n",
                   kLmmOotNested);
            rc = 5;
            break;
        }
        if (!parentsNeverMounted()) {
            printf("[TEST] FAIL(9): after OoT's mount a file in a PARENT of the loose folder is served\n");
            rc = 9;
            break;
        }

        // ---- Part 3: entering MM does not carry OoT's loose file across -----
        Combo_EnsureGameArchivesLoaded(GAME_MM);
        const std::string ownerMmBase = LmmOwnerOf(archiveMgr, kLmmContested);
        printf("[loose-mods-mount] after EnsureGameArchivesLoaded(GAME_MM), no MM mods yet: '%s' owner=%s\n",
               kLmmContested, ownerMmBase.c_str());
        if (ownerMmBase.empty() || LmmContains(ownerMmBase, "rsbs_test_loose_705")) {
            printf("[TEST] FAIL(6): entering MM left '%s' owned by '%s' — OoT's loose folder crosses into MM\n",
                   kLmmContested, ownerMmBase.c_str());
            rc = 6;
            break;
        }

        // ---- Part 4: MM's production path -----------------------------------
        const int mmMounted = MM_MountModArchivesHeadless(rootStr.c_str());
        const char* mm0 = Combo_GetModArchive(GAME_MM, 0);
        const char* mm1 = Combo_GetModArchive(GAME_MM, 1);
        if (mmMounted != 2 || Combo_GetModArchiveCount(GAME_MM) != 2 || mm0 == nullptr || mm1 == nullptr ||
            !LmmContains(mm0, "10-mm.o2r") || mmLooseAbs != mm1) {
            printf("[TEST] FAIL(7): MM mounted %d and registered %d ([0]='%s' [1]='%s'); expected 2: 10-mm.o2r then "
                   "'%s' (the loose layer LAST)\n",
                   mmMounted, Combo_GetModArchiveCount(GAME_MM), mm0 != nullptr ? mm0 : "(null)",
                   mm1 != nullptr ? mm1 : "(null)", mmLooseAbs.c_str());
            rc = 7;
            break;
        }
        if (!Combo_ArchivePathIsMM(mmLooseAbs.c_str()) || Combo_ArchivePathIsMM(ootLooseAbs.c_str())) {
            printf("[TEST] FAIL(7): MM's archive-origin registry has the loose folders wrong (mm/loose recorded: %d, "
                   "OoT's loose recorded: %d; expected 1 and 0)\n",
                   (int)Combo_ArchivePathIsMM(mmLooseAbs.c_str()), (int)Combo_ArchivePathIsMM(ootLooseAbs.c_str()));
            rc = 7;
            break;
        }
        if (Combo_GetModArchiveCount(GAME_OOT) != 2 || ootLooseAbs != Combo_GetModArchive(GAME_OOT, 1)) {
            printf("[TEST] FAIL(7): MM's mount changed OoT's registry\n");
            rc = 7;
            break;
        }
        const std::string ownerMm = LmmOwnerOf(archiveMgr, kLmmContested);
        const std::string bytesMm = LmmBytesOf(archiveMgr, kLmmContested);
        printf("[loose-mods-mount] after MM's mods: '%s' owner=%s bytes='%s'\n", kLmmContested, ownerMm.c_str(),
               bytesMm.c_str());
        if (ownerMm != mmLooseAbs || bytesMm != kLmmMmLooseBytes ||
            LmmBytesOf(archiveMgr, kLmmMmNested) != kLmmMmNestedBytes) {
            printf("[TEST] FAIL(8): the MM loose files do not override (owner '%s', bytes '%s')\n", ownerMm.c_str(),
                   bytesMm.c_str());
            rc = 8;
            break;
        }
        if (!parentsNeverMounted()) {
            printf("[TEST] FAIL(9): after MM's mount a file in a PARENT of a loose folder is served\n");
            rc = 9;
            break;
        }

        // ---- Part 5: the switch round trip ----------------------------------
        Combo_EnsureGameArchivesLoaded(GAME_OOT);
        const std::string bytesBackInOot = LmmBytesOf(archiveMgr, kLmmContested);
        printf("[loose-mods-mount] after EnsureGameArchivesLoaded(GAME_OOT): '%s' owner=%s bytes='%s'\n", kLmmContested,
               LmmOwnerOf(archiveMgr, kLmmContested).c_str(), bytesBackInOot.c_str());
        if (bytesBackInOot != kLmmOotLooseBytes || LmmOwnerOf(archiveMgr, kLmmContested) != ootLooseAbs) {
            printf("[TEST] FAIL(10): back in OoT, '%s' reads '%s' — expected OoT's loose bytes; MM's loose file "
                   "crossed into OoT or OoT's loose layer was not re-applied\n",
                   kLmmContested, bytesBackInOot.c_str());
            rc = 10;
            break;
        }
        Combo_EnsureGameArchivesLoaded(GAME_MM);
        const std::string bytesBackInMm = LmmBytesOf(archiveMgr, kLmmContested);
        if (bytesBackInMm != kLmmMmLooseBytes || LmmOwnerOf(archiveMgr, kLmmContested) != mmLooseAbs) {
            printf("[TEST] FAIL(10): back in MM, '%s' reads '%s' — expected MM's loose bytes\n", kLmmContested,
                   bytesBackInMm.c_str());
            rc = 10;
            break;
        }
        if (!parentsNeverMounted()) {
            printf("[TEST] FAIL(9): after the round trip a file in a PARENT of a loose folder is served\n");
            rc = 9;
            break;
        }

        // ---- Part 7: a file added later is picked up on the next arrival ----
        if (archiveMgr->HasFile(kLmmLate)) {
            printf("[TEST] FAIL(11): precondition — the late probe resolves before it was written\n");
            rc = 11;
            break;
        }
        if (!LmmWrite(root / "loose" / "rsbs705" / "late-probe", kLmmOotLooseBytes)) {
            printf("[TEST] FAIL(11): could not write the late probe\n");
            rc = 11;
            break;
        }
        Combo_EnsureGameArchivesLoaded(GAME_OOT);
        if (LmmOwnerOf(archiveMgr, kLmmLate) != ootLooseAbs) {
            printf("[TEST] FAIL(11): a file added to OoT's loose folder after mount was not picked up on the next "
                   "arrival in OoT\n");
            rc = 11;
            break;
        }

        // ---- Part 8: idempotence, and the unshared-roots case ---------------
        const int mmAgain = MM_MountModArchivesHeadless(rootStr.c_str());
        if (mmAgain != 2 || Combo_GetModArchiveCount(GAME_MM) != 2 || mmLooseAbs != Combo_GetModArchive(GAME_MM, 1)) {
            printf("[TEST] FAIL(12): a second MM mount reported %d and left %d registered (expected 2, 2, loose "
                   "last)\n",
                   mmAgain, Combo_GetModArchiveCount(GAME_MM));
            rc = 12;
            break;
        }
        Combo_ClearModArchives(GAME_OOT);
        const std::string foreignMmRoot = (root / "not-mms-root").generic_string();
        const int ootUnshared = OoT_MountModArchivesHeadless(rootStr.c_str(), foreignMmRoot.c_str());
        // root-level.o2r, mm/10-mm.o2r (OoT keeps mods/mm when MM is not globbing
        // it, #670), and OoT's loose folder — but NOT mm/loose as a loose folder.
        bool sawMmLoose = false;
        for (int i = 0; i < Combo_GetModArchiveCount(GAME_OOT); i++) {
            const char* p = Combo_GetModArchive(GAME_OOT, i);
            if (p != nullptr && mmLooseAbs == p) {
                sawMmLoose = true;
            }
        }
        const char* ootLast = Combo_GetModArchive(GAME_OOT, Combo_GetModArchiveCount(GAME_OOT) - 1);
        if (ootUnshared != 3 || sawMmLoose || ootLast == nullptr || ootLooseAbs != ootLast) {
            printf("[TEST] FAIL(13): with MM globbing a different root, OoT claimed %d (expected 3: two archives "
                   "and <mods>/loose last), mm/loose registered as OoT's: %d\n",
                   ootUnshared, (int)sawMmLoose);
            rc = 13;
            break;
        }
    } while (false);

    archiveMgr->SetArchives(archiveSnapshot);
    Combo_ClearModArchives(GAME_OOT);
    Combo_ClearModArchives(GAME_MM);
    std::filesystem::remove_all(root, ec);

    if (rc == 0) {
        printf("[loose-mods-mount] PASS: OoT mounts root-level.o2r then <mods>/loose, MM mounts 10-mm.o2r then "
               "<mods>/mm/loose; each loose file beats its game's base archive and packed mod with its own bytes "
               "(nested paths included), stays out of the other game across both switch directions, is re-applied "
               "on arrival (a file added later is picked up), and no parent folder is ever mounted\n");
    }
    return rc;
}
