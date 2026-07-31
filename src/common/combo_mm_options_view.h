/**
 * @file combo_mm_options_view.h
 * @brief View model over MM's randomizer option table for the combo options
 *        pane (#497 step 4, #499; ADR 0004, ADR 0008).
 *
 * WHAT PROBLEM THIS SOLVES. There is exactly one menu in the binary and all of
 * it is OoT's, so none of MM's 47 randomizer options could be seen or set by a
 * player: the paired MM world generated on whatever `Rando::StaticData::Options`
 * defaults happened to be, and no translation unit in the shipped link ever
 * wrote a `gRando.Options.*` CVar (#499). This is the surface that makes them
 * settable.
 *
 * WHY THE TABLE IS REGISTERED RATHER THAN DEFINED HERE. The option ids, their
 * defaults and their value enums are MM's (`games/mm/2s2h/Rando/Types.h`), and
 * common code must not acquire an MM header to see them — the same rule the
 * foreign-item pools follow (ADR 0009 decision 3: "each defined in the single TU
 * where its enum is in scope"). So the descriptor table is built MM-side, in
 * `games/mm/2s2h/Rando/OptionsUiSingleExe.cpp`, and handed here through
 * Combo_RegisterMMOptionTable from a file-scope registrar. This file holds no
 * MM knowledge at all; it holds a flat array of strings and integers.
 *
 * WHY NOT A SohMenu PANE. ADR 0008 settled the seam for panels that belong to
 * neither game: they are owned by src/common and registered on the shared
 * Ship::Context Gui. This pane must be reachable **while OoT is active, before
 * the paired world is created**, because the profile freezes into the world's
 * identity at the creation event (#498/#564) and a divergent arrival is
 * refused. A pane hung off MM's boot would only exist after the point at which
 * it can still change anything.
 *
 * CAPABILITY GATING IS PART OF THE MODEL, NOT THE WIDGET (ADR 0004 section 5).
 * Every descriptor carries a liveness class and, when it is not live, a reason
 * string. An option whose behaviour hook has no MM dispatch point (#438) widens
 * the check pool but never arms — items land on checks the game cannot award,
 * which ADR 0004 calls the worst state available. A control that flips a CVar
 * and changes nothing is the vacuous gate in UI form; the table refuses to
 * describe one as enabled.
 *
 * VALUES LIVE IN CVars — UNTIL THE CREATION EVENT (#498/#564; ADR 0009 D1 as
 * amended). CVars are the AUTHORING surface for the one window in which
 * authoring is legal: before the paired world is generated. Generation stamps
 * the resolved profile's identity into gComboCtx.mmProfileDigest, and from that
 * moment the profile is world identity: the two writers below
 * (Combo_MMOptionSetValue / Combo_MMOptionClear) REJECT writes while
 * Combo_MMProfileFrozen() is true, and the pane renders read-only. A
 * post-creation edit would not "apply later" — it would make the next MM
 * arrival's resolved profile diverge from the creation stamp, which the
 * arrival refuses as corruption (never honors). This model still reads the
 * CVars directly and caches nothing.
 *
 * Locked ROM-free by the MMRandoOptions CTest (table coverage and honesty,
 * driven MM-side where both tables are in scope) and the ComboMMOptionsWindow
 * CTest (the window that renders it).
 */

#ifndef RSBS_COMMON_COMBO_MM_OPTIONS_VIEW_H
#define RSBS_COMMON_COMBO_MM_OPTIONS_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Presentation groups, taken from the 9-sidebar taxonomy MM's own (link-elided)
 * rando menu uses at `games/mm/2s2h/Rando/Menu.cpp:988-1028`.
 *
 * Five, not nine. MM's other four sidebars carry no `RO_*` option at all:
 * General holds seed/spoiler controls (not options), Check Filter drives a
 * tracker filter held in separate CVars, and Item Tracker / Check Tracker are
 * window buttons. Inventing empty groups for them would put four permanently
 * blank sections in the pane and imply options are missing from it.
 */
typedef enum {
    COMBO_MM_GROUP_LOGIC,    // logic mode and the access/entry conditions
    COMBO_MM_GROUP_SHUFFLE,  // the RO_SHUFFLE_* family and its thresholds
    COMBO_MM_GROUP_ITEMS,    // item-pool shaping
    COMBO_MM_GROUP_STARTING, // what the file starts with
    COMBO_MM_GROUP_HINTS,    // the RO_HINTS_* family
    COMBO_MM_GROUP_COUNT
} ComboMMOptionGroup;

