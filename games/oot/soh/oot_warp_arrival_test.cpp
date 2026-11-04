/**
 * @file oot_warp_arrival_test.cpp
 * Lock for #1169 XW3: a cross-game warp's arrival in OoT goes through the seed's
 * entrance override, and nothing else's does. Rando-tier row `rando-warp-arrival`
 * (CTest RandoWarpArrival), dispatch in src/common/test_runner.cpp, same bring-up
 * as master-sword-respawn (OTRGlobals' randomizer context).
 *
 * WHAT IT LOCKS. A warp into OoT (an OoT warp song played in Majora's Mask, lane
 * XW7) targets the song's VANILLA pad: the one-shot warp target carries entrance
 * ids only (src/common/entrance.h). OoT's arrival (OoT_Play_Init) writes that id
 * raw and zeroes respawnFlag, so with warp songs shuffled the pad has to go
 * through the seed's override afterwards, as SoH's own warp songs do through
 * Entrance_SetWarpSongEntrance. OoT_Play_ApplyWarpArrival (z_play.c) is what
 * OoT_Play_Init runs, after that neutralisation; it calls
 * Entrance_OverrideWarpArrival (randomizer_entrance.c), the override tail of
 * Entrance_OverrideNextIndex without its crossing check, then the forced grotto
 * return.
 *
 * THE LEGS, each through OoT_Play_ApplyWarpArrival with a randomizer save whose
 * fileNum is the 0xFF sentinel (the entrance-discovery save is a no-op there),
 * respawnFlag already zeroed as the arrival leaves it, and OoT_gPlayState NULL
 * as it is at the arrival (the suspend nulled it; Play_Init sets it later). The
 * seed's overrides are written into the randomizer context's override table and
 * installed by the real Entrance_Init.
 *
 *   W1 BEFORE Entrance_Init (the table not filled for this file): a warp
 *      arrival on Minuet's pad keeps the pad, never entrance 0 and never a stale
 *      override. The row first installs the seed and resets it as OnExitGame
 *      does: Entrance_ResetEntranceTable clears the readiness flag but leaves
 *      the override table as it was, so the table holds Kakariko for Minuet's
 *      pad (checked, the control) in any process state, and only the readiness
 *      gate keeps the pad.
 *   W2 A SHUFFLED-WARP ARRIVAL lands on the override: Minuet's pad leads to
 *      Kakariko's front gate, and no respawn is set.
 *   W3 A GROTTO-RETURN TARGET: Serenade's pad leads to the Hyrule Field tektite
 *      grotto's exit, so the arrival lands on that grotto's return entrance with
 *      respawnFlag 2 and the grotto's return point (params 0x04FF, its position),
 *      set after the neutralisation left respawnFlag 0.
 *   W3b THE FORCED GROTTO RETURN, with Decoupled and Mixed Grotto Entrances off:
 *      Grotto_OverrideSpecialEntrance's load-from-save branch already sets W3's
 *      respawnFlag, params and position, but with both settings off its
 *      Grotto_SetupReturnInfo copies the return point's temp flags through. The
 *      row leaves stale temp flags there before the arrival; only
 *      Grotto_ForceGrottoReturnOnSpecialEntrance clears them. The down respawn
 *      keeps the copied values (the control: the copy-through branch ran).
 *   W4 A DOOR ARRIVAL WITH OVERWORLD ER (the door kind, as a door, an F10 return
 *      and a refused re-entry carry) keeps its entrance although that entrance
 *      has an override row; the same entrance under a warp kind does take the
 *      row (the control, so the door leg is not vacuous).
 *   W5 THE GATES SoH's own load path uses: a vanilla save, and a randomizer save
 *      with Shuffle Entrances off, keep the pad.
 *
 * Everything the row changes is put back: the options, the override table, the
 * entrance table (Entrance_ResetEntranceTable, or a re-run Entrance_Init if it was
 * installed before), the save, OoT_gPlayState and the hook registries.
 *
 * RED ON MAIN: neither OoT_Play_ApplyWarpArrival nor Entrance_OverrideWarpArrival
 * exists there, so the row does not link; without the readiness gate W1 lands on
 * the stale Kakariko override; without Grotto_ForceGrottoReturnOnSpecialEntrance
 * W3b's return point keeps the stale temp flags; without the kind gate W4's door
 * arrival is overridden; called before the neutralisation W3's respawnFlag would
 * be zeroed (that order is z_play.c's, not this row's).
 */

