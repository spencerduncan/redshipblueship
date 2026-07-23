/**
 * @file test_grant_sources.c
 * @brief ROM-free locks for the sourced-grant model (ADR 0005, netplay 1a #460).
 *
 * The four semantic gaps the netplay spike identified (docs/
 * netplay-increment-1-spike.md §3) are locked here, transport-free:
 *
 *  (1) IDEMPOTENCY IS DECIDABLE: a retransmitted grant (same source, same seq)
 *      delivers once; two distinct grants of the SAME item — two sources, or
 *      one source with fresh seqs — both deliver. Content de-dup stays correct
 *      for in-process producers and never crosses into the sourced domain.
 *
 *  (2) REDEMPTION NEEDS NO SWITCH: grants redeem through the ordinary
 *      consumer at any safe point, with zero switch machinery touched, in
 *      received order, single-use under repeated safe points.
 *
 *  (3) OVERFLOW CANNOT LOSE DATA SILENTLY: a full array refuses loudly (the
 *      durable overflow count), a refused sourced grant stays owed (cursor
 *      unmoved -> the retry is accepted, not deduped), and redeemed entries
 *      are reclaimed oldest-first with order preserved.
 *
 *  (4) THE MODEL IS DURABLE: cursors + sourced entries + overflow count ride
 *      the .redsave Tier-1 record (ADR 0002 growth contract), a reload closes
 *      the duplicate window instead of re-opening it, a pre-netplay legacy
 *      record loads with every new field unset, and ComboContext_Init retires
 *      items and cursors ATOMICALLY (the #440 composition contract).
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as
 * C++, like test_save_roundtrip.c) for the rsbs::SaveManager half; every
 * grant symbol it drives is C-linkage via shared_items.h.
 */

#include "../context.h"
#include "../save.h"
#include "../shared_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define GS_ASSERT(cond)                                                                                                \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

namespace {

const char* const kGrantSaveDir = "rsbs_test_saves_grants";

// Arbitrary nonzero source keys — the model treats them opaquely.
const uint32_t kGrantSrcA = 0xA11CE001u;
const uint32_t kGrantSrcB = 0xB0B00002u;

// Award recorder that captures the FULL award order, not just the last one:
// received-order redemption is one of the contracts under test.
struct GrantAwardLog {
    int count;
    uint16_t ids[RSBS_SHARED_ITEM_CAP];
    uint8_t origins[RSBS_SHARED_ITEM_CAP];
};

void GrantTestAward(const SharedItem* item, void* ctx) {
    GrantAwardLog* log = (GrantAwardLog*)ctx;
    if (log->count < (int)RSBS_SHARED_ITEM_CAP) {
        log->ids[log->count] = item->id;
        log->origins[log->count] = item->originGame;
    }
    log->count++;
}

// Clean slate shared by every grant test. Deliberately does NOT touch frozen
// state or the entrance table: these tests must prove the grant model has no
// dependency on the switch machinery.
void GrantTestReset(void) {
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
}

int GrantTestOccupiedTotal(void) {
    return Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true) +
           Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true);
}

