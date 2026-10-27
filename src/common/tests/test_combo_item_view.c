/**
 * @file test_combo_item_view.c
 * @brief ROM-free lock for the unified item view and both games' item
 *        adapters (#458 U1a: the view, OoT, the shared group; U1b: MM).
 *
 * The MM legs (U1b, CivMMLegs) repeat legs 2-4 over MM's REAL adapter, also
 * registered by Combo_TrackerWindow_Init: an MM SaveContext image authored in
 * MM's TU, committed as MM's shadow, read while OoT is active (bow ammo against
 * the quiver, beans, the bottle count, masks, songs with the progressive
 * lullaby naming its tier, remains, sword/shield tiers, the wallet's rupees
 * against its capacity); the shadow unchanged; a misaligned image refused; the
 * live-vs-shadow pick with BOTH games' live sources counted, so under GAME_MM
 * OoT's live source is never asked, under GAME_OOT MM's is never asked, and
 * under GAME_NONE neither is; the MM shadow restored byte-exact afterwards.
 *
 * What this proves, and how each claim would fail without the code under test:
 *
 * 1. UNREGISTERED IS INERT. With no adapter every per-game read answers
 *    UNAVAILABLE / 0 / false and the labels are empty (#927); a half-filled vtable
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
 * 5. THE SHARED GROUP. An empty pool is UNAVAILABLE, zero rows. After
 *    production harvests (rupees, the quiver tier and arrows, the heart
 *    quantity) the group is STALE and its rows carry the pooled values; the
 *    ocarina row exists exactly when that kind is armed. Hearts count whole
 *    hearts (pieces truncated); the wallet row is held at every tier, tier 0
 *    being the child's wallet. No label is a caption (#927; the
 *    ComboItemTrackerWindow row's leg 9 holds every label function).
 *
 * 5b. SHARED CAPACITY TIERS SHOW ONCE (#967). A world holding a Bow (tier-1
 *    quiver), a tier-2 bomb bag, sticks, nuts and the Kokiri Sword (authoring
 *    variant 2, both games) draws all five lit per game while the pool holds
 *    none of them. With the pool at those tiers each game's five per-game copies
 *    are dimmed (nothing held, no count) and nothing else changes; a copy ahead
 *    of the pool stays lit; the Shared Arrows row draws the Bow at tier 1 (each
 *    game's own art) and the quiver beyond; the Shared Kokiri Sword row exists
 *    only while the frozen rule is on; and a live game's own tiers and counts
 *    reach the Shared row before the next harvest.
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

#include "../ComboItemTrackerWindow.h" // #1142: SoH's DisplayType keys over the OoT tracker groups
#include "../ComboTrackerWindow.h"     // Combo_TrackerWindow_Init: the production registration
#include "../combo_item_view.h"
#include "../context.h"
#include "../foreign_items.h" // RSBS_XITEM_SHARED_OCARINA_BUTTONS: the #1067 leg
#include "../game.h"
#include "../save.h" // rsbs::SaveManager, SlotMeta: the new-file row's slot-panel leg (#873)
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h> // #1141: the MM group visibility keys

// The OoT-side authoring seam (games/oot/soh/Enhancements/randomizer/
// ItemAdapterSingleExe.cpp): writes a started OoT save into `buf` through
// OoT's own layout. Variant 0 is the "shadow" world, variant 1 the "live" one
// (see the seam for what each holds). Returns 0 when `size` is too small.
extern "C" int OoT_ItemAdapter_TestAuthorSave(void* buf, size_t size, int variant);
// The MM-side twin (games/mm/2s2h/Rando/ItemAdapterSingleExe.cpp), #458 U1b.
extern "C" int MM_ItemAdapter_TestAuthorSave(void* buf, size_t size, int variant);
// #1072: the variant-0 MM save with the file's frozen ocarina-button shuffle
// option and the held button bits set (mode 0 off, 1 on in a randomizer file,
// 2 on in a file that is not a randomizer file; heldMask bit i = the i-th
// button of A, C-Down, C-Right, C-Left, C-Up).
extern "C" int MM_ItemAdapter_TestAuthorOcarinaButtons(void* buf, size_t size, int mode, unsigned heldMask);
// #1140: the variant-0 MM save with tokens, stray fairies, keys, dungeon items,
// heart pieces, souls, owls, half-days and swim authored (see the seam), as a
// randomizer file (`rando` 1) or a vanilla one; `options` bits 0-5 switch the
// frozen RO_SHUFFLE_BOSS_SOULS, _ENEMY_SOULS, _OWL_STATUES, RO_CLOCK_SHUFFLE,
// RO_SHUFFLE_SWIM and RO_SHUFFLE_TRIFORCE_PIECES on, bit 6 the Clock Town fairy.
extern "C" int MM_ItemAdapter_TestAuthorTrackerWorld(void* buf, size_t size, int rando, unsigned options);
// #873, the combo-item-view-new-file row: a save Ship of Harkinian's own init
// path authors, copied into `buf`. Variant 0 is the player's new file
// (Save_InitFile(false)), 1 the title screen's attract save, 2 that debug save
// in a played session. Needs the full OoT bring-up. `outNewfMarked` receives
// whether the save carries "ZELDAZ".
extern "C" int OoT_ItemAdapter_TestAuthorNewFile(void* buf, size_t size, int variant, int* outNewfMarked);
// OoT's .redsave slot descriptor, as OoT's SaveManager constructor registers it.
extern "C" void OoT_SlotMeta_Register(void);
// #1142/#1158: the OoT tracker groups' seams (games/oot/soh/Enhancements/randomizer/ItemAdapterSingleExe.cpp).
// The tracker world: dungeon keys and items, souls, a jabber nut, two ocarina buttons, two overworld keys,
// the fishing pole and Greg, as a randomizer file (`rando` 1) or a vanilla one.
extern "C" int OoT_ItemAdapter_TestAuthorTrackerWorld(void* buf, size_t size, int rando);
// The frozen options the gated groups read (OoT's live in the seed's context, not the buffer); see kCivOpt*.
extern "C" void OoT_ItemAdapter_TestOverrideOptions(int enabled, unsigned bits);
// The adapter's guarded read of an item's inventory slot (SoH 7a7f942b8), -1 when it has none.
extern "C" int OoT_ItemAdapter_TestInvContent(const void* buf, int item);
extern "C" int OoT_ItemAdapter_TestItemSlotCount(void);

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

// MM's (#458 U1b): its own counter, so a leg can tell which game was asked.
static const void* sCivMMLive = NULL;
static int sCivMMLiveCalls = 0;

static const void* CivMMLiveSave(void) {
    sCivMMLiveCalls++;
    return sCivMMLive;
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
        CIV_ASSERT(row.name != NULL && (row.pad || row.name[0] != '\0'));
        // Every OoT row but a blank cell of SoH's dungeon table (#1142) has an icon.
        CIV_ASSERT(row.pad || (row.iconKey != NULL && row.iconKeyFaded != NULL));
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
    CIV_ASSERT(strcmp(Combo_ItemFreshnessLabel((uint8_t)GAME_OOT, COMBO_TRACKER_FRESH_LIVE), "") == 0); // #927

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

/** Legs 2-4 for MM (#458 U1b), over MM's REAL registered adapter `ops`, with
 *  OoT's real adapter `ootOps` alongside so one leg shows both games at once. */
static int CivMMLegs(const ComboItemOps* ops, const ComboItemOps* ootOps) {
    ComboItemRow row;

    // ---- 3. all-zero shadow: no data ------------------------------------
    std::vector<uint8_t> zeros((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    Context_UpdateShadowCopy(GAME_MM, zeros.data(), zeros.size());
    for (GameId g : { GAME_OOT, GAME_MM, GAME_NONE }) {
        Context_SetCurrentGame(g);
        CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_MM) == COMBO_TRACKER_FRESH_UNAVAILABLE);
        CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_MM) == 0);
        CIV_ASSERT(!Combo_ItemRowAt((uint8_t)GAME_MM, 0, &row));
    }

    // ---- 2. the authored shadow world, read while OoT is active -----------
    std::vector<uint8_t> shadow((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    CIV_ASSERT(MM_ItemAdapter_TestAuthorSave(shadow.data(), shadow.size(), 0) == 1);
    CIV_ASSERT(ops->hasSave(shadow.data()));
    CIV_ASSERT(!ops->hasSave(zeros.data()));
    Context_UpdateShadowCopy(GAME_MM, shadow.data(), shadow.size());
    const std::vector<uint8_t> shadowAuthored(shadow);

    Context_SetCurrentGame(GAME_OOT);
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_MM) == COMBO_TRACKER_FRESH_STALE);
    const int rows = Combo_ItemCount((uint8_t)GAME_MM);
    printf("[TEST] combo-item-view: MM rows=%d freshness=%u under GAME_OOT\n", rows,
           (unsigned)Combo_ItemFreshness((uint8_t)GAME_MM));
    CIV_ASSERT(rows == ops->count());
    for (int i = 0; i < rows; i++) {
        CIV_ASSERT(Combo_ItemRowAt((uint8_t)GAME_MM, i, &row));
        CIV_ASSERT(row.group != NULL && row.group[0] != '\0');
        if (row.name == NULL || row.name[0] == '\0' || strcmp(row.name, "Unknown Item") == 0) {
            printf("[TEST] combo-item-view: MM row %d (%s) has no name\n", i, row.group);
        }
        CIV_ASSERT(row.name != NULL && row.name[0] != '\0' && strcmp(row.name, "Unknown Item") != 0);
        CIV_ASSERT(row.iconKey != NULL && row.iconKeyFaded != NULL); // every MM row has an icon path
        CIV_ASSERT(row.freshness == COMBO_TRACKER_FRESH_STALE);
        CIV_ASSERT(row.count >= 0 && row.max >= 0);
    }
    CIV_ASSERT(!Combo_ItemRowAt((uint8_t)GAME_MM, rows, &row));
    CIV_ASSERT(!Combo_ItemRowAt((uint8_t)GAME_MM, -1, &row));

    // Row content: exactly the authored world (the seam's variant 0).
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Bow", &row));
    printf("[TEST] combo-item-view: MM shadow Bow have=%d count=%d max=%d group=%s\n", (int)row.have, row.count,
           row.max, row.group);
    CIV_ASSERT(row.have && row.count == 25 && row.max == 40 && strcmp(row.group, "Inventory") == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Magic Bean", &row));
    CIV_ASSERT(row.have && row.count == 7 && row.max == 20);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Hookshot", &row));
    CIV_ASSERT(!row.have && row.count == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Bottles", &row));
    CIV_ASSERT(row.have && row.count == 2 && row.max == 6);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Deku Mask", &row));
    CIV_ASSERT(row.have && strcmp(row.group, "Masks") == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Bunny Hood", &row) && row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Zora Mask", &row) && !row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Song of Healing", &row));
    CIV_ASSERT(row.have && strcmp(row.group, "Songs") == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Song of Soaring", &row) && !row.have);
    // The progressive lullaby names the tier held: the intro, not the full song.
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Goron Lullaby Intro", &row) && row.have);
    CIV_ASSERT(!CivFindRow((uint8_t)GAME_MM, "Goron Lullaby", &row));
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Odolwa's Remains", &row));
    CIV_ASSERT(row.have && strcmp(row.group, "Quest") == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Goht's Remains", &row) && !row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Razor Sword", &row) && row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Hero's Shield", &row) && row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Double Defense", &row) && !row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Adult's Wallet", &row));
    printf("[TEST] combo-item-view: MM shadow Adult's Wallet have=%d count=%d max=%d\n", (int)row.have, row.count,
           row.max);
    CIV_ASSERT(row.have && row.count == 150 && row.max == 200);

    // Read-only: the adapter wrote nothing into the shadow it was handed.
    const uint8_t* resident = (const uint8_t*)Context_GetMMSaveContext();
    CIV_ASSERT(resident != NULL && memcmp(resident, shadowAuthored.data(), shadowAuthored.size()) == 0);

    // The alignof contract: a misaligned image is refused, never read.
    CIV_ASSERT(!ops->rowAt(shadow.data() + 1, 0, &row));
    CIV_ASSERT(!ops->hasSave(shadow.data() + 1));

    // Under GAME_MM with the PRODUCTION live source: no MM play state in this
    // tier, so it answers NULL and the view shows the shadow, STALE.
    Context_SetCurrentGame(GAME_MM);
    CIV_ASSERT(ops->liveSave != NULL);
    CIV_ASSERT(ops->liveSave() == NULL);
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_MM) == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Razor Sword", &row) && row.freshness == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(strcmp(Combo_ItemFreshnessLabel((uint8_t)GAME_MM, COMBO_TRACKER_FRESH_STALE), "") == 0); // #927

    // ---- 4. the live-vs-shadow pick, both games registered -----------------
    std::vector<uint8_t> live((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    CIV_ASSERT(MM_ItemAdapter_TestAuthorSave(live.data(), live.size(), 1) == 1);
    static ComboItemOps sTestMMOps;
    sTestMMOps = *ops;
    sTestMMOps.liveSave = CivMMLiveSave;
    Combo_Item_RegisterOps((uint8_t)GAME_MM, &sTestMMOps);
    CIV_ASSERT(Combo_Item_GetOps((uint8_t)GAME_MM) == &sTestMMOps);
    sCivMMLive = live.data();
    // OoT beside it, with its own counted live source and an authored shadow,
    // so the leg can see which game the view asked.
    std::vector<uint8_t> ootShadow((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> ootLive((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    CIV_ASSERT(OoT_ItemAdapter_TestAuthorSave(ootShadow.data(), ootShadow.size(), 0) == 1);
    CIV_ASSERT(OoT_ItemAdapter_TestAuthorSave(ootLive.data(), ootLive.size(), 1) == 1);
    Context_UpdateShadowCopy(GAME_OOT, ootShadow.data(), ootShadow.size());
    static ComboItemOps sTestOoTOps;
    sTestOoTOps = *ootOps;
    sTestOoTOps.liveSave = CivLiveSave;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, &sTestOoTOps);
    sCivLive = ootLive.data();

    // A. MM active, its live save marked: MM LIVE with the live world's rows;
    // OoT, inactive, is read from its shadow and its live source never asked.
    Context_SetCurrentGame(GAME_MM);
    sCivMMLiveCalls = 0;
    sCivLiveCalls = 0;
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_MM) == COMBO_TRACKER_FRESH_LIVE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Hookshot", &row));
    printf("[TEST] combo-item-view: GAME_MM live Hookshot have=%d freshness=%u\n", (int)row.have,
           (unsigned)row.freshness);
    CIV_ASSERT(row.have && row.freshness == COMBO_TRACKER_FRESH_LIVE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Zora Mask", &row) && row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Kokiri Sword", &row) && row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Bow", &row) && !row.have);
    CIV_ASSERT(sCivMMLiveCalls > 0);
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Longshot", &row) && row.freshness == COMBO_TRACKER_FRESH_STALE);
    printf("[TEST] combo-item-view: under GAME_MM OoT liveSave calls=%d (must be 0)\n", sCivLiveCalls);
    CIV_ASSERT(sCivLiveCalls == 0);

    // B. OoT active: the mirror image. MM's live source is never asked and its
    // shadow's rows are STALE; OoT reads LIVE.
    Context_SetCurrentGame(GAME_OOT);
    sCivMMLiveCalls = 0;
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_MM) == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Razor Sword", &row) && row.have);
    CIV_ASSERT(row.freshness == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Bow", &row) && row.have && row.count == 25);
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_LIVE);
    printf("[TEST] combo-item-view: under GAME_OOT MM liveSave calls=%d (must be 0)\n", sCivMMLiveCalls);
    CIV_ASSERT(sCivMMLiveCalls == 0);

    // C. No game active: neither live source is asked; both STALE.
    Context_SetCurrentGame(GAME_NONE);
    sCivMMLiveCalls = 0;
    sCivLiveCalls = 0;
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_MM) == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Razor Sword", &row) && row.freshness == COMBO_TRACKER_FRESH_STALE);
    printf("[TEST] combo-item-view: under GAME_NONE MM liveSave calls=%d OoT liveSave calls=%d (must be 0)\n",
           sCivMMLiveCalls, sCivLiveCalls);
    CIV_ASSERT(sCivMMLiveCalls == 0 && sCivLiveCalls == 0);

    // D. MM active but its live save carries no marker: the shadow, STALE.
    Context_SetCurrentGame(GAME_MM);
    sCivMMLive = zeros.data();
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_MM) == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Razor Sword", &row) && row.freshness == COMBO_TRACKER_FRESH_STALE);

    // E. No live save and an empty shadow: UNAVAILABLE.
    sCivMMLive = NULL;
    Context_UpdateShadowCopy(GAME_MM, zeros.data(), zeros.size());
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_MM) == COMBO_TRACKER_FRESH_UNAVAILABLE);
    CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_MM) == 0);

    sCivLive = NULL;
    Combo_Item_RegisterOps((uint8_t)GAME_MM, ops);
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, ootOps);
    return TEST_PASS;
}

