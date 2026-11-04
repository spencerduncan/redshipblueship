/**
 * @file combo_woth.c
 * @brief The pair-level Way of the Hero (#1439). See combo_woth.h.
 *
 * Game-header-free like combo_area.c: hosts arrive as the coordinator keys them,
 * items are graded by the class owner each game feeds (shared_items.h), and areas
 * come from each game's area adapter.
 */

#include "combo_woth.h"

#include "context.h"
#include "foreign_items.h" // Combo_ComboLogicClaimsAllowed: No Logic claims nothing
#include "game.h"
#include "shared_items.h" // Combo_ItemChestGrade: OoTMM's path-hint item exclusions

#include <stdio.h>
#include <string.h>

/* The PRESENT tag: bit 0 of the OoT half (area 0 is COMBO_AREA_NONE). */
#define WOTH_PRESENT_BIT 0x01u

/* Every area of each half has a bit, the tag aside. */
RSBS_CTX_STATIC_ASSERT(COMBO_AREA_OOT_END <= RSBS_COMBO_WOTH_OOT_BYTES * 8u,
                       "ComboWothRecord's OoT half must hold a bit for every OoT area, bit 0 being the tag");
RSBS_CTX_STATIC_ASSERT(COMBO_AREA_MM_END <= RSBS_COMBO_WOTH_MM_BYTES * 8u,
                       "ComboWothRecord's MM half must hold a bit for every MM area");

static ComboLogicHostRef sRequired[RSBS_COMBO_WOTH_HOST_CAP];
static int sRequiredCount = 0;
static ComboWothReport sReport;

void Combo_Woth_Clear(void) {
    sRequiredCount = 0;
    memset(&sReport, 0, sizeof(sReport));
}

int Combo_Woth_Count(void) {
    return sRequiredCount;
}

bool Combo_Woth_At(int index, ComboLogicHostRef* out) {
    if (index < 0 || index >= sRequiredCount || out == NULL) {
        return false;
    }
    *out = sRequired[index];
    return true;
}

bool Combo_Woth_IsRequired(uint8_t hostGame, uint16_t hostCheck) {
    for (int i = 0; i < sRequiredCount; i++) {
        if (sRequired[i].hostGame == hostGame && sRequired[i].hostCheck == hostCheck) {
            return true;
        }
    }
    return false;
}

const ComboWothReport* Combo_Woth_LastReport(void) {
    return &sReport;
}

bool Combo_Woth_DefaultEligible(uint8_t hostGame, const ComboLogicPlacement* placement) {
    if (placement == NULL) {
        return false;
    }
    const ComboArea area = Combo_Area_OfCheck(hostGame, placement->hostCheck);
    if (area.area == COMBO_AREA_NONE || area.area == COMBO_AREA_POCKET) {
        return false;
    }
    // OoTMM hints.ts isLocationHintable, klass 'path': never a key, a stray fairy,
    // a silver rupee or a token. The item's chest grade is its own game's class of
    // it (OoT's Item::GetCategory, MM's randoItemType), published for either origin.
    switch (Combo_ItemChestGrade(placement->item)) {
        case RSBS_CHEST_GRADE_SMALL_KEY:
        case RSBS_CHEST_GRADE_BOSS_KEY:
        case RSBS_CHEST_GRADE_TOKEN:
        case RSBS_CHEST_GRADE_FAIRY:
            return false;
        default:
            return true;
    }
}

/** One round of the frozen goal with `forbidden` (NULL for none) withheld.
 *  1 = the goal holds, 0 = it does not, -1 = the round failed. */
static int WothRound(uint8_t goal, uint16_t triforceRequired, const ComboLogicHostRef* forbidden, int* rounds) {
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.goal = goal;
    req.triforceRequired = triforceRequired;
    req.forbidden = forbidden;
    req.forbiddenCount = forbidden != NULL ? 1 : 0;
    ComboLogicRoundResult res;
    memset(&res, 0, sizeof(res));
    (*rounds)++;
    if (Combo_Logic_RunRound(&req, &res) != RSBS_COMBO_LOGIC_OK) {
        return -1;
    }
    return res.goalExpression == 1 ? 1 : 0;
}

