/**
 * @file mm_shared_song_of_time_test.cpp
 * MM's half of the Shared Song of Time lock (#1069; the rule is
 * RSBS_XITEM_SHARED_SONG_TIME in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship rows `shared-song-of-time-pairs` and
 * `shared-song-of-time-arrival` (src/common/tests/test_shared_song_of_time.c)
 * drive MM's REAL MM_HarvestSharedResources / MM_ApplySharedResources through.
 * They only put the song's quest bit in a known state and read it back; every
 * merge decision is the production shims' and the registered family member's.
 * The Song of Time is playable on MM's ocarina exactly when its quest bit is
 * set: free play builds the available-song mask from the quest bits alone
 * (z_message.c, Message_DisplayOcarinaStaffImpl, the #1070 finding), and the
 * Song of Inverted Time and the Song of Double Time follow from it.
 */

#include "global.h"

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared Song of Time is a single-exe surface; test_runner.cpp declares
// these unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedSongOfTimeTest_SetHeld(int held) {
    (void)held;
}
extern "C" int MM_SharedSongOfTimeTest_Held(void) {
    return -1;
}
extern "C" int MM_SharedSongOfTimeTest_PoolId(void) {
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

/** Teach (1) or forget (0) the Song of Time, as MM's own
 *  Item_Give(ITEM_SONG_TIME) writes it: its quest bit and nothing else (the
 *  heart-piece bits above are kept). */
extern "C" void MM_SharedSongOfTimeTest_SetHeld(int held) {
    const uint32_t bit = 1u << QUEST_SONG_TIME;
    if (held != 0) {
        gSaveContext.save.saveInfo.inventory.questItems |= bit;
    } else {
        gSaveContext.save.saveInfo.inventory.questItems &= ~bit;
    }
}

/** 1 when MM knows the Song of Time (its ocarina can play it), else 0. */
extern "C" int MM_SharedSongOfTimeTest_Held(void) {
    return CHECK_QUEST_ITEM(QUEST_SONG_TIME) ? 1 : 0;
}

/** MM's pool id for the song: the RI_* its pool places it as. */
extern "C" int MM_SharedSongOfTimeTest_PoolId(void) {
    return (int)RI_SONG_TIME;
}

#endif
