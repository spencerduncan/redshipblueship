/**
 * @file test_combo_goal.c
 * @brief combo-goal-ending: the paired game ends when the FROZEN goal is met,
 *        in the game whose final-boss defeat meets it (OoTMM parity, #762).
 *        redship tier: display-free, ROM-free.
 *
 * WHAT IS LOCKED, leg by leg:
 *
 *   E1 THE PREDICATE, for every pinned goal value over every pair of defeats
 *      (and the shared piece count for triforce-hunt): the coordinator's own
 *      expression. `beat-oot` never reads Majora, `beat-mm` never reads Ganon,
 *      triforce-hunt reads neither boss; a value outside the table is -1.
 *   E2 THE DECISION over synthetic defeats, for every goal value and both
 *      orders: under `beat-both` the FIRST boss is withheld and the SECOND plays
 *      the ending; under `beat-either` the first plays it; under `beat-oot` /
 *      `beat-mm` only that game's boss plays it, and the other game's boss is
 *      withheld until then; under triforce-hunt a boss is withheld until the
 *      shared count reaches the frozen combo requirement.
 *   E3 UNPAIRED IS UPSTREAM: with no frozen combo record every defeat is "own"
 *      and nothing is recorded.
 *   E4 THE RECORD: one bit per game in sharedFlags word 0, idempotent; the
 *      creation event's invalidation (KEEP) clears it while keeping the frozen
 *      goal, so a new world never starts with a boss already beaten.
 *   E5 BOTH PORTS' REAL ENDING SITES (games/oot/soh/oot_combo_goal_test.cpp,
 *      games/mm/2s2h/mm_combo_goal_test.cpp): what BossGanon2's and Majora's
 *      warps leave behind. Withheld, OoT goes to Ganon's Tower as an adult with
 *      no cutscene and MM to South Clock Town at day 0, 05:59, with the cycle
 *      rolled over by the new-day save; met or unpaired, both warps stand.
 *
 * On main (before #762) every final-boss defeat played its own game's ending
 * under every goal: E2's and E5's withheld expectations are the red half (see
 * the PR for the counterfactual run).
 */

#include "../combo_goal.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../triforce_hunt.h"

#include <cstdio>
#include <cstring>

// Each port's site helper.
extern "C" {
int OoT_ComboGoalTest_EndingSite(void);
int OoT_ComboGoalTest_FinalBlow(void);
int MM_ComboGoalTest_EndingSite(void);
int MM_ComboGoalTest_FinalBlow(void);
}

