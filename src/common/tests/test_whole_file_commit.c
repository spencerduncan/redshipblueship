/**
 * @file test_whole_file_commit.c
 * @brief Headless locks for WHOLE-FILE COMMIT semantics (#589), which is what
 *        structurally retires the #531 permanent-loss bug.
 *
 * THE RULING (operator, 2026-08-04, on #589). A paired OoT+MM slot is ONE save
 * (#500/#564 one-game semantics). Therefore a durable save-and-quit in EITHER
 * half commits BOTH halves — the saving half live, the other half from its
 * frozen shadow, at ONE generation, in one instant, through the #569 commit
 * choke point — and on load, the newest WHOLE commit wins.
 *
 * WHAT #531 WAS. The write half of that was already true: StageCommit copies
 * Tier-1 + both shadows. The READ half was not. Load deliberately left Tier-2
 * unarmed ("OoT's own file{N+1}.sav is the authority for OoT state"), so after
 * an MM-side commit that landed later than OoT's last save point:
 *
 *   - Tier-1 committed, carrying RSBS_SHARED_ITEM_REDEEMED for a foreign item
 *     that had been awarded to OoT's LIVE save on a cross-game arrival, and
 *   - OoT resumed from its older .sav, which never contained that item.
 *
 * The redeem loop skips a REDEEMED entry unconditionally and forever
 * (shared_items.c), so the record outlived the item it accounted for: the item
 * was permanently gone and a paired seed could dead-end. The pre-ruling code
 * DETECTED this (the commit-generation comparison from #569) and printed a
 * warning. Detection is not authority; these locks are about authority.
 *
 * TWO LOCKS:
 *
 *   whole-file-commit — ATOMICITY and AUTHORITY as separable claims.
 *     (a) One commit carries BOTH halves, full width, at ONE generation — not
 *         one generation per half, and not a partial capture of either.
 *     (b) A load whose .redsave carries a NEWER whole commit than OoT's .sav
 *         ARMS the OoT half and reports it authoritative EXACTLY ONCE (the
 *         armed blob is single-use, #364).
 *     (c) Agreement and the pre-stamp (0) exemptions claim NO authority: the
 *         .sav delivers OoT's half, exactly as before the ruling. This is the
 *         check that stops the fix from being a blanket "the .redsave always
 *         wins", which would race a legacy or equal-instant copy of OoT's
 *         world against the file the engine just loaded.
 *     (d) A REFUSED load (#533 / the .sav-newer direction) claims no authority
 *         and commits nothing. The ruling is "the newest whole commit wins",
 *         NOT "arbitrate freshness in both directions" — a missing .redsave
 *         commit is still corruption to refuse.
 *
 *   whole-file-redeemed-item — the #531 scenario, end to end at the unit
 *     level, with its own counterfactual built in: the test asserts that the
 *     freshly-loaded .sav world does NOT contain the item (that is the losing
 *     state, reproduced) and then that the whole-commit delivery puts it back
 *     and leaves the REDEEMED record consistent with the world.
 *     COUNTERFACTUAL: delete the Tier-2 arming from SaveManager::LoadSlot (or
 *     the delivery seam this test mirrors from OoT's OnLoadFile hook) and this
 *     test goes red on exactly the assertion that names the loss.
 *
 * Linkage note: like test_commit_generation.c, this file is #included into
 * test_runner.cpp at FILE SCOPE (compiled as C++) so it can call the C++
 * rsbs::SaveManager API.
 */

#include "../context.h"
#include "../entrance.h"
#include "../game.h"
#include "../save.h"
#include "../shared_items.h"
#include "../test_runner.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// The delivery half of the whole-file commit lives in src/common/switch.cpp
// (restore + retire in one call, #364). No header declares it — z_play.c and
// OoT's SaveManager.cpp both take it by extern, and so does this file.
extern "C" int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);

#define WFC_ASSERT(cond, msg)                   \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

