#pragma once
/**
 * @file StoneMaskOoT.h
 * @brief Majora's Mask's Stone Mask as an Ocarina of Time item (#979, epic #975
 *        P4's second mask; OoTMM stoneMaskOot, sharedMaskStone). The
 *        definitions and the reasoning are in StoneMaskOoT.cpp; this header is
 *        what the pool, the logic and the locks call. C-compatible.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Actor;

/** Is the frozen Stone Mask (OoT) rule on (RSBS_XITEM_STONE_MASK_OOT)? False
 *  while nothing is frozen. The pool adds the item and the logic credits it only
 *  then (OoTMM's has_mask_stone: setting(stoneMaskOot)). */
bool OoT_StoneMaskOoTOn(void);

/**
 * Does actor `actorId` (with `params`) lose sight of a Link wearing the Stone
 * Mask? 1 for exactly the actors OoTMM's shouldActorIgnorePlayer lists
 * (generator src/oot/actors.c): its enemies, the Gerudo and the castle guards;
 * the Armos only when it is the enemy (params != 0), not the statue. Pure.
 */
int OoT_StoneMask_IgnoresWearer(int16_t actorId, int16_t params);

/**
 * Before `actor`'s update (ShouldActorUpdate): first put back the fields of a
 * listed actor whose update another handler kept from running earlier in this
 * pass (`frame`, the GameState frame; one from an earlier frame is forgotten,
 * never touched). Then, when `updates` and the worn Stone Mask hides Link from
 * `actor` (OoT_StoneMask_IgnoresWearer), replace its player-relative fields with
 * OoTMM's "far away, in a random direction" until OoT_StoneMask_AfterActorUpdate.
 * 1 when it replaced them.
 */
int OoT_StoneMask_BeforeActorUpdate(struct Actor* actor, uint32_t frame, int updates);

/** After `actor`'s update (OnActorUpdate): put back the fields
 *  OoT_StoneMask_BeforeActorUpdate replaced, when `actor` is the one it hid
 *  Link from. */
void OoT_StoneMask_AfterActorUpdate(struct Actor* actor);

/** Forget a held actor without touching it (a play state ending, a file load). */
void OoT_StoneMask_Forget(void);

/** Register the Stone Mask in the child trade cycle after the Blast Mask (OoTMM's
 *  order; this registers the Blast Mask's runtime, and OoT's ownership source,
 *  first) and give its ItemID its age row. Idempotent: run at start-up and at
 *  every file load; the locks call it after rows that clear them. */
void OoT_StoneMask_RegisterRuntime(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
