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
 * WHERE IT IS DRAWN. Since 2026-09-27, as two pages of OoT's live menu, Combo
 * > MM Randomizer and Combo > MM Tricks (combo_mm_options_page.h; ADR 0004's
 * host amendment of that date). They must be reachable **while OoT is active,
 * before the paired world is created**, because the profile freezes into the
 * world's identity at the creation event (#498/#564) and a divergent arrival is
 * refused; OoT's menu is up at file select, before creation. Until then they
 * were a common-owned pop-out window (ADR 0008), kept a window only because
 * src/common could not draw with SoH's widgets. Nothing hung off MM's boot
 * could serve: it would exist only after the point at which it can change
 * anything.
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
 * driven MM-side where both tables are in scope), the ComboMMOptionsPage CTest
 * (the pages' view model) and the MenuMmRandomizerPages CTest (the rows).
 */

#ifndef RSBS_COMMON_COMBO_MM_OPTIONS_VIEW_H
#define RSBS_COMMON_COMBO_MM_OPTIONS_VIEW_H

#include <stdbool.h>
#include <stddef.h>
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
 * Publish MM's frozen profile as GIVE CAPABILITIES (ADR 0011 O8's
 * values-publishing surface; the bits and the rationale live in
 * foreign_items.h). The twin of MM_Rando_ComputeProfileStamp: that one answers
 * "did the rules change", this one answers "what do the rules ARM".
 *
 * @param fromSave 0 resolves the profile from the CVars through the SAME
 *        ResolveProfileValues the creation stamp uses — the creation-freeze
 *        publish, which must happen BEFORE OoT's Fill() so the reverse
 *        placement pass can read it. Nonzero reads the save's frozen
 *        RANDO_SAVE_OPTIONS instead — the hydrate publish, for a later process
 *        that never generated and whose CVars are therefore not this world's
 *        rules.
 */
void MM_Rando_PublishProfileGiveCaps(int fromSave);

/**
 * MM's HALF of the combo triforce hunt (ADR 0010 answer O10; the rule is at
 * ComboTriforceRecord, context.h): with RO_SHUFFLE_TRIFORCE_PIECES on,
 * `*outTotal = RO_TRIFORCE_PIECES_MAX` and `*outRequired =
 * RO_TRIFORCE_PIECES_REQUIRED`; off, both 0. Values are reported unclamped so
 * the combo rule can refuse an out-of-range half rather than store a truncated
 * one. The third MM_Rando_* bridge of the creation freeze, with the SAME two
 * sources as MM_Rando_PublishProfileGiveCaps: `fromSave == 0` resolves the
 * CVars through the creation stamp's own ResolveProfileValues (OoT's creation
 * event, and MM's arrival gate, which compares it with the frozen record's MM
 * half); nonzero reads the save's frozen RANDO_SAVE_OPTIONS. Either out-pointer
 * may be NULL. DEFINED MM-SIDE in games/mm/2s2h/Rando/Foreign.cpp.
 */
void MM_Rando_ResolveTriforceHalf(int fromSave, uint16_t* outTotal, uint16_t* outRequired);

/** Outcomes of MM_Rando_RestoreProfileForLoad. */
enum {
    RSBS_MM_PROFILE_LOAD_MATCHES = 0,      // the live CVars already resolve the file's profile
    RSBS_MM_PROFILE_LOAD_RESTORED = 1,     // the file's options/tricks were written back; now they do
    RSBS_MM_PROFILE_LOAD_UNRESTORABLE = 2, // they differ and the file cannot say how (the keys are as they were)
    RSBS_MM_PROFILE_LOAD_RESTORABLE = 3,   // MM_Rando_ClassifyProfileForLoad only: they differ and the file's own
                                           // options and tricks restore them (nothing was written)
};

/**
 * The pure half of MM_Rando_RestoreProfileForLoad (#836): the same
 * classification, with NOTHING written. MATCHES, RESTORABLE (the restore below
 * would write the file's options and tricks back) or UNRESTORABLE. The
 * file-select probe asks this, so it refuses exactly the files whose MM profile
 * the load could not restore. DEFINED MM-SIDE in games/mm/2s2h/Rando/Foreign.cpp.
 */
