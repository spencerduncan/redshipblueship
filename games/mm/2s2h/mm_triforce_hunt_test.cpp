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
 *      REAL `Rando::GiveItem(RI_TRIFORCE_PIECE)` and reads what MM's own hunt
 *      ending leaves behind (Majora's soul granted and one ending transition
 *      queued):
 *        - UNARMED (no frozen hunt): MM's own `==` at MM's own requirement
 *          fires, exactly as upstream wrote it. This is what makes the next
 *          leg's silence mean something.
 *        - ARMED: reaching MM's OWN requirement does NOT fire — it is an input
 *          to the combo requirement, not the threshold — and reaching the COMBO
 *          requirement does.
 *      It also checks MM_Rando_ResolveTriforceHalf reads MM's half the way the
 *      record's rule says (both sources), and that the MM arrival's half
 *      compare refuses a half the record did not freeze.
 *
 * COUNTERFACTUAL, run before landing: replace the Combo_TriforceHuntOnPieceGiven
 * call in Rando/GiveItem.cpp with upstream's `== RANDO_SAVE_OPTIONS[...]` and
 * leg W2 goes red at the own-requirement give.
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

    // ---- W1: UNARMED, MM's own hunt, exactly as upstream ------------------
    // No frozen hunt (beat-both world): the arm must fire at MM's own
    // requirement. Without this leg, W2's silence could be a dead arm.
    MTH_ASSERT(FreezeWorld((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH) == RSBS_TRIFORCE_OK,
               "a beat-both creation must freeze (a zero triforce record)");
    MTH_ASSERT(!Combo_TriforceHuntArmed(), "a beat-both world must not arm the combo hunt");
    ArmHuntSave((uint16_t)(kMmRequired - 1));
    Rando::GiveItem(RI_TRIFORCE_PIECE);
    MTH_ASSERT(gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces == kMmRequired,
               "the piece give did not count the piece");
    MTH_ASSERT(SoulFound(), "UNARMED: reaching MM's own requirement did not grant Majora's soul - MM's own hunt "
                            "ending is not reached through the arm");
    MTH_ASSERT(queue.size() == queueBase + 1, "UNARMED: reaching MM's own requirement did not queue MM's ending "
                                              "transition");
    queue.resize(queueBase);

    // ---- W2: ARMED, the own requirement is NOT the threshold --------------
    MTH_ASSERT(FreezeWorld((uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) == RSBS_TRIFORCE_OK,
               "a triforce-hunt creation over two coherent halves must freeze");
    MTH_ASSERT(Combo_TriforceHuntArmed(), "a frozen triforce-hunt world must arm the combo hunt");
    MTH_ASSERT(Combo_TriforceHuntRequired() == (uint16_t)(kOotRequired + kMmRequired),
               "the combo requirement is not the sum of the two halves' requirements");
    ArmHuntSave((uint16_t)(kMmRequired - 1));
    Rando::GiveItem(RI_TRIFORCE_PIECE);
    MTH_ASSERT(gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces == kMmRequired, "the piece was not counted");
    MTH_ASSERT(!SoulFound() && queue.size() == queueBase,
               "ARMED: MM's OWN requirement ended a paired hunt - under the combo goal it is an input to the "
               "combo requirement, not the threshold (ADR 0010 O10)");

    // ---- W3: ARMED, the combo requirement IS the threshold ----------------
    // The counter is the one combo count (an arrival would have raised it to
    // pieces found in OoT too); set it one short and give.
    const uint16_t comboRequired = Combo_TriforceHuntRequired();
    gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces = (uint16_t)(comboRequired - 1);
    Rando::GiveItem(RI_TRIFORCE_PIECE);
    MTH_ASSERT(gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces == comboRequired, "the piece was not counted");
    MTH_ASSERT(SoulFound(), "ARMED: reaching the COMBO requirement in MM did not grant Majora's soul");
    MTH_ASSERT(queue.size() == queueBase + 1,
               "ARMED: reaching the COMBO requirement in MM did not queue MM's ending transition - the win must fire "
               "in whichever game the reaching collect happens");
    queue.resize(queueBase);
    // Equality, as MM tests it: one more piece past the requirement fires nothing.
    Rando::GiveItem(RI_TRIFORCE_PIECE);
    MTH_ASSERT(queue.size() == queueBase, "ARMED: a piece past the combo requirement fired the ending a second time");

    // ---- W4: MM's half, read the way the record's rule says ---------------
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
