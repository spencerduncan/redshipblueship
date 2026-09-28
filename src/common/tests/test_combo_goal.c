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
 *   E5 EACH PORT'S REDIRECT OVER A REPLICA OF ITS SITE'S ASSIGNMENTS
 *      (games/oot/soh/oot_combo_goal_test.cpp, games/mm/2s2h/mm_combo_goal_test.cpp):
 *      the helpers re-type BossGanon2's and Majora's upstream warp assignments on
 *      a heap PlayState and call the real redirect functions; the ACTORS are not
 *      run. Withheld, OoT goes to Ganon's Tower as an adult with no cutscene and
 *      MM to South Clock Town at day 0, 05:59, human form, no mask, with the
 *      cycle rolled over by the new-day save; met or unpaired, both warps stand.
 *   E6 TIME SPLITS: the REAL TimeSplitCompleteSplits (Ganon can be the last
 *      split) marks OoT complete only when the paired game is over.
 *   E7 THE WIRING, from source (RSBS_SOURCE_DIR): each call site (BossGanon2's
 *      warp, Majora's warp, Majora's final blow, OoT's final-blow completion and
 *      its hook, Time Splits' completion, and since #768 both triforce piece
 *      give arms) makes its call right after the
 *      upstream lines it follows, inside `#ifdef RSBS_SINGLE_EXECUTABLE`; E5's
 *      replicas replay the actors' lines; and no writer of OoT's `gameComplete`
 *      or caller of MM's OnGameCompletion exists beyond the ones this row names.
 *      Its counterfactuals run in the row itself: each site with its call
 *      removed and with its guard removed, each replica drifted, and one added
 *      writer, must each fail the check.
 *   E8 FAIL OPEN: a paired world whose frozen goal cannot be evaluated ends
 *      each game as its own game (logged as an error), never "withhold forever".
 *   E9 A HALF'S OWN TRIFORCE HUNT (#768): Combo_GoalOnTriforceHuntCompleted,
 *      for every goal value and both games: unpaired is "own" (upstream); under
 *      the four boss goals it is "withhold" (the hunt only unlocks its final
 *      boss) and records nothing; under triforce-hunt it is the goal predicate
 *      over the reaching count; an unevaluable goal fails open. And the
 *      creation's rule (Combo_GoalKeepsOwnHuntWin): a half's own hunt may stay a
 *      win condition only under triforce-hunt. E7 checks that both ports' piece
 *      give arms call it, under their guards.
 *
 * On main (before #762) every final-boss defeat played its own game's ending
 * under every goal: E2's and E5's withheld expectations are the red half (see
 * the PR for the counterfactual runs).
 */

#include "../combo_goal.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../triforce_hunt.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// Each port's site helper.
extern "C" {
int OoT_ComboGoalTest_EndingSite(void);
int OoT_ComboGoalTest_FinalBlow(void);
int MM_ComboGoalTest_EndingSite(void);
int MM_ComboGoalTest_FinalBlow(void);
int OoT_ComboGoalTest_TimeSplitsComplete(void);
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

// ---- E7: the wiring, from source --------------------------------------------

/** One call site: the upstream lines it follows (trimmed, consecutive, inside
 *  `function`'s body), the lines inside its `#ifdef RSBS_SINGLE_EXECUTABLE`
 *  block (comment lines skipped), and the lines right after the `#endif`. An
 *  empty `guarded` only requires the anchor to be present. */
struct CgSite {
    const char* what;
    const char* file;
    const char* function;
    std::vector<std::string> anchor;
    std::vector<std::string> guarded;
    std::vector<std::string> after;
};

std::string CgTrim(const std::string& line) {
    size_t b = 0;
    size_t e = line.size();
    while (b < e && (line[b] == ' ' || line[b] == '\t' || line[b] == '\r')) {
        b++;
    }
    while (e > b && (line[e - 1] == ' ' || line[e - 1] == '\t' || line[e - 1] == '\r')) {
        e--;
    }
    return line.substr(b, e - b);
}

std::vector<std::string> CgLines(const std::string& text) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            nl = text.size();
        }
        out.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return out;
}

