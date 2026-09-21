/**
 * The font-licensing invariant — the license follow-up to #578.
 *
 * Three things were true at once before this row existed, and each of them is a
 * shipping hazard that no build step noticed:
 *
 *   1. `Fipps-Regular.otf` sat in BOTH custom-asset trees
 *      (`games/{oot,mm}/assets/custom/fonts/`) and was packed into `soh.o2r`
 *      and `2ship.o2r` by this repository's own CMake. Its `name` table asserts
 *      `Copyright (c) 2007 by Stefanie Koerner (pheist). All rights reserved.`
 *      with no license description (name ID 13) and no license URL (name ID
 *      14) — i.e. no grant of any kind. It is the only asset in the tree in
 *      that position, and it shipped.
 *   2. The four fonts that DO carry a grant are SIL Open Font License 1.1, and
 *      the OFL requires its text to travel with the font files. It was not in
 *      the tree.
 *   3. The MM half of this combo is CC0-1.0 upstream, and that `LICENSE` was
 *      never copied in with the vendored snapshot, so the grant over
 *      `games/mm/` was invisible inside this repository.
 *
 * All three are now fixed in the tree, and this row is what keeps them fixed.
 * A licensing fact that lives only in `THIRD_PARTY_NOTICES.md` decays on the
 * first upstream sync; the point of asserting it here is that re-adding an
 * unlicensed font, or dropping a required license text, is a RED BUILD naming
 * the file rather than a redistribution problem discovered by a third party.
 *
 * WHY THE RESOLVER IS PART OF THIS ROW. Removing a font that a player may have
 * SELECTED is not a pure deletion. `gOverlayFont` persists the chosen name, and
 * `Ship::GameOverlay::SetCurrentFont` looks it up with `mFonts[name]` —
 * `std::unordered_map::operator[]`, which INSERTS a null-valued entry before it
 * logs the failure and returns (`libultraship/src/ship/window/gui/
 * GameOverlay.cpp`). The inserted row then shows up in `DrawSettings()`'s combo
 * as a selectable font that can never become current. So the game side resolves
 * the persisted name against the names it actually loaded BEFORE calling in.
 * This row exercises that resolver directly, in both directions: an unknown
 * name must come back as the fallback, and a known name must pass through
 * unchanged (a resolver that answered "Press Start 2P" to everything would
 * satisfy only half of it).
 *
 * WHAT THIS IS NOT. It is a source/asset invariant plus one linked pure
 * function. It does not open a window, load an archive or touch ImGui, and it
 * cannot prove what a built `.o2r` contains — the packing path is asserted
 * indirectly, by the absence of the file the packer would have picked up
 * (`OTRExporter/OTRExporter/Main.cpp` reads every file under the custom-assets
 * path that is not a format-suffixed PNG and adds it verbatim, so the tree IS
 * the archive's font manifest).
 *
 * ONE MORE THING THE ARCHIVE LAYER FORCES. The two `OFL.txt` copies are
 * byte-identical, and that is asserted here rather than left to whoever edits
 * them next. Both custom-asset trees are packed to the same archive path
 * (`fonts/OFL.txt`) in two archives that share one last-added-wins
 * `ArchiveManager`, so a per-directory notice makes the shipped license text a
 * function of which game booted first — issue #595, whose row
 * (`test_curated_archive_order.c`) is exactly what caught the first draft of
 * these notices. So one notice covers the whole shipped font set and records the
 * per-tree difference in a column instead.
 *
 * MM's `2s2h/BenPort.cpp` is excluded from every single-exe target
 * (`games/mm/CMakeLists.txt`'s `list(FILTER ship__ EXCLUDE REGEX
 * "2s2h/BenPort\\.cpp$")`) and its only caller, `InitOTR()`, is reached solely
 * from `MM_SDL_main`, which is itself behind `#ifndef RSBS_SINGLE_EXECUTABLE`
 * (`games/mm/src/code/main.c`). So `Ben::ResolveOverlayFontName` cannot be
 * CALLED from here — it is not in the link. Its shape is locked textually
 * instead, which is the honest thing this row can do about a TU nothing
 * compiles.
 *
 * ROM-free, display-free, no Ship::Context. Included at FILE SCOPE by
 * test_runner.cpp (compiled as C++), like the other files in this directory.
 */

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Defined in games/oot/soh/OTRGlobals.cpp. Declared here rather than included
// so redship_common does not take a header dependency on OoT's port glue —
// same approach as test_cvar_classification.c and test_seq_map_bounds.c.
namespace SOH {
const char* ResolveOverlayFontName(const char* requested);
const char* ResolveOverlayFontNameIn(const char* requested, const char* const* loaded, std::size_t loadedCount,
                                     const char* fallback);
const char* const* OverlayFontNames(std::size_t* count);
const char* OverlayFontFallback();
} // namespace SOH

