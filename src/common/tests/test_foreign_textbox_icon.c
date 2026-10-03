/**
 * @file test_foreign_textbox_icon.c
 * @brief Locks for a foreign item's icon inside MM's blue get-item textbox
 *        (#607, Tier 2b of #494).
 *
 * What is headless-lockable, and is locked here:
 *
 *  I1 REGISTRATION. OoT's source is registered, and it is the engine TU's own
 *     OoT_ComboTextboxIcon (pointer identity): a registrar the linker dropped
 *     (#516/#678 class) would leave every foreign pickup icon-less.
 *  I2 COVERAGE. EVERY OoT id the O8 classification owner calls PROGRESSION (the
 *     items that may cross onto an MM check) answers an icon, except exactly the
 *     three action-shuffle abilities OoT has no icon for, and the answer's
 *     path agrees with its declared shape (a 24x24 quest icon from
 *     icon_item_24_static, the note for a song, ...) and names an OoT archive
 *     path, never MM's `_yar` archives. Every answered icon also has an MM textbox
 *     branch, and every song note lands on the MM song drawn with exactly OoT's
 *     tint.
 *  I3 THE RIGHT ICON, not just an icon: named items pin their texture (a
 *     progressive shows its first tier, a small key shows OoT's small-key quest
 *     icon, a boss soul shows SoH's soul texture, Minuet's note is green).
 *  I4 FALLBACK. Unknown ids, an untagged item, and a source's half-answers (no
 *     texture, empty texture, no shape, an unknown shape) all answer 0 with the
 *     output zeroed, which is the pre-#607 icon-less textbox.
 *  I5 THE CHECK-LEVEL LOOKUP CheckQueue calls: an OoT item placed on an MM check
 *     resolves to the origin's texture and an MM textbox branch; with the
 *     texture's archive not mounted it falls back; a REAL MM check with no
 *     placement falls back (not RC_UNKNOWN, which a guard answers first). The
 *     mount test's PRODUCTION branch (override cleared) answers no for a path
 *     without the __OTR__ prefix and for an __OTR__ path no archive holds; its
 *     "yes" needs a mounted oot.o2r and is not driven headless.
 *  I6 THE TEXTBOX (games/mm/2s2h/Rando/ForeignTextboxIconSingleExe.cpp, run from
 *     here): the REAL CustomMessage load and the REAL vendored Message_DecodeHeader
 *     put the OoT texture in the icon segment, a consumed arm does not leak into
 *     the next message, native icons are untouched, and the REAL
 *     MM_Message_DrawItemIcon reads each shape at exactly its format and size.
 *     Delete the fenced hook in z_message.c and D1 goes red.
 *  I7 THE PRODUCTION WIRING: CheckQueue's REAL foreign give lambda. The row marks
 *     the host eligible, lets Rando::MiscBehavior::CheckQueue() queue its event,
 *     runs that event's giveItem, and asserts the Entry it leaves in
 *     activeCustomMessage carries the origin texture and MM item id, and that
 *     the OnOpenText load of that Entry plus the real decode puts the texture in
 *     the icon segment. Unmounted, the same lambda leaves the icon-less textbox.
 *     Build the Entry in CheckQueue without the two #607 fields and Q2 goes red.
 *     Q2 also compares the WHOLE sentence: "You found <article><name> (OoT)!"
 *     (#865).
 *  I8 THE WORDS (#865): every OoT item that may cross reads "You found
 *     <article><name> (OoT)!" through the production sentence builder, and MM's
 *     own line breaker keeps each on one page of the textbox.
 *
 * Not lockable headless: the pixels. Whether OoT's texture actually rasterizes in
 * MM's textbox is the playtest paragraph of the PR.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++);
 * every symbol it drives is C-linkage.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../foreign_textbox_icon.h"
#include "../game.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
int OoT_ComboTextboxIcon(uint16_t id, ComboTextboxIcon* out);
int OoT_ComboTextboxIcon_TestIdSpace(void);
int OoT_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
uint8_t MM_ForeignTextboxIcon_ItemIdForShape(const ComboTextboxIcon* icon);
int MM_ForeignTextboxIcon_TestNoteTintMatches(const ComboTextboxIcon* icon);
void MM_ForeignTextboxIcon_TestSetMountOverride(int value);
int MM_ForeignTextboxIcon_TestForCheck(uint16_t mmCheckId, const char** texture, uint8_t* textboxItemId);
uint16_t MM_ForeignTextboxIcon_TestSomeCheck(void);
uint16_t MM_ForeignTextboxIcon_TestOtherCheck(void);
int MM_ForeignTextboxIcon_TestTextureMountedReal(const char* texture);
int MM_ForeignTextboxIcon_TestCheckQueueGive(uint16_t mmCheckId, const char* wantTexture, uint8_t wantItemId);
int MM_ForeignTextboxIcon_RunHeadless(void);
int MM_ForeignTextboxIcon_TestPickupLayout(const char* article, const char* name, char* out, int cap, int* lines,
                                           int* pages);
}

#define FTI_ASSERT(cond, msg)                                                     \
    do {                                                                          \
        if (!(cond)) {                                                            \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);        \
            return TEST_FAIL;                                                     \
        }                                                                         \
    } while (0)

namespace {

SharedItem FtiItem(uint8_t origin, uint16_t id) {
    SharedItem item;
    item.originGame = origin;
    item.flags = 0;
    item.id = id;
    return item;
}

bool FtiZeroed(const ComboTextboxIcon& icon) {
    return icon.texture == nullptr && icon.shape == 0 && icon.r == 0 && icon.g == 0 && icon.b == 0;
}

bool FtiContains(const char* haystack, const char* needle) {
    return haystack != nullptr && std::strstr(haystack, needle) != nullptr;
}

/** The path a shape's texture must come from (OoT's archive layout). */
bool FtiPathFitsShape(const ComboTextboxIcon& icon) {
    switch (icon.shape) {
        case COMBO_TEXTBOX_ICON_ITEM:
            return FtiContains(icon.texture, "__OTR__textures/icon_item_static/gItemIcon") ||
                   FtiContains(icon.texture, "__OTR__textures/icon_item_static/gRocsFeatherTex") ||
                   FtiContains(icon.texture, "__OTR__textures/parameter_static/gBossSoul") ||
                   FtiContains(icon.texture, "__OTR__textures/parameter_static/gTriforcePiece");
        case COMBO_TEXTBOX_ICON_QUEST:
            return FtiContains(icon.texture, "__OTR__textures/icon_item_24_static/gQuestIcon");
        case COMBO_TEXTBOX_ICON_NOTE:
            return FtiContains(icon.texture, "/gSongNoteTex");
        case COMBO_TEXTBOX_ICON_RUPEE:
            return FtiContains(icon.texture, "/gRupeeCounterIconTex");
        default:
            return false;
    }
}

