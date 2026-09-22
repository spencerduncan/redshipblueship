/**
 * @file test_mm_mods_mount.c
 * @brief #670: MM mounts its mods/ folder in single-exe, the shared mods/ tree is
 *        partitioned between the two games, and an MM mod override survives a
 *        cross-game switch without ever shadowing an OoT path once OoT is active.
 *
 * TWO CTest rows (label "redship"), both dispatched from src/common/test_runner.cpp:
 *
 *   MMModsPartition ("mm-mods-partition") — the shared-tree partition and the
 *     shared extension rule, as pure path logic. No archive, no filesystem, no
 *     Ship::Context, so it NEVER skips: it runs in the archive-less netplay-relay
 *     job too (#562). It used to be part 1 of the row below, which meant the only
 *     lock on Combo_ModPathIsForGame — the predicate BOTH globs depend on — was
 *     skipped in exactly that job, together with a comment claiming the opposite.
 *   MMModsMount ("mm-mods-mount") — everything that needs real archives: both
 *     production globs, both registries, the ownership legs and the switch round
 *     trip. SKIPs when soh.o2r or 2ship.o2r is unstaged.
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
 *   1. The partition, over path spellings that actually occur: "./mods" vs "mods",
 *      '\' separators, "MM" in any case, the near-misses "mmx"/"xmm", a nested
 *      archive, and paths outside the root. Each game's answer is pinned
 *      INDEPENDENTLY against the table (the two sides of the predicate are two
 *      rules now, not one bool and its negation — see MMModsPartition_RunHeadless),
 *      and the derived properties are disjointness everywhere plus totality over
 *      paths under the root. Plus Combo_ModsRootsAreShared, which is what decides
 *      whether there is one shared tree to partition at all, and the shared
 *      extension rule, the other half of "whose file is this": `.o2r` yes, `.otr`
 *      yes (this build compiles the MPQ reader in), `.zip` no for BOTH games.
 *   1b. OoT's real glob honours the partition too. MM's half being locked is the
 *      easy half; the dangerous direction is a mod registered under the WRONG
 *      game, because THAT survives the switch, and before this leg existed the
 *      whole `#ifdef` block in mod_menu.cpp could be deleted with every row still
 *      green. OoT_MountModArchivesHeadless drives CollectOoTModFiles — the same
 *      function UpdateModFiles's own loop iterates — and MountAndRegisterOoTMod, the
 *      same per-archive mount+register pair the init leg calls. The leg asserts OoT
 *      claims the root-level archive and registers NEITHER mods/mm archive under
 *      GAME_OOT. What the seam does NOT drive, and what therefore stays uncovered,
 *      is the init leg's menu bookkeeping around that call: the enabled-set filter
 *      and the stem-keyed `filePaths` map that collapses two same-stem archives in
 *      different subfolders to one registration. PR #704's docstring claimed the
 *      seam registered "exactly as the init leg does"; it does not, and the seam's
 *      own contract now says so.
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
 *   7. The two roots are not always one directory. Handed a DIFFERENT MM root — the
 *      non-portable configuration, where LocateFileAcrossAppDirs really does
 *      separate by app name — OoT's walk claims all three staged archives,
 *      `mods/mm` included, because nothing else is globbing that tree. Without this
 *      leg an archive there is skipped by OoT and never seen by MM: mounted by
 *      NEITHER game, which is the failure the disjointness check in (1) is about and
 *      structurally cannot see, since it is only ever handed one root.
 *   8. One tree, one traversal rule: an archive reachable only through a symlinked
 *      folder under `mods/mm` is mounted, the way OoT has always mounted one under
 *      `mods/`. SKIPS with a printed reason where a directory symlink cannot be
 *      created (Windows without Developer Mode); CI's Linux job runs it.
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
 * it would skip on every CI run. MMModsMount SKIPs when soh.o2r or 2ship.o2r is
 * unstaged, the same policy as the #593/#595/#618 rows — the netplay-relay CI job
 * re-runs this label archive-less on purpose (#562). MMModsPartition needs
 * neither archive and never skips.
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
// OoT's production mods walk (CollectOoTModFiles, the one UpdateModFiles's loop
// iterates) plus the per-archive mount+register pair the init leg calls, over
// explicitly supplied OoT and MM roots (games/oot/soh/Enhancements/mod_menu.cpp,
// #670). The second root is what decides whether `mods/mm` is reserved at all.
int OoT_MountModArchivesHeadless(const char* modsRoot, const char* mmModsRoot);
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
 * The partition, the roots-shared question and the shared extension rule, as path
 * logic: no archive, no Ship::Context.
 *
 * Its own CTest row (MMModsPartition) precisely so it cannot be skipped. This was
 * part 1 of MMModsMount, whose wrapper returns TEST_SKIP before calling the body
 * when soh.o2r or 2ship.o2r is unstaged — so the only lock on
 * Combo_ModPathIsForGame, the predicate both globs share, did not run in the one CI
 * job that deliberately runs this label archive-less (#562), while the comment
 * here claimed "these run even when the archives are unstaged".
 *
 * WHAT THE PARTITION CHECKS MEASURE, and why they did not before. PR #704 shipped a
 * table that checked MM's answer against the authored expectation and then asserted
 * that `oot == mm` never happened. The predicate computed both answers from one bool
 * in one expression (`return game == GAME_MM ? isMm : !isMm;`), so that second
 * assertion was a restatement of the return statement: no input could make it fire,
 * and no counterfactual in that PR ever observed it red. Its reviewer was right.
 * Two changes, because fixing only the first would have left the second assertion
 * just as unreachable:
 *
 *   (a) OoT's side is its OWN rule in the production predicate now ("under the root,
 *       and the first component is not the reserved folder"), not the negation of
 *       MM's, so the two CAN disagree. The table below therefore pins EACH game's
 *       answer per path against the authored expectation — two measurements, not one
 *       restated.
 *   (b) The partition PROPERTIES — never both, never neither under the root — moved
 *       OUT of that table into a sweep over generated paths that carry no authored
 *       expectation at all (kShapes below). Inside the pinned loop those properties
 *       are unreachable BY CONSTRUCTION, however the predicate is written: once
 *       `mm == (owner == kOwnerMm)` and `oot == (owner == kOwnerOoT)` have both
 *       passed, "both claimed" would need `owner` to be two values at once and
 *       "neither claimed, under the root" would need it to be a third. Only a
 *       property asserted about paths nobody wrote an expectation for can go red —
 *       and it does: give MM's rule a prefix match and `mods/mmx/x.o2r` is claimed
 *       TWICE; drop a folder name from OoT's rule and `mods/sub/x.o2r` is claimed by
 *       NOBODY. Neither of those shapes needs to be in the pinned table.
 *
 * A path in some other tree is claimed by NEITHER, which is the honest answer and
 * a behaviour change: the negation-based predicate answered "OoT's" for it.
 *
 * The spellings covered are the ones that actually reach the two globs: modsRoot
 * arrives from LocateFileAcrossAppDirs (can be "./mods"), while mod_menu hands over
 * a lexically_normal()'d path ("mods/mm/x.o2r") and Windows iteration yields '\'
 * separators.
 *
 * @return 0 pass, non-zero failure code.
 */