namespace {

const char* const kWholeFileTestDir = "rsbs_test_whole_file_commit";

// A whole SaveContext-sized buffer standing in for a game's LIVE gSaveContext.
// The headless tier cannot include either game's z64save.h (that is the whole
// point of src/common), so a half is modelled as its blob: the same bytes the
// context layer's shadow holds and the same bytes the .redsave stores.
using HalfBlob = std::vector<uint8_t>;

HalfBlob MakeOoTHalf(uint8_t fill) {
    return HalfBlob(OOT_SAVE_CONTEXT_SIZE, fill);
}

HalfBlob MakeMMHalf(uint8_t fill) {
    return HalfBlob(MM_SAVE_CONTEXT_SIZE, fill);
}

// "Full width" means EVERY byte, so this scans rather than spot-checking. A
// prefix capture (the shape that has bitten this codebase twice: MM's excluded
// SaveManager mirroring only sizeof(Save), and the owl marshal's
// offsetof(SaveContext, fileNum) memcpy) leaves the tail at whatever a
// different instant put there, which a first/last/middle sample can miss.
bool HalfIsUniform(const void* blob, size_t size, uint8_t value) {
    if (blob == nullptr) {
        return false;
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(blob);
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != value) {
            return false;
        }
    }
    return true;
}

// ---- #531 scenario scaffolding -------------------------------------------

// Byte offset inside the OoT half standing in for the inventory slot a foreign
// item lands in. Well past any header, and NOT byte 0, so a zeroed or
// wrong-length blob cannot satisfy the "item present" check by accident.
constexpr size_t kOoTBowSlot = 0x1200;
constexpr uint8_t kOoTBowPresent = 0x3B;
constexpr uint16_t kFairyBowId = 0x2A;

// The award callback the arrival hook supplies to Combo_RedeemSharedItemsForGame:
// it mutates only the LIVE save, which is precisely the asymmetry #531 lives
// on — the flag it sets afterwards is durable, the mutation is not.
void WholeFileAwardToLiveOoT(const SharedItem* item, void* ctx) {
    if (item == nullptr || ctx == nullptr) {
        return;
    }
    if (item->id != kFairyBowId) {
        return;
    }
    static_cast<uint8_t*>(ctx)[kOoTBowSlot] = kOoTBowPresent;
}

}  // namespace

