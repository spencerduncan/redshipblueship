/**
 * @file mm_crossing_song_playable_test.cpp
 * ROM-free, display-free guard from the Woodfall forensics (#1203, #1070): an
 * MM song bought or found in Hyrule crosses into Termina, lands as its quest
 * bit, and free play then accepts it. CTest label "redship", row
 * MMCrossingSongPlayable in CMake/SingleExecutable.cmake (beside
 * ForeignAwardMM), dispatch "mm-crossing-song-playable" in
 * src/common/test_runner.cpp.
 *
 * WHY. #1070's operator played the Sonata from memory at the Woodfall platform
 * before owning it, and nothing happened: free play builds its playable set
 * from the quest bits (MM_Message_BuildOcarinaAvailableSongs, factored out of
 * Message_DisplayOcarinaStaffImpl in z_message.c), so an unowned song is not
 * heard. The forensics then had to establish that a song that DOES cross is
 * playable once it lands. This row pins that chain end to end, with the real
 * pieces: Combo_RecordSharedItemCrossing (an MM item taken from an OoT check),
 * MM_ConsumeSharedItems (the arrival), the deferred give drained over a
 * stand-in PlayState (MM_ForeignItem_TestFlushWithStandInPlay, which admits
 * these three songs because Item_GiveImpl's song branch never reads `play`),
 * and the mask free play reads.
 *
 * Green on origin/main and after #1203's fix: it guards the crossing path, it
 * does not lock a change.
 *
 * LEGS.
 *   1. Control: with the three quest bits clear, the mask lacks their bits
 *      (an unowned song is not playable, the #1070 observation).
 *   2. Three crossings (Sonata, Healing, Soaring) are recorded, the arrival
 *      redeems all three and queues them while no PlayState is live.
 *   3. The first gameplay frame's drain gives all three: the quest bits are set
 *      and the mask includes each song.
 * Cleanup restores the SaveContext image, gComboCtx, MM_gPlayState and the
 * pending queue, so the row is order-free.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
extern "C" int MM_CrossingSongPlayable_RunHeadless(void) {
    printf("[TEST] mm-crossing-song-playable: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdlib>
#include <cstring>
#include <memory>

#include "2s2h/Rando/Rando.h"
#include "context.h"
#include "shared_items.h"

extern "C" {
#include "variables.h"
#include "functions.h"

// The arrival hook z_play.c calls (GameExports_SingleExe.cpp).
void MM_ConsumeSharedItems(void);
// The deferred give's test surface (2s2h/Rando/ForeignItemsSingleExe.cpp).
int MM_ForeignItem_TestPendingCount(void);
void MM_ForeignItem_TestResetPending(void);
int MM_ForeignItem_TestFlushWithStandInPlay(void);
// The playable-song mask free play builds (z_message.c).
u32 MM_Message_BuildOcarinaAvailableSongs(void);
}

namespace {

struct CrossingSong {
    const char* name;
    RandoItemId riId;
    s32 questItem;
};

const CrossingSong kSongs[] = {
    { "Sonata of Awakening", RI_SONG_SONATA, QUEST_SONG_SONATA },
    { "Song of Healing", RI_SONG_HEALING, QUEST_SONG_HEALING },
    { "Song of Soaring", RI_SONG_SOARING, QUEST_SONG_SOARING },
};

/** The bit free play's mask carries for a song in the Sonata..Sun range
 *  (z_message.c's sOcarinaSongFlagsMap is 1 << index). */
u32 PlayableBit(s32 questItem) {
    return 1u << (questItem - QUEST_SONG_SONATA);
}

int Fail(int code, const char* msg) {
    printf("[TEST] FAIL(%d): mm-crossing-song-playable: %s\n", code, msg);
    return code;
}

