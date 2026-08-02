/**
 * CVar classification lock (issue #34, ADR 0003 §6.5).
 *
 * ADR 0003 and `docs/enhancement-classification.md` decide, per setting,
 * whether the two games share one CVar or keep their own. That decision is
 * prose, and prose decays on the first upstream sync. This test is the piece
 * that stops it decaying: it makes the classification a build-time invariant,
 * so a change that contradicts it is a red build NAMING THE KEY rather than a
 * silent behavioural merge discovered by a player months later.
 *
 * Both failure directions are locked, because both are real:
 *
 *   1. **A converged key diverges again.** Someone reintroduces MM's retired
 *      spelling, or adds a new sibling on a retired prefix. The setting
 *      silently stops crossing the game boundary — the exact defect that made
 *      OoT's master volume slider never apply to MM.
 *
 *   2. **A per-game key gets converged.** Someone sees two similar names and
 *      "fixes" the collision. This is the more dangerous direction, because it
 *      is invisible at runtime: merging OoT's 1-5x text-speed slider with MM's
 *      boolean produces a control that appears in the menu, responds to
 *      clicks, persists its value, and does nothing. The inventory's §4 rows
 *      each say why one control would be wrong; kMustStayDistinct carries them
 *      so a merge cannot land quietly.
 *
 * Plus the guard the ADR asks for explicitly: the deliberate `gCheats.*`
 * sharing must survive. 26 of the 30 measured collisions were correct and the
 * documents warn repeatedly against "fixing" them; this asserts it.
 *
 * ROM-free and cheap: a manifest self-consistency pass, a pure-function pass
 * over the migrator's decision rules, and a source scan over `games/`. No
 * archives, no window, no audio.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++), like the other
 * files in this directory.
 */

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "cvar_shared_keys.h"

// Defined in games/oot/soh/config/ConfigUpdaters.cpp. Declared here rather than
// included so redship_common does not take a header dependency on OoT's config
// layer — same approach as test_seq_map_bounds.c.
namespace SOH {
bool ShouldAdoptLegacyValue(bool legacyPresent, bool canonicalPresent);
int32_t VolumePercentFromFloatScale(float scale);
} // namespace SOH

#define CVARCLASS_CHECK(cond, msg)                     \
    do {                                               \
        if (!(cond)) {                                 \
            printf("[TEST] FAIL: %s\n", (msg));        \
            return TEST_FAIL;                          \
        }                                              \
    } while (0)

