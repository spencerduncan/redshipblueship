/**
 * @file mm_triforce_hunt_test.cpp
 * MM's half of the combo triforce hunt locks (ADR 0010 answer O10; the rule and
 * the contract are in src/common/triforce_hunt.h).
 *
 * Two surfaces live here because both need MM's `gSaveContext` in scope:
 *
 *   1. THE SHIM HELPERS the redship row `combo-triforce-hunt`
 *      (src/common/tests/test_triforce_hunt.c) drives MM's REAL
 *      MM_HarvestSharedResources / MM_ApplySharedResources through, for the
 *      cross-game sum: collect k in OoT and m in MM, and both counters read k+m
 *      after a switch. They only put MM's live save in a known state and read
 *      the counter; every merge decision is the production shims'.
 *
 *   2. THE WIN ARM, `MM_TriforceHuntWin_RunHeadless`, run by the rando row
 *      `rando-triforce-hunt-win` because MM's give path dispatches
 *      GameInteractor hooks and so needs MM's first-boot bring-up. It drives the
 *      REAL `Rando::GiveItem(RI_TRIFORCE_PIECE)` unpaired and under every combo
 *      goal value, and reads what MM's hunt ending leaves behind (Majora's soul,
 *      one OnGameCompletion dispatch, one ending transition queued):
 *        - W1 UNPAIRED: MM's own `==` at MM's own requirement fires all three,
 *          exactly as upstream wrote it. This is what makes the next legs'
 *          silence mean something.
 *        - W2 A BOSS GOAL (1, 2, 4, 5; #768): MM's own requirement grants the
 *          soul (the hunt's lock on Majora) and ends nothing.
 *        - W3/W4 TRIFORCE-HUNT (3): MM's OWN requirement does NOT fire - it is
 *          an input to the combo requirement, not the threshold - and reaching
 *          the COMBO requirement fires all three.
 *      It also checks MM_Rando_ResolveTriforceHalf reads MM's half the way the
 *      record's rule says, from BOTH sources: the save's frozen options
 *      (fromSave=1) and the CVar resolution (fromSave=0) that both production
 *      callers use (the creation event in playthrough.cpp and MM's arrival
 *      gate). And it checks that the arrival's half compare refuses a half the
 *      record did not freeze.
 *
 * COUNTERFACTUAL, run before landing (#768): main's arm (no goal block before
 * OnGameCompletion) turns W2 red. (#740 ran the other one: upstream's `==` in
 * place of the Combo_TriforceHuntOnPieceGiven call turns the own-requirement
 * leg under triforce-hunt, now W3, red.)
 */

#include "global.h"

#include <cstdio>
#include <cstring>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The combo hunt is a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build reports pass rather than failing
// to link.
extern "C" int MM_TriforceHuntWin_RunHeadless(void) {
    printf("[TEST] rando-triforce-hunt-win (MM): PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
extern "C" void MM_TriforceHuntTest_ArmLive(void) {
}
extern "C" void MM_TriforceHuntTest_SetCount(uint16_t count) {
    (void)count;
}
extern "C" int MM_TriforceHuntTest_Count(void) {
    return -1;
}
#else

#include "2s2h/Rando/Rando.h"
#include "2s2h/GameInteractor/GameInteractor.h" // OnGameCompletion, S2H::GameHooks (the registry MM dispatches)
#include "2s2h/Rando/StaticData/StaticData.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_mm_options_view.h" // MM_Rando_ResolveTriforceHalf
#include "context.h"
#include "foreign_items.h" // Combo_FreezeComboSettings, RSBS_COMBO_GOAL_TRIFORCE_HUNT
#include "mm_game_hooks.h" // MM_GameEvents_Queue
#include "triforce_hunt.h"

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;

// The CORE half of MM's rando bring-up (GameExports_SingleExe.cpp). Asset-free
// and once-guarded.
void MM_Rando_InitCore(void);
}

// ============================================================================
// (1) The shim helpers
// ============================================================================

/** MM's live save as a cross-game MM session runs it: the 0xFF "no flash slot"
 *  sentinel and GAMEMODE_NORMAL, which Combo_SaveIsLiveFile requires before
 *  MM_HarvestSharedResources does anything at all. Everything else zero, so the
 *  harvest offers no other kind a nonzero value and the pool holds only what the
 *  row puts there. */
extern "C" void MM_TriforceHuntTest_ArmLive(void) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
}

