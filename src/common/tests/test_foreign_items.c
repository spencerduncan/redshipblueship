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
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// The C-linkage pipeline pieces this test drives (declared locally like
// test_shared_state_roundtrip.c does for the switch policy).
extern "C" {
int MM_Rando_Foreign_RecordPickup(uint16_t randoCheckId);
int Switch_PrepareHotSwap(GameId departing, const void* saveContext, size_t size);
int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);

// #488 host-eligibility lock. The first is the REAL selection predicate
// PlaceForeignItems' candidate loop calls — driving it here is what makes this
// lock a test of selection rather than of a paraphrase. The rest are the
// inspection/stamping accessors that let a src/common test build a synthetic
// save over MM's real check table (Rando/Foreign.cpp's bridge block).
int MM_Rando_Foreign_IsEligibleHost(uint16_t randoCheckId);
int MM_Rando_Foreign_TestCheckIdMax(void);
int MM_Rando_Foreign_TestCheckClass(uint16_t randoCheckId, int* outIsChestType, int* outHasChestFlag);
void MM_Rando_Foreign_TestStampCheck(uint16_t randoCheckId, int shuffled, int skipped, uint16_t itemId);
void MM_Rando_Foreign_TestStampAllChecks(int shuffled, int skipped, uint16_t itemId);
void MM_Rando_Foreign_TestItemSentinels(uint16_t* outJunk, uint16_t* outNone, uint16_t* outUnknown);

// #510 reverse-pool bridges (games/mm/2s2h/Rando/ForeignItemsSingleExe.cpp).
// The first is the REAL id predicate MM_ForeignItem_Give gates on; the second
// reports an item's class straight out of MM's own table. Both are read from MM
// rather than re-derived here, so a pool row whose classification changes
// upstream moves this lock with it instead of leaving it asserting a stale copy.
int MM_ForeignItem_TestIsGiveableId(uint16_t riId);
int MM_ForeignItem_TestIsJunkClassId(uint16_t riId);

// #495 criterion-attribution bridges: each pool TU's table of ids that were
// CONSIDERED and rejected, with the criterion number that rejected them. This
// TU has neither game's enum in scope by design, so it cannot name RI_TRAP or
// RG_FAIRY_BOW itself — it walks these instead, which is also what keeps the
// lock testing the real tables rather than a second copy of them.
int MM_ForeignItem_TestExclusionAt(int index, uint16_t* outId, uint8_t* outCriterion);
int OoT_ForeignItem_TestExclusionAt(int index, uint16_t* outId, uint8_t* outCriterion);

// #510 OoT-side host predicate (games/oot/soh/Enhancements/randomizer/
// ForeignItemsSingleExe.cpp) — the SAME function OoT_PlaceForeignItems' candidate
// loop calls. Its fill-side half reads GetPlacedRandomizerGet(), so it accepts
// nothing until a real generation has run: see Test_ForeignPlacementOoT, which
// lives in the display-requiring `rando` tier for exactly that reason.
int OoT_Foreign_IsEligibleHost(uint16_t rc);

// #493 REVERSE-DIRECTION PRODUCTION CHAIN. Every symbol below is the real
// shipping one; there is no stand-in anywhere in this list, which is the whole
// point of the row that drives them:
//   OoT_Rando_Foreign_RecordPickup  the give-path core the RC-queue drain calls
//                                   (soh/.../ForeignItemsSingleExe.cpp)
//   MM_ConsumeSharedItems           MM's real consumer hook (the exact function
//                                   z_play.c's startup-entrance consumption calls)
//   MM_ForeignItem_TestPending*     the observable for "MM's real award reached
//                                   the real give" — with no PlayState the give
//                                   DEFERS into this queue (#502) rather than
//                                   dereferencing, which is exactly the state
//                                   MM's arrival point is in.
int OoT_Rando_Foreign_RecordPickup(uint16_t rc);
void MM_ConsumeSharedItems(void);
int MM_ForeignItem_TestPendingCount(void);
uint16_t MM_ForeignItem_TestPendingAt(int index);
void MM_ForeignItem_TestResetPending(void);
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

// Write a slot file whose Tier-1 record is truncated to `comboSize`, taking the
// bytes from the front of the CURRENT gComboCtx. That is byte-for-byte what an
// older build wrote, as long as fields are only ever appended — the growth
// contract. Local rather than shared with test_save_roundtrip.c's equivalent so
// this file does not reach into another test's internals; the OoT/MM tiers are
// zero-filled because nothing here reads them.
bool ForeignTestWriteShortRecord(const std::string& path, uint32_t comboSize) {
    std::vector<uint8_t> payload;
    const uint8_t* comboBytes = reinterpret_cast<const uint8_t*>(&gComboCtx);
    payload.insert(payload.end(), comboBytes, comboBytes + comboSize);
    payload.insert(payload.end(), OOT_SAVE_CONTEXT_SIZE, 0u);
    payload.insert(payload.end(), MM_SAVE_CONTEXT_SIZE, 0u);

    rsbs::RsbsSaveHeader h;
    memset(&h, 0, sizeof(h));
    memcpy(h.magic, RSBS_SAVE_MAGIC, sizeof(h.magic));
    h.version = RSBS_SAVE_VERSION_MIN;
    h.endian = RSBS_SAVE_ENDIAN_LE;
    h.slot = 0;
    h.headerSize = sizeof(rsbs::RsbsSaveHeader);
    h.comboSize = comboSize;
    h.ootSize = (uint32_t)OOT_SAVE_CONTEXT_SIZE;
    h.mmSize = (uint32_t)MM_SAVE_CONTEXT_SIZE;
    h.crc32 = rsbs::SaveManager::Crc32(payload.data(), payload.size());

    std::filesystem::create_directories(kForeignSaveDir);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(&h), sizeof(h));
    out.write(reinterpret_cast<const char*>(payload.data()), (std::streamsize)payload.size());
    return (bool)out;
}
} // namespace