int Combo_Woth_Analyze(uint8_t goal, uint16_t triforceRequired, int roundCap, ComboWothEligibleFn eligible,
                       ComboWothProgressFn progress, void* progressCtx, ComboWothReport* out) {
    Combo_Woth_Clear();

    // The candidates, the two tables interleaved (OoT, MM, OoT, ...), so a cap
    // that binds leaves both games' tails untested rather than one game's whole
    // table: the order is a pure function of the placements.
    static ComboLogicHostRef candidates[RSBS_COMBO_WOTH_HOST_CAP];
    int candidateCount = 0;
    const int ootCount = Combo_Logic_PlacementCount(GAME_OOT);
    const int mmCount = Combo_Logic_PlacementCount(GAME_MM);
    sReport.placements = ootCount + mmCount;
    for (int i = 0; i < ootCount || i < mmCount; i++) {
        const GameId games[2] = { GAME_OOT, GAME_MM };
        for (int g = 0; g < 2; g++) {
            ComboLogicPlacement p;
            if (i >= (g == 0 ? ootCount : mmCount) || !Combo_Logic_PlacementAt(games[g], i, &p)) {
                continue;
            }
            if (eligible != NULL && !eligible((uint8_t)games[g], &p)) {
                continue;
            }
            if (candidateCount < RSBS_COMBO_WOTH_HOST_CAP) {
                candidates[candidateCount].hostGame = (uint8_t)games[g];
                candidates[candidateCount].hostCheck = p.hostCheck;
                candidateCount++;
            }
        }
    }
    sReport.candidates = candidateCount;

    // The baseline: a world whose goal is not proved with nothing withheld says
    // nothing about what it needs (every host would read "required").
    const int baseline = WothRound(goal, triforceRequired, NULL, &sReport.rounds);
    if (baseline != 1) {
        sReport.status = baseline < 0 ? RSBS_COMBO_WOTH_ERR_ROUND : RSBS_COMBO_WOTH_NO_PROOF;
        if (out != NULL) {
            *out = sReport;
        }
        return sReport.status;
    }

    for (int i = 0; i < candidateCount; i++) {
        if (sReport.rounds >= roundCap) {
            sReport.capped = true;
            break;
        }
        if (progress != NULL) {
            progress(progressCtx, i, candidateCount);
        }
        const int holds = WothRound(goal, triforceRequired, &candidates[i], &sReport.rounds);
        sReport.tested++;
        if (holds < 0) {
            sRequiredCount = 0;
            sReport.required = 0;
            sReport.status = RSBS_COMBO_WOTH_ERR_ROUND;
            if (out != NULL) {
                *out = sReport;
            }
            return sReport.status;
        }
        if (holds == 0) {
            sRequired[sRequiredCount++] = candidates[i];
            sReport.required++;
        }
    }
    sReport.status = RSBS_COMBO_WOTH_OK;
    if (out != NULL) {
        *out = sReport;
    }
    return sReport.status;
}

// ============================================================================
// The area record
// ============================================================================

void Combo_WothRecord_Clear(ComboWothRecord* rec) {
    if (rec != NULL) {
        memset(rec, 0, sizeof(*rec));
    }
}

void Combo_WothRecord_MarkPresent(ComboWothRecord* rec) {
    if (rec != NULL) {
        rec->oot[0] |= WOTH_PRESENT_BIT;
    }
}

bool Combo_WothRecord_Present(const ComboWothRecord* rec) {
    return rec != NULL && (rec->oot[0] & WOTH_PRESENT_BIT) != 0;
}

/** Whether `a` may have a bit: an area of its game's half, never a special. The
 *  static asserts above make every such area's byte fall inside its half. */
static bool WothAreaStorable(ComboArea a) {
    return (a.game == (uint8_t)GAME_OOT || a.game == (uint8_t)GAME_MM) && a.area != COMBO_AREA_NONE &&
           a.area != COMBO_AREA_POCKET && (int)a.area <= Combo_Area_Count(a.game);
}

