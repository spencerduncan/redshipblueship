/**
 * @file foreign_items.h
 * @brief Cross-game (foreign) item placements, their names, and the combo-level
 *        settings record (Phase 3.0 Lane C1, #392; ADR 0002; ADR 0011).
 *
 * Data surfaces behind this header:
 *
 *  1. The PLACEMENT TABLES — `gComboCtx.foreignPlacements[]` /
 *     `foreignPlacementsOoT[]` (context.h), the serialized "host check X holds
 *     foreign item Y" record, and behind them the crossing store
 *     (crossing_store.h), which is where every crossing the single-bag fill
 *     makes lives (ADR 0010 increment 3). The give paths read both through
 *     Combo_GetForeignPlacementForCheck / ...ForOoTCheck. A host save's own
 *     check table keeps a legal item of its own game — a raw RG_* never enters
 *     an MM table and a raw RI_* never enters an OoT one (ADR 0002).
 *
 *  2. NAMES — each game's engine TU registers a describer (below) that answers
 *     an item id or check id of its OWN id-space with a stable display name.
 *     src/common never sees either game's enum.
 *
 * THE PINNED POOLS ARE RETIRED (ADR 0010 increment 3, D3; lane K11). Until
 * then two hand-transcribed tables (OoT's kForeignPoolV1, MM's kForeignPoolMMV1)
 * fed two overlay passes that pinned a few items onto the other game's junk as
 * duplicate copies. Under one bag items LEAVE their origin pools: the single-bag
 * fill (combo_single_bag.h) decides which items cross and where, the O8
 * classification owner (shared_items.h) decides which items may, and nothing
 * here enumerates a pool any more.
 *
 * The give-time flow (who calls what):
 *   host give path -> Combo_GetForeignPlacementForCheck (pinned table, then the
 *   crossing store) -> Combo_RecordSharedItemCrossing (shared_items.h; durable
 *   immediately, one copy per host) -> presented with Combo_GetForeignItemName.
 *   On the next arrival in the origin game, A1's consumer
 *   (Combo_RedeemSharedItemsForGame) awards it through that game's
 *   *_ForeignItem_Give and marks it RSBS_SHARED_ITEM_REDEEMED.
 */

#ifndef RSBS_COMMON_FOREIGN_ITEMS_H
#define RSBS_COMMON_FOREIGN_ITEMS_H

#include "context.h" // SharedItem, ComboForeignPlacement, gComboCtx, RSBS_FOREIGN_PLACEMENT_CAP

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Names for foreign items (ADR 0009 decision 3; ADR 0010 O7)
// ============================================================================
//
// (originGame, id) and (originGame, name) are the keys; a bare name is not.
// "Lens of Truth" is a real display name in BOTH id-spaces (OoT's
// RG_LENS_OF_TRUTH, MM's RI_LENS), so a name-only inverse would resolve to
// whichever game it asked first and write a WRONG ORIGIN TAG — the #356
// aliasing class, arriving through the path that rebuilds state from text on
// disk. The origin is therefore always an argument, and every lookup below is
// answered by that origin's describer alone.

/** Highest GameId the per-game registries (describers, engines) index,
 *  exclusive (GAME_NONE..GAME_MM). */
#define RSBS_FOREIGN_POOL_ORIGIN_COUNT 3u

/**
 * Display/spoiler name for a foreign item: its origin game's describer name
 * (Combo_DescribeItemName), or NULL when the origin registered no describer or
 * does not know the id. The SharedItem carries its own tag, so an OoT id is
 * never named from MM's table or vice versa. Flags are ignored on purpose: a
 * redeemed entry keeps its name.
 *
 * Before increment 3 this answered only for the pinned pool's rows, which also
 * carried an article and an arrival-toast icon; both now come from the same
 * describer (Combo_GetForeignItemArticle / Combo_GetForeignItemIconName).
 */
const char* Combo_GetForeignItemName(SharedItem item);

/**
 * The article that belongs in front of Combo_GetForeignItemName's result
 * ("the ", "a ", "an " or "" — it carries its own trailing space, so
 * `article + name` needs no separator), from the origin game's describer; NULL
 * when that game registered none or does not know the id.
 *
 * A cross-game item is presented as an ORDINARY pickup of whichever game the
 * player found it in (#510), and both games build that sentence as article +
 * name; the host cannot read the origin's item table (ADR 0002), so the article
 * travels through the describer exactly as the name does.
 */
const char* Combo_GetForeignItemArticle(SharedItem item);

/**
 * The arrival-toast icon for an item arriving in its OWN origin game (#494):
 * that game's texture-map key, or NULL for a text-only toast (Notification::Emit
 * renders one for a null icon, the same idiom the native pickup paths use). The
 * string is the describer's static storage and outlives a toast that keeps the
 * bare pointer.
 */
const char* Combo_GetForeignItemIconName(SharedItem item);

/**
 * The inverse of Combo_GetForeignItemName, keyed on (originGame, name): the
 * lowest id in @p originGame's id-space whose describer name is @p name. Used by
 * MM's spoiler-LOAD path to rebuild a placement table from a spoiler's
 * "foreign" section without either game's header in scope — the SharedItem is
 * tagged by construction, so a raw RG_* (or RI_*) is never fabricated on the
 * far side (ADR 0002).
 *
 * TOTAL over every nameable item of that origin, independent of any frozen
 * selection, so a spoiler that was valid at generation stays readable in a
 * process that never generated (ADR 0011 decision 3.2).
 *
 * @param originGame GAME_OOT or GAME_MM (GAME_NONE is rejected)
 * @param name       the display name to look up (NULL is rejected)
 * @param outItem    receives the tagged SharedItem on a match (may be NULL)
 * @return true if that origin's describer names some id @p name.
 */
bool Combo_GetForeignItemByNameFor(uint8_t originGame, const char* name, SharedItem* outItem);

/**
 * The OoT-origin name inverse. Retained so the call sites that predate the
 * origin dimension keep their exact meaning (they all read OoT-origin names).
 * New code should use the _For form and carry the origin explicitly.
 */
bool Combo_GetForeignItemByName(const char* name, SharedItem* outItem);

// ============================================================================
// Placement table accessors (gComboCtx.foreignPlacements)
// ============================================================================

/**
 * True when the paired-world keying is satisfied: an OoT rando world was
 * generated in the live path (gComboCtx.sourceIsRando) AND it recorded its
 * settings profile digest (sharedRandoSettingsHash != 0 — Lane B's carrier
 * contract: 0 means "no profile recorded" and the worlds must not pair).
 *
 * THE POST-CONDITION, past tense: "a paired world EXISTS". Read at give time.
 * It has two siblings and reviewers must not collapse the three (ADR 0009
 * decision 2's amendment — they are three tenses of one noun):
 *
 *   Combo_ForeignPairingRequested()  -- future:  a paired world is being ASKED for
 *   Combo_ComboSettingsFrozen()      -- present: this world's rules are FROZEN
 *   Combo_ForeignPairingActive()     -- past:    a paired world EXISTS
 *
 * NONE OF THE THREE CARRIES A DIRECTION TERM (#667, 2026-09-17). OFF is a
 * paired world with zero crossings (ADR 0011 decision 2.3), so "is it paired?"
 * is answered identically in all three tenses, and the separate question "does
 * it cross anything?" is Combo_ForeignCrossingsRequested() / the frozen
 * Combo_ComboDirectionArms(). Folding direction back into any of the three
 * makes OFF read as an unpaired world, which is the MM-plays-vanilla class.
 */
bool Combo_ForeignPairingActive(void);

