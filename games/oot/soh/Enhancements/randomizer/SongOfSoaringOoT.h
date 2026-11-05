#pragma once
/**
 * @file SongOfSoaringOoT.h
 * @brief Majora's Mask's Song of Soaring as an Ocarina of Time item (#1169 lane
 *        XW4; OoTMM songSoaringOot). The definitions and the reasoning are in
 *        SongOfSoaringOoT.cpp; this header is what the pool, the logic, the
 *        restricted song pass and the locks call.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Is the frozen Song of Soaring (OoT) rule on (RSBS_XITEM2_SONG_SOARING_OOT)?
 *  False while nothing is frozen. OoT's logic credits the song only then. */
bool OoT_SongOfSoaringOoTOn(void);

/** How many separate Songs of Soaring (OoT) the frozen world puts into OoT's
 *  pool (Combo_SongSoaringOoTCopies over the frozen second rules word): 1 with
 *  the rule on and the shared song off, else 0. */
int OoT_SongOfSoaringOoTPooledCopies(void);

#ifdef __cplusplus
}

#include <vector>

#include "randomizerTypes.h"

/**
 * THE GENERATE REFUSAL (OoTMM solve.ts placeSemiShuffled), asked by OoT's
 * restricted song pass (3drando/fill.cpp) with the songs it is about to place
 * (after the starting songs and the #1085 hold-back), the spots it would place
 * them on, and RSK_SHUFFLE_SONGS's value. 1 when the songs do not fit: the
 * refusal, beginning with OoTMM's "Not enough locations for songs/dungeon
 * rewards", is logged, written to stderr and recorded
 * (Combo_SongSpotsRecordRefusal), and the pass must end the generation. 0
 * otherwise, which is every world this build lets a player create.
 */
int OoT_SongSpots_RefuseOverflow(const std::vector<RandomizerGet>& songs, size_t spots, uint8_t shuffleSongs);
#endif

#endif // RSBS_SINGLE_EXECUTABLE