#ifdef RSBS_SOURCE_DIR
namespace {

/// Source extensions worth scanning. Matches the extraction in ADR 0003's
/// Appendix A so the lock and the measurement agree about what "the tree" is.
bool CvarClassIsSourceFile(const std::filesystem::path& p) {
    const std::string ext = p.extension().string();
    return ext == ".c" || ext == ".cpp" || ext == ".h" || ext == ".hpp";
}

/// Every source file under `root`, slurped once. The scan asks a few dozen
/// substring questions, so reading each file once and reusing the text beats
/// re-walking the tree per key.
std::vector<std::string> CvarClassSlurpTree(const std::filesystem::path& root, bool& ok) {
    std::vector<std::string> contents;
    std::error_code ec;

    if (!std::filesystem::exists(root, ec) || ec) {
        ok = false;
        return contents;
    }
    ok = true;

    for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec),
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
        if (!CvarClassIsSourceFile(it->path())) {
            continue;
        }
        std::ifstream file(it->path(), std::ios::binary);
        if (!file.is_open()) {
            continue;
        }
        contents.emplace_back((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }
    return contents;
}

/// Does any scanned file contain this quoted key literal? Substring form: the
/// needle is the OPENING quote plus the key, so a prefix (e.g. a retired family
/// namespace) matches every key under it.
bool CvarClassTreeContainsLiteral(const std::vector<std::string>& tree, const char* key) {
    const std::string needle = std::string("\"") + key;
    for (const std::string& text : tree) {
        if (text.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

/// Does any scanned file contain this key as a WHOLE quoted literal ("key")?
/// Exact form, used where a retired key is a PREFIX of a surviving one — e.g.
/// gEnhancements.Saving.Autosave retires while gEnhancements.Saving.AutosaveInterval
/// stays (MM-only). The substring form above would false-match the survivor and
/// report the retired key as still present. MM spells every CVar as a full
/// literal, so the closing quote is always there to anchor against.
bool CvarClassTreeContainsExactKey(const std::vector<std::string>& tree, const char* key) {
    const std::string needle = std::string("\"") + key + "\"";
    for (const std::string& text : tree) {
        if (text.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

/// One scanned file, with the repo-relative path kept. The slurp above throws
/// paths away because every question it asks is "does the tree contain X"; the
/// menu-index reader allowlist asks "WHICH file contains X", so it needs them.
struct CvarClassSourceFile {
    std::string relPath; // forward slashes, relative to `root`
    std::string text;
};

std::vector<CvarClassSourceFile> CvarClassSlurpTreeWithPaths(const std::filesystem::path& root, bool& ok) {
    std::vector<CvarClassSourceFile> files;
    std::error_code ec;

    if (!std::filesystem::exists(root, ec) || ec) {
        ok = false;
        return files;
    }
    ok = true;

    for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec),
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
        if (!CvarClassIsSourceFile(it->path())) {
            continue;
        }
        std::ifstream file(it->path(), std::ios::binary);
        if (!file.is_open()) {
            continue;
        }
        CvarClassSourceFile entry;
        entry.relPath = std::filesystem::relative(it->path(), root, ec).generic_string();
        if (ec) {
            ec.clear();
            entry.relPath = it->path().generic_string();
        }
        entry.text.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        files.push_back(std::move(entry));
    }
    return files;
}

} // namespace
#endif // RSBS_SOURCE_DIR

TestResult Test_CVarClassification(void) {
    printf("[TEST] cvar-classification: cross-game CVar classification matches ADR 0003 + the inventory (#34)\n");

    // ------------------------------------------------------------------
    // (1) Manifest self-consistency.
    //
    // The tables are hand-maintained. Before trusting them to police the tree,
    // check they are not internally contradictory — a manifest that disagrees
    // with itself would police nothing.
    // ------------------------------------------------------------------
    for (std::size_t i = 0; i < RSBS::kConvergedKeyCount; i++) {
        const RSBS::ConvergedKey& a = RSBS::kConvergedKeys[i];
        CVARCLASS_CHECK(a.legacy != nullptr && a.canonical != nullptr, "converged row has a null key");
        CVARCLASS_CHECK(strcmp(a.legacy, a.canonical) != 0, "converged row maps a key onto itself");

        for (std::size_t j = i + 1; j < RSBS::kConvergedKeyCount; j++) {
            const RSBS::ConvergedKey& b = RSBS::kConvergedKeys[j];
            CVARCLASS_CHECK(strcmp(a.legacy, b.legacy) != 0, "duplicate legacy key in the convergence table");
            CVARCLASS_CHECK(strcmp(a.canonical, b.canonical) != 0,
                            "two legacy keys converge onto the SAME canonical key — that silently merges two "
                            "distinct settings into one");
        }

        // A canonical target must not be a key some other row is retiring:
        // that would be a two-step rename the updater applies in one pass, and
        // the result depends on table order.
        for (std::size_t j = 0; j < RSBS::kConvergedKeyCount; j++) {
            CVARCLASS_CHECK(strcmp(a.canonical, RSBS::kConvergedKeys[j].legacy) != 0,
                            "a convergence target is itself being retired by another row (chained rename)");
        }
    }

    // The four menu-index keys (#451) and the disputed key (#454) must never be
    // swept into convergence. Both are decided by their own issues, and both
    // readings of each agree on this much.
    for (std::size_t i = 0; i < RSBS::kConvergedKeyCount; i++) {
        for (std::size_t m = 0; m < RSBS::kMenuIndexKeyCount; m++) {
            CVARCLASS_CHECK(strcmp(RSBS::kConvergedKeys[i].canonical, RSBS::kMenuIndexKeys[m]) != 0 &&
                                strcmp(RSBS::kConvergedKeys[i].legacy, RSBS::kMenuIndexKeys[m]) != 0,
                            "a menu-index key (#451) leaked into the convergence table — those are decided "
                            "separately and must never be merged by a rename pass");
        }
        for (std::size_t d = 0; d < RSBS::kDisputedClassificationKeyCount; d++) {
            CVARCLASS_CHECK(strcmp(RSBS::kConvergedKeys[i].canonical, RSBS::kDisputedClassificationKeys[d]) != 0 &&
                                strcmp(RSBS::kConvergedKeys[i].legacy, RSBS::kDisputedClassificationKeys[d]) != 0,
                            "a disputed-classification key (#454) leaked into the convergence table");
        }
    }

    // Every retired FAMILY prefix must cover at least one converged legacy key.
    // A prefix that matches nothing is stale: it would scan the tree for a family
    // that no longer converges and quietly protect nothing.
    //
    // Note the direction. This does NOT require every legacy key to sit under a
    // prefix. The two #461 prefixes (gSettings.Audio., gCosmetics.Link_) are
    // ABANDONED NAMESPACES — every MM key in them retired — so a prefix scan is
    // the right tool to catch a NEW sibling appearing there. The #462 §5.3 rows
    // instead retire an individual leaf out of a namespace that STAYS LIVE
    // (gEnhancements.Graphics. still hosts MM-only keys, gEnhancements.Saving.
    // still hosts AutosaveInterval, gCheats. still hosts the shared cheats), so
    // they have no family prefix. They are covered directly by the exact per-key
    // scan (3a) below, which names each retired spelling in full.
    for (std::size_t p = 0; p < RSBS::kRetiredKeyPrefixCount; p++) {
        const char* prefix = RSBS::kRetiredKeyPrefixes[p];
        bool covers = false;
        for (std::size_t i = 0; i < RSBS::kConvergedKeyCount; i++) {
            if (strncmp(RSBS::kConvergedKeys[i].legacy, prefix, strlen(prefix)) == 0) {
                covers = true;
                break;
            }
        }
        CVARCLASS_CHECK(covers, "a retired family prefix covers no converged legacy key — it is stale");
    }

    // #454: gDeveloperTools.DebugSaveFileMode is now genuine class (S). The
    // count static_asserts in cvar_shared_keys.h pin the SIZES (26 shared, 0
    // disputed), but not the identity — 26 could be reached by adding some other
    // key while this one silently vanished. Pin the identity too: the key is IN
    // kSharedIntentKeys and NOT in the disputed table.
    {
        bool sharedHasDebugSave = false;
        for (std::size_t i = 0; i < RSBS::kSharedIntentKeyCount; i++) {
            if (strcmp(RSBS::kSharedIntentKeys[i], "gDeveloperTools.DebugSaveFileMode") == 0) {
                sharedHasDebugSave = true;
                break;
            }
        }
        CVARCLASS_CHECK(sharedHasDebugSave,
                        "gDeveloperTools.DebugSaveFileMode must be in kSharedIntentKeys after #454 (S) resolution");
        for (std::size_t d = 0; d < RSBS::kDisputedClassificationKeyCount; d++) {
            CVARCLASS_CHECK(strcmp(RSBS::kDisputedClassificationKeys[d], "gDeveloperTools.DebugSaveFileMode") != 0,
                            "gDeveloperTools.DebugSaveFileMode is back in the disputed table — #454 reopened");
        }
    }

    // ------------------------------------------------------------------
    // (2) The migrator's decision rules (ADR 0003 §6.3).
    //
    // Pure functions, so the ROM-free tier can exercise the actual rules
    // rather than a restatement of them.
    // ------------------------------------------------------------------

    // OoT's value wins. The MM-spelled value is adopted ONLY when the OoT key
    // is absent — it is the one the user could have set through a live menu,
    // so it is the one they last saw take effect.
    CVARCLASS_CHECK(SOH::ShouldAdoptLegacyValue(true, false), "legacy value must be adopted when OoT's key is unset");
    CVARCLASS_CHECK(!SOH::ShouldAdoptLegacyValue(true, true),
                    "OoT's value must WIN when both spellings are present — adopting MM's would clobber the only "
                    "value a user could have set through a live menu");
    CVARCLASS_CHECK(!SOH::ShouldAdoptLegacyValue(false, false), "nothing to adopt when neither key is present");
    CVARCLASS_CHECK(!SOH::ShouldAdoptLegacyValue(false, true), "nothing to adopt when only OoT's key is present");

    // Float scale -> integer percent. The conversion exists because OoT stores
    // volume as integer percent while MM stored a float scale, and
    // libultraship returns the DEFAULT on a CVar type mismatch: copying the
    // float across unconverted would leave a key every reader ignores.
    CVARCLASS_CHECK(SOH::VolumePercentFromFloatScale(1.0f) == 100, "full scale must convert to 100%");
    CVARCLASS_CHECK(SOH::VolumePercentFromFloatScale(0.0f) == 0, "silence must convert to 0%");
    CVARCLASS_CHECK(SOH::VolumePercentFromFloatScale(0.5f) == 50, "half scale must convert to 50%");
    // Hand-edited configs can hold anything; the conversion must clamp rather
    // than produce an out-of-range percent the slider cannot represent.
    CVARCLASS_CHECK(SOH::VolumePercentFromFloatScale(4.0f) == 100, "over-unity scale must clamp to 100%");
    CVARCLASS_CHECK(SOH::VolumePercentFromFloatScale(-1.0f) == 0, "negative scale must clamp to 0%");

    // ------------------------------------------------------------------
    // (3) Source scan — the drift detector.
    // ------------------------------------------------------------------
#ifdef RSBS_SOURCE_DIR
    const std::filesystem::path sourceRoot(RSBS_SOURCE_DIR);
    bool mmOk = false;
    bool ootOk = false;
    const std::vector<std::string> mmTree = CvarClassSlurpTree(sourceRoot / "games" / "mm", mmOk);
    const std::vector<std::string> ootTree = CvarClassSlurpTree(sourceRoot / "games" / "oot", ootOk);

    if (!mmOk || !ootOk) {
        // Running from a relocated build (an artifact run, say). The manifest
        // and migrator halves above still ran and still passed; only the tree
        // scan is unavailable. Say so loudly rather than reporting a clean
        // pass that checked less than it looks like it did.
        printf("[TEST] WARNING: source tree not found at %s — manifest and migrator checks passed, but the\n"
               "[TEST]          source-drift scan was SKIPPED. In CI this path must not be taken.\n",
               RSBS_SOURCE_DIR);
    } else {
        CVARCLASS_CHECK(!mmTree.empty(), "scanned games/mm and found no source files at all — the scan would "
                                         "vacuously pass, which is worse than not running it");
        CVARCLASS_CHECK(!ootTree.empty(), "scanned games/oot and found no source files at all");

        // (3a) Retired spellings must be GONE. Direction 1: a converged
        // setting silently diverging again.
        for (std::size_t i = 0; i < RSBS::kConvergedKeyCount; i++) {
            const RSBS::ConvergedKey& key = RSBS::kConvergedKeys[i];
            if (CvarClassTreeContainsExactKey(mmTree, key.legacy)) {
                printf("[TEST] FAIL: MM still reads the retired key \"%s\".\n"
                       "[TEST]       It converged onto \"%s\"; reintroducing the old spelling means the setting\n"
                       "[TEST]       silently stops crossing the game boundary. Use the canonical key\n"
                       "[TEST]       (src/common/cvar_shared_keys.h), and note that the volume family also\n"
                       "[TEST]       changed representation: integer percent, read with CVarGetInteger.\n",
                       key.legacy, key.canonical);
                return TEST_FAIL;
            }
        }

        // (3b) The whole retired FAMILY stays retired. A new sibling on an
        // abandoned prefix is the same defect with a different leaf name.
        for (std::size_t p = 0; p < RSBS::kRetiredKeyPrefixCount; p++) {
            const char* prefix = RSBS::kRetiredKeyPrefixes[p];
            if (CvarClassTreeContainsLiteral(mmTree, prefix)) {
                printf("[TEST] FAIL: MM has a key on the retired prefix \"%s\".\n"
                       "[TEST]       That family converged onto OoT's spelling. A new sibling here diverges\n"
                       "[TEST]       again — add it under the canonical prefix instead, and add a row to\n"
                       "[TEST]       RSBS::kConvergedKeys + the version-7 migration if it needs migrating.\n",
                       prefix);
                return TEST_FAIL;
            }
        }

        // (3c) Direction 2 — the dangerous one. Pairs that look like renames
        // and must NOT be merged. If MM's key vanished, someone converged it.
        for (std::size_t i = 0; i < RSBS::kMustStayDistinctCount; i++) {
            const RSBS::DistinctPair& pair = RSBS::kMustStayDistinct[i];
            if (!CvarClassTreeContainsLiteral(mmTree, pair.mmKey)) {
                printf("[TEST] FAIL: MM no longer reads \"%s\" (inventory row: %s).\n"
                       "[TEST]       This key must stay DISTINCT from OoT's \"%s\".\n"
                       "[TEST]       Why: %s\n"
                       "[TEST]       If this removal is deliberate and correct, update\n"
                       "[TEST]       RSBS::kMustStayDistinct and docs/enhancement-classification.md together.\n",
                       pair.mmKey, pair.row, pair.ootKey, pair.why);
                return TEST_FAIL;
            }
        }

        // (3d) The casing hazard. MM's Timesavers (lowercase s) is one
        // keystroke from OoT's TimeSavers in a case-sensitive store, and which
        // spelling wins is undecided.
        if (!CvarClassTreeContainsLiteral(mmTree, RSBS::kParkedCasingPrefixMM)) {
            printf("[TEST] FAIL: MM no longer reads the \"%s\" family.\n"
                   "[TEST]       OoT spells it \"gEnhancements.TimeSavers.\" (capital S) — the two do NOT\n"
                   "[TEST]       collide, but they are one keystroke apart. Which spelling wins is an open\n"
                   "[TEST]       question; converging this family needs an explicit decision first.\n",
                   RSBS::kParkedCasingPrefixMM);
            return TEST_FAIL;
        }

        // (3d-2) #451's REAL arming condition, mechanized (#497 step 2).
        //
        // ADR 0004 discharges #451 by deciding there is only one menu shell —
        // but that discharge is a prose proviso ("provided MM's menu is never
        // revived as a second shell") with nothing enforcing it, and #446's
        // closure removed the technical deterrent that used to make reviving
        // BenMenu obviously dangerous.
        //
        // Note carefully what is asserted, because the obvious wording is RED
        // at head and would be "fixed" by weakening it. #497's lock text asks
        // that EVERY reader of a menu-index key be on a not-linked allowlist —
        // but OoT's live SohMenu reads all four by design, so that assertion
        // fails on correct code. The hazard is TWO SHELLS indexing one key, and
        // the second shell can only be MM's. So the invariant is scoped to
        // MM-side readers: inside games/mm, only the two known-elided BenGui
        // TUs may name these keys. A new MM reader entering the tree is exactly
        // the arming condition, and it fails here rather than in a player's
        // navigation state.
        {
            bool mmPathsOk = false;
            const std::vector<CvarClassSourceFile> mmFiles =
                CvarClassSlurpTreeWithPaths(sourceRoot / "games" / "mm", mmPathsOk);
            CVARCLASS_CHECK(mmPathsOk && !mmFiles.empty(),
                            "could not re-scan games/mm with paths for the menu-index reader allowlist");

            // The two TUs measured to be in NO CMake target: BenMenu.cpp is
            // excluded wholesale by games/mm/CMakeLists.txt's BenGui filter,
            // and BenGui/Menu.cpp survives only inside the elided
            // 2ship_rando_ui archive.
            static const char* const kAllowedMMMenuIndexReaders[] = {
                "2s2h/BenGui/BenMenu.cpp",
                "2s2h/BenGui/Menu.cpp",
            };

            for (std::size_t k = 0; k < RSBS::kMenuIndexKeyCount; k++) {
                const std::string needle = std::string("\"") + RSBS::kMenuIndexKeys[k];
                for (const CvarClassSourceFile& f : mmFiles) {
                    if (f.text.find(needle) == std::string::npos) {
                        continue;
                    }
                    bool allowed = false;
                    for (const char* ok : kAllowedMMMenuIndexReaders) {
                        if (f.relPath == ok) {
                            allowed = true;
                            break;
                        }
                    }
                    if (!allowed) {
                        printf("[TEST] FAIL: games/mm/%s reads the menu-index key \"%s\".\n"
                               "[TEST]       That key indexes a MENU. ADR 0004 discharges #451 by deciding there is\n"
                               "[TEST]       exactly one shell (OoT's SohMenu, extended); a second, MM-side shell\n"
                               "[TEST]       indexing the same key is #451's arming condition, and one game's\n"
                               "[TEST]       persisted section then selects an unrelated entry in the other.\n"
                               "[TEST]       If this file genuinely is not in the link, add it to\n"
                               "[TEST]       kAllowedMMMenuIndexReaders here WITH the CMake evidence. If it IS in\n"
                               "[TEST]       the link, #451 is now armed — say so on #451 rather than widening\n"
                               "[TEST]       this allowlist.\n",
                               f.relPath.c_str(), RSBS::kMenuIndexKeys[k]);
                        return TEST_FAIL;
                    }
                }
            }

            // Non-vacuity: the allowlist must actually be exercised. If MM ever
            // stops reading these keys entirely, the loop above passes without
            // examining anything and the lock silently retires.
            bool sawAnyReader = false;
            for (std::size_t k = 0; k < RSBS::kMenuIndexKeyCount && !sawAnyReader; k++) {
                const std::string needle = std::string("\"") + RSBS::kMenuIndexKeys[k];
                for (const CvarClassSourceFile& f : mmFiles) {
                    if (f.text.find(needle) != std::string::npos) {
                        sawAnyReader = true;
                        break;
                    }
                }
            }
            CVARCLASS_CHECK(sawAnyReader, "no MM file reads any menu-index key — the allowlist check just passed "
                                          "vacuously; if MM's menu TUs were deleted, retire this block and #451 "
                                          "with them rather than leaving a check that examines nothing");
        }

        // (3e) The deliberate cheat sharing survives. Both documents warn
        // against "fixing" this; here the warning is enforced.
        for (std::size_t i = 0; i < RSBS::kDeliberateSharedCheatKeyCount; i++) {
            const char* key = RSBS::kDeliberateSharedCheatKeys[i];
            if (!CvarClassTreeContainsLiteral(mmTree, key)) {
                printf("[TEST] FAIL: MM no longer reads the shared cheat key \"%s\".\n"
                       "[TEST]       Cheats are SHARED INTENT and this sharing is deliberate: the inventory\n"
                       "[TEST]       verified per-key that both implementations match. A cheat is a statement\n"
                       "[TEST]       about how the user wants to play RSBS, not about which game is in front.\n"
                       "[TEST]       Namespacing MM's cheats is a regression, not a collision fix.\n",
                       key);
                return TEST_FAIL;
            }
        }

        // (3f) The canonical keys are actually read by OoT. Catches a typo in
        // the manifest that would otherwise make every check above pass while
        // pointing at a key nothing uses. Skips Volume.Ambience, which is an
        // adoption rather than a rename — OoT has no ambience channel.
        //
        // OoT reaches gSettings.*, gEnhancements.* and gCheats.* through the
        // CVAR_SETTING / CVAR_ENHANCEMENT / CVAR_CHEAT macros, so the full
        // "gPrefix.Leaf" literal never appears in OoT source — only "Leaf" does.
        // Strip those prefixes and probe the leaf. gCosmetics.* has no prefix
        // macro on the OoT side (it is a raw literal), so it stays a full-literal
        // probe by falling through with nothing stripped.
        static const char* const kMacroBackedPrefixes[] = { "gSettings.", "gEnhancements.", "gCheats." };
        for (std::size_t i = 0; i < RSBS::kConvergedKeyCount; i++) {
            const char* canonical = RSBS::kConvergedKeys[i].canonical;
            if (strcmp(canonical, RSBS_CVAR_VOLUME_AMBIENCE) == 0) {
                continue;
            }
            const char* probe = canonical;
            for (const char* prefix : kMacroBackedPrefixes) {
                if (strncmp(canonical, prefix, strlen(prefix)) == 0) {
                    probe = canonical + strlen(prefix);
                    break;
                }
            }
            if (!CvarClassTreeContainsLiteral(ootTree, probe)) {
                printf("[TEST] FAIL: OoT does not appear to read the canonical key \"%s\" (probed \"%s\").\n"
                       "[TEST]       MM was converged onto a key OoT does not use — check the manifest for a\n"
                       "[TEST]       typo, or the ADR's convergence direction for this row.\n",
                       canonical, probe);
                return TEST_FAIL;
            }
        }

        printf("[TEST]   scanned %zu MM + %zu OoT source files\n", mmTree.size(), ootTree.size());
    }
#else
    printf("[TEST] WARNING: RSBS_SOURCE_DIR undefined — source-drift scan SKIPPED.\n");
#endif

    printf("[TEST] PASS: %zu converged keys retired cleanly, %zu per-game pairs still distinct, %zu shared cheat "
           "keys intact\n",
           RSBS::kConvergedKeyCount, RSBS::kMustStayDistinctCount, RSBS::kDeliberateSharedCheatKeyCount);
    return TEST_PASS;
}

#undef CVARCLASS_CHECK
