/**
 * ForeignItemsSingleExe.cpp — the MM-side redemption give for cross-game items
 * (Lane 6 / #502; ADR 0002, ADR 0005).
 *
 * The MM twin of soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp. It is
 * the ONE MM translation unit where the cross-game item class may name real
 * RI_* enumerators, because everything leaves it as an origin-tagged SharedItem
 * (or a display string) and a raw RI_* therefore never crosses a game boundary
 * (ADR 0002, the #356 bug class). Lane 1 extends this same TU with
 * kForeignPoolMMV1 — the reverse-direction SOURCE pool — plus its
 * Combo_RegisterForeignItemPool file-scope registrar; this file deliberately
 * stops at the give so that boundary stays obvious.
 *
 * Lives in 2s2h/Rando/, glob-collected into `2ship_rando`, which links
 * WHOLE_ARCHIVE — so this TU survives the link with no CMake edit.
 *
 * ============================================================================
 * WHY THE GIVE IS DEFERRED RATHER THAN IMMEDIATE
 * ============================================================================
 *
 * MM's redemption point runs BEFORE MM_gPlayState is assigned:
 * MM_ConsumeSharedItems() is called from MM_Play_ConsumeStartupEntrance at
 * z_play.c:2406, while `MM_gPlayState = this` happens at z_play.c:2468. That is
 * not an accident to be tidied away — the consume has to run after the frozen
 * save is restored and before gameplay observes it, which is exactly where it
 * sits.
 *
 * MM's give path is not NULL-play tolerant the way OoT's starting-item give is.
 * Rando::GiveItem's default branch is `MM_Item_Give(MM_gPlayState, itemId)`,
 * and Item_GiveImpl (z_parameter.c:4136) carries only PARTIAL 2S2H nullptr
 * guards — the sword and shield branches, added so OnFileCreate could grant
 * starting items outside gameplay. Three legs still deref `play` unguarded:
 *
 *   - Inventory_IncrementSkullTokenCount(play->sceneId)   (:4150, ITEM_SKULL_TOKEN)
 *   - Interface_LoadItemIconImpl(play, EQUIP_SLOT_C_*) and its Dpad twin, in
 *     the bottle-content loop                            (:4524-4550)
 *   - the same pair in the trade-item branch             (:4575, :4583)
 *
 * The last two are the ones that make an item-by-item audit treacherous. They
 * are reached only when the player ALREADY holds a bottle / trade item in that
 * slot and has it C- or D-equipped — which is impossible at file creation and
 * entirely ordinary on a mid-game arrival. That is precisely why
 * Rando::GrantStartingItems' NULL-play gives have never tripped them, and why
 * its success proves a NULL-play give works for SOME items in SOME save states
 * rather than for any item. A redemption is the mid-game case by definition.
 *
 * The lane brief offers two ways out: restrict the pool to a hand-audited
 * NULL-play-safe RI_* set, or move redemption to the gameplay-gated frame-tick
 * safe point shared_items.h:52-64 already defines. This TU takes the second,
 * for a reason specific to how the work is split: the reverse-direction pool
 * (kForeignPoolMMV1) is authored by a DIFFERENT lane, later. An allowlist
 * audited against today's pool would be a correctness guarantee that silently
 * expires the moment somebody adds a row — the failure mode being a crash on
 * the arrival path, i.e. the worst place to learn about it. Deferral is
 * item-agnostic and O(1) in pool size, so it cannot rot that way.
 *
 * The deferral is scoped as tightly as it can be: MM_AwardSharedItem still
 * runs at the presence-gated arrival point and still consumes the redemption
 * there (Combo_RedeemSharedItemsForGame sets RSBS_SHARED_ITEM_REDEEMED on
 * return, and we deliberately do NOT clear the entry). Only the give itself
 * waits, in a process-global RAM queue, for the first gameplay frame with a
 * live PlayState — a handful of frames later, in the same boot, with no player
 * input possible in between.
 *
 * DURABILITY OF THAT WINDOW. The obvious objection is "redeemed in gComboCtx
 * but not yet given — a crash there loses the item". It does not: the redeemed
 * bit only becomes durable when a .redsave is WRITTEN, and no save can be taken
 * between MM_Play_ConsumeStartupEntrance and the first Play frame. If the
 * process dies inside that window, the .redsave on disk still shows the entry
 * un-redeemed and the next arrival re-awards it. The queue therefore never has
 * to survive a process, which is why it is RAM-only by design rather than by
 * omission.
 *
 * ONE DEPENDENCY WORTH NAMING. The drain hangs off
 * Rando::MiscBehavior::CheckQueue, which is a COND_ID_HOOK gated on IS_RANDO
 * (MiscBehavior.cpp:25). A save whose IS_RANDO hooks are not armed therefore
 * never drains — the #439/#487 class of bug, where an arrival path leaves the
 * hooks matched against the wrong save. That is the correct coupling rather
 * than a gap to paper over: a save with no randomizer identity has no business
 * receiving cross-game randomizer items, and if the hooks are unarmed the
 * player has much larger problems than one un-given item. It does mean this
 * give inherits those locks' guarantees, so it is written down here.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/StaticData/StaticData.h"

extern "C" {
#include "variables.h" // MM_gPlayState
#include "functions.h"
}

// src/common. Included OUTSIDE any extern "C" block: the header manages its
// own linkage and pulls in <stdbool.h>/<stdint.h> (matching Foreign.cpp).
#include "foreign_items.h"
#include "shared_items.h" // RSBS_SHARED_ITEM_CAP (via context.h)

namespace {

// The pending-give queue. Capacity matches the durable array's, so a full
// gComboCtx redeeming entirely into MM in one arrival can never overflow it —
// the queue is not allowed to be the thing that loses an item.
uint16_t sPendingGives[RSBS_SHARED_ITEM_CAP];
int sPendingCount = 0;

/**
 * Is `riId` a real, giveable MM item id?
 *
 * RI_UNKNOWN is enumerator 0 — a zero-initialised slot — and RI_NONE is
 * "literally nothing"; both are declared RITYPE_JUNK, so a type-based test
 * accepts them (the #488 sentinel trap, same two values, different surface).
 * An id at or past RI_MAX is out of the table entirely and would index
 * Rando::StaticData::Items out of range inside the give.
 */
