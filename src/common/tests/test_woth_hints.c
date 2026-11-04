/**
 * @file test_woth_hints.c
 * @brief WothHints: the pair-level Way of the Hero (#1439) and Majora's Mask's
 *        hints on by default, ROM-free (redship tier).
 *
 * OoTMM's largest default hint bucket is `woth` (9 hints): "They say that <region>
 * is on the Way of the Hero", over analysis.ts's required set. A paired world had
 * none: OoT's seam skips CalculateWotH and MM's stones had no such variant. #1439
 * computes the set over the PAIRED world (src/common/combo_woth.h), hints it on
 * both games' stones, and turns MM's hints on as OoTMM ships them.
 *
 * Legs, split by what each can prove without a ROM:
 *
 *   A. THE DEFAULTS. Five of MM's six hint rows default on (gossip stones, spider
 *      houses, Hookshot, boss remains, Oath to Order) and Purchaseable Gossip
 *      Hints stays off (OoTMM has no rupee-bought hints), read off the descriptor
 *      table the options page draws, whose defaults are StaticData's own.
 *
 *   B. THE SET, over the coordinator's two stub engines (test_combo_logic.c's
 *      fixture, its "forbidden world") with one more placement nothing needs:
 *        OoT host 10 holds MM's Ocarina  (MM's host 21 needs it)
 *        OoT host 11 holds OoT's Lens    (OoT's goal)
 *        OoT host 12 holds OoT's Hook    (nothing needs it)
 *        MM  host 20 holds OoT's Sword   (OoT's host 11 needs it)
 *        MM  host 21 holds MM's Remains  (MM's goal)
 *      Under each paired goal the analysis must name exactly the hosts that goal
 *      cannot do without, WHICHEVER game hosts them and whichever game's item they
 *      hold: beat-both {10, 20, 11, 21}; beat-OoT {20, 11} (an MM host is on OoT's
 *      path); beat-MM {10, 21} (an OoT host is on MM's path); beat-either none
 *      (every host has the other half to fall back on). One round per candidate
 *      plus the baseline, the candidates interleaved OoT, MM, OoT, ...; no
 *      placement moves; the round cap stops it early and only drops hosts; a
 *      candidate filter is honoured; a goal the baseline cannot prove claims
 *      nothing.
 *
 *   C. THE RECORD MM's stones read: the set's areas (a stub area map), the
 *      PRESENT tag, both halves' bits, and nothing for a special or a stray area.
 *      Then the creation's own call: under No Logic (a frozen `none` rung) it runs
 *      no round and leaves the record ABSENT; under a proving rung it leaves it
 *      PRESENT.
 *
 *   D. A CREATION RECORD: rolled back, retracted, KEPT through the file-create
 *      invalidation, dropped by every other, and carried by a seed package's
 *      world record (OoT_SeedPackage_TestReapplyWorldRecord).
 *
 *   E. MM'S STONES, cross-game wording: with a frozen record holding an OoT area
 *      (Kokiri Forest) and an MM area (the Southern Swamp), over 512 stone seeds
 *      some free stones say exactly "They say %yKokiri Forest%w is on the %yWay of
 *      the Hero%w." or the Southern Swamp's sentence, both are said, about one in
 *      three speaks it, the same seed says the same thing twice, the free stone's
 *      whole text (MM_Rando_Hints_TestFreeStoneText) is that sentence or its item
 *      hint, and MM's global stream never moves. With the record ABSENT, or under
 *      No Logic, no stone says it.
 *
 *   F. OoT'S STONES, cross-game wording: the hint OoT's WotH entry places for an
 *      MM area (OoT_HintArea_ForeignWothHint) reads "They say that the Southern
 *      Swamp is on the way of the hero." and persists the area by its name.
 *
 *   G. THE WIRING, from source (RSBS_SOURCE_DIR), for what needs a generated world:
 *      the single-bag fill runs the analysis after its probe, OoT's remainder
 *      flags its WotH candidates before CreateAllHints, OoT's WotH entry draws the
 *      MM areas, CreateStoneHints loads them, MM's free stone consults the WotH
 *      variant after the foolish one, and both claim sites ask
 *      Combo_ComboLogicClaimsAllowed. Each check has a counterfactual that must
 *      fail it. The generated half (the flags, the record and the hints of a real
 *      paired world) is the rando tier's ComboSingleBag D2, through
 *      Rando_ValidatePairedWorldHints's P8.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * AFTER test_combo_logic.c, whose stub engines it drives.
 */

