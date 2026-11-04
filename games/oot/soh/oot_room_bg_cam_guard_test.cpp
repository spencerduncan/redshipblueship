/**
 * @file oot_room_bg_cam_guard_test.cpp
 * ROM-free, display-free lock for #1336 (SFX-1, upstream e9268c446 / f2c34d8c1,
 * plus SFX-18 c9414b4d4 registrar check):
 *   - func_80096A74 (games/oot/src/code/z_room.c) queries
 *     GameInteractor_Should(VB_SHOULD_LOAD_BG_IMAGE, true, &camId) before
 *     indexing cameraDataList[camId] via func_80041C10;
 *   - AlwaysOnFixes.cpp registers its VB_SHOULD_LOAD_BG_IMAGE handler at boot
 *     through ShipInit ("*") and short-circuits when camId == -1;
 *   - Goron.cpp registers RegisterGoronMessages in ShipInit ("IS_RANDO").
 *
 * CTest label "redship", row OoTRoomBgCamGuard in CMake/SingleExecutable.cmake,
 * dispatch "oot-room-bg-cam-guard" in src/common/test_runner.cpp.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <z64.h>

#include "context.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern PlayState* OoT_gPlayState;
BgImage* func_80096A74(PolygonType1* polygon1, PlayState* play);
}

namespace {

#define RBCG_ASSERT(cond, msg)                                            \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

bool HasFileSuffix(const std::string& path, const std::string& suffix) {
    return path.size() >= suffix.size() && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool RunAlwaysOnFixesRegistrar(void) {
    int found = 0;
    for (const auto& entry : ShipInit::GetAll()["*"]) {
        const std::string file = entry.registeredAt.file_name();
        if (HasFileSuffix(file, "AlwaysOnFixes.cpp")) {
            entry.initFunc();
            found++;
        }
    }
    printf("[TEST] AlwaysOnFixes.cpp boot registrars found: %d\n", found);
    return found == 1;
}

bool HasGoronMessagesRegistrar(void) {
    int found = 0;
    for (const auto& entry : ShipInit::GetAll()["IS_RANDO"]) {
        const std::string file = entry.registeredAt.file_name();
        if (HasFileSuffix(file, "Goron.cpp")) {
            found++;
        }
    }
    printf("[TEST] Goron.cpp IS_RANDO registrars found: %d\n", found);
    return found == 1;
}

int RunLegs(PlayState* play) {
    RBCG_ASSERT(RunAlwaysOnFixesRegistrar(),
                "AlwaysOnFixes.cpp boot registrar is not in ShipInit's '*' map exactly once");
    RBCG_ASSERT(HasGoronMessagesRegistrar(), "Goron.cpp registrar is not in ShipInit's 'IS_RANDO' map exactly once");

    Camera activeCam = {};
    play->activeCamera = MAIN_CAM;
    play->cameraPtrs[MAIN_CAM] = &activeCam;

    // (a) camDataIdx == -1 short-circuits via VB_SHOULD_LOAD_BG_IMAGE before
    // dereferencing play->colCtx.colHeader, GET_PLAYER(play), or polygon1.
    activeCam.camDataIdx = -1;
    play->colCtx.colHeader = NULL;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].head = NULL;
    BgImage* skipped = func_80096A74(NULL, play);
    RBCG_ASSERT(skipped == NULL, "(a): func_80096A74 did not return NULL when camDataIdx == -1");

    // (b) camDataIdx >= 0 proceeds through VB_SHOULD_LOAD_BG_IMAGE, resolves
    // camId2 from cameraDataList[camId].camPosData[2].y, stamps the low byte of
    // player->actor.params, and returns the matching BgImage entry.
    Vec3s camPosData[3] = {};
    camPosData[2].y = 1;

    CamData camData[1] = {};
    camData[0].camPosData = camPosData;

    CollisionHeader colHeader = {};
    colHeader.cameraDataList = camData;
    play->colCtx.colHeader = &colHeader;

    Player player = {};
    player.actor.params = (s16)0xAA00;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;

    BgImage bgImages[2] = {};
    bgImages[0].id = 0;
    bgImages[1].id = 1;

    PolygonType1 polygon1 = {};
    polygon1.multi.count = 2;
    polygon1.multi.list = bgImages;

    activeCam.camDataIdx = 0;
    BgImage* loaded = func_80096A74(&polygon1, play);
    RBCG_ASSERT(loaded == &bgImages[1], "(b): func_80096A74 did not return the matching BgImage for camId >= 0");
    RBCG_ASSERT(((u16)player.actor.params & 0xFF) == 1,
                "(b): func_80096A74 did not stamp the resolved camId onto player->actor.params");
    RBCG_ASSERT(((u16)player.actor.params & 0xFF00) == 0xAA00,
                "(b): func_80096A74 clobbered the high byte of player->actor.params");

    return 0;
}

} // namespace

extern "C" int OoT_RoomBgCamGuard_RunHeadless(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const GameId prevGame = Context_GetCurrentGame();
    PlayState* const prevPlay = OoT_gPlayState;
    Context_SetCurrentGame(GAME_OOT);

    int rc = 1;
    PlayState* play = (PlayState*)calloc(1, sizeof(PlayState));
    if (play == NULL) {
        printf("[TEST] FAIL: could not allocate a PlayState (%s:%d)\n", __FILE__, __LINE__);
    } else {
        OoT_gPlayState = play;
        rc = RunLegs(play);
    }

    OoT_gPlayState = prevPlay;
    free(play);
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] oot-room-bg-cam-guard: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