#include <cstring>

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/OTRGlobals.h"

#include "entrance.h" // src/common: COMBO_SWITCH_KIND_*

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

// games/oot/src/code/z_play.c: what OoT_Play_Init runs for the arrival's kind.
extern "C" void OoT_Play_ApplyWarpArrival(s32 switchKind);
extern "C" PlayState* OoT_gPlayState;
// games/oot/soh/Enhancements/randomizer/entrance.h declares this inside the
// header's C++-only region; redeclared here so this TU stays header-light.
extern "C" EntranceOverride* Randomizer_GetEntranceOverrides();

namespace {

/** A hook type's registry set aside for a scope: the discovery the override tail
 *  makes dispatches OnRandoEntranceDiscovered. */
template <typename H> struct WarpArrivalHooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;
    WarpArrivalHooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
    ~WarpArrivalHooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

int sWarpArrivalFailures = 0;

#define WA_CHECK(cond, ...)                                             \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("[TEST] FAIL (rando-warp-arrival): ");          \
            std::printf(__VA_ARGS__);                                   \
            std::printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sWarpArrivalFailures++;                                     \
        }                                                               \
    } while (0)

// The seed under test: Minuet's pad to Kakariko's front gate, Serenade's pad to
// the Hyrule Field tektite grotto's exit, and the door's OoT arrival (outside the
// Happy Mask Shop) to Lon Lon Ranch, which no real seed does (#661 pins that door)
// and which W4 needs so its door leg has an override to ignore.
constexpr s16 kMinuetOverride = ENTR_KAKARIKO_VILLAGE_FRONT_GATE;
constexpr s16 kSerenadeOverride = ENTRANCE_GROTTO_EXIT_START + GROTTO_HF_TEKTITE_OFFSET;
constexpr s16 kDoorArrival = 0x01D1; // src/common/entrance.h OOT_ENTR_MARKET_FROM_MASK_SHOP
constexpr s16 kDoorOverride = ENTR_LON_LON_RANCH_ENTRANCE;
// randomizer_grotto.c's grottoReturnTable[GROTTO_HF_TEKTITE_OFFSET].
constexpr s16 kTektiteReturnEntrance = ENTR_HYRULE_FIELD_CENTER_EXIT;
constexpr float kTektiteReturnX = -4949.58f;
constexpr float kTektiteReturnZ = 2837.59f;
// W3b: temp flags a previous scene left on the return point.
constexpr u32 kStaleTempSwchFlags = 0x00A50000;
constexpr u32 kStaleTempCollectFlags = 0x0000005A;

static_assert(OOT_ENTR_MARKET_FROM_MASK_SHOP == kDoorArrival, "the door's OoT arrival entrance");

void WriteSeedOverrides(EntranceOverride* overrides) {
    std::memset(overrides, 0, sizeof(EntranceOverride) * ENTRANCE_OVERRIDES_MAX_COUNT);
    overrides[0] = { 0, ENTR_SACRED_FOREST_MEADOW_WARP_PAD, 0, kMinuetOverride, 0 };
    overrides[1] = { 0, ENTR_LAKE_HYLIA_WARP_PAD, 0, kSerenadeOverride, 0 };
    overrides[2] = { 0, kDoorArrival, 0, kDoorOverride, 0 };
}

/** The save as OoT_Play_Init leaves it at the arrival block: the frozen half
 *  restored, @p entrance written raw, respawnFlag zeroed. */