#define FONTLIC_CHECK(cond, msg)                \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

#ifdef RSBS_SOURCE_DIR
namespace {

/** Source extensions worth scanning for a stale font reference. */
bool FontLicIsSourceFile(const std::filesystem::path& p) {
    const std::string ext = p.extension().string();
    return ext == ".c" || ext == ".cpp" || ext == ".h" || ext == ".hpp";
}

struct FontLicFile {
    std::string relPath; // forward slashes, relative to the repo root
    std::string text;
};

/**
 * This file's own repo-relative path.
 *
 * It must be excluded from the source scan below, and the reason is not
 * fastidiousness: this file NECESSARILY contains every string the scan looks
 * for — the removed font's name (in the resolver cases and in the prose) and all
 * three spellings of the resolved `SetCurrentFont` call. Scanned, it would fail
 * itself. The exclusion is asserted to have matched exactly once, so renaming or
 * moving this file cannot quietly turn the scan into one that skips nothing (or,
 * worse, skips a real call site that happens to share the name).
 */
const char* const kFontLicSelfPath = "src/common/tests/test_font_license.c";

std::string FontLicRead(const std::filesystem::path& path, bool& ok) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        ok = false;
        return std::string();
    }
    ok = true;
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

/**
 * Every source file under @p relRoot, with its repo-relative path. The roots are
 * narrow (`games`, `src`, `rsbs`) on purpose: a scan from the repo root would
 * walk `.claude/worktrees/`, where other lanes' full checkouts live, and count
 * their references as this tree's. Mirrors test_setmenu_count.c's slurp.
 */
std::vector<FontLicFile> FontLicSlurp(const std::filesystem::path& repoRoot, const char* relRoot, bool& ok) {
    std::vector<FontLicFile> files;
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
        if (!FontLicIsSourceFile(it->path())) {
            continue;
        }
        bool read = false;
        FontLicFile entry;
        entry.text = FontLicRead(it->path(), read);
        if (!read) {
            continue;
        }
        entry.relPath = std::filesystem::relative(it->path(), repoRoot, ec).generic_string();
        if (ec) {
            ec.clear();
            entry.relPath = it->path().generic_string();
        }
        files.push_back(std::move(entry));
    }
    return files;
}

/** Every regular file directly inside @p dir, by filename. */
std::vector<std::string> FontLicListDir(const std::filesystem::path& dir, bool& ok) {
    std::vector<std::string> names;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec) || ec) {
        ok = false;
        return names;
    }
    ok = true;
    for (std::filesystem::directory_iterator it(dir, ec), end; it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }
        names.push_back(it->path().filename().string());
    }
    return names;
}

bool FontLicHas(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

/// A font this project ships, and the copyright line its own `name` table
/// (name ID 0) carries. The OFL requires that notice to travel with the file,
/// so the OFL.txt beside it must name it.
struct FontLicShippedFont {
    const char* fileName;
    const char* copyrightFragment;
};

/**
 * Every font this project ships, from either tree, with the copyright fragment
 * its own `name` table carries. Verified against each file on 2026-09-21.
 *
 * ONE list for BOTH directories, because both `OFL.txt` copies must name the
 * whole set — see the byte-identity assertion below for why they cannot each
 * name only their own directory.
 */
const FontLicShippedFont kShippedFonts[] = {
    { "Inconsolata-Regular.ttf", "Copyright 2006 The Inconsolata Project Authors" },
    { "Montserrat-Regular.ttf", "Copyright 2011 The Montserrat Project Authors" },
    { "NotoSansJP-Regular.ttf", "with Reserved Font Name 'Source'" },
    { "PressStart2P-Regular.ttf", "Reserved Font Name \"Press Start 2P\"" },
};

/// Which of those files each tree actually carries. MM has no Japanese face.
const char* const kOotFontFiles[] = {
    "Inconsolata-Regular.ttf",
    "Montserrat-Regular.ttf",
    "NotoSansJP-Regular.ttf",
    "PressStart2P-Regular.ttf",
};
const char* const kMmFontFiles[] = {
    "Inconsolata-Regular.ttf",
    "Montserrat-Regular.ttf",
    "PressStart2P-Regular.ttf",
};

} // namespace
#endif // RSBS_SOURCE_DIR

