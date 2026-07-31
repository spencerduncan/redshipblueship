/**
 * @file test_commit_generation.c
 * @brief Headless locks for the .redsave commit choke point (#537/#531).
 *
 * Three locks:
 *
 *   commit-generation-monotonic — every commit staged through the choke point
 *   stamps a strictly-increasing generation into Tier-1, the stamp survives a
 *   round trip, and the counter RESUMES from the loaded value (monotonic
 *   across sessions, not just within one).
 *
 *   commit-torn-write — the #537 tear is unrepresentable through the choke
 *   point. The write phase serializes ONLY the immutable snapshot marshalled
 *   at stage time: state mutated after staging (including a deliberate
 *   half-written SharedItem, the exact interleave shape from the #537 report,
 *   and a live mutator thread scribbling the shadows WHILE the write runs)
 *   must not reach the artifact. COUNTERFACTUAL: revert the marshalling —
 *   i.e. make WriteStagedCommit (or the OnSaveFile hook it serves) read the
 *   live gComboCtx/shadows again — and this test goes red, because the
 *   post-stage mutations would then be serialized.
 *
 *   commit-generation-skew — the load-time freshness comparison between the
 *   two durable artifacts, surfaced through the #533 machinery: agreement and
 *   the pre-stamp (0) exemptions load cleanly; a .redsave NEWER than the .sav
 *   (the #531 loss shape) loads but records +1 on the slot's panel surface;
 *   a .sav NEWER than the .redsave (a missing .redsave commit) REFUSES before
 *   committing — quarantine, write latch, RSBS_REFUSE_COMMIT_SKEW — because
 *   committing the rolled-back Tier-1 would resurrect consumed shared-item
 *   records and roll back MM's only persistence.
 *
 * Linkage note: like test_save_roundtrip.c, this file is #included into
 * test_runner.cpp at FILE SCOPE (compiled as C++) so it can call the C++
 * rsbs::SaveManager API.
 */

#include "../context.h"
#include "../game.h"
#include "../save.h"
#include "../test_runner.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#define CG_ASSERT(cond, msg)                    \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

namespace {

const char* const kCommitGenTestDir = "rsbs_test_commit_gen";

// Fill both shadows with a single repeated byte. One byte per instant makes a
// torn mix of two instants detectable by scanning for ANY off-pattern byte.
void CommitGenFillShadows(uint8_t value) {
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, value);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, value);
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

bool CommitGenShadowsAreUniform(uint8_t value) {
    const uint8_t* oot = static_cast<const uint8_t*>(Context_GetOoTSaveContext());
    const uint8_t* mm = static_cast<const uint8_t*>(Context_GetMMSaveContext());
    if (oot == nullptr || mm == nullptr) {
        return false;
    }
    for (size_t i = 0; i < OOT_SAVE_CONTEXT_SIZE; i++) {
        if (oot[i] != value) {
            return false;
        }
    }
    for (size_t i = 0; i < MM_SAVE_CONTEXT_SIZE; i++) {
        if (mm[i] != value) {
            return false;
        }
    }
    return true;
}

}  // namespace

