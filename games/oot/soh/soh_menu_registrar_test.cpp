/**
 * @file soh_menu_registrar_test.cpp
 * @brief ROM-free, display-free lock for #640: soh_port's self-registering
 *        menu TUs must survive the link, proven by their EFFECT on the
 *        MenuInit registries rather than by naming their symbols.
 *
 * CTest label "redship", row OoTMenuRegistrars in CMake/SingleExecutable.cmake,
 * dispatch "oot-menu-registrars" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN
 * ===============
 * games/oot/soh/Network/Anchor/Menu.cpp populates the Network / Anchor page
 * only through `static RegisterMenuInitFunc menuInitFunc(RegisterAnchorMenu)`;
 * nothing else references a symbol in that TU. It is built into soh_port,
 * which until this fix was the one OoT archive linked WITHOUT WHOLE_ARCHIVE,
 * so the linker was free to drop the member -- and did: the registrar never
 * ran and the page SohMenuNetwork.cpp registers with columnCount = 2 had zero
 * widgets. Downstream, Menu::DrawElement's unconditional SetNextWindowPos was
 * never consumed by that widget-less multi-column page and undocked
 * libultraship's "Main Game" window (the grey client area and displaced game
 * rectangle in the report behind #640). SohGui/ResolutionEditor.cpp -- the
 * Settings / Graphics resolution editor -- is the same shape (nothing outside it
 * references RegisterResolutionWidgets, UpdateResolutionVars or
 * IsDroppingFrames) and was being dropped too, on Windows as well, which is
 * measured below. games/oot/CMakeLists.txt now WHOLE_ARCHIVEs soh_port like
 * soh_enh and soh_rando (#341); this row is the runtime half of that lock.
 *
 * WHY THIS ROW OBSERVES INSTEAD OF REFERENCING
 * ============================================
 * A test that took the address of RegisterAnchorMenu or
 * RegisterResolutionWidgets would itself pull their objects into the link --
 * the reference IS the explicit-reference fallback -- and the row would pass
 * vacuously with the CMake fix reverted. So this TU names no symbol from
 * either registrar TU. It reads the two MenuInit registries (function-local
 * statics of a header-defined struct, SohGui/MenuTypes.h:274-292, so one
 * instance program-wide) that only static initializers populate, before
 * main():
 *
 *   Leg 1  MenuInit::GetUpdateFuncs()["Settings"]["Graphics"] has exactly one
 *          registrant. ResolutionEditor.cpp is the SOLE RegisterMenuUpdateFunc
 *          registrant in the tree (`grep -rn 'static RegisterMenuUpdateFunc'
 *          games/oot`), so a populated entry is an exact, page-keyed tell that
 *          the soh_port TU linked AND its initializer ran.
 *
 *   Leg 2  MenuInit::GetInitFuncs().size() equals the number of
 *          `static RegisterMenuInitFunc` registrars compiled into the link:
 *          nine under soh/Enhancements (six in soh_enh, three in soh_rando,
 *          all unconditional, both archives WHOLE_ARCHIVE'd since #341) plus
 *          ResolutionEditor.cpp, plus Network/Anchor/Menu.cpp when
 *          ENABLE_REMOTE_CONTROL is defined. Only leg 2 can see the Anchor
 *          registrar at all: RegisterMenuInitFunc records no page, so there is
 *          nothing page-keyed to look at until the funcs are CALLED, which
 *          needs a window (below).
 *
 * The exact count is deliberate: a floor would let the next elided registrar
 * through. When an upstream sync adds a registrar, bump the constant below
 * after confirming the new TU really is in the link -- that confirmation is
 * the point.
 *
 * VERIFIED RED BEFORE GREEN (Windows / MSVC, warm tree, the ONLY difference
 * being games/oot/CMakeLists.txt's set(SOH_STATIC_TARGETS ...)):
 *
 *   soh_port plain   FAIL(1) 0 Settings/Graphics update registrants
 *                    FAIL(2) 9 init registrants, expected 10
 *   WHOLE_ARCHIVE    leg 1: 1 registrant, leg 2: 10 = 9 + 1  PASS
 *
 * The redship.map census agrees from the other end: 149 of soh_port's 150
 * objects reach the plain link, the missing one being ResolutionEditor.cpp.obj
 * (0 hits for RegisterResolutionWidgets); 150 of 150 with WHOLE_ARCHIVE. So the
 * resolution editor was missing from Settings / Graphics on Windows too, not
 * only on the CI builds where the Anchor page visibly broke.
 *
 * WHAT IS NOT COVERED LOCALLY. This workstation has no SDL2_net, so
 * ENABLE_REMOTE_CONTROL is off and the Anchor registrar is not compiled at all;
 * kPortMenuInitRegistrars is 1 here and the expected total is 10. CI passes
 * -DBUILD_REMOTE_CONTROL=1 on every platform (generate-builds.yml,
 * link-check.yml), where the same row expects 11 and the Anchor arm is what
 * bites. Beware also that the plain link is brittle in a way that makes a
 * "green" reading cheap: whether ResolutionEditor.cpp.obj gets pulled at all
 * depends on which TU the linker takes some std::unordered_map COMDAT
 * instantiation from, and adding an unrelated TU can flip it. Read a green
 * result as "the registrars ran", never as "the archive semantics are safe".
 *
 * WHY NOT "every multi-column page has a widget after AddMenuElements"
 * ===================================================================
 * That is the invariant Menu::DrawElement's #640 hardening protects, and it
 * is the row #640 asked for. It cannot run headless: RegisterResolutionWidgets
 * -- un-elided by this very fix -- dynamic_pointer_casts
 * Ship::Context::GetInstance()->GetWindow() to a Fast::Fast3dWindow and
 * dereferences it immediately (ResolutionEditor.cpp:382-383), and
 * Context::InitWindow() calls Window::Init() (a real SDL window), so there is
 * no display-free way to hand it one. The registries checked here sit one step
 * upstream of the page contents and are exactly what went missing.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/SohGui/MenuTypes.h"

#include <cstddef>
#include <cstdio>

namespace {

// `static RegisterMenuInitFunc` registrars under soh/Enhancements, all
// unconditional (no enclosing #if), all in WHOLE_ARCHIVE'd archives:
// audio/AudioEditor.cpp, controls/SohInputEditorWindow.cpp,
// cosmetics/CosmeticsEditor.cpp, Lang/Lang.cpp, mod_menu.cpp,
// Presets/Presets.cpp (soh_enh), randomizer/randomizer_check_tracker.cpp,
// randomizer/randomizer_entrance_tracker.cpp,
// randomizer/randomizer_item_tracker.cpp (soh_rando).
constexpr size_t kEnhancementMenuInitRegistrars = 9;

// soh_port's own: SohGui/ResolutionEditor.cpp always; Network/Anchor/Menu.cpp
// only when the Anchor client is compiled in.
constexpr size_t kPortMenuInitRegistrars = 1
#ifdef ENABLE_REMOTE_CONTROL
                                           + 1
#endif
    ;

#ifdef ENABLE_REMOTE_CONTROL
constexpr const char* kAnchorNote = "Anchor compiled in";
#else
constexpr const char* kAnchorNote = "Anchor compiled out (no ENABLE_REMOTE_CONTROL)";
#endif

} // namespace

extern "C" int OoT_MenuRegistrars_RunHeadless(void) {
    printf("[TEST] oot-menu-registrars: soh_port's menu registrars survive the link (#640)\n");

    int failures = 0;

    // Leg 1: the update-func registry. Keyed by (section, sidebar); only
    // ResolutionEditor.cpp registers into it, under ("Settings", "Graphics").
    // Observed with contains()/at() so the probe cannot create the entry it is
    // looking for.
    const auto& updateFuncs = MenuInit::GetUpdateFuncs();
    size_t graphicsUpdaters = 0;
    if (updateFuncs.contains("Settings") && updateFuncs.at("Settings").contains("Graphics")) {
        graphicsUpdaters = updateFuncs.at("Settings").at("Graphics").size();
    }
    if (graphicsUpdaters == 1) {
        printf("[TEST] leg 1: Settings/Graphics has 1 MenuInit update registrant (ResolutionEditor.cpp linked)\n");
    } else {
        printf("[TEST] FAIL(1): Settings/Graphics has %zu MenuInit update registrant(s), expected exactly 1 -- "
               "SohGui/ResolutionEditor.cpp's static registrar did not run (soh_port member elided from the link, "
               "or a second RegisterMenuUpdateFunc registrant appeared for that page)\n",
               graphicsUpdaters);
        failures++;
    }

    // Leg 2: the init-func registry. Every registrar in the link pushes exactly
    // one entry before main(), so the size is the number of registrar TUs that
    // survived. 9 is the whole-archived Enhancements set; the remainder is
    // soh_port's.
    const size_t expected = kEnhancementMenuInitRegistrars + kPortMenuInitRegistrars;
    const size_t actual = MenuInit::GetInitFuncs().size();
    if (actual == expected) {
        printf("[TEST] leg 2: %zu MenuInit init registrants = %zu Enhancements + %zu soh_port (%s)\n", actual,
               kEnhancementMenuInitRegistrars, kPortMenuInitRegistrars, kAnchorNote);
    } else {
        printf("[TEST] FAIL(2): %zu MenuInit init registrants, expected %zu = %zu Enhancements + %zu soh_port (%s) "
               "-- a soh_port registrar TU (SohGui/ResolutionEditor.cpp, Network/Anchor/Menu.cpp) was elided from "
               "the link, or the registrar census in this file is stale (grep -rn 'static RegisterMenuInitFunc' "
               "games/oot and update the constants after confirming the new TU is linked)\n",
               actual, expected, kEnhancementMenuInitRegistrars, kPortMenuInitRegistrars, kAnchorNote);
        failures++;
    }

    if (failures == 0) {
        printf("[TEST] oot-menu-registrars: PASS\n");
    }
    return failures;
}

#endif // RSBS_SINGLE_EXECUTABLE
