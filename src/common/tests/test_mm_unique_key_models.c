/**
 * @file test_mm_unique_key_models.c
 * @brief MmUniqueKeyModels: Majora's Mask's small and boss keys draw 2S2H's
 *        dungeon-specific key models (#1172).
 *
 * ROM-free, redship tier. Ship of Harkinian draws each dungeon's keys with their
 * own model; 2Ship gained the same with its "Unique Key Models" (2S2H 5d96fd84f,
 * f56c7939e): one small key body and one boss key body shared by every dungeon,
 * each carrying the dungeon's emblem. DrawSmallKey and DrawBossKey
 * (games/mm/2s2h/Rando/DrawItem.cpp) choose the model through
 * MM_Rando_UniqueKeyModel, which this row feeds each of MM's eight dungeon keys,
 * looked up by name:
 *
 *   1. a small key answers the shared small key body and its dungeon's emblem;
 *      a boss key the shared boss key body and the same emblem;
 *   2. an item that is not a dungeon key (a stray fairy, an id past the table)
 *      answers no unique model, so it keeps its own draw;
 *   3. every display list answered is a resource of 2ship.o2r's source tree
 *      (games/mm/assets/custom, RSBS_SOURCE_DIR), as is every object_gi_key
 *      resource those lists call or load (their vertex lists).
 *
 * The draw itself loads resources and falls back to the vanilla tinted model
 * when the mounted 2ship.o2r predates the models: the playtest shows it.
 *
 * Linkage note: #included into test_runner.cpp at file scope (compiled as C++).
 */

#include "../foreign_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
// games/mm/2s2h/Rando/DrawItem.cpp (#1172)
int MM_Rando_UniqueKeyModel(uint16_t randoItemId, const char** body, const char** emblem);
}

namespace {

constexpr const char* kUkmSmallBody = "__OTR__objects/object_gi_key/gGiDungeonSmallKeyDL";
constexpr const char* kUkmBossBody = "__OTR__objects/object_gi_key/gGiDungeonBossKeyDL";

struct UkmCase {
    const char* item;
    const char* body;
    const char* emblem;
};

const UkmCase kUkmCases[] = {
    { "Woodfall Small Key", kUkmSmallBody, "__OTR__objects/object_gi_key/gGiWoodfallKeyEmblemDL" },
    { "Snowhead Small Key", kUkmSmallBody, "__OTR__objects/object_gi_key/gGiSnowheadKeyEmblemDL" },
    { "Great Bay Small Key", kUkmSmallBody, "__OTR__objects/object_gi_key/gGiGreatBayKeyEmblemDL" },
    { "Stone Tower Small Key", kUkmSmallBody, "__OTR__objects/object_gi_key/gGiStoneTowerKeyEmblemDL" },
    { "Woodfall Boss Key", kUkmBossBody, "__OTR__objects/object_gi_key/gGiWoodfallKeyEmblemDL" },
    { "Snowhead Boss Key", kUkmBossBody, "__OTR__objects/object_gi_key/gGiSnowheadKeyEmblemDL" },
    { "Great Bay Boss Key", kUkmBossBody, "__OTR__objects/object_gi_key/gGiGreatBayKeyEmblemDL" },
    { "Stone Tower Boss Key", kUkmBossBody, "__OTR__objects/object_gi_key/gGiStoneTowerKeyEmblemDL" },
};

/** The whole text of 2ship.o2r's source resource for `otrPath` (an "__OTR__"
 *  path), or false when the checkout has no such file. */
bool UkmReadCustomResource(const char* otrPath, std::string* out) {
#ifdef RSBS_SOURCE_DIR
    const char* rel = std::strncmp(otrPath, "__OTR__", 7) == 0 ? otrPath + 7 : otrPath;
    FILE* f = fopen((std::string(RSBS_SOURCE_DIR) + "/games/mm/assets/custom/" + rel).c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    char buf[4096];
    size_t n;
    out->clear();
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        out->append(buf, n);
    }
    fclose(f);
    return true;
#else
    (void)otrPath;
    (void)out;
    return false;
#endif
}

/** (3) `otrPath` is in the source tree, and so is every object_gi_key resource it
 *  names in a Path="..." attribute. Prints and counts what is missing. */
int UkmMissingResources(const char* otrPath) {
    std::string text;
    if (!UkmReadCustomResource(otrPath, &text)) {
        printf("[TEST]   %s is not in games/mm/assets/custom\n", otrPath);
        return 1;
    }
    int missing = 0;
    const std::string key = "Path=\"objects/object_gi_key/";
    for (size_t at = text.find(key); at != std::string::npos; at = text.find(key, at + 1)) {
        const size_t start = at + 6; // past Path="
        const size_t end = text.find('"', start);
        if (end == std::string::npos) {
            break;
        }
        const std::string ref = text.substr(start, end - start);
        std::string unused;
        if (!UkmReadCustomResource(ref.c_str(), &unused)) {
            printf("[TEST]   %s names %s, which is not in games/mm/assets/custom\n", otrPath, ref.c_str());
            missing++;
        }
    }
    return missing;
}

} // namespace

TestResult Test_MmUniqueKeyModels(void) {
    printf("[TEST] mm-unique-key-models: MM's dungeon keys draw 2S2H's dungeon key models (#1172)\n");
    bool ok = true;

    // (1) and (3): the eight dungeon keys.
    for (const UkmCase& c : kUkmCases) {
        SharedItem item;
        if (!TestNamedItem((uint8_t)GAME_MM, c.item, &item)) {
            printf("[TEST] FAIL: MM has no item named %s\n", c.item);
            ok = false;
            continue;
        }
        const char* body = nullptr;
        const char* emblem = nullptr;
        const int unique = MM_Rando_UniqueKeyModel(item.id, &body, &emblem);
        printf("[TEST]   %s (id %u): unique %d, body %s, emblem %s\n", c.item, (unsigned)item.id, unique,
               body ? body : "(none)", emblem ? emblem : "(none)");
        if (unique != 1 || body == nullptr || emblem == nullptr || std::strcmp(body, c.body) != 0 ||
            std::strcmp(emblem, c.emblem) != 0) {
            printf("[TEST] FAIL: %s does not answer its dungeon key model (want body %s, emblem %s)\n", c.item, c.body,
                   c.emblem);
            ok = false;
            continue;
        }
        if (UkmMissingResources(body) + UkmMissingResources(emblem) != 0) {
            printf("[TEST] FAIL: %s's key model is not in 2ship.o2r's source tree (see the lines above)\n", c.item);
            ok = false;
        }
    }

    // (2) Not a dungeon key: no unique model.
    SharedItem fairy;
    if (!TestNamedItem((uint8_t)GAME_MM, "Clock Town Stray Fairy", &fairy)) {
        printf("[TEST] FAIL: MM has no item named Clock Town Stray Fairy\n");
        ok = false;
    } else {
        const char* body = "unset";
        const char* emblem = "unset";
        if (MM_Rando_UniqueKeyModel(fairy.id, &body, &emblem) != 0 || body != nullptr || emblem != nullptr) {
            printf("[TEST] FAIL: a stray fairy answers a key model\n");
            ok = false;
        }
    }
    const char* body = "unset";
    const char* emblem = "unset";
    if (MM_Rando_UniqueKeyModel(0xFFFF, &body, &emblem) != 0 || body != nullptr || emblem != nullptr) {
        printf("[TEST] FAIL: an id past MM's item table answers a key model\n");
        ok = false;
    }

    printf("[TEST] mm-unique-key-models: %s\n", ok ? "PASS" : "FAIL");
    return ok ? TEST_PASS : TEST_FAIL;
}
