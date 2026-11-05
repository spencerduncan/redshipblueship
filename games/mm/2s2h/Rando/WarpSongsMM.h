#pragma once
/**
 * @file WarpSongsMM.h
 * @brief Ocarina of Time's six warp songs as Majora's Mask items (#1169 lane XW5;
 *        OoTMM songMinuetMm .. songPreludeMm, "Minuet of Forest (MM)" ...). The
 *        definitions and the reasoning are in WarpSongsMM.cpp; this header is what
 *        the item pipeline (GiveItem, ConvertItem, GeneratePools, DrawItem,
 *        Items.cpp), the logic (Logic.h), the snapshot brackets and the locks call.
 *        C-compatible.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The extension rule (RSBS_XITEM2_SONG_MINUET_MM ...) that adds MM item `riId`
 *  (RI_SONG_MINUET .. RI_SONG_PRELUDE) to MM's pool; 0 for any other id. Pure. */
uint32_t MM_WarpSongMM_RuleBit(uint16_t riId);

/** The MM item of extension rule `ruleBit`; 0 (RI_UNKNOWN) for any other bit. Pure. */
uint16_t MM_WarpSongMM_ItemId(uint32_t ruleBit);

/** Is the frozen extension rule that adds `riId` on? False for any other id and
 *  while nothing is frozen. */
bool MM_WarpSongMM_RuleOn(uint16_t riId);

/** How many copies of `riId` the frozen world puts into MM's pool
 *  (Combo_MMWarpSongCopies over the frozen second rules word): 1 with its rule on
 *  and its shared song off, else 0. */
int MM_WarpSongMM_PooledCopies(uint16_t riId);

/** Does the pair own `riId` (its bit in gComboCtx.mmWarpSongsOwned)? The bit
 *  alone, whatever the rules say. */
bool MM_WarpSongMM_Owned(uint16_t riId);

/** Own `riId` (MM's give). No-op for any other id. */
void MM_WarpSongMM_Give(uint16_t riId);

/** OoTMM's has_song_tp_* in MM: the frozen rule holds the extension and the song
 *  is owned. What MM's logic reads (Logic.h, CAN_PLAY_OOT_WARP_SONG). */
bool MM_WarpSongMM_Usable(uint16_t riId);

/** The ownership byte, for a bracket that restores MM's save after a simulated
 *  playthrough gave items into it (the bag's MM engine, MM's crawl, its
 *  glitchless fill, the attempt ladder): read before, put back after. */
uint8_t MM_WarpSongsMM_SnapshotOwned(void);
void MM_WarpSongsMM_RestoreOwned(uint8_t owned);

/** The pause and tracker icon of `riId`: MM's own song note. NULL for any other id. */
const char* MM_WarpSongMM_IconPath(uint16_t riId);

/** Draw `riId`'s get-item model (DrawItem.cpp): Ocarina of Time's own song through
 *  the co-mounted archives (MM's host-native note in that song's colour), else
 *  MM's generic white note. */
void MM_WarpSongMM_DrawGetItem(uint16_t riId);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