// Hand-craft a PRE-NETPLAY .redsave: the Tier-1 record truncated exactly where
// the ADR 0005 carve begins (offsetof grantCursors — drift-locked to 672 by
// the context.h static-assert chain), taking the bytes from the CURRENT
// gComboCtx. That is byte-for-byte what a pre-carve build wrote, per the
// growth contract. Mirrors test_save_roundtrip.c's SaveTestWriteCraftedSlot
// but is kept local so this file does not reach into another test's internals.
//
// Why the offsetof is a legitimate boundary here and not a tautology (#490):
// it is correct ONLY because every legal carve lands AFTER
// sharedItemOverflowCount, from the front of reserved[]. Such a carve leaves
// offsetof(grantCursors) at 672, so this truncation keeps meaning "the
// pre-netplay prefix". An in-place widen of a field BEFORE grantCursors — a
// bump of RSBS_FOREIGN_PLACEMENT_CAP being the realistic one — would instead
// drag this boundary forward with it, silently redefining "legacy" to include
// bytes no pre-netplay build ever wrote, and this test would keep passing
// while the format broke. That is why context.h pins 672 and 736 as literals
// and why bumping the cap is a build error rather than a comment violation.
// Test_SaveComboLegacyRecord's v2 fixed-offset case is the runtime half of the
// same lock: it drives literal bytes at 672/676/736 through the real Load.
bool GrantTestWriteLegacySlot(const std::string& path) {
    const uint32_t comboSize = (uint32_t)offsetof(ComboContext, grantCursors);
    std::vector<uint8_t> payload;
    const uint8_t* comboBytes = reinterpret_cast<const uint8_t*>(&gComboCtx);
    payload.insert(payload.end(), comboBytes, comboBytes + comboSize);
    payload.insert(payload.end(), OOT_SAVE_CONTEXT_SIZE, 0u);
    payload.insert(payload.end(), MM_SAVE_CONTEXT_SIZE, 0u);

    rsbs::RsbsSaveHeader h;
    std::memset(&h, 0, sizeof(h));
    std::memcpy(h.magic, RSBS_SAVE_MAGIC, sizeof(h.magic));
    h.version = RSBS_SAVE_VERSION;
    h.endian = RSBS_SAVE_ENDIAN_LE;
    h.slot = 0;
    h.headerSize = sizeof(rsbs::RsbsSaveHeader);
    h.comboSize = comboSize;
    h.ootSize = (uint32_t)OOT_SAVE_CONTEXT_SIZE;
    h.mmSize = (uint32_t)MM_SAVE_CONTEXT_SIZE;
    h.crc32 = rsbs::SaveManager::Crc32(payload.data(), payload.size());

    std::filesystem::create_directories(kGrantSaveDir);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(&h), sizeof(h));
    out.write(reinterpret_cast<const char*>(payload.data()), (std::streamsize)payload.size());
    return static_cast<bool>(out);
}

} // namespace

