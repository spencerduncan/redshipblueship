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
    const char* name;    // e.g. "Progressive Hookshot"
    const char* article; // "the ", "a ", "an " or "" — see below
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
 */
bool Combo_ForeignPairingActive(void);

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