// ============================================================================
// Combo-level settings: the pinned value spaces (ADR 0011 decision 1.2.1)
// ============================================================================
//
// EVERY ENUMERATOR AND BIT POSITION BELOW IS ASSIGNED AN EXPLICIT NUMERIC
// LITERAL AND IS APPEND-ONLY. To remove a value, RETIRE IT IN PLACE — keep the
// literal, mark it dead, never renumber.
//
// This is stated as a rule rather than left to the implementer because the
// default C idiom (a bare sequential `enum`) gets it wrong SILENTLY, and two
// separate things break when it does:
//
//  1. A LOCAL save corruption with no netplay involved. Inserting a value
//     mid-list reassigns the meaning of direction/goal/logicRung/itemClass* in
//     every already-written formatVersion >= 1 record, so a newer binary reads
//     an old world's rules as different rules.
//  2. comboSettingsHash stops being usable as a cross-peer comparison term the
//     moment #574's identity handshake exchanges it: two peers on builds either
//     side of a renumbering compute different hashes for identical settings —
//     or, if two renumberings cancel, IDENTICAL hashes for DIFFERENT settings,
//     which is the failure mode a digest exists to prevent.
//
// Same discipline as RSBS_SHARED_RES_* (context.h, explicit = 0, = 1, = 2, ...)
// and as MM's own Options.cpp, which retires a dead option row rather than
// deleting it because RANDO_SAVE_OPTIONS is indexed by that number in every
// already-written MM rando save.

/** The ComboSettingsRecord format version this build writes and understands.
 *  Bump it when old content must be DISTINGUISHED, not merely extended (ADR
 *  0002 §4's COMBO_CONTEXT_VERSION rule, scoped to one block) — i.e. when a
 *  spare is spent by a field whose ZERO means something other than what a
 *  pre-existing record already meant, so a reader must tell "field absent" from
 *  "field zero".
 *
 *  STILL 1 AFTER #668, deliberately, and this is the test the next spender
 *  applies. `comboFlags` (byte 10, formerly spare0) was spent by a BITSET whose
 *  every clear bit reproduces the behaviour of a record written before the bit
 *  existed: shared ocarina OFF is what every world before #668 did. "Absent"
 *  and "zero" are therefore the same world, there is nothing to distinguish,
 *  and a bump would have been strictly harmful — formatVersion is
 *  canonical()[0], so every new world's comboSettingsHash would have moved and
 *  every determinism digest with it, for a setting nobody turned on. Spend
 *  `spare1` the same way ONLY if its zero is likewise the legacy behaviour. */
#define RSBS_COMBO_SETTINGS_FORMAT_VERSION 1u

// Direction. Pinned, append-only, retire-never-renumber.
// 0 is unreachable inside a formatted record: a legacy record zero-extends to
// all zeros, so if 0 meant OFF every pre-3.1 paired save would silently lose
// its crossings on the first load by a new build (context.h, decision 1.3).
#define RSBS_COMBO_DIR_OFF 1u     // paired world, zero crossings (a real, chooseable world)
#define RSBS_COMBO_DIR_FORWARD 2u // OoT-origin items into MM checks only
#define RSBS_COMBO_DIR_REVERSE 3u // MM-origin items into OoT checks only
#define RSBS_COMBO_DIR_BOTH 4u    // today's shipped behaviour; the accepted answer O2 default

// GOAL. ADR 0010 D1 owns the value LIST; ADR 0011 owns their ENCODING.
// Pinned, append-only.
#define RSBS_COMBO_GOAL_BEAT_BOTH 1u
#define RSBS_COMBO_GOAL_BEAT_EITHER 2u
#define RSBS_COMBO_GOAL_TRIFORCE_HUNT 3u

// Logic rung. ADR 0010 §2.2's ladder; same ownership split. The trick set T is
// a PARAMETER of the rung and is deliberately NOT encoded here — it is each
// half's own authored settings, reached through the half-digests
// comboSettingsHash folds.
#define RSBS_COMBO_RUNG_NONE 1u          // base: no proof; the spoiler carries the burden
#define RSBS_COMBO_RUNG_BEATABLE 2u      // GOAL provable under the frozen trick set
#define RSBS_COMBO_RUNG_ALL_REACHABLE 3u // GOAL provable and every location reachable

// Item classes. Pinned BIT POSITIONS, append-only: allocate the next free bit,
// never re-point an allocated one. Bits 0x0040..0x8000 are UNALLOCATED and must
// read as 0 in a formatVersion == 1 record, which is what lets a future build
// tell "class not armed" from "class did not exist yet" by the record's own
// version.
//
// A class bit is only ever a FILTER OVER CANDIDATES THAT ALREADY PASSED the six
// membership criteria (ADR 0011 decision 3.1). No bit can widen a pool past
// them — in particular no bit can readmit a #525 shared cross-game resource
// (wallet / heart / magic / ammo / hookshot), because the criteria run FIRST and
// the bitset selects among the survivors. Increment 3 may append bits; it may
// not weaken that ordering.
#define RSBS_ITEMCLASS_PROGRESSION 0x0001u   // progressive/major items
#define RSBS_ITEMCLASS_SONGS 0x0002u         //
#define RSBS_ITEMCLASS_MASKS 0x0004u         //
#define RSBS_ITEMCLASS_DUNGEON_ITEMS 0x0008u // small/boss keys, maps, compasses
#define RSBS_ITEMCLASS_DUNGEON_REWARD 0x0010u // medallions, stones, remains
#define RSBS_ITEMCLASS_SIDEQUEST 0x0020u     // non-progression sidequest rewards

/** Every bit ALLOCATED at formatVersion 1, and the shipped default for both
 *  directions. It is the union rather than a narrower selection on purpose:
 *  today's two pools are hand-transcribed and filtered by no class at all, so
 *  any narrower default would silently narrow them the moment increment 3
 *  makes the bitset load-bearing. "Reproduce today's world exactly" is the
 *  binding constraint on every default in this file. */
#define RSBS_ITEMCLASS_ALL_V1                                                                                   \
    (RSBS_ITEMCLASS_PROGRESSION | RSBS_ITEMCLASS_SONGS | RSBS_ITEMCLASS_MASKS | RSBS_ITEMCLASS_DUNGEON_ITEMS |   \
     RSBS_ITEMCLASS_DUNGEON_REWARD | RSBS_ITEMCLASS_SIDEQUEST)

// ---- comboFlags: the yes/no combo rules (record byte 10; #668) -------------
//
// Pinned BIT POSITIONS, append-only, retire-never-renumber — the same rule the
// item classes carry, and for the same two reasons (a local save re-read and
// #574's cross-peer hash). Bits 0x02..0x80 are UNALLOCATED and must read as 0.
//
// EVERY BIT IN THIS BYTE MUST HAVE "CLEAR == THE BEHAVIOUR BEFORE IT EXISTED".
// That is what let byte 10 be spent without a formatVersion bump (see
// RSBS_COMBO_SETTINGS_FORMAT_VERSION): a zero-extended legacy record, a record
// written by an older build, and a new record with the flag off are all the
// same world, so "absent" never has to be distinguished from "zero". A future
// flag whose CLEAR state would change an existing world does not belong in this
// byte at its current version — it needs the bump this one did not.

