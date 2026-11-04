/**
 * @file TimeTravelSword.cpp
 * @brief OoTMM's "Time Travel requires Master Sword" (#1434) for Ocarina of
 *        Time's randomizer: RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD, on by default.
 *
 * OoTMM (5f352be01). The setting is `timeTravelSword`, "Choose whether or not
 * Link needs the Master Sword to travel through time", default true
 * (packages/core/src/settings/data.ts:1240-1246). The generator hands it to the
 * game as OOT_TIME_TRAVEL_REQUIRES_MS (generator/lib/combo/randomizer/
 * world-config.ts:246), and the pedestal offers the pull only to a Link who owns
 * the sword, at either age (generator/src/oot/actors/Bg/Bg_Toki_Swd.c:
 * `!Config_Flag(CFG_OOT_TIME_TRAVEL_REQUIRES_MS) || (swords & EQ_OOT_SWORD_MASTER)`
 * before Actor_OfferCarry). Its logic makes the age change an event of the Temple
 * of Time, `TIME_TRAVEL: event(DOOR_OF_TIME_OPEN) && (has_sword_master ||
 * !setting(timeTravelSword))` (data/world/oot/overworld/temple_of_time.yml), and
 * the other spawn needs that event (data/world/oot/overworld/_system.yml).
 *
 * HERE, in three places, each asking the one rule below:
 *  - THE PEDESTAL. z_bg_toki_swd.c's wait action offers the pull only while
 *    VB_BE_ABLE_TO_TIME_TRAVEL_AT_PEDESTAL holds; the randomizer answers it with
 *    OoT_TimeTravelSword_PedestalAllows (hook_handlers.cpp). Both directions, as
 *    in OoTMM: a child cannot leave for adult, nor an adult return to child,
 *    without the sword in the inventory.
 *  - THE LOGIC. 3drando/fill.cpp's PropagateTimeTravel is the one place where an
 *    age reaches RR_ROOT through the pedestal (RR_TOT_BEYOND_DOOR_OF_TIME); it now
 *    also asks OoT_TimeTravelSword_LogicAllows, on the search's own save. Every
 *    search goes through it: the fill, the beatability and All Locations proofs,
 *    the paired coordinator (ComboLogicEngineOoT.cpp's ReachabilitySearch), the
 *    hints and the Check Tracker's Available Checks.
 *  - THE IDENTITY. The settings fold (3drando/playthrough.cpp) folds the option
 *    only while OoT_TimeTravelSword_RuleCanBlock: off it, the option changes
 *    nothing in the world, so a world it cannot touch keeps its fingerprint.
 *
 * AN UNSHUFFLED SWORD. Without Shuffle Master Sword, Ship of Harkinian's pedestal
 * is the vanilla pull that hands the sword over (VB_GIVE_ITEM_MASTER_SWORD), so
 * Link always holds it as he travels: the rule is met by the pull itself, which
 * is the `swordShuffled` term. OoTMM reaches the same place another way (its
 * pedestal is a child check holding the vanilla sword, and TIME_TRAVEL then has
 * it).
 *
 * THE ADULT START (OoTMM's mustStartWithMasterSword, packages/logic/src/
 * helpers.ts:55-66, and its macro `starts_with_master_sword`). OoTMM gives an
 * adult start the sword only when the sword is NOT shuffled and the rule is on,
 * "to not be stuck as adult forever". Ship of Harkinian already does that: an
 * adult start collects the pedestal's check at RR_ROOT (location_access/root.cpp),
 * which holds the vanilla sword when it is not shuffled (3drando/item_pool.cpp),
 * and z_sram.c's load gives an adult the sword unless it is shuffled. With the
 * sword shuffled OoTMM forces nothing, and neither does this: the child side,
 * and with it the child-only crossing to Termina (ComboLogicEngineOoT.cpp's
 * crossingOpen, RR_MARKET_MASK_SHOP->Child()), waits for the sword in logic, so
 * the fill puts it where the adult reaches it first.
 *
 * FROZEN AT CREATION. The option is one of the world's settings: the save's
 * randoSettings array and the seed package's OoT world carry it (SaveManager.cpp).
 * Off is 0, so a world saved before this option, whose array ends before it,
 * loads it off (SaveManager::LoadArray default-constructs the tail) and keeps the
 * pedestal it was generated with.
 *
 * NOT MODELLED: OoTMM's pedestal check is a CHILD check ("Temple of Time Master
 * Sword": `is_child && event(DOOR_OF_TIME_OPEN)`), handed over as the child walks
 * up. Ship of Harkinian's RC_TOT_MASTER_SWORD is collected at the first pull
 * (EVENTCHKINF_PULLED_MASTER_SWORD_FROM_PEDESTAL), so under the rule the
 * pedestal's own item is reached on the first trip to adult, after the sword.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "TimeTravelSword.h"

#include <memory>

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/location_access.h" // logic: the object every search asks

// At file scope first: z64.h includes <memory> under C++, which must not get C
// linkage. Its include guard makes the nested include below a no-op.
#include <z64.h>
extern "C" {
#include "macros.h"
#include "variables.h"
}

namespace {

/** The frozen option, read from the world's own context. */
bool RequiresSword(const std::shared_ptr<Rando::Context>& world) {
    return world->GetOption(RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD).Is(RO_GENERIC_ON);
}

/** The frozen Shuffle Master Sword: off, the pedestal holds the sword. */
bool SwordShuffled(const std::shared_ptr<Rando::Context>& world) {
    return world->GetOption(RSK_SHUFFLE_MASTER_SWORD).Is(RO_GENERIC_ON);
}

} // namespace

extern "C" bool OoT_TimeTravelSword_Allows(bool requiresSword, bool swordShuffled, bool ownsSword) {
    return !requiresSword || !swordShuffled || ownsSword;
}

extern "C" bool OoT_TimeTravelSword_RuleCanBlock(void) {
    const std::shared_ptr<Rando::Context> world = Rando::Context::GetInstance();
    return world != nullptr && RequiresSword(world) && SwordShuffled(world) &&
           world->GetOption(RSK_STARTING_MASTER_SWORD).Is(RO_GENERIC_OFF);
}

extern "C" bool OoT_TimeTravelSword_PedestalAllows(void) {
    const std::shared_ptr<Rando::Context> world = Rando::Context::GetInstance();
    if (world == nullptr || !IS_RANDO) {
        return true;
    }
    const bool owns = CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER) != 0;
    return OoT_TimeTravelSword_Allows(RequiresSword(world), SwordShuffled(world), owns);
}

extern "C" bool OoT_TimeTravelSword_LogicAllows(void) {
    const std::shared_ptr<Rando::Context> world = Rando::Context::GetInstance();
    if (world == nullptr || logic == nullptr) {
        return true;
    }
    // Logic's own save: the search's simulated inventory, or the live save while
    // the Check Tracker counts Available Checks (Logic::SetSaveContext).
    return OoT_TimeTravelSword_Allows(RequiresSword(world), SwordShuffled(world), logic->HasItem(RG_MASTER_SWORD));
}

#endif // RSBS_SINGLE_EXECUTABLE
