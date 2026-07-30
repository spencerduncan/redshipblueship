#include "GameInteractor_Hooks.h"

#ifdef RSBS_SINGLE_EXECUTABLE
#include <cstddef> // offsetof/size_t for the #395 layout probes below
#include <cstdint>
#include <type_traits> // std::is_same_v for the #470 payload-divergence premise
#include "context.h"   // src/common/context.h via redship_common's public include dir
// In the single executable MM's own GameInteractor layer is not compiled, so
// MM code links against these unprefixed extern "C" wrappers, and MM's
// GIVanillaBehavior ordinals alias OoT's (e.g. MM VB_SETUP_TRANSITION == OoT
// VB_PLAY_RAINBOW_BRIDGE_CS == 206 — an OoT TimeSaver hook answering that call
// vetoed MM's transition setup and crashed on the NULL transitionCtx.init).
// Suppress all OoT hook dispatch while MM is the active game: MM callers get
// vanilla behavior, matching the explicit stubs in src/common/mm_stubs.c.
// GAME_NONE (boot) and GAME_OOT keep today's behavior.
#define GI_SINGLE_EXE_GATE() \
    if (Context_GetCurrentGame() == GAME_MM) { \
        return; \
    }
#define GI_SINGLE_EXE_GATE_RET(defaultValue) \
    if (Context_GetCurrentGame() == GAME_MM) { \
        return (defaultValue); \
    }
#else
#define GI_SINGLE_EXE_GATE()
#define GI_SINGLE_EXE_GATE_RET(defaultValue)
#endif

// MARK: - Gameplay

void GameInteractor_ExecuteOnZTitleInit(void* gameState) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnZTitleInit>(gameState);
}

void GameInteractor_ExecuteOnZTitleUpdate(void* gameState) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnZTitleUpdate>(gameState);
}

void GameInteractor_ExecuteOnLoadGame(int32_t fileNum) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLoadGame>(fileNum);
}

void GameInteractor_ExecuteOnExitGame(int32_t fileNum) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnExitGame>(fileNum);
}

void GameInteractor_ExecuteOnGameStateMainStart() {
    GI_SINGLE_EXE_GATE();
    // Cleanup all hooks at the start of each frame
    GameInteractor::Instance->RemoveAllQueuedHooks();

    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGameStateMainStart>();
}

void GameInteractor_ExecuteOnGameFrameUpdate() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGameFrameUpdate>();
}

void GameInteractor_ExecuteOnCameraState(PlayState* play) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCameraState>(play);
}

void GameInteractor_ExecuteOnItemReceiveHooks(GetItemEntry itemEntry) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnItemReceive>(itemEntry);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnItemReceive>(itemEntry);
}

void GameInteractor_ExecuteOnEquipmentDelete(int16_t equipmentType, uint16_t equipValue) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnEquipmentDelete>(equipmentType, equipValue);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnEquipmentDelete>(equipmentType, equipValue);
}

void GameInteractor_ExecuteOnSaleEndHooks(GetItemEntry itemEntry) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSaleEnd>(itemEntry);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSaleEnd>(itemEntry);
}

void GameInteractor_ExecuteOnTransitionEndHooks(int16_t sceneNum) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnTransitionEnd>(sceneNum);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnTransitionEnd>(sceneNum, sceneNum);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnTransitionEnd>(sceneNum);
}

void GameInteractor_ExecuteOnSceneInit(int16_t sceneNum) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneInit>(sceneNum);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnSceneInit>(sceneNum, sceneNum);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSceneInit>(sceneNum);
}

void GameInteractor_ExecuteAfterSceneCommands(int16_t sceneNum) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::AfterSceneCommands>(sceneNum);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::AfterSceneCommands>(sceneNum, sceneNum);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::AfterSceneCommands>(sceneNum);
}

void GameInteractor_ExecuteOnSceneFlagSet(int16_t sceneNum, int16_t flagType, int16_t flag) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneFlagSet>(sceneNum, flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSceneFlagSet>(sceneNum, flagType, flag);
}

void GameInteractor_ExecuteOnSceneFlagUnset(int16_t sceneNum, int16_t flagType, int16_t flag) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneFlagUnset>(sceneNum, flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnSceneFlagUnset>(sceneNum, flagType, flag);
}

void GameInteractor_ExecuteOnFlagSet(int16_t flagType, int16_t flag) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnFlagSet>(flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnFlagSet>(flagType, flag);
}

void GameInteractor_ExecuteOnFlagUnset(int16_t flagType, int16_t flag) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnFlagUnset>(flagType, flag);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnFlagUnset>(flagType, flag);
}

void GameInteractor_ExecuteOnSceneSpawnActors() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneSpawnActors>();
}

void GameInteractor_ExecuteOnLinkSkeletonInit() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLinkSkeletonInit>();
}

