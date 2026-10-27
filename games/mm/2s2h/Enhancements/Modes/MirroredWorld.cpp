/**
 * @file MirroredWorld.cpp
 * @brief SoH's Mirrored World row drives Majora's Mask (#1236, #1240; lane W6-MODES-DIFF). A single-exe TU, carved
 *        into games/mm/CMakeLists.txt's ship__Enh_registrars list; 2Ship has no file of this name.
 *
 * MM mirrors the world, the camera and its X inputs while gModes.MirroredWorld.State is set (z_view.c, the
 * GameInteractor's inversion, z_kankyo.c, z_map_disp.c, synthesis.c and some twenty more read sites). In 2Ship the only
 * writer was BenMenu's "Mirrored World" checkbox (gModes.MirroredWorld.Mode), whose callback set or cleared .State;
 * BenMenu is not compiled into the single exe, so nothing set it and the mode never reached MM.
 *
 * Under one-game semantics the combo has one Mirrored World control, SoH's own row on Enhancements > Extra Modes
 * (gEnhancements.MirroredWorldMode). Its nine modes are decided per scene, so this TU is the translation
 * (RSBS::kTranslatedKeys, src/common/cvar_shared_keys.h): it reads SoH's mode and sets MM's .State when the row
 * changes (SoH's menu reaches MM's registrar through MM_ShipInit_OnCVarChanged) and on each MM scene change
 * (OnSceneInit, dispatched from Play_Init through MM_GameHooks_ExecuteOnSceneInit). It is the only MM writer of .State
 * and nothing writes .Mode (CVarClassification leg 3f-3b).
 *
 *  - Off and Always map exactly.
 *  - Random: a fresh coin per MM scene change, from this TU's own generator (MM's and OoT's game RNGs are untouched).
 *  - Random (Seeded): a function of the MM scene and the file, as SoH's is of the OoT scene and the file: the rando
 *    seed in a randomizer file, else the file's creation stamp.
 *  - Dungeons, Dungeons (Vanilla): MM's four temples and their lairs. Every MM dungeon is vanilla.
 *  - Dungeons (MQ): never mirrors in MM, which has no Master Quest dungeons. This is the one SoH mode with no MM
 *    meaning; SoH's tooltip ("Mirror the world in MQ Dungeons") already says so.
 *  - Dungeons Random, Dungeons Random (Seeded): the dungeon set above, with Random's or Random (Seeded)'s coin.
 * Outside an MM scene (the row changed while Ocarina of Time runs, or before MM's first scene) a per-scene mode
 * clears .State; MM's next scene change decides it.
 */

#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <cstdint>
#include <random>

extern "C" {
#include "variables.h"
}

#ifdef RSBS_SINGLE_EXECUTABLE
#include "context.h" // src/common: Context_GetCurrentGame
#endif

namespace {

// SoH's MirroredWorldMode values (games/oot/soh/Enhancements/enhancementTypes.h, which MM cannot include).
enum class SohMirrorMode : int32_t {
    Off = 0,
    Always = 1,
    Random = 2,
    RandomSeeded = 3,
    DungeonsAll = 4,
    DungeonsVanilla = 5,
    DungeonsMq = 6,
    DungeonsRandom = 7,
    DungeonsRandomSeeded = 8,
};

SohMirrorMode MmMirror_SohMode() {
    return static_cast<SohMirrorMode>(CVarGetInteger("gEnhancements.MirroredWorldMode", 0));
}

bool MmMirror_IsDungeon(int32_t sceneId) {
    switch (sceneId) {
        case SCENE_MITURIN:    // Woodfall Temple
        case SCENE_MITURIN_BS: // Odolwa's Lair
        case SCENE_HAKUGIN:    // Snowhead Temple
        case SCENE_HAKUGIN_BS: // Goht's Lair
        case SCENE_SEA:        // Great Bay Temple
        case SCENE_SEA_BS:     // Gyorg's Lair
        case SCENE_INISIE_N:   // Stone Tower Temple
        case SCENE_INISIE_R:   // Inverted Stone Tower Temple
        case SCENE_INISIE_BS:  // Twinmold's Lair
            return true;
        default:
            return false;
    }
}

bool MmMirror_UnseededCoin() {
    static std::mt19937 sGenerator{ std::random_device{}() };
    return std::uniform_int_distribution<int>(0, 1)(sGenerator) == 1;
}

/** SoH seeds its coin with scene + (rando seed or file creation stamp); this is the same input, mixed (splitmix64). */
bool MmMirror_SeededCoin(int32_t sceneId) {
    const bool isRando = gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO;
    uint64_t x = (isRando ? (uint64_t)gSaveContext.save.shipSaveInfo.rando.finalSeed
                          : gSaveContext.save.shipSaveInfo.fileCreatedAt) +
                 (uint64_t)(uint32_t)sceneId;
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return (x & 1) != 0;
}

/** Should MM's scene @p sceneId be mirrored under SoH's @p mode? @p sceneId < 0: no MM scene is running. */
bool MmMirror_ShouldMirror(SohMirrorMode mode, int32_t sceneId) {
    if (mode == SohMirrorMode::Always) {
        return true;
    }
    if (sceneId < 0) {
        return false;
    }
    switch (mode) {
        case SohMirrorMode::Random:
            return MmMirror_UnseededCoin();
        case SohMirrorMode::RandomSeeded:
            return MmMirror_SeededCoin(sceneId);
        case SohMirrorMode::DungeonsAll:
        case SohMirrorMode::DungeonsVanilla:
            return MmMirror_IsDungeon(sceneId);
        case SohMirrorMode::DungeonsRandom:
            return MmMirror_IsDungeon(sceneId) && MmMirror_UnseededCoin();
        case SohMirrorMode::DungeonsRandomSeeded:
            return MmMirror_IsDungeon(sceneId) && MmMirror_SeededCoin(sceneId);
        case SohMirrorMode::DungeonsMq: // MM has no Master Quest dungeons
        default:
            return false;
    }
}

void MmMirror_Apply(int32_t sceneId) {
    if (MmMirror_ShouldMirror(MmMirror_SohMode(), sceneId)) {
        CVarSetInteger("gModes.MirroredWorld.State", 1);
    } else {
        CVarClear("gModes.MirroredWorld.State");
    }
}

/** The MM scene running now, or -1 (no MM play state, or Ocarina of Time runs). */
int32_t MmMirror_CurrentScene() {
#ifdef RSBS_SINGLE_EXECUTABLE
    if (Context_GetCurrentGame() != GAME_MM) {
        return -1;
    }
#endif
    if (MM_gPlayState == nullptr || (GameState*)MM_gPlayState != MM_gGameState) {
        return -1;
    }
    return MM_gPlayState->sceneId;
}

void MmMirror_RegisterTranslation() {
    MmMirror_Apply(MmMirror_CurrentScene());
    COND_HOOK(OnSceneInit, MmMirror_SohMode() != SohMirrorMode::Off,
              [](s8 sceneId, s8 spawnNum) { MmMirror_Apply(sceneId); });
}

RegisterShipInitFunc sMmMirrorInit(MmMirror_RegisterTranslation, { "gEnhancements.MirroredWorldMode" });

} // namespace
