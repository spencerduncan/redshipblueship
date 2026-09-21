/**
 * @file mm_trick_table_test.cpp
 * @brief ROM-free and graph locks for MM's per-trick vocabulary (#578 part 1).
 *        CTest rows `mm-trick-table` (label `redship`) and `mm-trick-gbt-gate`
 *        (label `rando`), registered in src/common/test_runner.cpp.
 *
 * Lives MM-side for the same reason mm_rando_options_test.cpp does: this is the
 * one place where both tables are in scope — `Rando::StaticData::Tricks` (MM's,
 * needing MM's headers) and the flat descriptor table the combo pane consumes
 * (`src/common/combo_mm_tricks_view.h`).
 *
 * ============================================================================
 * mm-trick-table — the substrate lock
 * ============================================================================
 *
 * Drives the REAL registrar, not a copy, and asserts:
 *
 *  (a) TOTALITY over the ENUM, not over the map: every `MMRT_*` id below
 *      `MMRT_MAX` has a row, and the row count equals `MMRT_MAX`. Checked over
 *      the enum because iterating the map can only ever find rows that exist —
 *      the exact shape of the 47-ids/46-rows skew `RO_ACCESS_MAJORA_REMAINS`
 *      caused on the option table, where the missed slot silently made every
 *      iteration over the id space partial.
 *  (b) WELL-FORMEDNESS per row: non-empty name/cvar/label/tooltip, an area in
 *      range, a non-empty tag set with EXACTLY ONE difficulty rung, the derived
 *      `name`/`cvar` pair agreeing (`cvar == "gRando.Tricks." + name`), the name
 *      round-tripping through `GetTrickIdFromName`, and `reservedReason`
 *      non-NULL exactly when `reserved`.
 *  (c) the descriptor table MIRRORS the MM table exactly — same count, unique
 *      ids covering the id space, and every copied field equal. The pane must
 *      never bind a widget to a key MM does not read.
 *  (d) GATING HONESTY, both directions: `disabledReason` is non-empty exactly
 *      when the row is reserved or unbound, and empty exactly when it is
 *      settable. A dead control with no visible cause reads as a broken port; a
 *      live one carrying a reason reads as broken and is not (ADR 0004 §5).
 *  (e) the RESERVED keys are present and INERT — the whole point of declaring
 *      them. For each: the CVar resolution forces off, the predicate forces off
 *      EVEN WITH A 1 WRITTEN INTO THE SAVE ARRAY (so a hand-edited or
 *      pre-reservation save cannot arm one), and the pane's writer refuses.
 *  (f) the predicate reads the FROZEN SAVE, never the CVar. Set a bound key's
 *      CVar and leave the save zeroed: `MM_TRICK` must still be false. This is
 *      the difference between "trick selections are frozen world identity" and
 *      "a live setting", and it is what makes a post-creation toggle unable to
 *      change a running world's rules (ADR 0010 §3.3).
 *  (g) FINDING (a), NON-VACUOUSLY: `CanKillEnemy(ACTOR_EN_VM)` — Beamos, whose
 *      logic is literally `return (CAN_USE_EXPLOSIVE);` — over a save whose ONLY
 *      explosive source is a Powder Keg plus the Goron Mask. Trick off =>
 *      unreachable; trick on => reachable; and a control with a real bomb stays
 *      reachable with the trick off, so the gate narrowed the keg leg and
 *      nothing else. Before the gate the first assertion is RED: the keg
 *      disjunct was unconditional, which is why ADR 0010 O11's shipped rung
 *      `beatable(T = ∅)` was unachievable.
 *  (h) IDENTITY: two profiles differing ONLY in the trick set produce different
 *      digests, through the real `ResolvePairedProfile` and the real
 *      `MM_Rando_ComputeProfileStamp`. RED before `ProfileIdentityString` folds
 *      the trick term (#570's widened `mmProfileDigest` scope; ADR 0009 decision
 *      1's amendment that a narrower digest is vacuous). Also asserts the two
 *      sites agree bit-exact under identical inputs, because a creation stamp
 *      that disagreed with the arrival resolution would refuse every pair.
 *  (i) the resolved trick set REACHES THE SAVE, which is what (f) then reads.
 *
 * Deliberately NOT asserted: that any particular unbound key does something.
 * 84 of the 86 keys are declared and consulted by nothing in this PR (part 2
 * authors the bindings), and a test that pretended otherwise would be the
 * vacuous lock this file exists to avoid. What IS asserted about them is that
 * they are described honestly and cannot be armed.
 *
 * ============================================================================
 * mm-trick-gbt-gate — finding (b) on the live graph
 * ============================================================================
 *
 * The Great Bay Temple compass-room boss-key connection is a `std::function` in
 * `Rando::Logic::Regions`, which is populated by ShipInit registrars — so unlike
 * (g) it cannot be reached without MM's boot, and it runs in the `rando` tier
 * where that bring-up already happens. The probe evaluates the REAL connection
 * lambda (not a re-statement of its condition) over a save holding Zora Mask,
 * Bow, Ice Arrows and magic: trick off => false, trick on => true. Before the
 * gate the first assertion is RED, because the edge was unconditional.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Foreign.h"
#include "Rando/StaticData/StaticData.h"
#include "Rando/Logic/Logic.h"

// src/common — outside any extern "C" block; these headers manage their own
// linkage (matching Foreign.cpp and mm_rando_options_test.cpp).
#include "combo_mm_options_view.h"
#include "combo_mm_tricks_view.h"
#include "foreign_items.h"

extern "C" {
#include "variables.h"
}

namespace {

int Fail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-TRICK-TABLE] FAIL(%d): ", code);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    return code;
}

int GateFail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-TRICK-GBT] FAIL(%d): ", code);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    return code;
}

/** Exactly-one-bit test, used for the difficulty rung. */
bool ExactlyOneBit(uint32_t v) {
    return v != 0 && (v & (v - 1)) == 0;
}