/**
 * #1072: MM's five shuffled ocarina buttons are rows of MM's section, shown only
 * when the FILE's frozen RO_SHUFFLE_OCARINA_BUTTONS is on in a randomizer file,
 * and held from the save's own button bits (the bits the VB_PLAY_OCARINA_NOTE
 * gate reads, Player.cpp). Without the rows a player whose C-up is still
 * missing sees a Sonata or a Song of Soaring that will not play and nothing
 * that says why. The option is never read from the live CVar: it arrives in
 * the buffer the view hands over.
 */
static int CivMMOcarinaButtonLegs(const ComboItemOps* ops) {
    static const char* const kButtons[5] = { "A Button", "C Down Button", "C Right Button", "C Left Button",
                                             "C Up Button" };
    ComboItemRow row;
    std::vector<uint8_t> buf((size_t)MM_SAVE_CONTEXT_SIZE, 0);

    Context_SetCurrentGame(GAME_OOT); // MM inactive: its shadow is what the view reads
    const int baseRows = ops->count();

    // Option on, every button but C-up held: five rows, C-up alone unowned.
    CIV_ASSERT(MM_ItemAdapter_TestAuthorOcarinaButtons(buf.data(), buf.size(), 1, 0x0Fu) == 1);
    CIV_ASSERT(ops->hasSave(buf.data()));
    Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
    for (int i = 0; i < 5; i++) {
        CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, kButtons[i], &row));
        printf("[TEST] combo-item-view: MM button row %s have=%d group=%s\n", kButtons[i], (int)row.have, row.group);
        CIV_ASSERT(strcmp(row.group, "Ocarina Buttons") == 0);
        CIV_ASSERT(row.have == (i != 4));
        CIV_ASSERT(row.iconKey != NULL && row.iconKeyFaded != NULL); // MM's own icon
    }
    CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_MM) == baseRows + 5);
    for (int i = 0; i < Combo_ItemCount((uint8_t)GAME_MM); i++) {
        CIV_ASSERT(Combo_ItemRowAt((uint8_t)GAME_MM, i, &row)); // contiguous: no hole the drawer stops at
    }
    CIV_ASSERT(!Combo_ItemRowAt((uint8_t)GAME_MM, baseRows + 5, &row));

    // The same file once C-up is found: all five held.
    CIV_ASSERT(MM_ItemAdapter_TestAuthorOcarinaButtons(buf.data(), buf.size(), 1, 0x1Fu) == 1);
    Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "C Up Button", &row) && row.have);

    // Nothing held yet: five unowned rows, still shown.
    CIV_ASSERT(MM_ItemAdapter_TestAuthorOcarinaButtons(buf.data(), buf.size(), 1, 0u) == 1);
    Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
    for (int i = 0; i < 5; i++) {
        CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, kButtons[i], &row) && !row.have);
    }

    // The frozen option off: no button row, however many bits are set.
    CIV_ASSERT(MM_ItemAdapter_TestAuthorOcarinaButtons(buf.data(), buf.size(), 0, 0x0Fu) == 1);
    Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
    for (int i = 0; i < 5; i++) {
        CIV_ASSERT(!CivFindRow((uint8_t)GAME_MM, kButtons[i], &row));
    }
    CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_MM) == baseRows);

    // The option byte alone is not the gate: a file that is not a randomizer
    // file never ran the note gate, so it shows no buttons.
    CIV_ASSERT(MM_ItemAdapter_TestAuthorOcarinaButtons(buf.data(), buf.size(), 2, 0x0Fu) == 1);
    Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
    for (int i = 0; i < 5; i++) {
        CIV_ASSERT(!CivFindRow((uint8_t)GAME_MM, kButtons[i], &row));
    }

    // #1067: under the frozen Shared Ocarina Buttons rule the five buttons are
    // ONE set for the pair: drawn once, in the Shared group (held from MM's own
    // bits or the pool), and left out of MM's section.
    {
        ComboContext_Init();
        Combo_ResetSharedResourceWatermarks();
        gComboCtx.sourceIsRando = true;
        gComboCtx.sharedRandoSeed = 0x1067F00Du;
        gComboCtx.sharedRandoSettingsHash = 0x1067CA5Eu;
        gComboCtx.mmProfileDigest = 0x1067D16Eu;
        ComboSettingsRecord rec;
        Combo_ComboSettingsDefaults(&rec);
        ComboItemRulesRecord rules;
        memset(&rules, 0, sizeof(rules));
        rules.crossItemFlags = (uint32_t)RSBS_XITEM_SHARED_OCARINA_BUTTONS;
        Combo_FreezeComboSettingsEx(&rec, &rules);
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 1); // a Shared group to draw in
        CIV_ASSERT(MM_ItemAdapter_TestAuthorOcarinaButtons(buf.data(), buf.size(), 1, 0x0Fu) == 1);
        Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
        CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_MM) == baseRows);
        for (int i = 0; i < 5; i++) {
            CIV_ASSERT(!CivFindRow((uint8_t)GAME_MM, kButtons[i], &row)); // not in MM's section
            CIV_ASSERT(CivFindSharedRow(kButtons[i], &row));
            printf("[TEST] combo-item-view: shared button row %s have=%d\n", kButtons[i], (int)row.have);
            CIV_ASSERT(strcmp(row.group, COMBO_ITEM_SHARED_GROUP) == 0);
            CIV_ASSERT(row.have == (i != 4));
        }
        for (int i = 0; i < Combo_ItemCount((uint8_t)GAME_MM); i++) {
            CIV_ASSERT(Combo_ItemRowAt((uint8_t)GAME_MM, i, &row)); // contiguous after the walk-past
        }
        // The pool lights a button MM's own save does not hold yet (found in Hyrule).
        Combo_SharedFamiliesHarvest(GAME_OOT, (uint32_t)RSBS_XFAMILY_SLOT_OCARINA_C_UP);
        CIV_ASSERT(CivFindSharedRow("C Up Button", &row) && row.have);
        ComboContext_Init();
        Combo_ResetSharedResourceWatermarks();
    }
    return TEST_PASS;
}

/** The view's MM rows of `group`, in order. */
static std::vector<ComboItemRow> CivMMGroup(const char* group) {
    std::vector<ComboItemRow> out;
    ComboItemRow row;
    const int n = Combo_ItemCount((uint8_t)GAME_MM);
    for (int i = 0; i < n; i++) {
        if (Combo_ItemRowAt((uint8_t)GAME_MM, i, &row) && strcmp(row.group, group) == 0) {
            out.push_back(row);
        }
    }
    return out;
}

/** Every MM row reads, labelled and with an icon, and the next index is refused. */
static bool CivMMRowsContiguous(void) {
    ComboItemRow row;
    const int n = Combo_ItemCount((uint8_t)GAME_MM);
    for (int i = 0; i < n; i++) {
        if (!Combo_ItemRowAt((uint8_t)GAME_MM, i, &row) || row.iconKey == NULL || row.iconKeyFaded == NULL ||
            strcmp(row.name, "Unknown Item") == 0) {
            printf("[TEST] combo-item-view: MM row %d does not read with a name and an icon\n", i);
            return false;
        }
    }
    return !Combo_ItemRowAt((uint8_t)GAME_MM, n, &row);
}

/**
 * #1140: MM's tracker rows the curated v1 list left out. Always on, after Quest
 * and in MM's default-preset order with MM's group names: Tokens (Swamp, Ocean
 * from skullTokenCount's two halves, against the file's frozen minimum or the
 * Spider House's 30), Stray Fairies (Clock Town from its week-event bit, four
 * temple counts against the frozen minimum or 15), Dungeon (column = temple:
 * boss keys, small keys counted found-in-a-randomizer-file / held-otherwise
 * against 1/3/1/4, maps, compasses), plus a Quest heart-piece row n/4. Shuffle
 * gated, each only when the buffer's own FROZEN option is on in a randomizer
 * file: Boss Souls (Majora's hidden under Triforce Hunt), ONE Enemy Souls row
 * n/47 counted through SOUL_RI_TO_RANDO_INF, Owl Statues, Time, Misc (swim).
 * Every read leaves the shadow byte-unchanged.
 */