/** How the pane should draw an option's value. */
typedef enum {
    COMBO_MM_WIDGET_CHECKBOX, // two-valued (RO_GENERIC_OFF/ON, NO/YES)
    COMBO_MM_WIDGET_COMBO,    // pick one of `valueLabels[0..valueCount)`
    COMBO_MM_WIDGET_SLIDER,   // integer in [minValue, maxValue]
    COMBO_MM_WIDGET_TIME      // minutes-since-midnight, rendered as HH:MM
} ComboMMOptionWidget;

/**
 * Whether the behaviour behind an option is actually reachable in this binary
 * (ADR 0004 section 5's three-part test: the TU links, its registrar runs, and
 * its hook type has an MM dispatch point placed).
 */
typedef enum {
    /** Consumed only by generation — pools, starting state, logic. No runtime
     *  hook is needed, so it cannot be half-armed and is always honest. */
    COMBO_MM_LIVENESS_GENERATION_ONLY,
    /** Pool widening AND every hook it needs is dispatched. */
    COMBO_MM_LIVENESS_LIVE,
    /** Some legs live, at least one dormant. Enabling it widens the pool but
     *  part of the behaviour never arms — draw it disabled, with the reason. */
    COMBO_MM_LIVENESS_PARTIAL,
    /** The hook type it rides has no MM dispatch point (#438). */
    COMBO_MM_LIVENESS_DORMANT
} ComboMMOptionLiveness;

/**
 * One option, as the pane renders it.
 *
 * `id`, `name`, `cvar` and `defaultValue` mirror the MM `RandoStaticOption` row
 * exactly and are asserted equal to it by the MMRandoOptions lock — the pane
 * must never bind a widget to a key MM does not read.
 */
typedef struct {
    uint16_t id;         // RandoOptionId
    const char* name;    // stringified enumerator, e.g. "RO_SHUFFLE_COWS"
    const char* cvar;    // "gRando.Options.<name>"; never NULL
    const char* label;   // human label; never NULL or empty
    const char* tooltip; // one sentence; never NULL
    uint8_t group;       // ComboMMOptionGroup
    uint8_t widget;      // ComboMMOptionWidget
    int32_t defaultValue;
    int32_t minValue; // SLIDER/TIME only; 0 otherwise
    int32_t maxValue; // SLIDER/TIME only; 0 otherwise
    const char* const* valueLabels; // COMBO only, else NULL
    uint8_t valueCount;             // COMBO only, else 0
    uint8_t liveness;               // ComboMMOptionLiveness
    /** Operator-facing explanation, e.g. "Not yet available: MM OnOpenText
     *  dispatch not placed (#438)". EMPTY (never NULL) exactly when liveness is
     *  LIVE or GENERATION_ONLY — the lock asserts both directions, because a
     *  dormant row with no reason is a control that looks broken and a live row
     *  with a reason is one that looks broken and is not. */
    const char* disabledReason;
} ComboMMOptionDesc;

/**
 * Install MM's descriptor table. Called from a file-scope registrar in the MM
 * TU that owns it, so the pane works with no explicit bring-up call. Passing
 * (NULL, 0) un-registers, which exists so a test can restore the registry
 * rather than leave process-global state behind.
 *
 * A second registration replaces the first and complains on stderr: two tables
 * claiming one id space is the ambiguity this surface exists to prevent.
 */
void Combo_RegisterMMOptionTable(const ComboMMOptionDesc* table, int count);

/**
 * Build and register MM's descriptor table. DEFINED MM-SIDE
 * (games/mm/2s2h/Rando/OptionsUiSingleExe.cpp), declared here because the combo
 * entry point is what calls it.
 *
 * It is a call rather than a file-scope registrar on purpose: the table is
 * derived from `Rando::StaticData::Options`, a namespace-scope std::map in
 * another translation unit, and static initialization order across TUs is
 * unspecified. A registrar could publish a table built from an empty map, and
 * the symptom would be "the pane has no rows" with nothing naming the cause.
 */
void MM_RandoOptionsUi_Register(void);

/** Number of registered descriptors; 0 when the MM table did not link. */
int Combo_MMOptionCount(void);

/** Descriptor at `index` in table order, or NULL if out of range. */
const ComboMMOptionDesc* Combo_MMOptionAt(int index);

/** Descriptor for `id` (a RandoOptionId), or NULL if the table has no row. */
const ComboMMOptionDesc* Combo_MMOptionById(uint16_t id);

/** Display name for a ComboMMOptionGroup; never NULL (out-of-range yields a
 *  visible placeholder rather than a crash in a printf-family call). */
const char* Combo_MMOptionGroupName(uint8_t group);

/**
 * The option's current authored value: its CVar if set, else `defaultValue`.
 * A NULL descriptor yields 0.
 */
int32_t Combo_MMOptionGetValue(const ComboMMOptionDesc* desc);

