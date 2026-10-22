/**
 * @file combo_mm_locations_view.h
 * @brief View model over Majora's Mask's excluded-check list
 *        (`gRando.ExcludedChecks`) for Randomizer > MM Locations (#1101 lane
 *        R3; ADR 0004 section 4's 2026-10-21 amendment, ADR 0009 decision 3).
 *
 * WHAT THE PAGE IS. SoH's Randomizer > Locations is the Included / Excluded
 * table over OoT's `gRandoSettings.ExcludedLocations`; MM's own menu had the
 * same surface as its Check Filter tab ("Normal Checks" / "Forced Junk
 * Checks", games/mm/2s2h/Rando/Menu.cpp's DrawCheckFilterTab), which is elided
 * from the single-exe link with the rest of BenMenu. Until this page nothing
 * wrote MM's key, although the paired profile digest has folded it since #564
 * V4 (games/mm/2s2h/Rando/Foreign.cpp, ProfileIdentityString) and MM's pool
 * generation reads it (Logic/GeneratePools.cpp): an excluded check holds junk.
 *
 * Everything follows the option and trick views' rules unchanged:
 *
 *  - The descriptor table (one row per MM check: id, name, area) is built
 *    MM-SIDE, in games/mm/2s2h/Rando/TrackerAdapterSingleExe.cpp beside the
 *    combo tracker's check-name and area resolvers, and handed here as flat
 *    strings and integers. src/common acquires no MM header (ADR 0009 D3).
 *  - The CVar is the AUTHORING surface, and only until the creation event.
 *    Every writer below REJECTS while `Combo_MMProfileFrozen()` is true: from
 *    the creation stamp onward the list is the paired world's identity, and an
 *    edit would make the next arrival diverge.
 *  - CAPABILITY GATING IS DATA (ADR 0004 section 5). Under Vanilla logic MM's
 *    pool generation ignores the list (GeneratePools.cpp places vanilla items
 *    and never consults it), so the writers refuse there too and the page draws
 *    the table disabled with COMBO_MM_LOCATIONS_VANILLA_REASON, as BenMenu's
 *    tab did ("This setting is not compatible with Vanilla Logic.").
 *
 * THE ONE SPELLING. Every writer emits the list in the form MM's own menu wrote
 * (SaveExcludedChecks) and SoH writes its own list in: ascending ids, each
 * followed by a comma ("12,34,"); an empty list clears the key. The profile
 * digest folds the list in that same canonical form whatever spelling the key
 * holds, and the MM half of a paired file records the set
 * (`randoSaveExcludedChecks`), so a load can put a file's own list back (#781's
 * restore, extended to this key).
 *
 * Locked ROM-free by the `mm-locations` CTest (MMLocations: the table mirrors
 * MM's checks, the writers' spelling and gates, the digest moving with an edit)
 * and by `paired-load-restore` leg 2e (the load restores the list).
 */

#ifndef RSBS_COMMON_COMBO_MM_LOCATIONS_VIEW_H
#define RSBS_COMMON_COMBO_MM_LOCATIONS_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The Randomizer-header sidebar (#1101). Selection persists BY DISPLAY NAME in
 *  gSettings.Menu.RandomizerSidebarSection, so this is the one spelling. */
#define COMBO_MM_LOCATIONS_PAGE_NAME "MM Locations"

/** The key the page authors: MM's own, unchanged (no key is minted). */
#define COMBO_MM_EXCLUDED_CHECKS_CVAR "gRando.ExcludedChecks"

/** The table's disabled reason under Vanilla logic, in SoH's reason style. */
#define COMBO_MM_LOCATIONS_VANILLA_REASON "Not Compatible with Vanilla Logic"

/** One MM check, as the page lists it. */
typedef struct {
    uint16_t id;          // RandoCheckId
    const char* name;     // the name MM's check tracker shows; never NULL
    const char* areaName; // the scene the check is listed under; never NULL
    uint16_t areaKey;     // the order areas are listed in (MM's check tracker's scene order)
} ComboMMLocationDesc;

/**
 * Install MM's check table, sorted by (areaKey, id), and the predicate that
 * says whether the list applies under the current logic. Passing (NULL, 0,
 * NULL) un-registers, so a test can restore the registry. A second, DIFFERENT
 * registration replaces the first and complains on stderr.
 */
void Combo_RegisterMMLocationTable(const ComboMMLocationDesc* table, int count, bool (*vanillaLogic)(void));

/**
 * Build and register MM's check table. DEFINED MM-SIDE
 * (games/mm/2s2h/Rando/TrackerAdapterSingleExe.cpp). A call rather than a
 * file-scope registrar for the option and trick tables' reason: it reads
 * namespace-scope std::maps in other translation units. Idempotent: the rows
 * and their (owned) names are built once; the areas are re-resolved and the
 * rows re-sorted in place when MM's region graph has grown, so the page calls
 * it each frame it draws. The table pointer and row count never change.
 */
void MM_RandoLocationsUi_Register(void);

/** Number of registered checks; 0 when the MM table did not link. */
int Combo_MMLocationCount(void);

/** Descriptor at @p index in table order (area, then id), or NULL. */
const ComboMMLocationDesc* Combo_MMLocationAt(int index);

/** Descriptor for @p id (a RandoCheckId), or NULL if the table has no row. */
const ComboMMLocationDesc* Combo_MMLocationById(uint16_t id);

/**
 * The authored list: every id in the key, ascending and without duplicates.
 * Writes up to @p cap ids into @p out (which may be NULL when @p cap is 0) and
 * returns how many the key holds. Tokens that are not ids are skipped.
 */
int Combo_MMLocationsExcluded(uint16_t* out, int cap);

/** True when @p id is in the authored list. */
bool Combo_MMLocationIsExcluded(uint16_t id);

/** True when MM's pool generation ignores the list (Vanilla logic). */
bool Combo_MMLocationsBlockedByLogic(void);

/**
 * Put @p ids (@p count of them) into the list, or take them out, in one write.
 * Ids the table has no row for are ignored. REJECTED (returns false, nothing
 * written, stderr log) while the profile is frozen or the logic is Vanilla;
 * returns true otherwise, including when nothing changed.
 */
bool Combo_MMLocationsSetExcluded(const uint16_t* ids, int count, bool excluded);

/** One id: Combo_MMLocationsSetExcluded(&id, 1, excluded). */
bool Combo_MMLocationSetExcluded(uint16_t id, bool excluded);

/**
 * Clear the list (the key returns to "never chosen"). REJECTED while the
 * profile is frozen, like every writer; allowed under Vanilla logic, because
 * removing a list nobody's logic reads is always safe.
 */
bool Combo_MMLocationsClearExcluded(void);

/**
 * The page's gray note (never NULL): the frozen sentence every randomizer page
 * of ours shares once the world is decided (with the pair's fingerprint), the
 * Vanilla-logic sentence while the list does not apply, otherwise the shared
 * status sentence (COMBO_RANDO_PAGES_NEXT_WORLD_NOTE). The frozen sentence is
 * held in a static buffer until the next call.
 */
const char* Combo_MMLocationsPage_StatusNote(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_MM_LOCATIONS_VIEW_H