static int CivMMTrackerRowLegs(const ComboItemOps* ops) {
    ComboItemRow row;
    std::vector<uint8_t> buf((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    Context_SetCurrentGame(GAME_OOT); // MM inactive: its shadow is what the view reads

    // 60 curated rows + Tokens 2 + Stray Fairies 5 + Dungeon 16 + the heart pieces.
    printf("[TEST] combo-item-view: MM count()=%d (want 84)\n", ops->count());
    CIV_ASSERT(ops->count() == 84);
    // MM's own table widths (ItemTrackerSettings.cpp LoadAvailableWindows).
    CIV_ASSERT(ops->grid != NULL && ops->grid->groupColumns != NULL);
    CIV_ASSERT(ops->grid->groupColumns("Tokens") == 2);
    CIV_ASSERT(ops->grid->groupColumns("Stray Fairies") == 5);
    CIV_ASSERT(ops->grid->groupColumns("Dungeon") == 4);
    CIV_ASSERT(ops->grid->groupColumns("Boss Souls") == 5);
    CIV_ASSERT(ops->grid->groupColumns("Enemy Souls") == 6);
    CIV_ASSERT(ops->grid->groupColumns("Owl Statues") == 5);
    CIV_ASSERT(ops->grid->groupColumns("Time") == 6);
    CIV_ASSERT(ops->grid->groupColumns("Misc") == 6);

    // ---- A. a randomizer file, every gate off, the Clock Town fairy found -----
    CIV_ASSERT(MM_ItemAdapter_TestAuthorTrackerWorld(buf.data(), buf.size(), 1, 0x40u) == 1);
    Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
    const std::vector<uint8_t> authored(buf);
    printf("[TEST] combo-item-view: MM rando rows=%d (want 84)\n", Combo_ItemCount((uint8_t)GAME_MM));
    CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_MM) == 84);
    CIV_ASSERT(CivMMRowsContiguous());

    std::vector<ComboItemRow> g = CivMMGroup("Tokens");
    CIV_ASSERT(g.size() == 2);
    printf("[TEST] combo-item-view: MM tokens %s %d/%d, %s %d/%d\n", g[0].name, g[0].count, g[0].max, g[1].name,
           g[1].count, g[1].max);
    CIV_ASSERT(strcmp(g[0].name, "Swamp Gold Skulltula Token") == 0 && g[0].have && g[0].count == 12 && g[0].max == 20);
    CIV_ASSERT(strcmp(g[1].name, "Ocean Gold Skulltula Token") == 0 && g[1].have && g[1].count == 5 && g[1].max == 20);

    g = CivMMGroup("Stray Fairies");
    CIV_ASSERT(g.size() == 5);
    CIV_ASSERT(strcmp(g[0].name, "Clock Town Stray Fairy") == 0 && g[0].have && g[0].max == 0);
    CIV_ASSERT(strstr(g[0].iconKey, "gStrayFairyStoneTowerIconTex") != NULL); // MM's own pick
    static const char* const kFairies[4] = { "Woodfall Stray Fairy", "Snowhead Stray Fairy", "Great Bay Stray Fairy",
                                             "Stone Tower Stray Fairy" };
    static const int kFairyCounts[4] = { 10, 3, 0, 15 };
    for (int i = 0; i < 4; i++) {
        printf("[TEST] combo-item-view: MM %s %d/%d have=%d\n", g[i + 1].name, g[i + 1].count, g[i + 1].max,
               (int)g[i + 1].have);
        CIV_ASSERT(strcmp(g[i + 1].name, kFairies[i]) == 0);
        CIV_ASSERT(g[i + 1].count == kFairyCounts[i] && g[i + 1].max == 10 && g[i + 1].have == (kFairyCounts[i] > 0));
    }
    CIV_ASSERT(strcmp(g[1].iconKey, g[2].iconKey) != 0); // a per-temple icon

    // Dungeon, 4 wide, column = temple: boss keys, small keys, maps, compasses.
    g = CivMMGroup("Dungeon");
    CIV_ASSERT(g.size() == 16);
    static const char* const kTemples[4] = { "Woodfall", "Snowhead", "Great Bay", "Stone Tower" };
    static const char* const kKinds[4] = { "Boss Key", "Small Key", "Map", "Compass" };
    for (int line = 0; line < 4; line++) {
        for (int col = 0; col < 4; col++) {
            char want[64];
            snprintf(want, sizeof(want), "%s %s", kTemples[col], kKinds[line]); // the tooltip's full name
            CIV_ASSERT(strcmp(g[line * 4 + col].name, want) == 0);
        }
    }
    static const bool kBossKey[4] = { true, false, false, true };
    static const bool kMap[4] = { true, false, false, true };
    static const bool kCompass[4] = { false, true, false, true };
    for (int col = 0; col < 4; col++) {
        CIV_ASSERT(g[col].have == kBossKey[col]);
        CIV_ASSERT(g[8 + col].have == kMap[col] && g[12 + col].have == kCompass[col]);
        // MM_gItemIcons swaps the map and compass textures; GetIconTexturePath cancels it.
        CIV_ASSERT(strstr(g[8 + col].iconKey, "gQuestIconDungeonMapTex") != NULL);
        CIV_ASSERT(strstr(g[12 + col].iconKey, "gQuestIconCompassTex") != NULL);
    }
    // Small keys in a randomizer file: found against the temple's keys, lit while one is held.
    static const int kFound[4] = { 1, 3, 0, 2 };
    static const int kKeyMax[4] = { 1, 3, 1, 4 };
    static const bool kHeld[4] = { false, true, false, true };
    for (int col = 0; col < 4; col++) {
        const ComboItemRow& k = g[4 + col];
        printf("[TEST] combo-item-view: MM rando %s %d/%d have=%d\n", k.name, k.count, k.max, (int)k.have);
        CIV_ASSERT(k.count == kFound[col] && k.max == kKeyMax[col] && k.have == kHeld[col]);
    }

    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Heart Pieces", &row));
    CIV_ASSERT(strcmp(row.group, "Quest") == 0 && row.have && row.count == 3 && row.max == 4);

    // Every gate off: no gated group, however much of it the save holds.
    for (const char* gated : { "Boss Souls", "Enemy Souls", "Owl Statues", "Time", "Misc" }) {
        CIV_ASSERT(CivMMGroup(gated).empty());
    }

    // ---- B. the Clock Town fairy lights from its week-event bit alone ----------
    CIV_ASSERT(MM_ItemAdapter_TestAuthorTrackerWorld(buf.data(), buf.size(), 1, 0u) == 1);
    Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Clock Town Stray Fairy", &row) && !row.have);

    // ---- C. every gate on ------------------------------------------------------
    CIV_ASSERT(MM_ItemAdapter_TestAuthorTrackerWorld(buf.data(), buf.size(), 1, 0x1Fu) == 1);
    Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
    const std::vector<uint8_t> authoredGated(buf);
    printf("[TEST] combo-item-view: MM rows with every gate on=%d (want 107)\n", Combo_ItemCount((uint8_t)GAME_MM));
    CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_MM) == 84 + 5 + 1 + 10 + 6 + 1);
    CIV_ASSERT(CivMMRowsContiguous());
    // The gated groups follow the always-on rows, in MM's order.
    {
        static const char* const kOrder[] = { "Boss Souls", "Enemy Souls", "Owl Statues", "Time", "Misc" };
        static const int kRows[] = { 5, 1, 10, 6, 1 };
        int at = 84;
        for (int gi = 0; gi < 5; gi++) {
            for (int j = 0; j < kRows[gi]; j++, at++) {
                CIV_ASSERT(Combo_ItemRowAt((uint8_t)GAME_MM, at, &row) && strcmp(row.group, kOrder[gi]) == 0);
            }
        }
    }
    g = CivMMGroup("Boss Souls");
    CIV_ASSERT(g.size() == 5);
    static const char* const kSouls[5] = { "Soul of Odolwa", "Soul of Goht", "Soul of Gyorg", "Soul of Twinmold",
                                           "Soul of Majora" };
    static const bool kSoulHeld[5] = { true, false, true, false, false };
    for (int i = 0; i < 5; i++) {
        CIV_ASSERT(strcmp(g[i].name, kSouls[i]) == 0 && g[i].have == kSoulHeld[i]);
    }
    CIV_ASSERT(strstr(g[4].iconKey, "gDungeonMapSkullTex") != NULL);

    g = CivMMGroup("Enemy Souls");
    CIV_ASSERT(g.size() == 1);
    printf("[TEST] combo-item-view: MM %s %d/%d\n", g[0].name, g[0].count, g[0].max);
    // RI_SOUL_ENEMY_WOLFOS through the macro counts; the named WOLFOS flag (set by no item) does not.
    CIV_ASSERT(g[0].have && g[0].count == 1 && g[0].max == 47);

    g = CivMMGroup("Owl Statues");
    CIV_ASSERT(g.size() == 10);
    static const char* const kOwls[10] = { "Clock Town Owl Statue",       "Milk Road Owl Statue",
                                           "Southern Swamp Owl Statue",   "Woodfall Owl Statue",
                                           "Mountain Village Owl Statue", "Snowhead Owl Statue",
                                           "Great Bay Coast Owl Statue",  "Zora Cape Owl Statue",
                                           "Ikana Canyon Owl Statue",     "Stone Tower Owl Statue" };
    for (int i = 0; i < 10; i++) {
        CIV_ASSERT(strcmp(g[i].name, kOwls[i]) == 0 && g[i].have == (i == 0 || i == 9));
        CIV_ASSERT(strstr(g[i].iconKey, "gWorldMapOwlFaceTex") != NULL && g[i].iconAspect > 1.0f);
    }

    g = CivMMGroup("Time");
    CIV_ASSERT(g.size() == 6);
    static const char* const kHalves[6] = { "Time (Day 1)",   "Time (Night 1)", "Time (Day 2)",
                                            "Time (Night 2)", "Time (Day 3)",   "Time (Night 3)" };
    for (int i = 0; i < 6; i++) {
        CIV_ASSERT(strcmp(g[i].name, kHalves[i]) == 0 && g[i].have == (i == 0 || i == 3));
    }
    CIV_ASSERT(strcmp(g[0].iconKey, g[1].iconKey) != 0); // the sun and the moon

    g = CivMMGroup("Misc");
    CIV_ASSERT(g.size() == 1 && strcmp(g[0].name, "Ability to Swim") == 0 && g[0].have);

    // ---- D. Triforce Hunt hides Majora's soul ------------------------------------
    CIV_ASSERT(MM_ItemAdapter_TestAuthorTrackerWorld(buf.data(), buf.size(), 1, 0x21u) == 1);
    Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
    CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_MM) == 84 + 4);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Soul of Twinmold", &row));
    CIV_ASSERT(!CivFindRow((uint8_t)GAME_MM, "Soul of Majora", &row));
    CIV_ASSERT(CivMMRowsContiguous());

    // ---- E. one gate at a time: exactly its group --------------------------------
    {
        static const char* const kGroup[5] = { "Boss Souls", "Enemy Souls", "Owl Statues", "Time", "Misc" };
        static const int kRows[5] = { 5, 1, 10, 6, 1 };
        for (int bit = 0; bit < 5; bit++) {
            CIV_ASSERT(MM_ItemAdapter_TestAuthorTrackerWorld(buf.data(), buf.size(), 1, 1u << bit) == 1);
            Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
            CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_MM) == 84 + kRows[bit]);
            for (int other = 0; other < 5; other++) {
                CIV_ASSERT(CivMMGroup(kGroup[other]).size() == (size_t)(other == bit ? kRows[bit] : 0));
            }
        }
    }

    // ---- F. a vanilla file: the vanilla ceilings and held keys, no gated group ----
    CIV_ASSERT(MM_ItemAdapter_TestAuthorTrackerWorld(buf.data(), buf.size(), 0, 0x5Fu) == 1);
    Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
    CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_MM) == 84);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Swamp Gold Skulltula Token", &row));
    CIV_ASSERT(row.count == 12 && row.max == 30); // SPIDER_HOUSE_TOKENS_REQUIRED
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Stone Tower Stray Fairy", &row));
    CIV_ASSERT(row.count == 15 && row.max == 15); // STRAY_FAIRY_SCATTERED_TOTAL
    g = CivMMGroup("Dungeon");
    CIV_ASSERT(g.size() == 16);
    static const int kHeldCount[4] = { 0, 1, 0, 2 }; // -1 (never entered) reads 0
    for (int col = 0; col < 4; col++) {
        const ComboItemRow& k = g[4 + col];
        printf("[TEST] combo-item-view: MM vanilla %s %d/%d have=%d\n", k.name, k.count, k.max, (int)k.have);
        CIV_ASSERT(k.count == kHeldCount[col] && k.max == kKeyMax[col] && k.have == (kHeldCount[col] > 0));
    }
    for (const char* gated : { "Boss Souls", "Enemy Souls", "Owl Statues", "Time", "Misc" }) {
        CIV_ASSERT(CivMMGroup(gated).empty());
    }

    // Read-only: the last world read is still the bytes authored.
    const std::vector<uint8_t> authoredVanilla(buf);
    const uint8_t* resident = (const uint8_t*)Context_GetMMSaveContext();
    CIV_ASSERT(resident != NULL && memcmp(resident, authoredVanilla.data(), authoredVanilla.size()) == 0);
    Context_UpdateShadowCopy(GAME_MM, authoredGated.data(), authoredGated.size());
    CIV_ASSERT(CivMMRowsContiguous());
    resident = (const uint8_t*)Context_GetMMSaveContext();
    CIV_ASSERT(resident != NULL && memcmp(resident, authoredGated.data(), authoredGated.size()) == 0);
    Context_UpdateShadowCopy(GAME_MM, authored.data(), authored.size());
    CIV_ASSERT(CivMMRowsContiguous());
    resident = (const uint8_t*)Context_GetMMSaveContext();
    CIV_ASSERT(resident != NULL && memcmp(resident, authored.data(), authored.size()) == 0);
    return TEST_PASS;
}

/**
 * #1141 (lane IT-3): each of MM's groups the player can hide has its own
 * Preference key, gCombo.Tracker.Items.MM.<the group without spaces>, which
 * hides that group and no other, and shows it by default. The group strings
 * are the adapter's (every one is a group the gated world draws); MM's
 * Inventory, Masks, Songs, Quest and Ocarina Buttons, and every OoT group, have
 * no such key and always show here (OoT's sections follow SoH's own
 * DisplayType keys, which the overlay reads).
 */
