#ifndef GAME_INTERACTOR_H
#define GAME_INTERACTOR_H

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

extern "C" {
#endif // __cplusplus

void GameInteractor_ExecuteOnGameStateMainStart();
void GameInteractor_ExecuteOnGameStateMainFinish();
void GameInteractor_ExecuteOnGameStateDrawFinish();
void GameInteractor_ExecuteOnGameStateUpdate();
void GameInteractor_ExecuteOnConsoleLogoUpdate();
void GameInteractor_ExecuteOnKaleidoUpdate(PauseContext* pauseCtx);
void GameInteractor_ExecuteBeforeKaleidoDrawPage(PauseContext* pauseCtx, u16 pauseIndex);
void GameInteractor_ExecuteAfterKaleidoDrawPage(PauseContext* pauseCtx, u16 pauseIndex);
void GameInteractor_ExecuteOnSaveInit(s16 fileNum);
void GameInteractor_ExecuteOnSaveLoad(s16 fileNum);
void GameInteractor_ExecuteOnFileSelectSaveLoad(s16 fileNum, bool isOwlSave, SaveContext* saveContext);
void GameInteractor_ExecuteBeforeEndOfCycleSave();
void GameInteractor_ExecuteAfterEndOfCycleSave();
void GameInteractor_ExecuteBeforeMoonCrashSaveReset();
void GameInteractor_ExecuteOnInterfaceDrawStart();
void GameInteractor_ExecuteAfterInterfaceClockDraw();
void GameInteractor_ExecuteBeforeInterfaceClockDraw();
void GameInteractor_ExecuteOnGameCompletion();

void GameInteractor_ExecuteOnSceneInit(s16 sceneId, s8 spawnNum);
void GameInteractor_ExecuteOnRoomInit(s16 sceneId, s8 roomNum);
void GameInteractor_ExecuteAfterRoomSceneCommands(s16 sceneId, s8 roomNum);
void GameInteractor_ExecuteOnPlayDrawWorldEnd();
void GameInteractor_ExecuteOnPlayDestroy();

bool GameInteractor_ShouldActorInit(Actor* actor);
void GameInteractor_ExecuteOnActorInit(Actor* actor);
bool GameInteractor_ShouldActorUpdate(Actor* actor);
void GameInteractor_ExecuteOnActorUpdate(Actor* actor);
bool GameInteractor_ShouldActorDraw(Actor* actor);
void GameInteractor_ExecuteOnActorDraw(Actor* actor);
void GameInteractor_ExecuteOnActorKill(Actor* actor);
void GameInteractor_ExecuteOnActorDestroy(Actor* actor);
void GameInteractor_ExecuteOnPlayerPostLimbDraw(Player* player, s32 limbIndex);
void GameInteractor_ExecuteOnBossDefeated(s16 actorId);

void GameInteractor_ExecuteOnSceneFlagSet(s16 sceneId, FlagType flagType, u32 flag);
void GameInteractor_ExecuteOnSceneFlagUnset(s16 sceneId, FlagType flagType, u32 flag);
void GameInteractor_ExecuteOnFlagSet(FlagType flagType, u32 flag);
void GameInteractor_ExecuteOnFlagUnset(FlagType flagType, u32 flag);

void GameInteractor_ExecuteAfterCameraUpdate(Camera* camera);
void GameInteractor_ExecuteOnCameraChangeModeFlags(Camera* camera);
void GameInteractor_ExecuteOnCameraChangeSettingsFlags(Camera* camera);

void GameInteractor_ExecuteOnPassPlayerInputs(Input* input);

void GameInteractor_ExecuteOnOpenText(u16* textId, bool* loadFromMessageTable);

bool GameInteractor_ShouldItemGive(u8 item);
void GameInteractor_ExecuteOnItemGive(u8 item);

void GameInteractor_ExecuteOnBottleContentsUpdate(u8 item);

void GameInteractor_ExecuteOnSeqPlayerInit(int32_t playerIdx, int32_t seqId);

bool GameInteractor_Should(GIVanillaBehavior flag, uint32_t result, ...);
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

int GameInteractor_InvertControl(GIInvertType type);
uint32_t GameInteractor_Dpad(GIDpadType type, uint32_t buttonCombo);
uint32_t GameInteractor_RightStickOcarina(Input* input);

#if defined(RSBS_SINGLE_EXECUTABLE)
//
// Single-exe MM-owned "Should" dispatch (#392 VB follow-up; #395/#367 class).
//
// The five upstream names below would otherwise resolve to OoT's extern "C"
// wrappers (GameInteractor_Should/ShouldActorInit/ShouldActorUpdate in
// games/oot/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp) or to
// src/common/mm_stubs.c stubs (ShouldActorDraw/ShouldItemGive). Routing MM
// calls into OoT's registry is forbidden — MM's GIVanillaBehavior ordinals
// ALIAS OoT's (MM VB_SETUP_TRANSITION == OoT VB_PLAY_RAINBOW_BRIDGE_CS ==
// 206) — so those wrappers gate on the active game and return the vanilla
// verdict while MM runs. Net effect: no MM rando/enhancement override could
// ever fire, and a rando-handled check ALSO gave its vanilla contents (the
// C1 double-give).
//
// Rebind every MM call site — textually unchanged, upstream diffs still
// apply — to MM-owned dispatchers (games/mm/2s2h/GameExports_SingleExe.cpp)
// that consult ONLY the MM-owned S2H::GameHooks registries the macro
// redirection below populates. Only MM TUs see this header, so MM VB ids
// never reach OoT's GameInteractor_Should or its hook tables: ordinal safety
// by construction, the same technique as the registration-macro redirection.
// Placed after the upstream declarations above (which stay, un-renamed and
// unused by MM code) and before the close of the extern "C" block so C and
// C++ TUs both rebind.
//
bool MM_GameHooks_ExecuteVBShould(GIVanillaBehavior flag, uint32_t result, ...);
bool MM_GameHooks_ExecuteShouldActorInit(Actor* actor);
bool MM_GameHooks_ExecuteShouldActorUpdate(Actor* actor);
bool MM_GameHooks_ExecuteShouldActorDraw(Actor* actor);
bool MM_GameHooks_ExecuteShouldItemGive(u8 item);
#define GameInteractor_Should MM_GameHooks_ExecuteVBShould
#define GameInteractor_ShouldActorInit MM_GameHooks_ExecuteShouldActorInit
#define GameInteractor_ShouldActorUpdate MM_GameHooks_ExecuteShouldActorUpdate
#define GameInteractor_ShouldActorDraw MM_GameHooks_ExecuteShouldActorDraw
#define GameInteractor_ShouldItemGive MM_GameHooks_ExecuteShouldItemGive

//
// Single-exe MM-owned "Execute" dispatch (#438).
//
// Same hazard, same remedy, one lane later. These three types carry live
// COND_HOOK/COND_ID_HOOK registrants across the linked MM set — 21 TUs on
// ShouldActorInit's sibling OnActorInit, 24 on OnOpenText — and the single-exe
// COND_* macros park every one of them in S2H::GameHooks. Their dispatch,
// however, still went out through the upstream extern "C" names, which resolve
// to OoT's GameInteractor_Execute* (OnActorDraw) or to a src/common/mm_stubs.c
// no-op (OnOpenText). Neither consults the MM registry, so the registrants were
// REGISTERED AND NEVER RUN: chests kept their vanilla model because EnBox's
// ShouldActorInit rewrite never fired, and every rando text override was inert.
// The give path was unaffected, which is why items randomized while nothing
// about their presentation did — the symptom that surfaced this.
//
// Rebinding here rather than editing the ~30 call sites keeps MM's C files
// textually upstream, exactly as the Should block above does.
//
void MM_GameHooks_ExecuteOnActorInit(Actor* actor);
void MM_GameHooks_ExecuteOnActorDraw(Actor* actor);
void MM_GameHooks_ExecuteOnOpenText(u16* textId, bool* loadFromMessageTable);
#define GameInteractor_ExecuteOnActorInit MM_GameHooks_ExecuteOnActorInit
#define GameInteractor_ExecuteOnActorDraw MM_GameHooks_ExecuteOnActorDraw
#define GameInteractor_ExecuteOnOpenText MM_GameHooks_ExecuteOnOpenText

// The end-of-cycle pair (#514). Same rebind, but note what is different about
// it: these two names are MM-ONLY — OoT has no end-of-cycle concept — so the
// upstream names did not cross-bind to an active-game-gated OoT wrapper, they
// resolved to a pair of src/common/mm_stubs.c no-ops that were left stubbed on
// purpose until both halves could land at once. The damage was therefore not a
// missing cosmetic override but the vanilla three-day wipe running unattended:
// Sram_SaveEndOfCycle (games/mm/src/code/z_sram_NES.c, reached by Song of Time
// and "Dawn of the New Day") stripped dungeon keys, boss keys, stray fairies,
// skulltula tokens, frog flags and the trade slots while
// Rando::MiscBehavior::AfterEndOfCycleSave — the code that puts them back and
// clears cycleObtained — sat registered and unreachable. Checks stay flagged
// obtained, so none of it comes back.
//
// Rebinding BOTH is load-bearing, not tidiness: the rando half of Before is a
// pure snapshot into saveContextCopy and After is its only reader, so a
// one-sided rebind buys a memcpy per cycle save and no restored progress. See
// the bridge comment in GameExports_SingleExe.cpp for what else the Before
// half wakes up and for the one registrant set that stays link-elided.
void MM_GameHooks_ExecuteBeforeEndOfCycleSave(void);
void MM_GameHooks_ExecuteAfterEndOfCycleSave(void);
#define GameInteractor_ExecuteBeforeEndOfCycleSave MM_GameHooks_ExecuteBeforeEndOfCycleSave
#define GameInteractor_ExecuteAfterEndOfCycleSave MM_GameHooks_ExecuteAfterEndOfCycleSave

// The actor-lifecycle pair (#515). Third rebind block, and the one that was
// hardest to SEE was missing: the end-of-cycle names above at least resolved to
// a stub somebody had written down in src/common/mm_stubs.c, and the #438 block
// was found by chasing a visible symptom. These two have no stub at all. MM's
// z_actor.c call sites (MM_Actor_Kill, MM_Actor_Delete) simply bound OoT's
// identically-spelled GameInteractor_ExecuteOnActorKill / ...OnActorDestroy,
// which open with GI_SINGLE_EXE_GATE() and return immediately for the entire MM
// session. Registered, linked, silently never run.
//
// The cost was the largest in the class. EnemyDrops.cpp's OnActorKill
// registrant is the only thing that pays out the 18 DROP_TYPE_KILL enemies, and
// ObjGrass.cpp's ACTOR_OBJ_GRASS_UNIT registrant is the only writer of
// RandoCheckIds onto non-actor grass — roughly 230 checks that look and behave
// exactly vanilla while holding items no seed could ever collect.
//
// BOTH NAMES, IN ONE CHANGE. OnActorDestroy carries ObjGrass.cpp's frees for the
// element-keyed ObjectExtension entries the OnActorKill registrant creates, and
// z_actor.c's own actor-keyed ObjectExtension_Free cannot reach those (their key
// is an address inside the actor, not the actor). Rebinding Kill on its own
// would trade an inert bug for a per-scene leak of stale check-id keys over
// recycled arena addresses — see the bridge comment in GameExports_SingleExe.cpp
// for the full failure mode.
void MM_GameHooks_ExecuteOnActorKill(Actor* actor);
void MM_GameHooks_ExecuteOnActorDestroy(Actor* actor);
#define GameInteractor_ExecuteOnActorKill MM_GameHooks_ExecuteOnActorKill
#define GameInteractor_ExecuteOnActorDestroy MM_GameHooks_ExecuteOnActorDestroy

// OnGameCompletion (#438). Its registrant (RegisterSavingEnhancements' fileCompletedAt
// stamp) went live with #520; the call sites bound the mm_stubs.c no-op until this.
void MM_GameHooks_ExecuteOnGameCompletion(void);
#define GameInteractor_ExecuteOnGameCompletion MM_GameHooks_ExecuteOnGameCompletion

// The pause-menu / file-select batch (#438) — the last hook types that combine
// a LIVE registrant (2ship_rando links with WHOLE_ARCHIVE, so
// Rando::MiscBehavior's registrations are in the binary) with dead dispatch.
//
// OnKaleidoUpdate is the subtle one: OoT DEFINES the same extern "C" name as a
// 0-ARG wrapper (GameInteractor_Hooks.cpp) for its own z_kaleido_scope_call.c,
// so MM's 1-arg call in z_kaleido_scope_NES.c linked without complaint — C
// linkage encodes no arity — and bound a gated no-op for the whole MM session.
// That is KaleidoItemPage.cpp's trade-slot cycling input handler: without it,
// left/right on SLOT_TRADE_DEED/KEY_MAMA/COUPLE does nothing and shuffled
// alternate trade items are unreachable from the pause menu.
//
// The Before/AfterKaleidoDrawPage pair and OnFileSelectSaveLoad are MM-only
// names whose mm_stubs.c no-ops are deleted with this rebind, so a dropped
// rebind or bridge is a LINK error, not a silent pass. Both stubs also carried
// the #372/#424 signature-drift hazard ((void*, int) against
// (PauseContext*, u16) and (s16, bool, SaveContext*)). The draw pair is
// rebound TOGETHER on purpose — z_kaleido_scope_NES.c brackets every page
// draw with the pair, and mm_game_hooks.h records the pairing rule.
void MM_GameHooks_ExecuteOnKaleidoUpdate(PauseContext* pauseCtx);
void MM_GameHooks_ExecuteBeforeKaleidoDrawPage(PauseContext* pauseCtx, u16 pauseIndex);
void MM_GameHooks_ExecuteAfterKaleidoDrawPage(PauseContext* pauseCtx, u16 pauseIndex);
void MM_GameHooks_ExecuteOnFileSelectSaveLoad(s16 fileNum, bool isOwlSave, SaveContext* saveContext);
#define GameInteractor_ExecuteOnKaleidoUpdate MM_GameHooks_ExecuteOnKaleidoUpdate
#define GameInteractor_ExecuteBeforeKaleidoDrawPage MM_GameHooks_ExecuteBeforeKaleidoDrawPage
#define GameInteractor_ExecuteAfterKaleidoDrawPage MM_GameHooks_ExecuteAfterKaleidoDrawPage
#define GameInteractor_ExecuteOnFileSelectSaveLoad MM_GameHooks_ExecuteOnFileSelectSaveLoad
#endif // RSBS_SINGLE_EXECUTABLE

#ifdef __cplusplus
}

