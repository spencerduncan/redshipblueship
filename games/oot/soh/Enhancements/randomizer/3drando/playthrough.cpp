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
#include "foreign_items.h" // src/common — OoT_PlaceForeignItems (#510)
// src/common — MM_Rando_ComputeProfileStamp (defined MM-side, Foreign.cpp):
// the creation event freezes the MM half's option profile too (#498/#564).
#include "combo_mm_options_view.h"
#endif

namespace Playthrough {

int Playthrough_Init(uint32_t seed, std::set<RandomizerCheck> excludedLocations,
                     std::set<RandomizerTrick> enabledTricks) {
    // initialize the RNG with just the seed incase any settings need to be
    // resolved to something random
    Random_Init(seed);

    auto ctx = Rando::Context::GetInstance();
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

    int ret = Fill();
    if (ret < 0) {
        return ret;
    }

    GenerateHash();

    if (true) {
        // TODO: Handle different types of file output (i.e. Spoiler Log, Plando Template, Patch Files, Race Files,
        // etc.)
        //  write logs
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

    // Lane B unified-seed producer (ADR 0002 §3): publish this OoT world's
    // identity into the process-global ComboContext at GENERATION time — NOT at
    // freeze time. This is the ONE sanctioned stamp site (#598): OoT's legacy
    // freeze-time producer (OoT_FreezeState) has been deleted outright, and MM's
    // mirror in games/mm/2s2h/BenPort.cpp sits behind the same never-defined
    // `SINGLE_EXECUTABLE_BUILD` macro — neither is precedent for a second
    // writer. Both the live GUI path
    // (RandoMain::GenerateRando -> GenerateRandomizer -> here) and the headless
    // harness (Rando_HeadlessSeedTest -> GenerateRandomizer -> here) funnel
    // through this one point, and we only reach it on a fully successful fill +
    // spoiler write. `seed` == ctx->GetSeed() (the caller passes exactly that).
    // MM (Lane C) consumes these when it becomes reachable, to reproduce the
    // paired world (sharedRandoSeed) and verify the pinned settings profile
    // matches (sharedRandoSettingsHash).
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = seed;
    gComboCtx.sharedRandoSettingsHash = rsbsSettingsHash;

#ifdef RSBS_SINGLE_EXECUTABLE
    // #498/#564 phase 2 step 9: the creation event freezes the MM half's
    // option profile INTO the pairing identity, here, alongside the seed and
    // settings stamps it already publishes — everything about the one game
    // decides at one creation event. MM_Rando_ComputeProfileStamp resolves the
    // full profile (47 options with the resolved RO_LOGIC pin, excluded
    // checks, starting items) from the CVars through the SAME computation MM's
    // arrival re-runs to compare; a mismatch at arrival is refused through the
    // #533 machinery, never honored. This stamp — like the two above — is
    // carried through file-create invalidation by the KEEP policy
    // (context.cpp), and this is its ONLY writer for post-freeze pairs.
    gComboCtx.mmProfileDigest = MM_Rando_ComputeProfileStamp();
    SPDLOG_INFO("Paired identity: MM profile frozen at creation (digest {:08X})", gComboCtx.mmProfileDigest);

    // #510, the reverse foreign pool: hand a few MM items to OoT checks now that
    // this world's paired identity is stamped just above (the placement stream is
    // derived from it, so the order is load-bearing — not stylistic).
    //
    // The #ifdef is required, not defensive: OoT_PlaceForeignItems exists only in
    // the single-exe build, while Playthrough_Init is ordinary OoT code that also
    // compiles for a standalone SoH.
    //
    // Propagated through the return code rather than an exception. Nothing in
    // this chain catches — Rando_HeadlessSeedTest is extern "C" — so a throw here
    // would std::terminate the headless CI rows and the live generate alike.
    // A PARTIAL placement is not a failure and returns >= 0; only "the pairing is
    // on but could not be honoured at all" is negative (see foreign_items.h).
    const int foreignPlaced = OoT_PlaceForeignItems();
    if (foreignPlaced < 0) {
        SPDLOG_ERROR("Cross-game foreign placement failed ({}); aborting generation", foreignPlaced);
        return foreignPlaced;
    }
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