/**
 * @file test_foreign_model.c
 * @brief Locks for the foreign model descriptor registry (#577 M2).
 *
 * ROM-free: the sources answer from static tables (resource PATHS, never loaded
 * resources), and the collision table is checked against the asset trees in the
 * source checkout (RSBS_SOURCE_DIR).
 *
 *  M1 REGISTRATION. Each game's source and host-native mapper is registered and
 *     is that game's own function (pointer identity): a registrar the linker
 *     dropped (#516/#678 class) would leave every foreign item model-less.
 *  M2 EVERY DRAW ROW of both games' get-item tables classifies: a well-formed
 *     descriptor built from exactly that row's lists (each part is a list of the
 *     row, each list of the row is a part), or no model with a named reason, and
 *     the no-model rows are exactly the adjudicated ones (by count). Each
 *     descriptor then classifies for the other game as DESCRIPTOR (no colliding
 *     directory) or, colliding, as the host-native answer or "no model".
 *  M3 THE COLLISION TABLE is exactly the intersection of both games' object
 *     directories (vanilla + custom) in the checkout: a name in the table the
 *     trees do not share, or a shared name missing from it, fails.
 *  M4 EVERY PROGRESSION ITEM of both games classifies for the other game, and
 *     every DESCRIPTOR answer round-trips: the registry hands the host exactly
 *     the model the origin's source answered.
 *  M5 THE RIGHT MODEL, not just a model: named items pin their answer.
 *  M6 HALF-ANSWERS (a synthetic source) never reach a host.
 *  M7 HOST-NATIVE: a colliding model answers the host mapper's key; a mapper
 *     that declines, or none, is "no model"; a non-colliding model never asks.
 *  M11 THE HOST-NATIVE TABLES (#577 M7; run right after M3, whose collision
 *     table they rest on). Every draw row of each game whose model touches a
 *     directory both archives carry answers, in the other game, EITHER a
 *     host-native model (a row of the host's own draw table that the host's own
 *     recipe draws, and the registry hands out as HOST_NATIVE with that key) OR
 *     "no model" from an explicit table row that names its reason. A colliding
 *     row the table does not name, a table row no colliding model uses, and a
 *     mapping onto a host row the host cannot draw all fail. Every item id of
 *     both games is walked the same way, so items that reach a row through a
 *     SoH custom draw's fallback are covered too. #830: each of OoT's thirteen
 *     song-note rows and twelve song items answers a drawable MM note (a host
 *     key may name a host-side recipe as well as a draw row), and the seven
 *     tinted notes are seven table rows keyed by their tint.
 *  M8 FALLBACKS: native, untagged, non-game and unknown ids answer no model.
 *  M9 ADR 0002: foreign_model.{h,c} include no game header.
 *  M10 THE FIRST CONSUMER (#577 M3): CheckQueue's REAL foreign give-and-draw in
 *     MM. An OoT item placed on an MM check is queued by CheckQueue, given, and
 *     then drawn into a real MM GraphicsContext; the emitted OPA and XLU lists
 *     are read back and must carry exactly the origin's descriptor (its lists in
 *     order on their layers, its setup list, a matrix before the first list,
 *     its scroll segments and colours). With the archive not mounted, and for a
 *     colliding item with no host-native row, the same draw emits no model list
 *     (the model-less stand-in). Put Rando::DrawItem(RI_NONE) back in the draw,
 *     or let the give overwrite the check id the draw reads, and M10 goes red.
 *     #577 M7: a colliding OoT item with a host-native row (the hookshot) draws
 *     MM's OWN model for that row, exactly the lists MM's recipe gives it. #830:
 *     Song of Time draws MM's host-side tinted note, one XLU list and its env
 *     colour; a foreign item with no model (Gohma's Soul) keeps the stand-in.
 *
 * Not lockable headless: the pixels. The M3 playtest shows the drawn model; M4
 * draws these descriptors in OoT.
 *
 *  M12 OoT AS THE HOST (#800 S1): the model call the OoT shop shelf makes
 *     (OoT_ForeignModel_DrawForOoTCheck, what OoT_EnGirlA_Draw calls; the actor
 *     itself is not run here) for an OoT check hosting an MM item, into a real
 *     OoT GraphicsContext: a mounted MM-exclusive model (the Deku Mask) emits
 *     exactly MM's descriptor lists with a matrix first; a colliding MM model
 *     OoT's host-native table maps (MM's hookshot, #577 M7) emits OoT's OWN row
 *     for it, the lists OoT's own recipe gives that row (#832); unmounted, with
 *     no placement, or for a no-model answer (Odolwa's Remains) it emits no
 *     model list and answers 0, so the shelf keeps its stand-in.
 *  M13 OoT'S GET-ITEM CUTSCENE (#577 M4): the display-without-grant path for
 *     an OoT check hosting an MM item. The drain's REAL queueing takes the queue
 *     slot with a show-only entry (the show-only draw, a valid object, a draw
 *     id, the custom-item text, a randomizer major item); the textbox names the
 *     MM item; the give point's take refuses an ordinary custom-drawn entry,
 *     claims the show-only one once and releases the slot (the give is
 *     skipped); the entry's own draw function emits, into a real OoT
 *     GraphicsContext, MM's descriptor for an MM-exclusive model (Deku Mask,
 *     Great Fairy's Sword), OoT's OWN row for a colliding model
 *     OoT's host-native table maps (MM's Hookshot), and the mystery stand-in
 *     when unmounted or for a no-model answer (Odolwa's Remains). No placement:
 *     nothing queued. The give point itself (func_8084DFF4) needs a live
 *     Player: the playtest shows it.
 *  M14 OoT'S CHEST APPEARANCE (#577 M5): with "Chest Size & Texture Matches
 *     Contents" on, an OoT chest takes its look from the item the OoT table holds
 *     there, which for a crossing host is the junk cover. The category the chest
 *     presents (OoT_Rando_Foreign_ChestCategory, what z_en_box.c's init asks) is
 *     major for MM's Hookshot and Odolwa's Remains (MM grades both major; M15
 *     covers every other class), and the chest's own for a check with no MM item. The actor's texture swap
 *     itself loads resources: the playtest shows it.
 *  M15 A CROSSING CHEST IS GRADED BY THE CROSSED ITEM'S TYPE (#862, OoTMM's
 *     csmcFromItem): every crossing-eligible id of each game, placed on a chest
 *     of the other game, dresses that chest as the item's OWN game dresses a
 *     chest holding it natively (OoT's Item::GetCategory, MM's randoItemType):
 *     a small key shows the host's key chest, a token its token chest, a heart
 *     its heart chest. Where the host has no chest for the class (OoT has no
 *     fairy or mask chest) it shows major, the pre-#862 answer. The answer comes
 *     from the hosts' real chest functions (OoT_Rando_Foreign_ChestCategory,
 *     Rando::Foreign::ChestItemTypeForCheck); the expectation from each game's
 *     own table, read directly.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++);
 * every symbol it drives is C-linkage.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../foreign_model.h"
#include "../game.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <string>

extern "C" {
int OoT_ComboModel(uint16_t id, ComboModel* out);
int OoT_ComboModelHostNative(const ComboModel* foreign, uint16_t* hostKey);
int OoT_ComboModel_TestIdSpace(void);
int OoT_ComboModel_TestDrawRowCount(void);
int OoT_ComboModel_TestForDrawRow(int drawId, ComboModel* out, const char** reason);
int OoT_ComboModel_TestDrawRowLists(int drawId, const char* lists[COMBO_MODEL_MAX_PARTS]);
const char* OoT_ComboModel_TestItemReason(uint16_t id);
int MM_ComboModel(uint16_t id, ComboModel* out);
int MM_ComboModelHostNative(const ComboModel* foreign, uint16_t* hostKey);
int MM_ComboModel_TestIdSpace(void);
int MM_ComboModel_TestDrawRowCount(void);
int MM_ComboModel_TestForDrawRow(int drawId, ComboModel* out, const char** reason);
int MM_ComboModel_TestForHostKey(int hostKey, ComboModel* out, const char** reason);
int MM_ComboModel_TestDrawRowLists(int drawId, const char* lists[COMBO_MODEL_MAX_PARTS]);
const char* MM_ComboModel_TestItemReason(uint16_t id);
int OoT_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
int MM_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
void MM_ForeignModel_TestSetMountOverride(int value);
int MM_ForeignModel_TestPathMountedReal(const char* dl);
int MM_ForeignModel_TestCheckQueueDraw(uint16_t mmCheckId, const ComboModel* want);
int OoT_ComboModelHostNative_TestAnswer(const ComboModel* foreign, uint16_t* hostKey, const char** reason,
                                        int* tableRow);
int OoT_ComboModelHostNative_TestRowCount(void);
int MM_ComboModelHostNative_TestAnswer(const ComboModel* foreign, uint16_t* hostKey, const char** reason,
                                       int* tableRow);
int MM_ComboModelHostNative_TestRowCount(void);
uint16_t MM_ForeignTextboxIcon_TestSomeCheck(void);
// #800 S1: OoT as the host (games/oot/soh/Enhancements/randomizer/ForeignModelHostOoT.cpp).
int OoT_ForeignModel_ModelForOoTCheck(uint16_t rc, ComboModel* out);
void OoT_ForeignModel_TestSetMountOverride(int value);
int OoT_ForeignModel_TestPathMountedReal(const char* dl);
int OoT_ForeignModel_TestShelfDraw(uint16_t rc, const ComboModel* want);
// #577 M4: OoT's get-item cutscene (ForeignModelHostOoT.cpp).
int OoT_ForeignModel_TestShowOnlyGetItem(uint16_t rc, int wantEntry, const ComboModel* want, const char* wantName);
// #577 M5: OoT's chest appearance for a crossing host (ForeignItemsSingleExe.cpp).
int OoT_Rando_Foreign_TestChestShowsMajor(uint16_t rc);
// #862: both hosts' chest grade for a crossing (ForeignItemsSingleExe.cpp; MM's Foreign.cpp).
int OoT_Rando_Foreign_TestChestCategory(uint16_t rc);
int OoT_Rando_Foreign_TestOwnChestCategory(uint16_t rg);
const char* OoT_Rando_Foreign_TestChestCategoryName(int category);
int MM_Rando_Foreign_TestChestType(uint16_t randoCheckId);
int MM_Rando_Foreign_TestOwnChestType(uint16_t randoItemId);
const char* MM_Rando_Foreign_TestChestTypeName(int type);
}

#define FM_ASSERT(cond, msg)                                                \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            return TEST_FAIL;                                               \
        }                                                                   \
    } while (0)

namespace {

SharedItem FmItem(uint8_t origin, uint16_t id) {
    SharedItem item;
    item.originGame = origin;
    item.flags = 0;
    item.id = id;
    return item;
}

bool FmColorEqual(const ComboModelColor& a, const ComboModelColor& b) {
    return a.set == b.set && a.primLodFrac == b.primLodFrac && std::memcmp(a.prim, b.prim, 3) == 0 &&
           std::memcmp(a.env, b.env, 3) == 0;
}

bool FmScrollEqual(const ComboModelScroll& a, const ComboModelScroll& b) {
    return a.segment == b.segment && a.layer == b.layer && a.twoTiles == b.twoTiles && a.x1 == b.x1 && a.y1 == b.y1 &&
           a.x1PerFrame == b.x1PerFrame && a.y1PerFrame == b.y1PerFrame && a.w1 == b.w1 && a.h1 == b.h1 &&
           a.x2 == b.x2 && a.y2 == b.y2 && a.x2PerFrame == b.x2PerFrame && a.y2PerFrame == b.y2PerFrame &&
           a.w2 == b.w2 && a.h2 == b.h2;
}

/** Field-by-field (struct padding is not part of a model). Paths compare by
 *  pointer: a round-trip hands the host the origin's own static string. */
bool FmModelEqual(const ComboModel& a, const ComboModel& b) {
    if (a.partCount != b.partCount || a.opaSetupDl != b.opaSetupDl || a.xluSetupDl != b.xluSetupDl ||
        !FmColorEqual(a.opaColor, b.opaColor) || !FmColorEqual(a.xluColor, b.xluColor) || a.grayscale != b.grayscale ||
        std::memcmp(a.grayscaleRgb, b.grayscaleRgb, 3) != 0 || a.scale != b.scale) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (a.rotation[i] != b.rotation[i] || a.billboardOffset[i] != b.billboardOffset[i]) {
            return false;
        }
    }
    for (int i = 0; i < a.partCount; i++) {
        if (a.parts[i].dl != b.parts[i].dl || a.parts[i].layer != b.parts[i].layer ||
            a.parts[i].billboard != b.parts[i].billboard) {
            return false;
        }
    }
    for (int i = 0; i < COMBO_MODEL_MAX_SCROLLS; i++) {
        if (!FmScrollEqual(a.scrolls[i], b.scrolls[i])) {
            return false;
        }
    }
    return true;
}