/** OoT progression items with NO icon anywhere in OoT: SoH's action-shuffle
 *  abilities, which its tracker draws over a blank button background. Exact: the
 *  walk requires each to fall back and counts them, so a new gap cannot hide here
 *  and an ability that gains an icon must leave the list. */
const char* const kFtiIconlessByDesign[] = { "Climb", "Crawl", "Open Chests" };

bool FtiIsIconlessByDesign(const char* name) {
    if (name == nullptr) {
        return false;
    }
    for (const char* listed : kFtiIconlessByDesign) {
        if (std::strcmp(listed, name) == 0) {
            return true;
        }
    }
    return false;
}

// ---- I4: synthetic half-answers ---------------------------------------------
int sFtiSyntheticMode = 0;
int FtiSyntheticSource(uint16_t id, ComboTextboxIcon* out) {
    (void)id;
    static const char kTex[] = "__OTR__textures/icon_item_static/gItemIconHookshotTex";
    switch (sFtiSyntheticMode) {
        case 0: // NULL texture
            *out = { nullptr, (uint8_t)COMBO_TEXTBOX_ICON_ITEM, 1, 2, 3 };
            return 1;
        case 1: // empty texture
            *out = { "", (uint8_t)COMBO_TEXTBOX_ICON_ITEM, 1, 2, 3 };
            return 1;
        case 2: // no shape
            *out = { kTex, (uint8_t)COMBO_TEXTBOX_ICON_NONE, 1, 2, 3 };
            return 1;
        case 3: // unknown shape
            *out = { kTex, (uint8_t)99, 1, 2, 3 };
            return 1;
        case 4: // says no but leaves junk behind
            *out = { kTex, (uint8_t)COMBO_TEXTBOX_ICON_ITEM, 1, 2, 3 };
            return 0;
        default: // a whole answer
            *out = { kTex, (uint8_t)COMBO_TEXTBOX_ICON_ITEM, 255, 255, 255 };
            return 1;
    }
}

