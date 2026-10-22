/**
 * @file oot_triforce_hunt_test.cpp
 * OoT's half of the combo triforce hunt locks (ADR 0010 answer O10; the rule and
 * the contract are in src/common/triforce_hunt.h).
 *
 * Two surfaces live here because both need OoT's `gSaveContext` in scope:
 *
 *   1. THE SHIM HELPERS the redship row `combo-triforce-hunt`
 *      (src/common/tests/test_triforce_hunt.c) drives OoT's REAL
 *      OoT_HarvestSharedResources / OoT_ApplySharedResources through, for the
 *      cross-game sum. They only put OoT's live save in a known state and read
 *      the counter; every merge decision is the production shims'.
 *
 *   2. THE WIN ARM, `OoT_TriforceHuntWin_RunGenerated`, run by the rando row
 *      `rando-triforce-hunt-win` because OoT's give path reads the seed's
 *      settings through the randomizer context, which only a generation
 *      populates. It drives the REAL `Randomizer_Item_Give(NULL, RG_TRIFORCE_PIECE)`
 *      (the NULL-play give ForeignItemsSingleExe.cpp's redemption already uses)
 *      with OoT's own hunt in BOTH modes, unpaired and under every combo goal
 *      value, and reads what the arm leaves behind:
 *        - W0 (#768): the generation itself is a paired creation under the
 *          default goal (beat-both) with OoT's hunt authored as "Win"; it must
 *          come out as "Ganon's Boss Key", with RG_TRIFORCE at RC_GANON, and
 *          the option's description (its menu tooltip) must state the change.
 *        - W1 UNPAIRED: OoT's own `==` at OoT's own requirement grants Ganon's
 *          Boss Key, and ends the game exactly when OoT's own mode is "Win".
 *        - W2 A BOSS GOAL (1, 2, 4, 5; #768): the own requirement grants the
 *          Boss Key and ends nothing, in either mode.
 *        - W3/W4 TRIFORCE-HUNT (3): the own requirement grants nothing (it is
 *          an input); the COMBO requirement takes the Win branch, in either
 *          mode, because under the combo goal reaching it IS the win.
 *
 *
 * And, run by its own rando row `rando-combo-hunt-own-mode` (#997),
 * `OoT_ComboHuntOwnMode_RunGenerated`: a paired creation under the combo
 * triforce-hunt goal with OoT's own hunt authored as "Ganon's Boss Key" must
 * generate it as "Win", so RC_TRIFORCE_COMPLETED holds RG_TRIFORCE and never
 * Ganon's Boss Key: the boss key is granted only by the give that reaches the
 * COMBO requirement, which ends the paired game, so no location may be proved
 * behind it at OoT's own requirement. An authored "Win" stays "Win" (control).
 *
 * COUNTERFACTUALS, run before landing (#768): main's arm (the "Win" test ungated
 * by the goal) turns W2 red under "Win"; main's playthrough.cpp (no "Win"
 * change at a paired creation) turns W0 red, and so does main's
 * option_descriptions.cpp (no paired-world note). (#740 ran the others: upstream's
 * `==` in place of the Combo_TriforceHuntOnPieceGiven call, and the armed hunt
 * not forcing the Win branch, each turned its triforce-hunt leg red.)
 */

#include <cstdio>
#include <cstring>
#include <string>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"

#include "soh/Enhancements/randomizer/3drando/item_pool.hpp" // GenerateItemPool (#997)
#include "soh/Enhancements/randomizer/location_access.h"     // RegionTable_Init (#997)

#include "context.h"
#include "cvar_shared_keys.h" // RSBS_CVAR_COMBO_RANDO_GOAL (#997)
#include "foreign_items.h"    // Combo_FreezeComboSettings, RSBS_COMBO_GOAL_TRIFORCE_HUNT
#include "triforce_hunt.h"

extern "C" {
#include <z64.h>
#include "variables.h"
#include "functions.h" // Flags_GetRandomizerInf / Flags_UnsetRandomizerInf
u16 Randomizer_Item_Give(PlayState* play, GetItemEntry giEntry);
}

// games/oot/soh/Enhancements/randomizer/3drando/menu.cpp — the headless
// generation bridge the RandoGen rows use.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);

// ============================================================================
// (1) The shim helpers
// ============================================================================

/** OoT's live save as a real loaded file: GAMEMODE_NORMAL is what
 *  Combo_SaveIsLiveFile requires before OoT_HarvestSharedResources does
 *  anything. Everything else zero, so the harvest offers no other kind a nonzero
 *  value and the pool holds only what the row puts there. */
extern "C" void OoT_TriforceHuntTest_ArmLive(void) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
}

