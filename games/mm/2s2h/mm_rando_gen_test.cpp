/**
 * MM rando-gen smoke test bridge (Lane C0, #392) — ROM-free.
 *
 * Proves the un-elided 2ship_rando is REACHABLE end to end, not merely
 * linked: the ShipInit registrars populated the Logic/Regions graph, the
 * real generation path (Rando::MiscBehavior::OnFileCreate — seed hash,
 * GeneratePools, pool balancing, a logic apply, spoiler write) runs
 * headlessly, and the spoiler JSON lands on disk carrying the
 * 2S2H_RANDO_SPOILER tag — the historical proof-probe for "the randomizer is
 * actually in the binary".
 *
 * Driven by src/common/test_runner.cpp (dispatch "mm-rando-gen", CTest row
 * MMRandoGen in the rando label) after InitOTRForMMFirstBoot brings up the
 * shared Ship::Context — mirroring the OoT rando-gen harness. Kept behind an
 * extern "C" entry point so the delicate multi-include test_runner TU never
 * sees MM headers (the MM_SceneExecute_RunHeadless precedent).
 *
 * Returns 0 on success, a distinct nonzero step code on failure (printed by
 * the dispatcher; greppable in CI logs).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstdlib> // getenv — the attempt-ladder scan/digest modes
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <ship/Context.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>

#include "2s2h/BenPort.h"                       // appShortName
#include "2s2h/GameInteractor/GameInteractor.h" // S2H::GameHooks counters (single-exe tail)
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Foreign.h"
#include "2s2h/Rando/Spoiler/Spoiler.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/MiscBehavior/MiscBehavior.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"

// src/common — placement table + pool surface (Lane C1). Outside extern "C":
// it pulls context.h, whose <type_traits> include must not sit in C linkage.
#include "foreign_items.h"
// src/common — the #533 REFUSED surface (the arrival identity gate latches the
// active slot; Phase 4 of the switch-entry lock asserts it) and the
// creation-side profile stamp (combo_mm_options_view.h declares
// MM_Rando_ComputeProfileStamp, defined in Foreign.cpp).
#include "combo_mm_options_view.h"
#include "save.h"

extern "C" {
#include "z64save.h"
#include "regs.h"
void MM_Sram_InitNewSave(void);
void MM_Rando_Init(void);
extern SaveContext gSaveContext;

// Switch-entry lock (#439): the real consumption point plus the switch
// path's own state plumbing, driven exactly as rsbs/src/main.cpp and
// MM_Play_Init drive them.
void MM_Play_ConsumeStartupEntrance(void);
void Combo_FreezeState(const char* gameId, uint16_t returnEntrance, const void* saveContext, size_t size);
void Combo_ClearFrozenState(const char* gameId);
void Combo_SetStartupEntrance(uint16_t entrance);
void Combo_ClearStartupEntrance(void);

// Owl-save / file-copy readback locks (#487). Sram_UpdateWriteToFlashOwlSave
// and the owl page tables come from z64save.h above; func_80147414 is
// file-scope in z_sram_NES.c with no header declaration of its own.
void func_80147414(SramContext* sramCtx, s32 fileNum, s32 arg2);
}

extern "C" int MM_Rando_HeadlessGenTest(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(1): Ship::Context not live before generation\n");
        return 1;
    }

    // Registrars + Rando::Init (idempotent; MM_Game_Init also calls this in
    // the real boot path).
    MM_Rando_Init();

    // The Logic/Regions graph only populates if the 2ship_rando registrar
    // TUs were linked AND S2H::ShipInit::InitAll ran — the two halves of the
    // reachability claim. An empty graph is the historical "compiles fine,
    // does nothing" failure.
    if (Rando::Logic::Regions.empty()) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(2): Logic/Regions graph is empty — registrar TUs elided or InitAll "
                        "not run\n");
        return 2;
    }
    fprintf(stderr, "[MM-RANDO-GEN] Regions populated: %zu\n", Rando::Logic::Regions.size());

    // Pinned generation profile: new seed, fixed input strings, spoiler on,
    // RO_LOGIC = Nearly No Logic. Deliberate (Lane C0 scope): the MVP ships
    // free-form placement with the spoiler log carrying what logic would
    // otherwise carry (Lane D is deferred). Glitchless generation works too
    // — the #426 Deku Palace edge fix, locked by the GATING glitchless phase
    // below — but the MVP default stays pinned here.
    CVarSetInteger("gRando.Enabled", 1);
    CVarSetInteger("gRando.SpoilerFileIndex", 0);
    CVarSetInteger("gRando.GenerateSpoiler", 1);
    CVarSetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_NEARLY_NO_LOGIC);

    // Boot-time dependency the harness must stand in for: the give paths the
    // fill exercises write REG slots (e.g. Inventory_SetWorldMapCloudVisibility
    // ends with R_MINIMAP_DISABLED = false), and gRegEditor is only allocated
    // by Regs_Init() out of MM's system arena during the real MM_Game_Init.
    // Supply static storage instead of dragging the whole heap bring-up into
    // the unit harness. (First CI run of this test faulted at offset 0xb52
    // off a null gRegEditor, exactly here.)
    if (gRegEditor == NULL) {
        static RegEditor sHarnessRegEditor = {};
        gRegEditor = &sHarnessRegEditor;
    }

    // The real flow runs OnFileCreate on a fresh Sram new-save (file select →
    // Sram_InitSave → OnSaveInit hook). Reproduce that pre-state, retrying
    // over a FIXED seed list: MM's glitchless fill is a forward-fill with a
    // junk-swap heuristic that can genuinely dead-end on an unlucky seed
    // ("No non-junk items left" — in-game the player just regenerates; seed
    // RSBSMM1 dead-ends exactly this way, deterministically). The lock's
    // claim is that the generation MACHINERY works, so it passes on the
    // first successful seed and fails only if every attempt dead-ends —
    // deterministic either way, since the seed list and fill are fixed.
    static const char* kSeeds[] = { "RSBSMM2", "RSBSMM3", "RSBSMM4", "RSBSMM5", "RSBSMM6" };
    const char* usedSeed = NULL;
    for (const char* seed : kSeeds) {
        CVarSetString("gRando.InputSeed", seed);
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        MM_Sram_InitNewSave();

        // Through the REAL dispatch chain (Lane C1): this is the exact bridge
        // MM_Sram_InitSave calls (z_sram_NES.c) — GameInteractor_ExecuteOnSaveInit
        // -> S2H::GameHooks Execute<OnSaveInit> -> Rando::MiscBehavior::OnFileCreate.
        // If the bridge is ever re-stubbed (its former mm_stubs.c state),
        // generation silently never runs and this test fails at FAIL(3).
        GameInteractor_ExecuteOnSaveInit(0);

        // OnFileCreate's catch-path reverts the save type to vanilla on ANY
        // generation failure — a single, reliable failure signal.
        if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
            usedSeed = seed;
            break;
        }
        fprintf(stderr, "[MM-RANDO-GEN] seed %s dead-ended (fill threw); trying next\n", seed);
    }
    if (usedSeed == NULL) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(3): every seed attempt dead-ended — generation machinery broken\n");
        return 3;
    }
    fprintf(stderr, "[MM-RANDO-GEN] generated with seed %s\n", usedSeed);

    int shuffled = 0;
    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        if (randoStaticCheck.randoCheckId != RC_UNKNOWN && RANDO_SAVE_CHECKS[randoCheckId].shuffled) {
            shuffled++;
        }
    }
    fprintf(stderr, "[MM-RANDO-GEN] shuffled checks: %d\n", shuffled);
    if (shuffled < 50) {
        fprintf(stderr,
                "[MM-RANDO-GEN] FAIL(4): implausibly few shuffled checks (%d) — fill did not run over the "
                "real pools\n",
                shuffled);
        return 4;
    }

    // Through the module's own directory accessor (#439): the test must look
    // where the write path actually writes, not re-derive a second path that
    // can silently drift from it.
    std::string spoilerPath = Rando::Spoiler::SpoilerDirectory() + "/" + usedSeed + ".json";
    std::ifstream spoilerFile(spoilerPath);
    if (!spoilerFile.is_open()) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(5): spoiler file missing at %s\n", spoilerPath.c_str());
        return 5;
    }
    nlohmann::json spoiler;
    try {
        spoilerFile >> spoiler;
    } catch (const std::exception& e) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(6): spoiler unparseable: %s\n", e.what());
        return 6;
    }
    if (!spoiler.contains("type") || spoiler["type"] != "2S2H_RANDO_SPOILER") {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(7): spoiler missing the 2S2H_RANDO_SPOILER tag\n");
        return 7;
    }
    if (!spoiler.contains("checks") || spoiler["checks"].empty()) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(8): spoiler has no checks\n");
        return 8;
    }

    // Gating glitchless lock (#426): upstream 2S2H PR #1622's Deku Palace
    // region split dropped the bean-side -> upper-middle return edge, leaving
    // the {upper middle, cell side, cell ledge} ring with no incoming edge
    // from outside itself — so RC_DEKU_KINGS_CHAMBER_MONKEY (holding cell,
    // entered from the cell ledge) could never enter logic and EVERY
    // glitchless fill dead-ended with exactly that check left. Upstream PR
    // #1661 restored the edge; the port lives in Rando/Logic/Regions/
    // South.cpp. This phase locks it: glitchless must GENERATE. The fixed
    // seed list mirrors the main phase (the junk-swap forward fill can
    // genuinely dead-end on an unlucky seed); it fails only if every attempt
    // dead-ends.
    CVarSetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_GLITCHLESS);
    static const char* kGlitchlessSeeds[] = { "RSBSMMGL1", "RSBSMMGL2", "RSBSMMGL3", "RSBSMMGL4", "RSBSMMGL5" };
    const char* glitchlessSeed = NULL;
    for (const char* seed : kGlitchlessSeeds) {
        // Re-pin the GENERATE branch every attempt: the last successful
        // generation ran RefreshOptions, which repoints
        // gRando.SpoilerFileIndex at the spoiler it wrote — without this the
        // attempt silently LOADS that spoiler and "passes" regardless of the
        // glitchless fill's state.
        CVarSetInteger("gRando.SpoilerFileIndex", 0);
        CVarSetString("gRando.InputSeed", seed);
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        MM_Sram_InitNewSave();
        GameInteractor_ExecuteOnSaveInit(0);
        if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
            glitchlessSeed = seed;
            break;
        }
        fprintf(stderr, "[MM-RANDO-GEN] glitchless seed %s dead-ended (fill threw); trying next\n", seed);
    }
    if (glitchlessSeed == NULL) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(16): every glitchless seed dead-ended — glitchless reachability "
                        "regressed (#426)\n");
        return 16;
    }
    // The historical dead-end left RC_DEKU_KINGS_CHAMBER_MONKEY as the one
    // unplaceable check; assert it specifically received a placement so any
    // recurrence names itself in the log.
    if (!RANDO_SAVE_CHECKS[RC_DEKU_KINGS_CHAMBER_MONKEY].shuffled) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(17): glitchless world did not place RC_DEKU_KINGS_CHAMBER_MONKEY\n");
        return 17;
    }
    fprintf(stderr, "[MM-RANDO-GEN] glitchless generated with seed %s\n", glitchlessSeed);

    // ======================================================================
    // ADR 0010 increment 1.3 — reachable-closure COVERAGE lock.
    //
    // Every other lock on the closure proves it is deterministic, pure, and
    // that placements land inside it. None of them would notice a closure
    // that silently UNDER-reports, and under-reporting is this change's worst
    // failure mode: it invents shortfalls and strips hosts out of every
    // paired world without turning a single row red. (An over-reporting
    // closure is caught by FAIL(30) the moment it admits a host the world
    // cannot reach; the under-reporting direction had nothing.)
    //
    // The glitchless world just generated is the oracle, because in this one
    // logic mode the fill's own bookkeeping records what it reached:
    // GeneratePools writes `.shuffled = true` ONLY for user-excluded checks
    // (the `.skipped` rows, GeneratePools.cpp:150-152); every other
    // `.shuffled` bit in the save was written by ApplyGlitchlessLogicToSave-
    // Context's checksInLogic tail, i.e. exactly when the check ENTERED
    // LOGIC. So `shuffled && !skipped` is the fill's signed statement "I
    // reached this", and the closure — which runs the join-correct crawl, a
    // superset of the fill's first-visit-wins traversal, over the same world
    // — must contain all of it.
    //
    // Goes RED if the crawl's seeding entrance drifts, if the give path stops
    // crediting an item class (a new RI_* whose GiveItem leg is a no-op), if
    // the outer collect loop stops iterating to a fixpoint, or if the region
    // graph regresses. RC_DEKU_KINGS_CHAMBER_MONKEY is named separately: it
    // is the #426 check that a broken region edge made unreachable, so a
    // recurrence names itself here as well as at FAIL(17).
    // ======================================================================
    {
        const std::set<RandoCheckId> glitchlessReachable = Rando::Logic::ComputeReachableCheckSet();
        int placedByFill = 0;
        int missing = 0;
        RandoCheckId firstMissing = RC_UNKNOWN;
        for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
            if (randoStaticCheck.randoCheckId == RC_UNKNOWN) {
                continue;
            }
            const RandoSaveCheck& randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];
            if (!randoSaveCheck.shuffled || randoSaveCheck.skipped) {
                continue;
            }
            placedByFill++;
            if (!glitchlessReachable.contains(randoCheckId)) {
                if (missing == 0) {
                    firstMissing = randoCheckId;
                }
                missing++;
            }
        }
        if (placedByFill == 0) {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(33): the glitchless world recorded no in-logic placements — the "
                            "coverage lock below would be vacuous\n");
            return 33;
        }
        if (missing != 0) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(33): the reachable-check closure UNDER-REPORTS — %d of %d checks the "
                    "glitchless fill placed in logic are missing from it (first: %s). A closure narrower than the "
                    "fill's own reach silently strands foreign hosts and manufactures shortfalls\n",
                    missing, placedByFill, Rando::StaticData::Checks[firstMissing].name);
            return 33;
        }
        if (!glitchlessReachable.contains(RC_DEKU_KINGS_CHAMBER_MONKEY)) {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(33): RC_DEKU_KINGS_CHAMBER_MONKEY is outside the closure on a "
                            "glitchless world that placed it (#426 region-edge regression)\n");
            return 33;
        }
        fprintf(stderr,
                "[MM-RANDO-GEN] closure coverage verified on the glitchless world: %zu checks reachable, covering all "
                "%d the fill placed in logic\n",
                glitchlessReachable.size(), placedByFill);
    }

    CVarClear(Rando::StaticData::Options[RO_LOGIC].cvar);
    CVarSetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_NEARLY_NO_LOGIC);

    // ======================================================================
    // Paired-world phase (Lane C1, #392) — the spoiler's foreign section
    // lock. Stamp the Lane B carrier the way OoT's live producer would, then
    // generate through the real dispatch chain: the single-exe branch in
    // OnFileCreate must derive the seed from the master seed, swap junk
    // placements for the pinned OoT items (gComboCtx.foreignPlacements; the
    // MM table keeps RI_JUNK), and describe every crossing in the spoiler's
    // "foreign" section. Retries over fixed master seeds mirror the main
    // phase: an unlucky derived seed can genuinely dead-end the fill.
    // ======================================================================
    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPool(&pool);
    if (poolCount <= 0 || pool == NULL) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(9): pinned foreign pool is empty\n");
        return 9;
    }

    ComboContext_Init();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSettingsHash = 0x51A7E57Du; // nonzero: "profile recorded" (Lane B contract)
    static const uint32_t kMasterSeeds[] = { 0x00C0C0A1u, 0x00C0C0A2u, 0x00C0C0A3u, 0x00C0C0A4u, 0x00C0C0A5u };
    bool pairedGenerated = false;
    for (uint32_t masterSeed : kMasterSeeds) {
        gComboCtx.sharedRandoSeed = masterSeed;
        // The paired branch must IGNORE the user seed; leave a poison value to
        // prove it (a world generated from this string would name a different
        // spoiler file than the derived name asserted below).
        CVarSetString("gRando.InputSeed", "USERSEEDPOISON");
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        MM_Sram_InitNewSave();
        GameInteractor_ExecuteOnSaveInit(0);
        if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
            pairedGenerated = true;
            break;
        }
        fprintf(stderr, "[MM-RANDO-GEN] paired master seed %08X dead-ended; trying next\n", masterSeed);
    }
    if (!pairedGenerated) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(10): every paired master seed dead-ended\n");
        return 10;
    }

    // ADR 0010 increment 1.3: the placement count is now min(pool, reachable
    // eligible hosts) — under-supply places fewer rather than placing an
    // unreachable host. Zero placements would make every assertion below
    // vacuous, so that stays a hard failure.
    const int placedCount = Combo_CountForeignPlacements();
    const Rando::Foreign::PlacementStats& placeStats = Rando::Foreign::LastPlacementStats();
    const int expectedPlaced =
        poolCount < placeStats.reachableEligibleHosts ? poolCount : placeStats.reachableEligibleHosts;
    if (placedCount == 0 || placedCount != expectedPlaced || placedCount != placeStats.placed) {
        fprintf(stderr,
                "[MM-RANDO-GEN] FAIL(11): expected %d foreign placements (pool %d, reachable eligible hosts %d), "
                "found %d (test-side pairing key: sourceIsRando=%d settingsHash=%08X seed=%08X)\n",
                expectedPlaced, poolCount, placeStats.reachableEligibleHosts, placedCount,
                gComboCtx.sourceIsRando ? 1 : 0, gComboCtx.sharedRandoSettingsHash, gComboCtx.sharedRandoSeed);
        return 11;
    }

    // ======================================================================
    // Reachability-gate locks (ADR 0010 increment 1.3, #500 work item 2).
    //
    // (a) The closure itself is side-effect-free and deterministic in-process:
    //     computing it twice over the same world yields the identical set and
    //     leaves gSaveContext byte-identical (the memcpy swap discipline).
    //     The two-PROCESS half of the determinism lock is the mmReachable*
    //     lines MM_Rando_HeadlessForeignDigest folds into SeedDeterminism's
    //     byte-compare.
    // (b) Every placement the shipping code just recorded is a member of the
    //     closure recomputed from the post-fill save (asserted inside the
    //     placement loop below). Counterfactual: revert the reachability gate
    //     in Rando::Foreign::PlaceForeignItems and, for this pinned world,
    //     the ungated selection provably pins an unreachable host — premise
    //     (c) is what keeps that counterfactual deterministic rather than
    //     xorshift-lucky.
    // (c) Premise / non-vacuity: replay the UNGATED selection (the exact
    //     xorshift stream over the eligible-only candidate list the pre-gate
    //     code used) and assert it picks at least one host OUTSIDE the
    //     closure. If a master-seed change ever breaks this premise, pin a
    //     seed that restores it — do not delete the premise, it is what makes
    //     lock (b) falsifiable.
    // ======================================================================
    auto reachSnapshot = std::make_unique<SaveContext>();
    memcpy(reachSnapshot.get(), &gSaveContext, sizeof(SaveContext));
    const std::set<RandoCheckId> reachable = Rando::Logic::ComputeReachableCheckSet();
    if (memcmp(reachSnapshot.get(), &gSaveContext, sizeof(SaveContext)) != 0) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(28): ComputeReachableCheckSet left gSaveContext mutated — the memcpy "
                        "swap discipline is broken\n");
        return 28;
    }
    if (reachable.empty() || Rando::Logic::ComputeReachableCheckSet() != reachable) {
        fprintf(stderr,
                "[MM-RANDO-GEN] FAIL(28): reachable-check closure empty or unstable across two in-process runs "
                "(%zu checks)\n",
                reachable.size());
        return 28;
    }
    fprintf(stderr, "[MM-RANDO-GEN] reachable-check closure: %zu checks\n", reachable.size());

    {
        // (c): the ungated candidate universe, then the shipping selection
        // stream replayed over it. Stream recipe and step mirror
        // Rando/Foreign.cpp (seed string ":foreign-v1", xorshift32 13/17/5,
        // zero displaced to 0xB5297A4D); if that recipe ever changes, this
        // probe fails loudly and moves with it.
        std::vector<RandoCheckId> ungated;
        int unreachableEligible = 0;
        for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
            if (Rando::Foreign::IsEligibleHost(randoCheckId)) {
                ungated.push_back(randoCheckId);
                if (!reachable.contains(randoCheckId)) {
                    unreachableEligible++;
                }
            }
        }
        if (unreachableEligible == 0) {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(29): every eligible host is reachable in this world — the gate has "
                            "nothing to exclude and lock (b) is vacuous; pin a different master seed\n");
            return 29;
        }
        uint32_t simState = Ship_Hash(std::to_string(gComboCtx.sharedRandoSeed) + ":" +
                                      std::to_string(gComboCtx.sharedRandoSettingsHash) + ":" +
                                      std::to_string(gSaveContext.save.shipSaveInfo.rando.finalSeed) + ":foreign-v1");
        if (simState == 0) {
            simState = 0xB5297A4Du;
        }
        bool ungatedWouldStrand = false;
        for (int i = 0; i < poolCount && !ungated.empty(); i++) {
            uint32_t x = simState;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            simState = (x != 0) ? x : 0xB5297A4Du;
            const size_t pick = (size_t)(simState % (uint32_t)ungated.size());
            if (!reachable.contains(ungated[pick])) {
                ungatedWouldStrand = true;
            }
            ungated.erase(ungated.begin() + (std::ptrdiff_t)pick);
        }
        if (!ungatedWouldStrand) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(29): the ungated selection replay picks only reachable hosts for this seed "
                    "(%d of %d eligible hosts unreachable) — reverting the gate would not go red; pin a master seed "
                    "whose ungated pick strands\n",
                    unreachableEligible, placeStats.eligibleHosts);
            return 29;
        }
        fprintf(stderr,
                "[MM-RANDO-GEN] gate premise holds: %d of %d eligible hosts unreachable, ungated replay would have "
                "stranded a foreign item\n",
                unreachableEligible, placeStats.eligibleHosts);
    }
    // ADR 0002: hosting checks keep a legal MM item (junk-class) in the MM
    // save table; every recorded placement carries the OoT origin tag.
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        const ComboForeignPlacement& p = gComboCtx.foreignPlacements[i];
        if (p.item.originGame == GAME_NONE) {
            continue;
        }
        if (p.item.originGame != GAME_OOT || Combo_GetForeignItemName(p.item) == NULL) {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(12): placement slot %d not a named OoT-tagged pool item\n", i);
            return 12;
        }
        const RandoCheckId hostCheck = (RandoCheckId)p.mmCheckId;

        // #488, part 1 — the INDEPENDENT restatement, and the load-bearing one.
        //
        // A post-condition that calls IsEligibleHost cannot fail for a wrong
        // host rule: PlaceForeignItems selected these very checks by calling
        // IsEligibleHost, and nothing between selection and here mutates either
        // of its inputs (Rando::StaticData::Checks is static; the only
        // RANDO_SAVE_CHECKS writes in between are `.eligible` on two starting-
        // item rows, a field the predicate never reads). Loosen the predicate
        // and both sides move together. The condition this replaced —
        // "shuffled && holds a junk-class item" — had the same defect; it was
        // simply a textual copy of the selector rather than a call to it.
        //
        // So state the property #488 is actually about, read straight from the
        // static table with no reference to the predicate: the host belongs to
        // a check class whose `.eligible` bit is armed by game code. Tier A is
        // RCTYPE_CHEST carrying FLAG_CYCL_SCENE_CHEST (z_en_box.c ->
        // Flags_SetTreasure -> OnFlagSet). If someone widens IsEligibleHost
        // without widening the audit, THIS is the assertion that catches it, on
        // an actual paired fill rather than the synthetic table the ROM-free
        // ForeignHostEligibility lock uses.
        const auto staticIt = Rando::StaticData::Checks.find(hostCheck);
        if (staticIt == Rando::StaticData::Checks.end() || staticIt->second.randoCheckType != RCTYPE_CHEST ||
            staticIt->second.flagType != FLAG_CYCL_SCENE_CHEST) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(12): hosting check %u is not a Tier A (chest/FLAG_CYCL_SCENE_CHEST) host — "
                    "its .eligible bit is not game-armed, so this placement would strand\n",
                    (unsigned)p.mmCheckId);
            return 12;
        }

        // #488, part 2 — the cheap consistency tie-back. Tautological w.r.t.
        // the selector as argued above, and kept anyway for the one thing it
        // does catch: a placement whose mmCheckId did not survive the u16
        // round-trip through the placement table would fail the predicate here
        // even though part 1's static lookup might land on some other real row.
        // (Calls the C++ predicate directly rather than the
        // MM_Rando_Foreign_IsEligibleHost bridge: this is an MM C++ TU that
        // already includes Foreign.h, and the bridge is a one-line forwarder to
        // exactly this function. The bridge exists for src/common, which cannot
        // see the namespace.)
        if (!Rando::Foreign::IsEligibleHost(hostCheck)) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(12): hosting check %u is a chest row but fails the host predicate (held item "
                    "%d, skipped=%d) — placement/table inconsistency\n",
                    (unsigned)p.mmCheckId, (int)RANDO_SAVE_CHECKS[hostCheck].randoItemId,
                    RANDO_SAVE_CHECKS[hostCheck].skipped ? 1 : 0);
            return 12;
        }

        // ADR 0010 increment 1.3, lock (b): the placement the SHIPPING code
        // recorded is in the closure recomputed from the post-fill save. RED
        // with the gate reverted — premise (c) above proved the ungated
        // stream picks an unreachable host for this pinned world.
        if (!reachable.contains(hostCheck)) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(30): hosting check %u (%s) is OUTSIDE the reachable-check closure — the "
                    "reachability gate (ADR 0010 increment 1.3) is not being applied and this foreign item is "
                    "stranded\n",
                    (unsigned)p.mmCheckId, staticIt->second.name);
            return 30;
        }
    }

    // The spoiler landed under the DERIVED paired name and describes every
    // crossing: "foreign" as {checkName: {originGame, item}}, and the human-
    // readable checks list reads as the foreign item, not junk.
    std::string pairedSpoilerPath =
        Rando::Spoiler::SpoilerDirectory() + "/RSBSPAIR" + std::to_string(gComboCtx.sharedRandoSeed) + ".json";
    std::ifstream pairedFile(pairedSpoilerPath);
    if (!pairedFile.is_open()) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(13): paired spoiler missing at %s (seed derivation broken?)\n",
                pairedSpoilerPath.c_str());
        return 13;
    }
    nlohmann::json pairedSpoiler;
    try {
        pairedFile >> pairedSpoiler;
    } catch (const std::exception& e) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(13): paired spoiler unparseable: %s\n", e.what());
        return 13;
    }
    if (!pairedSpoiler.contains("foreign") || !pairedSpoiler["foreign"].is_object() ||
        (int)pairedSpoiler["foreign"].size() != placedCount) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(14): spoiler 'foreign' section missing or wrong size\n");
        return 14;
    }
    // Shortfall bookkeeping must be consistent with the world: a full
    // placement writes NO shortfall record; a short one writes the loud one
    // (that branch is driven deterministically by the under-supply phase
    // below).
    if (placedCount == poolCount && pairedSpoiler.contains("foreignShortfall")) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(14): fully-placed world wrote a foreignShortfall record\n");
        return 14;
    }
    if (placedCount < poolCount && !pairedSpoiler.contains("foreignShortfall")) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(14): short-placed world wrote no foreignShortfall record\n");
        return 14;
    }
    for (auto& [checkName, entry] : pairedSpoiler["foreign"].items()) {
        if (!entry.contains("originGame") || entry["originGame"] != "OOT" || !entry.contains("item")) {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(14): foreign entry '%s' malformed\n", checkName.c_str());
            return 14;
        }
        const std::string itemName = entry["item"];
        if (!pairedSpoiler["checks"].contains(checkName) ||
            pairedSpoiler["checks"][checkName] != itemName + " (Ocarina of Time)") {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(14): checks entry for '%s' does not read as the foreign item\n",
                    checkName.c_str());
            return 14;
        }
    }
    fprintf(stderr, "[MM-RANDO-GEN] paired world: %d foreign placements, spoiler foreign section verified\n",
            placedCount);

    // ======================================================================
    // Spoiler-LOAD reconstruction lock (Lane C1 follow-up, #392). The paired
    // world just generated wrote gComboCtx.foreignPlacements AND a spoiler with
    // a "foreign" section. Prove the spoiler-LOAD path
    // (Rando::Spoiler::ReconstructForeignPlacements, the counterpart of
    // generation's PlaceForeignItems, reached through ApplyToSaveContext)
    // rebuilds those placements exactly — the gap the C1 landing on #392 noted,
    // where a loaded paired world degraded its foreign checks to junk — while
    // never disturbing redeemed cross-game state.
    // ======================================================================
    ComboForeignPlacement generated[RSBS_FOREIGN_PLACEMENT_CAP];
    memcpy(generated, gComboCtx.foreignPlacements, sizeof(generated));

    // Re-read the just-written spoiler through the REAL LoadFromFile validator.
    nlohmann::json loadedSpoiler;
    try {
        loadedSpoiler =
            Rando::Spoiler::LoadFromFile(std::string("RSBSPAIR") + std::to_string(gComboCtx.sharedRandoSeed) + ".json");
    } catch (const std::exception& e) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(16): loading the paired spoiler threw: %s\n", e.what());
        return 16;
    }

    // Preserve-guard: with the live table still populated, reconstruction must
    // REFUSE (return -1) and leave it byte-identical — a spoiler load must never
    // clobber a live paired session's placements.
    if (Rando::Spoiler::ReconstructForeignPlacements(loadedSpoiler) != -1 ||
        memcmp(generated, gComboCtx.foreignPlacements, sizeof(generated)) != 0) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(16): reconstruction overwrote a live placement table instead of "
                        "preserving it\n");
        return 16;
    }

    // Plant a REDEEMED cross-game entry so the lock proves reconstruction never
    // touches sharedItemsTagged (redemption survives a spoiler load).
    SharedItem redeemed = {};
    redeemed.originGame = (uint8_t)GAME_OOT;
    redeemed.flags = RSBS_SHARED_ITEM_REDEEMED;
    redeemed.id = pool[0].item.id;
    gComboCtx.sharedItemsTagged[0] = redeemed;

    // Clear the table (the state a new-file spoiler load starts from, after
    // OnFileCreate's pre-apply clear) and reconstruct from the loaded spoiler.
    Combo_ClearForeignPlacements();
    const int reconstructed = Rando::Spoiler::ReconstructForeignPlacements(loadedSpoiler);
    if (reconstructed != placedCount) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(17): reconstructed %d placements, expected %d\n", reconstructed,
                placedCount);
        return 17;
    }

    // Reconstructed table must match the generated one EXACTLY (host check ->
    // origin-tagged item), order-independently (a JSON object has no slot order).
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        if (generated[i].item.originGame == GAME_NONE) {
            continue;
        }
        const SharedItem* got = Combo_GetForeignPlacementForCheck(generated[i].mmCheckId);
        if (got == NULL || got->originGame != generated[i].item.originGame || got->id != generated[i].item.id ||
            got->flags != generated[i].item.flags) {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(18): host check %u did not reconstruct to its generated item\n",
                    (unsigned)generated[i].mmCheckId);
            return 18;
        }
        // ADR 0002 host invariant on the load path: the MM save table keeps a
        // legal junk-class MM item at the host check (never RI_UNKNOWN / a raw
        // RG_*), so the check degrades to junk if the placement is ever absent.
        const RandoItemId heldAfterLoad = RANDO_SAVE_CHECKS[(RandoCheckId)generated[i].mmCheckId].randoItemId;
        if (Rando::StaticData::Items[heldAfterLoad].randoItemType != RITYPE_JUNK) {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(18): reconstructed host check %u holds a non-junk MM item (%d)\n",
                    (unsigned)generated[i].mmCheckId, (int)heldAfterLoad);
            return 18;
        }
    }
    if (Combo_CountForeignPlacements() != placedCount) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(18): reconstructed count %d != generated %d\n",
                Combo_CountForeignPlacements(), placedCount);
        return 18;
    }

    // Redemption survived untouched.
    if (memcmp(&gComboCtx.sharedItemsTagged[0], &redeemed, sizeof(SharedItem)) != 0) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(19): reconstruction mutated redeemed shared-item state\n");
        return 19;
    }

    // Malformed foreign section: refuse (throw) and leave the table untouched —
    // validate-then-commit is atomic, no partial population.
    {
        Combo_ClearForeignPlacements();
        nlohmann::json malformed = loadedSpoiler;
        for (auto& [checkName, entry] : malformed["foreign"].items()) {
            entry["item"] = "Definitely Not A Pinned Pool Item";
            break;
        }
        bool threw = false;
        try {
            Rando::Spoiler::ReconstructForeignPlacements(malformed);
        } catch (const std::exception&) { threw = true; }
        if (!threw || Combo_CountForeignPlacements() != 0) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(20): malformed foreign section not rejected atomically (threw=%d, "
                    "placements=%d)\n",
                    threw ? 1 : 0, Combo_CountForeignPlacements());
            return 20;
        }
    }

    // Absent foreign section: not an error; yields an empty table cleanly.
    {
        Combo_ClearForeignPlacements();
        nlohmann::json noForeign = loadedSpoiler;
        noForeign.erase("foreign");
        if (Rando::Spoiler::ReconstructForeignPlacements(noForeign) != 0 || Combo_CountForeignPlacements() != 0) {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(21): absent foreign section did not yield an empty table cleanly\n");
            return 21;
        }
    }

    // #488, step 6: a spoiler written by a PRE-tightening build can name a host
    // the current rule rejects — the old predicate accepted every non-shop
    // check type, and ~94% of its candidate pool was non-chest. Reconstruction
    // must drop that placement rather than rebuild a crossing the give path can
    // never deliver, and must leave the host holding the legal junk sentinel
    // (not the unresolvable RI_UNKNOWN that ApplyToSaveContext leaves at a
    // foreign host, which would arm `.eligible` and then give nothing).
    //
    // Without this leg the reject branch ships untested: the ROM-free
    // ForeignHostEligibility lock drives the predicate, not the load path.
    {
        Combo_ClearForeignPlacements();

        // First non-chest row in the table, chosen at runtime so this does not
        // pin a check name that a future table edit could remove.
        const Rando::StaticData::RandoStaticCheck* ineligible = nullptr;
        for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
            if (randoStaticCheck.randoCheckId != RC_UNKNOWN && randoStaticCheck.randoCheckType != RCTYPE_CHEST &&
                randoStaticCheck.randoCheckType != RCTYPE_SHOP &&
                randoStaticCheck.randoCheckType != RCTYPE_TINGLE_SHOP) {
                ineligible = &randoStaticCheck;
                break;
            }
        }
        if (ineligible == nullptr) {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(22): no non-chest check row to build the stale-host case from\n");
            return 22;
        }

        nlohmann::json staleHost = loadedSpoiler;
        nlohmann::json foreignEntry;
        for (auto& [checkName, entry] : staleHost["foreign"].items()) {
            foreignEntry = entry;
            break;
        }
        staleHost["foreign"] = nlohmann::json::object();
        staleHost["foreign"][ineligible->name] = foreignEntry;

        // The state ApplyToSaveContext leaves at a foreign host: shuffled, with
        // the "<item> (Ocarina of Time)" name unresolvable to a RandoItemId.
        RANDO_SAVE_CHECKS[ineligible->randoCheckId].randoItemId = RI_UNKNOWN;
        RANDO_SAVE_CHECKS[ineligible->randoCheckId].shuffled = true;
        RANDO_SAVE_CHECKS[ineligible->randoCheckId].skipped = false;

        const int staleReconstructed = Rando::Spoiler::ReconstructForeignPlacements(staleHost);
        if (staleReconstructed != 0 || Combo_CountForeignPlacements() != 0 ||
            Combo_GetForeignPlacementForCheck((uint16_t)ineligible->randoCheckId) != NULL) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(22): stale spoiler host %s (type %d) was reconstructed anyway (returned %d, "
                    "table holds %d)\n",
                    ineligible->name, (int)ineligible->randoCheckType, staleReconstructed,
                    Combo_CountForeignPlacements());
            return 22;
        }
        const RandoItemId heldAfterReject = RANDO_SAVE_CHECKS[ineligible->randoCheckId].randoItemId;
        if (Rando::StaticData::Items[heldAfterReject].randoItemType != RITYPE_JUNK || heldAfterReject == RI_UNKNOWN ||
            heldAfterReject == RI_NONE) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(22): rejected host %s left holding %d — must degrade to a legal junk item, "
                    "not a sentinel that arms .eligible and gives nothing\n",
                    ineligible->name, (int)heldAfterReject);
            return 22;
        }
    }

    // Wiring: the REAL apply path (ApplyToSaveContext, the spoiler-LOAD entry
    // point OnFileCreate's load branch invokes) must itself drive reconstruction
    // — not just the focused helper above. Clear, apply the whole spoiler, and
    // confirm the placements came back.
    Combo_ClearForeignPlacements();
    try {
        Rando::Spoiler::ApplyToSaveContext(loadedSpoiler);
    } catch (const std::exception& e) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(22): ApplyToSaveContext threw on the paired spoiler: %s\n", e.what());
        return 22;
    }
    if (Combo_CountForeignPlacements() != placedCount) {
        fprintf(stderr,
                "[MM-RANDO-GEN] FAIL(22): ApplyToSaveContext did not reconstruct foreign placements (found %d)\n",
                Combo_CountForeignPlacements());
        return 22;
    }

    // Restore the paired world's live placements for the OnSaveLoad phase below,
    // and clear the planted redeemed entry.
    Combo_ClearForeignPlacements();
    memcpy(gComboCtx.foreignPlacements, generated, sizeof(generated));
    memset(&gComboCtx.sharedItemsTagged[0], 0, sizeof(SharedItem));

    fprintf(stderr, "[MM-RANDO-GEN] spoiler-LOAD reconstruction verified: preserve-live, rebuild-exact, "
                    "redemption-safe, malformed-refused, absent-ok, stale-host-rejected, apply-wired\n");

    // ======================================================================
    // Under-supply lock (ADR 0010 increment 1.3 rule 3 — cap ≠ promise).
    // Deterministically shrink the reachable eligible host supply below the
    // pool size ON THE REAL PLACEMENT PATH: keep two of the gated run's own
    // hosts (reachable + eligible by construction), mark every other eligible
    // host user-excluded (.skipped — the bit GeneratePools writes for
    // excluded checks, which IsEligibleHost rejects), rerun PlaceForeignItems,
    // and assert it places exactly the supply — no throw, stats recorded,
    // spoiler carrying the loud shortfall record. Counterfactuals: restore
    // the pre-ADR-0010 shortfall-is-fatal throw and this phase dies in the
    // catch; drop the spoiler record and the foreignShortfall assert goes
    // red. Honest limit: this drives PlaceForeignItems directly, so
    // OnFileCreate's no-throw-on-shortfall stance is covered by review, not
    // by this phase (forcing a natural shortfall through the full
    // OnFileCreate chain would need exclusion-list fixtures whose fill
    // dead-end behavior is not deterministic).
    // ======================================================================
    {
        auto shortSnapshot = std::make_unique<SaveContext>();
        memcpy(shortSnapshot.get(), &gSaveContext, sizeof(SaveContext));

        std::set<RandoCheckId> keepers;
        for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP && (int)keepers.size() < 2; i++) {
            if (generated[i].item.originGame != GAME_NONE) {
                keepers.insert((RandoCheckId)generated[i].mmCheckId);
            }
        }
        if ((int)keepers.size() != 2 || poolCount <= 2) {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(31): under-supply premise broken (%zu keepers, pool %d)\n",
                    keepers.size(), poolCount);
            return 31;
        }
        for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
            if (Rando::Foreign::IsEligibleHost(randoCheckId) && !keepers.contains(randoCheckId)) {
                RANDO_SAVE_CHECKS[randoCheckId].skipped = true;
            }
        }

        int shortPlaced = -1;
        try {
            shortPlaced = Rando::Foreign::PlaceForeignItems();
        } catch (const std::exception& e) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(31): under-supply THREW instead of placing fewer (cap != promise): %s\n",
                    e.what());
            return 31;
        }
        const Rando::Foreign::PlacementStats& shortStats = Rando::Foreign::LastPlacementStats();
        if (shortPlaced != 2 || shortStats.placed != 2 || shortStats.requested != poolCount ||
            shortStats.reachableEligibleHosts != 2) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(31): under-supply placed %d (stats: placed %d, requested %d, reachable "
                    "eligible %d) — expected exactly the supply of 2\n",
                    shortPlaced, shortStats.placed, shortStats.requested, shortStats.reachableEligibleHosts);
            return 31;
        }
        for (RandoCheckId keeper : keepers) {
            if (Combo_GetForeignPlacementForCheck((uint16_t)keeper) == NULL) {
                fprintf(stderr, "[MM-RANDO-GEN] FAIL(31): under-supply run did not host on remaining supply check %u\n",
                        (unsigned)keeper);
                return 31;
            }
        }

        nlohmann::json shortSpoiler = Rando::Spoiler::GenerateFromSaveContext();
        if (!shortSpoiler.contains("foreignShortfall") || (int)shortSpoiler["foreign"].size() != 2 ||
            shortSpoiler["foreignShortfall"]["requested"] != poolCount ||
            shortSpoiler["foreignShortfall"]["placed"] != 2 ||
            shortSpoiler["foreignShortfall"]["reachableEligibleHosts"] != 2) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(32): spoiler does not carry the loud shortfall record (foreignShortfall) "
                    "for a short-placed world\n");
            return 32;
        }
        fprintf(stderr, "[MM-RANDO-GEN] under-supply verified: placed 2 of %d, shortfall recorded in the spoiler\n",
                poolCount);

        // Restore the world and the real run's placements for the phases below.
        memcpy(&gSaveContext, shortSnapshot.get(), sizeof(SaveContext));
        Combo_ClearForeignPlacements();
        memcpy(gComboCtx.foreignPlacements, generated, sizeof(generated));
    }

    // OnSaveLoad chain lock (Lane C1): dispatching the real save-load bridge
    // with the rando save live must arm the rando behaviors — the
    // OnSaveLoadHandler -> OnFileLoad -> COND_HOOK chain registers OnFlagSet
    // handlers in the MM-owned registry.
    const size_t flagHooksBefore = S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>();
    GameInteractor_ExecuteOnSaveLoad(0);
    const size_t flagHooksAfter = S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>();
    if (flagHooksAfter == 0) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(15): OnSaveLoad dispatch armed no OnFlagSet hooks (%zu -> %zu)\n",
                flagHooksBefore, flagHooksAfter);
        return 15;
    }
    fprintf(stderr, "[MM-RANDO-GEN] OnSaveLoad dispatch armed rando hooks (OnFlagSet: %zu -> %zu)\n", flagHooksBefore,
            flagHooksAfter);

    // ======================================================================
    // VB-dispatch phase (#392 VB follow-up) — the double-give lock, both
    // directions. GameInteractor_Should at MM call sites now resolves (macro
    // rebind, MM's GameInteractor.h) to MM_GameHooks_ExecuteVBShould, which
    // consults only the S2H::GameHooks registries. A rando-armed give VB
    // returning FALSE is exactly "the vanilla item is NOT also given" at the
    // call site (e.g. z_en_item00.c: `if (GameInteractor_Should(VB_..., true,
    // ...)) { give vanilla item }`); the crossing itself flows through the
    // CheckQueue give path instead. Probes use hooks whose bodies touch no
    // play state, so they are headless-safe:
    //  - VB_GIVE_ITEM_FROM_GREAT_FAIRY: COND_VB_SHOULD ForID leg
    //    (EnElfgrp.cpp, unconditional *should = false under IS_RANDO);
    //  - VB_GIVE_NEW_WAVE_BOSSA_NOVA: non-ID leg (ActorBehavior.cpp's
    //    MiscVanillaBehaviorHandler switch, *should = false);
    //  - VB_SETUP_TRANSITION: registered by nothing in the linked set — the
    //    caller's verdict must pass through untouched in BOTH polarities
    //    (this is also the ordinal-aliasing poster child: OoT's
    //    VB_PLAY_RAINBOW_BRIDGE_CS hooks must never answer it).
    // ======================================================================
    const size_t vbHooks = S2H::GameHooks::CountForTest<GameInteractor::ShouldVanillaBehavior>();
    const size_t actorInitHooks = S2H::GameHooks::CountForTest<GameInteractor::ShouldActorInit>();
    if (vbHooks == 0 || actorInitHooks == 0) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(23): OnSaveLoad armed no Should hooks (VB %zu, ShouldActorInit %zu)\n",
                vbHooks, actorInitHooks);
        return 23;
    }
    if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(24): rando-armed ForID give VB not suppressed — the vanilla item "
                        "would ALSO be given (double-give)\n");
        return 24;
    }
    if (GameInteractor_Should(VB_GIVE_NEW_WAVE_BOSSA_NOVA, true)) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(24): rando-armed non-ID give VB not suppressed — the vanilla item "
                        "would ALSO be given (double-give)\n");
        return 24;
    }
    if (!GameInteractor_Should(VB_SETUP_TRANSITION, true) || GameInteractor_Should(VB_SETUP_TRANSITION, false)) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(25): un-overridden VB did not pass the caller's verdict through\n");
        return 25;
    }
    fprintf(stderr,
            "[MM-RANDO-GEN] VB dispatch (rando save): give VBs suppressed (ForID + non-ID), "
            "pass-through intact (%zu VB hooks)\n",
            vbHooks);

    // Vanilla direction: a non-rando file re-run through the REAL OnSaveLoad
    // chain must disarm the overrides (the COND_* macros re-evaluate IS_RANDO
    // and unregister), and the same give VBs must return the vanilla verdict
    // again — VB dispatch must not disturb non-rando files.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(26): plain Sram_InitNewSave produced a rando save — vanilla-direction "
                        "premise broken\n");
        return 26;
    }
    GameInteractor_ExecuteOnSaveLoad(0);
    if (!GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true) ||
        GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, false) ||
        !GameInteractor_Should(VB_GIVE_NEW_WAVE_BOSSA_NOVA, true)) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(27): vanilla file's give VBs did not return the vanilla verdict — "
                        "rando overrides leaked into a non-rando file\n");
        return 27;
    }
    fprintf(stderr, "[MM-RANDO-GEN] VB dispatch (vanilla save): overrides disarmed, vanilla behavior unchanged\n");

    // Leave the shared context clean (this process may run more dispatches).
    ComboContext_Init();

    fprintf(stderr, "[MM-RANDO-GEN] PASS: %zu regions, %d shuffled checks, spoiler written + tagged\n",
            Rando::Logic::Regions.size(), shuffled);
    return 0;
}

/**
 * Foreign-placement determinism digest (Lane C1, #392) — the MM half of the
 * SeedDeterminism lock. Runs inside the rando-determinism dispatch AFTER the
 * OoT half generated and its LIVE producer stamped gComboCtx (this bridge
 * refuses to run otherwise), so the pairing key under test is the real
 * end-to-end one, not a test fixture. Generates the paired MM world through
 * the real dispatch chain and APPENDS the MM world identity + every foreign
 * placement to the digest file; CheckSeedDeterminism's two-process
 * byte-compare then covers OoT's fill AND the MM fill AND the foreign
 * placements in one diff.
 *
 * Since ADR 0010 increment 1.2 the generation itself carries the
 * deterministic attempt ladder (OnFileCreate), so a first-try dead-end
 * re-rolls deterministically rather than failing this bridge; the digest
 * folds gComboCtx.mmPairedAttempt so the two-process diff also locks WHICH
 * ladder rung both processes converged on. Exhaustion still fails loudly
 * (FAIL(3)) and the fix is still to pin a different determinism seed.
 *
 * Returns 0 on success; nonzero step codes with stderr markers otherwise.
 */