TestResult Test_WholeFileCommit(void) {
    printf("[TEST] whole-file-commit: one commit carries both halves; the newest whole commit wins (#589)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kWholeFileTestDir);

    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(0);

    // A warm-up commit first, so the whole commit below lands at a generation
    // whose PREDECESSOR is a real stamp rather than the pre-stamp 0. The
    // ".redsave newer" case has to be tested against a .sav that makes a
    // claim; against 0 it would take the legacy exemption instead and the
    // authority assertions would pass for the wrong reason.
    {
        HalfBlob warm = MakeOoTHalf(0x01);
        HalfBlob warmMM = MakeMMHalf(0x02);
        Context_UpdateShadowCopy(GAME_OOT, warm.data(), warm.size());
        Context_UpdateShadowCopy(GAME_MM, warmMM.data(), warmMM.size());
    }
    WFC_ASSERT(mgr.Save(0), "the warm-up commit failed");

    // ---- (a) ATOMICITY: both halves, full width, at ONE generation --------
    // Distinct fills per half so a commit that captured one half twice, or
    // captured either half short, is visible rather than plausible.
    const uint8_t kOoTFill = 0x71;
    const uint8_t kMMFill = 0x2E;
    {
        HalfBlob oot = MakeOoTHalf(kOoTFill);
        HalfBlob mm = MakeMMHalf(kMMFill);
        Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
        Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
    }
    gComboCtx.sourceGame = GAME_MM;  // an MM-side durable route, the #531 shape
    gComboCtx.sharedFlags[2] = 0xFEEDFACEu;

    const uint32_t genBefore = gComboCtx.commitGeneration;
    WFC_ASSERT(mgr.Save(0), "the whole-file commit failed");
    const uint32_t wholeGen = gComboCtx.commitGeneration;
    WFC_ASSERT(wholeGen == genBefore + 1,
               "a whole-file commit is ONE commit: both halves must share a single generation advance, "
               "not take one each");

    // The file, not the memory: wipe both shadows and Tier-1 before reloading.
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    WFC_ASSERT(HalfIsUniform(Context_GetOoTSaveContext(), OOT_SAVE_CONTEXT_SIZE, 0),
               "test setup: the clear must have zeroed the OoT shadow");

    WFC_ASSERT(RsbsSave_LoadSlotChecked(0, wholeGen) == RSBS_LOAD_OK, "the whole commit must load back");
    WFC_ASSERT(gComboCtx.commitGeneration == wholeGen, "Tier-1 must carry the commit's generation");
    WFC_ASSERT(gComboCtx.sharedFlags[2] == 0xFEEDFACEu, "Tier-1 content must survive the whole commit");
    WFC_ASSERT(HalfIsUniform(Context_GetOoTSaveContext(), OOT_SAVE_CONTEXT_SIZE, kOoTFill),
               "the OoT half must come back WHOLE from a commit authored while MM was live — this is the "
               "other half the ruling requires every commit to carry");
    WFC_ASSERT(HalfIsUniform(Context_GetMMSaveContext(), MM_SAVE_CONTEXT_SIZE, kMMFill),
               "the MM half must come back whole from the same commit");

    // ---- (c) AGREEMENT claims no authority --------------------------------
    // The commit that stamped this generation also wrote OoT's .sav, so Tier-2
    // and the .sav describe one instant. Arming here would race a redundant
    // copy of OoT's world against the file the engine just loaded.
    WFC_ASSERT(RsbsSave_OoTHalfIsAuthoritative() == 0,
               "agreeing artifacts must claim NO OoT-half authority: the .sav delivers OoT's half");
    WFC_ASSERT(Context_HasFrozenState(GAME_OOT) == 0, "agreement must leave Tier-2 unarmed");
    WFC_ASSERT(Context_HasFrozenState(GAME_MM) == 1, "the MM half is armed unconditionally (#528), as before");

    // Pre-stamp (0) .sav: a legacy artifact makes no claim either way, so it
    // must not be adjudicated on evidence it does not carry.
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    mgr.ResetSlotSessionState();
    WFC_ASSERT(RsbsSave_LoadSlotChecked(0, 0) == RSBS_LOAD_OK, "a pre-stamp .sav must still load");
    WFC_ASSERT(RsbsSave_OoTHalfIsAuthoritative() == 0, "a pre-stamp .sav must claim no OoT-half authority");
    WFC_ASSERT(Context_HasFrozenState(GAME_OOT) == 0, "the pre-stamp exemption must leave Tier-2 unarmed");

    // ---- (b) NEWER whole commit: authority, armed, delivered ONCE ---------
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    mgr.ResetSlotSessionState();
    WFC_ASSERT(RsbsSave_LoadSlotChecked(0, wholeGen - 1) == RSBS_LOAD_OK,
               "a .redsave carrying a newer whole commit must load");
    WFC_ASSERT(RsbsSave_GetSlotCommitSkew(0) == 1, "the newer whole commit must be recorded on the slot");
    WFC_ASSERT(Context_HasFrozenState(GAME_OOT) == 1,
               "the newest whole commit's OoT half must be ARMED — leaving it unarmed is #531 exactly");
    WFC_ASSERT(RsbsSave_OoTHalfIsAuthoritative() == 1, "the newer whole commit must claim OoT-half authority");

    // The delivery seam, mirrored from OoT's OnLoadFile hook: take the
    // authority (one-shot) and consume the armed blob into the live half.
    {
        HalfBlob live = MakeOoTHalf(0x00);
        WFC_ASSERT(RsbsSave_TakeOoTHalfAuthority() == 1, "taking the authority must report it");
        WFC_ASSERT(RsbsSave_TakeOoTHalfAuthority() == 0,
                   "the authority is ONE-SHOT: the armed blob is single-use (#364), so a second taker must "
                   "not be told to re-apply a blob that has already been retired");
        WFC_ASSERT(Combo_ConsumeFrozenState("oot", live.data(), live.size()) == 1,
                   "consuming the armed OoT half must restore it");
        WFC_ASSERT(HalfIsUniform(live.data(), live.size(), kOoTFill),
                   "the delivered OoT half must be the .redsave's Tier-2, whole");
        WFC_ASSERT(Context_HasFrozenState(GAME_OOT) == 0,
                   "consumption must RETIRE the blob (#364): a surviving blob rolls the player back on the "
                   "next return leg");
    }

    // ---- (d) A REFUSED load claims no authority ---------------------------
    // The .sav-newer direction is unchanged by the ruling: a MISSING .redsave
    // commit is corruption to refuse, not freshness to arbitrate. Nothing may
    // be committed, nothing armed, nothing claimed.
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    mgr.ResetSlotSessionState();
    gComboCtx.saveSlot = 0x5AFE0007;  // sentinel: a refusal must not touch live state
    WFC_ASSERT(RsbsSave_LoadSlotChecked(0, wholeGen + 5) == RSBS_LOAD_REFUSED,
               "a .sav newer than the .redsave must still REFUSE (#533/#569)");
    WFC_ASSERT(gComboCtx.saveSlot == 0x5AFE0007, "the refusal clobbered live ComboContext");
    WFC_ASSERT(RsbsSave_OoTHalfIsAuthoritative() == 0,
               "a REFUSED slot has no authoritative half — the ruling must never hand a quarantined file's "
               "world to the live session");
    WFC_ASSERT(Context_HasFrozenState(GAME_OOT) == 0, "a refused load must arm nothing");
    WFC_ASSERT(RsbsSave_IsSlotWritable(0) == 0, "the skew refusal must still latch writes (#533)");

    // ---- The three release events also release the authority --------------
    // A stale authority claim is a rollback waiting for a consumer, so the same
    // events that clear a refusal clear it too.
    mgr.DeleteSave(0);
    WFC_ASSERT(RsbsSave_OoTHalfIsAuthoritative() == 0, "erase must clear any authority claim");
    mgr.ResetSlotSessionState();
    WFC_ASSERT(RsbsSave_OoTHalfIsAuthoritative() == 0, "a fresh session must claim nothing");

    Context_ClearAllFrozenStates();
    ComboContext_Init();
    printf("[TEST] PASS: one commit, both halves, one generation; the newest whole commit is the authority\n");
    return TEST_PASS;
}

TestResult Test_WholeFileRedeemedItem(void) {
    printf("[TEST] whole-file-redeemed-item: a REDEEMED record and the world it was redeemed into survive "
           "together (#531)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kWholeFileTestDir);

    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(0);

    // The two artifacts of the ONE save. `savGeneration` stands in for the
    // "rsbsCommitGeneration" key OoT's file{N+1}.sav mirrors: it advances only
    // when OoT's OWN save flow runs, which is the entire asymmetry #531 lives
    // on. `savWorld` stands in for the OoT world that .sav holds.
    uint32_t savGeneration = 0;
    HalfBlob savWorld = MakeOoTHalf(0x11);

    // ---- 1. OoT plays and saves at a save point ---------------------------
    // Both artifacts authored by one commit: the .redsave's Tier-2 and the
    // .sav agree, and the .sav mirrors the generation.
    HalfBlob ootLive = savWorld;
    Context_UpdateShadowCopy(GAME_OOT, ootLive.data(), ootLive.size());
    {
        HalfBlob mmCold = MakeMMHalf(0x00);
        Context_UpdateShadowCopy(GAME_MM, mmCold.data(), mmCold.size());
    }
    gComboCtx.sourceGame = GAME_OOT;
    WFC_ASSERT(mgr.Save(0), "OoT's own save must commit");
    savGeneration = gComboCtx.commitGeneration;
    WFC_ASSERT(ootLive[kOoTBowSlot] != kOoTBowPresent, "test setup: the saved OoT world must NOT hold the item");

    // ---- 2. The MM leg produces a foreign OoT item ------------------------
    WFC_ASSERT(Combo_RecordSharedItem(GAME_OOT, kFairyBowId) >= 0, "recording the foreign OoT item failed");
    WFC_ASSERT(Combo_CountSharedItems(GAME_OOT, false) == 1, "the item must be pending for OoT");

    // ---- 3. Arrival in OoT: the item is awarded to the LIVE save ----------
    // This is the asymmetry: the award mutates only the live SaveContext,
    // while RSBS_SHARED_ITEM_REDEEMED goes into durable Tier-1.
    WFC_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, WholeFileAwardToLiveOoT, ootLive.data()) == 1,
               "the arrival must redeem the pending item");
    WFC_ASSERT(ootLive[kOoTBowSlot] == kOoTBowPresent, "the award must have reached the live OoT save");
    WFC_ASSERT(Combo_CountSharedItems(GAME_OOT, false) == 0, "the entry must now be REDEEMED (skipped forever)");

    // ---- 4. The player walks straight back into the portal ----------------
    // No OoT save point in between: file{N+1}.sav still holds the pre-item
    // world and still mirrors the OLD generation. The crossing freezes OoT's
    // half — which is where the whole commit later reads it from.
    Context_FreezeState(GAME_OOT, OOT_ENTR_MARKET_FROM_MASK_SHOP, ootLive.data(), ootLive.size());

    // ---- 5. MM owl-saves: a WHOLE commit, at a new generation -------------
    {
        HalfBlob mmLive = MakeMMHalf(0x9C);
        Context_UpdateShadowCopy(GAME_MM, mmLive.data(), mmLive.size());
    }
    gComboCtx.sourceGame = GAME_MM;
    WFC_ASSERT(mgr.Save(0), "the MM-side owl commit failed");
    const uint32_t owlGeneration = gComboCtx.commitGeneration;
    WFC_ASSERT(owlGeneration > savGeneration,
               "test setup: an MM-side commit cannot rewrite OoT's .sav, so it must be the newer generation");

    // ---- 6. Process exit, relaunch, load the slot -------------------------
    // Everything in memory goes; OoT's engine loads file{N+1}.sav into its
    // live SaveContext (savWorld, WITHOUT the item) and only then does the
    // OnLoadFile hook run.
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    mgr.ResetSlotSessionState();

    HalfBlob resumed = savWorld;
    WFC_ASSERT(resumed[kOoTBowSlot] != kOoTBowPresent,
               "THE LOSING STATE, reproduced: the .sav world OoT resumes from does not contain the item. "
               "Everything below is about whether it stays that way");

    WFC_ASSERT(RsbsSave_LoadSlotChecked(0, savGeneration) == RSBS_LOAD_OK,
               "the slot must load (an MM-side commit after OoT's save point is the DESIGNED state)");
    WFC_ASSERT(gComboCtx.commitGeneration == owlGeneration, "Tier-1 must come from the newest whole commit");

    // The REDEEMED record came back — durable, as it always was.
    WFC_ASSERT(Combo_CountSharedItems(GAME_OOT, true) == 1, "the shared-item record must have survived");
    WFC_ASSERT(Combo_CountSharedItems(GAME_OOT, false) == 0, "it must still be REDEEMED, i.e. never redelivered");

    // ---- 7. The delivery seam (OoT's OnLoadFile hook, mirrored) -----------
    WFC_ASSERT(RsbsSave_TakeOoTHalfAuthority() == 1,
               "the .redsave holds a newer WHOLE commit, so its OoT half is the authority for this load");
    WFC_ASSERT(Combo_ConsumeFrozenState("oot", resumed.data(), resumed.size()) == 1,
               "the authoritative OoT half must be consumable");

    // ---- 8. Record and world are consistent -------------------------------
    // This is the assertion that names the bug. Before the ruling, the load
    // left Tier-2 unarmed, `resumed` kept the .sav's pre-item world, and the
    // REDEEMED record above blocked redelivery forever: the item was gone and
    // the seed could dead-end.
    WFC_ASSERT(resumed[kOoTBowSlot] == kOoTBowPresent,
               "#531: the item the REDEEMED record accounts for must be present in the resumed OoT world. "
               "A durable record whose effect did not survive with it is permanent loss of a progression "
               "item — the record and the world it was redeemed into commit together or not at all");

    // The redeem loop legitimately skips the entry now — and that is CORRECT,
    // because the world it accounts for is the one we are standing in. Assert
    // both halves of that: no redelivery, AND the item still present after the
    // pass (a redelivery here would be cross-game duplication, the failure the
    // REDEEMED flag exists to prevent).
    WFC_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, WholeFileAwardToLiveOoT, resumed.data()) == 0,
               "a REDEEMED entry must not redeliver");
    WFC_ASSERT(resumed[kOoTBowSlot] == kOoTBowPresent, "and the item must still be there after the pass");

    // The MM half of the same commit came back too — it is one file.
    WFC_ASSERT(HalfIsUniform(Context_GetMMSaveContext(), MM_SAVE_CONTEXT_SIZE, 0x9C),
               "the MM half of the newest whole commit must have loaded with it");
    WFC_ASSERT(Context_HasFrozenState(GAME_MM) == 1, "the MM half must be armed for its own arrival (#528)");

    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    printf("[TEST] PASS: the REDEEMED record and the item it accounts for survive the reload together\n");
    return TEST_PASS;
}
