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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "cvar_shared_keys.h"
#include "mm_hud_editor_elements.h" // W6-HUD-A: the HUD-editor family leg (3d-3)

// Defined in games/oot/soh/config/ConfigUpdaters.cpp. Declared here rather than
// included so redship_common does not take a header dependency on OoT's config
// layer — same approach as test_seq_map_bounds.c.
namespace SOH {
bool ShouldAdoptLegacyValue(bool legacyPresent, bool canonicalPresent);
int32_t VolumePercentFromFloatScale(float scale);
} // namespace SOH

// W6-COS-A (#1239): MM's colour table, read through its view in games/mm/2s2h/CosmeticGfxSingleExe.cpp (the colour
// family leg below).
extern "C" int MM_CosmeticElement_Count(void);
extern "C" int MM_CosmeticElement_At(int index, int* id, const char** mmColor, const char** mmChanged,
                                     const char** sohValue, const char** sohChanged, const char** sohFallbackValue,
                                     const char** sohFallbackChanged);

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
        { "gAudioEditor.", "CVAR_AUDIO(\"" },                        // #1239: SoH's Audio Editor rows
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

// ---- (3f-2c) helpers: every scalar MM-row define is bound and read (#1233) ----

/// @p text with every comment blanked (string and character literals kept, newlines kept), so a binding or a define
/// spelled in prose is never counted as code.
std::string CvarClassStripComments(const std::string& text) {
    std::string out = text;
    std::size_t i = 0;
    while (i < out.size()) {
        const char c = out[i];
        if (c == '"' || c == '\'') {
            for (i++; i < out.size() && out[i] != c && out[i] != '\n'; i++) {
                if (out[i] == '\\') {
                    i++;
                }
            }
            i++;
        } else if (c == '/' && i + 1 < out.size() && out[i + 1] == '/') {
            for (; i < out.size() && out[i] != '\n'; i++) {
                out[i] = ' ';
            }
        } else if (c == '/' && i + 1 < out.size() && out[i + 1] == '*') {
            const std::size_t end = out.find("*/", i + 2);
            const std::size_t stop = end == std::string::npos ? out.size() : end + 2;
            for (; i < stop; i++) {
                if (out[i] != '\n') {
                    out[i] = ' ';
                }
            }
        } else {
            i++;
        }
    }
    return out;
}