extern "C" void MM_TriforceHuntTest_SetCount(uint16_t count) {
    gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces = count;
}

extern "C" int MM_TriforceHuntTest_Count(void) {
    return (int)gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces;
}

// ============================================================================
// (2) The win arm
// ============================================================================

namespace {

#define MTH_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// MM's own hunt in this row: 4 pieces in MM's pool, 3 of them required.
const uint8_t kMmTotal = 4;
const uint8_t kMmRequired = 3;
// OoT's half of the combo: 5 pieces, 3 required. Combo requirement 3 + 3 = 6.
const uint8_t kOotTotal = 5;
const uint8_t kOotRequired = 3;

/** An MM hunt save, with Majora's soul unfound and the piece counter at `count`. */
void ArmHuntSave(uint16_t count) {
    MM_TriforceHuntTest_ArmLive();
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_TRIFORCE_PIECES] = RO_GENERIC_YES;
    RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_MAX] = kMmTotal;
    RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_REQUIRED] = kMmRequired;
    gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces = count;
}

/** Freeze a paired world whose combo goal is `goal`, with the O10 record the
 *  creation rule resolves from the two halves above (a no-op record for any
 *  goal other than triforce-hunt). */
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

bool SoulFound() {
    return Flags_GetRandoInf(RANDO_INF_OBTAINED_SOUL_OF_BOSS_MAJORA) != 0;
}

} // namespace