TestResult Test_ForeignItemGive(void) {
    printf("[TEST] foreign-item-give: give-path core tags the shared structure; crossing survives + awards once "
           "(Lane C1)\n");

    // ------------------------------------------------------------------
    // Pool sanity: pinned, OoT-tagged, named, lookup round-trips.
    // ------------------------------------------------------------------
    //
    // #495'S PRIMARY LOCK LIVES HERE, AND IT IS RE-AIMED (ADR 0011 decision
    // 3.2). The issue asked for "a different sharedRandoSeed must produce a
    // different pool — otherwise the 'rule' is a constant wearing a rule's
    // clothes". That assertion MUST NOT BE WRITTEN: it is satisfiable only by a
    // seed-varying class, which makes Combo_GetForeignItemByNameFor PARTIAL on
    // the spoiler-LOAD path (a name that was in the pool at generation is absent
    // at load, in a process that never generated). A lock that cannot pass gets
    // "fixed" by weakening it, which is why the correction is recorded in an ADR
    // rather than in a review comment.
    //
    // Re-aimed at the two observables that DO matter, and both are asserted —
    // "a different seed must produce different PLACEMENTS" by SeedDeterminism's
    // foreignOoTHash fold and MMRandoGen's digest, and "a different itemClass*
    // bitset must produce a different derived pool" by the ForeignItemClass row
    // below, which also carries the parity pin this one leaves implicit.
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPool(&pool);
    FI_ASSERT(pool != NULL);
    FI_ASSERT(poolCount >= 1 && poolCount <= (int)RSBS_FOREIGN_PLACEMENT_CAP);
    // The pool this row goes on to drive is the DRAWN pool under the shipped
    // rules: with every class armed the draw is the whole table, so everything
    // below is testing what a created world actually places. Written against
    // the explicit v1 union rather than the resolved mask because this block
    // runs BEFORE the clean-slate ComboContext_Init below, and `--test all`
    // shares one process — the claim is about the TABLE, not about whatever the
    // previous row left frozen.
    FI_ASSERT(Combo_ForeignPoolClassMembersFor((uint8_t)GAME_OOT, (uint16_t)RSBS_ITEMCLASS_ALL_V1, NULL, 0) ==
              poolCount);
    for (int i = 0; i < poolCount; i++) {
        FI_ASSERT(pool[i].item.originGame == (uint8_t)GAME_OOT);
        FI_ASSERT(pool[i].item.id != 0);
        FI_ASSERT(pool[i].item.flags == 0);
        FI_ASSERT(pool[i].name != NULL && pool[i].name[0] != '\0');
        FI_ASSERT(Combo_GetForeignItemName(pool[i].item) == pool[i].name);
        // #510: MM's pickup textbox reads "You found " + article + name, and MM
        // cannot look up OoT's item table for the article — so it rides here.
        FI_ASSERT(pool[i].article != NULL);
        FI_ASSERT(Combo_GetForeignItemArticle(pool[i].item) == pool[i].article);
        if (pool[i].article[0] != '\0') {
            FI_ASSERT(pool[i].article[strlen(pool[i].article) - 1] == ' ');
        }
        // #494: the arrival-toast icon accessor serves the pool's own iconName
        // pointer back verbatim (identity, not a copy), origin-keyed like the
        // name/article lookups. Every OoT pool entry carries an ITEM_* key, so
        // none is NULL here — but a NULL entry would be a text-only toast, not a
        // defect, so the contract asserted is "serves the column exactly", not
        // "always non-NULL".
        FI_ASSERT(Combo_GetForeignItemIconName(pool[i].item) == pool[i].iconName);
        FI_ASSERT(pool[i].iconName != NULL && pool[i].iconName[0] == 'I'); // ITEM_* texture-map key
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
        // NOT "the name must not exist in MM's pool": since #510 a real MM pool
        // is registered and "Lens of Truth" is a genuine row in BOTH id-spaces, so
        // that assertion would be false-by-design. The invariant that actually
        // matters is that the two id-spaces never bleed: if the name resolves
        // under GAME_MM at all, it comes back MM-tagged and is a DIFFERENT item
        // from the OoT row of the same name.
        SharedItem mmSide;
        if (Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, pool[i].name, &mmSide)) {
            FI_ASSERT(mmSide.originGame == (uint8_t)GAME_MM);
            FI_ASSERT(mmSide.originGame != back.originGame);
        }
        FI_ASSERT(!Combo_GetForeignItemByNameFor((uint8_t)GAME_NONE, pool[i].name, NULL));
    }

    // ------------------------------------------------------------------
    // ADR 0009 decision 3: (origin, name) is the key; bare name is NOT.
    // ------------------------------------------------------------------
    // The reason the lookups take an origin at all. "Lens of Truth" is a real
    // display name in BOTH id-spaces — OoT's RG_LENS_OF_TRUTH row and MM's
    // RI_LENS row — so a name-only inverse resolves it to whichever
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
    // Driven against the REAL MM pool since #510 (kForeignPoolMMV1). The
    // synthetic stand-in that used to live here — and its
    // Combo_RegisterForeignItemPool / un-register pair — is GONE, deliberately:
    // registering over the real pool clobbers process-global state, and the
    // un-register left GAME_MM with NO pool for every later row in
    // `--test all`. The collision is real now, so it is tested for real.
    {
        const ComboForeignItemDef* mmPool = NULL;
        const int mmPoolCount = Combo_GetForeignItemPoolFor((uint8_t)GAME_MM, &mmPool);
        FI_ASSERT(mmPoolCount >= 1 && mmPool != NULL); // MM's file-scope registrar ran

        // The collision this whole surface exists for must ACTUALLY be present in
        // the two real pools, or everything below passes for want of a conflict.
        //
        // The colliding pair is "Lens of Truth" (OoT's RG_LENS_OF_TRUTH row
        // against MM's RI_LENS row). It was "Bomb Bag" until shared ammo (#525)
        // made the bomb-bag capacity one cross-game quantity and criterion 6
        // retired BOTH halves of that pair at once — which is exactly why the
        // OoT Lens row and these assertions landed in the same commit as the
        // deletions. Renaming a row to dodge a collision is never the fix: the
        // display name is the spoiler-load persistence key.
        int ootCollisionIdx = -1;
        for (int k = 0; k < poolCount; k++) {
            if (strcmp(pool[k].name, "Lens of Truth") == 0) {
                ootCollisionIdx = k;
            }
        }
        int mmCollisionIdx = -1;
        for (int k = 0; k < mmPoolCount; k++) {
            if (strcmp(mmPool[k].name, "Lens of Truth") == 0) {
                mmCollisionIdx = k;
            }
        }
        FI_ASSERT(ootCollisionIdx >= 0); // OoT's RG_LENS_OF_TRUTH row
        FI_ASSERT(mmCollisionIdx >= 0);  // MM's RI_LENS row

        // The colliding bare name resolves to a DIFFERENT item under each
        // origin — never to the same one, and never to nothing.
        SharedItem fromOoT, fromMM;
        FI_ASSERT(Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, "Lens of Truth", &fromOoT));
        FI_ASSERT(Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, "Lens of Truth", &fromMM));
        FI_ASSERT(fromOoT.originGame == (uint8_t)GAME_OOT);
        FI_ASSERT(fromMM.originGame == (uint8_t)GAME_MM);
        // Pin BOTH sides to their real pool rows rather than asserting the two
        // results merely differ. Comparing (id, origin) pairs would be
        // tautological — the origins are already asserted distinct just above —
        // and would still pass if a lookup returned the wrong row of its own pool.
        FI_ASSERT(fromOoT.id == pool[ootCollisionIdx].item.id);
        FI_ASSERT(fromMM.id == mmPool[mmCollisionIdx].item.id);

        // The legacy bare-name entry point keeps its exact previous meaning:
        // the OoT pool. Call sites that predate the origin dimension must not
        // have silently changed behavior when the MM pool appeared.
        SharedItem legacy;
        FI_ASSERT(Combo_GetForeignItemByName("Lens of Truth", &legacy));
        FI_ASSERT(legacy.originGame == (uint8_t)GAME_OOT && legacy.id == fromOoT.id);

        // Forward direction dispatches on the item's own tag: two items with the
        // SAME raw id in different id-spaces must not resolve to one name.
        const ComboForeignItemDef& mmProbe = mmPool[0];
        FI_ASSERT(Combo_GetForeignItemName(mmProbe.item) != NULL);
        FI_ASSERT(strcmp(Combo_GetForeignItemName(mmProbe.item), mmProbe.name) == 0);
        SharedItem ootSameId = mmProbe.item;
        ootSameId.originGame = (uint8_t)GAME_OOT;
        // Same raw id, OoT id-space: must not borrow MM's name. (It may be a
        // real OoT pool entry with its OWN name, or nothing; either is correct,
        // borrowing MM's is not.)
        const char* ootName = Combo_GetForeignItemName(ootSameId);
        FI_ASSERT(ootName == NULL || strcmp(ootName, mmProbe.name) != 0);

        // An untagged item resolves to no pool and therefore to no name — and,
        // by the same walk, to no icon (#494).
        SharedItem untaggedName;
        untaggedName.originGame = (uint8_t)GAME_NONE;
        untaggedName.flags = 0;
        untaggedName.id = mmProbe.item.id;
        FI_ASSERT(Combo_GetForeignItemName(untaggedName) == NULL);
        FI_ASSERT(Combo_GetForeignItemIconName(untaggedName) == NULL);
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

