/**
 * Rando::Foreign — MM-side foreign-item placement + give-path core
 * (Phase 3.0 Lane C1, #392).
 *
 * Placement model (the C0 handoff's design, ADR 0002 at every boundary):
 * after MM's own fill has populated RANDO_SAVE_CHECKS, deterministically pick
 * N shuffled checks holding junk-CLASS items and mark them as hosting the
 * pinned OoT progression items. The MM save table KEEPS its junk-class MM
 * item at those checks — a raw RG_* never enters an MM table — while
 * gComboCtx.foreignPlacements records "check X hosts SharedItem{GAME_OOT,
 * id}". If the placement table is ever absent (a pre-C1 .redsave
 * zero-extends to an empty table), the hosting checks degrade to the junk
 * they physically hold: nothing crashes, nothing aliases.
 *
 * Determinism: selection uses a LOCAL xorshift32 stream seeded from the
 * paired-world identity (master seed + settings digest + MM final seed), so
 * it can never be perturbed by other Ship_Random consumers, and candidate
 * order comes from std::map's sorted iteration. Same gComboCtx + same MM
 * options => same placements, which is what the SeedDeterminism fold locks.
 *
 * Paired-world generation failure (a fill dead-end under the derived seed) is
 * NOT retried here: OnFileCreate's existing catch reverts the save to vanilla,
 * exactly as upstream does for any failed generation. The derivation is
 * attempt-free on purpose — a retry counter would make the world identity
 * depend on runtime state the digest cannot see. If a pinned CI seed
 * dead-ends, the fix is to pin a different seed, not to bend the derivation.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "Foreign.h"
#include "Rando/Rando.h"
#include "2s2h/ShipUtils.h"
// ResolvePairedProfile reads the option CVars (and asks CVarExists whether the
// player ever set one) and logs which branch of the logic pin it took.
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

// src/common — placement table + pinned pool surface, and the A1 producer
// (Combo_RecordSharedItem). Included OUTSIDE any extern "C" block: they pull
// context.h, whose <type_traits> include must not be wrapped in C linkage
// (see context.h's header comment).
#include "foreign_items.h"
#include "shared_items.h"
// Combo_CVarIsExplicitInt — "did the player choose this, or is it just the
// default", which is what the paired logic pin now turns on.
#include "combo_mm_options_view.h"

#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include "variables.h"
// SPIDER_HOUSE_TOKENS_REQUIRED — the vanilla token requirement the profile
// falls back to when skulltulas are unshuffled (same include OnFileCreate.cpp
// carries for the same constant).
#include "overlays/actors/ovl_En_Sth/z_en_sth.h"
}

namespace Rando {
namespace Foreign {

bool PairingActive() {
    return Combo_ForeignPairingActive();
}

std::string PairedInputSeedString() {
    // Plain decimal of the master seed with a stable prefix: deterministic,
    // filename-safe (the spoiler lands at randomizer/<inputSeed>.json), and
    // recognizable in logs/artifacts as a paired-world file.
    return "RSBSPAIR" + std::to_string(gComboCtx.sharedRandoSeed);
}

static std::string MMOptionsString() {
    // The persisted options ARE the finalized MM settings profile — the loop
    // below must run after OnFileCreate copied them into the save. std::map
    // iteration gives a stable option order.
    //
    // SEED-DERIVATION INPUT ONLY, deliberately unchanged by the #498/#564
    // identity widening: MixPairedFinalSeed() hashes this exact string, so
    // touching it would re-derive every shipped pair's MM world. The widened
    // IDENTITY string lives in ProfileIdentityString below.
    std::string s;
    for (auto& [randoOptionId, randoStaticOption] : Rando::StaticData::Options) {
        s += std::to_string(RANDO_SAVE_OPTIONS[randoOptionId]);
        s += ';';
    }
    return s;
}

// ---------------------------------------------------------------------------
// The profile freeze (#498 decision 1 per #564, phase 2 step 9).
//
// ONE resolution, ONE identity string, TWO call sites. The creation event
// (OoT's Playthrough_Init, via MM_Rando_ComputeProfileStamp at the bottom of
// this file) resolves the profile from the CVars and stamps its digest into
// gComboCtx.mmProfileDigest; every later arrival that would generate resolves
// the SAME way and compares. Both sites go through ResolveProfileValues +
// ProfileIdentityString, so "what creation froze" and "what arrival checks"
// cannot drift apart — they are one computation.
// ---------------------------------------------------------------------------

/** Resolve the full option profile from the CVars into `values` (RO_MAX
 *  entries), with no side effects: the generic copy, the skulltula
 *  correction, and — paired only — the RO_LOGIC default pin. This is the
 *  resolution OnFileCreate has always performed; extracting it is what lets
 *  the creation event run it without an MM save to write into. */