void GameInteractor_ExecuteOnLinkEquipmentChange() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLinkEquipmentChange>();
}

void GameInteractor_ExecuteOnPlayerUpdate() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerUpdate>();
}

void GameInteractor_ExecuteOnSetDoAction(uint16_t action) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSetDoAction>(action);
}

void GameInteractor_ExecuteOnPlayerSfx(u16 sfxId) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerSfx>(sfxId);
}

void GameInteractor_ExecuteOnOcarinaSongAction() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOcarinaSongAction>();
}

void GameInteractor_ExecuteOnOcarinaNote(uint8_t note, float modulator, int8_t bend) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOcarinaNote>(note, modulator, bend);
}

void GameInteractor_ExecuteOnCuccoOrChickenHatch() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnCuccoOrChickenHatch>();
}

void GameInteractor_ExecuteOnShopSlotChangeHooks(uint8_t cursorIndex, int16_t price) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnShopSlotChange>(cursorIndex, price);
}

void GameInteractor_ExecuteOnDungeonKeyUsedHooks(uint16_t mapIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnDungeonKeyUsed>(mapIndex);
}

bool GameInteractor_ShouldActorInit(void* actor) {
    GI_SINGLE_EXE_GATE_RET(true);
    bool result = true;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::ShouldActorInit>(actor, &result);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::ShouldActorInit>(((Actor*)actor)->id, actor, &result);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::ShouldActorInit>((uintptr_t)actor, actor, &result);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::ShouldActorInit>(actor, &result);
    return result;
}

void GameInteractor_ExecuteOnActorInit(void* actor) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorInit>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorInit>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorInit>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorInit>(actor);
}

void GameInteractor_ExecuteOnActorSpawn(void* actor) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorSpawn>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorSpawn>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorSpawn>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorSpawn>(actor);
}

bool GameInteractor_ShouldActorUpdate(void* actor) {
    GI_SINGLE_EXE_GATE_RET(true);
    bool result = true;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::ShouldActorUpdate>(actor, &result);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::ShouldActorUpdate>(((Actor*)actor)->id, actor, &result);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::ShouldActorUpdate>((uintptr_t)actor, actor, &result);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::ShouldActorUpdate>(actor, &result);
    return result;
}

void GameInteractor_ExecuteOnActorUpdate(void* actor) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorUpdate>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorUpdate>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorUpdate>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorUpdate>(actor);
}

void GameInteractor_ExecuteOnActorKill(void* actor) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorKill>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorKill>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorKill>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorKill>(actor);
}

void GameInteractor_ExecuteOnActorDestroy(void* actor) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnActorDestroy>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnActorDestroy>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnActorDestroy>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnActorDestroy>(actor);
}

void GameInteractor_ExecuteOnEnemyDefeat(void* actor) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnEnemyDefeat>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnEnemyDefeat>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnEnemyDefeat>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnEnemyDefeat>(actor);
}

void GameInteractor_ExecuteOnBossDefeat(void* actor) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnBossDefeat>(actor);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnBossDefeat>(((Actor*)actor)->id, actor);
    GameInteractor::Instance->ExecuteHooksForPtr<GameInteractor::OnBossDefeat>((uintptr_t)actor, actor);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnBossDefeat>(actor);
}

void GameInteractor_ExecuteOnTimestamp(u8 item) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnTimestamp>(item);
}

void GameInteractor_ExecuteOnPlayerBonk() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerBonk>();
}

void GameInteractor_ExecuteOnPlayerSetModels(Player* player, u8 modelGroup) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerSetModels>(player, modelGroup);
}

void GameInteractor_ExecuteOnPlayerHealthChange(int16_t amount) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerHealthChange>(amount);
}

void GameInteractor_ExecuteOnPlayerBottleUpdate(int16_t contents) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerBottleUpdate>(contents);
}

void GameInteractor_ExecuteOnPlayerHoldUpShield() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerHoldUpShield>();
}

void GameInteractor_ExecuteOnPlayerFirstPersonControl(Player* player) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerFirstPersonControl>(player);
}

void GameInteractor_ExecuteOnPlayerShieldControl(float_t* sp50, float_t* sp54) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerShieldControl>(sp50, sp54);
}

void GameInteractor_ExecuteOnPlayerProcessStick() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerProcessStick>();
}

void GameInteractor_ExecuteOnPlayDestroy() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDestroy>();
}

void GameInteractor_ExecuteOnPlayDrawBegin() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDrawBegin>();
}

void GameInteractor_ExecuteOnPlayDrawEnd() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDrawEnd>();
}