static int CivMMGroupVisibilityLegs(const ComboItemOps* ops) {
    (void)ops;
    static const struct {
        const char* group;
        const char* key;
    } kGroups[] = {
        { "Tokens", "gCombo.Tracker.Items.MM.Tokens" },
        { "Stray Fairies", "gCombo.Tracker.Items.MM.StrayFairies" },
        { "Dungeon", "gCombo.Tracker.Items.MM.Dungeon" },
        { "Boss Souls", "gCombo.Tracker.Items.MM.BossSouls" },
        { "Enemy Souls", "gCombo.Tracker.Items.MM.EnemySouls" },
        { "Owl Statues", "gCombo.Tracker.Items.MM.OwlStatues" },
        { "Time", "gCombo.Tracker.Items.MM.Time" },
        { "Misc", "gCombo.Tracker.Items.MM.Misc" },
    };
    std::vector<uint8_t> buf((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    Context_SetCurrentGame(GAME_OOT);
    CIV_ASSERT(MM_ItemAdapter_TestAuthorTrackerWorld(buf.data(), buf.size(), 1, 0x1Fu) == 1);
    Context_UpdateShadowCopy(GAME_MM, buf.data(), buf.size());
    for (const auto& g : kGroups) {
        CVarClear(g.key);
    }
    for (const auto& g : kGroups) {
        // The contract: a group the adapter draws, under exactly this key.
        CIV_ASSERT(!CivMMGroup(g.group).empty());
        const char* key = Combo_ItemGroupVisibilityKey((uint8_t)GAME_MM, g.group);
        printf("[TEST] combo-item-view #1141: MM \"%s\" -> %s\n", g.group, key != NULL ? key : "(none)");
        CIV_ASSERT(key != NULL && strcmp(key, g.key) == 0);
        CIV_ASSERT(Combo_ItemGroupShown((uint8_t)GAME_MM, g.group)); // shown by default
        CIV_ASSERT(Combo_ItemGroupVisibilityKey((uint8_t)GAME_OOT, g.group) == NULL);
        CIV_ASSERT(Combo_ItemGroupShown((uint8_t)GAME_OOT, g.group));
    }
    for (const auto& g : kGroups) {
        CVarSetInteger(g.key, 0);
        for (const auto& other : kGroups) {
            CIV_ASSERT(Combo_ItemGroupShown((uint8_t)GAME_MM, other.group) == (other.key != g.key));
        }
        CIV_ASSERT(Combo_ItemGroupShown((uint8_t)GAME_OOT, g.group)); // an MM key never hides an OoT group
        CVarSetInteger(g.key, 1);
        CIV_ASSERT(Combo_ItemGroupShown((uint8_t)GAME_MM, g.group));
        CVarClear(g.key);
    }
    for (const char* always : { "Inventory", "Masks", "Songs", "Quest", "Ocarina Buttons" }) {
        CIV_ASSERT(Combo_ItemGroupVisibilityKey((uint8_t)GAME_MM, always) == NULL);
        CIV_ASSERT(Combo_ItemGroupShown((uint8_t)GAME_MM, always));
    }
    CIV_ASSERT(Combo_ItemGroupVisibilityKey((uint8_t)GAME_MM, NULL) == NULL);
    CIV_ASSERT(Combo_ItemGroupShown((uint8_t)GAME_MM, NULL));
    CIV_ASSERT(Combo_ItemGroupVisibilityKey((uint8_t)GAME_NONE, "Tokens") == NULL);
    printf("[TEST] combo-item-view #1141: each MM group hides under its own key and shows by default\n");
    return TEST_PASS;
}

/** Leg 5: the Shared group, over a freshly initialized context. */
static int CivSharedLegs(void) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    ComboItemRow row;

    CIV_ASSERT(Combo_ItemSharedFreshness() == COMBO_TRACKER_FRESH_UNAVAILABLE);
    CIV_ASSERT(strcmp(Combo_ItemSharedLabel(), "") == 0); // #927: no caption
    CIV_ASSERT(Combo_ItemSharedCount() == 0);
    CIV_ASSERT(!Combo_ItemSharedRowAt(0, &row));

    // Production harvests, as a departing OoT would: 250 rupees, quiver tier 2
    // holding 33 arrows, 5 hearts and 2 pieces (0x10 per heart, 4 per piece).
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 250);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_QUIVER_TIER, 2);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_ARROW_COUNT, 33);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(0x50, 2));

    CIV_ASSERT(Combo_ItemSharedFreshness() == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(strcmp(Combo_ItemSharedLabel(), "") == 0); // #927: no caption, whatever the pool holds
    const int n = Combo_ItemSharedCount();
    printf("[TEST] combo-item-view: shared rows=%d\n", n);
    CIV_ASSERT(n > 0);
    for (int i = 0; i < n; i++) {
        CIV_ASSERT(Combo_ItemSharedRowAt(i, &row));
        CIV_ASSERT(strcmp(row.group, COMBO_ITEM_SHARED_GROUP) == 0);
        CIV_ASSERT(row.name != NULL && row.name[0] != '\0');
        // The icon game's own icon and its faded twin come together, or neither
        // (#458 U3; the keys themselves are held by combo-item-tracker-window).
        CIV_ASSERT((row.iconKey == NULL) == (row.iconKeyFaded == NULL));
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
    // #879: the Shared Ocarina rule is retired, so the overlay names no shared
    // ocarina in any world (an older world with the bit set: the end of this leg).
    CIV_ASSERT(!CivFindSharedRow("Ocarina", &row));
    CIV_ASSERT(CivFindSharedRow("Triforce Pieces", &row) ==
               Combo_SharedResourceKindArmed(RSBS_SHARED_RES_TRIFORCE_PIECES));

    // Labels: no caption for the pool or either game at any freshness (#927).
    for (uint8_t f : { (uint8_t)COMBO_TRACKER_FRESH_LIVE, (uint8_t)COMBO_TRACKER_FRESH_STALE,
                       (uint8_t)COMBO_TRACKER_FRESH_UNAVAILABLE }) {
        CIV_ASSERT(strcmp(Combo_ItemFreshnessLabel((uint8_t)GAME_OOT, f), "") == 0);
    }

    // #879: an OLDER world, whose frozen record carries the retired
    // RSBS_COMBO_FLAG_SHARED_OCARINA bit, with both games' ocarinas harvested,
    // still shows no shared "Ocarina" row. Before the retirement this exact
    // world armed the kind and the overlay drew an "Ocarina 2/2" row. (The
    // context is restored by Combo_ItemView_RunHeadless.)
    {
        ComboContext_Init();
        Combo_ResetSharedResourceWatermarks();
        ComboSettingsRecord rec;
        Combo_ComboSettingsDefaults(&rec);
        rec.comboFlags = (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA;
        Combo_FreezeComboSettings(&rec);
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 99);
        Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_OCARINA_TIER, 1); // MM's starting Ocarina of Time
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, 2);
        const bool ocarinaShown = CivFindSharedRow("Ocarina", &row);
        printf("[TEST] combo-item-view: older world with the Shared Ocarina bit set, both ocarinas harvested: "
               "shared Ocarina row shown=%d (expected 0, #879)\n",
               ocarinaShown ? 1 : 0);
        CIV_ASSERT(CivFindSharedRow("Rupees", &row) && row.count == 99); // the scan itself reads this world
        CIV_ASSERT(!ocarinaShown);
    }
    return TEST_PASS;
}

/** Every row of `game` as the view hands it over, in order. */
static void CivReadAll(uint8_t game, std::vector<ComboItemRow>& out) {
    out.clear();
    const int n = Combo_ItemCount(game);
    for (int i = 0; i < n; i++) {
        ComboItemRow row;
        if (Combo_ItemRowAt(game, i, &row)) {
            out.push_back(row);
        }
    }
}

/** Is `name` one of the NULL-terminated `names`? */
static bool CivNameIn(const char* name, const char* const* names) {
    for (; *names != NULL; names++) {
        if (strcmp(name, *names) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * The rows `pooled` draws dimmed that `base` drew held (#967): the per-game
 * copies of an item the Shared column now holds. Every such row is empty
 * (nothing held, no count, no ceiling), and no other row may differ from `base`
 * in `have`. Returns the number of dimmed rows, or -1 on a violation; the names
 * of the dimmed rows go to `names` (at most `cap`).
 */
static int CivDimmedRows(const std::vector<ComboItemRow>& base, const std::vector<ComboItemRow>& pooled,
                         const char** names, int cap) {
    if (base.size() != pooled.size()) {
        return -1;
    }
    int dimmed = 0;
    for (size_t i = 0; i < base.size(); i++) {
        if (strcmp(base[i].name, pooled[i].name) != 0) {
            return -1;
        }
        if (base[i].have && !pooled[i].have) {
            if (pooled[i].count != 0 || pooled[i].max != 0) {
                return -1; // a dimmed copy prints no second count
            }
            if (dimmed < cap) {
                names[dimmed] = pooled[i].name;
            }
            dimmed++;
        } else if (base[i].have != pooled[i].have) {
            return -1;
        }
    }
    return dimmed;
}

/**
 * Leg 5b (#967): a shared capacity tier shows once, in the Shared column, and
 * the per-game copy of it is dimmed.
 *
 * The "capacity tier" world (authoring variant 2, both games): a Bow in a tier-1
 * quiver holding 12 arrows, bombs in a tier-2 bag holding 7, 5 Deku Sticks in a
 * tier-1 bag, 9 Deku Nuts in a tier-3 bag, and the Kokiri Sword. Without the pool
 * holding those tiers, both games draw all five lit. With it (the pool at the
 * same tiers), each game's five copies are dimmed and nothing else changes; a
 * copy AHEAD of the pool stays lit (it is the only place the find shows); the
 * Shared Arrows row draws the Bow at tier 1 and the quiver beyond; and a LIVE
 * game's own tiers and counts reach the Shared row without waiting for the next
 * harvest (the pool is written at a game switch or a save only).
 */
static int CivSharedTierLegs(const ComboItemOps* ootOps, const ComboItemOps* mmOps) {
    ComboItemRow row;
    ComboItemRow sharedRow;
    std::vector<uint8_t> ootT((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> mmT((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    CIV_ASSERT(OoT_ItemAdapter_TestAuthorSave(ootT.data(), ootT.size(), 2) == 1);
    CIV_ASSERT(MM_ItemAdapter_TestAuthorSave(mmT.data(), mmT.size(), 2) == 1);
    Context_UpdateShadowCopy(GAME_OOT, ootT.data(), ootT.size());
    Context_UpdateShadowCopy(GAME_MM, mmT.data(), mmT.size());
    Context_SetCurrentGame(GAME_NONE); // no live source: both games read their shadows

    // ---- baseline: the pool holds rupees only ----------------------------------
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    rec.comboFlags = (uint8_t)RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD;
    Combo_FreezeComboSettings(&rec);
    CIV_ASSERT(Combo_SharedResourceKindArmed(RSBS_SHARED_RES_KOKIRI_SWORD));
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 99);
    std::vector<ComboItemRow> ootBase, mmBase, ootPooled, mmPooled;
    CivReadAll((uint8_t)GAME_OOT, ootBase);
    CivReadAll((uint8_t)GAME_MM, mmBase);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Fairy Bow", &row) && row.have && row.count == 12 && row.max == 30);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Bombs", &row) && row.have && row.count == 7 && row.max == 30);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Deku Sticks", &row) && row.have && row.count == 5 && row.max == 10);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Deku Nuts", &row) && row.have && row.count == 9 && row.max == 40);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Kokiri Sword", &row) && row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Bow", &row) && row.have && row.count == 12 && row.max == 30);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Kokiri Sword", &row) && row.have);
    CIV_ASSERT(CivFindSharedRow("Arrows", &sharedRow) && !sharedRow.have);       // the pool holds no quiver yet
    CIV_ASSERT(CivFindSharedRow("Kokiri Sword", &sharedRow) && !sharedRow.have); // the rule is on: the row is there

    // ---- the pool at the same tiers: five copies per game dimmed -----------------
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_QUIVER_TIER, 1);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_BOMB_BAG_TIER, 2);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_STICK_TIER, 1);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_NUT_TIER, 3);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_KOKIRI_SWORD, 1);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_ARROW_COUNT, 12);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_BOMB_COUNT, 7);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_STICK_COUNT, 5);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_NUT_COUNT, 9);
    CivReadAll((uint8_t)GAME_OOT, ootPooled);
    CivReadAll((uint8_t)GAME_MM, mmPooled);
    const char* dimmedNames[16] = { NULL };
    static const char* const kOoTShared[] = { "Fairy Bow", "Bombs", "Deku Sticks", "Deku Nuts", "Kokiri Sword", NULL };
    int dimmed = CivDimmedRows(ootBase, ootPooled, dimmedNames, 16);
    printf("[TEST] combo-item-view: OoT copies dimmed with the pool at the world's tiers: %d (expected 5)\n", dimmed);
    CIV_ASSERT(dimmed == 5);
    for (int i = 0; i < 5; i++) {
        CIV_ASSERT(CivNameIn(dimmedNames[i], kOoTShared));
    }
    dimmed = CivDimmedRows(mmBase, mmPooled, dimmedNames, 16);
    printf("[TEST] combo-item-view: MM copies dimmed with the pool at the world's tiers: %d (expected 5)\n", dimmed);
    CIV_ASSERT(dimmed == 5);
    static const char* const kMMShared[] = { "Bow", "Kokiri Sword", NULL };
    int mmNamed = 0;
    for (int i = 0; i < 5; i++) {
        mmNamed += CivNameIn(dimmedNames[i], kMMShared) ? 1 : 0;
    }
    CIV_ASSERT(mmNamed == 2); // the other three are MM's bombs, sticks and nuts, named by MM's own table

    // The Shared column holds each once: the bow icon at quiver tier 1 (each
    // game's own art), the bag tiers beyond, counts under them.
    int sharedChecked = 0;
    for (int i = 0; i < Combo_ItemSharedCount(); i++) {
        CIV_ASSERT(Combo_ItemSharedRowAtWithIcons(i, (uint8_t)GAME_OOT, &sharedRow));
        if (strcmp(sharedRow.name, "Arrows") == 0) {
            CIV_ASSERT(sharedRow.have && sharedRow.count == 12 && sharedRow.max == 30);
            CIV_ASSERT(sharedRow.iconKey != NULL &&
                       strcmp(sharedRow.iconKey, "ITEM_BOW") == 0); // the Bow, not a quiver
            sharedChecked++;
        } else if (strcmp(sharedRow.name, "Bombs") == 0) {
            CIV_ASSERT(sharedRow.have && sharedRow.count == 7 && sharedRow.max == 30);
            CIV_ASSERT(sharedRow.iconKey != NULL && strcmp(sharedRow.iconKey, "ITEM_BOMB_BAG_30") == 0);
            sharedChecked++;
        } else if (strcmp(sharedRow.name, "Deku Sticks") == 0) {
            CIV_ASSERT(sharedRow.have && sharedRow.count == 5 && sharedRow.max == 10);
            sharedChecked++;
        } else if (strcmp(sharedRow.name, "Deku Nuts") == 0) {
            CIV_ASSERT(sharedRow.have && sharedRow.count == 9 && sharedRow.max == 40);
            sharedChecked++;
        } else if (strcmp(sharedRow.name, "Kokiri Sword") == 0) {
            CIV_ASSERT(sharedRow.have && sharedRow.iconKey != NULL &&
                       strcmp(sharedRow.iconKey, "ITEM_SWORD_KOKIRI") == 0);
            sharedChecked++;
        }
    }
    CIV_ASSERT(sharedChecked == 5);
    // MM's own art: the Shared bow icon is the one MM's own Bow row draws.
    const ComboItemRow* mmBowBase = NULL;
    for (const ComboItemRow& b : mmBase) {
        if (strcmp(b.name, "Bow") == 0) {
            mmBowBase = &b;
        }
    }
    CIV_ASSERT(mmBowBase != NULL && mmBowBase->iconKey != NULL);
    for (int i = 0; i < Combo_ItemSharedCount(); i++) {
        CIV_ASSERT(Combo_ItemSharedRowAtWithIcons(i, (uint8_t)GAME_MM, &sharedRow));
        if (strcmp(sharedRow.name, "Arrows") == 0) {
            CIV_ASSERT(sharedRow.iconKey != NULL && strcmp(sharedRow.iconKey, mmBowBase->iconKey) == 0);
        }
    }

    // ---- a quiver at tier 2 is one Bow row, and both tier-1 copies are dimmed -----
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_QUIVER_TIER, 2);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_ARROW_COUNT, 33);
    CIV_ASSERT(CivFindSharedRow("Arrows", &sharedRow));
    CIV_ASSERT(sharedRow.have && sharedRow.count == 33 && sharedRow.max == 40);
    CIV_ASSERT(sharedRow.iconKey != NULL && strcmp(sharedRow.iconKey, "ITEM_QUIVER_40") == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Fairy Bow", &row) && !row.have && row.count == 0 && row.max == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Bow", &row) && !row.have && row.count == 0 && row.max == 0);

    // ---- a copy ahead of the pool stays lit --------------------------------------
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 99);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_NUT_TIER, 2); // behind the world's tier-3 nut bag
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Fairy Bow", &row) && row.have && row.count == 12 && row.max == 30);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Bow", &row) && row.have && row.count == 12);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Deku Nuts", &row) && row.have && row.count == 9 && row.max == 40);

    // ---- the rule off: no Shared Kokiri Sword row, the sword stays lit -------------
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 99);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_KOKIRI_SWORD, 1);
    CIV_ASSERT(!CivFindSharedRow("Kokiri Sword", &sharedRow));
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Kokiri Sword", &row) && row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_MM, "Kokiri Sword", &row) && row.have);

    // ---- a live game's tiers and counts reach the Shared column at once -----------
    static ComboItemOps sTierOoTOps;
    static ComboItemOps sTierMMOps;
    sTierOoTOps = *ootOps;
    sTierOoTOps.liveSave = CivLiveSave;
    sTierMMOps = *mmOps;
    sTierMMOps.liveSave = CivMMLiveSave;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, &sTierOoTOps);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, &sTierMMOps);
    for (GameId active : { GAME_OOT, GAME_MM }) {
        ComboContext_Init();
        Combo_ResetSharedResourceWatermarks();
        Combo_FreezeComboSettings(&rec);
        Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, 99); // the pool holds nothing of the tiers
        sCivLive = (active == GAME_OOT) ? ootT.data() : NULL;
        sCivMMLive = (active == GAME_MM) ? mmT.data() : NULL;
        Context_SetCurrentGame(active);
        const uint8_t activeGame = (uint8_t)active;
        CIV_ASSERT(Combo_ItemFreshness(activeGame) == COMBO_TRACKER_FRESH_LIVE);
        CIV_ASSERT(CivFindSharedRow("Arrows", &sharedRow));
        printf("[TEST] combo-item-view: live %s, empty pool: shared Arrows have=%d count=%d max=%d\n",
               active == GAME_OOT ? "OoT" : "MM", (int)sharedRow.have, sharedRow.count, sharedRow.max);
        CIV_ASSERT(sharedRow.have && sharedRow.count == 12 && sharedRow.max == 30);
        CIV_ASSERT(CivFindSharedRow("Bombs", &sharedRow) && sharedRow.have && sharedRow.count == 7 &&
                   sharedRow.max == 30);
        CIV_ASSERT(CivFindSharedRow("Deku Sticks", &sharedRow) && sharedRow.have && sharedRow.count == 5 &&
                   sharedRow.max == 10);
        CIV_ASSERT(CivFindSharedRow("Deku Nuts", &sharedRow) && sharedRow.have && sharedRow.count == 9 &&
                   sharedRow.max == 40);
        if (active == GAME_OOT) {
            CIV_ASSERT(CivFindSharedRow("Kokiri Sword", &sharedRow) && sharedRow.have);
            CIV_ASSERT(CivFindRow(activeGame, "Fairy Bow", &row) && !row.have && row.count == 0);
        } else {
            CIV_ASSERT(CivFindRow(activeGame, "Bow", &row) && !row.have && row.count == 0);
        }
        // The inactive game, read from its shadow, is dimmed against the live tiers too.
        const uint8_t other = (active == GAME_OOT) ? (uint8_t)GAME_MM : (uint8_t)GAME_OOT;
        CIV_ASSERT(CivFindRow(other, other == (uint8_t)GAME_MM ? "Bow" : "Fairy Bow", &row) && !row.have);
    }
    sCivLive = NULL;
    sCivMMLive = NULL;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, ootOps);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, mmOps);
    printf("[TEST] combo-item-view: shared capacity tiers shown once, per-game copies dimmed PASS (#967)\n");
    return TEST_PASS;
}

