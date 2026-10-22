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
 *
 * THE WORLD READERS the rando rows `shared-songs-one-copy-*` (#1085) count with:
 * how many of OoT's own checks hold a song in the generated world, and which
 * vanilla songs are still at their vanilla checks. They read the seed context
 * only.
 */

#include <cstring>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (oot_shared_kokiri_sword_test.cpp says why).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

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

// ---- #1085: the world readers ------------------------------------------------

namespace {
/** OoT's vanilla song placement, item_pool.cpp's Shuffle Songs Off block. */
struct OoTVanillaSong {
    RandomizerCheck rc;
    RandomizerGet song;
};
const OoTVanillaSong kOoTVanillaSongs[] = {
    { RC_SHEIK_IN_FOREST, RG_MINUET_OF_FOREST },
    { RC_SHEIK_IN_CRATER, RG_BOLERO_OF_FIRE },
    { RC_SHEIK_IN_ICE_CAVERN, RG_SERENADE_OF_WATER },
    { RC_SHEIK_AT_COLOSSUS, RG_REQUIEM_OF_SPIRIT },
    { RC_SHEIK_IN_KAKARIKO, RG_NOCTURNE_OF_SHADOW },
    { RC_SHEIK_AT_TEMPLE, RG_PRELUDE_OF_LIGHT },
    { RC_SONG_FROM_IMPA, RG_ZELDAS_LULLABY },
    { RC_SONG_FROM_MALON, RG_EPONAS_SONG },
    { RC_SONG_FROM_SARIA, RG_SARIAS_SONG },
    { RC_SONG_FROM_ROYAL_FAMILYS_TOMB, RG_SUNS_SONG },
    { RC_SONG_FROM_OCARINA_OF_TIME, RG_SONG_OF_TIME },
    { RC_SONG_FROM_WINDMILL, RG_SONG_OF_STORMS },
};

RandomizerGet OoTPlacedAt(const std::shared_ptr<Rando::Context>& ctx, RandomizerCheck rc) {
    Rando::ItemLocation* il = ctx->GetItemLocation(rc);
    return (il == nullptr) ? RG_NONE : il->GetPlacedRandomizerGet();
}
} // namespace

/** How many of the generated world's OoT checks hold @p rg (an RG_* id), or -1
 *  with no seed context. */
extern "C" int OoT_SharedSongsTest_WorldCopies(int rg) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return -1;
    }
    int copies = 0;
    for (const RandomizerCheck rc : ctx->allLocations) {
        if (OoTPlacedAt(ctx, rc) == (RandomizerGet)rg) {
            copies++;
        }
    }
    return copies;
}

/** How many of the twelve vanilla songs NOT among @p sharedRgs (RG_* ids) are at
 *  their vanilla check, or -1 with no seed context. */
extern "C" int OoT_SharedSongsTest_OtherVanillaSongsAtHome(const int* sharedRgs, int sharedCount) {
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return -1;
    }
    int atHome = 0;
    for (const OoTVanillaSong& v : kOoTVanillaSongs) {
        bool shared = false;
        for (int i = 0; sharedRgs != nullptr && i < sharedCount; i++) {
            shared = shared || sharedRgs[i] == (int)v.song;
        }
        if (!shared && OoTPlacedAt(ctx, v.rc) == v.song) {
            atHome++;
        }
    }
    return atHome;
}

/** The number of vanilla songs OoT_SharedSongsTest_OtherVanillaSongsAtHome walks. */
extern "C" int OoT_SharedSongsTest_VanillaSongCount(void) {
    return (int)(sizeof(kOoTVanillaSongs) / sizeof(kOoTVanillaSongs[0]));
}