bool GameInteractor_Should(GIVanillaBehavior flag, u32 result, ...) {
    GI_SINGLE_EXE_GATE_RET(static_cast<bool>(result));
    // Only the external function can use the Variadic Function syntax
    // To pass the va args to the next caller must be done using va_list and reading the args into it
    // Because there can be N subscribers registered to each template call, the subscribers will be responsible for
    // creating a copy of this va_list to avoid incrementing the original pointer between calls
    va_list args;
    va_start(args, result);

    // Because of default argument promotion, even though our incoming "result" is just a bool, it needs to be typed as
    // an int to be permitted to be used in `va_start`, otherwise it is undefined behavior.
    // Here we downcast back to a bool for our actual hook handlers
    bool boolResult = static_cast<bool>(result);

    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnVanillaBehavior>(flag, &boolResult, args);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnVanillaBehavior>(flag, flag, &boolResult, args);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnVanillaBehavior>(flag, &boolResult, args);

    va_end(args);
    return boolResult;
}

// MARK: -  Save Files

void GameInteractor_ExecuteOnSaveFile(int32_t fileNum, int32_t sectionID) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSaveFile>(fileNum, sectionID);
}

void GameInteractor_ExecuteOnLoadFile(int32_t fileNum) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnLoadFile>(fileNum);
}

void GameInteractor_ExecuteOnDeleteFile(int32_t fileNum) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnDeleteFile>(fileNum);
}

// MARK: - Dialog

void GameInteractor_ExecuteOnDialogMessage() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnDialogMessage>();
}

void GameInteractor_ExecuteOnPresentTitleCard() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPresentTitleCard>();
}

void GameInteractor_ExecuteOnInterfaceUpdate() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnInterfaceUpdate>();
}

void GameInteractor_ExecuteOnKaleidoscopeUpdate(int16_t inDungeonScene) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoscopeUpdate>(inDungeonScene);
}

// MARK: - Main Menu

void GameInteractor_ExecuteOnPresentFileSelect() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPresentFileSelect>();
}

void GameInteractor_ExecuteOnUpdateFileSelectSelection(uint16_t optionIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileSelectSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileSelectConfirmationSelection(uint16_t optionIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileSelectConfirmationSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileCopySelection(uint16_t optionIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileCopySelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileCopyConfirmationSelection(uint16_t optionIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileCopyConfirmationSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileEraseSelection(uint16_t optionIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileEraseSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileEraseConfirmationSelection(uint16_t optionIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileEraseConfirmationSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileAudioSelection(uint8_t optionIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileAudioSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileTargetSelection(uint8_t optionIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileTargetSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileLanguageSelection(uint8_t optionIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileLanguageSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileQuestSelection(uint8_t questIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileQuestSelection>(questIndex);
}

void GameInteractor_ExecuteOnUpdateFileBossRushOptionSelection(uint8_t optionIndex, uint8_t optionValue) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileBossRushOptionSelection>(optionIndex,
                                                                                                optionValue);
}

void GameInteractor_ExecuteOnUpdateFileRandomizerOptionSelection(uint8_t optionIndex) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileRandomizerOptionSelection>(optionIndex);
}

void GameInteractor_ExecuteOnUpdateFileNameSelection(int16_t charCode) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnUpdateFileNameSelection>(charCode);
}

void GameInteractor_ExecuteOnFileChooseMain(void* gameState) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnFileChooseMain>(gameState);
}

// MARK: - Game

void GameInteractor_ExecuteOnSetGameLanguage() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSetGameLanguage>();
}

// MARK: - System

void GameInteractor_RegisterOnAssetAltChange(void (*fn)(void)) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnAssetAltChange>(fn);
}

// MARK: Pause Menu

void GameInteractor_ExecuteOnKaleidoUpdate() {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnKaleidoUpdate>();
}

// MARK: Messages
void GameInteractor_ExecuteOnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnOpenText>(textId, loadFromMessageTable);
    GameInteractor::Instance->ExecuteHooksForID<GameInteractor::OnOpenText>(*textId, textId, loadFromMessageTable);
    GameInteractor::Instance->ExecuteHooksForFilter<GameInteractor::OnOpenText>(textId, loadFromMessageTable);
}

// Mark: Audio
void GameInteractor_ExecuteOnSeqPlayerInit(int32_t playerIdx, int32_t seqId) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSeqPlayerInit>(playerIdx, seqId);
}

// MARK: - Rando
void GameInteractor_ExecuteOnRandoEntranceDiscovered(u16 entranceIndex, u8 isReversedEntrance) {
    GI_SINGLE_EXE_GATE();
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnRandoEntranceDiscovered>(entranceIndex,
                                                                                      isReversedEntrance);
}

#ifdef RSBS_SINGLE_EXECUTABLE
// MARK: - Test support (redship --test vb-affinity, src/common/test_runner.cpp)
// Arms a hook that vetoes one vanilla-behavior id so the test can prove the
// GI_SINGLE_EXE_GATE above keeps MM-active calls at vanilla behavior. The
// headless test binary never runs OTRGlobals init, so create the registry on
// demand.
static HOOK_ID sTestVBVetoHookId = 0;

extern "C" void GameInteractor_TestDisarmVBVeto(void) {
    if (GameInteractor::Instance != nullptr && sTestVBVetoHookId != 0) {
        GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnVanillaBehavior>(sTestVBVetoHookId);
    }
    sTestVBVetoHookId = 0;
}

extern "C" void GameInteractor_TestArmVBVeto(int32_t flag) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    GameInteractor_TestDisarmVBVeto();
    sTestVBVetoHookId = GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnVanillaBehavior>(
        static_cast<GIVanillaBehavior>(flag),
        [](GIVanillaBehavior _, bool* should, va_list _originalArgs) { *should = false; });
}