#include "../combo_area.h"
#include "../combo_logic.h"
#include "../combo_mm_options_view.h"
#include "../combo_woth.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../game.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" {
// MM (HintWothSingleExe.cpp, EnGs.cpp's bridge block)
int MM_HintWoth_TestFreeStoneText(uint32_t stoneSeed, char* out, int cap);
int MM_Rando_Hints_TestFreeStoneText(uint16_t randoCheckId, uint32_t stoneSeed, char* out, int cap);
int MM_Rando_Hints_TestFreeStoneKeepsStream(uint16_t randoCheckId, uint32_t stoneSeed, uint64_t streamSeed);
int MM_Rando_Hints_TestGossipText(uint16_t randoCheckId, int purchased, char* out, int cap);
int MM_HintArea_TestCheckRow(uint16_t id, int* outIsEnemyDrop);
int MM_HintArea_TestCheckIdMax(void);
// OoT (HintAreaOoT.cpp, SeedPackageSingleExe.cpp)
int OoT_HintArea_TestForeignWothText(uint8_t mmArea, char* out, int cap, char* outForeignName, int foreignCap);
void OoT_SeedPackage_TestReapplyWorldRecord(void);
}

#define WH_ASSERT(cond, msg)                                                \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

// ---- A. the defaults --------------------------------------------------------

bool WhCheckDefaults() {
    struct Want {
        const char* name;
        int32_t value;
    };
    const Want wants[] = {
        { "RO_HINTS_GOSSIP_STONES", 1 }, { "RO_HINTS_SPIDER_HOUSES", 1 }, { "RO_HINTS_HOOKSHOT", 1 },
        { "RO_HINTS_BOSS_REMAINS", 1 },  { "RO_HINTS_OATH_TO_ORDER", 1 }, { "RO_HINTS_PURCHASEABLE", 0 },
    };
    WH_ASSERT(Combo_MMOptionCount() > 0, "A: MM's option table did not register, so no default can be read");
    for (const Want& w : wants) {
        const ComboMMOptionDesc* found = nullptr;
        for (int i = 0; i < Combo_MMOptionCount() && found == nullptr; i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            if (d != nullptr && d->name != nullptr && strcmp(d->name, w.name) == 0) {
                found = d;
            }
        }
        printf("[TEST]   A: %s defaults to %d (want %d)\n", w.name, found != nullptr ? (int)found->defaultValue : -1,
               (int)w.value);
        WH_ASSERT(found != nullptr, "A: a hint option has no descriptor row");
        WH_ASSERT(found->defaultValue == w.value,
                  "A: an MM hint option's default is not OoTMM's (hints on; no rupee-bought hints)");
    }
    return true;
}

// ---- B. the set, over the stub engines -------------------------------------

/** The required set as {game, check} pairs, in the order it was found. */
std::vector<std::pair<uint8_t, uint16_t>> WhSet() {
    std::vector<std::pair<uint8_t, uint16_t>> out;
    ComboLogicHostRef h;
    for (int i = 0; Combo_Woth_At(i, &h); i++) {
        out.push_back({ h.hostGame, h.hostCheck });
    }
    return out;
}

std::string WhSetText(const std::vector<std::pair<uint8_t, uint16_t>>& set) {
    std::string s = "{";
    for (size_t i = 0; i < set.size(); i++) {
        s += (i == 0 ? "" : ", ");
        s += set[i].first == (uint8_t)GAME_OOT ? "OoT " : "MM ";
        s += std::to_string(set[i].second);
    }
    return s + "}";
}

bool WhNoMM(uint8_t hostGame, const ComboLogicPlacement*) {
    return hostGame != (uint8_t)GAME_MM;
}