TestResult FtiPin(const char* name, uint8_t shape, const char* textureTail, uint8_t r, uint8_t g, uint8_t b) {
    SharedItem item;
    if (!TestNamedItem((uint8_t)GAME_OOT, name, &item)) {
        printf("[TEST] FAIL: OoT has no item named \"%s\"\n", name);
        return TEST_FAIL;
    }
    ComboTextboxIcon icon;
    if (Combo_GetForeignItemTextboxIcon(item, &icon) != 1) {
        printf("[TEST] FAIL: \"%s\" (RG %u) has no textbox icon\n", name, (unsigned)item.id);
        return TEST_FAIL;
    }
    const size_t len = std::strlen(icon.texture);
    const size_t tail = std::strlen(textureTail);
    const bool tailOk = len >= tail && std::strcmp(icon.texture + len - tail, textureTail) == 0;
    if (icon.shape != shape || !tailOk || icon.r != r || icon.g != g || icon.b != b) {
        printf("[TEST] FAIL: \"%s\" -> shape %u \"%s\" (%u,%u,%u); want shape %u \"...%s\" (%u,%u,%u)\n", name,
               (unsigned)icon.shape, icon.texture, icon.r, icon.g, icon.b, (unsigned)shape, textureTail, r, g, b);
        return TEST_FAIL;
    }
    printf("[TEST]   pin: %-26s -> shape %u %s\n", name, (unsigned)icon.shape, icon.texture);
    return TEST_PASS;
}

} // namespace

