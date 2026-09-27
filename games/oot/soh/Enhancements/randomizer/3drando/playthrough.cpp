#include "playthrough.hpp"

#include <libultraship/libultraship.h>
#include "fill.hpp"
#include "../location_access.h"
#include "random.hpp"
#include "spoiler_log.hpp"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/ShipUtils.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "variables.h"
#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "../option.h"
#include "soh/Enhancements/debugger/performanceTimer.h"
#include "context.h" // src/common — gComboCtx, Lane B unified-seed carrier (ADR 0002)
#ifdef RSBS_SINGLE_EXECUTABLE
#include "foreign_items.h" // src/common — the combo record and pairing identity
// src/common — MM_Rando_ComputeProfileStamp (defined MM-side, Foreign.cpp):
// the creation event freezes the MM half's option profile too (#498/#564).
#include "combo_mm_options_view.h"
#include "gen_budget.h" // src/common — the #582 progress surface (OoT's half)
// ComboLogicEngineOoT.cpp: the paired world's general pass, deferred to the
// single-bag fill at the creation event (ADR 0010 increment 3, lane K11).
extern "C" void OoT_ComboLogic_SetGeneralPassDeferred(int deferred);
extern "C" int OoT_ComboLogic_GeneralPassDeferred(void);
#endif

namespace Playthrough {

int Playthrough_Init(uint32_t seed, std::set<RandomizerCheck> excludedLocations,
                     std::set<RandomizerTrick> enabledTricks) {
    // initialize the RNG with just the seed incase any settings need to be
    // resolved to something random
    Random_Init(seed);

    auto ctx = Rando::Context::GetInstance();
#ifdef RSBS_SINGLE_EXECUTABLE
    // A new generation replaces the world, and with it any general pass a
    // previous paired generation deferred to a creation that never happened.
    OoT_ComboLogic_SetGeneralPassDeferred(0);
#endif
    ctx->overrides.clear();
    ctx->ItemReset();
    ctx->HintReset();
    ctx->GetLogic()->Reset();
    StartPerformanceTimer(PT_REGION_RESET);
    Regions::AccessReset();
    StopPerformanceTimer(PT_REGION_RESET);

    ctx->FinalizeSettings(excludedLocations, enabledTricks);
    // once the settings have been finalized turn them into a string for hashing
    std::string settingsStr;
    auto& optionGroups = Rando::Settings::GetInstance()->GetOptionGroups();
    for (size_t i = 0; i < RSG_MAX; i++) {
        auto& optionGroup = optionGroups[i];
        // don't go through non-menus
        if (optionGroup.GetContainsType() == Rando::OptionGroupType::SUBGROUP) {
            continue;
        }

        for (Rando::Option* option : optionGroup.GetOptions()) {
            if (option->IsCategory(Rando::OptionCategory::Setting)) {
                if (option->GetOptionCount() > 0) {
                    if (i >= RSG_EXCLUDES_KOKIRI_FOREST && i <= RSG_EXCLUDES_GANONS_CASTLE) {
                        auto locationOption = static_cast<Rando::LocationOption*>(option);
                        settingsStr += option->GetOptionText(ctx->GetLocationOption(locationOption->GetKey()).Get());
                    } else if (i == RSG_TRICKS) {
                        auto trickOption = static_cast<Rando::TrickOption*>(option);
                        settingsStr += option->GetOptionText(ctx->GetTrickOption(trickOption->GetKey()).Get());
                    } else {
                        settingsStr += option->GetOptionText(ctx->GetOption(option->GetKey()).Get());
                    }
                }
            }
        }
    }

    // Lane B (ADR 0002 §3): fingerprint the finalized settings profile BEFORE the
    // DontGenerateSpoiler build-version mixing below, so the digest identifies the
    // settings alone — seed-independent and build-independent. This is the second
    // half of the unified-seed contract: sharedRandoSeed alone does NOT determine
    // the fill, because the RNG is re-seeded with Hash(seed + settingsStr) just
    // below, so the same numeric seed reproduces a world only under the same
    // settings (see gComboCtx.sharedRandoSettingsHash).
    const uint32_t rsbsSettingsHash = SohUtils::Hash(settingsStr);

    if (CVarGetInteger(CVAR_RANDOMIZER_SETTING("DontGenerateSpoiler"), 0)) {
        settingsStr += (char*)OoT_gBuildVersion;
    }

    uint32_t finalHash = SohUtils::Hash(std::to_string(ctx->GetSeed()) + settingsStr);
    Random_Init(finalHash);
    ctx->SetHash(std::to_string(finalHash));

#ifdef RSBS_SINGLE_EXECUTABLE
    // ========================================================================
    // THE FREEZE (#564 creation-event step 1; ADR 0009 decision 2's amendment;
    // solver-inventory P1/P2; ADR 0010 increment 2).
    //
    // WHY THIS BLOCK MOVED ABOVE Fill(). It used to sit after the fill and the
    // spoiler write, which made the whole identity a POST-hoc receipt of a world
    // already decided. Two things were wrong with that, and both are structural
    // rather than cosmetic:
    //
    //  (1) ADR 0009 D2's amendment requires the MM option profile to freeze
    //      BEFORE OoT's Fill(), because the reverse-pool membership rule
    //      (criterion 3, games/mm/2s2h/Rando/ForeignItemsSingleExe.cpp) excludes
    //      whole item families ONLY because "OoT's placement pass runs at OoT
    //      generation time — possibly before the paired MM world exists at all".
    //      That is a TIMING accident, and this is the line that retires it: from
    //      here on, everything downstream — the fill, both crossing passes, the
    //      MM generation the creation seam now runs (z_sram.c) — reads one
    //      frozen surface, and nothing reads a CVar.
    //  (2) Increment 3's single-bag fill IS Fill(), so a freeze that happens
    //      after it can never be the fill's input. Moving it now is what makes
    //      that increment a change of algorithm rather than a change of order.
    //
    // NOTHING HERE CONSUMES THE RNG STREAM, which is why moving it above Fill()
    // does not move a single generated world: MM_Rando_ComputeProfileStamp reads
    // CVars and hashes, Combo_ResolveComboSettings reads CVars, and
    // Combo_FreezeComboSettings is a struct copy plus an FNV fold. Random_Init
    // above is the last thing to touch the stream before Fill() draws from it,
    // exactly as before. (SeedDeterminism / MMRandoGen / HeadlessForeignDigest
    // are unchanged by this commit, and that is the acceptance bar for it.)
    //
    // ORDER INSIDE THE BLOCK IS ADR 0011 decision 4.1's ORDER AND IS
    // LOAD-BEARING: seed + settings hash -> MM profile digest -> combo record ->
    // comboSettingsHash LAST, because the fingerprint folds both half-digests
    // (accepted answer O6 — a digest narrower than the generator's input set is
    // vacuous), so computing it any earlier would fold a term not yet decided.
    //
    // ROLLBACK, NOT "STAMP AND HOPE". #564 step 8 says the identity is published
    // as the post-condition of the WHOLE creation: no partial identity, ever.
    // Freezing before the fill means a fill that fails would otherwise leave a
    // stamped identity behind with no world under it — the exact state
    // Combo_MMProfileFrozen() and the options pane read as "this world's rules
    // are decided". So the previous terms are snapshotted here and restored on
    // every failure exit below. The two states are "fully frozen" and
    // "untouched"; there is no third.
    // ========================================================================
    const bool rsbsPriorSourceIsRando = gComboCtx.sourceIsRando;
    const uint32_t rsbsPriorSeed = gComboCtx.sharedRandoSeed;
    const uint32_t rsbsPriorSettingsHash = gComboCtx.sharedRandoSettingsHash;
    const uint32_t rsbsPriorMmProfileDigest = gComboCtx.mmProfileDigest;
    const ComboSettingsRecord rsbsPriorComboSettings = gComboCtx.comboSettings;
    const uint32_t rsbsPriorComboSettingsHash = gComboCtx.comboSettingsHash;
    const uint32_t rsbsPriorGiveCaps = Combo_ForeignGiveCaps((uint8_t)GAME_MM);
    const bool rsbsPriorGiveCapsPublished = Combo_ForeignGiveCapsPublished((uint8_t)GAME_MM);
    auto rsbsRollbackFreeze = [&]() {
        gComboCtx.sourceIsRando = rsbsPriorSourceIsRando;
        gComboCtx.sharedRandoSeed = rsbsPriorSeed;
        gComboCtx.sharedRandoSettingsHash = rsbsPriorSettingsHash;
        gComboCtx.mmProfileDigest = rsbsPriorMmProfileDigest;
        gComboCtx.comboSettings = rsbsPriorComboSettings;
        gComboCtx.comboSettingsHash = rsbsPriorComboSettingsHash;
        Combo_ClearForeignGiveCaps();
        if (rsbsPriorGiveCapsPublished) {
            Combo_PublishForeignGiveCaps((uint8_t)GAME_MM, rsbsPriorGiveCaps);
        }
        SPDLOG_ERROR("Paired identity: generation failed; the creation freeze was rolled back (no partial identity)");
    };

    // THE PRE-FILL PAIRING GATE (ADR 0009 decision 2; ADR 0010 :615;
    // solver-inventory P2; finished in #657). It is asked HERE, in the future
    // tense, before a single item is placed, because that is the only place an
    // answer can still shape the fill; increment 3's single-bag fill is its real
    // consumer.
    //
    // TWO QUESTIONS, NOT ONE, and #667 is why they had to be separated before
    // this gate could have a consequence:
    //
    //   Combo_ForeignPairingRequested()   -- is a paired WORLD being asked for?
    //                                        TRUE for every pinned direction,
    //                                        OFF included: OFF is a paired world
    //                                        with zero crossings, so a gate that
    //                                        skipped the paired creation for it
    //                                        would ship a file whose MM half was
    //                                        never authored.
    //   Combo_ForeignCrossingsRequested() -- does this creation author any
    //                                        CROSSINGS? The old body of the
    //                                        predicate above, and the answer the
    //                                        freeze below has to preserve.
    //
    // A creation that cannot answer the first question YES must not generate at
    // all. Nothing is frozen yet at this point and no progress session is open,
    // so the refusal is a plain early return: no rollback to do, no partial
    // identity to retract, and — deliberately — no world on disk whose rules no
    // consumer can interpret (see Combo_ComboSettingsDescribePairedWorld).
    const bool rsbsPairingRequested = Combo_ForeignPairingRequested();
    const bool rsbsCrossingsRequested = Combo_ForeignCrossingsRequested();
    if (!rsbsPairingRequested) {
        SPDLOG_ERROR("Paired identity: the resolved combo record does not describe a creatable paired world "
                     "(direction {}); refusing to generate (#657)",
                     (unsigned)Combo_ComboDirection());
        return -1;
    }

    // The progress surface (#582), OoT's half. Two sessions per paired creation,
    // not one: this one measures the staged OoT generation, and the file-create
    // seam opens a second one for the MM half. Measuring them as one session
    // would fold in however long the player spent in the menu between pressing
    // Generate and creating the file, which is not a cost anything can budget.
    Combo_GenProgress_Begin();
    Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_FREEZE, 0, nullptr);