// ---- the Shared column follows the active game's live save (#971) -------------

/** The pool's value for `kind`, 0 when nothing was ever written. Read-only. */
static uint16_t CivPoolValue(uint8_t kind) {
    uint16_t v = 0;
    return Combo_GetSharedResource(kind, &v) ? v : 0;
}

/** A pool holding `rupees` and nothing else, the way a departing game's harvest leaves it. */
static void CivFreshPool(uint16_t rupees) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, rupees);
}

/**
 * The Shared column over the ACTIVE game's live save, with no harvest: a hookshot or
 * a wallet found since the last save or crossing shows at once, the monotonic tiers
 * never fall below the pool, a consumable count is the live game's own, and the
 * pool is never written by a draw. The inactive game keeps its snapshot.
 */
static int CivSharedLiveLegs(const ComboItemOps* ootOps, const ComboItemOps* mmOps) {
    ComboItemRow row;
    ComboItemRow sharedRow;
    std::vector<uint8_t> ootV0((size_t)OOT_SAVE_CONTEXT_SIZE, 0); // Longshot, Adult's Wallet, 123 rupees, tier-2 quiver
    std::vector<uint8_t> ootV2((size_t)OOT_SAVE_CONTEXT_SIZE, 0); // the shadow: no hookshot
    std::vector<uint8_t> mmV0((size_t)MM_SAVE_CONTEXT_SIZE, 0);   // Adult's Wallet, 150 rupees, magic
    std::vector<uint8_t> mmV1((size_t)MM_SAVE_CONTEXT_SIZE, 0);   // the Hookshot
    std::vector<uint8_t> mmV2((size_t)MM_SAVE_CONTEXT_SIZE, 0);   // the shadow: no hookshot
    CIV_ASSERT(OoT_ItemAdapter_TestAuthorSave(ootV0.data(), ootV0.size(), 0) == 1);
    CIV_ASSERT(OoT_ItemAdapter_TestAuthorSave(ootV2.data(), ootV2.size(), 2) == 1);
    CIV_ASSERT(MM_ItemAdapter_TestAuthorSave(mmV0.data(), mmV0.size(), 0) == 1);
    CIV_ASSERT(MM_ItemAdapter_TestAuthorSave(mmV1.data(), mmV1.size(), 1) == 1);
    CIV_ASSERT(MM_ItemAdapter_TestAuthorSave(mmV2.data(), mmV2.size(), 2) == 1);
    Context_UpdateShadowCopy(GAME_OOT, ootV2.data(), ootV2.size());
    Context_UpdateShadowCopy(GAME_MM, mmV2.data(), mmV2.size());

    static ComboItemOps sLiveOoTOps;
    static ComboItemOps sLiveMMOps;
    sLiveOoTOps = *ootOps;
    sLiveOoTOps.liveSave = CivLiveSave;
    sLiveMMOps = *mmOps;
    sLiveMMOps.liveSave = CivMMLiveSave;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, &sLiveOoTOps);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, &sLiveMMOps);

    // ---- OoT played live, the pool holding 99 rupees and nothing else ---------------
    CivFreshPool(99);
    sCivLive = ootV0.data();
    sCivMMLive = NULL;
    Context_SetCurrentGame(GAME_OOT);
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_LIVE);
    CIV_ASSERT(CivPoolValue(RSBS_SHARED_RES_HOOKSHOT_TIER) == 0);
    CIV_ASSERT(CivFindSharedRow("Hookshot", &sharedRow));
    printf("[TEST] combo-item-view: OoT live Longshot, pool 0: shared Hookshot have=%d count=%d max=%d\n",
           (int)sharedRow.have, sharedRow.count, sharedRow.max);
    CIV_ASSERT(sharedRow.have && sharedRow.count == 2 && sharedRow.max == 2);
    // The draw wrote nothing: no harvest ran, so the pool still holds only the rupees.
    CIV_ASSERT(CivPoolValue(RSBS_SHARED_RES_HOOKSHOT_TIER) == 0);
    CIV_ASSERT(CivPoolValue(RSBS_SHARED_RES_WALLET_TIER) == 0);
    CIV_ASSERT(CivPoolValue(RSBS_SHARED_RES_HEALTH_QUARTERS) == 0);
    CIV_ASSERT(CivPoolValue(RSBS_SHARED_RES_RUPEES) == 99);

    // A wallet and the hearts the live save holds (three whole hearts).
    CIV_ASSERT(CivFindSharedRow("Wallet", &sharedRow));
    CIV_ASSERT(sharedRow.have && sharedRow.count == 1 && sharedRow.max == 3);
    CIV_ASSERT(CivFindSharedRow("Hearts", &sharedRow));
    printf("[TEST] combo-item-view: OoT live, pool 0: shared Hearts count=%d\n", sharedRow.count);
    CIV_ASSERT(sharedRow.have && sharedRow.count == 3);

    // The rupees are the live game's, below or above the pool's.
    CIV_ASSERT(CivFindSharedRow("Rupees", &sharedRow));
    printf("[TEST] combo-item-view: OoT live 123 rupees, pool 99: shared Rupees count=%d\n", sharedRow.count);
    CIV_ASSERT(sharedRow.have && sharedRow.count == 123);
    CivFreshPool(250);
    CIV_ASSERT(CivFindSharedRow("Rupees", &sharedRow));
    printf("[TEST] combo-item-view: OoT live 123 rupees, pool 250: shared Rupees count=%d\n", sharedRow.count);
    CIV_ASSERT(sharedRow.have && sharedRow.count == 123);
    CIV_ASSERT(CivPoolValue(RSBS_SHARED_RES_RUPEES) == 250);

    // A monotonic tier never falls below the pool: 16 hearts pooled, 3 live.
    CivFreshPool(99);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(0x100, 0));
    CIV_ASSERT(CivFindSharedRow("Hearts", &sharedRow) && sharedRow.have && sharedRow.count == 16);

    // The ammo: the live quiver (tier 2, 35 arrows) is ahead of a tier-1 pool, so
    // its count shows; a live quiver behind the pool leaves the pool's.
    CivFreshPool(99);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_QUIVER_TIER, 1);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_ARROW_COUNT, 10);
    CIV_ASSERT(CivFindSharedRow("Arrows", &sharedRow));
    CIV_ASSERT(sharedRow.have && sharedRow.count == 35 && sharedRow.max == 40);
    CivFreshPool(99);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_QUIVER_TIER, 3);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_ARROW_COUNT, 50);
    CIV_ASSERT(CivFindSharedRow("Arrows", &sharedRow));
    CIV_ASSERT(sharedRow.have && sharedRow.count == 50 && sharedRow.max == 50);

    // The inactive game keeps its snapshot: MM's own Hookshot (its shadow holds none) stays dark.
    CIV_ASSERT(!(CivFindRow((uint8_t)GAME_MM, "Hookshot", &row) && row.have));
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_MM) == COMBO_TRACKER_FRESH_STALE);

    // No live source (a title screen, file select): the column is the pool alone.
    CivFreshPool(250);
    sCivLive = NULL;
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(CivFindSharedRow("Hookshot", &sharedRow) && !sharedRow.have && sharedRow.count == 0);
    CIV_ASSERT(CivFindSharedRow("Rupees", &sharedRow) && sharedRow.count == 250);

    // ---- MM played live ---------------------------------------------------------------
    CivFreshPool(99);
    sCivLive = NULL;
    sCivMMLive = mmV1.data();
    Context_SetCurrentGame(GAME_MM);
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_MM) == COMBO_TRACKER_FRESH_LIVE);
    CIV_ASSERT(CivFindSharedRow("Hookshot", &sharedRow));
    printf("[TEST] combo-item-view: MM live Hookshot, pool 0: shared Hookshot have=%d count=%d\n", (int)sharedRow.have,
           sharedRow.count);
    CIV_ASSERT(sharedRow.have && sharedRow.count == 1);
    CIV_ASSERT(CivPoolValue(RSBS_SHARED_RES_HOOKSHOT_TIER) == 0);
    // MM tops out at a hookshot: a Longshot the pool holds stays.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HOOKSHOT_TIER, 2);
    CIV_ASSERT(CivFindSharedRow("Hookshot", &sharedRow) && sharedRow.have && sharedRow.count == 2);

    CivFreshPool(99);
    sCivMMLive = mmV0.data();
    CIV_ASSERT(CivFindSharedRow("Rupees", &sharedRow) && sharedRow.count == 150);
    CIV_ASSERT(CivFindSharedRow("Wallet", &sharedRow) && sharedRow.have && sharedRow.count == 1);
    CIV_ASSERT(CivFindSharedRow("Magic", &sharedRow) && sharedRow.have && sharedRow.count == 1);
    CIV_ASSERT(CivPoolValue(RSBS_SHARED_RES_MAGIC_LEVEL) == 0);
    CIV_ASSERT(CivPoolValue(RSBS_SHARED_RES_WALLET_TIER) == 0);
    CIV_ASSERT(CivPoolValue(RSBS_SHARED_RES_RUPEES) == 99);

    sCivLive = NULL;
    sCivMMLive = NULL;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, ootOps);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, mmOps);
    printf("[TEST] combo-item-view: the Shared column follows the active game's live save, the pool untouched PASS "
           "(#971)\n");
    return TEST_PASS;
}

// ---- OoT's tracker groups beyond SoH's default sections (#1142, #1158) ---------
//
// SoH's main window adds, by its DisplayType keys, the dungeon items in its
// compact table, Greg, the fishing pole, the bean and boss souls, the jabber
// nuts, the ocarina buttons and the overworld keys. Each must be a row of OoT's
// adapter, read from the buffer it is handed: the dungeon tables in every file,
// the rest only in a randomizer file whose frozen option shuffles them (Greg:
// any randomizer file). RED before #1142: none of these rows existed.

// The frozen options OoT_ItemAdapter_TestOverrideOptions stands in for.
static const unsigned kCivOptBeanSouls = 1u << 0;
static const unsigned kCivOptBossSouls = 1u << 1;
static const unsigned kCivOptBossSoulsGanon = 1u << 2; // with kCivOptBossSouls: RO_BOSS_SOULS_ON_PLUS_GANON
static const unsigned kCivOptSpeak = 1u << 3;
static const unsigned kCivOptButtons = 1u << 4;
static const unsigned kCivOptOverworldDoors = 1u << 5;
static const unsigned kCivOptFishingPole = 1u << 6;
static const unsigned kCivOptAll =
    kCivOptBeanSouls | kCivOptBossSouls | kCivOptSpeak | kCivOptButtons | kCivOptOverworldDoors | kCivOptFishingPole;

/** OoT's rows of `group`, as the view hands them over. */
static std::vector<ComboItemRow> CivOoTGroup(const char* group) {
    std::vector<ComboItemRow> out;
    ComboItemRow row;
    const int n = Combo_ItemCount((uint8_t)GAME_OOT);
    for (int i = 0; i < n; i++) {
        if (Combo_ItemRowAt((uint8_t)GAME_OOT, i, &row) && strcmp(row.group, group) == 0) {
            out.push_back(row);
        }
    }
    return out;
}

