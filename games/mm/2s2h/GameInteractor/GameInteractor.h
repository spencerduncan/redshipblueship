#ifndef MM_GAME_INTERACTOR_H
#define MM_GAME_INTERACTOR_H

#include <stdarg.h>

#ifdef __cplusplus
#include <string>
#include <variant>
extern "C" {
#endif
#include "z64.h"
#ifdef __cplusplus
}
#endif

#include "GameInteractor_VanillaBehavior.h"

typedef enum {
    FLAG_NONE,
    FLAG_WEEK_EVENT_REG,
    FLAG_WEEK_EVENT_REG_HORSE_RACE,
    FLAG_EVENT_INF,
    FLAG_SCENES_VISIBLE,
    FLAG_OWL_ACTIVATION,
    FLAG_PERM_SCENE_CHEST,
    FLAG_PERM_SCENE_SWITCH,
    FLAG_PERM_SCENE_CLEARED_ROOM,
    FLAG_PERM_SCENE_COLLECTIBLE,
    FLAG_PERM_SCENE_UNK_14,
    FLAG_PERM_SCENE_ROOMS,
    FLAG_CYCL_SCENE_CHEST,
    FLAG_CYCL_SCENE_SWITCH,
    FLAG_CYCL_SCENE_CLEARED_ROOM,
    FLAG_CYCL_SCENE_COLLECTIBLE,
    FLAG_RANDO_INF,
} FlagType;

typedef enum {
    GI_INVERT_CAMERA_RIGHT_STICK_X,
    GI_INVERT_CAMERA_RIGHT_STICK_Y,
    GI_INVERT_MOVEMENT_X,
    GI_INVERT_SHIELD_X,
    GI_INVERT_SHIELD_Y,
    GI_INVERT_SHOP_X,
    GI_INVERT_HORSE_X,
    GI_INVERT_ZORA_SWIM_X,
    GI_INVERT_DEBUG_DPAD_X,
    GI_INVERT_TELESCOPE_X,
    GI_INVERT_FIRST_PERSON_AIM_X,
    GI_INVERT_FIRST_PERSON_AIM_Y,
    GI_INVERT_FIRST_PERSON_GYRO_X,
    GI_INVERT_FIRST_PERSON_GYRO_Y,
    GI_INVERT_FIRST_PERSON_RIGHT_STICK_X,
    GI_INVERT_FIRST_PERSON_RIGHT_STICK_Y,
    GI_INVERT_FIRST_PERSON_MOVING_X,
} GIInvertType;

typedef enum {
    GI_DPAD_OCARINA,
    GI_DPAD_EQUIP,
} GIDpadType;

typedef enum {
    GI_EVENT_NONE,
    GI_EVENT_GIVE_ITEM,
    GI_EVENT_SPAWN_ACTOR,
    GI_EVENT_TRANSITION,
} GIEventType;

#ifdef __cplusplus

#include <vector>
#include <functional>
#include <map>
#include <unordered_map>
#include <cstdint>
#include <algorithm>

#include <version>
#ifdef __cpp_lib_source_location
#include <source_location>
#else
#pragma message("Compiling without <source_location> support, the Hook Debugger will not be available")
#endif

typedef uint32_t HOOK_ID;

enum HookType {
    HOOK_TYPE_NORMAL,
    HOOK_TYPE_ID,
    HOOK_TYPE_PTR,
    HOOK_TYPE_FILTER,
};

struct HookRegisteringInfo {
    bool valid;
    const char* file;
    std::uint_least32_t line;
    std::uint_least32_t column;
    const char* function;
    HookType type;

    HookRegisteringInfo()
        : valid(false), file("unknown file"), line(0), column(0), function("unknown function"), type(HOOK_TYPE_NORMAL) {
    }

