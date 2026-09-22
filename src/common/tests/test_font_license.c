/**
 * The font-licensing invariant — the license follow-up to #578.
 *
 * Three things were true at once before this row existed, and each of them is a
 * shipping hazard that no build step noticed:
 *
 *   1. An ungranted font sat in BOTH custom-asset trees
 *      (`games/{oot,mm}/assets/custom/fonts/`) and was packed into `soh.o2r`
 *      and `2ship.o2r` by this repository's own CMake. Its `name` table asserts
 *      `All rights reserved` with no license description (name ID 13) and no
 *      license URL (name ID 14) — i.e. no grant of any kind.
 *      THIRD_PARTY_NOTICES.md ("Resolved by removal") names the file; this
 *      source deliberately does too, but ONLY as the string constants the
 *      resolver cases below need, and this file is the one file the source scan
 *      skips for exactly that reason.
 *   2. The fonts that DO carry a grant are SIL Open Font License 1.1, and the
 *      OFL requires its text to travel with the font files. It was not in the
 *      tree.
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
 * WHAT THE ASSET HALF ASSERTS, AND WHY IT IS DERIVED RATHER THAN LISTED. Review
 * of the first draft showed the lock was specific to one filename prefix: it
 * rejected `Fipps*` and nothing else, so dropping the identical bytes back in
 * under any other name passed green, and a fifth font added to either tree was
 * never required to appear in `OFL.txt` at all. So the expectation is now taken
 * FROM THE DIRECTORY: every regular file in each fonts directory must be either
 * `OFL.txt` or a font this row knows about, every known font must be present,
 * and — the part that makes the notice mean something — each font file's own
 * `name` table is PARSED HERE and its name-ID-0 copyright string must appear
 * verbatim in `OFL.txt`. Swapping in a differently dated build of the same
 * family therefore fails until the notice is updated, which is the actual OFL
 * 1.1 §2 obligation ("each copy contains the above copyright notice").
 *
 * WHY THE RESOLVER IS PART OF THIS ROW. Removing a font that a player may have
 * SELECTED is not a pure deletion. `CVAR_GAME_OVERLAY_FONT` —
 * `gSettings.OverlayFont` in this build, per `CMake/lus-cvars.cmake:16` over
 * `CMake/soh-cvars.cmake`'s `gSettings` prefix; `gOverlayFont` is the
 * pre-migration spelling `soh/config/ConfigMigrators.h` renames away — persists
 * the chosen name, and `Ship::GameOverlay::SetCurrentFont` looks it up with
 * `mFonts[name]` — `std::unordered_map::operator[]`, which INSERTS a null-valued
 * entry before it logs the failure and returns
 * (`libultraship/src/ship/window/gui/GameOverlay.cpp`). The inserted row then
 * shows up in `DrawSettings()`'s combo as a selectable font that can never
 * become current. So the game side resolves the persisted name against the names
 * it actually loaded BEFORE calling in. This row exercises that resolver
 * directly, in both directions: an unknown name must come back as the fallback,
 * and a known name must pass through unchanged (a resolver that answered
 * "Press Start 2P" to everything would satisfy only half of it).
 *
 * WHAT THIS IS NOT. It is a source/asset invariant plus one linked pure
 * function. It does not open a window, load an archive or touch ImGui, and it
 * cannot prove what a built `.o2r` contains — the packing path is asserted
 * indirectly, by the absence of the file the packer would have picked up
 * (`OTRExporter/OTRExporter/Main.cpp` reads every file under the custom-assets
 * path that is not a format-suffixed PNG and adds it verbatim, so the tree IS
 * the archive's font manifest).
 *
 * THE SUBMODULE IS NOW IN SCOPE (2026-09-21). A third byte-identical copy of the
 * removed font used to sit at `OTRExporter/assets/fonts/Fipps-Regular.otf` in the
 * pinned fork, and this row used to disclaim it as another repository's problem.
 * It was removed fork-side and the pin here moved, so the disclaimer is gone and
 * `OTRExporter/assets/fonts` is scanned instead — otherwise the next pin bump
 * could bring the file back with nothing red. That directory carries no `OFL.txt`
 * and is packed into no archive, so the inventory-plus-notice rule the two
 * custom-asset trees get would be the wrong rule for it. What is asserted there
 * is the GRANT, read out of each font's own `name` table: a font file with
 * neither a license description (name ID 13) nor a license URL (name ID 14)
 * fails. That is precisely what separates the removed font (neither record
 * present; its ID 0 reads "All rights reserved") from the one that remains
 * (`PressStart2P-Regular.ttf`, whose ID 13 names OFL 1.1), and unlike a filename
 * check it does not care what the file is called. This row's PASS line says what
 * it scanned rather than claiming the whole tree.
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
 * CALLED from here — it is not in the link, and the static-analysis job's
 * `clang-tidy -p build` has no compile command for it either. The one property
 * this row asserts over OoT's copy at runtime ("the fallback is itself a loaded
 * name") is therefore arranged BY CONSTRUCTION in both TUs — the fallback is
 * defined as `kOverlayFontNames[0]`, not as a second literal — and it is that
 * construction, not the resolver's behaviour, that the textual half checks in
 * MM's file. A structural guarantee is the only kind that holds over a TU
 * nothing compiles.
 *
 * ROM-free, display-free, no Ship::Context. Included at FILE SCOPE by
 * test_runner.cpp (compiled as C++), like the other files in this directory.
 */