void ClearAllTrickCVars() {
    for (auto& [mmRandoTrickId, row] : Rando::StaticData::Tricks) {
        (void)mmRandoTrickId;
        CVarClear(row.cvar);
    }
}

/**
 * A save with NO inventory at all: every slot ITEM_NONE (0xFF), not zero. A
 * zeroed `items[]` would read as holding item id 0x00, so `HAS_ITEM` would be
 * true for it — an "empty" inventory that silently holds something is exactly
 * how a reachability probe becomes vacuous.
 */
void ResetSaveWithEmptyInventory() {
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
}

void GiveInventoryItem(ItemId itemId) {
    INV_CONTENT(itemId) = itemId;
}

/** Arm the frozen trick set directly — the state a generated file is in. */
void SetFrozenTrick(MMRandoTrickId mmRandoTrickId, bool on) {
    gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[mmRandoTrickId] = on ? 1 : 0;
}

} // namespace

extern "C" int MM_TrickTable_RunHeadless(void) {
    printf("[TEST] mm-trick-table: MM's %d trick keys have rows, honest gating, frozen storage, and the Powder Keg "
           "gate is real (#578 part 1)\n",
           (int)MMRT_MAX);

    const GameId prevGame = Context_GetCurrentGame();

    // Drive the real registrar. Combo_MMOptionsWindow_Init also calls this;
    // calling it directly keeps this a lock on the TABLE, not on the window.
    MM_RandoTricksUi_Register();

    // ---- (a) totality over the ENUM ---------------------------------------
    for (int id = 0; id < MMRT_MAX; id++) {
        if (Rando::StaticData::Tricks.find((MMRandoTrickId)id) == Rando::StaticData::Tricks.end()) {
            return Fail(1, "MMRandoTrickId %d has no Rando::StaticData::Tricks row", id);
        }
    }
    if ((int)Rando::StaticData::Tricks.size() != MMRT_MAX) {
        return Fail(2, "Tricks table holds %d rows for %d ids", (int)Rando::StaticData::Tricks.size(), (int)MMRT_MAX);
    }

    // ---- (b) per-row well-formedness --------------------------------------
    int reservedCount = 0;
    for (auto& [mmRandoTrickId, row] : Rando::StaticData::Tricks) {
        if (row.mmRandoTrickId != mmRandoTrickId) {
            return Fail(3, "row keyed %d carries id %d", (int)mmRandoTrickId, (int)row.mmRandoTrickId);
        }
        if (row.name == NULL || row.name[0] == '\0') {
            return Fail(4, "trick %d has no name", (int)mmRandoTrickId);
        }
        if (strncmp(row.name, "MMRT_", 5) != 0) {
            return Fail(5, "trick name '%s' is not a stringified MMRT_ enumerator", row.name);
        }
        if (row.cvar == NULL) {
            return Fail(6, "trick '%s' has no cvar", row.name);
        }
        // The macro derives both from the enumerator; if they can disagree, the
        // pane can write a key the profile resolution never reads.
        const std::string expectedCvar = std::string("gRando.Tricks.") + row.name;
        if (expectedCvar != row.cvar) {
            return Fail(7, "trick '%s' cvar is '%s', expected '%s'", row.name, row.cvar, expectedCvar.c_str());
        }
        if (Rando::StaticData::GetTrickIdFromName(row.name) != mmRandoTrickId) {
            return Fail(8, "trick '%s' does not round-trip through GetTrickIdFromName", row.name);
        }
        if (row.displayName == NULL || row.displayName[0] == '\0') {
            return Fail(9, "trick '%s' has no display name", row.name);
        }
        if (row.tooltip == NULL || row.tooltip[0] == '\0') {
            return Fail(10, "trick '%s' has no tooltip", row.name);
        }
        if ((int)row.area < 0 || (int)row.area >= (int)MMRTA_MAX) {
            return Fail(11, "trick '%s' has area %d, outside [0, %d)", row.name, (int)row.area, (int)MMRTA_MAX);
        }
        if (row.tags == 0) {
            return Fail(12, "trick '%s' has an empty tag set", row.name);
        }
        if (!ExactlyOneBit(row.tags & (uint32_t)MMRTT_DIFFICULTY_MASK)) {
            return Fail(13, "trick '%s' carries %s difficulty rung (tags=%04X)", row.name,
                        (row.tags & (uint32_t)MMRTT_DIFFICULTY_MASK) == 0 ? "no" : "more than one", (unsigned)row.tags);
        }
        // reserved <=> a reason, both directions: a reserved row with no reason
        // is a dead control with no cause, and a live row with one lies.
        if (row.reserved && (row.reservedReason == NULL || row.reservedReason[0] == '\0')) {
            return Fail(14, "reserved trick '%s' has no reason", row.name);
        }
        if (!row.reserved && row.reservedReason != NULL) {
            return Fail(15, "live trick '%s' carries a reserved reason ('%s')", row.name, row.reservedReason);
        }
        // Every reserved row must also be tagged as such, so a tag-driven filter
        // and the flag cannot disagree about which rows increment 3 unlocks.
        if (row.reserved != ((row.tags & (uint32_t)MMRTT_COMBO) != 0)) {
            return Fail(16, "trick '%s': reserved=%d but MMRTT_COMBO=%d", row.name, row.reserved ? 1 : 0,
                        (row.tags & (uint32_t)MMRTT_COMBO) != 0 ? 1 : 0);
        }
        if (row.reserved) {
            reservedCount++;
        }
    }

    // The reserved COUNT, transcribed. Not a style check: it is the tell that a
    // table edit reclassified a key. Measured against OoTMM `master` on
    // 2026-09-17 — 20 of the 85 ported keys require an OoT-side item on every leg
    // their own definition states. ADR 0010 increment 3 is expected to drive this
    // to 0, one batch at a time, and each batch updates this number on purpose.
    if (reservedCount != 20) {
        return Fail(17,
                    "expected 20 reserved keys, found %d — a key was reclassified; update this count and say why "
                    "in the commit",
                    reservedCount);
    }

    // ---- (c) the descriptor table mirrors the MM table ---------------------
    const int descCount = Combo_MMTrickCount();
    if (descCount != MMRT_MAX) {
        return Fail(18, "the descriptor table holds %d rows for %d ids", descCount, (int)MMRT_MAX);
    }
    std::set<uint16_t> seen;
    for (int i = 0; i < descCount; i++) {
        const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
        if (desc == NULL) {
            return Fail(19, "descriptor %d is NULL inside the registered count", i);
        }
        if (!seen.insert(desc->id).second) {
            return Fail(20, "descriptor id %u appears twice", (unsigned)desc->id);
        }
        if (desc->id >= (uint16_t)MMRT_MAX) {
            return Fail(21, "descriptor id %u is outside the id space", (unsigned)desc->id);
        }
        const Rando::StaticData::RandoStaticTrick& row = Rando::StaticData::Tricks.at((MMRandoTrickId)desc->id);
        if (strcmp(desc->name, row.name) != 0 || strcmp(desc->cvar, row.cvar) != 0 ||
            strcmp(desc->label, row.displayName) != 0 || strcmp(desc->tooltip, row.tooltip) != 0) {
            return Fail(22, "descriptor '%s' does not mirror its MM row's strings", desc->name);
        }
        if (desc->area != (uint8_t)row.area || desc->tags != row.tags || desc->reserved != row.reserved) {
            return Fail(23, "descriptor '%s' does not mirror its MM row's classification", desc->name);
        }
        if (desc->areaName == NULL || desc->areaName[0] == '\0' || desc->tagSummary == NULL ||
            desc->tagSummary[0] == '\0') {
            return Fail(24, "descriptor '%s' has an empty area or tag summary", desc->name);
        }
        if (Combo_MMTrickById(desc->id) != desc) {
            return Fail(25, "Combo_MMTrickById(%u) does not return the table's own row", (unsigned)desc->id);
        }

        // ---- (d) gating honesty, both directions --------------------------
        const bool settable = desc->bound && !desc->reserved;
        const bool hasReason = desc->disabledReason != NULL && desc->disabledReason[0] != '\0';
        if (settable == hasReason) {
            return Fail(26, "descriptor '%s': settable=%d but disabledReason=%s", desc->name, settable ? 1 : 0,
                        hasReason ? "present" : "empty");
        }
    }
    if ((int)seen.size() != MMRT_MAX) {
        return Fail(27, "the descriptor table covers %d of %d ids", (int)seen.size(), (int)MMRT_MAX);
    }

    // Both bound keys must actually be described as bound; otherwise (g) and the
    // GBT row below would be locking behaviour the pane calls unavailable.
    for (MMRandoTrickId bound : { MMRT_KEG_EXPLOSIVES, MMRT_GBT_BOSS_KEY_ICE }) {
        const ComboMMTrickDesc* desc = Combo_MMTrickById((uint16_t)bound);
        if (desc == NULL || !desc->bound || desc->reserved) {
            return Fail(28, "'%s' has a logic binding but the table does not describe it as settable",
                        Rando::StaticData::Tricks.at(bound).name);
        }
    }

    // ---- (e) the reserved keys are present and INERT -----------------------
    // Unfrozen carrier: the pane's writers reject while a creation stamp exists,
    // and this leg has to observe the RESERVED refusal specifically, not the
    // freeze refusal standing in for it.
    ComboContext_Init();
    ResetSaveWithEmptyInventory();
    ClearAllTrickCVars();
    for (auto& [mmRandoTrickId, row] : Rando::StaticData::Tricks) {
        if (!row.reserved) {
            continue;
        }
        // Arm it every way it could be armed.
        CVarSetInteger(row.cvar, 1);
        SetFrozenTrick(mmRandoTrickId, true);
        if (Rando::StaticData::ResolveTrickFromCVar(mmRandoTrickId)) {
            return Fail(29, "reserved trick '%s' resolved ON from its CVar", row.name);
        }
        if (Rando::StaticData::IsTrickEnabled(mmRandoTrickId)) {
            return Fail(30,
                        "reserved trick '%s' read ON from the frozen save — the reader does not refuse what the "
                        "writer refuses",
                        row.name);
        }
        if (MM_TRICK(mmRandoTrickId)) {
            return Fail(31, "MM_TRICK disagrees with IsTrickEnabled for reserved trick '%s'", row.name);
        }
        SetFrozenTrick(mmRandoTrickId, false);
        CVarClear(row.cvar);
        // And the pane's writer refuses to set it in the first place.
        const ComboMMTrickDesc* desc = Combo_MMTrickById((uint16_t)mmRandoTrickId);
        Combo_MMTrickSetValue(desc, true);
        if (CVarGetInteger(row.cvar, 0) != 0) {
            return Fail(32, "the pane's writer armed reserved trick '%s'", row.name);
        }
        if (Combo_MMTrickGetValue(desc)) {
            return Fail(33, "the pane reads reserved trick '%s' as on", row.name);
        }
    }

    // An UNBOUND live key is refused by the writer too — a CVar that changes
    // nothing would still move the frozen identity (ADR 0004 §5).
    {
        const ComboMMTrickDesc* unbound = NULL;
        for (int i = 0; i < descCount; i++) {
            const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
            if (desc != NULL && !desc->reserved && !desc->bound) {
                unbound = desc;
                break;
            }
        }
        if (unbound == NULL) {
            return Fail(34, "no unbound key found — part 2 has landed, so this leg needs rewriting rather than "
                            "passing vacuously");
        }
        CVarClear(unbound->cvar);
        Combo_MMTrickSetValue(unbound, true);
        if (CVarGetInteger(unbound->cvar, 0) != 0) {
            return Fail(35, "the pane's writer armed unbound trick '%s'", unbound->name);
        }
    }

    // ---- (f)/(i) the predicate reads the FROZEN SAVE, not the CVar ---------
    ResetSaveWithEmptyInventory();
    ClearAllTrickCVars();
    CVarSetInteger(Rando::StaticData::Tricks.at(MMRT_KEG_EXPLOSIVES).cvar, 1);
    if (MM_TRICK(MMRT_KEG_EXPLOSIVES)) {
        return Fail(36, "MM_TRICK honoured the authoring CVar over the frozen save — a post-creation toggle would "
                        "change a live world's rules (ADR 0010 §3.3)");
    }
    // The resolution is what moves it from the CVar into the save.
    ComboContext_Init();
    Rando::Foreign::ResolvePairedProfile(false);
    if (gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[MMRT_KEG_EXPLOSIVES] == 0) {
        return Fail(37, "the profile resolution did not write the trick set into the save");
    }
    if (!MM_TRICK(MMRT_KEG_EXPLOSIVES)) {
        return Fail(38, "MM_TRICK is false for a trick the resolution just froze on");
    }

    // ---- (g) FINDING (a): the Powder Keg gate, non-vacuously ---------------
    // ACTOR_EN_VM (Beamos) is `return (CAN_USE_EXPLOSIVE);` and nothing else, so
    // its verdict IS the macro's. The inventory holds a Powder Keg and the Goron
    // Mask and NO bomb, bombchu or Blast Mask, so the keg disjunct is the only
    // explosive source in play.
    ResetSaveWithEmptyInventory();
    ClearAllTrickCVars();
    GiveInventoryItem(ITEM_POWDER_KEG);
    GiveInventoryItem(ITEM_MASK_GORON);
    SetFrozenTrick(MMRT_KEG_EXPLOSIVES, false);
    if (Rando::Logic::CanKillEnemy(ACTOR_EN_VM)) {
        return Fail(39, "with MMRT_KEG_EXPLOSIVES OFF, a keg-only save still satisfies an explosive-only edge — "
                        "the Powder Keg is ungated, so ADR 0010 O11's beatable(T = empty) rung is unachievable");
    }
    SetFrozenTrick(MMRT_KEG_EXPLOSIVES, true);
    if (!Rando::Logic::CanKillEnemy(ACTOR_EN_VM)) {
        return Fail(40, "with MMRT_KEG_EXPLOSIVES ON, a keg-only save does NOT satisfy an explosive-only edge — the "
                        "gate removed the route instead of gating it");
    }
    // The control: the gate narrowed the KEG leg and nothing else.
    SetFrozenTrick(MMRT_KEG_EXPLOSIVES, false);
    GiveInventoryItem(ITEM_BOMB);
    if (!Rando::Logic::CanKillEnemy(ACTOR_EN_VM)) {
        return Fail(41, "a real bomb no longer satisfies an explosive-only edge — the gate caught the wrong "
                        "disjunct");
    }

    // ---- (h) IDENTITY: the trick set is folded into the digest -------------
    // Modelled as two separate CREATIONS, not an edit to a live pair: under
    // freeze semantics a stamped pair REFUSES a divergent resolution rather than
    // re-stamping, so each probe re-arms an unfrozen carrier first.
    auto restampUnfrozenCarrier = []() {
        ComboContext_Init();
        gComboCtx.sourceIsRando = true;
        gComboCtx.sharedRandoSeed = 0xC0FFEE11u;
        gComboCtx.sharedRandoSettingsHash = 0x5EED0411u;
    };
    Context_SetCurrentGame(GAME_MM);
    ResetSaveWithEmptyInventory();
    ClearAllTrickCVars();
    CVarClear("gRando.ExcludedChecks");
    for (auto& [randoOptionId, optionRow] : Rando::StaticData::Options) {
        (void)randoOptionId;
        CVarClear(optionRow.cvar);
    }

    restampUnfrozenCarrier();
    const uint32_t tricksOffDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (tricksOffDigest == 0) {
        return Fail(42, "the tricks-off paired resolution froze a zero digest");
    }
    const uint32_t tricksOffStamp = MM_Rando_ComputeProfileStamp();
    if (tricksOffStamp != tricksOffDigest) {
        return Fail(43,
                    "creation-side stamp %08X disagrees with the arrival-side resolution %08X under identical "
                    "inputs — every pair would refuse its own arrival",
                    tricksOffStamp, tricksOffDigest);
    }

    CVarSetInteger(Rando::StaticData::Tricks.at(MMRT_KEG_EXPLOSIVES).cvar, 1);
    restampUnfrozenCarrier();
    const uint32_t kegOnDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (kegOnDigest == tricksOffDigest) {
        return Fail(44,
                    "two profiles differing ONLY in the trick set produced the same digest %08X — the frozen "
                    "identity is blind to the generator's trick input (#570; ADR 0009 D1 as amended)",
                    kegOnDigest);
    }
    const uint32_t kegOnStamp = MM_Rando_ComputeProfileStamp();
    if (kegOnStamp != kegOnDigest) {
        return Fail(45, "the two identity sites disagree once a trick is set (%08X vs %08X)", kegOnStamp, kegOnDigest);
    }
    // A DIFFERENT trick must give a different digest again — otherwise the term
    // could be folded as "any trick set at all" rather than as the set itself.
    CVarClear(Rando::StaticData::Tricks.at(MMRT_KEG_EXPLOSIVES).cvar);
    CVarSetInteger(Rando::StaticData::Tricks.at(MMRT_GBT_BOSS_KEY_ICE).cvar, 1);
    restampUnfrozenCarrier();
    const uint32_t gbtOnDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (gbtOnDigest == tricksOffDigest || gbtOnDigest == kegOnDigest) {
        return Fail(46, "the identity term does not distinguish WHICH trick is on (off=%08X keg=%08X gbt=%08X)",
                    tricksOffDigest, kegOnDigest, gbtOnDigest);
    }
    // A RESERVED key must NOT move the digest: it resolves off, so it is a
    // constant term. If it moved the digest, a stale CVar could refuse a healthy
    // pair over a trick MM's logic cannot even consult.
    CVarClear(Rando::StaticData::Tricks.at(MMRT_GBT_BOSS_KEY_ICE).cvar);
    const Rando::StaticData::RandoStaticTrick* reservedRow = NULL;
    for (auto& [mmRandoTrickId, row] : Rando::StaticData::Tricks) {
        (void)mmRandoTrickId;
        if (row.reserved) {
            reservedRow = &row;
            break;
        }
    }
    if (reservedRow == NULL) {
        return Fail(47, "no reserved key found — check (e) already required 20 of them");
    }
    CVarSetInteger(reservedRow->cvar, 1);
    restampUnfrozenCarrier();
    const uint32_t reservedDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (reservedDigest != tricksOffDigest) {
        return Fail(48,
                    "setting reserved trick '%s' moved the profile digest (%08X vs %08X) — a key the logic "
                    "cannot consult must be a constant in the identity",
                    reservedRow->name, reservedDigest, tricksOffDigest);
    }

    // Leave global state clean for whatever runs next.
    ClearAllTrickCVars();
    CVarClear("gRando.ExcludedChecks");
    for (auto& [randoOptionId, optionRow] : Rando::StaticData::Options) {
        (void)randoOptionId;
        CVarClear(optionRow.cvar);
    }
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    Context_SetCurrentGame(prevGame);

    printf("[TEST] PASS: %d trick keys (20 reserved and inert), the descriptor mirror is exact, the predicate reads "
           "the frozen save, the Powder Keg gate moves an explosive-only edge, and the trick set moves the profile "
           "digest\n",
           (int)MMRT_MAX);
    return 0;
}