bool CvarClassIsIdentChar(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/// Does @p text hold @p ident as a whole identifier anywhere in [begin, end)?
bool CvarClassHasIdentIn(const std::string& text, const std::string& ident, std::size_t begin, std::size_t end) {
    for (std::size_t p = text.find(ident, begin); p != std::string::npos && p + ident.size() <= end;
         p = text.find(ident, p + 1)) {
        const bool leftOk = p == 0 || !CvarClassIsIdentChar(text[p - 1]);
        const bool rightOk = p + ident.size() >= text.size() || !CvarClassIsIdentChar(text[p + ident.size()]);
        if (leftOk && rightOk) {
            return true;
        }
    }
    return false;
}

/// How many shared-menu rows in @p code (comment-stripped) bind @p define: `.CVar(DEFINE)`, a SohMenu page row's
/// form, plus every `UIWidgets::CVar*(...)` call that passes DEFINE as an argument, a window row's form.
int CvarClassCountMenuBindings(const std::string& code, const std::string& define) {
    int count = 0;
    const std::string pageForm = ".CVar(" + define + ")";
    for (std::size_t p = code.find(pageForm); p != std::string::npos; p = code.find(pageForm, p + 1)) {
        count++;
    }
    const std::string windowCall = "UIWidgets::CVar";
    for (std::size_t p = code.find(windowCall); p != std::string::npos; p = code.find(windowCall, p + 1)) {
        std::size_t q = p + windowCall.size();
        while (q < code.size() && CvarClassIsIdentChar(code[q])) {
            q++;
        }
        if (q >= code.size() || code[q] != '(') {
            continue;
        }
        // The call's argument list, to its matching parenthesis (string literals skipped).
        int depth = 0;
        std::size_t close = std::string::npos;
        for (std::size_t r = q; r < code.size(); r++) {
            const char c = code[r];
            if (c == '"') {
                for (r++; r < code.size() && code[r] != '"'; r++) {
                    if (code[r] == '\\') {
                        r++;
                    }
                }
            } else if (c == '(') {
                depth++;
            } else if (c == ')' && --depth == 0) {
                close = r;
                break;
            }
        }
        if (close != std::string::npos && CvarClassHasIdentIn(code, define, q, close)) {
            count++;
        }
    }
    return count;
}

/// One `#define RSBS_CVAR_MM_<NAME> "<key>"`. @c key is empty when the define is not one string literal.
struct CvarClassMmDefine {
    std::string name;
    std::string key;
};

/// Every RSBS_CVAR_MM_* define in @p keysText (comments ignored) whose name does not end in _PREFIX: the scalar MM-row
/// keys. The _PREFIX family defines have their own legs (W6-HUD-A, W6-COS-A, W6-AUDIO-SEQ).
std::vector<CvarClassMmDefine> CvarClassScalarMmDefines(const std::string& keysText) {
    std::vector<CvarClassMmDefine> defines;
    const std::string code = CvarClassStripComments(keysText);
    const std::string marker = "#define RSBS_CVAR_MM_";
    for (std::size_t p = code.find(marker); p != std::string::npos; p = code.find(marker, p + 1)) {
        std::size_t nameEnd = p + std::strlen("#define ");
        while (nameEnd < code.size() && CvarClassIsIdentChar(code[nameEnd])) {
            nameEnd++;
        }
        CvarClassMmDefine d;
        d.name = code.substr(p + std::strlen("#define "), nameEnd - p - std::strlen("#define "));
        const std::string suffix = "_PREFIX";
        if (d.name.size() >= suffix.size() &&
            d.name.compare(d.name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            continue;
        }
        const std::size_t lineEnd = code.find('\n', nameEnd);
        std::string value = code.substr(nameEnd, lineEnd == std::string::npos ? std::string::npos : lineEnd - nameEnd);
        const std::size_t first = value.find_first_not_of(" \t\r");
        const std::size_t last = value.find_last_not_of(" \t\r");
        value = first == std::string::npos ? std::string() : value.substr(first, last - first + 1);
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"' &&
            value.find('"', 1) == value.size() - 1) {
            d.key = value.substr(1, value.size() - 2);
        }
        defines.push_back(d);
    }
    return defines;
}

/// The leg's verdicts, one line per breach: a scalar MM-row define that is not one literal, that is bound by no
/// shared-menu row or by more than one under games/oot/soh (@p ootMenuCode, comment-stripped production sources), or
/// whose key no file under games/mm spells as a literal (@p mmTree).
std::vector<std::string> CvarClassScalarMmDefineBreaches(const std::vector<CvarClassMmDefine>& defines,
                                                         const std::vector<std::string>& ootMenuCode,
                                                         const std::vector<std::string>& mmTree) {
    std::vector<std::string> breaches;
    for (const CvarClassMmDefine& d : defines) {
        if (d.key.empty()) {
            breaches.push_back(d.name + " is not one string literal, so no lock can follow it");
            continue;
        }
        int bindings = 0;
        for (const std::string& code : ootMenuCode) {
            bindings += CvarClassCountMenuBindings(code, d.name);
        }
        if (bindings != 1) {
            breaches.push_back(d.name + " (\"" + d.key + "\") is bound by " + std::to_string(bindings) +
                               " shared-menu rows under games/oot/soh, expected exactly one: .CVar(" + d.name +
                               ") on a page row, or a UIWidgets::CVar* call passing it in a window");
        }
        if (!CvarClassTreeContainsExactKey(mmTree, d.key.c_str())) {
            breaches.push_back(d.name + " (\"" + d.key +
                               "\"): no file under games/mm reads the key as a literal, so "
                               "its row would write a key nothing in Majora's Mask reads");
        }
    }
    return breaches;
}

// ---- (2b) helpers: the 2Ship import list (#1240, W6-IMPORT) ----

/// How many shared-menu rows in @p code (comment-stripped) bind the import-list entry spelled @p spelling: an
/// RSBS_CVAR_* define's name, bound as (3f-2c) binds it, or a quoted key literal, which SoH's own page rows bind
/// through the literal or a soh/cvar_prefixes.h macro (`.CVar(CVAR_CHEAT("NoClip"))`).
int CvarClassCountImportBindings(const std::string& code, const std::string& spelling) {
    if (spelling.size() < 2 || spelling.front() != '"' || spelling.back() != '"') {
        return CvarClassCountMenuBindings(code, spelling);
    }
    const std::string key = spelling.substr(1, spelling.size() - 2);
    std::vector<std::string> forms = { spelling };
    struct PrefixMacro {
        const char* prefix;
        const char* macro;
    };
    static const PrefixMacro kMacros[] = {
        { "gCheats.", "CVAR_CHEAT" },      { "gEnhancements.", "CVAR_ENHANCEMENT" },
        { "gSettings.", "CVAR_SETTING" },  { "gDeveloperTools.", "CVAR_DEVELOPER_TOOLS" },
        { "gAudioEditor.", "CVAR_AUDIO" },
    };
    for (const PrefixMacro& m : kMacros) {
        const std::size_t len = std::strlen(m.prefix);
        if (key.compare(0, len, m.prefix) == 0) {
            forms.push_back(std::string(m.macro) + "(\"" + key.substr(len) + "\")");
        }
    }
    int count = 0;
    for (const std::string& form : forms) {
        const std::string pageForm = ".CVar(" + form + ")";
        for (std::size_t p = code.find(pageForm); p != std::string::npos; p = code.find(pageForm, p + 1)) {
            count++;
        }
    }
    return count;
}

/// Is @p key on kTwoShipDoNotImport, by itself or under one of its families?
const RSBS::TwoShipDoNotImport* CvarClassTwoShipDoNotImportEntry(const std::string& key) {
    for (std::size_t i = 0; i < RSBS::kTwoShipDoNotImportCount; i++) {
        const RSBS::TwoShipDoNotImport& entry = RSBS::kTwoShipDoNotImport[i];
        if (entry.family ? key.compare(0, std::strlen(entry.key), entry.key) == 0 : key == entry.key) {
            return &entry;
        }
    }
    return nullptr;
}

/// The scalar MM-row defines in @p defines that are on neither the import list nor kTwoShipDoNotImport: a returning
/// 2Ship player's setting the import would silently drop.
std::vector<std::string> CvarClassTwoShipUnlisted(const std::vector<CvarClassMmDefine>& defines) {
    std::vector<std::string> unlisted;
    for (const CvarClassMmDefine& d : defines) {
        bool listed = CvarClassTwoShipDoNotImportEntry(d.key) != nullptr;
        for (std::size_t i = 0; !listed && i < RSBS::kTwoShipImportKeyCount; i++) {
            listed = d.key == RSBS::kTwoShipImportKeys[i];
        }
        if (!listed) {
            unlisted.push_back(d.name + " (\"" + d.key + "\")");
        }
    }
    return unlisted;
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
    // #1239: SoH's "Disable Navi Call Audio" (Audio Editor > Audio Options). MM's Tatl call mute
    // (Audio/Sfx/DisableTatlCallAudio.cpp) read gAudioEditor.DisableTatlCallAudio, which nothing writes. Both bool,
    // default off.
    { 1239, "gAudioEditor.DisableTatlCallAudio", "gAudioEditor.DisableNaviCallAudio" },
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
    // #1239: the Audio Editor's four other Sfx providers, each keyed on the key SoH's Audio Options block writes.
    { 1239, "gAudioEditor.DisableNaviCallAudio", "Audio/Sfx/DisableTatlCallAudio.cpp" },
    { 1239, "gAudioEditor.LinkVoiceFreqMultiplier", "Audio/Sfx/LinksVoicePitchMultiplier.cpp" },
    { 1239, "gAudioEditor.MuteCarpenterSfx", "Audio/Sfx/MuteCarpenterSfx.cpp" },
    { 1239, "gAudioEditor.ChildGoronCry", "Audio/Sfx/MuteCryingGoronChild.cpp" },
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
    // #1239: SoH's one voice-pitch float drives MM's .Enable/.Scale pair through one translator.
    { 1239, "gAudioEditor.LinkVoiceFreqMultiplier.Enable", "gAudioEditor.LinkVoiceFreqMultiplier",
      "2s2h/Enhancements/Audio/Sfx/LinksVoicePitchMultiplier.cpp" },
    { 1239, "gAudioEditor.LinkVoiceFreqMultiplier.Scale", "gAudioEditor.LinkVoiceFreqMultiplier",
      "2s2h/Enhancements/Audio/Sfx/LinksVoicePitchMultiplier.cpp" },
    // W6-AUDIO-SEQ (#1239): SoH's five-way randomize-audio combobox drives MM's two randomize-all bools.
    { 1239, "gAudioEditor.RandomizeAllOnNewScene", "gAudioEditor.RandomizeAudioGenModes",
      "2s2h/Enhancements/Audio/AudioEditorSingleExe.cpp" },
    { 1239, "gAudioEditor.RandomizeAllOnRandoGen", "gAudioEditor.RandomizeAudioGenModes",
      "2s2h/Enhancements/Audio/AudioEditorSingleExe.cpp" },
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

/// (3f-3b) W6-MODES-DIFF: does comment-stripped @p code WRITE @p key, i.e. pass it as a literal straight to a CVarSet*
/// or CVarClear call? A read (CVarGetInteger) is not a write.
bool CvarClassWritesKey(const std::string& code, const char* key) {
    const std::string literal = std::string("\"") + key + "\"";
    for (std::size_t p = code.find(literal); p != std::string::npos; p = code.find(literal, p + 1)) {
        std::size_t q = p;
        while (q > 0 && (code[q - 1] == ' ' || code[q - 1] == '\t' || code[q - 1] == '\n' || code[q - 1] == '\r')) {
            q--;
        }
        if (q == 0 || code[q - 1] != '(') {
            continue;
        }
        q--;
        while (q > 0 && (code[q - 1] == ' ' || code[q - 1] == '\t')) {
            q--;
        }
        std::size_t begin = q;
        while (begin > 0 && CvarClassIsIdentChar(code[begin - 1])) {
            begin--;
        }
        const std::string callee = code.substr(begin, q - begin);
        if (callee.rfind("CVarSet", 0) == 0 || callee == "CVarClear") {
            return true;
        }
    }
    return false;
}

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
    // (2b) W6-IMPORT (#1240): the 2Ship config import list.
    //
    // RSBS_TWOSHIP_IMPORT_KEYS is the compile-time list SOH::ImportTwoShipConfig adopts as-is; kTwoShipDoNotImport
    // holds the keys and families it must never carry. A key imported with no shared-menu row is stuck (#1240's
    // ordering rule), a do-not-import key copied raw is a hazard, a converged or translated MM spelling copied raw
    // bypasses its conversion, and a scalar MM-row define on neither list is a returning 2Ship player's setting
    // dropped without a word. TwoShipImportMmKeys drives the importer itself; this leg holds the lists.
    // ------------------------------------------------------------------
    {
        CVARCLASS_CHECK(RSBS::kTwoShipImportKeyCount > 0 && RSBS::kTwoShipDoNotImportCount > 0,
                        "(2b) the 2Ship import list or its do-not-import table is empty");
        for (std::size_t e = 0; e < RSBS::kTwoShipDoNotImportCount; e++) {
            const RSBS::TwoShipDoNotImport& entry = RSBS::kTwoShipDoNotImport[e];
            CVARCLASS_CHECK(entry.key != nullptr && entry.key[0] != '\0' && entry.why != nullptr &&
                                entry.why[0] != '\0',
                            "(2b) a kTwoShipDoNotImport entry has no key or no reason");
            CVARCLASS_CHECK(entry.family == (entry.key[strlen(entry.key) - 1] == '.'),
                            "(2b) a kTwoShipDoNotImport family must end in '.', and only a family may");
        }
        for (std::size_t i = 0; i < RSBS::kTwoShipImportKeyCount; i++) {
            const char* key = RSBS::kTwoShipImportKeys[i];
            const char* spelling = RSBS::kTwoShipImportKeySpellings[i];
            CVARCLASS_CHECK(key != nullptr && key[0] != '\0', "(2b) an import-list entry is empty");
            for (std::size_t j = i + 1; j < RSBS::kTwoShipImportKeyCount; j++) {
                if (strcmp(key, RSBS::kTwoShipImportKeys[j]) == 0) {
                    printf("[TEST] FAIL: (2b) \"%s\" is on the 2Ship import list twice (%s, %s).\n", key, spelling,
                           RSBS::kTwoShipImportKeySpellings[j]);
                    return TEST_FAIL;
                }
            }
            for (std::size_t e = 0; e < RSBS::kTwoShipDoNotImportCount; e++) {
                const RSBS::TwoShipDoNotImport& entry = RSBS::kTwoShipDoNotImport[e];
                const bool held =
                    entry.family ? strncmp(key, entry.key, strlen(entry.key)) == 0 : strcmp(key, entry.key) == 0;
                if (held) {
                    printf("[TEST] FAIL: (2b) %s (\"%s\") is on the 2Ship import list and on kTwoShipDoNotImport:\n"
                           "[TEST]       %s.\n",
                           spelling, key, entry.why);
                    return TEST_FAIL;
                }
            }
            for (std::size_t c = 0; c < RSBS::kConvergedKeyCount; c++) {
                if (strcmp(key, RSBS::kConvergedKeys[c].legacy) == 0) {
                    printf("[TEST] FAIL: (2b) %s (\"%s\") is a converged legacy spelling; the kConvergedKeys walk\n"
                           "[TEST]       already imports it onto \"%s\" (with its conversion), so do not list it.\n",
                           spelling, key, RSBS::kConvergedKeys[c].canonical);
                    return TEST_FAIL;
                }
            }
            for (std::size_t t = 0; t < RSBS::kTranslatedKeyCount; t++) {
                if (strcmp(key, RSBS::kTranslatedKeys[t].mmKey) == 0) {
                    printf("[TEST] FAIL: (2b) %s (\"%s\") is translated from SoH's \"%s\" (#%d); a raw import would\n"
                           "[TEST]       bypass the translation. Put it on kTwoShipDoNotImport instead.\n",
                           spelling, key, RSBS::kTranslatedKeys[t].ootKey, RSBS::kTranslatedKeys[t].issue);
                    return TEST_FAIL;
                }
            }
        }

        // Every key SoH's rows drive through a translation (kTranslatedKeys) is held back by name or family: a raw
        // copy of 2Ship's value would sit beside the translation with no row to clear it.
        for (std::size_t t = 0; t < RSBS::kTranslatedKeyCount; t++) {
            const char* mmKey = RSBS::kTranslatedKeys[t].mmKey;
            bool held = false;
            for (std::size_t e = 0; !held && e < RSBS::kTwoShipDoNotImportCount; e++) {
                const RSBS::TwoShipDoNotImport& entry = RSBS::kTwoShipDoNotImport[e];
                held = entry.family ? strncmp(mmKey, entry.key, strlen(entry.key)) == 0 : strcmp(mmKey, entry.key) == 0;
            }
            if (!held) {
                printf("[TEST] FAIL: (2b) MM's \"%s\" is translated from SoH's \"%s\" (#%d) but is not on\n"
                       "[TEST]       kTwoShipDoNotImport; add it there with its reason.\n",
                       mmKey, RSBS::kTranslatedKeys[t].ootKey, RSBS::kTranslatedKeys[t].issue);
                return TEST_FAIL;
            }
        }

#ifdef RSBS_SOURCE_DIR
        const std::filesystem::path importRoot(RSBS_SOURCE_DIR);
        bool sohOk = false;
        const std::vector<CvarClassSourceFile> sohFiles =
            CvarClassSlurpTreeWithPaths(importRoot / "games" / "oot" / "soh", sohOk);
        std::ifstream keysFile(importRoot / "src" / "common" / "cvar_shared_keys.h", std::ios::binary);
        if (sohOk && !sohFiles.empty() && keysFile.is_open()) {
            const std::string keysText((std::istreambuf_iterator<char>(keysFile)), std::istreambuf_iterator<char>());
            // Production menu code only, as (3f-2c) reads it: tests and the UI snapshot harness are not rows.
            std::vector<std::string> ootMenuCode;
            for (const CvarClassSourceFile& f : sohFiles) {
                if (f.relPath.find("_test.") == std::string::npos &&
                    f.relPath.find("soh_ui_snapshot.cpp") == std::string::npos) {
                    ootMenuCode.push_back(CvarClassStripComments(f.text));
                }
            }

            // The control, observed on every run: a synthetic define on neither list is reported, and a define on
            // the list or under a do-not-import family is not.
            {
                const std::string controlKeys = std::string("#define RSBS_CVAR_MM_RSBS_IMPORT_CONTROL "
                                                            "\"gEnhancements.RsbsImportControl\"\n") +
                                                "#define RSBS_CVAR_MM_RSBS_IMPORT_LISTED \"" +
                                                RSBS::kTwoShipImportKeys[0] + "\"\n" +
                                                "#define RSBS_CVAR_MM_RSBS_IMPORT_FAMILY \"gColors.RsbsImport\"\n";
                const std::vector<std::string> controlUnlisted =
                    CvarClassTwoShipUnlisted(CvarClassScalarMmDefines(controlKeys));
                CVARCLASS_CHECK(controlUnlisted.size() == 1 &&
                                    controlUnlisted[0].find("RSBS_CVAR_MM_RSBS_IMPORT_CONTROL") != std::string::npos,
                                "(2b) control: the completeness check did not report exactly the synthetic define "
                                "that is on neither list, so its green would prove nothing");
            }

            std::vector<std::string> breaches;
            for (std::size_t i = 0; i < RSBS::kTwoShipImportKeyCount; i++) {
                int bindings = 0;
                for (const std::string& code : ootMenuCode) {
                    bindings += CvarClassCountImportBindings(code, RSBS::kTwoShipImportKeySpellings[i]);
                }
                if (bindings == 0) {
                    breaches.push_back(std::string(RSBS::kTwoShipImportKeySpellings[i]) + " (\"" +
                                       RSBS::kTwoShipImportKeys[i] +
                                       "\") is on the 2Ship import list, but no shared-menu row under games/oot/soh "
                                       "binds it, so an imported value would be stuck");
                }
            }
            for (const std::string& u : CvarClassTwoShipUnlisted(CvarClassScalarMmDefines(keysText))) {
                breaches.push_back(u + " has a shared-menu row but is on neither RSBS_TWOSHIP_IMPORT_KEYS nor "
                                       "kTwoShipDoNotImport, so a returning 2Ship player's value for it is dropped");
            }
            for (const std::string& b : breaches) {
                printf("[TEST] FAIL: (2b) %s.\n", b.c_str());
            }
            if (!breaches.empty()) {
                printf("[TEST]       A scalar MM-row define goes on the 2Ship import list (cvar_shared_keys.h,\n"
                       "[TEST]       RSBS_TWOSHIP_IMPORT_KEYS) or, with its reason, on kTwoShipDoNotImport (#1240).\n");
                return TEST_FAIL;
            }
            printf("[TEST] (2b) %zu key(s) on the 2Ship import list, each bound by a shared-menu row; %zu "
                   "do-not-import entr(ies); every scalar MM-row define is on one of the two\n",
                   RSBS::kTwoShipImportKeyCount, RSBS::kTwoShipDoNotImportCount);
        }
#endif // RSBS_SOURCE_DIR
    }

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

        // (3c-3) W6-COS-A (#1239): MM's colour family. MM composes every colour key from one prefix, so the family has
        // one RSBS_CVAR_MM_COSMETIC_PREFIX define and no scalar define, and W6-HOST's generic leg (3f-2c) skips it.
        // This leg holds the family instead: every element of MM's colour table (the one CosmeticEditor_GetChangedColor
        // reads) is EITHER an MM-only element, whose own key is composed from the prefix and is an MM row's, OR an SoH
        // twin, whose pair is a kTranslatedKeys row of that translator (never a rename: SoH's child is .Value, MM's
        // .Color) and whose SoH key is an option SoH's Cosmetics Editor declares. Both directions: a cosmetics
        // translated row with no table element is a manifest entry nothing reads.
        {
            const std::string prefix = RSBS_CVAR_MM_COSMETIC_PREFIX;
            CVARCLASS_CHECK(prefix == "gCosmetic.",
                            "W6-COS-A: RSBS_CVAR_MM_COSMETIC_PREFIX is not MM's \"gCosmetic.\"");
            const int count = MM_CosmeticElement_Count();
            CVARCLASS_CHECK(count > 0,
                            "W6-COS-A: MM's colour table is empty, so CosmeticEditor_GetChangedColor changes "
                            "no colour and this leg would prove nothing");

            std::string sohEditor;
            {
                std::ifstream f(sourceRoot / "games" / "oot" / "soh" / "Enhancements" / "cosmetics" /
                                    "CosmeticsEditor.cpp",
                                std::ios::binary);
                sohEditor.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            }
            CVARCLASS_CHECK(!sohEditor.empty(), "W6-COS-A: could not read SoH's CosmeticsEditor.cpp");

            auto endsWith = [](const std::string& text, const char* suffix) {
                const std::size_t n = std::strlen(suffix);
                return text.size() > n && text.compare(text.size() - n, n, suffix) == 0;
            };
            // SoH's twin pair: "gCosmetics.<id>.Value" beside "gCosmetics.<id>.Changed", <id> an option SoH declares,
            // and the pair translated onto MM's key by this translator. Empty when it holds, else the breach.
            auto twinBreach = [&](const std::string& mmColor, const char* value, const char* changed) -> std::string {
                const std::string v = value;
                const std::string stem = v.size() > 6 ? v.substr(0, v.size() - 6) : std::string();
                if (v.rfind("gCosmetics.", 0) != 0 || !endsWith(v, ".Value") || changed == nullptr ||
                    stem + ".Changed" != changed) {
                    return "SoH twin \"" + v + "\" is not a gCosmetics.<option>.Value / .Changed pair";
                }
                const std::string option = "COSMETIC_OPTION(\"" + stem.substr(std::strlen("gCosmetics.")) + "\",";
                if (sohEditor.find(option) == std::string::npos) {
                    return "SoH twin \"" + v + "\" is not an option SoH's Cosmetics Editor declares";
                }
                bool translated = false;
                for (std::size_t i = 0; i < RSBS::kTranslatedKeyCount; i++) {
                    const RSBS::TranslatedKey& key = RSBS::kTranslatedKeys[i];
                    translated = translated || (mmColor == key.mmKey && v == key.ootKey &&
                                                strcmp(key.translator, RSBS::kCosmeticTranslator) == 0);
                }
                if (!translated) {
                    return "no kTranslatedKeys row translates SoH's \"" + v + "\" onto MM's \"" + mmColor + "\"";
                }
                for (std::size_t i = 0; i < RSBS::kConvergedKeyCount; i++) {
                    if (mmColor == RSBS::kConvergedKeys[i].legacy || v == RSBS::kConvergedKeys[i].canonical) {
                        return "\"" + mmColor + "\" is also a rename row; the importer would copy SoH's colour raw";
                    }
                }
                return std::string();
            };

            std::vector<std::string> breaches;
            std::vector<std::pair<std::string, std::string>> tablePairs;
            bool seenId[256] = {};
            for (int i = 0; i < count; i++) {
                int id = -1;
                const char* mmColor = nullptr;
                const char* mmChanged = nullptr;
                const char* sohValue = nullptr;
                const char* sohChanged = nullptr;
                const char* fallbackValue = nullptr;
                const char* fallbackChanged = nullptr;
                if (!MM_CosmeticElement_At(i, &id, &mmColor, &mmChanged, &sohValue, &sohChanged, &fallbackValue,
                                           &fallbackChanged) ||
                    id < 0 || id > 255 || mmColor == nullptr || mmChanged == nullptr) {
                    breaches.push_back("element " + std::to_string(i) + " has no id or no MM key");
                    continue;
                }
                const std::string color = mmColor;
                if (seenId[id]) {
                    breaches.push_back("element id " + std::to_string(id) + " is in the table twice");
                }
                seenId[id] = true;
                const std::string stem = endsWith(color, ".Color") ? color.substr(0, color.size() - 6) : color;
                if (color.rfind(prefix, 0) != 0 || !endsWith(color, ".Color") || stem + ".Changed" != mmChanged) {
                    breaches.push_back("\"" + color +
                                       "\" is not composed from RSBS_CVAR_MM_COSMETIC_PREFIX as "
                                       "<element>.Color / .Changed");
                }
                if (sohValue == nullptr) {
                    // MM-only: MM's own key decides, so it is no translated row, and games/oot never spells it whole
                    // (its MM row composes it from the prefix).
                    for (std::size_t k = 0; k < RSBS::kTranslatedKeyCount; k++) {
                        if (color == RSBS::kTranslatedKeys[k].mmKey) {
                            breaches.push_back("MM-only \"" + color + "\" is also a translated row");
                        }
                    }
                    if (CvarClassOotTreeNamesKey(ootTree, mmColor) || CvarClassOotTreeNamesKey(ootTree, mmChanged)) {
                        breaches.push_back("games/oot spells MM's \"" + color +
                                           "\" as a literal; bind it through "
                                           "RSBS_CVAR_MM_COSMETIC_PREFIX");
                    }
                    if (fallbackValue != nullptr) {
                        breaches.push_back("MM-only \"" + color + "\" has an SoH fallback twin");
                    }
                    continue;
                }
                std::string why = twinBreach(color, sohValue, sohChanged);
                if (why.empty()) {
                    tablePairs.push_back({ color, sohValue });
                } else {
                    breaches.push_back(why);
                }
                if (fallbackValue != nullptr) {
                    why = twinBreach(color, fallbackValue, fallbackChanged);
                    if (why.empty()) {
                        tablePairs.push_back({ color, fallbackValue });
                    } else {
                        breaches.push_back(why);
                    }
                }
            }
            for (std::size_t k = 0; k < RSBS::kTranslatedKeyCount; k++) {
                const RSBS::TranslatedKey& key = RSBS::kTranslatedKeys[k];
                if (strcmp(key.translator, RSBS::kCosmeticTranslator) != 0) {
                    continue;
                }
                bool inTable = false;
                for (const auto& pair : tablePairs) {
                    inTable = inTable || (pair.first == key.mmKey && pair.second == key.ootKey);
                }
                if (!inTable) {
                    breaches.push_back(std::string("the translated row \"") + key.ootKey + "\" -> \"" + key.mmKey +
                                       "\" has no element in MM's colour table, so nothing reads it");
                }
            }
            for (const std::string& b : breaches) {
                printf("[TEST] FAIL: W6-COS-A colour family: %s.\n", b.c_str());
            }
            CVARCLASS_CHECK(
                breaches.empty(),
                "W6-COS-A: MM's colour table and the manifest disagree (games/mm/2s2h/CosmeticGfxSingleExe.cpp, "
                "RSBS::kTranslatedKeys)");
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

        // (3d-3) W6-HUD-A (#1239): MM's HUD placement family. One define, RSBS_CVAR_MM_HUD_EDITOR_PREFIX, and one
        // table, RSBS_MM_HUD_EDITOR_ELEMENTS (src/common/mm_hud_editor_elements.h), from which the Majora's Mask
        // section of SoH's Cosmetics Editor composes every key it writes. (3f-2c) skips _PREFIX families; this holds
        // this one:
        //   - every table element's Mode, Position.X, Position.Y and Scale key, composed from the prefix, is exactly
        //     the key MM's reader reads for it (hudEditorElements, through MM_HudEditor_TableKey), so a row never
        //     writes a key the reader does not read;
        //   - every element MM's HUD draws name (a HUD_EDITOR_ELEMENT_* token outside BenGui, which owns the enum and
        //     2Ship's unlinked window) is in the table (W6-HUD-B, #1239, retired the pending list: every element is);
        //   - games/oot composes the keys from the prefix and never spells "gHudEditor." itself.
        {
            struct CvarClassHudRow {
                const char* id;
                const char* key;
            };
#define CVARCLASS_HUD_ROW(id, key, label, x, y, ws) { #id, key },
            static const CvarClassHudRow kHudRows[] = { RSBS_MM_HUD_EDITOR_ELEMENTS(CVARCLASS_HUD_ROW) };
#undef CVARCLASS_HUD_ROW
            struct CvarClassHudField {
                int which;
                const char* suffix;
            };
            static const CvarClassHudField kHudFields[] = {
                { RSBS_MM_HUD_KEY_MODE, ".Mode" },
                { RSBS_MM_HUD_KEY_POSITION_X, ".Position.X" },
                { RSBS_MM_HUD_KEY_POSITION_Y, ".Position.Y" },
                { RSBS_MM_HUD_KEY_SCALE, ".Scale" },
            };
            CVARCLASS_CHECK(std::strcmp(RSBS_CVAR_MM_HUD_EDITOR_PREFIX, "gHudEditor.") == 0,
                            "(3d-3) RSBS_CVAR_MM_HUD_EDITOR_PREFIX is not \"gHudEditor.\", the prefix MM's "
                            "HUD_EDITOR_ELEMENT composes (2s2h/BenGui/HudEditor.h)");
            const int hudRowCount = (int)(sizeof(kHudRows) / sizeof(kHudRows[0]));
            for (int r = 0; r < hudRowCount; r++) {
                for (const CvarClassHudField& field : kHudFields) {
                    const std::string composed =
                        std::string(RSBS_CVAR_MM_HUD_EDITOR_PREFIX) + kHudRows[r].key + field.suffix;
                    const char* reader = MM_HudEditor_TableKey(r, field.which);
                    if (reader == nullptr || composed != reader) {
                        printf("[TEST] FAIL: (3d-3) %s: the shared menu writes \"%s\", MM's reader reads %s%s%s.\n"
                               "[TEST]       The Majora's Mask HUD Placement row would move a key nothing in MM "
                               "reads. hudEditorElements must be\n"
                               "[TEST]       built from RSBS_MM_HUD_EDITOR_ELEMENTS (games/mm/2s2h/"
                               "mm_hudeditor_stubs.c).\n",
                               kHudRows[r].id, composed.c_str(), reader == nullptr ? "" : "\"",
                               reader == nullptr ? "nothing" : reader, reader == nullptr ? "" : "\"");
                        return TEST_FAIL;
                    }
                }
            }

            bool hudPathsOk = false;
            const std::vector<CvarClassSourceFile> hudFiles =
                CvarClassSlurpTreeWithPaths(sourceRoot / "games" / "mm", hudPathsOk);
            CVARCLASS_CHECK(hudPathsOk && !hudFiles.empty(), "(3d-3) could not re-scan games/mm with paths");
            const std::string token = "HUD_EDITOR_ELEMENT_";
            int drawSiteTokens = 0;
            for (const CvarClassSourceFile& f : hudFiles) {
                if (f.relPath.rfind("2s2h/BenGui/", 0) == 0) {
                    continue;
                }
                const std::string code = CvarClassStripComments(f.text);
                for (std::size_t p = code.find(token); p != std::string::npos; p = code.find(token, p + 1)) {
                    if (p > 0 && CvarClassIsIdentChar(code[p - 1])) {
                        continue;
                    }
                    std::size_t end = p + token.size();
                    while (end < code.size() && CvarClassIsIdentChar(code[end])) {
                        end++;
                    }
                    const std::string name = code.substr(p, end - p);
                    if (name.rfind("HUD_EDITOR_ELEMENT_MODE_", 0) == 0 || name == "HUD_EDITOR_ELEMENT_NONE" ||
                        name == "HUD_EDITOR_ELEMENT_MAX") {
                        continue;
                    }
                    bool known = false;
                    for (const CvarClassHudRow& row : kHudRows) {
                        known = known || name == row.id;
                    }
                    if (!known) {
                        printf("[TEST] FAIL: (3d-3) games/mm/%s draws the HUD element %s, which is neither in "
                               "RSBS_MM_HUD_EDITOR_ELEMENTS: its draw would ignore\n"
                               "[TEST]       the HUD Placement section. Add its row to "
                               "src/common/mm_hud_editor_elements.h.\n",
                               f.relPath.c_str(), name.c_str());
                        return TEST_FAIL;
                    }
                    drawSiteTokens++;
                }
            }
            CVARCLASS_CHECK(drawSiteTokens > 0, "(3d-3) found no HUD_EDITOR_ELEMENT_* draw site under games/mm: the "
                                                "draw-site check passed vacuously");

            CVARCLASS_CHECK(!CvarClassTreeContainsLiteral(ootTree, RSBS_CVAR_MM_HUD_EDITOR_PREFIX),
                            "(3d-3) games/oot spells \"gHudEditor.\" as a literal; compose MM's HUD placement keys "
                            "from RSBS_CVAR_MM_HUD_EDITOR_PREFIX");
            bool ootComposes = false;
            for (const std::string& text : ootTree) {
                ootComposes = ootComposes || text.find("RSBS_CVAR_MM_HUD_EDITOR_PREFIX") != std::string::npos;
            }
            CVARCLASS_CHECK(ootComposes, "(3d-3) nothing under games/oot composes a key from "
                                         "RSBS_CVAR_MM_HUD_EDITOR_PREFIX: the Majora's Mask HUD Placement section in "
                                         "SoH's Cosmetics Editor is missing, so no row writes MM's HUD placement keys");
            printf("[TEST] (3d-3) %d HUD-editor elements: each key composed from the prefix is the key MM's reader "
                   "reads, and all %d draw-site element names are in the table\n",
                   hudRowCount, drawSiteTokens);
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

        // (3f-1b) W6-AUDIO-SEQ (#1239): MM's sequence-editor key families never land on SoH's. Both editors compose a
        // replacement as <family><sfxKey>.value / .locked and a shuffle-pool exclusion as <family><sfxKey>, and eleven
        // sfxKeys name a sequence in both games (NA_BGM_BOSS, NA_BGM_MINI_BOSS, NA_BGM_ENEMY, ...), so one spelling
        // would let either game's tab set the other game's music. The colliding set is derived from the two sequence
        // tables, so a key either game adds is covered with no edit here. For each, MM's composed keys must differ
        // from SoH's and neither may nest under the other (LUS stores CVars as dot-nested JSON). MM's collection must
        // compose every key from the two _PREFIX defines (a CVAR_AUDIO("ReplacedSequences.") or CVAR_AUDIO("Excluded.")
        // spelling survives only on a #define line, the upstream branch), and games/oot never spells MM's namespace.
        {
            const std::string sohReplaced = "gAudioEditor.ReplacedSequences.";
            const std::string sohExcluded = "gAudioEditor.Excluded.";
            const std::string mmReplaced = RSBS_CVAR_MM_AUDIO_REPLACED_SEQUENCES_PREFIX;
            const std::string mmExcluded = RSBS_CVAR_MM_AUDIO_EXCLUDED_PREFIX;
            auto readFile = [](const std::filesystem::path& file) {
                std::ifstream in(file, std::ios::binary);
                return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            };
            // The second string literal of every SEQUENCE_MAP_ENTRY(...) row (the macro's own #define is skipped).
            auto sfxKeysOf = [](const std::string& text) {
                std::vector<std::string> keys;
                const std::string token = "SEQUENCE_MAP_ENTRY(";
                for (std::size_t pos = text.find(token); pos != std::string::npos; pos = text.find(token, pos + 1)) {
                    const std::size_t nl = text.rfind('\n', pos);
                    const std::size_t lineStart = nl == std::string::npos ? 0 : nl + 1;
                    if (text.substr(lineStart, pos - lineStart).find("#define") != std::string::npos) {
                        continue;
                    }
                    std::vector<std::string> literals;
                    std::size_t i = pos + token.size();
                    int depth = 1;
                    while (i < text.size() && depth > 0 && literals.size() < 2) {
                        const char c = text[i];
                        if (c == '(') {
                            depth++;
                        } else if (c == ')') {
                            depth--;
                        } else if (c == '"') {
                            std::string lit;
                            for (i++; i < text.size() && text[i] != '"'; i++) {
                                if (text[i] == '\\' && i + 1 < text.size()) {
                                    i++;
                                }
                                lit += text[i];
                            }
                            literals.push_back(lit);
                        }
                        i++;
                    }
                    if (literals.size() == 2) {
                        keys.push_back(literals[1]);
                    }
                }
                return keys;
            };
            // Is @p inner the same key as @p outer, or a JSON child of it?
            auto nests = [](const std::string& outer, const std::string& inner) {
                return inner == outer || (inner.size() > outer.size() && inner.compare(0, outer.size(), outer) == 0 &&
                                          inner[outer.size()] == '.');
            };

            CVARCLASS_CHECK(mmReplaced != sohReplaced && mmExcluded != sohExcluded && mmReplaced != mmExcluded &&
                                mmReplaced.back() == '.' && mmExcluded.back() == '.',
                            "W6-AUDIO-SEQ: MM's two sequence-editor _PREFIX defines must be distinct dotted families, "
                            "neither of them SoH's gAudioEditor.ReplacedSequences. / gAudioEditor.Excluded.");
            const std::string sohTable =
                readFile(sourceRoot / "games" / "oot" / "soh" / "Enhancements" / "audio" / "AudioCollection.cpp");
            const std::string mmCollection =
                readFile(sourceRoot / "games" / "mm" / "2s2h" / "Enhancements" / "Audio" / "AudioCollection.cpp");
            const std::vector<std::string> sohKeys = sfxKeysOf(sohTable);
            const std::vector<std::string> mmKeys = sfxKeysOf(mmCollection);
            std::vector<std::string> colliding;
            for (const std::string& k : mmKeys) {
                if (std::find(sohKeys.begin(), sohKeys.end(), k) != sohKeys.end()) {
                    colliding.push_back(k);
                }
            }
            // The control: the parse must find the three the issue names, so a parse that sees nothing is red.
            for (const char* named : { "NA_BGM_BOSS", "NA_BGM_MINI_BOSS", "NA_BGM_ENEMY" }) {
                if (std::find(colliding.begin(), colliding.end(), named) == colliding.end()) {
                    printf("[TEST] FAIL: W6-AUDIO-SEQ: %s is not in the sfxKey set both sequence tables share (%zu "
                           "found, SoH %zu rows, MM %zu rows): the table parse is broken, so this leg checks nothing\n",
                           named, colliding.size(), sohKeys.size(), mmKeys.size());
                    return TEST_FAIL;
                }
            }
            for (const std::string& k : colliding) {
                const std::pair<std::string, std::string> pairs[] = {
                    { sohReplaced + k + ".value", mmReplaced + k + ".value" },
                    { sohReplaced + k + ".locked", mmReplaced + k + ".locked" },
                    { sohExcluded + k, mmExcluded + k },
                };
                for (const auto& [soh, mm] : pairs) {
                    if (nests(soh, mm) || nests(mm, soh)) {
                        printf("[TEST] FAIL: W6-AUDIO-SEQ: MM's \"%s\" and SoH's \"%s\" are one stored key (or one "
                               "nests under the other), so one game's Audio Editor tab sets the other's music\n",
                               mm.c_str(), soh.c_str());
                        return TEST_FAIL;
                    }
                }
            }
            printf("[TEST] W6-AUDIO-SEQ: %zu sfxKeys name a sequence in both games; MM composes each under its own "
                   "families\n",
                   colliding.size());

            // MM's collection composes every key through the defines.
            CVARCLASS_CHECK(
                mmCollection.find("RSBS_CVAR_MM_AUDIO_REPLACED_SEQUENCES_PREFIX") != std::string::npos &&
                    mmCollection.find("RSBS_CVAR_MM_AUDIO_EXCLUDED_PREFIX") != std::string::npos,
                "W6-AUDIO-SEQ: MM's S2H::AudioCollection does not compose its replacement and exclusion "
                "keys from RSBS_CVAR_MM_AUDIO_REPLACED_SEQUENCES_PREFIX / RSBS_CVAR_MM_AUDIO_EXCLUDED_PREFIX, "
                "so MM's sequence tabs read and write SoH's keys");
            std::size_t lineStart = 0;
            while (lineStart < mmCollection.size()) {
                std::size_t lineEnd = mmCollection.find('\n', lineStart);
                if (lineEnd == std::string::npos) {
                    lineEnd = mmCollection.size();
                }
                const std::string line = mmCollection.substr(lineStart, lineEnd - lineStart);
                const bool shared = line.find("CVAR_AUDIO(\"ReplacedSequences.") != std::string::npos ||
                                    line.find("CVAR_AUDIO(\"Excluded.") != std::string::npos;
                if (shared && line.find("#define") == std::string::npos) {
                    printf("[TEST] FAIL: W6-AUDIO-SEQ: MM's AudioCollection.cpp still composes a key from SoH's "
                           "family: %s\n",
                           line.c_str());
                    return TEST_FAIL;
                }
                lineStart = lineEnd + 1;
            }
            CVARCLASS_CHECK(!CvarClassTreeContainsLiteral(ootTree, "gAudioEditor.MM."),
                            "W6-AUDIO-SEQ: games/oot spells MM's sequence-editor namespace as a literal; it reaches "
                            "MM's keys only through MM's editor seam");
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
            // #1239: SoH's Audio Editor window draws its Audio Options rows from its own TU.
            bool audioOk = false;
            const std::vector<std::string> sohAudioTree =
                CvarClassSlurpTree(sourceRoot / "games" / "oot" / "soh" / "Enhancements" / "audio", audioOk);
            CVARCLASS_CHECK(audioOk && !sohAudioTree.empty(),
                            "could not scan games/oot/soh/Enhancements/audio for the Audio Editor rows");
            sohMenuTree.insert(sohMenuTree.end(), sohAudioTree.begin(), sohAudioTree.end());

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
                    printf("[TEST] FAIL: #%d: no SoH menu row (SohGui, the input editor or the Audio Editor) writes "
                           "\"%s\".\n"
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

        // (3f-2c) #1233, (3f-2b) made generic: EVERY scalar RSBS_CVAR_MM_* define in cvar_shared_keys.h (one whose
        // name does not end in _PREFIX; the family defines have their own legs) is bound by exactly one shared-menu
        // row under games/oot/soh, passed straight to .CVar(...) (a page row, the hosted-row seam's form) or to a
        // UIWidgets::CVar* call (a window row), and its key is read under games/mm as a literal. A W6 lane commits its
        // define before its row, so this leg is that lane's RED lock; a second row on one key, or a define whose MM
        // reader is gone, is red here too. Tests and the UI snapshot harness are not menu rows and are not scanned.
        {
            bool sohOk = false;
            const std::vector<CvarClassSourceFile> sohFiles =
                CvarClassSlurpTreeWithPaths(sourceRoot / "games" / "oot" / "soh", sohOk);
            CVARCLASS_CHECK(sohOk && !sohFiles.empty(), "could not scan games/oot/soh for the MM-row bindings");
            std::vector<std::string> ootMenuCode;
            for (const CvarClassSourceFile& f : sohFiles) {
                const std::string& p = f.relPath;
                const bool isTest =
                    p.find("_test.") != std::string::npos || p.find("soh_ui_snapshot.cpp") != std::string::npos;
                if (!isTest) {
                    ootMenuCode.push_back(CvarClassStripComments(f.text));
                }
            }
            std::ifstream keysFile(sourceRoot / "src" / "common" / "cvar_shared_keys.h", std::ios::binary);
            const std::string keysText((std::istreambuf_iterator<char>(keysFile)), std::istreambuf_iterator<char>());

            // The control, observed on every run: a synthetic define no row binds and no MM file reads must be
            // reported (twice: no row, no reader), a define in a comment and a _PREFIX family define must not, and
            // a define a window row binds the UIWidgets way must count as bound. Without it a green leg could be a
            // scan that sees nothing.
            {
                const std::string controlKeys =
                    "#define RSBS_CVAR_MM_RSBS_CLASS_CONTROL \"gEnhancements.RsbsClassificationControl\"\n"
                    "#define RSBS_CVAR_MM_RSBS_CLASS_WINDOW \"gEnhancements.DifficultyOptions.DamageMultiplier\"\n"
                    "#define RSBS_CVAR_MM_RSBS_CLASS_FAMILY_PREFIX \"gEnhancements.RsbsFamily.\"\n"
                    "// #define RSBS_CVAR_MM_RSBS_CLASS_COMMENTED \"gEnhancements.RsbsCommented\"\n";
                const std::vector<std::string> controlMenu = { CvarClassStripComments(
                    "void Draw() {\n"
                    "    UIWidgets::CVarCheckbox(\"Window Row\", RSBS_CVAR_MM_RSBS_CLASS_WINDOW,\n"
                    "                            UIWidgets::CheckboxOptions().Tooltip(\"x (y).\"));\n"
                    "    // menu.AddWidget(path, \"Prose\", "
                    "WIDGET_CVAR_CHECKBOX).CVar(RSBS_CVAR_MM_RSBS_CLASS_CONTROL);\n"
                    "}\n") };
                const std::vector<CvarClassMmDefine> controlDefines = CvarClassScalarMmDefines(controlKeys);
                CVARCLASS_CHECK(controlDefines.size() == 2,
                                "(3f-2c) control: the define scan did not find exactly the two scalar synthetic "
                                "defines (a _PREFIX family or a commented define was counted, or a real one missed)");
                const std::vector<std::string> controlBreaches =
                    CvarClassScalarMmDefineBreaches(controlDefines, controlMenu, mmTree);
                bool namesControl = controlBreaches.size() == 2;
                for (const std::string& b : controlBreaches) {
                    namesControl = namesControl && b.find("RSBS_CVAR_MM_RSBS_CLASS_CONTROL") != std::string::npos;
                }
                CVARCLASS_CHECK(namesControl, "(3f-2c) control: the synthetic define with no row and no MM reader was "
                                              "not reported exactly twice (or the window-bound one was reported), so "
                                              "this leg's green would prove nothing");
            }

            const std::vector<CvarClassMmDefine> defines = CvarClassScalarMmDefines(keysText);
            CVARCLASS_CHECK(!defines.empty(), "(3f-2c) found no scalar RSBS_CVAR_MM_* define in cvar_shared_keys.h "
                                              "(RSBS_CVAR_MM_DAMAGE_MULTIPLIER is one), so the scan would be vacuous");
            const std::vector<std::string> breaches = CvarClassScalarMmDefineBreaches(defines, ootMenuCode, mmTree);
            for (const std::string& b : breaches) {
                printf("[TEST] FAIL: (3f-2c) %s.\n"
                       "[TEST]       An MM-row define is committed with its row (W6 rule 7a): bind it once, through\n"
                       "[TEST]       the hosted-row seam or a UIWidgets::CVar* window call, in the same change.\n",
                       b.c_str());
            }
            if (!breaches.empty()) {
                return TEST_FAIL;
            }
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

        // (3f-3b) W6-MODES-DIFF (#1236, #1240): Mirrored World's translation. MM acts on gModes.MirroredWorld.State
        // (z_view.c, the GameInteractor's X inversion, z_kankyo.c, z_map_disp.c, synthesis.c and some twenty more read
        // sites), which only 2Ship's elided BenMenu callback ever wrote. SoH's gEnhancements.MirroredWorldMode is the
        // one control: the row is pinned in RSBS::kTranslatedKeys, SoH's menu writes the mode, the translator reads it,
        // is carved into the registrars list and is the ONLY MM writer of .State, and nothing names 2Ship's .Mode.
        // Unlike (3f-4)'s rows, MM's readers keep reading .State (the translator writes it on each MM scene change
        // instead of being called at each read), so this row is not in kCvarClassTranslatedRows.
        {
            constexpr const char* kStateKey = "gModes.MirroredWorld.State";
            constexpr const char* kModeKey = "gModes.MirroredWorld.Mode";
            constexpr const char* kSohModeKey = "gEnhancements.MirroredWorldMode";
            constexpr const char* kTranslatorTu = "2s2h/Enhancements/Modes/MirroredWorld.cpp";

            // The control: the writer scan must see a set and a clear, and must not take a read for a write.
            CVARCLASS_CHECK(CvarClassWritesKey("CVarSetInteger(\"gModes.MirroredWorld.State\", 1);", kStateKey) &&
                                CvarClassWritesKey("CVarClear( \"gModes.MirroredWorld.State\" );", kStateKey) &&
                                !CvarClassWritesKey("CVarGetInteger(\"gModes.MirroredWorld.State\", 0)", kStateKey),
                            "(3f-3b) control: the .State writer scan missed a set or a clear, or took a read for a "
                            "write, so its green would prove nothing");

            bool pinned = false;
            for (std::size_t i = 0; i < RSBS::kTranslatedKeyCount; i++) {
                const RSBS::TranslatedKey& key = RSBS::kTranslatedKeys[i];
                pinned = pinned || (key.issue == 1236 && strcmp(key.mmKey, kStateKey) == 0 &&
                                    strcmp(key.ootKey, kSohModeKey) == 0 && strcmp(key.translator, kTranslatorTu) == 0);
            }
            if (!pinned) {
                printf("[TEST] FAIL: (3f-3b) #1236: RSBS::kTranslatedKeys has no row translating SoH's \"%s\" onto\n"
                       "[TEST]       MM's \"%s\" in games/mm/%s. Without it nothing records that SoH's Mirrored\n"
                       "[TEST]       World row drives Majora's Mask.\n",
                       kSohModeKey, kStateKey, kTranslatorTu);
                return TEST_FAIL;
            }
            for (std::size_t i = 0; i < RSBS::kConvergedKeyCount; i++) {
                const RSBS::ConvergedKey& key = RSBS::kConvergedKeys[i];
                CVARCLASS_CHECK(strcmp(key.legacy, kStateKey) != 0 && strcmp(key.legacy, kModeKey) != 0 &&
                                    strcmp(key.canonical, kSohModeKey) != 0,
                                "(3f-3b) #1236: Mirrored World is also a rename row; SoH's nine-mode index is not "
                                "MM's bool");
            }
            for (std::size_t i = 0; i < RSBS::kMustStayDistinctCount; i++) {
                CVARCLASS_CHECK(strcmp(RSBS::kMustStayDistinct[i].mmKey, kStateKey) != 0 &&
                                    strcmp(RSBS::kMustStayDistinct[i].mmKey, kModeKey) != 0,
                                "(3f-3b) #1236: Mirrored World is also held apart; SoH's row is the one control");
            }

            bool menuOk = false;
            const std::vector<std::string> sohMenuTree =
                CvarClassSlurpTree(sourceRoot / "games" / "oot" / "soh" / "SohGui", menuOk);
            CVARCLASS_CHECK(menuOk && CvarClassOotTreeNamesKey(sohMenuTree, kSohModeKey),
                            "(3f-3b) #1236: no SoH menu row writes gEnhancements.MirroredWorldMode, the one control");

            std::ifstream cmakeFile(sourceRoot / "games" / "mm" / "CMakeLists.txt", std::ios::binary);
            const std::string cmake((std::istreambuf_iterator<char>(cmakeFile)), std::istreambuf_iterator<char>());
            const std::size_t listBegin = cmake.find("set(ship__Enh_registrars");
            const std::size_t listEnd = cmake.find("list(REMOVE_ITEM ship__Enhancements ${ship__Enh_registrars})");
            const bool carved =
                listBegin != std::string::npos && listEnd != std::string::npos &&
                cmake.substr(listBegin, listEnd - listBegin).find(std::string("\"") + kTranslatorTu + "\"") !=
                    std::string::npos;

            bool mmPathsOk = false;
            const std::vector<CvarClassSourceFile> mmFiles =
                CvarClassSlurpTreeWithPaths(sourceRoot / "games" / "mm", mmPathsOk);
            CVARCLASS_CHECK(mmPathsOk && !mmFiles.empty(), "(3f-3b) could not re-scan games/mm for Mirrored World");
            bool translatorReads = false;
            bool translatorWrites = false;
            for (const CvarClassSourceFile& f : mmFiles) {
                // The elided BenMenu, and the ROM-free rows, which set and clear .State only to drive and observe.
                bool elided = f.relPath.find("_test.") != std::string::npos;
                for (const char* ok : kCvarClassTranslatedElidedWriters) {
                    elided = elided || f.relPath == ok;
                }
                if (elided) {
                    continue;
                }
                const std::string code = CvarClassStripComments(f.text);
                if (f.relPath == kTranslatorTu) {
                    translatorReads = code.find(std::string("\"") + kSohModeKey + "\"") != std::string::npos;
                    translatorWrites = CvarClassWritesKey(code, kStateKey);
                    continue;
                }
                if (CvarClassWritesKey(code, kStateKey) ||
                    code.find(std::string("\"") + kModeKey + "\"") != std::string::npos) {
                    printf("[TEST] FAIL: (3f-3b) #1236: games/mm/%s writes \"%s\" or names \"%s\".\n"
                           "[TEST]       SoH's Mirrored World row is the one control: only games/mm/%s sets\n"
                           "[TEST]       .State, and nothing writes 2Ship's .Mode.\n",
                           f.relPath.c_str(), kStateKey, kModeKey, kTranslatorTu);
                    return TEST_FAIL;
                }
            }
            if (!translatorReads || !translatorWrites || !carved) {
                printf("[TEST] FAIL: (3f-3b) #1236: games/mm/%s %s.\n"
                       "[TEST]       MM acts on \"%s\", and nothing else in the single exe writes it, so SoH's\n"
                       "[TEST]       Mirrored World row would do nothing in Majora's Mask.\n",
                       kTranslatorTu,
                       !translatorReads    ? "does not read SoH's gEnhancements.MirroredWorldMode"
                       : !translatorWrites ? "does not set gModes.MirroredWorld.State"
                                           : "is not in games/mm/CMakeLists.txt's ship__Enh_registrars list",
                       kStateKey);
                return TEST_FAIL;
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
            std::vector<std::string> sohMenuTree =
                CvarClassSlurpTree(sourceRoot / "games" / "oot" / "soh" / "SohGui", menuOk);
            CVARCLASS_CHECK(menuOk && !sohMenuTree.empty(), "could not scan games/oot/soh/SohGui for the menu rows");
            // #1239: the voice-pitch slider is an Audio Editor row, drawn from the editor's own TU.
            bool audioOk = false;
            const std::vector<std::string> sohAudioTree =
                CvarClassSlurpTree(sourceRoot / "games" / "oot" / "soh" / "Enhancements" / "audio", audioOk);
            CVARCLASS_CHECK(audioOk && !sohAudioTree.empty(),
                            "could not scan games/oot/soh/Enhancements/audio for the Audio Editor rows");
            sohMenuTree.insert(sohMenuTree.end(), sohAudioTree.begin(), sohAudioTree.end());

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
                    printf("[TEST] FAIL: #%d: no SoH menu row (SohGui or the Audio Editor) writes \"%s\".\n", row.issue,
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
                    // #1141: a RETIRED key (RSBS::kComboRetiredKeys) is spelled
                    // for its one-shot carry only; it is not a live key, and the
                    // checks after this scan hold that it is not classified. A
                    // retired colour's stored name ("<key>.Value") is the same key.
                    bool retired = false;
                    for (std::size_t k = 0; k < RSBS::kComboRetiredKeyCount; k++) {
                        const RSBS::ComboRetiredKey& r = RSBS::kComboRetiredKeys[k];
                        retired = retired || key == r.key || key == std::string(r.key) + r.stored;
                    }
                    if (retired) {
                        continue;
                    }
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

            // #1141 (lane IT-3): the Item Tracker overlay's five twins of SoH's
            // Item Tracker Settings retired (ruling IT-D6: one set of settings,
            // SoH's). Each is out of the manifest, carried to SoH's key of the
            // same name, and the manifest holds no other gCombo.Tracker.Items.*
            // twin; the overlay's keys SoH has no twin for stay, and MM's eight
            // per-group visibility keys are classified, as preferences.
            {
                const char* const kRetiredTwins[] = {
                    "gCombo.Tracker.Items.WindowType",     "gCombo.Tracker.Items.Draggable",
                    "gCombo.Tracker.Items.ShowOnlyPaused", "gCombo.Tracker.Items.BgColor",
                    "gCombo.Tracker.Items.IconSize",
                };
                CVARCLASS_CHECK(RSBS::kComboRetiredKeyCount == 5, "#1141: the retired table is not the five twins");
                for (const char* twin : kRetiredTwins) {
                    bool inTable = false;
                    for (std::size_t r = 0; r < RSBS::kComboRetiredKeyCount; r++) {
                        if (strcmp(RSBS::kComboRetiredKeys[r].key, twin) == 0) {
                            inTable = true;
                            const std::string suffix = std::string(twin).substr(strlen("gCombo.Tracker.Items."));
                            CVARCLASS_CHECK(std::string(RSBS::kComboRetiredKeys[r].carriedTo) ==
                                                "gTrackers.ItemTracker." + suffix,
                                            "#1141: a retired twin carries to a key that is not SoH's twin");
                        }
                    }
                    CVARCLASS_CHECK(inTable, "#1141: a twin of SoH's Item Tracker Settings is not in the retired "
                                             "table");
                    for (std::size_t k = 0; k < RSBS::kComboKeyCount; k++) {
                        if (strcmp(RSBS::kComboKeys[k].key, twin) == 0) {
                            printf("[TEST] FAIL: #1141: \"%s\" is still classified in RSBS::kComboKeys; SoH's "
                                   "gTrackers.ItemTracker.* key holds the setting now.\n",
                                   twin);
                            return TEST_FAIL;
                        }
                    }
                }
                for (std::size_t r = 0; r < RSBS::kComboRetiredKeyCount; r++) {
                    for (std::size_t k = 0; k < RSBS::kComboKeyCount; k++) {
                        CVARCLASS_CHECK(strcmp(RSBS::kComboKeys[k].key, RSBS::kComboRetiredKeys[r].key) != 0,
                                        "#1141: a retired key is still classified as a live one");
                    }
                }
                const char* const kItemTrackerKeys[] = {
                    "gCombo.Tracker.Items.Opacity",       "gCombo.Tracker.Items.Section.OoT",
                    "gCombo.Tracker.Items.Section.MM",    "gCombo.Tracker.Items.Section.Shared",
                    "gCombo.Tracker.Items.MM.Tokens",     "gCombo.Tracker.Items.MM.StrayFairies",
                    "gCombo.Tracker.Items.MM.Dungeon",    "gCombo.Tracker.Items.MM.BossSouls",
                    "gCombo.Tracker.Items.MM.EnemySouls", "gCombo.Tracker.Items.MM.OwlStatues",
                    "gCombo.Tracker.Items.MM.Time",       "gCombo.Tracker.Items.MM.Misc",
                };
                std::size_t itemKeys = 0;
                for (std::size_t k = 0; k < RSBS::kComboKeyCount; k++) {
                    itemKeys += RSBS::ComboKeyHasPrefix(RSBS::kComboKeys[k].key, "gCombo.Tracker.Items.") ? 1 : 0;
                }
                for (const char* itemKey : kItemTrackerKeys) {
                    bool preference = false;
                    for (std::size_t k = 0; k < RSBS::kComboKeyCount; k++) {
                        if (strcmp(RSBS::kComboKeys[k].key, itemKey) == 0) {
                            preference = RSBS::kComboKeys[k].cls == RSBS::ComboKeyClass::Preference;
                        }
                    }
                    if (!preference) {
                        printf("[TEST] FAIL: #1141: \"%s\" is not classified a preference in RSBS::kComboKeys.\n",
                               itemKey);
                        return TEST_FAIL;
                    }
                }
                printf("[TEST]   #1141: %zu gCombo.Tracker.Items.* keys (four overlay keys + eight MM groups), %zu "
                       "retired twins\n",
                       itemKeys, RSBS::kComboRetiredKeyCount);
                CVARCLASS_CHECK(itemKeys == 12, "#1141: the manifest's gCombo.Tracker.Items.* keys are not the four "
                                                "overlay keys and the eight MM group keys");
            }

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