extern "C" int MMModsPartition_RunHeadless(void) {
    printf("[TEST] mm-mods-partition: the shared mods/ tree partition, the roots-shared gate and the shared "
           "archive-extension rule, as path logic (#670)\n");

    enum ModsOwner { kOwnerMm, kOwnerOoT, kOwnerNeither };
    struct PartitionCase {
        const char* root;
        const char* path;
        ModsOwner owner;
        const char* why;
    };
    static const PartitionCase kCases[] = {
        { "./mods", "./mods/mm/10-mod.o2r", kOwnerMm, "the plain MM case" },
        { "./mods", "mods/mm/10-mod.o2r", kOwnerMm, "a lexically_normal()'d path against a './' root" },
        { "mods", "./mods/mm/10-mod.o2r", kOwnerMm, "a './' path against a bare root" },
        { "./mods", "./mods/mm/sub/20-mod.o2r", kOwnerMm, "nested under mods/mm" },
        { "./mods", "./mods/../mods/mm/10-mod.o2r", kOwnerMm, "a path that normalizes back into mods/mm" },
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
        { "./mods", ".\\mods\\mm\\10-mod.o2r", kOwnerMm, "native Windows separators" },
#endif
        { "./mods", "./mods/MM/10-mod.o2r", kOwnerMm, "the reserved name is case-insensitive" },
        { "./mods", "./mods/10-mod.o2r", kOwnerOoT, "the root belongs to OoT" },
        { "./mods", "./mods/sub/10-mod.o2r", kOwnerOoT, "any other subfolder belongs to OoT" },
        { "./mods", "./mods/mmx/10-mod.o2r", kOwnerOoT, "'mmx' is not 'mm' (prefix near-miss)" },
        { "./mods", "./mods/xmm/10-mod.o2r", kOwnerOoT, "'xmm' is not 'mm' (suffix near-miss)" },
        { "./mods", "./mods/mm-extra/10-mod.o2r", kOwnerOoT, "'mm-extra' is not 'mm'" },
        { "./mods", "./mods/sub/mm/10-mod.o2r", kOwnerOoT, "'mm' is reserved only as the FIRST component" },
        // The three NEITHER cases. These are what makes the disjointness check a
        // measurement rather than a tautology: with OoT's side written as the
        // negation of MM's, every one of them came back "OoT's", so no input could
        // ever produce two equal answers.
        { "./mods", "./elsewhere/mm/10-mod.o2r", kOwnerNeither, "a different tree entirely is neither game's" },
        { "./mods", "./other/10-mod.o2r", kOwnerNeither, "outside the root, and not mm-shaped either" },
        { "./mods", "./mods/../outside/10-mod.o2r", kOwnerNeither, "a path that normalizes OUT of the root" },
        { "./mods", "./mods", kOwnerNeither, "the mods root itself is not a file either game claims" },
    };
    int mmClaims = 0;
    int ootClaims = 0;
    int neitherClaims = 0;
    for (const auto& c : kCases) {
        const bool mm = Combo_ModPathIsForGame(GAME_MM, c.root, c.path);
        const bool oot = Combo_ModPathIsForGame(GAME_OOT, c.root, c.path);
        if (mm != (c.owner == kOwnerMm)) {
            printf("[TEST] FAIL(2): partition: MM %s '%s' under root '%s' (%s)\n",
                   mm ? "claimed" : "did not claim", c.path, c.root, c.why);
            return 2;
        }
        // OoT's answer against the AUTHORED expectation, not against MM's. The two
        // are computed by two independent rules now, so this is a second
        // measurement and not the same one restated.
        if (oot != (c.owner == kOwnerOoT)) {
            printf("[TEST] FAIL(2): partition: OoT %s '%s' under root '%s' (%s)\n",
                   oot ? "claimed" : "did not claim", c.path, c.root, c.why);
            return 2;
        }
        // NO disjointness or totality assertion here, deliberately. With both pins
        // above satisfied those two properties are unreachable by construction — see
        // point (b) of this function's comment — so asserting them in this loop is
        // the same shape of non-lock PR #704 shipped. They are measured below, over
        // paths that carry no authored expectation.
        if (mm) {
            mmClaims++;
        } else if (oot) {
            ootClaims++;
        } else {
            neitherClaims++;
        }
    }
    MMM_ASSERT(mmClaims > 0 && ootClaims > 0 && neitherClaims > 0, 2,
               "the pinned partition table did not exercise all three outcomes, so at least one of the three is not "
               "pinned at all");

    // ---- The partition PROPERTIES, over unpinned paths -------------------------
    // Generated shapes, on purpose: nobody authored an expected owner for these, so
    // "exactly one claimant under the root" and "no claimant outside it" are real
    // measurements of the two rules against each other rather than of each rule
    // against a hand-written answer. This is the half that has a reachable red.
    //
    // Shapes chosen so that a plausible drift in EITHER rule is caught: `mmx`/`xmm`/
    // `mm2`/`m`/`mm-extra` catch an MM rule that starts matching by prefix, substring
    // or truncation (both games would claim them); `sub`/`a`/`z-pack`/`MoDs`/the
    // root-level file catch an OoT rule that stops claiming some ordinary folder
    // (neither game would). Deep tails check that only the FIRST component decides.
    struct Shape {
        const char* first; // the first component under the root; "" for a root-level file
        const char* tail;  // the rest, relative to that component
    };
    static const Shape kShapes[] = {
        { "", "x.o2r" },         { "", "y.otr" },          { "mm", "x.o2r" },
        { "mm", "deep/x.o2r" },  { "MM", "x.o2r" },        { "mM", "deep/deeper/x.otr" },
        { "mmx", "x.o2r" },      { "xmm", "x.o2r" },       { "mm2", "deep/x.o2r" },
        { "m", "x.o2r" },        { "mm-extra", "x.o2r" },  { "mm.o2r", "x.o2r" },
        { "sub", "x.o2r" },      { "sub", "mm/x.o2r" },    { "sub", "deep/mm/x.o2r" },
        { "a", "x.o2r" },        { "z-pack", "deep/x.o2r" }, { "MoDs", "x.o2r" },
        { "notes.txt", "x" },    { "mm ", "x.o2r" },
    };
    // Three root spellings, because the two globs are handed the root by
    // LocateFileAcrossAppDirs and the properties must not depend on how it spelled it.
    const std::string absRoot = (std::filesystem::current_path() / "mods").generic_string();
    const char* kRoots[] = { "./mods", "mods", absRoot.c_str() };
    int sweptUnder = 0;
    for (const char* rootSpelling : kRoots) {
        for (const auto& s : kShapes) {
            std::string p = std::string(rootSpelling) + "/";
            if (s.first[0] != '\0') {
                p += std::string(s.first) + "/";
            }
            p += s.tail;
            const bool mm = Combo_ModPathIsForGame(GAME_MM, rootSpelling, p.c_str());
            const bool oot = Combo_ModPathIsForGame(GAME_OOT, rootSpelling, p.c_str());
            if (mm && oot) {
                printf("[TEST] FAIL(2): partition is not disjoint: BOTH games claimed '%s' under root '%s'. A path "
                       "claimed twice is mounted twice and registered under both games, and #593's switch-time "
                       "re-apply then stacks it over the OTHER game's base archives on every arrival.\n",
                       p.c_str(), rootSpelling);
                return 2;
            }
            if (!mm && !oot) {
                printf("[TEST] FAIL(2): partition is not total: NEITHER game claimed '%s', which is under root '%s'. "
                       "A mod archive there would be mounted by nobody, which is silent.\n",
                       p.c_str(), rootSpelling);
                return 2;
            }
            sweptUnder++;
        }
    }
    // The complement: outside the root, neither game claims. This is where the
    // negation-based predicate was wrong (it answered "OoT's" for every one of
    // these), and it is what makes "total" a statement about a domain instead of
    // about every string.
    static const char* kOutside[] = {
        "./elsewhere/mm/x.o2r", "./elsewhere/x.o2r", "./modsx/mm/x.o2r",   "./modsx/x.o2r",
        "../mods/mm/x.o2r",     "./mods/../x.o2r",   "./mods/../mm/x.o2r", "./mods",
        "./mods/.",             "./mods/..",
    };
    for (const char* p : kOutside) {
        if (Combo_ModPathIsForGame(GAME_MM, "./mods", p) || Combo_ModPathIsForGame(GAME_OOT, "./mods", p)) {
            printf("[TEST] FAIL(2): '%s' is not under './mods', yet a game claimed it — the partition's domain is "
                   "paths under the root, and a claim outside it would have OoT (or MM) mount a file from a tree it "
                   "never walked\n",
                   p);
            return 2;
        }
    }

    // ---- The roots-shared gate ------------------------------------------------
    // Combo_ModPathIsForGame answers "whose half of a SHARED tree is this". It is
    // the right question only when the two games really do glob one directory,
    // which is a portable-build property and NOT a source-level one: with
    // NON_PORTABLE the roots are per-app-name pref directories, and SHIP_HOME
    // collapses them again on Linux even then. PR #704 reserved `mods/mm` from OoT
    // unconditionally and its body claimed "the partition still holds" in that
    // configuration; it does not — an archive at `<soh-prefdir>/mods/mm/x.o2r` was
    // skipped by OoT while MM globbed a different directory entirely, so it was
    // mounted by NEITHER game. That is the same "mounted by nobody" failure the
    // totality sweep above is about, and no partition check can see it, because the
    // partition is asked about one root and this failure lives in the relationship
    // between two. Hence a separate predicate with its own cases.
    struct RootsCase {
        const char* oot;
        const char* mm;
        bool shared;
        const char* why;
    };
    const std::string cwdMods = (std::filesystem::current_path() / "mods").generic_string();
    const RootsCase kRootCases[] = {
        { "./mods", "./mods", true, "the portable-build collapse: one identical string" },
        { "./mods", "mods", true, "two spellings of one directory" },
        { "mods", cwdMods.c_str(), true, "absolute vs relative, same directory" },
        { "./mods", "./mods/mm", false, "MM's own subfolder is not OoT's root" },
        { "./mods", "./other-mods", false, "the non-portable case: two different directories" },
        { "./soh-prefdir/mods", "./2s2h-prefdir/mods", false, "two per-app-name pref directories" },
    };
    for (const auto& c : kRootCases) {
        const bool shared = Combo_ModsRootsAreShared(c.oot, c.mm);
        if (shared != c.shared) {
            printf("[TEST] FAIL(2): Combo_ModsRootsAreShared('%s','%s') = %s, expected %s (%s)\n", c.oot, c.mm,
                   shared ? "true" : "false", c.shared ? "true" : "false", c.why);
            return 2;
        }
    }
    MMM_ASSERT(!Combo_ModsRootsAreShared(nullptr, "./mods"), 3, "a NULL OoT root was called shared");
    MMM_ASSERT(!Combo_ModsRootsAreShared("./mods", nullptr), 3, "a NULL MM root was called shared");
    MMM_ASSERT(!Combo_ModsRootsAreShared("", "./mods"), 3, "an empty OoT root was called shared");
    MMM_ASSERT(!Combo_ModsRootsAreShared("./mods", ""), 3, "an empty MM root was called shared");

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

    // The other half of "whose file is this": ONE extension rule for both halves
    // of the shared tree. MM's glob used to take BenPort's `.zip` while OoT's
    // deliberately refuses it, so the same distribution zip mounted under mods/mm
    // and was ignored under mods/ — one folder tree, two file types.
    MMM_ASSERT(Combo_ModArchiveExtensionIsValid(".o2r"), 3, ".o2r is not accepted as a mod archive");
    MMM_ASSERT(Combo_ModArchiveExtensionIsValid(".O2R"), 3, "the extension rule is case-sensitive");
    // Unconditional, not #ifdef'd: this project sets INCLUDE_MPQ_SUPPORT ON
    // unconditionally (CMakeLists.txt:217) and libultraship exports it PUBLIC
    // (CMakeLists.txt:237). Asserting it here is what would catch the shared
    // predicate's TU losing sight of that definition and silently narrowing OoT's
    // accepted set, which an #ifdef in this file would hide.
    MMM_ASSERT(Combo_ModArchiveExtensionIsValid(".otr"), 3,
               ".otr is not accepted although this build compiles the MPQ reader in");
    MMM_ASSERT(!Combo_ModArchiveExtensionIsValid(".zip"), 3,
               ".zip is accepted as a mod archive — a distribution zip would be mounted as one");
    MMM_ASSERT(!Combo_ModArchiveExtensionIsValid(".txt"), 3, ".txt is accepted as a mod archive");
    MMM_ASSERT(!Combo_ModArchiveExtensionIsValid(""), 3, "an empty extension is accepted as a mod archive");
    MMM_ASSERT(!Combo_ModArchiveExtensionIsValid(nullptr), 3, "a NULL extension is accepted as a mod archive");

    printf("[mm-mods-partition] PASS: %d MM / %d OoT / %d neither pinned path cases, each game's answer pinned "
           "independently; %d unpinned paths under the root each claimed by exactly one game and %d outside it claimed "
           "by neither; %d roots-shared cases; degenerate arguments claim nothing; one extension rule for both games "
           "(.o2r/.otr yes, .zip no)\n",
           mmClaims, ootClaims, neitherClaims, sweptUnder, (int)(sizeof(kOutside) / sizeof(kOutside[0])),
           (int)(sizeof(kRootCases) / sizeof(kRootCases[0])));
    return 0;
}

