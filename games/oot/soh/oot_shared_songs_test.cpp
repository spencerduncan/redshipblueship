/**
 * @file oot_shared_songs_test.cpp
 * OoT's half of the Shared Epona's Song / Shared Song of Storms lock (#1066; the
 * rules are RSBS_XITEM_SHARED_SONG_EPONA and RSBS_XITEM_SHARED_SONG_STORMS in
 * src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship row `shared-songs-merge`
 * (src/common/tests/test_shared_songs.c) drives OoT's REAL
 * OoT_HarvestSharedResources / OoT_ApplySharedResources through. They only put
 * a song's quest bit in a known state and read it back; every merge decision is
 * the production shims' and the registered family member's. A song is playable
 * on OoT's ocarina exactly when its quest bit is set: the staff builds its
 * available-song mask from the quest bits alone (z_message_PAL.c, the
 * CHECK_QUEST_ITEM(QUEST_SONG_MINUET + i) loop), so the bit is the whole of
 * "knowing" the song. `song` is 0 for Epona's Song and 1 for the Song of Storms.
 */

#include <cstring>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (oot_shared_kokiri_sword_test.cpp says why).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

namespace {
int OoTSongQuestBit(int song) {
    return (song == 0) ? (int)QUEST_SONG_EPONA : (int)QUEST_SONG_STORMS;
}
} // namespace

/** Teach (1) or forget (0) the song, as OoT's own Item_Give(ITEM_SONG_*) writes
 *  it: its quest bit and nothing else. */
extern "C" void OoT_SharedSongsTest_SetHeld(int song, int held) {
    const uint32_t bit = 1u << OoTSongQuestBit(song);
    if (held != 0) {
        gSaveContext.inventory.questItems |= bit;
    } else {
        gSaveContext.inventory.questItems &= ~bit;
    }
}

/** 1 when OoT knows the song (its ocarina can play it), else 0. */
extern "C" int OoT_SharedSongsTest_Held(int song) {
    return CHECK_QUEST_ITEM(OoTSongQuestBit(song)) ? 1 : 0;
}

/** OoT's pool id for the song: the RG_* its pool places it as. */
extern "C" int OoT_SharedSongsTest_PoolId(int song) {
    return (song == 0) ? (int)RG_EPONAS_SONG : (int)RG_SONG_OF_STORMS;
}