bool FmAnswerZeroed(const ComboModelAnswer& a) {
    static const ComboModelAnswer kZero = {};
    return a.kind == 0 && a.hostKey == 0 && a.model.partCount == 0 && FmModelEqual(a.model, kZero.model);
}

bool FmModelCollides(const ComboModel& m) {
    for (int i = 0; i < m.partCount; i++) {
        if (Combo_ForeignModel_PathCollides(m.parts[i].dl)) {
            return true;
        }
    }
    return false;
}

struct FmGame {
    const char* name;
    uint8_t game;
    uint8_t other;
    int (*rowCount)(void);
    int (*forRow)(int, ComboModel*, const char**);
    int (*rowLists)(int, const char**);
    int (*idSpace)(void);
    int (*classify)(uint16_t, ComboItemClassRow*);
    const char* (*itemReason)(uint16_t);
    int expectedNoModelRows;
    /** This game's host-native table, as a HOST (M11). */
    int (*hostNativeAnswer)(const ComboModel*, uint16_t*, const char**, int*);
    int (*hostNativeRowCount)(void);
    /** The model this game draws, as a HOST, for one of its own host keys: a
     *  draw row, or (MM, #830) a host-side recipe it numbers itself. */
    int (*forHostKey)(int, ComboModel*, const char**);
};

/** M2 for one game. */
TestResult FmWalkDrawRows(const FmGame& g) {
    const int rows = g.rowCount();
    FM_ASSERT(rows > 100, "M2 the draw table is the game's full get-item table");
    int answered = 0;
    int noModel = 0;
    int byKind[3] = {};
    int failures = 0;
    for (int row = 0; row < rows; row++) {
        ComboModel model;
        const char* reason = nullptr;
        const int got = g.forRow(row, &model, &reason);
        if (got != 1) {
            if (got != 0 || reason == nullptr || reason[0] == '\0') {
                printf("[TEST]   %s row %d: answer %d with no named reason\n", g.name, row, got);
                failures++;
            } else {
                printf("[TEST]   %s row %3d: no model (%s)\n", g.name, row, reason);
            }
            noModel++;
            continue;
        }
        answered++;
        if (!Combo_ModelIsWellFormed(&model)) {
            printf("[TEST]   %s row %d: malformed descriptor\n", g.name, row);
            failures++;
            continue;
        }
        const char* lists[COMBO_MODEL_MAX_PARTS] = {};
        FM_ASSERT(g.rowLists(row, lists) == 1, "M2 the row's lists are readable");
        for (int i = 0; i < model.partCount; i++) {
            bool inRow = false;
            for (const char* list : lists) {
                inRow |= list != nullptr && list == model.parts[i].dl;
            }
            if (!inRow) {
                printf("[TEST]   %s row %d: part %d (%s) is not a list of the row\n", g.name, row, i,
                       model.parts[i].dl);
                failures++;
            }
        }
        for (const char* list : lists) {
            if (list == nullptr) {
                continue;
            }
            if (std::strstr(list, "BillboardRotMtx") != nullptr) {
                // A matrix RESOURCE (the seahorse's), not a display list: the descriptor expresses it as
                // billboardOffset (#964), so it must have turned into a non-zero offset.
                if (model.billboardOffset[0] == 0.0f && model.billboardOffset[1] == 0.0f &&
                    model.billboardOffset[2] == 0.0f) {
                    printf("[TEST]   %s row %d: the matrix %s is dropped\n", g.name, row, list);
                    failures++;
                }
                continue;
            }
            bool used = false;
            for (int i = 0; i < model.partCount; i++) {
                used |= model.parts[i].dl == list;
            }
            if (!used) {
                printf("[TEST]   %s row %d: the row's list %s is dropped\n", g.name, row, list);
                failures++;
            }
        }
        ComboModelAnswer answer;
        const uint8_t kind = Combo_ClassifyForeignModel(g.other, &model, &answer);
        const bool collides = FmModelCollides(model);
        if (kind > COMBO_MODEL_ANSWER_HOST_NATIVE || answer.kind != kind ||
            (kind == COMBO_MODEL_ANSWER_DESCRIPTOR) == collides ||
            (kind == COMBO_MODEL_ANSWER_DESCRIPTOR && !FmModelEqual(answer.model, model))) {
            printf("[TEST]   %s row %d: classified %u (collides %d) inconsistently\n", g.name, row, (unsigned)kind,
                   collides ? 1 : 0);
            failures++;
            continue;
        }
        byKind[kind]++;
    }
    printf("[TEST]   %s: %d draw rows: %d descriptors (%d draw as-is in the other game, %d host-native, %d collide "
           "and draw no model there), %d no model; %d failures\n",
           g.name, rows, answered, byKind[COMBO_MODEL_ANSWER_DESCRIPTOR], byKind[COMBO_MODEL_ANSWER_HOST_NATIVE],
           byKind[COMBO_MODEL_ANSWER_NONE], noModel, failures);
    FM_ASSERT(failures == 0, "M2 every draw row classifies to a descriptor of its own lists or a named no-model");
    FM_ASSERT(noModel == g.expectedNoModelRows, "M2 exactly the adjudicated rows answer no model");
    FM_ASSERT(byKind[COMBO_MODEL_ANSWER_DESCRIPTOR] > 0 && byKind[COMBO_MODEL_ANSWER_NONE] > 0,
              "M2 both the exclusive and the colliding classes are populated");
    return TEST_PASS;
}

/** M4 for one game. */
TestResult FmWalkProgression(const FmGame& g) {
    const int idSpace = g.idSpace();
    FM_ASSERT(idSpace > 1 && idSpace < 0xFFFF, "M4 id space");
    int progression = 0;
    int byKind[3] = {};
    int sourceDeclined = 0;
    int failures = 0;
    for (int id = 1; id < idSpace; id++) {
        ComboItemClassRow row;
        const int classified = g.classify((uint16_t)id, &row);
        FM_ASSERT(classified >= 0, "M4 the classifier is ready");
        if (classified != 1 || row.fillClass != RSBS_FILL_CLASS_PROGRESSION) {
            continue;
        }
        progression++;
        const SharedItem item = FmItem(g.game, (uint16_t)id);
        ComboModelAnswer answer;
        const uint8_t kind = Combo_GetForeignItemModel(g.other, item, &answer);
        ComboModel direct;
        const int sourced = Combo_GetModelSource(g.game)(item.id, &direct);
        const char* name = Combo_GetForeignItemName(item);
        if (kind > COMBO_MODEL_ANSWER_HOST_NATIVE || answer.kind != kind) {
            printf("[TEST]   %s %d \"%s\": answer kind %u out of range\n", g.name, id, name ? name : "?",
                   (unsigned)kind);
            failures++;
            continue;
        }
        byKind[kind]++;
        if (sourced != 1) {
            sourceDeclined++;
            const char* reason = g.itemReason((uint16_t)id);
            if (kind != COMBO_MODEL_ANSWER_NONE || !FmAnswerZeroed(answer) || reason == nullptr) {
                printf("[TEST]   %s %d \"%s\": the source declined without a reason, or a model leaked\n", g.name, id,
                       name ? name : "?");
                failures++;
            }
            continue;
        }
        if (kind == COMBO_MODEL_ANSWER_DESCRIPTOR && !FmModelEqual(answer.model, direct)) {
            printf("[TEST]   %s %d \"%s\": the registry did not round-trip the source's descriptor\n", g.name, id,
                   name ? name : "?");
            failures++;
        }
        if ((kind == COMBO_MODEL_ANSWER_DESCRIPTOR) == FmModelCollides(direct)) {
            printf("[TEST]   %s %d \"%s\": collision and answer disagree\n", g.name, id, name ? name : "?");
            failures++;
        }
    }
    printf("[TEST]   %s: %d progression items for the other game: %d descriptors, %d host-native, %d no model (%d "
           "declined by the source); %d failures\n",
           g.name, progression, byKind[COMBO_MODEL_ANSWER_DESCRIPTOR], byKind[COMBO_MODEL_ANSWER_HOST_NATIVE],
           byKind[COMBO_MODEL_ANSWER_NONE], sourceDeclined, failures);
    FM_ASSERT(progression > 50, "M4 the walk saw the game's progression items");
    FM_ASSERT(failures == 0, "M4 every progression item classifies and every descriptor round-trips");
    FM_ASSERT(byKind[COMBO_MODEL_ANSWER_DESCRIPTOR] > 0, "M4 some progression items draw their own model");
    FM_ASSERT(byKind[COMBO_MODEL_ANSWER_HOST_NATIVE] > 0,
              "M4 some progression items draw the other game's own model (#577 M7 host-native rows)");
    return TEST_PASS;
}

/**
 * M11 for one direction: every colliding draw row and item of `origin` answers,
 * in `host`, a host-native model or a named "no model". `used` marks the host
 * table rows that some colliding origin model reached.
 */
TestResult FmWalkHostNative(const FmGame& origin, const FmGame& host, std::set<int>* used) {
    const int tableRows = host.hostNativeRowCount();
    int colliding = 0;
    int mapped = 0;
    int declined = 0;
    int failures = 0;

    // One colliding origin model: 1 mapped, 0 declined with a reason, -1 failed.
    auto answerOne = [&](const char* what, int n, const ComboModel& model, bool print) -> int {
        uint16_t key = 0xFFFF;
        const char* reason = nullptr;
        int tableRow = -1;
        const int got = host.hostNativeAnswer(&model, &key, &reason, &tableRow);
        ComboModelAnswer a;
        const uint8_t kind = Combo_ClassifyForeignModel(host.game, &model, &a);
        if (got == 1) {
            ComboModel hostModel;
            const char* hostReason = nullptr;
            const bool drawable =
                host.forHostKey((int)key, &hostModel, &hostReason) == 1 && Combo_ModelIsWellFormed(&hostModel);
            if (!drawable || tableRow < 0 || kind != COMBO_MODEL_ANSWER_HOST_NATIVE || a.hostKey != key) {
                printf("[TEST]   %s %s %d (%s): mapped to %s row %u, which %s\n", origin.name, what, n,
                       model.parts[0].dl, host.name, (unsigned)key,
                       !drawable ? "the host's own recipe does not draw" : "the registry does not hand out");
                return -1;
            }
            used->insert(tableRow);
            if (print) {
                printf("[TEST]   %s %s %3d %-58s -> %s key %3u (%s)\n", origin.name, what, n, model.parts[0].dl + 15,
                       host.name, (unsigned)key, hostModel.parts[0].dl + 15);
            }
            return 1;
        }
        if (got != 0 || reason == nullptr || reason[0] == '\0' || tableRow < 0 || kind != COMBO_MODEL_ANSWER_NONE) {
            printf("[TEST]   %s %s %d (%s, %d part(s)) collides and the %s host answers neither a model nor a "
                   "named no-model\n",
                   origin.name, what, n, model.parts[0].dl, (int)model.partCount, host.name);
            return -1;
        }
        used->insert(tableRow);
        if (print) {
            printf("[TEST]   %s %s %3d %-58s -> no model in %s (%s)\n", origin.name, what, n, model.parts[0].dl + 15,
                   host.name, reason);
        }
        return 0;
    };

    const int rows = origin.rowCount();
    for (int row = 0; row < rows; row++) {
        ComboModel model;
        const char* reason = nullptr;
        if (origin.forRow(row, &model, &reason) != 1 || !FmModelCollides(model)) {
            continue;
        }
        colliding++;
        const int r = answerOne("row", row, model, true);
        mapped += r == 1 ? 1 : 0;
        declined += r == 0 ? 1 : 0;
        failures += r < 0 ? 1 : 0;
    }

    int items = 0;
    const int idSpace = origin.idSpace();
    for (int id = 1; id < idSpace; id++) {
        ComboModel model;
        if (Combo_GetModelSource(origin.game)((uint16_t)id, &model) != 1 || !FmModelCollides(model)) {
            continue;
        }
        items++;
        const int r = answerOne("item", id, model, false);
        ComboModelAnswer a;
        const uint8_t kind = Combo_GetForeignItemModel(host.game, FmItem(origin.game, (uint16_t)id), &a);
        if (r < 0 || kind != (r == 1 ? COMBO_MODEL_ANSWER_HOST_NATIVE : COMBO_MODEL_ANSWER_NONE)) {
            printf("[TEST]   %s item %d: the registry's answer (%u) disagrees with the host's table\n", origin.name, id,
                   (unsigned)kind);
            failures++;
        }
    }
    printf("[TEST]   %s models in %s: %d colliding draw rows (%d host-native, %d named no-model), %d colliding "
           "items; %s table has %d rows; %d failures\n",
           origin.name, host.name, colliding, mapped, declined, items, host.name, tableRows, failures);
    FM_ASSERT(colliding > 30, "M11 the walk saw the colliding draw rows");
    FM_ASSERT(failures == 0, "M11 every colliding model answers a host-native model or a named no-model");
    FM_ASSERT(mapped > 0, "M11 the host draws its own model for some colliding foreign models");
    return TEST_PASS;
}

