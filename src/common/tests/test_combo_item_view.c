/**
 * @file test_combo_item_view.c
 * @brief ROM-free lock for the unified item view and OoT's item adapter
 *        (#458 U1a). U1b adds the MM legs.
 *
 * What this proves, and how each claim would fail without the code under test:
 *
 * 1. UNREGISTERED IS INERT. With no adapter every per-game read answers
 *    UNAVAILABLE / 0 / false and the labels say "No data"; a half-filled vtable
 *    and a non-game id are refused at registration.
 *
 * 2. THE PRODUCTION ENTRY POINT REGISTERS OoT's ADAPTER. With the registry
 *    cleared, Combo_TrackerWindow_Init (what rsbs main calls; headless-safe,
 *    it also registers the check-tracker adapters, as production does) must
 *    leave OoT's item adapter registered. Without that call the view would read
 *    OoT as UNAVAILABLE forever while every other leg still passed.
 *
 *    THE OoT ADAPTER READS ONLY THE BUFFER IT IS HANDED. An OoT SaveContext
 *    image authored in the OoT TU (the layout never crosses into this file) is
 *    committed as OoT's frozen shadow through the production
 *    Context_UpdateShadowCopy, and the rows recover exactly the authored world:
 *    the progressive hookshot names its tier, the bow carries its ammo and the
 *    quiver's capacity, equipment and quest bits, the tokens, the heart pieces,
 *    the wallet's rupees and capacity, a bottle's contents. The shadow bytes are
 *    unchanged afterwards (the adapter never writes), and a misaligned buffer is
 *    refused rather than read (the alignof contract on the shadow).
 *
 * 3. AN ALL-ZERO SHADOW IS "NO DATA", NOT AN EMPTY INVENTORY. Before OoT ever
 *    froze, the shadow is zeros and the rows must not read as a save holding
 *    nothing.
 *
 * 4. THE LIVE-VS-SHADOW PICK, AND NEVER LIVE FOR THE OTHER GAME. A test vtable
 *    (the production one, liveSave pointed at an authored live buffer) drives
 *    the view: under GAME_OOT the live buffer's rows are LIVE; under GAME_MM and
 *    GAME_NONE the shadow's rows are STALE and the live source is never even
 *    asked (a call counter); an unmarked live buffer falls back to the shadow; no
 *    live source and no shadow is UNAVAILABLE. The production vtable's liveSave
 *    answers NULL with no OoT play state (this tier).
 *
 * 5. THE SHARED GROUP AND ITS LABEL. An empty pool is UNAVAILABLE, "No data",
 *    zero rows. After production harvests (rupees, the quiver tier and arrows,
 *    the heart quantity) the group is STALE, labelled "As of the last switch or
 *    save", and its rows carry the pooled values; the ocarina row exists exactly
 *    when that kind is armed. Hearts count whole hearts (pieces truncated); the
 *    wallet row is held at every tier, tier 0 being the child's wallet. Labels
 *    are player wording.
 *
 * 6. STATE PUT BACK. The OoT shadow goes back to the bytes this row inherited
 *    (asserted byte-equal; a shadow that was absent comes back all-zero, which
 *    reads as absent by the zero-means-no-data rule), the context, the active
 *    game and the item-adapter registry are restored. The shared-pool
 *    watermarks are RESET to zero, not restored: shared_resources.c has no
 *    accessor to snapshot them. The check-tracker adapters stay registered, as
 *    the production bring-up leaves them.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++);
 * every symbol it drives is extern-C. The entry point is a plain function the
 * Test_ComboItemView wrapper calls after the display-free shared bring-up.
 */

#include "../ComboTrackerWindow.h" // Combo_TrackerWindow_Init: the production registration
#include "../combo_item_view.h"
#include "../context.h"
#include "../game.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <vector>

// The OoT-side authoring seam (games/oot/soh/Enhancements/randomizer/
// ItemAdapterSingleExe.cpp): writes a started OoT save into `buf` through
// OoT's own layout. Variant 0 is the "shadow" world, variant 1 the "live" one
// (see the seam for what each holds). Returns 0 when `size` is too small.
extern "C" int OoT_ItemAdapter_TestAuthorSave(void* buf, size_t size, int variant);

#define CIV_ASSERT(cond)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return TEST_FAIL;                                              \
        }                                                                  \
    } while (0)

// ---- the test live source ---------------------------------------------------
static const void* sCivLive = NULL;
static int sCivLiveCalls = 0;

static const void* CivLiveSave(void) {
    sCivLiveCalls++;
    return sCivLive;
}