/**
 * ONE OCARINA ACROSS BOTH GAMES (#668; operator direction, 2026-09-16:
 * "there should be an option to have the ocarina be shared in both games").
 *
 * Set, the ocarina becomes RSBS_SHARED_RES_OCARINA_TIER — a MONOTONIC shared
 * resource on the hookshot's model (#525/#556): origin-tagged, harvested at
 * suspend, applied at the arriving game's startup entrance, never lowered by a
 * harvest. The tier is 0 none / 1 an ocarina / 2 OoT's Ocarina of Time, OoT's
 * ceiling is 2 and MM's is 1.
 *
 * THE MAPPING, stated here because it is the decision and not an implementation
 * detail:
 *   - ANY OoT ocarina gives MM its Ocarina of Time. This is the operator's
 *     direction read literally — MM has exactly one ocarina, so "shared in both
 *     games" can only mean the Fairy Ocarina counts.
 *   - MM's Ocarina of Time gives OoT the FAIRY Ocarina, not the Ocarina of
 *     Time. The shared model transports what the player HAS; it never invents a
 *     promotion. Mapping MM's single rung onto OoT's top rung would award the
 *     Door of Time's key for the price of a Termina round trip, to a player who
 *     only ever found the Fairy Ocarina — and that is the same ruling the
 *     hookshot already carries in the other direction (MM's tier-1 hookshot
 *     never becomes a longshot in OoT).
 *
 * Clear, NOTHING changes: no slot, no harvest, no apply, and a record whose
 * canonical bytes and fingerprint are exactly what they were before this bit
 * existed. That is the whole reason the default is off.
 */
#define RSBS_COMBO_FLAG_SHARED_OCARINA 0x01u

/** Every comboFlags bit ALLOCATED at formatVersion 1. The shipped default is 0
 *  — "reproduce today's world exactly" is the binding constraint on every
 *  default in this file, and every flag here changes a world. */
#define RSBS_COMBO_FLAGS_ALL_V1 (RSBS_COMBO_FLAG_SHARED_OCARINA)

// The pinning lock. A renumbering is a RED BUILD rather than a silently
// re-read save — the same shape RSBS_SHARED_RES_* carries, and the reason
// decision 1.2.1 is a BLOCKER finding's disposition rather than a style note.
RSBS_CTX_STATIC_ASSERT(RSBS_COMBO_DIR_OFF == 1u && RSBS_COMBO_DIR_FORWARD == 2u && RSBS_COMBO_DIR_REVERSE == 3u &&
                           RSBS_COMBO_DIR_BOTH == 4u,
                       "RSBS_COMBO_DIR_* values are .redsave format: pinned, append-only, "
                       "retire-never-renumber (ADR 0011 decision 1.2.1)");
RSBS_CTX_STATIC_ASSERT(RSBS_COMBO_GOAL_BEAT_BOTH == 1u && RSBS_COMBO_GOAL_BEAT_EITHER == 2u &&
                           RSBS_COMBO_GOAL_TRIFORCE_HUNT == 3u,
                       "RSBS_COMBO_GOAL_* values are .redsave format: pinned, append-only, "
                       "retire-never-renumber (ADR 0011 decision 1.2.1)");
RSBS_CTX_STATIC_ASSERT(RSBS_COMBO_RUNG_NONE == 1u && RSBS_COMBO_RUNG_BEATABLE == 2u &&
                           RSBS_COMBO_RUNG_ALL_REACHABLE == 3u,
                       "RSBS_COMBO_RUNG_* values are .redsave format: pinned, append-only, "
                       "retire-never-renumber (ADR 0011 decision 1.2.1)");
RSBS_CTX_STATIC_ASSERT(RSBS_ITEMCLASS_PROGRESSION == 0x0001u && RSBS_ITEMCLASS_SONGS == 0x0002u &&
                           RSBS_ITEMCLASS_MASKS == 0x0004u && RSBS_ITEMCLASS_DUNGEON_ITEMS == 0x0008u &&
                           RSBS_ITEMCLASS_DUNGEON_REWARD == 0x0010u && RSBS_ITEMCLASS_SIDEQUEST == 0x0020u &&
                           RSBS_ITEMCLASS_ALL_V1 == 0x003Fu,
                       "RSBS_ITEMCLASS_* bit positions are .redsave format: pinned, append-only, "
                       "allocate the next free bit and never re-point an allocated one (ADR 0011 "
                       "decision 1.2.1)");
RSBS_CTX_STATIC_ASSERT(RSBS_COMBO_FLAG_SHARED_OCARINA == 0x01u && RSBS_COMBO_FLAGS_ALL_V1 == 0x01u,
                       "RSBS_COMBO_FLAG_* bit positions are .redsave format: pinned, append-only, "
                       "allocate the next free bit and never re-point an allocated one (ADR 0011 "
                       "decision 1.2.1, amended 2026-09-16 for #668)");

// ============================================================================
// The class rule (#495; ADR 0011 decision 3; ADR 0010 increment 3)
// ============================================================================
//
// The six MEMBERSHIP CRITERIA this block used to number (and the per-pool
// exclusion tables that reported against them) retired with the pinned pools
// (ADR 0010 increment 3, D3). Which items may cross is now the O8
// classification owner's answer (shared_items.h, Combo_SharedItemClassFor): an
// item enters the bag as a crossing candidate only when it is a PROGRESSION
// row whose arming conditions the frozen profile meets. The #525 shared
// resources are reconciled there once (#731), a world event is armed by nothing,
// and sentinels and junk are not progression — the old criteria's outcomes,
// decided in one place instead of two hand tables.

/** One class bit's name ("progression", "songs", ...), or "(unknown)" for an
 *  unallocated bit or a mask with more than one bit set. Never NULL. */
const char* Combo_ForeignItemClassName(uint16_t classBit);

/**
 * The RESOLVED item-class bitset for crossings ORIGINATING in @p originGame:
 * the frozen record's when frozen, else the shipped default (every allocated
 * bit). The exact twin of Combo_ComboPoolSizeFor, read for the same reason —
 * the world's rules are FROZEN AT CREATION, so no live CVar may reach a
 * placement pass mid-session.
 *
 * A FROZEN zero is honoured verbatim: inside a formatted record `itemClass == 0`
 * is a legitimate "no classes armed for this direction", and it is not a hidden
 * second OFF because the direction byte says so first (ADR 0011 decision 3.3).
 *
 * UNDER ONE BAG (ADR 0010 increment 3): the single-bag fill reads the
 * PROGRESSION bit of this mask per origin, together with
 * Combo_ComboDirectionArms. A clear bit (or an unarmed direction) makes every
 * bag row of that origin HOME_ONLY — it is still placed, but only on its own
 * game's checks. No other bit is read today: the bag holds progression rows
 * only, so the other allocated classes have nothing to select among.
 * An UNFROZEN record falls back to the default instead — a zero-extended legacy
 * record would otherwise resolve to "no eligible source items at all", which is
 * the opposite of the world it was generated with.
 *
 * @return an RSBS_ITEMCLASS_* mask, or 0 for an origin with no pool.
 */
uint16_t Combo_ComboItemClassFor(uint8_t originGame);

// ============================================================================
// Combo-level settings: predicates, freeze, digest, divergence (ADR 0011)
// ============================================================================

/**
 * Fill @p out with the SHIPPED DEFAULTS — the rules that reproduce today's
 * world exactly (accepted answers O2, O4, O7 and ADR 0010 answer O11):
 * direction BOTH, both pool sizes at RSBS_FOREIGN_PLACEMENT_CAP, every
 * version-1 item class armed, GOAL beat-both, logic rung beatable.
 *
 * This is also what the O5 transitional writer freezes into a legacy pair, and
 * it is deliberately ONE definition rather than a set of scattered fallbacks:
 * a default that differs from shipped behaviour silently changes every new
 * world at the moment the setting lands, and the change is invisible in a diff.
 */
void Combo_ComboSettingsDefaults(ComboSettingsRecord* out);

