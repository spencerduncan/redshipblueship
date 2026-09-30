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
 * The NAME surface is also locked: an item is named, articled and iconed by its
 * origin game's describer, the (origin, name) inverse round-trips, and the two
 * id-spaces never borrow each other's names — the MM textbox, the OoT arrival
 * toast and both spoiler surfaces depend on it. (The pinned pool these legs
 * used to walk retired in ADR 0010 increment 3; the items are now looked up by
 * name, see test_named_items.h.)
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as
 * C++, like test_save_roundtrip.c) for the rsbs::SaveManager half; every
 * pipeline symbol it drives is C-linkage.
 */

#include "../context.h"
#include "../crossing_store.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

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
void MM_Rando_Foreign_TestSetObtained(uint16_t randoCheckId, int obtained);
int Switch_PrepareHotSwap(GameId departing, const void* saveContext, size_t size);
int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);

// The foreign-host CLASS (#488's surviving half; ADR 0010 increment 3): the
// predicate the MM engine's hostAcceptsForeign answers with, plus the
// inspection accessors that restate the class independently from MM's real
// check table (Rando/Foreign.cpp's bridge block).
int MM_Rando_Foreign_TestIsForeignHostClass(uint16_t randoCheckId);
int MM_Rando_Foreign_TestCheckIdMax(void);
int MM_Rando_Foreign_TestCheckClass(uint16_t randoCheckId, int* outIsChestType, int* outHasChestFlag);
void MM_Rando_Foreign_TestItemSentinels(uint16_t* outJunk, uint16_t* outNone, uint16_t* outUnknown);

// MM's give id predicate (games/mm/2s2h/Rando/ForeignItemsSingleExe.cpp): the
// reverse legs pick an MM item MM's own give accepts.
int MM_ForeignItem_TestIsGiveableId(uint16_t riId);
int MM_ForeignItem_TestIsJunkClassId(uint16_t riId);