    HookRegisteringInfo(const char* _file, std::uint_least32_t _line, std::uint_least32_t _column,
                        const char* _function, HookType _type)
        : valid(true), file(_file), line(_line), column(_column), function(_function), type(_type) {
        // Trim off user parent directories
        const char* trimmed = strstr(_file, "mm/2s2h/");
        if (trimmed != nullptr) {
            file = trimmed;
        }
    }
};

struct HookInfo {
    uint32_t calls;
    HookRegisteringInfo registering;
};

#ifdef __cpp_lib_source_location
#define GET_CURRENT_REGISTERING_INFO(type) \
    (HookRegisteringInfo{ location.file_name(), location.line(), location.column(), location.function_name(), type })
#else
#define GET_CURRENT_REGISTERING_INFO(type) (HookRegisteringInfo{})
#endif

struct GIEventNone {};

struct GIEventGiveItem {
    // Whether or not to show the get item cutscene. If true and the player is in the air, the
    // player will instead be frozen for a few seconds. If this is true you _must_ call
    // CustomMessage::SetActiveCustomMessage in the giveItem function otherwise you'll just see a blank message.
    bool showGetItemCutscene;
    // Arbitrary s16 that can be accessed from within the give/draw functions with CUSTOM_ITEM_PARAM
    s16 param;
    // These are run in the context of an item00 actor. This isn't super important but can be useful in some cases
    ActorFunc giveItem;
    ActorFunc drawItem;
};

struct GIEventSpawnActor {
    s16 actorId;
    f32 posX;
    f32 posY;
    f32 posZ;
    s16 rotX;
    s16 rotY;
    s16 rotZ;
    s32 params;
    // if true, the coordinates are made relative to the player's position and rotation, 0 rotation is facing the same
    // direction as the player, x+ is to the players right, y+ is up, z+ is in front of the player
    bool relativeCoords;
};

struct GIEventTransition {
    u16 entrance;
    u16 cutsceneIndex;
    s8 transitionTrigger;
    u8 transitionType;
};

struct GIEventTrap {
    std::function<void()> action;
};

typedef std::variant<GIEventNone, GIEventGiveItem, GIEventSpawnActor, GIEventTransition, GIEventTrap> GIEvent;

namespace MM {

class GameInteractor {
  public:
    static GameInteractor* Instance;

    void RegisterOwnHooks();

    // Game State
    std::vector<GIEvent> events = {};
    GIEvent currentEvent = GIEventNone();

    // Game Hooks
    HOOK_ID nextHookId = 1;

    template <typename H> struct RegisteredGameHooks {
        inline static std::unordered_map<HOOK_ID, typename H::fn> functions;
        inline static std::unordered_map<int32_t, std::unordered_map<HOOK_ID, typename H::fn>> functionsForID;
        inline static std::unordered_map<uintptr_t, std::unordered_map<HOOK_ID, typename H::fn>> functionsForPtr;
        inline static std::unordered_map<HOOK_ID, std::pair<typename H::filter, typename H::fn>> functionsForFilter;

        // Used for the hook debugger
        inline static std::map<HOOK_ID, HookInfo> hookData;
    };
    template <typename H> struct HooksToUnregister {
        inline static std::vector<HOOK_ID> hooks;
        inline static std::vector<HOOK_ID> hooksForID;
        inline static std::vector<HOOK_ID> hooksForPtr;
        inline static std::vector<HOOK_ID> hooksForFilter;
    };

    template <typename H> std::map<uint32_t, HookInfo>* GetHookData() {
        return &RegisteredGameHooks<H>::hookData;
    }

