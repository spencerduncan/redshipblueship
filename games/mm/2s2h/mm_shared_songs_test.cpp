/**
 * @file mm_shared_songs_test.cpp
 * MM's half of the Shared Epona's Song / Shared Song of Storms lock (#1066; the
 * rules are RSBS_XITEM_SHARED_SONG_EPONA and RSBS_XITEM_SHARED_SONG_STORMS in
 * src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship row `shared-songs-merge`
 * (src/common/tests/test_shared_songs.c) drives MM's REAL
 * MM_HarvestSharedResources / MM_ApplySharedResources through. They only put a
 * song's quest bit in a known state and read it back; every merge decision is
 * the production shims' and the registered family member's. A song is playable
 * on MM's ocarina exactly when its quest bit is set: free play builds the
 * available-song mask from the quest bits alone (z_message.c,
 * Message_DisplayOcarinaStaffImpl's CHECK_QUEST_ITEM(QUEST_SONG_SONATA + i)
 * loop, the #1070 finding). `song` is 0 for Epona's Song and 1 for the Song of
 * Storms.
 */

#include "global.h"

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared songs are a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedSongsTest_SetHeld(int song, int held) {
    (void)song;
    (void)held;
}
extern "C" int MM_SharedSongsTest_Held(int song) {
    (void)song;
    return -1;
}
extern "C" int MM_SharedSongsTest_PoolId(int song) {
    (void)song;
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

namespace {
int MMSongQuestBit(int song) {
    return (song == 0) ? (int)QUEST_SONG_EPONA : (int)QUEST_SONG_STORMS;
}
} // namespace

/** Teach (1) or forget (0) the song, as MM's own Item_Give(ITEM_SONG_*) writes
 *  it: its quest bit and nothing else (the heart-piece bits above are kept). */
extern "C" void MM_SharedSongsTest_SetHeld(int song, int held) {
    const uint32_t bit = 1u << MMSongQuestBit(song);
    if (held != 0) {
        gSaveContext.save.saveInfo.inventory.questItems |= bit;
    } else {
        gSaveContext.save.saveInfo.inventory.questItems &= ~bit;
    }
}

/** 1 when MM knows the song (its ocarina can play it), else 0. */
extern "C" int MM_SharedSongsTest_Held(int song) {
    return CHECK_QUEST_ITEM(MMSongQuestBit(song)) ? 1 : 0;
}

/** MM's pool id for the song: the RI_* its pool places it as. */
extern "C" int MM_SharedSongsTest_PoolId(int song) {
    return (song == 0) ? (int)RI_SONG_EPONA : (int)RI_SONG_STORMS;
}

#endif
