#pragma once
/**
 * @file warp_song_items.h
 * @brief The cross-game warp songs' item rules that are neither game's (#1169,
 *        lane XW4; OoTMM songSoaringOot and sharedSongSoaring). Game-header-free.
 *
 * THE SONG OF SOARING (OoT). Under the frozen second-word rule
 * RSBS_XITEM2_SONG_SOARING_OOT (foreign_items.h) Ocarina of Time's pool gains one
 * separate item, "Song of Soaring (OoT)" (RG_SONG_OF_SOARING_OOT,
 * games/oot/soh/Enhancements/randomizer/SongOfSoaringOoT.cpp). Under the shared
 * rule RSBS_XITEM2_SHARED_SONG_SOARING (which requires the extension) OoTMM
 * instead replaces Majora's Mask's copy with one shared item
 * (logic/src/world/transform.ts: `replaceItem(MM_SONG_SOARING,
 * SHARED_SONG_SOARING)` and NO `addItem(OOT_SONG_SOARING)`): that family is lane
 * XW10's, and until it exists OoT's separate copy is HELD BACK, never placed.
 * Combo_SongSoaringOoTCopies is that rule, one place.
 *
 * ONE SPOT SHORT. OoT has twelve song spots under Shuffle Songs: Song Locations
 * (ShuffleSongs.cpp) and twelve under Dungeon Rewards (the eight boss hearts, the
 * Bottom of the Well lens chest, the Gerudo Training Ground final chest, Sheik in
 * Ice Cavern and Impa; 3drando/fill.cpp), and twelve songs of its own. A separate
 * Song of Soaring makes thirteen, so OoTMM refuses before placing anything
 * (solver/solve.ts, placeSemiShuffled: "Not enough locations for
 * songs/dungeon rewards"). Operator ruling 2026-11-04 (decision 2): OoTMM's
 * behaviour exactly, so Generate refuses with that wording, the option's text
 * warns, and the option is never greyed out. Combo_SongSpotsOverflow is the count
 * and the wording; OoT's restricted song pass asks it after the starting songs
 * left the pool and after the #1085 shared hold-back, the two ways a player makes
 * room (OoTMM's "share or start with at least one song"). Each game fills its own
 * song spots here, not one combined pass as OoTMM does, so only an Ocarina of Time
 * starting song frees one of OoT's spots; the texts therefore say "start with an
 * Ocarina of Time song", a stated divergence from OoTMM's wording.
 *
 * INERT UNTIL XW12. Every RSBS_XITEM2_* bit is hidden (RSBS_XITEM2_HIDDEN): Generate
 * refuses a world with one set before anything is frozen, and a load refuses one
 * another build froze. So no world a player can make has the bit, the pool never
 * gains the item, and the song pass never holds more than twelve songs.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** OoTMM's refusal, verbatim (packages/logic/src/solver/solve.ts,
 *  placeSemiShuffled). Every refusal message begins with it. */
#define RSBS_SONG_SPOTS_REFUSAL "Not enough locations for songs/dungeon rewards"

/**
 * The Song of Soaring (OoT) row's tooltip (OoTMM data.ts songSoaringOot's
 * description, with Dungeon Rewards named too, since OoT's Shuffle Songs has
 * both restricted modes, and "an Ocarina of Time song" for OoTMM's "at least one
 * song", above). Lane XW12 draws the row; the text is here so the warning ships
 * with the refusal it describes.
 */
#define RSBS_SONG_SOARING_OOT_TOOLTIP                                                                               \
    "Adds Majora's Mask's Song of Soaring to Ocarina of Time. Usable only as a child. If Shuffle Songs is on Song " \
    "Locations or Dungeon Rewards, you must share a song or start with an Ocarina of Time song: those twelve "      \
    "spots cannot hold a thirteenth song, and Generate refuses."

/** OoT's restricted Shuffle Songs values (RO_SONG_SHUFFLE_SONG_LOCATIONS and
 *  RO_SONG_SHUFFLE_DUNGEON_REWARDS, randomizerTypes.h), the two passes with a
 *  fixed set of spots. SongOfSoaringOoT.cpp static-asserts both. */
#define RSBS_SONG_SPOTS_SONG_LOCATIONS 1u
#define RSBS_SONG_SPOTS_DUNGEON_REWARDS 2u

/**
 * How many copies of OoT's separate Song of Soaring a world whose frozen second
 * rules word is @p rules2 puts into OoT's pool: 1 with the extension on and the
 * shared song off; 0 with the rule off, and 0 with the shared song on (the
 * family path, held back, never placed as a separate item). Pure.
 */
uint32_t Combo_SongSoaringOoTCopies(uint32_t rules2);

/**
 * THE OVERFLOW REFUSAL. @p songs songs for @p spots spots in the restricted song
 * pass @p mode (RSBS_SONG_SPOTS_*): returns how many songs do not fit (0 when
 * every song has a spot). When it is not 0 and @p msg is not NULL, writes the
 * refusal, beginning with RSBS_SONG_SPOTS_REFUSAL, naming the counts and the
 * mode, and, when @p soaringOoT (the separate Song of Soaring is among the
 * songs), naming it and the ways out. Always NUL-terminates @p msg when @p len >
 * 0 ("" when nothing overflows). Pure.
 */
uint32_t Combo_SongSpotsOverflow(uint32_t songs, uint32_t spots, uint32_t mode, bool soaringOoT, char* msg, size_t len);

/** Record the refusal a generation ended with (NULL or "" clears it). OoT's song
 *  pass records it as it refuses; the locks read it back. */
void Combo_SongSpotsRecordRefusal(const char* msg);

/** The last recorded refusal, or "" when none. Never NULL. */
const char* Combo_SongSpotsLastRefusal(void);

#ifdef __cplusplus
}
#endif