TestResult Test_GrantIdempotency(void) {
    printf("[TEST] grant-idempotency: retransmit delivers once; a second gift of the same item delivers twice "
           "(ADR 0005)\n");

    GrantTestReset();

    // Malformed submissions change nothing.
    GS_ASSERT(Combo_SubmitSourcedGrant(0, 1, GAME_OOT, 10) == RSBS_GRANT_REJECTED);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 0, GAME_OOT, 10) == RSBS_GRANT_REJECTED);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 1, GAME_NONE, 10) == RSBS_GRANT_REJECTED);
    GS_ASSERT(Combo_CountGrantSources() == 0 && GrantTestOccupiedTotal() == 0);
    GS_ASSERT(Combo_GetGrantCursor(kGrantSrcA) == 0 && Combo_GetGrantCursor(0) == 0);

    // A new source must start at seq 1: out-of-the-blue seq 5 is a GAP, and
    // no cursor slot is burned for it.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 5, GAME_OOT, 10) == RSBS_GRANT_GAP);
    GS_ASSERT(Combo_CountGrantSources() == 0 && GrantTestOccupiedTotal() == 0);

    // First delivery.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 1, GAME_OOT, 10) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_GetGrantCursor(kGrantSrcA) == 1);
    GS_ASSERT(GrantTestOccupiedTotal() == 1);
    GS_ASSERT((gComboCtx.sharedItemsTagged[0].flags & RSBS_SHARED_ITEM_SOURCED) != 0);
    GS_ASSERT((gComboCtx.sharedItemsTagged[0].flags & RSBS_SHARED_ITEM_REDEEMED) == 0);

    // RETRANSMIT: same source, same seq — delivers once. This used to be
    // indistinguishable from a second gift at the record API (spike §3).
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 1, GAME_OOT, 10) == RSBS_GRANT_DUPLICATE);
    GS_ASSERT(GrantTestOccupiedTotal() == 1 && Combo_GetGrantCursor(kGrantSrcA) == 1);

    // THE GAP-2 REGRESSION: a second source gifting the SAME item is a second
    // item, not a merge.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcB, 1, GAME_OOT, 10) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(GrantTestOccupiedTotal() == 2);

    // Same source gifting the same item again under a fresh seq: also a
    // second item.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 2, GAME_OOT, 10) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(GrantTestOccupiedTotal() == 3 && Combo_GetGrantCursor(kGrantSrcA) == 2);

    // A skipped seq is a GAP (cursor and array unmoved); filling in order
    // resumes delivery.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 4, GAME_OOT, 11) == RSBS_GRANT_GAP);
    GS_ASSERT(GrantTestOccupiedTotal() == 3 && Combo_GetGrantCursor(kGrantSrcA) == 2);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 3, GAME_OOT, 11) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 4, GAME_OOT, 12) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_GetGrantCursor(kGrantSrcA) == 4 && GrantTestOccupiedTotal() == 5);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 2, GAME_OOT, 10) == RSBS_GRANT_DUPLICATE); // stale retransmit

    // The in-process producer's content de-dup is UNCHANGED — and disjoint
    // from the sourced domain: un-redeemed sourced (OOT, 10) entries exist,
    // yet a local pickup of (OOT, 10) records fresh instead of merging.
    int localSlot = Combo_RecordSharedItem(GAME_OOT, 10);
    GS_ASSERT(localSlot >= 0);
    GS_ASSERT(GrantTestOccupiedTotal() == 6);
    GS_ASSERT(gComboCtx.sharedItemsTagged[localSlot].flags == 0); // in-process: no SOURCED bit
    // A re-fired local pickup still merges into the local entry.
    GS_ASSERT(Combo_RecordSharedItem(GAME_OOT, 10) == localSlot);
    GS_ASSERT(GrantTestOccupiedTotal() == 6);

    // Source-table exhaustion is explicit, not silent: fill all cursor slots,
    // then a 9th source is refused with nothing recorded.
    GS_ASSERT(Combo_CountGrantSources() == 2);
    for (uint32_t i = 0; i < RSBS_GRANT_SOURCE_CAP - 2u; i++) {
        GS_ASSERT(Combo_SubmitSourcedGrant(0xC0DE0000u + i, 1, GAME_OOT, (uint16_t)(900u + i)) ==
                  RSBS_GRANT_ACCEPTED);
    }
    GS_ASSERT(Combo_CountGrantSources() == (int)RSBS_GRANT_SOURCE_CAP);
    const int occupiedBefore = GrantTestOccupiedTotal();
    GS_ASSERT(Combo_SubmitSourcedGrant(0xDEADBEEFu, 1, GAME_OOT, 999) == RSBS_GRANT_NO_SOURCE_SLOT);
    GS_ASSERT(GrantTestOccupiedTotal() == occupiedBefore);
    GS_ASSERT(Combo_CountGrantSources() == (int)RSBS_GRANT_SOURCE_CAP);

    GrantTestReset();
    printf("[TEST] PASS: retransmit vs second gift is decidable; domains disjoint; exhaustion explicit\n");
    return TEST_PASS;
}

TestResult Test_GrantRedeemNoSwitch(void) {
    printf("[TEST] grant-redeem-no-switch: grants redeem in received order with zero switch machinery (ADR 0005)\n");

    GrantTestReset();
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();

    // Interleave two sources; one grant is bound for the OTHER game.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 1, GAME_OOT, 101) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcB, 1, GAME_OOT, 201) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 2, GAME_OOT, 102) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcB, 2, GAME_MM, 301) == RSBS_GRANT_ACCEPTED);

    // Nothing switch-shaped has happened — and must not need to.
    GS_ASSERT(!ComboContext_IsSwitchPending());
    GS_ASSERT(!Context_HasFrozenState(GAME_OOT) && !Context_HasFrozenState(GAME_MM));

    // Redeem for OoT directly — the safe-point call a mid-session tick will
    // make. Received order across sources, only OoT-bound entries.
    GrantAwardLog log;
    std::memset(&log, 0, sizeof(log));
    GS_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, GrantTestAward, &log) == 3);
    GS_ASSERT(log.count == 3);
    GS_ASSERT(log.ids[0] == 101 && log.ids[1] == 201 && log.ids[2] == 102);
    GS_ASSERT(log.origins[0] == (uint8_t)GAME_OOT && log.origins[1] == (uint8_t)GAME_OOT &&
              log.origins[2] == (uint8_t)GAME_OOT);

    // Single-use under a repeated safe point.
    std::memset(&log, 0, sizeof(log));
    GS_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, GrantTestAward, &log) == 0);
    GS_ASSERT(log.count == 0);

    // The MM-bound grant was untouched and redeems for MM.
    GS_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 1);
    std::memset(&log, 0, sizeof(log));
    GS_ASSERT(Combo_RedeemSharedItemsForGame(GAME_MM, GrantTestAward, &log) == 1);
    GS_ASSERT(log.ids[0] == 301 && log.origins[0] == (uint8_t)GAME_MM);

    // A grant arriving AFTER a redemption pass redeems at the next safe point.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 3, GAME_OOT, 103) == RSBS_GRANT_ACCEPTED);
    std::memset(&log, 0, sizeof(log));
    GS_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, GrantTestAward, &log) == 1);
    GS_ASSERT(log.ids[0] == 103);

    GrantTestReset();
    printf("[TEST] PASS: redemption is a safe-point call, in received order, single-use, no switch required\n");
    return TEST_PASS;
}