bool IsGiveableItemId(uint16_t riId) {
    return riId != (uint16_t)RI_UNKNOWN && riId != (uint16_t)RI_NONE && riId < (uint16_t)RI_MAX;
}

/** The actual give. Precondition: MM_gPlayState != NULL and the id is real. */
void GiveNow(uint16_t riId) {
    Rando::GiveItem((RandoItemId)riId);
}

} // namespace

// ============================================================================
// Redemption give (called by MM_AwardSharedItem, the A1 consumer callback)
// ============================================================================

/**
 * Award one MM-origin foreign item. Gives immediately when a PlayState is live,
 * otherwise queues for MM_ForeignItem_FlushPending (see the file header for why
 * that is the safe shape here).
 *
 * @return 1 if the item was given or queued for the next gameplay frame,
 *         0 if the id names no real MM item (logged) or the queue is full.
 *         The caller's consumer sets RSBS_SHARED_ITEM_REDEEMED either way — a
 *         give we could not perform is a loud log, not a stalled arrival.
 */
extern "C" int MM_ForeignItem_Give(uint16_t riId) {
    if (!IsGiveableItemId(riId)) {
        fprintf(stderr, "[MM] foreign give: RI id=%u names no real MM item — not given\n", (unsigned)riId);
        return 0;
    }

    if (MM_gPlayState != NULL) {
        GiveNow(riId);
        return 1;
    }

    if (sPendingCount >= (int)RSBS_SHARED_ITEM_CAP) {
        // Unreachable while the queue is sized to the durable array (one
        // arrival cannot redeem more entries than the array holds), so this is
        // a guard against a future resize, not an expected path.
        fprintf(stderr, "[MM] foreign give: pending queue full, dropping RI id=%u\n", (unsigned)riId);
        return 0;
    }
    sPendingGives[sPendingCount++] = riId;
    return 1;
}

