/**
 * The `SetMenu`-count invariant — #497 step 2's second half, ADR 0004 §3's
 * "one shell" proviso.
 *
 * ADR 0004 discharges #451 only "while MM's menu is never revived as a second
 * shell". That is a prose proviso, and #446's closure removed the technical
 * deterrent that used to enforce it by accident. #497 asked for it to be
 * mechanized and recorded that it was not: `.github/scripts/check-registrar-
 * elision.sh` carries no `SetMenu`/`BenMenu` check, so the FACT held at every
 * head that was measured while NOTHING asserted it.
 *
 * WHY THE COUNT IS THE INVARIANT, and not something softer. This test answers
 * #497's own open investigation question — "Does `Ship::Gui::SetMenu` hold a
 * single slot or a list?" — by reading libultraship's declaration: a single
 * `std::shared_ptr<GuiWindow> mMenu`. The consequence is the opposite of
 * reassuring. A single slot means a second `SetMenu` call does not collide, does
 * not warn and does not fail: it SILENTLY REPLACES the first, and whichever shell
 * ran second owns the menu. So "there is one menu in the binary" cannot be left
 * to the fact that only one call site happens to be compiled. Both halves are
 * asserted here:
 *
 *   1. Exactly TWO `SetMenu(` call sites exist in the first-party tree, at the
 *      two known paths, and the MM one is in a TU that `games/mm/CMakeLists.txt`
 *      excludes from every target.
 *   2. `BenMenu.cpp` — the shell itself, and the only definition of
 *      `BenMenu::AddWidget` — is named by no target.
 *   3. `Ship::Gui` still holds one menu SLOT, so if upstream ever turns it into a
 *      container the reasoning above stops holding and this row says so.
 *
 * WHAT THIS IS NOT. It is a SOURCE invariant, not an nm-based link check. The
 * link-level complement is `.github/scripts/check-registrar-elision.sh`, which is
 * Linux-only; this runs on every platform and catches the thing that actually
 * happens, which is somebody re-adding a TU in CMake. Its limitation is stated
 * rather than hidden: a TU pulled into the link by a path this scan does not model
 * (a new target file, a glob in another CMakeLists) would not be caught here.
 *
 * ROM-free, display-free, no Ship::Context. Included at FILE SCOPE by
 * test_runner.cpp (compiled as C++), like the other files in this directory.
 */

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef RSBS_SOURCE_DIR
namespace {

/** Source extensions worth scanning for a call site. */
bool SetMenuInvIsSourceFile(const std::filesystem::path& p) {
    const std::string ext = p.extension().string();
    return ext == ".c" || ext == ".cpp" || ext == ".h" || ext == ".hpp";
}

struct SetMenuInvFile {
    std::string relPath; // forward slashes, relative to the repo root
    std::string text;
};

/**
 * Every source file under @p root, with its repo-relative path. The roots passed
 * below are narrow (`games`, `src`, `rsbs`) on purpose: a scan from the repo root
 * would walk `.claude/worktrees/`, where other lanes' full checkouts live, and
 * count their call sites as this tree's.
 */
std::vector<SetMenuInvFile> SetMenuInvSlurp(const std::filesystem::path& repoRoot, const char* relRoot, bool& ok) {
    std::vector<SetMenuInvFile> files;
    std::error_code ec;
    const std::filesystem::path root = repoRoot / relRoot;

    if (!std::filesystem::exists(root, ec) || ec) {
        ok = false;
        return files;
    }
    ok = true;

    for (std::filesystem::recursive_directory_iterator it(root,
                                                          std::filesystem::directory_options::skip_permission_denied,
                                                          ec),
         end;
         it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }
        if (!SetMenuInvIsSourceFile(it->path())) {
            continue;
        }
        std::ifstream file(it->path(), std::ios::binary);
        if (!file.is_open()) {
            continue;
        }
        SetMenuInvFile entry;
        entry.relPath = std::filesystem::relative(it->path(), repoRoot, ec).generic_string();
        if (ec) {
            ec.clear();
            entry.relPath = it->path().generic_string();
        }
        entry.text.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        files.push_back(std::move(entry));
    }
    return files;
}

/** How many times @p needle occurs in @p haystack. */
int SetMenuInvCount(const std::string& haystack, const std::string& needle) {
    int n = 0;
    for (std::size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + needle.size())) {
        n++;
    }
    return n;
}

