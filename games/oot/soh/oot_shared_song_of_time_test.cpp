/**
 * @file oot_shared_song_of_time_test.cpp
 * OoT's half of the Shared Song of Time lock (#1069; the rule is
 * RSBS_XITEM_SHARED_SONG_TIME in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship rows `shared-song-of-time-pairs` and
 * `shared-song-of-time-arrival` (src/common/tests/test_shared_song_of_time.c)
 * drive OoT's REAL OoT_HarvestSharedResources / OoT_ApplySharedResources
 * through. They only put the song's quest bit in a known state and read it back;
 * every merge decision is the production shims' and the registered family
 * member's. The Song of Time is playable on OoT's ocarina exactly when its quest
 * bit is set: the staff builds its available-song mask from the quest bits alone
 * (z_message_PAL.c, the CHECK_QUEST_ITEM(QUEST_SONG_MINUET + i) loop).
 */

#include <cstring>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (oot_shared_kokiri_sword_test.cpp says why).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

/** Teach (1) or forget (0) the Song of Time, as OoT's own
 *  Item_Give(ITEM_SONG_TIME) writes it: its quest bit and nothing else. */
extern "C" void OoT_SharedSongOfTimeTest_SetHeld(int held) {
    const uint32_t bit = 1u << QUEST_SONG_TIME;
    if (held != 0) {
        gSaveContext.inventory.questItems |= bit;
    } else {
        gSaveContext.inventory.questItems &= ~bit;
    }
}

/** 1 when OoT knows the Song of Time (its ocarina can play it), else 0. */
extern "C" int OoT_SharedSongOfTimeTest_Held(void) {
    return CHECK_QUEST_ITEM(QUEST_SONG_TIME) ? 1 : 0;
}

/** OoT's pool id for the song: the RG_* its pool places it as. */
extern "C" int OoT_SharedSongOfTimeTest_PoolId(void) {
    return (int)RG_SONG_OF_TIME;
}
