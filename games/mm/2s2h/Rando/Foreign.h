/**
 * Rando::Foreign — MM's half of the cross-game foreign-item pipeline
 * (Phase 3.0 Lane C1, #392). Single-exe only: the whole surface is a no-op
 * shape outside RSBS_SINGLE_EXECUTABLE (the standalone port has no paired
 * world to key off).
 *
 * See Foreign.cpp for the placement algorithm and src/common/foreign_items.h
 * for the shared placement-table/pool contract this module drives.
 */
#ifndef RANDO_FOREIGN_H
#define RANDO_FOREIGN_H

#ifdef RSBS_SINGLE_EXECUTABLE

#include <string>
#include "Types.h"

namespace Rando {
namespace Foreign {

/** True when the paired-world keying is satisfied (Lane B's carrier contract:
 *  gComboCtx.sourceIsRando && sharedRandoSettingsHash != 0). */
bool PairingActive();

/** The deterministic input-seed string the paired MM world generates under —
 *  derived from gComboCtx.sharedRandoSeed, so one OoT seed names one MM
 *  world (and one spoiler file). */
std::string PairedInputSeedString();

/** The paired final seed: Ship_Hash(master seed + MM's persisted options),
 *  mirroring OoT's own Hash(seed + settingsStr) double-reseed (Lane B
 *  contract). Call AFTER the options are persisted into RANDO_SAVE_OPTIONS. */
uint32_t MixPairedFinalSeed();

/** True if this MM check is a SAFE host for a foreign (cross-game) item —
 *  i.e. the check is in the fill, holds a legal junk-class MM item, and belongs
 *  to a check class whose `.eligible` bit is armed by GAME code rather than by
 *  a rando hook that might not exist. #488: the previous inline predicate was a
 *  blocklist over a check table that carries no give-path information at all,
 *  so a host with no runtime give-path stranded its pinned OoT item forever —
 *  invisible, unwinnable, indistinguishable from a missing item.
 *
 *  Extracted (rather than left inline) so the CI lock can drive the REAL
 *  selection predicate through MM_Rando_Foreign_IsEligibleHost instead of
 *  re-deriving it; see the bridge block at the bottom of Foreign.cpp. */
bool IsEligibleHost(RandoCheckId randoCheckId);

/** Swap deterministically-chosen junk placements for the pinned foreign pool
 *  and record them in gComboCtx.foreignPlacements. Call after the fill/logic
 *  apply populated RANDO_SAVE_CHECKS. Returns the number placed. */
int PlaceForeignItems();

/** True if this MM check hosts a foreign item in the current paired world. */
bool IsForeignCheck(RandoCheckId randoCheckId);

/** Display name for the foreign item hosted by this check, or nullptr. */
const char* ForeignNameForCheck(RandoCheckId randoCheckId);

/** Give-path core: record the check's foreign item into the shared structure
 *  (durable immediately; de-duped by the A1 producer). Returns true if a
 *  foreign item was recorded. The CheckQueue lambda calls this and layers the
 *  generic presentation on top; the ROM-free unit test drives it directly. */
bool RecordForeignPickup(RandoCheckId randoCheckId);

} // namespace Foreign
} // namespace Rando

#endif // RSBS_SINGLE_EXECUTABLE

#endif // RANDO_FOREIGN_H
