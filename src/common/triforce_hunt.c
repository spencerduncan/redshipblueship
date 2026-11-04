/**
 * @file triforce_hunt.c
 * @brief The combo triforce hunt's frozen record, arming gate and win decision
 *        (ADR 0010 answer O10). See triforce_hunt.h for the contract.
 *
 * Game-header-free: both ports reach this file through their own single-exe
 * seams (the creation event, the shared-resource shims, the two piece-give
 * arms), and the ROM-free locks drive it directly.
 */

#include "triforce_hunt.h"

#include "combo_settings_view.h" // Combo_ComboSettingResolved: the [Both Games] rows' one reader (#1389)
#include "foreign_items.h"       // RSBS_COMBO_GOAL_*, RSBS_COMBO_DIVERGE_TRIFORCE, Combo_ComboSettingsFrozen

#include <stdio.h>
#include <string.h>

const char* Combo_TriforceStatusName(int status) {
    switch (status) {
        case RSBS_TRIFORCE_OK:
            return "ok";
        case RSBS_TRIFORCE_ERR_NO_PIECES:
            return "no-pieces";
        case RSBS_TRIFORCE_ERR_BAD_HALF:
            return "bad-half";
        case RSBS_TRIFORCE_ERR_OVER_CAP:
            return "over-cap";
        case RSBS_TRIFORCE_ERR_BAD_REQUEST:
            return "bad-request";
        case RSBS_TRIFORCE_ERR_NO_GOAL:
            return "no-goal";
        case RSBS_TRIFORCE_ERR_GOAL_OVER_PIECES:
            return "goal-over-pieces";
        default:
            return "(unknown)";
    }
}

/** One half on its own: it never requires more than it holds, so a half that
 *  holds nothing requires nothing. MM's half holding pieces with a zero share
 *  of the requirement is coherent since #1389 (Combo_TriforceSplit gives MM's
 *  half that share when the goal is 1): the requirement is one combo number.
 *  OoT's half holding pieces never has a zero share: SoH's Required Pieces
 *  option stores its value - 1, and the split gives OoT ceil(goal / 2) >= 1,
 *  so no creation in any era writes OoT pieces with no requirement. Those bytes
 *  are damage, and this check is the only guard OoT's half has (no arrival
 *  compares it with OoT's settings, as MM's arrival does MM's half). */
static bool TriforceHalfCoherent(uint32_t total, uint32_t required, bool zeroShareAllowed) {
    if (total == 0u) {
        return required == 0u;
    }
    return required <= total && (zeroShareAllowed || required >= 1u);
}

/** THE RULE, over plain numbers, so the resolver and the stored-record check
 *  cannot drift apart: they are one function. */
static int TriforceValidate(uint32_t totalOoT, uint32_t requiredOoT, uint32_t totalMM, uint32_t requiredMM) {
    if (!TriforceHalfCoherent(totalOoT, requiredOoT, /*zeroShareAllowed=*/false) ||
        !TriforceHalfCoherent(totalMM, requiredMM, /*zeroShareAllowed=*/true)) {
        return RSBS_TRIFORCE_ERR_BAD_HALF;
    }
    const uint32_t total = totalOoT + totalMM;
    if (total == 0u) {
        return RSBS_TRIFORCE_ERR_NO_PIECES;
    }
    if (total > RSBS_TRIFORCE_COMBO_MAX) {
        return RSBS_TRIFORCE_ERR_OVER_CAP;
    }
    // requiredOoT + requiredMM <= total follows from each half's own bound, so
    // the combo requirement can never exceed the pieces the two pools hold. A
    // requirement of zero would be a hunt won before its first piece.
    if (requiredOoT + requiredMM == 0u) {
        return RSBS_TRIFORCE_ERR_NO_GOAL;
    }
    return RSBS_TRIFORCE_OK;
}

int Combo_TriforceResolve(const ComboTriforceHalf* oot, const ComboTriforceHalf* mm, ComboTriforceRecord* out) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (oot == NULL || mm == NULL || out == NULL) {
        return RSBS_TRIFORCE_ERR_BAD_REQUEST;
    }
    const int status = TriforceValidate(oot->total, oot->required, mm->total, mm->required);
    if (status != RSBS_TRIFORCE_OK) {
        return status;
    }
    // Each half fits a byte: the combo total bounds both, and it is <= 255.
    out->totalOoT = (uint8_t)oot->total;
    out->requiredOoT = (uint8_t)oot->required;
    out->totalMM = (uint8_t)mm->total;
    out->requiredMM = (uint8_t)mm->required;
    return RSBS_TRIFORCE_OK;
}

int Combo_TriforceRecordCheck(const ComboTriforceRecord* rec) {
    if (rec == NULL) {
        return RSBS_TRIFORCE_ERR_BAD_REQUEST;
    }
    return TriforceValidate(rec->totalOoT, rec->requiredOoT, rec->totalMM, rec->requiredMM);
}

bool Combo_TriforceRecordPresent(const ComboTriforceRecord* rec) {
    return rec != NULL && ((uint32_t)rec->totalOoT + (uint32_t)rec->totalMM) != 0u;
}