static void ResolveProfileValues(uint32_t* values, bool paired) {
    // (1) The generic copy: every registered option's CVar, with the
    // StaticData row's default as the fallback — what makes the pane's CVars
    // the (pre-creation) authoring surface.
    for (auto& [randoOptionId, randoStaticOption] : Rando::StaticData::Options) {
        values[randoOptionId] = (uint32_t)CVarGetInteger(randoStaticOption.cvar, randoStaticOption.defaultValue);
    }

    // (2) Derived correction: with skulltulas unshuffled the token requirement
    // is the vanilla one, whatever the slider says. Runs for solo and paired
    // files alike, as it always has.
    if (!values[RO_SHUFFLE_GOLD_SKULLTULAS]) {
        values[RO_MINIMUM_SKULLTULA_TOKENS] = SPIDER_HOUSE_TOKENS_REQUIRED;
    }

    if (!paired) {
        return;
    }

    // (3) The logic pin (#426 rationale), a DEFAULT rather than a law (#499
    // step 3): the MVP pairs under Nearly No Logic unless the player made an
    // explicit choice. EXISTENCE, not "is the value already extreme" — only
    // existence distinguishes "the player chose Glitchless" from "nobody ever
    // touched the key and the default is Glitchless". Because the pin runs
    // inside the shared resolution, the RESOLVED value is what both the
    // creation stamp and the arrival compare see (#564 V3): nothing after
    // creation depends on CVar existence except through this one resolution,
    // and a mid-gap flip of the key is a detected divergence, not a silently
    // honored choice.
    if (!Combo_CVarIsExplicitInt(Rando::StaticData::Options[RO_LOGIC].cvar)) {
        SPDLOG_INFO("Paired profile resolution: no explicit MM logic choice; defaulting to Nearly No Logic (#426)");
        values[RO_LOGIC] = RO_LOGIC_NEARLY_NO_LOGIC;
    } else {
        SPDLOG_INFO("Paired profile resolution: honouring explicit MM logic choice ({})", (unsigned)values[RO_LOGIC]);
    }
}

/** The canonical identity string the profile digest hashes (#564 V4's widened
 *  term). Wider than MMOptionsString on purpose: gRando.ExcludedChecks and the
 *  StartingItems config block shape the generated world exactly as the 47
 *  options do (Logic/GeneratePools.cpp:32, StartingItems.cpp:135), so a digest
 *  that omits them is a vacuous guard — same seed + same digest could still
 *  yield different MM worlds. OoT's own settings hash is the precedent: it
 *  folds excludes and tricks. */
static std::string ProfileIdentityString(const uint32_t* values) {
    std::string s;
    for (auto& [randoOptionId, randoStaticOption] : Rando::StaticData::Options) {
        s += std::to_string(values[randoOptionId]);
        s += ';';
    }
    // The excluded-check list, folded as the raw CVar string GeneratePools
    // consumes. Raw rather than parse-normalized on purpose: the only writer
    // is UI code emitting a canonical "id,id,..." form, and a string that
    // changed in ANY way between creation and arrival means something wrote
    // the identity input mid-pair — which is precisely what the digest exists
    // to catch.
    s += "|excluded:";
    s += CVarGetString("gRando.ExcludedChecks", "");
    // The StartingItems config block, folded as the resolved item-id list —
    // the SAME accessor generation consumes (unknown names dropped both
    // places, defaults on an absent config/context both places), so the
    // identity term and the generation input are one computation.
    s += "|starting:";
    for (RandoItemId randoItemId : Rando::GetStartingItemsFromConfig()) {
        s += std::to_string((int)randoItemId);
        s += ',';
    }
    return s;
}

