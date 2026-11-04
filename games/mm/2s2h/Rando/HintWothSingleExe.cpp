/**
 * @file HintWothSingleExe.cpp
 * @brief The free gossip stone's Way of the Hero variant (#1439), in MM's voice.
 *
 * The Way of the Hero itself is src/common's (combo_woth.h): the pair-level
 * required set the creation event's single-bag fill computes over BOTH games'
 * hosts once the world is proved, and the areas it touches, frozen into the
 * .redsave's combo record (gComboCtx.comboWoth). A stone cannot recompute it (it
 * takes rounds over both engines, which only the creation has live), so this TU
 * reads the frozen record: Majora's Mask's own areas and Ocarina of Time's, the
 * latter named as the shared vocabulary names them ("Kokiri Forest"), the way
 * #575 H3c's stones already name an OoT area for an MM item that crossed.
 *
 * OoTMM's sentence (packages/generator/src/common/hint.c, HINT_TYPE_PATH): "They
 * say that <region> is on the Way of the Hero." In the stone's own voice here:
 * "They say", the area in yellow as the item hint names it, and "Way of the
 * Hero" in yellow as OoTMM colours it.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "2s2h/CustomMessage/CustomMessage.h"
#include "Rando/HintWoth.h"

#include "combo_area.h"    // src/common: the shared vocabulary
#include "combo_woth.h"    // src/common: the pair's frozen Way of the Hero
#include "foreign_items.h" // src/common: Combo_ComboLogicClaimsAllowed (No Logic claims nothing)
#include "game.h"          // src/common: GAME_MM / GAME_OOT

namespace {

/** The Way of the Hero stream's domain tag ("WOTH"): the same stone seed as the
 *  item pick, under its own tag, in a generator of its own. */
constexpr uint64_t kFreeStoneWothDomain = 0x574F5448ull;

/** One draw of the stone's own stream (splitmix64), which it advances. Local for
 *  the reason HintFoolishSingleExe.cpp's is: MM's global Ship_Random stream is
 *  the item pick's and the flavour line's, and must not move. */
uint64_t NextStoneDraw(uint64_t* state) {
    uint64_t x = (*state += 0x9E3779B97F4A7C15ull);
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

/** Every area on the frozen path: Majora's Mask's first, then Ocarina of Time's,
 *  each half ascending, so the list is a pure function of the record. */
std::vector<ComboArea> FrozenWothAreas() {
    std::vector<ComboArea> areas;
    const ComboWothRecord* rec = Combo_Woth_FrozenRecord();
    if (!Combo_WothRecord_Present(rec)) {
        return areas;
    }
    const uint8_t games[2] = { (uint8_t)GAME_MM, (uint8_t)GAME_OOT };
    for (const uint8_t game : games) {
        uint8_t ids[0x100];
        const int n = Combo_WothRecord_Areas(rec, game, ids, (int)sizeof(ids));
        for (int i = 0; i < n && i < (int)sizeof(ids); i++) {
            areas.push_back(ComboArea{ game, ids[i] });
        }
    }
    return areas;
}

} // namespace

std::string Rando::HintWoth::FreeStoneText(uint32_t stoneSeed) {
    // No Logic (combo rung `none`) proved nothing, so nothing is claimed needed
    // (the creation froze no record then either; this is the reader's half).
    if (!Combo_ComboLogicClaimsAllowed()) {
        return "";
    }
    const std::vector<ComboArea> areas = FrozenWothAreas();
    if (areas.empty()) {
        return "";
    }
    uint64_t stream = ((uint64_t)stoneSeed << 32) ^ kFreeStoneWothDomain;
    if (NextStoneDraw(&stream) % (uint64_t)kFreeStoneOneIn != 0) {
        return "";
    }
    const ComboArea area = areas[(size_t)(NextStoneDraw(&stream) % areas.size())];
    const char* name = Combo_Area_Name(area);
    if (name == nullptr) {
        return "";
    }
    std::string msg = "They say %y{{location}}%w is on the %yWay of the Hero%w.";
    CustomMessage::Replace(&msg, "{{location}}", name);
    return msg;
}

/** TEST BRIDGE (redship tier; src/common/tests/test_woth_hints.c, #1439): what
 *  Rando::HintWoth::FreeStoneText says for stone seed `stoneSeed`, before colours
 *  and line breaks. Returns the full length (0 when the stone keeps its item
 *  hint), or -1 for a null buffer. */
extern "C" int MM_HintWoth_TestFreeStoneText(uint32_t stoneSeed, char* out, int cap) {
    if (out == nullptr || cap <= 0) {
        return -1;
    }
    const std::string msg = Rando::HintWoth::FreeStoneText(stoneSeed);
    snprintf(out, (size_t)cap, "%s", msg.c_str());
    return (int)msg.size();
}

#endif /* RSBS_SINGLE_EXECUTABLE */
