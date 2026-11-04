/**
 * @file SongOfSoaringOoT.cpp
 * @brief Majora's Mask's Song of Soaring as an Ocarina of Time item (#1169 lane
 *        XW4; OoTMM songSoaringOot, "Song of Soaring (OoT)").
 *
 * THE ITEM. Under the frozen second-word rule RSBS_XITEM2_SONG_SOARING_OOT
 * (src/common/foreign_items.h) OoT's pool gains one RG_SONG_OF_SOARING_OOT
 * (3drando/item_pool.cpp), with the songs' counts. It is a song
 * (ITEMTYPE_SONG), so OoT's restricted song passes count and place it, but it
 * has no quest bit: owning it is RAND_INF_OBTAINED_SONG_OF_SOARING_OOT, which its
 * get-item sets (randomizer.cpp's randomizerGetToRandInf) and OoT's logic reads
 * (logic.cpp: HasItem, and ApplyItemEffect's song branch, which would otherwise
 * set quest bit 0, the Forest Medallion). Under the shared rule
 * RSBS_XITEM2_SHARED_SONG_SOARING the pool gains nothing
 * (Combo_SongSoaringOoTCopies): the shared song is the family lane XW10 builds.
 *
 * NO NEW ItemID. The song has no inventory slot, pause row or C-button use, so
 * it adds nothing past ITEM_MASK_STONE and reaches none of the vanilla-sized
 * ItemID tables (OoT_gItemSlots, gItemAgeReqs, sItemActions; memory note
 * "custom-itemid-table-bounds"). Its GetItemEntry carries its RG id, as every
 * MOD_RANDOMIZER row does, and the give routes on that.
 *
 * WHAT IT DOES. Nothing in play yet: lane XW6 adds OoT's note matcher, the
 * Soaring gates and the crossing to an owl, and lane XW9 the logic's travel
 * facts. Here the logic only knows how to PLAY it (CanUse: OoTMM's OoT
 * can_play_soaring, a child with an ocarina and the C-down, C-up and C-left
 * buttons, which Ocarina Button Shuffle decides), and no region uses that.
 *
 * ONE SPOT SHORT. OoT_SongSpots_RefuseOverflow is OoTMM's "Not enough locations"
 * refusal, asked by the restricted song pass (warp_song_items.h says why).
 *
 * INERT. RSBS_XITEM2_HIDDEN covers the bit until lane XW12: Generate refuses it
 * before the freeze (3drando/playthrough.cpp) and a load refuses a world that has
 * it, so the frozen word is 0, the pool never gains the item, CanUse is false and
 * the song pass never sees a thirteenth song.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include <spdlog/spdlog.h>

#include "soh/Enhancements/randomizer/draw.h"
#include "SongOfSoaringOoT.h"

#include "context.h"         // src/common: SharedItem, GAME_MM
#include "foreign_items.h"   // src/common: the second rules word, Combo_GetForeignItemByNameFor
#include "warp_song_items.h" // src/common: the copies rule and the overflow refusal

// At file scope first: z64.h includes <memory> under C++, which must not get C
// linkage. Its include guard makes the nested include below a no-op.
#include <z64.h>
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
// ForeignModelHostOoT.cpp: an MM item's model, drawn by OoT as the host.
int OoT_ForeignModel_DrawForeignItem(PlayState* play, uint8_t originGame, uint16_t id);
}

// THE APPEND-ONLY LOCKS. Each id sits right after the Stone Mask's, so no id a
// world or a save already holds moves, and the RAND_INF words OoT saves are still
// the 129 they were: one more flag would make a 130th word and move every field
// after it in the save.
static_assert(RG_SONG_OF_SOARING_OOT == RG_STONE_MASK + 1,
              "RG_SONG_OF_SOARING_OOT is appended right after the masks, before the logic-only ids (#1169 XW4)");
static_assert(RHT_SONG_OF_SOARING_OOT == RHT_STONE_MASK + 1,
              "RHT_SONG_OF_SOARING_OOT is appended after RHT_STONE_MASK (#1169 XW4)");
static_assert(RAND_INF_OBTAINED_SONG_OF_SOARING_OOT == RAND_INF_OBTAINED_STONE_MASK + 1,
              "RAND_INF_OBTAINED_SONG_OF_SOARING_OOT is appended after the Stone Mask's flag (#1169 XW4)");
static_assert((RAND_INF_MAX + 15) / 16 == 129 && sizeof(ShipSaveContextData::randomizerInf) == 129 * sizeof(u16),
              "OoT's RAND_INF flags must still fit the 129 saved u16 words: a 130th word moves the save layout "
              "(#1169 XW4)");
// The refusal names the mode from RSK_SHUFFLE_SONGS's raw value, which fill.cpp
// passes through; warp_song_items.h spells the two restricted values game-free.
static_assert(RSBS_SONG_SPOTS_SONG_LOCATIONS == (unsigned)RO_SONG_SHUFFLE_SONG_LOCATIONS &&
                  RSBS_SONG_SPOTS_DUNGEON_REWARDS == (unsigned)RO_SONG_SHUFFLE_DUNGEON_REWARDS,
              "RSBS_SONG_SPOTS_* must equal OoT's RO_SONG_SHUFFLE_* restricted values (#1169 XW4)");

extern "C" bool OoT_SongOfSoaringOoTOn(void) {
    return Combo_ComboCrossItemRule2(RSBS_XITEM2_SONG_SOARING_OOT);
}

extern "C" int OoT_SongOfSoaringOoTPooledCopies(void) {
    return (int)Combo_SongSoaringOoTCopies(Combo_ComboCrossItemFlags2());
}

int OoT_SongSpots_RefuseOverflow(const std::vector<RandomizerGet>& songs, size_t spots, uint8_t shuffleSongs) {
    const bool soaringOoT = std::find(songs.begin(), songs.end(), RG_SONG_OF_SOARING_OOT) != songs.end();
    char msg[256];
    if (Combo_SongSpotsOverflow((uint32_t)songs.size(), (uint32_t)spots, (uint32_t)shuffleSongs, soaringOoT, msg,
                                sizeof(msg)) == 0u) {
        return 0;
    }
    SPDLOG_ERROR("{}; refusing to generate (#1169)", msg);
    fprintf(stderr, "[OoT] generation REFUSED: %s (#1169)\n", msg);
    Combo_SongSpotsRecordRefusal(msg);
    return 1;
}

/**
 * RG_SONG_OF_SOARING_OOT's get-item model (a #1185 descriptor): Majora's Mask's
 * own Song of Soaring, drawn by OoT as the host through the co-mounted archives
 * (OoT_ForeignModel_DrawForeignItem: since #1185 MM's songs answer a note and OoT
 * draws its own note in DrawSong's Soaring colour); OoT's generic white note
 * (GID_SONG_GENERIC, OoTMM's draw [NOTE]) when no archive answers.
 */
extern "C" void Randomizer_DrawSongOfSoaringOoT(PlayState* play, GetItemEntry* getItemEntry) {
    (void)getItemEntry;
    static SharedItem sMmSoaring;
    static int sResolved = -1;
    if (play == nullptr) {
        return;
    }
    if (sResolved < 0) {
        sResolved = Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, "Song of Soaring", &sMmSoaring) ? 1 : 0;
    }
    if (sResolved == 1 && OoT_ForeignModel_DrawForeignItem(play, sMmSoaring.originGame, sMmSoaring.id) == 1) {
        return;
    }
    OoT_GetItem_Draw(play, GID_SONG_GENERIC);
}

#endif // RSBS_SINGLE_EXECUTABLE
