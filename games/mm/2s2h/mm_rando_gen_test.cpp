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
#include <fstream>
#include <string>

#include <ship/Context.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>

#include "2s2h/BenPort.h" // appShortName
#include "2s2h/GameInteractor/GameInteractor.h" // S2H::GameHooks counters (single-exe tail)
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Foreign.h"
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
    // otherwise carry (Lane D is deferred), and the vendored upstream
    // glitchless fill deterministically dead-ends on
    // RC_DEKU_KINGS_CHAMBER_MONKEY under default options + default starting
    // items (every seed; the check's own condition and the give path both
    // verify correct in isolation — see the follow-up issue). The
    // non-gating glitchless probe below keeps that state visible in every
    // run.
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

    std::string spoilerPath =
        Ship::Context::GetPathRelativeToAppDirectory(std::string("randomizer/") + usedSeed + ".json", appShortName);
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

    // Non-gating glitchless probe: the vendored upstream glitchless fill
    // currently dead-ends on RC_DEKU_KINGS_CHAMBER_MONKEY with default
    // options (see the follow-up issue filed from Lane C0). Run one attempt
    // and REPORT so every CI log tracks whether that state changes — it does
    // not gate this reachability lock.
    CVarSetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_GLITCHLESS);
    CVarSetString("gRando.InputSeed", "RSBSMMGL1");
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    GameInteractor_ExecuteOnSaveInit(0);
    fprintf(stderr, "[MM-RANDO-GEN][info] glitchless probe: %s\n",
            gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO ? "GENERATED" : "dead-ended (known)");
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
        fprintf(stderr, "[MM-RANDO-GEN] FAIL(11): expected %d foreign placements, found %d\n", poolCount, placedCount);
        return 11;
    }
    // ADR 0002: hosting checks keep a legal MM item (RI_JUNK) in the MM save
    // table; every recorded placement carries the OoT origin tag.
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
        if (!RANDO_SAVE_CHECKS[hostCheck].shuffled || RANDO_SAVE_CHECKS[hostCheck].randoItemId != RI_JUNK) {
            fprintf(stderr, "[MM-RANDO-GEN] FAIL(12): hosting check %u does not hold RI_JUNK in the MM table\n",
                    (unsigned)p.mmCheckId);
            return 12;
        }
    }

    // The spoiler landed under the DERIVED paired name and describes every
    // crossing: "foreign" as {checkName: {originGame, item}}, and the human-
    // readable checks list reads as the foreign item, not junk.
    std::string pairedSpoilerPath = Ship::Context::GetPathRelativeToAppDirectory(
        std::string("randomizer/") + "RSBSPAIR" + std::to_string(gComboCtx.sharedRandoSeed) + ".json", appShortName);
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

#endif // RSBS_SINGLE_EXECUTABLE