uint16_t Combo_TriforceRecordRequired(const ComboTriforceRecord* rec) {
    return rec == NULL ? 0u : (uint16_t)((uint32_t)rec->requiredOoT + (uint32_t)rec->requiredMM);
}

uint16_t Combo_TriforceRecordTotal(const ComboTriforceRecord* rec) {
    return rec == NULL ? 0u : (uint16_t)((uint32_t)rec->totalOoT + (uint32_t)rec->totalMM);
}

void Combo_ResolveComboTriforce(uint16_t* outPieces, uint16_t* outGoal) {
    // Through THE ONE READER, so an unset key and an out-of-space stored value
    // are both the shipped default, with the reason logged once.
    const int32_t pieces = Combo_ComboSettingResolved(COMBO_SETTING_TRIFORCE_PIECES);
    int32_t goal = Combo_ComboSettingResolved(COMBO_SETTING_TRIFORCE_GOAL);
    // Two keys, one constraint between them: OoTMM clamps its goal to its pieces,
    // and so does this, so no pair of keys resolves to a hunt nobody can win.
    if (goal > pieces) {
        goal = pieces;
    }
    if (outPieces != NULL) {
        *outPieces = (uint16_t)pieces;
    }
    if (outGoal != NULL) {
        *outGoal = (uint16_t)goal;
    }
}

int Combo_TriforceSplit(uint16_t pieces, uint16_t goal, ComboTriforceHalf* oot, ComboTriforceHalf* mm) {
    if (oot != NULL) {
        memset(oot, 0, sizeof(*oot));
    }
    if (mm != NULL) {
        memset(mm, 0, sizeof(*mm));
    }
    if (oot == NULL || mm == NULL) {
        return RSBS_TRIFORCE_ERR_BAD_REQUEST;
    }
    if (pieces == 0u) {
        return RSBS_TRIFORCE_ERR_NO_PIECES;
    }
    if (goal == 0u) {
        return RSBS_TRIFORCE_ERR_NO_GOAL;
    }
    if ((uint32_t)pieces > RSBS_TRIFORCE_COMBO_MAX) {
        return RSBS_TRIFORCE_ERR_OVER_CAP;
    }
    if (goal > pieces) {
        return RSBS_TRIFORCE_ERR_GOAL_OVER_PIECES;
    }
    // OoT takes the larger half of each number, inside its own option range;
    // MM takes the rest. MM's half of the goal never exceeds its half of the
    // pieces: uncapped, floor(goal/2) <= floor(pieces/2); capped (pieces > 200),
    // MM holds pieces-100 >= pieces/2 pieces and requires goal-100 or
    // floor(goal/2), both within it.
    uint32_t totalOoT = ((uint32_t)pieces + 1u) / 2u;
    if (totalOoT > RSBS_TRIFORCE_OOT_HALF_MAX) {
        totalOoT = RSBS_TRIFORCE_OOT_HALF_MAX;
    }
    uint32_t requiredOoT = ((uint32_t)goal + 1u) / 2u;
    if (requiredOoT > totalOoT) {
        requiredOoT = totalOoT;
    }
    oot->total = (uint16_t)totalOoT;
    oot->required = (uint16_t)requiredOoT;
    mm->total = (uint16_t)((uint32_t)pieces - totalOoT);
    mm->required = (uint16_t)((uint32_t)goal - requiredOoT);
    return RSBS_TRIFORCE_OK;
}