/** The part list of a named item's answer, for the pins. */
struct FmPin {
    uint8_t origin;
    const char* name;
    uint8_t kind;
    const char* firstListTail; // nullptr: no model
    uint8_t partCount;
    uint8_t firstLayer;
    /** HOST_NATIVE only: the tail of a list the HOST row it maps to draws. */
    const char* hostListTail;
};

bool FmTailIs(const char* dl, const char* tail) {
    const size_t len = dl != nullptr ? std::strlen(dl) : 0;
    const size_t n = std::strlen(tail);
    return len >= n && std::strcmp(dl + len - n, tail) == 0;
}

TestResult FmCheckPin(const FmPin& pin) {
    SharedItem item;
    if (!TestNamedItem(pin.origin, pin.name, &item)) {
        printf("[TEST] FAIL: game %u has no item named \"%s\"\n", (unsigned)pin.origin, pin.name);
        return TEST_FAIL;
    }
    const uint8_t host = pin.origin == (uint8_t)GAME_OOT ? (uint8_t)GAME_MM : (uint8_t)GAME_OOT;
    ComboModelAnswer answer;
    const uint8_t kind = Combo_GetForeignItemModel(host, item, &answer);
    bool ok = kind == pin.kind;
    if (ok && pin.firstListTail != nullptr) {
        const char* dl = answer.model.parts[0].dl;
        const size_t len = dl != nullptr ? std::strlen(dl) : 0;
        const size_t tail = std::strlen(pin.firstListTail);
        ok = len >= tail && std::strcmp(dl + len - tail, pin.firstListTail) == 0 &&
             answer.model.partCount == pin.partCount && answer.model.parts[0].layer == pin.firstLayer;
    }
    const char* hostFirst = "-";
    if (ok && pin.kind == COMBO_MODEL_ANSWER_HOST_NATIVE) {
        // The host's own row, as the host's own recipe draws it.
        ComboModel hostModel;
        const char* reason = nullptr;
        const int drawn = host == (uint8_t)GAME_MM ? MM_ComboModel_TestForHostKey(answer.hostKey, &hostModel, &reason)
                                                   : OoT_ComboModel_TestForDrawRow(answer.hostKey, &hostModel, &reason);
        hostFirst = drawn == 1 ? hostModel.parts[0].dl : "(host row not drawable)";
        bool hasList = false;
        for (int i = 0; drawn == 1 && pin.hostListTail != nullptr && i < hostModel.partCount; i++) {
            hasList |= FmTailIs(hostModel.parts[i].dl, pin.hostListTail);
        }
        ok = drawn == 1 && hasList;
    }
    printf("[TEST]   pin: %-24s -> kind %u, %u parts, %s%s%s\n", pin.name, (unsigned)kind,
           (unsigned)answer.model.partCount, answer.model.partCount > 0 ? answer.model.parts[0].dl : "-",
           kind == COMBO_MODEL_ANSWER_HOST_NATIVE ? " => host row first " : "",
           kind == COMBO_MODEL_ANSWER_HOST_NATIVE ? hostFirst : "");
    if (!ok) {
        printf("[TEST] FAIL: \"%s\" wants kind %u, %u parts, first \"...%s\"\n", pin.name, (unsigned)pin.kind,
               (unsigned)pin.partCount, pin.firstListTail != nullptr ? pin.firstListTail : "-");
        return TEST_FAIL;
    }
    return TEST_PASS;
}

// ---- M11 songs (#830) --------------------------------------------------------
const char kFmNoteTail[] = "/object_gi_melody/gGiSongNoteDL";

/** `m` has a part ending in `tail` on `layer`. */
bool FmHasPart(const ComboModel& m, const char* tail, uint8_t layer) {
    for (int i = 0; i < m.partCount; i++) {
        if (m.parts[i].layer == layer && FmTailIs(m.parts[i].dl, tail)) {
            return true;
        }
    }
    return false;
}

/** The tinted note MM's DrawSong draws (DrawItem.cpp): exactly gGiSongNoteDL on
 *  XLU under setup list 25, no OPA layer, tinted by the env colour `rgb`. */
bool FmIsTintedNote(const ComboModel& m, const uint8_t rgb[3]) {
    return m.partCount == 1 && m.parts[0].layer == COMBO_MODEL_LAYER_XLU && FmTailIs(m.parts[0].dl, kFmNoteTail) &&
           m.opaSetupDl == 0 && m.xluSetupDl == 25 && m.xluColor.set == 1 && std::memcmp(m.xluColor.env, rgb, 3) == 0 &&
           m.grayscale == 0;
}

/**
 * M11 for OoT's songs in MM (#830). Every OoT draw row that draws the song note
 * (six coloured notes, seven tinted ones) answers a drawable MM note, and the
 * seven tinted notes are seven rows of MM's table, keyed by their tint. Each of
 * OoT's twelve song items answers a HOST_NATIVE note; where MM has a same-item
 * note (DrawSong: Sun, Time, Storms, Epona) it carries DrawSong's env colour,
 * otherwise OoT's own tint (Lullaby, Saria) or the env colour OoT's colour list
 * sets (Minuet, Bolero), as OoTMM draws every song as a tinted note.
 */
TestResult FmWalkOoTSongsInMM(void) {
    struct TintToEnv {
        uint8_t tint[3];
        uint8_t env[3];
    };
    // OoT's GetItem_DrawGenericMusicNote tint -> the env colour MM draws.
    static const TintToEnv kTints[] = {
        { { 255, 255, 255 }, { 255, 255, 255 } }, // the generic note: OoT's tint
        { { 109, 73, 143 }, { 109, 73, 143 } },   // Zelda's Lullaby: OoT's tint
        { { 217, 110, 48 }, { 146, 87, 49 } },    // Epona's Song: DrawSong's RI_SONG_EPONA
        { { 62, 109, 23 }, { 62, 109, 23 } },     // Saria's Song: OoT's tint
        { { 237, 231, 62 }, { 237, 231, 62 } },   // Sun's Song: DrawSong's RI_SONG_SUN
        { { 98, 177, 211 }, { 98, 177, 211 } },   // Song of Time: DrawSong's RI_SONG_TIME
        { { 146, 146, 146 }, { 146, 146, 146 } }, // Song of Storms: DrawSong's RI_SONG_STORMS
    };
    const uint8_t kXluLayer = COMBO_MODEL_LAYER_XLU;
    int songRows = 0;
    int tinted = 0;
    int failures = 0;
    std::set<int> tintedTableRows;
    const int rows = OoT_ComboModel_TestDrawRowCount();
    for (int row = 0; row < rows; row++) {
        ComboModel model;
        const char* reason = nullptr;
        if (OoT_ComboModel_TestForDrawRow(row, &model, &reason) != 1 || !FmHasPart(model, kFmNoteTail, kXluLayer)) {
            continue;
        }
        songRows++;
        uint16_t key = 0xFFFF;
        const char* why = nullptr;
        int tableRow = -1;
        const int got = MM_ComboModelHostNative_TestAnswer(&model, &key, &why, &tableRow);
        ComboModel host;
        Combo_ModelInit(&host);
        const char* hostWhy = nullptr;
        bool ok = got == 1 && MM_ComboModel_TestForHostKey(key, &host, &hostWhy) == 1 &&
                  Combo_ModelIsWellFormed(&host) && FmHasPart(host, kFmNoteTail, kXluLayer) && host.opaSetupDl == 0;
        if (model.grayscale) {
            tinted++;
            if (tableRow >= 0) {
                tintedTableRows.insert(tableRow);
            }
            const TintToEnv* want = nullptr;
            for (const TintToEnv& t : kTints) {
                want = std::memcmp(t.tint, model.grayscaleRgb, 3) == 0 ? &t : want;
            }
            ok = ok && want != nullptr && FmIsTintedNote(host, want->env);
        }
        printf("[TEST]   OoT song row %3d %-40s tint %3d,%3d,%3d -> MM %s, table row %d, key %u: %u part(s), env "
               "%3d,%3d,%3d%s%s\n",
               row, model.parts[0].dl + 15, model.grayscale ? model.grayscaleRgb[0] : -1,
               model.grayscale ? model.grayscaleRgb[1] : -1, model.grayscale ? model.grayscaleRgb[2] : -1,
               got == 1 ? "draws" : "NO MODEL", tableRow, (unsigned)key, (unsigned)host.partCount,
               host.xluColor.set ? host.xluColor.env[0] : -1, host.xluColor.set ? host.xluColor.env[1] : -1,
               host.xluColor.set ? host.xluColor.env[2] : -1, ok ? "" : "  <- ",
               ok         ? ""
               : got == 1 ? "wrong note"
               : why      ? why
                          : "no table row");
        failures += ok ? 0 : 1;
    }
    printf("[TEST]   OoT song rows in MM: %d rows (%d tinted, on %zu MM table rows); %d failures\n", songRows, tinted,
           tintedTableRows.size(), failures);
    FM_ASSERT(songRows == 13, "M11 #830: OoT's get-item table has thirteen song-note rows");
    FM_ASSERT(failures == 0, "M11 #830: every OoT song row answers a drawable MM note (see the lines above)");
    FM_ASSERT(tinted == 7 && tintedTableRows.size() == 7,
              "M11 #830: the seven tinted notes are seven rows of MM's table, keyed by their tint");

    struct SongPin {
        const char* name;
        bool tintedNote; // false: MM's own GID_04..GID_07 row (colour list + note)
        uint8_t env[3];
    };
    static const SongPin kSongs[] = {
        { "Minuet of Forest", true, { 0, 200, 0 } }, // gGiMinuetColorDL's env colour
        { "Bolero of Fire", true, { 255, 50, 0 } },  // gGiBoleroColorDL's env colour
        { "Serenade of Water", false, {} },
        { "Requiem of Spirit", false, {} },
        { "Nocturne of Shadow", false, {} },
        { "Prelude of Light", false, {} },
        { "Zelda's Lullaby", true, { 109, 73, 143 } },
        { "Epona's Song", true, { 146, 87, 49 } },
        { "Saria's Song", true, { 62, 109, 23 } },
        { "Sun's Song", true, { 237, 231, 62 } },
        { "Song of Time", true, { 98, 177, 211 } },
        { "Song of Storms", true, { 146, 146, 146 } },
    };
    for (const SongPin& song : kSongs) {
        SharedItem item;
        FM_ASSERT(TestNamedItem((uint8_t)GAME_OOT, song.name, &item), "M11 #830: named OoT song");
        ComboModelAnswer a;
        const uint8_t kind = Combo_GetForeignItemModel((uint8_t)GAME_MM, item, &a);
        ComboModel host;
        Combo_ModelInit(&host);
        const char* why = nullptr;
        const bool drawn = kind == COMBO_MODEL_ANSWER_HOST_NATIVE &&
                           MM_ComboModel_TestForHostKey(a.hostKey, &host, &why) == 1 && Combo_ModelIsWellFormed(&host);
        const bool ok = drawn && (song.tintedNote ? FmIsTintedNote(host, song.env)
                                                  : host.partCount == 2 && FmHasPart(host, kFmNoteTail, kXluLayer) &&
                                                        host.parts[0].layer == kXluLayer);
        printf("[TEST]   %-20s -> kind %u, MM key %u: %u part(s), env %d,%d,%d\n", song.name, (unsigned)kind,
               (unsigned)a.hostKey, (unsigned)host.partCount, host.xluColor.set ? host.xluColor.env[0] : -1,
               host.xluColor.set ? host.xluColor.env[1] : -1, host.xluColor.set ? host.xluColor.env[2] : -1);
        if (!ok) {
            printf("[TEST] FAIL: %s wants %s in MM\n", song.name,
                   song.tintedNote ? "a tinted note (gGiSongNoteDL on XLU, its env colour)" : "MM's own note row");
            return TEST_FAIL;
        }
    }
    return TEST_PASS;
}