#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdint>
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

/**
 * Source and build extensions worth scanning for a stale font reference.
 *
 * `.cmake` and `CMakeLists.txt` are in the list because review found the
 * removed font's name in `CMake/SingleExecutable.cmake` — in the very block that
 * registers this row — while the scan covered only C and C++ extensions under
 * `games`, `src` and `rsbs`. A build file that names it is the same creep toward
 * a load as a comment in a header.
 */
bool FontLicIsSourceFile(const std::filesystem::path& p) {
    const std::string ext = p.extension().string();
    if (ext == ".c" || ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".cmake") {
        return true;
    }
    return p.filename().string() == "CMakeLists.txt";
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
 * Every source or build file under @p relRoot, with its repo-relative path. The
 * roots are narrow (`games`, `src`, `rsbs`, `CMake`, `OTRExporter`) on purpose: a scan from the
 * repo root would walk `.claude/worktrees/`, where other lanes' full checkouts
 * live, and count their references as this tree's. Mirrors test_setmenu_count.c's
 * slurp.
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

bool FontLicIsFontFile(const std::string& name) {
    const std::size_t dot = name.rfind('.');
    if (dot == std::string::npos) {
        return false;
    }
    std::string ext = name.substr(dot);
    for (char& c : ext) {
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }
    return ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".woff" || ext == ".woff2";
}

// ---------------------------------------------------------------------------
// A minimal sfnt `name`-table reader.
//
// The point of parsing rather than hardcoding: the notice this project ships has
// to be the notice the FILE carries, and only the file can say what that is. A
// literal list re-asserts what its author believed on the day they wrote it, so
// it goes stale silently on the next font update — which is the exact failure
// the OFL's condition 2 turns into a redistribution problem.
//
// Deliberately small: enough of the table directory to find `name`, enough of
// `name` to pull one string, bounds-checked at every step, and ASCII-only
// decoding (every notice in this tree is ASCII; a non-ASCII byte becomes '?' and
// the OFL.txt comparison then fails loudly rather than passing on a mangled
// string). TTF and OTF share this layout, so one reader covers both.
// ---------------------------------------------------------------------------

std::uint16_t FontLicBE16(const std::string& d, std::size_t off) {
    return static_cast<std::uint16_t>((static_cast<unsigned char>(d[off]) << 8) |
                                      static_cast<unsigned char>(d[off + 1]));
}

std::uint32_t FontLicBE32(const std::string& d, std::size_t off) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(d[off])) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(d[off + 1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(d[off + 2])) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(d[off + 3]));
}

