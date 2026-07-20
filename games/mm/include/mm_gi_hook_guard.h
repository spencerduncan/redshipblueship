/**
 * Compile-time lock for #395: MM translation units must not instantiate
 * GameInteractor's hook-REGISTRATION members.
 *
 * In single-exe builds the one GameInteractor allocation is OoT's (sizeof 4),
 * while MM's force-included 2s2h/GameInteractor/GameInteractor.h compiles the
 * same-named class at sizeof 104 / nextHookId offset 96 (MSVC; 72 / 64 under
 * Linux GCC). The Register* member templates read AND write this->nextHookId,
 * so an MM-compiled instantiation (inlined at the call site, or winning the
 * linker's COMDAT fold) writes ~60-92 bytes past the end of the real
 * allocation. MM code registers per-frame hooks through the MM-owned
 * extern "C" shim instead: games/mm/include/mm_game_hooks.h.
 *
 * The Execute*/Unregister*/GetHookData members only touch the inline-static
 * per-hook-type maps (no instance state), so they stay usable — e.g. the
 * GameInteractor_ExecuteOnRoomInit wrappers in GameExports_SingleExe.cpp.
 *
 * Mechanism: force-included AFTER GameInteractor.h (games/mm/CMakeLists.txt,
 * the _force_include_cxx_guarded list), so the class definition itself parses
 * with the real member names; any LATER use of a poisoned name in the TU
 * fails to compile with an identifier that points here. Applied to
 * 2ship_port/2ship_src; 2ship_enh and 2ship_rando are exempt until Lane C
 * migrates their (currently link-elided) upstream RegisterGameHook calls onto
 * the shim — extend the guarded list in games/mm/CMakeLists.txt when that
 * happens (#392).
 *
 * Escape hatch for deliberate probes: `#undef` the macro in the probing TU
 * (precedent: the rename #undefs in games/mm/2s2h/mm_culling_test.cpp).
 */
#ifndef MM_GI_HOOK_GUARD_H
#define MM_GI_HOOK_GUARD_H

#ifdef __cplusplus

#define RegisterGameHook RSBS_MM_must_not_touch_GameInteractor_registration_use_mm_game_hooks_h_395
#define RegisterGameHookForID RSBS_MM_must_not_touch_GameInteractor_registration_use_mm_game_hooks_h_395_ForID
#define RegisterGameHookForPtr RSBS_MM_must_not_touch_GameInteractor_registration_use_mm_game_hooks_h_395_ForPtr
#define RegisterGameHookForFilter RSBS_MM_must_not_touch_GameInteractor_registration_use_mm_game_hooks_h_395_ForFilter

#endif /* __cplusplus */

#endif /* MM_GI_HOOK_GUARD_H */