/**
 * @param sohArchive a staged soh.o2r (an OoT-side archive, and the source for the
 *                   MM mod that deliberately carries OoT-owned paths).
 * @param mmArchive  a staged 2ship.o2r (MM's port-asset archive: the base the
 *                   MM-side override has to beat).
 * @return 0 pass, non-zero failure code.
 */
extern "C" int MMModsMount_RunHeadless(const char* sohArchive, const char* mmArchive) {
    printf("[TEST] mm-mods-mount: MM mounts mods/mm, both globs honour the partition, and the override survives a "
           "switch (#670)\n");

    auto ctx = Ship::Context::GetInstance();
    MMM_ASSERT(ctx != nullptr && ctx->GetResourceManager() != nullptr &&
                   ctx->GetResourceManager()->GetArchiveManager() != nullptr,
               1, "no Ship::Context/ResourceManager/ArchiveManager — run the shared bring-up first");
    MMM_ASSERT(sohArchive != nullptr && sohArchive[0] != '\0' && mmArchive != nullptr && mmArchive[0] != '\0', 1,
               "no staged archives supplied");
    auto archiveMgr = ctx->GetResourceManager()->GetArchiveManager();

    // The pure-path part runs here too, so this row stays self-contained and its
    // preconditions are stated rather than assumed. MMModsPartition is the row that
    // guarantees it also runs with no archives staged.
    const int partitionRc = MMModsPartition_RunHeadless();
    if (partitionRc != 0) {
        return partitionRc;
    }

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
    const std::string notAnArchive = (root / "mm" / "notes.txt").generic_string();

    // ONE error_code PER COPY, checked immediately. std::filesystem's error_code
    // overloads CLEAR ec on success, so sharing one across three copies and
    // checking it after the last means a failure of the FIRST copy is erased by the
    // third's success. The first copy is root-level.o2r — the entire negative
    // control for "MM's glob skips the root". With it silently missing, the
    // staged tree holds 2 MM archives instead of 3 files and MM's mount still
    // reports 2, so the FAIL(6) leg below passes while asserting a property of a
    // file that does not exist. Same for notes.txt: an unchecked fopen would make
    // "ignores a non-archive file" vanish with no assertion at all.
    const auto copyOpts = std::filesystem::copy_options::overwrite_existing;
    struct StagedCopy {
        const char* from;
        const std::string& to;
        const char* why;
    };
    const StagedCopy kCopies[] = {
        { mmArchive, rootMod, "the root-level archive: OoT's, the negative control for MM's partition skip" },
        { mmArchive, mmModFirst, "the first mods/mm archive" },
        { sohArchive, mmModLast, "the nested mods/mm archive that carries OoT-owned paths" },
    };
    for (const auto& c : kCopies) {
        std::error_code copyEc;
        std::filesystem::copy_file(c.from, c.to, copyOpts, copyEc);
        if (copyEc) {
            printf("[TEST] FAIL(4): could not stage %s -> %s (%s) — %s\n", c.from, c.to.c_str(),
                   copyEc.message().c_str(), c.why);
            std::filesystem::remove_all(root, ec);
            return 4;
        }
    }
    {
        std::FILE* f = std::fopen(notAnArchive.c_str(), "wb");
        if (f == nullptr || std::fputs("not an archive\n", f) < 0) {
            if (f != nullptr) {
                std::fclose(f);
            }
            printf("[TEST] FAIL(4): could not write the non-archive control file %s — the 'ignores a non-archive file' "
                   "assertion would not be measuring anything\n",
                   notAnArchive.c_str());
            std::filesystem::remove_all(root, ec);
            return 4;
        }
        std::fclose(f);
    }

    // All four staged entries exist before either glob runs. Without this, every
    // count assertion below is conditional on staging that was never confirmed.
    for (const std::string& staged : { rootMod, mmModFirst, mmModLast, notAnArchive }) {
        std::error_code existsEc;
        if (!std::filesystem::is_regular_file(staged, existsEc)) {
            printf("[TEST] FAIL(4): staged entry %s is not a regular file (%s) — the glob assertions would be "
                   "vacuous\n",
                   staged.c_str(), existsEc.message().c_str());
            std::filesystem::remove_all(root, ec);
            return 4;
        }
    }

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

        // ---- Part 8: the OoT half of the partition -------------------------
        // The MM half above is the easy half. THIS is the direction the whole
        // design is afraid of: a mod registered under the WRONG game survives the
        // switch, because Combo_EnsureGameArchivesLoaded re-applies the arriving
        // game's registered mods on top of its base archives every time. An MM mod
        // in OoT's list would therefore shadow OoT permanently, not just while MM
        // is active — and until this leg existed, the entire RSBS_SINGLE_EXECUTABLE
        // block in mod_menu.cpp could be deleted with every row in the tree still
        // green.
        //
        // Drives OoT's production walk: OoT_MountModArchivesHeadless ->
        // CollectOoTModFiles, the same function UpdateModFiles's own loop iterates,
        // then UpdateModFiles's init-leg registration (AddArchive then
        // Combo_RegisterModArchive(GAME_OOT, ...)). Over the SAME staged tree, so
        // the two globs are measured against one another: MM claimed exactly the
        // two under mods/mm, OoT must claim exactly the one in the root.
        Combo_ClearModArchives(GAME_OOT);
        // Both roots are the staged tree: the SHARED-tree case, which is what a
        // portable build has and what every release ships.
        const int ootClaimed = OoT_MountModArchivesHeadless(root.generic_string().c_str(), root.generic_string().c_str());
        if (ootClaimed != 1 || Combo_GetModArchiveCount(GAME_OOT) != 1) {
            printf("[TEST] FAIL(14): OoT's glob claimed %d archive(s) and registered %d, expected 1 and 1 (only the "
                   "root-level one; the two under mods/mm are MM's and notes.txt is not an archive)\n",
                   ootClaimed, Combo_GetModArchiveCount(GAME_OOT));
            rc = 14;
            break;
        }
        const char* ootReg0 = Combo_GetModArchive(GAME_OOT, 0);
        if (ootReg0 == nullptr || !MmmPathContains(ootReg0, "root-level.o2r")) {
            printf("[TEST] FAIL(14): OoT registered '%s', expected the root-level archive\n",
                   ootReg0 != nullptr ? ootReg0 : "(null)");
            rc = 14;
            break;
        }
        for (int i = 0; i < Combo_GetModArchiveCount(GAME_OOT); i++) {
            const char* p = Combo_GetModArchive(GAME_OOT, i);
            if (p != nullptr && (MmmPathContains(p, "10-mm.o2r") || MmmPathContains(p, "20-soh.o2r"))) {
                printf("[TEST] FAIL(15): OoT registered an archive from mods/mm ('%s'). The #593 switch-time re-apply "
                       "would then stack an MM mod over OoT's base archives on EVERY OoT arrival — the shadowing that "
                       "survives the switch (#670)\n",
                       p);
                rc = 15;
                break;
            }
        }
        if (rc != 0) {
            break;
        }
        // MM's list is untouched by OoT's walk: the partition is not a race between
        // two globs over one registry.
        if (Combo_GetModArchiveCount(GAME_MM) != 2) {
            printf("[TEST] FAIL(15): OoT's walk changed MM's registry (now %d entries, expected 2)\n",
                   Combo_GetModArchiveCount(GAME_MM));
            rc = 15;
            break;
        }
        const std::string rootStr = root.generic_string();
        if (OoT_MountModArchivesHeadless(nullptr, rootStr.c_str()) != -1 ||
            OoT_MountModArchivesHeadless("", rootStr.c_str()) != -1 ||
            OoT_MountModArchivesHeadless(rootStr.c_str(), nullptr) != -1 ||
            OoT_MountModArchivesHeadless(rootStr.c_str(), "") != -1) {
            printf("[TEST] FAIL(15): OoT's seam did not report -1 for a NULL/empty root\n");
            rc = 15;
            break;
        }

        // ---- Part 9: the two roots are NOT always the same directory -------
        // The partition is a property of a SHARED tree, and the tree is shared only
        // because a portable build's LocateFileAcrossAppDirs ignores its appName.
        // With NON_PORTABLE it does not (SDL_GetPrefPath per app name,
        // libultraship/src/ship/Context.cpp), so OoT's root and MM's root are
        // different directories and MM never looks under OoT's. Reserving `mods/mm`
        // from OoT there would leave an archive at `<soh-prefdir>/mods/mm/x.o2r`
        // mounted by NEITHER game — the silent gap the disjointness assertion in
        // MMModsPartition is about and structurally cannot see, since it is handed
        // one root. PR #704 shipped exactly that, with a body claiming the partition
        // still held.
        //
        // Same staged tree, same walk, only MM's root differs: OoT must now claim
        // ALL THREE archives, mods/mm included, i.e. behave exactly as it did before
        // #670. This is the leg that goes red if the gate is deleted (it reported 1).
        Combo_ClearModArchives(GAME_OOT);
        const std::string foreignMmRoot = (root / "not-mms-root").generic_string();
        const int ootClaimedUnshared = OoT_MountModArchivesHeadless(rootStr.c_str(), foreignMmRoot.c_str());
        if (ootClaimedUnshared != 3 || Combo_GetModArchiveCount(GAME_OOT) != 3) {
            printf("[TEST] FAIL(16): with MM globbing a DIFFERENT root, OoT claimed %d archive(s) and registered %d, "
                   "expected 3 and 3 — `mods/mm` is reserved from OoT even though nothing else mounts it, so those "
                   "archives are mounted by neither game (#670, PR #704 review)\n",
                   ootClaimedUnshared, Combo_GetModArchiveCount(GAME_OOT));
            rc = 16;
            break;
        }
        {
            bool sawMmSubdirArchive = false;
            for (int i = 0; i < Combo_GetModArchiveCount(GAME_OOT); i++) {
                const char* p = Combo_GetModArchive(GAME_OOT, i);
                if (p != nullptr && MmmPathContains(p, "20-soh.o2r")) {
                    sawMmSubdirArchive = true;
                }
            }
            if (!sawMmSubdirArchive) {
                printf("[TEST] FAIL(16): OoT claimed 3 archives but not the nested mods/mm one — the count is right "
                       "for the wrong reason\n");
                rc = 16;
                break;
            }
        }
        // MM's registry is untouched: the unshared case changes who claims OoT's
        // tree, not who claims MM's.
        if (Combo_GetModArchiveCount(GAME_MM) != 2) {
            printf("[TEST] FAIL(16): the unshared-roots leg changed MM's registry (now %d entries, expected 2)\n",
                   Combo_GetModArchiveCount(GAME_MM));
            rc = 16;
            break;
        }
        Combo_ClearModArchives(GAME_OOT);

        // ---- Part 10: one tree, one traversal rule ------------------------
        // OoT's walk has always passed `follow_directory_symlink`; MM's new glob
        // passed only `skip_permission_denied`, so the installation docs/MODDING.md
        // blesses — a mod that ships as its own folder, kept in one library and
        // linked into an install — worked under `mods/` and silently did nothing
        // under `mods/mm/`. Both now take Rsbs::kModsWalkOptions.
        //
        // SKIPPED, not failed, where a directory symlink cannot be created: on
        // Windows that needs Developer Mode or SeCreateSymbolicLinkPrivilege, and a
        // row that went red on a privilege would be a worse lock than one that says
        // out loud which half of itself ran. CI's Linux job always creates it.
        {
            const std::filesystem::path linkTarget = root / "linked-library";
            // One error_code per step, checked immediately — the same discipline the
            // staging block above states: std::filesystem's overloads CLEAR ec on
            // success, so a shared one lets a later success erase an earlier failure
            // and the leg would assert a property of a file that is not there.
            std::error_code mkdirEc;
            std::filesystem::create_directories(linkTarget, mkdirEc);
            if (mkdirEc) {
                printf("[TEST] FAIL(17): could not create the symlink target directory (%s)\n",
                       mkdirEc.message().c_str());
                rc = 17;
                break;
            }
            const std::string linkedMod = (linkTarget / "30-link.o2r").generic_string();
            std::error_code copyEc;
            std::filesystem::copy_file(mmArchive, linkedMod, copyOpts, copyEc);
            if (copyEc) {
                printf("[TEST] FAIL(17): could not stage the archive behind the symlink (%s)\n",
                       copyEc.message().c_str());
                rc = 17;
                break;
            }
            std::error_code linkEc;
            std::filesystem::create_directory_symlink(linkTarget, root / "mm" / "linked", linkEc);
            if (linkEc) {
                printf("[mm-mods-mount] NOTE: directory symlinks are unavailable here (%s) — the shared-walk-options "
                       "leg did not run. It runs on CI's Linux job.\n",
                       linkEc.message().c_str());
            } else {
                const int mountedWithLink = MM_MountModArchivesHeadless(rootStr.c_str());
                if (mountedWithLink != 3 || Combo_GetModArchiveCount(GAME_MM) != 3) {
                    printf("[TEST] FAIL(17): with an archive reachable only through a symlinked folder under mods/mm, "
                           "MM mounted %d and has %d registered, expected 3 and 3 — MM's walk is not following "
                           "directory symlinks, which OoT's walk over the same tree does (#670, PR #704 review)\n",
                           mountedWithLink, Combo_GetModArchiveCount(GAME_MM));
                    rc = 17;
                    break;
                }
                bool sawLinked = false;
                for (int i = 0; i < Combo_GetModArchiveCount(GAME_MM); i++) {
                    const char* p = Combo_GetModArchive(GAME_MM, i);
                    if (p != nullptr && MmmPathContains(p, "30-link.o2r")) {
                        sawLinked = true;
                    }
                }
                if (!sawLinked) {
                    printf("[TEST] FAIL(17): MM mounted 3 archives but none of them is the one behind the symlink — "
                           "the count is right for the wrong reason\n");
                    rc = 17;
                    break;
                }
                printf("[mm-mods-mount] the symlinked folder under mods/mm was traversed: 3 MM mods mounted\n");
            }
        }
    } while (false);

    // Leave the shared manager and both registries EXACTLY as we found them.
    archiveMgr->SetArchives(archiveSnapshot);
    Combo_ClearModArchives(GAME_OOT);
    Combo_ClearModArchives(GAME_MM);
    std::filesystem::remove_all(root, ec);

    if (rc == 0) {
        printf("[mm-mods-mount] PASS: MM mounted 2 of 4 staged files (mods/mm only, nested included, sorted by stem) "
               "and OoT claimed exactly the 1 in the root, both registries fed, the override beats MM's base archive, "
               "an OoT path is reclaimed by soh.o2r on the switch to OoT and the override returns on the switch back, "
               "a second mount is a no-op, OoT claims all 3 again when MM's root is a DIFFERENT directory, and MM "
               "traverses a symlinked folder under mods/mm the way OoT traverses one under mods/\n");
    }
    return rc;
}
