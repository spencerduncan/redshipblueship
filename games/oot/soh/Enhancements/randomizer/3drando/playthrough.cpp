#include "playthrough.hpp"

#include <atomic>
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
#include "triforce_hunt.h" // src/common — the frozen O10 triforce record (ADR 0010)
#include "combo_goal.h"    // src/common — Combo_GoalKeepsOwnHuntWin (#768)
#include "gen_budget.h" // src/common — the #582 progress surface (OoT's half)
#include "shared_resources.h" // src/common — Combo_FreezeFillWalletsAtCreation (#1284)
// ComboLogicEngineOoT.cpp: the paired world's general pass, deferred to the
// single-bag fill at the creation event (ADR 0010 increment 3, lane K11).
extern "C" void OoT_ComboLogic_SetGeneralPassDeferred(int deferred);
extern "C" int OoT_ComboLogic_GeneralPassDeferred(void);
// ForeignItemsSingleExe.cpp: the record of which file consumed the last staged
// generation (#564 V2), which a new generation replaces.
extern "C" void OoT_Creation_ForgetConsumedGeneration(void);

// #702: a record of what the LAST settings fold below actually folded, so a
// rando-tier row can assert that the per-area exclude-location groups reach the
// fingerprint at their shipped sizes (RandoSettingsFoldExcludes,
// games/oot/soh/oot_settings_fold_test.cpp). Written only by the fold, read only
// by that row. It counts option lines; it is not a copy of the string.
namespace {
constexpr int kRsbsFoldExcludeGroups = RSG_EXCLUDES_GANONS_CASTLE - RSG_EXCLUDES_KOKIRI_FOREST + 1;
uint32_t sRsbsFoldTotalLines = 0;
uint32_t sRsbsFoldExcludeLines[kRsbsFoldExcludeGroups] = {};
int sRsbsFoldRuns = 0;
// #1350: 1 while the last generation began with OoT's Logic authored as anything
// but Glitchless and generated it Glitchless under a proving combo rung (the
// derivation in Playthrough_Init, #1367).
// Written by the Generate worker, read on the game thread after the worker is
// joined (SeedPackageSingleExe.cpp) and by the rando-mask-quest-paired row.
std::atomic<int> sRsbsPairedLogicPinned{ 0 };
} // namespace

extern "C" int OoT_Rando_PairedLogicRulesPinned(void) {
    return sRsbsPairedLogicPinned.load();
}

// Returns how many folds have run in this process (0 = none yet) and fills the
// total option-line count plus, for up to `groupCap` groups in RSG_EXCLUDES_*
// order, the number of lines each exclude group contributed.
extern "C" int OoT_Rando_LastSettingsFold(uint32_t* outTotalLines, uint32_t* outExcludeLines, int groupCap) {
    if (outTotalLines != nullptr) {
        *outTotalLines = sRsbsFoldTotalLines;
    }
    for (int i = 0; outExcludeLines != nullptr && i < groupCap && i < kRsbsFoldExcludeGroups; i++) {
        outExcludeLines[i] = sRsbsFoldExcludeLines[i];
    }
    return sRsbsFoldRuns;
}

// #1389: OoT's share of the [Both Games] Triforce Goal is not bounded by the
// player's own Triforce Hunt Total Pieces, but SoH's Total Pieces callback
// (settings.cpp) shrinks the Required Pieces option's list to that own total,
// and Option::GetOptionText clamps an index past the list to its last entry. The
// settings string hashed below and the spoiler both print through it, so under
// the combo hunt the list goes back to the option's own 1..100 (NumOpts(1, 100)
// in settings.cpp): the text then names the derived requirement. Only ever
// widened: ChangeOptions writes the CVar when the index falls outside a shorter
// list. The Required Pieces callback calls this too, so a menu rebuild under the
// hunt goal keeps the list wide until the creation event's spoiler is written.
extern "C" void OoT_Rando_WidenTriforceRequiredOptions(void) {
    constexpr size_t kOptionCount = 100;
    Rando::Option& required = Rando::Settings::GetInstance()->GetOption(RSK_TRIFORCE_HUNT_PIECES_REQUIRED);
    if (required.GetOptionCount() >= kOptionCount) {
        return;
    }
    std::vector<std::string> counts;
    counts.reserve(kOptionCount);
    for (size_t n = 1; n <= kOptionCount; n++) {
        counts.push_back(std::to_string(n));
    }
    required.ChangeOptions(std::move(counts));
}
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
    // previous paired generation deferred to a creation that never happened,
    // and the record of which file consumed the previous one (#564 V2).
    OoT_ComboLogic_SetGeneralPassDeferred(0);
    OoT_Creation_ForgetConsumedGeneration();