extern "C" int MM_Rando_HeadlessForeignDigest(const char* outPath) {
    if (!Combo_ForeignPairingActive()) {
        fprintf(stderr, "[MM-FOREIGN-DIGEST] FAIL(1): pairing key not stamped (OoT live producer did not run?)\n");
        return 1;
    }

    MM_Rando_Init();
    if (Rando::Logic::Regions.empty()) {
        fprintf(stderr, "[MM-FOREIGN-DIGEST] FAIL(2): Logic/Regions graph empty\n");
        return 2;
    }

    // Pin every NON-IDENTITY CVar this generation depends on — CVar state can
    // leak between dispatches through the shared config store, and
    // determinism must not hinge on which test ran first. No spoiler file:
    // the digest is the artifact, and skipping the write avoids the
    // same-path-overwrite pitfall the OoT digest documents.
    //
    // Deliberately NO option-table writes here (this used to pin RO_LOGIC to
    // Nearly No Logic): the OoT half of this dispatch already ran the
    // CREATION event, which stamped gComboCtx.mmProfileDigest from the live
    // CVars (#570), so flipping ANY profile input between that stamp and this
    // generation IS the post-creation divergence the machinery refuses — the
    // flipped Glitchless default (ADR 0010 increment 1.1) made the old NNL
    // pin exactly such a flip, and the refusal correctly killed this bridge.
    // The MM fill therefore runs under the same resolved profile the stamp
    // froze (all-defaults in CI => Glitchless), with the attempt ladder
    // absorbing any first-try dead-end deterministically.
    CVarSetInteger("gRando.Enabled", 1);
    CVarSetInteger("gRando.SpoilerFileIndex", 0);
    CVarSetInteger("gRando.GenerateSpoiler", 0);
    CVarSetString("gRando.InputSeed", "USERSEEDPOISON"); // paired branch must ignore this

    if (gRegEditor == NULL) {
        static RegEditor sDigestRegEditor = {};
        gRegEditor = &sDigestRegEditor;
    }

    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveInit(0);
    if (gSaveContext.save.shipSaveInfo.saveType != SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-FOREIGN-DIGEST] FAIL(3): paired MM fill dead-ended under the pinned seed — pin a "
                        "different determinism seed\n");
        return 3;
    }

    // The digest must describe an ACTUAL cross-game world: a paired
    // generation that placed nothing would produce a stable-but-empty digest,
    // turning lock (c) vacuous. Fail loudly instead. ADR 0010 increment 1.3:
    // the expected count is min(pool, reachable eligible hosts) — the
    // reachability gate may legitimately place fewer than the pool — but zero
    // stays fatal (vacuity).
    {
        const ComboForeignItemDef* digestPool = NULL;
        const int digestPoolCount = Combo_GetForeignItemPool(&digestPool);
        const Rando::Foreign::PlacementStats& digestStats = Rando::Foreign::LastPlacementStats();
        const int digestExpected =
            digestPoolCount < digestStats.reachableEligibleHosts ? digestPoolCount : digestStats.reachableEligibleHosts;
        if (Combo_CountForeignPlacements() == 0 || Combo_CountForeignPlacements() != digestExpected) {
            fprintf(stderr,
                    "[MM-FOREIGN-DIGEST] FAIL(5): expected %d foreign placements (pool %d, reachable eligible %d), "
                    "found %d\n",
                    digestExpected, digestPoolCount, digestStats.reachableEligibleHosts,
                    Combo_CountForeignPlacements());
            return 5;
        }
    }

    // Canonical MM placement blob in fixed check order (std::map), folded
    // through the project's FNV-1a — mirrors the OoT digest's placementHash.
    std::string blob;
    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        if (randoStaticCheck.randoCheckId == RC_UNKNOWN) {
            continue;
        }
        blob += std::to_string((int)randoCheckId);
        blob += ':';
        blob += std::to_string((int)RANDO_SAVE_CHECKS[randoCheckId].randoItemId);
        blob += ';';
    }
    const uint32_t mmPlacementHash = Ship_Hash(blob);

    // ADR 0010 increment 1.3: the reachable-check closure is now a GENERATION
    // INPUT (the foreign-host gate filters candidates by it), so the
    // determinism artifact folds it too. Two processes computing different
    // closures for the same world is exactly the divergence
    // CheckSeedDeterminism's byte-compare exists to catch — this is the
    // two-process half of the callable's determinism lock (the in-process
    // half lives in MM_Rando_HeadlessGenTest's paired phase).
    std::string reachableBlob;
    const std::set<RandoCheckId> digestReachable = Rando::Logic::ComputeReachableCheckSet();
    for (RandoCheckId randoCheckId : digestReachable) {
        reachableBlob += std::to_string((int)randoCheckId);
        reachableBlob += ';';
    }
    const uint32_t mmReachableHash = Ship_Hash(reachableBlob);

    FILE* out = stdout;
    bool closeOut = false;
    if (outPath != NULL && outPath[0] != '\0') {
        out = fopen(outPath, "a"); // APPEND: the OoT half wrote the file first
        if (out == NULL) {
            fprintf(stderr, "[MM-FOREIGN-DIGEST] FAIL(4): cannot open digest output '%s'\n", outPath);
            return 4;
        }
        closeOut = true;
    }
    fprintf(out,
            "mmFinalSeed=%08X\n"
            "mmPlacementHash=%08X\n"
            "mmReachableCount=%zu\n"
            "mmReachableHash=%08X\n"
            "foreignCount=%d\n"
            // ADR 0010 increment 1.2: the winning ladder attempt joins the
            // two-process diff, so both processes must not merely reach the
            // same world — they must reach it via the same derivation.
            "mmPairedAttempt=%u\n",
            gSaveContext.save.shipSaveInfo.rando.finalSeed, mmPlacementHash, digestReachable.size(), mmReachableHash,
            Combo_CountForeignPlacements(), (unsigned)gComboCtx.mmPairedAttempt);
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        const ComboForeignPlacement& p = gComboCtx.foreignPlacements[i];
        if (p.item.originGame == GAME_NONE) {
            continue;
        }
        fprintf(out, "foreign%d=%u:%u:%u\n", i, (unsigned)p.mmCheckId, (unsigned)p.item.originGame,
                (unsigned)p.item.id);
    }
    if (closeOut) {
        fclose(out);
    }
    fprintf(stderr, "[MM-FOREIGN-DIGEST] mmFinalSeed=%08X mmPlacementHash=%08X mmReachableHash=%08X foreign=%d\n",
            gSaveContext.save.shipSaveInfo.rando.finalSeed, mmPlacementHash, mmReachableHash,
            Combo_CountForeignPlacements());
    return 0;
}

