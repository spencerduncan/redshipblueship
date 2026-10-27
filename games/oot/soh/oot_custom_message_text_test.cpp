/**
 * @file oot_custom_message_text_test.cpp
 * #1187 locks: the message-text halves of SoH 983d04d36 that PR #1181 left out.
 *
 * ROW CustomMessageReplace (redship tier, no bring-up: CustomMessage is plain strings).
 *   THE DEFECT. CustomMessage::Replace searched again from position 0 after each
 *   replacement, so a replacement that contains the search string matched itself
 *   forever and the call never returned. AutoFormat(ItemID) is one such caller in
 *   production: it replaces WAIT_FOR_INPUT with WAIT_FOR_INPUT + the item icon, so
 *   any item text long enough to need a second page hung the game.
 *   R1. "banana", "a" -> "aa" reads "baanaanaa" in every language (the string overload).
 *   R2. the same through the CustomMessage overload.
 *   R3. a two-page message through AutoFormat(ITEM_DUNGEON_MAP) returns, and both
 *       pages open with the map icon.
 *   Each call runs on a worker thread under a watchdog: the defect never returns,
 *   so a call still running after the deadline is reported and the process exits
 *   with a failure code at once (the runaway thread would otherwise keep growing
 *   its string until the process ends).
 *
 * ROW MapCompassText (rando tier: it generates for the context BuildMapMessage reads).
 *   M1. a dungeon map's get-item text names the map ("Great Deku Tree Map"), never
 *       the "[[name]]" placeholder (BuildMapMessage never replaced it).
 *   M2. it is auto-formatted for the map: it opens with the map's item icon and
 *       ends with the message end code (BuildMapMessage never called AutoFormat).
 *   C1. every compass row of the item table opens TEXT_ITEM_COMPASS and every map
 *       row TEXT_ITEM_DUNGEON_MAP (the Great Deku Tree Compass opened the map's).
 *
 * RED: without the Replace fix R1-R3 time out; without the BuildMapMessage half M1
 * and M2 fail; without the item_list.cpp half C1 fails on RG_DEKU_TREE_COMPASS.
 */

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"

// 3drando/menu.cpp: the headless generation bridge every rando row uses.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
// Messages/ItemMessages.cpp: dungeon map `rg`'s get-item text, raw English.
extern "C" int OoT_Rando_MapMessageText(uint16_t rg, char* out, size_t cap);

namespace {

constexpr auto kCmtDeadline = std::chrono::seconds(10);

/**
 * Runs `work` on a worker thread and returns what it produced. When it has not
 * returned by the deadline the defect is the infinite loop: report it and exit the
 * process with a failure code, since the thread cannot be stopped.
 */
std::string CmtRunGuarded(const char* leg, std::function<std::string()> work) {
    struct State {
        std::mutex mutex;
        std::condition_variable done;
        bool finished = false;
        std::string result;
    };
    auto state = std::make_shared<State>();
    std::thread worker([state, work]() {
        std::string result = work();
        std::lock_guard<std::mutex> lock(state->mutex);
        state->result = std::move(result);
        state->finished = true;
        state->done.notify_one();
    });
    std::unique_lock<std::mutex> lock(state->mutex);
    if (!state->done.wait_for(lock, kCmtDeadline, [&state]() { return state->finished; })) {
        printf("[TEST] FAIL %s: CustomMessage::Replace did not return within %lld s; a replacement that contains "
               "the search string is matched again forever (#1187)\n",
               leg, (long long)kCmtDeadline.count());
        fflush(stdout);
        fflush(stderr);
        std::_Exit(1);
    }
    lock.unlock();
    worker.join();
    return state->result;
}

/** The three languages' raw text, joined with '|'. */
std::string CmtAllRaw(const CustomMessage& msg) {
    return msg.GetEnglish(MF_RAW) + "|" + msg.GetGerman(MF_RAW) + "|" + msg.GetFrench(MF_RAW);
}

/** Printable form of a raw message: control bytes as \xNN. */
std::string CmtPrintable(const std::string& raw) {
    std::string out;
    for (const unsigned char c : raw) {
        if (c < 0x20 || c >= 0x7F) {
            char hex[8];
            snprintf(hex, sizeof(hex), "\\x%02X", (unsigned)c);
            out += hex;
        } else {
            out += (char)c;
        }
    }
    return out;
}

size_t CmtCount(const std::string& haystack, const std::string& needle) {
    size_t count = 0;
    for (size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + needle.size())) {
        count++;
    }
    return count;
}

} // namespace

