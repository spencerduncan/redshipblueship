/**
 * ROM-free locks for MM's randomizer option surface (#497 step 4, #499).
 * CTest label "redship" (display-free); rows `mm-rando-options` and
 * `mm-paired-profile` in src/common/test_runner.cpp.
 *
 * Lives MM-side because it is the ONE place where both tables are in scope:
 * `Rando::StaticData::Options` (MM's, needing MM's headers) and the flat
 * descriptor table the combo pane consumes (`src/common/combo_mm_options_view.h`).
 * Asserting they agree from src/common would require common code to acquire an
 * MM header, which is exactly what the seam exists to avoid.
 *
 * ============================================================================
 * mm-rando-options — the table lock
 * ============================================================================
 *
 * Drives the REAL registrar, not a copy, and asserts:
 *
 *  (a) every `RandoOptionId < RO_MAX` has a `StaticData::Options` row. RED
 *      before this arc: RO_ACCESS_MAJORA_REMAINS was declared and never given
 *      one (47 ids, 46 rows), so the snapshot loop skipped its
 *      RANDO_SAVE_OPTIONS slot and every iteration over the id space was
 *      silently partial.
 *  (b) every id also has a DESCRIPTOR row, and the descriptor's `cvar`,
 *      `name` and `defaultValue` are the StaticData row's own. This is the
 *      lock that matters: a pane bound to a key MM does not read is a control
 *      that flips a CVar and changes nothing — ADR 0004 §5's vacuous gate, in
 *      the form hardest to notice, because the widget works.
 *  (c) the emitted set covers the table EXACTLY — no duplicate ids, no rows
 *      for ids outside the table, count equal on both sides.
 *  (d) capability-gating honesty, both directions: a PARTIAL/DORMANT row must
 *      carry a non-empty reason, and a LIVE/GENERATION_ONLY row must carry an
 *      empty one. A dormant row with no reason reads as a broken port; a live
 *      row with a reason reads as broken and is not.
 *  (e) widget well-formedness: combo rows have >= 2 value labels and a default
 *      inside them, slider/time rows have min < max and a default inside them.
 *      A combo whose default sits outside its label array indexes out of
 *      bounds in the pane's preview.
 *  (f) the value accessors round-trip through the real CVar store, and clamp:
 *      an out-of-range write must not reach RANDO_SAVE_OPTIONS, which is
 *      indexed by generation code.
 *
 * ============================================================================
 * mm-paired-profile — the profile lock (#499 Tier 1; #498/#564 phase 2)
 * ============================================================================
 *
 * Drives the REAL `Rando::Foreign::ResolvePairedProfile()` — the same function
 * OnFileCreate calls, not a re-derivation — over a zeroed MM SaveContext, with
 * no fill and no display. It asserts the profile, the logic default's
 * honour-an-explicit-choice rule, and the FREEZE semantics the #564 ruling
 * made mandatory:
 *
 *  - an unstamped (legacy) pair freezes its profile at resolution;
 *  - a stamped pair REFUSES a divergent resolution (throws, never overwrites
 *    the stamp — reverting the compare turns the refusal assertions red);
 *  - the creation-side computation (MM_Rando_ComputeProfileStamp, what OoT's
 *    Playthrough_Init stamps) and the arrival-side resolution agree bit-exact
 *    under identical inputs — the two sites cannot drift;
 *  - the identity term is WIDER than the option table (#564 V4): a
 *    gRando.ExcludedChecks edit moves the digest even though no option value
 *    changed. Under the pre-widening digest that assertion is red.
 *
 * THE TRAP THIS DESIGN AVOIDS, stated because the obvious test is vacuous: an
 * assertion of the form "set a CVar, generate, observe it in the save" passes
 * TODAY — that path has always worked — and proves nothing about the profile
 * being chosen rather than defaulted. What is asserted instead is behavior
 * that was not true before: explicit choices survive the pin, divergence
 * refuses, and every generation input moves the digest.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Foreign.h"
#include "Rando/StaticData/StaticData.h"

// src/common — outside any extern "C" block; these headers manage their own
// linkage (matching Foreign.cpp).
#include "combo_mm_options_view.h"
#include "foreign_items.h"

extern "C" {
#include "variables.h"
// SPIDER_HOUSE_TOKENS_REQUIRED — the vanilla token requirement the derived
// correction falls back to.
#include "overlays/actors/ovl_En_Sth/z_en_sth.h"
}

namespace {

int Fail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-RANDO-OPTIONS] FAIL(%d): ", code);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    return code;
}

/** Clear every option CVar so a run starts from "nobody has chosen anything". */
void ClearAllOptionCVars() {
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        (void)randoOptionId;
        CVarClear(row.cvar);
    }
}

} // namespace