// ============================================================================
// Switch-entry pairing lock (#439)
// ============================================================================
/**
 * The harness gap #439 was filed for. MMRandoGen's paired phase drives the
 * OnSaveInit chain DIRECTLY, so it proved generation works while the only
 * flow a player actually takes — walking into the Happy Mask Shop — never
 * reached that chain at all. Everything below drives the SWITCH-ENTRY path:
 * the cold gamestate-chain boot the switch performs, then the real
 * MM_Play_ConsumeStartupEntrance consumption point, with no direct call to
 * the generation entry point anywhere in the success path.
 *
 * Three phases, matching the entry-path matrix in #439:
 *
 *  1. fresh switch-entry + live paired OoT world -> the paired MM world
 *     activates (rando save, foreign placements, spoiler with a foreign
 *     section) AND the IS_RANDO behavior hooks end up armed. The boot chain
 *     dispatches OnSaveLoad against its vanilla bootstrap file, which
 *     DISARMS them; if nothing re-dispatches afterwards, MM plays vanilla
 *     even with a perfectly generated world. Locked here in both halves.
 *
 *  2. return leg (frozen paired save) -> restored, NOT regenerated
 *     (finalSeed and a save sentinel both survive), hooks armed again.
 *
 *  3. return leg with an EXISTING VANILLA MM save -> left strictly alone.
 *     This is the "never silently modify the player's file" contract: a
 *     paired OoT world does not entitle anything to rewrite a save that
 *     already exists.
 *
 * Returns 0 on success, a distinct nonzero step code otherwise.
 */