// ---- M6: synthetic half-answers ---------------------------------------------
int sFmSyntheticMode = 0;
int FmSyntheticSource(uint16_t id, ComboModel* out) {
    (void)id;
    static const char kHammer[] = "__OTR__objects/object_gi_hammer/gGiHammerDL";
    Combo_ModelInit(out);
    out->partCount = 1;
    out->parts[0].dl = kHammer;
    out->parts[0].layer = COMBO_MODEL_LAYER_OPA;
    out->opaSetupDl = 25;
    switch (sFmSyntheticMode) {
        case 0: // no parts
            out->partCount = 0;
            return 1;
        case 1: // NULL path
            out->parts[0].dl = nullptr;
            return 1;
        case 2: // no __OTR__ prefix
            out->parts[0].dl = "objects/object_gi_hammer/gGiHammerDL";
            return 1;
        case 3: // not an object path
            out->parts[0].dl = "__OTR__textures/icon_item_static/gItemIconHammerTex";
            return 1;
        case 4: // unknown layer
            out->parts[0].layer = 7;
            return 1;
        case 5: // layer with no setup list
            out->opaSetupDl = 0;
            return 1;
        case 6: // setup list for a layer nothing draws into
            out->xluSetupDl = 25;
            return 1;
        case 7: // too many parts
            out->partCount = COMBO_MODEL_MAX_PARTS + 1;
            return 1;
        case 8: // a scroll on an engine segment
            out->scrolls[0].segment = 6;
            out->scrolls[0].layer = COMBO_MODEL_LAYER_OPA;
            out->scrolls[0].w1 = 32;
            out->scrolls[0].h1 = 32;
            return 1;
        case 9: // a half-filled unused scroll slot
            out->scrolls[1].w1 = 32;
            return 1;
        case 10: // a scroll on a layer nothing draws into
            out->scrolls[0].segment = 8;
            out->scrolls[0].layer = COMBO_MODEL_LAYER_XLU;
            out->scrolls[0].w1 = 32;
            out->scrolls[0].h1 = 32;
            return 1;
        case 11: // zero scale
            out->scale = 0.0f;
            return 1;
        case 12: // NaN scale
            out->scale = std::nanf("");
            return 1;
        case 13: // colours for a layer nothing draws into
            out->xluColor.set = 1;
            return 1;
        case 14: // says no but leaves a whole model behind
            return 0;
        default: // a whole answer
            return 1;
    }
}
constexpr int kFmHalfAnswerModes = 15;

int sFmMapperCalls = 0;
int sFmMapperAnswer = 1;
int FmSyntheticMapper(const ComboModel* foreign, uint16_t* hostKey) {
    sFmMapperCalls++;
    if (foreign == nullptr || sFmMapperAnswer != 1) {
        return 0;
    }
    *hostKey = 0x1234;
    return 1;
}

/** M3: object directory names under `rel` in the checkout. */
bool FmListDirs(const char* rel, std::set<std::string>* out) {
#ifdef RSBS_SOURCE_DIR
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::path(RSBS_SOURCE_DIR) / rel;
    std::filesystem::directory_iterator it(root, ec);
    if (ec) {
        return false;
    }
    for (const auto& entry : it) {
        if (entry.is_directory(ec)) {
            out->insert(entry.path().filename().string());
        }
    }
    return true;
#else
    (void)rel;
    (void)out;
    return false;
#endif
}

/** M9: the #include targets of a checkout file. */
bool FmIncludes(const char* rel, std::set<std::string>* out) {
#ifdef RSBS_SOURCE_DIR
    FILE* f = fopen((std::string(RSBS_SOURCE_DIR) + "/" + rel).c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    char line[1024];
    while (fgets(line, sizeof(line), f) != nullptr) {
        const char* p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (std::strncmp(p, "#include", 8) != 0) {
            continue;
        }
        p += 8;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        const char close = *p == '<' ? '>' : '"';
        const char* end = std::strchr(p + 1, close);
        if (end != nullptr) {
            out->insert(std::string(p + 1, end));
        }
    }
    fclose(f);
    return true;
#else
    (void)rel;
    (void)out;
    return false;
#endif
}

} // namespace