/** Hash the identity string, displacing 0: zero is the growth contract's
 *  "unset" for every field carved from reserved[] (context.h) — and now also
 *  the frozen-state predicate's "not frozen" — so a real profile hashing to 0
 *  would read as "no identity recorded", a mismatch that silently cannot be
 *  detected. The collision this introduces is with one other profile out of
 *  2^32, against a certainty of a false negative. */
static uint32_t DigestFromIdentity(const std::string& identity) {
    uint32_t digest = Ship_Hash(identity);
    if (digest == 0) {
        digest = 0x4D4D5044u; // 'MMPD'
    }
    return digest;
}

uint32_t ResolvePairedProfile(bool paired) {
    std::vector<uint32_t> values(RO_MAX, 0);
    ResolveProfileValues(values.data(), paired);
    for (auto& [randoOptionId, randoStaticOption] : Rando::StaticData::Options) {
        RANDO_SAVE_OPTIONS[randoOptionId] = values[randoOptionId];
    }

    if (!paired) {
        // A solo MM rando file has no cross-game identity to publish, and must
        // not stamp one: gComboCtx.mmProfileDigest is read as "the paired
        // world's frozen profile", and writing it here would make an unpaired
        // file claim a pairing it does not have.
        return 0;
    }

    // Compare-or-stamp against the creation identity (#498/#564 phase 2).
    // Seed-INDEPENDENT on purpose: this answers "was this world generated
    // under the frozen MM rules", a question about the rules alone;
    // MixPairedFinalSeed() folds the master seed separately for the different
    // question of "which world does this seed derive".
    const uint32_t digest = DigestFromIdentity(ProfileIdentityString(values.data()));
    if (gComboCtx.mmProfileDigest != 0 && gComboCtx.mmProfileDigest != digest) {
        // NEVER self-heal (overwrite the stamp with the divergent profile) and
        // NEVER generate under it. Throwing lands in OnFileCreate's catch,
        // which reverts the save to vanilla — no divergent world is authored.
        // The player-visible surface for the natural path lives EARLIER, in
        // MM_Rando_PairOnCrossGameArrival, which runs this same computation
        // and refuses before dispatching generation at all; this throw is the
        // class-level backstop for every other route into OnFileCreate (MM
        // file-select escapes, dev tools).
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "paired profile identity mismatch: creation stamped %08X, this resolution yields %08X "
                 "(options/excludes/starting items changed after creation)",
                 (unsigned)gComboCtx.mmProfileDigest, (unsigned)digest);
        SPDLOG_ERROR("{}", msg);
        throw std::runtime_error(msg);
    }
    if (gComboCtx.mmProfileDigest == 0) {
        // LEGACY pre-freeze pair (created before the creation event stamped
        // identities): the first crossing is where its profile freezes. The
        // one transitional writer besides the creation event.
        fprintf(stderr, "[MM] profile: no creation-time stamp (pre-freeze pair); freezing profile at this "
                        "generation (digest %08X)\n",
                (unsigned)digest);
        gComboCtx.mmProfileDigest = digest;
    }
    return digest;
}

uint32_t MixPairedFinalSeed() {
    // Mirrors OoT's Playthrough_Init: Hash(str(master seed) + settings), so
    // the same master seed reproduces the MM world only under the same MM
    // options — the "one seed + one pinned settings profile" contract.
    return Ship_Hash(std::to_string(gComboCtx.sharedRandoSeed) + MMOptionsString());
}

// Local, self-contained PRNG for placement selection (see the file header).
static uint32_t sSelectState;