/**
 * The @p wantedNameId string from @p font's `name` table, ASCII-decoded.
 *
 * Prefers a Windows (platform 3) record, which is UTF-16BE, and falls back to a
 * Macintosh (platform 1) record, which is single-byte. Returns false with a
 * reason in @p why when the file is not an sfnt this reader understands — a
 * malformed font must fail the row with a message, not crash it.
 */
bool FontLicNameString(const std::string& font, std::uint16_t wantedNameId, std::string& out, std::string& why) {
    out.clear();
    if (font.size() < 12) {
        why = "shorter than an sfnt header";
        return false;
    }
    const std::uint16_t numTables = FontLicBE16(font, 4);
    std::size_t nameOff = 0;
    std::size_t nameLen = 0;
    for (std::uint16_t i = 0; i < numTables; i++) {
        const std::size_t rec = 12 + static_cast<std::size_t>(i) * 16;
        if (rec + 16 > font.size()) {
            why = "table directory runs past end of file";
            return false;
        }
        if (font.compare(rec, 4, "name") == 0) {
            nameOff = FontLicBE32(font, rec + 8);
            nameLen = FontLicBE32(font, rec + 12);
            break;
        }
    }
    if (nameLen == 0 || nameOff + nameLen > font.size() || nameLen < 6) {
        why = "no usable `name` table";
        return false;
    }
    const std::uint16_t count = FontLicBE16(font, nameOff + 2);
    const std::size_t stringOff = nameOff + FontLicBE16(font, nameOff + 4);

    int bestScore = -1;
    for (std::uint16_t i = 0; i < count; i++) {
        const std::size_t rec = nameOff + 6 + static_cast<std::size_t>(i) * 12;
        if (rec + 12 > nameOff + nameLen) {
            break;
        }
        const std::uint16_t platformId = FontLicBE16(font, rec + 0);
        const std::uint16_t nameId = FontLicBE16(font, rec + 6);
        const std::uint16_t length = FontLicBE16(font, rec + 8);
        const std::size_t offset = FontLicBE16(font, rec + 10);
        if (nameId != wantedNameId) {
            continue;
        }
        if (stringOff + offset + length > font.size()) {
            continue;
        }
        const int score = (platformId == 3) ? 2 : ((platformId == 1 || platformId == 0) ? 1 : 0);
        if (score <= bestScore) {
            continue;
        }
        std::string decoded;
        if (platformId == 3 || platformId == 0) {
            for (std::size_t b = 0; b + 1 < length; b += 2) {
                const unsigned char hi = static_cast<unsigned char>(font[stringOff + offset + b]);
                const unsigned char lo = static_cast<unsigned char>(font[stringOff + offset + b + 1]);
                decoded.push_back((hi == 0 && lo < 0x80) ? static_cast<char>(lo) : '?');
            }
        } else {
            for (std::size_t b = 0; b < length; b++) {
                const unsigned char ch = static_cast<unsigned char>(font[stringOff + offset + b]);
                decoded.push_back(ch < 0x80 ? static_cast<char>(ch) : '?');
            }
        }
        out = decoded;
        bestScore = score;
    }
    if (bestScore < 0) {
        why = "no record for that name ID";
        return false;
    }
    return true;
}

/// A font this project ships, and which of the two trees carry it.
struct FontLicShippedFont {
    const char* fileName;
    bool inOot;
    bool inMm;
};

/**
 * Every font FILE this project ships, from either tree. MM has no Japanese face.
 *
 * ONE list for BOTH directories, because both `OFL.txt` copies must name the
 * whole set — see the byte-identity assertion below for why they cannot each
 * name only their own directory. No copyright fragments here on purpose: those
 * come from each file's own `name` table at run time.
 */
