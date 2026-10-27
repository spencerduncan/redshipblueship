/**
 * @file oot_master_sword_respawn_test.cpp
 * Lock for #1218: under Shuffle Master Sword, a respawn out of the Ganon's Tower
 * collapse or Ganon's arena must not hand over the Master Sword the seed placed
 * elsewhere. Rando-tier row `master-sword-respawn` (CTest MasterSwordRespawn),
 * dispatch in src/common/test_runner.cpp, same bring-up as bombchu-drop-rule
 * (OTRGlobals' randomizer context).
 *
 * WHAT WAS BROKEN. Play_LoadToLastEntrance (games/oot/src/code/z_play.c), the
 * tail of Play_TriggerRespawn, redirects any respawn from the four collapse
 * scenes to ENTR_GANONS_TOWER_COLLAPSE_EXTERIOR_0 and calls
 * OoT_Item_Give(ITEM_SWORD_MASTER) with no randomizer test. Every other vanilla
 * Master Sword grant is gated under Shuffle Master Sword (the pedestal's
 * VB_GIVE_ITEM_MASTER_SWORD, the load-time restore in z_sram.c), and
 * BossGanon2_Init makes the swordless Ganon fight unwinnable on purpose. Dying
 * and continuing, letting the escape timer run out (ovl_En_Eg) or voiding out
 * gave the sword anyway, and Ganon was then beatable without the seed's sword.
 *
 * THE INVARIANTS, each through the REAL Play_LoadToLastEntrance with a zeroed
 * PlayState (OoT_Item_Give touches play only through Interface_LoadItemIcon1,
 * whose DMA is a no-op in the port):
 *
 *   1. Randomizer save, Shuffle Master Sword on, sword not found: in each of the
 *      four collapse scenes the respawn still leaves through
 *      ENTR_GANONS_TOWER_COLLAPSE_EXTERIOR_0 (non-vacuity: the give's branch ran)
 *      and the Master Sword is neither owned nor on B afterwards.
 *   2. Shuffle Master Sword on, sword already found: the respawn still puts the
 *      Master Sword back on B, as vanilla does (Ganon knocks it out of Link's
 *      hand without touching the save).
 *   3. Randomizer save with Shuffle Master Sword off, and a vanilla save: the
 *      respawn gives the sword exactly as before.
 *   4. Outside the collapse scenes nothing is given and the respawn goes to the
 *      last entrance.
 *
 * The randomizer option, the hook registries the give dispatches and the whole
 * save are put back before returning.
 */

#include <cstring>

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/OTRGlobals.h"

#include <cstdio>
#include <memory>

// The respawn site under test (games/oot/src/code/z_play.c).
extern "C" void Play_LoadToLastEntrance(PlayState* play);

namespace {

/** A hook type's registry set aside for a scope. OoT_Item_Give dispatches
 *  OnItemReceive and OnTimestamp, and the bring-up (or an earlier row inside
 *  AllTests) can leave gameplay hooks registered that read a live play state
 *  this row does not have. */
template <typename H> struct HooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;
    HooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
    ~HooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

// File-static: PlayState and SoH's runtime SaveContext are both large.
PlayState sPlay;

constexpr s16 kLastEntrance = ENTR_KOKIRI_FOREST_0;

/** An adult Link on a fresh save of the given quest, holding no Master Sword
 *  (B empty) unless @p swordFound, standing in @p scene. */
void Arm(bool rando, bool swordFound, s16 scene) {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    std::memset(gSaveContext.inventory.items, ITEM_NONE, sizeof(gSaveContext.inventory.items));
    gSaveContext.ship.quest.id = rando ? QUEST_RANDOMIZER : QUEST_NORMAL;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    gSaveContext.healthCapacity = 0x30;
    gSaveContext.health = 0x30;
    gSaveContext.entranceIndex = kLastEntrance;
    gSaveContext.equips.buttonItems[0] = ITEM_NONE;
    if (swordFound) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER);
    }
    std::memset(&sPlay, 0, sizeof(PlayState));
    sPlay.sceneNum = scene;
}

bool SwordOwned(void) {
    return CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER) != 0;
}

bool SwordOnB(void) {
    return gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER;
}

int sFailures = 0;

#define MSR_CHECK(cond, ...)                                            \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("[TEST] FAIL (master-sword-respawn): ");        \
            std::printf(__VA_ARGS__);                                   \
            std::printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sFailures++;                                                \
        }                                                               \
    } while (0)

struct CollapseScene {
    s16 scene;
    const char* name;
};