void ArmArrival(bool rando, s16 entrance) {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.ship.quest.id = rando ? QUEST_RANDOMIZER : QUEST_NORMAL;
    gSaveContext.fileNum = 0xFF; // the entrance-discovery Save_SaveSection is a no-op
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gSaveContext.entranceIndex = entrance;
    gSaveContext.respawnFlag = 0;
    gSaveContext.nextTransitionType = TRANS_TYPE_INSTANT;
}

s16 Arrive(bool rando, s16 entrance, s32 kind) {
    ArmArrival(rando, entrance);
    OoT_Play_ApplyWarpArrival(kind);
    return (s16)gSaveContext.entranceIndex;
}

void RunLegs(Rando::Context& ctx) {
    ctx.GetOption(RSK_SHUFFLE_ENTRANCES).Set(RO_GENERIC_ON);
    ctx.GetOption(RSK_SHUFFLE_WARP_SONGS).Set(RO_GENERIC_ON);
    ctx.GetOption(RSK_SHUFFLE_OVERWORLD_ENTRANCES).Set(RO_GENERIC_ON);
    ctx.GetOption(RSK_DECOUPLED_ENTRANCES).Set(RO_GENERIC_ON);
    ctx.GetOption(RSK_MIX_GROTTO_ENTRANCES).Set(RO_GENERIC_OFF);
    WriteSeedOverrides(Randomizer_GetEntranceOverrides());

    // W1. Before Entrance_Init: the vanilla pad, never entrance 0 or a stale
    // override. Installing the seed and resetting it as OnExitGame does leaves
    // the override table stale (Reset restores the entrance table and clears the
    // readiness flag, nothing else), so the readiness gate is all that keeps the
    // pad, whatever ran earlier in this process.
    Entrance_Init();
    Entrance_ResetEntranceTable();
    const s16 ungated = Entrance_GetOverride(ENTR_SACRED_FOREST_MEADOW_WARP_PAD);
    const s16 early = Arrive(true, ENTR_SACRED_FOREST_MEADOW_WARP_PAD, COMBO_SWITCH_KIND_WARP_SONG);
    std::printf("[TEST] rando-warp-arrival W1: before Entrance_Init (ready %d) Minuet's pad 0x%04X lands on 0x%04X; "
                "the ungated override reads 0x%04X\n",
                (int)Entrance_WarpArrivalOverridesReady(), (unsigned)ENTR_SACRED_FOREST_MEADOW_WARP_PAD,
                (unsigned)(u16)early, (unsigned)(u16)ungated);
    WA_CHECK(!Entrance_WarpArrivalOverridesReady(), "the override table must read not ready before Entrance_Init\n");
    WA_CHECK(ungated == kMinuetOverride,
             "control: Entrance_ResetEntranceTable must leave the previous file's override in the table\n");
    WA_CHECK(early == ENTR_SACRED_FOREST_MEADOW_WARP_PAD && early != 0,
             "an arrival before Entrance_Init must keep the vanilla pad, never entrance 0 or a stale override\n");

    Entrance_Init();
    WA_CHECK(Entrance_WarpArrivalOverridesReady(), "Entrance_Init must mark the override table ready\n");

    // W2. A shuffled-warp arrival lands on the override.
    const s16 minuet = Arrive(true, ENTR_SACRED_FOREST_MEADOW_WARP_PAD, COMBO_SWITCH_KIND_WARP_SONG);
    std::printf("[TEST] rando-warp-arrival W2: Minuet's pad lands on 0x%04X (override 0x%04X), respawnFlag %d\n",
                (unsigned)(u16)minuet, (unsigned)(u16)kMinuetOverride, (int)gSaveContext.respawnFlag);
    WA_CHECK(minuet == kMinuetOverride, "a warp arrival on a shuffled pad must land on the seed's override\n");
    WA_CHECK(gSaveContext.respawnFlag == 0, "an ordinary override must set no respawn\n");

    // W3. A grotto-return target sets the respawn after the neutralisation.
    const s16 serenade = Arrive(true, ENTR_LAKE_HYLIA_WARP_PAD, COMBO_SWITCH_KIND_WARP_SONG);
    const RespawnData& ret = gSaveContext.respawn[RESPAWN_MODE_RETURN];
    std::printf("[TEST] rando-warp-arrival W3: Serenade's pad lands on 0x%04X, respawnFlag %d, return point 0x%04X "
                "params 0x%04X (%.2f, %.2f), next transition %d\n",
                (unsigned)(u16)serenade, (int)gSaveContext.respawnFlag, (unsigned)(u16)ret.entranceIndex,
                (unsigned)(u16)ret.playerParams, ret.pos.x, ret.pos.z, (int)gSaveContext.nextTransitionType);
    WA_CHECK(serenade == kTektiteReturnEntrance,
             "a warp whose override is a grotto exit must land on that grotto's return entrance\n");
    WA_CHECK(gSaveContext.respawnFlag == 2,
             "a grotto-return target must set respawnFlag 2 after the arrival's neutralisation zeroed it\n");
    WA_CHECK(ret.entranceIndex == kTektiteReturnEntrance && ret.playerParams == 0x04FF &&
                 std::fabs(ret.pos.x - kTektiteReturnX) < 0.01f && std::fabs(ret.pos.z - kTektiteReturnZ) < 0.01f,
             "the return respawn must be the tektite grotto's return point\n");
    WA_CHECK(ret.tempSwchFlags == 0 && ret.tempCollectFlags == 0, "the forced grotto return must clear temp flags\n");

    // W3b. With Decoupled and Mixed Grotto Entrances off, Grotto_SetupReturnInfo
    // copies the return point's temp flags through, so only the forced grotto
    // return clears them; the down respawn keeps the copy (the control).
    ctx.GetOption(RSK_DECOUPLED_ENTRANCES).Set(RO_GENERIC_OFF);
    ArmArrival(true, ENTR_LAKE_HYLIA_WARP_PAD);
    gSaveContext.respawn[RESPAWN_MODE_RETURN].tempSwchFlags = kStaleTempSwchFlags;
    gSaveContext.respawn[RESPAWN_MODE_RETURN].tempCollectFlags = kStaleTempCollectFlags;
    OoT_Play_ApplyWarpArrival(COMBO_SWITCH_KIND_WARP_SONG);
    const s16 coupled = (s16)gSaveContext.entranceIndex;
    const RespawnData& down = gSaveContext.respawn[RESPAWN_MODE_DOWN];
    std::printf("[TEST] rando-warp-arrival W3b: coupled Serenade's pad lands on 0x%04X, respawnFlag %d, return temp "
                "flags 0x%08X/0x%08X, down temp flags 0x%08X/0x%08X\n",
                (unsigned)(u16)coupled, (int)gSaveContext.respawnFlag, (unsigned)ret.tempSwchFlags,
                (unsigned)ret.tempCollectFlags, (unsigned)down.tempSwchFlags, (unsigned)down.tempCollectFlags);
    WA_CHECK(coupled == kTektiteReturnEntrance && gSaveContext.respawnFlag == 2,
             "a coupled grotto-return target must still land on the grotto's return with respawnFlag 2\n");
    WA_CHECK(down.tempSwchFlags == kStaleTempSwchFlags && down.tempCollectFlags == kStaleTempCollectFlags,
             "control: with Decoupled and Mixed Grotto Entrances off the return point's temp flags are copied "
             "through\n");
    WA_CHECK(ret.tempSwchFlags == 0 && ret.tempCollectFlags == 0,
             "the forced grotto return must clear the return point's stale temp flags\n");
    ctx.GetOption(RSK_DECOUPLED_ENTRANCES).Set(RO_GENERIC_ON);

    // W4. A door arrival with overworld ER keeps its entrance; a warp kind on the
    // same entrance takes the row (the control).
    const s16 door = Arrive(true, kDoorArrival, COMBO_SWITCH_KIND_DOOR);
    const bool doorDiscovered = Entrance_GetIsEntranceDiscovered(kDoorArrival) != 0;
    const s16 doorAsWarp = Arrive(true, kDoorArrival, COMBO_SWITCH_KIND_WARP_SONG);
    std::printf("[TEST] rando-warp-arrival W4: the door arrival 0x%04X stays 0x%04X (discovered %d); as a warp it "
                "would land on 0x%04X\n",
                (unsigned)(u16)kDoorArrival, (unsigned)(u16)door, doorDiscovered ? 1 : 0, (unsigned)(u16)doorAsWarp);
    WA_CHECK(door == kDoorArrival, "a door arrival must not run the override tail, even with overworld ER on\n");
    WA_CHECK(!doorDiscovered, "a door arrival must not mark its entrance discovered\n");
    WA_CHECK(doorAsWarp == kDoorOverride, "control: the door's entrance has a live override row\n");

    // W5. The gates SoH's own load path uses.
    const s16 vanilla = Arrive(false, ENTR_SACRED_FOREST_MEADOW_WARP_PAD, COMBO_SWITCH_KIND_WARP_SONG);
    WA_CHECK(vanilla == ENTR_SACRED_FOREST_MEADOW_WARP_PAD, "a vanilla save's warp arrival must keep the pad\n");
    ctx.GetOption(RSK_SHUFFLE_ENTRANCES).Set(RO_GENERIC_OFF);
    const s16 noShuffle = Arrive(true, ENTR_SACRED_FOREST_MEADOW_WARP_PAD, COMBO_SWITCH_KIND_WARP_SONG);
    WA_CHECK(noShuffle == ENTR_SACRED_FOREST_MEADOW_WARP_PAD,
             "with Shuffle Entrances off a warp arrival must keep the pad\n");
    std::printf("[TEST] rando-warp-arrival W5: vanilla save 0x%04X, Shuffle Entrances off 0x%04X\n",
                (unsigned)(u16)vanilla, (unsigned)(u16)noShuffle);
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_WarpArrivalTest_Run(void) {
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        std::printf("[TEST] FAIL: rando-warp-arrival needs OTRGlobals' randomizer context\n");
        return 1;
    }
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const std::shared_ptr<Rando::Context> ctx = OTRGlobals::Instance->gRandoContext;
    const RandomizerSettingKey kKeys[] = { RSK_SHUFFLE_ENTRANCES, RSK_SHUFFLE_WARP_SONGS,
                                           RSK_SHUFFLE_OVERWORLD_ENTRANCES, RSK_DECOUPLED_ENTRANCES,
                                           RSK_MIX_GROTTO_ENTRANCES };
    uint8_t priorOptions[sizeof(kKeys) / sizeof(kKeys[0])];
    for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); i++) {
        priorOptions[i] = ctx->GetOption(kKeys[i]).Get();
    }
    EntranceOverride* overrides = Randomizer_GetEntranceOverrides();
    std::vector<EntranceOverride> priorOverrides(overrides, overrides + ENTRANCE_OVERRIDES_MAX_COUNT);
    const bool priorReady = Entrance_WarpArrivalOverridesReady() != 0;
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));
    PlayState* const priorPlay = OoT_gPlayState;

    sWarpArrivalFailures = 0;
    OoT_gPlayState = nullptr;
    {
        WarpArrivalHooksAside<GameInteractor::OnRandoEntranceDiscovered> discovered;
        RunLegs(*ctx);
    }

    for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); i++) {
        ctx->GetOption(kKeys[i]).Set(priorOptions[i]);
    }
    std::memcpy(overrides, priorOverrides.data(), sizeof(EntranceOverride) * ENTRANCE_OVERRIDES_MAX_COUNT);
    Entrance_ResetEntranceTable();
    if (priorReady) {
        Entrance_Init();
    }
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    OoT_gPlayState = priorPlay;

    if (sWarpArrivalFailures != 0) {
        std::printf("[TEST] FAIL (rando-warp-arrival): %d failure(s)\n", sWarpArrivalFailures);
        return 1;
    }
    std::printf("[TEST] PASS: rando-warp-arrival\n");
    return 0;
}