extern "C" int MM_Rando_HeadlessPairSwitchEntry(void) {
    const uint16_t kArrival = 0xD800; // ENTRANCE(SOUTH_CLOCK_TOWN, 0) — the OoT->MM arrival

    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(1): Ship::Context not live\n");
        return 1;
    }

    MM_Rando_Init();
    if (Rando::Logic::Regions.empty()) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(2): Logic/Regions graph empty\n");
        return 2;
    }

    if (gRegEditor == NULL) {
        static RegEditor sPairRegEditor = {};
        gRegEditor = &sPairRegEditor;
    }

    // gRando.Enabled stays OFF on purpose: MM's rando menu is link-elided in
    // the single exe, so the paired OoT generation is the ONLY opt-in. If
    // activation ever starts depending on this CVar, this row fails.
    CVarSetInteger("gRando.Enabled", 0);
    CVarSetInteger("gRando.GenerateSpoiler", 1);
    CVarSetString("gRando.InputSeed", "USERSEEDPOISON"); // the paired branch must ignore this
    CVarSetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_NEARLY_NO_LOGIC);
    // A stale SpoilerFileIndex must not divert the paired branch into LOADING
    // an old world — pin the hostile value the real session can carry.
    CVarSetInteger("gRando.SpoilerFileIndex", 3);

    const ComboForeignItemDef* pool = NULL;
    const int poolCount = Combo_GetForeignItemPool(&pool);
    if (poolCount <= 0 || pool == NULL) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(3): pinned foreign pool is empty\n");
        return 3;
    }

    // ----------------------------------------------------------------------
    // Phase 1 — fresh switch-entry into a live paired OoT world.
    // ----------------------------------------------------------------------
    // A fixed master-seed list, for the same reason MMRandoGen keeps one: the
    // MM fill can genuinely dead-end on an unlucky seed, and the claim under
    // test is that the switch-entry path REACHES generation, not that any
    // particular seed fills. Deterministic either way (fixed list, fixed fill).
    static const uint32_t kMasterSeeds[] = { 2108649350u, 1234567u, 77777777u, 424242u, 999983u };
    uint32_t usedMasterSeed = 0;

    for (uint32_t masterSeed : kMasterSeeds) {
        // Lane B's carrier, stamped as a live OoT generation stamps it.
        Combo_ClearForeignPlacements();
        Combo_ClearFrozenState("mm");
        Combo_ClearStartupEntrance();
        gComboCtx.sourceIsRando = 1;
        gComboCtx.sharedRandoSeed = masterSeed;
        gComboCtx.sharedRandoSettingsHash = 0x5DAD32CEu;
        // Each iteration models a fresh LEGACY (pre-freeze) pair: no creation
        // stamp, so the arrival identity gate (#498/#564) takes the
        // freeze-at-first-crossing path rather than comparing against a stale
        // digest a previous iteration froze.
        gComboCtx.mmProfileDigest = 0;

        // The cold gamestate-chain boot a switch performs: ConsoleLogo skips
        // to TitleSetup, TitleSetup authors a VANILLA bootstrap file with
        // MM_Sram_InitNewSave and dispatches OnSaveLoad against it. File
        // select is never touched, so OnSaveInit is never dispatched here —
        // that absence is the whole bug.
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        MM_Sram_InitNewSave();
        GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);

        // Arm state through a VB VERDICT, not a hook count (#491). COND_HOOK's
        // Unregister is DEFERRED (mm_game_hooks.h: the id is queued and erased
        // only by FlushPendingUnregistrations at the next Execute of that hook
        // type), so CountForTest lags a disarm — as a disarm signal a count is
        // simply wrong, and as an arm signal a "> baseline" comparison against
        // a stale non-zero baseline is satisfiable without the hooks being
        // live. GameInteractor_Should dispatches Execute<ShouldVanillaBehavior>,
        // which applies the flush first, so the verdict is current. Same probe
        // MMReloadArmState's Phase 4 and MMMoonCrashArmState use.
        //
        // The boot chain above dispatched OnSaveLoad against a VANILLA
        // bootstrap, so the IS_RANDO give override must be DOWN here.
        // Asserting that is what makes the re-arm assertion after the consume
        // non-vacuous — previously the "arm" leg could pass on state left
        // armed by an earlier row in the same process.
        if (!GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
            fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(19): the boot chain did not disarm the IS_RANDO give override — "
                            "the re-arm assertion below would be vacuous\n");
            return 19;
        }

        // The switch path's pending arrival, then the real consumption point.
        Combo_SetStartupEntrance(kArrival);
        MM_Play_ConsumeStartupEntrance();

        if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
            if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
                fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(5): paired world generated but the IS_RANDO hooks were left "
                                "DISARMED by the boot chain's OnSaveLoad — MM would play with vanilla behavior in a "
                                "rando world\n");
                return 5;
            }
            fprintf(stderr, "[MM-PAIR-SWITCH] hooks re-armed at consumption (give VB suppressed)\n");
            usedMasterSeed = masterSeed;
            break;
        }

        // Discriminator: did the switch-entry path fail to REACH generation,
        // or did this seed's fill dead-end? Re-run the same gComboCtx through
        // the DIRECT chain MMRandoGen uses. If the direct chain pairs and the
        // switch-entry path did not, the dispatch site is gone — the exact
        // #439 regression, and a distinct, unambiguous failure.
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        MM_Sram_InitNewSave();
        Combo_ClearForeignPlacements();
        GameInteractor_ExecuteOnSaveInit(gSaveContext.fileNum);
        if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
            fprintf(stderr,
                    "[MM-PAIR-SWITCH] FAIL(4): the direct OnSaveInit chain pairs under master seed %u but "
                    "SWITCH-ENTRY did not — MM_Play_ConsumeStartupEntrance no longer reaches generation (#439)\n",
                    masterSeed);
            return 4;
        }
        fprintf(stderr, "[MM-PAIR-SWITCH] master seed %u dead-ended on both paths (fill); trying next\n", masterSeed);
    }

    if (usedMasterSeed == 0) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(6): every pinned master seed dead-ended the MM fill — pin a "
                        "different seed (generation machinery itself is covered by MMRandoGen)\n");
        return 6;
    }
    fprintf(stderr, "[MM-PAIR-SWITCH] switch-entry paired under master seed %u\n", usedMasterSeed);

    // ADR 0010 increment 1.3: expected count is min(pool, reachable eligible
    // hosts), never zero; and every placement this pinned-seed row wrote must
    // be inside the reachable closure recomputed from the arrived save — the
    // placements-in-reachable-set assertion extended over the switch-entry
    // pinned seed, per the ADR's increment-1 lock list.
    const int pairSwitchPlaced = Combo_CountForeignPlacements();
    {
        const Rando::Foreign::PlacementStats& switchStats = Rando::Foreign::LastPlacementStats();
        const int switchExpected =
            poolCount < switchStats.reachableEligibleHosts ? poolCount : switchStats.reachableEligibleHosts;
        if (pairSwitchPlaced == 0 || pairSwitchPlaced != switchExpected) {
            fprintf(stderr,
                    "[MM-PAIR-SWITCH] FAIL(7): expected %d foreign placements after switch-entry pairing (pool %d, "
                    "reachable eligible %d), found %d\n",
                    switchExpected, poolCount, switchStats.reachableEligibleHosts, pairSwitchPlaced);
            return 7;
        }
        const std::set<RandoCheckId> switchReachable = Rando::Logic::ComputeReachableCheckSet();
        for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
            const ComboForeignPlacement& p = gComboCtx.foreignPlacements[i];
            if (p.item.originGame == GAME_NONE) {
                continue;
            }
            if (!switchReachable.contains((RandoCheckId)p.mmCheckId)) {
                fprintf(stderr,
                        "[MM-PAIR-SWITCH] FAIL(7): hosting check %u is outside the reachable-check closure — the "
                        "reachability gate (ADR 0010 increment 1.3) did not hold on the switch-entry path\n",
                        (unsigned)p.mmCheckId);
                return 7;
            }
        }
    }
    if (gSaveContext.save.entrance != kArrival) {
        fprintf(stderr,
                "[MM-PAIR-SWITCH] FAIL(8): arrival entrance 0x%04X lost to generation's start state "
                "(save.entrance=0x%04X)\n",
                kArrival, gSaveContext.save.entrance);
        return 8;
    }

    // The spoiler must exist AND be findable through the module's own
    // directory accessor (#439 second bug: the write path resolved against a
    // second app directory that portable installs never create, so even a
    // working pairing produced no file the operator could find).
    const std::string pairedSpoiler =
        Rando::Spoiler::SpoilerDirectory() + "/RSBSPAIR" + std::to_string(usedMasterSeed) + ".json";
    {
        std::ifstream sf(pairedSpoiler);
        if (!sf.is_open()) {
            fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(9): no MM spoiler at %s after switch-entry pairing\n",
                    pairedSpoiler.c_str());
            return 9;
        }
        nlohmann::json j;
        try {
            sf >> j;
        } catch (const std::exception& e) {
            fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(9): paired spoiler unparseable: %s\n", e.what());
            return 9;
        }
        if (!j.contains("foreign") || !j["foreign"].is_object() || (int)j["foreign"].size() != pairSwitchPlaced) {
            fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(10): spoiler 'foreign' section missing or wrong size\n");
            return 10;
        }
    }
    fprintf(stderr, "[MM-PAIR-SWITCH] spoiler verified at %s\n", pairedSpoiler.c_str());

    // ----------------------------------------------------------------------
    // Phase 2 — return leg: an existing PAIRED save is restored, not remade.
    // ----------------------------------------------------------------------
    const u32 pairedFinalSeed = gSaveContext.save.shipSaveInfo.rando.finalSeed;
    ComboForeignPlacement pairedPlacements[RSBS_FOREIGN_PLACEMENT_CAP];
    memcpy(pairedPlacements, gComboCtx.foreignPlacements, sizeof(pairedPlacements));

    // Progress the player made before leaving for OoT.
    gSaveContext.save.saveInfo.playerData.rupees = 142;
    gSaveContext.save.day = 3;
    Combo_FreezeState("mm", kArrival, &gSaveContext, sizeof(gSaveContext));

    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    // Same VB-verdict disarm assertion as the arrival leg (#491): the return
    // leg's own boot chain must have taken the hooks DOWN, or the re-arm
    // assertion below proves nothing.
    if (!GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(20): the return leg's boot chain did not disarm the IS_RANDO give "
                        "override — the re-arm assertion below would be vacuous\n");
        return 20;
    }
    Combo_SetStartupEntrance(kArrival);
    MM_Play_ConsumeStartupEntrance();

    if (gSaveContext.save.shipSaveInfo.saveType != SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(11): return leg lost the paired save type\n");
        return 11;
    }
    if (gSaveContext.save.shipSaveInfo.rando.finalSeed != pairedFinalSeed) {
        fprintf(stderr,
                "[MM-PAIR-SWITCH] FAIL(12): return leg REGENERATED the world (finalSeed %08X -> %08X) — an "
                "existing MM save must never be re-paired\n",
                pairedFinalSeed, gSaveContext.save.shipSaveInfo.rando.finalSeed);
        return 12;
    }
    if (gSaveContext.save.saveInfo.playerData.rupees != 142 || gSaveContext.save.day != 3) {
        fprintf(stderr,
                "[MM-PAIR-SWITCH] FAIL(13): return leg did not preserve the player's progress "
                "(rupees=%d day=%d)\n",
                gSaveContext.save.saveInfo.playerData.rupees, gSaveContext.save.day);
        return 13;
    }
    if (memcmp(pairedPlacements, gComboCtx.foreignPlacements, sizeof(pairedPlacements)) != 0) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(14): return leg disturbed the foreign placement table\n");
        return 14;
    }
    if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(15): return leg left the IS_RANDO hooks disarmed — a restored rando "
                        "save would play with vanilla behavior\n");
        return 15;
    }
    fprintf(stderr, "[MM-PAIR-SWITCH] return leg: restored, not regenerated, hooks re-armed\n");

    // ----------------------------------------------------------------------
    // Phase 3 — an EXISTING VANILLA MM save is never silently converted.
    // ----------------------------------------------------------------------
    // gComboCtx still carries the live paired keying, so pairing is "possible"
    // here in every sense except the one that matters: this file already
    // exists and belongs to the player.
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    gSaveContext.save.saveInfo.playerData.rupees = 77;
    gSaveContext.save.day = 2;
    Combo_FreezeState("mm", kArrival, &gSaveContext, sizeof(gSaveContext));

    ComboForeignPlacement beforeVanilla[RSBS_FOREIGN_PLACEMENT_CAP];
    memcpy(beforeVanilla, gComboCtx.foreignPlacements, sizeof(beforeVanilla));

    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    Combo_SetStartupEntrance(kArrival);
    MM_Play_ConsumeStartupEntrance();

    if (gSaveContext.save.shipSaveInfo.saveType != SAVETYPE_VANILLA) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(16): an existing VANILLA MM save was silently converted to rando "
                        "on arrival — existing files must be skipped with a logged reason\n");
        return 16;
    }
    if (gSaveContext.save.saveInfo.playerData.rupees != 77 || gSaveContext.save.day != 2) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(17): existing vanilla save was modified (rupees=%d day=%d)\n",
                gSaveContext.save.saveInfo.playerData.rupees, gSaveContext.save.day);
        return 17;
    }
    if (memcmp(beforeVanilla, gComboCtx.foreignPlacements, sizeof(beforeVanilla)) != 0) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(18): skipping a vanilla save still disturbed the placement table\n");
        return 18;
    }
    // The skip path must also leave the OVERRIDES down, in both polarities: a
    // save that stayed vanilla while the rando give override remained armed
    // from the paired legs above would play a rando behavior set on a vanilla
    // file. Both-polarity form because a one-sided check passes on a hook that
    // answers unconditionally.
    if (!GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true) ||
        GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, false)) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(21): the skipped vanilla save left the rando give override armed — "
                        "rando behavior leaked onto a non-rando file\n");
        return 21;
    }
    fprintf(stderr, "[MM-PAIR-SWITCH] existing vanilla save left untouched (skip path)\n");

    // ----------------------------------------------------------------------
    // Phase 4 — the arrival identity gate (#498 decision 1 per #564, phase 2
    // step 9): a creation-stamped pair whose inputs diverged by arrival is
    // REFUSED — no world generated, stamp never self-healed, the active slot
    // latched and surfaced REFUSED through the #533 machinery — while a
    // matching arrival still generates.
    //
    // THE COUNTERFACTUALS THIS LOCKS — TWO LAYERS, each independently red
    // (both measured, 2026-07-31, not asserted):
    //
    //  - Revert the digest compare in MM_Rando_PairOnCrossGameArrival ALONE
    //    and FAIL(24)/FAIL(26) go red, NOT FAIL(22): the arrival dispatches
    //    generation, ResolvePairedProfile's backstop throws, and OnFileCreate's
    //    catch reverts to vanilla — so no divergent world is authored, but the
    //    refusal degrades into #564 V7's silent vanilla revert. No REFUSED
    //    state, no reason, no write latch: the healthy pair's .redsave is left
    //    open to this session's captures. That is what the arrival gate buys
    //    over the backstop, and it is what these assertions measure.
    //  - Revert BOTH compares (arrival gate + ResolvePairedProfile) and
    //    FAIL(22) goes red: the divergent leg GENERATES a world under the
    //    mid-session-edited CVars — a post-creation edit silently changing the
    //    paired world, exactly what the one-game ruling forbids.
    // ----------------------------------------------------------------------
    const char* const kPhase4SaveDir = "rsbs_test_pairswitch_saves";
    rsbs::SaveManager::Instance().SetSaveDirectory(kPhase4SaveDir);
    RsbsSave_ResetSlotSessionState();
    RsbsSave_ArmSlotOnCreate(0); // the create seam: slot 0 is this session's
    RsbsSave_SetActiveSlot(0);
    if (!RsbsSave_IsSlotWritable(0)) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(30): phase 4 could not arm its slot fixture\n");
        return 30;
    }

    // 4a. The refuse leg. A fresh pair "created" under the CVars in force
    // since Phase 1: the stamp is computed exactly as Playthrough_Init
    // computes it (same MM_Rando_ComputeProfileStamp call).
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    Combo_ClearForeignPlacements();
    gComboCtx.sourceIsRando = 1;
    gComboCtx.sharedRandoSeed = usedMasterSeed;
    gComboCtx.sharedRandoSettingsHash = 0x5DAD32CEu;
    gComboCtx.mmProfileDigest = MM_Rando_ComputeProfileStamp();
    const uint32_t phase4Stamp = gComboCtx.mmProfileDigest;
    if (phase4Stamp == 0) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(31): creation stamp computed as zero\n");
        return 31;
    }

    // The mid-session divergence: an option edit AFTER creation.
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_COWS].cvar, RO_GENERIC_ON);

    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    Combo_SetStartupEntrance(kArrival);
    MM_Play_ConsumeStartupEntrance();

    if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(22): a DIVERGENT arrival generated a world — a mid-session option "
                        "edit changed the paired world instead of being refused (#498/#564)\n");
        return 22;
    }
    if (gComboCtx.mmProfileDigest != phase4Stamp) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(23): the refusal self-healed the creation stamp (%08X -> %08X)\n",
                phase4Stamp, gComboCtx.mmProfileDigest);
        return 23;
    }
    if (RsbsSave_GetSlotState(0) != (int)RSBS_SLOT_REFUSED) {
        fprintf(stderr,
                "[MM-PAIR-SWITCH] FAIL(24): the refused arrival did not surface REFUSED on the active slot "
                "(state=%d)\n",
                RsbsSave_GetSlotState(0));
        return 24;
    }
    if (RsbsSave_GetSlotRefuseReason(0) != (int)RSBS_REFUSE_IDENTITY) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(25): refusal reason is %d, expected RSBS_REFUSE_IDENTITY\n",
                RsbsSave_GetSlotRefuseReason(0));
        return 25;
    }
    if (RsbsSave_IsSlotWritable(0)) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(26): the refused arrival left the slot writable — the divergent "
                        "session could capture its unpaired world into the pair's .redsave\n");
        return 26;
    }
    fprintf(stderr, "[MM-PAIR-SWITCH] divergent arrival refused: no world, stamp intact, slot latched + surfaced\n");

    // 4b. The match leg: same pair, divergence undone — the gate must not
    // refuse a faithful arrival. (Same master seed and CVars as Phase 1, so
    // the fill is known to succeed.)
    CVarClear(Rando::StaticData::Options[RO_SHUFFLE_COWS].cvar);
    RsbsSave_ResetSlotSessionState();
    RsbsSave_ArmSlotOnCreate(0);
    RsbsSave_SetActiveSlot(0);
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    Combo_ClearForeignPlacements();
    gComboCtx.mmProfileDigest = MM_Rando_ComputeProfileStamp();
    if (gComboCtx.mmProfileDigest != phase4Stamp) {
        fprintf(stderr,
                "[MM-PAIR-SWITCH] FAIL(27): undoing the divergence did not restore the creation profile "
                "(%08X vs %08X) — the identity computation is unstable\n",
                gComboCtx.mmProfileDigest, phase4Stamp);
        return 27;
    }

    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    Combo_SetStartupEntrance(kArrival);
    MM_Play_ConsumeStartupEntrance();

    if (gSaveContext.save.shipSaveInfo.saveType != SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(28): a MATCHING arrival was refused or dead-ended — the identity "
                        "gate must pass a faithful arrival through to generation\n");
        return 28;
    }
    if (!RsbsSave_IsSlotWritable(0)) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(29): a matching arrival latched the slot anyway\n");
        return 29;
    }
    fprintf(stderr, "[MM-PAIR-SWITCH] matching arrival generated normally under the frozen profile\n");

    // Leave clean global state for later dispatches in the same process.
    RsbsSave_DeleteSave(0);
    RsbsSave_ResetSlotSessionState();
    RsbsSave_SetActiveSlot(-1);
    rsbs::SaveManager::Instance().SetSaveDirectory("Save"); // the documented harness default
    {
        std::error_code phase4Ec;
        std::filesystem::remove_all(kPhase4SaveDir, phase4Ec);
    }
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    Combo_ClearForeignPlacements();
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();

    fprintf(stderr, "[MM-PAIR-SWITCH] PASS: pairing activates on the switch-entry path, existing saves untouched, "
                    "and a divergent arrival refuses through the #533 surface\n");
    return 0;
}

