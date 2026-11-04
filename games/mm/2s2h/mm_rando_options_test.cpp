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
 *  (g) no gated row blames a hook type that dispatches (#669). This is the
 *      anti-staleness lock, and it exists because the defect recurred: a
 *      reason names a hook as its blocker, the hook later gains an MM dispatch
 *      point, and the row stays disabled for months citing a cause that is
 *      gone. kReasonActorInitDrop did it once (#438's remainder) and
 *      kReasonOpenText did it again to thirteen rows (#669). The dispatched set
 *      is not transcribed here — each entry takes the address of the
 *      MM_GameHooks_Execute* bridge that dispatches it, so the linker proves
 *      the claim on every build and the list cannot rot the way the reasons
 *      did. Check (d) already said a gated row must explain itself; (g) says
 *      the explanation must be true.
 *  (h) every option sits in SoH's randomizer vocabulary (#1101, option C):
 *      the group taxonomy is SoH's Randomizer headers, in SoH's order (Logic,
 *      Win Condition, Area Access, Shuffle Items, Shuffle Shops & Merchants,
 *      Additional Items, Traps, Hints, Starting Items), and each id is under
 *      the header its OoT twin uses. Spelled here by NAME, independently of
 *      the enum, so a regrouping that renames or drops a header turns red.
 *
 * ============================================================================
 * mm-paired-profile — the profile lock (#499 Tier 1; #498/#564 phase 2)
 * ============================================================================
 *
 * Drives the REAL `Rando::Foreign::ResolvePairedProfile()` — the same function
 * OnFileCreate calls, not a re-derivation — over a zeroed MM SaveContext, with
 * no fill and no display. It asserts the profile, the paired MM Logic
 * derived from [Both Games] Logic (#1367: Glitchless under a proving rung, No
 * Logic under No Logic, whatever MM's own key holds, while a world created
 * before keeps its own), and the FREEZE semantics the #564 ruling made
 * mandatory:
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
 * that was not true before: the rung decides MM's Logic, divergence refuses,
 * and every generation input moves the digest.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Foreign.h"
#include "Rando/StaticData/StaticData.h"

// src/common — outside any extern "C" block; these headers manage their own
// linkage (matching Foreign.cpp).
#include "combo_mm_options_view.h"
#include "combo_settings_view.h" // COMBO_SETTING_LOGIC: [Both Games] Logic (#1367)
#include "foreign_items.h"
#include "shared_items.h" // ComboItemClassRow, RSBS_FILL_ARM_CLOCKS* (#1437)

extern "C" {
#include "variables.h"
// Foreign.cpp (#1367): the stamp a world created before [Both Games] Logic carried.
uint32_t MM_Rando_ComputeLegacyProfileStampForTest(void);
// ComboLogicEngineSingleExe.cpp (#1437): MM's classifier, for the clock rows' arming.
int MM_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
// SPIDER_HOUSE_TOKENS_REQUIRED — the vanilla token requirement the derived
// correction falls back to.
#include "overlays/actors/ovl_En_Sth/z_en_sth.h"
}

namespace {

// ---------------------------------------------------------------------------
// THE DISPATCHED HOOK TYPES (#669) — the set a reason string may not name.
//
// Every stale reason this table has ever carried failed the same way: it named
// a hook type as the blocker, someone later gave that hook type an MM dispatch
// point, and the reason went on telling the player a lie no test could see.
// That happened to kReasonActorInitDrop (#438's remainder) and then to
// kReasonOpenText and the whole PARTIAL batch around it (#669). Two rounds of
// the same defect is a missing lock, not bad luck.
//
// So: a PARTIAL/DORMANT row may not name a hook type that dispatches. The set
// below is not a hand-copied list of names — each row takes the ADDRESS of the
// MM_GameHooks_Execute* bridge that dispatches that hook type, so the claim
// "this hook type has an MM dispatch point" is checked by the LINKER on every
// build. Delete a bridge and this TU stops linking; rename one and it stops
// compiling. Neither can drift quietly, which is the whole point.
//
// The bridges are declared in 2s2h/GameInteractor/GameInteractor.h (force-
// included into every MM C++ TU) and games/mm/include/mm_game_hooks.h. A
// handful of dispatched hook types reach S2H::GameHooks through differently
// spelled entry points that no header this TU sees declares (the MM-only
// names, and the ones fixed in place rather than bridged); they are listed
// after the macro with a null bridge, since the lock still has to reject a
// reason naming them. Derived from the dispatch points in
// games/mm/2s2h/GameExports_SingleExe.cpp, which is the same source of truth
// the mm-hook-dispatch row drives.
// ---------------------------------------------------------------------------
#define MM_BRIDGED_HOOK_TYPES(X)                                    \
    X(AfterCameraUpdate, AfterCameraUpdate)                         \
    X(AfterEndOfCycleSave, AfterEndOfCycleSave)                     \
    X(AfterInterfaceClockDraw, AfterInterfaceClockDraw)             \
    X(AfterKaleidoDrawPage, AfterKaleidoDrawPage)                   \
    X(BeforeEndOfCycleSave, BeforeEndOfCycleSave)                   \
    X(BeforeInterfaceClockDraw, BeforeInterfaceClockDraw)           \
    X(BeforeKaleidoDrawPage, BeforeKaleidoDrawPage)                 \
    X(OnActorDestroy, OnActorDestroy)                               \
    X(OnActorDraw, OnActorDraw)                                     \
    X(OnActorInit, OnActorInit)                                     \
    X(OnActorKill, OnActorKill)                                     \
    X(OnBossDefeated, OnBossDefeated)                               \
    X(OnBottleContentsUpdate, OnBottleContentsUpdate)               \
    X(OnCameraChangeModeFlags, OnCameraChangeModeFlags)             \
    X(OnCameraChangeSettingsFlags, OnCameraChangeSettingsFlags)     \
    X(OnConsoleLogoUpdate, OnConsoleLogoUpdate)                     \
    X(OnFileSelectSaveLoad, OnFileSelectSaveLoad)                   \
    X(OnGameCompletion, OnGameCompletion)                           \
    X(OnGameStateMainFinish, OnGameStateMainFinish)                 \
    X(OnGameStateMainStart, OnGameStateMainStart)                   \
    X(OnInterfaceDrawStart, OnInterfaceDrawStart)                   \
    X(OnItemGive, OnItemGive)                                       \
    X(OnKaleidoUpdate, OnKaleidoUpdate)                             \
    X(OnOpenText, OnOpenText)                                       \
    X(OnPlayDestroy, OnPlayDestroy)                                 \
    X(OnPlayDrawWorldEnd, OnPlayDrawWorldEnd)                       \
    X(OnPlayerPostLimbDraw, OnPlayerPostLimbDraw)                   \
    X(OnSeqPlayerInit, OnSeqPlayerInit)                             \
    X(ShouldActorDraw, ShouldActorDraw)                             \
    X(ShouldActorInit, ShouldActorInit)                             \
    X(ShouldActorUpdate, ShouldActorUpdate)                         \
    X(ShouldItemGive, ShouldItemGive)                               \
    /* The one whose bridge is not spelled after its hook type. */  \
    X(ShouldVanillaBehavior, VBShould)

struct DispatchedHook {
    const char* name;
    /** The bridge that dispatches it, or NULL when this TU has no declaration. */
    const void* bridge;
};

const DispatchedHook kDispatchedHooks[] = {
#define MM_DISPATCHED_HOOK_ROW(hookType, bridge) \
    { #hookType, reinterpret_cast<const void*>(&MM_GameHooks_Execute##bridge) },
    MM_BRIDGED_HOOK_TYPES(MM_DISPATCHED_HOOK_ROW)
#undef MM_DISPATCHED_HOOK_ROW
    // Dispatched, but not through a bridge declared where this TU can see it.
    // AfterRoomSceneCommands / OnRoomInit were fixed in place (their MM-side
    // definitions walked the wrong registry, #438); the rest are MM-only names
    // defined directly in GameExports_SingleExe.cpp.
    { "AfterRoomSceneCommands", NULL },
    { "BeforeMoonCrashSaveReset", NULL },
    { "OnActorUpdate", NULL },
    { "OnFlagSet", NULL },
    { "OnGameStateDrawFinish", NULL },
    { "OnGameStateUpdate", NULL },
    { "OnPassPlayerInputs", NULL },
    { "OnRoomInit", NULL },
    { "OnSaveInit", NULL },
    { "OnSaveLoad", NULL },
    { "OnSceneFlagSet", NULL },
    { "OnSceneInit", NULL },
};

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

    // Drive the real registrar. Combo_MMOptionsPages_Init also calls this;
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

    // ---- the dispatched-hook table is real, not a wish list (#669) --------
    // Reading the addresses is what makes the linker resolve every bridge in
    // MM_BRIDGED_HOOK_TYPES; without a runtime use the table could be folded
    // away and the "the linker proves it" claim in check (g) would be empty.
    for (const DispatchedHook& hook : kDispatchedHooks) {
        if (hook.name == NULL || hook.name[0] == '\0') {
            return Fail(74, "the dispatched-hook table has an unnamed entry");
        }
    }

    // ---- (b)(c) descriptor coverage, exactness, and binding ---------------
    // Every RO_* option, plus the combo's own MM rows (#1069's Moon Crash
    // Behavior, #1175's Keep Items on Cycle Reset), which no RandoStaticOption
    // row backs.
    const int kComboOwnRows = 2;
    const int descCount = Combo_MMOptionCount();
    if (descCount != RO_MAX + kComboOwnRows) {
        return Fail(25, "descriptor table holds %d rows for %d option ids and %d combo rows — the pane would "
                        "silently omit options",
                    descCount, (int)RO_MAX, kComboOwnRows);
    }

    bool seen[RO_MAX] = { false };
    int moonCrashAt = -1;
    int keepItemsAt = -1;
    for (int i = 0; i < descCount; i++) {
        const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
        if (desc == NULL) {
            return Fail(26, "descriptor index %d is NULL below the reported count", i);
        }
        if (desc->id == (uint16_t)RSBS_MM_OPTION_ID_MOON_CRASH) {
            // #1069: OoTMM's moonCrash, the combo's own row. No StaticData row to
            // compare with, so its binding is checked against the one spelling.
            if (desc->cvar == NULL || strcmp(desc->cvar, RSBS_CVAR_MM_MOON_CRASH) != 0 ||
                desc->defaultValue != RSBS_MM_MOON_CRASH_RESET || desc->widget != (uint8_t)COMBO_MM_WIDGET_COMBO ||
                desc->valueCount != 2 || desc->valueLabels == NULL || desc->label == NULL || desc->tooltip == NULL ||
                desc->liveness != (uint8_t)COMBO_MM_LIVENESS_LIVE || desc->disabledReason == NULL ||
                desc->disabledReason[0] != '\0') {
                return Fail(77, "the Moon Crash Behavior row does not bind %s, default Last Save, two values, live",
                            RSBS_CVAR_MM_MOON_CRASH);
            }
            moonCrashAt = i;
            continue;
        }
        if (desc->id == (uint16_t)RSBS_MM_OPTION_ID_KEEP_ITEMS_RESET) {
            // #1175: OoTMM's keepItemsReset ("Keep Items on Cycle Reset", a
            // boolean, default off), the combo's own row, drawn right after Moon
            // Crash Behavior in its group. Checked against the one spelling.
            if (desc->cvar == NULL || strcmp(desc->cvar, RSBS_CVAR_MM_KEEP_ITEMS_RESET) != 0 ||
                desc->defaultValue != 0 || desc->widget != (uint8_t)COMBO_MM_WIDGET_CHECKBOX || desc->label == NULL ||
                strcmp(desc->label, "Keep Items on Cycle Reset") != 0 || desc->tooltip == NULL ||
                desc->tooltip[0] == '\0' || desc->liveness != (uint8_t)COMBO_MM_LIVENESS_LIVE ||
                desc->disabledReason == NULL || desc->disabledReason[0] != '\0') {
                return Fail(79, "the Keep Items on Cycle Reset row does not bind %s, a checkbox, default off, live",
                            RSBS_CVAR_MM_KEEP_ITEMS_RESET);
            }
            keepItemsAt = i;
            continue;
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

        // ---- (g) a reason may not blame a hook that dispatches (#669) ------
        // The staleness lock. Twice now a reason has outlived its blocker by
        // naming a hook type that later gained an MM dispatch point, and both
        // times the row stayed disabled for months after the thing it blamed
        // was fixed. A reason naming a live hook is not a small inaccuracy: it
        // is the pane telling the player an option is unavailable for a cause
        // that no longer exists, and there is no way to notice by reading it.
        //
        // Counterfactual: restore kReasonOpenText ("...MM OnOpenText dispatch
        // not placed") on any gated row and this goes red on the name.
        if (blocked) {
            for (const DispatchedHook& hook : kDispatchedHooks) {
                if (strstr(desc->disabledReason, hook.name) != NULL) {
                    return Fail(72,
                                "%s is gated with a reason naming '%s', but that hook type HAS an MM dispatch "
                                "point — the row is stale, not blocked (reason: '%s')",
                                row.name, hook.name, desc->disabledReason);
                }
            }
            // #438 is the dispatch-coverage tracker, and it is complete: every
            // hook type MM registers on now has a dispatch point. So a reason
            // citing it is stale by construction, whatever else it says —
            // which catches the paraphrase the name check above would miss
            // ("needs the MM hook dispatch from #438").
            if (strstr(desc->disabledReason, "#438") != NULL) {
                return Fail(73,
                            "%s is gated with a reason citing #438 ('%s'), but #438's dispatch surface is "
                            "complete — name the real blocker instead of the tracker",
                            row.name, desc->disabledReason);
            }
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
    // #1175: Keep Items on Cycle Reset is the row after Moon Crash Behavior, in
    // the same group (the page draws a group's rows in table order).
    if (keepItemsAt < 0 || moonCrashAt < 0 || keepItemsAt != moonCrashAt + 1 ||
        Combo_MMOptionAt(keepItemsAt)->group != Combo_MMOptionAt(moonCrashAt)->group) {
        return Fail(80, "Keep Items on Cycle Reset is not the row after Moon Crash Behavior in its group (at %d, %d)",
                    keepItemsAt, moonCrashAt);
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

    // ---- (h) SoH's vocabulary (#1101, option C) ---------------------------
    // The groups are SoH's Randomizer headers, in SoH's order, and every option
    // sits under the header its OoT twin uses (settings.cpp's RSG_MENU_SECTION_*
    // groups): the win counts and the triforce rows under Win Condition, as
    // SoH's Triforce Hunt is; the two access rows under Area Access; the souls,
    // swim, buttons, clock and pool rows under Additional Items; the two trap
    // rows under Traps. Names, not enumerators, so this cannot agree with a
    // regrouping by construction.
    {
        static const char* const kSohHeaders[] = {
            "Logic",
            "Win Condition",
            "Area Access",
            "Shuffle Items",
            "Shuffle Shops & Merchants",
            "Additional Items",
            "Traps",
            "Hints",
            "Starting Items",
        };
        const int headerCount = (int)(sizeof(kSohHeaders) / sizeof(kSohHeaders[0]));
        if ((int)COMBO_MM_GROUP_COUNT != headerCount) {
            return Fail(77, "the option taxonomy has %d groups, SoH's Randomizer headers are %d",
                        (int)COMBO_MM_GROUP_COUNT, headerCount);
        }
        for (int g = 0; g < headerCount; g++) {
            if (strcmp(Combo_MMOptionGroupName((uint8_t)g), kSohHeaders[g]) != 0) {
                return Fail(77, "option group %d is named '%s', SoH's header in that place is '%s'", g,
                            Combo_MMOptionGroupName((uint8_t)g), kSohHeaders[g]);
            }
        }
        struct Placement {
            RandoOptionId id;
            const char* header;
        };
        static const Placement kPlacement[] = {
            { RO_LOGIC, "Logic" },
            { RO_SHUFFLE_TRIFORCE_PIECES, "Win Condition" },
            { RO_TRIFORCE_PIECES_MAX, "Win Condition" },
            { RO_TRIFORCE_PIECES_REQUIRED, "Win Condition" },
            { RO_ACCESS_MOON_MASKS_COUNT, "Win Condition" },
            { RO_ACCESS_MOON_REMAINS_COUNT, "Win Condition" },
            { RO_ACCESS_MAJORA_MASKS_COUNT, "Win Condition" },
            { RO_ACCESS_MAJORA_REMAINS_COUNT, "Win Condition" },
            { RO_ACCESS_MAJORA_REMAINS, "Win Condition" },
            { RO_ACCESS_DUNGEONS, "Area Access" },
            { RO_ACCESS_TRIALS, "Area Access" },
            { RO_SHUFFLE_COWS, "Shuffle Items" },
            { RO_SHUFFLE_OWL_STATUES, "Shuffle Items" },
            { RO_SHUFFLE_FREESTANDING_ITEMS, "Shuffle Items" },
            { RO_SHUFFLE_POT_DROPS, "Shuffle Items" },
            { RO_SHUFFLE_SNOWBALL_DROPS, "Shuffle Items" },
            { RO_SHUFFLE_CRATE_DROPS, "Shuffle Items" },
            { RO_SHUFFLE_BARREL_DROPS, "Shuffle Items" },
            { RO_SHUFFLE_GRASS_DROPS, "Shuffle Items" },
            { RO_SHUFFLE_TREE_DROPS, "Shuffle Items" },
            { RO_SHUFFLE_BOSS_REMAINS, "Shuffle Items" },
            { RO_SHUFFLE_GOLD_SKULLTULAS, "Shuffle Items" },
            { RO_MINIMUM_SKULLTULA_TOKENS, "Shuffle Items" },
            { RO_MINIMUM_STRAY_FAIRIES, "Shuffle Items" },
            { RO_SHUFFLE_FROGS, "Shuffle Items" },
            { RO_SHUFFLE_SHOPS, "Shuffle Shops & Merchants" },
            { RO_SHOP_PRICES, "Shuffle Shops & Merchants" },
            { RO_SHUFFLE_TINGLE_SHOPS, "Shuffle Shops & Merchants" },
            { RO_TINGLE_PRICES, "Shuffle Shops & Merchants" },
            { RO_SHUFFLE_SWIM, "Additional Items" },
            { RO_SHUFFLE_OCARINA_BUTTONS, "Additional Items" },
            { RO_SHUFFLE_BOSS_SOULS, "Additional Items" },
            { RO_SHUFFLE_ENEMY_SOULS, "Additional Items" },
            { RO_SHUFFLE_ENEMY_DROPS, "Additional Items" },
            { RO_CLOCK_SHUFFLE, "Additional Items" },
            { RO_CLOCK_SHUFFLE_PROGRESSIVE, "Additional Items" },
            { RO_CLOCK_TERMINAL_TIME, "Additional Items" },
            { RO_PLENTIFUL_ITEMS, "Additional Items" },
            { RO_SHUFFLE_TRAPS, "Traps" },
            { RO_TRAP_AMOUNT, "Traps" },
            { RO_HINTS_GOSSIP_STONES, "Hints" },
            { RO_HINTS_PURCHASEABLE, "Hints" },
            { RO_HINTS_SPIDER_HOUSES, "Hints" },
            { RO_HINTS_HOOKSHOT, "Hints" },
            { RO_HINTS_BOSS_REMAINS, "Hints" },
            { RO_HINTS_OATH_TO_ORDER, "Hints" },
            { RO_STARTING_HEALTH, "Starting Items" },
            { RO_STARTING_CONSUMABLES, "Starting Items" },
            { RO_STARTING_RUPEES, "Starting Items" },
            { RO_STARTING_MAPS_AND_COMPASSES, "Starting Items" },
        };
        const int placed = (int)(sizeof(kPlacement) / sizeof(kPlacement[0]));
        if (placed != RO_MAX) {
            return Fail(78, "the vocabulary table places %d options of %d: an option was added without a header",
                        placed, (int)RO_MAX);
        }
        for (const Placement& want : kPlacement) {
            const ComboMMOptionDesc* desc = Combo_MMOptionById((uint16_t)want.id);
            if (desc == NULL) {
                return Fail(78, "option id %d has no descriptor", (int)want.id);
            }
            if (strcmp(Combo_MMOptionGroupName(desc->group), want.header) != 0) {
                return Fail(78, "%s sits under '%s', SoH's vocabulary puts it under '%s'", desc->name,
                            Combo_MMOptionGroupName(desc->group), want.header);
            }
        }
        // Within a group the rows keep SoH's reading order: the triforce rows
        // lead Win Condition, as SoH's Triforce Hunt leads its own.
        int firstWin = -1;
        for (int i = 0; i < descCount && firstWin < 0; i++) {
            if (strcmp(Combo_MMOptionGroupName(Combo_MMOptionAt(i)->group), "Win Condition") == 0) {
                firstWin = i;
            }
        }
        if (firstWin < 0 || Combo_MMOptionAt(firstWin)->id != (uint16_t)RO_SHUFFLE_TRIFORCE_PIECES) {
            return Fail(78, "Win Condition does not open with Triforce Hunt, as SoH's does");
        }
    }

    ClearAllOptionCVars();
    printf("[TEST] PASS: %d option ids all have StaticData rows and descriptor rows, every widget is bound to the "
           "row's own cvar, and every gated row names a reason that is not a hook with a dispatch point\n",
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
        // #1437: Time Progression's unset key resolves to Random while Shuffle
        // Time is off (its default), whatever the row's own default.
        const uint32_t expected = randoOptionId == RO_CLOCK_SHUFFLE_PROGRESSIVE ? (uint32_t)RO_CLOCK_SHUFFLE_RANDOM
                                                                                : (uint32_t)row.defaultValue;
        if (RANDO_SAVE_OPTIONS[randoOptionId] != expected && !(randoOptionId == RO_MINIMUM_SKULLTULA_TOKENS)) {
            return Fail(53, "unpaired resolution wrote %u to %s, expected %u",
                        (unsigned)RANDO_SAVE_OPTIONS[randoOptionId], row.name, (unsigned)expected);
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

    // ---- paired, MM's own Logic explicitly chosen: [Both Games] Logic wins ---
    // #1367 (operator ruling 2026-11-02): a paired world's strictness is ONE
    // [Both Games] choice and MM's Logic row follows it. Before, an explicit MM
    // choice was honoured (#499 step 3), but the paired fill never read it (the
    // combo rung decides the proof), so Nearly No Logic only re-seeded a world
    // still proved Beatable. Now an explicit NEARLY NO LOGIC changes nothing at
    // the default rung: the same Glitchless profile and the same stamp, with the
    // stamp still in place (no throw). Counterfactual: restore the "honour
    // explicit" pin and Fail(90) goes red.
    CVarSetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_NEARLY_NO_LOGIC);
    {
        bool threw = false;
        uint32_t explicitDigest = 0;
        try {
            explicitDigest = Rando::Foreign::ResolvePairedProfile(true);
        } catch (const std::exception&) {
            threw = true;
        }
        if (threw || RANDO_SAVE_OPTIONS[RO_LOGIC] != RO_LOGIC_GLITCHLESS || explicitDigest != defaultDigest ||
            MM_Rando_ComputeProfileStamp() != defaultDigest) {
            return Fail(90,
                        "an explicit MM Nearly No Logic decided a paired world (threw %d, RO_LOGIC %u, digest %08X "
                        "vs the default %08X); [Both Games] Logic decides it (#1367)",
                        threw ? 1 : 0, (unsigned)RANDO_SAVE_OPTIONS[RO_LOGIC], explicitDigest, defaultDigest);
        }
        if (MM_Rando_PairedLogicShown() != RO_LOGIC_GLITCHLESS) {
            return Fail(91, "MM's Logic row shows %d under Beatable Only with Nearly No Logic in its key; it must "
                            "show what the world is made with (Glitchless)",
                        MM_Rando_PairedLogicShown());
        }
    }

    // ---- STAMPED pair + divergent inputs: REFUSED, never re-stamped --------
    // The freeze lock itself (#564 V8: the digest acquires comparators).
    // gComboCtx still carries defaultDigest; turning a shuffle on now models a
    // post-creation edit reaching generation. The resolution must THROW
    // (OnFileCreate's catch turns that into the vanilla revert — no divergent
    // world is authored) and must NOT self-heal the stamp. Revert the compare
    // in ResolvePairedProfile and both halves go red. (A shuffle, not MM's
    // Logic, since #1367: MM's Logic no longer diverges a paired profile.)
    CVarSetInteger(Rando::StaticData::Options[RO_SHUFFLE_COWS].cvar, RO_GENERIC_ON);
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
    CVarClear(Rando::StaticData::Options[RO_SHUFFLE_COWS].cvar);

    // ---- paired, [Both Games] Logic "No Logic": MM's Logic derives No Logic --
    // #1367: the base rung makes the MM half No Logic at creation, whatever MM's
    // own key holds (it still holds Nearly No Logic: it folds into No Logic),
    // and that is a different world: a different stamp. A fresh carrier: this
    // is a NEW creation. Counterfactual: drop the derivation and Fail(60) goes red.
    restampUnfrozenCarrier();
    if (Combo_ComboSettingSet(COMBO_SETTING_LOGIC, (int32_t)RSBS_COMBO_RUNG_NONE) != 1) {
        return Fail(92, "[Both Games] Logic \"No Logic\" could not be authored before a creation");
    }
    const uint32_t noLogicDigest = Rando::Foreign::ResolvePairedProfile(true);
    const uint32_t noLogicMm = RANDO_SAVE_OPTIONS[RO_LOGIC];
    const int noLogicShown = MM_Rando_PairedLogicShown();
    const uint32_t noLogicStamp = MM_Rando_ComputeProfileStamp();
    Combo_ComboSettingClear(COMBO_SETTING_LOGIC);
    printf("[TEST] mm-paired-profile #1367: rung No Logic -> MM Logic %u (row shows %d), digest %08X (stamp %08X); "
           "rung Beatable Only -> %08X\n",
           (unsigned)noLogicMm, noLogicShown, noLogicDigest, noLogicStamp, defaultDigest);
    if (noLogicMm != RO_LOGIC_NO_LOGIC || noLogicShown != RO_LOGIC_NO_LOGIC) {
        return Fail(60, "[Both Games] Logic \"No Logic\" created MM Logic %u (row shows %d), not No Logic (#1367)",
                    (unsigned)noLogicMm, noLogicShown);
    }
    if (noLogicDigest == defaultDigest || noLogicStamp != noLogicDigest) {
        return Fail(61, "the No Logic world's MM stamp %08X equals the Beatable Only one, or the creation stamp "
                        "%08X disagrees with the resolution — two different MM worlds would carry one identity",
                    noLogicDigest, noLogicStamp);
    }

    // ---- a world created BEFORE #1367 keeps its own MM Logic -----------------
    // Such a world stamped MM's own row (explicit Nearly No Logic here) at rung
    // Beatable. The compare's second candidate matches it while the key holds
    // its Logic, and a re-resolution against its stamp keeps that Logic; with
    // the key cleared it does not match (the load restore writes it back,
    // paired-load-restore leg 2f). A NEW world's stamp still matches with the
    // stale key in place.
    {
        const uint32_t legacyDigest = MM_Rando_ComputeLegacyProfileStampForTest();
        const bool legacyMatches = MM_Rando_ProfileStampMatches(legacyDigest) != 0;
        const bool newMatches = MM_Rando_ProfileStampMatches(defaultDigest) != 0;
        restampUnfrozenCarrier();
        gComboCtx.mmProfileDigest = legacyDigest;
        bool threw = false;
        try {
            Rando::Foreign::ResolvePairedProfile(true);
        } catch (const std::exception&) {
            threw = true;
        }
        const uint32_t legacyMm = RANDO_SAVE_OPTIONS[RO_LOGIC];
        const int legacyShown = MM_Rando_PairedLogicShown();
        CVarClear(Rando::StaticData::Options[RO_LOGIC].cvar);
        const bool legacyMatchesCleared = MM_Rando_ProfileStampMatches(legacyDigest) != 0;
        printf("[TEST] mm-paired-profile #1367 legacy: stamp %08X matches %d (new world's %d), re-resolved MM Logic "
               "%u (threw %d), row shows %d; with the key cleared it matches %d\n",
               legacyDigest, legacyMatches ? 1 : 0, newMatches ? 1 : 0, (unsigned)legacyMm, threw ? 1 : 0,
               legacyShown, legacyMatchesCleared ? 1 : 0);
        if (legacyDigest == defaultDigest || !legacyMatches || !newMatches) {
            return Fail(93, "the compare does not accept both a pre-#1367 world with its own MM Logic and a new "
                            "world under the same keys; existing files would be refused");
        }
        if (threw || legacyMm != RO_LOGIC_NEARLY_NO_LOGIC || legacyShown != RO_LOGIC_NEARLY_NO_LOGIC) {
            return Fail(94, "a pre-#1367 world re-resolved with MM Logic %u (threw %d, row shows %d), not its own "
                            "Nearly No Logic",
                        (unsigned)legacyMm, threw ? 1 : 0, legacyShown);
        }
        if (legacyMatchesCleared) {
            return Fail(95, "a pre-#1367 world's stamp matched with MM's Logic key cleared: the second candidate is "
                            "not keyed on the world's own Logic");
        }
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

    // ---- clock crossings arm only in the RANDOM clock mode (#681 review) ----
    // RSBS_GIVECAP_CLOCKS admits the six CONCRETE half-day rows (Time (Day 1) ..
    // Time (Night 3)) into the reverse draw. In the two progressive modes MM's
    // logic reads time ownership as a COUNT, not per flag (Logic/Logic.h,
    // OwnsHalfDayForMode: ASCENDING owns "the first N half-days", DESCENDING
    // "the last N"), and the world only ever hands out half-days in that order
    // (ConvertItem.cpp, RI_TIME_PROGRESSIVE). A crossed Time (Night 3) in an
    // ascending world would set the Night 3 flag while the logic reads "Day 1
    // owned" off the count: the flags and the logic would describe different
    // worlds. So the capability is published only in RANDOM mode, where both
    // read the flags. RED before the gate: ASCENDING and DESCENDING published it.
    //
    // #1437: the progressive modes publish RSBS_GIVECAP_CLOCKS_PROGRESSIVE
    // instead, which admits Progressive Time alone (its give resolves in the
    // world's order at the redemption, so the count stays true). RED before
    // #1437: no mode published it, so a progressive clock never left MM.
    ClearAllOptionCVars();
    CVarSetInteger(Rando::StaticData::Options[RO_CLOCK_SHUFFLE].cvar, RO_GENERIC_ON);
    struct ClockModeCase {
        int32_t mode;
        bool arms;
        bool armsProgressive;
        const char* name;
    };
    static const ClockModeCase kClockModes[] = {
        { RO_CLOCK_SHUFFLE_RANDOM, true, false, "RANDOM" },
        { RO_CLOCK_SHUFFLE_ASCENDING, false, true, "ASCENDING" },
        { RO_CLOCK_SHUFFLE_DESCENDING, false, true, "DESCENDING" },
    };
    for (const ClockModeCase& m : kClockModes) {
        CVarSetInteger(Rando::StaticData::Options[RO_CLOCK_SHUFFLE_PROGRESSIVE].cvar, m.mode);
        MM_Rando_PublishProfileGiveCaps(/*fromSave=*/0);
        const bool armed = (Combo_ForeignGiveCaps((uint8_t)GAME_MM) & RSBS_GIVECAP_CLOCKS) != 0u;
        if (armed != m.arms) {
            return Fail(75,
                        "clock shuffle ON in %s mode published RSBS_GIVECAP_CLOCKS=%d, expected %d - a concrete "
                        "half-day crossing %s",
                        m.name, armed ? 1 : 0, m.arms ? 1 : 0,
                        m.arms ? "is now refused where the logic reads each flag"
                               : "would break the progressive order the logic counts");
        }
        const bool armedProgressive = (Combo_ForeignGiveCaps((uint8_t)GAME_MM) & RSBS_GIVECAP_CLOCKS_PROGRESSIVE) != 0u;
        if (armedProgressive != m.armsProgressive) {
            return Fail(96,
                        "clock shuffle ON in %s mode published RSBS_GIVECAP_CLOCKS_PROGRESSIVE=%d, expected %d - "
                        "Progressive Time %s (#1437)",
                        m.name, armedProgressive ? 1 : 0, m.armsProgressive ? 1 : 0,
                        m.armsProgressive ? "stays confined to MM checks, as before OoTMM's rule was followed"
                                          : "would cross into a world whose pool holds the concrete half-days");
        }
    }
    // Control: clock shuffle OFF never arms either family, in the modes that would.
    CVarSetInteger(Rando::StaticData::Options[RO_CLOCK_SHUFFLE].cvar, RO_GENERIC_OFF);
    for (const ClockModeCase& m : kClockModes) {
        CVarSetInteger(Rando::StaticData::Options[RO_CLOCK_SHUFFLE_PROGRESSIVE].cvar, m.mode);
        MM_Rando_PublishProfileGiveCaps(/*fromSave=*/0);
        if ((Combo_ForeignGiveCaps((uint8_t)GAME_MM) & (RSBS_GIVECAP_CLOCKS | RSBS_GIVECAP_CLOCKS_PROGRESSIVE)) != 0u) {
            return Fail(76, "clock shuffle OFF in %s mode published a clock capability", m.name);
        }
    }
    Combo_ClearForeignGiveCaps();

    // ---- #1437: each clock row carries its own mode's bit -------------------
    // The classifier is what the bag reads: the six concrete half-days need the
    // RANDOM bit and Progressive Time the progressive one, so neither family can
    // roam under the other mode's caps.
    for (int ri = RI_TIME_DAY_1; ri <= RI_TIME_PROGRESSIVE; ri++) {
        ComboItemClassRow row;
        if (MM_ComboLogic_ClassifyItem((uint16_t)ri, &row) != 1) {
            return Fail(97, "MM's classifier has no row for clock RI id %d", ri);
        }
        const uint32_t want =
            ri == RI_TIME_PROGRESSIVE ? (uint32_t)RSBS_FILL_ARM_CLOCKS_PROGRESSIVE : (uint32_t)RSBS_FILL_ARM_CLOCKS;
        if (row.armedBy != want) {
            return Fail(97, "clock RI id %d is armed by %08X, expected %08X", ri, (unsigned)row.armedBy,
                        (unsigned)want);
        }
    }

    // ---- #1437: Time Progression ships Ascending, inert while Shuffle Time is off
    // OoTMM's progressiveClocks defaults to 'ascending' (data.ts). The row says so
    // (the pane reads it there); the resolution keeps the unset key at Random
    // while Shuffle Time is off, so a default world's options string and identity
    // are the ones every world before the change froze, and an explicit key is
    // honoured in both states.
    const auto& modeRow = Rando::StaticData::Options[RO_CLOCK_SHUFFLE_PROGRESSIVE];
    if (modeRow.defaultValue != RO_CLOCK_SHUFFLE_ASCENDING) {
        return Fail(98, "Time Progression defaults to %u, expected Ascending (OoTMM's progressiveClocks)",
                    (unsigned)modeRow.defaultValue);
    }
    ClearAllOptionCVars();
    restampUnfrozenCarrier();
    const uint32_t unsetDigest = Rando::Foreign::ResolvePairedProfile(true);
    if (RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE] != RO_CLOCK_SHUFFLE_RANDOM || unsetDigest != defaultDigest) {
        return Fail(99,
                    "with Shuffle Time off and Time Progression unset the mode resolved to %u (digest %08X vs the "
                    "default world's %08X); expected Random and the same world",
                    (unsigned)RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE], unsetDigest, defaultDigest);
    }
    CVarSetInteger(modeRow.cvar, RO_CLOCK_SHUFFLE_RANDOM);
    restampUnfrozenCarrier();
    if (Rando::Foreign::ResolvePairedProfile(true) != defaultDigest) {
        return Fail(99, "an explicit Random with Shuffle Time off is not the default world's identity");
    }
    CVarSetInteger(modeRow.cvar, RO_CLOCK_SHUFFLE_ASCENDING);
    restampUnfrozenCarrier();
    if (Rando::Foreign::ResolvePairedProfile(true) == defaultDigest ||
        RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE] != RO_CLOCK_SHUFFLE_ASCENDING) {
        return Fail(99,
                    "an explicit Ascending with Shuffle Time off was not honoured (resolved %u): a world frozen "
                    "with it would no longer match its own stamp",
                    (unsigned)RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE]);
    }
    ClearAllOptionCVars();
    CVarSetInteger(Rando::StaticData::Options[RO_CLOCK_SHUFFLE].cvar, RO_GENERIC_ON);
    restampUnfrozenCarrier();
    Rando::Foreign::ResolvePairedProfile(true);
    if (RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE] != RO_CLOCK_SHUFFLE_ASCENDING) {
        return Fail(100, "with Shuffle Time on and Time Progression unset the mode resolved to %u, expected Ascending",
                    (unsigned)RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE]);
    }
    MM_Rando_PublishProfileGiveCaps(/*fromSave=*/0);
    if ((Combo_ForeignGiveCaps((uint8_t)GAME_MM) & (RSBS_GIVECAP_CLOCKS | RSBS_GIVECAP_CLOCKS_PROGRESSIVE)) !=
        RSBS_GIVECAP_CLOCKS_PROGRESSIVE) {
        return Fail(100,
                    "the shipped Shuffle Time profile published caps %04X, expected the progressive clock bit "
                    "alone",
                    (unsigned)Combo_ForeignGiveCaps((uint8_t)GAME_MM));
    }
    Combo_ClearForeignGiveCaps();

    // ---- #1437: the load restore puts a Random world's mode back ------------
    // A world frozen with Shuffle Time on and Time Progression left at the old
    // default (Random), loaded while the key holds Shuffle Time off: the unset
    // mode agrees with the file (Random while off) until Shuffle Time is put back,
    // and then resolves to Ascending. RED with one pass: the restore wrote Shuffle
    // Time alone and its after-check refused the file.
    {
        ClearAllOptionCVars();
        CVarSetInteger(Rando::StaticData::Options[RO_CLOCK_SHUFFLE].cvar, RO_GENERIC_ON);
        CVarSetInteger(modeRow.cvar, RO_CLOCK_SHUFFLE_RANDOM);
        restampUnfrozenCarrier();
        const uint32_t frozen = MM_Rando_ComputeProfileStamp();
        gComboCtx.mmProfileDigest = frozen;
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
        Rando::Foreign::ResolvePairedProfile(true);
        std::vector<uint8_t> half(sizeof(SaveContext));
        memcpy(half.data(), &gSaveContext, sizeof(SaveContext));
        ClearAllOptionCVars(); // the player turned Shuffle Time off at the title
        const int classified = MM_Rando_ClassifyProfileForLoad(half.data(), half.size(), frozen);
        char names[256] = { 0 };
        int count = 0;
        const int restored =
            MM_Rando_RestoreProfileForLoad(half.data(), half.size(), frozen, names, sizeof(names), &count);
        const uint32_t after = MM_Rando_ComputeProfileStamp();
        printf("[TEST] mm-paired-profile: Random clock world, keys cleared: classified %d, restore %d (%d key(s): "
               "%s), stamp %08X vs frozen %08X\n",
               classified, restored, count, names, (unsigned)after, (unsigned)frozen);
        if (classified != RSBS_MM_PROFILE_LOAD_RESTORABLE || restored != RSBS_MM_PROFILE_LOAD_RESTORED ||
            after != frozen || CVarGetInteger(modeRow.cvar, RO_CLOCK_SHUFFLE_ASCENDING) != RO_CLOCK_SHUFFLE_RANDOM) {
            return Fail(101,
                        "a world frozen with Shuffle Time in Random was not restored (classified %d, restore %d, "
                        "stamp %08X vs %08X): the restore's second pass is missing",
                        classified, restored, (unsigned)after, (unsigned)frozen);
        }
        gComboCtx.mmProfileDigest = 0;
    }

    // ---- #1437: a world made before Ascending shipped loads where it did ------
    // Created with Shuffle Time on and Time Progression never touched, so its
    // stamp folds the old row default, Random. Its half cannot always say so: a
    // pair created before K11 has none, and a half written before #1101 reads
    // later SaveContext fields where the excluded-check set now sits, so its
    // candidate misses. With the keys as the player left them the load matched on
    // main and must still match, and a generation against the stamp keeps Random.
    // RED without the pre-#1437 candidate: both classify UNRESTORABLE (the file
    // select refuses the slot) and the re-resolution throws.
    {
        ClearAllOptionCVars();
        CVarSetInteger(Rando::StaticData::Options[RO_CLOCK_SHUFFLE].cvar, RO_GENERIC_ON);
        // The stamp a creation before #1437 made from these keys. Spelled out only
        // to compute it: the identity folds values, not whether a key was set.
        CVarSetInteger(modeRow.cvar, RO_CLOCK_SHUFFLE_RANDOM);
        restampUnfrozenCarrier();
        const uint32_t frozen = MM_Rando_ComputeProfileStamp();
        // The half that world's creation wrote, then aged into one written before
        // #1101: the bytes now read as the excluded-check set hold something else.
        gComboCtx.mmProfileDigest = frozen;
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
        Rando::Foreign::ResolvePairedProfile(true);
        gSaveContext.save.shipSaveInfo.rando.randoSaveExcludedChecks[4] = 0xFF;
        std::vector<uint8_t> oldHalf(sizeof(SaveContext));
        memcpy(oldHalf.data(), &gSaveContext, sizeof(SaveContext));
        CVarClear(modeRow.cvar); // the keys as the player left them
        const uint32_t today = MM_Rando_ComputeProfileStamp();
        const bool matches = MM_Rando_ProfileStampMatches(frozen) != 0;
        const int classifiedNone = MM_Rando_ClassifyProfileForLoad(nullptr, 0, frozen);
        const int classifiedOld = MM_Rando_ClassifyProfileForLoad(oldHalf.data(), oldHalf.size(), frozen);
        char names[256] = { 0 };
        int count = -1;
        const int restored =
            MM_Rando_RestoreProfileForLoad(oldHalf.data(), oldHalf.size(), frozen, names, sizeof(names), &count);
        const bool keyUnset = !Combo_CVarIsExplicitInt(modeRow.cvar);
        // The arrival of a pair with no half: its generation re-resolves the keys
        // against the stamp (ResolvePairedProfile's compare-or-stamp).
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        bool threw = false;
        uint32_t resolved = 0;
        try {
            resolved = Rando::Foreign::ResolvePairedProfile(true);
        } catch (const std::exception&) {
            threw = true;
        }
        const uint32_t resolvedMode = RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE];
        printf("[TEST] mm-paired-profile #1437 legacy: stamp %08X (today's keys %08X) matches %d; no half %d, old half "
               "%d, restore %d (%d key(s), mode key unset %d); re-resolved %08X mode %u (threw %d)\n",
               (unsigned)frozen, (unsigned)today, matches ? 1 : 0, classifiedNone, classifiedOld, restored, count,
               keyUnset ? 1 : 0, (unsigned)resolved, (unsigned)resolvedMode, threw ? 1 : 0);
        if (today == frozen || !matches || classifiedNone != RSBS_MM_PROFILE_LOAD_MATCHES ||
            classifiedOld != RSBS_MM_PROFILE_LOAD_MATCHES || restored != RSBS_MM_PROFILE_LOAD_MATCHES || count != 0 ||
            !keyUnset) {
            return Fail(102,
                        "a world created with Shuffle Time on and Time Progression unset before it shipped Ascending "
                        "no longer loads from a half that cannot restore it (matches %d, no half %d, old half %d, "
                        "restore %d)",
                        matches ? 1 : 0, classifiedNone, classifiedOld, restored);
        }
        if (threw || resolved != frozen || resolvedMode != RO_CLOCK_SHUFFLE_RANDOM) {
            return Fail(102,
                        "a pair created before #1437 with no half re-resolved mode %u (digest %08X vs %08X, threw "
                        "%d); its generation must keep Random",
                        (unsigned)resolvedMode, (unsigned)resolved, (unsigned)frozen, threw ? 1 : 0);
        }

        // Guards: the candidate is the unset key's old value and nothing else. An
        // explicit Ascending is a different world, and a world frozen in
        // Descending is not matched by the candidate (it supplies Random alone).
        CVarSetInteger(modeRow.cvar, RO_CLOCK_SHUFFLE_ASCENDING);
        const int explicitAscending = MM_Rando_ClassifyProfileForLoad(nullptr, 0, frozen);
        CVarSetInteger(modeRow.cvar, RO_CLOCK_SHUFFLE_DESCENDING);
        const uint32_t descending = MM_Rando_ComputeProfileStamp();
        CVarClear(modeRow.cvar);
        const int descendingUnset = MM_Rando_ClassifyProfileForLoad(nullptr, 0, descending);
        if (explicitAscending != RSBS_MM_PROFILE_LOAD_UNRESTORABLE ||
            descendingUnset != RSBS_MM_PROFILE_LOAD_UNRESTORABLE) {
            return Fail(103,
                        "the pre-#1437 candidate matched a different world (Random stamp with an explicit Ascending: "
                        "%d; Descending stamp with the key unset: %d)",
                        explicitAscending, descendingUnset);
        }
        gComboCtx.mmProfileDigest = 0;
    }

    // ---- #1437: the restore compares Time Progression last ------------------
    // A default file (Shuffle Time off, the mode Random) loaded while the session
    // has Shuffle Time on and the mode unset. Putting Shuffle Time back makes the
    // unset mode resolve to Random again, so only Shuffle Time is written and
    // named, and the mode key stays unset (the next creation still gets
    // Ascending). RED when the mode is compared in the first pass: it is written
    // as an explicit Random and named in the load toast.
    {
        ClearAllOptionCVars();
        restampUnfrozenCarrier();
        const uint32_t frozen = MM_Rando_ComputeProfileStamp();
        gComboCtx.mmProfileDigest = frozen;
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
        Rando::Foreign::ResolvePairedProfile(true);
        std::vector<uint8_t> half(sizeof(SaveContext));
        memcpy(half.data(), &gSaveContext, sizeof(SaveContext));
        CVarSetInteger(Rando::StaticData::Options[RO_CLOCK_SHUFFLE].cvar, RO_GENERIC_ON);
        char names[256] = { 0 };
        int count = 0;
        const int restored =
            MM_Rando_RestoreProfileForLoad(half.data(), half.size(), frozen, names, sizeof(names), &count);
        const bool keyUnset = !Combo_CVarIsExplicitInt(modeRow.cvar);
        const uint32_t after = MM_Rando_ComputeProfileStamp();
        printf("[TEST] mm-paired-profile #1437: default file loaded with Shuffle Time on: restore %d (%d key(s): %s), "
               "mode key unset %d, stamp %08X vs frozen %08X\n",
               restored, count, names, keyUnset ? 1 : 0, (unsigned)after, (unsigned)frozen);
        if (restored != RSBS_MM_PROFILE_LOAD_RESTORED || count != 1 || !keyUnset || after != frozen) {
            return Fail(104,
                        "loading a default file while Shuffle Time was on restored %d key(s) (%s), mode key unset %d: "
                        "Time Progression was written for nothing",
                        count, names, keyUnset ? 1 : 0);
        }
        gComboCtx.mmProfileDigest = 0;
    }

    // Leave global state clean for whatever runs next.
    ClearAllOptionCVars();
    CVarClear("gRando.ExcludedChecks");
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    Context_SetCurrentGame(prevGame);

    printf("[TEST] PASS: the paired profile derives MM's Logic from [Both Games] Logic, freezes at creation, keeps a "
           "pre-#1367 world's own Logic, refuses divergence without self-healing, and its widened digest moves with "
           "every generation input\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