    // General Hooks
    template <typename H>
#ifdef __cpp_lib_source_location
    HOOK_ID RegisterGameHook(typename H::fn h, const std::source_location location = std::source_location::current()) {
#else
    HOOK_ID RegisterGameHook(typename H::fn h) {
#endif
        if (this->nextHookId == 0 || this->nextHookId >= UINT32_MAX)
            this->nextHookId = 1;
        while (RegisteredGameHooks<H>::functions.find(this->nextHookId) != RegisteredGameHooks<H>::functions.end()) {
            this->nextHookId++;
        }

        RegisteredGameHooks<H>::functions[this->nextHookId] = h;
        RegisteredGameHooks<H>::hookData[this->nextHookId] =
            HookInfo{ 0, GET_CURRENT_REGISTERING_INFO(HOOK_TYPE_NORMAL) };
        return this->nextHookId++;
    }
    template <typename H> void UnregisterGameHook(HOOK_ID hookId) {
        if (hookId == 0)
            return;
        HooksToUnregister<H>::hooks.push_back(hookId);
    }
    template <typename H, typename... Args> void ExecuteHooks(Args&&... args) {
        // Remove pending hooks for this type
        for (auto& hookId : HooksToUnregister<H>::hooks) {
            RegisteredGameHooks<H>::functions.erase(hookId);
            RegisteredGameHooks<H>::hookData.erase(hookId);
        }
        HooksToUnregister<H>::hooks.clear();
        // Execute hooks
        for (auto& hook : RegisteredGameHooks<H>::functions) {
            hook.second(std::forward<Args>(args)...);
            RegisteredGameHooks<H>::hookData[hook.first].calls += 1;
        }
    }

    // ID based Hooks
    template <typename H>
#ifdef __cpp_lib_source_location
    HOOK_ID RegisterGameHookForID(int32_t id, typename H::fn h,
                                  std::source_location location = std::source_location::current()) {
#else
    HOOK_ID RegisterGameHookForID(int32_t id, typename H::fn h) {
#endif
        if (this->nextHookId == 0 || this->nextHookId >= UINT32_MAX)
            this->nextHookId = 1;
        while (RegisteredGameHooks<H>::functionsForID[id].find(this->nextHookId) !=
               RegisteredGameHooks<H>::functionsForID[id].end()) {
            this->nextHookId++;
        }

        RegisteredGameHooks<H>::functionsForID[id][this->nextHookId] = h;
        RegisteredGameHooks<H>::hookData[this->nextHookId] = HookInfo{ 0, GET_CURRENT_REGISTERING_INFO(HOOK_TYPE_ID) };
        return this->nextHookId++;
    }
    template <typename H> void UnregisterGameHookForID(HOOK_ID hookId) {
        if (hookId == 0)
            return;
        HooksToUnregister<H>::hooksForID.push_back(hookId);
    }
    template <typename H, typename... Args> void ExecuteHooksForID(int32_t id, Args&&... args) {
        // Remove pending hooks for this type
        for (auto hookIdIt = HooksToUnregister<H>::hooksForID.begin();
             hookIdIt != HooksToUnregister<H>::hooksForID.end();) {
            bool remove = false;

            if (RegisteredGameHooks<H>::functionsForID[id].size() == 0) {
                break;
            }

            for (auto it = RegisteredGameHooks<H>::functionsForID[id].begin();
                 it != RegisteredGameHooks<H>::functionsForID[id].end();) {
                if (it->first == *hookIdIt) {
                    it = RegisteredGameHooks<H>::functionsForID[id].erase(it);
                    RegisteredGameHooks<H>::hookData.erase(*hookIdIt);
                    remove = true;
                    break;
                } else {
                    ++it;
                }
            }

            if (remove) {
                hookIdIt = HooksToUnregister<H>::hooksForID.erase(hookIdIt);
            } else {
                ++hookIdIt;
            }
        }
        // Execute hooks
        for (auto& hook : RegisteredGameHooks<H>::functionsForID[id]) {
            hook.second(std::forward<Args>(args)...);
            RegisteredGameHooks<H>::hookData[hook.first].calls += 1;
        }
    }

    // PTR based Hooks
    template <typename H>
#ifdef __cpp_lib_source_location
    HOOK_ID RegisterGameHookForPtr(uintptr_t ptr, typename H::fn h,
                                   const std::source_location location = std::source_location::current()) {
#else
    HOOK_ID RegisterGameHookForPtr(uintptr_t ptr, typename H::fn h) {
#endif
        if (this->nextHookId == 0 || this->nextHookId >= UINT32_MAX)
            this->nextHookId = 1;
        while (RegisteredGameHooks<H>::functionsForPtr[ptr].find(this->nextHookId) !=
               RegisteredGameHooks<H>::functionsForPtr[ptr].end()) {
            this->nextHookId++;
        }

        RegisteredGameHooks<H>::functionsForPtr[ptr][this->nextHookId] = h;
        RegisteredGameHooks<H>::hookData[this->nextHookId] = HookInfo{ 0, GET_CURRENT_REGISTERING_INFO(HOOK_TYPE_PTR) };
        return this->nextHookId++;
    }
    template <typename H> void UnregisterGameHookForPtr(HOOK_ID hookId) {
        if (hookId == 0)
            return;
        HooksToUnregister<H>::hooksForPtr.push_back(hookId);
    }
    template <typename H, typename... Args> void ExecuteHooksForPtr(uintptr_t ptr, Args&&... args) {
        // Remove pending hooks for this type
        for (auto hookIdIt = HooksToUnregister<H>::hooksForPtr.begin();
             hookIdIt != HooksToUnregister<H>::hooksForPtr.end();) {
            bool remove = false;

            if (RegisteredGameHooks<H>::functionsForPtr[ptr].size() == 0) {
                break;
            }

            for (auto it = RegisteredGameHooks<H>::functionsForPtr[ptr].begin();
                 it != RegisteredGameHooks<H>::functionsForPtr[ptr].end();) {
                if (it->first == *hookIdIt) {
                    it = RegisteredGameHooks<H>::functionsForPtr[ptr].erase(it);
                    RegisteredGameHooks<H>::hookData.erase(*hookIdIt);
                    remove = true;
                    break;
                } else {
                    ++it;
                }
            }

            if (remove) {
                hookIdIt = HooksToUnregister<H>::hooksForPtr.erase(hookIdIt);
            } else {
                ++hookIdIt;
            }
        }
        // Execute hooks
        for (auto& hook : RegisteredGameHooks<H>::functionsForPtr[ptr]) {
            hook.second(std::forward<Args>(args)...);
            RegisteredGameHooks<H>::hookData[hook.first].calls += 1;
        }
    }

    // Filter based Hooks
    template <typename H>
#ifdef __cpp_lib_source_location
    HOOK_ID RegisterGameHookForFilter(typename H::filter f, typename H::fn h,
                                      const std::source_location location = std::source_location::current()) {
#else
    HOOK_ID RegisterGameHookForFilter(typename H::filter f, typename H::fn h) {
#endif
        if (this->nextHookId == 0 || this->nextHookId >= UINT32_MAX)
            this->nextHookId = 1;
        while (RegisteredGameHooks<H>::functionsForFilter.find(this->nextHookId) !=
               RegisteredGameHooks<H>::functionsForFilter.end()) {
            this->nextHookId++;
        }

        RegisteredGameHooks<H>::functionsForFilter[this->nextHookId] = std::make_pair(f, h);
        RegisteredGameHooks<H>::hookData[this->nextHookId] =
            HookInfo{ 0, GET_CURRENT_REGISTERING_INFO(HOOK_TYPE_FILTER) };
        return this->nextHookId++;
    }
    template <typename H> void UnregisterGameHookForFilter(HOOK_ID hookId) {
        if (hookId == 0)
            return;
        HooksToUnregister<H>::hooksForFilter.push_back(hookId);
    }
    template <typename H, typename... Args> void ExecuteHooksForFilter(Args&&... args) {
        // Remove pending hooks for this type
        for (auto& hookId : HooksToUnregister<H>::hooksForFilter) {
            RegisteredGameHooks<H>::functionsForFilter.erase(hookId);
            RegisteredGameHooks<H>::hookData.erase(hookId);
        }
        HooksToUnregister<H>::hooksForFilter.clear();
        // Execute hooks
        for (auto& hook : RegisteredGameHooks<H>::functionsForFilter) {
            if (hook.second.first(std::forward<Args>(args)...)) {
                hook.second.second(std::forward<Args>(args)...);
                RegisteredGameHooks<H>::hookData[hook.first].calls += 1;
            }
        }
    }

    template <typename H> void ProcessUnregisteredHooks() {
        // Normal
        for (auto& hookId : HooksToUnregister<H>::hooks) {
            RegisteredGameHooks<H>::functions.erase(hookId);
            RegisteredGameHooks<H>::hookData.erase(hookId);
        }
        HooksToUnregister<H>::hooks.clear();

        // ID
        for (auto& hookId : HooksToUnregister<H>::hooksForID) {
            for (auto& idGroup : RegisteredGameHooks<H>::functionsForID) {
                for (auto it = idGroup.second.begin(); it != idGroup.second.end();) {
                    if (it->first == hookId) {
                        it = idGroup.second.erase(it);
                        RegisteredGameHooks<H>::hookData.erase(hookId);
                    } else {
                        ++it;
                    }
                }
            }
        }
        HooksToUnregister<H>::hooksForID.clear();

        // Ptr
        for (auto& hookId : HooksToUnregister<H>::hooksForPtr) {
            for (auto& ptrGroup : RegisteredGameHooks<H>::functionsForPtr) {
                for (auto it = ptrGroup.second.begin(); it != ptrGroup.second.end();) {
                    if (it->first == hookId) {
                        it = ptrGroup.second.erase(it);
                        RegisteredGameHooks<H>::hookData.erase(hookId);
                    } else {
                        ++it;
                    }
                }
            }
        }
        HooksToUnregister<H>::hooksForPtr.clear();

        // Filter
        for (auto& hookId : HooksToUnregister<H>::hooksForFilter) {
            RegisteredGameHooks<H>::functionsForFilter.erase(hookId);
            RegisteredGameHooks<H>::hookData.erase(hookId);
        }
        HooksToUnregister<H>::hooksForFilter.clear();
    }

    void RemoveAllQueuedHooks() {
#define DEFINE_HOOK(name, _) ProcessUnregisteredHooks<name>();

#include "GameInteractor_HookTable.h"

#undef DEFINE_HOOK
    }

    class HookFilter {
      public:
        static auto ActorNotPlayer(Actor* actor) {
            return actor->id != ACTOR_PLAYER;
        }
        // For use with Should hooks
        static auto SActorNotPlayer(Actor* actor, bool* result) {
            return actor->id != ACTOR_PLAYER;
        }
        static auto ActorMatchIdAndParams(int16_t id, int16_t params) {
            return [id, params](Actor* actor) { return actor->id == id && actor->params == params; };
        }
        // For use with Should hooks
        static auto SActorMatchIdAndParams(int16_t id, int16_t params) {
            return [id, params](Actor* actor, bool* result) { return actor->id == id && actor->params == params; };
        }
    };

#define DEFINE_HOOK(name, args)                  \
    struct name {                                \
        typedef std::function<void args> fn;     \
        typedef std::function<bool args> filter; \
    };

#include "GameInteractor_HookTable.h"

#undef DEFINE_HOOK
};

} // namespace MM

// Allow unqualified access to MM::GameInteractor within MM code
using MM::GameInteractor;

extern "C" {
#endif // __cplusplus

// MM-prefixed C function declarations (actual implementations)
void MM_GameInteractor_ExecuteOnGameStateMainStart();
void MM_GameInteractor_ExecuteOnGameStateMainFinish();
void MM_GameInteractor_ExecuteOnGameStateDrawFinish();
void MM_GameInteractor_ExecuteOnGameStateUpdate();
void MM_GameInteractor_ExecuteOnConsoleLogoUpdate();
void MM_GameInteractor_ExecuteOnKaleidoUpdate(PauseContext* pauseCtx);
void MM_GameInteractor_ExecuteBeforeKaleidoDrawPage(PauseContext* pauseCtx, u16 pauseIndex);
void MM_GameInteractor_ExecuteAfterKaleidoDrawPage(PauseContext* pauseCtx, u16 pauseIndex);
void MM_GameInteractor_ExecuteOnSaveInit(s16 fileNum);
void MM_GameInteractor_ExecuteOnSaveLoad(s16 fileNum);
void MM_GameInteractor_ExecuteOnFileSelectSaveLoad(s16 fileNum, bool isOwlSave, SaveContext* saveContext);
void MM_GameInteractor_ExecuteBeforeEndOfCycleSave();
void MM_GameInteractor_ExecuteAfterEndOfCycleSave();
void MM_GameInteractor_ExecuteBeforeMoonCrashSaveReset();
void MM_GameInteractor_ExecuteOnInterfaceDrawStart();
void MM_GameInteractor_ExecuteAfterInterfaceClockDraw();
void MM_GameInteractor_ExecuteBeforeInterfaceClockDraw();
void MM_GameInteractor_ExecuteOnGameCompletion();

void MM_GameInteractor_ExecuteOnSceneInit(s16 sceneId, s8 spawnNum);
void MM_GameInteractor_ExecuteOnRoomInit(s16 sceneId, s8 roomNum);
void MM_GameInteractor_ExecuteAfterRoomSceneCommands(s16 sceneId, s8 roomNum);
void MM_GameInteractor_ExecuteOnPlayDrawWorldEnd();
void MM_GameInteractor_ExecuteOnPlayDestroy();

bool MM_GameInteractor_ShouldActorInit(Actor* actor);
void MM_GameInteractor_ExecuteOnActorInit(Actor* actor);
bool MM_GameInteractor_ShouldActorUpdate(Actor* actor);
void MM_GameInteractor_ExecuteOnActorUpdate(Actor* actor);
bool MM_GameInteractor_ShouldActorDraw(Actor* actor);
void MM_GameInteractor_ExecuteOnActorDraw(Actor* actor);
void MM_GameInteractor_ExecuteOnActorKill(Actor* actor);
void MM_GameInteractor_ExecuteOnActorDestroy(Actor* actor);
void MM_GameInteractor_ExecuteOnPlayerPostLimbDraw(Player* player, s32 limbIndex);
void MM_GameInteractor_ExecuteOnBossDefeated(s16 actorId);

void MM_GameInteractor_ExecuteOnSceneFlagSet(s16 sceneId, FlagType flagType, u32 flag);
void MM_GameInteractor_ExecuteOnSceneFlagUnset(s16 sceneId, FlagType flagType, u32 flag);
void MM_GameInteractor_ExecuteOnFlagSet(FlagType flagType, u32 flag);
void MM_GameInteractor_ExecuteOnFlagUnset(FlagType flagType, u32 flag);

void MM_GameInteractor_ExecuteAfterCameraUpdate(Camera* camera);
void MM_GameInteractor_ExecuteOnCameraChangeModeFlags(Camera* camera);
void MM_GameInteractor_ExecuteOnCameraChangeSettingsFlags(Camera* camera);

void MM_GameInteractor_ExecuteOnPassPlayerInputs(Input* input);

void MM_GameInteractor_ExecuteOnOpenText(u16* textId, bool* loadFromMessageTable);

bool MM_GameInteractor_ShouldItemGive(u8 item);
void MM_GameInteractor_ExecuteOnItemGive(u8 item);

void MM_GameInteractor_ExecuteOnBottleContentsUpdate(u8 item);

void MM_GameInteractor_ExecuteOnSeqPlayerInit(int32_t playerIdx, int32_t seqId);

bool MM_GameInteractor_Should(GIVanillaBehavior flag, uint32_t result, ...);

int MM_GameInteractor_InvertControl(GIInvertType type);
uint32_t MM_GameInteractor_Dpad(GIDpadType type, uint32_t buttonCombo);
uint32_t MM_GameInteractor_RightStickOcarina(Input* input);

// Redirect unprefixed names to MM-prefixed versions for MM game code
#define GameInteractor_ExecuteOnGameStateMainStart MM_GameInteractor_ExecuteOnGameStateMainStart
#define GameInteractor_ExecuteOnGameStateMainFinish MM_GameInteractor_ExecuteOnGameStateMainFinish
#define GameInteractor_ExecuteOnGameStateDrawFinish MM_GameInteractor_ExecuteOnGameStateDrawFinish
#define GameInteractor_ExecuteOnGameStateUpdate MM_GameInteractor_ExecuteOnGameStateUpdate
#define GameInteractor_ExecuteOnConsoleLogoUpdate MM_GameInteractor_ExecuteOnConsoleLogoUpdate
#define GameInteractor_ExecuteOnKaleidoUpdate MM_GameInteractor_ExecuteOnKaleidoUpdate
#define GameInteractor_ExecuteBeforeKaleidoDrawPage MM_GameInteractor_ExecuteBeforeKaleidoDrawPage
#define GameInteractor_ExecuteAfterKaleidoDrawPage MM_GameInteractor_ExecuteAfterKaleidoDrawPage
#define GameInteractor_ExecuteOnSaveInit MM_GameInteractor_ExecuteOnSaveInit
#define GameInteractor_ExecuteOnSaveLoad MM_GameInteractor_ExecuteOnSaveLoad
#define GameInteractor_ExecuteOnFileSelectSaveLoad MM_GameInteractor_ExecuteOnFileSelectSaveLoad
#define GameInteractor_ExecuteBeforeEndOfCycleSave MM_GameInteractor_ExecuteBeforeEndOfCycleSave
#define GameInteractor_ExecuteAfterEndOfCycleSave MM_GameInteractor_ExecuteAfterEndOfCycleSave
#define GameInteractor_ExecuteBeforeMoonCrashSaveReset MM_GameInteractor_ExecuteBeforeMoonCrashSaveReset
#define GameInteractor_ExecuteOnInterfaceDrawStart MM_GameInteractor_ExecuteOnInterfaceDrawStart
#define GameInteractor_ExecuteAfterInterfaceClockDraw MM_GameInteractor_ExecuteAfterInterfaceClockDraw
#define GameInteractor_ExecuteBeforeInterfaceClockDraw MM_GameInteractor_ExecuteBeforeInterfaceClockDraw
#define GameInteractor_ExecuteOnGameCompletion MM_GameInteractor_ExecuteOnGameCompletion
#define GameInteractor_ExecuteOnSceneInit MM_GameInteractor_ExecuteOnSceneInit
#define GameInteractor_ExecuteOnRoomInit MM_GameInteractor_ExecuteOnRoomInit
#define GameInteractor_ExecuteAfterRoomSceneCommands MM_GameInteractor_ExecuteAfterRoomSceneCommands
#define GameInteractor_ExecuteOnPlayDrawWorldEnd MM_GameInteractor_ExecuteOnPlayDrawWorldEnd
#define GameInteractor_ExecuteOnPlayDestroy MM_GameInteractor_ExecuteOnPlayDestroy
#define GameInteractor_ShouldActorInit MM_GameInteractor_ShouldActorInit
#define GameInteractor_ExecuteOnActorInit MM_GameInteractor_ExecuteOnActorInit
#define GameInteractor_ShouldActorUpdate MM_GameInteractor_ShouldActorUpdate
#define GameInteractor_ExecuteOnActorUpdate MM_GameInteractor_ExecuteOnActorUpdate
#define GameInteractor_ShouldActorDraw MM_GameInteractor_ShouldActorDraw
#define GameInteractor_ExecuteOnActorDraw MM_GameInteractor_ExecuteOnActorDraw
#define GameInteractor_ExecuteOnActorKill MM_GameInteractor_ExecuteOnActorKill
#define GameInteractor_ExecuteOnActorDestroy MM_GameInteractor_ExecuteOnActorDestroy
#define GameInteractor_ExecuteOnPlayerPostLimbDraw MM_GameInteractor_ExecuteOnPlayerPostLimbDraw
#define GameInteractor_ExecuteOnBossDefeated MM_GameInteractor_ExecuteOnBossDefeated
#define GameInteractor_ExecuteOnSceneFlagSet MM_GameInteractor_ExecuteOnSceneFlagSet
#define GameInteractor_ExecuteOnSceneFlagUnset MM_GameInteractor_ExecuteOnSceneFlagUnset
#define GameInteractor_ExecuteOnFlagSet MM_GameInteractor_ExecuteOnFlagSet
#define GameInteractor_ExecuteOnFlagUnset MM_GameInteractor_ExecuteOnFlagUnset
#define GameInteractor_ExecuteAfterCameraUpdate MM_GameInteractor_ExecuteAfterCameraUpdate
#define GameInteractor_ExecuteOnCameraChangeModeFlags MM_GameInteractor_ExecuteOnCameraChangeModeFlags
#define GameInteractor_ExecuteOnCameraChangeSettingsFlags MM_GameInteractor_ExecuteOnCameraChangeSettingsFlags
#define GameInteractor_ExecuteOnPassPlayerInputs MM_GameInteractor_ExecuteOnPassPlayerInputs
#define GameInteractor_ExecuteOnOpenText MM_GameInteractor_ExecuteOnOpenText
#define GameInteractor_ShouldItemGive MM_GameInteractor_ShouldItemGive
#define GameInteractor_ExecuteOnItemGive MM_GameInteractor_ExecuteOnItemGive
#define GameInteractor_ExecuteOnBottleContentsUpdate MM_GameInteractor_ExecuteOnBottleContentsUpdate
#define GameInteractor_ExecuteOnSeqPlayerInit MM_GameInteractor_ExecuteOnSeqPlayerInit
#define GameInteractor_Should MM_GameInteractor_Should
#define GameInteractor_InvertControl MM_GameInteractor_InvertControl
#define GameInteractor_Dpad MM_GameInteractor_Dpad
#define GameInteractor_RightStickOcarina MM_GameInteractor_RightStickOcarina

#ifdef __cplusplus
}

#define REGISTER_VB_SHOULD(flag, body)                                                      \
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldVanillaBehavior>( \
        flag, [](GIVanillaBehavior _, bool* should, va_list originalArgs) {                 \
            va_list args;                                                                   \
            va_copy(args, originalArgs);                                                    \
            body;                                                                           \
            va_end(args);                                                                   \
        })
#define COND_HOOK(hookType, condition, body)                                                     \
    {                                                                                            \
        static HOOK_ID hookId = 0;                                                               \
        GameInteractor::Instance->UnregisterGameHook<GameInteractor::hookType>(hookId);          \
        hookId = 0;                                                                              \
        if (condition) {                                                                         \
            hookId = GameInteractor::Instance->RegisterGameHook<GameInteractor::hookType>(body); \
        }                                                                                        \
    }
#define COND_ID_HOOK(hookType, id, condition, body)                                                       \
    {                                                                                                     \
        static HOOK_ID hookId = 0;                                                                        \
        GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::hookType>(hookId);              \
        hookId = 0;                                                                                       \
        if (condition) {                                                                                  \
            hookId = GameInteractor::Instance->RegisterGameHookForID<GameInteractor::hookType>(id, body); \
        }                                                                                                 \
    }
#define COND_VB_SHOULD(id, condition, body)                                                               \
    {                                                                                                     \
        static HOOK_ID hookId = 0;                                                                        \
        GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::ShouldVanillaBehavior>(hookId); \
        hookId = 0;                                                                                       \
        if (condition) {                                                                                  \
            hookId = REGISTER_VB_SHOULD(id, body);                                                        \
        }                                                                                                 \
    }

#endif

#endif // MM_GAME_INTERACTOR_H
