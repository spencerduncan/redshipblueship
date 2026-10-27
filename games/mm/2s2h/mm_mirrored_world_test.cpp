/**
 * @file mm_mirrored_world_test.cpp
 * ROM-free, display-free lock for Mirrored World in Majora's Mask (#1236, #1240; lane W6-MODES-DIFF).
 * CTest row MMMirroredWorld in CMake/SingleExecutable.cmake, dispatch "mm-mirrored-world" in
 * src/common/test_runner.cpp.
 *
 * What was broken: MM mirrors the world, the camera and its X inputs while gModes.MirroredWorld.State is set
 * (z_view.c, the GameInteractor's inversion, z_kankyo.c, z_map_disp.c, synthesis.c and some twenty more read sites),
 * and the only thing that ever set it was 2Ship's BenMenu callback, which the single exe does not compile. SoH's own
 * Mirrored World row (gEnhancements.MirroredWorldMode, Enhancements > Extra Modes) is the one control under one-game
 * semantics, so its translator (games/mm/2s2h/Enhancements/Modes/MirroredWorld.cpp, RSBS::kTranslatedKeys) sets
 * .State: when SoH's row changes, and on each MM scene change.
 *
 * The row drives the translator only through the paths the game uses: MM's ShipInit registrar for SoH's key (the
 * menu reaches it through MM_ShipInit_OnCVarChanged) and MM's OnSceneInit dispatch (MM_GameHooks_ExecuteOnSceneInit,
 * called from Play_Init). Without the translator neither path sets .State, so the first check is red.
 *  1. Always: the row's change sets .State, and an MM scene change keeps it set.
 *  2. Off: the row's change clears a .State left set (a config saved by an earlier session), and a scene change
 *     leaves it clear.
 *  3. Dungeons: mirrored in Woodfall Temple and Odolwa's Lair, not in Termina Field.
 *  4. Dungeons (MQ): never mirrored in MM, which has no Master Quest dungeons.
 *  5. Random (Seeded): each MM scene's answer is a function of the scene and the file, and both answers occur.
 * Every key the row sets is cleared, and gSaveContext's two identity fields are restored.
 */

#include "global.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/ShipInit.hpp"

#include <cstdio>

extern "C" void MM_GameHooks_ExecuteOnSceneInit(s16 sceneId, s8 spawnNum);

namespace {

constexpr const char* kSohModeKey = "gEnhancements.MirroredWorldMode";
constexpr const char* kStateKey = "gModes.MirroredWorld.State";

// SoH's MirroredWorldMode values (games/oot/soh/Enhancements/enhancementTypes.h).
constexpr int32_t kModeOff = 0;
constexpr int32_t kModeAlways = 1;
constexpr int32_t kModeRandomSeeded = 3;
constexpr int32_t kModeDungeons = 4;
constexpr int32_t kModeDungeonsMq = 6;

#define MIRROR_ASSERT(cond, code, msg)                                                  \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            Cleanup();                                                                  \
            return (code);                                                              \
        }                                                                               \
    } while (0)

/** SoH's row changes: the value is written and the registrars keyed on it run, as MM_ShipInit_OnCVarChanged does. */
void SetSohMode(int32_t mode) {
    CVarSetInteger(kSohModeKey, mode);
    ShipInit::Init(kSohModeKey);
}

int State() {
    return CVarGetInteger(kStateKey, 0);
}

void SceneChange(s16 sceneId) {
    MM_GameHooks_ExecuteOnSceneInit(sceneId, 0);
}

uint64_t sSavedCreatedAt = 0;
SaveType sSavedSaveType = SAVETYPE_VANILLA;

void Cleanup() {
    CVarClear(kSohModeKey);
    ShipInit::Init(kSohModeKey);
    CVarClear(kStateKey);
    gSaveContext.save.shipSaveInfo.fileCreatedAt = sSavedCreatedAt;
    gSaveContext.save.shipSaveInfo.saveType = sSavedSaveType;
}

} // namespace

extern "C" int MM_MirroredWorld_RunHeadless(void) {
    printf("[TEST] mm-mirrored-world: SoH's Mirrored World row sets the key Majora's Mask acts on (#1236)\n");
    sSavedCreatedAt = gSaveContext.save.shipSaveInfo.fileCreatedAt;
    sSavedSaveType = gSaveContext.save.shipSaveInfo.saveType;
    CVarClear(kStateKey);

    // ---------------------------------------------------------------- 1
    SetSohMode(kModeAlways);
    MIRROR_ASSERT(State() == 1, 1,
                  "SoH's Mirrored World set to Always did not set gModes.MirroredWorld.State, so Majora's Mask "
                  "stays unmirrored (nothing in the single exe writes the key MM acts on)");
    SceneChange(SCENE_00KEIKOKU);
    MIRROR_ASSERT(State() == 1, 1, "an MM scene change under Always cleared gModes.MirroredWorld.State");

    // ---------------------------------------------------------------- 2
    SetSohMode(kModeOff);
    MIRROR_ASSERT(State() == 0, 2, "SoH's Mirrored World set to Off left gModes.MirroredWorld.State set");
    CVarSetInteger(kStateKey, 1); // as an earlier session's config could hold it
    SetSohMode(kModeOff);
    MIRROR_ASSERT(State() == 0, 2, "Off did not clear a gModes.MirroredWorld.State left set by an earlier session");
    SceneChange(SCENE_MITURIN);
    MIRROR_ASSERT(State() == 0, 2, "an MM scene change under Off set gModes.MirroredWorld.State");

    // ---------------------------------------------------------------- 3
    SetSohMode(kModeDungeons);
    SceneChange(SCENE_MITURIN);
    MIRROR_ASSERT(State() == 1, 3, "Dungeons did not mirror Woodfall Temple");
    SceneChange(SCENE_00KEIKOKU);
    MIRROR_ASSERT(State() == 0, 3, "Dungeons mirrored Termina Field");
    SceneChange(SCENE_MITURIN_BS);
    MIRROR_ASSERT(State() == 1, 3, "Dungeons did not mirror Odolwa's Lair");

    // ---------------------------------------------------------------- 4
    SetSohMode(kModeDungeonsMq);
    SceneChange(SCENE_MITURIN);
    MIRROR_ASSERT(State() == 0, 4, "Dungeons (MQ) mirrored an MM temple; MM has no Master Quest dungeons");

    // ---------------------------------------------------------------- 5
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_VANILLA;
    gSaveContext.save.shipSaveInfo.fileCreatedAt = 0x5253425357364D44ULL; // a fixed file identity
    SetSohMode(kModeRandomSeeded);
    int mirrored = 0;
    int plain = 0;
    for (s16 scene = 0; scene < SCENE_MAX; scene++) {
        SceneChange(scene);
        const int first = State();
        SceneChange(SCENE_00KEIKOKU);
        SceneChange(scene);
        MIRROR_ASSERT(State() == first, 5, "Random (Seeded) gave one MM scene two answers for one file");
        (first ? mirrored : plain)++;
    }
    MIRROR_ASSERT(mirrored > 0 && plain > 0, 5, "Random (Seeded) gave every MM scene the same answer");

    Cleanup();
    MIRROR_ASSERT(State() == 0, 6, "clearing SoH's Mirrored World left gModes.MirroredWorld.State set");

    printf("[TEST] mm-mirrored-world: PASS\n");
    return 0;
}