/**
 * The session's RESOLVED combo settings — what a creation event would freeze
 * right now, and what an arrival compares the frozen record against.
 *
 * ONE RESOLVER, TWO CALL SITES, exactly as Rando::Foreign::ResolveProfileValues
 * is one resolution behind both the creation stamp and the arrival compare:
 * "what creation froze" and "what arrival checks" cannot drift apart if they
 * are one computation.
 *
 * THE SHIPPED DEFAULTS OVERLAID WITH THE FIVE AUTHORED FIELDS (ADR 0011
 * increment 2): direction, both pool sizes and both class bitsets are read
 * from the tier-4 `gCombo.Rando.*` keys through combo_settings_view.h's
 * Combo_ComboSettingResolved, which validates each stored value against its
 * pinned space and resolves an out-of-space value to the shipped default with
 * a logged reason — never to a new enumerator. An unset key, or a process with
 * no CVar store at all (every ROM-free row that never brings up a
 * Ship::Context), resolves to the default, so "nothing authored" and "what
 * ships" are the same record. `goal` and `logicRung` stay at their defaults:
 * ADR 0010 owns their authoring.
 *
 * "AND THE DETERMINISM DIGESTS DO NOT MOVE" USED TO END THAT SENTENCE, AND IT
 * NAMED NOTHING (#688). The determinism rows run one seed twice and diff the two
 * runs against each other; they cannot notice a world that moved, only one that
 * moved BETWEEN two runs of the same binary. What enforces the claim since #688
 * is the GOLDEN rows (GoldenSeedDigestDefault, GoldenSeedDigestProfileV1,
 * GoldenPairedAttemptDigest), which compare one run against a digest stored in
 * `tests/golden/`. If a change here moves the resolved record, those go red with
 * comboSettingsHash named as a moved INPUT field, and re-pinning is a deliberate
 * commit (docs/determinism-goldens.md).
 *
 * Read BEFORE the freeze at the creation event (Playthrough_Init: resolve ->
 * freeze), so the frozen record is what the player authored. After the freeze
 * nothing reads a CVar to decide world behaviour, and the writers in
 * combo_settings_view.h refuse while Combo_ComboSettingsFrozen() is true.
 */
void Combo_ResolveComboSettings(ComboSettingsRecord* out);

/**
 * "Could a paired world be created from THIS record?" — the #667 ruling as a
 * predicate over an arbitrary record rather than over the live session, which
 * is what makes the ruling falsifiable (test_combo_settings.c drives it with
 * every pinned direction plus two out-of-space ones).
 *
 * TRUE for RSBS_COMBO_DIR_OFF, because OFF is a paired world with zero
 * crossings (ADR 0011 decision 2.3). FALSE only for a record no consumer can
 * interpret: formatVersion 0 (the ABSENT tag, decision 4.2) or a direction
 * outside the pinned space. See the .c for why each of those must refuse a
 * creation rather than be clamped into one.
 */
bool Combo_ComboSettingsDescribePairedWorld(const ComboSettingsRecord* rec);

/**
 * The PRE-CONDITION predicate ADR 0009 decision 2 designed: "a paired WORLD is
 * being ASKED for" (future tense), answerable BEFORE generation by construction.
 *
 * Derived from the resolved settings and NEVER from gComboCtx's stamp, so it is
 * immune to the ordering hazard that made hoisting the stamp above Fill() look
 * necessary. Do not "simplify" it into Combo_ForeignPairingActive(): that one
 * is the post-condition and answers a different question in a different tense.
 *
 * ================== THE OFF RULING (#667, 2026-09-17) ======================
 * IT ANSWERS TRUE FOR RSBS_COMBO_DIR_OFF, and that is a fix rather than a
 * loosening. Until this commit the body was `direction != OFF`, which made the
 * future tense disagree with its own past tense about what "paired" means:
 *
 *   Combo_ForeignPairingActive()    == sourceIsRando && sharedRandoSettingsHash
 *                                      != 0 -- no direction term at all, so it
 *                                      is TRUE under OFF
 *   Combo_ForeignPairingRequested() == direction != OFF -- FALSE under OFF
 *
 * ADR 0011 decision 2.3 is explicit that "RSBS_COMBO_DIR_OFF is a real value,
 * and its world is a paired world with no crossings — not an unpaired world",
 * and one-game semantics make that binding: the combo is ONE game, so a rando
 * creation always authors both halves and OFF describes a Termina that exists
 * and simply hosts nothing. A gate that read OFF as "skip the paired creation"
 * would leave a file whose MM half was never authored, which arrival then has to
 * refuse (hydrate-or-refuse, ADR 0009 decision 2's #564 amendment) — the
 * MM-plays-vanilla failure class, reached through a setting rather than a bug.
 * Every OTHER consumer already behaved this way: OoT_RunPairedCreationEvent
 * gates on Combo_ForeignPairingActive() (direction-free) and both placement
 * passes gate on Combo_ComboDirectionArms(). This predicate was the only
 * disagreeing reader, so the flip is what makes the system consistent.
 *
 * THE QUESTION THE OLD BODY ACTUALLY ANSWERED now has its own name:
 * Combo_ForeignCrossingsRequested() below. Both are pre-Fill and CVar-derived;
 * they differ in what they decide, and the creation gate asks both.
 */
bool Combo_ForeignPairingRequested(void);

/**
 * "Does this creation author any CROSSINGS at all?" (future tense, #667).
 *
 * `direction != RSBS_COMBO_DIR_OFF`, read from the resolved settings — the body
 * Combo_ForeignPairingRequested() used to carry, under the name that describes
 * it. It is the pre-Fill twin of `Combo_ComboDirectionArms(GAME_OOT) ||
 * Combo_ComboDirectionArms(GAME_MM)`, which is the same fact read from the
 * FROZEN record after the freeze; Combo_ForeignCreationGateHolds() below is the
 * assertion that the two agree, and test_combo_settings.c locks the identity so
 * a new direction enumerator cannot arrive on one side only.
 *
 * FALSE is a real, chooseable world (ADR 0011 decision 2.3), never an error:
 * both halves are still generated, both are still armed, and both placement
 * passes simply place nothing.
 */
bool Combo_ForeignCrossingsRequested(void);

/**
 * THE PRE-FILL CREATION GATE'S POST-CHECK (#657, ADR 0009 decision 2).
 *
 * The gate ADR 0009 decision 2 designed is two acts, not one: ask the CVars
 * before Fill() what world is being asked for, then FREEZE that answer so
 * nothing downstream can reach a different one from a re-read CVar. This is the
 * second act's verification — call it with the answer
 * Combo_ForeignCrossingsRequested() gave BEFORE the freeze, and it reports
 * whether the record the freeze actually wrote reproduces it.
 *
 * @param crossingsRequestedPreFill what Combo_ForeignCrossingsRequested()
 *        answered before Combo_FreezeComboSettings ran.
 * @return true when the frozen record agrees. FALSE means the creation must
 *         FAIL: either nothing froze (so there is no record for the seam and
 *         every arrival to read, and the world would be identity-less), or a
 *         writer moved the direction between the ask and the freeze, so the
 *         world about to be filled is not the world the player asked for.
 *
 * Refusing rather than picking a side is the one-game rule: a combo-level
 * decision freezes at file creation, and a later disagreement about it is
 * corruption to refuse, never a divergence to honour.
 */
bool Combo_ForeignCreationGateHolds(bool crossingsRequestedPreFill);