// ============================================================================
// Non-arrival entry-path arm-state lock (#439 follow-up)
// ============================================================================
/**
 * MMPairSwitchEntry above locks the CROSS-GAME arrival convergence
 * (MM_Play_ConsumeStartupEntrance re-dispatching OnSaveLoad). This row locks
 * the OTHER convergence points into MM gameplay the #439 follow-up audit
 * enumerated — the ones the arrival fix deliberately does NOT touch — against
 * the same failure the arrival path had: a save reaching a live PlayState with
 * the IS_RANDO COND_HOOKs in the wrong arm state.
 *
 * IS_RANDO (Rando.h) is `gSaveContext.save.shipSaveInfo.saveType ==
 * SAVETYPE_RANDO`, re-evaluated on every GameInteractor_ExecuteOnSaveLoad
 * (Rando.cpp OnSaveLoadHandler -> MiscBehavior::OnFileLoad et al.). Arm state
 * is read through S2H::GameHooks::CountForTest<OnFlagSet> — the same probe
 * MMPairSwitchEntry uses; the OnFlagSet COND_HOOK (MiscBehavior.cpp) registers
 * iff IS_RANDO at the last dispatch.
 *
 * Two production convergence points, each modeled through its real dispatch:
 *
 *  1. The MM file-select LOAD path (FileSelect_LoadGame,
 *     z_file_choose_NES.c): MM_Sram_OpenSave memcpy's the save from flash,
 *     THEN OnSaveLoad fires (z_file_choose_NES.c:2250). The cold boot chain
 *     (TitleSetup_SetupTitleScreen, z_opening.c:32) authored a VANILLA
 *     bootstrap and its OnSaveLoad DISARMED the hooks first, so this dispatch
 *     must RE-ARM against the loaded rando save. That disarm-then-rearm
 *     ordering is exactly what #439 got wrong on the arrival path, and it is
 *     the convergence the owl-save reload leans on (owl-save-and-quit ->
 *     TitleSetup disarm -> title -> file-select LOAD re-arm; z_message.c
 *     MSGMODE_OWL_SAVE + z_play.c:806 -> MM_TitleSetup_Init). A LOADED rando
 *     save is modeled the way MM_Sram_OpenSave produces one — a plain new-file
 *     image with saveType stamped SAVETYPE_RANDO — because the LOAD path loads
 *     bytes, it does NOT re-run generation (that is the CREATE path, and
 *     MMRandoGen covers it).
 *
 *  2. The in-session reload paths (Song of Time / cycle reset, DayTelop):
 *     these re-enter MM_Play_Init WITHOUT a startup entrance, so
 *     MM_Play_ConsumeStartupEntrance early-returns (z_play.c:2282) and NOTHING
 *     re-dispatches OnSaveLoad. That is correct precisely because these paths
 *     do NOT run the boot chain's disarming dispatch — a live rando session's
 *     armed hooks must simply survive. Locked here so a future change that
 *     either routes a cycle reset through the boot chain, or drops a stray
 *     disarming dispatch into the consumption early-return, fails loudly
 *     instead of shipping a paired world that silently plays vanilla after the
 *     first Song of Time.
 *
 * Returns 0 on success, a distinct nonzero step code otherwise.
 */