/** Find `game`'s row named `name`; false when absent. */
static bool CivFindRow(uint8_t game, const char* name, ComboItemRow* out) {
    const int n = Combo_ItemCount(game);
    for (int i = 0; i < n; i++) {
        if (Combo_ItemRowAt(game, i, out) && strcmp(out->name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool CivFindSharedRow(const char* name, ComboItemRow* out) {
    const int n = Combo_ItemSharedCount();
    for (int i = 0; i < n; i++) {
        if (Combo_ItemSharedRowAt(i, out) && strcmp(out->name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool CivLabelIsPlayerWording(const char* label) {
    if (label == NULL || label[0] < 'A' || label[0] > 'Z') {
        return false;
    }
    const size_t len = strlen(label);
    if (label[len - 1] == '.' || label[len - 1] == ')') {
        return false;
    }
    for (const char* jargon : { "freeze", "shadow", "suspend", "heap", "pool", "harvest" }) {
        if (strstr(label, jargon) != NULL) {
            return false;
        }
    }
    return true;
}

/** Legs 2-4 over OoT's REAL registered adapter, `ops`. */
static int CivOoTLegs(const ComboItemOps* ops) {
    ComboItemRow row;

    // ---- 3. all-zero shadow: no data ------------------------------------
    std::vector<uint8_t> zeros((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    Context_UpdateShadowCopy(GAME_OOT, zeros.data(), zeros.size());
    for (GameId g : { GAME_OOT, GAME_MM, GAME_NONE }) {
        Context_SetCurrentGame(g);
        CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_UNAVAILABLE);
        CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_OOT) == 0);
        CIV_ASSERT(!Combo_ItemRowAt((uint8_t)GAME_OOT, 0, &row));
    }

    // ---- 2. the authored shadow world, read while MM is active ------------
    std::vector<uint8_t> shadow((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    CIV_ASSERT(OoT_ItemAdapter_TestAuthorSave(shadow.data(), shadow.size(), 0) == 1);
    CIV_ASSERT(ops->hasSave(shadow.data()));
    CIV_ASSERT(!ops->hasSave(zeros.data()));
    Context_UpdateShadowCopy(GAME_OOT, shadow.data(), shadow.size());
    const std::vector<uint8_t> shadowAuthored(shadow);

    Context_SetCurrentGame(GAME_MM);
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_STALE);
    const int rows = Combo_ItemCount((uint8_t)GAME_OOT);
    printf("[TEST] combo-item-view: OoT rows=%d freshness=%u under GAME_MM\n", rows,
           (unsigned)Combo_ItemFreshness((uint8_t)GAME_OOT));
    CIV_ASSERT(rows == ops->count());
    for (int i = 0; i < rows; i++) {
        CIV_ASSERT(Combo_ItemRowAt((uint8_t)GAME_OOT, i, &row));
        CIV_ASSERT(row.group != NULL && row.group[0] != '\0');
        CIV_ASSERT(row.name != NULL && row.name[0] != '\0');
        CIV_ASSERT(row.iconKey != NULL && row.iconKeyFaded != NULL); // every OoT row has an icon
        CIV_ASSERT(row.freshness == COMBO_TRACKER_FRESH_STALE);
        CIV_ASSERT(row.count >= 0 && row.max >= 0);
    }
    CIV_ASSERT(!Combo_ItemRowAt((uint8_t)GAME_OOT, rows, &row));
    CIV_ASSERT(!Combo_ItemRowAt((uint8_t)GAME_OOT, -1, &row));
    CIV_ASSERT(!Combo_ItemRowAt((uint8_t)GAME_OOT, 0, NULL));

    // Row content: exactly the authored world (the seam's variant 0).
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Longshot", &row));
    CIV_ASSERT(row.have && strcmp(row.group, "Inventory") == 0 && strcmp(row.iconKey, "ITEM_LONGSHOT") == 0);
    CIV_ASSERT(!CivFindRow((uint8_t)GAME_OOT, "Hookshot", &row)); // the row names its tier
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Fairy Bow", &row));
    printf("[TEST] combo-item-view: shadow Fairy Bow have=%d count=%d max=%d\n", (int)row.have, row.count, row.max);
    CIV_ASSERT(row.have && row.count == 35 && row.max == 40);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Fairy Slingshot", &row));
    CIV_ASSERT(!row.have && row.count == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Kokiri Sword", &row));
    CIV_ASSERT(row.have && strcmp(row.group, "Equipment") == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Master Sword", &row));
    CIV_ASSERT(!row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Zelda's Lullaby", &row));
    CIV_ASSERT(row.have && strcmp(row.group, "Songs") == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Song of Time", &row));
    CIV_ASSERT(!row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Forest Medallion", &row));
    CIV_ASSERT(row.have && strcmp(row.group, "Dungeon Rewards") == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Gold Skulltula Tokens", &row));
    CIV_ASSERT(row.have && row.count == 17 && row.max == 100);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Pieces of Heart", &row));
    CIV_ASSERT(row.have && row.count == 5 && row.max == 36);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Adult's Wallet", &row));
    CIV_ASSERT(row.have && row.count == 123 && row.max == 200);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Red Potion", &row));
    CIV_ASSERT(row.have && strcmp(row.iconKey, "ITEM_POTION_RED") == 0);

    // Read-only: the adapter wrote nothing into the shadow it was handed.
    const uint8_t* resident = (const uint8_t*)Context_GetOoTSaveContext();
    CIV_ASSERT(resident != NULL && memcmp(resident, shadowAuthored.data(), shadowAuthored.size()) == 0);

    // The alignof contract: a misaligned image is refused, never read.
    CIV_ASSERT(!ops->rowAt(shadow.data() + 1, 0, &row));
    CIV_ASSERT(!ops->hasSave(shadow.data() + 1));

    // Under GAME_OOT with the PRODUCTION live source: no OoT play state in this
    // tier, so it answers NULL and the view shows the shadow, STALE.
    Context_SetCurrentGame(GAME_OOT);
    CIV_ASSERT(ops->liveSave != NULL);
    CIV_ASSERT(ops->liveSave() == NULL);
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Longshot", &row) && row.freshness == COMBO_TRACKER_FRESH_STALE);

    // ---- 4. the live-vs-shadow pick --------------------------------------
    std::vector<uint8_t> live((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    CIV_ASSERT(OoT_ItemAdapter_TestAuthorSave(live.data(), live.size(), 1) == 1);
    static ComboItemOps sTestOps;
    sTestOps = *ops;
    sTestOps.liveSave = CivLiveSave;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, &sTestOps);
    CIV_ASSERT(Combo_Item_GetOps((uint8_t)GAME_OOT) == &sTestOps);
    sCivLive = live.data();

    // A. OoT active, its live save marked: LIVE, the live world's rows.
    Context_SetCurrentGame(GAME_OOT);
    sCivLiveCalls = 0;
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_LIVE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Hookshot", &row));
    printf("[TEST] combo-item-view: GAME_OOT live Hookshot have=%d freshness=%u\n", (int)row.have,
           (unsigned)row.freshness);
    CIV_ASSERT(row.have && row.freshness == COMBO_TRACKER_FRESH_LIVE);
    CIV_ASSERT(!CivFindRow((uint8_t)GAME_OOT, "Longshot", &row));
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Fairy Bow", &row) && !row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Song of Time", &row) && row.have);
    CIV_ASSERT(sCivLiveCalls > 0);
    CIV_ASSERT(strcmp(Combo_ItemFreshnessLabel((uint8_t)GAME_OOT, COMBO_TRACKER_FRESH_LIVE), "Updated live") == 0);

    // B + C. Another game (or none) active: the live source is never asked,
    // the shadow's rows are STALE.
    for (GameId g : { GAME_MM, GAME_NONE }) {
        Context_SetCurrentGame(g);
        sCivLiveCalls = 0;
        CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_STALE);
        CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Longshot", &row));
        CIV_ASSERT(row.have && row.freshness == COMBO_TRACKER_FRESH_STALE);
        CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Fairy Bow", &row) && row.have && row.count == 35);
        printf("[TEST] combo-item-view: under %s liveSave calls=%d (must be 0)\n",
               g == GAME_MM ? "GAME_MM" : "GAME_NONE", sCivLiveCalls);
        CIV_ASSERT(sCivLiveCalls == 0);
    }

    // D. OoT active but its live save carries no marker (a title or menu
    // save): the shadow, STALE.
    Context_SetCurrentGame(GAME_OOT);
    sCivLive = zeros.data();
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Longshot", &row) && row.freshness == COMBO_TRACKER_FRESH_STALE);

    // E. No live save and an empty shadow: UNAVAILABLE.
    sCivLive = NULL;
    Context_UpdateShadowCopy(GAME_OOT, zeros.data(), zeros.size());
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_UNAVAILABLE);
    CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_OOT) == 0);

    Combo_Item_RegisterOps((uint8_t)GAME_OOT, ops);
    return TEST_PASS;
}

/** Leg 5: the Shared group, over a freshly initialized context. */
static int CivSharedLegs(void) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    ComboItemRow row;

    CIV_ASSERT(Combo_ItemSharedFreshness() == COMBO_TRACKER_FRESH_UNAVAILABLE);
    CIV_ASSERT(strcmp(Combo_ItemSharedLabel(), "No data") == 0);
    CIV_ASSERT(Combo_ItemSharedCount() == 0);
    CIV_ASSERT(!Combo_ItemSharedRowAt(0, &row));

    // Production harvests, as a departing OoT would: 250 rupees, quiver tier 2
    // holding 33 arrows, 5 hearts and 2 pieces (0x10 per heart, 4 per piece).
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 250);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_QUIVER_TIER, 2);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_ARROW_COUNT, 33);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(0x50, 2));

    CIV_ASSERT(Combo_ItemSharedFreshness() == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(strcmp(Combo_ItemSharedLabel(), COMBO_ITEM_SHARED_LABEL) == 0);
    CIV_ASSERT(strcmp(COMBO_ITEM_SHARED_LABEL, "As of the last switch or save") == 0);
    const int n = Combo_ItemSharedCount();
    printf("[TEST] combo-item-view: shared rows=%d label=\"%s\"\n", n, Combo_ItemSharedLabel());
    CIV_ASSERT(n > 0);
    for (int i = 0; i < n; i++) {
        CIV_ASSERT(Combo_ItemSharedRowAt(i, &row));
        CIV_ASSERT(strcmp(row.group, COMBO_ITEM_SHARED_GROUP) == 0);
        CIV_ASSERT(row.name != NULL && row.name[0] != '\0');
        CIV_ASSERT(row.iconKey == NULL && row.iconKeyFaded == NULL); // text until U3
        CIV_ASSERT(row.freshness == COMBO_TRACKER_FRESH_STALE);
    }
    CIV_ASSERT(!Combo_ItemSharedRowAt(n, &row));
    CIV_ASSERT(!Combo_ItemSharedRowAt(0, NULL));

    CIV_ASSERT(CivFindSharedRow("Rupees", &row));
    CIV_ASSERT(row.have && row.count == 250);
    CIV_ASSERT(CivFindSharedRow("Arrows", &row));
    printf("[TEST] combo-item-view: shared Arrows have=%d count=%d max=%d\n", (int)row.have, row.count, row.max);
    CIV_ASSERT(row.have && row.count == 33 && row.max == 40);
    CIV_ASSERT(CivFindSharedRow("Hearts", &row));
    CIV_ASSERT(row.have && row.count == 5 && row.max == 20); // whole hearts: the 2 pieces are truncated
    // The wallet's tier 0 is the child's wallet every file holds: held, count =
    // the tier, never "0 of 3 not held" beside OoT's own (held) wallet row.
    CIV_ASSERT(CivFindSharedRow("Wallet", &row));
    printf("[TEST] combo-item-view: shared Wallet (tier 0) have=%d count=%d max=%d\n", (int)row.have, row.count,
           row.max);
    CIV_ASSERT(row.have && row.count == 0 && row.max == 3);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_WALLET_TIER, 1);
    CIV_ASSERT(CivFindSharedRow("Wallet", &row));
    CIV_ASSERT(row.have && row.count == 1 && row.max == 3);
    CIV_ASSERT(CivFindSharedRow("Bombs", &row));
    CIV_ASSERT(!row.have && row.count == 0); // never shared: nothing held
    CIV_ASSERT(CivFindSharedRow("Ocarina", &row) == Combo_SharedResourceKindArmed(RSBS_SHARED_RES_OCARINA_TIER));
    CIV_ASSERT(CivFindSharedRow("Triforce Pieces", &row) ==
               Combo_SharedResourceKindArmed(RSBS_SHARED_RES_TRIFORCE_PIECES));

    // Labels: player wording, and the pool's label is not a per-game one.
    CIV_ASSERT(CivLabelIsPlayerWording(Combo_ItemSharedLabel()));
    for (uint8_t f : { (uint8_t)COMBO_TRACKER_FRESH_LIVE, (uint8_t)COMBO_TRACKER_FRESH_STALE,
                       (uint8_t)COMBO_TRACKER_FRESH_UNAVAILABLE }) {
        CIV_ASSERT(CivLabelIsPlayerWording(Combo_ItemFreshnessLabel((uint8_t)GAME_OOT, f)));
        CIV_ASSERT(strcmp(Combo_ItemFreshnessLabel((uint8_t)GAME_OOT, f), Combo_ItemSharedLabel()) != 0);
    }
    CIV_ASSERT(strcmp(Combo_ItemFreshnessLabel((uint8_t)GAME_OOT, COMBO_TRACKER_FRESH_STALE),
                      "As of the last game switch or save") == 0);
    return TEST_PASS;
}

extern "C" int Combo_ItemView_RunHeadless(void) {
    printf("[TEST] combo-item-view: per-game item adapters over live/shadow buffers, the shared group (#458 U1a)\n");

    // ---- snapshot everything this row touches (restored at the end) --------
    const GameId prevGame = Context_GetCurrentGame();
    const ComboItemOps* prevOoT = Combo_Item_GetOps((uint8_t)GAME_OOT);
    const ComboItemOps* prevMM = Combo_Item_GetOps((uint8_t)GAME_MM);
    std::vector<uint8_t> ootShadowBackup((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    if (const uint8_t* p = (const uint8_t*)Context_GetOoTSaveContext()) {
        memcpy(ootShadowBackup.data(), p, ootShadowBackup.size());
    }
    static ComboContext sCtxBackup;
    sCtxBackup = gComboCtx;

    int result = TEST_PASS;
    do {
        // ---- 1. unregistered: inert --------------------------------------
        Combo_Item_RegisterOps((uint8_t)GAME_OOT, NULL);
        Combo_Item_RegisterOps((uint8_t)GAME_MM, NULL);
        ComboItemRow row;
        bool ok = true;
        for (uint8_t g : { (uint8_t)GAME_OOT, (uint8_t)GAME_MM, (uint8_t)GAME_NONE }) {
            ok = ok && Combo_ItemFreshness(g) == COMBO_TRACKER_FRESH_UNAVAILABLE && Combo_ItemCount(g) == 0 &&
                 !Combo_ItemRowAt(g, 0, &row) &&
                 strcmp(Combo_ItemFreshnessLabel(g, COMBO_TRACKER_FRESH_UNAVAILABLE), "No data") == 0;
        }
        if (!ok) {
            printf("[TEST] FAIL: an unregistered adapter is not inert\n");
            result = TEST_FAIL;
            break;
        }
        static const ComboItemOps kHalf = { NULL, NULL, NULL, NULL };
        Combo_Item_RegisterOps((uint8_t)GAME_OOT, &kHalf);
        Combo_Item_RegisterOps((uint8_t)GAME_NONE, &kHalf);
        if (Combo_Item_GetOps((uint8_t)GAME_OOT) != NULL || Combo_Item_GetOps((uint8_t)GAME_NONE) != NULL) {
            printf("[TEST] FAIL: a half-filled vtable or a non-game id was accepted\n");
            result = TEST_FAIL;
            break;
        }

        // ---- 2-4. OoT's production adapter -------------------------------
        // Registered through the PRODUCTION bring-up (rsbs main ->
        // Combo_TrackerWindow_Init, headless-safe here), never a direct
        // OoT_ItemAdapter_Register: the registry was cleared just above, so
        // a non-NULL vtable now can only have come from that entry point.
        Combo_TrackerWindow_Init();
        const ComboItemOps* ops = Combo_Item_GetOps((uint8_t)GAME_OOT);
        if (ops == NULL) {
            printf("[TEST] FAIL: Combo_TrackerWindow_Init did not register OoT's item adapter\n");
            result = TEST_FAIL;
            break;
        }
        if (ops->count() < 55 || ops->count() > 75) {
            printf("[TEST] FAIL: OoT item adapter row count %d is not the curated ~60\n", ops->count());
            result = TEST_FAIL;
            break;
        }
        printf("[TEST] combo-item-view: OoT adapter registered with %d curated rows\n", ops->count());
        if (CivOoTLegs(ops) != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }

        // ---- 5. the Shared group -----------------------------------------
        if (CivSharedLegs() != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }
    } while (0);

    // ---- 6. put state back (watermarks: reset, not restored; see header) ---
    sCivLive = NULL;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, prevOoT);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, prevMM);
    Context_UpdateShadowCopy(GAME_OOT, ootShadowBackup.data(), ootShadowBackup.size());
    gComboCtx = sCtxBackup;
    Combo_ResetSharedResourceWatermarks();
    Context_SetCurrentGame(prevGame);
    const uint8_t* restored = (const uint8_t*)Context_GetOoTSaveContext();
    if (restored == NULL || memcmp(restored, ootShadowBackup.data(), ootShadowBackup.size()) != 0) {
        printf("[TEST] FAIL: the OoT shadow was not restored byte-exact\n");
        return TEST_FAIL;
    }
    if (result != TEST_PASS) {
        return result;
    }

    printf("[TEST] PASS: item rows recover authored OoT live/shadow saves with honest freshness, never LIVE for the "
           "inactive game, and the shared group carries the pool and its label (#458 U1a)\n");
    return TEST_PASS;
}