extern "C" int MM_RandoOptions_RunHeadless(void) {
    printf("[TEST] mm-rando-options: MM's 47 rando options have rows, labels, honest gating, and CVar-bound "
           "widgets (#497 step 4, #499 step 5)\n");

    // Drive the real registrar. Combo_MMOptionsWindow_Init also calls this;
    // calling it directly is what keeps this test a lock on the TABLE rather
    // than on the window.
    MM_RandoOptionsUi_Register();

    // ---- (a) every id has a StaticData row --------------------------------
    // This is the 47-vs-46 skew. It must be checked over the ENUM, not over the
    // map: iterating the map can only ever find rows that exist.
    for (int id = 0; id < RO_MAX; id++) {
        if (Rando::StaticData::Options.find((RandoOptionId)id) == Rando::StaticData::Options.end()) {
            return Fail(23, "RandoOptionId %d has no Rando::StaticData::Options row (the 47-vs-46 skew)", id);
        }
    }
    if ((int)Rando::StaticData::Options.size() != RO_MAX) {
        return Fail(24, "Options table holds %d rows for %d ids", (int)Rando::StaticData::Options.size(), (int)RO_MAX);
    }

    // ---- (b)(c) descriptor coverage, exactness, and binding ---------------
    const int descCount = Combo_MMOptionCount();
    if (descCount != RO_MAX) {
        return Fail(25, "descriptor table holds %d rows for %d option ids — the pane would silently omit options",
                    descCount, (int)RO_MAX);
    }

    bool seen[RO_MAX] = { false };
    for (int i = 0; i < descCount; i++) {
        const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
        if (desc == NULL) {
            return Fail(26, "descriptor index %d is NULL below the reported count", i);
        }
        if (desc->id >= (uint16_t)RO_MAX) {
            return Fail(27, "descriptor %d carries id %u, outside the option id space", i, (unsigned)desc->id);
        }
        if (seen[desc->id]) {
            return Fail(28, "descriptor id %u appears twice — the pane would draw two widgets on one CVar",
                        (unsigned)desc->id);
        }
        seen[desc->id] = true;

        const auto& row = Rando::StaticData::Options.at((RandoOptionId)desc->id);
        if (desc->cvar == NULL || strcmp(desc->cvar, row.cvar) != 0) {
            return Fail(29, "descriptor for %s binds cvar '%s', but MM reads '%s'", row.name,
                        desc->cvar ? desc->cvar : "(null)", row.cvar);
        }
        if (desc->name == NULL || strcmp(desc->name, row.name) != 0) {
            return Fail(30, "descriptor for %s carries name '%s'", row.name, desc->name ? desc->name : "(null)");
        }
        if (desc->defaultValue != (int32_t)row.defaultValue) {
            return Fail(31, "descriptor for %s carries default %d, StaticData says %u", row.name,
                        (int)desc->defaultValue, (unsigned)row.defaultValue);
        }
        if (desc->label == NULL || desc->label[0] == '\0') {
            return Fail(32, "descriptor for %s has no human label", row.name);
        }
        if (desc->tooltip == NULL) {
            return Fail(33, "descriptor for %s has a NULL tooltip (ImGui would be handed an invalid pointer)",
                        row.name);
        }
        if (desc->group >= (uint8_t)COMBO_MM_GROUP_COUNT) {
            return Fail(34, "descriptor for %s is in group %u, outside the taxonomy", row.name,
                        (unsigned)desc->group);
        }

        // ---- (d) capability-gating honesty, BOTH directions ---------------
        const bool blocked =
            desc->liveness == COMBO_MM_LIVENESS_PARTIAL || desc->liveness == COMBO_MM_LIVENESS_DORMANT;
        if (desc->disabledReason == NULL) {
            return Fail(35, "descriptor for %s has a NULL reason string", row.name);
        }
        if (blocked && desc->disabledReason[0] == '\0') {
            return Fail(36, "%s is gated (liveness %u) but carries no reason — the row would read as broken",
                        row.name, (unsigned)desc->liveness);
        }
        if (!blocked && desc->disabledReason[0] != '\0') {
            return Fail(37, "%s is live but carries a disabled reason ('%s')", row.name, desc->disabledReason);
        }

        // ---- (e) widget well-formedness ------------------------------------
        switch ((ComboMMOptionWidget)desc->widget) {
            case COMBO_MM_WIDGET_CHECKBOX:
                if (desc->defaultValue < 0 || desc->defaultValue > 1) {
                    return Fail(38, "%s is a checkbox with default %d", row.name, (int)desc->defaultValue);
                }
                break;
            case COMBO_MM_WIDGET_COMBO:
                if (desc->valueLabels == NULL || desc->valueCount < 2) {
                    return Fail(39, "%s is a combo with %u value labels", row.name, (unsigned)desc->valueCount);
                }
                if (desc->defaultValue < 0 || desc->defaultValue >= (int32_t)desc->valueCount) {
                    return Fail(40, "%s defaults to %d, outside its %u value labels — the pane would index out "
                                    "of bounds drawing the preview",
                                row.name, (int)desc->defaultValue, (unsigned)desc->valueCount);
                }
                for (int v = 0; v < (int)desc->valueCount; v++) {
                    if (desc->valueLabels[v] == NULL || desc->valueLabels[v][0] == '\0') {
                        return Fail(41, "%s value label %d is empty", row.name, v);
                    }
                }
                break;
            case COMBO_MM_WIDGET_SLIDER:
            case COMBO_MM_WIDGET_TIME:
                if (desc->minValue >= desc->maxValue) {
                    return Fail(42, "%s is a slider with range [%d, %d]", row.name, (int)desc->minValue,
                                (int)desc->maxValue);
                }
                if (desc->defaultValue < desc->minValue || desc->defaultValue > desc->maxValue) {
                    return Fail(43, "%s defaults to %d, outside its range [%d, %d]", row.name,
                                (int)desc->defaultValue, (int)desc->minValue, (int)desc->maxValue);
                }
                break;
            default:
                return Fail(44, "%s carries widget kind %u", row.name, (unsigned)desc->widget);
        }
    }
    for (int id = 0; id < RO_MAX; id++) {
        if (!seen[id]) {
            return Fail(45, "option id %d (%s) has no descriptor row — the pane cannot show it", id,
                        Rando::StaticData::Options.at((RandoOptionId)id).name);
        }
    }

    // ---- the RO_ACCESS_MAJORA_REMAINS retirement (ADR 0010 answer O1) -----
    // Operator-accepted 2026-07-31: the dead row is RETIRED — it never gains a
    // consumer and never becomes a working control — while the always-zero
    // StaticData row stays as a save-format tombstone and the pane keeps
    // drawing it disabled-with-reason. Pinned here so "implementing" it one
    // day is a deliberate act against a red test naming the ruling, never an
    // accidental finish of what looked like a TODO. Counterfactual: flip the
    // descriptor's liveness to LIVE (or drop the retirement wording from its
    // reason) and this goes red.
    {
        const ComboMMOptionDesc* retired = Combo_MMOptionById((uint16_t)RO_ACCESS_MAJORA_REMAINS);
        if (retired == NULL) {
            return Fail(69, "RO_ACCESS_MAJORA_REMAINS has no descriptor");
        }
        if (retired->liveness != COMBO_MM_LIVENESS_DORMANT) {
            return Fail(70, "RO_ACCESS_MAJORA_REMAINS is liveness %u — the operator retired this row (ADR 0010 "
                            "answer O1); it must stay a dormant tombstone, never a working control",
                        (unsigned)retired->liveness);
        }
        if (retired->disabledReason == NULL || strstr(retired->disabledReason, "Retired") == NULL) {
            return Fail(71, "RO_ACCESS_MAJORA_REMAINS's reason ('%s') no longer names the retirement — the pane "
                            "would present an operator-retired row as merely unfinished",
                        retired->disabledReason ? retired->disabledReason : "(null)");
        }
    }

    // ---- (f) the value accessors actually reach the CVar store ------------
    {
        const ComboMMOptionDesc* logic = Combo_MMOptionById((uint16_t)RO_LOGIC);
        if (logic == NULL) {
            return Fail(46, "RO_LOGIC has no descriptor");
        }
        CVarClear(logic->cvar);
        if (Combo_MMOptionIsExplicit(logic)) {
            return Fail(47, "RO_LOGIC reads as explicitly set after CVarClear");
        }
        if (Combo_MMOptionGetValue(logic) != logic->defaultValue) {
            return Fail(48, "an unset option does not read back its default");
        }
        Combo_MMOptionSetValue(logic, RO_LOGIC_VANILLA);
        if (!Combo_MMOptionIsExplicit(logic) || Combo_MMOptionGetValue(logic) != RO_LOGIC_VANILLA) {
            return Fail(49, "a written option does not read back its value");
        }
        // Clamping: RO_LOGIC has 4 values, so 99 must land on 3, not on 99.
        // An unclamped write reaches RANDO_SAVE_OPTIONS and is then used as an
        // index by generation code.
        Combo_MMOptionSetValue(logic, 99);
        if (Combo_MMOptionGetValue(logic) != (int32_t)logic->valueCount - 1) {
            return Fail(50, "an over-range combo write was not clamped (read back %d)",
                        (int)Combo_MMOptionGetValue(logic));
        }
        Combo_MMOptionSetValue(logic, -7);
        if (Combo_MMOptionGetValue(logic) != 0) {
            return Fail(51, "an under-range combo write was not clamped (read back %d)",
                        (int)Combo_MMOptionGetValue(logic));
        }
        Combo_MMOptionClear(logic);
        if (Combo_MMOptionIsExplicit(logic)) {
            return Fail(52, "Combo_MMOptionClear left the option explicitly set");
        }
    }

    ClearAllOptionCVars();
    printf("[TEST] PASS: %d option ids all have StaticData rows and descriptor rows, every widget is bound to the "
           "row's own cvar, and every gated row names its reason\n",
           (int)RO_MAX);
    return 0;
}

