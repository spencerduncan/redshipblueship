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
 *    after MM's GameInteractor.h) poisons the Register* member names in
 *    2ship_port/2ship_src C++ TUs; extend it to 2ship_enh/2ship_rando as
 *    their TUs migrate.
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
#endif

#endif /* RSBS_SINGLE_EXECUTABLE */

#endif /* MM_GAME_HOOKS_H */