/**
 * Drain the pending queue once a PlayState is live. Called every frame from
 * Rando::MiscBehavior::CheckQueue (the gameplay-gated per-frame rando hook),
 * BEFORE its own early-out, so a queued give is not held behind a queued check.
 *
 * Order is preserved: Combo_RedeemSharedItemsForGame awards in slot order and
 * that order is a contract (progressive items resolve against the live save, so
 * order changes WHAT the player receives — shared_items.h). The queue is FIFO
 * for the same reason.
 *
 * @return the number of items given this call (0 when there is nothing pending
 *         or no live PlayState).
 */
extern "C" int MM_ForeignItem_FlushPending(void) {
    if (sPendingCount == 0 || MM_gPlayState == NULL) {
        return 0;
    }

    const int count = sPendingCount;
    // Clear the queue BEFORE giving. A give re-entering this function (an item
    // whose give path spins the frame loop) must not see the same entries
    // again; re-entrancy that re-gave would be a duplicate the redeemed bit
    // cannot catch, because the crossing was already consumed.
    uint16_t drained[RSBS_SHARED_ITEM_CAP];
    for (int i = 0; i < count; i++) {
        drained[i] = sPendingGives[i];
    }
    sPendingCount = 0;

    for (int i = 0; i < count; i++) {
        fprintf(stderr, "[MM] foreign give (deferred to gameplay): RI id=%u\n", (unsigned)drained[i]);
        GiveNow(drained[i]);
    }
    return count;
}

// ============================================================================
// ROM-free test bridge (redship tier; src/common/tests/test_foreign_award.c).
//
// The give itself needs a PlayState and a loaded save, so the ROM-free tier
// cannot observe it directly. What it CAN observe — and what the #502 lock is
// actually about — is that the real consumer walk reaches the real
// MM_AwardSharedItem, that the award lands here exactly once per crossing, and
// that a NULL PlayState defers instead of dereferencing. These two accessors
// are the observable for that; neither is reachable from gameplay.
// ============================================================================

/** How many gives are waiting for a live PlayState. */
extern "C" int MM_ForeignItem_TestPendingCount(void) {
    return sPendingCount;
}

/** The queued id at `index`, or 0 (RI_UNKNOWN) if out of range. */
extern "C" uint16_t MM_ForeignItem_TestPendingAt(int index) {
    if (index < 0 || index >= sPendingCount) {
        return (uint16_t)RI_UNKNOWN;
    }
    return sPendingGives[index];
}

/** Drop the queue without giving. Test isolation only — `--test all` runs every
 *  row in one process, and a queue left behind would leak into the next row. */
extern "C" void MM_ForeignItem_TestResetPending(void) {
    sPendingCount = 0;
}

/** RI_MAX — the exclusive upper bound for a RandoItemId walk, so a src/common
 *  test can find real ids without importing MM's enum (the #488 bridge's
 *  MM_Rando_Foreign_TestCheckIdMax, one enum over). */
extern "C" int MM_ForeignItem_TestItemIdMax(void) {
    return (int)RI_MAX;
}

/** The REAL id predicate MM_ForeignItem_Give gates on. Exposed rather than
 *  re-derived in the test for the same reason MM_Rando_Foreign_IsEligibleHost
 *  is: a lock that paraphrases the rule stops testing it the moment the rule
 *  moves. */
extern "C" int MM_ForeignItem_TestIsGiveableId(uint16_t riId) {
    return IsGiveableItemId(riId) ? 1 : 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