TestResult Test_FontLicense(void) {
    printf("[TEST] font-license: no unlicensed font ships, the OFL and CC0 texts are in the tree, and a stale "
           "gOverlayFont resolves (license follow-up to #578)\n");

    // ---- (0) The resolution RULE, over a set it can be wrong about ----------
    //
    // Driven through ResolveOverlayFontNameIn with a synthetic three-member set
    // whose fallback is its FIRST member, on purpose. The production set has one
    // member and that member IS the fallback, so asking the one-argument form
    // whether it "passed the name through" is unanswerable: pass-through and
    // fallback return the same string. With three members, a resolver that
    // answered the fallback unconditionally fails on "Beta" and "Gamma".
    {
        static const char* const kSynthetic[] = { "Alpha", "Beta", "Gamma" };
        const char* kSyntheticFallback = kSynthetic[0];
        const std::size_t kSyntheticCount = sizeof(kSynthetic) / sizeof(kSynthetic[0]);

        for (std::size_t i = 0; i < kSyntheticCount; i++) {
            const char* resolved =
                SOH::ResolveOverlayFontNameIn(kSynthetic[i], kSynthetic, kSyntheticCount, kSyntheticFallback);
            if (resolved == nullptr || strcmp(resolved, kSynthetic[i]) != 0) {
                printf("[TEST] FAIL: the resolver did not pass the loaded name \"%s\" through — it returned \"%s\". "
                       "A resolver that answers the fallback to every input would satisfy the unknown-name cases "
                       "below while silently discarding every real font selection.\n",
                       kSynthetic[i], resolved == nullptr ? "(null)" : resolved);
                return TEST_FAIL;
            }
        }

        // Non-members, including the shapes a real config can produce.
        static const char* const kSyntheticMisses[] = {
            "Delta",  // simply not loaded
            "alpha",  // right name, wrong case: SetCurrentFont is case-sensitive
            "",       // an empty CVar string
            "Alpha "  // trailing space
        };
        for (const char* miss : kSyntheticMisses) {
            const char* resolved =
                SOH::ResolveOverlayFontNameIn(miss, kSynthetic, kSyntheticCount, kSyntheticFallback);
            if (resolved == nullptr || strcmp(resolved, kSyntheticFallback) != 0) {
                printf("[TEST] FAIL: the resolver answered \"%s\" for the unloaded name \"%s\" instead of the "
                       "fallback \"%s\". An unloaded name reaching SetCurrentFont inserts a null mFonts entry "
                       "(operator[]) that DrawSettings() then offers as a dead font.\n",
                       resolved == nullptr ? "(null)" : resolved, miss, kSyntheticFallback);
                return TEST_FAIL;
            }
        }

        const char* nullCase = SOH::ResolveOverlayFontNameIn(nullptr, kSynthetic, kSyntheticCount, kSyntheticFallback);
        FONTLIC_CHECK(nullCase != nullptr && strcmp(nullCase, kSyntheticFallback) == 0,
                      "the resolver must answer the fallback for a null request, not crash or return null — "
                      "CVarGetString can hand back null if its default is ever dropped");
        printf("[TEST]   rule: %zu loaded names pass through, %zu non-members plus null map to the fallback\n",
               kSyntheticCount, sizeof(kSyntheticMisses) / sizeof(kSyntheticMisses[0]));
    }

    // ---- (0b) The PRODUCTION set the one-argument form closes over ----------
    {
        std::size_t loadedCount = 0;
        const char* const* loaded = SOH::OverlayFontNames(&loadedCount);
        const char* fallback = SOH::OverlayFontFallback();
        FONTLIC_CHECK(loaded != nullptr && fallback != nullptr,
                      "games/oot/soh/OTRGlobals.cpp exposes no overlay-font candidate set");
        FONTLIC_CHECK(loadedCount >= 1, "the overlay-font candidate set is empty, so every persisted name would "
                                        "resolve to a fallback that is itself not loaded");

        // The fallback must be a MEMBER of the loaded set. A fallback that is not
        // loaded reintroduces the exact defect the resolver exists to prevent:
        // SetCurrentFont would insert a null mFonts row for it.
        bool fallbackIsLoaded = false;
        for (std::size_t i = 0; i < loadedCount; i++) {
            if (loaded[i] != nullptr && strcmp(loaded[i], fallback) == 0) {
                fallbackIsLoaded = true;
                break;
            }
        }
        FONTLIC_CHECK(fallbackIsLoaded,
                      "the overlay-font fallback is not one of the names the TU loads — resolving to it would insert "
                      "the very null mFonts entry this resolver exists to prevent");

        // Every declared name resolves to itself through the production entry
        // point, and the removed font does not.
        for (std::size_t i = 0; i < loadedCount; i++) {
            const char* resolved = SOH::ResolveOverlayFontName(loaded[i]);
            if (resolved == nullptr || strcmp(resolved, loaded[i]) != 0) {
                printf("[TEST] FAIL: SOH::ResolveOverlayFontName(\"%s\") returned \"%s\" for a name the TU declares "
                       "it loads.\n",
                       loaded[i], resolved == nullptr ? "(null)" : resolved);
                return TEST_FAIL;
            }
        }
        static const char* const kStaleConfigValues[] = {
            "Fipps",   // the removed font, as an old config still spells it
            "Default", // GameOverlay's initial mCurrentFont, never in mFonts
            ""         // an empty CVar string
        };
        for (const char* stale : kStaleConfigValues) {
            const char* resolved = SOH::ResolveOverlayFontName(stale);
            if (resolved == nullptr || strcmp(resolved, fallback) != 0) {
                printf("[TEST] FAIL: SOH::ResolveOverlayFontName(\"%s\") returned \"%s\", not the fallback \"%s\".\n",
                       stale, resolved == nullptr ? "(null)" : resolved, fallback);
                return TEST_FAIL;
            }
        }
        FONTLIC_CHECK(SOH::ResolveOverlayFontName(nullptr) != nullptr,
                      "SOH::ResolveOverlayFontName(nullptr) must not return null");
        printf("[TEST]   production set: %zu loaded name(s), fallback \"%s\" is one of them, %zu stale values plus "
               "null fall back\n",
               loadedCount, fallback, sizeof(kStaleConfigValues) / sizeof(kStaleConfigValues[0]));
    }

#ifndef RSBS_SOURCE_DIR
    printf("[TEST] FAIL: RSBS_SOURCE_DIR is undefined, so the asset and source halves of this row can assert\n"
           "[TEST]       nothing at all. They are tree invariants by construction; a silent skip would be a green\n"
           "[TEST]       row that checked almost nothing.\n");
    return TEST_FAIL;
#else
    const std::filesystem::path repoRoot(RSBS_SOURCE_DIR);

    // ---- (1) No Fipps* file under either fonts directory -------------------
    // Asserted together with the POSITIVE claim that the expected fonts are
    // still there: "no file named Fipps*" is trivially true of a directory
    // that got wiped, and that would be a worse bug shipped green.
    struct FontDir {
        const char* relPath;
        const char* const* fontFiles;
        std::size_t fontFileCount;
    };
    const FontDir kFontDirs[] = {
        { "games/oot/assets/custom/fonts", kOotFontFiles, sizeof(kOotFontFiles) / sizeof(kOotFontFiles[0]) },
        { "games/mm/assets/custom/fonts", kMmFontFiles, sizeof(kMmFontFiles) / sizeof(kMmFontFiles[0]) },
    };
    std::string oflTexts[sizeof(kFontDirs) / sizeof(kFontDirs[0])];
    std::size_t dirIndex = 0;

    for (const FontDir& dir : kFontDirs) {
        const std::filesystem::path path = repoRoot / dir.relPath;
        bool ok = false;
        const std::vector<std::string> names = FontLicListDir(path, ok);
        if (!ok) {
            printf("[TEST] FAIL: %s is not a directory; this row cannot run from a relocated build and must not "
                   "pretend to\n",
                   dir.relPath);
            return TEST_FAIL;
        }

        for (const std::string& name : names) {
            if (name.rfind("Fipps", 0) == 0) {
                printf("[TEST] FAIL: %s/%s is back. That font asserts \"All rights reserved\" with no license "
                       "description (name ID 13) and no license URL (name ID 14); the custom-asset packer adds "
                       "every file under this directory to the shipped archive verbatim, so re-adding it puts an "
                       "ungranted work into every build.\n",
                       dir.relPath, name.c_str());
                return TEST_FAIL;
            }
        }

        // The positive half: every font that is supposed to be here, is.
        for (std::size_t i = 0; i < dir.fontFileCount; i++) {
            bool found = false;
            for (const std::string& name : names) {
                if (name == dir.fontFiles[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                printf("[TEST] FAIL: %s/%s is missing. The check above passes vacuously over an empty or gutted font "
                       "directory, so the expected set is asserted explicitly.\n",
                       dir.relPath, dir.fontFiles[i]);
                return TEST_FAIL;
            }
        }

        // ---- (2) The OFL text travels with the fonts ----------------------
        const std::filesystem::path oflPath = path / "OFL.txt";
        bool oflRead = false;
        const std::string ofl = FontLicRead(oflPath, oflRead);
        if (!oflRead) {
            printf("[TEST] FAIL: %s/OFL.txt is missing. SIL OFL 1.1 §2 conditions redistribution of the font files "
                   "on this notice and license travelling with them.\n",
                   dir.relPath);
            return TEST_FAIL;
        }
        if (!FontLicHas(ofl, "SIL OPEN FONT LICENSE Version 1.1") || !FontLicHas(ofl, "PERMISSION & CONDITIONS")) {
            printf("[TEST] FAIL: %s/OFL.txt does not contain the OFL 1.1 body (its version heading and "
                   "\"PERMISSION & CONDITIONS\" clause). A paraphrase does not discharge the condition.\n",
                   dir.relPath);
            return TEST_FAIL;
        }
        // EVERY shipped font's notice, not just this directory's. The two copies
        // are byte-identical (asserted below), so each covers the whole set.
        for (const FontLicShippedFont& font : kShippedFonts) {
            if (!FontLicHas(ofl, font.copyrightFragment)) {
                printf("[TEST] FAIL: %s/OFL.txt does not carry %s's copyright notice (looking for \"%s\"). The OFL "
                       "requires the notice of each font it covers, and a Reserved Font Name is part of it.\n",
                       dir.relPath, font.fileName, font.copyrightFragment);
                return TEST_FAIL;
            }
        }
        oflTexts[dirIndex++] = ofl;
        printf("[TEST]   %s: %zu font file(s), no Fipps*, OFL.txt names all %zu shipped fonts\n", dir.relPath,
               dir.fontFileCount, sizeof(kShippedFonts) / sizeof(kShippedFonts[0]));
    }

    // ---- (2b) The two OFL.txt copies must be BYTE-IDENTICAL ----------------
    //
    // Not tidiness — an archive-layer requirement, and the reason this file
    // carries one notice for both trees instead of a per-directory one. Both
    // custom-asset trees are packed into curated archives (soh.o2r, 2ship.o2r)
    // that are mounted into ONE flat libultraship ArchiveManager in a
    // single-executable build, where resolution is last-added-wins with no
    // priority field. A path carried by both archives with different bytes
    // therefore resolves differently depending on which game booted first.
    // That is issue #595, and test_curated_archive_order.c fails on it — it is
    // what caught the first draft of these notices, which named only each
    // directory's own fonts. This assertion states the cause at the source
    // file, so the next person to "fix" the over-inclusive list reads why.
    {
        FONTLIC_CHECK(dirIndex == sizeof(kFontDirs) / sizeof(kFontDirs[0]),
                      "not every fonts directory yielded an OFL.txt, so the byte-identity check below would compare "
                      "an empty string against itself");
        if (oflTexts[0] != oflTexts[1]) {
            printf("[TEST] FAIL: the two OFL.txt copies differ (%zu vs %zu bytes). They are packed to the SAME "
                   "archive path (fonts/OFL.txt) in two archives that share one ArchiveManager, so differing bytes "
                   "make the shipped notice depend on which game booted first (#595). Keep one notice covering every "
                   "shipped font in both trees; record the per-tree difference in its \"carried in\" column.\n",
                   oflTexts[0].size(), oflTexts[1].size());
            return TEST_FAIL;
        }
        printf("[TEST]   both OFL.txt copies byte-identical (%zu bytes), so fonts/OFL.txt is mount-order independent\n",
               oflTexts[0].size());
    }

    // ---- (3) MM's CC0 grant is visible in this repository ------------------
    {
        bool read = false;
        const std::string mmLicense = FontLicRead(repoRoot / "games/mm/LICENSE", read);
        FONTLIC_CHECK(read, "games/mm/LICENSE is missing — 2Ship2Harkinian's CC0-1.0 grant over the vendored MM tree "
                            "must be visible in this repository, not only upstream");
        FONTLIC_CHECK(FontLicHas(mmLicense, "CC0 1.0 Universal"),
                      "games/mm/LICENSE is not the CC0 1.0 Universal text");
        FONTLIC_CHECK(FontLicHas(mmLicense, "Creative Commons Legal Code"),
                      "games/mm/LICENSE is missing CC0's \"Creative Commons Legal Code\" heading — a summary or a "
                      "link does not carry the dedication");
        printf("[TEST]   games/mm/LICENSE: CC0 1.0 Universal, %zu bytes\n", mmLicense.size());
    }

    // ---- (4) No source file references the removed font --------------------
    // ...and each of the two overlay-font call sites resolves the persisted
    // name before handing it to SetCurrentFont. The MM site is locked
    // textually because its TU is in no target's source list.
    {
        static const char* const kRoots[] = { "games", "src", "rsbs" };
        std::vector<FontLicFile> tree;
        for (const char* relRoot : kRoots) {
            bool ok = false;
            std::vector<FontLicFile> part = FontLicSlurp(repoRoot, relRoot, ok);
            if (!ok) {
                printf("[TEST] FAIL: source root %s/%s not found; this row cannot run from a relocated build\n",
                       RSBS_SOURCE_DIR, relRoot);
                return TEST_FAIL;
            }
            for (FontLicFile& f : part) {
                tree.push_back(std::move(f));
            }
        }
        FONTLIC_CHECK(!tree.empty(), "scanned games/, src/ and rsbs/ and found no source files at all — the scan "
                                     "would vacuously pass, which is worse than not running it");

        int resolvedCallSites = 0;
        int bareCallSites = 0;
        int selfExclusions = 0;
        for (const FontLicFile& f : tree) {
            if (f.relPath == kFontLicSelfPath) {
                selfExclusions++;
                continue;
            }
            if (FontLicHas(f.text, "Fipps")) {
                printf("[TEST] FAIL: %s names the removed font. The check is on the bare name and covers COMMENTS "
                       "too, deliberately: the font file is gone from both asset trees, so a load can only fail, and "
                       "a comment naming it is how the name creeps back toward a load. THIRD_PARTY_NOTICES.md "
                       "(\"Resolved by removal\") and docs/known-issues.md are where it is named; a source file "
                       "should point there instead.\n",
                       f.relPath.c_str());
                return TEST_FAIL;
            }
            if (FontLicHas(f.text, "SetCurrentFont(ResolveOverlayFontName(") ||
                FontLicHas(f.text, "SetCurrentFont(SOH::ResolveOverlayFontName(") ||
                FontLicHas(f.text, "SetCurrentFont(Ben::ResolveOverlayFontName(")) {
                resolvedCallSites++;
            }
            if (FontLicHas(f.text, "SetCurrentFont(CVarGetString(")) {
                printf("[TEST] FAIL: %s hands a persisted CVar straight to SetCurrentFont. An unloaded name inserts "
                       "a null mFonts entry (operator[]) that DrawSettings() then offers as a dead font; resolve it "
                       "against the loaded names first.\n",
                       f.relPath.c_str());
                bareCallSites++;
            }
        }
        FONTLIC_CHECK(bareCallSites == 0, "an overlay-font call site bypasses the resolver (named above)");
        if (selfExclusions != 1) {
            printf("[TEST] FAIL: expected to skip exactly 1 file as this test's own source (%s); skipped %d. This "
                   "file contains every string the scan looks for, so the exclusion must hit it exactly once — a "
                   "miss fails the row against itself, and a rename must not silently widen the skip.\n",
                   kFontLicSelfPath, selfExclusions);
            return TEST_FAIL;
        }
        if (resolvedCallSites != 2) {
            printf("[TEST] FAIL: expected exactly 2 resolved SetCurrentFont call sites (OoT's "
                   "games/oot/soh/OTRGlobals.cpp and MM's games/mm/2s2h/BenPort.cpp); found %d. A third site is an "
                   "overlay-font path this row does not model.\n",
                   resolvedCallSites);
            return TEST_FAIL;
        }
        printf("[TEST]   scanned %zu source files (1 skipped as this row's own source): 0 name the removed font, 2 "
               "resolved SetCurrentFont call sites, 0 bare\n",
               tree.size());
    }

    printf("[TEST] PASS: no ungranted font in the tree, OFL 1.1 beside both font sets, CC0 over games/mm, and a "
           "stale gOverlayFont resolves to a loaded font\n");
    return TEST_PASS;
#endif // RSBS_SOURCE_DIR
}

#undef FONTLIC_CHECK
