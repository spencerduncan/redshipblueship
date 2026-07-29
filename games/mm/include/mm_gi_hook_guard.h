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
 * The Execute*, Unregister*, and GetHookData members only touch the
 * inline-static per-hook-type maps (no instance state), so they stay usable
 * — e.g. the GameInteractor_ExecuteOnRoomInit wrappers in
 * GameExports_SingleExe.cpp. That permission is fold-safe as well as
 * OOB-safe: since #470 MM's hook types are tag-scoped under
 * GameInteractor::MM_HookTypes (see the DEFINE_HOOK block in
 * 2s2h/GameInteractor/GameInteractor.h), so the per-hook-type statics an MM
 * Execute*/GetHookData instantiation touches mangle MM-distinct and cannot
 * COMDAT-fold with OoT's divergent-payload instantiations of the same hook
 * NAME.
 *
 * Mechanism: force-included AFTER GameInteractor.h (games/mm/CMakeLists.txt,
 * the _force_include_cxx_guarded list), so the class definition itself parses
 * with the real member names; any LATER use of a poisoned name in the TU
 * fails to compile with an identifier that points here. Applied to every MM
 * C++ target with no exemptions: 2ship_src/2ship_port (#415),
 * 2ship_rando/2ship_rando_ui (Lane C0, #392), and 2ship_enh (#427 item 2,
 * which retired the last one). #442's interim carve-out — guarding only the
 * six 2ship_enh sources the link census confirmed were non-elided
 * (_mm_gi_hook_guard_linked_enh_sources in games/mm/CMakeLists.txt, added
 * after SavingEnhancements.cpp's raw sites reached the shared instance
 * through the target-wide exemption) — is gone with it: whole-target
 * coverage subsumes the list, so there is no longer a census to keep in sync
 * and no way for a newly un-elided 2ship_enh TU to arrive carrying raw
 * registrations.
 *
 * The only upstream raw-registration file left untouched is
 * 2s2h/Enhancements/Audio/AudioEditor.cpp, which is not compiled at all in
 * single-exe builds (dropped from ship__Enhancements in
 * games/mm/CMakeLists.txt because it depends on the excluded BenGui/BenMenu
 * surface). If it is ever restored to the build, its
 * OnRandoSeedGeneration registration must migrate first — the guard will say
 * so at compile time.
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
