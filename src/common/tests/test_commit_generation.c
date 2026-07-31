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
 *   two durable artifacts: agree == 0, .redsave newer == +1 (the #531 loss
 *   shape), .sav newer == -1, and either side at 0 (pre-stamp legacy) exempt.
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

    mgr.DeleteSave(0);
    ComboContext_Init();
    CommitGenFillShadows(0x00);
    printf("[TEST] PASS: the artifact is the staged instant, whole; the tear is unrepresentable\n");
    return TEST_PASS;
}

TestResult Test_CommitGenerationSkew(void) {
    printf("[TEST] commit-generation-skew: cross-artifact freshness comparison (#531/#564 V16)\n");

    Context_InitFrozenStates();
    ComboContext_Init();

    // Agreement — and the legacy exemptions (either side 0 predates the stamp).
    gComboCtx.commitGeneration = 7;
    CG_ASSERT(RsbsSave_CheckCommitGenerationSkew(0, 7) == 0, "equal generations must read as agreement");
    CG_ASSERT(RsbsSave_CheckCommitGenerationSkew(0, 0) == 0, "a pre-stamp .sav (0) must be exempt");
    gComboCtx.commitGeneration = 0;
    CG_ASSERT(RsbsSave_CheckCommitGenerationSkew(0, 5) == 0, "a pre-stamp .redsave (0) must be exempt");

    // The #531 shape: .redsave ahead of the .sav (an MM-side commit landed
    // after OoT's last save point). Must be detected, not silently composited.
    gComboCtx.commitGeneration = 9;
    CG_ASSERT(RsbsSave_CheckCommitGenerationSkew(0, 3) == 1,
              ".redsave newer than .sav must be detected (the #531 loss shape)");

    // The inverse: the unified slot missed a commit.
    gComboCtx.commitGeneration = 2;
    CG_ASSERT(RsbsSave_CheckCommitGenerationSkew(0, 6) == -1, ".sav newer than .redsave must be detected");

    ComboContext_Init();
    printf("[TEST] PASS: skew detection distinguishes agree / redsave-newer / sav-newer / legacy\n");
    return TEST_PASS;
}