extern "C" int MM_Rando_HeadlessReloadArmState(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        fprintf(stderr, "[MM-RELOAD-ARM] FAIL(1): Ship::Context not live\n");
        return 1;
    }

    MM_Rando_Init();
    if (Rando::Logic::Regions.empty()) {
        fprintf(stderr, "[MM-RELOAD-ARM] FAIL(2): Logic/Regions graph empty — rando surface elided or InitAll not "
                        "run\n");
        return 2;
    }
    if (gRegEditor == NULL) {
        static RegEditor sReloadRegEditor = {};
        gRegEditor = &sReloadRegEditor;
    }

    // Clean cross-game state: no pairing, no pending arrival, no frozen save.
    // This row is purely about the save-shape -> hook arm-state relationship;
    // the paired-world producer is MMPairSwitchEntry's job.
    ComboContext_Init();
    Combo_ClearForeignPlacements();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();

    // ----------------------------------------------------------------------
    // Phase 1 — the boot chain authors a VANILLA bootstrap and DISARMS.
    // Exactly TitleSetup_SetupTitleScreen (z_opening.c): a plain new file,
    // then OnSaveLoad against it. The IS_RANDO COND_HOOKs unregister here.
    // ----------------------------------------------------------------------
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-RELOAD-ARM] FAIL(3): a plain new file came up rando — vanilla-bootstrap premise "
                        "broken\n");
        return 3;
    }
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    const size_t disarmed = S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>();

    // ----------------------------------------------------------------------
    // Phase 2 — the file-select LOAD of a RANDO save RE-ARMS after the disarm.
    // FileSelect_LoadGame: MM_Sram_OpenSave populates gSaveContext from flash
    // (modeled here as a new-file image stamped SAVETYPE_RANDO, the way a
    // loaded rando save reads), THEN GameInteractor_ExecuteOnSaveLoad
    // (z_file_choose_NES.c:2250) — the "fires AFTER the save is populated, not
    // before" contract this audit had to confirm.
    // ----------------------------------------------------------------------
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO; // what OpenSave loads for a rando file
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    const size_t armed = S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>();
    if (armed == 0 || armed <= disarmed) {
        fprintf(stderr,
                "[MM-RELOAD-ARM] FAIL(4): the file-select LOAD dispatch did not re-arm the IS_RANDO hooks after "
                "the boot chain's disarm (OnFlagSet %zu -> %zu) — a loaded rando save would play with vanilla "
                "behavior\n",
                disarmed, armed);
        return 4;
    }
    fprintf(stderr, "[MM-RELOAD-ARM] file-select LOAD re-armed after the boot-chain disarm (OnFlagSet %zu -> %zu)\n",
            disarmed, armed);

    // ----------------------------------------------------------------------
    // Phase 3 — an in-session reload (Song of Time / cycle reset / DayTelop)
    // PRESERVES the armed state. Those reloads reach MM_Play_Init with no
    // startup entrance, so MM_Play_ConsumeStartupEntrance early-returns and
    // must not disturb the hooks. Drive that exact consumption with the rando
    // save live and the pending arrival cleared.
    // ----------------------------------------------------------------------
    Combo_ClearStartupEntrance(); // in-session reloads carry no pending arrival
    MM_Play_ConsumeStartupEntrance();
    const size_t afterReload = S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>();
    if (afterReload != armed) {
        fprintf(stderr,
                "[MM-RELOAD-ARM] FAIL(5): an in-session reload disturbed the IS_RANDO hooks (OnFlagSet %zu -> %zu) "
                "— a cycle reset must never silently re-arm/disarm a paired world\n",
                armed, afterReload);
        return 5;
    }
    if (gSaveContext.save.shipSaveInfo.saveType != SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-RELOAD-ARM] FAIL(6): the in-session reload no-op mutated the live save type\n");
        return 6;
    }
    fprintf(stderr, "[MM-RELOAD-ARM] in-session reload preserved the armed hooks (OnFlagSet %zu)\n", afterReload);

    // ----------------------------------------------------------------------
    // Phase 4 — vanilla direction: a file-select LOAD of a VANILLA save
    // DISARMS the rando overrides, so the hooks track the loaded save and can
    // never leak into a non-rando file loaded after a rando one.
    //
    // Probed through a VB verdict, NOT the OnFlagSet count: COND_HOOK's
    // Unregister is DEFERRED (mm_game_hooks.h — the id is queued and only erased
    // by FlushPendingUnregistrations at the next Execute of that hook type), so
    // CountForTest lags a disarm and still shows the not-yet-flushed hook even
    // though it can no longer fire. GameInteractor_Should dispatches
    // Execute<ShouldVanillaBehavior>, which applies that flush, so the verdict
    // reflects the disarm immediately. The give override is
    // COND_VB_SHOULD(VB_GIVE_ITEM_FROM_GREAT_FAIRY, IS_RANDO, { *should=false }),
    // re-evaluated on the same OnSaveLoad chain (ShipInit::Init("IS_RANDO")) and
    // headless-safe — its body touches no play state. MMRandoGen uses the same
    // probe for its vanilla direction.
    // ----------------------------------------------------------------------
    // Armed baseline (the rando save is still live from Phases 2-3): the
    // IS_RANDO give override suppresses the vanilla great-fairy give.
    if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-RELOAD-ARM] FAIL(7): the armed rando save did not suppress the give VB — the "
                        "arm-state probe premise is broken\n");
        return 7;
    }
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    // The override must be gone: the caller's verdict passes through in BOTH
    // polarities (true stays true, false stays false).
    if (!GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true) ||
        GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, false)) {
        fprintf(stderr, "[MM-RELOAD-ARM] FAIL(8): a vanilla-save LOAD left the rando give override armed — rando "
                        "overrides leaked into a non-rando file\n");
        return 8;
    }
    fprintf(stderr, "[MM-RELOAD-ARM] vanilla-direction LOAD disarmed the rando overrides (give VB passes through)\n");

    // Leave clean global state for later dispatches in the same process.
    ComboContext_Init();
    Combo_ClearForeignPlacements();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    memset(&gSaveContext, 0, sizeof(gSaveContext));

    fprintf(stderr, "[MM-RELOAD-ARM] PASS: file-select LOAD re-arms after a boot-chain disarm; in-session reload "
                    "preserves; vanilla LOAD disarms\n");
    return 0;
}

// ============================================================================
// Moon-crash arm-state lock — the operator-confirmed P0.
//
// A moon crash (the three-day clock expiring, e.g. while AFK) runs
// Sram_ResetSaveFromMoonCrash (games/mm/src/code/z_sram_NES.c), which re-reads
// the file from flash and memcpy's sizeof(Save) over gSaveContext.save.
// ShipSaveInfo -- carrying saveType AND the whole rando block (finalSeed,
// options, the RC_MAX placement table) -- is a MEMBER of Save, so that memcpy
// overwrites the randomizer identity wholesale.
//
// A cross-game paired MM world is authored IN MEMORY at the arrival and is not
// in MM flash until the player saves, so the reload pulls a vanilla on-disk
// save: saveType reverts to SAVETYPE_VANILLA, every IS_RANDO COND_HOOK
// unregisters, and MM plays vanilla for the rest of the session -- then the next
// switch-out freezes THAT save, so the vanilla world survives every later return
// leg. That is the "paired MM world plays vanilla" report.
//
// This row drives the REAL Sram_ResetSaveFromMoonCrash against a live paired
// world. Arm state is probed through a VB verdict rather than a hook COUNT:
// COND_HOOK's Unregister is DEFERRED (mm_game_hooks.h), so CountForTest lags a
// disarm and would let this pass vacuously -- the same weakness that let the
// original bug ship green.
// ============================================================================
extern "C" int MM_Rando_HeadlessMoonCrashArmState(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        fprintf(stderr, "[MM-MOONCRASH] FAIL(1): Ship::Context not live\n");
        return 1;
    }

    MM_Rando_Init();
    if (Rando::Logic::Regions.empty()) {
        fprintf(stderr, "[MM-MOONCRASH] FAIL(2): Logic/Regions graph empty — rando surface elided\n");
        return 2;
    }
    if (gRegEditor == NULL) {
        static RegEditor sMoonCrashRegEditor = {};
        gRegEditor = &sMoonCrashRegEditor;
    }

    ComboContext_Init();
    Combo_ClearForeignPlacements();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();

    // ----------------------------------------------------------------------
    // Phase 1 — a live paired rando world, armed.
    // fileNum 0 (not the cross-game session's 0xFF) keeps the flash page-table
    // indexing inside gFlashSaveStartPages; this row is about saveType
    // preservation, not the fileNum indexing question.
    // ----------------------------------------------------------------------
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    gSaveContext.fileNum = 0;
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    gSaveContext.save.shipSaveInfo.rando.finalSeed = 0xC0FFEE01;
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART].shuffled = true;
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);

    if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-MOONCRASH] FAIL(3): the paired world did not arm the IS_RANDO give override — "
                        "premise broken before the moon crash\n");
        return 3;
    }

    // ----------------------------------------------------------------------
    // Phase 2 — the REAL moon-crash reset. Flash holds no rando save for this
    // in-memory paired world (nothing was ever written), which is exactly the
    // production condition: the reload yields a zeroed/vanilla Save.
    //
    // Production (z_demo.c:379) resets from &play->sramCtx with a live play
    // state, and #516 Phase 2 revived RegisterSavingEnhancements, whose
    // BeforeMoonCrashSaveReset leg (DeleteOwlSave) dereferences MM_gPlayState.
    // So stand up a play state and reset from ITS sramCtx, faithfully — rather
    // than the free-standing SramContext this used before the leg existed. Only
    // sramCtx is touched on this path; a zeroed PlayState with a real save buffer
    // is sufficient and its 0x19258 bytes sit in BSS.
    // ----------------------------------------------------------------------
    static u8 sMoonCrashSaveBuf[SAVE_BUFFER_SIZE];
    static PlayState sMoonCrashPlayState;
    memset(&sMoonCrashPlayState, 0, sizeof(sMoonCrashPlayState));
    sMoonCrashPlayState.sramCtx.saveBuf = sMoonCrashSaveBuf;

    PlayState* savedPlayState = MM_gPlayState;
    MM_gPlayState = &sMoonCrashPlayState;

    Sram_ResetSaveFromMoonCrash(&MM_gPlayState->sramCtx);

    MM_gPlayState = savedPlayState;

    // ----------------------------------------------------------------------
    // Phase 3 — the world identity must survive, and the hooks must still be
    // armed against it. A moon crash resets the CYCLE, never the rando identity.
    // ----------------------------------------------------------------------
    if (gSaveContext.save.shipSaveInfo.saveType != SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-MOONCRASH] FAIL(4): the moon-crash save reload reverted saveType to vanilla — every "
                        "IS_RANDO hook disarms and MM plays vanilla for the rest of the session\n");
        return 4;
    }
    if (gSaveContext.save.shipSaveInfo.rando.finalSeed != 0xC0FFEE01) {
        fprintf(stderr,
                "[MM-MOONCRASH] FAIL(5): the moon-crash reload lost the paired world identity "
                "(finalSeed %08X, expected C0FFEE01)\n",
                gSaveContext.save.shipSaveInfo.rando.finalSeed);
        return 5;
    }
    if (!RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART].shuffled) {
        fprintf(stderr, "[MM-MOONCRASH] FAIL(6): the moon-crash reload wiped the placement table — the world would "
                        "be armed but empty\n");
        return 6;
    }
    if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-MOONCRASH] FAIL(7): saveType survived but the IS_RANDO hooks were left unregistered by "
                        "the reload — MM would still play vanilla on a save that claims to be rando\n");
        return 7;
    }

    // ----------------------------------------------------------------------
    // Phase 4 — SELF-HEAL an already-corrupted save, through the REAL arrival.
    //
    // Saves damaged by this bug before the fix landed are still on disk: a
    // complete rando world (finalSeed + placement table intact) sitting under
    // saveType=VANILLA. MM's own code can never author that combination --
    // Sram_ResetSave memsets shipSaveInfo wholesale and MM_Sram_InitNewSave then
    // stamps SAVETYPE_VANILLA, so a vanilla file always has finalSeed == 0.
    // Seeing it is positive evidence that a rando file lost its type byte, and
    // MM_Rando_PairOnCrossGameArrival re-stamps SAVETYPE_RANDO rather than
    // accepting a permanently-vanilla world.
    //
    // Driven through MM_Play_ConsumeStartupEntrance -- the real consumption
    // point -- not by calling the pairing directly, so this covers the actual
    // arrival path a player takes.
    // ----------------------------------------------------------------------
    const uint16_t kArrival = 0xD800; // ENTRANCE(SOUTH_CLOCK_TOWN, 0) — the OoT->MM arrival

    ComboContext_Init();
    Combo_ClearForeignPlacements();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();

    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    gSaveContext.fileNum = 0;

    // DISARM first, against the vanilla bootstrap -- exactly what the arrival's
    // cold gamestate-chain boot does before the consume. Without this the
    // hooks would still be armed from Phase 1 and the re-arm assertion below
    // would pass whether or not the repair actually re-dispatched anything,
    // i.e. vacuously. (That is precisely the weakness this row exists to
    // avoid: MMPairSwitchEntry's arming assertion is a bare count delta.)
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    if (!GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-MOONCRASH] FAIL(8): the vanilla bootstrap did not disarm the IS_RANDO give override — "
                        "the Phase 4 re-arm assertion would be vacuous\n");
        return 8;
    }

    // The corrupted shape: live rando world, vanilla type byte.
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_VANILLA;
    gSaveContext.save.shipSaveInfo.rando.finalSeed = 0xC0FFEE02;
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART].shuffled = true;

    // A paired OoT world exists (Lane B stamp), and this is a return leg: a
    // frozen MM session restored by the consume below.
    gComboCtx.sourceIsRando = 1;
    gComboCtx.sharedRandoSeed = 4170548651u;
    gComboCtx.sharedRandoSettingsHash = 0x5DAD32CEu;
    Combo_FreezeState("mm", kArrival, &gSaveContext, sizeof(gSaveContext));
    Combo_SetStartupEntrance(kArrival);

    MM_Play_ConsumeStartupEntrance();

    if (gSaveContext.save.shipSaveInfo.saveType != SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-MOONCRASH] FAIL(9): a restored save carrying a complete rando world under "
                        "saveType=vanilla was accepted as vanilla — the world is unrecoverable and MM plays "
                        "vanilla forever\n");
        return 9;
    }
    if (gSaveContext.save.shipSaveInfo.rando.finalSeed != 0xC0FFEE02) {
        fprintf(stderr, "[MM-MOONCRASH] FAIL(10): the repair regenerated or clobbered the player's world "
                        "(finalSeed changed) — an existing file must never be regenerated\n");
        return 10;
    }
    if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-MOONCRASH] FAIL(11): the repaired save did not leave the IS_RANDO hooks armed\n");
        return 11;
    }
    fprintf(stderr, "[MM-MOONCRASH] self-heal repaired a vanilla-stamped rando world through the real arrival\n");

    // Leave clean global state for later dispatches in the same process.
    ComboContext_Init();
    Combo_ClearForeignPlacements();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    memset(&gSaveContext, 0, sizeof(gSaveContext));

    fprintf(stderr, "[MM-MOONCRASH] PASS: a moon crash preserves the paired world identity, leaves the IS_RANDO "
                    "hooks armed, and an already-corrupted save self-heals on arrival\n");
    return 0;
}

