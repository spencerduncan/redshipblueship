/**
 * MM-owned GameInteractor shim for single-executable builds (#395, #383).
 *
 * WHY THIS EXISTS. Both ports define a global-namespace `class GameInteractor`
 * with different layouts: OoT's has one data member (HOOK_ID nextHookId,
 * sizeof 4) and owns the single shared allocation; MM's carries
 * std::vector<GIEvent> events / GIEvent currentEvent ahead of nextHookId
 * (sizeof 104 / offset 96 under MSVC, 72 / 64 under Linux GCC). MM's
 * implementation TUs are excluded from the single-exe link ("use OoT's",
 * games/mm/CMakeLists.txt), so any MM code that touches an instance member
 * through the class reads/writes MM offsets into OoT's 4-byte allocation —
 * a ~60-92-byte out-of-bounds access (#395, machine-verified). The PR #415
 * diagnostic run showed the linker's COMDAT fold happened to hand MM's
 * non-inlined registration calls OoT's body on Linux, making the write
 * latent there — a fold-order accident (see the #383 FlagTable flip), not a
 * guarantee, and no help for the events/currentEvent data members.
 *
 * Namespacing MM's class (the ShipInit/S2H precedent) does NOT transfer here:
 * MM's only allocator (BenPort.cpp) is an excluded TU, so an S2H-namespaced
 * Instance would never be allocated — null derefs and silently un-armed
 * integration tests. Instead, MM code registers per-frame hooks and stores
 * GIEvents through this MM-owned, extern "C" surface, and never names the
 * C++ class. The registry below is dispatched from MM's own frame loop
 * (games/mm/src/code/game.c, MM_GameState_Update) — the semantics MM's
 * upstream executor had. This also side-steps the #367 hazard class: MM hook
 * ids never enter OoT's registry, so no MM handler can run against OoT state
 * (or vice versa) via ordinal/type aliasing.
 *
 * CONTRACT FOR LANE C (2ship_rando / 2ship_enh un-elision, #392):
 *  - Every `GameInteractor::Instance->RegisterGameHook<...>` in MM code must
 *    migrate to an MM_GameHooks_* registration BEFORE its TU enters the link.
 *    Add further hook types here as they are needed (OnSaveLoad, OnSaveInit,
 *    OnFlagSet, ... — one Register/Unregister/Execute triple per type, with
 *    the Execute call placed at the point in MM's frame/save path where
 *    upstream 2S2H dispatched that hook).
 *  - Every `GameInteractor::Instance->events` / `->currentEvent` access must
 *    migrate to MM_GameEvents_Queue() / MM_GameEvents_Current() below.
 *  - A compile-time guard (games/mm/include/mm_gi_hook_guard.h, force-included
 *    after MM's GameInteractor.h) poisons the Register* member names in every
 *    MM C++ target (2ship_src/2ship_port/2ship_enh/2ship_rando/
 *    2ship_rando_ui) — the 2ship_rando targets migrated in Lane C0 and
 *    2ship_enh in #427 item 2, so no exemptions remain.
 */
#ifndef MM_GAME_HOOKS_H
#define MM_GAME_HOOKS_H

#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register fn to run once per MM frame, at the top of MM_GameState_Update —
 * where upstream 2S2H dispatched OnGameStateMainStart. Returns a nonzero
 * hook id, or 0 if fn is NULL. Hooks registered during dispatch first run on
 * the NEXT dispatch.
 */
uint32_t MM_GameHooks_RegisterOnGameStateMainStart(void (*fn)(void));

/**
 * Queue a hook for removal. Safe to call from inside the hook itself; the
 * removal is applied at the start of the next dispatch (mirroring the C++
 * registry's deferred-unregister contract). Id 0 is ignored.
 */
void MM_GameHooks_UnregisterOnGameStateMainStart(uint32_t hookId);

/**
 * Dispatch point — called from MM's frame loop (games/mm/src/code/game.c).
 * Flushes queued unregistrations, then runs the registered hooks in
 * registration order.
 */
void MM_GameHooks_ExecuteOnGameStateMainStart(void);

/** Number of currently registered hooks (queued unregistrations included
 *  until the next dispatch flushes them). Test/introspection helper. */
uint32_t MM_GameHooks_CountOnGameStateMainStart(void);

/** Drop all hooks and pending unregistrations. Unit-test harness only —
 *  production code must unregister by id. */
void MM_GameHooks_ResetForTest(void);

#ifdef __cplusplus
}
#endif

/*
 * C++ surface: MM-owned storage for the GIEvent queue that MM code used to
 * reach as GameInteractor::Instance->events / ->currentEvent (MM-only data
 * members entirely past the end of the 4-byte shared allocation — the same #395
 * OOB, on the read/write path). GIEvent comes from MM's force-included
 * 2s2h/GameInteractor/GameInteractor.h; its include guard gates this section
 * so C TUs and non-MM includers see only the C API above.
 */
#if defined(__cplusplus) && defined(GAME_INTERACTOR_H)
#include <vector>

std::vector<GIEvent>& MM_GameEvents_Queue();
GIEvent& MM_GameEvents_Current();

