/**
 * @file HintFoolishSingleExe.cpp
 * @brief MM's half of the shared foolish rule and the free stone's foolish
 *        variant (epic #575, child H5a).
 *
 * The rule itself is src/common's (combo_area.h, THE FOOLISH RULE: an area is
 * foolish when one of its checks is hintable and none holds anything
 * important). This TU answers for MM's own hosts, from the frozen world only:
 * MM's save table (RANDO_SAVE_CHECKS) and the crossing store, which is why MM
 * can recompute the set every time a stone is read and persists nothing.
 * ADR 0002: MM's ids are read only here; what crosses into src/common is a
 * standing bit pair per check id of MM's own id-space.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <string>
#include <vector>

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/ShipUtils.h"
#include "Rando/Foreign.h"
#include "Rando/HintArea.h"
#include "Rando/HintFoolish.h"
#include "Rando/Rando.h"
#include "Rando/StaticData/StaticData.h"

#include "combo_area.h"   // src/common: the shared vocabulary and the foolish rule
#include "context.h"      // src/common: SharedItem
#include "game.h"         // src/common: GAME_MM
#include "shared_items.h" // src/common: the item-class owner

namespace {

/**
 * One MM check's standing (ComboFoolishAdapter::hostStanding).
 *
 * Only SHUFFLED checks count, as OoTMM's rule counts only fully shuffled
 * locations: an unshuffled check gives its vanilla item, which the player
 * already knows. Such a check is HINTABLE when it has an area a hint can name
 * (the stone's own filter, Rando::HintArea::HasArea). It is IMPORTANT when:
 *  - it is a crossing host (Rando::Foreign::IsForeignCheck, the crossing store):
 *    it physically holds MM's junk cover, and a crossing is always important;
 *  - or the item it holds is not filler. The item-class owner's RECONCILED class
 *    (shared_items.h) is PROGRESSION for exactly the rows the single bag places
 *    (THE BAG COMPOSITION RULE admits every PROGRESSION row and nothing else),
 *    so "a placement the coordinator made" is recomputable from the save without
 *    the coordinator's tables, which a load does not restore. It also covers MM's
 *    own fill predicate (non-junk, non-health), and a shared-health heart, which
 *    reconciles to PROGRESSION. A class the owner cannot give yet (NONE) is
 *    IMPORTANT: an unknown is never a foolish claim.
 */
uint8_t MmHostStanding(uint16_t check) {
    if (check <= RC_UNKNOWN || check >= RC_MAX) {
        return 0;
    }
    const RandoSaveCheck& saveCheck = RANDO_SAVE_CHECKS[check];
    if (!saveCheck.shuffled) {
        return 0;
    }
    uint8_t standing = Rando::HintArea::HasArea((RandoCheckId)check) ? COMBO_HOST_HINTABLE : 0;
    if (Rando::Foreign::IsForeignCheck((RandoCheckId)check)) {
        return standing | COMBO_HOST_IMPORTANT;
    }
    SharedItem item;
    item.originGame = (uint8_t)GAME_MM;
    item.flags = 0;
    item.id = (uint16_t)saveCheck.randoItemId;
    switch (Combo_ItemClassOf(item)) {
        case RSBS_FILL_CLASS_JUNK:
        case RSBS_FILL_CLASS_RENEWABLE:
        case RSBS_FILL_CLASS_TRAP:
            return standing;
        default:
            return standing | COMBO_HOST_IMPORTANT;
    }
}

const ComboFoolishAdapter kMmFoolishAdapter = { (uint16_t)RC_MAX, MmHostStanding, nullptr };

/** The describer's registrar shape: stores a pointer, calls nothing. 2ship_rando
 *  links WHOLE_ARCHIVE, so it survives. */
struct MmFoolishAdapterRegistrar {
    MmFoolishAdapterRegistrar() {
        Combo_RegisterFoolishAdapter((uint8_t)GAME_MM, &kMmFoolishAdapter);
    }
};
const MmFoolishAdapterRegistrar gMmFoolishAdapterRegistrar;

/** The foolish stream's domain tag ("FOOL"): the same stone seed as the item
 *  pick, a different stream, so the item pick a stone keeps never moves. */
constexpr uint64_t kFreeStoneFoolishDomain = 0x464F4F4Cull;

/** splitmix64's finalizer: Ship_Random's PCG returns its first draw straight off
 *  the seeded state, so neighbouring stone seeds would otherwise draw alike. */
uint64_t MixStoneSeed(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

} // namespace

std::string Rando::HintFoolish::FreeStoneText(uint32_t stoneSeed) {
    ComboFoolishAreas foolish;
    if (Combo_Area_ComputeFoolish((uint8_t)GAME_MM, &foolish) <= 0) {
        return "";
    }
    std::vector<uint8_t> areas;
    for (int a = 1; a <= Combo_Area_Count((uint8_t)GAME_MM); a++) {
        if (foolish.foolish[a]) {
            areas.push_back((uint8_t)a);
        }
    }
    // Ship_Random(min, max) draws from [min, max): its upper bound is EXCLUSIVE.
    Ship_Random_Seed(MixStoneSeed(((uint64_t)stoneSeed << 32) ^ kFreeStoneFoolishDomain));
    if (Ship_Random(0, kFreeStoneOneIn) != 0) {
        return "";
    }
    const ComboArea area = { (uint8_t)GAME_MM, areas[(size_t)Ship_Random(0, (s32)areas.size())] };
    // OoTMM's foolish sentence ("plundering <region> is a foolish choice", in
    // pink) in the stone's own voice ("They say ...", the area in yellow as the
    // item hint names it).
    std::string msg = "They say plundering %y{{location}}%w is a %pfoolish choice%w.";
    CustomMessage::Replace(&msg, "{{location}}", Combo_Area_Name(area));
    return msg;
}

/** TEST BRIDGE (combo-single-bag D2; #575 H5a): MM's standing for one check of
 *  the live world (COMBO_HOST_* bits), as the foolish rule reads it. */
extern "C" int MM_HintFoolish_TestHostStanding(uint16_t check) {
    return (int)MmHostStanding(check);
}

#endif /* RSBS_SINGLE_EXECUTABLE */
