/**
 * @file combo_single_bag.c
 * @brief The single-bag fill at the creation event (see combo_single_bag.h).
 *
 * Game-header-free like combo_logic.c: every game fact arrives through the two
 * ports' extern "C" halves declared in the header, as game-neutral scalars.
 */

#include "combo_single_bag.h"

#include "context.h"
#include "foreign_items.h"
#include "gen_budget.h"
#include "gen_progress_overlay.h"
#include "shared_items.h"

#include <stdio.h>
#include <string.h>

// ============================================================================
// Buffers. Static, like every bag-sized buffer in combo_logic.c: the fill runs on
// the game thread inside the creation bracket, and these are far past any sane
// frame budget.
// ============================================================================

/** Pool rows of both games in one fill: OoT's general-pass pool (a few hundred
 *  rows) plus MM's GeneratePools pool (283 on the shipped profile, the whole
 *  graph's ~2250 checks' worth on a profile that shuffles every location). */
#define COMBO_SINGLE_BAG_ROW_CAP 8192

static ComboLogicPoolRow sRows[COMBO_SINGLE_BAG_ROW_CAP];
static uint16_t sOoTItems[COMBO_SINGLE_BAG_ROW_CAP];
static uint16_t sOoTFlags[COMBO_SINGLE_BAG_ROW_CAP];
static ComboLogicBagItem sBag[RSBS_COMBO_LOGIC_BAG_CAP];
static int sBagPoolIndex[RSBS_COMBO_LOGIC_BAG_CAP];
static int sOoTBagRows[RSBS_COMBO_LOGIC_BAG_CAP];

static ComboSingleBagReport sReport;

const ComboSingleBagReport* Combo_SingleBag_LastReport(void) {
    return &sReport;
}

// ============================================================================
// The seed
// ============================================================================

static void SingleBagFold(uint32_t* h, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        *h ^= (v >> (8 * i)) & 0xFFu;
        *h *= 16777619u;
    }
}

uint32_t Combo_SingleBag_SeedFor(int ladderAttempt) {
    // FNV-1a over the frozen identity, a domain tag and the ladder attempt. The
    // tag keeps this stream disjoint from every other identity-derived stream in
    // the tree (the old placement passes' xorshifts, MM's attempt mix), so a
    // change to one can never alias the other.
    uint32_t h = 2166136261u;
    static const char kTag[] = "rsbs-single-bag-v1";
    for (size_t i = 0; i + 1 < sizeof(kTag); ++i) {
        h ^= (uint8_t)kTag[i];
        h *= 16777619u;
    }
    SingleBagFold(&h, gComboCtx.sharedRandoSeed);
    SingleBagFold(&h, gComboCtx.sharedRandoSettingsHash);
    SingleBagFold(&h, gComboCtx.mmProfileDigest);
    SingleBagFold(&h, gComboCtx.comboSettingsHash);
    SingleBagFold(&h, (uint32_t)ladderAttempt);
    return h;
}

// ============================================================================
// The observer: progress, frames, and the per-attempt stop
// ============================================================================

typedef struct {
    int ladderAttempt;
    uint32_t budgetMs;
    uint32_t startMs;        // Combo_GenBudget_NowMs() at the fill's start
    uint32_t presentStartMs; // Combo_GenProgress_PresentationMs() at the fill's start
    uint32_t lastReportMs;   // generation ms at the last progress report
    int lastStage;
    int lastAttempt;
    bool aborted;
    char detail[96];
} SingleBagWatch;

/** Generation time since the fill started: wall time minus what the overlay
 *  spent presenting. Two measurements in ONE clock (gen_budget's), which is the
 *  rule gen_budget.h states for both of the creation's stops. */
static uint32_t SingleBagGenerationMs(const SingleBagWatch* w) {
    const uint32_t now = Combo_GenBudget_NowMs();
    const uint32_t wall = (now >= w->startMs) ? now - w->startMs : 0u;
    const uint32_t presentNow = Combo_GenProgress_PresentationMs();
    const uint32_t presented = (presentNow >= w->presentStartMs) ? presentNow - w->presentStartMs : 0u;
    return (wall > presented) ? wall - presented : 0u;
}

