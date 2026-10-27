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

#define CVARCLASS_CHECK(cond, msg)              \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            return TEST_FAIL;                   \
        }                                       \
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

    for (std::filesystem::recursive_directory_iterator
             it(root, std::filesystem::directory_options::skip_permission_denied, ec),
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

/// Does OoT's tree NAME this key? OoT spells most CVars through the
/// soh/cvar_prefixes.h macros (`CVAR_ENHANCEMENT("Foo")` expands to
/// "gEnhancements.Foo" via CMake/soh-cvars.cmake), so a whole-literal search
/// alone would miss the idiomatic OoT read. Checks the whole literal and, for
/// the three prefixes a kMustStayDistinct key can carry, the macro form.
bool CvarClassOotTreeNamesKey(const std::vector<std::string>& tree, const char* key) {
    if (CvarClassTreeContainsExactKey(tree, key)) {
        return true;
    }
    struct PrefixMacro {
        const char* prefix;
        const char* macro;
    };
    static const PrefixMacro kMacros[] = {
        { "gEnhancements.", "CVAR_ENHANCEMENT(\"" },
        { "gCheats.", "CVAR_CHEAT(\"" },
        { "gSettings.", "CVAR_SETTING(\"" },
        { "gRandoEnhancements.", "CVAR_RANDOMIZER_ENHANCEMENT(\"" }, // #1060's translated row
    };
    const std::string k(key);
    for (const PrefixMacro& m : kMacros) {
        const std::string prefix(m.prefix);
        if (k.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        const std::string needle = std::string(m.macro) + k.substr(prefix.size()) + "\"";
        for (const std::string& text : tree) {
            if (text.find(needle) != std::string::npos) {
                return true;
            }
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

    for (std::filesystem::recursive_directory_iterator
             it(root, std::filesystem::directory_options::skip_permission_denied, ec),
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

namespace {

/// One row of the #966 key-name-gap audit: SoH's menu writes `canonical`, MM
/// used to read the unmapped twin `legacy`. The table is the lock's own copy of
/// the audit's issue list, kept apart from RSBS::kConvergedKeys on purpose: if it
/// derived from the manifest, deleting a manifest row would delete its own
/// assertion and the lock would pass, which is exactly the regression it exists
/// to catch. Every entry must stay in the manifest, in MM's readers (under the
/// canonical spelling), in SoH's menu, and in ConfigVersion9Updater's table.
struct CvarClassKeyGapRow {
    int issue;
    const char* legacy;
    const char* canonical;
};

constexpr CvarClassKeyGapRow kCvarClassKeyGapRows[] = {
    { 1027, "gEnhancements.Graphics.IncreaseActorDrawDistance", "gEnhancements.DisableDrawDistance" },
    { 1028, "gEnhancements.Graphics.ActorCullingAccountsForWidescreen", "gEnhancements.WidescreenActorCulling" },
    { 1029, "gEnhancements.Fixes.FixTexturesOOB", "gEnhancements.FixTexturesOOB" },
    { 1030, "gEnhancements.A11y.NoScreenFlashForEnemyKill", "gSettings.A11yNoScreenFlashForFinishingBlow" },
    { 1031, "gEnhancements.Graphics.3DItemDrops", "gEnhancements.NewDrops" },
    { 1032, "gEnhancements.Cutscenes.SkipMiscInteractions", "gEnhancements.TimeSavers.SkipMiscInteractions" },
    { 1033, "gEnhancements.Playback.SkipScarecrowSong", "gEnhancements.InstantScarecrow" },
    { 1034, "gEnhancements.PlayerActions.ArrowCycle", "gEnhancements.BowArrowCycle" },
    { 1035, "gEnhancements.Equipment.MagicArrowEquipSpeed", "gEnhancements.SkipArrowAnimation" },
    { 1036, "gEnhancements.DifficultyOptions.PermanentHeartLoss", "gEnhancements.PermanentHeartLoss" },
    { 1038, "gEnhancements.Restorations.N64WeirdFrames", "gEnhancements.N64WeirdFrames" },
    // #1062: the ocarina enhancements MM kept under Playback.*. SoH writes the D-pad and right-stick switches as
    // gSettings.CustomOcarina.Dpad / .RightStick (Settings > Controls > Ocarina Controls, drawn from the input editor)
    // and "Prevent Dropped Ocarina Inputs" as gEnhancements.DpadNoDropOcarinaInput. All bool, default off; the P6
    // held-apart entry retires with these rows.
    { 1062, "gEnhancements.Playback.DpadOcarina", "gSettings.CustomOcarina.Dpad" },
    { 1062, "gEnhancements.Playback.RightStickOcarina", "gSettings.CustomOcarina.RightStick" },
    { 1062, "gEnhancements.Playback.NoDropOcarinaInput", "gEnhancements.DpadNoDropOcarinaInput" },
    // #1064: the container-matches-contents switch. MM's rando readers (chests, pots, grass, crates, barrels) read
    // gRando.CSMC; SoH writes gEnhancements.ChestSizeAndTextureMatchContents. Both bool, default off.
    { 1064, "gRando.CSMC", "gEnhancements.ChestSizeAndTextureMatchContents" },
    // #1123: SoH's "Skip Boss Introductions". MM's boss/miniboss intro skips (Cutscenes/SkipEnemyCutscenes.cpp) read
    // gEnhancements.Cutscenes.SkipEnemyCutscenes, which nothing in the single exe writes. Both bool, default off in MM.
    { 1123, "gEnhancements.Cutscenes.SkipEnemyCutscenes", "gEnhancements.TimeSavers.SkipCutscene.BossIntro" },
};

/// One reader of a #966 key-gap key that has to be IN THE LINK, not just mapped. Mapping MM's reader onto SoH's key
/// (PR #1046) is necessary and not sufficient: every one of these providers registers only through a file-scope
/// RegisterShipInitFunc, in the plain `2ship_enh` archive, which the linker drops, so the control moved a key
/// nothing in the single exe consulted. The carve-out list in games/mm/CMakeLists.txt (`ship__Enh_registrars`,
/// WHOLE_ARCHIVE'd) is what keeps a provider; `path` is relative to games/mm/2s2h/Enhancements and names either
/// one file or a directory (every .cpp directly inside it). The table is the lock's own copy, like the key-gap
/// rows above: deleting a CMake entry has to turn this red, and deleting a table entry is a reviewed edit here.
struct CvarClassLinkedReader {
    int issue;
    const char* canonical;
    const char* path;
};

constexpr CvarClassLinkedReader kCvarClassLinkedReaders[] = {
    { 1030, "gSettings.A11yNoScreenFlashForFinishingBlow", "Accessibility/NoScreenFlashForEnemyKill.cpp" },
    { 1031, "gEnhancements.NewDrops", "Graphics/3DItemDrops.cpp" },
    { 1032, "gEnhancements.TimeSavers.SkipMiscInteractions", "Cutscenes/MiscInteractions" },
    { 1033, "gEnhancements.InstantScarecrow", "Songs/SkipScarecrowSong.cpp" },
    { 1034, "gEnhancements.BowArrowCycle", "Equipment/ArrowCycle.cpp" },
    { 1035, "gEnhancements.SkipArrowAnimation", "Equipment/SkipMagicArrowEquip.cpp" },
    { 1036, "gEnhancements.PermanentHeartLoss", "DifficultyOptions/PermanentHeartLoss.cpp" },
    { 1038, "gEnhancements.N64WeirdFrames", "Restorations/N64WeirdFrames/N64WeirdFrames.cpp" },
    // #1037: the held-apart damage multiplier. Not a key-name gap (nothing is mapped), but the same defect: the
    // provider registers only through a file-scope RegisterShipInitFunc in the plain 2ship_enh archive.
    { 1037, "gEnhancements.DifficultyOptions.DamageMultiplier", "DifficultyOptions/DamageMultiplier.cpp" },
    { 1123, "gEnhancements.TimeSavers.SkipCutscene.BossIntro", "Cutscenes/SkipEnemyCutscenes.cpp" },
};

/// #1037: held APART, not mapped, and HOSTED: OoT's DamageMult list runs 0-8, MM's 0-4 plus 10, so each game keeps its
/// own key and the SoH menu draws MM's own row beside SoH's (Enhancements > Difficulty).
constexpr const char* kCvarClassDamageMultMm = "gEnhancements.DifficultyOptions.DamageMultiplier";
constexpr const char* kCvarClassDamageMultOot = "gEnhancements.DamageMult";

/// #1060: SoH's three-tier "Skip Get Item Animations" drives MM's four-tier
/// get-item presentation through a translator. The lock's own copy of the row,
/// apart from RSBS::kTranslatedKeys for the reason the key-gap rows are.
struct CvarClassTranslatedRow {
    int issue;
    const char* mmKey;
    const char* ootKey;
    const char* translator;
};

constexpr CvarClassTranslatedRow kCvarClassTranslatedRows[] = {
    { 1060, "gEnhancements.Cutscenes.SkipGetItemCutscenes", "gRandoEnhancements.TimeSavers.SkipGetItemAnimation",
      "2s2h/Rando/StaticData/Items.cpp" },
};

/// #1060's linked read sites: each must call the translator's tier.
constexpr const char* kCvarClassGetItemTierReaders[] = {
    "2s2h/Rando/StaticData/Items.cpp",
    "2s2h/Rando/MiscBehavior/CheckQueue.cpp",
};

/// #1060's story-cutscene gives (they queue only outside rando): each must ask
/// StoryGiveShowsGetItemCutscene(), which keeps the hold unless SoH says All
/// Items, so the default Junk Items never strips a mask, song or the Oath.
constexpr const char* kCvarClassStoryGiveReaders[] = {
    "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipGiantsChamber.cpp",
    "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipHealingDarmani.cpp",
    "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipHealingMikau.cpp",
    "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipIkanaCurseCutscenes.cpp",
    "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningElegyOfEmptiness.cpp",
    "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningGoronLullaby.cpp",
    "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningGoronLullabyIntro.cpp",
    "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningNewWaveBossaNova.cpp",
    "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningSonataOfAwakening.cpp",
    "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningSongOfSoaring.cpp",
    "2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearningSongOfStorms.cpp",
};

/// MM files that may still name a translated row's MM key: BenMenu.cpp, whose
/// widget writes it, is excluded wholesale by games/mm/CMakeLists.txt's BenGui
/// filter, so nothing linked reads or writes the key.
constexpr const char* kCvarClassTranslatedElidedWriters[] = {
    "2s2h/BenGui/BenMenu.cpp",
};

} // namespace

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

    // The #966 key-name-gap rows (#1027-#1036, #1038): each pinned row is in the
    // manifest exactly as the audit specified, and is classified ONCE (a legacy
    // spelling is not also a shared-intent key, which would claim "both games
    // read this key" of a key only one game reads).
    for (const CvarClassKeyGapRow& row : kCvarClassKeyGapRows) {
        bool inManifest = false;
        for (std::size_t i = 0; i < RSBS::kConvergedKeyCount; i++) {
            const RSBS::ConvergedKey& key = RSBS::kConvergedKeys[i];
            if (strcmp(key.legacy, row.legacy) == 0 && strcmp(key.canonical, row.canonical) == 0) {
                inManifest = !key.scaledPercent;
                break;
            }
        }
        if (!inManifest) {
            printf("[TEST] FAIL: #%d: RSBS::kConvergedKeys has no plain row mapping MM's \"%s\" onto SoH's \"%s\".\n"
                   "[TEST]       SoH's menu writes the canonical key; without the row MM reads a key nothing\n"
                   "[TEST]       writes and the control silently does nothing in Majora's Mask (#966).\n",
                   row.issue, row.legacy, row.canonical);
            return TEST_FAIL;
        }
        for (std::size_t i = 0; i < RSBS::kSharedIntentKeyCount; i++) {
            if (strcmp(RSBS::kSharedIntentKeys[i], row.legacy) == 0 ||
                strcmp(RSBS::kSharedIntentKeys[i], row.canonical) == 0) {
                printf("[TEST] FAIL: #%d: \"%s\" is both a converged key and a shared-intent key.\n"
                       "[TEST]       A key is classified once: converged rows retire the MM spelling, shared-intent\n"
                       "[TEST]       rows claim both games already read ONE spelling.\n",
                       row.issue, strcmp(RSBS::kSharedIntentKeys[i], row.legacy) == 0 ? row.legacy : row.canonical);
                return TEST_FAIL;
            }
        }
    }

    // #1037 is the one audit row that must NOT be mapped: the value spaces differ, so it stays in kMustStayDistinct
    // with a ruled reason (hosted beside SoH's row, one key per game), not as a PARKED question.
    {
        bool held = false;
        for (std::size_t i = 0; i < RSBS::kMustStayDistinctCount; i++) {
            if (strcmp(RSBS::kMustStayDistinct[i].mmKey, kCvarClassDamageMultMm) == 0 &&
                strcmp(RSBS::kMustStayDistinct[i].ootKey, kCvarClassDamageMultOot) == 0) {
                held = true;
                CVARCLASS_CHECK(strncmp(RSBS::kMustStayDistinct[i].row, "PARKED", 6) != 0,
                                "#1037: the damage multiplier entry is still PARKED. The ruling is made: the value "
                                "spaces differ, MM's own row is hosted beside SoH's, and the entry says so");
            }
        }
        CVARCLASS_CHECK(held, "#1037: the damage multiplier pair is not in kMustStayDistinct — OoT's DamageMult "
                              "runs 0-8 and MM's 0-4 plus 10 (1-hit KO), so a plain key swap is a regression");
        for (std::size_t i = 0; i < RSBS::kConvergedKeyCount; i++) {
            CVARCLASS_CHECK(strcmp(RSBS::kConvergedKeys[i].legacy, kCvarClassDamageMultMm) != 0 &&
                                strcmp(RSBS::kConvergedKeys[i].canonical, kCvarClassDamageMultOot) != 0,
                            "#1037: the damage multiplier was converged while its value spaces still differ");
        }
    }

    // #1060: each translated row is in the manifest as pinned, and is classified
    // ONCE: a rename row or a held-apart pair would contradict the translation.
    for (const CvarClassTranslatedRow& row : kCvarClassTranslatedRows) {
        bool inManifest = false;
        for (std::size_t i = 0; i < RSBS::kTranslatedKeyCount; i++) {
            const RSBS::TranslatedKey& key = RSBS::kTranslatedKeys[i];
            if (key.issue == row.issue && strcmp(key.mmKey, row.mmKey) == 0 && strcmp(key.ootKey, row.ootKey) == 0 &&
                strcmp(key.translator, row.translator) == 0) {
                inManifest = true;
                break;
            }
        }
        if (!inManifest) {
            printf("[TEST] FAIL: #%d: RSBS::kTranslatedKeys has no row translating SoH's \"%s\" onto MM's \"%s\"\n"
                   "[TEST]       in games/mm/%s. Without it nothing records that SoH's control drives MM, and\n"
                   "[TEST]       MM's key, which nothing in the single exe writes, decides alone (#1060).\n",
                   row.issue, row.ootKey, row.mmKey, row.translator);
            return TEST_FAIL;
        }
        for (std::size_t i = 0; i < RSBS::kConvergedKeyCount; i++) {
            const RSBS::ConvergedKey& key = RSBS::kConvergedKeys[i];
            CVARCLASS_CHECK(strcmp(key.legacy, row.mmKey) != 0 && strcmp(key.canonical, row.ootKey) != 0,
                            "#1060: a translated key is also a rename row; the value spaces differ, so a key swap "
                            "would give SoH's All Items MM's Everything But Major");
        }
        for (std::size_t i = 0; i < RSBS::kSharedIntentKeyCount; i++) {
            CVARCLASS_CHECK(strcmp(RSBS::kSharedIntentKeys[i], row.mmKey) != 0 &&
                                strcmp(RSBS::kSharedIntentKeys[i], row.ootKey) != 0,
                            "#1060: a translated key is also a shared-intent key");
        }
        for (std::size_t i = 0; i < RSBS::kMustStayDistinctCount; i++) {
            CVARCLASS_CHECK(strcmp(RSBS::kMustStayDistinct[i].mmKey, row.mmKey) != 0,
                            "#1060: a translated key is also held apart in kMustStayDistinct");
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

        // (3c-2) Direction 2, the OTHER half (#693, PR #730 review). (3c)
        // only notices a pair collapsing by MM's key DISAPPEARING. The same
        // convergence can happen with every MM key still in place: OoT starts
        // reading MM's key (e.g. soh/Enhancements/QoL/Autosave.cpp reading
        // gEnhancements.Saving.AutosaveInterval, so the MM-only slider on
        // Combo -> MM Enhancements silently moves OoT's fixed 3-minute
        // interval), or MM starts reading OoT's. So neither game may NAME the
        // other side's key of a distinct pair. An ootKey that is prose rather
        // than a key (the autosave interval's "(none — ...)") has nothing to
        // search for; the scan covers games/oot and games/mm only, so the
        // shared manifest in src/common and the menu that draws it from there
        // are not readers here.
        for (std::size_t i = 0; i < RSBS::kMustStayDistinctCount; i++) {
            const RSBS::DistinctPair& pair = RSBS::kMustStayDistinct[i];
            if (CvarClassOotTreeNamesKey(ootTree, pair.mmKey)) {
                printf("[TEST] FAIL: OoT now names MM's \"%s\" (inventory row: %s).\n"
                       "[TEST]       The pair is held DISTINCT; OoT's side is \"%s\".\n"
                       "[TEST]       Why: %s\n"
                       "[TEST]       An OoT reader of MM's key converges the pair from OoT's side: the\n"
                       "[TEST]       MM-only control would now move OoT too. Host it from src/common's\n"
                       "[TEST]       manifest (RSBS::kHostedMmEnhancements) rather than naming it in games/oot.\n",
                       pair.mmKey, pair.row, pair.ootKey, pair.why);
                return TEST_FAIL;
            }
            if (pair.ootKey[0] == 'g' && CvarClassTreeContainsExactKey(mmTree, pair.ootKey)) {
                printf("[TEST] FAIL: MM now reads OoT's \"%s\" (inventory row: %s).\n"
                       "[TEST]       The pair is held DISTINCT; MM's side is \"%s\".\n"
                       "[TEST]       Why: %s\n",
                       pair.ootKey, pair.row, pair.mmKey, pair.why);
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

        // (3f-2) The #966 key-name gaps, in the tree. The manifest row alone proves
        // nothing: MM's reader must actually read SoH's spelling (the retired MM
        // spelling being gone is (3a)), SoH's menu must actually write it, and the
        // version-9 migration must carry a stored MM-spelled value across.
        {
            bool menuOk = false;
            std::vector<std::string> sohMenuTree =
                CvarClassSlurpTree(sourceRoot / "games" / "oot" / "soh" / "SohGui", menuOk);
            CVARCLASS_CHECK(menuOk && !sohMenuTree.empty(), "could not scan games/oot/soh/SohGui for the menu rows");
            // Settings > Controls is drawn partly from the input editor window (the ocarina and D-pad rows, #1062),
            // which lives beside the controls code rather than under SohGui.
            bool controlsOk = false;
            const std::vector<std::string> sohControlsTree =
                CvarClassSlurpTree(sourceRoot / "games" / "oot" / "soh" / "Enhancements" / "controls", controlsOk);
            CVARCLASS_CHECK(controlsOk && !sohControlsTree.empty(),
                            "could not scan games/oot/soh/Enhancements/controls for the input editor rows");
            sohMenuTree.insert(sohMenuTree.end(), sohControlsTree.begin(), sohControlsTree.end());

            std::ifstream migratorFile(sourceRoot / "games" / "oot" / "soh" / "config" / "ConfigMigrators.h",
                                       std::ios::binary);
            const std::string migrators((std::istreambuf_iterator<char>(migratorFile)),
                                        std::istreambuf_iterator<char>());
            const std::size_t v9 = migrators.find("version9Migrations");
            CVARCLASS_CHECK(v9 != std::string::npos, "ConfigMigrators.h has no version9Migrations table (#966)");
            const std::string v9Text = migrators.substr(v9);

            for (const CvarClassKeyGapRow& row : kCvarClassKeyGapRows) {
                if (!CvarClassTreeContainsExactKey(mmTree, row.canonical)) {
                    printf("[TEST] FAIL: #%d: no file under games/mm reads SoH's \"%s\".\n"
                           "[TEST]       MM's reader of \"%s\" has to move onto the key SoH's menu writes, or the\n"
                           "[TEST]       control does nothing in Majora's Mask.\n",
                           row.issue, row.canonical, row.legacy);
                    return TEST_FAIL;
                }
                if (!CvarClassOotTreeNamesKey(sohMenuTree, row.canonical)) {
                    printf("[TEST] FAIL: #%d: no SoH menu row (SohGui or the input editor) writes \"%s\".\n"
                           "[TEST]       The manifest maps MM onto a key no widget sets; check the spelling.\n",
                           row.issue, row.canonical);
                    return TEST_FAIL;
                }
                const std::string legacyLiteral = std::string("\"") + row.legacy + "\"";
                const std::string canonicalLiteral = std::string("\"") + row.canonical + "\"";
                if (v9Text.find(legacyLiteral) == std::string::npos ||
                    v9Text.find(canonicalLiteral) == std::string::npos) {
                    printf("[TEST] FAIL: #%d: version9Migrations lacks \"%s\" -> \"%s\".\n"
                           "[TEST]       A config storing the MM spelling would keep it and lose the setting.\n",
                           row.issue, row.legacy, row.canonical);
                    return TEST_FAIL;
                }
            }
        }

        // (3f-2b) #1037, the HOSTED half of a held-apart pair. The two keys stay distinct (the value spaces differ),
        // so MM's own key needs a control of its own or it does nothing in the single exe. The control is a row on
        // SoH's Enhancements > Difficulty page, bound through the src/common constant: games/oot may not spell MM's
        // key (3c-2), and a literal there would converge the pair from OoT's side.
        {
            bool menuOk = false;
            const std::vector<std::string> sohGuiTree =
                CvarClassSlurpTree(sourceRoot / "games" / "oot" / "soh" / "SohGui", menuOk);
            CVARCLASS_CHECK(menuOk && !sohGuiTree.empty(), "could not scan games/oot/soh/SohGui for the hosted row");
            std::ifstream keysFile(sourceRoot / "src" / "common" / "cvar_shared_keys.h", std::ios::binary);
            const std::string keysText((std::istreambuf_iterator<char>(keysFile)), std::istreambuf_iterator<char>());
            const std::string defineLine =
                std::string("#define RSBS_CVAR_MM_DAMAGE_MULTIPLIER \"") + kCvarClassDamageMultMm + "\"";
            CVARCLASS_CHECK(keysText.find(defineLine) != std::string::npos,
                            "#1037: src/common/cvar_shared_keys.h does not define RSBS_CVAR_MM_DAMAGE_MULTIPLIER as "
                            "MM's damage multiplier key");
            bool hosted = false;
            for (const std::string& text : sohGuiTree) {
                if (text.find(".CVar(RSBS_CVAR_MM_DAMAGE_MULTIPLIER)") != std::string::npos) {
                    hosted = true;
                }
            }
            CVARCLASS_CHECK(hosted, "#1037: no row under games/oot/soh/SohGui writes MM's damage multiplier key "
                                    "(.CVar(RSBS_CVAR_MM_DAMAGE_MULTIPLIER)), so the MM reader has no control");
            CVARCLASS_CHECK(CvarClassOotTreeNamesKey(sohGuiTree, kCvarClassDamageMultOot),
                            "#1037: SoH's own Damage Multiplier row no longer writes gEnhancements.DamageMult");
        }

        // (3f-3) Mapped is not live: the #966 key-gap rows whose provider is a RegisterShipInitFunc-only TU
        // (#1030-#1034). (3f-2) proves MM reads SoH's spelling; this proves the TU that reads it is carved into the
        // WHOLE_ARCHIVE'd registrars archive, so the single exe contains it at all. Each reader file is also
        // required to still spell the canonical key, so a table row cannot outlive a reader that moved.
        {
            std::ifstream cmakeFile(sourceRoot / "games" / "mm" / "CMakeLists.txt", std::ios::binary);
            const std::string cmake((std::istreambuf_iterator<char>(cmakeFile)), std::istreambuf_iterator<char>());
            const std::size_t listBegin = cmake.find("set(ship__Enh_registrars");
            const std::size_t listEnd = cmake.find("list(REMOVE_ITEM ship__Enhancements ${ship__Enh_registrars})");
            CVARCLASS_CHECK(listBegin != std::string::npos && listEnd != std::string::npos && listEnd > listBegin,
                            "games/mm/CMakeLists.txt has no ship__Enh_registrars carve-out list (#1071) — the "
                            "mapped-is-not-live scan cannot run");
            const std::string registrars = cmake.substr(listBegin, listEnd - listBegin);

            for (const CvarClassLinkedReader& row : kCvarClassLinkedReaders) {
                const std::filesystem::path readerPath =
                    sourceRoot / "games" / "mm" / "2s2h" / "Enhancements" / row.path;
                std::vector<std::filesystem::path> readers;
                std::error_code ec;
                if (std::filesystem::is_directory(readerPath, ec)) {
                    for (const auto& entry : std::filesystem::directory_iterator(readerPath, ec)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".cpp") {
                            readers.push_back(entry.path());
                        }
                    }
                } else {
                    readers.push_back(readerPath);
                }
                if (readers.empty()) {
                    printf("[TEST] FAIL: #%d: no reader file at games/mm/2s2h/Enhancements/%s.\n"
                           "[TEST]       The lock table names a provider that is gone; update the row with the move.\n",
                           row.issue, row.path);
                    return TEST_FAIL;
                }
                for (const std::filesystem::path& reader : readers) {
                    std::ifstream readerFile(reader, std::ios::binary);
                    const std::string text((std::istreambuf_iterator<char>(readerFile)),
                                           std::istreambuf_iterator<char>());
                    if (text.find(std::string("\"") + row.canonical + "\"") == std::string::npos) {
                        printf("[TEST] FAIL: #%d: %s no longer spells \"%s\".\n"
                               "[TEST]       The lock table's reader moved or was repointed; update the row.\n",
                               row.issue, reader.generic_string().c_str(), row.canonical);
                        return TEST_FAIL;
                    }
                    const std::string listed = std::string("\"2s2h/Enhancements/") +
                                               (std::filesystem::is_directory(readerPath, ec)
                                                    ? std::string(row.path) + "/" + reader.filename().generic_string()
                                                    : std::string(row.path)) +
                                               "\"";
                    if (registrars.find(listed) == std::string::npos) {
                        printf("[TEST] FAIL: #%d: %s is not in the ship__Enh_registrars carve-out list.\n"
                               "[TEST]       MM reads SoH's \"%s\" there, but the plain 2ship_enh archive drops a TU "
                               "whose\n"
                               "[TEST]       only anchor is a RegisterShipInitFunc, so the row does nothing in the "
                               "single\n"
                               "[TEST]       exe (#966). Append it to the list in games/mm/CMakeLists.txt and add a\n"
                               "[TEST]       MMRegistrarCoverage probe for it.\n",
                               row.issue, listed.c_str(), row.canonical);
                        return TEST_FAIL;
                    }
                }
            }
        }

        // (3f-4) #1060's translated rows, in the tree: the translator reads SoH's
        // key, SoH's menu writes it, every linked MM read site calls the
        // translator, and no linked MM file names MM's own key any more.
        {
            bool mmPathsOk = false;
            const std::vector<CvarClassSourceFile> mmFiles =
                CvarClassSlurpTreeWithPaths(sourceRoot / "games" / "mm", mmPathsOk);
            CVARCLASS_CHECK(mmPathsOk && !mmFiles.empty(), "could not re-scan games/mm for the translated rows");
            bool menuOk = false;
            const std::vector<std::string> sohMenuTree =
                CvarClassSlurpTree(sourceRoot / "games" / "oot" / "soh" / "SohGui", menuOk);
            CVARCLASS_CHECK(menuOk && !sohMenuTree.empty(), "could not scan games/oot/soh/SohGui for the menu rows");

            auto fileText = [&mmFiles](const char* relPath) -> const std::string* {
                for (const CvarClassSourceFile& f : mmFiles) {
                    if (f.relPath == relPath) {
                        return &f.text;
                    }
                }
                return nullptr;
            };

            for (const CvarClassTranslatedRow& row : kCvarClassTranslatedRows) {
                const std::string mmLiteral = std::string("\"") + row.mmKey + "\"";
                const std::string ootLiteral = std::string("\"") + row.ootKey + "\"";
                const std::string* translator = fileText(row.translator);
                if (translator == nullptr || translator->find(ootLiteral) == std::string::npos) {
                    printf("[TEST] FAIL: #%d: games/mm/%s does not read SoH's \"%s\".\n"
                           "[TEST]       The translator must read the key SoH's menu writes, or the control does\n"
                           "[TEST]       nothing in Majora's Mask.\n",
                           row.issue, row.translator, row.ootKey);
                    return TEST_FAIL;
                }
                if (!CvarClassOotTreeNamesKey(sohMenuTree, row.ootKey)) {
                    printf("[TEST] FAIL: #%d: no SoH menu row under games/oot/soh/SohGui writes \"%s\".\n", row.issue,
                           row.ootKey);
                    return TEST_FAIL;
                }
                for (const CvarClassSourceFile& f : mmFiles) {
                    if (f.text.find(mmLiteral) == std::string::npos || f.relPath == row.translator) {
                        continue;
                    }
                    bool elided = false;
                    for (const char* ok : kCvarClassTranslatedElidedWriters) {
                        elided = elided || f.relPath == ok;
                    }
                    if (!elided) {
                        printf("[TEST] FAIL: #%d: games/mm/%s reads MM's \"%s\" directly.\n"
                               "[TEST]       Nothing in the single exe writes that key; SoH's \"%s\" decides it\n"
                               "[TEST]       through the translator in games/mm/%s. Call the translator instead.\n",
                               row.issue, f.relPath.c_str(), row.mmKey, row.ootKey, row.translator);
                        return TEST_FAIL;
                    }
                }
            }
            for (const char* reader : kCvarClassGetItemTierReaders) {
                const std::string* text = fileText(reader);
                if (text == nullptr || text->find("SkipGetItemCutsceneTier()") == std::string::npos) {
                    printf("[TEST] FAIL: #1060: games/mm/%s does not call SkipGetItemCutsceneTier(); its get-item\n"
                           "[TEST]       presentation would not follow SoH's Skip Get Item Animations.\n",
                           reader);
                    return TEST_FAIL;
                }
            }
            for (const char* reader : kCvarClassStoryGiveReaders) {
                const std::string* text = fileText(reader);
                if (text == nullptr || text->find("StoryGiveShowsGetItemCutscene()") == std::string::npos ||
                    text->find("!Rando::StaticData::SkipGetItemCutsceneTier()") != std::string::npos) {
                    printf("[TEST] FAIL: #1060: games/mm/%s does not ask StoryGiveShowsGetItemCutscene() for its\n"
                           "[TEST]       story give's hold (or still negates the raw tier, which strips the hold\n"
                           "[TEST]       at SoH's default Junk Items).\n",
                           reader);
                    return TEST_FAIL;
                }
            }
        }

        // (3f-5) W6-CAM-AIM (#1236): MM's aiming, camera and shield inversion / sensitivity keys are held apart from
        // SoH's (P3, P7), and MM reads every one of them, so each needs a control of its own. Each is bound by exactly
        // one row in SoH's Bindings window (SohInputEditorWindow.cpp, Camera Controls), its RSBS_CVAR_MM_* define
        // passed straight to a UIWidgets::CVar* call as its key; games/oot never spells the key; MM still reads it,
        // with the default the row declares. Three of them MM reads inverted by default (1), so an unset key must
        // draw ticked, the way Majora's Mask plays it. The table is the lock's own copy, like the key-gap rows.
        {
            struct CamAimRow {
                const char* define;      // the define's name, as the row must spell it
                const char* defineValue; // what the define expands to
                const char* key;         // MM's key
                const char* mmRead;      // how MM reads it: the literal, then this default
                const char* rowDefault;  // the option the row's chain must carry (nullptr: a checkbox left off)
            };
#define CAM_AIM_ROW(def, key, mmRead, rowDefault) \
    { #def, def, key, mmRead, rowDefault }
            static const CamAimRow kCamAimRows[] = {
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_FP_DISABLE_AUTO_CENTER,
                            "gEnhancements.Camera.FirstPerson.DisableFirstPersonAutoCenterView", ", 0)", nullptr),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_FP_INVERT_X, "gEnhancements.Camera.FirstPerson.InvertX", ", 0)",
                            nullptr),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_FP_INVERT_Y, "gEnhancements.Camera.FirstPerson.InvertY", ", 1)",
                            ".DefaultValue(true)"),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_FP_SENSITIVITY_X, "gEnhancements.Camera.FirstPerson.SensitivityX",
                            ", 1.0f)", ".DefaultValue(1.0f)"),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_FP_SENSITIVITY_Y, "gEnhancements.Camera.FirstPerson.SensitivityY",
                            ", 1.0f)", ".DefaultValue(1.0f)"),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_FP_RIGHT_STICK_ENABLED,
                            "gEnhancements.Camera.FirstPerson.RightStickEnabled", ", 0)", nullptr),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_FP_MOVE_IN_FIRST_PERSON,
                            "gEnhancements.Camera.FirstPerson.MoveInFirstPerson", ", 0)", nullptr),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_FP_RIGHT_STICK_INVERT_X,
                            "gEnhancements.Camera.FirstPerson.RightStickInvertX", ", 0)", nullptr),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_FP_RIGHT_STICK_INVERT_Y,
                            "gEnhancements.Camera.FirstPerson.RightStickInvertY", ", 1)", ".DefaultValue(true)"),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_FP_RIGHT_STICK_SENSITIVITY_X,
                            "gEnhancements.Camera.FirstPerson.RightStickSensitivityX", ", 1.0f)",
                            ".DefaultValue(1.0f)"),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_FP_RIGHT_STICK_SENSITIVITY_Y,
                            "gEnhancements.Camera.FirstPerson.RightStickSensitivityY", ", 1.0f)",
                            ".DefaultValue(1.0f)"),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_RIGHT_STICK_INVERT_X, "gEnhancements.Camera.RightStick.InvertXAxis",
                            ", 0)", nullptr),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_RIGHT_STICK_INVERT_Y, "gEnhancements.Camera.RightStick.InvertYAxis",
                            ", 1)", ".DefaultValue(true)"),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_RIGHT_STICK_SENSITIVITY_X,
                            "gEnhancements.Camera.RightStick.CameraSensitivity.X", ", 1.0f)", ".DefaultValue(1.0f)"),
                CAM_AIM_ROW(RSBS_CVAR_MM_CAMERA_RIGHT_STICK_SENSITIVITY_Y,
                            "gEnhancements.Camera.RightStick.CameraSensitivity.Y", ", 1.0f)", ".DefaultValue(1.0f)"),
                CAM_AIM_ROW(RSBS_CVAR_MM_EQUIPMENT_INVERT_SHIELD_Y, "gEnhancements.Equipment.InvertShieldY", ", 0)",
                            nullptr),
            };
