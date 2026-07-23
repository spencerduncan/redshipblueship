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
 * mm-paired-profile — the profile lock (#499 Tier 1)
 * ============================================================================
 *
 * Drives the REAL `Rando::Foreign::ResolvePairedProfile()` — the same function
 * OnFileCreate calls, not a re-derivation — over a zeroed MM SaveContext, with
 * no fill and no display. It asserts the profile, the logic default's new
 * honour-an-explicit-choice rule, and the `gComboCtx.mmProfileDigest` carve.
 *
 * THE TRAP THIS DESIGN AVOIDS, stated because the obvious test is vacuous: an
 * assertion of the form "set a CVar, generate, observe it in the save" passes
 * TODAY — that path has always worked — and proves nothing about the profile
 * being chosen rather than defaulted. What is asserted instead is the thing
 * that was not true before: that an explicitly-set option survives the paired
 * pin, that an unset one gets the pinned default, and that the two produce
 * DIFFERENT digests.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdio>
#include <cstring>
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

    // ---- paired, nothing chosen: the #426 default applies ------------------
    // Stamp the Lane B carrier the way OoT's producer would, so
    // Combo_ForeignPairingActive() is true for the summary read below.
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE11u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED0411u;

    const uint32_t defaultDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (RANDO_SAVE_OPTIONS[RO_LOGIC] != RO_LOGIC_NEARLY_NO_LOGIC) {
        return Fail(56, "with no explicit choice, the paired profile did not default RO_LOGIC to Nearly No Logic "
                        "(got %u)",
                    (unsigned)RANDO_SAVE_OPTIONS[RO_LOGIC]);
    }
    if (defaultDigest == 0 || gComboCtx.mmProfileDigest != defaultDigest) {
        return Fail(57, "paired resolution did not publish a non-zero digest (returned %08X, ctx holds %08X)",
                    defaultDigest, gComboCtx.mmProfileDigest);
    }

    // Stability: same inputs, same digest. Without this, "the digest detects a
    // mismatch" is unprovable — an unstable digest reports a mismatch between
    // a world and itself.
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

    // ---- paired, logic explicitly chosen: the choice survives --------------
    // This is the behaviour #499 step 3 asked for and the reason the pin moved
    // from "unless already extreme" to "unless the player chose". The old
    // condition could not tell a chosen Glitchless from an untouched key whose
    // StaticData default happens to be Glitchless, and pinned both.
    CVarSetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_GLITCHLESS);
    const uint32_t chosenDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (RANDO_SAVE_OPTIONS[RO_LOGIC] != RO_LOGIC_GLITCHLESS) {
        return Fail(60, "an explicitly chosen RO_LOGIC was overridden by the paired pin (got %u)",
                    (unsigned)RANDO_SAVE_OPTIONS[RO_LOGIC]);
    }
    if (chosenDigest == defaultDigest) {
        return Fail(61, "changing the profile did not change its digest — two different MM worlds would carry one "
                        "identity");
    }

    // ---- a flipped shuffle row also moves the digest -----------------------
    CVarClear(Rando::StaticData::Options[RO_LOGIC].cvar);
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_COWS].cvar, RO_GENERIC_ON);
    const uint32_t cowsDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (RANDO_SAVE_OPTIONS[RO_SHUFFLE_COWS] != RO_GENERIC_ON) {
        return Fail(62, "an explicitly enabled shuffle did not reach RANDO_SAVE_OPTIONS");
    }
    if (cowsDigest == defaultDigest) {
        return Fail(63, "flipping RO_SHUFFLE_COWS did not change the profile digest");
    }

    // ---- the derived correction still applies -----------------------------
    // With skulltulas unshuffled the token requirement is the vanilla one
    // whatever the slider says. Regressing this makes an unreachable check.
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_GOLD_SKULLTULAS].cvar, RO_GENERIC_OFF);
    CVarSetInteger(Rando::StaticData::Options[RO_MINIMUM_SKULLTULA_TOKENS].cvar, 5);
    Rando::Foreign::ResolvePairedProfile(true);
    if (RANDO_SAVE_OPTIONS[RO_MINIMUM_SKULLTULA_TOKENS] != SPIDER_HOUSE_TOKENS_REQUIRED) {
        return Fail(64, "with skulltulas unshuffled the token requirement was not forced to vanilla (got %u)",
                    (unsigned)RANDO_SAVE_OPTIONS[RO_MINIMUM_SKULLTULA_TOKENS]);
    }

    // Leave global state clean for whatever runs next.
    ClearAllOptionCVars();
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    Context_SetCurrentGame(prevGame);

    printf("[TEST] PASS: the paired profile honours an explicit choice, defaults the rest, and publishes a stable "
           "digest that moves when the profile does\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