// #575 item 2: MM's gossip-stone candidate filter and item-name substitution
// (games/mm/2s2h/Rando/ActorBehavior/EnGs.cpp's bridge block).
int MM_Rando_Hints_TestGossipCandidate(uint16_t randoCheckId, int excludeObtained);
int MM_Rando_Hints_TestGossipItemName(uint16_t randoCheckId, char* out, int cap);
void MM_Rando_Hints_TestSetCheck(uint16_t randoCheckId, uint16_t randoItemId, int shuffled, int obtained,
                                 uint16_t* priorItemId, int* priorShuffled, int* priorObtained);

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
int OoT_Rando_Foreign_TestSetObtained(uint16_t rc, int obtained);
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
    // The name surface: describer-backed, origin-keyed, round-trips.
    // ------------------------------------------------------------------
    //
    // Two real OoT items, looked up by (origin, name) — the spoiler-LOAD
    // inverse — rather than by a hardcoded RG_* this TU cannot name. Lens of
    // Truth is ALSO a real MM display name, which is what the collision legs
    // below need.
    SharedItem ootLens;
    SharedItem ootHammer;
    FI_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &ootLens));
    FI_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Megaton Hammer", &ootHammer));
    const SharedItem kOoTItems[] = { ootLens, ootHammer };
    for (const SharedItem& item : kOoTItems) {
        FI_ASSERT(item.originGame == (uint8_t)GAME_OOT);
        FI_ASSERT(item.id != 0);
        FI_ASSERT(item.flags == 0);
        const char* name = Combo_GetForeignItemName(item);
        FI_ASSERT(name != NULL && name[0] != '\0');
        // Same answer through both entry points: one describer, two names for it.
        FI_ASSERT(Combo_DescribeItemName(item) == name);
        // #510: MM's pickup textbox reads "You found " + article + name, and MM
        // cannot look up OoT's item table for the article — it rides the
        // describer, with its own trailing space.
        const char* article = Combo_GetForeignItemArticle(item);
        FI_ASSERT(article != NULL);
        if (article[0] != '\0') {
            FI_ASSERT(article[strlen(article) - 1] == ' ');
        }
        // #494: the OoT arrival toast's icon is an ITEM_* texture-map key.
        const char* icon = Combo_GetForeignItemIconName(item);
        FI_ASSERT(icon != NULL && icon[0] == 'I');
        // The inverse round-trips to the SAME id, under OoT only.
        SharedItem back;
        FI_ASSERT(Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, name, &back));
        FI_ASSERT(back.originGame == item.originGame && back.id == item.id);
        FI_ASSERT(!Combo_GetForeignItemByNameFor((uint8_t)GAME_NONE, name, NULL));
    }
    FI_ASSERT(ootLens.id != ootHammer.id);
    FI_ASSERT(strcmp(Combo_GetForeignItemArticle(ootLens), "the ") == 0);
    FI_ASSERT(strcmp(Combo_GetForeignItemIconName(ootLens), "ITEM_LENS") == 0);

    // ------------------------------------------------------------------
    // ADR 0009 decision 3: (origin, name) is the key; bare name is NOT.
    // ------------------------------------------------------------------
    // "Lens of Truth" is a real display name in BOTH id-spaces, so a name-only
    // inverse would resolve it to whichever game it asked first and write a
    // WRONG ORIGIN TAG into a placement table — the #356 aliasing class through
    // the spoiler-LOAD path. The collision is asserted HANDLED, never absent:
    // renaming an item to dodge it would degrade the spoiler to work around a
    // lookup bug.
    {
        SharedItem fromOoT, fromMM;
        FI_ASSERT(Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, "Lens of Truth", &fromOoT));
        FI_ASSERT(Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, "Lens of Truth", &fromMM));
        FI_ASSERT(fromOoT.originGame == (uint8_t)GAME_OOT);
        FI_ASSERT(fromMM.originGame == (uint8_t)GAME_MM);
        FI_ASSERT(fromOoT.id == ootLens.id);
        FI_ASSERT(MM_ForeignItem_TestIsGiveableId(fromMM.id) == 1); // a real MM item, not a sentinel
        FI_ASSERT(strcmp(Combo_GetForeignItemName(fromMM), "Lens of Truth") == 0);

        // The legacy bare-name entry point keeps its exact previous meaning:
        // the OoT id-space.
        SharedItem legacy;
        FI_ASSERT(Combo_GetForeignItemByName("Lens of Truth", &legacy));
        FI_ASSERT(legacy.originGame == (uint8_t)GAME_OOT && legacy.id == fromOoT.id);

        // Forward direction dispatches on the item's own tag: the SAME raw id in
        // the other id-space must not borrow this one's name.
        SharedItem ootSameId = fromMM;
        ootSameId.originGame = (uint8_t)GAME_OOT;
        const char* ootName = Combo_GetForeignItemName(ootSameId);
        FI_ASSERT(ootName == NULL || strcmp(ootName, "Lens of Truth") != 0 || ootSameId.id == fromOoT.id);

        // An untagged item has no id-space: no name, no article, no icon.
        SharedItem untaggedName;
        untaggedName.originGame = (uint8_t)GAME_NONE;
        untaggedName.flags = 0;
        untaggedName.id = fromMM.id;
        FI_ASSERT(Combo_GetForeignItemName(untaggedName) == NULL);
        FI_ASSERT(Combo_GetForeignItemArticle(untaggedName) == NULL);
        FI_ASSERT(Combo_GetForeignItemIconName(untaggedName) == NULL);
        // A name no game has resolves to nothing, in either id-space.
        FI_ASSERT(!Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, "Not An Item Anywhere", NULL));
        FI_ASSERT(!Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, "Not An Item Anywhere", NULL));
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
    FI_ASSERT(Combo_SetForeignPlacement(0, ootLens) < 0);              // RC_UNKNOWN rejected
    FI_ASSERT(Combo_SetForeignPlacement(kForeignTestCheckA, ootLens) >= 0);
    FI_ASSERT(Combo_SetForeignPlacement(kForeignTestCheckA, ootLens) < 0); // duplicate check rejected
    FI_ASSERT(Combo_SetForeignPlacement(kForeignTestCheckB, ootHammer) >= 0);
    FI_ASSERT(Combo_CountForeignPlacements() == 2);
    const SharedItem* hosted = Combo_GetForeignPlacementForCheck(kForeignTestCheckA);
    FI_ASSERT(hosted != NULL);
    FI_ASSERT(hosted->originGame == (uint8_t)GAME_OOT && hosted->id == ootLens.id);
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
    FI_ASSERT(gComboCtx.sharedItemsTagged[0].id == ootLens.id);
    // One COPY per crossing pickup (ADR 0010 increment 3): the record is never
    // content-merged, and the once-per-host gate is the check's `obtained` bit,
    // which the CheckQueue lambda sets right after the call.
    FI_ASSERT(gComboCtx.sharedItemsTagged[0].flags == RSBS_SHARED_ITEM_CROSSING);
    MM_Rando_Foreign_TestSetObtained(kForeignTestCheckA, 1);
    FI_ASSERT(MM_Rando_Foreign_RecordPickup(kForeignTestCheckA) == 0); // a later cycle: no second copy
    FI_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 1);
    MM_Rando_Foreign_TestSetObtained(kForeignTestCheckA, 0);

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
    FI_ASSERT(award.lastId == ootLens.id);
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
    FI_ASSERT(gComboCtx.foreignPlacements[0].item.id == ootLens.id);
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
    SharedItem ootItem;
    FI_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &ootItem));
    FI_ASSERT(Combo_SetForeignPlacement(kForeignTestCheckA, ootItem) >= 0);
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

    FI_ASSERT(Combo_SetForeignPlacement(kForeignTestCheckB, ootItem) >= 0);
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

    // A REAL MM item, found by name: MM's give only accepts ids its own table
    // knows, so an arbitrary u16 would be refused downstream and the chain would
    // look broken for the wrong reason.
    SharedItem placedItem;
    FI_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &placedItem));
    FI_ASSERT(MM_ForeignItem_TestIsGiveableId(placedItem.id) == 1);
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
    FI_ASSERT(gComboCtx.sharedItemsTagged[0].flags == RSBS_SHARED_ITEM_CROSSING);

    // ---- (3) A collected host does not deliver twice -------------------
    // The once-per-host gate is the location's collected status (the RC-queue
    // drain checks it too); a record is one copy and is never content-merged.
    // The gate needs OoT's location table, which a ROM-free process may not have
    // built; there the drain's own `!loc->HasObtained()` is the whole gate and
    // this leg has nothing to drive, which it says rather than asserting it.
    if (OoT_Rando_Foreign_TestSetObtained(kForeignTestCheckA, 1) != 0) {
        FI_ASSERT(OoT_Rando_Foreign_RecordPickup(kForeignTestCheckA) == 0);
        FI_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == 1);
        OoT_Rando_Foreign_TestSetObtained(kForeignTestCheckA, 0);
    } else {
        printf("[TEST] foreign-item-give-reverse: no OoT location table in this process; the collected-host gate is "
               "the RC-queue drain's own check\n");
    }

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
// #488: the foreign-HOST CLASS.
//
// The give path only reaches a foreign placement from inside
// `if (randoSaveCheck.eligible)` (MM's MiscBehavior/CheckQueue.cpp), so a host
// whose `.eligible` bit is never armed strands its crossing permanently —
// invisible, unwinnable, and indistinguishable in-game from an item that was
// never placed. #488 tightened the host rule to Tier A: cycle-reset chests,
// whose flag game code arms on the ordinary path.
//
// Under the single bag (ADR 0010 increment 3) this CLASS is the whole of the
// MM host rule: the MM engine's hostAcceptsForeign answers with
// Rando::Foreign::IsForeignHostClass, and the fill draws a crossing onto no
// other MM check. The fill-side halves of the old predicate ("the fill put junk
// here", "not skipped", "not a sentinel") retired with the overlay pass that
// needed them: the single-bag fill places onto a host before any junk exists,
// and skipped checks are not in its host pool at all. This drives the REAL
// predicate over MM's REAL static check table and restates the class
// independently through the inspection bridge, so the two cannot drift.
// ============================================================================
TestResult Test_ForeignHostEligibility(void) {
    printf("[TEST] foreign-host-eligibility: only game-armed check classes can host a crossing (#488)\n");

    const int checkIdMax = MM_Rando_Foreign_TestCheckIdMax();
    FI_ASSERT(checkIdMax > 1);

    int acceptedCount = 0;
    int chestRowCount = 0;
    int chestRowsMissingFlag = 0;
    int firstNonChestAcceptedId = 0;
    int firstRejectedNonChestId = 0;
    for (int id = 1; id < checkIdMax; id++) {
        int isChestType = 0;
        int hasChestFlag = 0;
        if (MM_Rando_Foreign_TestCheckClass((uint16_t)id, &isChestType, &hasChestFlag) == 0) {
            FI_ASSERT(MM_Rando_Foreign_TestIsForeignHostClass((uint16_t)id) == 0); // not a real row
            continue;
        }
        if (isChestType) {
            chestRowCount++;
            if (!hasChestFlag) {
                chestRowsMissingFlag++;
            }
        } else if (firstRejectedNonChestId == 0) {
            firstRejectedNonChestId = id;
        }
        if (MM_Rando_Foreign_TestIsForeignHostClass((uint16_t)id)) {
            acceptedCount++;
            if (!isChestType && firstNonChestAcceptedId == 0) {
                firstNonChestAcceptedId = id;
            }
        }
    }
    printf("[TEST] foreign-host-eligibility: %d host-class checks over %d chest rows (table has %d check ids)\n",
           acceptedCount, chestRowCount, checkIdMax - 1);

    // Tier A ships alone: nothing outside RCTYPE_CHEST may be accepted, and the
    // sweep must actually have met a non-chest row to reject.
    FI_ASSERT(firstNonChestAcceptedId == 0);
    FI_ASSERT(firstRejectedNonChestId != 0);
    // Static-table invariant: every chest row carries FLAG_CYCL_SCENE_CHEST. A
    // FLAG_NONE chest row would have no vanilla setter and would strand.
    FI_ASSERT(chestRowCount > 0);
    FI_ASSERT(chestRowsMissingFlag == 0);
    // Acceptance is exactly the chest rows: an inequality means the class rule
    // and the table have drifted apart.
    FI_ASSERT(acceptedCount == chestRowCount);
    // RC_UNKNOWN and an out-of-range id are never hosts.
    FI_ASSERT(MM_Rando_Foreign_TestIsForeignHostClass(0) == 0);
    FI_ASSERT(MM_Rando_Foreign_TestIsForeignHostClass((uint16_t)checkIdMax) == 0);

    printf("[TEST] PASS: only chest-class checks can host a crossing\n");
    return TEST_PASS;
}