static uint32_t SelectNext() {
    uint32_t x = sSelectState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sSelectState = (x != 0) ? x : 0xB5297A4Du;
    return sSelectState;
}

// ---------------------------------------------------------------------------
// Host eligibility (#488).
//
// The predicate this replaced was a BLOCKLIST — "shuffled, holds a junk-class
// item, and is not a shop" — evaluated over a check table (RandoStaticCheck:
// {id, name, type, scene, flagType, flag, item}) that carries no give-path
// information whatsoever. That shape is the structural defect: CheckQueue's
// foreign branch is a fork nested INSIDE `if (randoSaveCheck.eligible)`
// (MiscBehavior/CheckQueue.cpp:39-53), so any host whose `.eligible` bit is
// never armed strands a pinned OoT progression item forever. Nothing logs,
// nothing errors, and the paired world is unwinnable by construction.
//
// So it is an ALLOWLIST now: a check class is a legal host only once someone
// has traced its arming chain. Ship Tier A only.
//
// TIER A — RCTYPE_CHEST carrying FLAG_CYCL_SCENE_CHEST. The arming chain,
// traced end to end: z_en_box.c:488-495 (MM_EnBox_WaitOpen) calls
// MM_Flags_SetTreasure, which at z_actor.c:860-869 fires
// GameInteractor_ExecuteOnSceneFlagSet(sceneId, FLAG_CYCL_SCENE_CHEST, flag),
// which MiscBehavior/OnFlagSet.cpp:19-33 turns into `.eligible = true` for the
// resolved check. All 128 chest rows carry FLAG_CYCL_SCENE_CHEST today; the
// flagType is re-checked per row rather than assumed, because 60
// RCTYPE_SKULL_TOKEN rows share that flag space and a future FLAG_NONE chest
// row must not silently inherit Tier A's guarantee.
//
// One honest caveat, since the point of this predicate is not to overclaim:
// 31 of the 128 chest rows hold a stray fairy in vanilla, and z_en_box.c:488
// routes GI_STRAY_FAIRY down a branch that never calls Flags_SetTreasure.
// Those chests arm because the rando EnBox ShouldActorInit hook
// (ActorBehavior/EnBox.cpp:176-197) first rewrites the contained item to
// GI_RECOVERY_HEART, putting them back on the flag-setting branch. That hook
// bails on `!RANDO_SAVE_CHECKS[id].shuffled` — the SAME bit this predicate
// requires below — so it always runs for a check we would host on. The
// guarantee is therefore "armed for every check this predicate accepts", not
// "armed with zero rando code involved"; the distinction matters only if the
// EnBox hook's registration condition ever diverges from `.shuffled`.
//
// TIER B — every non-shop check with `flagType != FLAG_NONE` minus the false
// friend FLAG_RANDO_INF (rando-inf flags are written by rando code, not by
// vanilla actors — z_actor.c:1026). Compiled out. It is NOT audited: the
// FLAG_WEEK_EVENT_REG-backed NPC/MINIGAME rows have not been checked for a
// rando VB hook that replaces the NPC's give AND suppresses the vanilla flag
// write, which is exactly the failure this whole predicate exists to prevent.
// Do not define RSBS_FOREIGN_HOST_TIER_B until that audit lands (#488).
static bool IsAllowedHostClass(const Rando::StaticData::RandoStaticCheck& randoStaticCheck) {
    // Kept explicit even though the allowlist below already excludes them: the
    // shop give/price flow and spoiler shape differ from the ordinary
    // eligible->CheckQueue path the generic foreign presentation targets, so
    // this exclusion must survive any future widening of the tiers.
    if (randoStaticCheck.randoCheckType == RCTYPE_SHOP || randoStaticCheck.randoCheckType == RCTYPE_TINGLE_SHOP) {
        return false;
    }

    // Tier A.
    if (randoStaticCheck.randoCheckType == RCTYPE_CHEST && randoStaticCheck.flagType == FLAG_CYCL_SCENE_CHEST) {
        return true;
    }

#ifdef RSBS_FOREIGN_HOST_TIER_B
    if (randoStaticCheck.flagType != FLAG_NONE && randoStaticCheck.flagType != FLAG_RANDO_INF) {
        return true;
    }
#endif

    return false;
}