bool WhCheckSet() {
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;
    ClBuildForbiddenWorld();
    WH_ASSERT(ClPlaceForbiddenWorld(), "B: the four authored placements were refused");
    WH_ASSERT(Combo_Logic_Place(GAME_OOT, 12, ClItem(O, kOotHook), RSBS_ITEMCLASS_PROGRESSION),
              "B: the Hook on OoT host 12 was refused");
    const uint32_t digestBefore = Combo_Logic_PlacementDigest();

    struct Case {
        uint8_t goal;
        const char* name;
        std::vector<std::pair<uint8_t, uint16_t>> want;
    };
    const Case cases[] = {
        { (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, "beat-both", { { O, 10 }, { M, 20 }, { O, 11 }, { M, 21 } } },
        { (uint8_t)RSBS_COMBO_GOAL_BEAT_OOT, "beat-OoT", { { M, 20 }, { O, 11 } } },
        { (uint8_t)RSBS_COMBO_GOAL_BEAT_MM, "beat-MM", { { O, 10 }, { M, 21 } } },
        { (uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER, "beat-either", {} },
    };
    for (const Case& c : cases) {
        ComboWothReport r;
        const int rc = Combo_Woth_Analyze(c.goal, 0, RSBS_COMBO_WOTH_ROUND_CAP, nullptr, nullptr, nullptr, &r);
        const auto got = WhSet();
        printf("[TEST]   B: %s: status %d, %d placement(s), %d candidate(s), %d tested, %d round(s); required %s "
               "(want %s)\n",
               c.name, rc, r.placements, r.candidates, r.tested, r.rounds, WhSetText(got).c_str(),
               WhSetText(c.want).c_str());
        WH_ASSERT(rc == RSBS_COMBO_WOTH_OK && r.status == RSBS_COMBO_WOTH_OK, "B: the analysis did not run clean");
        WH_ASSERT(r.placements == 5 && r.candidates == 5 && r.tested == 5 && r.rounds == 6 && !r.capped,
                  "B: the analysis is not one round per placement plus the baseline");
        WH_ASSERT(got == c.want, "B: the Way of the Hero is not exactly the hosts the paired goal cannot do without");
        WH_ASSERT(r.required == (int)c.want.size() && Combo_Woth_Count() == (int)c.want.size(),
                  "B: the report's count and the resident set disagree");
        for (const auto& h : c.want) {
            WH_ASSERT(Combo_Woth_IsRequired(h.first, h.second), "B: IsRequired misses a host of the set");
        }
        WH_ASSERT(!Combo_Woth_IsRequired(O, 12) && !Combo_Woth_IsRequired(M, 12),
                  "B: the host nothing needs (or another game's id) reads as required");
    }
    WH_ASSERT(Combo_Logic_PlacementDigest() == digestBefore, "B: the analysis moved a placement");

    // The cap: the baseline and two candidates (OoT 10, then MM 20), and no more.
    {
        ComboWothReport r;
        (void)Combo_Woth_Analyze((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, 0, 3, nullptr, nullptr, nullptr, &r);
        const std::vector<std::pair<uint8_t, uint16_t>> want = { { O, 10 }, { M, 20 } };
        printf("[TEST]   B: capped at 3 rounds: %d tested, capped %d, required %s\n", r.tested, r.capped ? 1 : 0,
               WhSetText(WhSet()).c_str());
        WH_ASSERT(r.capped && r.tested == 2 && r.rounds == 3 && WhSet() == want,
                  "B: the round cap did not stop after the first two candidates in interleaved order");
    }
    // A candidate filter: MM's hosts left out are never tested, never required.
    {
        ComboWothReport r;
        (void)Combo_Woth_Analyze((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, 0, RSBS_COMBO_WOTH_ROUND_CAP, WhNoMM, nullptr,
                                 nullptr, &r);
        const std::vector<std::pair<uint8_t, uint16_t>> want = { { O, 10 }, { O, 11 } };
        WH_ASSERT(r.candidates == 3 && r.tested == 3 && WhSet() == want, "B: the candidate filter was not honoured");
    }
    // A goal the baseline cannot prove claims nothing (triforce hunt with no requirement has no evaluator).
    {
        ComboWothReport r;
        const int rc = Combo_Woth_Analyze((uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT, 0, RSBS_COMBO_WOTH_ROUND_CAP, nullptr,
                                          nullptr, nullptr, &r);
        WH_ASSERT(rc == RSBS_COMBO_WOTH_NO_PROOF && r.rounds == 1 && Combo_Woth_Count() == 0,
                  "B: an unproved baseline still named hosts on the Way of the Hero");
    }
    WH_ASSERT(ClContractClean(), "B: an analysis round broke the engine contract");
    return true;
}

// ---- C. the record ---------------------------------------------------------

ComboArea WhStubArea(uint8_t hostGame, uint16_t hostCheck) {
    ComboArea a = { hostGame, (uint8_t)COMBO_AREA_NONE };
    if (hostGame == (uint8_t)GAME_OOT) {
        a.area = hostCheck == 10   ? (uint8_t)COMBO_AREA_OOT_KOKIRI_FOREST
                 : hostCheck == 11 ? (uint8_t)COMBO_AREA_OOT_DEKU_TREE
                 : hostCheck == 12 ? (uint8_t)COMBO_AREA_OOT_HYRULE_FIELD
                                   : (uint8_t)COMBO_AREA_NONE;
    } else if (hostGame == (uint8_t)GAME_MM) {
        a.area = hostCheck == 20   ? (uint8_t)COMBO_AREA_MM_SOUTHERN_SWAMP
                 : hostCheck == 21 ? (uint8_t)COMBO_AREA_MM_WOODFALL_TEMPLE
                                   : (uint8_t)COMBO_AREA_NONE;
    }
    return a;
}

bool WhCheckRecord() {
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;
    // The set is beat-both's (B's last full run under that goal is redone here).
    (void)Combo_Woth_Analyze((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, 0, RSBS_COMBO_WOTH_ROUND_CAP, nullptr, nullptr,
                             nullptr, nullptr);
    ComboWothRecord rec;
    const int areas = Combo_Woth_BuildRecord(&rec, WhStubArea);
    uint8_t oot[8] = {};
    uint8_t mm[8] = {};
    const int ootN = Combo_WothRecord_Areas(&rec, O, oot, 8);
    const int mmN = Combo_WothRecord_Areas(&rec, M, mm, 8);
    printf("[TEST]   C: record of the beat-both set: %d area(s), OoT %d [%u %u], MM %d [%u %u]\n", areas, ootN,
           (unsigned)oot[0], (unsigned)oot[1], mmN, (unsigned)mm[0], (unsigned)mm[1]);
    WH_ASSERT(Combo_WothRecord_Present(&rec) && areas == 4, "C: the record does not hold the set's four areas");
    WH_ASSERT(ootN == 2 && oot[0] == COMBO_AREA_OOT_KOKIRI_FOREST && oot[1] == COMBO_AREA_OOT_DEKU_TREE,
              "C: the OoT half is not {Kokiri Forest, Deku Tree}, ascending");
    WH_ASSERT(mmN == 2 && mm[0] == COMBO_AREA_MM_WOODFALL_TEMPLE && mm[1] == COMBO_AREA_MM_SOUTHERN_SWAMP,
              "C: the MM half is not {Woodfall Temple, the Southern Swamp}, ascending");
    WH_ASSERT(!Combo_WothRecord_Has(&rec, WhStubArea(O, 12)), "C: the area of the host nothing needs is on the path");
    // Area 24 is the Deku Tree in OoT's half and Milk Road in MM's: only the first is on the path.
    WH_ASSERT(Combo_WothRecord_Has(&rec, ComboArea{ O, (uint8_t)COMBO_AREA_OOT_DEKU_TREE }) &&
                  !Combo_WothRecord_Has(&rec, ComboArea{ M, (uint8_t)COMBO_AREA_OOT_DEKU_TREE }),
              "C: a half's bit answers for the other half");

    ComboWothRecord edge;
    Combo_WothRecord_Clear(&edge);
    WH_ASSERT(!Combo_WothRecord_Present(&edge) && Combo_WothRecord_Count(&edge, O) == 0,
              "C: an all-zero record does not read ABSENT");
    WH_ASSERT(Combo_WothRecord_Add(&edge, ComboArea{ O, (uint8_t)COMBO_AREA_OOT_GANONS_CASTLE }) &&
                  Combo_WothRecord_Add(&edge, ComboArea{ M, (uint8_t)COMBO_AREA_MM_GORON_RACETRACK }),
              "C: the last area of a half has no bit");
    WH_ASSERT(!Combo_WothRecord_Has(&edge, ComboArea{ O, (uint8_t)COMBO_AREA_OOT_GANONS_CASTLE }),
              "C: an ABSENT record (no PRESENT tag) still answers");
    Combo_WothRecord_MarkPresent(&edge);
    WH_ASSERT(Combo_WothRecord_Has(&edge, ComboArea{ O, (uint8_t)COMBO_AREA_OOT_GANONS_CASTLE }) &&
                  Combo_WothRecord_Has(&edge, ComboArea{ M, (uint8_t)COMBO_AREA_MM_GORON_RACETRACK }),
              "C: the last area of a half does not read back");
    WH_ASSERT(!Combo_WothRecord_Add(&edge, ComboArea{ O, (uint8_t)COMBO_AREA_NONE }) &&
                  !Combo_WothRecord_Add(&edge, ComboArea{ O, (uint8_t)COMBO_AREA_POCKET }) &&
                  !Combo_WothRecord_Add(&edge, ComboArea{ O, (uint8_t)COMBO_AREA_OOT_END }) &&
                  !Combo_WothRecord_Add(&edge, ComboArea{ M, (uint8_t)COMBO_AREA_MM_END }) &&
                  !Combo_WothRecord_Add(&edge, ComboArea{ (uint8_t)GAME_NONE, 1 }),
              "C: a special, a stray area or a non-game took a bit");
    WH_ASSERT(Combo_WothRecord_Count(&edge, O) == 1 && Combo_WothRecord_Count(&edge, M) == 1,
              "C: a refused area changed the record");

    // The creation's own call: No Logic runs nothing and claims nothing.
    ComboSettingsRecord settings;
    Combo_ComboSettingsDefaults(&settings);
    settings.logicRung = (uint8_t)RSBS_COMBO_RUNG_NONE;
    Combo_FreezeComboSettings(&settings);
    gComboCtx.comboWoth = rec;
    const int noLogic = Combo_Woth_RunAtCreation((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, 0, nullptr, nullptr);
    printf("[TEST]   C: the creation's call under No Logic: status %d, %d round(s), record %s\n", noLogic,
           Combo_Woth_LastReport()->rounds, Combo_WothRecord_Present(&gComboCtx.comboWoth) ? "PRESENT" : "ABSENT");
    WH_ASSERT(noLogic == RSBS_COMBO_WOTH_NO_CLAIMS && Combo_Woth_LastReport()->rounds == 0 &&
                  !Combo_WothRecord_Present(&gComboCtx.comboWoth) && Combo_Woth_Count() == 0,
              "C: under No Logic the creation still analysed, or left a Way of the Hero behind");
    settings.logicRung = (uint8_t)RSBS_COMBO_RUNG_BEATABLE;
    Combo_FreezeComboSettings(&settings);
    const int proving = Combo_Woth_RunAtCreation((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, 0, nullptr, nullptr);
    printf("[TEST]   C: the creation's call under a proving rung: status %d, %d round(s), record %s\n", proving,
           Combo_Woth_LastReport()->rounds, Combo_WothRecord_Present(&gComboCtx.comboWoth) ? "PRESENT" : "ABSENT");
    WH_ASSERT(proving == RSBS_COMBO_WOTH_OK && Combo_Woth_LastReport()->rounds >= 1 &&
                  Combo_WothRecord_Present(&gComboCtx.comboWoth),
              "C: under a proving rung the creation did not write a PRESENT record");
    return true;
}

// ---- D. a creation record --------------------------------------------------

ComboWothRecord WhSampleRecord() {
    ComboWothRecord r;
    Combo_WothRecord_Clear(&r);
    Combo_WothRecord_MarkPresent(&r);
    (void)Combo_WothRecord_Add(&r, ComboArea{ (uint8_t)GAME_OOT, (uint8_t)COMBO_AREA_OOT_KOKIRI_FOREST });
    (void)Combo_WothRecord_Add(&r, ComboArea{ (uint8_t)GAME_MM, (uint8_t)COMBO_AREA_MM_SOUTHERN_SWAMP });
    return r;
}

bool WhSame(const ComboWothRecord& a, const ComboWothRecord& b) {
    return memcmp(&a, &b, sizeof(a)) == 0;
}

bool WhCheckCreationRecord() {
    const ComboWothRecord sample = WhSampleRecord();
    ComboWothRecord zero;
    Combo_WothRecord_Clear(&zero);

    ComboContext_Init();
    gComboCtx.comboWoth = sample;
    ComboCreationRecords prior;
    Combo_SnapshotCreationRecords(&prior);
    Combo_WothRecord_Clear(&gComboCtx.comboWoth);
    Combo_RestoreCreationRecords(&prior);
    const bool restored = WhSame(gComboCtx.comboWoth, sample);
    Combo_RetractCreationRecords();
    const bool retracted = WhSame(gComboCtx.comboWoth, zero);

    ComboContext_Init();
    gComboCtx.comboWoth = sample;
    Context_InvalidateSessionState(RSBS_SEED_STAMP_KEEP);
    const bool kept = WhSame(gComboCtx.comboWoth, sample);
    Context_InvalidateSessionState(RSBS_SEED_STAMP_DROP);
    const bool dropped = WhSame(gComboCtx.comboWoth, zero);

    ComboContext_Init();
    gComboCtx.comboWoth = sample;
    OoT_SeedPackage_TestReapplyWorldRecord();
    const bool packaged = WhSame(gComboCtx.comboWoth, sample);
    printf("[TEST]   D: the record: rollback-restored %d, retracted %d, file-create KEEP %d, dropped %d, through a "
           "seed package's world record %d (want 1 1 1 1 1)\n",
           restored ? 1 : 0, retracted ? 1 : 0, kept ? 1 : 0, dropped ? 1 : 0, packaged ? 1 : 0);
    WH_ASSERT(restored, "D: a failed generation's rollback does not restore the Way of the Hero");
    WH_ASSERT(retracted, "D: a retracted creation leaves its Way of the Hero behind");
    WH_ASSERT(kept, "D: the file-create invalidation drops the Way of the Hero the creation just wrote");
    WH_ASSERT(dropped, "D: a session drop keeps a dead world's Way of the Hero");
    WH_ASSERT(packaged, "D: a seed package's world record does not carry the Way of the Hero");
    ComboContext_Init();
    return true;
}

// ---- E. MM's stones --------------------------------------------------------

const char* const kWhMmKokiri = "They say %yKokiri Forest%w is on the %yWay of the Hero%w.";
const char* const kWhMmSwamp = "They say %ythe Southern Swamp%w is on the %yWay of the Hero%w.";

/** Free stone texts over 512 seeds: how many speak the WotH variant, and of which area. */
struct WhStoneTally {
    int woth = 0;
    int kokiri = 0;
    int swamp = 0;
    int other = 0;
};

bool WhTallyStones(WhStoneTally* t) {
    for (uint32_t seed = 0; seed < 512; seed++) {
        char text[512];
        char again[512];
        const int n = MM_HintWoth_TestFreeStoneText(seed, text, (int)sizeof(text));
        WH_ASSERT(n >= 0, "E: the WotH bridge refused its buffer");
        (void)MM_HintWoth_TestFreeStoneText(seed, again, (int)sizeof(again));
        WH_ASSERT(strcmp(text, again) == 0, "E: the same stone seed said two different things");
        if (n == 0) {
            continue;
        }
        t->woth++;
        if (strcmp(text, kWhMmKokiri) == 0) {
            t->kokiri++;
        } else if (strcmp(text, kWhMmSwamp) == 0) {
            t->swamp++;
        } else {
            t->other++;
            printf("[TEST]   E: unexpected stone text \"%s\"\n", text);
        }
    }
    return true;
}

bool WhCheckMmStones() {
    ComboContext_Init();
    WH_ASSERT(Combo_ComboLogicClaimsAllowed(), "E: with no frozen combo record the logic claims are refused");

    // ABSENT: no stone says it.
    {
        WhStoneTally t;
        if (!WhTallyStones(&t)) {
            return false;
        }
        WH_ASSERT(t.woth == 0, "E: a world with no frozen Way of the Hero still speaks of one");
    }

    gComboCtx.comboWoth = WhSampleRecord();
    WhStoneTally t;
    if (!WhTallyStones(&t)) {
        return false;
    }
    printf("[TEST]   E: 512 free stones with {Kokiri Forest, the Southern Swamp} frozen: %d WotH (%d Kokiri Forest, "
           "%d the Southern Swamp, %d other)\n",
           t.woth, t.kokiri, t.swamp, t.other);
    WH_ASSERT(t.other == 0, "E: a stone's WotH text is not exactly OoTMM's sentence in MM's voice");
    WH_ASSERT(t.kokiri > 0 && t.swamp > 0, "E: the stones never name one of the two games' areas");
    WH_ASSERT(t.woth >= 512 * 20 / 100 && t.woth <= 512 * 47 / 100,
              "E: the WotH variant is not spoken by about one free stone in three");

    // The free stone's whole text: its foolish variant first (if MM's live save
    // makes any area foolish), else its WotH sentence, else its item hint; and
    // working it out never moves MM's global stream.
    uint16_t check = 0;
    for (int id = 1; id < MM_HintArea_TestCheckIdMax() && check == 0; id++) {
        if (MM_HintArea_TestCheckRow((uint16_t)id, NULL)) {
            check = (uint16_t)id;
        }
    }
    char itemText[512];
    WH_ASSERT(check != 0 && MM_Rando_Hints_TestGossipText(check, 0, itemText, (int)sizeof(itemText)) > 0,
              "E: no MM check has an item hint to compare with");
    int wothSaid = 0;
    for (uint32_t seed = 0; seed < 512; seed++) {
        char woth[512];
        char full[512];
        const int n = MM_HintWoth_TestFreeStoneText(seed, woth, (int)sizeof(woth));
        WH_ASSERT(MM_Rando_Hints_TestFreeStoneText(check, seed, full, (int)sizeof(full)) > 0,
                  "E: the free stone said nothing");
        if (strstr(full, "foolish choice") == nullptr) {
            WH_ASSERT(strcmp(full, n > 0 ? woth : itemText) == 0,
                      "E: the free stone's text is neither its WotH sentence nor its item hint");
            wothSaid += n > 0 ? 1 : 0;
        }
        WH_ASSERT(MM_Rando_Hints_TestFreeStoneKeepsStream(check, seed, 0x1234567800000000ull + seed) == 1,
                  "E: working out the WotH variant moved MM's global Ship_Random stream");
    }
    printf("[TEST]   E: the free stone's own text (check %u) says the WotH sentence for %d of 512 seeds\n",
           (unsigned)check, wothSaid);
    WH_ASSERT(wothSaid > 0, "E: the free stone's hook text never carries the WotH sentence");

    // No Logic: nothing was proved, so no stone claims a Way of the Hero.
    ComboSettingsRecord settings;
    Combo_ComboSettingsDefaults(&settings);
    settings.logicRung = (uint8_t)RSBS_COMBO_RUNG_NONE;
    Combo_FreezeComboSettings(&settings);
    gComboCtx.comboWoth = WhSampleRecord();
    {
        WhStoneTally none;
        if (!WhTallyStones(&none)) {
            return false;
        }
        printf("[TEST]   E: frozen at rung none: %d WotH stone(s) (want 0)\n", none.woth);
        WH_ASSERT(none.woth == 0, "E: under No Logic a stone still claims a Way of the Hero");
    }
    ComboContext_Init();
    return true;
}

// ---- F. OoT's stones -------------------------------------------------------

bool WhCheckOoTStones() {
    char text[512];
    char foreign[128];
    const int n = OoT_HintArea_TestForeignWothText((uint8_t)COMBO_AREA_MM_SOUTHERN_SWAMP, text, (int)sizeof(text),
                                                   foreign, (int)sizeof(foreign));
    printf("[TEST]   F: OoT's stone for the Southern Swamp: \"%s\" (foreignAreas \"%s\")\n", n >= 0 ? text : "",
           n >= 0 ? foreign : "");
    WH_ASSERT(n > 0, "F: OoT's foreign WotH hint rendered nothing");
    const std::string said(text);
    WH_ASSERT(said.rfind("They say that the Southern Swamp is on", 0) == 0 &&
                  said.find("way of the hero") != std::string::npos && said.find("Termina") == std::string::npos,
              "F: OoT's stone does not name the MM area in OoT's own WotH sentence (\"They say that <area> is on "
              "the way of the hero.\")");
    WH_ASSERT(strcmp(foreign, "the Southern Swamp") == 0, "F: the hint does not persist the MM area by its name");
    return true;
}

// ---- G. the wiring, from source --------------------------------------------

std::vector<std::string> WhLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string cur;
    for (const char c : text) {
        if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur += c;
        }
    }
    lines.push_back(cur);
    return lines;
}

std::string WhTrim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) {
        return "";
    }
    const size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

/** One wiring fact: inside the block of `file` that starts with a line starting
 *  `head` and ends at the first later line exactly `close`, the trimmed line
 *  `first` and then a line starting `then` (when non-NULL) appear, in that order.
 *  `mutate` is the #1439 line whose removal must break the fact. */
struct WhSite {
    const char* file;
    const char* head;
    const char* close;
    const char* first;
    const char* then;
    const char* mutate;
    const char* what;
};

const char* const kWhRunAtCreation =
    "(void)Combo_Woth_RunAtCreation(sReport.goal, freq.triforceRequired, SingleBagWothProgress, &watch);";
const char* const kWhForeignDraw =
    "if (dist.type == HINT_TYPE_WOTH && RsbsPlaceForeignWothHint(hintPool.size(), dist.copies, dist.name)) {";
const char* const kWhLoadForeign =
    "RsbsLoadForeignWothAreas(); // #1439: empty unless OoT's paired remainder is hinting";
const char* const kWhMmWoth = "const std::string woth = Rando::HintWoth::FreeStoneText(stoneSeed);";
const char* const kWhClaimGate = "if (!Combo_ComboLogicClaimsAllowed()) {";
const char* const kWhOoTWindow =
    "return OoT_ComboLogic_HintingPairedRemainder() != 0 && Combo_ComboLogicClaimsAllowed();";

const WhSite kWhSites[] = {
    { "src/common/combo_single_bag.c", "int Combo_SingleBag_Run(", "}", "sPostFillProbe(sPostFillProbeCtx);",
      kWhRunAtCreation, kWhRunAtCreation,
      "the single-bag fill runs the Way of the Hero analysis once the world is proved" },
    { "games/oot/soh/Enhancements/randomizer/3drando/fill.cpp", "int RsbsFinishPairedGeneralPass(", "}",
      "OoT_HintArea_MarkPairedWoth();", "CreateAllHints();", "OoT_HintArea_MarkPairedWoth();",
      "OoT's remainder flags its WotH candidates before it draws its hints" },
    { "games/oot/soh/Enhancements/randomizer/3drando/hints.cpp", "static void DistributeAndPlaceHints(", "}",
      kWhForeignDraw, "RandomizerCheck hintedLocation = CreateRandomHint(", kWhForeignDraw,
      "OoT's WotH entry draws the MM areas beside its own candidates" },
    { "games/oot/soh/Enhancements/randomizer/3drando/hints.cpp", "void CreateStoneHints() {", "}", kWhLoadForeign,
      "DistributeAndPlaceHints(distTable, totalStones);", kWhLoadForeign,
      "OoT's stone pass loads the MM areas before it draws" },
    { "games/mm/2s2h/Rando/ActorBehavior/EnGs.cpp",
      "static std::string FreeStoneHintTextSeeded(const GossipHintTarget& target, uint32_t stoneSeed) {", "}",
      "const std::string foolish = Rando::HintFoolish::FreeStoneText(stoneSeed);", kWhMmWoth, kWhMmWoth,
      "MM's free stone consults the WotH variant after the foolish one" },
    { "games/mm/2s2h/Rando/HintWothSingleExe.cpp", "std::string Rando::HintWoth::FreeStoneText(uint32_t stoneSeed) {",
      "}", kWhClaimGate, "return \"\";", kWhClaimGate, "MM's WotH stone asks the No Logic gate first" },
    { "games/oot/soh/Enhancements/randomizer/HintAreaOoT.cpp", "static bool OoTWothWindowOpen() {", "}", kWhOoTWindow,
      nullptr, kWhOoTWindow,
      "OoT speaks of the Way of the Hero only in its remainder and only when the rung proved something" },
};

/** "" when `text` (the file of `site`) holds the fact, else what is wrong. */
std::string WhSiteProblem(const WhSite& site, const std::string& text) {
    const std::vector<std::string> lines = WhLines(text);
    for (size_t i = 0; i < lines.size(); i++) {
        if (WhTrim(lines[i]).rfind(site.head, 0) != 0) {
            continue;
        }
        size_t firstAt = 0;
        for (size_t j = i + 1; j < lines.size(); j++) {
            if (lines[j] == site.close) {
                break;
            }
            const std::string l = WhTrim(lines[j]);
            if (firstAt == 0 && l == site.first) {
                firstAt = j;
                if (site.then == nullptr) {
                    return "";
                }
            } else if (firstAt != 0 && site.then != nullptr && l.rfind(site.then, 0) == 0) {
                return "";
            }
        }
        return std::string("\"") + site.first + "\"" +
               (site.then != nullptr ? " then \"" + std::string(site.then) + "\"" : "") + " not found in order in " +
               site.head;
    }
    return std::string("\"") + site.head + "\" was not found";
}

bool WhCheckWiring() {
#ifndef RSBS_SOURCE_DIR
    printf("[TEST] FAIL: RSBS_SOURCE_DIR is undefined: the Way of the Hero's wiring cannot be checked\n");
    return false;
#else
    for (const WhSite& site : kWhSites) {
        std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + site.file, std::ios::binary);
        WH_ASSERT(in.good(), "G: a wired source file cannot be read");
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const std::string problem = WhSiteProblem(site, text);
        if (!problem.empty()) {
            printf("[TEST] FAIL: G: %s: %s (%s)\n", site.file, problem.c_str(), site.what);
            return false;
        }
        // The counterfactual: #1439's own line gone must fail the check.
        std::string mutated = text;
        const size_t at = mutated.find(site.mutate);
        WH_ASSERT(at != std::string::npos, "G: the wired line vanished between the check and its counterfactual");
        mutated.replace(at, strlen(site.mutate), "/* removed */");
        if (WhSiteProblem(site, mutated).empty()) {
            printf("[TEST] FAIL: G: the check of %s is vacuous: it passes with the wired line gone (%s)\n", site.file,
                   site.what);
            return false;
        }
        printf("[TEST]   G: %s\n", site.what);
    }
    return true;
#endif
}

} // namespace

TestResult Test_WothHints(void) {
    printf("[TEST] woth-hints: the pair-level Way of the Hero over both games' hosts, both games' stones naming it, "
           "and MM's hints on by default (#1439)\n");
    const ComboContext savedCtx = gComboCtx;
    bool ok = WhCheckDefaults();
    if (ok) {
        // WhCheckSet installs the stub engines first thing, so they are what
        // ClUninstall resets and un-registers, whatever the two legs found.
        ok = WhCheckSet() && WhCheckRecord();
        ClUninstall();
    }
    Combo_Woth_Clear();
    ok = ok && WhCheckCreationRecord() && WhCheckMmStones() && WhCheckOoTStones() && WhCheckWiring();
    Combo_Woth_Clear();
    gComboCtx = savedCtx;
    if (!ok) {
        return TEST_FAIL;
    }
    printf("[TEST] woth-hints: PASS\n");
    return TEST_PASS;
}