// ============================================================================
// #575 item 2: an MM gossip stone can hint a CROSSING HOST, by the crossed item.
//
// A crossing host physically holds MM's RI_JUNK cover (the MM engine's `place`
// writes it for every OoT-origin item; the real item lives in the crossing
// store). The stone's candidate filter dropped every RITYPE_JUNK holder, so the
// one hint that names the other game's item in this world could never be given;
// and a forced one would have read the cover ("Junk (...)"). Drives the stone's
// REAL candidate filter and name substitution (EnGs.cpp) over MM's real check
// table, with the crossing held where a production world holds it: the store.
// ============================================================================
TestResult Test_ForeignHostGossipHint(void) {
    printf("[TEST] foreign-host-gossip-hint: an MM gossip stone hints a crossing host by the crossed item (#575)\n");

    SharedItem ootHammer;
    SharedItem ootLens;
    SharedItem mmLens;
    FI_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Megaton Hammer", &ootHammer));
    FI_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &ootLens));
    FI_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &mmLens));
    const char* hammerName = Combo_GetForeignItemName(ootHammer);
    const char* hammerArticle = Combo_GetForeignItemArticle(ootHammer);
    FI_ASSERT(hammerName != NULL && hammerName[0] != '\0');
    FI_ASSERT(hammerArticle != NULL);
    // In the stone's voice: article + name, as it says a native item, then the
    // item's game: an MM host only ever holds an OoT item, and many names exist
    // in both games. " (OoT)" is the check trackers' and OoTMM's marker.
    const std::string hammerSaid = std::string(hammerArticle) + hammerName + " (OoT)";
    uint16_t riJunk = 0;
    MM_Rando_Foreign_TestItemSentinels(&riJunk, NULL, NULL);

    // Three real host-class checks: the crossing host, a plain junk holder, and
    // a native major item. All three are rows the fill can really write.
    uint16_t rows[3] = { 0, 0, 0 };
    int found = 0;
    const int checkIdMax = MM_Rando_Foreign_TestCheckIdMax();
    for (int id = 1; id < checkIdMax && found < 3; id++) {
        if (MM_Rando_Foreign_TestIsForeignHostClass((uint16_t)id)) {
            rows[found++] = (uint16_t)id;
        }
    }
    FI_ASSERT(found == 3);
    const uint16_t kHost = rows[0];
    const uint16_t kJunk = rows[1];
    const uint16_t kNative = rows[2];

    ComboContext_Init();
    Combo_Crossings_Clear();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE02u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED5A5Bu;
    FI_ASSERT(Combo_ForeignPairingActive());

    uint16_t priorItem[3];
    int priorShuffled[3];
    int priorObtained[3];
    // What MM's engine `place` writes for an OoT-origin item: the cover, shuffled.
    MM_Rando_Hints_TestSetCheck(kHost, riJunk, 1, 0, &priorItem[0], &priorShuffled[0], &priorObtained[0]);
    MM_Rando_Hints_TestSetCheck(kJunk, riJunk, 1, 0, &priorItem[1], &priorShuffled[1], &priorObtained[1]);
    MM_Rando_Hints_TestSetCheck(kNative, mmLens.id, 1, 0, &priorItem[2], &priorShuffled[2], &priorObtained[2]);
    ComboCrossing crossing;
    memset(&crossing, 0, sizeof(crossing));
    crossing.hostCheck = kHost;
    crossing.itemClass = (uint16_t)RSBS_ITEMCLASS_PROGRESSION;
    crossing.item = ootHammer;
    FI_ASSERT(Combo_Crossings_Replace(NULL, 0, &crossing, 1) == 1);
    FI_ASSERT(Combo_GetForeignPlacementForCheck(kHost) != NULL);

    // Controls: the native filter is unchanged. A native major item is hinted by
    // its own name; a junk holder that hosts nothing is still never hinted.
    char name[128];
    FI_ASSERT(MM_Rando_Hints_TestGossipCandidate(kNative, 0) == 1);
    FI_ASSERT(MM_Rando_Hints_TestGossipItemName(kNative, name, (int)sizeof(name)) > 0);
    printf("[TEST] foreign-host-gossip-hint: native check %u holds MM id %u, stone says \"%s\"\n", (unsigned)kNative,
           (unsigned)mmLens.id, name);
    FI_ASSERT(strcmp(name, "the Lens of Truth") == 0); // MM's hint names an item with its article
    FI_ASSERT(MM_Rando_Hints_TestGossipCandidate(kNative, 1) == 1);
    FI_ASSERT(MM_Rando_Hints_TestGossipCandidate(kJunk, 0) == 0);
    FI_ASSERT(MM_Rando_Hints_TestGossipCandidate(kJunk, 1) == 0);

    // The crossing host: a candidate on both paths, named by the crossed item.
    const int hostCandidate = MM_Rando_Hints_TestGossipCandidate(kHost, 0);
    const int hostCandidatePurchasable = MM_Rando_Hints_TestGossipCandidate(kHost, 1);
    const int nameLen = MM_Rando_Hints_TestGossipItemName(kHost, name, (int)sizeof(name));
    printf("[TEST] foreign-host-gossip-hint: host %u (holds the cover, hosts %s): candidate=%d purchasable=%d, "
           "stone says \"%s\"\n",
           (unsigned)kHost, hammerSaid.c_str(), hostCandidate, hostCandidatePurchasable, nameLen > 0 ? name : "");
    FI_ASSERT(hostCandidate == 1);
    FI_ASSERT(hostCandidatePurchasable == 1);

    // A name both games use: OoT's Lens crossing into MM must not read like MM's
    // own Lens on the native check, or two stones could name "the Lens of
    // Truth" at two places with nothing saying which game's Lens each means.
    {
        // The store freezes on its first write (a world's crossings are its
        // identity), so a different set goes in only after a clear.
        ComboCrossing lensCrossing = crossing;
        lensCrossing.item = ootLens;
        Combo_Crossings_Clear();
        FI_ASSERT(Combo_Crossings_Replace(NULL, 0, &lensCrossing, 1) == 1);
        char nativeName[128];
        char lensName[128];
        FI_ASSERT(MM_Rando_Hints_TestGossipItemName(kNative, nativeName, (int)sizeof(nativeName)) > 0);
        FI_ASSERT(MM_Rando_Hints_TestGossipItemName(kHost, lensName, (int)sizeof(lensName)) > 0);
        printf("[TEST] foreign-host-gossip-hint: same-name pair: host %u (OoT's Lens) says \"%s\", native %u (MM's "
               "Lens) says \"%s\"\n",
               (unsigned)kHost, lensName, (unsigned)kNative, nativeName);
        FI_ASSERT(strcmp(lensName, nativeName) != 0);
        const std::string lensSaid =
            std::string(Combo_GetForeignItemArticle(ootLens)) + Combo_GetForeignItemName(ootLens) + " (OoT)";
        FI_ASSERT(lensSaid == lensName);
        Combo_Crossings_Clear();
        FI_ASSERT(Combo_Crossings_Replace(NULL, 0, &crossing, 1) == 1);
    }
    FI_ASSERT(nameLen > 0 && hammerSaid == name);

    // The purchasable path still skips a collected host, like any other check.
    MM_Rando_Foreign_TestSetObtained(kHost, 1);
    FI_ASSERT(MM_Rando_Hints_TestGossipCandidate(kHost, 1) == 0);
    FI_ASSERT(MM_Rando_Hints_TestGossipCandidate(kHost, 0) == 1);
    MM_Rando_Foreign_TestSetObtained(kHost, 0);

    // With no crossing on it the host is the junk it holds, and is not hinted.
    Combo_Crossings_Clear();
    FI_ASSERT(Combo_GetForeignPlacementForCheck(kHost) == NULL);
    FI_ASSERT(MM_Rando_Hints_TestGossipCandidate(kHost, 0) == 0);

    for (int i = 0; i < 3; i++) {
        MM_Rando_Hints_TestSetCheck(rows[i], priorItem[i], priorShuffled[i], priorObtained[i], NULL, NULL, NULL);
    }
    ComboContext_Init();

    printf("[TEST] PASS: a crossing host is a gossip-stone candidate and is named by the crossed item\n");
    return TEST_PASS;
}

// ============================================================================
// #510's foreign-pool-mm row and #495's foreign-item-class row are RETIRED with
// the pinned pools they locked (ADR 0010 increment 3, D3; lane K11). What they
// asserted and where it lives now:
//   - the pool is registered / well-formed / non-junk / giveable: there is no
//     pool. Which items may cross is the O8 classification owner's answer,
//     locked by shared-items-class; that crossings deliver is locked end to end
//     by combo-single-bag and combo-creation-event.
//   - the class bitset draws a rule-defined pool: the bitset now gates HOME_ONLY
//     per origin (combo_single_bag.c), locked by combo-single-bag's leg C (the
//     frozen direction OFF) and leg C2 (a frozen class set without PROGRESSION:
//     that origin crosses nothing while the other still crosses).
//   - the name inverse is total: Combo_GetForeignItemByNameFor is describer-
//     backed and total by construction; the name legs above drive it.
// ============================================================================