bool IsEligibleHost(RandoCheckId randoCheckId) {
    if (randoCheckId <= RC_UNKNOWN || randoCheckId >= RC_MAX) {
        return false;
    }

    const auto staticIt = Rando::StaticData::Checks.find(randoCheckId);
    if (staticIt == Rando::StaticData::Checks.end() || staticIt->second.randoCheckId == RC_UNKNOWN) {
        return false;
    }
    if (!IsAllowedHostClass(staticIt->second)) {
        return false;
    }

    const RandoSaveCheck& randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];

    // `.shuffled` alone was the old predicate's only save-side test, and it is
    // not sufficient in either direction. A USER-EXCLUDED check is marked
    // `shuffled = true; randoItemId = RI_JUNK; skipped = true` by
    // Logic/GeneratePools.cpp and is deliberately kept OUT of checkPool — so
    // under the old predicate an excluded check was a top-priority host for a
    // pinned progression item. `.skipped` is the bit that says so.
    if (!randoSaveCheck.shuffled || randoSaveCheck.skipped) {
        return false;
    }

    // RI_UNKNOWN (enumerator 0, i.e. a zero-initialised or unresolvable slot)
    // and RI_NONE ("literally nothing") are both declared RITYPE_JUNK, so a
    // type-only test accepts an item that is not an item. Rejecting them keeps
    // ADR 0002's host invariant honest: a foreign host must physically hold a
    // LEGAL junk-class MM item, because that is what the check degrades to if
    // the placement table is ever absent.
    const RandoItemId heldItem = randoSaveCheck.randoItemId;
    if (heldItem == RI_UNKNOWN || heldItem == RI_NONE) {
        return false;
    }
    const auto itemIt = Rando::StaticData::Items.find(heldItem);
    if (itemIt == Rando::StaticData::Items.end()) {
        return false;
    }
    return itemIt->second.randoItemType == RITYPE_JUNK;
}

int PlaceForeignItems() {
    Combo_ClearForeignPlacements();

    if (!PairingActive()) {
        return 0;
    }

    const ComboForeignItemDef* pool = nullptr;
    const int poolCount = Combo_GetForeignItemPool(&pool);
    if (poolCount <= 0 || pool == nullptr) {
        return 0;
    }

    // Candidates: checks IsEligibleHost accepts, in ascending RandoCheckId
    // order (std::map), so selection stays deterministic. The predicate is a
    // named function rather than an inline condition precisely so the CI lock
    // can drive it directly — see IsEligibleHost above for what it enforces and
    // why the previous inline blocklist was unsound.
    //
    // The junk-class requirement inside it is the one part carried over
    // unchanged: the literal RI_JUNK sentinel alone is NOT the criterion,
    // because the pool balancer only injects it when the check pool outnumbers
    // the item pool (a measured CI fill had 2), while junk-class items are
    // plentiful.
    std::vector<RandoCheckId> candidates;
    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        if (IsEligibleHost(randoCheckId)) {
            candidates.push_back(randoCheckId);
        }
    }

    // Printed every generation on purpose: Tier A drops the candidate set from
    // ~2000 to a few dozen, so host supply is now a number worth watching in
    // CI logs and playtest output BEFORE it becomes a shortfall.
    fprintf(stderr, "[MM] foreign placement: %d pool items over %zu eligible host checks\n", poolCount,
            candidates.size());

    sSelectState =
        Ship_Hash(std::to_string(gComboCtx.sharedRandoSeed) + ":" + std::to_string(gComboCtx.sharedRandoSettingsHash) +
                  ":" + std::to_string(gSaveContext.save.shipSaveInfo.rando.finalSeed) + ":foreign-v1");
    if (sSelectState == 0) {
        sSelectState = 0xB5297A4Du;
    }

    int placed = 0;
    for (int i = 0; i < poolCount && !candidates.empty(); i++) {
        const size_t pick = (size_t)(SelectNext() % (uint32_t)candidates.size());
        const RandoCheckId hostCheck = candidates[pick];
        candidates.erase(candidates.begin() + (std::ptrdiff_t)pick);

        if (Combo_SetForeignPlacement((uint16_t)hostCheck, pool[i].item) >= 0) {
            placed++;
            fprintf(stderr, "[MM] foreign placement: '%s' hosted at MM check %s\n", pool[i].name,
                    Rando::StaticData::Checks[hostCheck].name);
        }
    }

    if (placed < poolCount) {
        fprintf(stderr, "[MM] foreign placement: only %d of %d pool items placed (eligible hosts exhausted)\n", placed,
                poolCount);
    }
    return placed;
}

