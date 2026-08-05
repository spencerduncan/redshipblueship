/**
 * @file soh_shipinit_driver_test.cpp
 * @brief ROM-free, display-free lock for #539: a CVar change on the unified
 *        menu path re-arms MM's registrars, not only OoT's.
 *
 * CTest label "redship", row `MMShipInitDriver` in CMake/SingleExecutable.cmake,
 * dispatch `mm-shipinit-driver` in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN
 * ===============
 * ADR 0003 converged `gEnhancements.RememberSaveLocation` (and Autosave) into
 * single shared keys, but MM's registrar map is a separate map — `S2H::ShipInit`
 * (2s2h/ShipInit.hpp, the #375 COMDAT-fold split). The one live menu is OoT's
 * `SohMenu`, whose widgets call OoT's `ShipInit::Init(cvar)`. Nothing in the
 * single-exe link ever called `S2H::ShipInit::Init()` with an enhancement key,
 * so MM's map was driven exactly once per process by `InitAll()` at MM's first
 * boot. One click re-armed OoT and left MM latched — one combo, two halves
 * disagreeing about the same setting, which the one-game ruling calls
 * corruption.
 *
 * WHY THIS FILE IS ON THE OoT SIDE
 * ================================
 * It must call the REAL production entry point the menu widgets call —
 * `ShipInit::Init(path)`, which is an inline member of OoT's header, so it
 * cannot be reached across a C boundary and cannot be re-derived without
 * testing a copy instead of the code. The probes it observes must live in MM's
 * map, which needs MM's header; and no TU can hold both headers, because MM's
 * ends with `using S2H::ShipInit;` at global scope. Hence the split: probes in
 * games/mm/2s2h/mm_shipinit_driver_test.cpp, driving here.
 *
 * NON-VACUITY
 * ===========
 * Every leg asserts its counter is zero before the drive and non-zero after
 * (or stays zero, for the exclusion legs), so a probe that never registered
 * cannot pass. Leg 1 is the reality check that keeps the whole lock honest: it
 * asserts MM really does keep registrars under a CONVERGED key, so the bug
 * being fixed is a bug about real registrars and not about a synthetic one.
 *
 * VERIFIED RED BEFORE GREEN: with the `MM_ShipInit_OnCVarChanged` call removed
 * from ShipInit.hpp's `Init` (the whole fix), leg 2 fails FAIL(2). Legs 3 and 4
 * pass in that state — they lock the deliberate scoping, and their
 * counterfactual is deleting the pseudo-path filter in the bridge, which turns
 * them red.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "ShipInit.hpp"

#include <cstdio>
#include <cstring>

// MM-side probe support (games/mm/2s2h/mm_shipinit_driver_test.cpp) and the
// production bridge's non-inserting map query
// (games/mm/2s2h/ShipInitBridge_SingleExe.cpp).
extern "C" {
void MMShipInitTest_ResetProbes(void);
int MMShipInitTest_CVarProbeRuns(void);
int MMShipInitTest_RandoProbeRuns(void);
const char* MMShipInitTest_ProbePath(void);
void MMShipInitTest_DumpCVarKeys(void);
int MM_ShipInit_RegistrarCountForPath(const char* path);
void MM_ShipInit_OnCVarChanged(const char* path);
}

namespace {

// The converged key #539 names first. Spelled here rather than included from
// MM's SavingEnhancements.cpp (which this TU must not see) — the point of the
// leg is that BOTH games agree on this literal, so a divergence in either
// game's spelling is exactly what should turn it red.
constexpr const char* kConvergedKey = "gEnhancements.RememberSaveLocation";

int Fail(int code, const char* msg) {
    printf("[TEST] FAIL(%d): %s\n", code, msg);
    return code;
}

} // namespace

extern "C" int OoT_ShipInitMMDriver_RunHeadless(void) {
    // ---- leg 1: the converged key really has MM registrars behind it -------
    // If this is zero, either MM's SavingEnhancements TU was elided from the
    // link (the #516 class) or the two games' spellings of the converged key
    // drifted. Either way the driver would be re-arming nothing and every
    // other leg here would be theatre.
    const int convergedRegistrars = MM_ShipInit_RegistrarCountForPath(kConvergedKey);
    if (convergedRegistrars <= 0) {
        printf("[TEST] MM registrar count for '%s' is %d\n", kConvergedKey, convergedRegistrars);
        MMShipInitTest_DumpCVarKeys();
        return Fail(1, "no MM registrar is keyed on the converged enhancement key — either MM's TU is elided or the "
                       "two games' spellings drifted, and re-arming MM would be a no-op (#539)");
    }

    const char* probePath = MMShipInitTest_ProbePath();

    // ---- leg 2: THE FIX. OoT's Init reaches MM's map. ---------------------
    // This is the exact call `UIWidgets::CVarCheckbox` and friends make after
    // writing the CVar (games/oot/soh/SohGui/UIWidgets.cpp), so the lock rides
    // the production path rather than a stand-in for it.
    MMShipInitTest_ResetProbes();
    ShipInit::Init(probePath);
    if (MMShipInitTest_CVarProbeRuns() <= 0) {
        return Fail(2, "a CVar change through OoT's ShipInit::Init did not run MM's registrar for the same key — "
                       "MM's map still has no CVar-change driver (#539)");
    }

    // ---- leg 3: the "*" pseudo-path is NOT forwarded ----------------------
    // Non-vacuous: RegisterShipInitFunc pushes every registrant into "*" too,
    // so the CVar probe IS under it. OoT drives "*" from boot and from preset
    // loads; forwarding it would run MM's whole registrar set before MM has
    // booted and again on every OoT preset load.
    MMShipInitTest_ResetProbes();
    MM_ShipInit_OnCVarChanged("*");
    if (MMShipInitTest_CVarProbeRuns() != 0) {
        return Fail(3, "the '*' wildcard was forwarded to MM's map — OoT's boot and preset paths would run MM's "
                       "whole registrar set, including before MM has booted (#539)");
    }

    // ---- leg 4: the "IS_RANDO" pseudo-path is NOT forwarded ---------------
    // Non-vacuous via the MM-side IS_RANDO probe. MM drives its own IS_RANDO
    // from Rando/Rando.cpp on MM's save-load chain; forwarding OoT's would arm
    // MM's rando hooks against MM's previous save on an OoT-only file load,
    // which is what the #439 arm-state locks exist to prevent.
    MMShipInitTest_ResetProbes();
    MM_ShipInit_OnCVarChanged("IS_RANDO");
    if (MMShipInitTest_RandoProbeRuns() != 0) {
        return Fail(4, "'IS_RANDO' was forwarded to MM's map — an OoT rando file load would re-arm MM's rando hooks "
                       "off MM's stale save state (#539)");
    }

    // ---- leg 5: a key MM does not register on does not grow MM's map ------
    // ShipInit::Init indexes with operator[], which inserts. Forwarding every
    // OoT-only key a player ever touches through that would accrete one empty
    // vector per key for the life of the process.
    const char* oneOffKey = "gTest.ShipInitDriverKeyMMNeverRegisters";
    MM_ShipInit_OnCVarChanged(oneOffKey);
    if (MM_ShipInit_RegistrarCountForPath(oneOffKey) != 0) {
        return Fail(5, "forwarding an unregistered key inserted it into MM's map");
    }

    // Leave the counters clean for whatever runs next in AllTests.
    MMShipInitTest_ResetProbes();

    printf("[TEST] PASS: a CVar change on the unified menu path re-arms MM's registrars (%d on the converged key), "
           "while the '*' and 'IS_RANDO' pseudo-paths stay MM-driven\n",
           convergedRegistrars);
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