/*
 * S2H::GameHooks — the generic MM-owned hook registry behind the Lane C
 * migration (#392, contract in PR #415).
 *
 * MM's upstream registry lives in `GameInteractor::RegisteredGameHooks<H>` /
 * `HooksToUnregister<H>` inline statics, and the registering members
 * read/write `this->nextHookId` on the 4-byte shared allocation — the #395
 * OOB, which is the standing reason MM must not use them and which
 * include/mm_gi_hook_guard.h enforces per target.
 *
 * Those statics ALSO used to COMDAT-contend with OoT's identically-named
 * instantiations, whose H::fn payloads differ for 14 of the 16 shared hook
 * names (#395/#367 class). That half is closed since #470: MM's hook types
 * are tag-scoped under `GameInteractor::MM_HookTypes` (2s2h/GameInteractor/
 * GameInteractor.h), so no MM hook-keyed symbol can share a mangled name
 * with OoT's tree. Do not re-derive the folding argument from this file —
 * the #395 OOB alone is what keeps registration off the upstream members.
 *
 * This registry reproduces the upstream semantics MM's Rando code
 * expects (shared id counter, deferred unregistration flushed at dispatch)
 * under the S2H namespace: only MM TUs instantiate it, so every fold partner
 * agrees on MM's types, and no instance member of the shared class is ever
 * touched. Hook TYPES (e.g. GameInteractor::OnSaveLoad) are used purely as
 * compile-time tags; MM hook ids never enter OoT's registry, so VB/type
 * aliasing is excluded by construction.
 *
 * The single-exe COND_HOOK / COND_ID_HOOK / COND_VB_SHOULD /
 * REGISTER_VB_SHOULD macro redirection at the bottom of MM's
 * GameInteractor.h routes every macro registration site here textually
 * unchanged; direct `Instance->RegisterGameHook*` sites migrate by hand
 * (mm_gi_hook_guard.h poisons them per target as they do).
 *
 * Dispatch: nothing pumps these registries implicitly. Each hook type is
 * dispatched exactly where MM's own code path executes it (upstream 2S2H
 * semantics) — e.g. OnGameStateMainStart via the extern "C" triple above from
 * MM_GameState_Update. Hook types without a wired MM dispatch point yet are
 * REGISTERED but dormant; wiring their Execute calls is per-type, deliberate
 * Lane C work, never a side effect of registration.
 *
 * DORMANT TYPES INTRODUCED BY #427 item 2 (tracked on #438). The 2ship_enh
 * migration moved that target's direct Register* sites onto this registry.
 * Most of the hook types involved were ALREADY dormant — they carry
 * COND_HOOK/COND_ID_HOOK registrants from other MM targets and are listed in
 * #438's table (OnOpenText, OnActorInit, OnActorKill). Two types reach this
 * registry for the first time with that migration and have no Execute
 * placement, plus one that #438's table did not yet name:
 *
 *   - OnPlayDestroy       (Modes/PlayAsKafei.cpp, Songs/BetterSongOfDoubleTime.cpp)
 *   - BeforeKaleidoDrawPage (Masks/PersistentMasks.cpp)
 *   - OnPlayerPostLimbDraw  (Masks/PersistentMasks.cpp)
 *
 * These are dormant, NOT newly broken: before the migration the same
 * registrations went into MM's `GameInteractor::RegisteredGameHooks<H>`
 * statics while MM's call sites for GameInteractor_ExecuteOnPlayDestroy /
 * ExecuteBeforeKaleidoDrawPage / ExecuteOnPlayerPostLimbDraw resolved to
 * OoT's active-game-gated wrappers or to src/common/mm_stubs.c no-ops — so
 * the handlers never ran either way. What the migration removes is the #395
 * out-of-bounds write the raw registration performed on the way to being
 * dead. Wiring them is #438's job, and BeforeKaleidoDrawPage in particular
 * should be wired together with its AfterKaleidoDrawPage pair rather than
 * half-fixed (the reasoning mm_stubs.c records for the EndOfCycleSave pair).
 *
 * One upstream quirk is preserved deliberately rather than repaired here:
 * PersistentMasks.cpp unregisters BeforeKaleidoDrawPage through the plain
 * Unregister<> leg while registering it through RegisterForID<>, so the
 * pending id never matches and the registration is never removed — exactly
 * what upstream 2S2H does (its UnregisterGameHook and RegisterGameHookForID
 * likewise address different maps). While the type stays dormant the stale
 * entry is inert; whoever wires its Execute under #438 must fix the leg
 * pairing at the same time, or the mask border will draw twice.
 *
 * Deviation from upstream, on purpose: iteration uses std::map (stable id
 * order) rather than unordered_map, so dispatch order is deterministic —
 * this phase's locks diff generated worlds byte-for-byte.
 */
#include <cstdint>
#include <map>

