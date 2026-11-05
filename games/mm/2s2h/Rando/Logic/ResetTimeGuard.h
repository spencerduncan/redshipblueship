#ifndef RANDO_LOGIC_RESET_TIME_GUARD_H
#define RANDO_LOGIC_RESET_TIME_GUARD_H

/**
 * MM's reset-time guard: ADR 0010 answer O2's MM-side terms (#645).
 *
 * THE GAP IT CLOSES (inherited from upstream 2Ship, 222acd18). MM's region graph
 * has no "can the player go back to Day 1" term: every region hangs off the root
 * through `CONNECTION(RR_MAX, true)` on South Clock Town (Regions/Central.cpp),
 * so the graph assumes a free reset at all times. In a paired file that holds
 * only under two conditions. With "Moon Crash Behavior" = "Last Save" (the
 * default, OptionsUiSingleExe.cpp) a crash rolls the file back to its last
 * save, so progress made in a cycle that cannot be reset is lost; and with
 * "Shuffle Ocarina Buttons" on, the paired start's Song of Time is held but
 * cannot be played until C-Right, A and C-Down are found. Under both, logic that
 * walks out of the arrival area can demand late-cycle items before the earlier
 * ones that unlock them, and a seed can come out unbeatable.
 *
 * THE GUARD, after OoTMM (data/macros/macros_mm.yml:301-304,
 * data/world/mm/overworld/clock_town.yml:6-21, data/world/mm/dungeons/moon.yml):
 *
 *   MmCanResetTime()       = frozen moon crash is "New Cycle" || CAN_PLAY_SONG(TIME)
 *                            (OoTMM can_reset_time_impl, less the MM_MAJORA_LOGIC
 *                            disjunct; see below)
 *   MmCanResetTimeOnMoon() = CAN_PLAY_SONG(TIME)
 *                            (OoTMM can_reset_time_on_moon: New Cycle does not
 *                            count, because the clock is stopped on the Moon)
 *
 * and ApplyResetTimeGuard() restricts a crawl's reached set accordingly:
 *
 *   - !MmCanResetTime(): only THE ARRIVAL NOOK is reached. Regions: the root
 *     (RR_MAX), RR_CLOCK_TOWN_SOUTH, RR_CLOCK_TOWER_INTERIOR and the region
 *     RR_CLOCK_TOWER_ROOF. Checks: the two starting checks on the root
 *     (RC_STARTING_ITEM_DEKU_MASK, RC_STARTING_ITEM_SONG_OF_HEALING), the
 *     platform heart piece and the South Clock Town owl statue. These are
 *     OoTMM's "Clock Town From Clock Tower", "Clock Town Near Clock Tower" and
 *     "Clock Tower Platform", whose exit to the rest of South Clock Town is
 *     `can_reset_time`. NOT the roof's checks (RC_CLOCK_TOWER_ROOF_*): OoTMM
 *     gates every one on a reset, because under Last Save the roof is a dead
 *     end. The tower interior stays in, so the MM->OoT crossing never closes.
 *   - MmCanResetTime() && !MmCanResetTimeOnMoon(): the Moon trial regions (the
 *     Deku, Goron and Zora trials and the Link trial's four rooms) and
 *     RC_MOON_FIERCE_DEITY_MASK are not reached (OoTMM gates the trial
 *     entrances and the mask on can_reset_time_on_moon). The Moon itself and
 *     Majora's lair stay reachable: OoTMM's "Moon Boss" exit carries no reset
 *     term, and in this build (ADR 0010 D1, ComboGoalEndingMM.cpp) beating
 *     Majora with the goal unmet is itself a reset to a new cycle.
 *
 * DELIBERATE DIVERGENCES FROM OoTMM, both conservative:
 *   - No owl landings from the nook. OoTMM's nook reaches its "GLOBAL" exits,
 *     so a soaring player reaches checks local to an activated owl's landing
 *     without a reset; here the root's soaring exits are not part of the nook.
 *   - MMRT_MAJORA_LOGIC (OoTMM's "event(MAJORA) && trick(MM_MAJORA_LOGIC)"
 *     disjunct) is left unbound. For MmCanResetTime it is vacuous here: Oath
 *     to Order's buttons include the Song of Time's, and the paired start holds
 *     the song unless the Shared Song of Time rule takes it away, which forces
 *     New Cycle, which already counts. It could only widen the on-Moon term
 *     under that rule, and the trick ships off (T = empty, ADR 0010 O11).
 *
 * MONOTONE. Both predicates only turn on as items are added (the Ocarina, the
 * song and the buttons are never taken away by a give), and the nook and the
 * trial set are fixed, so a guarded set only grows with the inventory (ADR 0010
 * §2.3).
 *
 * WHO CALLS IT. Both consumers of the shared crawl that answer "what is in
 * logic": the combo-logic engine's `expand` (ComboLogicEngineSingleExe.cpp,
 * which the paired fill and its goal proof run on) and the MM Check Tracker's
 * ComputeChecksInLogic (CheckTracker.cpp). One function, so the fill and the
 * tracker cannot disagree. CrawlReachableRegions itself is left unguarded: it is
 * the raw graph walk, and its other callers (the reachable-check closure, the
 * trick and gate locks) ask questions about the graph, not about the file.
 *
 * Outside RSBS_SINGLE_EXECUTABLE there is no frozen moon crash (it is a combo
 * setting), so both predicates answer true and the guard changes nothing: a
 * standalone 2Ship build keeps upstream's behaviour.
 */

#include "2s2h/Rando/Logic/Logic.h"

#include <set>

namespace Rando {

namespace Logic {

/** OoTMM's can_reset_time: the frozen moon crash is "New Cycle", or the Song of
 *  Time is playable (the Ocarina, the song, and C-Right, A and C-Down). Reads
 *  the live save and the frozen combo record. */
bool MmCanResetTime();

/** OoTMM's can_reset_time_on_moon: the Song of Time is playable. A New Cycle
 *  moon crash does not count; the clock is stopped on the Moon. */
bool MmCanResetTimeOnMoon();

/** Is `regionId` in the arrival nook (see the file header)? */
bool IsResetTimeNookRegion(RandoRegionId regionId);

/** Is `checkId` one of the nook's checks (see the file header)? */
bool IsResetTimeNookCheck(RandoCheckId checkId);

/**
 * Restrict `crawl` (its regions AND their time states) and `checks` (the
 * EvaluateReachableChecks answer over that crawl) by the reset-time guard, in
 * place, under the live save. Re-evaluates the checks over the pruned regions
 * when a region is removed (an enemy drop can live in a pruned region and a kept
 * one). Returns true when anything was removed.
 *
 * It prunes a FINISHED crawl rather than steering one, which is exact for MM's
 * graph as authored: the nook's four checks read no event and no time slice
 * (so events fired outside the nook cannot open them), every nook region is
 * reached from the arrival without leaving the nook, and the trial regions'
 * exits and connections lead only back to RR_MOON, which reaches them, or to
 * another trial region (the Link trial's rooms), which is pruned too. A region
 * reachable ONLY through a pruned one would need the guard inside
 * CrawlReachableRegions.
 */
bool ApplyResetTimeGuard(ReachabilityCrawl& crawl, std::set<RandoCheckId>& checks);

} // namespace Logic

} // namespace Rando

#endif // RANDO_LOGIC_RESET_TIME_GUARD_H
