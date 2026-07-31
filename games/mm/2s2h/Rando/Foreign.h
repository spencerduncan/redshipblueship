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

/** Resolve MM's randomizer option profile into RANDO_SAVE_OPTIONS and, for a
 *  paired world, CHECK it against the creation-frozen identity
 *  (#499 steps 2-4; ADR 0009 claim 3; #498/#564 phase 2).
 *
 *  This is the ONE place the profile is decided. It was three inline blocks in
 *  MiscBehavior/OnFileCreate.cpp, reachable only by running a whole fill, which
 *  is why the profile could not be locked without a display: extracting it is
 *  what lets the display-free MMPairedProfile CTest drive the REAL resolution
 *  rather than a copy of it.
 *
 *  FREEZE SEMANTICS (#564): the paired identity digest is stamped by the
 *  CREATION event (OoT's Playthrough_Init via MM_Rando_ComputeProfileStamp,
 *  the same computation). When gComboCtx.mmProfileDigest is nonzero, this
 *  function COMPARES the resolution against it and THROWS std::runtime_error
 *  on a mismatch — landing in OnFileCreate's catch, which reverts the file to
 *  vanilla, so a divergent world is never authored and the stamp is never
 *  self-healed. When the stamp is zero (a LEGACY pre-freeze pair), the digest
 *  is stamped here, at the pair's first crossing — the one transitional
 *  writer besides creation.
 *
 *  @param paired  true on the cross-game path (Foreign::PairingActive()). Only
 *                 a paired world gets the logic pin and the identity check; a
 *                 solo MM rando file resolves its options and nothing else,
 *                 exactly as before.
 *  @return the profile digest (0 when `paired` is false).
 *
 *  ORDERING CONTRACT, unchanged and load-bearing: call this BEFORE
 *  MixPairedFinalSeed(), which hashes the finalized options. Resolving after
 *  the mix would derive the world from a profile it does not describe. */
uint32_t ResolvePairedProfile(bool paired);

/** The paired final seed: Ship_Hash(master seed + MM's persisted options),
 *  mirroring OoT's own Hash(seed + settingsStr) double-reseed (Lane B
 *  contract). Call AFTER the options are persisted into RANDO_SAVE_OPTIONS.
 *  Identical to MixPairedFinalSeedForAttempt(0). */
uint32_t MixPairedFinalSeed();

/** The ATTEMPT LADDER's seed derivation (ADR 0010 increment 1.2; accepted
 *  answer O3 delegates recipe/bound/recording to the implementer under the
 *  determinism rules). The documented recipe:
 *
 *      finalSeed(0) = Ship_Hash(decimal(masterSeed) ++ optionsString)
 *      finalSeed(n) = Ship_Hash(decimal(masterSeed) ++ optionsString
 *                               ++ ":glitchless-attempt-" ++ decimal(n))   n >= 1
 *
 *  where optionsString is the persisted RANDO_SAVE_OPTIONS in StaticData
 *  option order ("<value>;" per row — MMOptionsString in Foreign.cpp), i.e.
 *  the SAME settings term the shipped attempt-0 derivation has always hashed.
 *  Attempt 0 is therefore byte-identical to the pre-ladder derivation: a
 *  world that converged before the ladder existed re-derives unchanged. The
 *  ":glitchless-attempt-" literal is the ADR's domain-separation tag and is
 *  used under EVERY logic mode (it names the ladder, not the rung).
 *
 *  Same ordering contract as MixPairedFinalSeed: resolve the profile first. */
uint32_t MixPairedFinalSeedForAttempt(uint32_t attempt);

/** Attempt-ladder bound (ADR 0010 increment 1.2: "bounded attempts"). Ten is
 *  a product budget, not a tuning knob: each Glitchless attempt is capped by
 *  the fill's own 10s wall-clock abort (GlitchlessLogic.cpp), so the ladder's
 *  worst case stays inside the arrival transition a player will actually sit
 *  through, while ten deterministic re-rolls make a persistent dead-end
 *  overwhelmingly a settings problem (surfaced loudly) rather than bad luck. */
inline constexpr int kPairedGenMaxAttempts = 10;

/** Bookkeeping for the most recent PAIRED generation dispatch (OnFileCreate
 *  calls this; the arrival gate and the CI locks read it through the C
 *  accessors below). attemptsTried == 0 resets the record. */
void NotePairedGenerationOutcome(int attemptsTried, bool exhausted);

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

/** The article MM's pickup textbox prepends to that name ("the ", "a ", "an ",
 *  or "" — it carries its own trailing space). Empty string if the check hosts
 *  nothing, so callers can concatenate unconditionally.
 *
 *  #510: a cross-game item is presented as an ORDINARY MM pickup, and MM builds
 *  every native pickup sentence as article + name. The article has to come from
 *  the pooled descriptor because MM cannot look up OoT's item table — that is
 *  the ADR 0002 boundary this whole surface exists to hold. */
const char* ForeignArticleForCheck(RandoCheckId randoCheckId);

/** Give-path core: record the check's foreign item into the shared structure
 *  (durable immediately; de-duped by the A1 producer). Returns true if a
 *  foreign item was recorded. The CheckQueue lambda calls this and layers the
 *  generic presentation on top; the ROM-free unit test drives it directly. */
bool RecordForeignPickup(RandoCheckId randoCheckId);

} // namespace Foreign
} // namespace Rando

// ---------------------------------------------------------------------------
// Attempt-ladder observability (ADR 0010 increment 1.2). C linkage so the
// arrival gate (GameExports_SingleExe.cpp) and the CI bridges can read the
// most recent paired generation's outcome without reaching into the
// namespace. Values describe the LAST paired dispatch only: attempts == 0
// means no paired generation has run (or one is being reset).
// ---------------------------------------------------------------------------
extern "C" int MM_Rando_PairedGenMaxAttempts(void);
extern "C" int MM_Rando_PairedGenLastAttempts(void);
extern "C" int MM_Rando_PairedGenLastExhausted(void);

#endif // RSBS_SINGLE_EXECUTABLE

#endif // RANDO_FOREIGN_H