extern "C" void OoT_TriforceHuntTest_SetCount(uint16_t count) {
    gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected = (u8)count;
}

extern "C" int OoT_TriforceHuntTest_Count(void) {
    return (int)gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;
}

// ============================================================================
// (2) The win arm
// ============================================================================

namespace {

#define OTH_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// OoT's own hunt in this row: 5 pieces, 3 required (option indices are value-1).
const uint8_t kOotTotal = 5;
const uint8_t kOotRequired = 3;
// MM's half of the combo: 4 pieces, 2 required. Combo requirement 3 + 2 = 5.
const uint8_t kMmTotal = 4;
const uint8_t kMmRequired = 2;

int FreezeWorld(uint8_t goal) {
    ComboContext_Init();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0010F00Du;
    gComboCtx.sharedRandoSettingsHash = 0x0010CA5Eu;
    gComboCtx.mmProfileDigest = 0x0010D16Eu;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    rec.goal = goal;
    Combo_FreezeComboSettings(&rec);
    const ComboTriforceHalf oot = { kOotTotal, kOotRequired };
    const ComboTriforceHalf mm = { kMmTotal, kMmRequired };
    return Combo_TriforceFreezeAtCreation(&oot, &mm);
}

/** Counter at `count`, the Boss Key grant, the completion flag and the credits
 *  warp all clear. */
void ResetHuntState(uint8_t count) {
    gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected = count;
    Flags_UnsetRandomizerInf(RAND_INF_GRANT_GANONS_BOSSKEY);
    gSaveContext.ship.stats.gameComplete = 0;
    GameInteractor::State::TriforceHuntCreditsWarpActive = 0;
    GameInteractor::State::TriforceHuntPieceGiven = 0;
}

bool BossKeyGranted() {
    return Flags_GetRandomizerInf(RAND_INF_GRANT_GANONS_BOSSKEY) != 0;
}

bool GameEnded() {
    return gSaveContext.ship.stats.gameComplete != 0 && GameInteractor::State::TriforceHuntCreditsWarpActive != 0;
}

} // namespace