    // Lane B unified-seed producer (ADR 0002 §3) — the ONE sanctioned stamp site
    // (#598). OoT's legacy freeze-time producer (OoT_FreezeState) was deleted
    // outright and MM's mirror sits behind a never-defined macro, so neither is
    // precedent for a second writer. Both the live GUI path (RandoMain::
    // GenerateRando -> GenerateRandomizer -> here) and the headless harness
    // (Rando_HeadlessSeedTest -> GenerateRandomizer -> here) funnel through this
    // point. `seed` == ctx->GetSeed() (the caller passes exactly that). These
    // are carried through file-create invalidation by the KEEP policy
    // (context.cpp).
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = seed;
    gComboCtx.sharedRandoSettingsHash = rsbsSettingsHash;

    // The MM half's option profile, frozen INTO the pairing identity.
    // MM_Rando_ComputeProfileStamp resolves the full profile (every registered
    // option with the resolved RO_LOGIC pin, excluded checks, starting items)
    // from the CVars through the SAME computation MM's arrival re-runs to
    // compare; a mismatch at arrival is refused through the #533 machinery,
    // never honored. This is its ONLY writer for post-freeze pairs.
    gComboCtx.mmProfileDigest = MM_Rando_ComputeProfileStamp();
    // ...and the VALUES half of the same freeze (ADR 0011 O8): the give
    // capabilities the frozen profile arms, published game-neutrally so the
    // reverse placement pass below can finally ask a question a digest could
    // never answer. See foreign_items.h.
    MM_Rando_PublishProfileGiveCaps(/*fromSave=*/0);
    SPDLOG_INFO("Paired identity: MM profile frozen at creation (digest {:08X}, giveCaps {:04X}, paired world "
                "requested {}, crossings requested {})",
                gComboCtx.mmProfileDigest, Combo_ForeignGiveCaps((uint8_t)GAME_MM), rsbsPairingRequested ? 1 : 0,
                rsbsCrossingsRequested ? 1 : 0);

