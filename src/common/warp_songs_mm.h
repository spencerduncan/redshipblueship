#pragma once
/**
 * @file warp_songs_mm.h
 * @brief Ocarina of Time's six warp songs as Majora's Mask items (#1169 lane
 *        XW5; OoTMM songMinuetMm, songBoleroMm, songSerenadeMm, songRequiemMm,
 *        songNocturneMm, songPreludeMm): the game-neutral rules and the ownership
 *        byte. Game-header-free.
 *
 * THE ITEMS. Under its frozen second-word rule (RSBS_XITEM2_SONG_MINUET_MM ...,
 * foreign_items.h) Majora's Mask's pool gains one copy of that song, an MM item
 * named as OoTMM names it ("Minuet of Forest (MM)" ...), RI_SONG_MINUET ..
 * RI_SONG_PRELUDE appended before RI_MAX_TRAP (games/mm/2s2h/Rando/Types.h,
 * WarpSongsMM.cpp). Under the shared rule (RSBS_XITEM2_SHARED_SONG_MINUET ...,
 * which requires the extension) OoTMM replaces both games' copies with one shared
 * item named without the suffix ("Minuet of Forest"); that family is lane XW10's,
 * and until it exists the MM copy is HELD BACK, never placed as a separate item.
 * Combo_MMWarpSongCopies is that rule, one place, the twin of XW4's
 * Combo_SongSoaringOoTCopies.
 *
 * OWNERSHIP is one bit per song in gComboCtx.mmWarpSongsOwned (context.h, ADR 0009
 * claim 20), in OoTMM's order, each one bit below its rule bit. The reasons it is
 * there and not in MM's half, why it is not in the invalidation KEEP set and why
 * it is not folded into the fingerprint are the field's comment.
 *
 * WHAT OWNING ONE DOES: nothing in play yet. Lane XW7 adds MM's note matcher, the
 * prompt, the En_Test7 departure and the crossing; lane XW9 the logic's travel
 * facts. Here MM's logic only knows how to PLAY each song (Logic.h,
 * CAN_PLAY_OOT_WARP_SONG: OoTMM's can_play_tp_* button sets), and no region uses
 * it.
 *
 * INERT UNTIL XW12. Every RSBS_XITEM2_* bit is hidden (RSBS_XITEM2_HIDDEN):
 * Generate refuses a world with one set before anything is frozen, and a seed
 * package or a load refuses one another build froze. So no world a player can
 * make pools one of these items, and the ownership byte stays 0.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Ownership bits in gComboCtx.mmWarpSongsOwned, in OoTMM's order. Each is its
 *  rule bit (RSBS_XITEM2_SONG_*_MM, 0x02 .. 0x40) shifted down by one. PINNED:
 *  .redsave and seed-package format. */
#define RSBS_MM_WARP_SONG_OWNED_MINUET 0x01u
#define RSBS_MM_WARP_SONG_OWNED_BOLERO 0x02u
#define RSBS_MM_WARP_SONG_OWNED_SERENADE 0x04u
#define RSBS_MM_WARP_SONG_OWNED_REQUIEM 0x08u
#define RSBS_MM_WARP_SONG_OWNED_NOCTURNE 0x10u
#define RSBS_MM_WARP_SONG_OWNED_PRELUDE 0x20u
/** Every allocated ownership bit; bits 6 and 7 are unallocated and stay zero. */
#define RSBS_MM_WARP_SONG_OWNED_ALL 0x3Fu
/** How many warp songs Majora's Mask can host. */
#define RSBS_MM_WARP_SONG_COUNT 6u

/**
 * The six MM rows' tooltips: OoTMM's descriptions (data.ts songMinuetMm ..
 * songPreludeMm) WITHOUT their second sentence ("If Songs are on Song Locations,
 * you must share or start with at least one song"). A stated divergence: 2Ship
 * has no song-location pass and an MM song never enters OoT's, so a song added to
 * MM never overflows any song spots. Lane XW12 draws the rows; the text is here
 * so it ships with the items it describes.
 */
#define RSBS_MM_WARP_SONG_TOOLTIP_MINUET "Add the Minuet of Forest in Majora's Mask."
#define RSBS_MM_WARP_SONG_TOOLTIP_BOLERO "Add the Bolero of Fire in Majora's Mask."
#define RSBS_MM_WARP_SONG_TOOLTIP_SERENADE "Add the Serenade of Water in Majora's Mask."
#define RSBS_MM_WARP_SONG_TOOLTIP_REQUIEM "Add the Requiem of Spirit in Majora's Mask."
#define RSBS_MM_WARP_SONG_TOOLTIP_NOCTURNE "Add the Nocturne of Shadow in Majora's Mask."
#define RSBS_MM_WARP_SONG_TOOLTIP_PRELUDE "Add the Prelude of Light in Majora's Mask."

/** The ownership bit of extension rule @p ruleBit (RSBS_XITEM2_SONG_MINUET_MM ..
 *  RSBS_XITEM2_SONG_PRELUDE_MM), or 0 for any other bit or mask. Pure. */
uint8_t Combo_MMWarpSongOwnedBit(uint32_t ruleBit);

/** The extension rule bit of ownership bit @p ownedBit, or 0 for any other bit
 *  or mask. Pure. */
uint32_t Combo_MMWarpSongRuleBit(uint8_t ownedBit);

/**
 * How many copies of the MM song whose extension rule is @p ruleBit a world whose
 * frozen second rules word is @p rules2 puts into Majora's Mask's pool: 1 with the
 * extension on and its shared song off; 0 with the extension off, and 0 with the
 * shared song on (the family path, held back, never placed as a separate item).
 * 0 for any bit that is not one of the six extensions. Pure.
 */
uint32_t Combo_MMWarpSongCopies(uint32_t rules2, uint32_t ruleBit);

/** The MM item's name ("Minuet of Forest (MM)" ...; OoTMM's option name), or NULL
 *  for any other bit. */
const char* Combo_MMWarpSongName(uint32_t ruleBit);

/** The Ocarina of Time item's own name ("Minuet of Forest" ...), the name its
 *  get-item model is asked by, or NULL for any other bit. */
const char* Combo_MMWarpSongOoTName(uint32_t ruleBit);

/** The MM row's tooltip (RSBS_MM_WARP_SONG_TOOLTIP_*), or NULL for any other bit. */
const char* Combo_MMWarpSongTooltip(uint32_t ruleBit);

/** The ownership byte, allocated bits only. */
uint8_t Combo_MMWarpSongsOwned(void);

/** Is the song whose extension rule is @p ruleBit owned? The bit alone, whatever
 *  the rules say; false for any other bit. */
bool Combo_MMWarpSongOwned(uint32_t ruleBit);

/** Own the song whose extension rule is @p ruleBit (MM's give). No-op for any
 *  other bit. */
void Combo_MMWarpSongGive(uint32_t ruleBit);

/** Replace the ownership byte with @p owned (allocated bits only): a new MM file
 *  (0), and every snapshot bracket that restores MM's save after a simulated
 *  playthrough gave items into it. */
void Combo_MMWarpSongsSetOwned(uint8_t owned);

#ifdef __cplusplus
}
#endif