TestResult Test_ForeignTextboxIcon(void) {
    printf("[TEST] ForeignTextboxIcon: a foreign item's icon in MM's get-item textbox (#607)\n");

    // ---- I1 --------------------------------------------------------------------
    FTI_ASSERT(Combo_GetTextboxIconSource((uint8_t)GAME_OOT) == OoT_ComboTextboxIcon,
               "I1 OoT's registered textbox-icon source is OoT_ComboTextboxIcon");
    FTI_ASSERT(Combo_GetTextboxIconSource((uint8_t)GAME_NONE) == nullptr, "I1 GAME_NONE has no source");
    FTI_ASSERT(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "I1 OoT's item table is available");

    // ---- I2 --------------------------------------------------------------------
    const int idSpace = OoT_ComboTextboxIcon_TestIdSpace();
    FTI_ASSERT(idSpace > 1 && idSpace < 0xFFFF, "I2 OoT id space");
    int progression = 0;
    int byShape[COMBO_TEXTBOX_ICON_SHAPE_COUNT] = {};
    int otherAnswered = 0;
    int iconless = 0;
    int failures = 0;
    for (int id = 1; id < idSpace; id++) {
        ComboItemClassRow row;
        const int classified = OoT_ComboLogic_ClassifyItem((uint16_t)id, &row);
        FTI_ASSERT(classified >= 0, "I2 OoT's classifier is ready");
        ComboTextboxIcon icon;
        const int answered = Combo_GetForeignItemTextboxIcon(FtiItem((uint8_t)GAME_OOT, (uint16_t)id), &icon);
        if (classified != 1 || row.fillClass != RSBS_FILL_CLASS_PROGRESSION) {
            otherAnswered += answered;
            continue;
        }
        progression++;
        const SharedItem item = FtiItem((uint8_t)GAME_OOT, (uint16_t)id);
        const char* name = Combo_GetForeignItemName(item);
        if (answered != 1) {
            if (FtiIsIconlessByDesign(name)) {
                iconless++;
                continue;
            }
            printf("[TEST]   no icon: RG %d \"%s\"\n", id, name != nullptr ? name : "?");
            failures++;
            continue;
        }
        if (FtiIsIconlessByDesign(name)) {
            printf("[TEST]   answers an icon but is listed as icon-less: RG %d \"%s\"\n", id, name);
            failures++;
        }
        byShape[icon.shape]++;
        if (!FtiPathFitsShape(icon) || FtiContains(icon.texture, "_yar")) {
            printf("[TEST]   shape/path disagree: RG %d \"%s\" shape %u %s\n", id, name != nullptr ? name : "?",
                   (unsigned)icon.shape, icon.texture);
            failures++;
        }
        if (MM_ForeignTextboxIcon_ItemIdForShape(&icon) == 0xFE) {
            printf("[TEST]   no MM textbox branch: RG %d \"%s\" shape %u\n", id, name != nullptr ? name : "?",
                   (unsigned)icon.shape);
            failures++;
        }
        if (MM_ForeignTextboxIcon_TestNoteTintMatches(&icon) != 1) {
            printf("[TEST]   note tint (%u,%u,%u) has no MM song: RG %d \"%s\"\n", icon.r, icon.g, icon.b, id,
                   name != nullptr ? name : "?");
            failures++;
        }
    }
    printf("[TEST]   %d OoT progression items: %d item, %d quest, %d note, %d rupee icons, %d icon-less by "
           "design; %d failures (%d non-progression ids also answer)\n",
           progression, byShape[COMBO_TEXTBOX_ICON_ITEM], byShape[COMBO_TEXTBOX_ICON_QUEST],
           byShape[COMBO_TEXTBOX_ICON_NOTE], byShape[COMBO_TEXTBOX_ICON_RUPEE], iconless, failures, otherAnswered);
    FTI_ASSERT(iconless == (int)(sizeof(kFtiIconlessByDesign) / sizeof(kFtiIconlessByDesign[0])),
               "I2 exactly the adjudicated icon-less abilities fall back (shrink the list when one gains an icon)");
    FTI_ASSERT(progression > 100, "I2 the walk saw OoT's progression items");
    FTI_ASSERT(failures == 0, "I2 every OoT progression item has a well-formed textbox icon with an MM branch");
    FTI_ASSERT(byShape[COMBO_TEXTBOX_ICON_ITEM] > 0 && byShape[COMBO_TEXTBOX_ICON_QUEST] > 0 &&
                   byShape[COMBO_TEXTBOX_ICON_NOTE] > 0,
               "I2 progression spans the item, quest and note layouts");

    // ---- I3 --------------------------------------------------------------------
    struct Pin {
        const char* name;
        uint8_t shape;
        const char* tail;
        uint8_t r, g, b;
    };
    const Pin pins[] = {
        { "Progressive Hookshot", COMBO_TEXTBOX_ICON_ITEM, "/gItemIconHookshotTex", 255, 255, 255 },
        { "Strength Upgrade", COMBO_TEXTBOX_ICON_ITEM, "/gItemIconGoronsBraceletTex", 255, 255, 255 },
        { "Bombchus (10)", COMBO_TEXTBOX_ICON_ITEM, "/gItemIconBombchuTex", 255, 255, 255 },
        { "Minuet of Forest", COMBO_TEXTBOX_ICON_NOTE, "/gSongNoteTex", 150, 255, 100 },
        { "Zelda's Lullaby", COMBO_TEXTBOX_ICON_NOTE, "/gSongNoteTex", 255, 255, 255 },
        { "Forest Medallion", COMBO_TEXTBOX_ICON_QUEST, "/gQuestIconMedallionForestTex", 255, 255, 255 },
        { "Piece of Heart", COMBO_TEXTBOX_ICON_QUEST, "/gQuestIconHeartPieceTex", 255, 255, 255 },
        { "Forest Temple Small Key", COMBO_TEXTBOX_ICON_QUEST, "/gQuestIconSmallKeyTex", 255, 255, 255 },
        { "Gohma's Soul", COMBO_TEXTBOX_ICON_ITEM, "/gBossSoul", 255, 255, 255 },
    };
    for (const Pin& pin : pins) {
        if (FtiPin(pin.name, pin.shape, pin.tail, pin.r, pin.g, pin.b) != TEST_PASS) {
            return TEST_FAIL;
        }
    }

    // ---- I4 --------------------------------------------------------------------
    {
        ComboTextboxIcon icon;
        const uint16_t unknown[] = { 0, (uint16_t)idSpace, 0xFFFF };
        for (uint16_t id : unknown) {
            icon.texture = "x";
            icon.shape = 1;
            FTI_ASSERT(Combo_GetForeignItemTextboxIcon(FtiItem((uint8_t)GAME_OOT, id), &icon) == 0 && FtiZeroed(icon),
                       "I4 an id OoT does not know falls back, output zeroed");
        }
        FTI_ASSERT(Combo_GetForeignItemTextboxIcon(FtiItem((uint8_t)GAME_NONE, 1), &icon) == 0 && FtiZeroed(icon),
                   "I4 an untagged item falls back");
        FTI_ASSERT(Combo_GetForeignItemTextboxIcon(FtiItem(7, 1), &icon) == 0 && FtiZeroed(icon),
                   "I4 a non-game tag falls back");
        FTI_ASSERT(Combo_GetForeignItemTextboxIcon(FtiItem((uint8_t)GAME_OOT, 1), nullptr) >= 0,
                   "I4 a null output is tolerated");

        // Half-answers, through a synthetic source borrowed into MM's slot.
        const ComboTextboxIconFn savedMm = Combo_GetTextboxIconSource((uint8_t)GAME_MM);
        Combo_RegisterTextboxIconSource((uint8_t)GAME_MM, FtiSyntheticSource);
        bool halfAnswersRefused = true;
        for (sFtiSyntheticMode = 0; sFtiSyntheticMode <= 4; sFtiSyntheticMode++) {
            if (Combo_GetForeignItemTextboxIcon(FtiItem((uint8_t)GAME_MM, 1), &icon) != 0 || !FtiZeroed(icon)) {
                printf("[TEST]   half-answer mode %d was accepted\n", sFtiSyntheticMode);
                halfAnswersRefused = false;
            }
        }
        sFtiSyntheticMode = 5;
        const bool wholeAccepted = Combo_GetForeignItemTextboxIcon(FtiItem((uint8_t)GAME_MM, 1), &icon) == 1 &&
                                   icon.shape == (uint8_t)COMBO_TEXTBOX_ICON_ITEM;
        Combo_RegisterTextboxIconSource((uint8_t)GAME_MM, savedMm);
        FTI_ASSERT(Combo_GetTextboxIconSource((uint8_t)GAME_MM) == savedMm, "I4 MM's slot restored");
        FTI_ASSERT(halfAnswersRefused, "I4 a source's half-answers fall back");
        FTI_ASSERT(wholeAccepted, "I4 a whole answer is passed through (the half-answer refusals are not vacuous)");
    }

    // ---- I5 --------------------------------------------------------------------
    {
        SharedItem hookshot;
        FTI_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Progressive Hookshot", &hookshot), "I5 named item");
        ComboTextboxIcon expected;
        FTI_ASSERT(Combo_GetForeignItemTextboxIcon(hookshot, &expected) == 1, "I5 origin answer");

        ComboForeignPlacement saved[RSBS_FOREIGN_PLACEMENT_CAP];
        std::memcpy(saved, gComboCtx.foreignPlacements, sizeof(saved));
        Combo_ClearForeignPlacements();
        const uint16_t check = MM_ForeignTextboxIcon_TestSomeCheck();
        const int placed = Combo_SetForeignPlacement(check, hookshot);

        const char* texture = nullptr;
        uint8_t itemId = 0;
        MM_ForeignTextboxIcon_TestSetMountOverride(1);
        const int mounted = MM_ForeignTextboxIcon_TestForCheck(check, &texture, &itemId);
        const bool mountedOk = mounted == 1 && texture == expected.texture &&
                               itemId == MM_ForeignTextboxIcon_ItemIdForShape(&expected) && itemId != 0xFE;
        MM_ForeignTextboxIcon_TestSetMountOverride(0);
        const int unmounted = MM_ForeignTextboxIcon_TestForCheck(check, &texture, &itemId);
        const bool unmountedOk = unmounted == 0 && texture == nullptr && itemId == 0xFE;
        MM_ForeignTextboxIcon_TestSetMountOverride(1);
        const uint16_t other = MM_ForeignTextboxIcon_TestOtherCheck();
        const int unplaced = MM_ForeignTextboxIcon_TestForCheck(other, &texture, &itemId);
        const bool unplacedOk = unplaced == 0 && texture == nullptr && itemId == 0xFE;
        // The same lookup with the SAME mount answer on the placed host says yes,
        // so the unplaced "no" is the placement table's, not the mount's.
        const int placedAgain = MM_ForeignTextboxIcon_TestForCheck(check, &texture, &itemId);

        // I7, while the placement stands: CheckQueue's real give lambda.
        const uint8_t wantItemId = MM_ForeignTextboxIcon_ItemIdForShape(&expected);
        const int giveMounted = MM_ForeignTextboxIcon_TestCheckQueueGive(check, expected.texture, wantItemId);
        MM_ForeignTextboxIcon_TestSetMountOverride(0);
        const int giveUnmounted = MM_ForeignTextboxIcon_TestCheckQueueGive(check, nullptr, 0xFE);
        MM_ForeignTextboxIcon_TestSetMountOverride(-1);

        // The production mount branch (no override): both answers are "no".
        const int realNoPrefix = MM_ForeignTextboxIcon_TestTextureMountedReal(
            "textures/icon_item_static/gItemIconHookshotTex");
        const int realMissing = MM_ForeignTextboxIcon_TestTextureMountedReal(
            "__OTR__textures/rsbs_no_such_object/gRsbsNoSuchTex");
        const int realNull = MM_ForeignTextboxIcon_TestTextureMountedReal(nullptr);

        std::memcpy(gComboCtx.foreignPlacements, saved, sizeof(saved));
        FTI_ASSERT(placed >= 0, "I5 placement accepted");
        FTI_ASSERT(mountedOk, "I5 a placed OoT item resolves to the origin texture and an MM textbox branch");
        FTI_ASSERT(unmountedOk, "I5 an unmounted origin archive falls back to the icon-less textbox");
        FTI_ASSERT(other != check && other != 0, "I5 the unplaced host is a real, different MM check");
        FTI_ASSERT(unplacedOk, "I5 a real MM check with no placement falls back");
        FTI_ASSERT(placedAgain == 1, "I5 the placed host still resolves under the same mount answer");
        FTI_ASSERT(realNoPrefix == 0, "I5 the production mount test refuses a path without the __OTR__ prefix");
        FTI_ASSERT(realMissing == 0, "I5 the production mount test refuses an __OTR__ path no archive holds");
        FTI_ASSERT(realNull == 0, "I5 the production mount test refuses a null path");
        FTI_ASSERT(giveMounted == 0,
                   "I7 CheckQueue's real give lambda hands the textbox the OoT icon (see the Q-line above)");
        FTI_ASSERT(giveUnmounted == 0,
                   "I7 unmounted, CheckQueue's real give lambda leaves the icon-less textbox (see the Q-line above)");
    }

    // ---- I8 (#865) ---------------------------------------------------------------
    // The textbox's words mark the item's game, for EVERY OoT item that may cross
    // onto an MM check, through the production sentence builder, and MM's own line
    // breaker keeps each marked sentence on one page. The names both games use
    // (the Song of Time, the Hookshot, the Lens of Truth, ...) are the reason: the
    // icon alone cannot tell the two Songs of Time apart (both are music notes).
    {
        int swept = 0;
        int shared = 0;
        int failures8 = 0;
        bool songOfTimeShared = false;
        std::string longestShared;
        std::string longestSharedText;
        int longestSharedLines = 0;
        int mostLines = 0;
        std::string mostLinesText;
        for (int id = 1; id < idSpace; id++) {
            ComboItemClassRow row;
            if (OoT_ComboLogic_ClassifyItem((uint16_t)id, &row) != 1 ||
                row.fillClass != RSBS_FILL_CLASS_PROGRESSION) {
                continue;
            }
            const SharedItem item = FtiItem((uint8_t)GAME_OOT, (uint16_t)id);
            const char* name = Combo_GetForeignItemName(item);
            const char* article = Combo_GetForeignItemArticle(item);
            if (name == nullptr || article == nullptr) {
                printf("[TEST]   I8 RG %d has no name or article\n", id);
                failures8++;
                continue;
            }
            char text[256];
            int lines = 0;
            int pages = 0;
            MM_ForeignTextboxIcon_TestPickupLayout(article, name, text, (int)sizeof(text), &lines, &pages);
            const std::string want = std::string("You found ") + article + name + " (OoT)!";
            if (want != text) {
                if (failures8 < 5) {
                    printf("[TEST]   I8 RG %d's textbox reads \"%s\", want \"%s\"\n", id, text, want.c_str());
                }
                failures8++;
                continue;
            }
            if (pages != 1) {
                printf("[TEST]   I8 RG %d's sentence \"%s\" runs to %d pages of MM's textbox\n", id, text, pages);
                failures8++;
            }
            swept++;
            if (lines > mostLines) {
                mostLines = lines;
                mostLinesText = text;
            }
            if (Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, name, nullptr)) {
                shared++;
                songOfTimeShared = songOfTimeShared || std::strcmp(name, "Song of Time") == 0;
                if (std::strlen(name) > longestShared.size()) {
                    longestShared = name;
                    longestSharedText = text;
                    longestSharedLines = lines;
                }
            }
        }
        printf("[TEST]   I8 %d OoT progression sentences marked \" (OoT)\", each on one page of MM's textbox (most "
               "lines: %d, \"%s\"); %d share a name with an MM item, the longest \"%s\": \"%s\" on %d line(s); %d "
               "failures\n",
               swept, mostLines, mostLinesText.c_str(), shared, longestShared.c_str(), longestSharedText.c_str(),
               longestSharedLines, failures8);
        FTI_ASSERT(failures8 == 0, "I8 every OoT item's MM textbox sentence is marked \" (OoT)\" and fits one page "
                                   "(see the I8 lines above)");
        FTI_ASSERT(swept > 100, "I8 the sweep saw OoT's progression items");
        FTI_ASSERT(songOfTimeShared && shared > 1, "I8 the shared-name set is real (the Song of Time is in it)");
    }

    // ---- I6 --------------------------------------------------------------------
    FTI_ASSERT(MM_ForeignTextboxIcon_RunHeadless() == 0,
               "I6 the real load + vendored decode + draw put the OoT icon in MM's textbox (see the D-line above)");

    printf("[TEST] ForeignTextboxIcon: PASS\n");
    return TEST_PASS;
}

#undef FTI_ASSERT
