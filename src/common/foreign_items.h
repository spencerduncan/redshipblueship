/**
 * @file foreign_items.h
 * @brief Cross-game (foreign) item placements and the pinned foreign-item pool
 *        (Phase 3.0 Lane C1, #392; ADR 0002).
 *
 * The MVP contract ships ONE direction, ONE item class: a pinned set of OoT
 * progression items placeable into MM checks. Two data surfaces live behind
 * this header:
 *
 *  1. The PLACEMENT TABLE — `gComboCtx.foreignPlacements[]` (context.h), the
 *     serialized record of "MM check X hosts foreign item Y". Written once by
 *     MM's generation pass (Rando::Foreign::PlaceForeignItems at OnFileCreate)
 *     when a paired world generates; read by MM's give path and both spoiler
 *     surfaces. The MM save's own check table keeps a legal MM item (RI_JUNK)
 *     at hosting checks — a raw RG_* never enters an MM table (ADR 0002).
 *
 *  2. The PINNED POOL — the fixed foreign-item class. Its table is DEFINED on
 *     the OoT side (games/oot/soh/Enhancements/randomizer/
 *     ForeignItemsSingleExe.cpp), the only place the real RG_* enumerators are
 *     in scope, and served here as origin-tagged SharedItems plus stable
 *     display names. MM and the tests consume it through these C entry points
 *     and never see an OoT header.
 *
 * The give-time flow (who calls what):
 *   MM CheckQueue foreign branch -> Combo_GetForeignPlacementForCheck ->
 *   Combo_RecordSharedItem (shared_items.h; durable immediately) -> presented
 *   with Combo_GetForeignItemName. On the next arrival in OoT, A1's consumer
 *   (Combo_RedeemSharedItemsForGame) awards it via OoT_ForeignItem_Give and
 *   marks it RSBS_SHARED_ITEM_REDEEMED — single-use per crossing.
 */

#ifndef RSBS_COMMON_FOREIGN_ITEMS_H
#define RSBS_COMMON_FOREIGN_ITEMS_H