bool CgIsComment(const std::string& trimmed) {
    return trimmed.rfind("//", 0) == 0 || trimmed.rfind("/*", 0) == 0 || trimmed.rfind("*", 0) == 0;
}

bool CgReadFile(const std::string& rel, std::string* out) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return false;
    }
    out->assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
#else
    (void)rel;
    (void)out;
    return false;
#endif
}

/** "" when `text` wires `site`, else what is wrong. */
std::string CgCheckSite(const std::string& text, const CgSite& site) {
    const std::vector<std::string> raw = CgLines(text);
    std::vector<std::string> lines;
    for (const std::string& l : raw) {
        lines.push_back(CgTrim(l));
    }
    size_t def = lines.size();
    for (size_t i = 0; i < raw.size(); i++) {
        if (raw[i].rfind(site.function, 0) == 0) {
            if (def != lines.size()) {
                return std::string("two definitions of ") + site.function;
            }
            def = i;
        }
    }
    if (def == lines.size()) {
        return std::string("no definition of ") + site.function;
    }
    // The body ends at the first closing brace in column 0.
    size_t end = def + 1;
    while (end < raw.size() && raw[end].rfind("}", 0) != 0) {
        end++;
    }
    size_t at = lines.size();
    for (size_t i = def; i + site.anchor.size() <= end; i++) {
        bool match = true;
        for (size_t k = 0; k < site.anchor.size() && match; k++) {
            match = lines[i + k] == site.anchor[k];
        }
        if (match) {
            if (at != lines.size()) {
                return "the upstream lines it follows appear twice in " + std::string(site.function);
            }
            at = i;
        }
    }
    if (at == lines.size()) {
        return "the upstream lines it follows (" + site.anchor.back() + ") are not in " + site.function;
    }
    size_t j = at + site.anchor.size();
    if (site.guarded.empty()) {
        return "";
    }
    if (j >= end || lines[j] != "#ifdef RSBS_SINGLE_EXECUTABLE") {
        return "no #ifdef RSBS_SINGLE_EXECUTABLE right after " + site.anchor.back();
    }
    j++;
    std::vector<std::string> body;
    while (j < end && lines[j] != "#endif") {
        if (!lines[j].empty() && !CgIsComment(lines[j])) {
            body.push_back(lines[j]);
        }
        j++;
    }
    if (j >= end) {
        return "the #ifdef RSBS_SINGLE_EXECUTABLE block is not closed inside " + std::string(site.function);
    }
    if (body != site.guarded) {
        return "the guarded block does not hold exactly " + site.guarded.front();
    }
    j++;
    for (const std::string& want : site.after) {
        if (j > end || lines[j] != want) {
            return "the guarded block is not followed by " + want;
        }
        j++;
    }
    return "";
}

/** "" when `text` (a test helper) replays exactly `anchor` and then `call`
 *  once, else what drifted. Comment and blank lines are skipped. */
std::string CgCheckReplica(const std::string& text, const std::vector<std::string>& anchor, const std::string& call) {
    std::vector<std::string> lines;
    for (const std::string& l : CgLines(text)) {
        const std::string t = CgTrim(l);
        if (!t.empty() && !CgIsComment(t)) {
            lines.push_back(t);
        }
    }
    std::vector<std::string> want = anchor;
    want.push_back(call);
    int found = 0;
    for (size_t i = 0; i + want.size() <= lines.size(); i++) {
        bool match = true;
        for (size_t k = 0; k < want.size() && match; k++) {
            match = lines[i + k] == want[k];
        }
        found += match ? 1 : 0;
    }
    if (found != 1) {
        return "the replica does not replay the actor's lines then " + call + " exactly once (found " +
               std::to_string(found) + ")";
    }
    return "";
}

/** Completion writes on the non-comment, non-declaration lines of `text`:
 *  lines holding `needle`; with `valueTrue`, only those assigning true or 1
 *  after it. */