/**
 * Write `value` to the option's CVar, clamped into the descriptor's legal range
 * (checkbox 0..1, combo 0..valueCount-1, slider/time min..max).
 *
 * Clamping rather than rejecting is deliberate: the value is persisted to
 * `RANDO_SAVE_OPTIONS` and then indexed by generation code, so an out-of-range
 * write reaches MM's tables as an out-of-range index. Refusing silently would
 * leave the pane showing a value the save does not hold.
 *
 * REJECTED (no CVar write, stderr log) while Combo_MMProfileFrozen() is true:
 * post-creation the profile is world identity, not a setting (#498/#564).
 */
void Combo_MMOptionSetValue(const ComboMMOptionDesc* desc, int32_t value);

/**
 * True when `cvar` has an explicitly authored integer value, as opposed to
 * being absent and falling through to whatever default a reader supplies.
 *
 * WHY THIS IS NOT `CVarExists`. libultraship DECLARES `CVarExists` in
 * `bridge/consolevariablebridge.h` but the submodule pinned here defines it
 * nowhere — a declaration with no definition, which links only if nobody calls
 * it. So the existence question is answered by probing instead: read the key
 * twice with two different defaults. An absent key returns each default and the
 * two reads disagree; a present key returns its own value both times and they
 * agree. No new libultraship symbol, and nothing to un-break when the real
 * `CVarExists` eventually lands.
 *
 * Scope honesty: this answers the question for INTEGER cvars, which is all the
 * option keys are. A string- or float-valued key would read as absent.
 */
bool Combo_CVarIsExplicitInt(const char* cvar);

/** True when the option has an explicitly authored CVar (the player chose it),
 *  as opposed to falling through to `defaultValue`. This is the distinction the
 *  paired logic default turns on — see Rando::Foreign::ResolvePairedProfile. */
bool Combo_MMOptionIsExplicit(const ComboMMOptionDesc* desc);

/** Clear the option's CVar, returning it to "never chosen". REJECTED while
 *  Combo_MMProfileFrozen() is true, same as Combo_MMOptionSetValue: clearing is
 *  a write (it flips the resolved value back to the default and the logic pin
 *  back to "no explicit choice"), so it diverges the arrival profile exactly
 *  as a set does. */
void Combo_MMOptionClear(const ComboMMOptionDesc* desc);

/**
 * The frozen-state predicate (#498/#564; ADR 0004 §6's fourth presentation
 * state, ADR 0009 D1 as amended): true once a creation event has stamped the
 * MM profile identity — literally `gComboCtx.mmProfileDigest != 0`, a
 * src/common fact, never a gSaveContext read (ADR 0008 rule 5). While true,
 * the two option writers above reject and the pane renders read-only. Cleared
 * only when the identity itself goes: session invalidation on a DROP path, or
 * a .redsave load of an unfrozen pair.
 */
bool Combo_MMProfileFrozen(void);

/**
 * Resolve the FULL MM profile identity from the option CVars and compute its
 * digest, with NO side effects — nothing is written to any save, CVar, or
 * gComboCtx. DEFINED MM-SIDE (games/mm/2s2h/Rando/Foreign.cpp, the TU that
 * owns the one canonical identity-string builder), declared here because the
 * two callers live outside MM: OoT's Playthrough_Init stamps the result into
 * gComboCtx.mmProfileDigest at the creation event, and MM's arrival gate
 * recomputes it to compare against that stamp (#498/#564 phase 2 step 9).
 *
 * The identity covers every generation input the digest guards: the 47
 * resolved option values (including the paired RO_LOGIC default pin),
 * gRando.ExcludedChecks, and the StartingItems config block (#564 V4 — a
 * digest narrower than the generator's input set is vacuous).
 */
uint32_t MM_Rando_ComputeProfileStamp(void);

/**
 * Pairing header for the pane: whether a paired world exists, its identity, and
 * the MM profile digest it was generated under.
 *
 * `paired == false` means the worlds were never paired; the other fields are
 * then whatever `gComboCtx` holds (typically 0) and must not be shown as a real
 * pairing. `mmProfileDigest == 0` means the profile IDENTITY IS NOT FROZEN
 * (#564 V8's reinterpretation): for a pair created since the freeze that state
 * is unreachable (creation stamps it), so it marks a LEGACY pre-freeze pair
 * that has not crossed yet — whose options remain editable until its first
 * crossing stamps them.
 */
typedef struct {
    bool paired;
    uint32_t sharedRandoSeed;
    uint32_t sharedRandoSettingsHash;
    uint32_t mmProfileDigest;
} ComboMMProfileSummary;

/** Fill `out` with the pairing header. NULL `out` is ignored. */
void Combo_MMProfileSummary(ComboMMProfileSummary* out);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_MM_OPTIONS_VIEW_H