// ============================================================================
// mm-trick-gbt-gate (rando tier) — finding (b) on the live region graph.
// ============================================================================
extern "C" int MM_TrickGbtGate_RunHeadless(void) {
    printf("[TEST] mm-trick-gbt-gate: the Great Bay Temple boss-key connection is trick-gated (#578 finding (b))\n");

    // The graph is populated by ShipInit registrars, which the caller's MM boot
    // ran. If it is empty, the probe would pass vacuously by never evaluating
    // anything — so say so instead.
    auto regionIt = Rando::Logic::Regions.find(RR_GREAT_BAY_TEMPLE_COMPASS_ROOM);
    if (regionIt == Rando::Logic::Regions.end()) {
        return GateFail(1, "RR_GREAT_BAY_TEMPLE_COMPASS_ROOM is not in the region graph — the ShipInit registrars "
                           "did not run, so this probe would be vacuous");
    }
    auto connectionIt = regionIt->second.connections.find(RR_GREAT_BAY_TEMPLE_COMPASS_ROOM_WITH_BOSS_KEY_CHEST);
    if (connectionIt == regionIt->second.connections.end()) {
        return GateFail(2, "the compass-room -> boss-key-chest connection is gone entirely; upstream 2Ship DELETED "
                           "this edge and the operator ruled sync-AND-GATE, not delete");
    }
    const std::function<bool()>& condition = connectionIt->second.first;

    // A save that satisfies everything the edge asks for EXCEPT the trick:
    // Zora Mask, Bow, Ice Arrows, magic.
    // Heap, not stack: MM's runtime SaveContext is tens of kilobytes and this runs
    // from arbitrary harness stack depths (the same reason
    // ComputeReachableCheckSet heap-allocates its snapshot).
    auto saved = std::make_unique<SaveContext>();
    memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
    INV_CONTENT(ITEM_MASK_ZORA) = ITEM_MASK_ZORA;
    INV_CONTENT(ITEM_BOW) = ITEM_BOW;
    INV_CONTENT(ITEM_ARROW_ICE) = ITEM_ARROW_ICE;
    gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;

    int rc = 0;
    gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[MMRT_GBT_BOSS_KEY_ICE] = 0;
    if (condition()) {
        rc = GateFail(3, "with MMRT_GBT_BOSS_KEY_ICE OFF the boss-key connection is still open — the edge upstream "
                         "2Ship removed pending a trick menu is ungated here");
    }
    if (rc == 0) {
        gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[MMRT_GBT_BOSS_KEY_ICE] = 1;
        if (!condition()) {
            rc = GateFail(4, "with MMRT_GBT_BOSS_KEY_ICE ON the boss-key connection is closed — the route was "
                             "removed rather than gated");
        }
    }
    if (rc == 0) {
        // Without the Ice Arrows the trick alone must not open it: the gate is an
        // extra conjunct, not a replacement for the item requirement.
        INV_CONTENT(ITEM_ARROW_ICE) = ITEM_NONE;
        if (condition()) {
            rc = GateFail(5, "the trick alone opens the connection without Ice Arrows — the gate replaced the item "
                             "requirement instead of joining it");
        }
    }

    memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    if (rc == 0) {
        printf("[TEST] PASS: the GBT boss-key connection is closed with the trick off, open with it on, and still "
               "requires Ice Arrows either way\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