/**
 * The PRESENT-tense predicate (ADR 0009 decision 2's amendment; the exact twin
 * of Combo_MMProfileFrozen): true once a creation event — or the O5
 * transitional writer — has frozen this world's combo rules. Literally
 * `gComboCtx.comboSettings.formatVersion != 0`, a src/common fact, never a
 * gSaveContext read (ADR 0008 rule 5).
 *
 * On a LEGACY (pre-carve) file, false means "no combo settings were ever
 * frozen" — correct, exempt from comparison, repaired at the first crossing.
 * On a file CREATED since this carve, false means IDENTITY NOT FROZEN, a state
 * no created combo file may be in; any surface that renders it as a benign
 * default will report corruption as normal (ADR 0011 decision 4.2).
 */
bool Combo_ComboSettingsFrozen(void);

/** The resolved direction: the frozen record's when frozen, else the shipped
 *  default. Increment 4 is where each placement pass NO-OPS on an unarmed
 *  direction — increment 1 only reads and reports it, because that increment is
 *  the only one that can change a generated world and it lands on top of a
 *  frozen, compared, rendered setting rather than under one. */
uint8_t Combo_ComboDirection(void);

/**
 * IS THE OCARINA ONE SHARED INSTRUMENT in this world (#668)?
 *
 * The frozen record's RSBS_COMBO_FLAG_SHARED_OCARINA bit when frozen, else the
 * live resolution's — the exact shape of Combo_ComboDirection, and for the same
 * reason: after creation the rules are world identity, so a session that
 * resolved differently is REFUSED at the arrival gate (by name, through
 * RSBS_COMBO_DIVERGE_SHARED_OCARINA) rather than honoured here.
 *
 * FALSE is the shipped default and means NOTHING CHANGES — no shared-resource
 * slot, no harvest, no apply. Its one consumer is
 * Combo_SharedResourceKindArmed (shared_resources.h), which is where both the
 * harvest and the apply halves of RSBS_SHARED_RES_OCARINA_TIER pass through, so
 * the gate cannot be applied to one direction and not the other.
 */
bool Combo_ComboSharedOcarina(void);

/** Does the resolved direction arm crossings ORIGINATING in @p originGame?
 *  Increment 4's gate; see Combo_ComboDirection. Under one bag (ADR 0010
 *  increment 3) the single-bag fill reads it per bag row: a row whose origin is
 *  not armed is HOME_ONLY — still placed, still proved, only never on the other
 *  game's checks. OFF is therefore one fill with one proof and no crossings. */
bool Combo_ComboDirectionArms(uint8_t originGame);

/**
 * The frozen record's pool size for @p originGame, CLAMPED to
 * RSBS_FOREIGN_PLACEMENT_CAP.
 *
 * NO GENERATION READS IT ANY MORE (ADR 0010 increment 3, D3). It bounded how
 * many duplicate copies an overlay pass pinned; under one bag the number of
 * crossings is an outcome of the fill, not a setting, and nothing caps it here.
 * The field stays in the record because the record is format and the field is
 * folded into comboSettingsHash (changing its meaning would move every world's
 * identity); the accessor stays for the record's own locks.
 *
 * An UNFROZEN record yields the shipped default (the cap), so a legacy world,
 * a pre-freeze world and a world generated before this carve all place exactly
 * what they place today. That fallback is not defensive tidiness — without it a
 * zero-extended record would resolve to pool size 0 and silently generate a
 * paired world with no crossings at all.
 *
 * @param originGame GAME_OOT (into MM checks) or GAME_MM (into OoT checks)
 * @return 1..RSBS_FOREIGN_PLACEMENT_CAP, or 0 for an origin with no pool.
 */
int Combo_ComboPoolSizeFor(uint8_t originGame);

/** Number of bytes Combo_ComboSettingsCanonical writes. Equal to
 *  sizeof(ComboSettingsRecord) on every supported host BY CONSTRUCTION rather
 *  than by luck — see the encoder. */
#define RSBS_COMBO_SETTINGS_CANONICAL_LEN 12u

/**
 * canonical(comboSettings) — the byte-pinned DIGEST INPUT (ADR 0011 decision
 * 1.4; ADR 0007 §2's codec discipline).
 *
 * The twelve bytes of the record encoded FIELD BY FIELD IN DECLARATION ORDER,
 * each uint16_t little-endian, WRITTEN A BYTE AT A TIME — never a struct memcpy
 * and never a cast of a packed struct, so a big-endian host emits identical
 * bytes. The member-offset static_asserts in context.h pin the STORAGE format;
 * this pins the DIGEST INPUT; on every supported host they are the same twelve
 * bytes in the same order, and the byte-at-a-time codec makes that true by
 * construction.
 *
 * This also keeps comboSettingsHash in family with the two terms it folds,
 * which are both encoded-first rather than struct-hashed (OoT's settings hash
 * runs over a GetOptionText string, MM's profile digest over
 * ProfileIdentityString). A raw-struct hash beside two string-first digests
 * would be the one term in the fingerprint whose bytes depend on the toolchain.
 *
 * @param rec  NULL is treated as an all-zero record.
 * @param out  receives exactly RSBS_COMBO_SETTINGS_CANONICAL_LEN bytes.
 */
void Combo_ComboSettingsCanonical(const ComboSettingsRecord* rec, uint8_t* out);

/**
 * The WHOLE PAIR's fingerprint (accepted answer O6):
 *
 *   Hash( canonical(rec) || ":" || LE32(sharedRandoSettingsHash) || ":" ||
 *         LE32(mmProfileDigest) )
 *
 * FNV-1a 32 over that byte string — the project's hash, the same one
 * SohUtils::Hash and Ship_Hash compute — with the two u32 terms encoded
 * little-endian a byte at a time for decision 1.4's reason.
 *
 * ZERO DISPLACES to a fixed nonzero constant, exactly as DigestFromIdentity
 * does: a real identity hashing to 0 would read as "not frozen" and become an
 * undetectable mismatch. One collision in 2^32 against a certain false negative.
 *
 * Pure — reads nothing, writes nothing. Both the creation stamp and every
 * arrival cross-check go through it.
 */
uint32_t Combo_ComputeComboSettingsHash(const ComboSettingsRecord* rec, uint32_t sharedRandoSettingsHash,
                                        uint32_t mmProfileDigest);

/**
 * Freeze @p rec into gComboCtx as this world's combo identity, then compute and
 * stamp comboSettingsHash over it and BOTH half-digests.
 *
 * CALL ORDER IS LOAD-BEARING (ADR 0011 decision 4.1): the creation event must
 * have stamped sharedRandoSettingsHash and mmProfileDigest FIRST, because the
 * fingerprint folds them and a hash computed earlier folds a term that has not
 * been decided yet. The creation event's order is therefore: resolve the combo
 * record -> stamp sharedRandoSettingsHash -> stamp mmProfileDigest -> call this.
 *
 * Forces formatVersion to RSBS_COMBO_SETTINGS_FORMAT_VERSION: a caller must not
 * be able to freeze a record that reads as absent.
 *
 * @return the stamped comboSettingsHash (always nonzero).
 */
uint32_t Combo_FreezeComboSettings(const ComboSettingsRecord* rec);

/**
 * Recompute comboSettingsHash from the RESIDENT record and the two resident
 * half-digests, and stamp it. Used by Combo_FreezeComboSettings and by the O5
 * transitional writer; exposed so a caller that re-stamps a half-digest can
 * restore the fingerprint's ordering invariant without re-freezing the record.
 * A no-op returning 0 when the record is not frozen.
 */
uint32_t Combo_StampComboSettingsHash(void);