static const SharedItem* PlacementFor(RandoCheckId randoCheckId) {
    if (randoCheckId == RC_UNKNOWN) {
        return nullptr;
    }
    return Combo_GetForeignPlacementForCheck((uint16_t)randoCheckId);
}

bool IsForeignCheck(RandoCheckId randoCheckId) {
    return PlacementFor(randoCheckId) != nullptr;
}

const char* ForeignNameForCheck(RandoCheckId randoCheckId) {
    const SharedItem* item = PlacementFor(randoCheckId);
    if (item == nullptr) {
        return nullptr;
    }
    return Combo_GetForeignItemName(*item);
}

const char* ForeignArticleForCheck(RandoCheckId randoCheckId) {
    const SharedItem* item = PlacementFor(randoCheckId);
    if (item == nullptr) {
        return "";
    }
    const char* article = Combo_GetForeignItemArticle(*item);
    return article != nullptr ? article : "";
}

bool RecordForeignPickup(RandoCheckId randoCheckId) {
    const SharedItem* item = PlacementFor(randoCheckId);
    if (item == nullptr) {
        return false;
    }
    // Durable immediately (Combo_RecordSharedItem writes the serialized array,
    // so an MM save+quit before the next switch cannot lose the pickup — the
    // stage/commit outbox is RAM-only, see shared_items.h). The producer
    // de-dups an identical un-redeemed entry, so a re-fired give cannot
    // double-record.
    return Combo_RecordSharedItem((GameId)item->originGame, item->id) >= 0;
}

} // namespace Foreign
} // namespace Rando

// ============================================================================
// Creation-stamp bridge (#498/#564 phase 2 step 9; declared in
// src/common/combo_mm_options_view.h).
//
// The ONE freeze/compare computation, callable from outside MM: resolves the
// full profile identity from the CVars — through the SAME ResolveProfileValues
// + ProfileIdentityString the arrival's ResolvePairedProfile uses — and
// returns its digest, with no side effects on any save, CVar, or gComboCtx.
// OoT's Playthrough_Init assigns the result to gComboCtx.mmProfileDigest as
// part of publishing the pairing identity (the creation event); MM's arrival
// gate (MM_Rando_PairOnCrossGameArrival) recomputes it to compare against
// that stamp before any generation dispatch.
// ============================================================================
extern "C" uint32_t MM_Rando_ComputeProfileStamp(void) {
    std::vector<uint32_t> values(RO_MAX, 0);
    ResolveProfileValues(values.data(), /*paired=*/true);
    return DigestFromIdentity(ProfileIdentityString(values.data()));
}

// ============================================================================
// ROM-free test bridge (redship tier; src/common/tests/test_foreign_items.c).
// Drives the SAME give-path core the CheckQueue lambda calls, so the lock
// covers the real recording path, not a copy.
// ============================================================================
extern "C" int MM_Rando_Foreign_RecordPickup(uint16_t randoCheckId) {
    return Rando::Foreign::RecordForeignPickup((RandoCheckId)randoCheckId) ? 1 : 0;
}