TestResult Test_GrantOverflow(void) {
    printf("[TEST] grant-overflow: full array refuses loudly, backpressures sources, reclaims redeemed slots "
           "(ADR 0005)\n");

    GrantTestReset();
    GS_ASSERT(Combo_GetSharedItemOverflowCount() == 0);

    // Fill the array: 62 OoT-bound grants, then 2 MM-bound, all one source.
    for (uint32_t i = 1; i <= RSBS_SHARED_ITEM_CAP - 2u; i++) {
        GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, i, GAME_OOT, (uint16_t)(1000u + i)) == RSBS_GRANT_ACCEPTED);
    }
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, RSBS_SHARED_ITEM_CAP - 1u, GAME_MM, 501) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, RSBS_SHARED_ITEM_CAP, GAME_MM, 502) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(GrantTestOccupiedTotal() == (int)RSBS_SHARED_ITEM_CAP);

    // Every entry is un-redeemed: the next sourced grant is BACKPRESSURED —
    // refused loudly, cursor unmoved, durable overflow count up.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, RSBS_SHARED_ITEM_CAP + 1u, GAME_OOT, 1065) ==
              RSBS_GRANT_RETRY_FULL);
    GS_ASSERT(Combo_GetGrantCursor(kGrantSrcA) == RSBS_SHARED_ITEM_CAP);
    GS_ASSERT(Combo_GetSharedItemOverflowCount() == 1);

    // In-process refusals are counted too (a genuine loss, made diagnosable).
    GS_ASSERT(Combo_RecordSharedItem(GAME_OOT, 2000) == -1);
    GS_ASSERT(Combo_GetSharedItemOverflowCount() == 2);
    GS_ASSERT(Combo_StageSharedItem(GAME_OOT, 2001) == true);
    GS_ASSERT(Combo_CommitStagedSharedItems() == 0);
    GS_ASSERT(Combo_GetSharedItemOverflowCount() == 3);

    // Redeem the two MM entries: capacity returns via oldest-first reclaim.
    GrantAwardLog log;
    std::memset(&log, 0, sizeof(log));
    GS_ASSERT(Combo_RedeemSharedItemsForGame(GAME_MM, GrantTestAward, &log) == 2);
    GS_ASSERT(log.ids[0] == 501 && log.ids[1] == 502);

    // THE NO-SILENT-LOSS PROPERTY: the refused seq, retried after capacity
    // returned, is ACCEPTED — the cursor never advanced past it, so the retry
    // is not mistaken for a duplicate and the grant is not lost.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, RSBS_SHARED_ITEM_CAP + 1u, GAME_OOT, 1065) ==
              RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, RSBS_SHARED_ITEM_CAP + 2u, GAME_OOT, 1066) ==
              RSBS_GRANT_ACCEPTED);
    GS_ASSERT(GrantTestOccupiedTotal() == (int)RSBS_SHARED_ITEM_CAP);
    GS_ASSERT(Combo_GetSharedItemOverflowCount() == 3); // reclaim is not a refusal

    // Both redeemed entries are gone; a third grant is backpressured again.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, RSBS_SHARED_ITEM_CAP + 3u, GAME_OOT, 1067) ==
              RSBS_GRANT_RETRY_FULL);
    GS_ASSERT(Combo_GetSharedItemOverflowCount() == 4);

    // Reclamation preserved received order: the 62 originals award before the
    // two late arrivals, all in acceptance order.
    std::memset(&log, 0, sizeof(log));
    GS_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, GrantTestAward, &log) == (int)RSBS_SHARED_ITEM_CAP);
    GS_ASSERT(log.count == (int)RSBS_SHARED_ITEM_CAP);
    for (uint32_t i = 0; i < RSBS_SHARED_ITEM_CAP - 2u; i++) {
        GS_ASSERT(log.ids[i] == (uint16_t)(1000u + i + 1u));
    }
    GS_ASSERT(log.ids[RSBS_SHARED_ITEM_CAP - 2u] == 1065 && log.ids[RSBS_SHARED_ITEM_CAP - 1u] == 1066);

    // The once-refused-then-accepted seq is now firmly a duplicate.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, RSBS_SHARED_ITEM_CAP + 1u, GAME_OOT, 1065) ==
              RSBS_GRANT_DUPLICATE);

    GrantTestReset();
    printf("[TEST] PASS: overflow refuses loudly and durably; backpressure retries deliver; order survives "
           "reclamation\n");
    return TEST_PASS;
}