/**
 * THE O5 TRANSITIONAL WRITER (ADR 0011 decision 4.4). A paired file whose
 * record reads absent (formatVersion == 0) predates this carve; it was
 * generated when there was only ever ONE rule set, so it freezes the SHIPPED
 * DEFAULTS at its FIRST CROSSING and compares normally thereafter. This is the
 * ResolvePairedProfile precedent applied verbatim — the one transitional writer
 * besides the creation event.
 *
 * Refusing legacy pairs instead would orphan every already-written paired
 * .redsave to detect a divergence that cannot have happened.
 *
 * Does nothing (returning 0) when there is no live pairing or the record is
 * already frozen — so a second crossing COMPARES rather than re-freezes, which
 * is the property that makes 4.4 a behaviour and not a promise.
 *
 * @return 1 if this call froze the defaults, 0 otherwise.
 */
int Combo_FreezeLegacyComboSettings(void);

// ---- Field-level divergence (ADR 0011 decision 1.1 justification 2) --------
//
// A refusal that can only say "something diverged" is the un-repairable case
// ADR 0009 accepted for want of an alternative. Twelve stored bytes buy the
// alternative, and they only buy it if something DIFFS them — so this is a
// scheduled surface with its own test lock, not an implied benefit of the carve.

#define RSBS_COMBO_DIVERGE_DIRECTION 0x0001u
#define RSBS_COMBO_DIVERGE_POOL_SIZE_OOT 0x0002u
#define RSBS_COMBO_DIVERGE_POOL_SIZE_MM 0x0004u
#define RSBS_COMBO_DIVERGE_ITEM_CLASS_OOT 0x0008u
#define RSBS_COMBO_DIVERGE_ITEM_CLASS_MM 0x0010u
#define RSBS_COMBO_DIVERGE_GOAL 0x0020u
#define RSBS_COMBO_DIVERGE_LOGIC_RUNG 0x0040u
/** Some UNALLOCATED bit of comboFlags differs — a state a formatVersion-1
 *  record may not be in, so it is named for the byte rather than for a rule.
 *  The value is the one `spare0` carried before byte 10 was spent (#668): the
 *  byte, its offset and its canonical position are unchanged, only its name and
 *  its meaning are, and a runtime bit that is not .redsave format is free to be
 *  renamed where a stored one would not be. */
#define RSBS_COMBO_DIVERGE_COMBO_FLAGS 0x0080u
#define RSBS_COMBO_DIVERGE_SPARE1 0x0100u
/** The frozen record carries a formatVersion this build does not understand, so
 *  its fields cannot be compared at all. Refuse; never guess. */
#define RSBS_COMBO_DIVERGE_UNREADABLE 0x0200u
/** The stored comboSettingsHash is not the fingerprint its own resident record
 *  and half-digests produce. Set only by the session-level diff, because it is
 *  a property of gComboCtx rather than of two records. */
#define RSBS_COMBO_DIVERGE_FINGERPRINT 0x0400u
/** The shared-ocarina rule differs (#668). A BIT of comboFlags gets its own
 *  divergence bit rather than hiding inside the byte's, because decision 1.1's
 *  whole justification for storing twelve bytes is that the refusal can name
 *  WHICH RULE diverged — and "comboFlags" is a field name, not a rule. */
#define RSBS_COMBO_DIVERGE_SHARED_OCARINA 0x0800u
/** The frozen triforce-hunt record (ADR 0010 answer O10, ComboTriforceRecord)
 *  does not agree with the combo record beside it or with the half its own game
 *  re-derives: a hunt goal with no valid record, a record under any other goal,
 *  or a half that is not what that game's frozen settings produce. DAMAGE, not a
 *  session change: the record is written once by the creation event from inputs
 *  the fingerprint and the MM profile digest already pin, so a disagreement here
 *  is the stored identity contradicting itself. See triforce_hunt.h. */
#define RSBS_COMBO_DIVERGE_TRIFORCE 0x1000u

/**
 * Which FIELDS differ between a frozen record and a live resolution, as
 * RSBS_COMBO_DIVERGE_* bits. 0 means "these are the same rules".
 *
 * Fields are compared at the LOWER of the two format versions: a field
 * introduced at version N is authoritative only in a record at version >= N, so
 * comparing it against an older record would refuse a world for carrying rules
 * that did not exist when it was made. A frozen record NEWER than this build
 * yields RSBS_COMBO_DIVERGE_UNREADABLE alone.
 *
 * An ABSENT frozen record (formatVersion == 0) yields 0 — it is exempt from
 * comparison until the O5 writer freezes it, which is decision 4.2's meaning of
 * zero, not a silent pass.
 */
uint32_t Combo_ComboSettingsDivergenceBetween(const ComboSettingsRecord* frozen, const ComboSettingsRecord* live);

/**
 * The full diff for ONE stored identity: @p frozen against the live resolution,
 * PLUS the fingerprint cross-check (RSBS_COMBO_DIVERGE_FINGERPRINT) recomputed
 * from that identity's OWN three terms.
 *
 * Takes the terms explicitly rather than reading gComboCtx so the .redsave LOAD
 * path can run it over the record it just read, BEFORE committing those bytes
 * over the resident context — a check that had to read the destination would be
 * checking the wrong world.
 *
 * The fingerprint cross-check is not redundant with the field diff: the record,
 * the two half-digests and the stored hash all ride the SAME Tier-1 write, so a
 * disagreement between them is not a settings change — it is a record and a hash
 * that describe different worlds.
 */
uint32_t Combo_ComboSettingsDivergenceFor(const ComboSettingsRecord* frozen, uint32_t storedHash,
                                          uint32_t sharedRandoSettingsHash, uint32_t mmProfileDigest);

/**
 * The session-level diff the arrival refusal calls: Combo_ComboSettingsDivergenceFor
 * over the RESIDENT gComboCtx identity. 0 when nothing is frozen (exempt).
 */
uint32_t Combo_ComboSettingsDivergence(void);

/** The field name for ONE RSBS_COMBO_DIVERGE_* bit ("direction",
 *  "poolSizeOoT", ...). Never NULL; an unknown bit reports "(unknown)". */
const char* Combo_ComboSettingsDivergenceFieldName(uint32_t bit);

/**
 * Render EVERY set bit of @p bits as a comma-separated field list into @p out —
 * what the refusal notification and log line say instead of "something
 * diverged". Always NUL-terminates when len > 0; writes "(none)" for 0.
 * @return the number of named fields.
 */
int Combo_ComboSettingsDivergenceDescribe(uint32_t bits, char* out, size_t len);

/**
 * Does a divergence bitset describe DAMAGE to the stored identity — an
 * unreadable record, or a fingerprint its own record and half-digests do not
 * produce — rather than a SESSION that diverged from a healthy file (field
 * bits only)? Both are refused; they must be HANDLED differently. A damaged
 * file is evidence to quarantine. A healthy file met by a divergent session is
 * precisely what must be left in place: RefuseSlotIdentity's contract (save.h)
 * is that an identity refusal "quarantines NOTHING — the on-disk .redsave is
 * healthy and is precisely what must be protected".
 *
 * Load-bearing from increment 2 on: with the tier-4 keys authorable, a
 * field-only divergence at LOAD is the ordinary case — the player changed a
 * rule at the title screen and then loaded an older paired file — not the
 * damage case it could only be while the resolver was constant. A load path
 * that quarantines on it renames a healthy save away.
 */
bool Combo_ComboSettingsDivergenceIsDamage(uint32_t bits);

/**
 * The pairing header for the combo pane (ADR 0011 increment 2) and for any
 * renderer — the twin of Combo_MMProfileSummary, reading gComboCtx and nothing
 * else (ADR 0008 rule 5).
 *
 * `frozen == false` with `paired == true` marks a LEGACY pre-carve pair that has
 * not crossed yet. ADR 0004 §6 state 4 requires the value shown post-creation to
 * come FROM THE SAVE rather than from the CVar — after creation the two may
 * legitimately differ, and the save is the one the world was built from — which
 * is exactly why the record exists and a digest could not have served.
 */