#endif
    ctx->overrides.clear();
    ctx->ItemReset();
    ctx->HintReset();
    ctx->GetLogic()->Reset();
    StartPerformanceTimer(PT_REGION_RESET);
    Regions::AccessReset();
    StopPerformanceTimer(PT_REGION_RESET);

#ifdef RSBS_SINGLE_EXECUTABLE
    // #1367 (operator ruling 2026-11-02): a paired world's logic is ONE [Both Games]
    // choice, the combo rung (Randomizer > Cross-Game, OoTMM's `logic`), and OoT's
    // own Logic row only follows it. So OoT's Logic and All Locations Reachable are
    // DERIVED from the rung here, whatever their CVars hold:
    //   - a proving rung (Beatable Only, All Locations) generates OoT Glitchless
    //     with All Locations Reachable on. #1350: OoT's "No Logic" makes every OoT
    //     condition true (location_access.h's EventAccess::ConditionsMet and
    //     LocationAccess::GetConditionsMet, entrance.cpp's
    //     Entrance::GetConditionsMet answer true unless the rules are
    //     Glitchless), and the pair's proof runs over those same conditions (the
    //     OoT engine's expand is this port's ReachabilitySearch, its crossingOpen
    //     reads RR_MARKET_MASK_SHOP's child access and its goalReached the reached
    //     Triforce host, ComboLogicEngineOoT.cpp), so a proving rung over No Logic
    //     would stamp a world proved that nothing proved. All Locations Reachable
    //     stays on under both proving rungs: it is SoH's default, it is folded into
    //     the settings hash, and turning it off would re-seed every default world;
    //     in a paired world it only gates the OoT-only early stop in fill.cpp.
    //   - the base rung (No Logic) generates OoT No Logic, with All Locations
    //     Reachable off: no proof is claimed (the coordinator runs no round and
    //     Combo_Logic_UnprovedHalves names both halves), and OoT's restricted
    //     passes place without reachability (fill.cpp), as OoTMM's No Logic does.
    // Only the context's values move, never the CVars, so the settings hash, the
    // seed, the spoiler and the save (SaveManager writes the context's options) all
    // describe the world that is generated. Generation only: a file created before
    // this restores its own saved values (SaveManager's randoSettings) and plays as
    // it was made. Decided before FinalizeSettings, so nothing downstream reads an
    // authored value; it reads settings only and consumes no RNG. Every generation
    // in this build is a paired creation (the gate below refuses any other), as for
    // #973's block.
    //
    // The combo record is resolved once, here, for this block and #768's below:
    // the resolver reads only the gCombo keys, which FinalizeSettings never
    // writes, so both blocks see the record the creation freeze resolves.
    ComboSettingsRecord rsbsComboPreview;
    Combo_ResolveComboSettings(&rsbsComboPreview);
    sRsbsPairedLogicPinned = 0;
    {
        const bool rsbsProvingRung = Combo_LogicRungProves(rsbsComboPreview.logicRung);
        const uint8_t rsbsRules = rsbsProvingRung ? (uint8_t)RO_LOGIC_GLITCHLESS : (uint8_t)RO_LOGIC_NO_LOGIC;
        const uint8_t rsbsAlr = rsbsProvingRung ? (uint8_t)RO_GENERIC_ON : (uint8_t)RO_GENERIC_OFF;
        if (ctx->GetOption(RSK_LOGIC_RULES).IsNot(rsbsRules)) {
            SPDLOG_WARN("Paired world: OoT's Logic {} is generated as {} under combo rung {}: [Both Games] Logic "
                        "decides both games' logic (#1367)",
                        (unsigned)ctx->GetOption(RSK_LOGIC_RULES).Get(), (unsigned)rsbsRules,
                        (unsigned)rsbsComboPreview.logicRung);
            fprintf(stderr,
                    "[OoT] Logic: %u generated as %u under combo rung %u: OoT's own Logic row follows [Both Games] "
                    "Logic (#1367)\n",
                    (unsigned)ctx->GetOption(RSK_LOGIC_RULES).Get(), (unsigned)rsbsRules,
                    (unsigned)rsbsComboPreview.logicRung);
            // #1350's toast: an authored No Logic the proving rung overrode. The
            // base rung asks for No Logic itself, so nothing is said there.
            sRsbsPairedLogicPinned = rsbsProvingRung ? 1 : 0;
            ctx->GetOption(RSK_LOGIC_RULES).Set(rsbsRules);
        }
        if (ctx->GetOption(RSK_ALL_LOCATIONS_REACHABLE).IsNot(rsbsAlr)) {
            fprintf(stderr,
                    "[OoT] All Locations Reachable: %u generated as %u under combo rung %u: it follows [Both Games] "
                    "Logic (#1367)\n",
                    (unsigned)ctx->GetOption(RSK_ALL_LOCATIONS_REACHABLE).Get(), (unsigned)rsbsAlr,
                    (unsigned)rsbsComboPreview.logicRung);
            ctx->GetOption(RSK_ALL_LOCATIONS_REACHABLE).Set(rsbsAlr);
        }
    }