extern "C" int OoT_TriforceHuntWin_RunGenerated(void) {
    printf("[TEST] rando-triforce-hunt-win (OoT): OoT's real piece give ends a PAIRED hunt at the combo requirement "
           "only, and OoT's own hunt never under a boss goal (ADR 0010 O10, #768)\n");

    // ---- W0: the creation resolves OoT's own "Win" under a boss goal (#768) --
    // The generation below is a paired creation under the default combo goal
    // (beat-both) with OoT's own hunt authored as "Win". SoH would move OoT's win
    // off Ganon (RG_TRIFORCE at RC_TRIFORCE_COMPLETED, a blue rupee at RC_GANON)
    // while the paired game ends at Ganon; the creation must generate the hunt
    // as "Ganon's Boss Key" instead, so Ganon is OoT's win in the proof too.
    const char* const huntCVar = CVAR_RANDOMIZER_SETTING("TriforceHunt");
    CVarSetInteger(huntCVar, RO_TRIFORCE_HUNT_WIN);
    const int genRc = Rando_HeadlessSeedTest("RSBSTRIFORCEO10");
    CVarClear(huntCVar);
    OTH_ASSERT(genRc == 0, "headless OoT seed generation failed");
    auto ctx = Rando::Context::GetInstance();
    OTH_ASSERT(ctx != nullptr && OTRGlobals::Instance != nullptr && OTRGlobals::Instance->gRandomizer != nullptr,
               "no randomizer context after generation");
    OTH_ASSERT(Combo_ComboSettingsFrozen() && gComboCtx.comboSettings.goal == (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH,
               "W0: the headless creation did not freeze the default combo goal (beat-both)");
    {
        const int mode = (int)ctx->GetOption(RSK_TRIFORCE_HUNT).Get();
        const RandomizerGet atGanon = ctx->GetItemLocation(RC_GANON)->GetPlacedRandomizerGet();
        const RandomizerGet atHunt = ctx->GetItemLocation(RC_TRIFORCE_COMPLETED)->GetPlacedRandomizerGet();
        printf(
            "[TEST] W0: authored Win under beat-both -> mode %d, RC_GANON holds %d, RC_TRIFORCE_COMPLETED holds %d\n",
            mode, (int)atGanon, (int)atHunt);
        OTH_ASSERT(
            mode == RO_TRIFORCE_HUNT_GBK,
            "W0: a paired creation under a boss goal kept OoT's own triforce hunt as \"Win\" - the hunt would be "
            "OoT's proved goal while the paired game ends at Ganon (#768)");
        OTH_ASSERT(atGanon == RG_TRIFORCE && atHunt == RG_GANONS_CASTLE_BOSS_KEY,
                   "W0: the generated world does not put OoT's win at Ganon and Ganon's Boss Key at the hunt's "
                   "completion");
        // The change is the player's to see, not only the log's: the option's
        // description (the tooltip OoT's randomizer menu renders for it) states
        // it where "Win" is chosen.
        const std::string& huntDesc = Rando::Settings::GetInstance()->GetOption(RSK_TRIFORCE_HUNT).GetDescription();
        const bool descStates =
            huntDesc.find("Under a boss goal, \"Win\" is generated as \"Ganon's Boss Key\"") != std::string::npos;
        printf("[TEST] W0: the Triforce Hunt option description %s the paired-world change\n",
               descStates ? "states" : "does NOT state");
        OTH_ASSERT(descStates, "W0: the Triforce Hunt option description does not tell the player that a paired "
                               "creation under a boss goal generates \"Win\" as \"Ganon's Boss Key\" (#768)");
    }

    // OoT's own hunt, 5 pieces, 3 required. The give reads these back through
    // the context; each leg below names the mode it drives.
    ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_TOTAL).Set((uint8_t)(kOotTotal - 1));
    ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_REQUIRED).Set((uint8_t)(kOotRequired - 1));
    // Randomizer::GetRandoSettingValue, which the give reads, is exactly this.
    OTH_ASSERT(ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_REQUIRED).Get() + 1 == kOotRequired,
               "the hunt settings did not take in the seed context");
    // The Win branch emits a toast; a headless process has no audio session to
    // play its chime into.
    CVarSetInteger(CVAR_SETTING("Notifications.Mute"), 1);

    const GetItemEntry piece = Rando::StaticData::RetrieveItem(RG_TRIFORCE_PIECE).GetGIEntry_Copy();
    const uint8_t kModes[2] = { RO_TRIFORCE_HUNT_WIN, RO_TRIFORCE_HUNT_GBK };
    const uint8_t kGoals[5] = {
        (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH,     (uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER,
        (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT, (uint8_t)RSBS_COMBO_GOAL_BEAT_OOT,
        (uint8_t)RSBS_COMBO_GOAL_BEAT_MM,
    };

    for (uint8_t mode : kModes) {
        ctx->GetOption(RSK_TRIFORCE_HUNT).Set(mode);
        OTH_ASSERT(ctx->GetOption(RSK_TRIFORCE_HUNT).Get() == mode, "the hunt mode did not take in the seed context");
        const char* modeName = mode == RO_TRIFORCE_HUNT_WIN ? "Win" : "Ganon's Boss Key";
        const bool ownModeWins = mode == RO_TRIFORCE_HUNT_WIN;

        // ---- W1: UNPAIRED, OoT's own hunt, exactly as upstream ----------------
        // No frozen combo record: the arm is upstream's. Without this leg the
        // paired legs' silence could be a dead arm.
        ComboContext_Init();
        OTH_ASSERT(!Combo_ComboSettingsFrozen(), "an initialized context must hold no frozen combo record");
        ResetHuntState((uint8_t)(kOotRequired - 1));
        Randomizer_Item_Give(NULL, piece);
        OTH_ASSERT(gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected == kOotRequired,
                   "the piece give did not count the piece");
        if (!BossKeyGranted() || GameEnded() != ownModeWins) {
            printf("[TEST] FAIL: W1 unpaired, mode %s: Boss Key %d, game ended %d (expected 1, %d) (%s:%d)\n", modeName,
                   BossKeyGranted() ? 1 : 0, GameEnded() ? 1 : 0, ownModeWins ? 1 : 0, __FILE__, __LINE__);
            return 1;
        }

        for (uint8_t goal : kGoals) {
            OTH_ASSERT(FreezeWorld(goal) == RSBS_TRIFORCE_OK, "a paired creation over two coherent halves must freeze");
            if (goal != (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) {
                // ---- W2: a BOSS goal: OoT's own requirement is the hunt, never
                // the win (#768). Ganon's Boss Key is granted (the hunt's lock on
                // Ganon); the game is not complete, not saved-and-credited.
                OTH_ASSERT(!Combo_TriforceHuntArmed(), "a boss-goal world must not arm the combo hunt");
                ResetHuntState((uint8_t)(kOotRequired - 1));
                Randomizer_Item_Give(NULL, piece);
                OTH_ASSERT(gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected == kOotRequired,
                           "the piece was not counted");
                if (!BossKeyGranted() || GameEnded() || gSaveContext.ship.stats.gameComplete != 0) {
                    printf("[TEST] FAIL: W2 goal %u, mode %s: OoT's OWN triforce hunt %s in a paired world whose "
                           "combo goal is a boss goal (Boss Key %d, gameComplete %d, credits warp %d) - the frozen "
                           "combo goal is the only win condition; the hunt only unlocks Ganon (#768) (%s:%d)\n",
                           (unsigned)goal, modeName, BossKeyGranted() ? "ended the game" : "did not grant the Boss Key",
                           BossKeyGranted() ? 1 : 0, (int)gSaveContext.ship.stats.gameComplete,
                           (int)GameInteractor::State::TriforceHuntCreditsWarpActive, __FILE__, __LINE__);
                    return 1;
                }
                continue;
            }

            // ---- W3: triforce-hunt, the own requirement is NOT the threshold ----
            OTH_ASSERT(Combo_TriforceHuntArmed(), "a frozen triforce-hunt world must arm the combo hunt");
            const uint16_t comboRequired = Combo_TriforceHuntRequired();
            OTH_ASSERT(comboRequired == (uint16_t)(kOotRequired + kMmRequired),
                       "the combo requirement is not the sum of the two halves' requirements");
            ResetHuntState((uint8_t)(kOotRequired - 1));
            Randomizer_Item_Give(NULL, piece);
            OTH_ASSERT(gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected == kOotRequired,
                       "the piece was not counted");
            OTH_ASSERT(
                !BossKeyGranted() && !GameEnded(),
                "ARMED: OoT's OWN requirement fired OoT's hunt in a paired world - under the combo goal it is an "
                "input to the combo requirement, not the threshold (ADR 0010 O10)");

            // ---- W4: triforce-hunt, the combo requirement IS the win ------------
            ResetHuntState((uint8_t)(comboRequired - 1));
            Randomizer_Item_Give(NULL, piece);
            OTH_ASSERT(gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected == comboRequired,
                       "the piece was not counted");
            if (!GameEnded()) {
                printf("[TEST] FAIL: W4 mode %s: ARMED: reaching the COMBO requirement in OoT did not take the Win "
                       "branch (game complete and the credits warp) - under the combo goal reaching it is the win, "
                       "whichever mode OoT's own setting named (%s:%d)\n",
                       modeName, __FILE__, __LINE__);
                return 1;
            }
            // Equality, as OoT tests it: one more piece fires nothing new.
            ResetHuntState(comboRequired);
            Randomizer_Item_Give(NULL, piece);
            OTH_ASSERT(!BossKeyGranted() && !GameEnded(),
                       "ARMED: a piece past the combo requirement fired the win again");
        }
        printf("[TEST] OoT mode %s: unpaired as upstream; under goals 1, 2, 4, 5 the own hunt grants the Boss Key and "
               "ends nothing; under goal 3 only the combo requirement ends the game\n",
               modeName);
    }

    ComboContext_Init();
    ResetHuntState(0);
    printf("[TEST] PASS: rando-triforce-hunt-win (OoT)\n");
    return 0;
}

// ============================================================================
// (3) OoT's own hunt mode under the combo hunt (#997)
// ============================================================================

namespace {

/** One paired creation through the headless bridge with the combo goal and
 *  OoT's own hunt mode authored. Both keys are cleared afterwards, so nothing
 *  authored here outlives the call (and nothing is saved to a config file).
 *  Returns the bridge's rc; the effective mode stays in the seed context. */
int GenerateAuthored(uint8_t goal, uint8_t ownMode, const char* seed) {
    const char* const huntCVar = CVAR_RANDOMIZER_SETTING("TriforceHunt");
    CVarSetInteger(RSBS_CVAR_COMBO_RANDO_GOAL, (int32_t)goal);
    CVarSetInteger(huntCVar, ownMode);
    ComboSettingsRecord preview;
    Combo_ResolveComboSettings(&preview);
    const bool goalTook = preview.goal == goal;
    const int rc = Rando_HeadlessSeedTest(seed);
    CVarClear(huntCVar);
    CVarClear(RSBS_CVAR_COMBO_RANDO_GOAL);
    return goalTook ? rc : -100;
}

} // namespace

extern "C" int OoT_ComboHuntOwnMode_RunGenerated(void) {
    printf("[TEST] rando-combo-hunt-own-mode: under the combo triforce hunt, OoT's own hunt is generated as \"Win\", "
           "so no location is proved behind Ganon's Boss Key at OoT's own requirement (#997)\n");
    const uint8_t hunt = (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT;

    // ---- H1: authored "Ganon's Boss Key" under the combo hunt -> "Win" ------
    // The generation itself may be refused later in Playthrough_Init (a paired
    // triforce-hunt world is not carried by the single-bag fill yet); the mode
    // is decided right after the settings are finalized, before any refusal,
    // so the context holds the effective mode either way.
    const int rc1 = GenerateAuthored(hunt, RO_TRIFORCE_HUNT_GBK, "RSBSHUNTGBK997");
    OTH_ASSERT(rc1 != -100, "H1: the authored combo goal did not reach the creation's resolver");
    auto ctx = Rando::Context::GetInstance();
    OTH_ASSERT(ctx != nullptr, "no randomizer context after generation");
    const int mode1 = (int)ctx->GetOption(RSK_TRIFORCE_HUNT).Get();
    printf("[TEST] H1: combo goal triforce-hunt, OoT hunt authored \"Ganon's Boss Key\" -> mode %d (bridge rc %d)\n",
           mode1, rc1);
    OTH_ASSERT(mode1 == RO_TRIFORCE_HUNT_WIN,
               "H1: under the combo triforce hunt OoT's own hunt stayed \"Ganon's Boss Key\": the fill would put "
               "Ganon's Boss Key at RC_TRIFORCE_COMPLETED behind OoT's OWN requirement while the game grants it only "
               "at the COMBO requirement, as the paired game ends (#997)");
    // What the effective mode places: the fixed placements of OoT's item pool.
    // The refused creation never reached Fill(), so no location pool exists
    // yet: build it first, as VanillaFill and Fill do (fill.cpp), or the item
    // pool's junk count (locCount - itemPool.size(), a size_t) wraps on an
    // empty pool and the junk loop never ends. RegionTable_Init comes first, as
    // it does there: it binds the world graph's `ctx` (location_access.cpp),
    // which GenerateLocationPool reads, and nothing before Fill() binds it.
    RegionTable_Init();
    ctx->GenerateLocationPool();
    ctx->ItemReset();
    OTH_ASSERT(ctx->CountEmptyLocations(false) > 0,
               "H1: the location pool is empty, so the item pool cannot be generated against it");
    GenerateItemPool();
    const RandomizerGet atHunt = ctx->GetItemLocation(RC_TRIFORCE_COMPLETED)->GetPlacedRandomizerGet();
    const RandomizerGet atGanon = ctx->GetItemLocation(RC_GANON)->GetPlacedRandomizerGet();
    printf("[TEST] H1: RC_TRIFORCE_COMPLETED holds %d, RC_GANON holds %d\n", (int)atHunt, (int)atGanon);
    OTH_ASSERT(atHunt != RG_GANONS_CASTLE_BOSS_KEY,
               "H1: RC_TRIFORCE_COMPLETED holds Ganon's Boss Key under the combo triforce hunt (#997)");
    OTH_ASSERT(atHunt == RG_TRIFORCE && atGanon != RG_TRIFORCE,
               "H1: the generated world does not put OoT's hunt completion where its \"Win\" mode does");
    // The player is told where the option is set.
    const std::string& huntDesc = Rando::Settings::GetInstance()->GetOption(RSK_TRIFORCE_HUNT).GetDescription();
    const bool descStates =
        huntDesc.find("Under the Triforce Hunt combo goal, \"Ganon's Boss Key\" is generated as \"Win\"") !=
        std::string::npos;
    printf("[TEST] H1: the Triforce Hunt option description %s the combo-hunt change\n",
           descStates ? "states" : "does NOT state");
    OTH_ASSERT(descStates, "H1: the Triforce Hunt option description does not tell the player that the combo "
                           "triforce hunt generates \"Ganon's Boss Key\" as \"Win\" (#997)");
    // Leave no placement behind for the control's creation.
    ctx->ItemReset();

    // ---- H2 (control): authored "Win" under the combo hunt stays "Win" -------
    const int rc2 = GenerateAuthored(hunt, RO_TRIFORCE_HUNT_WIN, "RSBSHUNTWIN997");
    OTH_ASSERT(rc2 != -100, "H2: the authored combo goal did not reach the creation's resolver");
    const int mode2 = (int)ctx->GetOption(RSK_TRIFORCE_HUNT).Get();
    printf("[TEST] H2: combo goal triforce-hunt, OoT hunt authored \"Win\" -> mode %d (bridge rc %d)\n", mode2, rc2);
    OTH_ASSERT(mode2 == RO_TRIFORCE_HUNT_WIN, "H2: under the combo triforce hunt an authored \"Win\" did not stay "
                                              "\"Win\" (the #768 boss-goal rewrite must not apply)");

    ComboContext_Init();
    printf("[TEST] PASS: rando-combo-hunt-own-mode\n");
    return 0;
}