    // ADR 0011 decision 4.1: the creation event also freezes the COMBO-LEVEL
    // rules — the ones governing the crossing itself, which belong to neither
    // game's save by construction — and computes the whole-pair fingerprint over
    // all three terms.
    {
        ComboSettingsRecord comboSettings;
        Combo_ResolveComboSettings(&comboSettings);
        Combo_FreezeComboSettings(&comboSettings);
        SPDLOG_INFO("Paired identity: combo settings frozen at creation (fingerprint {:08X})",
                    gComboCtx.comboSettingsHash);
    }

    // THE GATE'S SECOND ACT (#657). Asking the CVars before Fill() is only half
    // of what ADR 0009 decision 2 designed: the answer then has to be FROZEN, so
    // that every later reader — the file-create seam, both placement passes,
    // every arrival — reaches the same conclusion without consulting a CVar
    // again. This is the assertion that the freeze actually preserved it.
    //
    // A disagreement here is not a state to honour. It means either nothing
    // froze, or something moved the direction between the ask and the freeze, so
    // the world about to be filled is not the world the player asked for — and a
    // combo-level decision freezes at creation, once, for the life of the file.
    // Rolled back the same way a failed fill is: the two states are "fully
    // frozen" and "untouched", never a third.
    if (!Combo_ForeignCreationGateHolds(rsbsCrossingsRequested)) {
        SPDLOG_ERROR("Paired identity: the pre-Fill gate's answer did not survive the freeze; aborting generation "
                     "(#657)");
        rsbsRollbackFreeze();
        Combo_GenProgress_End(false);
        return -1;
    }