const FontLicShippedFont kShippedFonts[] = {
    { "Inconsolata-Regular.ttf", true, true },
    { "Montserrat-Regular.ttf", true, true },
    { "NotoSansJP-Regular.ttf", true, false },
    { "PressStart2P-Regular.ttf", true, true },
};

/// Files that may sit in a fonts directory without being a font.
const char* const kAllowedNonFontFiles[] = { "OFL.txt" };

/**
 * Notices `OFL.txt` must carry that no font file in these directories can supply.
 *
 * Font Awesome 4 is the case that matters: it ships base85-compressed inside
 * `libultraship/include/ship/window/gui/Fonts.h` and is pushed into the ImGui
 * atlas unconditionally, so it is in the binary without ever being a file in the
 * tree — which is why the Fonts table in THIRD_PARTY_NOTICES.md missed it until
 * review. Its OFL grant comes from FortAwesome/Font-Awesome's README at ref
 * `4.x`; the font's own `name` table has no license description. Noto Sans JP's
 * trademark line is here because it is name ID 7, not the copyright line the
 * parser above pulls.
 */
struct FontLicExtraNotice {
    const char* what;
    const char* fragment;
};
const FontLicExtraNotice kExtraOflNotices[] = {
    { "Noto Sans JP's name ID 7 trademark line",
      "Source is a trademark of Adobe in the United States and/or other countries." },
    { "the embedded Font Awesome 4 face's file name", "fontawesome-webfont.ttf" },
    { "the embedded Font Awesome 4 face's copyright line", "Copyright Dave Gandy 2016. All rights reserved." },
};

/// Where the embedded icon face lives, and the declaration that proves it is
/// still embedded. If this ever moves or goes away, the notice above stops
/// describing the build and must be revisited rather than left over-claiming.
const char* const kEmbeddedFontHeader = "libultraship/include/ship/window/gui/Fonts.h";
const char* const kEmbeddedFontSymbol = "fontawesome_compressed_data_base85";

/**
 * The exporter submodule's own font directory, and the rule that applies to it.
 *
 * Nothing packs it and it has no OFL.txt, so it gets the GRANT rule rather than
 * the inventory rule: every font file in it must carry a license description
 * (`name` ID 13) or a license URL (`name` ID 14). Non-font files are ignored
 * here — this directory is not an archive manifest, so a stray README is not a
 * redistribution the way a stray file in a custom-asset tree is.
 */
const char* const kExporterFontsDir = "OTRExporter/assets/fonts";
const std::uint16_t kNameIdLicenseDescription = 13;
const std::uint16_t kNameIdLicenseUrl = 14;

/// The construction that makes "the fallback is one of the loaded names" true
/// without a test having to run — the one guarantee available over MM's
/// uncompiled TU. Expected in BOTH overlay-font TUs.
const char* const kFallbackByConstruction = "kOverlayFontFallback = kOverlayFontNames[0]";

} // namespace
#endif // RSBS_SOURCE_DIR