/** Row `name` of `rows`, or NULL. */
static const ComboItemRow* CivRowNamed(const std::vector<ComboItemRow>& rows, const char* name) {
    for (const ComboItemRow& r : rows) {
        if (!r.pad && r.name != NULL && strcmp(r.name, name) == 0) {
            return &r;
        }
    }
    return NULL;
}

/** Does `rows` hold a row named `name`, held as `have`, labelled `label` (NULL: none), with an icon? */
static bool CivRowIs(const std::vector<ComboItemRow>& rows, const char* name, bool have, const char* label) {
    const ComboItemRow* r = CivRowNamed(rows, name);
    if (r == NULL) {
        printf("[TEST] combo-item-view: no OoT row \"%s\"\n", name);
        return false;
    }
    const bool labelOk = (label == NULL) ? r->label == NULL : (r->label != NULL && strcmp(r->label, label) == 0);
    if (r->have != have || !labelOk || r->iconKey == NULL || r->iconKeyFaded == NULL) {
        printf("[TEST] combo-item-view: OoT row \"%s\" have=%d (want %d) label=%s (want %s) icon=%s\n", name,
               (int)r->have, (int)have, r->label != NULL ? r->label : "(none)", label != NULL ? label : "(none)",
               r->iconKey != NULL ? r->iconKey : "(none)");
        return false;
    }
    return true;
}

/** A dungeon's small-key row: `count` collected, `held` in hand, of `max`; lit while a key is held or used. */
static bool CivKeyRowIs(const std::vector<ComboItemRow>& rows, const char* name, bool have, int count, int held,
                        int max, const char* label) {
    if (!CivRowIs(rows, name, have, label)) {
        return false;
    }
    const ComboItemRow* r = CivRowNamed(rows, name);
    if (r->countKind != COMBO_ITEM_COUNT_KIND_KEYS || r->count != count || r->held != held || r->max != max) {
        printf("[TEST] combo-item-view: OoT keys \"%s\" kind=%u collected=%d held=%d max=%d (want %d/%d/%d)\n", name,
               (unsigned)r->countKind, r->count, r->held, r->max, count, held, max);
        return false;
    }
    return true;
}