std::string SetMenuInvReadFile(const std::filesystem::path& path, bool& ok) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        ok = false;
        return std::string();
    }
    ok = true;
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

} // namespace
#endif // RSBS_SOURCE_DIR

TestResult Test_SetMenuCount(void) {
    printf("[TEST] setmenu-count: ADR 0004 §3's \"one shell\" proviso, mechanized (#497 step 2's second half)\n");

#ifndef RSBS_SOURCE_DIR
    printf("[TEST] FAIL: RSBS_SOURCE_DIR is undefined, so this row can assert nothing at all. It is a source\n"
           "[TEST]       invariant by construction; a silent skip would be a green row that checked nothing.\n");
    return TEST_FAIL;
#else
    int failures = 0;
    const std::filesystem::path repoRoot(RSBS_SOURCE_DIR);

    // ---- (1) The call sites -------------------------------------------------
    // "SetMenu(" and not "SetMenu": SetMenuBar( is a different function and must
    // not be counted. libultraship is deliberately out of the scanned roots -- it
    // DEFINES SetMenu, and the invariant is about first-party CALLERS.
    static const char* const kRoots[] = { "games", "src", "rsbs" };
    std::vector<SetMenuInvFile> tree;
    for (const char* relRoot : kRoots) {
        bool ok = false;
        std::vector<SetMenuInvFile> part = SetMenuInvSlurp(repoRoot, relRoot, ok);
        if (!ok) {
            printf("[TEST] FAIL: source root %s/%s not found; this row cannot run from a relocated build and must "
                   "not pretend to\n",
                   RSBS_SOURCE_DIR, relRoot);
            return TEST_FAIL;
        }
        for (SetMenuInvFile& f : part) {
            tree.push_back(std::move(f));
        }
    }
    if (tree.empty()) {
        printf("[TEST] FAIL: scanned games/, src/ and rsbs/ and found no source files at all -- the scan would "
               "vacuously pass, which is worse than not running it\n");
        return TEST_FAIL;
    }

    // The two known call sites. The MM one is in a TU that no target compiles;
    // the OoT one is the live menu. A THIRD anywhere is a second shell, and
    // because Ship::Gui holds one slot (checked below) the loser would simply
    // vanish with no error.
    static const char* const kAllowedCallSites[] = {
        "games/mm/2s2h/BenGui/BenGui.cpp", // excluded from every target -- see (2)
        "games/oot/soh/SohGui/SohGui.cpp", // the one live shell
    };
    int totalCallSites = 0;
    bool sawOoTCallSite = false;
    for (const SetMenuInvFile& f : tree) {
        const int hits = SetMenuInvCount(f.text, "SetMenu(");
        if (hits == 0) {
            continue;
        }
        totalCallSites += hits;
        bool allowed = false;
        for (const char* ok : kAllowedCallSites) {
            if (f.relPath == ok) {
                allowed = true;
            }
        }
        if (f.relPath == "games/oot/soh/SohGui/SohGui.cpp") {
            sawOoTCallSite = true;
            if (hits != 1) {
                printf("[TEST] FAIL: %s calls SetMenu( %d times, expected once\n", f.relPath.c_str(), hits);
                failures++;
            }
        }
        if (!allowed) {
            printf("[TEST] FAIL: %s calls Ship::Gui::SetMenu( (%d time(s)).\n"
                   "[TEST]       ADR 0004 §3 decides there is ONE menu shell, and Ship::Gui holds a single menu\n"
                   "[TEST]       slot -- so a second SetMenu call does not collide, it SILENTLY REPLACES the\n"
                   "[TEST]       first. It also arms #451: a second shell reads gSettings.Menu.* index keys,\n"
                   "[TEST]       which is that issue's real condition (mechanized as the MM-side reader\n"
                   "[TEST]       allowlist in test_cvar_classification.c). Extend SohMenu instead; if this call\n"
                   "[TEST]       is genuinely correct, the ADR has to change first and this allowlist with it.\n",
                   f.relPath.c_str(), hits);
            failures++;
        }
    }
    if (!sawOoTCallSite) {
        printf("[TEST] FAIL: games/oot/soh/SohGui/SohGui.cpp no longer calls SetMenu( -- the one live shell is not "
               "handed to the shared Gui, which means there is no menu at all\n");
        failures++;
    }
    if (totalCallSites != 2) {
        printf("[TEST] FAIL: %d SetMenu( call sites in games/, src/ and rsbs/, expected exactly 2 (MM's excluded one "
               "and OoT's live one)\n",
               totalCallSites);
        failures++;
    }

    // ---- (2) MM's shell is in no target ------------------------------------
    bool cmakeOk = false;
    const std::string mmCMake = SetMenuInvReadFile(repoRoot / "games" / "mm" / "CMakeLists.txt", cmakeOk);
    if (!cmakeOk) {
        printf("[TEST] FAIL: could not read games/mm/CMakeLists.txt\n");
        failures++;
    } else {
        // The blanket exclusion. Its two documented exceptions (UIWidgets.cpp for
        // #383, Menu.cpp for #446) are re-added by name into 2ship_rando_ui;
        // BenGui.cpp and BenMenu.cpp are not, and must not be.
        if (mmCMake.find("list(FILTER ship__ EXCLUDE REGEX \"2s2h/BenGui/.*\\\\.cpp$\")") == std::string::npos) {
            printf("[TEST] FAIL: games/mm/CMakeLists.txt no longer carries the blanket\n"
                   "[TEST]       list(FILTER ship__ EXCLUDE REGEX \"2s2h/BenGui/.*\\\\.cpp$\") exclusion. That filter\n"
                   "[TEST]       is what keeps BenMenu.cpp and BenGui.cpp out of every target, and with them\n"
                   "[TEST]       MM's SetMenu call site. If it was reworded, reword this assertion in the same\n"
                   "[TEST]       commit -- do not drop it.\n");
            failures++;
        }
        static const char* const kMustNameNoTarget[] = { "BenMenu.cpp", "BenGui/BenGui.cpp" };
        for (const char* forbidden : kMustNameNoTarget) {
            if (mmCMake.find(forbidden) != std::string::npos) {
                printf("[TEST] FAIL: games/mm/CMakeLists.txt names \"%s\". #497's decision 1 is that MM's menu shell\n"
                       "[TEST]       is never ported and never revived; re-adding that TU puts a second SetMenu call\n"
                       "[TEST]       site in the link and arms #451.\n",
                       forbidden);
                failures++;
            }
        }
    }

    // ---- (3) Ship::Gui still holds ONE menu slot ---------------------------
    // #497's open investigation question, answered by reading the declaration
    // rather than by assuming. If upstream ever makes this a container, the
    // "silently replaces" reasoning above stops holding and the invariant has to
    // be rethought -- so this is a red row, not a comment.
    bool guiOk = false;
    const std::string guiHeader =
        SetMenuInvReadFile(repoRoot / "libultraship" / "include" / "ship" / "window" / "gui" / "Gui.h", guiOk);
    if (!guiOk) {
        printf("[TEST] WARNING: libultraship/include/ship/window/gui/Gui.h not found (submodule not initialized?) -- "
               "the single-slot half was SKIPPED. The call-site and CMake halves above still ran.\n");
    } else {
        if (guiHeader.find("std::shared_ptr<GuiWindow> mMenu;") == std::string::npos) {
            printf("[TEST] FAIL: Ship::Gui no longer declares a single `std::shared_ptr<GuiWindow> mMenu;`.\n"
                   "[TEST]       The whole reason the SetMenu COUNT is the invariant is that the slot is single,\n"
                   "[TEST]       so a second shell replaces the first with no error. If it became a container,\n"
                   "[TEST]       two shells can now coexist and ADR 0004 §3 needs a different enforcement.\n");
            failures++;
        }
        if (SetMenuInvCount(guiHeader, "void SetMenu(") != 1) {
            printf("[TEST] FAIL: Ship::Gui declares %d SetMenu overloads, expected one\n",
                   SetMenuInvCount(guiHeader, "void SetMenu("));
            failures++;
        }
    }

    if (failures == 0) {
        printf("[TEST] setmenu-count: PASS -- %d SetMenu( call sites (MM's excluded, OoT's live), BenMenu.cpp in no "
               "target, Ship::Gui holds one menu slot\n",
               totalCallSites);
        return TEST_PASS;
    }
    printf("[TEST] setmenu-count: %d failure(s)\n", failures);
    return TEST_FAIL;
#endif // RSBS_SOURCE_DIR
}