uint32_t Combo_TriforceCountsDivergence(const ComboSettingsRecord* settings, const ComboTriforceRecord* rec) {
    if (settings == NULL || rec == NULL || settings->formatVersion == 0 ||
        settings->goal != (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT ||
        Combo_TriforceRecordCheck(rec) != RSBS_TRIFORCE_OK) {
        return 0u;
    }
    uint16_t pieces = 0;
    uint16_t goal = 0;
    Combo_ResolveComboTriforce(&pieces, &goal);
    return (pieces != Combo_TriforceRecordTotal(rec) || goal != Combo_TriforceRecordRequired(rec))
               ? RSBS_COMBO_DIVERGE_TRIFORCE_COUNTS
               : 0u;
}

void Combo_TriforceCountsShown(uint16_t* outPieces, uint16_t* outGoal) {
    if (Combo_TriforceHuntArmed()) {
        // The world's own numbers, as every other row shows the frozen record.
        if (outPieces != NULL) {
            *outPieces = Combo_TriforceHuntTotal();
        }
        if (outGoal != NULL) {
            *outGoal = Combo_TriforceHuntRequired();
        }
        return;
    }
    Combo_ResolveComboTriforce(outPieces, outGoal);
}

int Combo_TriforceFreezeAtCreation(const ComboTriforceHalf* oot, const ComboTriforceHalf* mm) {
    // Zero first: whatever a previous creation in this process froze belongs to
    // a world this creation replaces, and an error below must leave "no hunt",
    // never the previous world's hunt.
    memset(&gComboCtx.comboTriforce, 0, sizeof(gComboCtx.comboTriforce));

    if (!Combo_ComboSettingsFrozen() || gComboCtx.comboSettings.goal != (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) {
        // Every world that is not a triforce hunt — the shipped default among
        // them — stores the four zero bytes a pre-carve record reads as.
        return RSBS_TRIFORCE_OK;
    }

    ComboTriforceRecord rec;
    const int status = Combo_TriforceResolve(oot, mm, &rec);
    if (status != RSBS_TRIFORCE_OK) {
        fprintf(stderr,
                "[Combo] triforce hunt: the combo goal is triforce-hunt but the halves cannot describe one (%s: "
                "OoT %u of %u, MM %u of %u); the creation must refuse\n",
                Combo_TriforceStatusName(status), oot != NULL ? (unsigned)oot->required : 0u,
                oot != NULL ? (unsigned)oot->total : 0u, mm != NULL ? (unsigned)mm->required : 0u,
                mm != NULL ? (unsigned)mm->total : 0u);
        return status;
    }
    gComboCtx.comboTriforce = rec;
    fprintf(stderr,
            "[Combo] triforce hunt FROZEN: %u of %u pieces across both worlds (OoT pool %u requiring %u, MM pool %u "
            "requiring %u)\n",
            (unsigned)Combo_TriforceRecordRequired(&rec), (unsigned)Combo_TriforceRecordTotal(&rec),
            (unsigned)rec.totalOoT, (unsigned)rec.requiredOoT, (unsigned)rec.totalMM, (unsigned)rec.requiredMM);
    return RSBS_TRIFORCE_OK;
}

uint32_t Combo_TriforceRecordDivergence(const ComboSettingsRecord* settings, const ComboTriforceRecord* rec) {
    if (rec == NULL) {
        return 0;
    }
    const bool hunt =
        settings != NULL && settings->formatVersion != 0 && settings->goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT;
    if (hunt) {
        // A frozen hunt the stored record cannot describe: absent, or bytes the
        // creation rule could never have written.
        return Combo_TriforceRecordCheck(rec) == RSBS_TRIFORCE_OK ? 0u : RSBS_COMBO_DIVERGE_TRIFORCE;
    }
    // Any other goal, or no combo record at all: a hunt record here describes a
    // hunt nobody froze. Every byte must be zero, including a stray requirement
    // with a zero total, which Present() alone would not see.
    const bool allZero = rec->totalOoT == 0 && rec->requiredOoT == 0 && rec->totalMM == 0 && rec->requiredMM == 0;
    return allZero ? 0u : RSBS_COMBO_DIVERGE_TRIFORCE;
}

bool Combo_TriforceHalfDiverges(const ComboTriforceRecord* rec, GameId game, const ComboTriforceHalf* live) {
    if (!Combo_TriforceRecordPresent(rec) || live == NULL) {
        return false;
    }
    uint32_t total;
    uint32_t required;
    if (game == GAME_OOT) {
        total = rec->totalOoT;
        required = rec->requiredOoT;
    } else if (game == GAME_MM) {
        total = rec->totalMM;
        required = rec->requiredMM;
    } else {
        return false;
    }
    return total != (uint32_t)live->total || required != (uint32_t)live->required;
}

bool Combo_TriforceHuntArmed(void) {
    return Combo_ComboSettingsFrozen() && gComboCtx.comboSettings.goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT &&
           Combo_TriforceRecordCheck(&gComboCtx.comboTriforce) == RSBS_TRIFORCE_OK;
}

uint16_t Combo_TriforceHuntRequired(void) {
    return Combo_TriforceHuntArmed() ? Combo_TriforceRecordRequired(&gComboCtx.comboTriforce) : 0u;
}

uint16_t Combo_TriforceHuntTotal(void) {
    return Combo_TriforceHuntArmed() ? Combo_TriforceRecordTotal(&gComboCtx.comboTriforce) : 0u;
}

uint16_t Combo_TriforceHuntShownRequired(uint16_t ownRequired) {
    return Combo_TriforceHuntArmed() ? Combo_TriforceHuntRequired() : ownRequired;
}

uint16_t Combo_TriforceHuntShownTotal(uint16_t ownTotal) {
    return Combo_TriforceHuntArmed() ? Combo_TriforceHuntTotal() : ownTotal;
}

int Combo_TriforceHuntOnPieceGiven(GameId game, uint16_t countAfterGive, uint16_t ownRequired) {
    if (game != GAME_OOT && game != GAME_MM) {
        return RSBS_TRIFORCE_WIN_NONE;
    }
    if (Combo_TriforceHuntArmed()) {
        // Paired: the game's own requirement is an INPUT that was used, never
        // the threshold. Only the combo requirement ends the combo.
        return countAfterGive == Combo_TriforceHuntRequired() ? RSBS_TRIFORCE_WIN_COMBO : RSBS_TRIFORCE_WIN_NONE;
    }
    return countAfterGive == ownRequired ? RSBS_TRIFORCE_WIN_OWN : RSBS_TRIFORCE_WIN_NONE;
}