static int CivOoTTrackerGroupLegs(const ComboItemOps* ops) {
    std::vector<uint8_t> world((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> vanilla((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    CIV_ASSERT(OoT_ItemAdapter_TestAuthorTrackerWorld(world.data(), world.size(), 1) == 1);
    CIV_ASSERT(OoT_ItemAdapter_TestAuthorTrackerWorld(vanilla.data(), vanilla.size(), 0) == 1);
    CIV_ASSERT(ops->hasSave(world.data()) && ops->hasSave(vanilla.data()));
    Context_UpdateShadowCopy(GAME_OOT, world.data(), world.size());
    Context_SetCurrentGame(GAME_MM);
    OoT_ItemAdapter_TestOverrideOptions(1, 0u);

    // ---- A. the dungeon items: SoH's compact table with maps, column-major, 36 cells
    // (GetDungeonItemsVector(itemTrackerDungeonsWithMapsCompact), Thieves' Hideout in cell 35).
    std::vector<ComboItemRow> dungeon = CivOoTGroup("Dungeon Items");
    printf("[TEST] combo-item-view: OoT \"Dungeon Items\" rows=%zu (want 36)\n", dungeon.size());
    CIV_ASSERT(dungeon.size() == 36);
    static const char* const kWithMaps[36] = {
        "Forest Temple Small Key", "Fire Temple Small Key",    "Water Temple Small Key",
        "Spirit Temple Small Key", "Shadow Temple Small Key",  "Bottom of the Well Small Key",
        "Forest Temple Boss Key",  "Fire Temple Boss Key",     "Water Temple Boss Key",
        "Spirit Temple Boss Key",  "Shadow Temple Boss Key",   "Bottom of the Well Map",
        "Forest Temple Map",       "Fire Temple Map",          "Water Temple Map",
        "Spirit Temple Map",       "Shadow Temple Map",        "Bottom of the Well Compass",
        "Forest Temple Compass",   "Fire Temple Compass",      "Water Temple Compass",
        "Spirit Temple Compass",   "Shadow Temple Compass",    NULL,
        "Deku Tree Map",           "Dodongo's Cavern Map",     "Jabu-Jabu's Belly Map",
        "Ice Cavern Map",          "Ganon's Castle Small Key", "Gerudo Training Ground Small Key",
        "Deku Tree Compass",       "Dodongo's Cavern Compass", "Jabu-Jabu's Belly Compass",
        "Ice Cavern Compass",      "Ganon's Castle Boss Key",  "Thieves' Hideout Small Key",
    };
    for (size_t i = 0; i < dungeon.size(); i++) {
        if (kWithMaps[i] == NULL) {
            CIV_ASSERT(dungeon[i].pad); // SoH's ITEM_NONE blank
            continue;
        }
        if (dungeon[i].pad || strcmp(dungeon[i].name, kWithMaps[i]) != 0) {
            printf("[TEST] FAIL: OoT dungeon cell %zu is \"%s\", SoH draws \"%s\" there\n", i,
                   dungeon[i].pad ? "(blank)" : dungeon[i].name, kWithMaps[i]);
            return TEST_FAIL;
        }
    }
    // Keys: collected (ship.stats), in hand, of the dungeon's vanilla keys; lit while any is held or used (SoH:
    // dungeonKeys >= 0). Labels: SoH's short names over each small key and the four map-only dungeons' maps.
    CIV_ASSERT(CivKeyRowIs(dungeon, "Forest Temple Small Key", true, 3, 2, 5, "FRST"));
    CIV_ASSERT(CivKeyRowIs(dungeon, "Fire Temple Small Key", true, 1, 0, 8, "FIRE"));
    CIV_ASSERT(CivKeyRowIs(dungeon, "Water Temple Small Key", false, 0, 0, 6, "WATR"));
    CIV_ASSERT(CivKeyRowIs(dungeon, "Thieves' Hideout Small Key", true, 2, 1, 4, "HIDE"));
    CIV_ASSERT(CivKeyRowIs(dungeon, "Gerudo Training Ground Small Key", false, 0, 0, 9, "GTG"));
    CIV_ASSERT(CivRowIs(dungeon, "Forest Temple Boss Key", true, NULL));
    CIV_ASSERT(CivRowIs(dungeon, "Forest Temple Map", true, NULL));
    CIV_ASSERT(CivRowIs(dungeon, "Forest Temple Compass", false, NULL));
    CIV_ASSERT(CivRowIs(dungeon, "Deku Tree Compass", true, NULL));
    CIV_ASSERT(CivRowIs(dungeon, "Deku Tree Map", false, "DEKU"));
    CIV_ASSERT(CivRowIs(dungeon, "Ice Cavern Map", false, "ICE"));
    CIV_ASSERT(CivRowIs(dungeon, "Ganon's Castle Boss Key", true, NULL)); // Ganon's Tower's scene holds it
    CIV_ASSERT(CivRowIs(dungeon, "Spirit Temple Boss Key", false, NULL));

    // The compact table without maps (DungeonItems.DisplayMaps off): 15 cells, keys only.
    std::vector<ComboItemRow> keysOnly = CivOoTGroup("Dungeon Keys");
    printf("[TEST] combo-item-view: OoT \"Dungeon Keys\" rows=%zu (want 15)\n", keysOnly.size());
    CIV_ASSERT(keysOnly.size() == 15);
    static const char* const kKeysOnly[15] = {
        "Forest Temple Small Key",      "Fire Temple Small Key",
        "Water Temple Small Key",       "Spirit Temple Small Key",
        "Shadow Temple Small Key",      "Ganon's Castle Small Key",
        "Forest Temple Boss Key",       "Fire Temple Boss Key",
        "Water Temple Boss Key",        "Spirit Temple Boss Key",
        "Shadow Temple Boss Key",       "Ganon's Castle Boss Key",
        "Bottom of the Well Small Key", "Gerudo Training Ground Small Key",
        "Thieves' Hideout Small Key",
    };
    for (size_t i = 0; i < keysOnly.size(); i++) {
        CIV_ASSERT(!keysOnly[i].pad && strcmp(keysOnly[i].name, kKeysOnly[i]) == 0);
    }
    CIV_ASSERT(CivKeyRowIs(keysOnly, "Forest Temple Small Key", true, 3, 2, 5, "FRST"));

    // ---- B. the shuffle-gated groups: none while every option is off (Greg: any randomizer file) --------
    static const struct {
        const char* group;
        unsigned option;
        size_t rows;
    } kGated[] = {
        { "Fishing Pole", kCivOptFishingPole, 1 }, { "Bean Souls", kCivOptBeanSouls, 10 },
        { "Boss Souls", kCivOptBossSouls, 9 },     { "Jabber Nuts", kCivOptSpeak, 6 },
        { "Ocarina Buttons", kCivOptButtons, 5 },  { "Overworld Keys", kCivOptOverworldDoors, 24 },
    };
    const int fixedRows = ops->count();
    CIV_ASSERT(ops->countFor != NULL);
    CIV_ASSERT(CivOoTGroup("Greg").size() == 1 && CivRowIs(CivOoTGroup("Greg"), "Greg the Green Rupee", true, NULL));
    for (const auto& g : kGated) {
        CIV_ASSERT(CivOoTGroup(g.group).empty());
    }
    CIV_ASSERT(ops->countFor(world.data()) == fixedRows + 1);
    // Each option shows its own group and nothing else.
    for (const auto& g : kGated) {
        OoT_ItemAdapter_TestOverrideOptions(1, g.option);
        for (const auto& other : kGated) {
            const size_t want = (&other == &g) ? other.rows : 0u;
            if (CivOoTGroup(other.group).size() != want) {
                printf("[TEST] FAIL: option bit 0x%x alone: OoT group \"%s\" has %zu rows (want %zu)\n", g.option,
                       other.group, CivOoTGroup(other.group).size(), want);
                return TEST_FAIL;
            }
        }
    }
    // A vanilla file shows none of them, whatever the options say, and no Greg; its dungeon tables stay.
    OoT_ItemAdapter_TestOverrideOptions(1, kCivOptAll);
    CIV_ASSERT(ops->countFor(vanilla.data()) == fixedRows);
    Context_UpdateShadowCopy(GAME_OOT, vanilla.data(), vanilla.size());
    CIV_ASSERT(CivOoTGroup("Greg").empty() && CivOoTGroup("Boss Souls").empty());
    CIV_ASSERT(CivOoTGroup("Dungeon Items").size() == 36 && CivOoTGroup("Dungeon Keys").size() == 15);
    Context_UpdateShadowCopy(GAME_OOT, world.data(), world.size());

    // Every option on: SoH's order after the dungeon tables, contiguous, each row read from the buffer.
    const int allRows = ops->countFor(world.data());
    printf("[TEST] combo-item-view: OoT rows with every option on: %d (fixed %d)\n", allRows, fixedRows);
    CIV_ASSERT(allRows == fixedRows + 1 + 1 + 10 + 9 + 6 + 5 + 24);
    ComboItemRow probe;
    for (int i = 0; i < allRows; i++) {
        memset(&probe, 0, sizeof(probe));
        CIV_ASSERT(ops->rowAt(world.data(), i, &probe));
        CIV_ASSERT(probe.group != NULL && probe.name != NULL);
        CIV_ASSERT(probe.pad || (probe.name[0] != '\0' && probe.iconKey != NULL && probe.iconKeyFaded != NULL));
    }
    CIV_ASSERT(!ops->rowAt(world.data(), allRows, &probe));
    static const char* const kOrder[] = { "Songs",           "Dungeon Items", "Dungeon Keys", "Greg",
                                          "Fishing Pole",    "Bean Souls",    "Boss Souls",   "Jabber Nuts",
                                          "Ocarina Buttons", "Overworld Keys" };
    int lastFirst = -1;
    for (const char* group : kOrder) {
        int first = -1;
        for (int i = 0; i < allRows && first < 0; i++) {
            memset(&probe, 0, sizeof(probe));
            if (ops->rowAt(world.data(), i, &probe) && strcmp(probe.group, group) == 0) {
                first = i;
            }
        }
        CIV_ASSERT(first > lastFirst);
        lastFirst = first;
    }
    std::vector<ComboItemRow> g = CivOoTGroup("Fishing Pole");
    CIV_ASSERT(CivRowIs(g, "Fishing Pole", true, NULL));
    g = CivOoTGroup("Bean Souls");
    CIV_ASSERT(CivRowIs(g, "Kokiri Forest Bean Soul", true, "KF"));
    CIV_ASSERT(CivRowIs(g, "Graveyard Bean Soul", false, "GY"));
    g = CivOoTGroup("Boss Souls");
    CIV_ASSERT(CivRowIs(g, "Gohma's Soul", true, "GOHMA"));
    CIV_ASSERT(CivRowIs(g, "Twinrova's Soul", true, "TWIN"));
    CIV_ASSERT(CivRowIs(g, "Volvagia's Soul", false, "VOLV"));
    CIV_ASSERT(CivRowIs(g, "Ganon's Soul", true, "GANON")); // not shuffled: SoH draws it held
    g = CivOoTGroup("Jabber Nuts");
    CIV_ASSERT(CivRowIs(g, "Zora Jabber Nut", true, "ZORA"));
    CIV_ASSERT(CivRowIs(g, "Deku Jabber Nut", false, "DEKU"));
    g = CivOoTGroup("Ocarina Buttons");
    CIV_ASSERT(CivRowIs(g, "Ocarina A Button", true, "A"));
    CIV_ASSERT(CivRowIs(g, "Ocarina C Left Button", true, "C-L"));
    CIV_ASSERT(CivRowIs(g, "Ocarina C Up Button", false, "C-U"));
    // OoT's copy of each Shared Ocarina Buttons family slot (#1067), as MM's rows are.
    CIV_ASSERT(CivRowNamed(g, "Ocarina A Button")->sharedFamily == RSBS_XITEM_SHARED_OCARINA_BUTTONS);
    CIV_ASSERT(CivRowNamed(g, "Ocarina C Left Button")->sharedFamily == RSBS_XFAMILY_SLOT_OCARINA_C_LEFT);
    CIV_ASSERT(CivRowNamed(g, "Ocarina C Up Button")->sharedFamily == RSBS_XFAMILY_SLOT_OCARINA_C_UP);
    CIV_ASSERT(CivRowNamed(g, "Ocarina C Down Button")->sharedFamily == RSBS_XFAMILY_SLOT_OCARINA_C_DOWN);
    CIV_ASSERT(CivRowNamed(g, "Ocarina C Right Button")->sharedFamily == RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT);
    g = CivOoTGroup("Overworld Keys");
    CIV_ASSERT(CivRowIs(g, "Guard House Key", true, "GUARD"));
    CIV_ASSERT(CivRowIs(g, "Fishing Hole Key", true, "FISH"));
    CIV_ASSERT(CivRowIs(g, "Windmill Key", false, "WIND"));
    // Ganon's soul shuffled (RO_BOSS_SOULS_ON_PLUS_GANON): its own flag, unset here.
    OoT_ItemAdapter_TestOverrideOptions(1, kCivOptAll | kCivOptBossSoulsGanon);
    CIV_ASSERT(CivRowIs(CivOoTGroup("Boss Souls"), "Ganon's Soul", false, "GANON"));
    OoT_ItemAdapter_TestOverrideOptions(1, kCivOptAll);

    // ---- C. each gated group on its own line of SoH's grid; the dungeon tables flow on after the songs ----
    const ComboItemGridStyle* grid = Combo_ItemGridStyle((uint8_t)GAME_OOT);
    CIV_ASSERT(grid->groupOwnLine != NULL && grid->columns == 6 && grid->cellPx == 36.0f && grid->gapPx == 12.0f);
    for (const char* own :
         { "Greg", "Fishing Pole", "Bean Souls", "Boss Souls", "Jabber Nuts", "Ocarina Buttons", "Overworld Keys" }) {
        CIV_ASSERT(grid->groupOwnLine(own));
    }
    for (const char* flow : { "Inventory", "Songs", "Dungeon Items", "Dungeon Keys" }) {
        CIV_ASSERT(!grid->groupOwnLine(flow));
    }

    // ---- D. SoH's DisplayType keys govern them in the overlay, hidden by SoH's default -------------------
    static const struct {
        const char* group;
        const char* key;
    } kKeys[] = {
        { "Dungeon Items", "gTrackers.ItemTracker.DisplayType.DungeonItems" },
        { "Greg", "gTrackers.ItemTracker.DisplayType.Greg" },
        { "Fishing Pole", "gTrackers.ItemTracker.DisplayType.FishingPole" },
        { "Bean Souls", "gTrackers.ItemTracker.DisplayType.BeanSouls" },
        { "Boss Souls", "gTrackers.ItemTracker.DisplayType.BossSouls" },
        { "Jabber Nuts", "gTrackers.ItemTracker.DisplayType.JabberNuts" },
        { "Ocarina Buttons", "gTrackers.ItemTracker.DisplayType.OcarinaButtons" },
        { "Overworld Keys", "gTrackers.ItemTracker.DisplayType.OverworldKeys" },
    };
    const char* const kMapsKey = "gTrackers.ItemTracker.DungeonItems.DisplayMaps";
    for (const auto& k : kKeys) {
        CVarClear(k.key);
    }
    CVarClear(kMapsKey);
    ComboGui::ComboItemTrackerKeys keys = ComboGui::ComboReadItemTrackerKeys();
    int visibilityFailures = 0;
    for (const auto& k : kKeys) {
        visibilityFailures += ComboGui::ComboItemTrackerSohGroupShown(keys, k.group) ? 1 : 0;
    }
    visibilityFailures += ComboGui::ComboItemTrackerSohGroupShown(keys, "Dungeon Keys") ? 1 : 0;
    visibilityFailures += ComboGui::ComboItemTrackerSohGroupShown(keys, "Inventory") ? 0 : 1;
    for (const auto& k : kKeys) {
        for (int value : { 1, 2 }) { // Main Window; Separate (or Misc Window): the overlay is one window
            CVarSetInteger(k.key, value);
            keys = ComboGui::ComboReadItemTrackerKeys();
            for (const auto& other : kKeys) {
                if (ComboGui::ComboItemTrackerSohGroupShown(keys, other.group) != (&other == &k)) {
                    visibilityFailures++;
                }
            }
        }
        CVarClear(k.key);
    }
    // The dungeon table follows DungeonItems.DisplayMaps: with maps (SoH's default), or keys only.
    CVarSetInteger(kKeys[0].key, 1);
    keys = ComboGui::ComboReadItemTrackerKeys();
    const bool mapsShown = ComboGui::ComboItemTrackerSohGroupShown(keys, "Dungeon Items");
    const bool keysHidden = !ComboGui::ComboItemTrackerSohGroupShown(keys, "Dungeon Keys");
    CVarSetInteger(kMapsKey, 0);
    keys = ComboGui::ComboReadItemTrackerKeys();
    const bool mapsHidden = !ComboGui::ComboItemTrackerSohGroupShown(keys, "Dungeon Items");
    const bool keysShown = ComboGui::ComboItemTrackerSohGroupShown(keys, "Dungeon Keys");
    CVarClear(kMapsKey);

    // The overlay's OoT section with every key on lays each gated group from a line's start.
    for (const auto& k : kKeys) {
        CVarSetInteger(k.key, 1);
    }
    ComboGui::ComboItemTrackerSection section;
    ComboGui::ComboItemTrackerCollectSection(ComboGui::COMBO_ITEM_SECTION_OOT, section);
    for (const auto& k : kKeys) {
        CVarClear(k.key);
    }
    std::vector<ComboGui::ComboItemGridCell> cells;
    ComboGui::ComboItemIconLayout(section.rows, *section.grid, cells);
    int lineStarts = 0;
    for (size_t i = 0; i < section.rows.size() && i < cells.size(); i++) {
        const bool first = i == 0 || strcmp(section.rows[i - 1].group, section.rows[i].group) != 0;
        if (first && grid->groupOwnLine != NULL && grid->groupOwnLine(section.rows[i].group)) {
            lineStarts += cells[i].column == 0 ? 1 : 0;
        }
    }
    printf("[TEST] combo-item-view: OoT overlay rows with every DisplayType on: %zu, gated groups from a line's "
           "start: %d (want 7); visibility failures %d; maps %d/%d keys %d/%d\n",
           section.rows.size(), lineStarts, visibilityFailures, (int)mapsShown, (int)mapsHidden, (int)keysShown,
           (int)keysHidden);
    CIV_ASSERT(visibilityFailures == 0 && mapsShown && keysHidden && mapsHidden && keysShown);
    CIV_ASSERT(lineStarts == 7);
    CIV_ASSERT(section.rows.size() == (size_t)(66 + 36 + 1 + 1 + 10 + 9 + 6 + 5 + 24));

    // ---- E. the slot guard (SoH 7a7f942b8): an item id past gItemSlots names no slot ---------------------
    const int slots = OoT_ItemAdapter_TestItemSlotCount();
    const int kItemHookshot = 0x0A, kItemLongshot = 0x0B, kItemFishingPole = 0x59, kItemNone = 0xFF; // z64item.h
    printf("[TEST] combo-item-view: guarded slot reads: hookshot=%d fishing pole=%d past table=%d (slots %d)\n",
           OoT_ItemAdapter_TestInvContent(world.data(), kItemHookshot),
           OoT_ItemAdapter_TestInvContent(world.data(), kItemFishingPole),
           OoT_ItemAdapter_TestInvContent(world.data(), slots), slots);
    CIV_ASSERT(slots > kItemLongshot && slots <= kItemFishingPole);
    CIV_ASSERT(OoT_ItemAdapter_TestInvContent(world.data(), kItemHookshot) == kItemLongshot);
    CIV_ASSERT(OoT_ItemAdapter_TestInvContent(world.data(), kItemFishingPole) == kItemNone);
    CIV_ASSERT(OoT_ItemAdapter_TestInvContent(world.data(), slots) == kItemNone);
    CIV_ASSERT(OoT_ItemAdapter_TestInvContent(world.data(), 255) == kItemNone);
    CIV_ASSERT(OoT_ItemAdapter_TestInvContent(world.data(), -1) == kItemNone);

    OoT_ItemAdapter_TestOverrideOptions(0, 0u);
    return TEST_PASS;
}

// ---- the Shared column shows the pooled pieces toward the next heart (#1159) ----
//
// Health is ONE quantity for the pair (Combo_MakeHealthQuarters: capacity plus
// four units per piece, sixteen per heart), max-merged at every harvest and
// raised into the arriving game at every apply. So four pieces make a heart
// whichever game each came from, and the Shared column shows the pieces toward
// the next one right after the hearts. RED before #1159: the Shared Hearts row
// truncated the pieces and no row carried them.
static int CivSharedHeartLegs(void) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Context_SetCurrentGame(GAME_NONE); // the pool alone, no live game
    ComboItemRow hearts;
    ComboItemRow pieces;
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(0x50, 2));
    CIV_ASSERT(CivFindSharedRow("Hearts", &hearts) && hearts.count == 5 && hearts.max == 20);
    const bool found = CivFindSharedRow("Heart Pieces", &pieces);
    printf("[TEST] combo-item-view: pool 5 hearts + 2 pieces: Heart Pieces row %s count=%d max=%d have=%d\n",
           found ? "shown" : "MISSING", found ? pieces.count : -1, found ? pieces.max : -1,
           found ? (int)pieces.have : -1);
    CIV_ASSERT(found && pieces.have && pieces.count == 2 && pieces.max == 4);
    // It follows the Hearts row in the Shared column.
    int heartsAt = -1;
    int piecesAt = -1;
    ComboItemRow row;
    for (int i = 0; i < Combo_ItemSharedCount(); i++) {
        CIV_ASSERT(Combo_ItemSharedRowAt(i, &row));
        heartsAt = strcmp(row.name, "Hearts") == 0 ? i : heartsAt;
        piecesAt = strcmp(row.name, "Heart Pieces") == 0 ? i : piecesAt;
    }
    CIV_ASSERT(heartsAt >= 0 && piecesAt == heartsAt + 1);
    // Each game's own heart-piece icon (OoT: SoH's ITEM_HEART_PIECE; MM: its own, not the container).
    CIV_ASSERT(Combo_ItemSharedRowAtWithIcons(piecesAt, (uint8_t)GAME_OOT, &row));
    CIV_ASSERT(row.iconKey != NULL && strcmp(row.iconKey, "ITEM_HEART_PIECE") == 0);
    ComboItemRow mmHearts;
    CIV_ASSERT(Combo_ItemSharedRowAtWithIcons(heartsAt, (uint8_t)GAME_MM, &mmHearts));
    CIV_ASSERT(Combo_ItemSharedRowAtWithIcons(piecesAt, (uint8_t)GAME_MM, &row));
    CIV_ASSERT(row.iconKey != NULL && mmHearts.iconKey != NULL && strcmp(row.iconKey, mmHearts.iconKey) != 0);

    // A third piece found in MM (the pool's two applied there first) is the pair's third.
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(0x50, 3));
    CIV_ASSERT(CivFindSharedRow("Heart Pieces", &pieces) && pieces.count == 3);
    CIV_ASSERT(CivFindSharedRow("Hearts", &hearts) && hearts.count == 5);
    // A fourth from OoT completes the heart, for both games: 6 hearts, no pieces.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(0x60, 0));
    CIV_ASSERT(CivFindSharedRow("Hearts", &hearts) && hearts.count == 6);
    CIV_ASSERT(CivFindSharedRow("Heart Pieces", &pieces) && pieces.count == 0 && !pieces.have);
    // OoT holding a fourth piece it has not converted yet (it converts when the textbox closes) reads the same.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(0x60, 3));
    CIV_ASSERT(CivFindSharedRow("Heart Pieces", &pieces) && pieces.count == 3);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(0x60, 4));
    CIV_ASSERT(CivFindSharedRow("Hearts", &hearts) && hearts.count == 7);
    CIV_ASSERT(CivFindSharedRow("Heart Pieces", &pieces) && pieces.count == 0);
    // The cap: a game past 20 hearts (neither give path clamps) harvests 20, and no piece shows past it.
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_HEALTH_QUARTERS, Combo_MakeHealthQuarters(0x150, 3));
    CIV_ASSERT(CivFindSharedRow("Hearts", &hearts) && hearts.count == 20 && hearts.max == 20);
    CIV_ASSERT(CivFindSharedRow("Heart Pieces", &pieces) && pieces.count == 0);
    uint16_t pooled = 0;
    CIV_ASSERT(Combo_GetSharedResource(RSBS_SHARED_RES_HEALTH_QUARTERS, &pooled) &&
               pooled == RSBS_SHARED_RES_MAX_HEALTH_QUARTERS);
    return TEST_PASS;
}