TestResult Test_CommitGenerationMonotonic(void) {
    printf("[TEST] commit-generation-monotonic: choke-point commits stamp a monotonic Tier-1 generation (#537)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kCommitGenTestDir);

    Context_InitFrozenStates();
    ComboContext_Init();
    CommitGenFillShadows(0x11);
    // Hermetic latch state (#533): DeleteSave is one of the three legitimate
    // arming events, so the commits below are latch-legal by construction.
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(0);

    CG_ASSERT(gComboCtx.commitGeneration == 0, "a fresh ComboContext must start at generation 0 (unset)");

    // N commits through the one-call form of the choke point: strictly
    // increasing by one, stamped into the live Tier-1 at stage time.
    for (uint32_t i = 1; i <= 5; i++) {
        CG_ASSERT(mgr.Save(0), "Save(0) failed");
        CG_ASSERT(gComboCtx.commitGeneration == i, "commit generation must advance by exactly one per commit");
    }

    // The stamp is IN the artifact: wipe live state, load, and the counter
    // resumes from the stored value rather than restarting.
    ComboContext_Init();
    CG_ASSERT(gComboCtx.commitGeneration == 0, "ComboContext_Init must reset the live counter");
    CG_ASSERT(mgr.Load(0), "Load(0) failed");
    CG_ASSERT(gComboCtx.commitGeneration == 5, "the loaded Tier-1 must carry the last committed generation");

    // Monotonic ACROSS the load: the next commit continues the sequence.
    CG_ASSERT(mgr.Save(0), "post-load Save(0) failed");
    CG_ASSERT(gComboCtx.commitGeneration == 6, "the counter must resume from the loaded value, not restart");

    // The two-phase form stamps identically (this is the path the threaded
    // OoT save takes: stage on the game thread, write on the worker).
    const uint32_t staged = RsbsSave_StageCommit();
    CG_ASSERT(staged == 7, "StageCommit must stamp the next generation");
    CG_ASSERT(gComboCtx.commitGeneration == 7, "StageCommit must stamp the live Tier-1");
    CG_ASSERT(RsbsSave_WriteStagedCommit(0) == 1, "WriteStagedCommit failed");
    ComboContext_Init();
    CG_ASSERT(mgr.Load(0), "reload failed");
    CG_ASSERT(gComboCtx.commitGeneration == 7, "the artifact must carry the staged generation");

    mgr.DeleteSave(0);
    ComboContext_Init();
    printf("[TEST] PASS: commit generation is monotonic within and across sessions\n");
    return TEST_PASS;
}

TestResult Test_CommitTornWrite(void) {
    printf("[TEST] commit-torn-write: post-stage mutation cannot reach the artifact (#537)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kCommitGenTestDir);

    Context_InitFrozenStates();
    ComboContext_Init();
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(0);

    // ---- Instant A: the state the game thread marshals -------------------
    const uint8_t kInstantA = 0x5A;
    CommitGenFillShadows(kInstantA);
    gComboCtx.sharedFlags[3] = 0xAAAA5555u;
    gComboCtx.sourceGame = GAME_OOT;
    gComboCtx.sharedItemsTagged[0].originGame = GAME_OOT;
    gComboCtx.sharedItemsTagged[0].flags = 0;
    gComboCtx.sharedItemsTagged[0].id = 0x1234;

    const uint32_t stagedGen = RsbsSave_StageCommit();
    CG_ASSERT(stagedGen != 0, "StageCommit refused with shadows present");

    // ---- The game thread moves on before the worker writes ---------------
    // This is the #537 failure shape verbatim: a SharedItem whose origin was
    // rewritten but whose id was not (a half-flipped record), the shadows
    // overwritten by a departure freeze, and gComboCtx fields mutated by
    // suspend-time staging — all between the save being INITIATED and the
    // worker's file write. Through the choke point none of it can reach disk.
    const uint8_t kInstantB = 0xB7;
    CommitGenFillShadows(kInstantB);
    gComboCtx.sharedFlags[3] = 0xBBBB0000u;
    gComboCtx.sourceGame = GAME_MM;
    gComboCtx.sharedItemsTagged[0].originGame = GAME_MM;  // half-write: id left at 0x1234
    gComboCtx.commitGeneration = 999;                     // scribble the live stamp too

    // And keep mutating WHILE the write runs, from another thread — the
    // literal #537 interleave. This is race-free by construction precisely
    // BECAUSE WriteStagedCommit reads only its staged copy; if it read live
    // state this would be the mid-copy tear.
    std::atomic<bool> stop{ false };
    std::thread mutator([&stop]() {
        uint8_t v = 0;
        std::vector<uint8_t> scribble(OOT_SAVE_CONTEXT_SIZE);
        while (!stop.load(std::memory_order_relaxed)) {
            std::memset(scribble.data(), v, scribble.size());
            Context_UpdateShadowCopy(GAME_OOT, scribble.data(), scribble.size());
            Context_UpdateShadowCopy(GAME_MM, scribble.data(), MM_SAVE_CONTEXT_SIZE);
            gComboCtx.sharedFlags[7] = v;
            v++;
        }
    });

    const int wrote = RsbsSave_WriteStagedCommit(0);
    stop.store(true);
    mutator.join();
    CG_ASSERT(wrote == 1, "WriteStagedCommit failed");

    // ---- The artifact must be instant A, whole ---------------------------
    ComboContext_Init();
    CommitGenFillShadows(0x00);
    CG_ASSERT(mgr.Load(0), "Load(0) failed");

    CG_ASSERT(gComboCtx.sharedFlags[3] == 0xAAAA5555u, "TORN: post-stage gComboCtx mutation reached the artifact");
    CG_ASSERT(gComboCtx.sourceGame == GAME_OOT, "TORN: post-stage sourceGame flip reached the artifact");
    CG_ASSERT(gComboCtx.sharedItemsTagged[0].originGame == GAME_OOT && gComboCtx.sharedItemsTagged[0].id == 0x1234,
              "TORN: the half-written SharedItem reached the artifact (the #537 failure shape)");
    CG_ASSERT(gComboCtx.commitGeneration == stagedGen,
              "the artifact must carry the generation stamped AT STAGE TIME, not the live scribble");
    CG_ASSERT(CommitGenShadowsAreUniform(kInstantA),
              "TORN: the artifact's world tiers mix instants — the write read live shadows");

    // ---- The choke point respects the #533 write latch -------------------
    // A perfectly coherent staged snapshot must STILL not reach a slot the
    // session has not established: stage a new instant, drop the armed state
    // (a fresh process / a refused slot), and the write phase must refuse and
    // leave the on-disk artifact untouched. COUNTERFACTUAL: remove the latch
    // check from WriteStagedCommit and instant C lands in the file — the
    // reload below then fails the instant-A assertions.
    const uint8_t kInstantC = 0xC3;
    CommitGenFillShadows(kInstantC);
    CG_ASSERT(RsbsSave_StageCommit() != 0, "staging instant C failed");
    rsbs::SaveManager::Instance().ResetSlotSessionState();
    CG_ASSERT(RsbsSave_WriteStagedCommit(0) == 0, "an unarmed slot must refuse the staged write (#533)");
    ComboContext_Init();
    CommitGenFillShadows(0x00);
    CG_ASSERT(mgr.Load(0), "reload after the latched write failed");
    CG_ASSERT(CommitGenShadowsAreUniform(kInstantA),
              "the latched write reached the artifact — the choke point ignored the #533 latch");
    CG_ASSERT(gComboCtx.sharedFlags[3] == 0xAAAA5555u,
              "the latched write replaced the artifact's Tier-1");

    mgr.DeleteSave(0);
    ComboContext_Init();
    CommitGenFillShadows(0x00);
    printf("[TEST] PASS: the artifact is the staged instant, whole; the tear is unrepresentable\n");
    return TEST_PASS;
}

TestResult Test_CommitGenerationSkew(void) {
    printf("[TEST] commit-generation-skew: cross-artifact freshness at load, surfaced via #533 (#531/#564 V16)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kCommitGenTestDir);

    Context_InitFrozenStates();
    ComboContext_Init();
    CommitGenFillShadows(0x22);
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(0);

    // ---- The pure comparison: agree / legacy exemptions / both directions -
    CG_ASSERT(RsbsSave_CompareCommitGenerations(7, 7) == 0, "equal generations must read as agreement");
    CG_ASSERT(RsbsSave_CompareCommitGenerations(7, 0) == 0, "a pre-stamp .sav (0) must be exempt");
    CG_ASSERT(RsbsSave_CompareCommitGenerations(0, 5) == 0, "a pre-stamp .redsave (0) must be exempt");
    CG_ASSERT(RsbsSave_CompareCommitGenerations(9, 3) == 1,
              ".redsave newer than .sav must be detected (the #531 loss shape)");
    CG_ASSERT(RsbsSave_CompareCommitGenerations(2, 6) == -1, ".sav newer than .redsave must be detected");

    // ---- Author a real artifact at generation 3 --------------------------
    CG_ASSERT(mgr.Save(0) && mgr.Save(0) && mgr.Save(0), "authoring the generation-3 artifact failed");
    CG_ASSERT(gComboCtx.commitGeneration == 3, "authoring must have stamped generation 3");

    // ---- Agreement: same commit signed both artifacts --------------------
    mgr.ResetSlotSessionState();
    ComboContext_Init();
    CG_ASSERT(RsbsSave_LoadSlotChecked(0, 3) == RSBS_LOAD_OK, "agreeing artifacts must load");
    CG_ASSERT(RsbsSave_GetSlotCommitSkew(0) == 0, "agreement must record no skew");
    CG_ASSERT(gComboCtx.commitGeneration == 3, "the agreed load must commit Tier-1");

    // ---- Pre-stamp .sav (0): exempt, loads cleanly -----------------------
    mgr.ResetSlotSessionState();
    ComboContext_Init();
    CG_ASSERT(RsbsSave_LoadSlotChecked(0, 0) == RSBS_LOAD_OK, "a pre-stamp .sav must be exempt at load");
    CG_ASSERT(RsbsSave_GetSlotCommitSkew(0) == 0, "the exemption must record no skew");

    // ---- .redsave newer (+1): the #531 loss shape — load, but SURFACE it -
    // An MM-side commit landed after OoT's last save point. Refusing here
    // would refuse every ordinary MM session's reload, so the load proceeds;
    // the divergence is recorded on the slot and reaches the file panel.
    mgr.ResetSlotSessionState();
    ComboContext_Init();
    CG_ASSERT(RsbsSave_LoadSlotChecked(0, 1) == RSBS_LOAD_OK, "a redsave-newer slot must still load");
    CG_ASSERT(RsbsSave_GetSlotCommitSkew(0) == 1, "the #531 loss shape must be recorded (+1)");
    CG_ASSERT(mgr.ReadMeta(0).commitSkew == 1, "the skew must surface on the file-panel meta");
    CG_ASSERT(RsbsSave_IsSlotWritable(0) == 1, "a redsave-newer slot stays writable (next commit heals forward)");
    CG_ASSERT(mgr.GetSlotState(0) == RSBS_SLOT_VALID, "a redsave-newer slot is VALID, not REFUSED");

    // ---- .sav newer (-1): a .redsave commit is MISSING — REFUSE (#533) ---
    // Committing the rolled-back Tier-1 would resurrect consumed shared-item
    // records and roll back MM's only persistence. Full machinery: no commit,
    // quarantined evidence, sticky write latch, surfaced reason.
    mgr.ResetSlotSessionState();
    ComboContext_Init();
    gComboCtx.saveSlot = 0x51C3B00F;  // sentinel: a refused load must not clobber live state
    CG_ASSERT(RsbsSave_LoadSlotChecked(0, 9) == RSBS_LOAD_REFUSED,
              "a sav-newer slot must REFUSE (a .redsave commit is missing)");
    CG_ASSERT(gComboCtx.saveSlot == 0x51C3B00F, "the skew refusal clobbered live ComboContext");
    CG_ASSERT(mgr.GetSlotState(0) == RSBS_SLOT_REFUSED, "the slot must read REFUSED");
    CG_ASSERT(mgr.GetSlotRefuseReason(0) == RSBS_REFUSE_COMMIT_SKEW, "the reason must name the skew");
    CG_ASSERT(RsbsSave_IsSlotWritable(0) == 0, "the skew refusal must latch writes");
    CG_ASSERT(RsbsSave_HasQuarantine(0) == 1, "the stale .redsave must be quarantined as evidence");
    CG_ASSERT(RsbsSave_HasSave(0) == 0, "the slot path must be empty after quarantine");
    CG_ASSERT(!mgr.Save(0), "a latched Save after the skew refusal must refuse");
    CG_ASSERT(gComboCtx.commitGeneration == 0,
              "the latched Save must not advance the generation (latch precedes staging)");

    // ---- Explicit erase releases the slot and the evidence ---------------
    mgr.DeleteSave(0);
    CG_ASSERT(mgr.GetSlotState(0) == RSBS_SLOT_ABSENT, "erase must clear the REFUSED state");
    CG_ASSERT(RsbsSave_HasQuarantine(0) == 0, "erase disposes the quarantined evidence");

    mgr.ResetSlotSessionState();
    ComboContext_Init();
    CommitGenFillShadows(0x00);
    printf("[TEST] PASS: agree/legacy load, redsave-newer surfaces, sav-newer refuses via #533\n");
    return TEST_PASS;
}