bool Combo_WothRecord_Add(ComboWothRecord* rec, ComboArea a) {
    if (rec == NULL || !WothAreaStorable(a)) {
        return false;
    }
    uint8_t* half = a.game == (uint8_t)GAME_OOT ? rec->oot : rec->mm;
    half[a.area / 8] |= (uint8_t)(1u << (a.area % 8));
    return true;
}

bool Combo_WothRecord_Has(const ComboWothRecord* rec, ComboArea a) {
    if (!Combo_WothRecord_Present(rec) || !WothAreaStorable(a)) {
        return false;
    }
    const uint8_t* half = a.game == (uint8_t)GAME_OOT ? rec->oot : rec->mm;
    return (half[a.area / 8] & (1u << (a.area % 8))) != 0;
}

int Combo_WothRecord_Areas(const ComboWothRecord* rec, uint8_t game, uint8_t* out, int cap) {
    int n = 0;
    const int count = Combo_Area_Count(game);
    for (int a = 1; a <= count; a++) {
        const ComboArea area = { game, (uint8_t)a };
        if (Combo_WothRecord_Has(rec, area)) {
            if (out != NULL && n < cap) {
                out[n] = (uint8_t)a;
            }
            n++;
        }
    }
    return n;
}

int Combo_WothRecord_Count(const ComboWothRecord* rec, uint8_t game) {
    return Combo_WothRecord_Areas(rec, game, NULL, 0);
}

int Combo_Woth_BuildRecord(ComboWothRecord* out, ComboWothAreaFn areaOf) {
    if (out == NULL) {
        return 0;
    }
    Combo_WothRecord_Clear(out);
    Combo_WothRecord_MarkPresent(out);
    for (int i = 0; i < sRequiredCount; i++) {
        const ComboArea a = areaOf != NULL ? areaOf(sRequired[i].hostGame, sRequired[i].hostCheck)
                                           : Combo_Area_OfCheck(sRequired[i].hostGame, sRequired[i].hostCheck);
        (void)Combo_WothRecord_Add(out, a);
    }
    return Combo_WothRecord_Count(out, (uint8_t)GAME_OOT) + Combo_WothRecord_Count(out, (uint8_t)GAME_MM);
}

int Combo_Woth_RunAtCreation(uint8_t goal, uint16_t triforceRequired, ComboWothProgressFn progress, void* progressCtx) {
    Combo_Woth_Clear();
    Combo_WothRecord_Clear(&gComboCtx.comboWoth);
    if (!Combo_ComboLogicClaimsAllowed()) {
        sReport.status = RSBS_COMBO_WOTH_NO_CLAIMS;
        fprintf(stderr, "[ComboWoth] No Logic: no Way of the Hero is claimed (nothing was proved)\n");
        return RSBS_COMBO_WOTH_NO_CLAIMS;
    }
    ComboWothReport report;
    const int status = Combo_Woth_Analyze(goal, triforceRequired, RSBS_COMBO_WOTH_ROUND_CAP, Combo_Woth_DefaultEligible,
                                          progress, progressCtx, &report);
    int areas = 0;
    if (status == RSBS_COMBO_WOTH_OK) {
        areas = Combo_Woth_BuildRecord(&gComboCtx.comboWoth, NULL);
    }
    fprintf(stderr,
            "[ComboWoth] goal %u: %s; %d placement(s), %d candidate(s), %d tested in %d round(s)%s, %d required, %d "
            "area(s) on the Way of the Hero (OoT %d, MM %d)\n",
            (unsigned)goal,
            status == RSBS_COMBO_WOTH_OK         ? "ok"
            : status == RSBS_COMBO_WOTH_NO_PROOF ? "the baseline proved nothing, no claim"
                                                 : "a round failed, no claim",
            report.placements, report.candidates, report.tested, report.rounds, report.capped ? " (round cap)" : "",
            report.required, areas, Combo_WothRecord_Count(&gComboCtx.comboWoth, (uint8_t)GAME_OOT),
            Combo_WothRecord_Count(&gComboCtx.comboWoth, (uint8_t)GAME_MM));
    return status;
}

const ComboWothRecord* Combo_Woth_FrozenRecord(void) {
    return &gComboCtx.comboWoth;
}