#endif
    ctx->FinalizeSettings(excludedLocations, enabledTricks);
#ifdef RSBS_SINGLE_EXECUTABLE
    // #768: in a paired world the frozen combo goal is the only win condition
    // (src/common/combo_goal.h). OoT's own triforce hunt in its "Win" mode is a
    // second one, and SoH generates it by moving OoT's win off Ganon
    // (item_pool.cpp puts RG_TRIFORCE at RC_TRIFORCE_COMPLETED and a blue rupee
    // at RC_GANON), so the coordinator would prove the hunt as OoT's half while
    // the paired game ends at Ganon: a world whose proof is not its ending.
    // Under a boss goal the hunt is therefore generated as "Ganon's Boss Key":
    // the same pieces, the same requirement, completing it opens Ganon's tower
    // door, and Ganon is OoT's win in the proof and in play. Decided here,
    // before the settings string is hashed, so the settings hash, the seed, the
    // spoiler and the save all describe the world that is generated. The goal is
    // read through the one resolver the creation freeze below uses (resolved
    // once above, for #1350's block too); nothing here consumes the RNG.
    {
        const ComboSettingsRecord& rsbsGoalPreview = rsbsComboPreview;
        if (!Combo_GoalKeepsOwnHuntWin(rsbsGoalPreview.goal) &&
            ctx->GetOption(RSK_TRIFORCE_HUNT).Is(RO_TRIFORCE_HUNT_WIN)) {
            ctx->GetOption(RSK_TRIFORCE_HUNT).Set(RO_TRIFORCE_HUNT_GBK);
            SPDLOG_WARN("Paired world: OoT's Triforce Hunt \"Win\" is generated as \"Ganon's Boss Key\" under combo "
                        "goal {}: the combo goal decides the end, and the hunt opens Ganon's tower door (#768)",
                        (unsigned)rsbsGoalPreview.goal);
            fprintf(stderr,
                    "[OoT] Triforce Hunt: \"Win\" generated as \"Ganon's Boss Key\" under combo goal %u (#768)\n",
                    (unsigned)rsbsGoalPreview.goal);
        }
        // #997, the reverse: under the combo triforce hunt the give that reaches
        // the COMBO requirement grants Ganon's Boss Key and ends the paired game
        // (it takes the Win branch whichever mode is named here), so "Ganon's
        // Boss Key" would put the boss key at RC_TRIFORCE_COMPLETED behind OoT's
        // OWN requirement, crediting Ganondorf's Lair to the proof long before
        // play opens it. Generated as "Win": the same pieces and requirement (the
        // creation freeze below reads them unchanged), RG_TRIFORCE at
        // RC_TRIFORCE_COMPLETED, and the lair is post-game, as it is in play.
        if (Combo_GoalGeneratesOwnHuntAsWin(rsbsGoalPreview.goal) &&
            ctx->GetOption(RSK_TRIFORCE_HUNT).Is(RO_TRIFORCE_HUNT_GBK)) {
            ctx->GetOption(RSK_TRIFORCE_HUNT).Set(RO_TRIFORCE_HUNT_WIN);
            SPDLOG_WARN("Paired world: OoT's Triforce Hunt \"Ganon's Boss Key\" is generated as \"Win\" under the "
                        "combo triforce hunt: the combo requirement ends the game and opens Ganon's tower door in "
                        "the same give (#997)");
            fprintf(stderr, "[OoT] Triforce Hunt: \"Ganon's Boss Key\" generated as \"Win\" under the combo "
                            "triforce hunt (#997)\n");
        }
        // #1389: under the combo triforce hunt OoT's own hunt is OoT's half of the
        // [Both Games] Triforce Pieces / Triforce Goal split (Combo_TriforceSplit,
        // OoTMM's triforcePieces / triforceGoal), whatever OoT's own rows hold:
        // generated as "Win" (#997's reason) with OoT's share of the pieces and of
        // the goal. MM's paired resolution takes MM's half of the same split, and
        // the creation freeze below checks both against it. Settings only, no RNG.
        if (rsbsGoalPreview.goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) {
            uint16_t rsbsPieces = 0;
            uint16_t rsbsGoalCount = 0;
            Combo_ResolveComboTriforce(&rsbsPieces, &rsbsGoalCount);
            ComboTriforceHalf rsbsOoTHalf;
            ComboTriforceHalf rsbsMMHalf;
            if (Combo_TriforceSplit(rsbsPieces, rsbsGoalCount, &rsbsOoTHalf, &rsbsMMHalf) == RSBS_TRIFORCE_OK) {
                // The split gives OoT 1..100 pieces and 1..its pieces required, the
                // options' own 1..100 ranges (each stores its value - 1). The
                // Required list is widened back to that range first, so the
                // settings string and the spoiler print OoT's share unclamped.
                OoT_Rando_WidenTriforceRequiredOptions();
                ctx->GetOption(RSK_TRIFORCE_HUNT).Set(RO_TRIFORCE_HUNT_WIN);
                ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_TOTAL).Set((uint8_t)(rsbsOoTHalf.total - 1));
                ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_REQUIRED).Set((uint8_t)(rsbsOoTHalf.required - 1));
                fprintf(stderr,
                        "[OoT] Triforce Hunt: generated as OoT's half of [Both Games] %u of %u pieces: %u of %u "
                        "(MM's half %u of %u) (#1389)\n",
                        (unsigned)rsbsGoalCount, (unsigned)rsbsPieces, (unsigned)rsbsOoTHalf.required,
                        (unsigned)rsbsOoTHalf.total, (unsigned)rsbsMMHalf.required, (unsigned)rsbsMMHalf.total);
            }
        }
    }
    // #973: the Happy Mask Shop door IS the OoT<->MM crossing (entrance.cpp pins it
    // out of every shuffle pool), so the shop's room never loads, and two settings
    // assume it does. Mask Quest "Vanilla" credits borrowing the masks there (the
    // LOGIC_CAN_BORROW_MASKS chain starts in RR_MARKET_MASK_SHOP), and "Completed"
    // credits them once the shop opens, which in play needs the shop or the
    // MaskSelect enhancement no seed can see; either way the fill could put
    // progression on the Deku Theater checks with no mask to show. The Mask Shop
    // Hint is the sign in the shop's own room (text 0x0207, an En_Wonder_Talk2 in
    // face_shop_room_0), which nobody can read. OoTMM's Mask Shop is only the
    // crossing and OoTMM shuffles all eight OoT masks, so a paired world is
    // generated as "Shuffle" with the sign hint off, here for #768's reason: before
    // the settings string is hashed. Unconditional, like the door's pin: every
    // generation in this build is a paired creation (the gate below refuses any
    // other).
    if (ctx->GetOption(RSK_MASK_QUEST).IsNot(RO_MASK_QUEST_SHUFFLE)) {
        SPDLOG_WARN("Paired world: Mask Quest {} is generated as \"Shuffle\": the Happy Mask Shop door leads to "
                    "Termina, so the shop's masks are in the item pool (#973)",
                    (unsigned)ctx->GetOption(RSK_MASK_QUEST).Get());
        fprintf(stderr, "[OoT] Mask Quest: %u generated as \"Shuffle\" (#973)\n",
                (unsigned)ctx->GetOption(RSK_MASK_QUEST).Get());
        ctx->GetOption(RSK_MASK_QUEST).Set(RO_MASK_QUEST_SHUFFLE);
    }
    if (ctx->GetOption(RSK_MASK_SHOP_HINT)) {
        SPDLOG_WARN("Paired world: the Mask Shop Hint is generated off: its sign is inside the Happy Mask Shop (#973)");
        fprintf(stderr, "[OoT] Mask Shop Hint: generated off (#973)\n");
        ctx->GetOption(RSK_MASK_SHOP_HINT).Set(RO_GENERIC_OFF);
    }
    // #1369 (operator ruling 2026-11-02): ONE Shuffle Swim for the pair under the
    // Shared Swim rule, and ONE Shuffle Ocarina Buttons under Shared Ocarina
    // Buttons (a deliberate divergence from OoTMM). When the rule's key is on and
    // MM shuffles the item by its own options while OoT does not, OoT's copy is
    // generated shuffled too, here for #768's reason: before the settings string is
    // hashed, so every OoT reader of the option (the pool, the start, the logic,
    // the gates, both trackers, the spoiler) agrees and the saved seed carries it.
    // MM takes the pair's shuffle from the frozen record instead
    // (Rando::Foreign::ShuffleSwimForWorld), so its profile identity stays its own.
    {
        const uint32_t rsbsCoupled = Combo_ComboCouplingAuthored();
        const uint32_t rsbsMmOwn = MM_Rando_ResolveOwnShuffleCaps(/*fromSave=*/0);
        if ((rsbsCoupled & rsbsMmOwn & RSBS_GIVECAP_SWIM) != 0u &&
            ctx->GetOption(RSK_SHUFFLE_SWIM).Is(RO_GENERIC_OFF)) {
            SPDLOG_WARN("Paired world: OoT's Shuffle Swim is generated on: Majora's Mask shuffles swim and the Shared "
                        "Swim rule makes one Shuffle Swim for the pair (#1369)");
            fprintf(stderr, "[OoT] Shuffle Swim: generated on, for the pair's Shared Swim (#1369)\n");
            ctx->GetOption(RSK_SHUFFLE_SWIM).Set(RO_GENERIC_ON);
        }
        if ((rsbsCoupled & rsbsMmOwn & RSBS_GIVECAP_OCARINA_BUTTONS) != 0u &&
            ctx->GetOption(RSK_SHUFFLE_OCARINA_BUTTONS).Is(RO_GENERIC_OFF)) {
            SPDLOG_WARN("Paired world: OoT's Shuffle Ocarina Buttons is generated on: Majora's Mask shuffles its "
                        "buttons and the Shared Ocarina Buttons rule makes one shuffle for the pair (#1369)");
            fprintf(stderr, "[OoT] Shuffle Ocarina Buttons: generated on, for the pair's Shared Ocarina Buttons "
                            "(#1369)\n");
            ctx->GetOption(RSK_SHUFFLE_OCARINA_BUTTONS).Set(RO_GENERIC_ON);
        }
    }
