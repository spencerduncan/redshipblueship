/**
 * ForeignItemsSingleExe.cpp — the MM-side redemption give for cross-game items
 * (Lane 6 / #502; ADR 0002, ADR 0005).
 *
 * The MM twin of soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp. It is
 * the ONE MM translation unit where the cross-game item class may name real
 * RI_* enumerators, because everything leaves it as an origin-tagged SharedItem
 * (or a display string) and a raw RI_* therefore never crosses a game boundary
 * (ADR 0002, the #356 bug class). It holds MM's redemption give. The
 * reverse-direction SOURCE pool that used to live here too (kForeignPoolMMV1,
 * #510) is RETIRED with the overlay pass that drew from it (ADR 0010 increment
 * 3, D3; lane K11): under one bag any MM progression item may cross, and which
 * one does is the single-bag fill's decision, not a table's.
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
 * for a reason specific to how the work was split: the reverse-direction pool
 * (kForeignPoolMMV1, since retired) was authored by a DIFFERENT lane, later,
 * and under one bag any MM progression item may arrive here. An allowlist
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
#include <cstring>
#include <string>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/StaticData/StaticData.h"
// The arrival toast (#494), sent through MM_Notify_Emit — the explicit #427
// item-1 bridge to OoT's shared overlay. MM's own BenGui/Notification.cpp is
// excluded from single-exe builds and Notification::Emit is deliberately NOT
// declared there; the layouts both sides compile against are locked by
// src/common/notification_layout_probe.h and mm_notification_binding_test.cpp.
#include "2s2h/BenGui/Notification.h"

extern "C" {
#include "variables.h" // MM_gPlayState
#include "functions.h"
}

// src/common. Included OUTSIDE any extern "C" block: the header manages its
// own linkage and pulls in <stdbool.h>/<stdint.h> (matching Foreign.cpp).
#include "foreign_items.h"
#include "shared_items.h" // RSBS_SHARED_ITEM_CAP (via context.h)

// ============================================================================
// kForeignPoolMMV1 and its attributed exclusions — RETIRED (ADR 0010 increment
// 3, D3; lane K11)
// ============================================================================
//
// This TU used to define the reverse-direction SOURCE pool (~116 hand-adjudicated
// MM rows, each tagged with an ADR 0011 selection bit and, since #681, a required
// give capability) and the exclusion table that attributed every rejected id to
// one of ADR 0011's six criteria. Both existed to feed OoT_PlaceForeignItems,
// the overlay pass that pinned a few MM items onto OoT junk as DUPLICATE copies.
//
// Under the single bag, items leave origin pools: which MM item crosses is the
// fill's decision over the whole bag, and the membership question the criteria
// answered is the O8 classification owner's (src/common/shared_items.h): every
// PROGRESSION row whose arming conditions the frozen profile meets may enter the
// bag and cross (criteria 1-3 and 5: sentinels and junk are not progression,
// capability rows are gated by their arming, a trap is its own class), a world
// event is armed by nothing and stays home (criterion 4: the triforce pieces),
// and #525's shared quantities are PROGRESSION kinds the owner reconciles once
// (criterion 6 is superseded there, #731). The give below was always
// item-agnostic, so nothing it does depended on the table.

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

/**
 * The WHOLE arrival toast for `riId`, read from MM's OWN item table.
 *
 * Returns the complete Notification::Options rather than the item name alone,
 * so that the ROM-free tier's assertion covers every field the player can read
 * (MM_ForeignItem_TestArrivalText at the bottom of this file renders it the way
 * the overlay does). A bridge that exposed only the name would let a re-added
 * cross-game tell — an origin badge in `.prefix`, "from Ocarina of Time"
 * appended to the verb — pass the lock untouched, which is exactly the
 * regression #510 exists to prevent.
 *
 * Icon and name come from the same two accessors MM's native rando pickup toast
 * calls (Rando/MiscBehavior/CheckQueue.cpp) — this is an MM item being received
 * in MM, so nothing here is cross-game and nothing needs an archive that is not
 * mounted. `.itemIcon` may be nullptr; Emit then renders text-only.
 *
 * Field arrangement matches that native toast exactly (verb in `.message`, item
 * in `.suffix`, no `.prefix`), because Options colours each field differently
 * and a bespoke arrangement would itself be a "this one is special" tell.
 */
Notification::Options BuildArrivalToast(RandoItemId randoItemId) {
    return Notification::Options{
        .itemIcon = Rando::StaticData::GetIconTexturePath(randoItemId),
        .message = "You got",
        .suffix = Rando::StaticData::GetItemName(randoItemId), // MM's article + name, e.g. "the Bunny Hood"
    };
}

/**
 * The toast as one line of text, joined the way Notification::Window::Draw lays
 * it out: every non-empty field on the same line, one gap between neighbours
 * (soh/Notification/Notification.cpp's ImGui::SameLine). The ROM-free lock's
 * observable — see MM_ForeignItem_TestArrivalText.
 */
