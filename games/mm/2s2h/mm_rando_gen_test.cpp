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
#include <cstring>
#include <cstdint>
#include <fstream>
#include <string>

#include <ship/Context.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>

#include "2s2h/BenPort.h" // appShortName
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

    const int placedCount = Combo_CountForeignPlacements();
    if (placedCount != poolCount) {
        fprintf(stderr,
                "[MM-RANDO-GEN] FAIL(11): expected %d foreign placements, found %d (test-side pairing key: "
                "sourceIsRando=%d settingsHash=%08X seed=%08X)\n",
                poolCount, placedCount, gComboCtx.sourceIsRando ? 1 : 0, gComboCtx.sharedRandoSettingsHash,
                gComboCtx.sharedRandoSeed);
        return 11;
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
        const RandoItemId heldItem = RANDO_SAVE_CHECKS[hostCheck].randoItemId;
        if (!RANDO_SAVE_CHECKS[hostCheck].shuffled ||
            Rando::StaticData::Items[heldItem].randoItemType != RITYPE_JUNK) {
            fprintf(stderr,
                    "[MM-RANDO-GEN] FAIL(12): hosting check %u does not hold a junk-class MM item (holds %d)\n",
                    (unsigned)p.mmCheckId, (int)heldItem);
            return 12;
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
        (int)pairedSpoiler["foreign"].size() != poolCount) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(14): spoiler 'foreign' section missing or wrong size\n");
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
    if (reconstructed != poolCount) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(17): reconstructed %d placements, expected %d\n", reconstructed,
                poolCount);
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
    if (Combo_CountForeignPlacements() != poolCount) {
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(18): reconstructed count %d != generated %d\n",
                Combo_CountForeignPlacements(), poolCount);
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
    if (Combo_CountForeignPlacements() != poolCount) {
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
                    "redemption-safe, malformed-refused, absent-ok, apply-wired\n");

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
    fprintf(stderr, "[MM-RANDO-GEN] VB dispatch (rando save): give VBs suppressed (ForID + non-ID), "
                    "pass-through intact (%zu VB hooks)\n", vbHooks);

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
 * Deliberately attempt-free: the derived MM seed is a pure function of the
 * digest run's pinned OoT seed. If that derivation ever dead-ends the MM
 * fill, this fails loudly (rather than retrying into a world the derivation
 * cannot name) and the fix is to pin a different determinism seed.
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

    // Pin every CVar this generation depends on — CVar state can leak between
    // dispatches through the shared config store, and determinism must not
    // hinge on which test ran first. No spoiler file: the digest is the
    // artifact, and skipping the write avoids the same-path-overwrite pitfall
    // the OoT digest documents.
    CVarSetInteger("gRando.Enabled", 1);
    CVarSetInteger("gRando.SpoilerFileIndex", 0);
    CVarSetInteger("gRando.GenerateSpoiler", 0);
    CVarSetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_NEARLY_NO_LOGIC);
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
    // turning lock (c) vacuous. Fail loudly instead.
    {
        const ComboForeignItemDef* digestPool = NULL;
        const int digestPoolCount = Combo_GetForeignItemPool(&digestPool);
        if (Combo_CountForeignPlacements() != digestPoolCount) {
            fprintf(stderr, "[MM-FOREIGN-DIGEST] FAIL(5): expected %d foreign placements, found %d\n", digestPoolCount,
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
            "foreignCount=%d\n",
            gSaveContext.save.shipSaveInfo.rando.finalSeed, mmPlacementHash, Combo_CountForeignPlacements());
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
    fprintf(stderr, "[MM-FOREIGN-DIGEST] mmFinalSeed=%08X mmPlacementHash=%08X foreign=%d\n",
            gSaveContext.save.shipSaveInfo.rando.finalSeed, mmPlacementHash, Combo_CountForeignPlacements());
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

        // The cold gamestate-chain boot a switch performs: ConsoleLogo skips
        // to TitleSetup, TitleSetup authors a VANILLA bootstrap file with
        // MM_Sram_InitNewSave and dispatches OnSaveLoad against it. File
        // select is never touched, so OnSaveInit is never dispatched here —
        // that absence is the whole bug.
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        MM_Sram_InitNewSave();
        GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);

        const size_t hooksAfterBootChain = S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>();

        // The switch path's pending arrival, then the real consumption point.
        Combo_SetStartupEntrance(kArrival);
        MM_Play_ConsumeStartupEntrance();

        if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
            const size_t hooksAfterConsume = S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>();
            if (hooksAfterConsume == 0 || hooksAfterConsume <= hooksAfterBootChain) {
                fprintf(stderr,
                        "[MM-PAIR-SWITCH] FAIL(5): paired world generated but the IS_RANDO hooks were left "
                        "DISARMED by the boot chain's OnSaveLoad (OnFlagSet %zu -> %zu) — MM would play with "
                        "vanilla behavior in a rando world\n",
                        hooksAfterBootChain, hooksAfterConsume);
                return 5;
            }
            fprintf(stderr, "[MM-PAIR-SWITCH] hooks re-armed at consumption (OnFlagSet %zu -> %zu)\n",
                    hooksAfterBootChain, hooksAfterConsume);
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

    if (Combo_CountForeignPlacements() != poolCount) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(7): expected %d foreign placements after switch-entry pairing, found "
                        "%d\n",
                poolCount, Combo_CountForeignPlacements());
        return 7;
    }
    if (gSaveContext.save.entrance != kArrival) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(8): arrival entrance 0x%04X lost to generation's start state "
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
        if (!j.contains("foreign") || !j["foreign"].is_object() || (int)j["foreign"].size() != poolCount) {
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
    const size_t returnHooksAfterBootChain = S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>();
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
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(13): return leg did not preserve the player's progress "
                        "(rupees=%d day=%d)\n",
                gSaveContext.save.saveInfo.playerData.rupees, gSaveContext.save.day);
        return 13;
    }
    if (memcmp(pairedPlacements, gComboCtx.foreignPlacements, sizeof(pairedPlacements)) != 0) {
        fprintf(stderr, "[MM-PAIR-SWITCH] FAIL(14): return leg disturbed the foreign placement table\n");
        return 14;
    }
    {
        const size_t after = S2H::GameHooks::CountForTest<GameInteractor::OnFlagSet>();
        if (after == 0 || after <= returnHooksAfterBootChain) {
            fprintf(stderr,
                    "[MM-PAIR-SWITCH] FAIL(15): return leg left the IS_RANDO hooks disarmed (OnFlagSet %zu -> "
                    "%zu) — a restored rando save would play with vanilla behavior\n",
                    returnHooksAfterBootChain, after);
            return 15;
        }
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
    fprintf(stderr, "[MM-PAIR-SWITCH] existing vanilla save left untouched (skip path)\n");

    // Leave clean global state for later dispatches in the same process.
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    Combo_ClearForeignPlacements();
    memset(&gSaveContext, 0, sizeof(gSaveContext));

    fprintf(stderr, "[MM-PAIR-SWITCH] PASS: pairing activates on the switch-entry path, existing saves untouched\n");
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

#endif // RSBS_SINGLE_EXECUTABLE