extern "C" int OoT_CustomMessageReplaceTest_Run(void) {
    printf("[TEST] custom-message-replace: CustomMessage::Replace returns when the replacement contains the search "
           "string (#1187)\n");
    int failures = 0;

    // R1. The string overload.
    const std::string r1 = CmtRunGuarded("R1", []() {
        CustomMessage msg("banana", "banana", "banana");
        msg.Replace("a", "aa");
        return CmtAllRaw(msg);
    });
    if (r1 != "baanaanaa|baanaanaa|baanaanaa") {
        printf("[TEST] FAIL R1: \"banana\" with \"a\" -> \"aa\" reads \"%s\", not \"baanaanaa\" in each language\n",
               r1.c_str());
        failures++;
    }

    // R2. The CustomMessage overload.
    const std::string r2 = CmtRunGuarded("R2", []() {
        CustomMessage msg("banana", "banana", "banana");
        msg.Replace("a", CustomMessage("aa", "aa", "aa"));
        return CmtAllRaw(msg);
    });
    if (r2 != "baanaanaa|baanaanaa|baanaanaa") {
        printf("[TEST] FAIL R2: the CustomMessage overload reads \"%s\", not \"baanaanaa\" in each language\n",
               r2.c_str());
        failures++;
    }

    // R3. AutoFormat(ItemID), the production caller: two pages, each opening with the icon.
    const std::string icon = CustomMessage::ITEM_OBTAINED(ITEM_DUNGEON_MAP);
    const std::string r3 = CmtRunGuarded("R3", []() {
        CustomMessage msg("You found the map!^It shows the whole dungeon.", TEXTBOX_TYPE_BLUE);
        msg.AutoFormat(ITEM_DUNGEON_MAP);
        return msg.GetEnglish(MF_RAW);
    });
    const std::string page = CustomMessage::WAIT_FOR_INPUT();
    if (r3.rfind(icon, 0) != 0 || CmtCount(r3, page + icon) != 1 || CmtCount(r3, icon) != 2) {
        printf("[TEST] FAIL R3: a two-page AutoFormat(ITEM_DUNGEON_MAP) reads \"%s\"; each page must open with the "
               "map icon exactly once\n",
               CmtPrintable(r3).c_str());
        failures++;
    }

    if (failures != 0) {
        printf("[TEST] FAIL: custom-message-replace: %d assertion(s) failed\n", failures);
        return 1;
    }
    printf("[TEST] PASS: Replace returns and replaces every occurrence once, in both overloads and through "
           "AutoFormat(ItemID)\n");
    return 0;
}

extern "C" int RandoTest_MapCompassText(void) {
    printf("[TEST] map-compass-text: a dungeon map's text names the map and is formatted; every compass opens the "
           "compass text (#1187)\n");
    if (Rando_HeadlessSeedTest("RSBSMAPTEXT1") != 0) {
        printf("[TEST] FAIL: headless generation failed, so there is no context to read the map text from\n");
        return 1;
    }
    int failures = 0;

    // M1 / M2: the Great Deku Tree Map's get-item text.
    char text[2048] = {};
    if (OoT_Rando_MapMessageText(static_cast<uint16_t>(RG_DEKU_TREE_MAP), text, sizeof(text)) < 0) {
        printf("[TEST] FAIL: the map text bridge refused RG_DEKU_TREE_MAP\n");
        return 1;
    }
    const std::string raw = text;
    const std::string name = Rando::StaticData::RetrieveItem(RG_DEKU_TREE_MAP).GetName().GetEnglish();
    printf("[TEST] map-compass-text: the Great Deku Tree Map reads \"%s\"\n", CmtPrintable(raw).c_str());
    if (name.empty() || raw.find(name) == std::string::npos || raw.find("[[name]]") != std::string::npos) {
        printf("[TEST] FAIL M1: the map's text does not name \"%s\" (or still carries [[name]])\n", name.c_str());
        failures++;
    }
    const std::string icon = CustomMessage::ITEM_OBTAINED(ITEM_DUNGEON_MAP);
    const std::string end = CustomMessage::MESSAGE_END();
    if (raw.rfind(icon, 0) != 0 || raw.size() < end.size() ||
        raw.compare(raw.size() - end.size(), end.size(), end) != 0) {
        printf("[TEST] FAIL M2: the map's text is not auto-formatted: it must open with the map icon and end with "
               "the message end code\n");
        failures++;
    }

    // C1: every map and compass row of the item table.
    int maps = 0;
    int compasses = 0;
    for (int rg = RG_NONE + 1; rg < RG_MAX; rg++) {
        const Rando::Item& item = Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rg));
        const ItemType type = item.GetItemType();
        if (type != ITEMTYPE_MAP && type != ITEMTYPE_COMPASS) {
            continue;
        }
        const uint16_t want = type == ITEMTYPE_MAP ? TEXT_ITEM_DUNGEON_MAP : TEXT_ITEM_COMPASS;
        const uint16_t got = item.GetGIEntry_Copy().textId;
        (type == ITEMTYPE_MAP ? maps : compasses)++;
        if (got != want) {
            printf("[TEST] FAIL C1: \"%s\" (%d) opens text 0x%04X; a %s opens 0x%04X\n",
                   item.GetName().GetEnglish().c_str(), rg, (unsigned)got, type == ITEMTYPE_MAP ? "map" : "compass",
                   (unsigned)want);
            failures++;
        }
    }
    printf("[TEST] map-compass-text: %d map row(s), %d compass row(s) checked\n", maps, compasses);
    if (maps == 0 || compasses == 0) {
        printf("[TEST] FAIL C1: the item table has no map or no compass row, so C1 proves nothing\n");
        failures++;
    }

    if (failures != 0) {
        printf("[TEST] FAIL: map-compass-text: %d assertion(s) failed\n", failures);
        return 1;
    }
    printf("[TEST] PASS: the map's text names the map and opens with its icon; every compass opens the compass "
           "text\n");
    return 0;
}
