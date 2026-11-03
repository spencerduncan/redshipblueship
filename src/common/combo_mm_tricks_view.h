/**
 * @file combo_mm_tricks_view.h
 * @brief View model over MM's per-trick table for the combo options pane
 *        (#578 part 1; ADR 0004 §5-§6, ADR 0008, ADR 0009 decision 3).
 *
 * The twin of `combo_mm_options_view.h`, and a SEPARATE table on purpose rather
 * than extra rows in that one. `ComboMMOptionDesc::id` is a `RandoOptionId` and
 * the MMRandoOptions lock asserts, in both directions, that the descriptor set
 * covers `Rando::StaticData::Options` exactly — ids, cvars, names and defaults.
 * Trick keys are a DIFFERENT id space (`MMRT_*`, sized by `MMRT_MAX`, indexing a
 * different save array), so folding them into the option table would have to
 * break that lock to do it. Two tables, two locks, one pane.
 *
 * Everything else follows the option view's rules unchanged:
 *
 *  - The descriptor table is built MM-SIDE, in
 *    `games/mm/2s2h/Rando/OptionsUiSingleExe.cpp`, and handed here as flat
 *    strings and integers. src/common acquires no MM header (ADR 0009 D3).
 *  - CVars are the AUTHORING surface, and only until the creation event. Both
 *    writers below REJECT while `Combo_MMProfileFrozen()` is true, because from
 *    the creation stamp onward the trick set is the paired world's identity and
 *    an edit would make the next arrival diverge (ADR 0010 §3.3).
 *  - CAPABILITY GATING IS DATA (ADR 0004 §5). A trick row is drawn enabled only
 *    when turning it on actually changes the world. Two conditions can make that
 *    false, and they are different facts, so they are different fields:
 *      `reserved` — the trick needs an OoT-side item MM cannot hold until ADR
 *        0010 increment 3. Twenty of the 85 ported keys are in this state.
 *      `bound == false` — the key exists but no region condition consults it
 *        yet (#578 part 2 authors the bindings). A control that flips a CVar and
 *        changes nothing is ADR 0004 §5's vacuous gate; the table refuses to
 *        describe one as enabled. WHY it is unbound is `unboundClass` (#1368):
 *        one of the classes #697 recorded for every such key, so the row names
 *        its real blocker instead of a generic "not yet".
 *    `disabledReason` is non-empty EXACTLY when `reserved || !bound`, and the
 *    lock asserts both directions.
 *
 * WHY AN UNBOUND KEY IS STILL FOLDED INTO THE FROZEN IDENTITY (it is, MM-side,
 * in Rando::Foreign::ProfileIdentityString): the identity string's SHAPE then
 * does not change when part 2 binds a key, so pairs created now stay comparable
 * against arrivals from a later build. A reserved key is forced off at the
 * resolution instead, so it contributes a constant.
 *
 * Locked ROM-free by the `mm-trick-table` CTest (table coverage and honesty,
 * driven MM-side where both tables are in scope).
 */

#ifndef RSBS_COMMON_COMBO_MM_TRICKS_VIEW_H
#define RSBS_COMMON_COMBO_MM_TRICKS_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#include "combo_ui.h" // ComboUiTone: a chip's palette entry, named without an SoH header

