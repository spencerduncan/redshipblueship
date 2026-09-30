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
 *  M8 FALLBACKS: native, untagged, non-game and unknown ids answer no model.
 *  M9 ADR 0002: foreign_model.{h,c} include no game header.
 *
 * Not lockable headless: the pixels. M3/M4 of the epic draw these descriptors.
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
int MM_ComboModel_TestDrawRowLists(int drawId, const char* lists[COMBO_MODEL_MAX_PARTS]);
const char* MM_ComboModel_TestItemReason(uint16_t id);
int OoT_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
int MM_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
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
           "with no mapping yet), %d no model; %d failures\n",
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
    FM_ASSERT(byKind[COMBO_MODEL_ANSWER_HOST_NATIVE] == 0,
              "M4 no production host-native mapping exists before #577 M7 (its table is empty)");
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
};

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
    printf("[TEST]   pin: %-24s -> kind %u, %u parts, %s\n", pin.name, (unsigned)kind, (unsigned)answer.model.partCount,
           answer.model.partCount > 0 ? answer.model.parts[0].dl : "-");
    if (!ok) {
        printf("[TEST] FAIL: \"%s\" wants kind %u, %u parts, first \"...%s\"\n", pin.name, (unsigned)pin.kind,
               (unsigned)pin.partCount, pin.firstListTail != nullptr ? pin.firstListTail : "-");
        return TEST_FAIL;
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
    // OoT: the Triforce piece and the fishing pole. MM: the three empty rows,
    // the seahorse, the bottled fairy, the Moon's Tear and the four remains.
    const FmGame oot = { "OoT",
                         (uint8_t)GAME_OOT,
                         (uint8_t)GAME_MM,
                         OoT_ComboModel_TestDrawRowCount,
                         OoT_ComboModel_TestForDrawRow,
                         OoT_ComboModel_TestDrawRowLists,
                         OoT_ComboModel_TestIdSpace,
                         OoT_ComboLogic_ClassifyItem,
                         OoT_ComboModel_TestItemReason,
                         2 };
    const FmGame mm = { "MM",
                        (uint8_t)GAME_MM,
                        (uint8_t)GAME_OOT,
                        MM_ComboModel_TestDrawRowCount,
                        MM_ComboModel_TestForDrawRow,
                        MM_ComboModel_TestDrawRowLists,
                        MM_ComboModel_TestIdSpace,
                        MM_ComboLogic_ClassifyItem,
                        MM_ComboModel_TestItemReason,
                        10 };
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

    // ---- M4 --------------------------------------------------------------------
    if (FmWalkProgression(oot) != TEST_PASS || FmWalkProgression(mm) != TEST_PASS) {
        return TEST_FAIL;
    }

    // ---- M5 --------------------------------------------------------------------
    const uint8_t kDesc = COMBO_MODEL_ANSWER_DESCRIPTOR;
    const uint8_t kNone = COMBO_MODEL_ANSWER_NONE;
    const uint8_t kOpa = COMBO_MODEL_LAYER_OPA;
    const uint8_t kXlu = COMBO_MODEL_LAYER_XLU;
    const FmPin pins[] = {
        { (uint8_t)GAME_OOT, "Megaton Hammer", kDesc, "/object_gi_hammer/gGiHammerDL", 1, kOpa },
        { (uint8_t)GAME_OOT, "Hover Boots", kDesc, "/object_gi_hoverboots/gGiHoverBootsDL", 1, kOpa },
        { (uint8_t)GAME_OOT, "Kokiri's Emerald", kDesc, "/object_gi_jewel/gGiKokiriEmeraldGemDL", 2, kXlu },
        { (uint8_t)GAME_OOT, "Forest Medallion", kDesc, "/object_gi_medal/gGiForestMedallionFaceDL", 2, kOpa },
        { (uint8_t)GAME_OOT, "Master Sword", kDesc, "/object_toki_objects/object_toki_objects_DL_001BD0", 1, kOpa },
        { (uint8_t)GAME_OOT, "Roc's Feather", kDesc, "/object_rocs_feather/gGiRocsFeatherDL", 1, kXlu },
        // object_gi_hookshot and object_gi_key are in both archives: no mapping yet.
        { (uint8_t)GAME_OOT, "Progressive Hookshot", kNone, nullptr, 0, 0 },
        { (uint8_t)GAME_OOT, "Forest Temple Small Key", kNone, nullptr, 0, 0 },
        { (uint8_t)GAME_OOT, "Gohma's Soul", kNone, nullptr, 0, 0 },
        { (uint8_t)GAME_MM, "Deku Mask", kDesc, "/object_gi_nutsmask/gGiDekuMaskEmptyDL", 2, kOpa },
        { (uint8_t)GAME_MM, "Hookshot", kNone, nullptr, 0, 0 },
        { (uint8_t)GAME_MM, "Odolwa's Remains", kNone, nullptr, 0, 0 },
    };
    for (const FmPin& pin : pins) {
        if (FmCheckPin(pin) != TEST_PASS) {
            return TEST_FAIL;
        }
    }
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
        FM_ASSERT(OoT_ComboModelHostNative(&direct, &key) == 0 && MM_ComboModelHostNative(&direct, &key) == 0 &&
                      OoT_ComboModelHostNative(nullptr, &key) == 0,
                  "M7 the production mappers are empty until #577 M7 adds rows");
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

    printf("[TEST] ForeignModel: PASS\n");
    return TEST_PASS;
}

#undef FM_ASSERT