// #488's host-eligibility lock. This is the bridge that makes the lock
// non-vacuous: it calls the SAME function PlaceForeignItems' candidate loop
// calls, so a test that drives it is testing selection, not a paraphrase of
// selection. Without the extraction above, the only observable for "was this a
// safe host?" would be a whole generated world.
extern "C" int MM_Rando_Foreign_IsEligibleHost(uint16_t randoCheckId) {
    return Rando::Foreign::IsEligibleHost((RandoCheckId)randoCheckId) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Test-support surface for the same lock (src/common/tests/test_foreign_items.c).
//
// The predicate reads two things the ROM-free tier cannot reach from
// src/common: Rando::StaticData::Checks (MM C++ static data) and
// RANDO_SAVE_CHECKS (a field deep inside MM's gSaveContext). These accessors
// exist so the test can build a synthetic save over the REAL check table
// rather than a mock one. They are inspection/stamping only — none of them is
// reachable from gameplay, and none duplicates predicate logic.
// ---------------------------------------------------------------------------

/** RC_MAX — the exclusive upper bound for a RandoCheckId walk. */
extern "C" int MM_Rando_Foreign_TestCheckIdMax(void) {
    return (int)RC_MAX;
}

/** Static classification of one check row. Returns 1 if the id names a real
 *  row, 0 otherwise. Reports the two facts Tier A is defined in terms of, so
 *  the test can assert the table invariant (every chest row carries
 *  FLAG_CYCL_SCENE_CHEST) without importing MM's enums into src/common. */
extern "C" int MM_Rando_Foreign_TestCheckClass(uint16_t randoCheckId, int* outIsChestType, int* outHasChestFlag) {
    const auto it = Rando::StaticData::Checks.find((RandoCheckId)randoCheckId);
    if (it == Rando::StaticData::Checks.end() || it->second.randoCheckId == RC_UNKNOWN) {
        return 0;
    }
    if (outIsChestType != nullptr) {
        *outIsChestType = (it->second.randoCheckType == RCTYPE_CHEST) ? 1 : 0;
    }
    if (outHasChestFlag != nullptr) {
        *outHasChestFlag = (it->second.flagType == FLAG_CYCL_SCENE_CHEST) ? 1 : 0;
    }
    return 1;
}

/** Stamp one row's save-side state (the three fields the predicate reads). */
extern "C" void MM_Rando_Foreign_TestStampCheck(uint16_t randoCheckId, int shuffled, int skipped, uint16_t itemId) {
    if (randoCheckId == 0 || randoCheckId >= (uint16_t)RC_MAX) {
        return;
    }
    RandoSaveCheck& randoSaveCheck = RANDO_SAVE_CHECKS[(RandoCheckId)randoCheckId];
    randoSaveCheck.shuffled = (shuffled != 0);
    randoSaveCheck.skipped = (skipped != 0);
    randoSaveCheck.randoItemId = (RandoItemId)itemId;
}

/** Stamp every row — the "synthetic all-shuffled save" the whole-table sweep
 *  runs over, and the reset the test leaves behind. */
extern "C" void MM_Rando_Foreign_TestStampAllChecks(int shuffled, int skipped, uint16_t itemId) {
    for (int i = 1; i < (int)RC_MAX; i++) {
        MM_Rando_Foreign_TestStampCheck((uint16_t)i, shuffled, skipped, itemId);
    }
}

/** The three RandoItemId values the predicate treats specially: the legal junk
 *  filler, and the two sentinels that are RITYPE_JUNK but are not items. */
extern "C" void MM_Rando_Foreign_TestItemSentinels(uint16_t* outJunk, uint16_t* outNone, uint16_t* outUnknown) {
    if (outJunk != nullptr) {
        *outJunk = (uint16_t)RI_JUNK;
    }
    if (outNone != nullptr) {
        *outNone = (uint16_t)RI_NONE;
    }
    if (outUnknown != nullptr) {
        *outUnknown = (uint16_t)RI_UNKNOWN;
    }
}

#endif // RSBS_SINGLE_EXECUTABLE