    // THE GOAL MUST HAVE AN EVALUATOR, AND THIS IS WHERE A CREATION CAN STILL BE
    // TOLD SO (ADR 0010 increment 3, lane K11). The single-bag fill's exit
    // condition is the frozen GOAL, and RSBS_COMBO_GOAL_TRIFORCE_HUNT has no
    // evaluator in this build: answer O10's ONE shared piece count across both
    // worlds is not on main (combo_logic.h, RSBS_COMBO_LOGIC_ERR_UNSUPPORTED_GOAL).
    // Refused HERE, at Generate, with the reason, rather than after the player has
    // named a file — and never silently proved as beat-both, which would ship a
    // world whose stated goal is not the one that was proved.
    if (gComboCtx.comboSettings.goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) {
        SPDLOG_ERROR("Paired identity: the combo GOAL is triforce-hunt, which this build cannot prove (ADR 0010 "
                     "O10's shared piece count is not built); refusing to generate");
        fprintf(stderr, "[OoT] generation REFUSED: combo GOAL triforce-hunt has no evaluator in this build (one "
                        "shared piece count across both worlds, ADR 0010 O10, is not built yet)\n");
        rsbsRollbackFreeze();
        Combo_GenProgress_End(false);
        return -1;
    }
#endif

#ifdef RSBS_SINGLE_EXECUTABLE
    Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_OOT_FILL, 0, nullptr);
