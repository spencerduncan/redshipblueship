/**
 * @file mm_capture_harvest_gate_test.cpp
 * ROM-free, display-free lock for #591: MM's unified-save capture must not
 * advance the SHARED-RESOURCE POOL for a commit the write latch refuses.
 * CTest label "redship", row MMCaptureHarvestGate in
 * CMake/SingleExecutable.cmake, dispatch "mm-capture-harvest-gate" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. MM_Combo_CaptureSaveToUnifiedSlot (games/mm/2s2h/
 * GameExports_SingleExe.cpp) ran MM_HarvestSharedResources() and only THEN
 * called RsbsSave_Save. RsbsSave_Save checks the #533/#568 armed-session latch
 * and refuses outright when the slot was never established this session, or was
 * REFUSED for identity (#570) / generation (#500) divergence — but by then the
 * harvest had already folded MM's live rupees, wallet tier, hearts, magic and
 * every ammo count into gComboCtx.sharedResources and moved the RAM watermark
 * table. The pool had advanced for a record that never reached disk.
 *
 * WHY THAT CORRUPTS, and why it is not merely untidy. Harvest and apply are not
 * symmetric: for the CONSUMABLE kinds apply ASSIGNS (`*liveValue = applied`,
 * shared_resources.c), so whatever the phantom harvest left in the pool is
 * materialized verbatim in the other game at the next arrival — the far side
 * gets rupees, a health bar, a magic meter and ammo counts derived from a
 * commit nobody holds, in EITHER direction (a lower live balance debits the
 * pool just as a higher one credits it). The MONOTONIC kinds are worse in kind:
 * max-merge cannot decay, so a wallet/quiver/heart tier that enters the pool
 * here is in it permanently. This is the same one-sided-gate class the #525
 * shared-resource work already paid for once with Combo_SaveIsLiveFile.
 *
 * THE INVARIANTS THIS LOCKS:
 *
 *   1. The harvest is REAL. A permitted capture into an established slot must
 *      move the pool — otherwise every check below could pass against a
 *      feature that does nothing. (This is the anti-vacuity check the whole
 *      file turns on: "pool unchanged" is trivially true if nothing ever
 *      changes the pool.)
 *
 *   2. A capture into a slot this session never established leaves the pool
 *      BIT-FOR-BIT unchanged — the consumable count that would have been
 *      credited, and the monotonic tier that would have been raised for good.
 *
 *   3. A capture into a slot refused for identity divergence (#570) does the
 *      same, in the DEBIT direction: a live balance BELOW the watermark would
 *      have walked the pool down.
 *
 *   4. The RAM watermark table is untouched by a refused capture. This is the
 *      half a "pool value unchanged" assertion alone cannot see: a harvest that
 *      dragged the watermark to the refused session's balance would leave the
 *      pool looking correct right up until the next PERMITTED capture computed
 *      its delta against the wrong baseline. Checked by re-arming the slot and
 *      requiring the next permitted capture's delta to be measured from the
 *      last COMMITTED balance.
 *
 *   5. The permitted path still works after all of it — the gate is a refusal,
 *      not a removal. (Deleting MM_HarvestSharedResources() outright would
 *      satisfy 2-4 and fails here.)
 *
 * COUNTERFACTUAL, run before landing: move the MM_HarvestSharedResources() call
 * back above the RsbsSave_IsSlotWritable check in
 * MM_Combo_CaptureSaveToUnifiedSlot and checks 2, 3 and 4 go red.
 */

#include "global.h"

#include "save.h"
#include "context.h"
#include "shared_resources.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

extern "C" SaveContext gSaveContext;
extern "C" int MM_Combo_CaptureSaveToUnifiedSlot(void);

namespace {

#define HGATE_ASSERT(cond, msg)                                           \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// Read a pool value, reporting "never shared" as a distinguishable sentinel so
// an absent slot and a zero-valued slot cannot be confused.
int PoolValue(uint8_t kind) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource(kind, &value)) {
        return -1;
    }
    return (int)value;
}