TestResult Test_ForeignItemGiveReverse(void) {
    printf("[TEST] foreign-item-give-reverse: OoT-hosted placement carve is a separate key space, serializes, "
           "and redeems once (#493)\n");

    // The reverse twin of Test_ForeignItemGive. What it locks is the half of
    // #493 that is NOT a mirror: a SECOND placement table, keyed by an OoT
    // RandomizerCheck, sharing a struct whose member is named mmCheckId and
    // sharing a .redsave record with the forward table.
    //
    // SCOPE, RESTATED (#493). This row used to carry a note saying it "does not
    // yet enter the OoT give path" and that MM's real award did not exist. Both
    // halves of that note are now discharged and the row drives the WHOLE
    // reverse chain with no stand-in at any step:
    //
    //   Combo_SetForeignPlacementOoT       the real generation-side accessor
    //     -> OoT_Rando_Foreign_RecordPickup  the real give-path core the
    //                                        RC-queue drain calls
    //       -> Combo_RecordSharedItem        the real durable producer
    //         -> MM_ConsumeSharedItems       MM's real arrival hook
    //           -> MM_AwardSharedItem        MM's real award callback (#507)
    //             -> MM_ForeignItem_Give     the real give entry point
    //
    // plus the REDEEMED latch, and a whole-file commit + reload over the top.
    // The one thing it still does NOT assert is presentation: the pickup toast
    // and the tracker write stay at the hook, in the gameplay tier, because a
    // display-free process cannot honestly claim anything about pixels.

    ComboContext_Init();
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();

    // An MM item, in MM's id-space. Deliberately NOT drawn from the OoT pool:
    // the whole point of the reverse direction is that the hosted item belongs
    // to the other game.
    SharedItem mmItem;
    mmItem.originGame = (uint8_t)GAME_MM;
    mmItem.flags = 0;
    mmItem.id = 0x0037;

    // ------------------------------------------------------------------
    // The reverse accessors: same rejections, same de-dup.
    // ------------------------------------------------------------------
    SharedItem untagged;
    untagged.originGame = (uint8_t)GAME_NONE;
    untagged.flags = 0;
    untagged.id = 7;
    FI_ASSERT(Combo_SetForeignPlacementOoT(kForeignTestCheckA, untagged) < 0); // untagged rejected
    FI_ASSERT(Combo_SetForeignPlacementOoT(0, mmItem) < 0);                    // RC_UNKNOWN rejected
    FI_ASSERT(Combo_CountForeignPlacementsOoT() == 0);

    FI_ASSERT(Combo_SetForeignPlacementOoT(kForeignTestCheckA, mmItem) >= 0);
    FI_ASSERT(Combo_SetForeignPlacementOoT(kForeignTestCheckA, mmItem) < 0); // duplicate check rejected
    FI_ASSERT(Combo_CountForeignPlacementsOoT() == 1);

    const SharedItem* hostedOoT = Combo_GetForeignPlacementForOoTCheck(kForeignTestCheckA);
    FI_ASSERT(hostedOoT != NULL);
    FI_ASSERT(hostedOoT->originGame == (uint8_t)GAME_MM && hostedOoT->id == mmItem.id);

    // ------------------------------------------------------------------
    // THE hazard: two tables, one raw-u16 key space each, and they collide.
    // ------------------------------------------------------------------
    // kForeignTestCheckA is a valid check id in BOTH games' enumerations —
    // that is not a contrived value, it is the normal case, because an OoT
    // RandomizerCheck and an MM RandoCheckId are unrelated enumerations that
    // overlap freely as integers. A single shared table with no host
    // discriminator, or an accessor that consulted both, would answer this
    // lookup with the wrong game's item. RED against exactly that mistake.
    FI_ASSERT(Combo_GetForeignPlacementForCheck(kForeignTestCheckA) == NULL);
    FI_ASSERT(Combo_CountForeignPlacements() == 0);

    // And symmetrically, once the forward table holds the same key.
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPool(&pool);
    FI_ASSERT(poolCount >= 1 && pool != NULL);
    FI_ASSERT(Combo_SetForeignPlacement(kForeignTestCheckA, pool[0].item) >= 0);
    FI_ASSERT(Combo_CountForeignPlacements() == 1);
    FI_ASSERT(Combo_CountForeignPlacementsOoT() == 1);

    const SharedItem* fwd = Combo_GetForeignPlacementForCheck(kForeignTestCheckA);
    const SharedItem* rev = Combo_GetForeignPlacementForOoTCheck(kForeignTestCheckA);
    FI_ASSERT(fwd != NULL && rev != NULL);
    FI_ASSERT(fwd->originGame == (uint8_t)GAME_OOT); // OoT item, hosted in an MM check
    FI_ASSERT(rev->originGame == (uint8_t)GAME_MM);  // MM item, hosted in an OoT check
    FI_ASSERT(fwd != rev);

    // Clearing one direction must not retire the other: they are generated
    // independently, and only session invalidation retires both.
    Combo_ClearForeignPlacements();
    FI_ASSERT(Combo_CountForeignPlacements() == 0);
    FI_ASSERT(Combo_CountForeignPlacementsOoT() == 1);
    FI_ASSERT(Combo_GetForeignPlacementForOoTCheck(kForeignTestCheckA) != NULL);

    FI_ASSERT(Combo_SetForeignPlacement(kForeignTestCheckB, pool[0].item) >= 0);
    Combo_ClearForeignPlacementsOoT();
    FI_ASSERT(Combo_CountForeignPlacementsOoT() == 0);
    FI_ASSERT(Combo_CountForeignPlacements() == 1);
    Combo_ClearForeignPlacements();

    // ------------------------------------------------------------------
    // Carve serialization: the OoT-keyed block round-trips byte-exact.
    // ------------------------------------------------------------------
    FI_ASSERT(Combo_SetForeignPlacementOoT(kForeignTestCheckA, mmItem) >= 0);
    SharedItem mmItemB;
    mmItemB.originGame = (uint8_t)GAME_MM;
    mmItemB.flags = 0;
    mmItemB.id = 0x0041;
    FI_ASSERT(Combo_SetForeignPlacementOoT(kForeignTestCheckB, mmItemB) >= 0);

    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE02u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED5A5Bu;

    ComboForeignPlacement expectedOoT[RSBS_FOREIGN_PLACEMENT_CAP];
    memcpy(expectedOoT, gComboCtx.foreignPlacementsOoT, sizeof(expectedOoT));

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kForeignSaveDir);
    mgr.DeleteSave(0);
    FI_ASSERT(mgr.Save(0));

    ComboContext_Init();
    memset(gComboCtx.foreignPlacementsOoT, 0x5A, sizeof(gComboCtx.foreignPlacementsOoT));
    FI_ASSERT(mgr.Load(0));
    FI_ASSERT(memcmp(expectedOoT, gComboCtx.foreignPlacementsOoT, sizeof(expectedOoT)) == 0);
    // Typed spot-checks, so a memcmp-passing-but-misread layout fails loudly.
    FI_ASSERT(gComboCtx.foreignPlacementsOoT[0].mmCheckId == kForeignTestCheckA);
    FI_ASSERT(gComboCtx.foreignPlacementsOoT[0].item.originGame == (uint8_t)GAME_MM);
    FI_ASSERT(gComboCtx.foreignPlacementsOoT[0].item.id == mmItem.id);
    FI_ASSERT(gComboCtx.foreignPlacementsOoT[1].mmCheckId == kForeignTestCheckB);
    FI_ASSERT(gComboCtx.foreignPlacementsOoT[1].item.id == mmItemB.id);
    // Unset slots read unset (growth contract: zero means absent).
    const int lastSlot = (int)RSBS_FOREIGN_PLACEMENT_CAP - 1;
    FI_ASSERT(gComboCtx.foreignPlacementsOoT[lastSlot].mmCheckId == 0 &&
              gComboCtx.foreignPlacementsOoT[lastSlot].item.originGame == (uint8_t)GAME_NONE &&
              gComboCtx.foreignPlacementsOoT[lastSlot].item.flags == 0 &&
              gComboCtx.foreignPlacementsOoT[lastSlot].item.id == 0);
    // The forward table did not acquire the reverse table's rows in transit.
    FI_ASSERT(Combo_CountForeignPlacements() == 0);
    mgr.DeleteSave(0);

    // ------------------------------------------------------------------
    // A pre-3.1 record reads the new block as all-unset.
    // ------------------------------------------------------------------
    // The growth contract's zero-extension, applied to this carve specifically:
    // a .redsave written before foreignPlacementsOoT existed is shorter than
    // the current struct, and Load stages it into a zero-filled buffer. Every
    // slot must therefore read absent rather than as whatever the live struct
    // held. Scribbled first so a pass cannot come from leftover zeros.
    ComboContext_Init();
    gComboCtx.saveSlot = 0x0BADF00D;
    FI_ASSERT(ForeignTestWriteShortRecord(mgr.SlotPath(0), RSBS_COMBO_CONTEXT_PRECARVE_SIZE));

    ComboContext_Init();
    memset(gComboCtx.foreignPlacementsOoT, 0x5A, sizeof(gComboCtx.foreignPlacementsOoT));
    FI_ASSERT(mgr.Load(0));
    FI_ASSERT(gComboCtx.saveSlot == 0x0BADF00D); // the legacy prefix really did load
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        FI_ASSERT(gComboCtx.foreignPlacementsOoT[i].mmCheckId == 0);
        FI_ASSERT(gComboCtx.foreignPlacementsOoT[i].item.originGame == (uint8_t)GAME_NONE);
        FI_ASSERT(gComboCtx.foreignPlacementsOoT[i].item.flags == 0);
        FI_ASSERT(gComboCtx.foreignPlacementsOoT[i].item.id == 0);
    }
    FI_ASSERT(Combo_CountForeignPlacementsOoT() == 0);
    mgr.DeleteSave(0);

    // ------------------------------------------------------------------
    // The redeem walk: an MM-origin crossing awards exactly once, to MM.
    // ------------------------------------------------------------------
    // The real Combo_RedeemSharedItemsForGame with an INSTRUMENTED award
    // callback. It is kept alongside the real-award chain below rather than
    // replaced by it, because it observes something the pending queue cannot:
    // WHICH game an entry was offered to. The real MM award simply ignores an
    // OoT-origin entry, so "not awarded to OoT" and "MM declined it" are
    // indistinguishable downstream; here they are not.
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    FI_ASSERT(Combo_RecordSharedItem(GAME_MM, mmItem.id) >= 0);

    ForeignAwardCtx award;
    memset(&award, 0, sizeof(award));
    // Wrong game first: an MM-origin entry must not be awarded to OoT.
    FI_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, ForeignTestAward, &award) == 0);
    FI_ASSERT(award.awardCount == 0);

    FI_ASSERT(Combo_RedeemSharedItemsForGame(GAME_MM, ForeignTestAward, &award) == 1);
    FI_ASSERT(award.awardCount == 1);
    FI_ASSERT(award.lastOrigin == (uint8_t)GAME_MM);
    FI_ASSERT(award.lastId == mmItem.id);
    FI_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 0);
    FI_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == 1);

    memset(&award, 0, sizeof(award));
    FI_ASSERT(Combo_RedeemSharedItemsForGame(GAME_MM, ForeignTestAward, &award) == 0);
    FI_ASSERT(award.awardCount == 0);

    // ==================================================================
    // THE PRODUCTION CHAIN (#493). No stand-in at any step.
    // ==================================================================
    // Everything above drives ACCESSORS. This drives the give: the same
    // function the RC-queue drain calls, into the same durable producer, into
    // MM's real arrival hook, into MM's real award, into MM's real give. A row
    // that reached Combo_RecordSharedItem directly would assert that the
    // shared-item machinery works — which SharedItemRoundtrip already asserts —
    // and would say nothing at all about the reverse direction.
    ComboContext_Init();
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();
    FI_ASSERT(MM_ForeignItem_TestPendingCount() == 0);

    // A REAL MM pool entry: the reverse pass can only ever place one of these,
    // and MM's give only accepts ids its own table knows. An arbitrary u16
    // would be refused downstream and the chain would look broken for the wrong
    // reason.
    const ComboForeignItemDef* mmPool = NULL;
    const int mmPoolCount = Combo_GetForeignItemPoolFor((uint8_t)GAME_MM, &mmPool);
    FI_ASSERT(mmPoolCount > 0 && mmPool != NULL);
    const SharedItem placedItem = mmPool[0].item;
    FI_ASSERT(placedItem.originGame == (uint8_t)GAME_MM);

    FI_ASSERT(Combo_SetForeignPlacementOoT(kForeignTestCheckA, placedItem) >= 0);

    // ---- (1) THE #610 REFUSAL, FIRST -----------------------------------
    // Non-vacuity leg, and the bug this lane found: the forward direction has
    // refused to author a crossing for a world that does not exist since #610
    // (Rando::Foreign::RecordForeignPickup), and the reverse direction did not.
    // Combo_RecordSharedItem performs no identity check of its own, so whatever
    // reaches it is redeemed by whichever paired MM world arrives next — blind
    // to which world authored it. RED before the gate landed: the pickup
    // recorded, and an unpaired OoT session minted a crossing.
    FI_ASSERT(!Combo_ForeignPairingActive());
    FI_ASSERT(OoT_Rando_Foreign_RecordPickup(kForeignTestCheckA) == 0);
    FI_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == 0);

    // ---- (2) Paired: the real core records the crossing, MM-tagged -----
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE03u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED5A5Cu;
    FI_ASSERT(Combo_ForeignPairingActive());

    // A check that hosts nothing records nothing — the ordinary case for every
    // OoT check in the world, and the reason the hook falls through to the
    // local give.
    FI_ASSERT(OoT_Rando_Foreign_RecordPickup(kForeignTestCheckB) == 0);
    FI_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == 0);

    FI_ASSERT(OoT_Rando_Foreign_RecordPickup(kForeignTestCheckA) == 1);
    FI_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 1);
    FI_ASSERT(gComboCtx.sharedItemsTagged[0].originGame == (uint8_t)GAME_MM);
    FI_ASSERT(gComboCtx.sharedItemsTagged[0].id == placedItem.id);
    FI_ASSERT(gComboCtx.sharedItemsTagged[0].flags == 0);

    // ---- (3) A re-fired queue de-dups rather than doubling -------------
    FI_ASSERT(OoT_Rando_Foreign_RecordPickup(kForeignTestCheckA) == 1);
    FI_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == 1);

    // ---- (4) The crossing: OoT suspends, MM arrives, MM awards ---------
    // The real switch seam, then MM's real consumer. With no PlayState the give
    // DEFERS into MM's pending queue (#502) — the state MM's arrival point is
    // genuinely in — so the queue is the faithful observable that the id
    // survived the whole path in MM's own id-space.
    {
        static uint8_t ootSave[256];
        static uint8_t mmScratch[256];
        memset(ootSave, 0xA7, sizeof(ootSave));
        FI_ASSERT(Switch_PrepareHotSwap(GAME_OOT, ootSave, sizeof(ootSave)) == 1);
        FI_ASSERT(Combo_CommitStagedSharedItems() == 0); // recorded directly; the outbox stays empty
        memset(mmScratch, 0x00, sizeof(mmScratch));
        Combo_ConsumeFrozenState("mm", mmScratch, sizeof(mmScratch)); // first MM arrival: nothing frozen
    }

    MM_ConsumeSharedItems();
    FI_ASSERT(MM_ForeignItem_TestPendingCount() == 1);
    FI_ASSERT(MM_ForeignItem_TestPendingAt(0) == placedItem.id);
    // The REDEEMED latch: retired, still present (the durable record of the
    // crossing, which is what keeps the spoiler truthful).
    FI_ASSERT((gComboCtx.sharedItemsTagged[0].flags & RSBS_SHARED_ITEM_REDEEMED) != 0);
    FI_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 0);
    FI_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == 1);
    // And the placement survived redemption: the world still hosts the item.
    FI_ASSERT(Combo_GetForeignPlacementForOoTCheck(kForeignTestCheckA) != NULL);

    // ---- (5) Single-use: a second arrival awards nothing ---------------
    MM_ForeignItem_TestResetPending();
    MM_ConsumeSharedItems();
    FI_ASSERT(MM_ForeignItem_TestPendingCount() == 0);

    // ---- (6) A WHOLE-FILE COMMIT + RELOAD carries all three ------------
    // Tier-1 is written as one unit (ADR 0009 decision 4 / #612), so the
    // placement, the REDEEMED crossing and the pairing identity ride the same
    // write. What must NOT happen is the REDEEMED bit going durable while the
    // placement does not (the crossing would re-fire into a world that already
    // has it) or the reverse (a redeemed crossing re-awarded on the next
    // arrival). Scribbled before the load so a pass cannot come from residue.
    {
        rsbs::SaveManager& commitMgr = rsbs::SaveManager::Instance();
        commitMgr.SetSaveDirectory(kForeignSaveDir);
        commitMgr.DeleteSave(0);
        FI_ASSERT(commitMgr.Save(0));

        ComboContext_Init();
        memset(gComboCtx.foreignPlacementsOoT, 0x5A, sizeof(gComboCtx.foreignPlacementsOoT));
        memset(gComboCtx.sharedItemsTagged, 0x5A, sizeof(gComboCtx.sharedItemsTagged));
        FI_ASSERT(commitMgr.Load(0));

        const SharedItem* reloaded = Combo_GetForeignPlacementForOoTCheck(kForeignTestCheckA);
        FI_ASSERT(reloaded != NULL);
        FI_ASSERT(reloaded->originGame == (uint8_t)GAME_MM && reloaded->id == placedItem.id);
        FI_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == 1);
        FI_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 0);
        FI_ASSERT((gComboCtx.sharedItemsTagged[0].flags & RSBS_SHARED_ITEM_REDEEMED) != 0);
        FI_ASSERT(Combo_ForeignPairingActive()); // the identity rode the same record

        // The load-side arrival awards nothing: the latch is what makes the
        // crossing single-use ACROSS a process, not merely within one.
        MM_ForeignItem_TestResetPending();
        MM_ConsumeSharedItems();
        FI_ASSERT(MM_ForeignItem_TestPendingCount() == 0);
        commitMgr.DeleteSave(0);
    }

    // Leave global state clean for any subsequent test.
    MM_ForeignItem_TestResetPending();
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();

    printf("[TEST] PASS: reverse carve is a separate key space, serializes byte-exact, zero-extends; the real "
           "pickup core refuses an unpaired session, records once, and MM's real award redeems it once across a "
           "whole-file commit\n");
    return TEST_PASS;
}

