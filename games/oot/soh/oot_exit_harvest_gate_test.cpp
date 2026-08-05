/**
 * @file oot_exit_harvest_gate_test.cpp
 * ROM-free, display-free lock for #606: OoT's OnExitGame commit must not
 * advance the SHARED-RESOURCE POOL for a write the #533/#568 latch refuses.
 * CTest label "redship", row OoTExitHarvestGate in
 * CMake/SingleExecutable.cmake, dispatch "oot-exit-harvest-gate" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. The OnExitGame GameInteractor hook registered in
 * SaveManager.cpp (SaveManager::SaveManager(), :252-275) ran
 * OoT_HarvestSharedResources() + Context_UpdateShadowCopy() + the
 * gComboCtx.sourceGame write UNCONDITIONALLY, then called RsbsSave_Save(
 * fileNum). RsbsSave_Save checks the #533/#568 armed-session latch and
 * refuses outright when the slot was never established this session, or was
 * REFUSED for identity (#570) / generation (#500) divergence -- but by then
 * the harvest had already folded OoT's live rupees, wallet tier, hearts,
 * health and double defense into gComboCtx.sharedResources and moved the RAM
 * watermark table. The pool had advanced for a record that never reached
 * disk. This is the exact class #591/PR #600 fixed on MM's capture path
 * (games/mm/2s2h/mm_capture_harvest_gate_test.cpp) -- OoT's OWN staging seam
 * at SaveManager.cpp SaveSection (~:1553, `if (RsbsSave_IsSlotWritable(
 * fileNum))`) already guards the identical sequence correctly; OnExitGame was
 * the one seam left unguarded.
 *
 * WHY THAT CORRUPTS, and why it is not merely untidy. Harvest and apply are
 * not symmetric: for the CONSUMABLE kinds apply ASSIGNS (`*liveValue =
 * applied`, shared_resources.c), so whatever the phantom harvest left in the
 * pool is materialized verbatim in the other game at the next arrival -- the
 * far side gets rupees, a health bar and a magic meter derived from a commit
 * nobody holds, in EITHER direction (a lower live balance debits the pool
 * just as a higher one credits it). The MONOTONIC kinds are worse in kind:
 * max-merge cannot decay, so a wallet tier that enters the pool here is in it
 * permanently.
 *
 * WHY THIS TEST DRIVES THE REAL HOOK, not a copy of its body. OnExitGame's
 * logic is a lambda registered inside SaveManager::SaveManager() onto the
 * GLOBAL GameInteractor::Instance -- there is no standalone callable function
 * the way MM's MM_Combo_CaptureSaveToUnifiedSlot is. So this test constructs
 * a real SaveManager (its constructor is display-free/ROM-free: it only
 * builds lookup tables, starts a 1-thread pool, and registers hooks -- no
 * Ship::Context / CVar / ROM access anywhere in it) and dispatches through
 * GameInteractor::ExecuteHooks<OnExitGame>, isolated to just the hook ids
 * THIS SaveManager registers (see IsolatedOnExitGameHooks below) so a
 * differently-shaped process history (e.g. an earlier test in the "all"
 * dispatch that already booted a GameInteractor) cannot pollute the run.
 *
 * THE INVARIANTS THIS LOCKS (mirrors mm_capture_harvest_gate_test.cpp):
 *
 *   1. The harvest is REAL. A permitted exit-save into an established slot
 *      must move the pool -- otherwise every check below could pass against a
 *      feature that does nothing.
 *
 *   2. An exit-save into a slot this session never established leaves the
 *      pool BIT-FOR-BIT unchanged -- the consumable count that would have
 *      been credited, and the monotonic tier that would have been raised for
 *      good.
 *
 *   3. An exit-save into a slot refused for identity divergence (#570) does
 *      the same, in the DEBIT direction: a live balance BELOW the watermark
 *      would have walked the pool down.
 *
 *   4. The RAM watermark table is untouched by a refused exit-save. Checked
 *      by re-arming the slot and requiring the next permitted exit-save's
 *      delta to be measured from the last COMMITTED balance.
 *
 *   5. The permitted path still works after all of it -- the gate is a
 *      refusal, not a removal.
 *
 * COUNTERFACTUAL, run before landing: move the OoT_HarvestSharedResources()
 * call in the OnExitGame hook back above (or remove) the
 * RsbsSave_IsSlotWritable(fileNum) guard and checks 2, 3 and 4 go red.
 */