int MM_Rando_ClassifyProfileForLoad(const void* mmHalf, size_t mmHalfSize, uint32_t frozenDigest);

/** Outcomes of MM_Rando_ClassifyHalfForPair. */
enum {
    RSBS_MM_HALF_PAIR_WORLD = 0, // this pair's world (also under a lost type byte, which the arrival repairs)
    RSBS_MM_HALF_VANILLA = 1,    // no world: a vanilla type byte and no seed, or a half too short to read
    RSBS_MM_HALF_FOREIGN = 2,    // a world, but not one this pair's master seed derives (another pair's)
};

/**
 * Which world an MM half carries, for the pair whose master seed is
 * @p masterSeed and whose recorded ladder rung is @p pairedAttempt (the
 * record's mmPairedAttempt, 0 = none recorded) (#836 PR 2, #564 V7 and V11).
 * Reads only the half's type byte, its finalSeed and its persisted options
 * (@p mmHalf is the raw Tier-3 SaveContext bytes) and writes nothing; the
 * membership rule is Rando::Foreign::FinalSeedBelongsToPair. The file select's
 * probe passes the record it read; the arrival gate passes the live pairing.
 * A paired file whose half is VANILLA or FOREIGN has no Majora's Mask world of
 * its own. DEFINED MM-SIDE in games/mm/2s2h/Rando/Foreign.cpp.
 */
int MM_Rando_ClassifyHalfForPair(const void* mmHalf, size_t mmHalfSize, uint32_t masterSeed, uint32_t pairedAttempt);

/**
 * FROZEN WINS AT LOAD, MM's half (#781; one-game semantics). The load-time
 * twin of the arrival gate's profile compare: recomputes the SAME digest
 * MM_Rando_ComputeProfileStamp computes (what MM_Rando_GateCrossGameArrival
 * compares) and, when it differs from @p frozenDigest, puts the file's own
 * option values and trick set back into the `gRando.Options.*` /
 * `gRando.Tricks.*` CVars, read from the file's MM half (@p mmHalf, the raw
 * Tier-3 SaveContext bytes the .redsave carries; its RANDO_SAVE_OPTIONS and
 * randoSaveTricks are what creation resolved).
 *
 * Writes only when the half's options and tricks, folded with the LIVE
 * excluded-check list and starting-item block, reproduce @p frozenDigest
 * exactly — i.e. only when writing them is known to restore the file's
 * identity. The excluded-check list and the starting-item block are identity
 * inputs the file does not record (and no single-exe page authors), so a
 * divergence there, an MM half that is not a randomizer save, or a half too
 * short to read returns UNRESTORABLE with nothing written: the arrival gate
 * stays the last line of defence for those. If the keys it did write still do
 * not resolve @p frozenDigest (unreachable while the resolver and the save
 * write agree), every one of them is put back as it was, set to its old value
 * or unset, before UNRESTORABLE returns: UNRESTORABLE always leaves the keys
 * as the player left them.
 *
 * @p names (may be NULL) receives as many of the restored rows' labels as fit
 * WHOLE ("Starting Hearts, <trick>"; never a cut label), "" otherwise;
 * @p outCount (may be NULL) the true number of rows restored, which is larger
 * than the names shown when the list did not fit.
 * DEFINED MM-SIDE in games/mm/2s2h/Rando/Foreign.cpp; call only with a CVar
 * store (Combo_ComboSettingStoreAvailable).
 */
int MM_Rando_RestoreProfileForLoad(const void* mmHalf, size_t mmHalfSize, uint32_t frozenDigest, char* names,
                                   size_t namesLen, int* outCount);

/** Test hook (#781 paired-load-restore leg 5): force the restore's after-check
 *  to fail, so the put-back is exercised. Never called in production. */
void MM_Rando_ForceProfileRestoreVerifyFailForTest(int on);

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