// ============================================================================
// #488: foreign-HOST eligibility.
//
// The give path only reaches a foreign placement from inside
// `if (randoSaveCheck.eligible)` (MM's MiscBehavior/CheckQueue.cpp:39-53), so a
// host whose `.eligible` bit is never armed strands its pinned OoT progression
// item permanently â€” invisible, unwinnable, and indistinguishable in-game from
// an item that was never placed. The old host predicate said nothing about
// arming: it required only the fill-time `.shuffled` bit plus "holds a
// junk-class item", and excluded only shops.
//
// This drives MM_Rando_Foreign_IsEligibleHost â€” the SAME function
// PlaceForeignItems' candidate loop calls, which is the whole reason the
// predicate was extracted â€” over MM's REAL Rando::StaticData::Checks table with
// a synthetic all-shuffled save. It is not a reimplementation of the rule; if
// the rule changes, this moves with it.
//
// The negatives that were GREEN-on-a-bug before #488 are the `.skipped` leg,
// the two sentinel legs (RI_UNKNOWN/RI_NONE are both declared RITYPE_JUNK, so
// a type-only test accepted them), and the whole-table class sweep (the old
// predicate accepted every non-shop check type). The `.shuffled` and
// RC_UNKNOWN legs are NOT new — the old inline predicate tested both — and are
// kept as positive/negative controls rather than as regression evidence.
// ============================================================================
TestResult Test_ForeignHostEligibility(void) {
    printf("[TEST] foreign-host-eligibility: only game-armed check classes can host a foreign item (#488)\n");

    const int checkIdMax = MM_Rando_Foreign_TestCheckIdMax();
    FI_ASSERT(checkIdMax > 1);

    uint16_t riJunk = 0xFFFF;
    uint16_t riNone = 0xFFFF;
    uint16_t riUnknown = 0xFFFF;
    MM_Rando_Foreign_TestItemSentinels(&riJunk, &riNone, &riUnknown);
    // RI_UNKNOWN is enumerator 0 â€” a zero-initialised slot. That it is a
    // DISTINCT value from the legal junk filler is the premise of the sentinel
    // rejection below; assert it rather than assume it.
    FI_ASSERT(riUnknown == 0);
    FI_ASSERT(riJunk != riUnknown && riJunk != riNone);

    // The synthetic save: every check shuffled, not skipped, holding the legal
    // junk filler. Under the OLD predicate this made nearly every row in the
    // table a legal host; under the new one only the armed classes survive.
    MM_Rando_Foreign_TestStampAllChecks(/*shuffled=*/1, /*skipped=*/0, riJunk);

    // ------------------------------------------------------------------
    // Whole-table sweep. Two facts at once: what the predicate accepts, and
    // the static-table invariant Tier A is defined in terms of.
    // ------------------------------------------------------------------
    int acceptedCount = 0;
    int chestRowCount = 0;
    int chestRowsMissingFlag = 0;
    int firstChestId = 0;
    int firstNonChestAcceptedId = 0;
    int firstRejectedNonChestId = 0;
    for (int id = 1; id < checkIdMax; id++) {
        int isChestType = 0;
        int hasChestFlag = 0;
        if (MM_Rando_Foreign_TestCheckClass((uint16_t)id, &isChestType, &hasChestFlag) == 0) {
            continue; // not a real row (RC_UNKNOWN sentinel / gap)
        }
        if (isChestType) {
            chestRowCount++;
            if (!hasChestFlag) {
                chestRowsMissingFlag++;
            }
            if (firstChestId == 0) {
                firstChestId = id;
            }
        } else if (firstRejectedNonChestId == 0) {
            firstRejectedNonChestId = id;
        }

        if (MM_Rando_Foreign_IsEligibleHost((uint16_t)id)) {
            acceptedCount++;
            if (!isChestType && firstNonChestAcceptedId == 0) {
                firstNonChestAcceptedId = id;
            }
        }
    }

    // This number sizes the foreign pool â€” it is the input #495 (the
    // rule-defined pool) needs, so print it whether or not anything fails.
    printf("[TEST] foreign-host-eligibility: %d eligible hosts over %d chest rows (table has %d check ids)\n",
           acceptedCount, chestRowCount, checkIdMax - 1);

    // Tier A ships alone: nothing outside RCTYPE_CHEST may be accepted.
    FI_ASSERT(firstNonChestAcceptedId == 0);
    // ...and the sweep must have actually exercised a non-chest row, or the
    // assertion above passed for want of anything to reject.
    FI_ASSERT(firstRejectedNonChestId != 0);
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost((uint16_t)firstRejectedNonChestId) == 0);

    // Static-table invariant: every chest row carries FLAG_CYCL_SCENE_CHEST.
    // A FLAG_NONE chest row added later would have no vanilla setter and would
    // strand â€” and nothing else in the tree would notice.
    FI_ASSERT(chestRowCount > 0);
    FI_ASSERT(chestRowsMissingFlag == 0);
    // Because the predicate's other conditions are all satisfied by the
    // synthetic save, acceptance must be exactly the chest rows. An inequality
    // here means the class rule and the table have drifted apart.
    FI_ASSERT(acceptedCount == chestRowCount);

    // Supply: the tightened predicate must still be able to host the whole
    // pinned pool. Falling under this is the "stop and report" condition, not
    // a cue to widen the predicate until the number is comfortable.
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPool(&pool);
    FI_ASSERT(poolCount > 0);
    FI_ASSERT(acceptedCount >= poolCount);

    // ------------------------------------------------------------------
    // Per-condition negatives, on a real chest row. The positive control is
    // re-asserted between each one, so a negative can never pass because the
    // host became ineligible for an unrelated reason.
    // ------------------------------------------------------------------
    FI_ASSERT(firstChestId != 0);
    const uint16_t chest = (uint16_t)firstChestId;
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost(chest) == 1);

    // RC_UNKNOWN is never a host, and neither is an out-of-range id.
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost(0) == 0);
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost((uint16_t)checkIdMax) == 0);

    // Defect A: a user-EXCLUDED check is marked `shuffled = true;
    // randoItemId = RI_JUNK; skipped = true` by GeneratePools and kept out of
    // checkPool â€” so under the old predicate it was a top-priority host for a
    // pinned progression item.
    MM_Rando_Foreign_TestStampCheck(chest, /*shuffled=*/1, /*skipped=*/1, riJunk);
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost(chest) == 0);
    MM_Rando_Foreign_TestStampCheck(chest, 1, 0, riJunk);
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost(chest) == 1);

    // Defect B: RI_UNKNOWN (a zero-initialised or unresolvable slot) and
    // RI_NONE ("literally nothing") are both declared RITYPE_JUNK, so a
    // type-only test accepts an item that is not an item.
    MM_Rando_Foreign_TestStampCheck(chest, 1, 0, riUnknown);
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost(chest) == 0);
    MM_Rando_Foreign_TestStampCheck(chest, 1, 0, riNone);
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost(chest) == 0);
    MM_Rando_Foreign_TestStampCheck(chest, 1, 0, riJunk);
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost(chest) == 1);

    // Carried over unchanged from the old predicate: a check outside the fill
    // is not a host. (The non-junk-item rejection is covered by the sentinel
    // legs above and by the whole-table sweep, which stamps only junk.)
    MM_Rando_Foreign_TestStampCheck(chest, /*shuffled=*/0, 0, riJunk);
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost(chest) == 0);
    MM_Rando_Foreign_TestStampCheck(chest, 1, 0, riJunk);
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost(chest) == 1);

    // Leave MM's check table as a fresh save would: nothing shuffled, nothing
    // held. `--test all` runs every row in one process.
    MM_Rando_Foreign_TestStampAllChecks(/*shuffled=*/0, /*skipped=*/0, riUnknown);
    FI_ASSERT(MM_Rando_Foreign_IsEligibleHost(chest) == 0);

    printf("[TEST] PASS: only chest-class hosts with a live, non-skipped, legal-junk slot can host a foreign item\n");
    return TEST_PASS;
}