#if defined(RSBS_SINGLE_EXECUTABLE)
//
// Single-exe hook-macro redirection (#395 / #392 Lane C).
//
// The upstream macro definitions above expand to
// GameInteractor::Instance->RegisterGameHook<...> — in the single exe the
// one GameInteractor allocation is OoT's 4-byte object, so an MM-compiled
// registration writes past its end, and the C++ registry statics the members
// touch COMDAT-contend with OoT's incompatible instantiations
// (mm_game_hooks.h has the full story). Rather than editing the ~650
// COND_HOOK / COND_ID_HOOK / COND_VB_SHOULD / REGISTER_VB_SHOULD sites
// across 2s2h/Rando and 2s2h/Enhancements, the macros themselves are
// redefined here — after the upstream definitions, inside this
// force-included header, so every MM C++ TU sees the redirected expansion —
// to route through the MM-owned S2H::GameHooks registry. Call sites compile
// textually unchanged; upstream diffs to the macro USES still apply cleanly.
//
// Direct (non-macro) Instance->RegisterGameHook sites are migrated by hand
// and locked per target by include/mm_gi_hook_guard.h.
//
#include "mm_game_hooks.h" // S2H::GameHooks registry + MM_GameEvents_* (C++ view)

#undef REGISTER_VB_SHOULD
#undef COND_HOOK
#undef COND_ID_HOOK
#undef COND_VB_SHOULD