static int SingleBagObserve(void* ctx, const ComboLogicFillProgress* p) {
    SingleBagWatch* w = (SingleBagWatch*)ctx;
    const uint32_t genMs = SingleBagGenerationMs(w);

    // Frames first, so a slow round still leaves a live window. The heartbeat is
    // rate-limited to 10 Hz inside the overlay; its clock is this attempt's.
    if (ComboGenOverlay_WantsHeartbeat()) {
        (void)ComboGenOverlay_Heartbeat(genMs);
    }

    // THE CAPTION: the coordinator's phases, reported on every change of stage
    // or batch attempt and at most once a second in between — every report is a
    // stderr line and a painted frame, and a fill runs a round per bag row.
    const bool stageChanged = p->stage != w->lastStage || p->attempt != w->lastAttempt;
    if (stageChanged || genMs - w->lastReportMs >= 1000u) {
        switch (p->stage) {
            case RSBS_COMBO_FILL_STAGE_PROOF:
                snprintf(w->detail, sizeof(w->detail), "Proving the paired world can be finished");
                break;
            case RSBS_COMBO_FILL_STAGE_SURPLUS:
                snprintf(w->detail, sizeof(w->detail), "Placing the extra copies");
                break;
            default:
                snprintf(w->detail, sizeof(w->detail), "Placing item %d of %d in both worlds (round %d, batch %d)",
                         p->requiredPlaced + 1, p->requiredCount, p->rounds + 1, p->attempt);
                break;
        }
        Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_MM_FILL, w->ladderAttempt + 1, w->detail);
        w->lastReportMs = genMs;
        w->lastStage = p->stage;
        w->lastAttempt = p->attempt;
    }

    // THE STOP. A wall clock decides nothing about the world: this only ever
    // ends the fill, with a status the ladder refuses to climb on.
    if (w->budgetMs > 0u && genMs > w->budgetMs) {
        fprintf(stderr,
                "[SingleBag] per-attempt budget exhausted: %ums of generation against %ums, after %d rounds (a "
                "wall-clock STOP, never a rung)\n",
                genMs, w->budgetMs, p->rounds);
        w->aborted = true;
        return 1;
    }
    return 0;
}

// ============================================================================
// The fill
// ============================================================================

static bool SingleBagOriginMayCross(uint8_t origin) {
    return Combo_ComboDirectionArms(origin) &&
           (Combo_ComboItemClassFor(origin) & (uint16_t)RSBS_ITEMCLASS_PROGRESSION) != 0u;
}