// ============================================================================
// Owl-save / file-copy readback lock (#487) — the second live leg of the class
// #485 caught at the moon crash.
//
// Sram_UpdateWriteToFlashOwlSave (games/mm/src/code/z_sram_NES.c) finishes a
// normal owl save by re-reading the file it just wrote and memcpy'ing that
// buffer over gSaveContext for offsetof(SaveContext, fileNum) — all of struct
// Save, ShipSaveInfo included. In single-exe MM's flash read is a stub that
// fills nothing (games/mm/2s2h/mm_save_manager_stubs.c), so the commit is a
// commit of ZEROS: saveType -> SAVETYPE_VANILLA, finalSeed -> 0, the RC_MAX
// placement table -> empty. Unlike the moon crash this happens on the ordinary
// save-and-quit a player performs every cycle. func_80147414 (the owl half of
// Sram_CopySave) has the identical shape.
//
// Arm state is probed through a VB verdict, never a hook COUNT: COND_HOOK's
// Unregister is DEFERRED (mm_game_hooks.h), so CountForTest lags a disarm. And
// this particular corruption is invisible to a count in BOTH directions —
// nothing re-dispatches OnSaveLoad after the readback, so the hooks stay
// registered and keep firing against an emptied table. A count probe here would
// not merely be weak, it would be blind.
//
// Non-vacuity is structural: each leg DISARMS against a vanilla bootstrap and
// asserts the disarm took before re-arming, so a "just force rando on" fix
// fails; and the assertions cover finalSeed and a placement-table entry, so a
// fix that preserves only the type byte fails too.
// ============================================================================
extern "C" int MM_Rando_HeadlessOwlSaveArmState(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        fprintf(stderr, "[MM-OWL-SAVE] FAIL(1): Ship::Context not live\n");
        return 1;
    }

    MM_Rando_Init();
    if (Rando::Logic::Regions.empty()) {
        fprintf(stderr, "[MM-OWL-SAVE] FAIL(2): Logic/Regions graph empty — rando surface elided\n");
        return 2;
    }
    if (gRegEditor == NULL) {
        static RegEditor sOwlSaveRegEditor = {};
        gRegEditor = &sOwlSaveRegEditor;
    }

    ComboContext_Init();
    Combo_ClearForeignPlacements();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();

    static u8 sOwlSaveBuf[SAVE_BUFFER_SIZE];
    SramContext owlSram = {};
    owlSram.saveBuf = sOwlSaveBuf;

    // ----------------------------------------------------------------------
    // Phase 0 — DISARM against a vanilla bootstrap, and assert the disarm took.
    // Without this every "armed" assertion below would pass on state left over
    // from an earlier row in the same process, i.e. vacuously.
    // fileNum 0 (not the cross-game 0xFF sentinel) keeps the owl page-table
    // indexing in bounds; this row is about saveType preservation, not the
    // fileNum indexing question that mm-flash-filenum-oob owns.
    // ----------------------------------------------------------------------
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    gSaveContext.fileNum = 0;
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    if (!GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-OWL-SAVE] FAIL(3): the vanilla bootstrap did not disarm the IS_RANDO give override — "
                        "every arm assertion in this row would be vacuous\n");
        return 3;
    }

    // ----------------------------------------------------------------------
    // Phase 1 — a live paired rando world, armed. Modeled the way a paired
    // world reads in memory: saveType stamped, a world identity, a shuffled
    // placement entry.
    // ----------------------------------------------------------------------
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    gSaveContext.save.shipSaveInfo.rando.finalSeed = 0xC0FFEE03;
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART].shuffled = true;
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-OWL-SAVE] FAIL(4): the paired world did not arm the IS_RANDO give override — premise "
                        "broken before the owl save\n");
        return 4;
    }

    // ----------------------------------------------------------------------
    // Phase 2 — the REAL owl-save completion. status is neither 7 nor 8 and
    // startWriteOsTime is 0, so the elapsed-time branch (the one carrying the
    // readback) is the branch taken. Asserting status afterwards is mandatory:
    // if the call fell into one of the write-progress branches instead, this
    // row would silently test nothing.
    // ----------------------------------------------------------------------
    owlSram.status = 6;
    owlSram.curPage = gFlashOwlSaveStartPages[0];
    owlSram.numPages = gFlashOwlSaveNumPages[0];
    owlSram.startWriteOsTime = 0;
    gSaveContext.save.isOwlSave = true;

    Sram_UpdateWriteToFlashOwlSave(&owlSram);

    if (owlSram.status != 0) {
        fprintf(stderr,
                "[MM-OWL-SAVE] FAIL(5): the readback branch never ran (status %d) — the assertions below would be "
                "vacuous\n",
                owlSram.status);
        return 5;
    }

    // ----------------------------------------------------------------------
    // Phase 3 — the world identity must survive the readback, and the hooks
    // must still be armed against it.
    // ----------------------------------------------------------------------
    if (gSaveContext.save.shipSaveInfo.saveType != SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-OWL-SAVE] FAIL(6): the owl-save readback reverted saveType to vanilla — every IS_RANDO "
                        "hook disarms at the next load and MM plays vanilla for the rest of the session\n");
        return 6;
    }
    if (gSaveContext.save.shipSaveInfo.rando.finalSeed != 0xC0FFEE03) {
        fprintf(stderr,
                "[MM-OWL-SAVE] FAIL(7): the owl-save readback lost the paired world identity (finalSeed %08X, "
                "expected C0FFEE03)\n",
                gSaveContext.save.shipSaveInfo.rando.finalSeed);
        return 7;
    }
    if (!RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART].shuffled) {
        fprintf(stderr, "[MM-OWL-SAVE] FAIL(8): the owl-save readback wiped the placement table — the world would be "
                        "armed but empty\n");
        return 8;
    }
    if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-OWL-SAVE] FAIL(9): saveType survived the owl save but the IS_RANDO hooks were left "
                        "unregistered — MM would still play vanilla on a save that claims to be rando\n");
        return 9;
    }
    fprintf(stderr, "[MM-OWL-SAVE] owl-save completion preserved the paired world and its armed hooks\n");

    // ----------------------------------------------------------------------
    // Phase 4 — the same class on the file-copy path (func_80147414, the owl
    // half of Sram_CopySave), which reads the SOURCE file over the live
    // gSaveContext. Disarm and re-arm again rather than inheriting Phase 3's
    // armed state, so this leg is non-vacuous on its own.
    // ----------------------------------------------------------------------
    // The memset also clears the placement table: RANDO_SAVE_CHECKS is
    // gSaveContext.save.shipSaveInfo.rando.randoSaveChecks (Rando.h).
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    gSaveContext.fileNum = 0;
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    if (!GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-OWL-SAVE] FAIL(10): the vanilla bootstrap did not disarm before the copy leg — the "
                        "assertions below would be vacuous\n");
        return 10;
    }

    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    gSaveContext.save.shipSaveInfo.rando.finalSeed = 0xC0FFEE04;
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART].shuffled = true;
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-OWL-SAVE] FAIL(11): the copy leg's paired world did not arm — premise broken\n");
        return 11;
    }

    memset(sOwlSaveBuf, 0, SAVE_BUFFER_SIZE);
    func_80147414(&owlSram, 0, 1);

    if (gSaveContext.save.shipSaveInfo.saveType != SAVETYPE_RANDO ||
        gSaveContext.save.shipSaveInfo.rando.finalSeed != 0xC0FFEE04 ||
        !RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART].shuffled) {
        fprintf(stderr, "[MM-OWL-SAVE] FAIL(12): copying an owl file stripped the LIVE world's randomizer identity "
                        "(saveType/finalSeed/placement table)\n");
        return 12;
    }
    if (GameInteractor_Should(VB_GIVE_ITEM_FROM_GREAT_FAIRY, true)) {
        fprintf(stderr, "[MM-OWL-SAVE] FAIL(13): the copy path left the IS_RANDO hooks unregistered\n");
        return 13;
    }
    fprintf(stderr, "[MM-OWL-SAVE] file-copy readback left the live paired world intact and armed\n");

    // Leave clean global state for later dispatches in the same process. The
    // gSaveContext memset also clears the placement table and the save type,
    // both of which live inside it.
    ComboContext_Init();
    Combo_ClearForeignPlacements();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    memset(&gSaveContext, 0, sizeof(gSaveContext));

    fprintf(stderr, "[MM-OWL-SAVE] PASS: an owl save and a file copy both preserve the paired world identity and "
                    "leave the IS_RANDO hooks armed\n");
    return 0;
}

// ============================================================================
// Attempt-ladder determinism lock (ADR 0010 increment 1.2 — lock (b) of the
// increment's CI set).
// ============================================================================
namespace {

/** Clear every option CVar so the paired profile resolves from "nobody chose
 *  anything" — which, under increment 1.1's pin, is GLITCHLESS. */
void LadderClearOptionCVars() {
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        (void)randoOptionId;
        CVarClear(row.cvar);
    }
    CVarClear("gRando.ExcludedChecks");
}

/**
 * THE PINNED LADDER MASTER SEED, run under the ALL-DEFAULT (Glitchless)
 * profile with ONE deterministic ladder rung injected
 * (Rando::Foreign::ForceShortForeignPlacements): attempt 0 throws the #488
 * structural placement failure (under ADR 0010 increment 1.3 a mere shortfall
 * is under-supply and no longer fails the attempt, so the injection rides the
 * retained structural throw), attempt 1 re-derives by the documented recipe
 * and converges — attempts == 2, the minimal ladder-exercising shape.
 *
 * WHY INJECTED AND NOT A NATURALLY DEAD-ENDING SEED. The obvious fixture is
 * "scan for a master seed that dead-ends first try". Measured at this tree on
 * 2026-07-31, that fixture does not exist and cannot be made to exist
 * honestly: scanning master seeds 1..66 under the dead-end-PRONE Glitchless
 * profile (ocarina buttons + swim + boss remains + owl statues + clock
 * shuffle) produced 14 first-attempt failures and EVERY ONE of them was the
 * Glitchless fill's 10s WALL-CLOCK abort — not one deterministic dead-end.
 * (The all-default profile converges first-try on every seed tried; Nearly No
 * Logic's apply cannot throw at all.)
 *
 * A seed pinned on a wall-clock failure is a RACE, not a lock: a faster or
 * less loaded machine converges the same seed first try and the row flips to
 * FAIL(4) on timing alone, while a slower one climbs further and changes the
 * world. That is also precisely why the ladder REFUSES to re-roll a
 * wall-clock abort (Rando::Logic::GenerationTimeout; MMPairedExhaustion's
 * third leg locks that) — so a pinned wall-clock seed would additionally be
 * pinning behaviour the ladder no longer has.
 *
 * Injecting the rung instead keeps the lock honest about what it proves: the
 * CAUSE of attempt 0's failure is not under test, the ladder's RESPONSE is —
 * pre-attempt save restored, seed re-derived as
 * Ship_Hash(decimal(master) + options + ":glitchless-attempt-1"), a REAL fill
 * and a REAL placement pass on the winning attempt (the injection count is
 * asserted drained), the winning index recorded in gComboCtx and the digest,
 * and the whole thing byte-reproducible across two processes.
 *
 * RE-PIN RECIPE (if the fill ever grows a deterministic dead-end and a natural
 * seed becomes available): run the scan mode —
 *
 *     RSBS_ATTEMPT_SEED_SCAN=<startSeed>:<count> redship --test mm-paired-attempt
 *
 * — which prints `seed=<n> attempts=<k> exhausted=<e> converged=<c>` per
 * candidate under the heavy dead-end-prone profile. `attempts>=2 converged=1`
 * is a natural ladder seed; `attempts=1 converged=0` is a wall-clock abort and
 * must NEVER be pinned. This is the SeedDeterminism re-pin discipline: the fix
 * for a moved fill is a deliberate new pin, never a bent derivation.
 */
constexpr uint32_t kLadderMasterSeed = 1u;
/** Ladder rungs injected before the winning attempt. 1 => attempt 0 fails
 *  deterministically, attempt 1 wins. */
constexpr int kLadderInjectedRungs = 1;

/** A master seed verified (same 2026-07-31 scan) to converge on attempt 0
 *  under the ALL-DEFAULT (Glitchless-pinned) profile — the exhaustion row's
 *  counter-leg fixture, where first-try convergence is exactly what is
 *  wanted: the leg proves a SUCCESSFUL default-profile arrival latches
 *  nothing. */
constexpr uint32_t kDefaultProfileConvergingSeed = 1u;
} // namespace

/**
 * Lock (b): a paired generation whose first ladder attempt fails
 * DETERMINISTICALLY climbs one rung, converges through the real OnSaveInit
 * dispatch chain, and reaches a world that is a pure function of the frozen
 * identity. The in-process assertions here cover convergence, the non-vacuity
 * floor (attempts >= 2), the injection actually draining (so the winning
 * attempt ran a real placement pass) and the gComboCtx provenance record;
 * byte-identical cross-PROCESS reproduction is driven by the
 * MMPairedAttemptDeterminism CTest row
 * (CMake/CheckPairedAttemptDeterminism.cmake), which runs this dispatch twice
 * in two fresh processes and diffs the digest written to
 * RSBS_ATTEMPT_DIGEST_OUT — two processes for the same reason SeedDeterminism
 * uses them.
 *
 * See kLadderMasterSeed for why the failing rung is injected rather than
 * scan-pinned (short version: at this tree every natural first-attempt
 * failure is a wall-clock abort, and pinning one would be pinning a race).
 *
 * COUNTERFACTUALS (each independently red): make the ladder derivation
 * consume runtime state (e.g. seed attempt n from the previous attempt's RNG
 * position instead of the documented hash) and the two-process digest diff
 * fails; drop the gComboCtx.mmPairedAttempt stamp and FAIL(6) fires; remove
 * the ladder entirely and FAIL(3) fires, because the injected rung then goes
 * straight to the vanilla revert with nothing to retry.
 *
 * Returns 0 on success, a distinct nonzero step code otherwise.
 */
extern "C" int MM_Rando_HeadlessPairedAttemptDigest(const char* outPath) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        fprintf(stderr, "[MM-ATTEMPT] FAIL(1): Ship::Context not live\n");
        return 1;
    }

    MM_Rando_Init();
    if (Rando::Logic::Regions.empty()) {
        fprintf(stderr, "[MM-ATTEMPT] FAIL(2): Logic/Regions graph empty\n");
        return 2;
    }
    if (gRegEditor == NULL) {
        static RegEditor sAttemptRegEditor = {};
        gRegEditor = &sAttemptRegEditor;
    }

    // The pinned path runs the ALL-DEFAULT profile — which, under increment
    // 1.1's flip, resolves to Glitchless — because the failing rung is
    // injected rather than coaxed out of a dead-end-prone configuration (see
    // kLadderMasterSeed). That also keeps this row off the wall-clock cliff:
    // the heavy profile below is where 10s aborts live, and a lock must not
    // depend on how busy the machine is.
    LadderClearOptionCVars();
    CVarSetInteger("gRando.SpoilerFileIndex", 0);
    CVarSetInteger("gRando.GenerateSpoiler", 0); // the digest is the artifact
    CVarSetString("gRando.InputSeed", "USERSEEDPOISON"); // the paired branch must ignore this

    // One paired generation for one master seed, through the real dispatch
    // chain, from a fresh legacy (pre-freeze) carrier each time.
    auto generateFor = [](uint32_t masterSeed) {
        ComboContext_Init();
        gComboCtx.sourceIsRando = 1;
        gComboCtx.sharedRandoSeed = masterSeed;
        gComboCtx.sharedRandoSettingsHash = 0x1ADD3125u; // nonzero: "profile recorded" (Lane B contract)
        gComboCtx.mmProfileDigest = 0;
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        MM_Sram_InitNewSave();
        GameInteractor_ExecuteOnSaveInit(0);
    };

    // Re-pin scan mode (see kLadderMasterSeed's recipe). Prints per-seed
    // ladder outcomes and asserts nothing — it exists to HUNT for a natural
    // deterministic dead-end, so it runs the heavy dead-end-prone profile
    // rather than the pinned path's defaults. Read its output with the
    // wall-clock discriminator in mind: `attempts=1 converged=0` is a timeout
    // and is NOT a pinnable dead-end.
    const char* scanSpec = std::getenv("RSBS_ATTEMPT_SEED_SCAN");
    if (scanSpec != NULL && scanSpec[0] != '\0') {
        CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_OCARINA_BUTTONS].cvar, RO_GENERIC_ON);
        CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_SWIM].cvar, RO_GENERIC_ON);
        CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_BOSS_REMAINS].cvar, RO_GENERIC_ON);
        CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_OWL_STATUES].cvar, RO_GENERIC_ON);
        CVarSetInteger(Rando::StaticData::Options[RO_CLOCK_SHUFFLE].cvar, RO_GENERIC_ON);
        unsigned scanStart = 0;
        unsigned scanCount = 0;
        if (sscanf(scanSpec, "%u:%u", &scanStart, &scanCount) != 2 || scanCount == 0) {
            fprintf(stderr, "[MM-ATTEMPT] FAIL(9): RSBS_ATTEMPT_SEED_SCAN must be <startSeed>:<count>\n");
            return 9;
        }
        for (unsigned i = 0; i < scanCount; i++) {
            const uint32_t seed = (uint32_t)(scanStart + i);
            generateFor(seed);
            fprintf(stderr, "[MM-ATTEMPT] scan: seed=%u attempts=%d exhausted=%d converged=%d\n", (unsigned)seed,
                    MM_Rando_PairedGenLastAttempts(), MM_Rando_PairedGenLastExhausted(),
                    gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO ? 1 : 0);
        }
        LadderClearOptionCVars();
        ComboContext_Init();
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        return 0;
    }

    // Arm exactly one deterministic ladder rung, then generate. The injection
    // is consumed by attempt 0's placement pass; attempt 1 runs the real one.
    Rando::Foreign::ForceShortForeignPlacements(kLadderInjectedRungs);
    generateFor(kLadderMasterSeed);
    Rando::Foreign::ForceShortForeignPlacements(0); // disarm before any early return

    if (gSaveContext.save.shipSaveInfo.saveType != SAVETYPE_RANDO) {
        fprintf(stderr,
                "[MM-ATTEMPT] FAIL(3): the pinned ladder master seed %u EXHAUSTED the ladder (%d attempts) — with "
                "only %d rung(s) injected the ladder must converge, so either the ladder is gone (an injected rung "
                "now goes straight to the vanilla revert) or the fill stopped converging for this identity\n",
                (unsigned)kLadderMasterSeed, MM_Rando_PairedGenLastAttempts(), kLadderInjectedRungs);
        return 3;
    }
    const int attempts = MM_Rando_PairedGenLastAttempts();
    if (attempts != kLadderInjectedRungs + 1) {
        fprintf(stderr,
                "[MM-ATTEMPT] FAIL(4): %d rung(s) injected but the ladder reports attempts=%d (expected %d) — at "
                "attempts<=1 the ladder was never exercised and this lock is vacuous; above it, a rung was climbed "
                "for a reason the fixture does not name\n",
                kLadderInjectedRungs, attempts, kLadderInjectedRungs + 1);
        return 4;
    }
    if (Rando::Foreign::ForcedShortForeignPlacementsRemaining() != 0) {
        fprintf(stderr, "[MM-ATTEMPT] FAIL(11): injected rungs did not drain — the winning attempt never ran a real "
                        "foreign-placement pass, so this digest describes a world that was never placed\n");
        return 11;
    }
    if (MM_Rando_PairedGenLastExhausted()) {
        fprintf(stderr, "[MM-ATTEMPT] FAIL(5): converged world reports an exhausted ladder — bookkeeping broken\n");
        return 5;
    }
    if (gComboCtx.mmPairedAttempt != (uint32_t)attempts) {
        fprintf(stderr,
                "[MM-ATTEMPT] FAIL(6): gComboCtx.mmPairedAttempt=%u but the ladder reports %d attempts — the combo "
                "record would replay a different derivation than the one that authored this world\n",
                (unsigned)gComboCtx.mmPairedAttempt, attempts);
        return 6;
    }
    {
        // ADR 0010 increment 1.3: the winning attempt's placement pass runs
        // under the reachability gate, so the expected count is
        // min(pool, reachable eligible hosts) — the gate may legitimately
        // place fewer than the pool — but ZERO stays fatal (a ladder digest
        // describing a world that hosted nothing would be vacuous), mirroring
        // MM_Rando_HeadlessForeignDigest's FAIL(5).
        const ComboForeignItemDef* pool = NULL;
        const int poolCount = Combo_GetForeignItemPool(&pool);
        const Rando::Foreign::PlacementStats& attemptStats = Rando::Foreign::LastPlacementStats();
        const int attemptExpected =
            poolCount < attemptStats.reachableEligibleHosts ? poolCount : attemptStats.reachableEligibleHosts;
        if (Combo_CountForeignPlacements() == 0 || Combo_CountForeignPlacements() != attemptExpected) {
            fprintf(stderr,
                    "[MM-ATTEMPT] FAIL(7): expected %d foreign placements (pool %d, reachable eligible %d), "
                    "found %d\n",
                    attemptExpected, poolCount, attemptStats.reachableEligibleHosts, Combo_CountForeignPlacements());
            return 7;
        }
    }

    // Canonical world digest, mirroring MM_Rando_HeadlessForeignDigest's
    // shape: final seed + attempt + placement blob hash + every foreign
    // placement. Written fresh (not appended) — this row owns its file.
    std::string blob;
    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        if (randoStaticCheck.randoCheckId == RC_UNKNOWN) {
            continue;
        }
        blob += std::to_string((int)randoCheckId);
        blob += ':';
        blob += std::to_string((int)RANDO_SAVE_CHECKS[randoCheckId].randoItemId);
        blob += ';';
    }
    const uint32_t placementHash = Ship_Hash(blob);

    FILE* out = stdout;
    bool closeOut = false;
    if (outPath != NULL && outPath[0] != '\0') {
        out = fopen(outPath, "w");
        if (out == NULL) {
            fprintf(stderr, "[MM-ATTEMPT] FAIL(8): cannot open digest output '%s'\n", outPath);
            return 8;
        }
        closeOut = true;
    }
    fprintf(out,
            "ladderMasterSeed=%u\n"
            "mmFinalSeed=%08X\n"
            "winningAttempt=%d\n"
            "mmPairedAttempt=%u\n"
            "mmPlacementHash=%08X\n"
            "foreignCount=%d\n",
            (unsigned)kLadderMasterSeed, gSaveContext.save.shipSaveInfo.rando.finalSeed, attempts - 1,
            (unsigned)gComboCtx.mmPairedAttempt, placementHash, Combo_CountForeignPlacements());
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        const ComboForeignPlacement& p = gComboCtx.foreignPlacements[i];
        if (p.item.originGame == GAME_NONE) {
            continue;
        }
        fprintf(out, "foreign%d=%u:%u:%u\n", i, (unsigned)p.mmCheckId, (unsigned)p.item.originGame,
                (unsigned)p.item.id);
    }
    if (closeOut) {
        fclose(out);
    }
    fprintf(stderr, "[MM-ATTEMPT] PASS: pinned seed %u converged on ladder attempt %d (mmFinalSeed=%08X, "
                    "placementHash=%08X)\n",
            (unsigned)kLadderMasterSeed, attempts - 1, gSaveContext.save.shipSaveInfo.rando.finalSeed, placementHash);

    // Leave clean global state for later dispatches in the same process — the
    // pinned profile CVars included, since they are exactly the kind of
    // leaked-through-the-shared-config-store state this file's other bridges
    // document as a cross-dispatch determinism hazard.
    LadderClearOptionCVars();
    ComboContext_Init();
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    return 0;
}