const CollapseScene kCollapseScenes[] = {
    { SCENE_GANONS_TOWER_COLLAPSE_INTERIOR, "Ganon's Tower collapse (interior)" },
    { SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR, "Ganon's Tower collapse (exterior)" },
    { SCENE_INSIDE_GANONS_CASTLE_COLLAPSE, "Inside Ganon's Castle collapse" },
    { SCENE_GANON_BOSS, "Ganon's arena" },
};

void RunLegs(Rando::Context& ctx) {
    // 1. #1218: Shuffle Master Sword on, the sword not found.
    ctx.GetOption(RSK_SHUFFLE_MASTER_SWORD).Set(RO_GENERIC_ON);
    for (const auto& s : kCollapseScenes) {
        Arm(true, false, s.scene);
        Play_LoadToLastEntrance(&sPlay);
        std::printf("[TEST] master-sword-respawn: shuffled and unfound, respawn from %s: owned=%d onB=%d next=0x%X\n",
                    s.name, SwordOwned() ? 1 : 0, SwordOnB() ? 1 : 0, (unsigned)(u16)sPlay.nextEntranceIndex);
        MSR_CHECK(sPlay.nextEntranceIndex == ENTR_GANONS_TOWER_COLLAPSE_EXTERIOR_0,
                  "a respawn from %s must still leave through the collapse exterior entrance\n", s.name);
        MSR_CHECK(sPlay.transitionTrigger == TRANS_TRIGGER_START, "a respawn from %s must start a transition\n",
                  s.name);
        MSR_CHECK(!SwordOwned(),
                  "a respawn from %s gave the Master Sword the seed placed elsewhere (Shuffle Master Sword on)\n",
                  s.name);
        MSR_CHECK(!SwordOnB(), "a respawn from %s put the unfound Master Sword on B\n", s.name);
    }

    // 2. Shuffle Master Sword on, the sword already found: vanilla's re-equip stays.
    Arm(true, true, SCENE_GANON_BOSS);
    Play_LoadToLastEntrance(&sPlay);
    MSR_CHECK(SwordOwned() && SwordOnB(),
              "with the shuffled Master Sword already found, the respawn must put it back on B (owned=%d onB=%d)\n",
              SwordOwned() ? 1 : 0, SwordOnB() ? 1 : 0);

    // 3a. Randomizer save with Shuffle Master Sword off: unchanged vanilla give.
    ctx.GetOption(RSK_SHUFFLE_MASTER_SWORD).Set(RO_GENERIC_OFF);
    Arm(true, false, SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR);
    Play_LoadToLastEntrance(&sPlay);
    MSR_CHECK(SwordOwned() && SwordOnB(),
              "with Shuffle Master Sword off the respawn must give the Master Sword (owned=%d onB=%d)\n",
              SwordOwned() ? 1 : 0, SwordOnB() ? 1 : 0);

    // 3b. A vanilla save, the option left on to prove it is not read there.
    ctx.GetOption(RSK_SHUFFLE_MASTER_SWORD).Set(RO_GENERIC_ON);
    Arm(false, false, SCENE_GANONS_TOWER_COLLAPSE_INTERIOR);
    Play_LoadToLastEntrance(&sPlay);
    MSR_CHECK(SwordOwned() && SwordOnB(), "a vanilla save's respawn must give the Master Sword (owned=%d onB=%d)\n",
              SwordOwned() ? 1 : 0, SwordOnB() ? 1 : 0);

    // 4. Outside the collapse: no give, back to the last entrance.
    Arm(false, false, SCENE_HYRULE_FIELD);
    Play_LoadToLastEntrance(&sPlay);
    MSR_CHECK(!SwordOwned() && !SwordOnB(), "a respawn outside the collapse must give nothing\n");
    MSR_CHECK(sPlay.nextEntranceIndex == kLastEntrance,
              "a respawn outside the collapse must go to the last entrance\n");
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_MasterSwordRespawnTest_Run(void) {
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        std::printf("[TEST] FAIL: master-sword-respawn needs OTRGlobals' randomizer context\n");
        return 1;
    }
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const std::shared_ptr<Rando::Context> ctx = OTRGlobals::Instance->gRandoContext;
    const uint8_t priorShuffle = ctx->GetOption(RSK_SHUFFLE_MASTER_SWORD).Get();
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));

    sFailures = 0;
    {
        HooksAside<GameInteractor::OnItemReceive> receive;
        HooksAside<GameInteractor::OnTimestamp> timestamp;
        RunLegs(*ctx);
    }

    ctx->GetOption(RSK_SHUFFLE_MASTER_SWORD).Set(priorShuffle);
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));

    if (sFailures != 0) {
        std::printf("[TEST] FAIL (master-sword-respawn): %d failure(s)\n", sFailures);
        return 1;
    }
    std::printf("[TEST] PASS: master-sword-respawn\n");
    return 0;
}