int Combo_SingleBag_Run(const uint16_t* mmItems, const uint16_t* mmFlags, int mmCount, int ladderAttempt,
                        uint32_t budgetMs, uint8_t* outMmInBag, ComboSingleBagReport* out) {
    const uint32_t t0 = Combo_GenBudget_NowMs();
    memset(&sReport, 0, sizeof(sReport));
    sReport.ladderAttempt = ladderAttempt;
    sReport.budgetMs = budgetMs;
    sReport.goal = gComboCtx.comboSettings.goal;
    sReport.rung = gComboCtx.comboSettings.logicRung;
    int status = RSBS_COMBO_LOGIC_OK;

    if (mmCount < 0 || (mmCount > 0 && (mmItems == NULL || mmFlags == NULL))) {
        status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
        goto finish;
    }
    if (!Combo_ComboSettingsFrozen() || !Combo_ForeignPairingActive()) {
        // The fill's every input is a frozen fact; with no frozen record there is
        // no GOAL, rung, direction or class bitset to fill under.
        fprintf(stderr, "[SingleBag] refused: no frozen paired identity (frozen=%d paired=%d)\n",
                Combo_ComboSettingsFrozen() ? 1 : 0, Combo_ForeignPairingActive() ? 1 : 0);
        status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
        goto finish;
    }
    if (!OoT_ComboLogic_GeneralPassDeferred()) {
        // OoT's world is not waiting at its general pass: either it was never
        // generated in this process, or it already finished. Filling now would
        // place the bag over a world that already has a general pass.
        fprintf(stderr, "[SingleBag] refused: OoT's world is not at its general-pass point\n");
        status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
        goto finish;
    }

    // --- the rows: OoT's general-pass pool, then MM's pool -----------------
    {
        const int ootTotal = OoT_ComboLogic_ExportPool(0, sOoTItems, NULL, sOoTFlags, COMBO_SINGLE_BAG_ROW_CAP);
        if (ootTotal < 0) {
            fprintf(stderr, "[SingleBag] refused: OoT's pool export is not ready\n");
            status = RSBS_COMBO_LOGIC_ERR_NO_ENGINE;
            goto finish;
        }
        if (ootTotal + mmCount > COMBO_SINGLE_BAG_ROW_CAP) {
            fprintf(stderr, "[SingleBag] refused: %d + %d pool rows exceed the %d-row buffer\n", ootTotal, mmCount,
                    COMBO_SINGLE_BAG_ROW_CAP);
            status = RSBS_COMBO_LOGIC_ERR_CAPACITY;
            goto finish;
        }
        sReport.ootRows = ootTotal;
        sReport.mmRows = mmCount;
        for (int i = 0; i < ootTotal; ++i) {
            memset(&sRows[i], 0, sizeof(sRows[i]));
            sRows[i].item.originGame = (uint8_t)GAME_OOT;
            sRows[i].item.id = sOoTItems[i];
            sRows[i].poolFlags = sOoTFlags[i];
        }
        for (int j = 0; j < mmCount; ++j) {
            ComboLogicPoolRow* r = &sRows[ootTotal + j];
            memset(r, 0, sizeof(*r));
            r->item.originGame = (uint8_t)GAME_MM;
            r->item.id = mmItems[j];
            r->poolFlags = mmFlags[j];
        }
    }

    // --- 1. the bag -----------------------------------------------------------
    {
        Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_MM_FILL, ladderAttempt + 1,
                                 "Building the single bag of both worlds' items");
        ComboLogicComposeRequest creq;
        memset(&creq, 0, sizeof(creq));
        creq.rows = sRows;
        creq.rowCount = sReport.ootRows + sReport.mmRows;
        creq.armedOoT = OoT_ComboLogic_ConfinementArmed() | Combo_ItemClassArmedFromFrozen((uint8_t)GAME_OOT);
        creq.armedMM = Combo_ItemClassArmedFromFrozen((uint8_t)GAME_MM);
        creq.composeFlags = RSBS_COMBO_COMPOSE_ADMIT_CONFINED_HOME;
        status = Combo_Logic_ComposeBag(&creq, sBag, RSBS_COMBO_LOGIC_BAG_CAP, sBagPoolIndex, &sReport.compose);
        if (status != RSBS_COMBO_LOGIC_OK) {
            fprintf(stderr, "[SingleBag] the bag could not be composed: %s\n", Combo_Logic_StatusName(status));
            goto finish;
        }
        sReport.bagCount = sReport.compose.bagCount;
    }

    // --- 2. which origins may cross -------------------------------------------
    {
        const bool ootMayCross = SingleBagOriginMayCross((uint8_t)GAME_OOT);
        const bool mmMayCross = SingleBagOriginMayCross((uint8_t)GAME_MM);
        for (int i = 0; i < sReport.bagCount; ++i) {
            const bool mayCross = (sBag[i].item.originGame == (uint8_t)GAME_OOT) ? ootMayCross : mmMayCross;
            if (!mayCross) {
                sBag[i].bagFlags |= RSBS_COMBO_BAG_HOME_ONLY;
            }
            if ((sBag[i].bagFlags & RSBS_COMBO_BAG_HOME_ONLY) != 0u) {
                sReport.homeOnlyRows++;
            }
            // The ADR 0011 selection bit the row was admitted under, carried into
            // the placement tables and the crossing store (combo_logic.h:
            // "carried, not filtered on"). Every bag row is O8-progression.
            sBag[i].itemClass = (uint16_t)RSBS_ITEMCLASS_PROGRESSION;
        }
        fprintf(stderr,
                "[SingleBag] bag: %d rows (OoT %d req + %d surplus + %d confined; MM %d req + %d surplus + %d confined) "
                "from %d OoT + %d MM pool rows; %d home-only; crossings armed OoT->MM=%d MM->OoT=%d; filler left to "
                "each game: OoT %d junk / %d renewable / %d trap, MM %d junk / %d renewable / %d trap\n",
                sReport.bagCount, sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_REQUIRED],
                sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_SURPLUS],
                sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_CONFINED],
                sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_REQUIRED],
                sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_SURPLUS],
                sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_CONFINED], sReport.ootRows, sReport.mmRows,
                sReport.homeOnlyRows, ootMayCross ? 1 : 0, mmMayCross ? 1 : 0,
                sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_JUNK],
                sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_RENEWABLE],
                sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_TRAP],
                sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_JUNK],
                sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_RENEWABLE],
                sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_TRAP]);
    }

    // --- 3 + 4. the fill, seeded from the identity, stopped by the budget -----
    {
        SingleBagWatch watch;
        memset(&watch, 0, sizeof(watch));
        watch.ladderAttempt = ladderAttempt;
        watch.budgetMs = budgetMs;
        watch.startMs = t0;
        watch.presentStartMs = Combo_GenProgress_PresentationMs();
        watch.lastStage = -1;
        watch.lastAttempt = -1;

        sReport.seed = Combo_SingleBag_SeedFor(ladderAttempt);
        ComboLogicFillRequest freq;
        memset(&freq, 0, sizeof(freq));
        freq.bag = sBag;
        freq.bagCount = sReport.bagCount;
        freq.goal = sReport.goal;
        freq.logicRung = sReport.rung;
        freq.seed = sReport.seed;
        freq.maxAttempts = 0; // RSBS_COMBO_LOGIC_FILL_RETRIES batch roll-backs
        freq.observer = SingleBagObserve;
        freq.observerCtx = &watch;
        status = Combo_Logic_RunFill(&freq, &sReport.fill);
        if (status != RSBS_COMBO_LOGIC_OK) {
            fprintf(stderr, "[SingleBag] fill failed on ladder attempt %d: %s after %d batch attempt(s), %d rounds%s\n",
                    ladderAttempt + 1, Combo_Logic_StatusName(status), sReport.fill.attempts, sReport.fill.rounds,
                    watch.aborted ? " (the per-attempt budget stopped it)" : "");
            goto finish;
        }
    }

    // --- success: the bookkeeping each game's own pass needs -----------------
    {
        int ootBag = 0;
        if (outMmInBag != NULL) {
            memset(outMmInBag, 0, (size_t)mmCount);
        }
        for (int i = 0; i < sReport.bagCount; ++i) {
            const int poolRow = sBagPoolIndex[i];
            if (poolRow < sReport.ootRows) {
                sOoTBagRows[ootBag++] = poolRow;
            } else if (outMmInBag != NULL) {
                outMmInBag[poolRow - sReport.ootRows] = 1;
            }
        }
        OoT_ComboLogic_NoteBagRows(sOoTBagRows, ootBag);

        ComboLogicPlacement p;
        for (int i = 0; i < Combo_Logic_PlacementCount(GAME_MM); ++i) {
            if (Combo_Logic_PlacementAt(GAME_MM, i, &p) && p.item.originGame == (uint8_t)GAME_OOT) {
                sReport.crossingsIntoMM++;
            }
        }
        for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
            if (Combo_Logic_PlacementAt(GAME_OOT, i, &p) && p.item.originGame == (uint8_t)GAME_MM) {
                sReport.crossingsIntoOoT++;
            }
        }
    }