TestResult Test_GrantPersistence(void) {
    printf("[TEST] grant-persistence: cursors + overflow ride the .redsave; legacy loads unset; reset retires "
           "atomically (ADR 0005)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kGrantSaveDir);

    // Save-harness precondition, NOT a grant-model dependency: SaveManager::Save
    // serializes Tiers 2/3 from the context-layer shadow buffers and refuses
    // outright (save.cpp: "refuse rather than write a half-empty file") if either
    // is absent. Context_InitFrozenStates allocates both at capacity; it is
    // idempotent, and ClearAll only zeroes them, so once initialized they stay
    // present. GrantTestReset deliberately does not do this — the other three
    // grant tests must prove the model needs no switch machinery — so this test
    // states the precondition itself instead of inheriting it from whichever
    // save test happened to run earlier in the same process. Without it,
    // `redship --test grant-persistence` standalone fails at Save while the
    // pooled `--test all` run passes, which is exactly how this presented.
    Context_InitFrozenStates();
    GS_ASSERT(Context_GetOoTSaveContext() != NULL && Context_GetMMSaveContext() != NULL);

    // ------------------------------------------------------------------
    // (a) Round-trip: sourced entries, cursors, and the overflow count all
    //     survive Save/Load byte-exact — and the duplicate window STAYS
    //     closed across the reload.
    // ------------------------------------------------------------------
    GrantTestReset();
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 1, GAME_OOT, 11) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 2, GAME_MM, 22) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcB, 1, GAME_OOT, 33) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, NULL, NULL) == 2);
    gComboCtx.sharedItemOverflowCount = 7; // stand-in for a recorded refusal; serialization is what's under test

    SharedItem expectedItems[RSBS_SHARED_ITEM_CAP];
    ComboGrantSourceCursor expectedCursors[RSBS_GRANT_SOURCE_CAP];
    std::memcpy(expectedItems, gComboCtx.sharedItemsTagged, sizeof(expectedItems));
    std::memcpy(expectedCursors, gComboCtx.grantCursors, sizeof(expectedCursors));

    mgr.DeleteSave(0);
    GS_ASSERT(mgr.Save(0));

    // Wipe, then scribble, so restored zeros are provably the loader's doing.
    ComboContext_Init();
    std::memset(gComboCtx.sharedItemsTagged, 0x5A, sizeof(gComboCtx.sharedItemsTagged));
    std::memset(gComboCtx.grantCursors, 0x5A, sizeof(gComboCtx.grantCursors));
    gComboCtx.sharedItemOverflowCount = 0xDEADBEEFu;

    GS_ASSERT(mgr.Load(0));
    GS_ASSERT(std::memcmp(expectedItems, gComboCtx.sharedItemsTagged, sizeof(expectedItems)) == 0);
    GS_ASSERT(std::memcmp(expectedCursors, gComboCtx.grantCursors, sizeof(expectedCursors)) == 0);
    GS_ASSERT(gComboCtx.sharedItemOverflowCount == 7);
    // Typed spot-checks so a memcmp-passing-but-misread layout fails loudly.
    GS_ASSERT(Combo_GetGrantCursor(kGrantSrcA) == 2 && Combo_GetGrantCursor(kGrantSrcB) == 1);
    GS_ASSERT(Combo_CountGrantSources() == 2);
    GS_ASSERT(gComboCtx.sharedItemsTagged[0].flags == (RSBS_SHARED_ITEM_SOURCED | RSBS_SHARED_ITEM_REDEEMED));
    GS_ASSERT(gComboCtx.sharedItemsTagged[1].flags == RSBS_SHARED_ITEM_SOURCED); // MM grant still pending

    // The reload did NOT re-open the duplicate window: every delivered seq is
    // still a duplicate, and delivery resumes exactly at cursor + 1.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 1, GAME_OOT, 11) == RSBS_GRANT_DUPLICATE);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 2, GAME_MM, 22) == RSBS_GRANT_DUPLICATE);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcB, 1, GAME_OOT, 33) == RSBS_GRANT_DUPLICATE);
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 3, GAME_OOT, 44) == RSBS_GRANT_ACCEPTED);
    mgr.DeleteSave(0);

    // ------------------------------------------------------------------
    // (b) Migration: a PRE-NETPLAY .redsave (Tier-1 truncated at the ADR 0005
    //     carve) loads with items intact and every new field unset.
    // ------------------------------------------------------------------
    GrantTestReset();
    GS_ASSERT(Combo_RecordSharedItem(GAME_OOT, 0x00A7) >= 0); // a real legacy save holds in-process entries only
    GS_ASSERT(Combo_RecordSharedItem(GAME_MM, 0x0042) >= 0);
    GS_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, NULL, NULL) == 1);
    GS_ASSERT(GrantTestWriteLegacySlot(mgr.SlotPath(0)));

    ComboContext_Init();
    std::memset(gComboCtx.grantCursors, 0x5A, sizeof(gComboCtx.grantCursors));
    gComboCtx.sharedItemOverflowCount = 0xDEADBEEFu;

    GS_ASSERT(mgr.Load(0));
    GS_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true) == 1);
    GS_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 1);
    GS_ASSERT(Combo_CountGrantSources() == 0);              // no source ever delivered to a legacy save
    GS_ASSERT(Combo_GetSharedItemOverflowCount() == 0);     // and it never overflowed
    for (uint32_t i = 0; i < RSBS_GRANT_SOURCE_CAP; i++) {
        GS_ASSERT(gComboCtx.grantCursors[i].sourceKey == 0 && gComboCtx.grantCursors[i].lastSeq == 0);
    }
    // A legacy world accepts its first sourced delivery like any fresh one.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 1, GAME_OOT, 55) == RSBS_GRANT_ACCEPTED);
    mgr.DeleteSave(0);

    // ------------------------------------------------------------------
    // (c) Reset invalidation (the #440 composition contract): one
    //     ComboContext_Init retires items AND cursors AND the overflow count
    //     together, so a dead session can neither replay its grants into a
    //     fresh world nor block that world's own deliveries.
    // ------------------------------------------------------------------
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 2, GAME_MM, 66) == RSBS_GRANT_ACCEPTED);
    GS_ASSERT(Combo_CountGrantSources() == 1 && GrantTestOccupiedTotal() > 0);

    ComboContext_Init();
    GS_ASSERT(Combo_CountGrantSources() == 0);
    GS_ASSERT(GrantTestOccupiedTotal() == 0);
    GS_ASSERT(Combo_GetSharedItemOverflowCount() == 0);
    // A dead-room retransmit is NOT silently accepted into the fresh world...
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 2, GAME_MM, 66) == RSBS_GRANT_GAP);
    GS_ASSERT(GrantTestOccupiedTotal() == 0);
    // ...while a fresh session's own delivery stream starts cleanly.
    GS_ASSERT(Combo_SubmitSourcedGrant(kGrantSrcA, 1, GAME_OOT, 77) == RSBS_GRANT_ACCEPTED);

    GrantTestReset();
    printf("[TEST] PASS: durable across save/load; legacy records read unset; reset retires items + cursors "
           "atomically\n");
    return TEST_PASS;
}