// MARK: - #395 GameInteractor layout probes (redship --test mm-gi-shim)
//
// OoT's class here has sizeof 4 with nextHookId at offset 0; MM's same-named
// class compiles at sizeof 104 with nextHookId at offset 96, and the linker
// keeps ONE copy of every RegisterGameHook<...> COMDAT instantiation. These
// helpers let the mm-gi-shim test measure, from a real OoT translation unit,
// which layout the surviving registration code actually writes — by running a
// registration against a caller-supplied oversized zeroed buffer and letting
// the test inspect which bytes changed. Two variants:
//
//  - Direct: a normal inlinable call, measuring what OoT's real registration
//    call sites (this TU and the rest of soh) execute.
//  - OutOfLine: through a member-function pointer, which cannot be inlined
//    and therefore measures the linker-selected COMDAT symbol itself — the
//    copy any non-inlined call site in EITHER game binds.
//
// The write goes to nextHookId only; the hook map/hookData are inline-static
// (per-process, instance-independent), which is why a fake instance works.
// Unregister + Pump exist so probes can clean the shared static map back up.

extern "C" size_t OoT_GI_InstanceSize(void) {
    return sizeof(GameInteractor);
}

extern "C" size_t OoT_GI_NextHookIdOffset(void) {
    return offsetof(GameInteractor, nextHookId);
}

extern "C" uint32_t OoT_GI_ProbeRegisterOnMainStartDirect(void* storage, void (*fn)(void)) {
    return static_cast<GameInteractor*>(storage)->RegisterGameHook<GameInteractor::OnGameStateMainStart>(fn);
}

extern "C" uint32_t OoT_GI_ProbeRegisterOnMainStartOutOfLine(void* storage, void (*fn)(void)) {
    auto reg = &GameInteractor::RegisterGameHook<GameInteractor::OnGameStateMainStart>;
    GameInteractor* gi = static_cast<GameInteractor*>(storage);
#ifdef __cpp_lib_source_location
    return (gi->*reg)(fn, std::source_location::current());
#else
    return (gi->*reg)(fn);
#endif
}

extern "C" void OoT_GI_ProbeUnregisterOnMainStart(uint32_t hookId) {
    // Unregister only queues into an inline-static vector; no instance state
    // is touched, so no fake storage is needed here.
    GameInteractor gi;
    gi.UnregisterGameHook<GameInteractor::OnGameStateMainStart>(hookId);
}

extern "C" void OoT_GI_ProbePumpOnMainStart(void) {
    // Flushes queued unregistrations, then runs whatever remains registered.
    // Callers unregister their probe hooks first, so after the flush this
    // executes nothing in the unit-test harness.
    GameInteractor gi;
    gi.ExecuteHooks<GameInteractor::OnGameStateMainStart>();
}

// MARK: - #470 registry-identity probes (redship --test mm-gi-shim)
//
// OoT's view of the OnSceneInit hook registry, from a real OoT TU. The
// mm-gi-shim lock compares these against the MM-side twins exported from
// games/mm/2s2h/GameExports_SingleExe.cpp: the addresses must DIFFER, or the
// linker has folded both ports' same-named registries into one object whose
// std::function payloads disagree (MM's OnSceneInit::fn is the two-arg
// (s8, s8) form — the static_assert below pins this side's one-arg premise,
// its MM twin pins the divergence, so the fault class is proven live rather
// than assumed). MM's side stays un-foldable because its hook types are
// tag-scoped under GameInteractor::MM_HookTypes (#470).
static_assert(std::is_same_v<GameInteractor::OnSceneInit::fn, std::function<void(int16_t)>>,
              "OoT's OnSceneInit payload changed shape — revisit the #470 registry-identity lock");

extern "C" void* OoT_GI_OnSceneInitRegistryAddr(void) {
    return &GameInteractor::RegisteredGameHooks<GameInteractor::OnSceneInit>::functions;
}

extern "C" void* OoT_GI_OnSceneInitUnregQueueAddr(void) {
    return &GameInteractor::HooksToUnregister<GameInteractor::OnSceneInit>::hooks;
}
#endif