namespace S2H {
namespace GameHooks {

template <typename H> struct Registry {
    inline static std::map<uint32_t, typename H::fn> functions;
    inline static std::map<int32_t, std::map<uint32_t, typename H::fn>> functionsForID;
    inline static std::map<uintptr_t, std::map<uint32_t, typename H::fn>> functionsForPtr;
    inline static std::vector<uint32_t> pendingUnregister;
    inline static std::vector<uint32_t> pendingUnregisterForID;
    inline static std::vector<uint32_t> pendingUnregisterForPtr;
};

// Shared across all hook types, mirroring upstream's instance-wide
// nextHookId. Inline-function local static => exactly one instance
// program-wide among MM TUs; OoT never includes this header.
inline uint32_t& NextHookId() {
    static uint32_t next = 1;
    return next;
}

template <typename H> inline uint32_t Register(typename H::fn h) {
    if (!h) {
        return 0;
    }
    uint32_t id = NextHookId()++;
    Registry<H>::functions[id] = h;
    return id;
}

template <typename H> inline uint32_t RegisterForID(int32_t forId, typename H::fn h) {
    if (!h) {
        return 0;
    }
    uint32_t id = NextHookId()++;
    Registry<H>::functionsForID[forId][id] = h;
    return id;
}

// Ptr-keyed leg (upstream RegisterGameHookForPtr): hooks bound to one live
// object (actor instance), keyed by its address. Added for 2ship_enh's
// OnActorUpdate-for-actor registrant (#427 item 2); the Filter leg is still
// deliberately absent — no compiled MM TU registers one (re-audit if
// DeveloperTools.cpp un-elides).
template <typename H> inline uint32_t RegisterForPtr(uintptr_t ptr, typename H::fn h) {
    if (!h) {
        return 0;
    }
    uint32_t id = NextHookId()++;
    Registry<H>::functionsForPtr[ptr][id] = h;
    return id;
}

// Deferred, like upstream: safe to call from inside a running hook; applied
// at the start of the next Execute/ExecuteForID of this hook type. Id 0
// (never issued) is ignored.
template <typename H> inline void Unregister(uint32_t hookId) {
    if (hookId != 0) {
        Registry<H>::pendingUnregister.push_back(hookId);
    }
}

template <typename H> inline void UnregisterForID(uint32_t hookId) {
    if (hookId != 0) {
        Registry<H>::pendingUnregisterForID.push_back(hookId);
    }
}

template <typename H> inline void UnregisterForPtr(uint32_t hookId) {
    if (hookId != 0) {
        Registry<H>::pendingUnregisterForPtr.push_back(hookId);
    }
}

template <typename H> inline void FlushPendingUnregistrations() {
    for (uint32_t id : Registry<H>::pendingUnregister) {
        Registry<H>::functions.erase(id);
    }
    Registry<H>::pendingUnregister.clear();
    for (uint32_t id : Registry<H>::pendingUnregisterForID) {
        for (auto& [_, bucket] : Registry<H>::functionsForID) {
            bucket.erase(id);
        }
    }
    Registry<H>::pendingUnregisterForID.clear();
    for (uint32_t id : Registry<H>::pendingUnregisterForPtr) {
        for (auto& [_, bucket] : Registry<H>::functionsForPtr) {
            bucket.erase(id);
        }
    }
    Registry<H>::pendingUnregisterForPtr.clear();
}

template <typename H, typename... Args> inline void Execute(Args&&... args) {
    FlushPendingUnregistrations<H>();
    for (auto& [_, fn] : Registry<H>::functions) {
        fn(std::forward<Args>(args)...);
    }
}

template <typename H, typename... Args> inline void ExecuteForID(int32_t forId, Args&&... args) {
    FlushPendingUnregistrations<H>();
    auto it = Registry<H>::functionsForID.find(forId);
    if (it == Registry<H>::functionsForID.end()) {
        return;
    }
    for (auto& [_, fn] : it->second) {
        fn(std::forward<Args>(args)...);
    }
}

template <typename H, typename... Args> inline void ExecuteForPtr(uintptr_t ptr, Args&&... args) {
    FlushPendingUnregistrations<H>();
    auto it = Registry<H>::functionsForPtr.find(ptr);
    if (it == Registry<H>::functionsForPtr.end()) {
        return;
    }
    for (auto& [_, fn] : it->second) {
        fn(std::forward<Args>(args)...);
    }
}

// Test/introspection helpers (unit harness only).
template <typename H> inline size_t CountForTest() {
    size_t n = Registry<H>::functions.size();
    for (auto& [_, bucket] : Registry<H>::functionsForID) {
        n += bucket.size();
    }
    for (auto& [_, bucket] : Registry<H>::functionsForPtr) {
        n += bucket.size();
    }
    return n;
}

template <typename H> inline void ResetForTest() {
    Registry<H>::functions.clear();
    Registry<H>::functionsForID.clear();
    Registry<H>::functionsForPtr.clear();
    Registry<H>::pendingUnregister.clear();
    Registry<H>::pendingUnregisterForID.clear();
    Registry<H>::pendingUnregisterForPtr.clear();
}

} // namespace GameHooks
} // namespace S2H
#endif

#endif /* RSBS_SINGLE_EXECUTABLE */

#endif /* MM_GAME_HOOKS_H */
