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
 *      with OoT's own hunt set to "Ganon's Boss Key" — the mode whose own
 *      threshold does NOT end the game — and reads what the arm leaves behind:
 *        - UNARMED: OoT's own `==` at OoT's own requirement grants Ganon's Boss
 *          Key and does not end the game, exactly as upstream.
 *        - ARMED: OoT's own requirement grants nothing (it is an input); the
 *          COMBO requirement takes the Win branch — game complete and the
 *          credits warp armed — although OoT's own mode is Ganon's Boss Key,
 *          because under the combo goal reaching the requirement IS the win.
 *
 * COUNTERFACTUAL, run before landing: replace the Combo_TriforceHuntOnPieceGiven
 * call in randomizer.cpp with upstream's `== RSK_..._REQUIRED + 1` and leg W2
 * goes red at the own-requirement give; drop the `Combo_TriforceHuntArmed() ||`
 * and W3 goes red (the GBK mode never ends the game).
 */

#include <cstdio>
#include <cstring>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

#include "context.h"
#include "foreign_items.h" // Combo_FreezeComboSettings, RSBS_COMBO_GOAL_TRIFORCE_HUNT
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
           "only (ADR 0010 O10)\n");

    OTH_ASSERT(Rando_HeadlessSeedTest("RSBSTRIFORCEO10") == 0, "headless OoT seed generation failed");
    auto ctx = Rando::Context::GetInstance();
    OTH_ASSERT(ctx != nullptr && OTRGlobals::Instance != nullptr && OTRGlobals::Instance->gRandomizer != nullptr,
               "no randomizer context after generation");

    // OoT's own hunt: "Ganon's Boss Key" mode, so OoT's OWN threshold grants the
    // key and does NOT end the game — which is what lets W3 tell the combo win
    // apart from OoT's own. The give reads these back through the context.
    ctx->GetOption(RSK_TRIFORCE_HUNT).Set(RO_TRIFORCE_HUNT_GBK);
    ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_TOTAL).Set((uint8_t)(kOotTotal - 1));
    ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_REQUIRED).Set((uint8_t)(kOotRequired - 1));
    // Randomizer::GetRandoSettingValue, which the give reads, is exactly this.
    OTH_ASSERT(ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_REQUIRED).Get() + 1 == kOotRequired &&
                   ctx->GetOption(RSK_TRIFORCE_HUNT).Get() == RO_TRIFORCE_HUNT_GBK,
               "the hunt settings did not take in the seed context");
    // The Win branch emits a toast; a headless process has no audio session to
    // play its chime into.
    CVarSetInteger(CVAR_SETTING("Notifications.Mute"), 1);

    const GetItemEntry piece = Rando::StaticData::RetrieveItem(RG_TRIFORCE_PIECE).GetGIEntry_Copy();

    // ---- W1: UNARMED, OoT's own hunt, exactly as upstream -----------------
    OTH_ASSERT(FreezeWorld((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH) == RSBS_TRIFORCE_OK,
               "a beat-both creation must freeze (a zero triforce record)");
    OTH_ASSERT(!Combo_TriforceHuntArmed(), "a beat-both world must not arm the combo hunt");
    ResetHuntState((uint8_t)(kOotRequired - 1));
    Randomizer_Item_Give(NULL, piece);
    OTH_ASSERT(gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected == kOotRequired,
               "the piece give did not count the piece");
    OTH_ASSERT(BossKeyGranted(), "UNARMED: reaching OoT's own requirement did not grant Ganon's Boss Key - OoT's own "
                                 "hunt is not reached through the arm");
    OTH_ASSERT(!GameEnded(), "UNARMED: OoT's own Ganon's-Boss-Key mode ended the game");

    // ---- W2: ARMED, the own requirement is NOT the threshold --------------
    OTH_ASSERT(FreezeWorld((uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) == RSBS_TRIFORCE_OK,
               "a triforce-hunt creation over two coherent halves must freeze");
    OTH_ASSERT(Combo_TriforceHuntArmed(), "a frozen triforce-hunt world must arm the combo hunt");
    const uint16_t comboRequired = Combo_TriforceHuntRequired();
    OTH_ASSERT(comboRequired == (uint16_t)(kOotRequired + kMmRequired),
               "the combo requirement is not the sum of the two halves' requirements");
    ResetHuntState((uint8_t)(kOotRequired - 1));
    Randomizer_Item_Give(NULL, piece);
    OTH_ASSERT(gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected == kOotRequired,
               "the piece was not counted");
    OTH_ASSERT(!BossKeyGranted() && !GameEnded(),
               "ARMED: OoT's OWN requirement fired OoT's hunt in a paired world - under the combo goal it is an input "
               "to the combo requirement, not the threshold (ADR 0010 O10)");

    // ---- W3: ARMED, the combo requirement IS the threshold, and the WIN ----
    ResetHuntState((uint8_t)(comboRequired - 1));
    Randomizer_Item_Give(NULL, piece);
    OTH_ASSERT(gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected == comboRequired,
               "the piece was not counted");
    OTH_ASSERT(GameEnded(), "ARMED: reaching the COMBO requirement in OoT did not take the Win branch (game complete "
                            "and the credits warp) - under the combo goal reaching it is the win, whichever mode OoT's "
                            "own setting named");
    // Equality, as OoT tests it: one more piece fires nothing new.
    ResetHuntState(comboRequired);
    Randomizer_Item_Give(NULL, piece);
    OTH_ASSERT(!BossKeyGranted() && !GameEnded(), "ARMED: a piece past the combo requirement fired the win again");

    ComboContext_Init();
    ResetHuntState(0);
    printf("[TEST] PASS: rando-triforce-hunt-win (OoT)\n");
    return 0;
}
