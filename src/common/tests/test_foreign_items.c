/**
 * @file test_foreign_items.c
 * @brief ROM-free locks for the Lane C1 foreign-item pipeline (#392, ADR 0002).
 *
 * Three of the C1 CI locks live here:
 *
 *  (a) GIVE-PATH TAGGING: a foreign item entering the REAL give-path core
 *      (MM_Rando_Foreign_RecordPickup -> Rando::Foreign::RecordForeignPickup,
 *      the exact function MM's CheckQueue foreign branch calls) lands in
 *      gComboCtx.sharedItemsTagged correctly origin-tagged, and a re-fired
 *      give de-dups instead of double-recording.
 *
 *  (b) ROUND-TRIP SURVIVAL (the SharedItemRoundtrip sibling, with a REAL
 *      pool entry rather than an arbitrary id): the recorded crossing
 *      survives the MM suspend -> OoT arrival leg through the real
 *      freeze/consume + consumer hooks, is awarded exactly once with the
 *      pool item's id, and never awards again.
 *
 *  (+) CARVE SERIALIZATION: gComboCtx.foreignPlacements round-trips
 *      byte-exact through a .redsave Save/Load, and unset slots stay unset
 *      (the growth contract's zero-means-unset, mirroring SaveTaggedItems).
 *
 * The pinned pool itself is also sanity-locked: every entry must be tagged
 * GAME_OOT with a nonzero id and a display name, and the name lookup must
 * round-trip — the MM textbox and both spoiler surfaces depend on it.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as
 * C++, like test_save_roundtrip.c) for the rsbs::SaveManager half; every
 * pipeline symbol it drives is C-linkage.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../shared_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>

// The C-linkage pipeline pieces this test drives (declared locally like
// test_shared_state_roundtrip.c does for the switch policy).
extern "C" {
int MM_Rando_Foreign_RecordPickup(uint16_t randoCheckId);
int Switch_PrepareHotSwap(GameId departing, const void* saveContext, size_t size);
int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);
}

#define FI_ASSERT(cond)                                                                                                \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

namespace {
// Arbitrary MM RandoCheckId values for the common-layer accessors: the table
// stores them opaquely (MM interprets them), so any nonzero u16 exercises the
// format. Zero is RC_UNKNOWN and must be rejected.
const uint16_t kForeignTestCheckA = 0x0123;
const uint16_t kForeignTestCheckB = 0x0456;
const char* const kForeignSaveDir = "rsbs_test_saves_foreign";

struct ForeignAwardCtx {
    int awardCount;
    uint16_t lastId;
    uint8_t lastOrigin;
};

void ForeignTestAward(const SharedItem* item, void* ctx) {
    ForeignAwardCtx* c = (ForeignAwardCtx*)ctx;
    c->awardCount++;
    c->lastId = item->id;
    c->lastOrigin = item->originGame;
}
} // namespace

TestResult Test_ForeignItemGive(void) {
    printf("[TEST] foreign-item-give: give-path core tags the shared structure; crossing survives + awards once "
           "(Lane C1)\n");

    // ------------------------------------------------------------------
    // Pool sanity: pinned, OoT-tagged, named, lookup round-trips.
    // ------------------------------------------------------------------
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPool(&pool);
    FI_ASSERT(pool != NULL);
    FI_ASSERT(poolCount >= 1 && poolCount <= (int)RSBS_FOREIGN_PLACEMENT_CAP);
    for (int i = 0; i < poolCount; i++) {
        FI_ASSERT(pool[i].item.originGame == (uint8_t)GAME_OOT);
        FI_ASSERT(pool[i].item.id != 0);
        FI_ASSERT(pool[i].item.flags == 0);
        FI_ASSERT(pool[i].name != NULL && pool[i].name[0] != '\0');
        FI_ASSERT(Combo_GetForeignItemName(pool[i].item) == pool[i].name);
    }
    // (originGame, name) uniqueness WITHIN a pool. Two entries sharing a name
    // in one id-space would make that origin's inverse ambiguous, which no
    // amount of origin-dispatch can repair.
    for (int i = 0; i < poolCount; i++) {
        for (int j = i + 1; j < poolCount; j++) {
            FI_ASSERT(strcmp(pool[i].name, pool[j].name) != 0);
            FI_ASSERT(pool[i].item.id != pool[j].item.id);
        }
    }
    // Round-trip through the origin-keyed inverse, and confirm the OoT pool is
    // NOT reachable by asking for MM's id-space.
    for (int i = 0; i < poolCount; i++) {
        SharedItem back;
        FI_ASSERT(Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, pool[i].name, &back));
        FI_ASSERT(back.originGame == pool[i].item.originGame && back.id == pool[i].item.id);
        FI_ASSERT(!Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, pool[i].name, NULL));
        FI_ASSERT(!Combo_GetForeignItemByNameFor((uint8_t)GAME_NONE, pool[i].name, NULL));
    }

    // ------------------------------------------------------------------
    // ADR 0009 decision 3: (origin, name) is the key; bare name is NOT.
    // ------------------------------------------------------------------
    // The reason the lookups take an origin at all. "Bomb Bag" is a real
    // display name in BOTH id-spaces — OoT's RG_BOMB_BAG row and MM's
    // RI_BOMB_BAG_20 row — so a name-only inverse resolves it to whichever
    // pool it happens to scan first and writes a WRONG ORIGIN TAG into the
    // placement table. That is the #356 aliasing class arriving through the
    // spoiler-LOAD path, which rebuilds state from untrusted text on disk.
    //
    // #493's issue text asks for a "no cross-pool name collision" assertion.
    // That assertion is NOT satisfiable and is deliberately not written here:
    // it would go red the moment a real MM pool exists, and the natural fix —
    // renaming an item away from its real name — would degrade the spoiler to
    // work around a lookup bug. We assert the collision is HANDLED instead.
    //
    // A synthetic MM pool stands in until the real one lands (Lane 6 creates
    // 2s2h/Rando/ForeignItemsSingleExe.cpp; Lane 1 fills kForeignPoolMMV1 into
    // it). When it does, switch this block to the real pool and drop the
    // registry restore below.
    {
        static const ComboForeignItemDef kSyntheticMMPool[] = {
            { { (uint8_t)GAME_MM, 0, 0x0037 }, "Bomb Bag" },   // deliberately collides with the OoT pool
            { { (uint8_t)GAME_MM, 0, 0x0041 }, "Hero's Bow" },
        };
        FI_ASSERT(Combo_GetForeignItemPoolFor((uint8_t)GAME_MM, NULL) == 0); // nothing registered yet
        Combo_RegisterForeignItemPool((uint8_t)GAME_MM, kSyntheticMMPool, 2);

        const ComboForeignItemDef* mmPool = NULL;
        FI_ASSERT(Combo_GetForeignItemPoolFor((uint8_t)GAME_MM, &mmPool) == 2 && mmPool == kSyntheticMMPool);

        // The colliding bare name resolves to a DIFFERENT item under each
        // origin — never to the same one, and never to nothing.
        SharedItem fromOoT, fromMM;
        FI_ASSERT(Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, "Bomb Bag", &fromOoT));
        FI_ASSERT(Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, "Bomb Bag", &fromMM));
        FI_ASSERT(fromOoT.originGame == (uint8_t)GAME_OOT);
        FI_ASSERT(fromMM.originGame == (uint8_t)GAME_MM);
        FI_ASSERT(fromMM.id == 0x0037);
        FI_ASSERT(fromOoT.id != fromMM.id || fromOoT.originGame != fromMM.originGame);

        // The legacy bare-name entry point keeps its exact previous meaning:
        // the OoT pool. Call sites that predate the origin dimension must not
        // have silently changed behavior when the MM pool appeared.
        SharedItem legacy;
        FI_ASSERT(Combo_GetForeignItemByName("Bomb Bag", &legacy));
        FI_ASSERT(legacy.originGame == (uint8_t)GAME_OOT && legacy.id == fromOoT.id);

        // Forward direction dispatches on the item's own tag: two items with
        // the SAME id in different id-spaces must not resolve to one name.
        SharedItem mmBow;
        mmBow.originGame = (uint8_t)GAME_MM;
        mmBow.flags = 0;
        mmBow.id = 0x0041;
        FI_ASSERT(Combo_GetForeignItemName(mmBow) != NULL);
        FI_ASSERT(strcmp(Combo_GetForeignItemName(mmBow), "Hero's Bow") == 0);
        SharedItem ootSameId = mmBow;
        ootSameId.originGame = (uint8_t)GAME_OOT;
        // Same raw id, OoT id-space: must not borrow MM's name. (It may be a
        // real OoT pool entry with its OWN name, or nothing; either is correct,
        // borrowing MM's is not.)
        const char* ootName = Combo_GetForeignItemName(ootSameId);
        FI_ASSERT(ootName == NULL || strcmp(ootName, "Hero's Bow") != 0);

        // An untagged item resolves to no pool and therefore to no name.
        SharedItem untaggedName;
        untaggedName.originGame = (uint8_t)GAME_NONE;
        untaggedName.flags = 0;
        untaggedName.id = 0x0037;
        FI_ASSERT(Combo_GetForeignItemName(untaggedName) == NULL);

        Combo_RegisterForeignItemPool((uint8_t)GAME_MM, NULL, 0); // restore process-global state
        FI_ASSERT(Combo_GetForeignItemPoolFor((uint8_t)GAME_MM, NULL) == 0);
    }

    // ------------------------------------------------------------------
    // Clean slate + the pairing gate (Lane B's carrier contract).
    // ------------------------------------------------------------------
    ComboContext_Init();
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    FI_ASSERT(!Combo_ForeignPairingActive()); // zero-extended state: no pairing
    gComboCtx.sourceIsRando = true;
    FI_ASSERT(!Combo_ForeignPairingActive()); // seed without settings digest: still no pairing
    gComboCtx.sharedRandoSeed = 0xC0FFEE01u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED5A5Au;
    FI_ASSERT(Combo_ForeignPairingActive());

    // ------------------------------------------------------------------
    // Placement-table accessors reject the malformed and de-dup by check.
    // ------------------------------------------------------------------
    SharedItem untagged;
    untagged.originGame = (uint8_t)GAME_NONE;
    untagged.flags = 0;
    untagged.id = 7;
    FI_ASSERT(Combo_SetForeignPlacement(kForeignTestCheckA, untagged) < 0); // untagged item rejected
    FI_ASSERT(Combo_SetForeignPlacement(0, pool[0].item) < 0);              // RC_UNKNOWN rejected
    FI_ASSERT(Combo_SetForeignPlacement(kForeignTestCheckA, pool[0].item) >= 0);
    FI_ASSERT(Combo_SetForeignPlacement(kForeignTestCheckA, pool[0].item) < 0); // duplicate check rejected
    if (poolCount > 1) {
        FI_ASSERT(Combo_SetForeignPlacement(kForeignTestCheckB, pool[1].item) >= 0);
    }
    FI_ASSERT(Combo_CountForeignPlacements() == (poolCount > 1 ? 2 : 1));
    const SharedItem* hosted = Combo_GetForeignPlacementForCheck(kForeignTestCheckA);
    FI_ASSERT(hosted != NULL);
    FI_ASSERT(hosted->originGame == (uint8_t)GAME_OOT && hosted->id == pool[0].item.id);
    FI_ASSERT(Combo_GetForeignPlacementForCheck(0x0999) == NULL);

    // ------------------------------------------------------------------
    // (a) The REAL give-path core records the foreign item, tagged; a
    //     re-fired give de-dups.
    // ------------------------------------------------------------------
    FI_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true) == 0);
    FI_ASSERT(MM_Rando_Foreign_RecordPickup(0x0999) == 0); // not a foreign check: nothing recorded
    FI_ASSERT(MM_Rando_Foreign_RecordPickup(kForeignTestCheckA) == 1);
    FI_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 1);
    FI_ASSERT(gComboCtx.sharedItemsTagged[0].originGame == (uint8_t)GAME_OOT);
    FI_ASSERT(gComboCtx.sharedItemsTagged[0].id == pool[0].item.id);
    FI_ASSERT(gComboCtx.sharedItemsTagged[0].flags == 0);
    FI_ASSERT(MM_Rando_Foreign_RecordPickup(kForeignTestCheckA) == 1); // de-dup: same slot, no double
    FI_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 1);

    // ------------------------------------------------------------------
    // (b) The crossing survives MM suspend -> OoT arrival through the real
    //     hooks and awards exactly once (RSBS_SHARED_ITEM_REDEEMED).
    // ------------------------------------------------------------------
    uint8_t mmSave[256];
    uint8_t scratch[256];
    memset(mmSave, 0xB2, sizeof(mmSave));
    FI_ASSERT(Switch_PrepareHotSwap(GAME_MM, mmSave, sizeof(mmSave)) == 1);
    FI_ASSERT(Combo_CommitStagedSharedItems() == 0); // give recorded directly; outbox stays empty

    memset(scratch, 0x00, sizeof(scratch));
    Combo_ConsumeFrozenState("oot", scratch, sizeof(scratch)); // first OoT arrival: no frozen OoT state
    ForeignAwardCtx award;
    memset(&award, 0, sizeof(award));
    FI_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, ForeignTestAward, &award) == 1);
    FI_ASSERT(award.awardCount == 1);
    FI_ASSERT(award.lastOrigin == (uint8_t)GAME_OOT);
    FI_ASSERT(award.lastId == pool[0].item.id);
    // Redeemed but still present — the durable record of the crossing.
    FI_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 0);
    FI_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true) == 1);
    // Single-use: a second arrival awards nothing.
    memset(&award, 0, sizeof(award));
    FI_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, ForeignTestAward, &award) == 0);
    FI_ASSERT(award.awardCount == 0);
    // The PLACEMENT survives redemption: the world still hosts the item (the
    // spoiler stays truthful); only the crossing is marked done.
    FI_ASSERT(Combo_GetForeignPlacementForCheck(kForeignTestCheckA) != NULL);

    // ------------------------------------------------------------------
    // Carve serialization: foreignPlacements round-trips byte-exact through
    // a .redsave; unset slots stay unset.
    // ------------------------------------------------------------------
    ComboForeignPlacement expected[RSBS_FOREIGN_PLACEMENT_CAP];
    memcpy(expected, gComboCtx.foreignPlacements, sizeof(expected));

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kForeignSaveDir);
    mgr.DeleteSave(0);
    FI_ASSERT(mgr.Save(0));

    ComboContext_Init(); // wipe live state...
    memset(gComboCtx.foreignPlacements, 0x5A, sizeof(gComboCtx.foreignPlacements)); // ...then scribble
    FI_ASSERT(mgr.Load(0));
    FI_ASSERT(memcmp(expected, gComboCtx.foreignPlacements, sizeof(expected)) == 0);
    // Typed spot-checks so a memcmp-passing-but-misread layout fails loudly.
    FI_ASSERT(gComboCtx.foreignPlacements[0].mmCheckId == kForeignTestCheckA);
    FI_ASSERT(gComboCtx.foreignPlacements[0].item.originGame == (uint8_t)GAME_OOT);
    FI_ASSERT(gComboCtx.foreignPlacements[0].item.id == pool[0].item.id);
    const int lastSlot = (int)RSBS_FOREIGN_PLACEMENT_CAP - 1;
    FI_ASSERT(gComboCtx.foreignPlacements[lastSlot].mmCheckId == 0 &&
              gComboCtx.foreignPlacements[lastSlot].item.originGame == (uint8_t)GAME_NONE);
    // The pairing key round-tripped with it (it rides the same record).
    FI_ASSERT(Combo_ForeignPairingActive());
    mgr.DeleteSave(0);

    // Leave global state clean for any subsequent test.
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();

    printf("[TEST] PASS: foreign give path tags + de-dups; crossing awards once; placements serialize\n");
    return TEST_PASS;
}