#include "SaveManager.h"
#include "Enhancements/game-interactor/GameInteractor.h"

#include "save.h"
#include "context.h"
#include "shared_resources.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

extern "C" SaveContext gSaveContext;
// games/oot/src/code/z_inventory.c -- pure bitfield math on gSaveContext, no
// PlayState/actor dependency, so it is safe to call headlessly. Declared here
// rather than pulling the full "functions.h" (which wraps the entire N64
// function surface) to keep this TU's footprint narrow, matching the
// "OTRGlobals C-only declarations" precedent used elsewhere in this file's
// sibling TUs (e.g. GameExports_SingleExe.cpp's Randomizer_GetSettingValue
// redeclaration).
extern "C" void OoT_Inventory_ChangeUpgrade(int16_t upgrade, int16_t value);

namespace {

#define EHGATE_ASSERT(cond, msg)                                          \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// Read a pool value, reporting "never shared" as a distinguishable sentinel
// so an absent slot and a zero-valued slot cannot be confused.
int PoolValue(uint8_t kind) {
    uint16_t value = 0;
    if (!Combo_GetSharedResource(kind, &value)) {
        return -1;
    }
    return (int)value;
}

// Put gSaveContext into the shape a live OoT session runs in: GAMEMODE_NORMAL
// is what Combo_SaveIsLiveFile (via OoT_SaveIsLiveFile) requires before
// OoT_HarvestSharedResources will do anything at all. A zeroed context
// already reads as NORMAL (GAMEMODE_NORMAL == 0); the assignment is written
// out so a renumber upstream fails here rather than silently disarming the
// whole test.
void ArmLiveOoTSession(int slot, int32_t rupees, int16_t walletTier) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = slot;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    OoT_Inventory_ChangeUpgrade(UPG_WALLET, walletTier);
    gSaveContext.rupees = (s16)rupees;
    gSaveContext.rupeeAccumulator = 0;
}

// RegisteredGameHooks<H>::functions (GameInteractor.h) is `inline static` --
// ONE process-wide table shared by every GameInteractor instance, not scoped
// to whichever object registers into it. IsolatedOnExitGameHooks snapshots
// the hook ids resident before a fresh SaveManager is constructed and
// diffs afterward, so dispatch below can run EXACTLY the hooks that fresh
// SaveManager just registered (SaveManager.cpp has two: the ThreadPoolWait
// hook at :143 and the harvest/save hook under test at :252) regardless of
// what an earlier test in the same "all" process may have left registered.
using OnExitGameHooks = GameInteractor::RegisteredGameHooks<GameInteractor::OnExitGame>;

std::vector<HOOK_ID> SnapshotOnExitGameHookIds() {
    std::vector<HOOK_ID> ids;
    ids.reserve(OnExitGameHooks::functions.size());
    for (auto& kv : OnExitGameHooks::functions) {
        ids.push_back(kv.first);
    }
    return ids;
}

} // namespace