#include "context.h" // SharedItem, ComboForeignPlacement, gComboCtx, RSBS_FOREIGN_PLACEMENT_CAP

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One pinned foreign-item class member: the origin-tagged item plus its
 * stable, human-readable name (used by the MM textbox presentation and by
 * both spoiler surfaces — the name is deliberately game-neutral text so the
 * spoiler stays meaningful without OoT's enum in scope).
 */
typedef struct {
    SharedItem item;     // originGame == GAME_OOT, flags == 0, id == the RG_* value
    const char* name;    // e.g. "Megaton Hammer"
    const char* article; // "the ", "a ", "an " or "" — see below
    // The host game's texture-map key for this item's arrival-toast icon, or
    // NULL for a text-only toast. Like `name`, it is filled in by the pool's
    // defining TU (where the item's icon is known) and served back through this
    // header, so src/common never has to translate a foreign id into a texture.
    // The OoT pool carries the `ITEM_*` key its Notification overlay resolves via
    // GetTextureByName (the same string GetTextureForItemId returns); see
    // Combo_GetForeignItemIconName.
    const char* iconName;
} ComboForeignItemDef;

// WHY THE ARTICLE IS PART OF THE DESCRIPTOR (#510). A cross-game item is
// presented as an ORDINARY pickup of whichever game the player found it in —
// "You found the Lens of Truth!", never "it belongs to the other game". Both games
// build that sentence by prepending a per-item article (MM:
// Rando::StaticData::Items[].article; OoT: Item::GetArticle()), but the HOST
// game cannot look up the FOREIGN game's item table — that is the whole ADR 0002
// boundary. So the article has to travel with the pooled descriptor, exactly as
// the display name already does. Without it the presentation has to hardcode
// "the ", which is wrong for a third of the pool ("a Bottle of Milk", "an Empty
// Bottle") and instantly reads as machine-generated.
//
// Includes its own trailing space when non-empty, so callers concatenate
// article + name with no separator logic.

// ============================================================================
// The pinned pools, indexed by ORIGIN game (ADR 0009 decision 3)
// ============================================================================
//
// There is one pool PER ORIGIN GAME, not one merged pool, and each pool's table
// is defined in the single TU where its enum is in scope — OoT's in
// soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp, MM's in
// 2s2h/Rando/ForeignItemsSingleExe.cpp. Neither game ever sees the other's
// header, which is the ADR 0002 / #356 constraint.
//
// Those TUs cannot be *called* from here without linking each game against the
// other, so they REGISTER instead: each pool TU hands its static table to
// Combo_RegisterForeignItemPool from a file-scope initializer, and the lookups
// below dispatch through the registry. That also means a build with only one
// pool linked resolves cleanly — the missing origin simply has no entries,
// rather than failing to link.
//
// (originGame, name) IS THE KEY. Bare `name` is not: "Lens of Truth" is
// literally present in BOTH pools (OoT's RG_LENS_OF_TRUTH row and MM's RI_LENS
// row — it took the job over from "Bomb Bag", whose two halves both left when
// shared ammo made the bomb-bag capacity a shared resource), so
// a name-only inverse silently resolves to whichever pool it scans first and
// writes a WRONG ORIGIN TAG into the placement table — the #356 aliasing class,
// arriving through the one path that rebuilds state from untrusted text on
// disk. See ADR 0009 decision 3.

/** Highest origin id the pool registry indexes, exclusive (GAME_NONE..GAME_MM). */
#define RSBS_FOREIGN_POOL_ORIGIN_COUNT 3u

/**
 * Publish `pool` as the pinned foreign-item pool for `originGame`. Called once
 * per pool from its defining TU's file-scope initializer, before main(). A
 * second registration for the same origin replaces the first and is logged —
 * two tables claiming one id-space is the ambiguity this whole surface exists
 * to prevent, so it must not pass silently.
 *
 * GAME_NONE is rejected: an untagged pool has no id-space and nothing could
 * safely be resolved out of it.
 *
 * Passing (NULL, 0) un-registers that origin, so a test can install a synthetic
 * pool for an origin whose real pool TU is not linked yet and then restore the
 * registry instead of leaving process-global state behind.
 */
void Combo_RegisterForeignItemPool(uint8_t originGame, const ComboForeignItemDef* pool, int count);

/**
 * The pinned foreign-item pool for one origin game.
 * @param originGame GAME_OOT or GAME_MM
 * @param outPool receives a pointer to that origin's static pool table (never
 *                NULL on a nonzero return; untouched on a zero return)
 * @return the number of pool entries, or 0 if that origin has no pool linked.
 */
int Combo_GetForeignItemPoolFor(uint8_t originGame, const ComboForeignItemDef** outPool);

/**
 * The OoT pool. Retained as the original single-pool entry point so the call
 * sites that predate the origin dimension keep compiling and keep their exact
 * previous behavior; new code should say which origin it means.
 */
int Combo_GetForeignItemPool(const ComboForeignItemDef** outPool);

/**
 * Display/spoiler name for a foreign item, or NULL if (originGame, id) is not
 * in that origin's pinned pool. Already origin-aware by construction — the
 * SharedItem carries its own tag — so this signature is unchanged; only the
 * pool it consults now depends on item.originGame. Flags are ignored on
 * purpose: a redeemed entry keeps its name.
 */
const char* Combo_GetForeignItemName(SharedItem item);

/**
 * The article that belongs in front of Combo_GetForeignItemName's result, or
 * NULL if (originGame, id) is not in that origin's pinned pool. Carries its own
 * trailing space when non-empty, so `article + name` needs no separator.
 *
 * Split from the name rather than baked into it because the two are consumed
 * separately: a spoiler line wants the bare name, a pickup textbox wants the
 * full "the Lens of Truth" phrase. See the note on ComboForeignItemDef.article.
 */
const char* Combo_GetForeignItemArticle(SharedItem item);

/**
 * The arrival-toast icon for a foreign item — the host game's texture-map key,
 * or NULL if (originGame, id) is not in that origin's pinned pool OR the pool
 * entry carries no icon. NULL is not a defect: Notification::Emit renders a
 * text-only toast for a null icon, the same idiom the native pickup paths use.
 *
 * The returned string is the pool's own static storage (a string literal in the
 * defining TU), so it outlives any toast that stores it as a bare pointer and
 * dereferences it at draw time (see notification_bridge.h). Origin-aware by the
 * same construction as Combo_GetForeignItemName: the tag on the SharedItem
 * selects which pool is walked, so an OoT-origin item resolves an OoT `ITEM_*`
 * key and nothing else. This is the accessor GameExports' OoT_AwardSharedItem
 * needs so the cross-game arrival toast can show the item's icon (#494).
 */
const char* Combo_GetForeignItemIconName(SharedItem item);

/**
 * The inverse of Combo_GetForeignItemName, keyed on (originGame, name). Used by
 * both spoiler-LOAD paths to rebuild the placement tables from a spoiler's
 * "foreign" section without either game's header in scope — the SharedItem is
 * copied straight out of the pinned pool, so a raw RG_* (or RI_*) is never
 * fabricated on the far side (ADR 0002).
 *
 * The origin argument is REQUIRED and is not a convenience: see the key note
 * above. A spoiler entry that does not carry an origin must be refused by its
 * caller, never guessed.
 *
 * @param originGame GAME_OOT or GAME_MM (GAME_NONE is rejected)
 * @param name       the display name to look up (NULL is rejected)
 * @param outItem    receives the tagged SharedItem on a match (may be NULL)
 * @return true if that origin's pool has an entry with this name.
 */
bool Combo_GetForeignItemByNameFor(uint8_t originGame, const char* name, SharedItem* outItem);

/**
 * The OoT-pool name inverse. Retained so the call sites that predate the origin
 * dimension keep compiling with their exact previous behavior (they all pass
 * names that came from the OoT pool). New code should use the _For form and
 * carry the origin explicitly.
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
 *  0002 §4's COMBO_CONTEXT_VERSION rule, scoped to one block) — i.e. when
 *  spare0/spare1 are spent and a reader must tell "field absent" from "field
 *  zero". */
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
 * INCREMENT 1 RESOLVES TO THE SHIPPED DEFAULTS, because the tier-4 authoring
 * keys (`gCombo.Rando.Direction`, `.PoolSize.*`, `.ItemClass.*`) are increment
 * 2's work and do not exist yet. When they land, THIS function grows the CVar
 * read and every call site follows with no change — which is the whole reason
 * the resolver is a named function rather than an inlined defaults copy.
 */
void Combo_ResolveComboSettings(ComboSettingsRecord* out);

/**
 * The PRE-CONDITION predicate ADR 0009 decision 2 designed and nobody built:
 * "a paired world is being ASKED for" (future tense), answerable BEFORE
 * generation by construction.
 *
 * Derived from the resolved settings and NEVER from gComboCtx's stamp, so it is
 * immune to the ordering hazard that made hoisting the stamp above Fill() look
 * necessary. Do not "simplify" it into Combo_ForeignPairingActive(): that one
 * is the post-condition and answers a different question in a different tense.
 */
bool Combo_ForeignPairingRequested(void);

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

/** Does the resolved direction arm crossings ORIGINATING in @p originGame?
 *  (GAME_OOT -> the forward pass, GAME_MM -> the reverse pass.) Increment 4's
 *  gate; see Combo_ComboDirection. */
bool Combo_ComboDirectionArms(uint8_t originGame);

/**
 * How many placements this direction may make: the frozen record's pool size
 * for @p originGame, CLAMPED to RSBS_FOREIGN_PLACEMENT_CAP.
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
#define RSBS_COMBO_DIVERGE_SPARE0 0x0080u
#define RSBS_COMBO_DIVERGE_SPARE1 0x0100u
/** The frozen record carries a formatVersion this build does not understand, so
 *  its fields cannot be compared at all. Refuse; never guess. */
#define RSBS_COMBO_DIVERGE_UNREADABLE 0x0200u
/** The stored comboSettingsHash is not the fingerprint its own resident record
 *  and half-digests produce. Set only by the session-level diff, because it is
 *  a property of gComboCtx rather than of two records. */
#define RSBS_COMBO_DIVERGE_FINGERPRINT 0x0400u

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
 * none. The returned pointer aliases gComboCtx (read-only use).
 */
const SharedItem* Combo_GetForeignPlacementForCheck(uint16_t mmCheckId);

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
 * hosts none. The returned pointer aliases gComboCtx (read-only use).
 */
const SharedItem* Combo_GetForeignPlacementForOoTCheck(uint16_t ootCheckId);

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

// ============================================================================
// Reverse-direction PRODUCER: OoT's generation-time placement pass (#510)
// ============================================================================
//
// Both are defined in games/oot/soh/Enhancements/randomizer/
// ForeignItemsSingleExe.cpp and exist ONLY under RSBS_SINGLE_EXECUTABLE — the
// whole cross-game item class is single-exe-only. Playthrough_Init is an
// ordinary (non-single-exe) function, so its call site MUST be wrapped in
// `#ifdef RSBS_SINGLE_EXECUTABLE` or a plain OoT build fails to link.

/**
 * Place MM-origin items (kForeignPoolMMV1) into eligible OoT checks, recording
 * them in gComboCtx.foreignPlacementsOoT. Called once per generation from
 * Playthrough_Init, AFTER the gComboCtx pairing stamp — the placement is derived
 * from that identity, so it must be live first.
 *
 * Selection is deterministic (a local xorshift32 seeded from seed + settings
 * digest, never from the fill's own RNG stream) and draws BOTH the pool entry
 * and the host without replacement, because the MM pool is far larger than
 * RSBS_FOREIGN_PLACEMENT_CAP.
 *
 * @return the number of placements made (>= 0; 0 when no pairing is active,
 *         which is the normal solo-rando case), or NEGATIVE when the pairing IS
 *         active but could not be honoured at all: -1 no MM pool registered,
 *         -2 no eligible host in the finished fill. Callers propagate the
 *         negative through Playthrough_Init's existing return-code convention.
 *         It never throws — the generation chain crosses an extern "C" boundary
 *         with no try/catch, where an exception is a std::terminate.
 */
int OoT_PlaceForeignItems(void);

/**
 * May OoT check `rc` host a foreign item? The real selection predicate
 * OoT_PlaceForeignItems' candidate loop uses, exposed for the CI lock so the
 * test drives selection rather than a paraphrase of it.
 *
 * True only for a genuine treasure chest (Location::GetActorID() == ACTOR_EN_BOX
 * — OoT has no RCTYPE_CHEST; chests are RCTYPE_STANDARD) that is not commerce
 * and whose FILL placed a junk-category item there. The junk requirement is the
 * degrade invariant: if the placement table is ever absent, the check quietly
 * yields the ordinary item it really holds.
 *
 * @return 1 if eligible, 0 otherwise.
 */
int OoT_Foreign_IsEligibleHost(uint16_t rc);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_FOREIGN_ITEMS_H