extern "C" int MM_TriforceHuntWin_RunHeadless(void) {
    printf("[TEST] rando-triforce-hunt-win (MM): MM's real piece give ends a PAIRED hunt at the combo requirement "
           "only (ADR 0010 O10)\n");
    MM_Rando_InitCore();

    std::vector<GIEvent>& queue = MM_GameEvents_Queue();
    const size_t queueBase = queue.size();
    // OnGameCompletion is observed directly: a counting hook beside whatever
    // else is registered (SavingEnhancements' file-completed stamp).
    static int sCompletions = 0;
    sCompletions = 0;
    const uint32_t completionHook =
        S2H::GameHooks::Register<GameInteractor::OnGameCompletion>([]() { sCompletions++; });

    // ---- W1: UNPAIRED, MM's own hunt, exactly as upstream ------------------
    // No frozen combo record: the arm must fire at MM's own requirement - soul,
    // OnGameCompletion and the ending transition. Without this leg the paired
    // legs' silence could be a dead arm.
    ComboContext_Init();
    MTH_ASSERT(!Combo_ComboSettingsFrozen(), "an initialized context must hold no frozen combo record");
    ArmHuntSave((uint16_t)(kMmRequired - 1));
    Rando::GiveItem(RI_TRIFORCE_PIECE);
    MTH_ASSERT(gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces == kMmRequired,
               "the piece give did not count the piece");
    MTH_ASSERT(SoulFound(), "UNPAIRED: reaching MM's own requirement did not grant Majora's soul - MM's own hunt "
                            "ending is not reached through the arm");
    MTH_ASSERT(queue.size() == queueBase + 1 && sCompletions == 1,
               "UNPAIRED: reaching MM's own requirement did not dispatch OnGameCompletion and queue MM's ending "
               "transition, as upstream does");
    queue.resize(queueBase);
    sCompletions = 0;

    const uint8_t kGoals[5] = {
        (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH,     (uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER,
        (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT, (uint8_t)RSBS_COMBO_GOAL_BEAT_OOT,
        (uint8_t)RSBS_COMBO_GOAL_BEAT_MM,
    };
    for (uint8_t goal : kGoals) {
        MTH_ASSERT(FreezeWorld(goal) == RSBS_TRIFORCE_OK, "a paired creation over two coherent halves must freeze");
        if (goal != (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) {
            // ---- W2: a BOSS goal: MM's own requirement is the hunt, never the
            // win (#768). Majora's soul is granted (the hunt's lock on Majora);
            // no OnGameCompletion, no ending transition.
            MTH_ASSERT(!Combo_TriforceHuntArmed(), "a boss-goal world must not arm the combo hunt");
            ArmHuntSave((uint16_t)(kMmRequired - 1));
            Rando::GiveItem(RI_TRIFORCE_PIECE);
            MTH_ASSERT(gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces == kMmRequired,
                       "the piece was not counted");
            if (!SoulFound() || queue.size() != queueBase || sCompletions != 0) {
                printf("[TEST] FAIL: W2 goal %u: MM's OWN triforce hunt in a paired world whose combo goal is a boss "
                       "goal: soul %d, ending transitions queued %d, OnGameCompletion dispatched %d (expected 1, 0, "
                       "0) - the frozen combo goal is the only win condition; the hunt only unlocks Majora (#768) "
                       "(%s:%d)\n",
                       (unsigned)goal, SoulFound() ? 1 : 0, (int)(queue.size() - queueBase), sCompletions, __FILE__,
                       __LINE__);
                queue.resize(queueBase);
                S2H::GameHooks::Unregister<GameInteractor::OnGameCompletion>(completionHook);
                return 1;
            }
            continue;
        }

        // ---- W3: triforce-hunt, the own requirement is NOT the threshold ----
        MTH_ASSERT(Combo_TriforceHuntArmed(), "a frozen triforce-hunt world must arm the combo hunt");
        MTH_ASSERT(Combo_TriforceHuntRequired() == (uint16_t)(kOotRequired + kMmRequired),
                   "the combo requirement is not the sum of the two halves' requirements");
        ArmHuntSave((uint16_t)(kMmRequired - 1));
        Rando::GiveItem(RI_TRIFORCE_PIECE);
        MTH_ASSERT(gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces == kMmRequired,
                   "the piece was not counted");
        MTH_ASSERT(!SoulFound() && queue.size() == queueBase && sCompletions == 0,
                   "ARMED: MM's OWN requirement ended a paired hunt - under the combo goal it is an input to the "
                   "combo requirement, not the threshold (ADR 0010 O10)");

        // ---- W4: triforce-hunt, the combo requirement IS the win -------------
        // The counter is the one combo count (an arrival would have raised it to
        // pieces found in OoT too); set it one short and give.
        const uint16_t comboRequired = Combo_TriforceHuntRequired();
        gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces = (uint16_t)(comboRequired - 1);
        Rando::GiveItem(RI_TRIFORCE_PIECE);
        MTH_ASSERT(gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces == comboRequired,
                   "the piece was not counted");
        MTH_ASSERT(SoulFound(), "ARMED: reaching the COMBO requirement in MM did not grant Majora's soul");
        MTH_ASSERT(queue.size() == queueBase + 1 && sCompletions == 1,
                   "ARMED: reaching the COMBO requirement in MM did not dispatch OnGameCompletion and queue MM's "
                   "ending transition - the win must fire in whichever game the reaching collect happens");
        queue.resize(queueBase);
        sCompletions = 0;
        // Equality, as MM tests it: one more piece past the requirement fires nothing.
        Rando::GiveItem(RI_TRIFORCE_PIECE);
        MTH_ASSERT(queue.size() == queueBase && sCompletions == 0,
                   "ARMED: a piece past the combo requirement fired the ending a second time");
    }
    S2H::GameHooks::Unregister<GameInteractor::OnGameCompletion>(completionHook);
    printf("[TEST] MM: unpaired as upstream; under goals 1, 2, 4, 5 the own hunt grants Majora's soul and ends "
           "nothing; under goal 3 only the combo requirement ends the game\n");

    // ---- W5: MM's half, read the way the record's rule says ---------------
    {
        ArmHuntSave(0);
        uint16_t total = 0xFFFF;
        uint16_t required = 0xFFFF;
        MM_Rando_ResolveTriforceHalf(/*fromSave=*/1, &total, &required);
        MTH_ASSERT(total == kMmTotal && required == kMmRequired,
                   "MM_Rando_ResolveTriforceHalf did not read MM's pool size and requirement from the save");
        RANDO_SAVE_OPTIONS[RO_SHUFFLE_TRIFORCE_PIECES] = RO_GENERIC_NO;
        MM_Rando_ResolveTriforceHalf(/*fromSave=*/1, &total, &required);
        MTH_ASSERT(total == 0 && required == 0, "MM's hunt OFF must contribute no pieces and no requirement");
        // The out-of-range half is reported as it is, so the rule can refuse it.
        RANDO_SAVE_OPTIONS[RO_SHUFFLE_TRIFORCE_PIECES] = RO_GENERIC_YES;
        RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_MAX] = 1000;
        MM_Rando_ResolveTriforceHalf(/*fromSave=*/1, &total, &required);
        MTH_ASSERT(total == 1000, "MM's half was truncated before the rule could see it");

        // The CVAR source (fromSave=0): what the creation event freezes from and
        // what MM's arrival gate compares against. The save is scribbled with
        // other values first so a pass cannot come from the save's options.
        ArmHuntSave(0);
        RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_MAX] = 9;
        RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_REQUIRED] = 8;
        const char* const shuffleCVar = Rando::StaticData::Options[RO_SHUFFLE_TRIFORCE_PIECES].cvar;
        const char* const maxCVar = Rando::StaticData::Options[RO_TRIFORCE_PIECES_MAX].cvar;
        const char* const requiredCVar = Rando::StaticData::Options[RO_TRIFORCE_PIECES_REQUIRED].cvar;
        CVarSetInteger(shuffleCVar, RO_GENERIC_YES);
        CVarSetInteger(maxCVar, kMmTotal);
        CVarSetInteger(requiredCVar, kMmRequired);
        total = 0xFFFF;
        required = 0xFFFF;
        MM_Rando_ResolveTriforceHalf(/*fromSave=*/0, &total, &required);
        const bool cvarOn = total == kMmTotal && required == kMmRequired;
        CVarSetInteger(shuffleCVar, RO_GENERIC_NO);
        MM_Rando_ResolveTriforceHalf(/*fromSave=*/0, &total, &required);
        const bool cvarOff = total == 0 && required == 0;
        CVarSetInteger(shuffleCVar, RO_GENERIC_YES);
        CVarSetInteger(maxCVar, 1000);
        MM_Rando_ResolveTriforceHalf(/*fromSave=*/0, &total, &required);
        const bool cvarWide = total == 1000;
        CVarClear(shuffleCVar);
        CVarClear(maxCVar);
        CVarClear(requiredCVar);
        MTH_ASSERT(cvarOn, "MM_Rando_ResolveTriforceHalf(fromSave=0) did not read MM's pool size and requirement from "
                           "the option CVars - the creation event would freeze, and the arrival compare against, "
                           "something other than MM's settings");
        MTH_ASSERT(cvarOff, "MM's hunt OFF in the CVars must contribute no pieces and no requirement");
        MTH_ASSERT(cvarWide, "MM's CVar half was truncated before the rule could see it");

        // The arrival's half compare (GameExports_SingleExe.cpp) against the
        // record frozen above: MM's own half matches, a moved one does not.
        const ComboTriforceHalf same = { kMmTotal, kMmRequired };
        const ComboTriforceHalf moved = { kMmTotal, (uint16_t)(kMmRequired - 1) };
        MTH_ASSERT(!Combo_TriforceHalfDiverges(&gComboCtx.comboTriforce, GAME_MM, &same),
                   "MM's own frozen half reads as divergent");
        MTH_ASSERT(Combo_TriforceHalfDiverges(&gComboCtx.comboTriforce, GAME_MM, &moved),
                   "an MM half whose requirement moved since creation is not refused");
    }

    ComboContext_Init();
    MM_TriforceHuntTest_ArmLive();
    printf("[TEST] PASS: rando-triforce-hunt-win (MM)\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