typedef struct {
    bool paired;
    bool frozen;
    ComboSettingsRecord record;
    uint32_t comboSettingsHash;
} ComboSettingsSummary;

/** Fill @p out with the combo-settings header. NULL @p out is ignored. */
void Combo_ComboSettingsSummary(ComboSettingsSummary* out);

/**
 * Record "MM check `mmCheckId` hosts `item`". Rejects (returning -1) an unset
 * item tag, an mmCheckId of 0 (MM's RC_UNKNOWN), a full table, or a duplicate
 * mmCheckId — one check hosts at most one foreign item.
 * @return the slot index used (>= 0), or -1.
 */
int Combo_SetForeignPlacement(uint16_t mmCheckId, SharedItem item);

/**
 * The foreign item hosted by MM check `mmCheckId`, or NULL if that check hosts
 * none. The returned pointer aliases gComboCtx or the crossing store (read-only
 * use).
 *
 * TWO SOURCES, PINNED FIRST (ADR 0010 O7). The pinned forward table answers
 * first; a host it does not list falls back to the crossing store
 * (crossing_store.h, Combo_Crossings_Lookup), which is where the single-bag
 * coordinator's placements persist. Both give paths (MM's CheckQueue foreign
 * branch and OoT's RC-queue drain) read through this accessor and its OoT twin,
 * so they honour coordinator crossings without a change of their own, and the
 * pinned lookup keeps working until lane K11 retires the pools. The two
 * sources never overlap in a real world (K11 replaces the pinned passes with
 * the coordinator); if they did, the pinned row would win.
 *
 * NOT every reader wants both. A reader that serializes the PINNED table for a
 * loader that rebuilds the pinned table (MM's spoiler "foreign" section, which
 * Apply.cpp's ReconstructForeignPlacements reads back by pool name and caps at
 * RSBS_FOREIGN_PLACEMENT_CAP) must use Combo_GetPinnedForeignPlacementForCheck:
 * the crossing store has its own spoiler section (combo.crossingStore).
 */
const SharedItem* Combo_GetForeignPlacementForCheck(uint16_t mmCheckId);

/**
 * The PINNED forward table's row for `mmCheckId`, or NULL: no crossing-store
 * fallback. For writers of the pinned table's own serializations (see above).
 */
const SharedItem* Combo_GetPinnedForeignPlacementForCheck(uint16_t mmCheckId);

/** Number of occupied placement slots. */
int Combo_CountForeignPlacements(void);

/**
 * Wipe the MM-hosted placement table. Called by MM's placement pass before
 * re-placing (a re-generated MM world must not inherit a previous world's
 * placements). Does NOT touch the OoT-hosted table — the two directions are
 * generated independently, and session-wide retirement is handled by
 * Context_InvalidateSessionState / ComboContext_Init, which memset the whole
 * struct and therefore cover both (ADR 0009).
 */
void Combo_ClearForeignPlacements(void);

// ============================================================================
// Reverse-direction placement table (gComboCtx.foreignPlacementsOoT, #493)
// ============================================================================
//
// The mirror of the four accessors above, keyed by an OoT RandomizerCheck
// hosting an MM item. A SEPARATE KEY SPACE, not a second half of the same
// array: an OoT RC and an MM RC are unrelated enumerations that collide freely
// as raw u16s, so nothing may look one table up with the other's accessor. The
// direction is the accessor; that is what stands in for the host-discriminator
// byte ComboForeignPlacement cannot grow (ADR 0009 decision 3).

/**
 * Record "OoT check `ootCheckId` hosts `item`" (item.originGame == GAME_MM for
 * the reverse direction). Same rejections as the forward accessor: an unset
 * item tag, a check id of 0 (RC_UNKNOWN), a full table, or a duplicate check id.
 * @return the slot index used (>= 0), or -1.
 */
int Combo_SetForeignPlacementOoT(uint16_t ootCheckId, SharedItem item);

/**
 * The foreign item hosted by OoT check `ootCheckId`, or NULL if that check
 * hosts none. The returned pointer aliases gComboCtx or the crossing store
 * (read-only use). Pinned reverse table first, then the crossing store: the
 * same two-source rule as Combo_GetForeignPlacementForCheck.
 */
const SharedItem* Combo_GetForeignPlacementForOoTCheck(uint16_t ootCheckId);

// ============================================================================
// Names for ANY item and check (ADR 0010 O7)
// ============================================================================
//
// Under the single bag a crossing can be any progression item on any check. The
// one spoiler, the trackers and the pickup text all print crossings by name,
// and src/common cannot learn a name without a game header (ADR 0002). So each
// game's engine TU, where its enums ARE in scope, registers a describer, the
// way the engines themselves register: a file-scope registrar that stores a
// pointer and calls nothing. Combo_GetForeignItemName (above) is this surface
// under its older name, kept for its callers.

typedef struct {
    /** Stable display name of `id` in this game's item id-space, or NULL. */
    const char* (*itemName)(uint16_t id);
    /** Stable display name of `check` in this game's check id-space, or NULL. */
    const char* (*checkName)(uint16_t check);
    /** The article a pickup sentence puts before itemName(id) ("the ", "a ",
     *  "an " or ""; it carries its own trailing space), or NULL. Optional. */
    const char* (*itemArticle)(uint16_t id);
    /** The arrival-toast icon key for `id` IN THIS GAME's own notification
     *  overlay, or NULL for a text-only toast. Optional. */
    const char* (*itemIcon)(uint16_t id);
} ComboGameDescriber;

/** Register (or, with NULL, un-register) `game`'s describer. A non-game is ignored. */
void Combo_RegisterGameDescriber(uint8_t game, const ComboGameDescriber* describer);

/** The item's name through its origin game's describer. NULL for an
 *  unregistered game or an id the describer does not know. */
const char* Combo_DescribeItemName(SharedItem item);

/** The name of `check` in `hostGame`'s check id-space through that game's
 *  describer, or NULL. */
const char* Combo_DescribeCheckName(uint8_t hostGame, uint16_t check);

/** Number of occupied OoT-hosted placement slots. */
int Combo_CountForeignPlacementsOoT(void);

/**
 * Wipe the OoT-hosted placement table. Called by OoT's placement pass before
 * re-placing. Does NOT touch the MM-hosted table — see the note on
 * Combo_ClearForeignPlacements.
 */
void Combo_ClearForeignPlacementsOoT(void);

/**
 * OoT-side redemption give (defined in ForeignItemsSingleExe.cpp): resolve the
 * RG_* id to its GetItemEntry — progressive items resolve against the LIVE
 * save, the same path SoH's own in-game gives take — and hand it to the
 * matching give routine (OoT_Item_Give for vanilla-equivalent entries,
 * Randomizer_Item_Give for MOD_RANDOMIZER entries).
 * @return 1 if the item was given, 0 if the give path was unavailable (logged;
 *         the redemption bit is set by the caller's consumer either way).
 */
int OoT_ForeignItem_Give(uint16_t rgId);