// ============================================================================
// #510: the reverse direction's SOURCE pool (kForeignPoolMMV1).
//
// The MM twin of the pool-sanity block inside Test_ForeignItemGive, plus the
// two membership rules that pool is authored under. Display-free: the table is a
// static in the WHOLE_ARCHIVE'd 2ship_rando and its registrar runs before main(),
// so this needs no fill, no save and no window.
//
// This row is also the runtime proof that MM's registrar SURVIVED THE LINK. The
// dead-registrar class (#516) is silent at compile and link time — a dropped
// file-scope initializer just leaves the pool empty — and an empty pool would
// make OoT_PlaceForeignItems return -1 and fail every paired generation.
// ============================================================================
TestResult Test_ForeignPoolMM(void) {
    printf("[TEST] foreign-pool-mm: MM's cross-game source pool is registered, well-formed and non-junk (#510)\n");

    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPoolFor((uint8_t)GAME_MM, &pool);
    printf("[TEST] foreign-pool-mm: %d MM source items registered (placement cap is %d per seed)\n", poolCount,
           (int)RSBS_FOREIGN_PLACEMENT_CAP);
    FI_ASSERT(pool != NULL);
    // Deliberately NOT `poolCount <= RSBS_FOREIGN_PLACEMENT_CAP` (the shape the
    // OoT pool's static_assert uses). The cap bounds PLACEMENTS PER SEED, not
    // candidates; this pool is intentionally far larger so the per-seed draw
    // varies. A pool clamped to the cap would be the bug, not the guarantee.
    FI_ASSERT(poolCount >= 1);

    for (int i = 0; i < poolCount; i++) {
        FI_ASSERT(pool[i].item.originGame == (uint8_t)GAME_MM);
        FI_ASSERT(pool[i].item.id != 0); // RI_UNKNOWN is enumerator 0
        FI_ASSERT(pool[i].item.flags == 0);
        FI_ASSERT(pool[i].name != NULL && pool[i].name[0] != '\0');
        FI_ASSERT(Combo_GetForeignItemName(pool[i].item) == pool[i].name);

        // #510 presentation: OoT builds the pickup line as article + name, and
        // it cannot read MM's item table to get the article — so every row must
        // carry one. NULL would print "You found Powder Keg"; the empty string is
        // legal and correct for the several MM items that take no article
        // ("Garo's Mask", "Epona's Song").
        FI_ASSERT(pool[i].article != NULL);
        FI_ASSERT(Combo_GetForeignItemArticle(pool[i].item) == pool[i].article);
        // A non-empty article carries its own trailing space, so callers
        // concatenate with no separator logic. Catches "the" for "the ".
        if (pool[i].article[0] != '\0') {
            FI_ASSERT(pool[i].article[strlen(pool[i].article) - 1] == ' ');
        }

        // Membership rule (1): every entry must be an id MM's own give ACCEPTS.
        // Driven through the real predicate MM_ForeignItem_Give gates on, not a
        // copy of it — an entry it refuses is an item the player is promised in
        // OoT ("it will be awarded there!") and then never receives in Termina.
        FI_ASSERT(MM_ForeignItem_TestIsGiveableId(pool[i].item.id) == 1);

        // Membership rule (2): no junk-class item may be a cross-game SOURCE.
        // Junk is what a foreign HOST degrades to, so crossing it would spend one
        // of at most RSBS_FOREIGN_PLACEMENT_CAP slots on a strictly worse
        // duplicate of what the host already physically holds. -1 would mean the
        // id names no row in MM's table at all.
        FI_ASSERT(MM_ForeignItem_TestIsJunkClassId(pool[i].item.id) == 0);
    }

    // (originGame, name) uniqueness WITHIN the pool, and id uniqueness with it. A
    // duplicate name makes this origin's spoiler-load inverse ambiguous, which no
    // amount of origin dispatch can repair — it is the same defect the (origin,
    // name) key exists to prevent, arriving from inside one pool instead of
    // between two.
    for (int i = 0; i < poolCount; i++) {
        for (int j = i + 1; j < poolCount; j++) {
            FI_ASSERT(strcmp(pool[i].name, pool[j].name) != 0);
            FI_ASSERT(pool[i].item.id != pool[j].item.id);
        }
    }

    // Round-trip through the origin-keyed inverse — the spoiler-LOAD path, which
    // rebuilds a placement table from untrusted text on disk and must never have
    // to fabricate an RI_*.
    for (int i = 0; i < poolCount; i++) {
        SharedItem back;
        FI_ASSERT(Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, pool[i].name, &back));
        FI_ASSERT(back.originGame == (uint8_t)GAME_MM && back.id == pool[i].item.id);
        FI_ASSERT(!Combo_GetForeignItemByNameFor((uint8_t)GAME_NONE, pool[i].name, NULL));
    }

    // Membership rule (6), #525: no SHARED CROSS-GAME RESOURCE may be a
    // crossing. Rupees and hearts are one quantity spanning both games now —
    // one wallet, one health bar (src/common/shared_resources.h) — so a wallet
    // or heart item has nothing left to carry across. Shipping one anyway would
    // hand the player a second copy of a capacity they already have, or a rupee
    // award the next harvest reconciles straight back out.
    //
    // Asserted as an EXCLUSION rather than an exact pool count on purpose: the
    // count is not the invariant and would go stale the moment an unrelated row
    // is added, whereas "these are not eligible to cross" is exactly what
    // criterion 6 says.
    //
    // Keyed on the DISPLAY NAME, not the RI_* id, because this TU is src/common
    // and has no MM headers in scope by design — the same reason the giveable /
    // junk-class checks above go through MM-side bridge predicates. The name is
    // not a weaker key here: it is the pool's own persistence key (the
    // spoiler-LOAD inverse is keyed on (originGame, name)), so a row that
    // answers to one of these names IS the row that must be gone.
    //
    // "Double Defense" is in this list because it lived under core equipment,
    // NOT the health block — a block-shaped delete misses it, and it is the row
    // a reviewer is most likely to miss too.
    static const char* const kSharedResourceNames[] = {
        "Progressive Wallet", "Adult's Wallet",  "Giant's Wallet",
        "Double Defense",     "Heart Container", "Heart Piece",
        // Shared magic (#525's optional tier): one meter across both games, so
        // the three MM magic rows left by the same criterion. Exact display
        // names, spelled from the pool's own rows — a typo here passes
        // vacuously, because the assertion is a not-equal sweep.
        "Progressive Magic",  "Power of Magic",  "Magic Upgrade",
        // Shared ammo: the capacity tiers are one cross-game quantity now, so
        // every bag and quiver row left. The BOW rows are here for a reason
        // that is easy to miss — MM's own bow give sets UPG_QUIVER to 1, so in
        // MM owning the bow IS quiver tier 1, which makes a bow crossing a
        // mutation of the shared resource rather than a new item.
        "Bomb Bag",           "Big Bomb Bag",    "Biggest Bomb Bag",
        "Large Quiver",       "Largest Quiver",  "Progressive Bomb Bag",
        "Bow",                "Progressive Bow",
        // Shared hookshot: one inventory byte in each game, so it crosses as a
        // monotonic tier (0/1/2) rather than as an item handed over once.
        "Hookshot",
    };
    for (size_t k = 0; k < sizeof(kSharedResourceNames) / sizeof(kSharedResourceNames[0]); k++) {
        for (int i = 0; i < poolCount; i++) {
            FI_ASSERT(strcmp(pool[i].name, kSharedResourceNames[k]) != 0);
        }
        // ...and the name-keyed inverse agrees, so a spoiler naming one of these
        // cannot resurrect a crossing the shared-resource model replaced.
        FI_ASSERT(!Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, kSharedResourceNames[k], NULL));
    }

    // THE SAME SWEEP AGAINST OoT's POOL. Criterion 6 had no OoT twin until
    // shared ammo, and that was a real hole rather than an omission worth
    // leaving: OoT's own pool shipped "Fairy Bow" and "Bomb Bag" rows, and both
    // became shared resources here, so nothing was watching the side that
    // actually had to lose entries. Reusing the MM name list is meaningful
    // wherever the two games spell a row identically — "Bomb Bag" collided
    // across the pools for exactly that reason — and inert elsewhere, so the
    // OoT-specific spellings are named separately below.
    const ComboForeignItemDef* ootPool = NULL;
    const int ootPoolCount = Combo_GetForeignItemPoolFor((uint8_t)GAME_OOT, &ootPool);
    FI_ASSERT(ootPoolCount >= 1 && ootPool != NULL);
    static const char* const kOoTSharedResourceNames[] = {
        // Retired from kForeignPoolV1 by criterion 6 when shared ammo landed.
        // Byte-exact as that table spelled them, which is what makes the sweep
        // non-vacuous: these strings were really there.
        "Fairy Bow",
        "Bomb Bag",
        // ...and this one when the hookshot became a shared tier.
        "Progressive Hookshot",
    };
    for (size_t k = 0; k < sizeof(kSharedResourceNames) / sizeof(kSharedResourceNames[0]); k++) {
        for (int i = 0; i < ootPoolCount; i++) {
            FI_ASSERT(strcmp(ootPool[i].name, kSharedResourceNames[k]) != 0);
        }
    }
    for (size_t k = 0; k < sizeof(kOoTSharedResourceNames) / sizeof(kOoTSharedResourceNames[0]); k++) {
        for (int i = 0; i < ootPoolCount; i++) {
            FI_ASSERT(strcmp(ootPool[i].name, kOoTSharedResourceNames[k]) != 0);
        }
        FI_ASSERT(!Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, kOoTSharedResourceNames[k], NULL));
    }

    // The shrink is real, not a rename: eighteen rows have left across the
    // three tiers and nothing took their place. Bounded rather than exact for
    // the reason above — a future shared resource removes more, and unrelated
    // work may add.
    printf("[TEST] foreign-pool-mm: %d entries after the #525 shared-resource shrink\n", poolCount);
    FI_ASSERT(poolCount <= 128);

    // OoT's pool must stay clear of test_combo_spoiler_view.c's floor of 3,
    // which it sat exactly on before shared ammo added the Lens, Boomerang and
    // Megaton Hammer rows. Asserted here rather than left implicit because the
    // ammo tier is what took two rows OUT of a four-row pool.
    printf("[TEST] foreign-pool-oot: %d entries\n", ootPoolCount);
    FI_ASSERT(ootPoolCount >= 3);

    printf("[TEST] PASS: MM source pool registered, well-formed, giveable, non-junk, name-invertible, "
           "no shared resources\n");
    return TEST_PASS;
}

