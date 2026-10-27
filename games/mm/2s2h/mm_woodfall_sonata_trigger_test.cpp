/**
 * @file mm_woodfall_sonata_trigger_test.cpp
 * ROM-free, display-free guard from the Woodfall forensics (#1203, #1070): the
 * Woodfall platform's Sonata trigger box. CTest label "redship", row
 * MMWoodfallSonataTrigger in CMake/SingleExecutable.cmake, dispatch
 * "mm-woodfall-sonata-trigger" in src/common/test_runner.cpp.
 *
 * WHAT IT PINS. func_80AA8698 (ovl_Dm_Char01/z_dm_char01.c), the Woodfall
 * scene object's idle action, raises the temple (WEEKEVENTREG_20_01) when Deku
 * Link has just played the Sonata (ocarinaMode EVENT, lastPlayedSong SONATA)
 * standing inside x in (-40, 40), z in (1000, 1078); no height bound. #1070's
 * forensics had to rule out a moved or shrunken box before landing on the
 * missing song (#1203); this row keeps that question answered: the platform
 * centre (0, 200, 1040) raises the temple, and (0, 200, 966), (0, 200, 987)
 * and (1, 200, 1094), outside the box, do not. Two controls at the centre: a
 * non-Deku form and a song other than the Sonata raise nothing.
 *
 * Green on origin/main and after #1203's fix: it guards the trigger, it does
 * not lock a change.
 *
 * Run with the rando behaviour disarmed (vanilla save, ActorBehavior::OnFileLoad):
 * Rando/ActorBehavior/DmChar01.cpp answers VB_OPEN_WOODFALL_FROM_SONG and hooks
 * the flag under some dungeon-access options, and this row is about the
 * vanilla box both of those build on. Cleanup restores the SaveContext image
 * and MM_gPlayState and re-runs OnFileLoad, so the row is order-free.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
extern "C" int MM_WoodfallSonataTrigger_RunHeadless(void) {
    printf("[TEST] mm-woodfall-sonata-trigger: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdlib>
#include <cstring>
#include <memory>

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "overlays/actors/ovl_Dm_Char01/z_dm_char01.h"

// The Woodfall scene object's idle action (ovl_Dm_Char01/z_dm_char01.c); no
// header declares it.
void func_80AA8698(DmChar01* thisx, PlayState* play);
}

namespace {

struct TriggerProbe {
    const char* what;
    f32 x;
    f32 y;
    f32 z;
    u8 form;
    u16 song;
    bool raises;
};

const TriggerProbe kProbes[] = {
    { "Deku Sonata at the platform centre (0, 200, 1040)", 0.0f, 200.0f, 1040.0f, PLAYER_FORM_DEKU, OCARINA_SONG_SONATA,
      true },
    { "Deku Sonata short of the box (0, 200, 966)", 0.0f, 200.0f, 966.0f, PLAYER_FORM_DEKU, OCARINA_SONG_SONATA,
      false },
    { "Deku Sonata short of the box (0, 200, 987)", 0.0f, 200.0f, 987.0f, PLAYER_FORM_DEKU, OCARINA_SONG_SONATA,
      false },
    { "Deku Sonata past the box (1, 200, 1094)", 1.0f, 200.0f, 1094.0f, PLAYER_FORM_DEKU, OCARINA_SONG_SONATA, false },
    { "human-form Sonata at the centre (control)", 0.0f, 200.0f, 1040.0f, PLAYER_FORM_HUMAN, OCARINA_SONG_SONATA,
      false },
    { "Deku Song of Healing at the centre (control)", 0.0f, 200.0f, 1040.0f, PLAYER_FORM_DEKU, OCARINA_SONG_HEALING,
      false },
};

int Fail(int code, const char* msg) {
    printf("[TEST] FAIL(%d): mm-woodfall-sonata-trigger: %s\n", code, msg);
    return code;
}

bool TempleRaised() {
    return CHECK_WEEKEVENTREG(WEEKEVENTREG_20_01) != 0;
}

/** Clear the temple flag directly: CLEAR_WEEKEVENTREG dispatches flag hooks
 *  this row is not about. */
void LowerTemple() {
    WEEKEVENTREG(WEEKEVENTREG_20_01 >> 8) &= (u8) ~(WEEKEVENTREG_20_01 & 0xFF);
}

int RunProbes(PlayState* play, Player* player, DmChar01* platform) {
    play->actorCtx.actorLists[ACTORCAT_PLAYER].first = &player->actor;
    play->msgCtx.ocarinaMode = OCARINA_MODE_EVENT;
    for (const auto& probe : kProbes) {
        LowerTemple();
        memset(platform, 0, sizeof(*platform));
        player->transformation = probe.form;
        player->stateFlags2 = 0; // not mid-song: the box chime branch stays out of a display-free run
        player->actor.world.pos.x = probe.x;
        player->actor.world.pos.y = probe.y;
        player->actor.world.pos.z = probe.z;
        play->msgCtx.lastPlayedSong = probe.song;
        func_80AA8698(platform, play);
        if (TempleRaised() != probe.raises) {
            printf("[TEST] mm-woodfall-sonata-trigger: %s: expected the temple %s\n", probe.what,
                   probe.raises ? "raised" : "untouched");
            return Fail(10, "the Woodfall Sonata trigger box no longer matches the vanilla box");
        }
        printf("[TEST] mm-woodfall-sonata-trigger: %s: temple %s\n", probe.what, probe.raises ? "raised" : "untouched");
    }
    return 0;
}

} // namespace

extern "C" int MM_WoodfallSonataTrigger_RunHeadless(void) {
    printf("[TEST] mm-woodfall-sonata-trigger: Deku Link's Sonata raises Woodfall Temple inside the platform box "
           "only (#1203 guard)\n");

    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;

    PlayState* play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    Player* player = static_cast<Player*>(calloc(1, sizeof(Player)));
    DmChar01* platform = static_cast<DmChar01*>(calloc(1, sizeof(DmChar01)));
    if (play == nullptr || player == nullptr || platform == nullptr) {
        free(play);
        free(player);
        free(platform);
        return Fail(1, "could not allocate the fixtures");
    }
    play->sceneId = SCENE_21MITURINMAE;
    MM_gPlayState = play;
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_VANILLA;
    Rando::ActorBehavior::OnFileLoad();

    const int rc = RunProbes(play, player, platform);

    // ---- Cleanup (pass and fail) -------------------------------------------
    MM_gPlayState = savedPlay;
    memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));
    Rando::ActorBehavior::OnFileLoad();
    free(play);
    free(player);
    free(platform);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-woodfall-sonata-trigger: PASS (the centre raises the temple; outside the box, another form "
           "or another song raise nothing)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