finish:
    if (status != RSBS_COMBO_LOGIC_OK && sReport.fill.attempts > 0) {
        // Roll BOTH engines back to their state on entry: OoT's general-pass hosts
        // to RG_NONE, MM's checks to their priors. The MM ladder restores MM's
        // whole save per attempt anyway, but nothing restores OoT's world except
        // this, and the next attempt must see its general pass empty.
        Combo_Logic_ResetPlacements();
    }
    sReport.status = status;
    sReport.wallMs = Combo_GenBudget_NowMs() - t0;
    fprintf(stderr,
            "[SingleBag] ladder attempt %d: %s — goal %u rung %u seed %08X; %d rows placed (%d required, %d surplus, "
            "%d dropped), %d crossings into MM, %d into OoT, leftovers OoT %d / MM %d; %d batch attempt(s), %d rounds, "
            "%ums (budget %ums)\n",
            ladderAttempt + 1, Combo_Logic_StatusName(status), (unsigned)sReport.goal, (unsigned)sReport.rung,
            sReport.seed, sReport.fill.placed, sReport.fill.requiredPlaced, sReport.fill.surplusPlaced,
            sReport.fill.surplusDropped, sReport.crossingsIntoMM, sReport.crossingsIntoOoT,
            sReport.fill.leftoverHostsOoT, sReport.fill.leftoverHostsMM, sReport.fill.attempts, sReport.fill.rounds,
            sReport.wallMs, budgetMs);
    fflush(stderr);
    if (out != NULL) {
        *out = sReport;
    }
    return status;
}

void Combo_SingleBag_Forget(void) {
    // Tables first (no engine call), then each engine's own record.
    (void)Combo_Logic_HydrateTables(NULL, 0, NULL, 0);
    OoT_ComboLogic_ForgetPlacements();
    MM_ComboLogic_ForgetPlacements();
}