// Put gSaveContext into the shape a live cross-game MM session runs in: the
// 0xFF "no real flash slot" sentinel and GAMEMODE_NORMAL, which is what
// Combo_SaveIsLiveFile requires before a harvest will do anything at all. A
// zeroed context already reads as NORMAL; the assignment is written out so a
// renumber upstream fails here rather than silently disarming the whole test.
void ArmLiveMMSession(int32_t rupees, uint32_t walletTier) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    MM_Inventory_ChangeUpgrade(UPG_WALLET, walletTier);
    gSaveContext.save.saveInfo.playerData.rupees = (s16)rupees;
}

} // namespace

extern "C" int MM_CaptureHarvestGate_RunHeadless(void) {
    // Isolated save directory so this never touches a real player's slots.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "rsbs_mm_capture_harvest_gate_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    rsbs::SaveManager::Instance().SetSaveDirectory(dir.string());
    RsbsSave_ResetSlotSessionState();

    Context_InitFrozenStates();
    ComboContext_Init();
    // Both stores start empty, so every "unchanged" assertion below is measured
    // against a world this test authored rather than whatever ran before it in
    // the same process.
    Combo_ResetSharedResourceWatermarks();

    const int kSlot = 0;
    const int kUnestablished = 1;

    // MM's wallet capacities are {99, 200, 500, 500} (z_inventory.c), so tier 1
    // holds exactly kCommittedRupees and tier 2 holds well past kRefusedHigh —
    // the harvest's own wallet clamp can never be what keeps a value out of the
    // pool in this test.
    const int32_t kCommittedRupees = 200;
    const int32_t kRefusedHigh = 350;
    const int32_t kRefusedLow = 20;

    // ---- 1. The harvest is REAL ------------------------------------------
    // Establish the slot the way production does (opening an empty slot IS the
    // create path and arms it), then commit a balance so the pool has a
    // non-trivial baseline for everything that follows.
    RsbsSave_SetActiveSlot(kSlot);
    HGATE_ASSERT(RsbsSave_LoadSlot(kSlot) == RSBS_LOAD_ABSENT, "an empty slot must open as ABSENT and arm");
    ArmLiveMMSession(kCommittedRupees, 1u);
    HGATE_ASSERT(MM_Combo_CaptureSaveToUnifiedSlot() == 1, "a capture into an armed slot must commit");
    HGATE_ASSERT(PoolValue(RSBS_SHARED_RES_RUPEES) == kCommittedRupees,
                 "a permitted capture must harvest MM's live rupees into the shared pool -- without this the "
                 "'pool unchanged' checks below would all pass vacuously");
    HGATE_ASSERT(PoolValue(RSBS_SHARED_RES_WALLET_TIER) == 1,
                 "a permitted capture must harvest MM's wallet tier into the shared pool");

    const uint32_t genAfterCommit = gComboCtx.commitGeneration;

    // ---- 2. Never-established slot: no pool movement, either kind ---------
    // The live session is RICHER than the pool and holds a HIGHER wallet tier.
    // With the harvest ahead of the latch check, the pool would gain 150 rupees
    // and a wallet tier it can never give back (monotonic max-merge cannot
    // decay), for a .redsave write that is about to be refused.
    {
        RsbsSave_SetActiveSlot(kUnestablished);
        HGATE_ASSERT(RsbsSave_IsSlotWritable(kUnestablished) == 0,
                     "a slot this session never loaded/created/erased must not be writable (#533/#568)");
        ArmLiveMMSession(kRefusedHigh, 2u);

        HGATE_ASSERT(MM_Combo_CaptureSaveToUnifiedSlot() == 0,
                     "a capture into an un-established slot must report that nothing was committed");
        HGATE_ASSERT(RsbsSave_HasSave(kUnestablished) == 0,
                     "a latch-refused capture must not have written a slot file");
        HGATE_ASSERT(gComboCtx.commitGeneration == genAfterCommit,
                     "a latch-refused capture must not advance the monotonic commit generation");
        HGATE_ASSERT(PoolValue(RSBS_SHARED_RES_RUPEES) == kCommittedRupees,
                     "a latch-refused capture must not credit the shared rupee pool -- the crossing that would "
                     "spend it has no record on disk (#591)");
        HGATE_ASSERT(PoolValue(RSBS_SHARED_RES_WALLET_TIER) == 1,
                     "a latch-refused capture must not raise a MONOTONIC tier in the pool -- max-merge cannot "
                     "decay, so this one would be permanent (#591)");
    }

    // ---- 3. Identity-refused slot: no pool movement in the DEBIT direction -
    // #570 latches a slot whose FILE is healthy because the running session
    // diverged from the pair's creation identity. Here the live balance is
    // BELOW the watermark the committed capture left, so a harvest would walk
    // the pool DOWN — the direction that silently deletes the player's money
    // rather than inventing it.
    {
        RsbsSave_SetActiveSlot(kSlot);
        HGATE_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 1, "the slot must still be writable before the refusal");
        RsbsSave_RefuseSlotIdentity(kSlot);
        HGATE_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 0, "an identity refusal must latch the slot (#570)");

        ArmLiveMMSession(kRefusedLow, 2u);
        HGATE_ASSERT(MM_Combo_CaptureSaveToUnifiedSlot() == 0,
                     "a capture into an identity-refused slot must report that nothing was committed");
        HGATE_ASSERT(gComboCtx.commitGeneration == genAfterCommit,
                     "an identity-refused capture must not advance the monotonic commit generation");
        HGATE_ASSERT(PoolValue(RSBS_SHARED_RES_RUPEES) == kCommittedRupees,
                     "an identity-refused capture must not DEBIT the shared rupee pool either -- harvest is a "
                     "delta against the watermark and runs in both directions (#591)");
        HGATE_ASSERT(PoolValue(RSBS_SHARED_RES_WALLET_TIER) == 1,
                     "an identity-refused capture must not raise the shared wallet tier");
    }

    // ---- 4/5. The watermark survived, and the permitted path still works --
    // Re-arm through the file-create seam rather than a load: RsbsSave_LoadSlot
    // deliberately DROPS the RAM watermarks (save.cpp, the first-harvest seed),
    // which would erase the very state this check exists to inspect.
    // ArmSlotOnCreate touches neither gComboCtx nor the watermark table.
    {
        RsbsSave_ArmSlotOnCreate(kSlot);
        HGATE_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 1, "the create seam must re-arm the slot");

        ArmLiveMMSession(kRefusedHigh, 2u);
        HGATE_ASSERT(MM_Combo_CaptureSaveToUnifiedSlot() == 1, "a capture into the re-armed slot must commit again");
        HGATE_ASSERT(gComboCtx.commitGeneration == genAfterCommit + 1,
                     "the permitted capture must advance the commit generation by exactly one");
        // 200 committed + (350 - 200) earned since = 350. If a refused capture
        // had moved the watermark to kRefusedLow (20), the delta measured here
        // would be 330 and the pool would land at 350 + 180 = 530 instead.
        HGATE_ASSERT(PoolValue(RSBS_SHARED_RES_RUPEES) == kRefusedHigh,
                     "the permitted capture's delta must be measured from the last COMMITTED balance -- a "
                     "refused capture must leave the RAM watermark alone as well as the pool (#591)");
        HGATE_ASSERT(PoolValue(RSBS_SHARED_RES_WALLET_TIER) == 2,
                     "the permitted capture must still raise the monotonic wallet tier -- the gate is a refusal, "
                     "not a removal of the harvest");
        HGATE_ASSERT(RsbsSave_HasSave(kSlot) == 1, "the permitted capture must have produced a readable slot file");
    }

    // Leave global state clean for whatever runs next in this process.
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    RsbsSave_SetActiveSlot(-1);
    RsbsSave_ResetSlotSessionState();
    std::filesystem::remove_all(dir, ec);

    printf("[TEST] mm-capture-harvest-gate: PASS\n");
    return 0;
}