#define CG_ASSERT(cond, ...)                                     \
    do {                                                         \
        if (!(cond)) {                                           \
            printf("[TEST] FAIL (%s:%d): ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                                 \
            printf("\n");                                        \
            ComboContext_Init();                                 \
            Combo_ResetSharedResourceWatermarks();               \
            return TEST_FAIL;                                    \
        }                                                        \
    } while (0)

namespace {

const uint16_t kHuntOoTTotal = 5;
const uint16_t kHuntOoTRequired = 3;
const uint16_t kHuntMMTotal = 4;
const uint16_t kHuntMMRequired = 2;

/** A paired world frozen with combo goal `goal` and no defeat recorded. */
void CgFreezeWorld(uint8_t goal) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x762E0D01u;
    gComboCtx.sharedRandoSettingsHash = 0x762E0D02u;
    gComboCtx.mmProfileDigest = 0x762E0D03u;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    rec.goal = goal;
    Combo_FreezeComboSettings(&rec);
    if (goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) {
        ComboTriforceHalf oot;
        oot.total = kHuntOoTTotal;
        oot.required = kHuntOoTRequired;
        ComboTriforceHalf mm;
        mm.total = kHuntMMTotal;
        mm.required = kHuntMMRequired;
        Combo_TriforceFreezeAtCreation(&oot, &mm);
    }
}

const char* CgGoalName(uint8_t goal) {
    switch (goal) {
        case RSBS_COMBO_GOAL_BEAT_BOTH:
            return "beat-both";
        case RSBS_COMBO_GOAL_BEAT_EITHER:
            return "beat-either";
        case RSBS_COMBO_GOAL_TRIFORCE_HUNT:
            return "triforce-hunt";
        case RSBS_COMBO_GOAL_BEAT_OOT:
            return "beat-oot";
        case RSBS_COMBO_GOAL_BEAT_MM:
            return "beat-mm";
        default:
            return "(invalid)";
    }
}

const char* CgGameName(GameId game) {
    return game == GAME_OOT ? "Ganon" : "Majora";
}

/** What the goal expression says for a pair of defeats (the expected value). */
bool CgExpectMet(uint8_t goal, bool oot, bool mm) {
    switch (goal) {
        case RSBS_COMBO_GOAL_BEAT_BOTH:
            return oot && mm;
        case RSBS_COMBO_GOAL_BEAT_EITHER:
            return oot || mm;
        case RSBS_COMBO_GOAL_BEAT_OOT:
            return oot;
        case RSBS_COMBO_GOAL_BEAT_MM:
            return mm;
        default:
            return false; // triforce-hunt: no boss is a term
    }
}

} // namespace

TestResult Test_ComboGoalEnding(void) {
    printf("[TEST] combo-goal-ending: the paired game ends when the frozen goal is met, in the game whose final boss "
           "meets it (#762, OoTMM parity)\n");

    const uint8_t kGoals[] = {
        (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH,     (uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER,
        (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT, (uint8_t)RSBS_COMBO_GOAL_BEAT_OOT,
        (uint8_t)RSBS_COMBO_GOAL_BEAT_MM,
    };
    const GameId kGames[2] = { GAME_OOT, GAME_MM };

    // ---- E1: the predicate ---------------------------------------------------
    for (uint8_t goal : kGoals) {
        if (goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) {
            continue;
        }
        for (int o = 0; o < 2; o++) {
            for (int m = 0; m < 2; m++) {
                const int got = Combo_GoalMet(goal, o != 0, m != 0, -1, 0);
                CG_ASSERT(got == (CgExpectMet(goal, o != 0, m != 0) ? 1 : 0),
                          "E1: %s with Ganon %s and Majora %s evaluates %d", CgGoalName(goal),
                          o ? "beaten" : "not beaten", m ? "beaten" : "not beaten", got);
            }
        }
    }
    const uint8_t hunt = (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT;
    CG_ASSERT(Combo_GoalMet(hunt, true, true, 4, 5) == 0,
              "E1: triforce-hunt with both bosses beaten and 4 of 5 pieces must not be met (no boss is a term)");
    CG_ASSERT(Combo_GoalMet(hunt, false, false, 5, 5) == 1, "E1: triforce-hunt at 5 of 5 pieces must be met");
    CG_ASSERT(Combo_GoalMet(hunt, false, false, 6, 5) == 1, "E1: triforce-hunt past its requirement must be met");
    CG_ASSERT(Combo_GoalMet(hunt, true, true, 5, 0) == -1, "E1: triforce-hunt with no requirement is unevaluable");
    CG_ASSERT(Combo_GoalMet(0, true, true, 0, 0) == -1 && Combo_GoalMet(6, true, true, 0, 0) == -1,
              "E1: a goal outside the pinned table must be unevaluable, never met");
    printf("[TEST] E1: the predicate is the coordinator's expression for all five goals\n");

    // ---- E3: unpaired is upstream --------------------------------------------
    ComboContext_Init();
    CG_ASSERT(!Combo_GoalArmed(), "E3: a context with no frozen combo record must not arm the goal");
    for (GameId game : kGames) {
        const int ending = Combo_GoalOnFinalBossDefeated(game, -1);
        CG_ASSERT(ending == RSBS_GOAL_ENDING_OWN, "E3: unpaired, %s's defeat decided '%s', expected 'own'",
                  CgGameName(game), Combo_GoalEndingName(ending));
    }
    CG_ASSERT(gComboCtx.sharedFlags[RSBS_SHARED_FLAGS_WORD_GOAL] == 0u,
              "E3: unpaired, a defeat wrote sharedFlags word 0 (0x%08x)",
              (unsigned)gComboCtx.sharedFlags[RSBS_SHARED_FLAGS_WORD_GOAL]);
    CG_ASSERT(OoT_ComboGoalTest_EndingSite() == 0,
              "E3: unpaired, OoT's real ending site did not leave the ending warp");
    CG_ASSERT(MM_ComboGoalTest_EndingSite() == 0, "E3: unpaired, MM's real ending site did not leave the ending warp");
    CG_ASSERT(OoT_ComboGoalTest_FinalBlow() == 1 && MM_ComboGoalTest_FinalBlow() == 1,
              "E3: unpaired, a final blow must let the game mark itself complete");
    printf("[TEST] E3: unpaired, both games end as upstream and nothing is recorded\n");

    // ---- E2 + E5: the decision, both orders, every goal -----------------------
    for (uint8_t goal : kGoals) {
        for (int order = 0; order < 2; order++) {
            const GameId first = kGames[order];
            const GameId second = kGames[1 - order];
            CgFreezeWorld(goal);
            CG_ASSERT(Combo_GoalArmed() && gComboCtx.comboSettings.goal == goal, "E2: the %s world did not freeze",
                      CgGoalName(goal));

            bool oot = false;
            bool mm = false;
            const GameId sequence[2] = { first, second };
            for (int step = 0; step < 2; step++) {
                const GameId game = sequence[step];
                (game == GAME_OOT ? oot : mm) = true;
                const bool met = CgExpectMet(goal, oot, mm);
                const int expect = met ? RSBS_GOAL_ENDING_PLAY : RSBS_GOAL_ENDING_WITHHOLD;

                // The final blow first, as each boss's death cutscene does.
                const int blow = game == GAME_OOT ? OoT_ComboGoalTest_FinalBlow() : MM_ComboGoalTest_FinalBlow();
                CG_ASSERT(blow == (met ? 1 : 0),
                          "E2: %s, %s defeated %s: the final blow says the game is %scomplete, expected %s",
                          CgGoalName(goal), CgGameName(game), step == 0 ? "first" : "second", blow ? "" : "not ",
                          met ? "complete" : "not complete");
                CG_ASSERT(Combo_GoalFinalBossRecorded(game), "E4: %s, %s's defeat was not recorded", CgGoalName(goal),
                          CgGameName(game));

                // Then the ending warp, through the port's real site.
                const int site = game == GAME_OOT ? OoT_ComboGoalTest_EndingSite() : MM_ComboGoalTest_EndingSite();
                CG_ASSERT(site == (met ? 0 : 1), "E5: %s, %s defeated %s: its real ending site %s, expected it to %s",
                          CgGoalName(goal), CgGameName(game), step == 0 ? "first" : "second",
                          site == 0 ? "played the ending" : (site == 1 ? "withheld the ending" : "left a broken warp"),
                          met ? "play the ending" : "withhold it and return the player to the game");

                // The decision itself is idempotent: the same answer again.
                const int again = Combo_GoalOnFinalBossDefeated(game, -1);
                CG_ASSERT(again == expect, "E2: %s, %s defeated %s: decided '%s', expected '%s'", CgGoalName(goal),
                          CgGameName(game), step == 0 ? "first" : "second", Combo_GoalEndingName(again),
                          Combo_GoalEndingName(expect));
            }
            const uint32_t word = gComboCtx.sharedFlags[RSBS_SHARED_FLAGS_WORD_GOAL];
            CG_ASSERT(word == (RSBS_GOAL_FLAG_OOT_FINAL_BOSS | RSBS_GOAL_FLAG_MM_FINAL_BOSS),
                      "E4: %s, after both defeats sharedFlags word 0 is 0x%08x, expected exactly the two goal bits",
                      CgGoalName(goal), (unsigned)word);
            for (int w = 1; w < 64; w++) {
                CG_ASSERT(gComboCtx.sharedFlags[w] == 0u, "E4: the goal record wrote sharedFlags word %d", w);
            }
        }
        printf("[TEST] E2/E5: %s: the ending plays exactly when the goal becomes true, in both orders\n",
               CgGoalName(goal));
    }

    // ---- E2 (triforce-hunt): the last piece, not a boss, meets the goal --------
    CgFreezeWorld(hunt);
    CG_ASSERT(Combo_TriforceHuntArmed(), "E2: the triforce-hunt world did not arm its hunt");
    const uint16_t required = Combo_TriforceHuntRequired();
    CG_ASSERT(required == kHuntOoTRequired + kHuntMMRequired, "E2: the combo requirement is %u", (unsigned)required);
    CG_ASSERT(Combo_GoalOnFinalBossDefeated(GAME_OOT, (int)required - 1) == RSBS_GOAL_ENDING_WITHHOLD,
              "E2: triforce-hunt, Ganon defeated one piece short must withhold the ending");
    CG_ASSERT(Combo_GoalOnFinalBossDefeated(GAME_MM, (int)required - 1) == RSBS_GOAL_ENDING_WITHHOLD,
              "E2: triforce-hunt, Majora defeated one piece short must withhold the ending");
    CG_ASSERT(Combo_GoalOnFinalBossDefeated(GAME_OOT, (int)required) == RSBS_GOAL_ENDING_PLAY,
              "E2: triforce-hunt at the combo requirement, a boss defeat must play the ending");
    printf("[TEST] E2: triforce-hunt: no boss ends the game until the shared count reaches the requirement\n");

    // ---- E4: the creation clears the record, keeps the goal -------------------
    CgFreezeWorld((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH);
    Combo_GoalOnFinalBossDefeated(GAME_OOT, -1);
    CG_ASSERT(Combo_GoalFinalBossRecorded(GAME_OOT), "E4: Ganon's defeat was not recorded");
    Context_InvalidateSessionState(RSBS_SEED_STAMP_KEEP);
    CG_ASSERT(Combo_GoalArmed() && gComboCtx.comboSettings.goal == (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH,
              "E4: the creation's invalidation (KEEP) dropped the frozen goal");
    CG_ASSERT(!Combo_GoalFinalBossRecorded(GAME_OOT) && !Combo_GoalFinalBossRecorded(GAME_MM),
              "E4: the creation's invalidation kept a defeat: a new world would start with Ganon beaten");
    printf("[TEST] E4: the record is session state: a new world starts with no boss beaten and its goal frozen\n");

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] PASS: combo-goal-ending\n");
    return TEST_PASS;
}
