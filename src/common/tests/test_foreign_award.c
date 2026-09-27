/**
 * @file test_foreign_award.c
 * @brief ROM-free lock for the MM-side foreign-item AWARD (Lane 6 / #502).
 *
 * The gap this closes is stated verbatim in test_foreign_items.c's reverse row:
 * that lock drives Combo_RedeemSharedItemsForGame with a TEST award callback,
 * "which is still a Lane-C placeholder fprintf, so asserting through it would
 * assert nothing". This file asserts through the REAL one.
 *
 * The chain under test, end to end, with no stand-ins:
 *
 *   Combo_RecordSharedItem(GAME_MM, id)     the real in-process producer
 *     -> MM_ConsumeSharedItems()            MM's real consumer hook (the exact
 *                                           function z_play.c:2406 calls)
 *       -> Combo_RedeemSharedItemsForGame   the real consumer walk
 *         -> MM_AwardSharedItem             MM's real award callback
 *           -> MM_ForeignItem_Give          the real give entry point
 *
 * WHAT IS OBSERVABLE AT THIS TIER, AND WHY THAT IS THE INTERESTING PART.
 * The give's terminal step (Rando::GiveItem) needs a live PlayState and a
 * loaded MM save, neither of which exists in a display-free test process. That
 * is not a limitation being worked around — it is precisely the production
 * condition #502 has to survive. MM's redemption point runs BEFORE
 * `MM_gPlayState = this` (z_play.c:2406 vs :2468), and MM's give path is not
 * NULL-play tolerant: Item_GiveImpl's Health_ChangeBy / Magic_Add /
 * Inventory_IncrementSkullTokenCount legs deref `play` unguarded. So the give
 * defers into a pending queue and drains on the first gameplay frame
 * (2s2h/Rando/ForeignItemsSingleExe.cpp).
 *
 * A test process is therefore in EXACTLY the state the arrival point is in —
 * MM_gPlayState == NULL — and the pending queue is a faithful observable for
 * "the award reached the give". A build that regressed to the unguarded
 * immediate give would not fail an assertion here; it would segfault, which is
 * a louder red than any assertion.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE, compiled as C++
 * like its siblings. Every symbol it drives is C-linkage.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>

extern "C" {
// MM's real consumer hook (GameExports_SingleExe.cpp) — C linkage because
// z_play.c calls it. Driving THIS rather than Combo_RedeemSharedItemsForGame
// directly is what puts MM_AwardSharedItem, which is static, under test.
void MM_ConsumeSharedItems(void);

// The give's test surface (2s2h/Rando/ForeignItemsSingleExe.cpp).
int MM_ForeignItem_Give(uint16_t riId);
int MM_ForeignItem_FlushPending(void);
int MM_ForeignItem_TestPendingCount(void);
uint16_t MM_ForeignItem_TestPendingAt(int index);
void MM_ForeignItem_TestResetPending(void);
int MM_ForeignItem_TestItemIdMax(void);
int MM_ForeignItem_TestIsGiveableId(uint16_t riId);
int MM_ForeignItem_TestIsJunkClassId(uint16_t riId);

// The arrival toast's payload, as the give builds it (#494). Same
// BuildArrivalToast the Notification::Emit in GiveNow is handed.
int MM_ForeignItem_TestArrivalText(uint16_t riId, char* out, int cap);
const char* MM_ForeignItem_TestArrivalIcon(uint16_t riId);

// The RandoItemId sentinels, from the #488 bridge block in Rando/Foreign.cpp.
void MM_Rando_Foreign_TestItemSentinels(uint16_t* outJunk, uint16_t* outNone, uint16_t* outUnknown);
}

#define FA_ASSERT(cond)                                                                                                \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

TestResult Test_ForeignAwardMM(void) {
    printf("[TEST] foreign-award-mm: MM's real award callback reaches the real give, once per crossing (#502)\n");

    uint16_t riJunk = 0xFFFF;
    uint16_t riNone = 0xFFFF;
    uint16_t riUnknown = 0xFFFF;
    MM_Rando_Foreign_TestItemSentinels(&riJunk, &riNone, &riUnknown);
    const int itemIdMax = MM_ForeignItem_TestItemIdMax();
    FA_ASSERT(itemIdMax > 1);

    // ------------------------------------------------------------------
    // The id predicate, driven directly (it is the real one, not a copy).
    // ------------------------------------------------------------------
    // RI_UNKNOWN is enumerator 0 and RI_NONE is "literally nothing"; both are
    // declared RITYPE_JUNK, so a type-based test accepts them — the same
    // sentinel trap #488 found on the HOST side, here on the ITEM side. An id
    // at or past RI_MAX would index StaticData::Items out of range inside the
    // give, which is a read past the end of a real array, not a benign no-op.
    FA_ASSERT(riUnknown == 0);
    FA_ASSERT(!MM_ForeignItem_TestIsGiveableId(riUnknown));
    FA_ASSERT(!MM_ForeignItem_TestIsGiveableId(riNone));
    FA_ASSERT(!MM_ForeignItem_TestIsGiveableId((uint16_t)itemIdMax));
    FA_ASSERT(!MM_ForeignItem_TestIsGiveableId((uint16_t)(itemIdMax + 1)));
    FA_ASSERT(MM_ForeignItem_TestIsGiveableId(riJunk));

    // Two distinct real ids for the order leg below, taken from the table
    // rather than hardcoded so this does not rot against the enum.
    uint16_t idA = 0;
    uint16_t idB = 0;
    for (int id = 1; id < itemIdMax; id++) {
        if (!MM_ForeignItem_TestIsGiveableId((uint16_t)id)) {
            continue;
        }
        if (idA == 0) {
            idA = (uint16_t)id;
        } else if (idB == 0) {
            idB = (uint16_t)id;
            break;
        }
    }
    FA_ASSERT(idA != 0 && idB != 0 && idA != idB);

    // ------------------------------------------------------------------
    // Clean slate.
    // ------------------------------------------------------------------
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();
    FA_ASSERT(MM_ForeignItem_TestPendingCount() == 0);

    // ------------------------------------------------------------------
    // (1) One crossing: real producer -> real consumer -> real award -> give.
    // ------------------------------------------------------------------
    FA_ASSERT(Combo_RecordSharedItem(GAME_MM, idA) >= 0);
    FA_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 1);

    MM_ConsumeSharedItems();

    // The award reached the give exactly once, carrying the right id.
    FA_ASSERT(MM_ForeignItem_TestPendingCount() == 1);
    FA_ASSERT(MM_ForeignItem_TestPendingAt(0) == idA);
    // ...and the crossing is retired in gComboCtx: redeemed, still present.
    FA_ASSERT(gComboCtx.sharedItemsTagged[0].originGame == (uint8_t)GAME_MM);
    FA_ASSERT(gComboCtx.sharedItemsTagged[0].id == idA);
    FA_ASSERT((gComboCtx.sharedItemsTagged[0].flags & RSBS_SHARED_ITEM_REDEEMED) != 0);
    FA_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 0);
    FA_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == 1);

    // ------------------------------------------------------------------
    // (2) Single-use: a second arrival awards nothing.
    // ------------------------------------------------------------------
    // This is the assertion the placeholder fprintf could never make. A give
    // that cleared the entry, or a consumer that re-awarded a redeemed one,
    // shows up here as a second queue entry.
    MM_ConsumeSharedItems();
    FA_ASSERT(MM_ForeignItem_TestPendingCount() == 1);

    // ------------------------------------------------------------------
    // (3) The NULL-play deferral is real, not incidental.
    // ------------------------------------------------------------------
    // The flush must decline while MM_gPlayState is NULL — which it is, here
    // and at MM's arrival point — and must NOT drop what it declined to give.
    // A flush that fired anyway would not fail this assertion; it would crash
    // inside Item_GiveImpl. Both outcomes are red; only one is legible.
    FA_ASSERT(MM_ForeignItem_FlushPending() == 0);
    FA_ASSERT(MM_ForeignItem_TestPendingCount() == 1);

    // ------------------------------------------------------------------
    // (4) Order is preserved across the whole chain.
    // ------------------------------------------------------------------
    // Received-order awarding is a contract, not an accident: the give paths
    // resolve progressive items against the live save, so order changes WHAT
    // the player receives (shared_items.h). Combo_RedeemSharedItemsForGame
    // awards in slot order; the pending queue has to be FIFO or that contract
    // is quietly reversed between the award and the frame that gives.
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();
    FA_ASSERT(Combo_RecordSharedItem(GAME_MM, idB) >= 0);
    FA_ASSERT(Combo_RecordSharedItem(GAME_MM, idA) >= 0);
    FA_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 2);

    MM_ConsumeSharedItems();
    FA_ASSERT(MM_ForeignItem_TestPendingCount() == 2);
    FA_ASSERT(MM_ForeignItem_TestPendingAt(0) == idB); // slot order, not id order
    FA_ASSERT(MM_ForeignItem_TestPendingAt(1) == idA);

    // ------------------------------------------------------------------
    // (5) An OoT-origin entry is never awarded to MM.
    // ------------------------------------------------------------------
    // The forward direction's items live in the same array. MM's consumer must
    // filter on originGame, or every OoT-bound crossing gets fed to MM's give
    // as a raw RI_* — the #356 aliasing class, at the arrival point.
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();
    SharedItem ootItem;
    FA_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &ootItem));
    FA_ASSERT(Combo_RecordSharedItem(GAME_OOT, ootItem.id) >= 0);

    MM_ConsumeSharedItems();
    FA_ASSERT(MM_ForeignItem_TestPendingCount() == 0);
    FA_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 1); // still owed to OoT

    // ------------------------------------------------------------------
    // (6) An unresolvable id consumes the redemption but queues nothing.
    // ------------------------------------------------------------------
    // A sentinel or out-of-range id in the durable array is a data bug (a
    // corrupt .redsave, a pool that outgrew its id-space). It must not be
    // retried on every future arrival, and it must never reach the give.
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();
    FA_ASSERT(Combo_RecordSharedItem(GAME_MM, (uint16_t)itemIdMax) >= 0);

    MM_ConsumeSharedItems();
    FA_ASSERT(MM_ForeignItem_TestPendingCount() == 0);
    FA_ASSERT((gComboCtx.sharedItemsTagged[0].flags & RSBS_SHARED_ITEM_REDEEMED) != 0);
    FA_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 0);
    // The direct entry point reports the refusal rather than swallowing it.
    FA_ASSERT(MM_ForeignItem_Give(riNone) == 0);
    FA_ASSERT(MM_ForeignItem_TestPendingCount() == 0);

    // ------------------------------------------------------------------
    // (7) The presentation RESOLUTION SURFACE (#494).
    // ------------------------------------------------------------------
    // Presentation is display-tier: what the player sees is the operator's to
    // verify, and CI must not pretend otherwise. What CI can honestly lock is
    // that every surface which HAS to name a crossing can: the MM pickup
    // textbox (Rando::Foreign::ForeignNameForCheck), the OoT arrival and pickup
    // toasts (Combo_GetForeignItemName) and both spoilers all resolve through
    // the origin game's describer. Under one bag ANY progression item may cross
    // (ADR 0010 increment 3), so this walks every id MM's give accepts, not a
    // pool: an MM item the give accepts but the describer cannot name would
    // present as "a foreign item" in OoT, silently.
    int mmNamed = 0;
    for (int id = 1; id < itemIdMax; id++) {
        // In the give's id range AND a row of MM's item table (the enum has gaps
        // with no table row; MM_ForeignItem_TestIsJunkClassId reports those -1).
        if (!MM_ForeignItem_TestIsGiveableId((uint16_t)id) || MM_ForeignItem_TestIsJunkClassId((uint16_t)id) < 0) {
            continue;
        }
        SharedItem mmItem;
        mmItem.originGame = (uint8_t)GAME_MM;
        mmItem.flags = 0;
        mmItem.id = (uint16_t)id;
        const char* name = Combo_GetForeignItemName(mmItem);
        if (name == NULL || name[0] == '\0') {
            printf("[TEST] FAIL: MM item id %d is giveable but its describer names it nothing\n", id);
            return TEST_FAIL;
        }
        FA_ASSERT(Combo_GetForeignItemArticle(mmItem) != NULL);
        mmNamed++;
    }
    FA_ASSERT(mmNamed > 0);

    // ------------------------------------------------------------------
    // (8) The MM ARRIVAL toast names the item, natively (#494).
    // ------------------------------------------------------------------
    // Until #494 the MM arrival was SILENT. The give now emits MM's own pickup
    // toast, and this is the honest lock on it — the pixels are the operator's
    // to verify, the RESOLUTION is CI's.
    //
    // The assertion is an EQUALITY against the WHOLE toast — every field the
    // overlay draws, joined the way it draws them — so any tell added back
    // anywhere ("… from Ocarina of Time", an origin badge in `.prefix`, a
    // changed verb) breaks it. The expected line is built from the DESCRIBER's
    // article + name (the surface every other presentation reads), while the
    // toast is built by MM's own GetItemName: two paths over MM's table that
    // must agree, for every item MM's give accepts.
    {
        char shown[128];
        char expected[128];
        int compared = 0;
        for (int id = 1; id < itemIdMax; id++) {
            if (!MM_ForeignItem_TestIsGiveableId((uint16_t)id) || MM_ForeignItem_TestIsJunkClassId((uint16_t)id) < 0 ||
                id == (int)riJunk) {
                continue; // not a table row; RI_JUNK's toast names the CURRENT junk item, by design
            }
            SharedItem mmItem;
            mmItem.originGame = (uint8_t)GAME_MM;
            mmItem.flags = 0;
            mmItem.id = (uint16_t)id;
            const int len = MM_ForeignItem_TestArrivalText((uint16_t)id, shown, (int)sizeof(shown));
            FA_ASSERT(len > 0);
            const char* article = Combo_GetForeignItemArticle(mmItem);
            FA_ASSERT(article != NULL);
            snprintf(expected, sizeof(expected), "You got %s%s", article, Combo_GetForeignItemName(mmItem));
            if (strcmp(shown, expected) != 0) {
                printf("[TEST] FAIL: arrival toast for MM item %d shows '%s', expected '%s'\n", id, shown, expected);
                return TEST_FAIL;
            }
            // The icon is allowed to be absent — Emit renders a text-only toast
            // for a null icon. What must not happen is an empty-but-present
            // path, which would draw a blank 24x24 hole.
            const char* icon = MM_ForeignItem_TestArrivalIcon((uint16_t)id);
            FA_ASSERT(icon == NULL || icon[0] != '\0');
            compared++;
        }
        FA_ASSERT(compared > 0);
        printf("[TEST] foreign-award-mm: %d MM arrival toasts match their describer article + name\n", compared);
        // A sentinel id resolves to nothing rather than to a fabricated string:
        // the toast is only ever built for an id the give accepted.
        FA_ASSERT(MM_ForeignItem_TestArrivalText(riNone, shown, (int)sizeof(shown)) == -1);
        FA_ASSERT(MM_ForeignItem_TestArrivalIcon(riUnknown) == NULL);
    }

    // Leave global state clean for any subsequent row.
    MM_ForeignItem_TestResetPending();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();

    printf("[TEST] PASS: MM's real award reaches the real give once per crossing, defers safely, "
           "preserves order, filters by origin, and names every arrival natively\n");
    return TEST_PASS;
}