// ============================================================================
// #495: the cross-game item class is a RULE, and the class BITSET is the
// setting (ADR 0011 decision 3; accepted answers O3 and O7).
//
// What replaced what: the pool draw used to be "the literal table, in order",
// so the four pinned OoT rows WERE the whole universe and the bitset carved by
// increment 1 was read but unconsumed. Now every row names one
// RSBS_ITEMCLASS_* bit, the draw is Combo_ForeignPoolDrawFor over the FROZEN
// bitset, and the pinned table is one class's membership.
//
// THE THREE CLAIMS THIS ROW EXISTS FOR, in the order they can fail:
//
//  (P) PARITY. Under the shipped defaults the draw is the identity permutation,
//      so every already-generated world is byte-identical. This is the pin the
//      whole increment is bounded by — SeedDeterminism's foreignOoTHash and
//      MMRandoGen's placement digest both fold the drawn entries, so if this
//      assertion is wrong those rows move.
//
//  (N) NARROWING. A narrowed bitset draws ONLY members of the armed classes.
//      RED before the rule engine: the bitset was stored and compared but no
//      code consumed it, so every mask produced the same full pool.
//
//  (T) TOTALITY. Combo_GetForeignItemByNameFor stays TOTAL over every item ANY
//      class can name, whatever is frozen. This is why the class carries no
//      seed term (O3) and why the FILTER returns indices while the REGISTRY
//      keeps serving the whole pool: the spoiler-LOAD path runs in processes
//      that never generated, and a selection-scoped inverse would make a
//      spoiler that was valid at generation unreadable at load.
//
// Display-free, ROM-free, save-free: both tables are statics in WHOLE_ARCHIVE'd
// libraries whose registrars run before main().
// ============================================================================

