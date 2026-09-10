/**
 * @file combo_settings_view.h
 * @brief The tier-4 combo-level settings AUTHORING surface: the five
 *        `gCombo.Rando.*` keys behind ComboSettingsRecord (ADR 0011 increment
 *        2, #498; ADR 0003 naming; ADR 0004 §6 state 4).
 *
 * WHAT THIS IS. ComboSettingsRecord (context.h) is the frozen 12-byte identity
 * of the crossing rules — direction, per-direction pool sizes, per-direction
 * item classes — and foreign_items.h owns it: the pinned value spaces, the
 * resolver, the freeze, the fingerprint, the divergence diff. What it did not
 * have until this increment was a way for a PLAYER to author the record:
 * Combo_ResolveComboSettings produced the shipped defaults, so the direction
 * gate (#632) and the class rule (#631) were armed but only their defaults
 * were reachable. This header is the authoring surface — the ONE reader that
 * turns a stored value into a record field, and the ONE writer pair that the
 * pane (ComboSettingsWindow.cpp) and every future caller go through.
 *
 * WHY A SEPARATE TU, AND WHY C++. foreign_items.c is deliberately
 * game-header-free AND libultraship-free: it is a pure function of gComboCtx
 * and the pinned tables, which is what lets the golden-vector and divergence
 * locks drive it in a process that never constructs a Ship::Context. The CVar
 * store lives on that singleton, and libultraship's C bridge dereferences it
 * unconditionally (consolevariablebridge.cpp: CVarGetInteger is literally
 * `Context::GetInstance()->GetConsoleVariables()->GetInteger(...)`). So the
 * store read has to be guarded on the singleton's existence, and only a C++
 * TU can ask. This one does (Combo_ComboSettingStoreAvailable); the resolver
 * in foreign_items.c calls through here and treats "no store" as "nothing
 * authored" — the defaults — which is exactly what every ROM-free row that
 * never brought up a context has always resolved to.
 *
 * THE CONTRACT (ADR 0009 decision 1 as amended; ADR 0011 decision 4):
 *
 *  - CVars AUTHOR, up to the creation event and no further. The creation
 *    event (Playthrough_Init) calls Combo_ResolveComboSettings, which reads
 *    these keys through Combo_ComboSettingResolved, and freezes the result.
 *    After that the frozen record is the world's identity and no CVar
 *    describes it.
 *  - ENFORCEMENT IS ON THE WRITERS, NOT THE WIDGET (ADR 0004 §6): both
 *    Combo_ComboSettingSet and Combo_ComboSettingClear REJECT while
 *    Combo_ComboSettingsFrozen() is true. The pane renders read-only with the
 *    reason "already decided" and shows the values FROM THE SAVE
 *    (Combo_ComboSettingsSummary), but the pane is one caller; the gate is
 *    here.
 *  - VALUES ARE THE PINNED SPACES (ADR 0011 decision 1.2.1): RSBS_COMBO_DIR_*
 *    for the direction, 1..RSBS_FOREIGN_PLACEMENT_CAP for a pool size, a mask
 *    within RSBS_ITEMCLASS_ALL_V1 for a class bitset (zero included — inside a
 *    formatted record it is a legitimate "no classes armed", decision 3.3). A
 *    stored value OUTSIDE its space RESOLVES TO THE SHIPPED DEFAULT WITH A
 *    LOGGED REASON — never to a new enumerator, and never to a clamp that
 *    invents a value the player did not choose. The writers refuse such a
 *    value outright, so the only way one reaches the store is out-of-band (a
 *    hand-edited config, the console).
 *  - ALL FIVE KEYS ARE WORLD IDENTITY, not preference (ADR 0004 §6's scope
 *    note). The manifest in cvar_shared_keys.h carries the classification and
 *    the cvar-classification lock refuses an unclassified `gCombo.` key.
 *
 * Locked ROM-free by ComboSettingsAuthoring (the defaults reproduce the
 * shipped record and its pinned fingerprint byte for byte; authored values
 * reach the record BEFORE the freeze and move the fingerprint; the writers
 * refuse once frozen; out-of-space store values resolve to the defaults) and
 * by ComboSettingsWindow (the pane that renders it).
 */

#ifndef RSBS_COMMON_COMBO_SETTINGS_VIEW_H
#define RSBS_COMMON_COMBO_SETTINGS_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The five AUTHORABLE fields of ComboSettingsRecord, in the record's own
 * declaration order. `goal` and `logicRung` are deliberately NOT here: ADR
 * 0010 owns their authoring, and until it lands they freeze at their shipped
 * defaults (Combo_ComboSettingsDefaults).
 */