#endif
    int ret = Fill();
    if (ret < 0) {
#ifdef RSBS_SINGLE_EXECUTABLE
        rsbsRollbackFreeze();
        Combo_GenProgress_End(false);
#endif
        return ret;
    }

    GenerateHash();

#ifdef RSBS_SINGLE_EXECUTABLE
    // A PAIRED world's OoT half is not finished yet: Fill() stopped at its general
    // pass, and the single-bag fill at the creation event places the rest. Its
    // spoiler is written there, once, describing the world the player will play;
    // a spoiler written here would describe a world with its general pass empty.
    const bool rsbsGeneralPassDeferred = OoT_ComboLogic_GeneralPassDeferred() != 0;
#else
    const bool rsbsGeneralPassDeferred = false;
#endif
    if (!rsbsGeneralPassDeferred) {
        // TODO: Handle different types of file output (i.e. Spoiler Log, Plando Template, Patch Files, Race Files,
        // etc.)
        //  write logs
#ifdef RSBS_SINGLE_EXECUTABLE
        Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_SPOILER, 0, nullptr);
#endif
        SPDLOG_INFO("Writing Spoiler Log...");
        StartPerformanceTimer(PT_SPOILER_LOG);
        if (SpoilerLog_Write()) {
            SPDLOG_INFO("Writing Spoiler Log Done");
        } else {
            SPDLOG_ERROR("Writing Spoiler Log Failed");
        }
        StopPerformanceTimer(PT_SPOILER_LOG);
    }

    ctx->playthroughLocations.clear();
    ctx->playthroughBeatable = false;

#ifndef RSBS_SINGLE_EXECUTABLE
    // Lane B unified-seed producer (ADR 0002 §3), standalone-SoH shape. In the
    // single exe this stamp moved ABOVE Fill() with the rest of the creation
    // freeze (see the block there); a standalone SoH has no MM half, no combo
    // record and no crossing pass, so there is nothing for it to precede and the
    // post-hoc stamp stays where it was.
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = seed;
    gComboCtx.sharedRandoSettingsHash = rsbsSettingsHash;
#endif

#ifdef RSBS_SINGLE_EXECUTABLE
    // THE REVERSE OVERLAY PASS IS RETIRED (ADR 0010 increment 3, D3; lane K11).
    // It used to hand a few MM items to OoT chests HERE, as duplicate copies on
    // top of OoT's finished fill. Under one bag nothing is a duplicate: every
    // crossing is a placement the single-bag fill makes at the creation event,
    // with the item removed from its origin's pool, and this generation has not
    // placed its general pass at all yet.
    Combo_GenProgress_End(true);
#endif

    return 1;
}

// used for generating a lot of seeds at once
int Playthrough_Repeat(std::set<RandomizerCheck> excludedLocations, std::set<RandomizerTrick> enabledTricks,
                       int count /*= 1*/) {
    SPDLOG_INFO("GENERATING {} SEEDS", count);
    auto ctx = Rando::Context::GetInstance();
    uint32_t repeatedSeed = 0;
    for (int i = 0; i < count; i++) {
        char seedString[11];
        for (size_t i = 0; i < 10; i++) {
            seedString[i] = '0' + ShipUtils::Random(0, 10);
        }
        seedString[10] = '\0';
        ctx->SetSeedString(std::string(seedString));
        repeatedSeed = SohUtils::Hash(ctx->GetSeedString());
        ctx->SetSeed(repeatedSeed);
        SPDLOG_DEBUG("testing seed: %d", repeatedSeed);
        ClearProgress();
        Playthrough_Init(ctx->GetSeed(), excludedLocations, enabledTricks);
        SPDLOG_INFO("Seeds Generated: {}", i + 1);
    }

    return 1;
}
} // namespace Playthrough