#endif
    // once the settings have been finalized turn them into a string for hashing
    std::string settingsStr;
    auto& optionGroups = Rando::Settings::GetInstance()->GetOptionGroups();
#ifdef RSBS_SINGLE_EXECUTABLE
    sRsbsFoldTotalLines = 0;
    for (uint32_t& lines : sRsbsFoldExcludeLines) {
        lines = 0;
    }
#endif
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
                        if (trickOption->GetKey() == RT_HOOKSHOT_LADDERS &&
                            ctx->GetTrickOption(RT_HOOKSHOT_LADDERS).Get() == 0) {
                            continue;
                        }
                        settingsStr += option->GetOptionText(ctx->GetTrickOption(trickOption->GetKey()).Get());
                    } else {
                        settingsStr += option->GetOptionText(ctx->GetOption(option->GetKey()).Get());
                    }
#ifdef RSBS_SINGLE_EXECUTABLE
                    sRsbsFoldTotalLines++;
                    if (i >= RSG_EXCLUDES_KOKIRI_FOREST && i <= RSG_EXCLUDES_GANONS_CASTLE) {
                        sRsbsFoldExcludeLines[i - RSG_EXCLUDES_KOKIRI_FOREST]++;
                    }
#endif
                }
            }
        }
    }
#ifdef RSBS_SINGLE_EXECUTABLE
    sRsbsFoldRuns++;