int CgCountWrites(const std::string& text, const char* needle, bool valueTrue) {
    int n = 0;
    for (const std::string& l : CgLines(text)) {
        const std::string t = CgTrim(l);
        const size_t at = t.find(needle);
        // A declaration names the function; it does not call it.
        if (CgIsComment(t) || at == std::string::npos || t.rfind("void ", 0) == 0 || t.rfind("extern ", 0) == 0) {
            continue;
        }
        if (valueTrue) {
            const std::string value = t.substr(at + strlen(needle));
            if (value.rfind("true;", 0) != 0 && value.rfind("1;", 0) != 0) {
                continue;
            }
        }
        n++;
    }
    return n;
}

/** The counterfactual source: `text` without the first line equal (trimmed) to
 *  `line` that comes after the first line starting with `after`. */
std::string CgDropLine(const std::string& text, const std::string& after, const std::string& line) {
    const std::vector<std::string> raw = CgLines(text);
    bool seen = false;
    bool dropped = false;
    std::string out;
    for (size_t i = 0; i < raw.size(); i++) {
        const std::string t = CgTrim(raw[i]);
        if (!seen && t.rfind(after, 0) == 0) {
            seen = true;
        } else if (seen && !dropped && t == line) {
            dropped = true;
            continue;
        }
        out += raw[i];
        if (i + 1 < raw.size()) {
            out += "\n";
        }
    }
    return out;
}

/** The counterfactual source: `text` without the `#ifdef RSBS_SINGLE_EXECUTABLE`
 *  line that opens `site`'s own guarded block (the line right after its
 *  anchor), or `text` unchanged when the site cannot be located. Two sites can
 *  share one function (MM's Rando::GiveItem), so "the first #ifdef after the
 *  function" is not necessarily this site's. */
std::string CgDropSiteGuard(const std::string& text, const CgSite& site) {
    const std::vector<std::string> raw = CgLines(text);
    size_t def = raw.size();
    for (size_t i = 0; i < raw.size() && def == raw.size(); i++) {
        if (raw[i].rfind(site.function, 0) == 0) {
            def = i;
        }
    }
    size_t guard = raw.size();
    for (size_t i = def; i < raw.size() && guard == raw.size(); i++) {
        bool match = i + site.anchor.size() < raw.size();
        for (size_t k = 0; k < site.anchor.size() && match; k++) {
            match = CgTrim(raw[i + k]) == site.anchor[k];
        }
        if (match && CgTrim(raw[i + site.anchor.size()]) == "#ifdef RSBS_SINGLE_EXECUTABLE") {
            guard = i + site.anchor.size();
        }
    }
    if (guard == raw.size()) {
        return text;
    }
    std::string out;
    for (size_t i = 0; i < raw.size(); i++) {
        if (i == guard) {
            continue;
        }
        out += raw[i];
        if (i + 1 < raw.size()) {
            out += "\n";
        }
    }
    return out;
}