namespace {
// Every allocated bit, spelled out rather than reusing RSBS_ITEMCLASS_ALL_V1,
// so a bit ADDED to the union without a matching lock update is a red row here
// instead of a silently widened default.
const uint16_t kAllocatedClassBits[] = {
    (uint16_t)RSBS_ITEMCLASS_PROGRESSION,   (uint16_t)RSBS_ITEMCLASS_SONGS,
    (uint16_t)RSBS_ITEMCLASS_MASKS,         (uint16_t)RSBS_ITEMCLASS_DUNGEON_ITEMS,
    (uint16_t)RSBS_ITEMCLASS_DUNGEON_REWARD, (uint16_t)RSBS_ITEMCLASS_SIDEQUEST,
};
const int kAllocatedClassBitCount = (int)(sizeof(kAllocatedClassBits) / sizeof(kAllocatedClassBits[0]));

int PopCount16(uint16_t v) {
    int n = 0;
    while (v != 0) {
        n += (v & 1u);
        v = (uint16_t)(v >> 1);
    }
    return n;
}
} // namespace

TestResult Test_ForeignItemClass(void) {
    printf("[TEST] foreign-item-class: the frozen class bitset selects the DRAW; the pool and the name inverse stay "
           "whole (#495)\n");

    ComboContext_Init();

    const uint8_t kOrigins[] = { (uint8_t)GAME_OOT, (uint8_t)GAME_MM };

    // ------------------------------------------------------------------
    // (0) The bit table is pinned, and every bit has a name.
    // ------------------------------------------------------------------
    // A renumbering is already a red BUILD (foreign_items.h's static_assert);
    // this is the runtime half — an allocated bit with no name would render as
    // "(unknown)" in every log line and refusal message the rule produces.
    FI_ASSERT((uint16_t)RSBS_ITEMCLASS_ALL_V1 == 0x003Fu);
    uint16_t unionOfBits = 0;
    for (int b = 0; b < kAllocatedClassBitCount; b++) {
        FI_ASSERT(PopCount16(kAllocatedClassBits[b]) == 1);
        FI_ASSERT(strcmp(Combo_ForeignItemClassName(kAllocatedClassBits[b]), "(unknown)") != 0);
        unionOfBits = (uint16_t)(unionOfBits | kAllocatedClassBits[b]);
    }
    FI_ASSERT(unionOfBits == (uint16_t)RSBS_ITEMCLASS_ALL_V1);
    // An UNALLOCATED bit has no name and, below, no members.
    FI_ASSERT(strcmp(Combo_ForeignItemClassName(0x0040u), "(unknown)") == 0);
    // The criteria are named too — an exclusion attributed to a criterion the
    // name table does not know is an exclusion nobody can read.
    for (uint8_t c = (uint8_t)RSBS_FOREIGN_CRIT_REAL_ITEM; c < (uint8_t)RSBS_FOREIGN_CRIT_COUNT; c++) {
        FI_ASSERT(strcmp(Combo_ForeignCriterionName(c), "(unknown)") != 0);
    }
    FI_ASSERT(strcmp(Combo_ForeignCriterionName((uint8_t)RSBS_FOREIGN_CRIT_COUNT), "(unknown)") == 0);

    // ------------------------------------------------------------------
    // (1) EVERY ROW OF BOTH POOLS IS CLASSIFIED, with EXACTLY ONE bit.
    // ------------------------------------------------------------------
    // Zero bits: the row is selected by no mask and would silently leave the
    // pool the moment the rule went live. Two bits: the row is drawn by either
    // class, which makes claim (N) untestable. C cannot express "exactly one
    // allocated bit" in an initializer, so it is asserted here.
    for (int o = 0; o < 2; o++) {
        const ComboForeignItemDef* pool = NULL;
        const int poolCount = Combo_GetForeignItemPoolFor(kOrigins[o], &pool);
        FI_ASSERT(poolCount >= 1 && pool != NULL);
        for (int i = 0; i < poolCount; i++) {
            FI_ASSERT(PopCount16(pool[i].itemClass) == 1);
            FI_ASSERT((pool[i].itemClass & (uint16_t)RSBS_ITEMCLASS_ALL_V1) == pool[i].itemClass);
        }
    }

    // ------------------------------------------------------------------
    // (P) THE PARITY PIN. Default bitset => the identity permutation.
    // ------------------------------------------------------------------
    // Asserted as index-for-index equality with 0..poolCount-1, not merely as
    // an equal COUNT: the forward pass assigns pool[draw[i]] to the i-th drawn
    // host, so a permutation with the right size and the wrong order would
    // re-order every already-generated world's crossings while passing a
    // count check.
    FI_ASSERT(!Combo_ComboSettingsFrozen()); // fresh gComboCtx: the unfrozen fallback path
    for (int o = 0; o < 2; o++) {
        const uint8_t origin = kOrigins[o];
        const ComboForeignItemDef* pool = NULL;
        const int poolCount = Combo_GetForeignItemPoolFor(origin, &pool);

        // The unfrozen fallback is the shipped default, NOT zero. A
        // zero-extended legacy record resolving to "no classes" would silently
        // generate a paired world with no crossings at all.
        FI_ASSERT(Combo_ComboItemClassFor(origin) == (uint16_t)RSBS_ITEMCLASS_ALL_V1);

        std::vector<int> draw((size_t)poolCount, -1);
        const int drawCount = Combo_ForeignPoolDrawFor(origin, draw.data(), poolCount);
        FI_ASSERT(drawCount == poolCount);
        for (int i = 0; i < poolCount; i++) {
            FI_ASSERT(draw[(size_t)i] == i);
        }
        printf("[TEST] foreign-item-class: origin %u parity — draw is the identity permutation over %d rows\n",
               (unsigned)origin, poolCount);
    }

    // The same parity under an explicitly FROZEN default record, because that
    // is the path a created world actually takes (the fallback above is only
    // for legacy/pre-freeze files).
    {
        ComboSettingsRecord defaults;
        Combo_ComboSettingsDefaults(&defaults);
        gComboCtx.comboSettings = defaults;
        FI_ASSERT(Combo_ComboSettingsFrozen());
        for (int o = 0; o < 2; o++) {
            const ComboForeignItemDef* pool = NULL;
            const int poolCount = Combo_GetForeignItemPoolFor(kOrigins[o], &pool);
            FI_ASSERT(Combo_ComboItemClassFor(kOrigins[o]) == (uint16_t)RSBS_ITEMCLASS_ALL_V1);
            std::vector<int> draw((size_t)poolCount, -1);
            FI_ASSERT(Combo_ForeignPoolDrawFor(kOrigins[o], draw.data(), poolCount) == poolCount);
            for (int i = 0; i < poolCount; i++) {
                FI_ASSERT(draw[(size_t)i] == i);
            }
        }
    }

    // ------------------------------------------------------------------
    // (N) NARROWING. Each armed bit yields ONLY members of that class, the
    //     classes PARTITION the pool, and the frozen record is what selects.
    // ------------------------------------------------------------------
    for (int o = 0; o < 2; o++) {
        const uint8_t origin = kOrigins[o];
        const ComboForeignItemDef* pool = NULL;
        const int poolCount = Combo_GetForeignItemPoolFor(origin, &pool);

        int summed = 0;
        for (int b = 0; b < kAllocatedClassBitCount; b++) {
            const uint16_t bit = kAllocatedClassBits[b];

            // Freeze a record that arms exactly this one class for this origin.
            // Written through the RECORD, not through a parameter, because the
            // claim under test is "the FROZEN setting selects" — a filter
            // driven only by an explicit mask argument would pass even if no
            // production path ever read gComboCtx.
            ComboSettingsRecord rec;
            Combo_ComboSettingsDefaults(&rec);
            if (origin == (uint8_t)GAME_OOT) {
                rec.itemClassOoT = bit;
            } else {
                rec.itemClassMM = bit;
            }
            gComboCtx.comboSettings = rec;
            FI_ASSERT(Combo_ComboItemClassFor(origin) == bit);

            std::vector<int> draw((size_t)poolCount, -1);
            const int n = Combo_ForeignPoolDrawFor(origin, draw.data(), poolCount);
            FI_ASSERT(n >= 0 && n <= poolCount);
            summed += n;

            int prev = -1;
            for (int i = 0; i < n; i++) {
                const int idx = draw[(size_t)i];
                FI_ASSERT(idx >= 0 && idx < poolCount);
                // ONLY members of the armed class...
                FI_ASSERT(pool[idx].itemClass == bit);
                // ...and still in POOL ORDER, which is world-visible.
                FI_ASSERT(idx > prev);
                prev = idx;
            }
            // The count-only form must agree with the filled form, or the
            // shortfall alarm that uses it reports a different number from the
            // pass it is describing.
            FI_ASSERT(Combo_ForeignPoolDrawFor(origin, NULL, 0) == n);

            printf("[TEST] foreign-item-class: origin %u class %-14s -> %d of %d rows\n", (unsigned)origin,
                   Combo_ForeignItemClassName(bit), n, poolCount);
        }
        // The classes PARTITION the pool: exactly-one-bit per row (asserted
        // above) plus per-class counts summing to the whole means no row is
        // double-counted and none is stranded.
        FI_ASSERT(summed == poolCount);
    }

    // The empty and unallocated masks select nothing — "no classes armed" is a
    // legitimate state (the direction byte is what says OFF), and an
    // unallocated bit must not resolve to members it cannot have.
    for (int o = 0; o < 2; o++) {
        FI_ASSERT(Combo_ForeignPoolClassMembersFor(kOrigins[o], 0u, NULL, 0) == 0);
        FI_ASSERT(Combo_ForeignPoolClassMembersFor(kOrigins[o], 0x0040u, NULL, 0) == 0);
    }
    // An origin with no pool has no members and no class, whatever is frozen.
    FI_ASSERT(Combo_ForeignPoolDrawFor((uint8_t)GAME_NONE, NULL, 0) == 0);
    FI_ASSERT(Combo_ComboItemClassFor((uint8_t)GAME_NONE) == 0);

    // A FROZEN zero is honoured verbatim rather than clamped up to the
    // defaults (ADR 0011 decision 3.3). This is the one place the class differs
    // from the pool SIZE, whose zero IS clamped, and getting it backwards would
    // silently re-arm a direction the player turned off.
    {
        ComboSettingsRecord rec;
        Combo_ComboSettingsDefaults(&rec);
        rec.itemClassOoT = 0;
        rec.itemClassMM = 0;
        gComboCtx.comboSettings = rec;
        FI_ASSERT(Combo_ComboItemClassFor((uint8_t)GAME_OOT) == 0);
        FI_ASSERT(Combo_ComboItemClassFor((uint8_t)GAME_MM) == 0);
        FI_ASSERT(Combo_ForeignPoolDrawFor((uint8_t)GAME_OOT, NULL, 0) == 0);
        FI_ASSERT(Combo_ForeignPoolDrawFor((uint8_t)GAME_MM, NULL, 0) == 0);
    }

    // ------------------------------------------------------------------
    // (T) TOTALITY of the name inverse, UNDER THE NARROWEST SELECTION.
    // ------------------------------------------------------------------
    // Left frozen at itemClass == 0 from the block above — the state in which a
    // selection-scoped inverse would resolve NOTHING. Every name any class can
    // produce must still round-trip, because this is the spoiler-LOAD path and
    // it runs in processes that never generated (accepted answer O3).
    FI_ASSERT(Combo_ComboItemClassFor((uint8_t)GAME_OOT) == 0);
    for (int o = 0; o < 2; o++) {
        const uint8_t origin = kOrigins[o];
        const ComboForeignItemDef* pool = NULL;
        const int poolCount = Combo_GetForeignItemPoolFor(origin, &pool);
        // The REGISTRY still serves the whole pool: only the DRAW narrows.
        FI_ASSERT(poolCount >= 1 && pool != NULL);
        for (int i = 0; i < poolCount; i++) {
            SharedItem back;
            FI_ASSERT(Combo_GetForeignItemByNameFor(origin, pool[i].name, &back));
            FI_ASSERT(back.originGame == origin && back.id == pool[i].item.id);
            // And the forward direction with it — a spoiler writes the name the
            // pool gave it, so both halves must span the same set.
            FI_ASSERT(Combo_GetForeignItemName(pool[i].item) == pool[i].name);
        }
        printf("[TEST] foreign-item-class: origin %u name inverse total over %d rows with ZERO classes armed\n",
               (unsigned)origin, poolCount);
    }

    // ------------------------------------------------------------------
    // (C) CRITERION ATTRIBUTION: every excluded candidate names the criterion
    //     that excluded it, and is absent from the pool AND the inverse.
    // ------------------------------------------------------------------
    // Without this the class rule has no observable and the lock degenerates
    // into "the table looks right" (ADR 0011's increment-3 test-locks row). It
    // is driven through each pool TU's own table rather than a list kept here,
    // so a row that drifts back into a pool goes red at the exclusion it
    // contradicts rather than passing quietly.
    for (int o = 0; o < 2; o++) {
        const uint8_t origin = kOrigins[o];
        const ComboForeignItemDef* pool = NULL;
        const int poolCount = Combo_GetForeignItemPoolFor(origin, &pool);

        int exclusions = 0;
        uint16_t excludedId = 0;
        uint8_t criterion = 0;
        int seenCriteria = 0;
        for (int index = 0;; index++) {
            const int ok = (origin == (uint8_t)GAME_OOT)
                               ? OoT_ForeignItem_TestExclusionAt(index, &excludedId, &criterion)
                               : MM_ForeignItem_TestExclusionAt(index, &excludedId, &criterion);
            if (!ok) {
                break;
            }
            exclusions++;
            // A real criterion, in the published range, with a real name.
            FI_ASSERT(criterion >= (uint8_t)RSBS_FOREIGN_CRIT_REAL_ITEM &&
                      criterion < (uint8_t)RSBS_FOREIGN_CRIT_COUNT);
            FI_ASSERT(strcmp(Combo_ForeignCriterionName(criterion), "(unknown)") != 0);
            seenCriteria |= (1 << criterion);
            // ...and the id it names really is out of the pool. This is the
            // assertion that catches a shared resource drifting back in.
            for (int i = 0; i < poolCount; i++) {
                FI_ASSERT(pool[i].item.id != excludedId);
            }
        }
        // Non-vacuous: an empty table would make every assertion above a no-op.
        FI_ASSERT(exclusions >= 5);
        // And the exclusions are not all one criterion — a table that only ever
        // said "criterion 6" would not be evidence that six criteria exist.
        FI_ASSERT(PopCount16((uint16_t)(seenCriteria & 0xFFFF)) >= 4);
        printf("[TEST] foreign-item-class: origin %u — %d attributed exclusions across %d criteria\n",
               (unsigned)origin, exclusions, PopCount16((uint16_t)(seenCriteria & 0xFFFF)));
    }

    // Leave global state clean for any subsequent row in `--test all`.
    ComboContext_Init();

    printf("[TEST] PASS: default bitset draws the pinned pool byte-identically; a narrowed bitset draws only its "
           "classes; the name inverse stays total\n");
    return TEST_PASS;
}