extern "C" int OoT_ExitHarvestGate_RunHeadless(void) {
    // Isolated save directory so this never touches a real player's slots.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "rsbs_oot_exit_harvest_gate_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    rsbs::SaveManager::Instance().SetSaveDirectory(dir.string());
    RsbsSave_ResetSlotSessionState();

    Context_InitFrozenStates();
    ComboContext_Init();
    // Both stores start empty, so every "unchanged" assertion below is
    // measured against a world this test authored rather than whatever ran
    // before it in the same process.
    Combo_ResetSharedResourceWatermarks();

    // Reach the real OnExitGame hook (see the file comment for why this
    // cannot be a direct function call): ensure GameInteractor::Instance
    // exists (the same lazy-init idiom production uses elsewhere, e.g.
    // Enhancements/randomizer/TrackerAdapterSingleExe.cpp), snapshot the
    // OnExitGame hook ids already resident, construct a fresh SaveManager
    // (registers exactly its own hooks onto GameInteractor::Instance), then
    // diff to isolate dispatch to just the ones this SaveManager added.
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const std::vector<HOOK_ID> before = SnapshotOnExitGameHookIds();
    SaveManager* freshSaveManager = new SaveManager();
    (void)freshSaveManager; // kept alive for the process lifetime, like every other *::Instance singleton here
    std::vector<HOOK_ID> ourHookIds;
    for (HOOK_ID id : SnapshotOnExitGameHookIds()) {
        if (std::find(before.begin(), before.end(), id) == before.end()) {
            ourHookIds.push_back(id);
        }
    }
    EHGATE_ASSERT(ourHookIds.size() == 2,
                  "expected exactly two OnExitGame hooks from a fresh SaveManager (ThreadPoolWait + the harvest/save "
                  "hook under test) -- SaveManager.cpp's registration count changed, revisit this test's isolation");

    auto dispatchOnExitGame = [&](int32_t fileNum) {
        for (HOOK_ID id : ourHookIds) {
            OnExitGameHooks::functions[id](fileNum);
        }
    };

    const int kSlot = 0;
    const int kUnestablished = 1;

    // OoT's wallet capacities are {99, 200, 500, 999} (OoT_gUpgradeCapacities,
    // GameExports_SingleExe.cpp), so tier 1 holds exactly kCommittedRupees and
    // tier 2 holds well past kRefusedHigh -- the harvest's own wallet clamp
    // can never be what keeps a value out of the pool in this test.
    const int32_t kCommittedRupees = 200;
    const int32_t kRefusedHigh = 350;
    const int32_t kRefusedLow = 20;

    // ---- 1. The harvest is REAL -------------------------------------------
    // Establish the slot the way production does (opening an empty slot IS
    // the create path and arms it), then commit a balance so the pool has a
    // non-trivial baseline for everything that follows.
    EHGATE_ASSERT(RsbsSave_LoadSlot(kSlot) == RSBS_LOAD_ABSENT, "an empty slot must open as ABSENT and arm");
    ArmLiveOoTSession(kSlot, kCommittedRupees, 1);
    dispatchOnExitGame(kSlot);
    EHGATE_ASSERT(PoolValue(RSBS_SHARED_RES_RUPEES) == kCommittedRupees,
                  "an exit-save into an armed slot must harvest OoT's live rupees into the shared pool -- without "
                  "this the 'pool unchanged' checks below would all pass vacuously");
    EHGATE_ASSERT(PoolValue(RSBS_SHARED_RES_WALLET_TIER) == 1,
                  "an exit-save into an armed slot must harvest OoT's wallet tier into the shared pool");
    EHGATE_ASSERT(RsbsSave_HasSave(kSlot) == 1, "the permitted exit-save must have produced a readable slot file");

    const uint32_t genAfterCommit = gComboCtx.commitGeneration;

    // ---- 2. Never-established slot: no pool movement, either kind ---------
    // The live session is RICHER than the pool and holds a HIGHER wallet
    // tier. With the harvest ahead of the latch check, the pool would gain
    // 150 rupees and a wallet tier it can never give back (monotonic
    // max-merge cannot decay), for a .redsave write that is about to be
    // refused.
    {
        EHGATE_ASSERT(RsbsSave_IsSlotWritable(kUnestablished) == 0,
                      "a slot this session never loaded/created/erased must not be writable (#533/#568)");
        ArmLiveOoTSession(kUnestablished, kRefusedHigh, 2);

        dispatchOnExitGame(kUnestablished);
        EHGATE_ASSERT(RsbsSave_HasSave(kUnestablished) == 0,
                      "a latch-refused exit-save must not have written a slot file");
        EHGATE_ASSERT(gComboCtx.commitGeneration == genAfterCommit,
                      "a latch-refused exit-save must not advance the monotonic commit generation");
        EHGATE_ASSERT(PoolValue(RSBS_SHARED_RES_RUPEES) == kCommittedRupees,
                      "a latch-refused exit-save must not credit the shared rupee pool -- the crossing that would "
                      "spend it has no record on disk (#606)");
        EHGATE_ASSERT(PoolValue(RSBS_SHARED_RES_WALLET_TIER) == 1,
                      "a latch-refused exit-save must not raise a MONOTONIC tier in the pool -- max-merge cannot "
                      "decay, so this one would be permanent (#606)");
    }

    // ---- 3. Identity-refused slot: no pool movement in the DEBIT direction
    // #570 latches a slot whose FILE is healthy because the running session
    // diverged from the pair's creation identity. Here the live balance is
    // BELOW the watermark the committed exit-save left, so a harvest would
    // walk the pool DOWN -- the direction that silently deletes the player's
    // money rather than inventing it.
    {
        EHGATE_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 1, "the slot must still be writable before the refusal");
        RsbsSave_RefuseSlotIdentity(kSlot);
        EHGATE_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 0, "an identity refusal must latch the slot (#570)");

        ArmLiveOoTSession(kSlot, kRefusedLow, 2);
        dispatchOnExitGame(kSlot);
        EHGATE_ASSERT(gComboCtx.commitGeneration == genAfterCommit,
                      "an identity-refused exit-save must not advance the monotonic commit generation");
        EHGATE_ASSERT(PoolValue(RSBS_SHARED_RES_RUPEES) == kCommittedRupees,
                      "an identity-refused exit-save must not DEBIT the shared rupee pool either -- harvest is a "
                      "delta against the watermark and runs in both directions (#606)");
        EHGATE_ASSERT(PoolValue(RSBS_SHARED_RES_WALLET_TIER) == 1,
                      "an identity-refused exit-save must not raise the shared wallet tier");
    }

    // ---- 4/5. The watermark survived, and the permitted path still works --
    // Re-arm through the file-create seam rather than a load: RsbsSave_LoadSlot
    // deliberately DROPS the RAM watermarks (save.cpp, the first-harvest
    // seed), which would erase the very state this check exists to inspect.
    // ArmSlotOnCreate touches neither gComboCtx nor the watermark table.
    {
        RsbsSave_ArmSlotOnCreate(kSlot);
        EHGATE_ASSERT(RsbsSave_IsSlotWritable(kSlot) == 1, "the create seam must re-arm the slot");

        ArmLiveOoTSession(kSlot, kRefusedHigh, 2);
        dispatchOnExitGame(kSlot);
        EHGATE_ASSERT(gComboCtx.commitGeneration == genAfterCommit + 1,
                      "the permitted exit-save must advance the commit generation by exactly one");
        // 200 committed + (350 - 200) earned since = 350. If a refused
        // exit-save had moved the watermark to kRefusedLow (20), the delta
        // measured here would be 330 and the pool would land at 350 + 180 =
        // 530 instead.
        EHGATE_ASSERT(PoolValue(RSBS_SHARED_RES_RUPEES) == kRefusedHigh,
                      "the permitted exit-save's delta must be measured from the last COMMITTED balance -- a "
                      "refused exit-save must leave the RAM watermark alone as well as the pool (#606)");
        EHGATE_ASSERT(PoolValue(RSBS_SHARED_RES_WALLET_TIER) == 2,
                      "the permitted exit-save must still raise the monotonic wallet tier -- the gate is a refusal, "
                      "not a removal of the harvest");
        EHGATE_ASSERT(RsbsSave_HasSave(kSlot) == 1, "the permitted exit-save must have produced a readable slot file");
    }

    // Leave global state clean for whatever runs next in this process.
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    RsbsSave_ResetSlotSessionState();
    std::filesystem::remove_all(dir, ec);

    printf("[TEST] oot-exit-harvest-gate: PASS\n");
    return 0;
}