extern "C" int Combo_ItemView_RunHeadless(void) {
    printf("[TEST] combo-item-view: per-game item adapters over live/shadow buffers, the shared group (#458 U1)\n");

    // ---- snapshot everything this row touches (restored at the end) --------
    const GameId prevGame = Context_GetCurrentGame();
    const ComboItemOps* prevOoT = Combo_Item_GetOps((uint8_t)GAME_OOT);
    const ComboItemOps* prevMM = Combo_Item_GetOps((uint8_t)GAME_MM);
    std::vector<uint8_t> ootShadowBackup((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    if (const uint8_t* p = (const uint8_t*)Context_GetOoTSaveContext()) {
        memcpy(ootShadowBackup.data(), p, ootShadowBackup.size());
    }
    std::vector<uint8_t> mmShadowBackup((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    if (const uint8_t* p = (const uint8_t*)Context_GetMMSaveContext()) {
        memcpy(mmShadowBackup.data(), p, mmShadowBackup.size());
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
                 strcmp(Combo_ItemFreshnessLabel(g, COMBO_TRACKER_FRESH_UNAVAILABLE), "") == 0;
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
        // SoH's 66 default main-window rows plus its two dungeon tables (36 and 15 cells, #1142).
        if (ops->count() < 110 || ops->count() > 125) {
            printf("[TEST] FAIL: OoT item adapter row count %d is not the curated 66 plus the dungeon tables\n",
                   ops->count());
            result = TEST_FAIL;
            break;
        }
        printf("[TEST] combo-item-view: OoT adapter registered with %d curated rows\n", ops->count());
        if (CivOoTLegs(ops) != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }

        // ---- 2 for OoT's dungeon tables and shuffle-gated groups (#1142, #1158) --
        const int ootGroupResult = CivOoTTrackerGroupLegs(ops);
        OoT_ItemAdapter_TestOverrideOptions(0, 0u);
        if (ootGroupResult != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }

        // ---- 2-4 for MM (#458 U1b), from the same production bring-up ------
        const ComboItemOps* mmOps = Combo_Item_GetOps((uint8_t)GAME_MM);
        if (mmOps == NULL) {
            printf("[TEST] FAIL: Combo_TrackerWindow_Init did not register MM's item adapter\n");
            result = TEST_FAIL;
            break;
        }
        // The 60 curated rows, MM's always-on Tokens, Stray Fairies and Dungeon (83 with them, #1140) and the
        // heart pieces.
        if (mmOps->count() < 75 || mmOps->count() > 95) {
            printf("[TEST] FAIL: MM item adapter row count %d is not the curated 83 plus the heart pieces\n",
                   mmOps->count());
            result = TEST_FAIL;
            break;
        }
        printf("[TEST] combo-item-view: MM adapter registered with %d curated rows\n", mmOps->count());
        if (CivMMLegs(mmOps, ops) != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }

        // ---- 2 for MM's ocarina buttons (#1072) --------------------------------
        if (CivMMOcarinaButtonLegs(mmOps) != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }

        // ---- 2 for MM's tokens, fairies, dungeon items and gated groups (#1140) --
        if (CivMMTrackerRowLegs(mmOps) != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }

        // ---- 2 for MM's per-group visibility keys (#1141) -----------------------
        if (CivMMGroupVisibilityLegs(mmOps) != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }

        // ---- 5. the Shared group -----------------------------------------
        if (CivSharedLegs() != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }

        // ---- 5a. the Shared column's pooled heart pieces (#1159) -----------
        if (CivSharedHeartLegs() != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }

        // ---- 5b. the shared capacity tiers, once (#967) -----------------------
        if (CivSharedTierLegs(ops, mmOps) != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }

        // ---- 5c. the Shared column follows the active game's live save (#971) --
        if (CivSharedLiveLegs(ops, mmOps) != TEST_PASS) {
            result = TEST_FAIL;
            break;
        }
    } while (0);

    // ---- 6. put state back (watermarks: reset, not restored; see header) ---
    sCivLive = NULL;
    sCivMMLive = NULL;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, prevOoT);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, prevMM);
    Context_UpdateShadowCopy(GAME_OOT, ootShadowBackup.data(), ootShadowBackup.size());
    Context_UpdateShadowCopy(GAME_MM, mmShadowBackup.data(), mmShadowBackup.size());
    gComboCtx = sCtxBackup;
    Combo_ResetSharedResourceWatermarks();
    Context_SetCurrentGame(prevGame);
    const uint8_t* restored = (const uint8_t*)Context_GetOoTSaveContext();
    if (restored == NULL || memcmp(restored, ootShadowBackup.data(), ootShadowBackup.size()) != 0) {
        printf("[TEST] FAIL: the OoT shadow was not restored byte-exact\n");
        return TEST_FAIL;
    }
    const uint8_t* restoredMM = (const uint8_t*)Context_GetMMSaveContext();
    if (restoredMM == NULL || memcmp(restoredMM, mmShadowBackup.data(), mmShadowBackup.size()) != 0) {
        printf("[TEST] FAIL: the MM shadow was not restored byte-exact\n");
        return TEST_FAIL;
    }
    if (result != TEST_PASS) {
        return result;
    }

    printf("[TEST] PASS: item rows recover authored OoT and MM live/shadow saves with honest freshness, never LIVE "
           "for the inactive game, and the shared group carries the pool and its label (#458 U1)\n");
    return TEST_PASS;
}

// ============================================================================
// combo-item-view-new-file (#873): a save Ship of Harkinian itself made
// ============================================================================
//
// Every leg above reads saves the OoT seam authors by hand, and that seam used
// to stamp the 'ZELDAZ' new-file marker, which Ship of Harkinian never writes:
// its new-file path (SaveManager::InitFileNormal, behind Save_InitFile) and its
// load path leave `newf` alone. So these legs passed while the overlay read
// "No data." for OoT in every real session. This row reads the save the
// production new-file path authors, unmarked, through the registered adapter:
//
//   A. the adapter accepts it as a save;
//   B. as OoT's frozen shadow while MM is played, every row reads, STALE, and
//      the rows are a new file's (nothing found, a child's wallet of 0/99);
//   C. as OoT's live save while OoT is played, LIVE;
//   D. the title screen's attract save (the debug save under fileNum 0xFF and
//      GAMEMODE_TITLE_SCREEN, which F10 there freezes, #837) is not the
//      player's file: UNAVAILABLE; the same debug save in a played session
//      (GAMEMODE_NORMAL) reads STALE;
//   E. an all-zero shadow (OoT never frozen) is still "No data";
//   F. the .redsave slot panel reads a slot whose OoT half is this save as
//      OoT-started, and an all-zero OoT half as not started.
//
// Rando tier: Save_InitFile reads OTRGlobals and SaveManager::Instance, which
// only the full OoT bring-up constructs.

static inline const char* kCivNewFileSaveDir() {
    return rsbs::test::ScratchDirCStr("rsbs_test_saves_item_view_new_file");
}
static bool sCivNewFileSaveDirSet = false; // the entry point puts "Save" back when set

static int CivNewFileSlotPanel(const std::vector<uint8_t>& newFile) {
    const std::vector<uint8_t> zeros((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    OoT_SlotMeta_Register();
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kCivNewFileSaveDir());
    sCivNewFileSaveDirSet = true;
    mgr.ResetSlotSessionState();

    mgr.DeleteSave(0); // an erase is what unlatches the slot for this session's write
    Context_UpdateShadowCopy(GAME_OOT, newFile.data(), newFile.size());
    CIV_ASSERT(mgr.Save(0));
    rsbs::SlotMeta meta = mgr.ReadMeta(0);
    printf("[TEST] combo-item-view-new-file: slot panel over the new file: valid=%d ootStarted=%d line=\"%s\"\n",
           (int)meta.valid, (int)meta.ootStarted, rsbs::SlotNameLine(meta).c_str());
    CIV_ASSERT(meta.valid && meta.ootStarted);

    mgr.DeleteSave(0);
    Context_UpdateShadowCopy(GAME_OOT, zeros.data(), zeros.size());
    CIV_ASSERT(mgr.Save(0));
    meta = mgr.ReadMeta(0);
    printf("[TEST] combo-item-view-new-file: slot panel over an all-zero OoT half: valid=%d ootStarted=%d\n",
           (int)meta.valid, (int)meta.ootStarted);
    CIV_ASSERT(meta.valid && !meta.ootStarted);
    return TEST_PASS;
}

static int CivNewFileLegs(const ComboItemOps* ops, const std::vector<uint8_t>& newFile) {
    ComboItemRow row;
    const std::vector<uint8_t> zeros((size_t)OOT_SAVE_CONTEXT_SIZE, 0);

    // ---- A. the adapter accepts a save SoH made -----------------------------
    const bool accepted = ops->hasSave(newFile.data());
    printf("[TEST] combo-item-view-new-file: OoT adapter hasSave(Save_InitFile(false) save)=%d\n", (int)accepted);
    CIV_ASSERT(accepted);
    CIV_ASSERT(!ops->hasSave(zeros.data()));

    // ---- B. the frozen shadow, read while MM is played ----------------------
    Context_UpdateShadowCopy(GAME_OOT, newFile.data(), newFile.size());
    Context_SetCurrentGame(GAME_MM);
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_STALE);
    const int rows = Combo_ItemCount((uint8_t)GAME_OOT);
    printf("[TEST] combo-item-view-new-file: OoT rows=%d freshness=%u under GAME_MM\n", rows,
           (unsigned)Combo_ItemFreshness((uint8_t)GAME_OOT));
    CIV_ASSERT(rows == ops->count());
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Kokiri Sword", &row) && !row.have);
    CIV_ASSERT(row.freshness == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Fairy Slingshot", &row) && !row.have && row.count == 0);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Song of Time", &row) && !row.have);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Child's Wallet", &row));
    printf("[TEST] combo-item-view-new-file: shadow Child's Wallet have=%d count=%d max=%d\n", (int)row.have, row.count,
           row.max);
    CIV_ASSERT(row.have && row.count == 0 && row.max == 99);

    // ---- C. the live save, read while OoT is played -------------------------
    static ComboItemOps sNewFileOps;
    sNewFileOps = *ops;
    sNewFileOps.liveSave = CivLiveSave;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, &sNewFileOps);
    sCivLive = newFile.data();
    Context_UpdateShadowCopy(GAME_OOT, zeros.data(), zeros.size());
    Context_SetCurrentGame(GAME_OOT);
    const uint8_t liveFreshness = Combo_ItemFreshness((uint8_t)GAME_OOT);
    printf("[TEST] combo-item-view-new-file: GAME_OOT live freshness=%u (LIVE=%u)\n", (unsigned)liveFreshness,
           (unsigned)COMBO_TRACKER_FRESH_LIVE);
    CIV_ASSERT(liveFreshness == COMBO_TRACKER_FRESH_LIVE);
    CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_OOT) == ops->count());
    sCivLive = NULL;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, ops);

    // ---- D. the title screen's attract save is not the player's file ---------
    // F10 on OoT's title freezes the debug save the attract demo plays (#837);
    // it has hearts and every item, and must not read as the player's rows.
    // The same debug save in a played session (fileNum 0xFF, GAMEMODE_NORMAL,
    // as the integration drives play it) is a file and reads.
    Context_SetCurrentGame(GAME_MM);
    std::vector<uint8_t> attract((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> debugPlayed((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    CIV_ASSERT(OoT_ItemAdapter_TestAuthorNewFile(attract.data(), attract.size(), 1, NULL) == 1);
    CIV_ASSERT(OoT_ItemAdapter_TestAuthorNewFile(debugPlayed.data(), debugPlayed.size(), 2, NULL) == 1);
    Context_UpdateShadowCopy(GAME_OOT, attract.data(), attract.size());
    const uint8_t attractFreshness = Combo_ItemFreshness((uint8_t)GAME_OOT);
    printf("[TEST] combo-item-view-new-file: the title's attract save as the shadow: hasSave=%d freshness=%u "
           "(UNAVAILABLE=%u)\n",
           (int)ops->hasSave(attract.data()), (unsigned)attractFreshness, (unsigned)COMBO_TRACKER_FRESH_UNAVAILABLE);
    CIV_ASSERT(!ops->hasSave(attract.data()));
    CIV_ASSERT(attractFreshness == COMBO_TRACKER_FRESH_UNAVAILABLE);
    CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_OOT) == 0);
    Context_UpdateShadowCopy(GAME_OOT, debugPlayed.data(), debugPlayed.size());
    CIV_ASSERT(ops->hasSave(debugPlayed.data()));
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_STALE);
    CIV_ASSERT(CivFindRow((uint8_t)GAME_OOT, "Fairy Slingshot", &row) && row.have); // the debug save's

    // ---- E. an all-zero shadow is still no data ------------------------------
    Context_UpdateShadowCopy(GAME_OOT, zeros.data(), zeros.size());
    CIV_ASSERT(Combo_ItemFreshness((uint8_t)GAME_OOT) == COMBO_TRACKER_FRESH_UNAVAILABLE);
    CIV_ASSERT(Combo_ItemCount((uint8_t)GAME_OOT) == 0);

    // ---- F. the .redsave slot panel ---------------------------------------------
    return CivNewFileSlotPanel(newFile);
}

extern "C" int Combo_ItemView_NewFileRunHeadless(void) {
    printf("[TEST] combo-item-view-new-file: the OoT item adapter and the slot panel accept the save Ship of "
           "Harkinian's own new-file path authors (#873)\n");

    const GameId prevGame = Context_GetCurrentGame();
    const ComboItemOps* prevOoT = Combo_Item_GetOps((uint8_t)GAME_OOT);
    const ComboItemOps* prevMM = Combo_Item_GetOps((uint8_t)GAME_MM);
    std::vector<uint8_t> ootShadowBackup((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    if (const uint8_t* p = (const uint8_t*)Context_GetOoTSaveContext()) {
        memcpy(ootShadowBackup.data(), p, ootShadowBackup.size());
    }
    static ComboContext sNewFileCtxBackup;
    sNewFileCtxBackup = gComboCtx;

    int result = TEST_PASS;
    do {
        // The production registration, as the display-free row does it.
        Combo_Item_RegisterOps((uint8_t)GAME_OOT, NULL);
        Combo_TrackerWindow_Init();
        const ComboItemOps* ops = Combo_Item_GetOps((uint8_t)GAME_OOT);
        if (ops == NULL) {
            printf("[TEST] FAIL: Combo_TrackerWindow_Init did not register OoT's item adapter\n");
            result = TEST_FAIL;
            break;
        }

        std::vector<uint8_t> newFile((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
        int newfMarked = -1;
        if (OoT_ItemAdapter_TestAuthorNewFile(newFile.data(), newFile.size(), 0, &newfMarked) != 1) {
            printf("[TEST] FAIL: the OoT seam could not run Save_InitFile(false) (bring-up missing?)\n");
            result = TEST_FAIL;
            break;
        }
        printf("[TEST] combo-item-view-new-file: Save_InitFile(false) authored the save; its newf carries "
               "'ZELDAZ'=%d\n",
               newfMarked);
        result = CivNewFileLegs(ops, newFile);
    } while (0);

    sCivLive = NULL;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, prevOoT);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, prevMM);
    Context_UpdateShadowCopy(GAME_OOT, ootShadowBackup.data(), ootShadowBackup.size());
    gComboCtx = sNewFileCtxBackup;
    Context_SetCurrentGame(prevGame);
    if (sCivNewFileSaveDirSet) {
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        mgr.DeleteSave(0);
        mgr.ResetSlotSessionState();
        mgr.SetSaveDirectory("Save");
        sCivNewFileSaveDirSet = false;
    }
    if (result != TEST_PASS) {
        return result;
    }
    printf("[TEST] PASS: a save made by Ship of Harkinian's new-file path reads as a save: rows from the shadow "
           "(STALE) and the live save (LIVE), the slot panel reads it started; the title's attract save and an "
           "all-zero half read no data (#873)\n");
    return TEST_PASS;
}