extern "C" int MM_PairedProfile_RunHeadless(void) {
    printf("[TEST] mm-paired-profile: the real ResolvePairedProfile decides the paired MM profile and publishes its "
           "digest (#499 steps 2-4)\n");

    const GameId prevGame = Context_GetCurrentGame();
    ClearAllOptionCVars();

    // A zeroed MM SaveContext, no fill, no display — the whole point of
    // extracting the resolver is that this is enough to drive it.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();

    // ---- unpaired: options resolve, nothing else happens -------------------
    Rando::Foreign::ResolvePairedProfile(false);
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        if (RANDO_SAVE_OPTIONS[randoOptionId] != row.defaultValue &&
            !(randoOptionId == RO_MINIMUM_SKULLTULA_TOKENS)) {
            return Fail(53, "unpaired resolution wrote %u to %s, expected its default %u",
                        (unsigned)RANDO_SAVE_OPTIONS[randoOptionId], row.name, (unsigned)row.defaultValue);
        }
    }
    if (RANDO_SAVE_OPTIONS[RO_LOGIC] != RO_LOGIC_GLITCHLESS) {
        return Fail(54, "unpaired resolution applied the paired logic pin (RO_LOGIC = %u)",
                    (unsigned)RANDO_SAVE_OPTIONS[RO_LOGIC]);
    }
    if (gComboCtx.mmProfileDigest != 0) {
        // A solo file claiming a paired identity is the mispairing class ADR
        // 0002 exists to prevent, arriving from the cheapest possible mistake.
        return Fail(55, "unpaired resolution stamped a profile digest (%08X)", gComboCtx.mmProfileDigest);
    }

    // Every paired probe below starts from a FRESH, unstamped pairing carrier:
    // under freeze semantics a stamped pair refuses a different profile rather
    // than re-stamping, so digest-motion probes must model what they are — a
    // NEW creation under new inputs, not an edit to a live pair.
    auto restampUnfrozenCarrier = []() {
        ComboContext_Init();
        gComboCtx.sourceIsRando = true;
        gComboCtx.sharedRandoSeed = 0xC0FFEE11u;
        gComboCtx.sharedRandoSettingsHash = 0x5EED0411u;
    };
    CVarClear("gRando.ExcludedChecks");

    // ---- paired, nothing chosen: the ADR 0010 default applies --------------
    // Increment 1.1's flip (lock a of the increment's CI set): the paired MM
    // world defaults to GLITCHLESS — beatable by MM's own logic — superseding
    // the #426 Nearly No Logic MVP default. Counterfactual: restore the old
    // NNL pin in ResolveProfileValues and this goes red.
    restampUnfrozenCarrier();
    const uint32_t defaultDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (RANDO_SAVE_OPTIONS[RO_LOGIC] != RO_LOGIC_GLITCHLESS) {
        return Fail(56, "with no explicit choice, the paired profile did not default RO_LOGIC to Glitchless "
                        "(ADR 0010 increment 1.1; got %u)",
                    (unsigned)RANDO_SAVE_OPTIONS[RO_LOGIC]);
    }
    if (defaultDigest == 0 || gComboCtx.mmProfileDigest != defaultDigest) {
        return Fail(57, "unstamped paired resolution did not freeze a non-zero digest (returned %08X, ctx holds "
                        "%08X)",
                    defaultDigest, gComboCtx.mmProfileDigest);
    }

    // ---- the creation site and the arrival site are ONE computation --------
    // MM_Rando_ComputeProfileStamp is what OoT's Playthrough_Init stamps at
    // the creation event; ResolvePairedProfile is what the arrival resolves.
    // If they can disagree under identical inputs, every post-freeze pair
    // refuses its own arrival — the falsifiable no-drift lock (#498/#564).
    const uint32_t creationStamp = MM_Rando_ComputeProfileStamp();
    if (creationStamp != defaultDigest) {
        return Fail(65, "creation-side stamp %08X disagrees with the arrival-side resolution %08X under identical "
                        "inputs — every pair would refuse its own arrival",
                    creationStamp, defaultDigest);
    }

    // Stability: same inputs, same digest, no refusal. Without this, "the
    // digest detects a mismatch" is unprovable — an unstable digest reports a
    // mismatch between a world and itself.
    const uint32_t repeatDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (repeatDigest != defaultDigest) {
        return Fail(58, "the profile digest is not stable across runs (%08X then %08X)", defaultDigest,
                    repeatDigest);
    }

    // The summary the pane reads must agree with the context.
    ComboMMProfileSummary summary;
    Combo_MMProfileSummary(&summary);
    if (!summary.paired || summary.mmProfileDigest != defaultDigest) {
        return Fail(59, "the pane's summary disagrees with gComboCtx (paired=%d digest=%08X)", summary.paired ? 1 : 0,
                    summary.mmProfileDigest);
    }

    // ---- STAMPED pair + divergent inputs: REFUSED, never re-stamped --------
    // The freeze lock itself (#564 V8: the digest acquires comparators).
    // gComboCtx still carries defaultDigest; choosing a different logic mode
    // now models a post-creation edit reaching generation. The resolution must
    // THROW (OnFileCreate's catch turns that into the vanilla revert — no
    // divergent world is authored) and must NOT self-heal the stamp. Revert
    // the compare in ResolvePairedProfile and both halves go red. (Nearly No
    // Logic here because the default is now Glitchless — an explicit
    // Glitchless choice would RESOLVE identically to the frozen default and
    // diverge nothing.)
    CVarSetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_NEARLY_NO_LOGIC);
    {
        bool refused = false;
        try {
            Rando::Foreign::ResolvePairedProfile(true);
        } catch (const std::exception&) {
            refused = true;
        }
        if (!refused) {
            return Fail(66, "a stamped pair accepted a DIVERGENT profile resolution — post-creation edits would be "
                            "honored instead of refused (#564)");
        }
        if (gComboCtx.mmProfileDigest != defaultDigest) {
            return Fail(67, "the refusal self-healed the creation stamp (%08X -> %08X) — divergence must never "
                            "rewrite identity",
                        defaultDigest, gComboCtx.mmProfileDigest);
        }
    }

    // ---- paired, logic explicitly chosen: the choice survives --------------
    // This is the behaviour #499 step 3 asked for and the reason the pin moved
    // from "unless already extreme" to "unless the player chose". Under ADR
    // 0010 increment 1.1 the leg doubles as the "choosing away the proof is
    // legitimate" lock: an explicit NEARLY NO LOGIC choice — a no-logic mode,
    // exactly what the flipped Glitchless default would otherwise override —
    // must survive the pin and move the digest. Counterfactual: make the pin
    // unconditional (a law instead of a default) and Fail(60) goes red. A
    // fresh carrier: this is a NEW creation under the chosen logic. (The CVar
    // still holds NNL from the divergence leg above.)
    restampUnfrozenCarrier();
    const uint32_t chosenDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (RANDO_SAVE_OPTIONS[RO_LOGIC] != RO_LOGIC_NEARLY_NO_LOGIC) {
        return Fail(60, "an explicitly chosen no-logic RO_LOGIC was overridden by the paired Glitchless pin "
                        "(got %u) — choosing away the proof is legitimate (ADR 0010 increment 1.1)",
                    (unsigned)RANDO_SAVE_OPTIONS[RO_LOGIC]);
    }
    if (chosenDigest == defaultDigest) {
        return Fail(61, "changing the profile did not change its digest — two different MM worlds would carry one "
                        "identity");
    }

    // ---- a flipped shuffle row also moves the digest -----------------------
    CVarClear(Rando::StaticData::Options[RO_LOGIC].cvar);
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_COWS].cvar, RO_GENERIC_ON);
    restampUnfrozenCarrier();
    const uint32_t cowsDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (RANDO_SAVE_OPTIONS[RO_SHUFFLE_COWS] != RO_GENERIC_ON) {
        return Fail(62, "an explicitly enabled shuffle did not reach RANDO_SAVE_OPTIONS");
    }
    if (cowsDigest == defaultDigest) {
        return Fail(63, "flipping RO_SHUFFLE_COWS did not change the profile digest");
    }

    // ---- the WIDENED identity term (#564 V4) -------------------------------
    // gRando.ExcludedChecks shapes the generated world (GeneratePools reads it
    // at fill time) but is not an option row, so the pre-widening digest could
    // not see it: same seed + same digest, different world — a vacuous guard.
    // With every option back at its default, an excluded-check edit ALONE must
    // move the digest. Narrow the identity back to the option table and this
    // goes red.
    ClearAllOptionCVars();
    CVarSetString("gRando.ExcludedChecks", "12,34");
    restampUnfrozenCarrier();
    const uint32_t excludedDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (excludedDigest == defaultDigest) {
        return Fail(68, "editing gRando.ExcludedChecks did not change the profile digest — the identity term is "
                        "narrower than the generator's input set (#564 V4)");
    }
    CVarClear("gRando.ExcludedChecks");

    // ---- the derived correction still applies -----------------------------
    // With skulltulas unshuffled the token requirement is the vanilla one
    // whatever the slider says. Regressing this makes an unreachable check.
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_GOLD_SKULLTULAS].cvar, RO_GENERIC_OFF);
    CVarSetInteger(Rando::StaticData::Options[RO_MINIMUM_SKULLTULA_TOKENS].cvar, 5);
    restampUnfrozenCarrier();
    Rando::Foreign::ResolvePairedProfile(true);
    if (RANDO_SAVE_OPTIONS[RO_MINIMUM_SKULLTULA_TOKENS] != SPIDER_HOUSE_TOKENS_REQUIRED) {
        return Fail(64, "with skulltulas unshuffled the token requirement was not forced to vanilla (got %u)",
                    (unsigned)RANDO_SAVE_OPTIONS[RO_MINIMUM_SKULLTULA_TOKENS]);
    }

    // Leave global state clean for whatever runs next.
    ClearAllOptionCVars();
    CVarClear("gRando.ExcludedChecks");
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    Context_SetCurrentGame(prevGame);

    printf("[TEST] PASS: the paired profile honours an explicit choice, freezes at creation, refuses divergence "
           "without self-healing, and its widened digest moves with every generation input\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
