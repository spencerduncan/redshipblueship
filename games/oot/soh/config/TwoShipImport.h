#pragma once

#include "libultraship/libultraship.h"

namespace SOH {

/**
 * @brief One-shot import of an existing 2Ship2Harkinian config (issue #34).
 *
 * A user arriving from a standalone 2Ship install has their settings in
 * `2ship2harkinian.json` under 2Ship's own app directory. RSBS runs from
 * `shipofharkinian.json`, so without this those settings are simply invisible.
 *
 * Scope, per ADR 0003 §6.4: this imports the CONVERGED keys only — the
 * settings where MM's spelling moved onto OoT's — mapped through the same
 * `RSBS::kConvergedKeys` manifest that `ConfigVersion7Updater` uses, so the
 * updater and the importer cannot drift. Keys MM still owns outright are
 * already spelled identically in both files and need no mapping; keys the
 * classification marks per-game or disputed are deliberately not touched.
 *
 * Guarantees:
 *   - **Runs at most once.** Stamped by a flag in the destination config.
 *   - **Never overwrites a value OoT could have written.** Same conflict rule
 *     as version 7: the OoT-spelled value wins; the imported value is adopted
 *     only where the OoT key is absent.
 *   - **No-op** when the source file is missing, unreadable, or malformed.
 *     A user with no 2Ship install pays nothing and sees nothing.
 *   - **Read-only on the source.** `2ship2harkinian.json` is never modified,
 *     so a standalone 2Ship install keeps working unchanged.
 *
 * Must be called AFTER `RunVersionUpdates()`, so imported values land in their
 * converged spelling and are not re-migrated by a version updater that has
 * already run.
 *
 * @param conf The destination (RSBS) config — used for the run-once stamp.
 * @return true if an import was performed on this call.
 */
bool ImportTwoShipConfig(Ship::Config* conf);

/**
 * @brief The run-once stamp key, exposed for tests and diagnostics.
 *
 * Lives in the destination config rather than beside the CVars so that a user
 * clearing their CVars does not silently re-arm a second import over settings
 * they have since changed.
 */
extern const char* const kTwoShipImportStampKey;

} // namespace SOH