std::string RenderToastLine(const Notification::Options& toast) {
    std::string line;
    const std::string* const fields[] = { &toast.prefix, &toast.message, &toast.suffix };
    for (const std::string* field : fields) {
        if (field->empty()) {
            continue;
        }
        if (!line.empty()) {
            line += ' ';
        }
        line += *field;
    }
    return line;
}

/** The actual give. Precondition: MM_gPlayState != NULL and the id is real. */
void GiveNow(uint16_t riId) {
    const RandoItemId randoItemId = (RandoItemId)riId;
    Rando::GiveItem(randoItemId);

    // #494: the arrival is the player's ONLY signal that this item landed.
    // Until now it was silent — the item appeared in the inventory an arbitrary
    // number of scenes after the OoT check that granted it, with no in-game
    // feedback at all. That is the same gap the OoT arrival closed for the other
    // direction (OoT_AwardSharedItem), here on MM's side.
    //
    // PRESENTED AS AN ORDINARY MM PICKUP: MM's own toast, MM's own icon, MM's
    // own "article + name" sentence, and no mention of where the item came from
    // (BuildArrivalToast, above).
    //
    // A toast rather than the get-item cutscene: this fires on a gameplay frame
    // the player did not initiate, where seizing the camera and the message
    // context would be a hijack. The toast is exactly what MM already falls back
    // to for its own pickups when the cutscene is skipped.
    //
    // EMITTED HERE AND NOT IN MM_AwardSharedItem (GameExports_SingleExe.cpp)
    // because this is the one point that is gameplay-gated by construction: both
    // callers reach it only with a live MM_gPlayState. The award callback runs at
    // the presence-gated arrival point, which the ROM-free rows drive headlessly
    // (src/common/tests/test_foreign_award.c calls the real MM_ConsumeSharedItems
    // with no PlayState and no Gui), so a Notification::Emit there would put an
    // ImGui/audio call on a display-free CI path.
    Notification::MM_Notify_Emit(BuildArrivalToast(randoItemId));
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
 *  re-derived in the test for the same reason MM_Rando_Foreign_TestIsForeignHostClass
 *  is: a lock that paraphrases the rule stops testing it the moment the rule
 *  moves. */
extern "C" int MM_ForeignItem_TestIsGiveableId(uint16_t riId) {
    return IsGiveableItemId(riId) ? 1 : 0;
}

/** Is `riId` declared RITYPE_JUNK in MM's item table? (#510)
 *
 *  The observable for the retired kForeignPoolMMV1's membership rule (2) — "no junk-class
 *  item may be a cross-game SOURCE, because junk is what a foreign HOST degrades
 *  to". Reported from MM's real table rather than re-derived in the test, so a
 *  row whose type changes upstream moves the lock with it instead of leaving it
 *  asserting a stale copy.
 *
 *  @return 1 if junk-class, 0 if a real non-junk item, -1 if the id names no row. */
extern "C" int MM_ForeignItem_TestIsJunkClassId(uint16_t riId) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)riId);
    if (it == Rando::StaticData::Items.end()) {
        return -1;
    }
    return (it->second.randoItemType == RITYPE_JUNK) ? 1 : 0;
}

/**
 * The WHOLE arrival toast for `riId` as one line, NUL-terminated into `out`
 * (#494) — every field the player can read, joined the way the overlay draws
 * them.
 *
 * The SAME BuildArrivalToast the give emits, not a paraphrase of it — a lock
 * that rebuilt the string itself would keep passing after the toast changed.
 * What CI can honestly assert is the resolution surface: that MM's own item
 * table names every entry of MM's own pool, and that the toast is that name
 * behind the native verb and NOTHING ELSE. Extra prose anywhere in the toast —
 * "it came from Ocarina of Time" appended to the verb, an origin badge in the
 * unused `.prefix` — breaks the equality, which is the no-tell contract (#510)
 * stated as an assertion. Reporting only the item name would have left both of
 * those regressions invisible to CI.
 *
 * @return the length written, or -1 if the id names no real MM item or `out` is
 *         too small (never a truncated string).
 */
extern "C" int MM_ForeignItem_TestArrivalText(uint16_t riId, char* out, int cap) {
    if (out == nullptr || cap <= 0 || !IsGiveableItemId(riId)) {
        return -1;
    }
    const std::string line = RenderToastLine(BuildArrivalToast((RandoItemId)riId));
    if ((int)line.size() >= cap) {
        return -1;
    }
    memcpy(out, line.c_str(), line.size() + 1);
    return (int)line.size();
}

/** The arrival toast's icon path for `riId`, or nullptr — which is a text-only
 *  toast, not a defect (Notification::Emit skips a null icon). */
extern "C" const char* MM_ForeignItem_TestArrivalIcon(uint16_t riId) {
    if (!IsGiveableItemId(riId)) {
        return nullptr;
    }
    return BuildArrivalToast((RandoItemId)riId).itemIcon;
}

#endif // RSBS_SINGLE_EXECUTABLE
