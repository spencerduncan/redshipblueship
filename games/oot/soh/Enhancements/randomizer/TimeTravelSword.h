#pragma once
/**
 * @file TimeTravelSword.h
 * @brief OoTMM's "Time Travel requires Master Sword" for Ocarina of Time's
 *        randomizer (#1434): RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD. The
 *        definitions and the reasoning are in TimeTravelSword.cpp; this header is
 *        what the pedestal's hook, the fill's time-travel step, the settings fold
 *        and the lock call. C-compatible.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** THE RULE, pure. May Link travel through time at the Pedestal of Time?
 *  @param requiresSword the frozen RSK_TIME_TRAVEL_REQUIRES_MASTER_SWORD
 *  @param swordShuffled the frozen RSK_SHUFFLE_MASTER_SWORD: off, the pedestal
 *         itself holds the sword and pulling it hands it over (VB_GIVE_ITEM_MASTER_SWORD)
 *  @param ownsSword     Link owns the Master Sword (the equipment bit, not B) */
bool OoT_TimeTravelSword_Allows(bool requiresSword, bool swordShuffled, bool ownsSword);

/** 1 when the frozen world's settings can make the rule stop Link: the option on,
 *  the sword shuffled and not a starting item. Off this, the option changes
 *  nothing in the world, so the settings fold leaves it out (playthrough.cpp). */
bool OoT_TimeTravelSword_RuleCanBlock(void);

/** The pedestal's answer on the live save (hook_handlers.cpp,
 *  VB_BE_ABLE_TO_TIME_TRAVEL_AT_PEDESTAL). True on a vanilla file. */
bool OoT_TimeTravelSword_PedestalAllows(void);

/** The search's answer on logic's own save (3drando/fill.cpp, PropagateTimeTravel). */
bool OoT_TimeTravelSword_LogicAllows(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