typedef enum {
    COMBO_SETTING_DIRECTION = 0,  // gCombo.Rando.Direction     -> record.direction    (RSBS_COMBO_DIR_*)
    COMBO_SETTING_POOL_SIZE_OOT,  // gCombo.Rando.PoolSize.OoT  -> record.poolSizeOoT  (1..CAP)
    COMBO_SETTING_POOL_SIZE_MM,   // gCombo.Rando.PoolSize.MM   -> record.poolSizeMM   (1..CAP)
    COMBO_SETTING_ITEM_CLASS_OOT, // gCombo.Rando.ItemClass.OoT -> record.itemClassOoT (RSBS_ITEMCLASS_* mask)
    COMBO_SETTING_ITEM_CLASS_MM,  // gCombo.Rando.ItemClass.MM  -> record.itemClassMM  (RSBS_ITEMCLASS_* mask)
    COMBO_SETTING_COUNT
} ComboSettingId;

/** The tier-4 CVar key for @p id (the RSBS_CVAR_COMBO_RANDO_* literal from
 *  cvar_shared_keys.h). Never NULL; "(invalid)" for an id past the table. */
const char* Combo_ComboSettingKey(ComboSettingId id);

/** A human-readable label for the pane. Never NULL. */
const char* Combo_ComboSettingLabel(ComboSettingId id);

/** The SHIPPED DEFAULT for @p id — the same field of Combo_ComboSettingsDefaults,
 *  so there is exactly one definition of "what ships". 0 for an invalid id. */
int32_t Combo_ComboSettingDefault(ComboSettingId id);

/**
 * Is @p value inside @p id's PINNED value space (ADR 0011 decision 1.2.1)?
 * Direction: exactly RSBS_COMBO_DIR_OFF..RSBS_COMBO_DIR_BOTH. Pool size:
 * 1..RSBS_FOREIGN_PLACEMENT_CAP. Item class: a mask with no bit outside
 * RSBS_ITEMCLASS_ALL_V1 (zero is valid). False for an invalid id.
 */
bool Combo_ComboSettingValueValid(ComboSettingId id, int32_t value);

/**
 * Is there a CVar store to read at all? False in a process that never
 * constructed a Ship::Context (every ROM-free row that does not bring one
 * up), where "nothing authored" is the only honest answer. Every accessor
 * below that touches the store checks this first, because libultraship's C
 * bridge does not.
 */
bool Combo_ComboSettingStoreAvailable(void);

/**
 * The RAW stored value for @p id, if a player (or an out-of-band writer) set
 * one. Returns false — and leaves @p out untouched — when the store is
 * unavailable or the key is unset. Explicitness is probed the way the MM pane
 * probes it (Combo_CVarIsExplicitInt), so a key holding the wrong TYPE reads
 * as unset rather than as its default. Performs NO validation: that is
 * Combo_ComboSettingResolved's job.
 */
bool Combo_ComboSettingReadStore(ComboSettingId id, int32_t* out);

/**
 * THE ONE READER the resolver uses: the stored value when it is set AND inside
 * its pinned space, else the shipped default. An out-of-space stored value is
 * logged (once per distinct value, so a resolver that runs per placement pass
 * does not flood the log) and resolves to the default — never to a new
 * enumerator, never to a clamp. An unset key resolves silently.
 */
int32_t Combo_ComboSettingResolved(ComboSettingId id);

/**
 * THE WRITER — the gate ADR 0004 §6 puts on the writers rather than the
 * widget. REJECTS (returning 0, writing nothing, logging why) when:
 *   - Combo_ComboSettingsFrozen() is true: the rules are world identity now,
 *     "already decided"; a post-creation edit would not apply later, it would
 *     make the next arrival or load diverge from the creation stamp and be
 *     refused as corruption;
 *   - @p value is outside @p id's pinned space;
 *   - there is no CVar store in this process.
 * @return 1 if the value was written, 0 if refused.
 */
int Combo_ComboSettingSet(ComboSettingId id, int32_t value);

/**
 * Clear @p id back to "unset" (which resolves to the shipped default).
 * Clearing is a write too, so it is refused under the same freeze gate and
 * the same missing-store condition as Combo_ComboSettingSet.
 * @return 1 if cleared, 0 if refused.
 */
int Combo_ComboSettingClear(ComboSettingId id);

/** Has @p id an explicit stored value (of the right type)? False when the
 *  store is unavailable. */
bool Combo_ComboSettingIsExplicit(ComboSettingId id);

/** A short name for an RSBS_COMBO_DIR_* value ("off", "forward", "reverse",
 *  "both"); "(unknown)" otherwise. Never NULL. */
const char* Combo_ComboDirectionName(uint8_t direction);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_SETTINGS_VIEW_H