// ============================================================================
// Attempt-ladder exhaustion lock (ADR 0010 increment 1.2 — lock (c) of the
// increment's CI set).
// ============================================================================
/**
 * A settings profile that CANNOT converge — every check user-excluded, so
 * every ladder attempt deterministically throws "No checks in logic" — must
 * exhaust the bounded ladder and surface the LOUD failure at the arrival:
 * save reverted to vanilla (no partial world), the active unified-save slot
 * latched REFUSED with RSBS_REFUSE_GENERATION through the #533 machinery
 * (this session's unpaired fallback world must never capture into the pair's
 * .redsave), and the shared-overlay toast emitted (presentation; the latch
 * and state are what this asserts). Driven through the FULL switch-entry
 * path — the only flow a player actually takes — exactly as
 * MM_Rando_HeadlessPairSwitchEntry drives it.
 *
 * A THIRD leg locks the determinism boundary: the fill's WALL-CLOCK abort is
 * the one failure that is a function of the machine rather than the seed, so
 * the ladder must stop on it instead of climbing to a different world. See
 * that leg's own comment for its counterfactual.
 *
 * COUNTERFACTUALS (each independently red): restore the silent-vanilla
 * fallback (drop RsbsSave_RefuseSlotGeneration from the arrival gate's
 * failure branch) and FAIL(6)/FAIL(7)/FAIL(8) fire; unbound the ladder and
 * the row times out instead of exhausting; latch unconditionally (refuse
 * even on success) and the counter-leg's FAIL(12)/FAIL(13) fire; treat a
 * wall-clock abort as a ladder rung and FAIL(15) fires.
 *
 * Returns 0 on success, a distinct nonzero step code otherwise.
 */
extern "C" int MM_Rando_HeadlessPairedExhaustion(void) {
    const uint16_t kArrival = 0xD800; // ENTRANCE(SOUTH_CLOCK_TOWN, 0) — the OoT->MM arrival

    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(1): Ship::Context not live\n");
        return 1;
    }

    MM_Rando_Init();
    if (Rando::Logic::Regions.empty()) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(2): Logic/Regions graph empty\n");
        return 2;
    }
    if (gRegEditor == NULL) {
        static RegEditor sExhaustRegEditor = {};
        gRegEditor = &sExhaustRegEditor;
    }

    LadderClearOptionCVars();
    CVarSetInteger("gRando.SpoilerFileIndex", 0);
    CVarSetInteger("gRando.GenerateSpoiler", 0);
    CVarSetString("gRando.InputSeed", "USERSEEDPOISON");

    // The cannot-converge fixture: exclude EVERY check. GeneratePools keeps
    // excluded checks out of checkPool (Logic/GeneratePools.cpp), so every
    // attempt throws "No checks in logic" before any fill work — the
    // exhaustion path in microseconds, not 10s-per-attempt wall clock. Built
    // in the canonical "id,id,..." form the UI writer emits (no trailing
    // comma), because ProfileIdentityString folds the raw string.
    {
        std::string excludeAll;
        for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
            if (randoStaticCheck.randoCheckId == RC_UNKNOWN) {
                continue;
            }
            if (!excludeAll.empty()) {
                excludeAll += ',';
            }
            excludeAll += std::to_string((int)randoCheckId);
        }
        CVarSetString("gRando.ExcludedChecks", excludeAll.c_str());
    }

    // Slot fixture, exactly as MMPairSwitchEntry's Phase 4 stands one up.
    const char* const kExhaustSaveDir = "rsbs_test_pairedexhaustion_saves";
    rsbs::SaveManager::Instance().SetSaveDirectory(kExhaustSaveDir);
    RsbsSave_ResetSlotSessionState();
    RsbsSave_ArmSlotOnCreate(0);
    RsbsSave_SetActiveSlot(0);
    if (!RsbsSave_IsSlotWritable(0)) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(3): could not arm the slot fixture\n");
        return 3;
    }

    // A fresh legacy (pre-freeze) pair arriving through the real switch-entry
    // path. No creation stamp: the identity gate must PASS this arrival (the
    // profile freezes at generation), so the refusal asserted below can only
    // come from the generation-failure branch — not from the identity gate.
    // Same master seed as the counter-leg below, so the two legs differ in
    // EXACTLY one input: the cannot-converge fixture.
    Combo_ClearForeignPlacements();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    ComboContext_Init();
    gComboCtx.sourceIsRando = 1;
    gComboCtx.sharedRandoSeed = kDefaultProfileConvergingSeed;
    gComboCtx.sharedRandoSettingsHash = 0x0E8A0570u;
    gComboCtx.mmProfileDigest = 0;

    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    Combo_SetStartupEntrance(kArrival);
    MM_Play_ConsumeStartupEntrance();

    if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(4): an all-excluded profile GENERATED a world — the cannot-converge "
                        "fixture premise is broken\n");
        return 4;
    }
    if (!MM_Rando_PairedGenLastExhausted() || MM_Rando_PairedGenLastAttempts() != MM_Rando_PairedGenMaxAttempts()) {
        fprintf(stderr,
                "[MM-EXHAUST] FAIL(5): the ladder did not run to its bound (attempts=%d of %d, exhausted=%d) — "
                "either an attempt short-circuited or the bound moved\n",
                MM_Rando_PairedGenLastAttempts(), MM_Rando_PairedGenMaxAttempts(), MM_Rando_PairedGenLastExhausted());
        return 5;
    }
    if (RsbsSave_GetSlotState(0) != (int)RSBS_SLOT_REFUSED) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(6): the exhausted arrival did not surface REFUSED on the active slot "
                        "(state=%d) — the silent-vanilla-revert class, back again (#564 V7)\n",
                RsbsSave_GetSlotState(0));
        return 6;
    }
    if (RsbsSave_GetSlotRefuseReason(0) != (int)RSBS_REFUSE_GENERATION) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(7): refusal reason is %d, expected RSBS_REFUSE_GENERATION — the file "
                        "panel would misname the failure\n",
                RsbsSave_GetSlotRefuseReason(0));
        return 7;
    }
    if (RsbsSave_IsSlotWritable(0)) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(8): the exhausted arrival left the slot writable — the unpaired vanilla "
                        "fallback session could capture into the pair's .redsave\n");
        return 8;
    }
    if (Combo_CountForeignPlacements() != 0) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(9): the aborted generation leaked %d foreign placements\n",
                Combo_CountForeignPlacements());
        return 9;
    }
    if (gComboCtx.mmPairedAttempt != 0) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(10): the aborted generation left a stale attempt-provenance stamp (%u)\n",
                (unsigned)gComboCtx.mmPairedAttempt);
        return 10;
    }
    fprintf(stderr, "[MM-EXHAUST] exhausted arrival refused loudly: vanilla revert, slot latched + surfaced "
                    "(reason=generation), no leaked placements\n");

    // ----------------------------------------------------------------------
    // Counter-leg (non-vacuity): the SAME arrival with the fixture undone
    // generates normally and latches nothing — the refusal is failure-driven,
    // not unconditional. All-default profile (the flipped Glitchless default
    // resolves and generates end to end here) with a master seed the pin scan
    // verified converges on attempt 0 under it.
    // ----------------------------------------------------------------------
    CVarClear("gRando.ExcludedChecks");
    RsbsSave_ResetSlotSessionState();
    RsbsSave_ArmSlotOnCreate(0);
    RsbsSave_SetActiveSlot(0);
    Combo_ClearForeignPlacements();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    ComboContext_Init();
    gComboCtx.sourceIsRando = 1;
    gComboCtx.sharedRandoSeed = kDefaultProfileConvergingSeed;
    gComboCtx.sharedRandoSettingsHash = 0x0E8A0570u;
    gComboCtx.mmProfileDigest = 0;

    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    Combo_SetStartupEntrance(kArrival);
    MM_Play_ConsumeStartupEntrance();

    if (gSaveContext.save.shipSaveInfo.saveType != SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(11): the counter-leg arrival failed to generate — the exhaustion leg's "
                        "assertions above cannot be attributed to the fixture\n");
        return 11;
    }
    if (RsbsSave_GetSlotState(0) == (int)RSBS_SLOT_REFUSED || !RsbsSave_IsSlotWritable(0)) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(12): a SUCCESSFUL paired arrival latched or refused the slot — the "
                        "refusal is not failure-driven\n");
        return 12;
    }
    if (gComboCtx.mmPairedAttempt == 0) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(13): the successful counter-leg did not record its winning attempt\n");
        return 13;
    }
    fprintf(stderr, "[MM-EXHAUST] counter-leg generated normally (attempt record %u), slot stays writable\n",
            (unsigned)gComboCtx.mmPairedAttempt);

    // ----------------------------------------------------------------------
    // Wall-clock leg (determinism): the Glitchless fill's 10s abort is the ONE
    // failure that is a function of the machine rather than of the seed, so
    // the ladder must NOT treat it as a rung. If it did, the same identity
    // would converge on attempt 0 on a fast machine and on some later attempt
    // on a slow one — two players, one seed, two different worlds, which is
    // the exact thing the ladder's hash recipe exists to make impossible.
    //
    // Driven by squeezing the fill's budget to 1ms (Logic.h's test-only
    // override) on the SAME profile and SAME seed the counter-leg above just
    // generated successfully, so the only changed input is "the fill ran out
    // of wall clock". The assertion that carries the property is
    // attempts == 1: the ladder stopped at the first attempt instead of
    // climbing.
    //
    // COUNTERFACTUAL (measured, not asserted): delete the
    // `catch (const Rando::Logic::GenerationTimeout&)` arm from the ladder in
    // OnFileCreate.cpp so a timeout falls into the generic dead-end arm — the
    // ladder then climbs all ten rungs and this leg goes red at FAIL(14)
    // (attempts=10, exhausted=1).
    // ----------------------------------------------------------------------
    RsbsSave_ResetSlotSessionState();
    RsbsSave_ArmSlotOnCreate(0);
    RsbsSave_SetActiveSlot(0);
    Combo_ClearForeignPlacements();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    ComboContext_Init();
    gComboCtx.sourceIsRando = 1;
    gComboCtx.sharedRandoSeed = kDefaultProfileConvergingSeed;
    gComboCtx.sharedRandoSettingsHash = 0x0E8A0570u;
    gComboCtx.mmProfileDigest = 0;

    Rando::Logic::gRsbsGlitchlessTimeoutMsOverride = 1; // the fill cannot finish in 1ms
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);
    Combo_SetStartupEntrance(kArrival);
    MM_Play_ConsumeStartupEntrance();
    Rando::Logic::gRsbsGlitchlessTimeoutMsOverride = 0; // restore before ANY assertion can return

    if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(14): a 1ms fill budget still produced a world — the wall-clock leg's "
                        "premise is broken and its determinism assertions below prove nothing\n");
        return 14;
    }
    if (MM_Rando_PairedGenLastAttempts() != 1 || MM_Rando_PairedGenLastExhausted()) {
        fprintf(stderr,
                "[MM-EXHAUST] FAIL(15): a WALL-CLOCK abort climbed the ladder (attempts=%d exhausted=%d, expected "
                "1/0) — the winning attempt, and therefore the world, would depend on machine speed: the same seed "
                "and settings would generate differently on a slow machine than on a fast one\n",
                MM_Rando_PairedGenLastAttempts(), MM_Rando_PairedGenLastExhausted());
        return 15;
    }
    if (RsbsSave_GetSlotState(0) != (int)RSBS_SLOT_REFUSED ||
        RsbsSave_GetSlotRefuseReason(0) != (int)RSBS_REFUSE_GENERATION) {
        fprintf(stderr,
                "[MM-EXHAUST] FAIL(16): the wall-clock failure did not refuse loudly (state=%d reason=%d) — "
                "stopping the ladder must take the LOUD path, not a quiet vanilla Termina\n",
                RsbsSave_GetSlotState(0), RsbsSave_GetSlotRefuseReason(0));
        return 16;
    }
    if (gComboCtx.mmPairedAttempt != 0) {
        fprintf(stderr, "[MM-EXHAUST] FAIL(17): the wall-clock abort left a stale attempt-provenance stamp (%u)\n",
                (unsigned)gComboCtx.mmPairedAttempt);
        return 17;
    }
    fprintf(stderr, "[MM-EXHAUST] wall-clock leg: the fill's non-deterministic abort stopped the ladder at attempt 1 "
                    "and refused loudly — no machine-speed-dependent rung\n");

    // Leave clean global state for later dispatches in the same process.
    Rando::Logic::gRsbsGlitchlessTimeoutMsOverride = 0;
    RsbsSave_DeleteSave(0);
    RsbsSave_ResetSlotSessionState();
    RsbsSave_SetActiveSlot(-1);
    rsbs::SaveManager::Instance().SetSaveDirectory("Save"); // the documented harness default
    {
        std::error_code exhaustEc;
        std::filesystem::remove_all(kExhaustSaveDir, exhaustEc);
    }
    LadderClearOptionCVars();
    Combo_ClearForeignPlacements();
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();

    fprintf(stderr, "[MM-EXHAUST] PASS: a cannot-converge profile exhausts the bounded ladder and refuses loudly "
                    "through the #533 surface; a converging one generates and latches nothing\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