int RunLegs() {
    // ---- Leg 1: control -----------------------------------------------------
    for (const auto& song : kSongs) {
        REMOVE_QUEST_ITEM(song.questItem);
    }
    const u32 before = MM_Message_BuildOcarinaAvailableSongs();
    for (const auto& song : kSongs) {
        if ((before & PlayableBit(song.questItem)) != 0) {
            printf("[TEST] mm-crossing-song-playable: %s is playable with its quest bit clear (mask 0x%08X)\n",
                   song.name, (unsigned)before);
            return Fail(2, "free play accepts a song the save does not own: the mask no longer follows the quest "
                           "bits, so the legs below would be vacuous");
        }
    }

    // ---- Leg 2: three crossings, one arrival --------------------------------
    for (const auto& song : kSongs) {
        printf("[TEST] mm-crossing-song-playable: recording the %s crossing (RI id %u)\n", song.name,
               (unsigned)song.riId);
        if (Combo_RecordSharedItemCrossing(GAME_MM, (uint16_t)song.riId) < 0) {
            return Fail(3, "Combo_RecordSharedItemCrossing refused an MM song");
        }
    }
    if (Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) != 3) {
        return Fail(3, "the three crossings are not all owed to Majora's Mask");
    }
    MM_gPlayState = nullptr; // the arrival runs before MM_gPlayState is set (z_play.c)
    MM_ConsumeSharedItems();
    if (Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) != 0) {
        return Fail(4, "the arrival left a song crossing unredeemed");
    }
    if (MM_ForeignItem_TestPendingCount() != 3) {
        printf("[TEST] mm-crossing-song-playable: %d gives pending\n", MM_ForeignItem_TestPendingCount());
        return Fail(4, "the arrival did not queue exactly the three song gives for the first gameplay frame");
    }

    // ---- Leg 3: the first gameplay frame ------------------------------------
    const int given = MM_ForeignItem_TestFlushWithStandInPlay();
    if (given != 3) {
        printf("[TEST] mm-crossing-song-playable: the stand-in drain returned %d\n", given);
        return Fail(5, "the first gameplay frame's drain did not give the three songs");
    }
    const u32 after = MM_Message_BuildOcarinaAvailableSongs();
    for (const auto& song : kSongs) {
        if (!CHECK_QUEST_ITEM(song.questItem)) {
            printf("[TEST] mm-crossing-song-playable: %s crossed and was given, but its quest bit is clear\n",
                   song.name);
            return Fail(6, "a crossed song did not land as its quest bit");
        }
        if ((after & PlayableBit(song.questItem)) == 0) {
            printf("[TEST] mm-crossing-song-playable: %s is owned but not playable (mask 0x%08X)\n", song.name,
                   (unsigned)after);
            return Fail(7, "free play does not accept a crossed song the save owns");
        }
    }
    printf("[TEST] mm-crossing-song-playable: playable mask 0x%08X -> 0x%08X\n", (unsigned)before, (unsigned)after);
    return 0;
}

} // namespace

extern "C" int MM_CrossingSongPlayable_RunHeadless(void) {
    printf("[TEST] mm-crossing-song-playable: a song that crosses from Hyrule is owned and playable in Termina "
           "(#1203 guard)\n");

    auto savedSave = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(savedSave.get(), &gSaveContext, sizeof(gSaveContext));
    auto savedCombo = std::make_unique<uint8_t[]>(sizeof(gComboCtx));
    memcpy(savedCombo.get(), &gComboCtx, sizeof(gComboCtx));
    PlayState* const savedPlay = MM_gPlayState;

    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();

    const int rc = RunLegs();

    // ---- Cleanup (pass and fail) -------------------------------------------
    MM_ForeignItem_TestResetPending();
    Combo_ClearSharedItemOutbox();
    memcpy(&gComboCtx, savedCombo.get(), sizeof(gComboCtx));
    MM_gPlayState = savedPlay;
    memcpy(&gSaveContext, savedSave.get(), sizeof(gSaveContext));

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-crossing-song-playable: PASS (unowned songs are not playable; Sonata, Healing and Soaring "
           "cross, land as quest bits and are playable)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