TestResult Test_ForeignModel(void) {
    printf("[TEST] ForeignModel: the foreign get-item model descriptor registry (#577 M2)\n");

    // ---- M1 --------------------------------------------------------------------
    FM_ASSERT(Combo_GetModelSource((uint8_t)GAME_OOT) == OoT_ComboModel, "M1 OoT's model source is OoT_ComboModel");
    FM_ASSERT(Combo_GetModelSource((uint8_t)GAME_MM) == MM_ComboModel, "M1 MM's model source is MM_ComboModel");
    FM_ASSERT(Combo_GetHostNativeModel((uint8_t)GAME_OOT) == OoT_ComboModelHostNative,
              "M1 OoT's host-native mapper is OoT_ComboModelHostNative");
    FM_ASSERT(Combo_GetHostNativeModel((uint8_t)GAME_MM) == MM_ComboModelHostNative,
              "M1 MM's host-native mapper is MM_ComboModelHostNative");
    FM_ASSERT(Combo_GetModelSource((uint8_t)GAME_NONE) == nullptr && Combo_GetHostNativeModel(7) == nullptr,
              "M1 a non-game has no source and no mapper");
    FM_ASSERT(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "M1 OoT's item table is available");

    // ---- M2 --------------------------------------------------------------------
    // OoT: the Triforce piece and the fishing pole. MM: the three empty rows and
    // the bottled fairy's row (no item names any of the four; #964 gave the
    // seahorse, the Moon's Tear and the four remains a model).
    const FmGame oot = { "OoT",
                         (uint8_t)GAME_OOT,
                         (uint8_t)GAME_MM,
                         OoT_ComboModel_TestDrawRowCount,
                         OoT_ComboModel_TestForDrawRow,
                         OoT_ComboModel_TestDrawRowLists,
                         OoT_ComboModel_TestIdSpace,
                         OoT_ComboLogic_ClassifyItem,
                         OoT_ComboModel_TestItemReason,
                         2,
                         OoT_ComboModelHostNative_TestAnswer,
                         OoT_ComboModelHostNative_TestRowCount,
                         OoT_ComboModel_TestForDrawRow };
    const FmGame mm = { "MM",
                        (uint8_t)GAME_MM,
                        (uint8_t)GAME_OOT,
                        MM_ComboModel_TestDrawRowCount,
                        MM_ComboModel_TestForDrawRow,
                        MM_ComboModel_TestDrawRowLists,
                        MM_ComboModel_TestIdSpace,
                        MM_ComboLogic_ClassifyItem,
                        MM_ComboModel_TestItemReason,
                        4,
                        MM_ComboModelHostNative_TestAnswer,
                        MM_ComboModelHostNative_TestRowCount,
                        MM_ComboModel_TestForHostKey };
    if (FmWalkDrawRows(oot) != TEST_PASS || FmWalkDrawRows(mm) != TEST_PASS) {
        return TEST_FAIL;
    }
    {
        ComboModel model;
        const char* reason = nullptr;
        FM_ASSERT(OoT_ComboModel_TestForDrawRow(OoT_ComboModel_TestDrawRowCount(), &model, &reason) == 0 &&
                      reason != nullptr,
                  "M2 a row past OoT's table is refused (GID_FISHING_POLE indexes one)");
        FM_ASSERT(MM_ComboModel_TestForDrawRow(-1, &model, &reason) == 0, "M2 a negative MM row is refused");
    }

    // ---- M3 --------------------------------------------------------------------
    {
        std::set<std::string> ootDirs;
        std::set<std::string> mmDirs;
        FM_ASSERT(FmListDirs("games/oot/assets/objects", &ootDirs) &&
                      FmListDirs("games/oot/assets/custom/objects", &ootDirs) &&
                      FmListDirs("games/mm/assets/objects", &mmDirs) &&
                      FmListDirs("games/mm/assets/custom/objects", &mmDirs),
                  "M3 both games' object trees are readable under RSBS_SOURCE_DIR");
        FM_ASSERT(ootDirs.size() > 300 && mmDirs.size() > 300, "M3 the trees are the full object sets");
        std::set<std::string> all = ootDirs;
        all.insert(mmDirs.begin(), mmDirs.end());
        int shared = 0;
        int mismatches = 0;
        for (const std::string& dir : all) {
            const bool both = ootDirs.count(dir) != 0 && mmDirs.count(dir) != 0;
            shared += both ? 1 : 0;
            if ((Combo_ForeignModel_ObjectDirCollides(dir.c_str()) == 1) != both) {
                printf("[TEST]   collision table disagrees with the trees on %s (in both: %d)\n", dir.c_str(),
                       both ? 1 : 0);
                mismatches++;
            }
        }
        printf("[TEST]   %zu OoT and %zu MM object directories, %d in both; the table holds %d\n", ootDirs.size(),
               mmDirs.size(), shared, Combo_ForeignModel_CollidingDirCount());
        FM_ASSERT(mismatches == 0, "M3 the collision table answers exactly the directories both trees carry");
        FM_ASSERT(shared == Combo_ForeignModel_CollidingDirCount(), "M3 the table has no name outside the trees");
        FM_ASSERT(Combo_ForeignModel_PathCollides("__OTR__objects/object_gi_hookshot/gGiHookshotDL") == 1 &&
                      Combo_ForeignModel_PathCollides("__OTR__objects/object_gi_hammer/gGiHammerDL") == 0 &&
                      Combo_ForeignModel_PathCollides("objects/object_gi_hookshot/gGiHookshotDL") == 0 &&
                      Combo_ForeignModel_PathCollides("__OTR__textures/object_gi_hookshot/x") == 0 &&
                      Combo_ForeignModel_PathCollides(nullptr) == 0 && Combo_ForeignModel_ObjectDirCollides("") == 0,
                  "M3 path parsing: only __OTR__objects/<dir>/<name> names a directory");
    }

    // ---- M11 (#577 M7), right after the collision table it rests on ------------
    {
        std::set<int> usedByOoTModels; // rows of MM's table
        std::set<int> usedByMMModels;  // rows of OoT's table
        if (FmWalkHostNative(oot, mm, &usedByOoTModels) != TEST_PASS ||
            FmWalkHostNative(mm, oot, &usedByMMModels) != TEST_PASS) {
            return TEST_FAIL;
        }
        printf("[TEST]   host-native tables: MM %d rows (%zu used), OoT %d rows (%zu used)\n",
               MM_ComboModelHostNative_TestRowCount(), usedByOoTModels.size(), OoT_ComboModelHostNative_TestRowCount(),
               usedByMMModels.size());
        FM_ASSERT((int)usedByOoTModels.size() == MM_ComboModelHostNative_TestRowCount(),
                  "M11 every row of MM's host-native table names a colliding OoT model (no dead or misspelt key)");
        FM_ASSERT((int)usedByMMModels.size() == OoT_ComboModelHostNative_TestRowCount(),
                  "M11 every row of OoT's host-native table names a colliding MM model (no dead or misspelt key)");
        if (FmWalkOoTSongsInMM() != TEST_PASS) {
            return TEST_FAIL;
        }
    }

    // ---- M4 --------------------------------------------------------------------
    if (FmWalkProgression(oot) != TEST_PASS || FmWalkProgression(mm) != TEST_PASS) {
        return TEST_FAIL;
    }

    // ---- M5 --------------------------------------------------------------------
    const uint8_t kDesc = COMBO_MODEL_ANSWER_DESCRIPTOR;
    const uint8_t kNone = COMBO_MODEL_ANSWER_NONE;
    const uint8_t kHost = COMBO_MODEL_ANSWER_HOST_NATIVE;
    const uint8_t kOpa = COMBO_MODEL_LAYER_OPA;
    const uint8_t kXlu = COMBO_MODEL_LAYER_XLU;
    const FmPin pins[] = {
        { (uint8_t)GAME_OOT, "Megaton Hammer", kDesc, "/object_gi_hammer/gGiHammerDL", 1, kOpa, nullptr },
        { (uint8_t)GAME_OOT, "Hover Boots", kDesc, "/object_gi_hoverboots/gGiHoverBootsDL", 1, kOpa, nullptr },
        { (uint8_t)GAME_OOT, "Kokiri's Emerald", kDesc, "/object_gi_jewel/gGiKokiriEmeraldGemDL", 2, kXlu, nullptr },
        { (uint8_t)GAME_OOT, "Forest Medallion", kDesc, "/object_gi_medal/gGiForestMedallionFaceDL", 2, kOpa, nullptr },
        { (uint8_t)GAME_OOT, "Master Sword", kDesc, "/object_toki_objects/object_toki_objects_DL_001BD0", 1, kOpa,
          nullptr },
        { (uint8_t)GAME_OOT, "Roc's Feather", kDesc, "/object_rocs_feather/gGiRocsFeatherDL", 1, kXlu, nullptr },
        // object_gi_hookshot, object_gi_key, object_gi_hearts, object_gi_golonmask
        // are in both archives: the host draws its OWN model (#577 M7).
        { (uint8_t)GAME_OOT, "Progressive Hookshot", kHost, "/object_gi_hookshot/gGiHookshotDL", 1, kOpa,
          "/object_gi_hookshot/gGiHookshotEmptyDL" },
        { (uint8_t)GAME_OOT, "Longshot", kHost, "/object_gi_hookshot/gGiLongshotDL", 1, kOpa,
          "/object_gi_hookshot/gGiHookshotEmptyDL" },
        { (uint8_t)GAME_OOT, "Forest Temple Small Key", kHost, "/object_gi_key/gGiSmallKeyDL", 1, kOpa,
          "/object_gi_key/gGiSmallKeyDL" },
        // Shares its first list with the heart container: the whole-part key tells them apart.
        { (uint8_t)GAME_OOT, "Piece of Heart", kHost, "/object_gi_hearts/gGiHeartBorderDL", 2, kXlu,
          "/object_gi_hearts/gGiHeartPieceDL" },
        // MM has no Minuet note row: its host-side tinted note draws it (#830).
        { (uint8_t)GAME_OOT, "Minuet of Forest", kHost, "/object_gi_melody/gGiMinuetColorDL", 2, kXlu,
          "/object_gi_melody/gGiSongNoteDL" },
        { (uint8_t)GAME_OOT, "Gohma's Soul", kNone, nullptr, 0, 0, nullptr },
        { (uint8_t)GAME_MM, "Deku Mask", kDesc, "/object_gi_nutsmask/gGiDekuMaskEmptyDL", 2, kOpa, nullptr },
        { (uint8_t)GAME_MM, "Hookshot", kHost, "/object_gi_hookshot/gGiHookshotEmptyDL", 2, kOpa,
          "/object_gi_hookshot/gGiHookshotDL" },
        { (uint8_t)GAME_MM, "Goron Mask", kHost, "/object_gi_golonmask/gGiGoronMaskEmptyDL", 2, kOpa,
          "/object_gi_golonmask/gGiGoronMaskDL" },
        { (uint8_t)GAME_MM, "Hero's Shield", kHost, "/object_gi_shield_2/gGiHerosShieldEmblemDL", 2, kOpa,
          "/object_gi_shield_2/gGiHylianShieldDL" },
        { (uint8_t)GAME_MM, "Silver Rupee", kNone, nullptr, 0, 0, nullptr },
        // #964: the custom-drawn rows that used to answer no model.
        { (uint8_t)GAME_MM, "Odolwa's Remains", kDesc, "/object_bsmask/gRemainsOdolwaDL", 1, kOpa, nullptr },
        { (uint8_t)GAME_MM, "Goht's Remains", kDesc, "/object_bsmask/gRemainsGohtDL", 1, kOpa, nullptr },
        { (uint8_t)GAME_MM, "Gyorg's Remains", kDesc, "/object_bsmask/gRemainsGyorgDL", 1, kOpa, nullptr },
        { (uint8_t)GAME_MM, "Twinmold's Remains", kDesc, "/object_bsmask/gRemainsTwinmoldDL", 1, kOpa, nullptr },
        { (uint8_t)GAME_MM, "Moon's Tear", kDesc, "/object_gi_reserve00/gGiMoonsTearItemDL", 2, kOpa, nullptr },
    };
    bool pinsOk = true;
    for (const FmPin& pin : pins) {
        pinsOk = FmCheckPin(pin) == TEST_PASS && pinsOk;
    }
    FM_ASSERT(pinsOk, "M5 every pinned item answers its pinned model");
    {
        // OoT's ice trap: OoT_Player_DrawGetItemImpl special-cases it (a growing
        // ice fragment), so its entry's gold-rupee row is not what OoT shows. The
        // source declines it, as MM's declines RI_TRAP. Checked on the SOURCE:
        // object_gi_rupy collides, so the classified answer is NONE either way.
        SharedItem iceTrap;
        FM_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Ice Trap", &iceTrap), "M5 named item");
        ComboModel direct;
        FM_ASSERT(OoT_ComboModel(iceTrap.id, &direct) == 0 && OoT_ComboModel_TestItemReason(iceTrap.id) != nullptr,
                  "M5 OoT's ice trap answers no model (OoT does not draw its entry's gold-rupee row)");
    }
    {
        // The emerald carries its jewel shape: upright, two scrolls, both colours.
        SharedItem emerald;
        FM_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Kokiri's Emerald", &emerald), "M5 named item");
        ComboModelAnswer a;
        Combo_GetForeignItemModel((uint8_t)GAME_MM, emerald, &a);
        FM_ASSERT(a.model.rotation[1] == -0x4000 && a.model.rotation[2] == 0x4000 && a.model.scrolls[0].segment == 9 &&
                      a.model.scrolls[1].segment == 8 && a.model.xluColor.set == 1 && a.model.xluColor.env[1] == 255 &&
                      a.model.opaColor.set == 1 && a.model.parts[1].layer == COMBO_MODEL_LAYER_OPA,
                  "M5 the Kokiri's Emerald carries GetItem_DrawJewel's rotation, scrolls and colours");
        SharedItem medallion;
        FM_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Forest Medallion", &medallion), "M5 named item");
        Combo_GetForeignItemModel((uint8_t)GAME_MM, medallion, &a);
        FM_ASSERT(a.model.opaSetupDl == 26 && a.model.xluSetupDl == 0,
                  "M5 a medallion keeps GetItem_DrawEggOrMedallion's setup list 26");
    }
    {
        // #964: the MM rows whose draw is custom code answer a model with the
        // shape that code gives them (z_draw.c), not just "a model".
        SharedItem remains[4];
        const char* const remainsNames[4] = { "Odolwa's Remains", "Goht's Remains", "Gyorg's Remains",
                                              "Twinmold's Remains" };
        const char* const remainsLists[4] = { "/object_bsmask/gRemainsOdolwaDL", "/object_bsmask/gRemainsGohtDL",
                                              "/object_bsmask/gRemainsGyorgDL", "/object_bsmask/gRemainsTwinmoldDL" };
        for (int i = 0; i < 4; i++) {
            FM_ASSERT(TestNamedItem((uint8_t)GAME_MM, remainsNames[i], &remains[i]), "M16 named item");
            ComboModelAnswer a;
            FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_OOT, remains[i], &a) == kDesc,
                      "M16 a boss remains is a DESCRIPTOR in OoT (#964)");
            // GetItem_DrawRemains: setup 25 OPA, MM_Matrix_Scale(0.02) on top of the caller's, one list, no
            // scroll, no camera-facing part. The list reads no segment, so no segment-6 bind is needed.
            FM_ASSERT(a.model.partCount == 1 && FmTailIs(a.model.parts[0].dl, remainsLists[i]) &&
                          a.model.parts[0].layer == COMBO_MODEL_LAYER_OPA && a.model.parts[0].billboard == 0 &&
                          a.model.opaSetupDl == 25 && a.model.xluSetupDl == 0 &&
                          std::fabs(a.model.scale - 0.02f) < 1e-6f && a.model.scrolls[0].segment == 0 &&
                          a.model.opaColor.set == 0 && a.model.xluColor.set == 0 && a.model.grayscale == 0,
                      "M16 a boss remains is GetItem_DrawRemains' shape: one OPA list under setup 25 at scale 0.02");
            FM_ASSERT(!Combo_ForeignModel_PathCollides(a.model.parts[0].dl),
                      "M16 object_bsmask is MM-only, so OoT draws the remains from the curated archive");
        }

        // GetItem_DrawMoonsTear: the item list on OPA under AnimatedMat segment 2 (+7 = 9), the glow on XLU under
        // segment 3 (+7 = 10) facing the camera, both two-tile scrolls (gGiMoonsTearTexAnim).
        SharedItem tear;
        FM_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Moon's Tear", &tear), "M16 named item");
        ComboModelAnswer t;
        FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_OOT, tear, &t) == kDesc,
                  "M16 the Moon's Tear is a DESCRIPTOR");
        FM_ASSERT(t.model.partCount == 2 && FmTailIs(t.model.parts[0].dl, "/object_gi_reserve00/gGiMoonsTearItemDL") &&
                      t.model.parts[0].layer == COMBO_MODEL_LAYER_OPA && t.model.parts[0].billboard == 0 &&
                      FmTailIs(t.model.parts[1].dl, "/object_gi_reserve00/gGiMoonsTearGlowDL") &&
                      t.model.parts[1].layer == COMBO_MODEL_LAYER_XLU && t.model.parts[1].billboard == 1 &&
                      t.model.opaSetupDl == 25 && t.model.xluSetupDl == 25 && t.model.billboardOffset[0] == 0.0f &&
                      t.model.billboardOffset[1] == 0.0f && t.model.billboardOffset[2] == 0.0f,
                  "M16 the Moon's Tear is the item list (OPA) and its camera-facing glow (XLU)");
        const ComboModelScroll* itemScroll = nullptr;
        const ComboModelScroll* glowScroll = nullptr;
        for (const ComboModelScroll& sc : t.model.scrolls) {
            itemScroll = sc.segment == 9 ? &sc : itemScroll;
            glowScroll = sc.segment == 10 ? &sc : glowScroll;
        }
        FM_ASSERT(itemScroll != nullptr && itemScroll->layer == COMBO_MODEL_LAYER_OPA && itemScroll->twoTiles == 1 &&
                      itemScroll->x1PerFrame == 1 && itemScroll->y1PerFrame == 1 && itemScroll->x2PerFrame == 0 &&
                      itemScroll->y2PerFrame == 0 && itemScroll->w1 == 32 && itemScroll->h1 == 32 &&
                      itemScroll->w2 == 32 && itemScroll->h2 == 32,
                  "M16 the Moon's Tear scrolls its item texture on segment 9 (xStep 1, yStep -1)");
        FM_ASSERT(glowScroll != nullptr && glowScroll->layer == COMBO_MODEL_LAYER_XLU && glowScroll->twoTiles == 1 &&
                      glowScroll->x1PerFrame == 0 && glowScroll->y1PerFrame == 0 && glowScroll->x2PerFrame == 1 &&
                      glowScroll->y2PerFrame == -2 && glowScroll->w1 == 32 && glowScroll->h1 == 32 &&
                      glowScroll->w2 == 32 && glowScroll->h2 == 32,
                  "M16 the Moon's Tear scrolls its glow texture on segment 10 (second tile xStep 1, yStep 2)");

        // GetItem_DrawSeahorse, row GID_SEAHORSE_CAUGHT (0x62 in z64item.h; no randomizer item names it, so it is
        // asserted on the row): the empty list on OPA, the body on XLU, then the glow facing the camera after
        // gGiSeahorseBillboardRotMtx, a pure translation (-2, 27, 0).
        constexpr int kSeahorseCaughtRow = 0x62;
        ComboModel d;
        const char* seahorseReason = nullptr;
        FM_ASSERT(MM_ComboModel_TestForDrawRow(kSeahorseCaughtRow, &d, &seahorseReason) == 1,
                  "M16 the seahorse row answers a model");
        FM_ASSERT(d.partCount == 3 && FmTailIs(d.parts[0].dl, "/object_gi_seahorse/gGiSeahorseEmptyDL") &&
                      d.parts[0].layer == COMBO_MODEL_LAYER_OPA && d.parts[0].billboard == 0 &&
                      FmTailIs(d.parts[1].dl, "/object_gi_seahorse/gGiSeahorseBodyDL") &&
                      d.parts[1].layer == COMBO_MODEL_LAYER_XLU && d.parts[1].billboard == 0 &&
                      FmTailIs(d.parts[2].dl, "/object_gi_seahorse/gGiSeahorseGlowDL") &&
                      d.parts[2].layer == COMBO_MODEL_LAYER_XLU && d.parts[2].billboard == 1 && d.opaSetupDl == 25 &&
                      d.xluSetupDl == 25,
                  "M16 the seahorse is the empty list (OPA), the body (XLU) and the camera-facing glow (XLU)");
        FM_ASSERT(d.billboardOffset[0] == -2.0f && d.billboardOffset[1] == 27.0f && d.billboardOffset[2] == 0.0f,
                  "M16 the seahorse's glow sits at gGiSeahorseBillboardRotMtx's translation");
    }

    // ---- M6 --------------------------------------------------------------------
    {
        const ComboModelSourceFn savedMm = Combo_GetModelSource((uint8_t)GAME_MM);
        Combo_RegisterModelSource((uint8_t)GAME_MM, FmSyntheticSource);
        bool refused = true;
        for (sFmSyntheticMode = 0; sFmSyntheticMode < kFmHalfAnswerModes; sFmSyntheticMode++) {
            ComboModelAnswer a;
            if (Combo_GetForeignItemModel((uint8_t)GAME_OOT, FmItem((uint8_t)GAME_MM, 1), &a) != kNone ||
                !FmAnswerZeroed(a)) {
                printf("[TEST]   half-answer mode %d was accepted\n", sFmSyntheticMode);
                refused = false;
            }
        }
        sFmSyntheticMode = kFmHalfAnswerModes;
        ComboModelAnswer whole;
        const bool wholeAccepted =
            Combo_GetForeignItemModel((uint8_t)GAME_OOT, FmItem((uint8_t)GAME_MM, 1), &whole) == kDesc &&
            whole.kind == kDesc && whole.model.partCount == 1;
        Combo_RegisterModelSource((uint8_t)GAME_MM, savedMm);
        FM_ASSERT(Combo_GetModelSource((uint8_t)GAME_MM) == savedMm, "M6 MM's slot restored");
        FM_ASSERT(refused, "M6 a source's half-answers are no model, output zeroed");
        FM_ASSERT(wholeAccepted, "M6 a whole answer is passed through (the refusals are not vacuous)");
    }

    // ---- M7 --------------------------------------------------------------------
    {
        SharedItem hookshot;
        SharedItem hammer;
        FM_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Progressive Hookshot", &hookshot) &&
                      TestNamedItem((uint8_t)GAME_OOT, "Megaton Hammer", &hammer),
                  "M7 named items");
        ComboModel direct;
        FM_ASSERT(OoT_ComboModel(hookshot.id, &direct) == 1 && FmModelCollides(direct),
                  "M7 OoT answers the hookshot's own (colliding) model");
        const ComboHostNativeModelFn savedMapper = Combo_GetHostNativeModel((uint8_t)GAME_MM);
        Combo_RegisterHostNativeModel((uint8_t)GAME_MM, FmSyntheticMapper);
        sFmMapperCalls = 0;
        sFmMapperAnswer = 1;
        ComboModelAnswer mapped;
        const uint8_t mappedKind = Combo_GetForeignItemModel((uint8_t)GAME_MM, hookshot, &mapped);
        const int callsForHookshot = sFmMapperCalls;
        ComboModelAnswer plain;
        const uint8_t plainKind = Combo_GetForeignItemModel((uint8_t)GAME_MM, hammer, &plain);
        const int callsForHammer = sFmMapperCalls - callsForHookshot;
        sFmMapperAnswer = 0;
        ComboModelAnswer declined;
        const uint8_t declinedKind = Combo_GetForeignItemModel((uint8_t)GAME_MM, hookshot, &declined);
        Combo_RegisterHostNativeModel((uint8_t)GAME_MM, nullptr);
        ComboModelAnswer unmapped;
        const uint8_t unmappedKind = Combo_GetForeignItemModel((uint8_t)GAME_MM, hookshot, &unmapped);
        Combo_RegisterHostNativeModel((uint8_t)GAME_MM, savedMapper);
        FM_ASSERT(Combo_GetHostNativeModel((uint8_t)GAME_MM) == savedMapper, "M7 MM's mapper restored");
        FM_ASSERT(mappedKind == COMBO_MODEL_ANSWER_HOST_NATIVE && mapped.kind == mappedKind &&
                      mapped.hostKey == 0x1234 && FmModelEqual(mapped.model, direct) && callsForHookshot == 1,
                  "M7 a colliding model answers the host mapper's key, with the origin's model beside it");
        FM_ASSERT(plainKind == kDesc && callsForHammer == 0,
                  "M7 a model with no colliding directory never consults the mapper");
        FM_ASSERT(declinedKind == kNone && FmAnswerZeroed(declined), "M7 a mapper that declines is no model");
        FM_ASSERT(unmappedKind == kNone && FmAnswerZeroed(unmapped), "M7 no mapper is no model");
        uint16_t key = 0;
        FM_ASSERT(MM_ComboModelHostNative(&direct, &key) == 1,
                  "M7 MM's production mapper names its own model for OoT's hookshot (#577 M7)");
        FM_ASSERT(OoT_ComboModelHostNative(&direct, &key) == 0 && OoT_ComboModelHostNative(nullptr, &key) == 0 &&
                      MM_ComboModelHostNative(&direct, nullptr) == 0,
                  "M7 OoT's table keys MM's models, not OoT's own; null arguments map nothing");
    }

    // ---- M8 --------------------------------------------------------------------
    {
        SharedItem hammer;
        FM_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Megaton Hammer", &hammer), "M8 named item");
        ComboModelAnswer a;
        FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_OOT, hammer, &a) == kNone && FmAnswerZeroed(a),
                  "M8 an item native to the host is no model (the host draws it itself)");
        FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_NONE, hammer, &a) == kNone && FmAnswerZeroed(a),
                  "M8 a non-game host is no model");
        FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_MM, FmItem((uint8_t)GAME_NONE, hammer.id), &a) == kNone &&
                      FmAnswerZeroed(a),
                  "M8 an untagged item is no model");
        FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_MM, FmItem(7, hammer.id), &a) == kNone,
                  "M8 a non-game tag is no model");
        const uint16_t unknownOoT[] = { 0, (uint16_t)OoT_ComboModel_TestIdSpace(), 0xFFFF };
        for (uint16_t id : unknownOoT) {
            FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_MM, FmItem((uint8_t)GAME_OOT, id), &a) == kNone &&
                          FmAnswerZeroed(a),
                      "M8 an id OoT does not know is no model");
        }
        const uint16_t unknownMM[] = { (uint16_t)MM_ComboModel_TestIdSpace(), 0xFFFF };
        for (uint16_t id : unknownMM) {
            FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_OOT, FmItem((uint8_t)GAME_MM, id), &a) == kNone &&
                          FmAnswerZeroed(a),
                      "M8 an id MM does not know is no model");
        }
        FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_MM, hammer, nullptr) == kDesc,
                  "M8 a null output is tolerated");
        FM_ASSERT(OoT_ComboModel(hammer.id, nullptr) == 0, "M8 a source with no output answers no model");
    }

    // ---- M9 --------------------------------------------------------------------
    {
        static const char* const kAllowed[] = { "stdint.h",  "math.h", "stdlib.h",       "string.h",
                                                "context.h", "game.h", "foreign_model.h" };
        const char* const files[] = { "src/common/foreign_model.h", "src/common/foreign_model.c" };
        for (const char* file : files) {
            std::set<std::string> includes;
            FM_ASSERT(FmIncludes(file, &includes), "M9 the registry source is readable under RSBS_SOURCE_DIR");
            for (const std::string& inc : includes) {
                bool allowed = false;
                for (const char* ok : kAllowed) {
                    allowed |= inc == ok;
                }
                if (!allowed) {
                    printf("[TEST]   %s includes <%s>\n", file, inc.c_str());
                }
                FM_ASSERT(allowed, "M9 ADR 0002: the registry includes no game header");
            }
        }
    }

    // ---- M10 (#577 M3) -----------------------------------------------------------
    {
        SharedItem boots;
        SharedItem emerald;
        SharedItem hookshot;
        SharedItem songOfTime;
        SharedItem gohmaSoul;
        FM_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Hover Boots", &boots) &&
                      TestNamedItem((uint8_t)GAME_OOT, "Kokiri's Emerald", &emerald) &&
                      TestNamedItem((uint8_t)GAME_OOT, "Progressive Hookshot", &hookshot) &&
                      TestNamedItem((uint8_t)GAME_OOT, "Song of Time", &songOfTime) &&
                      TestNamedItem((uint8_t)GAME_OOT, "Gohma's Soul", &gohmaSoul),
                  "M10 named items");
        ComboModelAnswer bootsA;
        ComboModelAnswer emeraldA;
        ComboModelAnswer hookshotA;
        ComboModelAnswer songOfTimeA;
        ComboModelAnswer gohmaSoulA;
        FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_MM, boots, &bootsA) == kDesc &&
                      Combo_GetForeignItemModel((uint8_t)GAME_MM, emerald, &emeraldA) == kDesc &&
                      Combo_GetForeignItemModel((uint8_t)GAME_MM, hookshot, &hookshotA) == kHost &&
                      Combo_GetForeignItemModel((uint8_t)GAME_MM, songOfTime, &songOfTimeA) == kHost &&
                      Combo_GetForeignItemModel((uint8_t)GAME_MM, gohmaSoul, &gohmaSoulA) == kNone,
                  "M10 the boots and the emerald are DESCRIPTORs in MM, the (colliding) hookshot and Song of Time "
                  "are MM's own models, Gohma's Soul is no model");
        // What MM draws for the hookshot: its own row, as its own recipe gives it.
        ComboModel mmHookshot;
        const char* mmHookshotReason = nullptr;
        FM_ASSERT(MM_ComboModel_TestForDrawRow(hookshotA.hostKey, &mmHookshot, &mmHookshotReason) == 1,
                  "M10 MM draws the hookshot's host-native row with its own recipe");
        // ... and for Song of Time (#830): its host-side tinted note, one XLU list
        // (gGiSongNoteDL) and DrawSong's env colour for RI_SONG_TIME.
        ComboModel mmTimeNote;
        const char* mmTimeNoteReason = nullptr;
        static const uint8_t kTimeEnv[3] = { 98, 177, 211 };
        FM_ASSERT(MM_ComboModel_TestForHostKey(songOfTimeA.hostKey, &mmTimeNote, &mmTimeNoteReason) == 1 &&
                      FmIsTintedNote(mmTimeNote, kTimeEnv),
                  "M10 MM's model for Song of Time is its tinted note: one XLU list, env colour 98,177,211");

        ComboForeignPlacement saved[RSBS_FOREIGN_PLACEMENT_CAP];
        std::memcpy(saved, gComboCtx.foreignPlacements, sizeof(saved));
        const uint16_t check = MM_ForeignTextboxIcon_TestSomeCheck();

        Combo_ClearForeignPlacements();
        const int placedBoots = Combo_SetForeignPlacement(check, boots);
        MM_ForeignModel_TestSetMountOverride(1);
        printf("[TEST]   M10 Hover Boots on an MM check, archive mounted:\n");
        const int drawBoots = MM_ForeignModel_TestCheckQueueDraw(check, &bootsA.model);
        MM_ForeignModel_TestSetMountOverride(0);
        printf("[TEST]   M10 Hover Boots, archive NOT mounted:\n");
        const int drawUnmounted = MM_ForeignModel_TestCheckQueueDraw(check, nullptr);
        MM_ForeignModel_TestSetMountOverride(1);

        Combo_ClearForeignPlacements();
        const int placedEmerald = Combo_SetForeignPlacement(check, emerald);
        printf("[TEST]   M10 Kokiri's Emerald on an MM check, archive mounted:\n");
        const int drawEmerald = MM_ForeignModel_TestCheckQueueDraw(check, &emeraldA.model);

        Combo_ClearForeignPlacements();
        const int placedHookshot = Combo_SetForeignPlacement(check, hookshot);
        printf("[TEST]   M10 Progressive Hookshot (colliding, host-native row: MM's own hookshot), archive mounted:\n");
        const int drawHookshot = MM_ForeignModel_TestCheckQueueDraw(check, &mmHookshot);

        Combo_ClearForeignPlacements();
        const int placedTime = Combo_SetForeignPlacement(check, songOfTime);
        printf("[TEST]   M10 Song of Time (colliding, host-side tinted note, #830), archive mounted:\n");
        const int drawTime = MM_ForeignModel_TestCheckQueueDraw(check, &mmTimeNote);

        Combo_ClearForeignPlacements();
        const int placedSoul = Combo_SetForeignPlacement(check, gohmaSoul);
        printf("[TEST]   M10 Gohma's Soul (no model), archive mounted:\n");
        const int drawSoul = MM_ForeignModel_TestCheckQueueDraw(check, nullptr);
        MM_ForeignModel_TestSetMountOverride(-1);

        // The production mount branch (no override): both answers are "no".
        const int realNoPrefix = MM_ForeignModel_TestPathMountedReal("objects/object_gi_hoverboots/gGiHoverBootsDL");
        const int realMissing = MM_ForeignModel_TestPathMountedReal("__OTR__objects/rsbs_no_such_object/gRsbsNoSuchDL");
        const int realNull = MM_ForeignModel_TestPathMountedReal(nullptr);

        std::memcpy(gComboCtx.foreignPlacements, saved, sizeof(saved));
        FM_ASSERT(placedBoots >= 0 && placedEmerald >= 0 && placedHookshot >= 0 && placedTime >= 0 && placedSoul >= 0,
                  "M10 placements accepted");
        FM_ASSERT(drawBoots == 0, "M10 MM's get-item cutscene draws OoT's Hover Boots model (see the Q-line above)");
        FM_ASSERT(drawEmerald == 0, "M10 MM's get-item cutscene draws OoT's Kokiri's Emerald with its jewel shape "
                                    "(see the Q-line above)");
        FM_ASSERT(drawUnmounted == 0, "M10 unmounted, the draw keeps the model-less stand-in (see the Q-line above)");
        FM_ASSERT(drawHookshot == 0, "M10 a colliding model with a host-native row draws MM's OWN model for that row "
                                     "(see the Q-line above)");
        FM_ASSERT(drawTime == 0, "M10 Song of Time draws MM's tinted note: one XLU list and its env colour (see the "
                                 "Q-line above)");
        FM_ASSERT(drawSoul == 0, "M10 a foreign item with no model keeps the stand-in (see the Q-line above)");
        FM_ASSERT(realNoPrefix == 0, "M10 the production mount test refuses a path without the __OTR__ prefix");
        FM_ASSERT(realMissing == 0, "M10 the production mount test refuses an __OTR__ path no archive holds");
        FM_ASSERT(realNull == 0, "M10 the production mount test refuses a null path");
    }

    // ---- M12 (#800 S1) -----------------------------------------------------------
    // OoT as the host: the model call the shop shelf makes for an OoT check that
    // hosts an MM item, into a real OoT GraphicsContext. A mounted MM-exclusive
    // model emits exactly MM's descriptor; unmounted, no placement at all, or any
    // answer other than a DESCRIPTOR, it emits no model list and answers 0 (the
    // shelf keeps its stand-in).
    {
        SharedItem dekuMask;
        SharedItem mmHookshot;
        SharedItem odolwa;
        SharedItem goht;
        FM_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Deku Mask", &dekuMask) &&
                      TestNamedItem((uint8_t)GAME_MM, "Hookshot", &mmHookshot) &&
                      TestNamedItem((uint8_t)GAME_MM, "Odolwa's Remains", &odolwa) &&
                      TestNamedItem((uint8_t)GAME_MM, "Soul of Goht", &goht),
                  "M12 named items");
        ComboModelAnswer maskA;
        ComboModelAnswer hookshotA;
        ComboModelAnswer odolwaA;
        ComboModelAnswer gohtA;
        const uint8_t hookshotKind = Combo_GetForeignItemModel((uint8_t)GAME_OOT, mmHookshot, &hookshotA);
        FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_OOT, dekuMask, &maskA) == kDesc &&
                      Combo_GetForeignItemModel((uint8_t)GAME_OOT, odolwa, &odolwaA) == kDesc &&
                      Combo_GetForeignItemModel((uint8_t)GAME_OOT, goht, &gohtA) == kNone,
                  "M12 MM's Deku Mask and Odolwa's Remains (#964) are DESCRIPTORs in OoT, the Soul of Goht is no "
                  "model");
        // MM's hookshot collides, and OoT's host-native table (#577 M7) maps it to
        // OoT's own hookshot row. #832: the shelf draws that row, as OoT's own
        // recipe gives it (the lists the get-item cutscene draws for it, M13).
        FM_ASSERT(hookshotKind == kHost, "M12 MM's (colliding) hookshot is HOST_NATIVE in OoT (#577 M7)");
        ComboModel ootHookshot;
        const char* ootHookshotReason = nullptr;
        FM_ASSERT(OoT_ComboModel_TestForDrawRow(hookshotA.hostKey, &ootHookshot, &ootHookshotReason) == 1,
                  "M12 OoT draws the hookshot's host-native row with its own recipe");
        bool ootHookshotList = false;
        for (uint8_t i = 0; i < ootHookshot.partCount; i++) {
            ootHookshotList |=
                std::strcmp(ootHookshot.parts[i].dl, "__OTR__objects/object_gi_hookshot/gGiHookshotDL") == 0;
        }
        FM_ASSERT(ootHookshotList, "M12 OoT's own hookshot row draws /object_gi_hookshot/gGiHookshotDL");

        ComboForeignPlacement saved[RSBS_FOREIGN_PLACEMENT_CAP];
        std::memcpy(saved, gComboCtx.foreignPlacementsOoT, sizeof(saved));
        const uint16_t shelf = 0x0123; // opaque to the table; the host draw keys on the placement alone

        Combo_ClearForeignPlacementsOoT();
        printf("[TEST]   M12 no placement on the OoT check:\n");
        OoT_ForeignModel_TestSetMountOverride(1);
        const int drawNone = OoT_ForeignModel_TestShelfDraw(shelf, nullptr);
        const int placedMask = Combo_SetForeignPlacementOoT(shelf, dekuMask);
        printf("[TEST]   M12 Deku Mask on an OoT shelf, archive mounted:\n");
        const int drawMask = OoT_ForeignModel_TestShelfDraw(shelf, &maskA.model);
        ComboModel answered;
        const int modelMask = OoT_ForeignModel_ModelForOoTCheck(shelf, &answered);
        OoT_ForeignModel_TestSetMountOverride(0);
        printf("[TEST]   M12 Deku Mask, archive NOT mounted:\n");
        const int drawUnmounted = OoT_ForeignModel_TestShelfDraw(shelf, nullptr);
        OoT_ForeignModel_TestSetMountOverride(1);

        Combo_ClearForeignPlacementsOoT();
        const int placedHookshot = Combo_SetForeignPlacementOoT(shelf, mmHookshot);
        printf("[TEST]   M12 MM Hookshot (colliding, host-native row: OoT's own hookshot), archive mounted:\n");
        const int drawHookshot = OoT_ForeignModel_TestShelfDraw(shelf, &ootHookshot);
        ComboModel answeredHookshot;
        const int modelHookshot = OoT_ForeignModel_ModelForOoTCheck(shelf, &answeredHookshot);
        OoT_ForeignModel_TestSetMountOverride(0);
        printf("[TEST]   M12 MM Hookshot, archive NOT mounted:\n");
        const int drawHookshotUnmounted = OoT_ForeignModel_TestShelfDraw(shelf, nullptr);
        OoT_ForeignModel_TestSetMountOverride(1);

        // #964: the remains bind OBJECT_BSMASK into segment 6 in MM, which a static
        // (object, list) row cannot do; the list reads no segment, so the shelf
        // draws it as a plain curated descriptor.
        Combo_ClearForeignPlacementsOoT();
        const int placedOdolwa = Combo_SetForeignPlacementOoT(shelf, odolwa);
        printf("[TEST]   M12 MM Odolwa's Remains (#964: a curated descriptor), archive mounted:\n");
        const int drawOdolwa = OoT_ForeignModel_TestShelfDraw(shelf, &odolwaA.model);
        ComboModel answeredOdolwa;
        const int modelOdolwa = OoT_ForeignModel_ModelForOoTCheck(shelf, &answeredOdolwa);
        OoT_ForeignModel_TestSetMountOverride(0);
        printf("[TEST]   M12 MM Odolwa's Remains, archive NOT mounted:\n");
        const int drawOdolwaUnmounted = OoT_ForeignModel_TestShelfDraw(shelf, nullptr);
        OoT_ForeignModel_TestSetMountOverride(1);

        Combo_ClearForeignPlacementsOoT();
        const int placedGoht = Combo_SetForeignPlacementOoT(shelf, goht);
        printf("[TEST]   M12 MM Soul of Goht (no model), archive mounted:\n");
        const int drawGoht = OoT_ForeignModel_TestShelfDraw(shelf, nullptr);
        const int modelGoht = OoT_ForeignModel_ModelForOoTCheck(shelf, nullptr);
        OoT_ForeignModel_TestSetMountOverride(-1);

        const int realMissing =
            OoT_ForeignModel_TestPathMountedReal("__OTR__objects/rsbs_no_such_object/gRsbsNoSuchDL");
        const int realNoPrefix = OoT_ForeignModel_TestPathMountedReal("objects/object_gi_nutsmask/gGiNutsMaskDL");
        const int realNull = OoT_ForeignModel_TestPathMountedReal(nullptr);

        std::memcpy(gComboCtx.foreignPlacementsOoT, saved, sizeof(saved));
        FM_ASSERT(placedMask >= 0 && placedHookshot >= 0 && placedOdolwa >= 0 && placedGoht >= 0,
                  "M12 placements accepted");
        FM_ASSERT(drawNone == 0, "M12 an OoT check with no MM item draws no foreign model (see the M12 lines above)");
        FM_ASSERT(drawMask == 0, "M12 the shelf draws MM's Deku Mask model (see the M12 lines above)");
        FM_ASSERT(modelMask == 1 && FmModelEqual(answered, maskA.model),
                  "M12 the shelf's model for the check is exactly MM's descriptor for the item it hosts");
        FM_ASSERT(drawUnmounted == 0, "M12 unmounted, the shelf keeps its stand-in (see the M12 lines above)");
        FM_ASSERT(drawHookshot == 0, "M12 a colliding MM model with a host-native row draws OoT's OWN model for that "
                                     "row on the shelf, not the stand-in (#832; see the M12 lines above)");
        FM_ASSERT(modelHookshot == 1 && FmModelEqual(answeredHookshot, ootHookshot),
                  "M12 the shelf's model for a host-native answer is exactly OoT's own row (#832)");
        FM_ASSERT(drawHookshotUnmounted == 0,
                  "M12 a host-native row whose lists no mounted archive holds keeps the stand-in (see the M12 lines "
                  "above)");
        FM_ASSERT(drawOdolwa == 0, "M12 the shelf draws Odolwa's Remains model, not the stand-in (#964; see the M12 "
                                   "lines above)");
        FM_ASSERT(modelOdolwa == 1 && FmModelEqual(answeredOdolwa, odolwaA.model),
                  "M12 the shelf's model for the remains is exactly MM's descriptor for them (#964)");
        FM_ASSERT(drawOdolwaUnmounted == 0,
                  "M12 the remains with the curated archive unmounted keep the stand-in (see the M12 lines above)");
        FM_ASSERT(drawGoht == 0 && modelGoht == 0,
                  "M12 a no-model answer keeps the stand-in (see the M12 lines above)");
        FM_ASSERT(realMissing == 0 && realNoPrefix == 0 && realNull == 0,
                  "M12 the production mount test refuses a missing, unprefixed or null path");
    }

    // ---- M13 (#577 M4) -----------------------------------------------------------
    // OoT's get-item cutscene shows an MM item and gives nothing.
    {
        SharedItem dekuMask;
        SharedItem sword;
        SharedItem mmHookshot;
        SharedItem odolwa;
        SharedItem goht;
        FM_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Deku Mask", &dekuMask) &&
                      TestNamedItem((uint8_t)GAME_MM, "Great Fairy's Sword", &sword) &&
                      TestNamedItem((uint8_t)GAME_MM, "Hookshot", &mmHookshot) &&
                      TestNamedItem((uint8_t)GAME_MM, "Odolwa's Remains", &odolwa) &&
                      TestNamedItem((uint8_t)GAME_MM, "Soul of Goht", &goht),
                  "M13 named items");
        ComboModelAnswer maskA;
        ComboModelAnswer swordA;
        ComboModelAnswer hookshotA;
        ComboModelAnswer odolwaA;
        ComboModelAnswer gohtA;
        FM_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_OOT, dekuMask, &maskA) == kDesc &&
                      Combo_GetForeignItemModel((uint8_t)GAME_OOT, sword, &swordA) == kDesc &&
                      Combo_GetForeignItemModel((uint8_t)GAME_OOT, mmHookshot, &hookshotA) == kHost &&
                      Combo_GetForeignItemModel((uint8_t)GAME_OOT, odolwa, &odolwaA) == kDesc &&
                      Combo_GetForeignItemModel((uint8_t)GAME_OOT, goht, &gohtA) == kNone,
                  "M13 in OoT: the mask, the sword and Odolwa's Remains (#964) are DESCRIPTORs, MM's (colliding) "
                  "hookshot is OoT's own row, the Soul of Goht is no model");
        // What OoT draws for MM's hookshot: its own row, as its own recipe gives it.
        ComboModel ootHookshot;
        const char* ootHookshotReason = nullptr;
        FM_ASSERT(OoT_ComboModel_TestForDrawRow(hookshotA.hostKey, &ootHookshot, &ootHookshotReason) == 1,
                  "M13 OoT draws the hookshot's host-native row with its own recipe");

        ComboForeignPlacement saved[RSBS_FOREIGN_PLACEMENT_CAP];
        std::memcpy(saved, gComboCtx.foreignPlacementsOoT, sizeof(saved));
        const uint16_t host = 0x0124; // opaque to the table; the path keys on the placement alone

        Combo_ClearForeignPlacementsOoT();
        OoT_ForeignModel_TestSetMountOverride(1);
        printf("[TEST]   M13 an OoT check with no MM item:\n");
        const int showNothing = OoT_ForeignModel_TestShowOnlyGetItem(host, 0, nullptr, nullptr);

        const int placedMask = Combo_SetForeignPlacementOoT(host, dekuMask);
        printf("[TEST]   M13 Deku Mask on an OoT check, archive mounted:\n");
        const int showMask = OoT_ForeignModel_TestShowOnlyGetItem(host, 1, &maskA.model, "Deku Mask");
        OoT_ForeignModel_TestSetMountOverride(0);
        printf("[TEST]   M13 Deku Mask, archive NOT mounted:\n");
        const int showUnmounted = OoT_ForeignModel_TestShowOnlyGetItem(host, 1, nullptr, "Deku Mask");
        OoT_ForeignModel_TestSetMountOverride(1);

        Combo_ClearForeignPlacementsOoT();
        const int placedSword = Combo_SetForeignPlacementOoT(host, sword);
        printf("[TEST]   M13 Great Fairy's Sword on an OoT check, archive mounted:\n");
        const int showSword = OoT_ForeignModel_TestShowOnlyGetItem(host, 1, &swordA.model, "Great Fairy's Sword");

        Combo_ClearForeignPlacementsOoT();
        const int placedHookshot = Combo_SetForeignPlacementOoT(host, mmHookshot);
        printf("[TEST]   M13 MM Hookshot (colliding, host-native row: OoT's own hookshot), archive mounted:\n");
        const int showHookshot = OoT_ForeignModel_TestShowOnlyGetItem(host, 1, &ootHookshot, "Hookshot");

        Combo_ClearForeignPlacementsOoT();
        const int placedOdolwa = Combo_SetForeignPlacementOoT(host, odolwa);
        printf("[TEST]   M13 Odolwa's Remains (#964: a curated descriptor), archive mounted:\n");
        const int showOdolwa = OoT_ForeignModel_TestShowOnlyGetItem(host, 1, &odolwaA.model, "Odolwa's Remains");

        Combo_ClearForeignPlacementsOoT();
        const int placedGoht = Combo_SetForeignPlacementOoT(host, goht);
        printf("[TEST]   M13 Soul of Goht (no model), archive mounted:\n");
        const int showGoht = OoT_ForeignModel_TestShowOnlyGetItem(host, 1, nullptr, "Soul of Goht");
        OoT_ForeignModel_TestSetMountOverride(-1);

        std::memcpy(gComboCtx.foreignPlacementsOoT, saved, sizeof(saved));
        FM_ASSERT(placedMask >= 0 && placedSword >= 0 && placedHookshot >= 0 && placedOdolwa >= 0 && placedGoht >= 0,
                  "M13 placements accepted");
        FM_ASSERT(showNothing == 0, "M13 an OoT check with no MM item queues no show-only entry (see the M13 lines "
                                    "above)");
        FM_ASSERT(showMask == 0, "M13 OoT's get-item cutscene shows MM's Deku Mask model and gives nothing (see the "
                                 "M13 lines above)");
        FM_ASSERT(showSword == 0, "M13 OoT's get-item cutscene shows MM's Great Fairy's Sword model (see the M13 "
                                  "lines above)");
        FM_ASSERT(showHookshot == 0, "M13 a colliding MM model with a host-native row shows OoT's OWN model for "
                                     "that row (see the M13 lines above)");
        FM_ASSERT(showOdolwa == 0, "M13 OoT's get-item cutscene shows Odolwa's Remains model, not the stand-in (#964; "
                                   "see the M13 lines above)");
        FM_ASSERT(showUnmounted == 0 && showGoht == 0,
                  "M13 unmounted, or no model: the cutscene shows the mystery stand-in and still gives nothing (see "
                  "the M13 lines above)");
    }

    // ---- M14 (#577 M5) -----------------------------------------------------------
    // OoT's chest appearance ("Chest Size & Texture Matches Contents") for a chest
    // that hosts an MM item: the category the chest presents, asked with the junk
    // cover's category the OoT table holds there (RG_BLUE_RUPEE).
    {
        SharedItem mmHookshot;
        SharedItem odolwa;
        FM_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Hookshot", &mmHookshot) &&
                      TestNamedItem((uint8_t)GAME_MM, "Odolwa's Remains", &odolwa),
                  "M14 named items");
        ComboForeignPlacement saved[RSBS_FOREIGN_PLACEMENT_CAP];
        std::memcpy(saved, gComboCtx.foreignPlacementsOoT, sizeof(saved));
        const uint16_t chest = 0x0125; // opaque to the table; the answer keys on the placement alone

        Combo_ClearForeignPlacementsOoT();
        const int nativeShows = OoT_Rando_Foreign_TestChestShowsMajor(chest);
        const int placedHookshot = Combo_SetForeignPlacementOoT(chest, mmHookshot);
        const int hookshotShows = OoT_Rando_Foreign_TestChestShowsMajor(chest);
        Combo_ClearForeignPlacementsOoT();
        const int placedOdolwa = Combo_SetForeignPlacementOoT(chest, odolwa);
        const int odolwaShows = OoT_Rando_Foreign_TestChestShowsMajor(chest);
        std::memcpy(gComboCtx.foreignPlacementsOoT, saved, sizeof(saved));
        printf("[TEST]   M14 junk-cover chest presents (1 major, 0 the cover's junk, -1 other): no MM item %d, MM "
               "Hookshot %d, MM Odolwa's Remains %d\n",
               nativeShows, hookshotShows, odolwaShows);
        FM_ASSERT(placedHookshot >= 0 && placedOdolwa >= 0, "M14 placements accepted");
        FM_ASSERT(nativeShows == 0, "M14 a chest with no MM item keeps its own item's appearance");
        FM_ASSERT(hookshotShows == 1 && odolwaShows == 1,
                  "M14 a chest hosting an MM item presents as a major chest, never as its junk cover (the MM side's "
                  "rule for an OoT item, EnBox.cpp; see the M14 line above)");
    }

    // ---- M15 (#862) -----------------------------------------------------------
    // Every crossing-eligible id of each game (progression under the most
    // permissive frozen arming), placed alone on a chest of the other game: the
    // host's chest presents the class the item's OWN game gives it.
    {
        ComboForeignPlacement savedMM[RSBS_FOREIGN_PLACEMENT_CAP];
        ComboForeignPlacement savedOoT[RSBS_FOREIGN_PLACEMENT_CAP];
        std::memcpy(savedMM, gComboCtx.foreignPlacements, sizeof(savedMM));
        std::memcpy(savedOoT, gComboCtx.foreignPlacementsOoT, sizeof(savedOoT));
        const uint16_t chest = 0x0125; // opaque to both tables; the answer keys on the placement alone
        const uint32_t armed = ~(uint32_t)RSBS_FILL_ARM_WORLD_EVENT;

        struct Leg {
            uint8_t origin;
            const char* name;
            int (*own)(uint16_t);
            const char* (*ownName)(int);
            int (*shown)(uint16_t);
            const char* (*shownName)(int);
        };
        const Leg legs[2] = {
            { (uint8_t)GAME_OOT, "OoT item in an MM chest", OoT_Rando_Foreign_TestOwnChestCategory,
              OoT_Rando_Foreign_TestChestCategoryName, MM_Rando_Foreign_TestChestType,
              MM_Rando_Foreign_TestChestTypeName },
            { (uint8_t)GAME_MM, "MM item in an OoT chest", MM_Rando_Foreign_TestOwnChestType,
              MM_Rando_Foreign_TestChestTypeName, OoT_Rando_Foreign_TestChestCategory,
              OoT_Rando_Foreign_TestChestCategoryName },
        };
        int failures = 0;
        int placeFailures = 0;
        int gradeFailures = 0;
        int smallKeys[2] = {};
        int smallKeysMajor[2] = {};
        std::set<std::string> shownClasses[2];
        for (int l = 0; l < 2; l++) {
            const Leg& leg = legs[l];
            const ComboItemClassSource* src = Combo_GetItemClassSource(leg.origin);
            FM_ASSERT(src != nullptr && Combo_ItemClassBuild(leg.origin) > 0, "M15 the item class table builds");
            int eligible = 0;
            std::map<std::string, int> tally;
            for (uint32_t id = 1; id < src->idSpace; id++) {
                const SharedItem item = FmItem(leg.origin, (uint16_t)id);
                if (!Combo_ItemClassMayCrossUnder(item, armed)) {
                    continue;
                }
                eligible++;
                const char* own = leg.ownName(leg.own((uint16_t)id));
                // A host with no chest for the class shows major (OoT has no fairy
                // or mask chest).
                const bool hostHasClass = !(leg.origin == (uint8_t)GAME_MM &&
                                            (std::strcmp(own, "fairy") == 0 || std::strcmp(own, "mask") == 0));
                const char* want = hostHasClass ? own : "major";
                int placed;
                if (leg.origin == (uint8_t)GAME_OOT) {
                    Combo_ClearForeignPlacements();
                    placed = Combo_SetForeignPlacement(chest, item);
                } else {
                    Combo_ClearForeignPlacementsOoT();
                    placed = Combo_SetForeignPlacementOoT(chest, item);
                }
                if (placed < 0) {
                    placeFailures++;
                    continue;
                }
                // The grade the item's own game publishes through the owner
                // (shared_items.h, THE CHEST GRADE) names the same class.
                const char* graded = Combo_ChestGradeName(Combo_ItemChestGrade(item));
                if (std::strcmp(graded, own) != 0) {
                    if (gradeFailures < 6) {
                        printf("[TEST]   M15 %s: id %u is \"%s\" in its own game, its published grade is \"%s\"\n",
                               leg.name, (unsigned)id, own, graded);
                    }
                    gradeFailures++;
                }
                const char* shown = leg.shownName(leg.shown(chest));
                tally[shown]++;
                shownClasses[l].insert(shown);
                if (std::strcmp(own, "small key") == 0) {
                    smallKeys[l]++;
                    if (std::strcmp(shown, "major") == 0) {
                        smallKeysMajor[l]++;
                    }
                }
                if (std::strcmp(shown, want) != 0) {
                    if (failures < 12) {
                        const char* nm = Combo_GetForeignItemName(item);
                        printf("[TEST]   M15 %s: id %u \"%s\" is \"%s\" in its own game, its host chest shows "
                               "\"%s\" (want \"%s\")\n",
                               leg.name, (unsigned)id, nm ? nm : "?", own, shown, want);
                    }
                    failures++;
                }
            }
            std::string line;
            for (const auto& kv : tally) {
                line += " " + kv.first + "=" + std::to_string(kv.second);
            }
            printf("[TEST]   M15 %s: %d crossing-eligible ids, host chest shows:%s; small keys %d (%d shown as "
                   "major); %d mismatches so far\n",
                   leg.name, eligible, line.c_str(), smallKeys[l], smallKeysMajor[l], failures);
        }
        Combo_ClearForeignPlacements();
        Combo_ClearForeignPlacementsOoT();
        std::memcpy(gComboCtx.foreignPlacements, savedMM, sizeof(savedMM));
        std::memcpy(gComboCtx.foreignPlacementsOoT, savedOoT, sizeof(savedOoT));
        FM_ASSERT(placeFailures == 0, "M15 every crossing-eligible id places on a chest of the other game");
        FM_ASSERT(gradeFailures == 0, "M15 each game publishes its own chest class as the item's grade (see the M15 "
                                      "lines above)");
        FM_ASSERT(smallKeys[0] > 0 && smallKeys[1] > 0, "M15 both games have crossing-eligible small keys");
        FM_ASSERT(smallKeysMajor[0] == 0 && smallKeysMajor[1] == 0,
                  "M15 a crossing small key never presents as a major chest (see the M15 lines above)");
        FM_ASSERT(failures == 0, "M15 a crossing chest presents the crossed item's own class, as OoTMM grades it "
                                 "(see the M15 lines above)");
        FM_ASSERT(shownClasses[0].count("token") == 1 && shownClasses[0].count("boss key") == 1 &&
                      shownClasses[1].count("token") == 1 && shownClasses[1].count("boss key") == 1,
                  "M15 the key and token classes are populated in both directions");
    }

    printf("[TEST] ForeignModel: PASS\n");
    return TEST_PASS;
}

#undef FM_ASSERT