#undef CAM_AIM_ROW
            const char* const kWindowFile = "games/oot/soh/Enhancements/controls/SohInputEditorWindow.cpp";
            std::ifstream windowFile(sourceRoot / kWindowFile, std::ios::binary);
            const std::string window((std::istreambuf_iterator<char>(windowFile)), std::istreambuf_iterator<char>());
            CVARCLASS_CHECK(!window.empty(), "W6-CAM-AIM: could not read SoH's Bindings window source "
                                             "(games/oot/soh/Enhancements/controls/SohInputEditorWindow.cpp)");

            // Every UIWidgets::CVar*("label" ..., DEFINE, ...) call in the window: the define it binds and the
            // call's own text (balanced parentheses, string literals skipped), so the default it declares is
            // read from that row's chain and no other.
            struct Binding {
                std::string define;
                std::string call;
            };
            std::vector<Binding> bindings;
            auto isIdent = [](char ch) {
                return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
            };
            auto skipSpace = [&window](std::size_t at) {
                while (at < window.size() &&
                       (window[at] == ' ' || window[at] == '\t' || window[at] == '\r' || window[at] == '\n')) {
                    at++;
                }
                return at;
            };
            auto skipLiteral = [&window](std::size_t at) { // at is on the opening quote
                for (at++; at < window.size() && window[at] != '"'; at++) {
                    if (window[at] == '\\') {
                        at++;
                    }
                }
                return at + 1;
            };
            for (std::size_t at = window.find("UIWidgets::CVar"); at != std::string::npos;
                 at = window.find("UIWidgets::CVar", at + 1)) {
                std::size_t open = at + std::strlen("UIWidgets::CVar");
                while (open < window.size() && isIdent(window[open])) {
                    open++;
                }
                if (open >= window.size() || window[open] != '(') {
                    continue;
                }
                std::size_t end = open;
                for (int depth = 0; end < window.size(); end++) {
                    if (window[end] == '"') {
                        end = skipLiteral(end) - 1;
                    } else if (window[end] == '(') {
                        depth++;
                    } else if (window[end] == ')' && --depth == 0) {
                        break;
                    }
                }
                std::size_t arg = skipSpace(open + 1);
                if (arg >= window.size() || window[arg] != '"') {
                    continue;
                }
                while (arg < window.size() && window[arg] == '"') {
                    arg = skipSpace(skipLiteral(arg));
                }
                if (arg >= window.size() || window[arg] != ',') {
                    continue;
                }
                const std::size_t nameBegin = skipSpace(arg + 1);
                std::size_t nameEnd = nameBegin;
                while (nameEnd < window.size() && isIdent(window[nameEnd])) {
                    nameEnd++;
                }
                bindings.push_back({ window.substr(nameBegin, nameEnd - nameBegin), window.substr(at, end - at + 1) });
            }

            for (const CamAimRow& row : kCamAimRows) {
                if (std::strcmp(row.defineValue, row.key) != 0) {
                    printf("[TEST] FAIL: W6-CAM-AIM: %s expands to \"%s\", not MM's \"%s\".\n", row.define,
                           row.defineValue, row.key);
                    return TEST_FAIL;
                }
                const std::string mmRead = std::string("\"") + row.key + "\"" + row.mmRead;
                if (!CvarClassTreeContainsLiteral(mmTree, mmRead.c_str() + 1)) {
                    printf(
                        "[TEST] FAIL: W6-CAM-AIM: games/mm no longer reads %s.\n"
                        "[TEST]       The row in SoH's Bindings window declares that default; if MM's reader moved\n"
                        "[TEST]       or changed its default, move the row's default with it and update this table.\n",
                        mmRead.c_str());
                    return TEST_FAIL;
                }
                if (CvarClassOotTreeNamesKey(ootTree, row.key)) {
                    printf("[TEST] FAIL: W6-CAM-AIM: games/oot spells MM's \"%s\" as a literal.\n"
                           "[TEST]       Bind it through %s (src/common/cvar_shared_keys.h), as the #1037 row does.\n",
                           row.key, row.define);
                    return TEST_FAIL;
                }
                const Binding* bound = nullptr;
                int count = 0;
                for (const Binding& b : bindings) {
                    if (b.define == row.define) {
                        bound = &b;
                        count++;
                    }
                }
                if (count != 1) {
                    printf("[TEST] FAIL: W6-CAM-AIM: %d rows in %s bind %s (want exactly one).\n"
                           "[TEST]       MM reads \"%s\" and nothing in the single exe writes it without that row:\n"
                           "[TEST]       pass the define straight to a UIWidgets::CVar* call beside its SoH twin.\n",
                           count, kWindowFile, row.define, row.key);
                    return TEST_FAIL;
                }
                const bool declares = row.rowDefault != nullptr
                                          ? bound->call.find(row.rowDefault) != std::string::npos
                                          : bound->call.find(".DefaultValue(true)") == std::string::npos;
                if (!declares) {
                    printf("[TEST] FAIL: W6-CAM-AIM: the row binding %s does not declare MM's default (%s).\n"
                           "[TEST]       MM reads %s; an unset key must draw the way Majora's Mask plays it.\n",
                           row.define, row.rowDefault != nullptr ? row.rowDefault : "off", mmRead.c_str());
                    return TEST_FAIL;
                }
            }
        }

        // (3g) The tier-4 namespace is CLASSIFIED (ADR 0004 §6's scope note;
        // ADR 0011 increment 2).
        //
        // State 4 (frozen-at-creation) applies to world-identity keys and not
        // to preference keys, and "an unclassified key defaults to identity,
        // because guessing 'preference' for an identity key is the failure
        // this state exists to prevent". So every key in the combo's own
        // namespace must have a row in RSBS::kComboKeys — and the row must be
        // reachable from the tree, or the manifest classifies a key nothing
        // spells. Both directions, like the convergence tables above.
        //
        // The scan is the whole tree this time (src/common, rsbs, both games),
        // because the namespace is common-owned and its keys are spelled in
        // src/common headers and in OoT's interim Cross-Game host alike. A
        // literal that is a bare prefix (ends in '.') is one of the manifest's
        // own prefix constants and is skipped.
        {
            const std::filesystem::path kComboRoots[] = {
                sourceRoot / "src" / "common",
                sourceRoot / "rsbs",
                sourceRoot / "games" / "oot",
                sourceRoot / "games" / "mm",
            };
            std::vector<CvarClassSourceFile> comboFiles;
            for (const std::filesystem::path& root : kComboRoots) {
                bool rootOk = false;
                std::vector<CvarClassSourceFile> part = CvarClassSlurpTreeWithPaths(root, rootOk);
                CVARCLASS_CHECK(rootOk && !part.empty(),
                                "could not scan a tree for the tier-4 namespace — the scan would be vacuous");
                for (CvarClassSourceFile& f : part) {
                    comboFiles.push_back(std::move(f));
                }
            }

            const std::string needle = std::string("\"") + RSBS::kComboKeyPrefix;
            bool seen[RSBS::kComboKeyCount] = {};
            std::size_t literalsSeen = 0;
            for (const CvarClassSourceFile& f : comboFiles) {
                std::size_t pos = 0;
                while ((pos = f.text.find(needle, pos)) != std::string::npos) {
                    const std::size_t start = pos + 1;
                    const std::size_t end = f.text.find('"', start);
                    if (end == std::string::npos) {
                        break;
                    }
                    const std::string key = f.text.substr(start, end - start);
                    pos = end + 1;
                    if (key.empty() || key.back() == '.') {
                        continue; // a prefix constant, not a key
                    }
                    if (key.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.") !=
                        std::string::npos) {
                        // Prose inside a quoted comment (a wildcard, a space),
                        // not a key: a CVar key spells only [A-Za-z0-9_.].
                        continue;
                    }
                    literalsSeen++;
                    bool classified = false;
                    for (std::size_t k = 0; k < RSBS::kComboKeyCount; k++) {
                        if (key == RSBS::kComboKeys[k].key) {
                            seen[k] = true;
                            classified = true;
                            break;
                        }
                    }
                    if (!classified) {
                        printf("[TEST] FAIL: %s spells the tier-4 key \"%s\", which is NOT classified.\n"
                               "[TEST]       Every key in the combo's own namespace is world identity or preference\n"
                               "[TEST]       (ADR 0004 §6 state 4), decided at introduction. Add a row to\n"
                               "[TEST]       RSBS::kComboKeys in src/common/cvar_shared_keys.h saying which, and why.\n"
                               "[TEST]       An unclassified key defaults to identity; do not guess 'preference'.\n",
                               f.relPath.c_str(), key.c_str());
                        return TEST_FAIL;
                    }
                }
            }
            CVARCLASS_CHECK(literalsSeen > 0, "no tier-4 literal anywhere in the tree — the namespace scan is vacuous");
            for (std::size_t k = 0; k < RSBS::kComboKeyCount; k++) {
                if (!seen[k]) {
                    printf("[TEST] FAIL: RSBS::kComboKeys classifies \"%s\" but nothing in the tree spells it.\n"
                           "[TEST]       A manifest row nothing reads is a classification of nothing — check the\n"
                           "[TEST]       spelling, or retire the row with the key.\n",
                           RSBS::kComboKeys[k].key);
                    return TEST_FAIL;
                }
            }

            // The identity rule, at runtime as well as at compile time
            // (cvar_shared_keys.h static_asserts it): every key under the
            // identity sub-namespace is Identity, and there are exactly the
            // direction ADR 0011 increment 2 introduced (its pool sizes and
            // item classes were retired by #801 and #834), the goal, #886's
            // Shared Kokiri Sword, a BIT of the same frozen record and therefore
            // identity for the same reason, and #1068's Shared Shields, #1065's
            // Shared Lens of Truth, #1066's two shared songs, #1067's Shared
            // Ocarina Buttons, #1069's Shared Song of Time, #968's three shared
            // arrows, #970's Shared Bottles and #978's five shared masks, bits of
            // the sibling frozen item rules. #668's shared ocarina was a third
            // until #879 retired it.
            std::size_t identityKeys = 0;
            for (std::size_t k = 0; k < RSBS::kComboKeyCount; k++) {
                if (strncmp(RSBS::kComboKeys[k].key, RSBS::kComboIdentityKeyPrefix,
                            strlen(RSBS::kComboIdentityKeyPrefix)) == 0) {
                    CVARCLASS_CHECK(RSBS::kComboKeys[k].cls == RSBS::ComboKeyClass::Identity,
                                    "a key under the identity sub-namespace is classified as a preference — it "
                                    "authors the frozen ComboSettingsRecord and must be Identity");
                    identityKeys++;
                }
            }
            CVARCLASS_CHECK(identityKeys == 18,
                            "exactly eighteen tier-4 identity keys (direction — ADR 0011 increment 2, whose two pool "
                            "sizes #801 and two item classes #834 retired —, the goal, ADR 0010 D1, the Shared "
                            "Kokiri Sword, #886, the Shared Shields, #1068, the Shared Lens of Truth, #1065, Shared "
                            "Epona's Song and Song of Storms, #1066, the Shared Ocarina Buttons, #1067, the "
                            "Shared Song of Time, #1069, Shared Fire, Ice and Light Arrows, #968, the Shared Bottles, "
                            "#970, and the five shared masks, "
                            "#978; #879 retired #668's shared ocarina)");
            printf("[TEST]   tier-4 namespace: %zu literal(s) across %zu files, all %zu manifest rows spelled, "
                   "%zu identity\n",
                   literalsSeen, comboFiles.size(), RSBS::kComboKeyCount, identityKeys);

            // #1131 (lane L4): the Combo Tracker's first keys of its own, which
            // SoH has no twin for, are classified, and as preferences: whether
            // the window scrolls to, or shows only, the current area says
            // nothing about a world.
            const char* const kTrackerNavKeys[] = {
                "gCombo.Tracker.Checks.AutoScrollToCurrentArea",
                "gCombo.Tracker.Checks.OnlyShowCurrentArea",
            };
            for (const char* navKey : kTrackerNavKeys) {
                bool preference = false;
                for (std::size_t k = 0; k < RSBS::kComboKeyCount; k++) {
                    if (strcmp(RSBS::kComboKeys[k].key, navKey) == 0) {
                        preference = RSBS::kComboKeys[k].cls == RSBS::ComboKeyClass::Preference;
                    }
                }
                if (!preference) {
                    printf("[TEST] FAIL: #1131: \"%s\" is not classified a preference in RSBS::kComboKeys.\n", navKey);
                    return TEST_FAIL;
                }
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