#ifdef __cplusplus
extern "C" {
#endif

/** The most tag chips one trick carries: its one difficulty rung plus the three
 *  flag tags (Experimental, Glitch, OoT Items). */
#define COMBO_MM_TRICK_MAX_CHIPS 4

/**
 * Why a live-capable trick (not reserved) has no binding (#1368), as #697
 * recorded it key by key. A row's `disabledReason` names the blocker in its
 * class's words; `mm-trick-table` holds each class to that wording and pins the
 * per-class counts. APPENDED, never reordered.
 */
typedef enum {
    /** The row is bound, or reserved (a reserved row carries its own reason). */
    COMBO_MM_TRICK_UNBOUND_NONE = 0,
    /** Every item leg needs an Ocarina of Time item used in Majora's Mask,
     *  which OoTMM opens with an "X in MM" setting (stoneAgonyMm,
     *  shortHookshotMm, tunicGoronMm/tunicZoraMm, scalesMm, strengthMm). The
     *  reason names the item. Not hidden: the key becomes bindable with that
     *  item. A key whose ONLY leg is short_hook_anywhere is not here: that is
     *  a logic setting, not an item (COMBO_MM_TRICK_UNBOUND_HOOKSHOT_ANYWHERE). */
    COMBO_MM_TRICK_UNBOUND_OOT_ITEM,
    /** Every leg needs an entrance shuffle MM's logic does not have. */
    COMBO_MM_TRICK_UNBOUND_ENTRANCE_RANDO,
    /** MM's logic already allows what the trick allows, so a binding could
     *  never change a verdict. */
    COMBO_MM_TRICK_UNBOUND_COVERED,
    /** MM's logic has no term for what the trick relaxes. */
    COMBO_MM_TRICK_UNBOUND_NO_SUCH_TERM,
    /** The trick's leg lands in an area (a region or a grotto) MM's logic does
     *  not map yet. */
    COMBO_MM_TRICK_UNBOUND_UNMAPPED_AREA,
    /** Every leg is OoTMM's short_hook_anywhere (`can_hookshot_short &&
     *  setting(hookshotAnywhereMm, logical)`, macros_mm.yml:424): it needs the
     *  "Hookshot Anywhere (MM)" logic setting (data.ts:1696), which MM's logic
     *  does not have (it never expects Hookshot Anywhere). MM's own Hookshot
     *  already satisfies can_hookshot_short, so no Ocarina of Time item is the
     *  blocker. */
    COMBO_MM_TRICK_UNBOUND_HOOKSHOT_ANYWHERE,
    COMBO_MM_TRICK_UNBOUND_CLASS_COUNT
} ComboMMTrickUnboundClass;

/** The reason a class's rows read, for the classes whose reason is one sentence
 *  (an item-class row names its own item; an entrance-class row may add the
 *  alternative to the shuffle after this prefix). Title Case fragments in
 *  SoH's disabledMap style, like the reserved rows' reasons, and spelling out
 *  "Majora's Mask" (R-N8). */
#define COMBO_MM_TRICK_REASON_ENTRANCE_RANDO "Needs Entrance Randomizer"
#define COMBO_MM_TRICK_REASON_COVERED "Already Covered by Majora's Mask Logic"
#define COMBO_MM_TRICK_REASON_NO_SUCH_TERM "No Such Requirement in Majora's Mask Logic"
#define COMBO_MM_TRICK_REASON_UNMAPPED_AREA "Area Not Yet Mapped in Majora's Mask Logic"
#define COMBO_MM_TRICK_REASON_HOOKSHOT_ANYWHERE "Needs Hookshot Anywhere in Logic"
/** How every item-class reason ends: the reserved rows' wording ("Needs Hover
 *  Boots From Ocarina of Time"). */
#define COMBO_MM_TRICK_REASON_OOT_ITEM_SUFFIX " From Ocarina of Time"

/**
 * One trick, as the pane renders it. Mirrors MM's `RandoStaticTrick` row; the
 * `mm-trick-table` lock asserts the mirror is exact.
 */
typedef struct {
    uint16_t id;      // MMRandoTrickId
    const char* name; // stringified enumerator, e.g. "MMRT_KEG_EXPLOSIVES"
    const char* cvar; // "gRando.Tricks.MMRT_<KEY>"; never NULL
    const char* label;
    const char* tooltip;
    uint8_t area;         // MMRandoTrickArea
    const char* areaName; // display name for `area`; never NULL
    uint32_t tags;        // bitwise OR of MMRandoTrickTag; never 0
    /**
     * The tag set as SoH's trick chips (Rando::Tricks::DrawTagChips): one label
     * and palette tone per tag, in tag order, so the difficulty rung is always
     * the first chip. Decided MM-side so common code never learns the tag
     * enum; SoH's own rung colours (tricks.cpp's GetTagColor) where MM shares
     * the rung. `mm-trick-table` asserts the chips mirror `tags` exactly, and
     * each shared rung's tone.
     */
    uint8_t chipCount;
    const char* chipLabels[COMBO_MM_TRICK_MAX_CHIPS];
    ComboUiTone chipTones[COMBO_MM_TRICK_MAX_CHIPS];
    /** Needs an OoT-side item MM cannot hold yet (increment 3). */
    bool reserved;
    /** Some region condition in THIS build consults the key. */
    bool bound;
    /** Operator-facing explanation; EMPTY (never NULL) exactly when the row is
     *  enabled, i.e. when `bound && !reserved`. */
    const char* disabledReason;
    /** ComboMMTrickUnboundClass: NONE exactly when the row is bound or
     *  reserved, else why the key has no binding (#1368). */
    uint8_t unboundClass;
} ComboMMTrickDesc;

/**
 * Install MM's trick descriptor table. Passing (NULL, 0) un-registers, so a test
 * can restore the registry rather than leave process-global state behind. A
 * second registration replaces the first and complains on stderr.
 */
void Combo_RegisterMMTrickTable(const ComboMMTrickDesc* table, int count);

/**
 * Build and register MM's trick descriptor table. DEFINED MM-SIDE
 * (games/mm/2s2h/Rando/OptionsUiSingleExe.cpp).
 *
 * A call rather than a file-scope registrar, for the same reason
 * `MM_RandoOptionsUi_Register` is: the table is derived from a namespace-scope
 * std::map in another translation unit, and static initialization order across
 * TUs is unspecified — a registrar could publish a table built from an empty
 * map, and the symptom would be "the pane has no tricks" with nothing naming
 * the cause.
 */
void MM_RandoTricksUi_Register(void);

/** Number of registered trick descriptors; 0 when the MM table did not link. */
int Combo_MMTrickCount(void);

/** Descriptor at `index` in table order, or NULL if out of range. */
const ComboMMTrickDesc* Combo_MMTrickAt(int index);

/** Descriptor for `id` (an MMRandoTrickId), or NULL if the table has no row. */
const ComboMMTrickDesc* Combo_MMTrickById(uint16_t id);

/** The trick's current authored value: its CVar if set, else off. A NULL
 *  descriptor, a reserved row and an unbound row all read false — the reader
 *  agrees with the writer about what can be on. */
bool Combo_MMTrickGetValue(const ComboMMTrickDesc* desc);

/**
 * Write `on` to the trick's CVar.
 *
 * REJECTED (no CVar write, stderr log) when the profile is frozen, when the row
 * is reserved, or when the row is unbound. The first is the identity freeze; the
 * other two are ADR 0004 §5 — a write that cannot change the world must not
 * reach the key, because the resolved value is folded into the frozen identity
 * and would then move a digest for a world that plays identically.
 */
void Combo_MMTrickSetValue(const ComboMMTrickDesc* desc, bool on);

/** Clear the trick's CVar, returning it to "never chosen". REJECTED while the
 *  profile is frozen, same as the setter. */
void Combo_MMTrickClear(const ComboMMTrickDesc* desc);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_MM_TRICKS_VIEW_H