/** `text` with the first line equal (trimmed) to `line` replaced by `with`. */
std::string CgReplaceLine(const std::string& text, const std::string& line, const std::string& with) {
    const std::vector<std::string> raw = CgLines(text);
    bool done = false;
    std::string out;
    for (size_t i = 0; i < raw.size(); i++) {
        if (!done && CgTrim(raw[i]) == line) {
            out += with;
            done = true;
        } else {
            out += raw[i];
        }
        if (i + 1 < raw.size()) {
            out += "\n";
        }
    }
    return out;
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
    CG_ASSERT(OoT_ComboGoalTest_TimeSplitsComplete() == 1,
              "E3: unpaired, Time Splits' last split must mark OoT complete as upstream");
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

                // Time Splits' last split (Ganon can be it), the real function.
                const int splits = OoT_ComboGoalTest_TimeSplitsComplete();
                CG_ASSERT(splits == (met ? 1 : 0),
                          "E6: %s, %s defeated %s: Time Splits' completion %s OoT complete, expected it %s",
                          CgGoalName(goal), CgGameName(game), step == 0 ? "first" : "second",
                          splits ? "marked" : "did not mark", met ? "to" : "not to");

                // Then the ending warp: the port's redirect over a replica of its
                // site's assignments (E7 ties the replica and the call to the actor).
                const int site = game == GAME_OOT ? OoT_ComboGoalTest_EndingSite() : MM_ComboGoalTest_EndingSite();
                CG_ASSERT(site == (met ? 0 : 1),
                          "E5: %s, %s defeated %s: its redirect over the site's assignments %s, expected it to %s",
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
        printf("[TEST] E2/E5/E6: %s: the ending plays exactly when the goal becomes true, in both orders\n",
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

    // ---- E8: fail open on a goal that cannot be evaluated -------------------
    // A triforce-hunt world whose record fails its check (the hunt disarmed,
    // the requirement reads 0), and a goal byte outside the pinned table: each
    // final boss ends its own game rather than none ever ending the world.
    CgFreezeWorld(hunt);
    gComboCtx.comboTriforce.requiredOoT = 0; // hunt on with no requirement: a damaged record
    CG_ASSERT(Combo_GoalArmed() && !Combo_TriforceHuntArmed() && Combo_TriforceHuntRequired() == 0,
              "E8: the damaged triforce-hunt record did not disarm the hunt");
    CG_ASSERT(Combo_GoalAllowsCompletion(-1), "E8: an unevaluable goal must let a game mark itself complete");
    CG_ASSERT(Combo_GoalOnFinalBossDefeated(GAME_OOT, -1) == RSBS_GOAL_ENDING_OWN,
              "E8: triforce-hunt with a damaged record: Ganon must end OoT as its own game, not withhold forever");
    CG_ASSERT(Combo_GoalFinalBossRecorded(GAME_OOT), "E8: the defeat was not recorded");
    CG_ASSERT(OoT_ComboGoalTest_EndingSite() == 0 && MM_ComboGoalTest_EndingSite() == 0,
              "E8: triforce-hunt with a damaged record: an ending site withheld its ending");
    CgFreezeWorld((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH);
    gComboCtx.comboSettings.goal = 6; // outside the pinned table
    CG_ASSERT(Combo_GoalOnFinalBossDefeated(GAME_MM, -1) == RSBS_GOAL_ENDING_OWN,
              "E8: goal byte 6: Majora must end MM as its own game, not withhold forever");
    printf("[TEST] E8: an unevaluable frozen goal fails open: each boss ends its own game, logged\n");

    // ---- E9: a half's OWN triforce hunt under the frozen goal (#768) ---------
    ComboContext_Init();
    for (GameId game : kGames) {
        CG_ASSERT(Combo_GoalOnTriforceHuntCompleted(game, 3) == RSBS_GOAL_ENDING_OWN,
                  "E9: unpaired, %s's own triforce hunt must end its own game as upstream", CgGameName(game));
    }
    for (uint8_t goal : kGoals) {
        CgFreezeWorld(goal);
        for (GameId game : kGames) {
            if (goal == hunt) {
                CG_ASSERT(Combo_GoalOnTriforceHuntCompleted(game, (int)required) == RSBS_GOAL_ENDING_PLAY,
                          "E9: triforce-hunt, the reaching give in %s's game must end the paired game",
                          CgGameName(game));
                CG_ASSERT(Combo_GoalOnTriforceHuntCompleted(game, (int)required - 1) == RSBS_GOAL_ENDING_WITHHOLD,
                          "E9: triforce-hunt one piece short in %s's game must not end the paired game",
                          CgGameName(game));
            } else {
                const int ending = Combo_GoalOnTriforceHuntCompleted(game, 99);
                CG_ASSERT(ending == RSBS_GOAL_ENDING_WITHHOLD,
                          "E9: %s, %s's OWN triforce hunt decided '%s', expected 'withhold': a half's own hunt is no "
                          "term of a boss goal (#768)",
                          CgGoalName(goal), CgGameName(game), Combo_GoalEndingName(ending));
            }
        }
        CG_ASSERT(gComboCtx.sharedFlags[RSBS_SHARED_FLAGS_WORD_GOAL] == 0u,
                  "E9: %s, a hunt's completion recorded a final-boss defeat (sharedFlags word 0 = 0x%08x)",
                  CgGoalName(goal), (unsigned)gComboCtx.sharedFlags[RSBS_SHARED_FLAGS_WORD_GOAL]);
        CG_ASSERT(Combo_GoalKeepsOwnHuntWin(goal) == (goal == hunt),
                  "E9: %s: the creation's rule says a half's own hunt %s a win condition", CgGoalName(goal),
                  Combo_GoalKeepsOwnHuntWin(goal) ? "stays" : "does not stay");
    }
    CG_ASSERT(Combo_GoalKeepsOwnHuntWin(0) && Combo_GoalKeepsOwnHuntWin(6),
              "E9: a goal byte outside the pinned table must leave a half's own hunt as authored");
    CgFreezeWorld(hunt);
    gComboCtx.comboTriforce.requiredOoT = 0; // damaged: the hunt disarms, the requirement reads 0
    CG_ASSERT(Combo_GoalOnTriforceHuntCompleted(GAME_OOT, 3) == RSBS_GOAL_ENDING_OWN,
              "E9: triforce-hunt with a damaged record: OoT's own hunt must fail open to its own ending");
    CgFreezeWorld((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH);
    gComboCtx.comboSettings.goal = 6;
    CG_ASSERT(Combo_GoalOnTriforceHuntCompleted(GAME_MM, 3) == RSBS_GOAL_ENDING_OWN,
              "E9: goal byte 6: MM's own hunt must fail open to its own ending");
    printf("[TEST] E9: a half's own triforce hunt ends its game only unpaired; under a boss goal it ends nothing and "
           "records nothing; under triforce-hunt the shared count decides\n");

    // ---- E7: the wiring, from source ------------------------------------------
#ifndef RSBS_SOURCE_DIR
    CG_ASSERT(false, "E7: RSBS_SOURCE_DIR is undefined: the call sites cannot be checked");
#else
    {
        const char* kGanon2 = "games/oot/src/overlays/actors/ovl_Boss_Ganon2/z_boss_ganon2.c";
        const char* kBoss07 = "games/mm/src/overlays/actors/ovl_Boss_07/z_boss_07.c";
        const char* kStamps = "games/oot/soh/Enhancements/GameplayStats/BossDefeatTimestamps.cpp";
        const char* kSplits = "games/oot/soh/Enhancements/timesplits/TimeSplits.cpp";
        const char* kOoTGive = "games/oot/soh/Enhancements/randomizer/randomizer.cpp";
        const char* kMMGive = "games/mm/2s2h/Rando/GiveItem.cpp";
        // #768: the triforce piece's win arms. OoT's guarded block replaces
        // upstream's threshold and its "Win" test (the #else keeps upstream's
        // lines verbatim); MM's threshold is one block and its ending another.
        const std::string kOoTHuntEnds = "if (OoT_ComboGoal_TriforceHuntEnds(OTRGlobals::Instance->gRandomizer->"
                                         "GetRandoSettingValue(";
        const std::string kMMHuntEnds = "if (!MM_ComboGoal_TriforceHuntEnds()) {";
        const std::vector<std::string> kOoTArm = {
            "if (Combo_TriforceHuntOnPieceGiven(",
            "GAME_OOT, gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected,",
            "(uint16_t)(OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(",
            "RSK_TRIFORCE_HUNT_PIECES_REQUIRED) +",
            "1)) != RSBS_TRIFORCE_WIN_NONE) {",
            "Flags_SetRandomizerInf(RAND_INF_GRANT_GANONS_BOSSKEY);",
            kOoTHuntEnds,
            "RSK_TRIFORCE_HUNT) == RO_TRIFORCE_HUNT_WIN)) {",
            "#else",
            "if (gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected ==",
            "(OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_TRIFORCE_HUNT_PIECES_REQUIRED) + 1)) {",
            "Flags_SetRandomizerInf(RAND_INF_GRANT_GANONS_BOSSKEY);",
            "if (OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_TRIFORCE_HUNT) ==",
            "RO_TRIFORCE_HUNT_WIN) {",
        };
        const std::vector<std::string> kGanonWarp = {
            "play->nextEntranceIndex = ENTR_CHAMBER_OF_THE_SAGES_0;",
            "gSaveContext.nextCutsceneIndex = 0xFFF2;",
            "play->transitionTrigger = TRANS_TRIGGER_START;",
            "play->transitionType = TRANS_TYPE_FADE_WHITE;",
            "play->linkAgeOnLoad = 1;",
        };
        const std::vector<std::string> kMajoraWarp = {
            "play->nextEntrance = ENTRANCE(TERMINA_FIELD, 0);",
            "gSaveContext.nextCutsceneIndex = 0xFFF7;",
            "play->transitionTrigger = TRANS_TRIGGER_START;",
        };
        const std::string kGanonCall = "OoT_ComboGoal_RedirectEndingIfWithheld(play);";
        const std::string kMajoraCall = "MM_ComboGoal_RedirectEndingIfWithheld(play);";
        const std::vector<CgSite> sites = {
            { "BossGanon2's ending warp", kGanon2, "void func_8090120C(BossGanon2* this, PlayState* play) {",
              kGanonWarp, { kGanonCall }, { "break;" } },
            { "Majora's ending warp", kBoss07, "void Boss07_Wrath_DeathCutscene(Boss07* this, PlayState* play) {",
              kMajoraWarp, { kMajoraCall }, { "}" } },
            { "Majora's final blow", kBoss07, "void Boss07_Wrath_SetupDeathCutscene(Boss07* this, PlayState* play) {",
              { "GameInteractor_ExecuteOnBossDefeated(this->actor.id); // 2S2H Time Splits" },
              { "if (!MM_ComboGoal_OnMajoraDefeated()) {", "return;", "}" },
              { "GameInteractor_ExecuteOnGameCompletion();", "}" } },
            { "OoT's final-blow completion", kStamps, "static void MarkGameCompleteOnGanonDefeat() {",
              { "static void MarkGameCompleteOnGanonDefeat() {" },
              { "if (!OoT_ComboGoal_OnGanonDefeated()) {", "return;", "}" },
              { "gSaveContext.ship.stats.gameComplete = true;", "}" } },
            { "OoT's final-blow hook", kStamps, "static void RegisterBossDefeatTimestamps() {",
              { "COND_ID_HOOK(OnBossDefeat, ACTOR_BOSS_GANON2, true, [](void* refActor) { "
                "MarkGameCompleteOnGanonDefeat(); });" },
              {},
              {} },
            { "Time Splits' completion", kSplits, "void TimeSplitCompleteSplits() {",
              { "void TimeSplitCompleteSplits() {",
                "gSaveContext.ship.stats.itemTimestamp[TIMESTAMP_DEFEAT_GANON] = GAMEPLAYSTAT_TOTAL_TIME;" },
              { "if (!OoT_ComboGoal_GameMayComplete()) {", "return;", "}" },
              { "gSaveContext.ship.stats.gameComplete = true;", "}" } },
            { "OoT's triforce piece win arm", kOoTGive,
              "extern \"C\" u16 Randomizer_Item_Give(PlayState* play, GetItemEntry giEntry) {",
              { "gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected++;",
                "GameInteractor_SetTriforceHuntPieceGiven(true);", "" },
              kOoTArm,
              { "gSaveContext.ship.stats.itemTimestamp[TIMESTAMP_TRIFORCE_COMPLETED] =",
                "static_cast<u32>(GAMEPLAYSTAT_TOTAL_TIME);", "gSaveContext.ship.stats.gameComplete = 1;" } },
            { "MM's triforce piece threshold", kMMGive, "void Rando::GiveItem(RandoItemId randoItemId) {",
              { "gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces++;" },
              { "if (Combo_TriforceHuntOnPieceGiven(GAME_MM, gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces,",
                "(uint16_t)RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_REQUIRED]) !=", "RSBS_TRIFORCE_WIN_NONE) {", "#else",
                "if (gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces ==",
                "RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_REQUIRED]) {" },
              {} },
            { "MM's triforce piece win arm", kMMGive, "void Rando::GiveItem(RandoItemId randoItemId) {",
              { "if (!Flags_GetRandoInf(RANDO_INF_OBTAINED_SOUL_OF_BOSS_MAJORA)) {",
                "Rando::GiveItem(RI_SOUL_BOSS_MAJORA);", "}" },
              { kMMHuntEnds, "break;", "}" },
              { "GameInteractor_ExecuteOnGameCompletion();", "MM_GameEvents_Queue().emplace_back(" } },
        };
        int counterfactuals = 0;
        for (const CgSite& site : sites) {
            std::string text;
            CG_ASSERT(CgReadFile(site.file, &text), "E7: cannot read %s", site.file);
            const std::string why = CgCheckSite(text, site);
            CG_ASSERT(why.empty(), "E7: %s (%s) is not wired: %s", site.what, site.file, why.c_str());
            // Counterfactuals, observed here: the call gone, the guard gone.
            const std::string& key = site.guarded.empty() ? site.anchor.back() : site.guarded.front();
            const std::string noCall = CgDropLine(text, site.function, key);
            CG_ASSERT(noCall != text && !CgCheckSite(noCall, site).empty(),
                      "E7: %s: the check still passes with '%s' removed (a vacuous check)", site.what, key.c_str());
            counterfactuals++;
            if (!site.guarded.empty()) {
                const std::string noGuard = CgDropSiteGuard(text, site);
                CG_ASSERT(noGuard != text && !CgCheckSite(noGuard, site).empty(),
                          "E7: %s: the check still passes with its #ifdef RSBS_SINGLE_EXECUTABLE removed", site.what);
                counterfactuals++;
            }
        }
        // #768's red half, from source: each arm as main had it must fail.
        {
            std::string oot;
            CG_ASSERT(CgReadFile(kOoTGive, &oot), "E7: cannot read %s", kOoTGive);
            std::string mainOoT = CgReplaceLine(oot, kOoTHuntEnds, "if (Combo_TriforceHuntArmed() ||");
            mainOoT = CgReplaceLine(mainOoT, "RSK_TRIFORCE_HUNT) == RO_TRIFORCE_HUNT_WIN)) {",
                                    "OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_TRIFORCE_HUNT) == "
                                    "RO_TRIFORCE_HUNT_WIN) {");
            CG_ASSERT(mainOoT != oot && !CgCheckSite(mainOoT, sites[sites.size() - 3]).empty(),
                      "E7: OoT's triforce arm as main had it (the \"Win\" test not asking the goal) still passes");
            counterfactuals++;
            std::string mm;
            CG_ASSERT(CgReadFile(kMMGive, &mm), "E7: cannot read %s", kMMGive);
            const std::string mainMM = CgDropLine(mm, "void Rando::GiveItem(", kMMHuntEnds);
            CG_ASSERT(mainMM != mm && !CgCheckSite(mainMM, sites.back()).empty(),
                      "E7: MM's triforce arm without its goal block still passes");
            counterfactuals++;
        }

        // E5's replicas replay the actors' own lines (the anchors above, which
        // the site checks just matched in the actors).
        struct Replica {
            const char* file;
            const std::vector<std::string>* anchor;
            const std::string* call;
        };
        const Replica replicas[] = {
            { "games/oot/soh/oot_combo_goal_test.cpp", &kGanonWarp, &kGanonCall },
            { "games/mm/2s2h/mm_combo_goal_test.cpp", &kMajoraWarp, &kMajoraCall },
        };
        for (const Replica& r : replicas) {
            std::string text;
            CG_ASSERT(CgReadFile(r.file, &text), "E7: cannot read %s", r.file);
            const std::string why = CgCheckReplica(text, *r.anchor, *r.call);
            CG_ASSERT(why.empty(), "E7: %s: %s", r.file, why.c_str());
            // Counterfactual: the replica's first line drifts from the actor's.
            const std::string drifted = CgReplaceLine(text, r.anchor->front(), "play->transitionTrigger = 0;");
            CG_ASSERT(drifted != text && !CgCheckReplica(drifted, *r.anchor, *r.call).empty(),
                      "E7: %s: a drifted replica still passes", r.file);
            counterfactuals++;
        }

        // No completion writer this row does not name. OoT's `gameComplete`
        // freezes its stat timers; MM's OnGameCompletion stamps the file.
        struct Writer {
            const char* file;
            int count;
            const char* why;
        };
        const Writer kOoTWriters[] = {
            { kStamps, 1, "the final-blow completion, gated (E7)" },
            { kSplits, 1, "Time Splits' completion, gated (E7)" },
            { "games/oot/soh/Enhancements/randomizer/randomizer.cpp", 1,
              "the triforce piece's win arm, gated (E7): the combo hunt's requirement, or unpaired OoT's own "
              "\"Win\"; never a half's own hunt under a boss goal (#768)" },
            { "games/oot/soh/Enhancements/boss-rush/BossRush.cpp", 1, "Boss Rush mode, never a paired world" },
        };
        const Writer kMMWriters[] = {
            { kBoss07, 1, "Majora's final blow, gated (E7)" },
            { "games/mm/2s2h/Rando/GiveItem.cpp", 1,
              "the triforce piece's win arm, gated (E7): the combo hunt's requirement, or unpaired MM's own hunt; "
              "never a half's own hunt under a boss goal (#768)" },
            { "games/mm/2s2h/mm_hook_dispatch_test.cpp", 1, "a hook-dispatch test row" },
        };
        const char* kOoTNeedle = "stats.gameComplete = ";
        const char* kMMNeedle = "GameInteractor_ExecuteOnGameCompletion();";
        struct Sweep {
            const char* root;
            const char* needle;
            bool valueTrue;
            const Writer* writers;
            size_t count;
        };
        const Sweep sweeps[] = {
            { "games/oot/soh", kOoTNeedle, true, kOoTWriters, sizeof(kOoTWriters) / sizeof(kOoTWriters[0]) },
            { "games/oot/src", kOoTNeedle, true, kOoTWriters, sizeof(kOoTWriters) / sizeof(kOoTWriters[0]) },
            { "games/mm/2s2h", kMMNeedle, false, kMMWriters, sizeof(kMMWriters) / sizeof(kMMWriters[0]) },
            { "games/mm/src", kMMNeedle, false, kMMWriters, sizeof(kMMWriters) / sizeof(kMMWriters[0]) },
        };
        const std::filesystem::path root(RSBS_SOURCE_DIR);
        for (const Sweep& sw : sweeps) {
            std::error_code ec;
            int files = 0;
            for (auto it = std::filesystem::recursive_directory_iterator(root / sw.root, ec);
                 !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
                if (!it->is_regular_file()) {
                    continue;
                }
                const std::string ext = it->path().extension().string();
                if (ext != ".c" && ext != ".cpp" && ext != ".h" && ext != ".hpp" && ext != ".inc") {
                    continue;
                }
                std::ifstream in(it->path(), std::ios::binary);
                const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                files++;
                const int hits = CgCountWrites(text, sw.needle, sw.valueTrue);
                if (hits == 0) {
                    continue;
                }
                const std::string rel = std::filesystem::relative(it->path(), root).generic_string();
                const Writer* named = nullptr;
                for (size_t w = 0; w < sw.count; w++) {
                    if (rel == sw.writers[w].file) {
                        named = &sw.writers[w];
                    }
                }
                CG_ASSERT(named != nullptr,
                          "E7: %s marks its game complete (%d x '%s') and this row does not name it: gate it on the "
                          "combo goal (combo_goal.h) and name it here",
                          rel.c_str(), hits, sw.needle);
                CG_ASSERT(hits == named->count, "E7: %s has %d completion writes, the row names %d (%s)", rel.c_str(),
                          hits, named->count, named->why);
            }
            CG_ASSERT(!ec && files > 100, "E7: the sweep of %s read %d files (%s)", sw.root, files,
                      ec ? ec.message().c_str() : "too few");
        }
        // The sweep's own counterfactual: one more ungated writer is seen.
        std::string splitsText;
        CG_ASSERT(CgReadFile(kSplits, &splitsText), "E7: cannot read %s", kSplits);
        CG_ASSERT(CgCountWrites(splitsText + "\n    gSaveContext.ship.stats.gameComplete = true;\n", kOoTNeedle,
                                true) == CgCountWrites(splitsText, kOoTNeedle, true) + 1,
                  "E7: the sweep does not see an added gameComplete write");
        counterfactuals++;
        printf("[TEST] E7: all %d call sites are wired under RSBS_SINGLE_EXECUTABLE, both replicas replay their "
               "actors, no unnamed completion writer exists; %d counterfactuals each failed the check\n",
               (int)sites.size(), counterfactuals);
    }
#endif

    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] PASS: combo-goal-ending\n");
    return TEST_PASS;
}