TestResult Test_FontLicense(void) {
    printf("[TEST] font-license: no unlicensed font ships, the OFL and CC0 texts are in the tree, and a stale "
           "overlay-font selection resolves (license follow-up to #578)\n");

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
        // SetCurrentFont would insert a null mFonts row for it. In the TU this is
        // arranged by construction (the fallback IS kOverlayFontNames[0]); the
        // textual half below asserts that construction in both TUs, including
        // MM's, which nothing links.
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

    // ---- (1) Each fonts directory holds EXACTLY the inventoried set ---------
    // Derived from the directory listing, not from a prefix match: the earlier
    // version rejected only names beginning "Fipps", which a rename walked
    // straight past, and never required a newly added font to be notified at
    // all. Both directions are now closed — nothing unexpected may be present,
    // and nothing expected may be missing (the negative half is trivially true
    // of a directory that got wiped, which would be a worse bug shipped green).
    struct FontDir {
        const char* relPath;
        bool FontLicShippedFont::*membership;
    };
    const FontDir kFontDirs[] = {
        { "games/oot/assets/custom/fonts", &FontLicShippedFont::inOot },
        { "games/mm/assets/custom/fonts", &FontLicShippedFont::inMm },
    };
    std::string oflTexts[sizeof(kFontDirs) / sizeof(kFontDirs[0])];
    std::size_t dirIndex = 0;
    int parsedNotices = 0;

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

        // Nothing in the directory may be unaccounted for. The custom-asset
        // packer adds every file here to the shipped archive verbatim, so an
        // un-inventoried file is an un-notified redistribution, whatever it is.
        for (const std::string& name : names) {
            bool known = false;
            for (const FontLicShippedFont& font : kShippedFonts) {
                if (name == font.fileName && font.*(dir.membership)) {
                    known = true;
                    break;
                }
            }
            for (const char* allowed : kAllowedNonFontFiles) {
                if (name == allowed) {
                    known = true;
                    break;
                }
            }
            if (known) {
                continue;
            }
            if (name.rfind("Fipps", 0) == 0) {
                printf("[TEST] FAIL: %s/%s is back. That font asserts \"All rights reserved\" with no license "
                       "description (name ID 13) and no license URL (name ID 14); the custom-asset packer adds "
                       "every file under this directory to the shipped archive verbatim, so re-adding it puts an "
                       "ungranted work into every build.\n",
                       dir.relPath, name.c_str());
                return TEST_FAIL;
            }
            printf("[TEST] FAIL: %s/%s is not inventoried. %s Add it to kShippedFonts here, to the Fonts table in "
                   "THIRD_PARTY_NOTICES.md, and — with its own name-table copyright line — to BOTH OFL.txt copies; "
                   "or delete it. Renaming an ungranted font is the obvious way past a filename check, so this row "
                   "does not use one.\n",
                   dir.relPath, name.c_str(),
                   FontLicIsFontFile(name) ? "It is a font file, and every font this project ships needs a notice "
                                             "travelling with it."
                                           : "Every file in this directory is packed into the shipped archive "
                                             "verbatim, so it needs a licensing decision even if it is not a font.");
            return TEST_FAIL;
        }

        // The positive half: every font that is supposed to be here, is.
        for (const FontLicShippedFont& font : kShippedFonts) {
            if (!(font.*(dir.membership))) {
                continue;
            }
            bool found = false;
            for (const std::string& name : names) {
                if (name == font.fileName) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                printf("[TEST] FAIL: %s/%s is missing. The check above passes vacuously over an empty or gutted font "
                       "directory, so the expected set is asserted explicitly.\n",
                       dir.relPath, font.fileName);
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

        // ---- (2a) The notice must be THE FILE'S OWN notice -----------------
        // Every shipped font's name-ID-0 string, parsed out of the bytes that
        // ship, must appear in OFL.txt verbatim. EVERY shipped font, not just
        // this directory's: the two copies are byte-identical (asserted below),
        // so each covers the whole set. This is the assertion that makes the
        // notice track the file instead of tracking its author's memory.
        for (const FontLicShippedFont& font : kShippedFonts) {
            // Read the file from whichever tree carries it.
            std::filesystem::path fontPath = repoRoot / "games/oot/assets/custom/fonts" / font.fileName;
            if (!font.inOot) {
                fontPath = repoRoot / "games/mm/assets/custom/fonts" / font.fileName;
            }
            bool fontRead = false;
            const std::string bytes = FontLicRead(fontPath, fontRead);
            if (!fontRead) {
                printf("[TEST] FAIL: could not read %s to parse its `name` table\n", font.fileName);
                return TEST_FAIL;
            }
            std::string notice;
            std::string why;
            if (!FontLicNameString(bytes, 0, notice, why)) {
                printf("[TEST] FAIL: %s has no readable copyright (`name` ID 0): %s. A font whose own notice cannot "
                       "be read cannot be shown to be notified in OFL.txt.\n",
                       font.fileName, why.c_str());
                return TEST_FAIL;
            }
            if (notice.empty()) {
                printf("[TEST] FAIL: %s declares an EMPTY copyright string at `name` ID 0\n", font.fileName);
                return TEST_FAIL;
            }
            if (!FontLicHas(ofl, notice.c_str())) {
                printf("[TEST] FAIL: %s/OFL.txt does not carry %s's own copyright notice. Its `name` table (ID 0) "
                       "says, verbatim:\n"
                       "[TEST]         %s\n"
                       "[TEST]       OFL 1.1 §2 conditions redistribution on THAT notice travelling with the file, so "
                       "a differently worded or differently dated line in the notice does not discharge it. Copy the "
                       "string above into both OFL.txt copies (they must stay byte-identical) and into "
                       "THIRD_PARTY_NOTICES.md.\n",
                       dir.relPath, font.fileName, notice.c_str());
                return TEST_FAIL;
            }
            parsedNotices++;
        }

        // ---- (2b) Notices no font file in these directories can supply -----
        for (const FontLicExtraNotice& extra : kExtraOflNotices) {
            if (!FontLicHas(ofl, extra.fragment)) {
                printf("[TEST] FAIL: %s/OFL.txt is missing %s (looking for \"%s\"). It cannot come from a `name` "
                       "table this row parses — either it is a different name ID, or the font is embedded in the "
                       "binary rather than present as a file — so it is asserted by literal.\n",
                       dir.relPath, extra.what, extra.fragment);
                return TEST_FAIL;
            }
        }

        oflTexts[dirIndex++] = ofl;
        printf("[TEST]   %s: %zu file(s), all inventoried, OFL.txt carries every shipped font's parsed name-ID-0 "
               "notice plus %zu literal notice(s)\n",
               dir.relPath, names.size(), sizeof(kExtraOflNotices) / sizeof(kExtraOflNotices[0]));
    }

    // ---- (2c) The embedded icon face is still embedded ---------------------
    // OFL.txt carries Font Awesome 4's notice because the face is compiled into
    // the binary as a base85 array. If that array ever leaves, the notice stops
    // describing the build; asserting the declaration keeps the two in step in
    // the direction that matters (a notice for something no longer shipped is
    // harmless; a shipped font with no notice is the bug).
    {
        bool read = false;
        const std::string fontsHeader = FontLicRead(repoRoot / kEmbeddedFontHeader, read);
        FONTLIC_CHECK(read, "libultraship/include/ship/window/gui/Fonts.h is unreadable — initialise submodules; the "
                            "embedded icon face this row notifies lives there");
        if (!FontLicHas(fontsHeader, kEmbeddedFontSymbol)) {
            printf("[TEST] FAIL: %s no longer declares %s. OFL.txt carries Font Awesome 4's copyright notice on the "
                   "strength of that array being compiled into the binary; if the embedded face moved or went away, "
                   "revisit the Fonts table in THIRD_PARTY_NOTICES.md and both OFL.txt copies rather than leaving a "
                   "notice that describes something this build no longer ships.\n",
                   kEmbeddedFontHeader, kEmbeddedFontSymbol);
            return TEST_FAIL;
        }
    }

    // ---- (2d) The two OFL.txt copies must be BYTE-IDENTICAL ----------------
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
        printf("[TEST]   both OFL.txt copies byte-identical (%zu bytes), so fonts/OFL.txt is mount-order independent; "
               "%d name-table notice(s) parsed and matched\n",
               oflTexts[0].size(), parsedNotices);
    }

    // ---- (2e) The exporter submodule's fonts must carry a GRANT -------------
    //
    // This directory used to be the one place this row looked away from, on the
    // grounds that the copy of the removed font living there was another
    // repository's problem. It was removed fork-side and the pin here moved, so
    // the exemption is gone. The rule is deliberately NOT the one the two
    // custom-asset trees get: nothing packs this directory into an archive and
    // there is no OFL.txt beside it, so demanding an inventory and a matching
    // notice would fail on the fork's own contents rather than on a problem.
    //
    // What it demands instead is the thing the removed font could never satisfy
    // and the surviving font satisfies trivially: a license grant in the font's
    // own `name` table. Fipps-Regular.otf has no ID 13 and no ID 14 at all (its
    // ID 0 is "Copyright (c) 2007 by Stefanie Koerner (pheist). All rights
    // reserved."); PressStart2P-Regular.ttf's ID 13 is the OFL 1.1 sentence. So
    // moving the pin back to the old commit fails this block by name, and a
    // renamed or newly added ungranted font fails it just the same — which a
    // filename check would not.
    {
        const std::filesystem::path exporterFonts = repoRoot / kExporterFontsDir;
        bool listed = false;
        const std::vector<std::string> names = FontLicListDir(exporterFonts, listed);
        if (!listed) {
            printf("[TEST] FAIL: %s is not a directory. The OTRExporter submodule must be initialised for this row to "
                   "assert anything about it (`git submodule update --init`); a silent skip is how the ungranted font "
                   "that used to live here came back unnoticed.\n",
                   kExporterFontsDir);
            return TEST_FAIL;
        }

        int grantedFonts = 0;
        for (const std::string& name : names) {
            if (!FontLicIsFontFile(name)) {
                continue;
            }
            bool fontRead = false;
            const std::string bytes = FontLicRead(exporterFonts / name, fontRead);
            if (!fontRead) {
                printf("[TEST] FAIL: could not read %s/%s to parse its `name` table\n", kExporterFontsDir,
                       name.c_str());
                return TEST_FAIL;
            }
            std::string description;
            std::string url;
            std::string why;
            const bool hasDescription =
                FontLicNameString(bytes, kNameIdLicenseDescription, description, why) && !description.empty();
            const bool hasUrl = FontLicNameString(bytes, kNameIdLicenseUrl, url, why) && !url.empty();
            if (!hasDescription && !hasUrl) {
                std::string copyright;
                std::string ignored;
                if (!FontLicNameString(bytes, 0, copyright, ignored)) {
                    copyright = "(no readable `name` ID 0 either)";
                }
                printf("[TEST] FAIL: %s/%s grants nothing. Its `name` table has no license description (ID 13) and no "
                       "license URL (ID 14); its copyright line reads:\n"
                       "[TEST]         %s\n"
                       "[TEST]       A font with no grant does not belong in a source tree this project redistributes, "
                       "whatever it is called and whether or not anything loads it. Either the OTRExporter pin moved "
                       "back to a commit that still carries the removed font, or a new ungranted font was added there. "
                       "Remove it fork-side and move the pin; see THIRD_PARTY_NOTICES.md, \"Resolved by removal\".\n",
                       kExporterFontsDir, name.c_str(), copyright.c_str());
                return TEST_FAIL;
            }
            grantedFonts++;
        }
        FONTLIC_CHECK(grantedFonts >= 1,
                      "no font file at all in OTRExporter/assets/fonts — the grant check above passes vacuously over "
                      "an empty directory, and this row should be revisited rather than left asserting nothing there");
        printf("[TEST]   %s: %zu file(s), %d font(s), every one carrying a `name`-table license grant (ID 13 or 14)\n",
               kExporterFontsDir, names.size(), grantedFonts);
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

    // ---- (4) No source or build file references the removed font -----------
    // ...and each of the two overlay-font call sites resolves the persisted
    // name before handing it to SetCurrentFont, with a fallback that is a loaded
    // name BY CONSTRUCTION. The MM site is locked textually because its TU is in
    // no target's source list, so nothing here can call it.
    {
        // `OTRExporter` joined this list on 2026-09-21, when the font it used to
        // carry was removed fork-side and the pin here moved. It is small (a few
        // dozen source and CMake files; its `assets/` tree holds none) and it is
        // the tool that builds the shipped archives, so a reference to a removed
        // font reappearing there is the same creep toward a load as one in this
        // repository's own sources. It does NOT widen the resolver counts below:
        // the exporter contains no overlay-font call site.
        static const char* const kRoots[] = { "games", "src", "rsbs", "CMake", "OTRExporter" };
        std::vector<FontLicFile> tree;
        for (const char* relRoot : kRoots) {
            bool ok = false;
            std::vector<FontLicFile> part = FontLicSlurp(repoRoot, relRoot, ok);
            if (!ok) {
                printf("[TEST] FAIL: source root %s/%s not found; this row cannot run from a relocated build, and an "
                       "uninitialised submodule root must fail rather than shrink the scan\n",
                       RSBS_SOURCE_DIR, relRoot);
                return TEST_FAIL;
            }
            for (FontLicFile& f : part) {
                tree.push_back(std::move(f));
            }
        }
        FONTLIC_CHECK(!tree.empty(), "scanned games/, src/, rsbs/, CMake/ and OTRExporter/ and found no source files "
                                     "at all — the scan would vacuously pass, which is worse than not running it");

        int resolvedCallSites = 0;
        int bareCallSites = 0;
        int selfExclusions = 0;
        int constructedFallbacks = 0;
        for (const FontLicFile& f : tree) {
            if (f.relPath == kFontLicSelfPath) {
                selfExclusions++;
                continue;
            }
            if (FontLicHas(f.text, "Fipps")) {
                printf("[TEST] FAIL: %s names the removed font. The check is on the bare name and covers COMMENTS "
                       "and build files too, deliberately: the font file is gone from both asset trees, so a load "
                       "can only fail, and a comment naming it is how the name creeps back toward a load. "
                       "THIRD_PARTY_NOTICES.md (\"Resolved by removal\") and docs/known-issues.md are where it is "
                       "named; a source or build file should point there instead.\n",
                       f.relPath.c_str());
                return TEST_FAIL;
            }
            if (FontLicHas(f.text, "SetCurrentFont(ResolveOverlayFontName(") ||
                FontLicHas(f.text, "SetCurrentFont(SOH::ResolveOverlayFontName(") ||
                FontLicHas(f.text, "SetCurrentFont(Ben::ResolveOverlayFontName(")) {
                resolvedCallSites++;
            }
            if (FontLicHas(f.text, kFallbackByConstruction)) {
                constructedFallbacks++;
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
        if (constructedFallbacks != 2) {
            printf("[TEST] FAIL: expected both overlay-font TUs to define their fallback AS a loaded name "
                   "(\"%s\"); found %d. This is the only guarantee available over MM's games/mm/2s2h/BenPort.cpp, "
                   "which NO configuration in this repository compiles: SINGLE_EXECUTABLE_BUILD defaults ON, MM's "
                   "CMakeLists filters the TU out, no workflow turns it off, and clang-tidy has no compile command "
                   "for it. A second literal there could name a font that is not loaded — the null mFonts row the "
                   "resolver exists to prevent — and nothing would notice.\n",
                   kFallbackByConstruction, constructedFallbacks);
            return TEST_FAIL;
        }
        printf("[TEST]   scanned %zu source/build files under games/ src/ rsbs/ CMake/ OTRExporter/ (1 skipped as this "
               "row's own source): 0 name the removed font, 2 resolved SetCurrentFont call sites, 0 bare, 2 fallbacks "
               "loaded by construction\n",
               tree.size());
    }

    printf("[TEST] PASS: both custom-asset font directories hold exactly their inventoried set, each shipped font's "
           "own name-table notice is in the byte-identical OFL.txt beside them, every font in the pinned "
           "OTRExporter/assets/fonts carries a name-table license grant, CC0 covers games/mm, and no source or build "
           "file under games/ src/ rsbs/ CMake/ OTRExporter/ names the removed font (see THIRD_PARTY_NOTICES.md, "
           "\"Resolved by removal\")\n");
    return TEST_PASS;
#endif // RSBS_SOURCE_DIR
}

#undef FONTLIC_CHECK