#define REGISTER_VB_SHOULD(flag, body)                                            \
    S2H::GameHooks::RegisterForID<GameInteractor::ShouldVanillaBehavior>(         \
        flag, [](GIVanillaBehavior _, bool* should, va_list originalArgs) {       \
            va_list args;                                                         \
            va_copy(args, originalArgs);                                          \
            body;                                                                 \
            va_end(args);                                                         \
        })
#define COND_HOOK(hookType, condition, body)                                      \
    {                                                                             \
        static HOOK_ID hookId = 0;                                                \
        S2H::GameHooks::Unregister<GameInteractor::hookType>(hookId);             \
        hookId = 0;                                                               \
        if (condition) {                                                          \
            hookId = S2H::GameHooks::Register<GameInteractor::hookType>(body);    \
        }                                                                         \
    }
#define COND_ID_HOOK(hookType, id, condition, body)                               \
    {                                                                             \
        static HOOK_ID hookId = 0;                                                \
        S2H::GameHooks::UnregisterForID<GameInteractor::hookType>(hookId);        \
        hookId = 0;                                                               \
        if (condition) {                                                          \
            hookId = S2H::GameHooks::RegisterForID<GameInteractor::hookType>(id, body); \
        }                                                                         \
    }
#define COND_VB_SHOULD(id, condition, body)                                              \
    {                                                                                    \
        static HOOK_ID hookId = 0;                                                       \
        S2H::GameHooks::UnregisterForID<GameInteractor::ShouldVanillaBehavior>(hookId);  \
        hookId = 0;                                                                      \
        if (condition) {                                                                 \
            hookId = REGISTER_VB_SHOULD(id, body);                                       \
        }                                                                                \
    }
#endif // RSBS_SINGLE_EXECUTABLE

#endif

#endif // GAME_INTERACTOR_H