#endif

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
    const uint8_t rsbsPriorMmKeepItemsReset = gComboCtx.mmKeepItemsReset;
    // The combo record, its fingerprint, the triforce record and the item rules
    // (#976): one snapshot, so a field the freeze gains is rolled back with it.
    ComboCreationRecords rsbsPriorCreationRecords;
    Combo_SnapshotCreationRecords(&rsbsPriorCreationRecords);
    const uint32_t rsbsPriorGiveCaps = Combo_ForeignGiveCaps((uint8_t)GAME_MM);
    const bool rsbsPriorGiveCapsPublished = Combo_ForeignGiveCapsPublished((uint8_t)GAME_MM);
    auto rsbsRollbackFreeze = [&]() {
        gComboCtx.sourceIsRando = rsbsPriorSourceIsRando;
        gComboCtx.sharedRandoSeed = rsbsPriorSeed;
        gComboCtx.sharedRandoSettingsHash = rsbsPriorSettingsHash;
        gComboCtx.mmProfileDigest = rsbsPriorMmProfileDigest;
        gComboCtx.mmKeepItemsReset = rsbsPriorMmKeepItemsReset;
        Combo_RestoreCreationRecords(&rsbsPriorCreationRecords);
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
    // #1175: MM's Keep Items on Cycle Reset freezes in the same act from the same
    // key the stamp above folds (when on), so the byte play reads and the identity
    // the arrival compares cannot disagree.
    MM_Rando_FreezeKeepItemsResetAtCreation();
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
        // #976: the cross-game item rules freeze in the same act, so the
        // fingerprint folds them (every rule off reproduces today's hash).
        ComboItemRulesRecord comboItemRules;
        Combo_ResolveComboItemRules(&comboItemRules);
        // #1369: the shuffle-gated rules decide from the two games' own shuffles
        // (OoT's after the paired shaping above, MM's own options): Shared Swim
        // freezes only where swim is shuffled, and the coupled-buttons marker only
        // where the Shared Ocarina Buttons rule meets a button shuffle. A world
        // nobody shuffles either in freezes exactly what it did before.
        {
            uint32_t rsbsShuffled = MM_Rando_ResolveOwnShuffleCaps(/*fromSave=*/0);
            if (ctx->GetOption(RSK_SHUFFLE_SWIM).IsNot(RO_GENERIC_OFF)) {
                rsbsShuffled |= RSBS_GIVECAP_SWIM;
            }
            if (ctx->GetOption(RSK_SHUFFLE_OCARINA_BUTTONS).IsNot(RO_GENERIC_OFF)) {
                rsbsShuffled |= RSBS_GIVECAP_OCARINA_BUTTONS;
            }
            Combo_ComboItemRulesApplyCreationShuffles(&comboSettings, &comboItemRules, rsbsShuffled);
        }
        Combo_FreezeComboSettingsEx(&comboSettings, &comboItemRules);
        // #1369: publish MM's give capabilities again, now that the record they
        // read the pair's one shuffle from is this world's (the publish above may
        // have read the last file's record).
        MM_Rando_PublishProfileGiveCaps(/*fromSave=*/0);
        // #1284: the pair's ONE fill-wallets rule (OoTMM's fillWallets), from
        // OoT's Full Wallets, frozen in the same act so MM's wallet pickup step
        // reads the pair's answer, never OoT's seed settings. A creation record:
        // the rollback below restores it with the others.
        Combo_FreezeFillWalletsAtCreation(ctx->GetOption(RSK_FULL_WALLETS).IsNot(RO_GENERIC_OFF));
        SPDLOG_INFO("Paired identity: combo settings frozen at creation (fingerprint {:08X})",
                    gComboCtx.comboSettingsHash);
    }

    // ADR 0010 answer O10: under the combo goal triforce-hunt, the ONE piece
    // count's requirement and the split of pieces between the two pools freeze
    // HERE (the rule is at ComboTriforceRecord, context.h). Since #1389 the two
    // halves are what each game GENERATES: OoT's from its context options (the
    // #1389 block above wrote them), MM's from its paired resolution, both the
    // split of the [Both Games] Triforce Pieces / Triforce Goal rows, which the
    // freeze then checks they are. Every other goal — the shipped default among
    // them — stores four zero bytes and this block changes nothing. Reads
    // settings only, so it consumes no RNG and moves no world. A hunt the rule
    // cannot describe, or halves that are not the rows' split, is refused like a
    // failed fill: rolled back, nothing generated.
    {
        ComboTriforceHalf ootHalf = { 0, 0 };
        ComboTriforceHalf mmHalf = { 0, 0 };
        bool rsbsHalvesAreTheSplit = true;
        // The halves are only resolved for a hunt: every other world reads
        // nothing here, not even MM's CVars.
        if (gComboCtx.comboSettings.goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) {
            if (ctx->GetOption(RSK_TRIFORCE_HUNT).IsNot(RO_TRIFORCE_HUNT_OFF)) {
                ootHalf.total = (uint16_t)(ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_TOTAL).Get() + 1);
                ootHalf.required = (uint16_t)(ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_REQUIRED).Get() + 1);
            }
            MM_Rando_ResolveTriforceHalf(/*fromSave=*/0, &mmHalf.total, &mmHalf.required);
            // #1389: the record is the [Both Games] rows' split, and each game must
            // be generating exactly its half of it, or the world proved would not
            // be the hunt the rows describe.
            uint16_t rsbsPieces = 0;
            uint16_t rsbsGoalCount = 0;
            Combo_ResolveComboTriforce(&rsbsPieces, &rsbsGoalCount);
            ComboTriforceHalf splitOoT;
            ComboTriforceHalf splitMM;
            rsbsHalvesAreTheSplit = Combo_TriforceSplit(rsbsPieces, rsbsGoalCount, &splitOoT, &splitMM) ==
                                        RSBS_TRIFORCE_OK &&
                                    ootHalf.total == splitOoT.total && ootHalf.required == splitOoT.required &&
                                    mmHalf.total == splitMM.total && mmHalf.required == splitMM.required;
            if (!rsbsHalvesAreTheSplit) {
                SPDLOG_ERROR("Paired identity: the two games generate Triforce Hunt halves (OoT {} of {}, MM {} of {}) "
                             "that are not the split of [Both Games] {} of {} (OoT {} of {}, MM {} of {}); aborting "
                             "generation (#1389)",
                             ootHalf.required, ootHalf.total, mmHalf.required, mmHalf.total, rsbsGoalCount, rsbsPieces,
                             splitOoT.required, splitOoT.total, splitMM.required, splitMM.total);
                fprintf(stderr,
                        "[OoT] generation REFUSED: the Triforce Hunt halves the two games generate are not the "
                        "[Both Games] split (#1389)\n");
            }
        }
        const int triforceStatus =
            rsbsHalvesAreTheSplit ? Combo_TriforceFreezeAtCreation(&ootHalf, &mmHalf) : RSBS_TRIFORCE_ERR_BAD_HALF;
        if (triforceStatus != RSBS_TRIFORCE_OK) {
            SPDLOG_ERROR("Paired identity: the combo goal is triforce-hunt but the two halves' piece settings cannot "
                         "describe one ({}: OoT {} of {}, MM {} of {}); aborting generation (ADR 0010 O10)",
                         Combo_TriforceStatusName(triforceStatus), ootHalf.required, ootHalf.total, mmHalf.required,
                         mmHalf.total);
            rsbsRollbackFreeze();
            Combo_GenProgress_End(false);
            return -1;
        }
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

    // A paired triforce-hunt world is generated like any other since #1389 (the
    // refusal ADR 0010 increment 3 left here is gone): both games' pieces are
    // single-bag rows the proof counts (each game's WORLD_EVENT family composes
    // HOME_ONLY, combo_single_bag.c), and the fill proves the frozen combo
    // requirement against the two engines' summed `triforcePieces`.
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