/**
 * THE REVERSE DIRECTION'S GIVE-PATH CORE (#493) — the exact twin of
 * MM_Rando_Foreign_RecordPickup, and the OoT counterpart of
 * Rando::Foreign::RecordForeignPickup.
 *
 * "OoT check @p rc was just collected; if it hosts an MM-origin foreign item,
 * author the durable crossing." Called by the RC-queue drain
 * (soh/Enhancements/randomizer/hook_handlers.cpp) — which is where OoT's rando
 * delivers CHEST checks, the only host class the OoT engine's hostAcceptsForeign
 * admits for a crossing,
 * because the vanilla chest give is suppressed for a shuffled location
 * (VB_GIVE_ITEM_FROM_CHEST) and the item is granted from the queue instead.
 *
 * ONE PRODUCTION FUNCTION, TWO CALLERS: the hook and the ForeignItemGiveReverse
 * lock. That is the whole reason it is extracted — a lock that reached the
 * placement table directly would assert the accessors and never the give.
 *
 * Records nothing when the check hosts no foreign item, and REFUSES to record
 * when there is no live cross-game pairing (the #610 rule, which the forward
 * direction has carried since that fix and this one did not): a record authored
 * with no paired world is redeemed by whichever world arrives next. The check
 * then degrades to the junk-class OoT item it physically holds.
 *
 * Presentation is deliberately NOT here — the caller emits the pickup toast and
 * marks the tracker, so a display-free tier can drive the record without
 * pretending to assert pixels.
 *
 * @return 1 if a durable crossing was authored, 0 otherwise.
 */
int OoT_Rando_Foreign_RecordPickup(uint16_t rc);

// ============================================================================
// THE VALUES-PUBLISHING SURFACE (ADR 0011 O8; solver-inventory P1/P5;
// ADR 0010 increment 2)
// ============================================================================
//
// WHAT WAS MISSING, EXACTLY. Reverse-pool criterion 3
// (games/mm/2s2h/Rando/ForeignItemsSingleExe.cpp) excludes the enemy/boss
// souls, the ocarina buttons, RI_ABILITY_SWIM and the clock items for ONE
// stated reason: "OoT's placement pass runs at OoT generation time — possibly
// before the paired MM world exists at all — so it CANNOT read MM's option
// profile". MM published a DIGEST of that profile and nothing else
// (MM_Rando_ComputeProfileStamp, Foreign.cpp), and a digest answers "did the
// rules change", never "what are the rules". ADR 0011 O8 chose (b): the
// narrowing is gated on the freeze preceding OoT's Fill() (delivered by this
// increment) PLUS a surface that publishes VALUES. This is that surface.
//
// WHY CAPABILITY BITS AND NOT OPTION VALUES. ADR 0002's boundary rule: an
// option id belongs to the game whose enum declares it, and src/common must
// never acquire an MM header to see one — the same rule the foreign pools and
// the options view-model follow. So the frozen profile is published as a small,
// GAME-NEUTRAL bitset naming the give CLASSES the world arms. The bits below
// are exactly the families criterion 3 names, one bit each, which keeps the
// surface the size of its only question. A publisher that needs a new family
// adds a bit; nothing here ever learns an id.
//
// SESSION SCOPE, ONE COMPUTATION, TWO PUBLISH POINTS. The caps are RAM-only —
// no .redsave field and no carve from reserved[] — because they are
// re-derivable from the frozen profile wherever that profile is resident.
// Published at the CREATION freeze from the CVar-resolved values, and
// re-published from the save's frozen RANDO_SAVE_OPTIONS when a hydrated MM
// half comes back in a later process. Both go through the one MM-side
// computation (MM_Rando_PublishProfileGiveCaps, combo_mm_options_view.h), for
// the same reason ResolveProfileValues is one computation behind both the
// creation stamp and the arrival compare.
//
// PUBLISHED-NESS IS A DISTINCT STATE from "published, all bits clear". An
// unpublished surface means "no MM profile has been frozen in this session",
// and a membership rule that read that as "the profile arms nothing" would
// narrow every pool to nothing on every process that never generated.
// Combo_ForeignGiveCapsPublished is that discriminator, and it is why this is
// not simply a uint32_t whose 0 would mean both things.

/** Enemy and boss soul shuffle (MM: RO_SHUFFLE_ENEMY_SOULS /
 *  RO_SHUFFLE_BOSS_SOULS). Unarmed, a soul give is a bare rando-inf flag with
 *  no meaning in the receiving world. */
#define RSBS_GIVECAP_SOULS 0x0001u
/** Ocarina-button shuffle (MM: RO_SHUFFLE_OCARINA_BUTTONS). */
#define RSBS_GIVECAP_OCARINA_BUTTONS 0x0002u
/** Swim-ability shuffle (MM: RO_SHUFFLE_SWIM). */
#define RSBS_GIVECAP_SWIM 0x0004u
/** Clock shuffle in the RANDOM clock mode (MM: RO_CLOCK_SHUFFLE on AND
 *  RO_CLOCK_SHUFFLE_PROGRESSIVE == RANDOM) — the concrete RI_TIME_* family.
 *  The progressive modes do not publish it: their logic reads ownership as a
 *  count of half-days owned in order, which a concrete out-of-order crossing
 *  would contradict (#681 review). */
#define RSBS_GIVECAP_CLOCKS 0x0008u
/** Every bit ALLOCATED at v1. Append only; an unallocated bit in a published
 *  word comes from a newer build and is masked off rather than reinterpreted. */
#define RSBS_GIVECAP_ALL_V1                                                                                        \
    (RSBS_GIVECAP_SOULS | RSBS_GIVECAP_OCARINA_BUTTONS | RSBS_GIVECAP_SWIM | RSBS_GIVECAP_CLOCKS)

/**
 * Publish @p caps as the give capabilities of @p originGame's FROZEN option
 * profile. Idempotent; the last publish of a session wins, and during a
 * creation the freeze is the only writer.
 *
 * @param originGame GAME_OOT or GAME_MM — the game whose profile these are.
 * @param caps       an OR of RSBS_GIVECAP_*; unallocated bits are masked off.
 */
void Combo_PublishForeignGiveCaps(uint8_t originGame, uint32_t caps);

/** The published caps for @p originGame, or 0 when nothing is published. Always
 *  pair a nonzero test with Combo_ForeignGiveCapsPublished — see the block
 *  comment for why "unpublished" and "arms nothing" are different states. */
uint32_t Combo_ForeignGiveCaps(uint8_t originGame);

/** True once a profile freeze published caps for @p originGame this session. */
bool Combo_ForeignGiveCapsPublished(uint8_t originGame);

/** Does @p originGame's frozen profile arm EVERY bit in @p caps? False when
 *  nothing is published: an unpublished profile promises nothing, which is the
 *  conservative answer for a pool-membership rule. */
bool Combo_ForeignGiveCapsArm(uint8_t originGame, uint32_t caps);

/** Retire the session's published caps (a new creation, a session
 *  invalidation). */
void Combo_ClearForeignGiveCaps(void);


/**
 * Join the paired MM half onto OoT's spoiler document, producing the ONE
 * artifact per pair #564 V23 requires and #660 tracks. Defined MM-side
 * (Rando/Foreign.cpp), where MM's spoiler generator and schema live.
 *
 * Reads the JSON at @p ootSpoilerPath, adds a single top-level "combo" key
 * carrying the identity tuple, the frozen combo record, MM's whole spoiler and
 * BOTH crossing directions (plus the #583 shortfall), and writes it back. An
 * unknown top-level key is ignored by every existing OoT spoiler reader, which
 * is why augmenting was chosen over a new schema.
 *
 * A spoiler is a REPORT of the world, not part of it: every failure logs and
 * returns nonzero, and the creation succeeds regardless.
 *
 * @param ootSpoilerPath absolute path to the OoT spoiler just written.
 * @return 0 on success; negative on a missing path, an unreadable or
 *         unwritable file, or a malformed document.
 */
int MM_Rando_AugmentSpoilerWithPairedHalf(const char* ootSpoilerPath);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_FOREIGN_ITEMS_H
